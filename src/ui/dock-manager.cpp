/*
Control Panel for OBS Studio
Copyright (C) 2026 Voidscape Development

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "dock-manager.hpp"
#include "panel-view.hpp"
#include "../core/runtime.hpp"

#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QSet>
#include <QToolButton>
#include <QVBoxLayout>

namespace cp {

namespace {
const char *kSwitcherDockId = "control-panel-switcher";

QString dockIdFor(const QString &panelId)
{
	return QStringLiteral("control-panel-") + panelId;
}
} // namespace

/* ===================================================================== */
/* SwitcherWidget                                                        */
/* ===================================================================== */

SwitcherWidget::SwitcherWidget(QWidget *parent) : QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);

	auto *bar = new QHBoxLayout();
	bar->setContentsMargins(2, 2, 2, 0);
	m_combo = new QComboBox(this);
	m_combo->setAccessibleName(T("Switcher.Panel"));
	m_combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	auto *edit = new QToolButton(this);
	edit->setText(QStringLiteral("✎"));
	edit->setToolTip(T("View.EditLayout"));
	edit->setAccessibleName(T("View.EditLayout"));
	edit->setCheckable(true);
	bar->addWidget(m_combo, 1);
	bar->addWidget(edit);
	layout->addLayout(bar);

	m_view = new PanelView(this);
	m_view->setShowSwitchMenu(true);
	layout->addWidget(m_view, 1);

	connect(m_combo, &QComboBox::activated, this, [this]() {
		const QString id = m_combo->currentData().toString();
		m_view->setPanelId(id);
		if (Runtime *rt = Runtime::get())
			rt->store().setSwitcherPanel(id);
	});
	connect(edit, &QToolButton::toggled, m_view, &PanelView::setEditMode);
	connect(m_view, &PanelView::editModeChanged, edit, [edit](bool on) {
		QSignalBlocker b(edit);
		edit->setChecked(on);
	});

	if (Runtime *rt = Runtime::get()) {
		connect(&rt->store(), &PanelStore::panelsReset, this, &SwitcherWidget::refreshList);
		connect(&rt->store(), &PanelStore::panelChanged, this, &SwitcherWidget::refreshList);
		connect(rt, &Runtime::collectionChanged, this, &SwitcherWidget::refreshList);
	}
	refreshList();
}

QString SwitcherWidget::panel() const
{
	return m_view->panelId();
}

void SwitcherWidget::setPanel(const QString &id)
{
	if (Runtime *rt = Runtime::get())
		rt->store().setSwitcherPanel(id);
	refreshList();
}

void SwitcherWidget::step(int delta)
{
	const int n = m_combo->count();
	if (n == 0)
		return;
	const int idx = (std::max(0, m_combo->currentIndex()) + delta + n) % n;
	setPanel(m_combo->itemData(idx).toString());
}

void SwitcherWidget::refreshList()
{
	Runtime *rt = Runtime::get();
	if (!rt)
		return;

	QSignalBlocker blocker(m_combo);
	m_combo->clear();
	for (const Panel &p : rt->store().panels())
		if (p.showInSwitcher && rt->isPanelActive(p))
			m_combo->addItem(p.name, p.id);

	/* Keep the remembered panel if it's still listed, else the first one. */
	int idx = m_combo->findData(rt->store().switcherPanel());
	if (idx < 0 && m_combo->count() > 0)
		idx = 0;
	m_combo->setCurrentIndex(idx);
	m_combo->setEnabled(m_combo->count() > 0);
	if (m_combo->count() == 0)
		m_combo->setPlaceholderText(T("Switcher.None"));
	m_view->setPanelId(idx >= 0 ? m_combo->itemData(idx).toString() : QString());
}

/* ===================================================================== */
/* DockManager                                                           */
/* ===================================================================== */

DockManager::DockManager(QObject *parent) : QObject(parent)
{
	Runtime *rt = Runtime::get();
	connect(&rt->store(), &PanelStore::panelsReset, this, &DockManager::syncDocks);
	connect(&rt->store(), &PanelStore::panelChanged, this, &DockManager::syncDocks);
	connect(rt, &Runtime::collectionChanged, this, &DockManager::applyBindings);
	connect(rt, &Runtime::showPanelRequested, this, &DockManager::showPanel);
	connect(rt, &Runtime::switchPanelRequested, this, &DockManager::switchTo);
}

DockManager::~DockManager()
{
	shutdown();
}

void DockManager::start()
{
	if (m_started)
		return;
	m_started = true;

	auto *switcher = new SwitcherWidget();
	switcher->setObjectName(QStringLiteral("ControlPanelSwitcher"));
	if (obs_frontend_add_dock_by_id(kSwitcherDockId, Tc("Switcher.Title"), switcher))
		m_switcher = switcher;
	else
		delete switcher;

	syncDocks();
}

void DockManager::markLoaded()
{
	m_loaded = true;
	applyBindings();
}

void DockManager::shutdown()
{
	if (!m_started)
		return;
	m_started = false;
	const QStringList ids = m_docks.keys();
	for (const QString &id : ids)
		removeDock(id);
	if (m_switcher) {
		obs_frontend_remove_dock(kSwitcherDockId);
		m_switcher = nullptr;
	}
}

QDockWidget *DockManager::dockOf(QWidget *content)
{
	/* obs_frontend_add_dock_by_id wraps our widget in a QDockWidget. */
	for (QWidget *w = content ? content->parentWidget() : nullptr; w; w = w->parentWidget())
		if (auto *dock = qobject_cast<QDockWidget *>(w))
			return dock;
	return nullptr;
}

void DockManager::addDock(const QString &panelId)
{
	Runtime *rt = Runtime::get();
	const Panel *panel = rt->store().panel(panelId);
	if (!panel)
		return;

	DockEntry entry;
	entry.dockId = dockIdFor(panelId);
	entry.title = panel->name;
	auto *view = new PanelView();
	view->setObjectName(QStringLiteral("ControlPanelView-") + panelId);
	view->setPanelId(panelId);
	if (!obs_frontend_add_dock_by_id(entry.dockId.toUtf8().constData(), panel->name.toUtf8().constData(), view)) {
		obs_log(LOG_WARNING, "Could not add dock for panel '%s'", qUtf8Printable(panel->name));
		delete view;
		return;
	}
	entry.view = view;
	entry.dock = dockOf(view);
	m_docks.insert(panelId, entry);

	/* Docks created while OBS is running (from the editor) appear right away;
	 * at startup OBS restores their saved visibility itself. */
	if (m_loaded && entry.dock && rt->isPanelActive(*panel)) {
		entry.dock->setFloating(true);
		entry.dock->show();
		entry.dock->raise();
	}
}

void DockManager::removeDock(const QString &panelId)
{
	const DockEntry entry = m_docks.take(panelId);
	if (!entry.dockId.isEmpty())
		obs_frontend_remove_dock(entry.dockId.toUtf8().constData());
}

void DockManager::syncDocks()
{
	if (!m_started)
		return;
	Runtime *rt = Runtime::get();

	QSet<QString> wanted;
	for (const Panel &p : rt->store().panels())
		if (p.showAsDock)
			wanted.insert(p.id);

	for (const QString &id : m_docks.keys())
		if (!wanted.contains(id))
			removeDock(id);

	for (const Panel &p : rt->store().panels()) {
		if (!p.showAsDock)
			continue;
		if (!m_docks.contains(p.id)) {
			addDock(p.id);
			continue;
		}
		DockEntry &entry = m_docks[p.id];
		if (entry.title != p.name && entry.dock) {
			entry.title = p.name;
			entry.dock->setWindowTitle(p.name);
		}
	}
	applyBindings();
}

void DockManager::applyBindings()
{
	/* Before OBS finishes loading the collection isn't known yet and dock
	 * visibility hasn't been restored; acting now would hide docks wrongly. */
	if (!m_started || !m_loaded)
		return;
	Runtime *rt = Runtime::get();
	PanelStore &store = rt->store();
	QStringList hidden = store.bindingHiddenDocks();

	for (auto it = m_docks.begin(); it != m_docks.end(); ++it) {
		QDockWidget *dock = it->dock;
		const Panel *panel = store.panel(it.key());
		if (!dock || !panel)
			continue;
		const bool active = rt->isPanelActive(*panel);
		dock->toggleViewAction()->setVisible(active);
		if (!active && dock->isVisible()) {
			if (!hidden.contains(it.key()))
				hidden.append(it.key());
			dock->hide();
		} else if (active && hidden.contains(it.key())) {
			hidden.removeAll(it.key());
			dock->show();
		}
	}
	store.setBindingHiddenDocks(hidden);
}

void DockManager::raise(QDockWidget *dock)
{
	if (!dock)
		return;
	dock->show();
	dock->raise();
	dock->activateWindow();
}

void DockManager::showPanel(const QString &id)
{
	Runtime *rt = Runtime::get();
	const Panel *panel = rt->store().panel(id);
	if (!panel || !rt->isPanelActive(*panel))
		return;
	if (m_docks.contains(id) && m_docks[id].dock)
		raise(m_docks[id].dock);
	else
		switchTo(id);
}

void DockManager::switchTo(const QString &id)
{
	Runtime *rt = Runtime::get();
	const Panel *panel = rt->store().panel(id);
	if (!panel || !rt->isPanelActive(*panel))
		return;
	if (!m_switcher || !panel->showInSwitcher) {
		/* Not listed in the switcher: fall back to its own dock. */
		if (m_docks.contains(id))
			raise(m_docks[id].dock);
		return;
	}
	m_switcher->setPanel(id);
	raise(dockOf(m_switcher));
}

void DockManager::stepSwitcher(int delta)
{
	if (!m_switcher)
		return;
	m_switcher->step(delta);
	raise(dockOf(m_switcher));
}

} // namespace cp
