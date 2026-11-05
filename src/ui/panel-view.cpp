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

#include "panel-view.hpp"
#include "cell-inspector.hpp"
#include "cell-painter.hpp"
#include "edit-ops.hpp"
#include "grid-canvas.hpp"
#include "../core/runtime.hpp"
#include "../model/grid.hpp"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace cp {

namespace {

constexpr int kPollMs = 250;

bool frontendReady()
{
	const Runtime *rt = Runtime::get();
	return rt && rt->isFrontendReady();
}

/* Background + palette host for widget-type cells. */
class CellFrame : public QWidget {
public:
	CellFrame(const ResolvedStyle &style, QWidget *parent) : QWidget(parent), m_style(style)
	{
		QPalette pal = palette();
		pal.setColor(QPalette::WindowText, style.text);
		pal.setColor(QPalette::Text, style.text);
		pal.setColor(QPalette::ButtonText, style.text);
		setPalette(pal);
		if (style.fontSize > 0) {
			QFont f = font();
			f.setPointSize(style.fontSize);
			setFont(f);
		}
		auto *l = new QVBoxLayout(this);
		l->setContentsMargins(2, 2, 2, 2);
	}

	void setChild(QWidget *child) { layout()->addWidget(child); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		QPainterPath path;
		path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), m_style.radius, m_style.radius);
		p.fillPath(path, m_style.background);
	}

private:
	ResolvedStyle m_style;
};

} // namespace

PanelView::PanelView(QWidget *parent) : QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	m_stack = new QStackedWidget(this);
	layout->addWidget(m_stack);

	m_runPage = new QWidget(m_stack);
	m_runPage->installEventFilter(this);
	m_placeholder = new QLabel(m_runPage);
	m_placeholder->setAlignment(Qt::AlignCenter);
	m_placeholder->setWordWrap(true);
	m_placeholder->setEnabled(false);
	m_stack->addWidget(m_runPage);

	/* Edit page: a slim toolbar over the canvas. */
	m_editPage = new QWidget(m_stack);
	auto *ev = new QVBoxLayout(m_editPage);
	ev->setContentsMargins(2, 2, 2, 2);
	ev->setSpacing(2);
	auto *bar = new QHBoxLayout();
	m_rows = new QSpinBox(m_editPage);
	m_cols = new QSpinBox(m_editPage);
	for (QSpinBox *s : {m_rows, m_cols})
		s->setRange(1, kMaxGridSize);
	m_rows->setPrefix(T("Editor.Rows") + QStringLiteral(" "));
	m_cols->setPrefix(T("Editor.Cols") + QStringLiteral(" "));
	auto *more = new QPushButton(T("Editor.OpenFull"), m_editPage);
	auto *done = new QPushButton(T("Editor.Done"), m_editPage);
	done->setDefault(true);
	bar->addWidget(m_rows);
	bar->addWidget(m_cols);
	bar->addStretch();
	bar->addWidget(more);
	bar->addWidget(done);
	ev->addLayout(bar);
	m_canvas = new GridCanvas(m_editPage);
	ev->addWidget(m_canvas, 1);
	m_stack->addWidget(m_editPage);

	connect(done, &QPushButton::clicked, this, [this]() { setEditMode(false); });
	connect(more, &QPushButton::clicked, this, [this]() {
		setEditMode(false);
		if (Runtime *rt = Runtime::get())
			rt->openEditor(m_panelId);
	});
	auto onGridSize = [this]() {
		if (m_rows->value() == m_panel.rows && m_cols->value() == m_panel.cols)
			return;
		const int rows = m_rows->value(), cols = m_cols->value();
		modifyPanel([this, rows, cols](Panel &p) { return edit::resizeGrid(this, p, rows, cols); });
		/* Reflect a cancelled resize. */
		QSignalBlocker b1(m_rows), b2(m_cols);
		m_rows->setValue(m_panel.rows);
		m_cols->setValue(m_panel.cols);
	};
	for (QSpinBox *s : {m_rows, m_cols}) {
		/* Only react to committed values, not every keystroke. */
		s->setKeyboardTracking(false);
		connect(s, &QSpinBox::valueChanged, this, onGridSize);
	}

	connect(m_canvas, &GridCanvas::createRequested, this, &PanelView::createCell);
	connect(m_canvas, &GridCanvas::editRequested, this, &PanelView::editCell);
	connect(m_canvas, &GridCanvas::geometryRequested, this, [this](const QString &id, const QRect &area) {
		modifyPanel([&](Panel &p) {
			Cell *c = p.cell(id);
			if (!c || !grid::isFree(p, area, id))
				return false;
			c->setArea(area);
			return true;
		});
	});
	connect(m_canvas, &GridCanvas::deleteRequested, this,
		[this](const QString &id) { modifyPanel([&](Panel &p) { return edit::removeCell(p, id); }); });
	connect(m_canvas, &GridCanvas::duplicateRequested, this, [this](const QString &id) {
		QString newId;
		modifyPanel([&](Panel &p) {
			newId = edit::duplicateCell(p, id);
			return !newId.isEmpty();
		});
		if (!newId.isEmpty())
			m_canvas->setSelectedCell(newId);
	});

	m_pollTimer.setInterval(kPollMs);
	connect(&m_pollTimer, &QTimer::timeout, this, &PanelView::tick);

	if (Runtime *rt = Runtime::get()) {
		connect(&rt->store(), &PanelStore::panelChanged, this, [this](const QString &id) {
			if (id == m_panelId)
				rebuild();
		});
		connect(&rt->store(), &PanelStore::panelsReset, this, &PanelView::rebuild);
		connect(rt, &Runtime::refreshRequested, this, [this]() {
			if (isVisible())
				tick();
		});
	}
	if (TypeRegistry *reg = TypeRegistry::instance())
		connect(reg, &TypeRegistry::typesChanged, this, &PanelView::rebuild);

	rebuild();
}

PanelView::~PanelView()
{
	destroyItems();
}

void PanelView::setPanelId(const QString &id)
{
	if (id == m_panelId)
		return;
	m_panelId = id;
	setEditMode(false);
	rebuild();
}

void PanelView::setEditMode(bool on)
{
	Runtime *rt = Runtime::get();
	if (on && (!rt || !rt->store().panel(m_panelId)))
		on = false;
	if (on == m_editMode)
		return;
	m_editMode = on;
	m_stack->setCurrentWidget(on ? m_editPage : m_runPage);
	if (on)
		m_canvas->setFocus();
	emit editModeChanged(on);
}

void PanelView::destroyItems()
{
	for (Item &item : m_items) {
		/* Let the type tear down its own widget (plugins may need to). */
		if (item.typeWidget && item.widgetType)
			item.widgetType->destroy(item.typeWidget);
		delete item.widget;
	}
	m_items.clear();
}

void PanelView::rebuild()
{
	destroyItems();

	Runtime *rt = Runtime::get();
	TypeRegistry *reg = TypeRegistry::instance();
	const Panel *panel = rt ? rt->store().panel(m_panelId) : nullptr;
	if (!panel || !reg) {
		m_panel = Panel();
		m_placeholder->setText(m_panelId.isEmpty() ? T("View.NoPanel") : T("View.PanelMissing"));
		m_placeholder->show();
		setEditMode(false);
		layoutItems();
		return;
	}
	m_panel = *panel;
	m_placeholder->setText(T("View.Empty"));
	m_placeholder->setVisible(m_panel.cells.isEmpty());

	QHash<QString, QString> widgetNames;
	for (const WidgetTypePtr &w : reg->widgets())
		widgetNames.insert(w->id(), w->name());
	m_canvas->setWidgetTypeNames(widgetNames);
	m_canvas->setPanel(m_panel);
	QSignalBlocker b1(m_rows), b2(m_cols);
	m_rows->setValue(m_panel.rows);
	m_cols->setValue(m_panel.cols);

	for (const Cell &cell : m_panel.cells) {
		Item item;
		item.cellId = cell.id;
		item.area = cell.area();
		const CellVisual visual = visualFor(cell, m_panel.theme, palette());

		switch (cell.kind) {
		case CellKind::Button: {
			auto *btn = new CellButton(m_runPage);
			btn->setVisual(visual);
			const QString panelId = m_panelId, cellId = cell.id;
			connect(btn, &CellButton::clicked, this, [panelId, cellId]() {
				/* Queued: an action may switch this very view to another
				 * panel, which would delete the button mid-click. */
				if (Runtime *r = Runtime::get())
					QMetaObject::invokeMethod(
						r, [r, panelId, cellId]() { r->triggerCell(panelId, cellId); },
						Qt::QueuedConnection);
			});
			const int idx = indicatorIndex(cell);
			if (idx >= 0) {
				item.stateType = reg->action(cell.actions[idx].type);
				if (item.stateType)
					item.stateSettings =
						settingsFor(item.stateType.get(), cell.actions[idx].settings);
				else
					btn->setState(CONTROL_PANEL_STATE_UNAVAILABLE);
			}
			/* A step whose type is gone (plugin removed) marks the button. */
			for (const Action &a : cell.actions)
				if (!reg->action(a.type))
					btn->setState(CONTROL_PANEL_STATE_UNAVAILABLE);
			btn->setToolTip(cell.label);
			item.button = btn;
			item.widget = btn;
			break;
		}
		case CellKind::Label: {
			auto *lbl = new CellButton(m_runPage);
			lbl->setVisual(visual);
			lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
			item.widget = lbl;
			break;
		}
		case CellKind::Widget: {
			auto *frame = new CellFrame(visual.style, m_runPage);
			item.widgetType = reg->widget(cell.widgetType);
			if (item.widgetType) {
				item.widgetSettings = settingsFor(item.widgetType.get(), cell.widgetSettings);
				item.typeWidget = item.widgetType->create(item.widgetSettings.get(), frame);
			}
			if (item.typeWidget) {
				frame->setChild(item.typeWidget);
			} else {
				item.widgetType.reset();
				auto *missing = new QLabel(T("Missing").arg(cell.widgetType), frame);
				missing->setAlignment(Qt::AlignCenter);
				missing->setWordWrap(true);
				missing->setEnabled(false);
				frame->setChild(missing);
			}
			item.widget = frame;
			break;
		}
		}
		item.widget->show();
		m_items.append(item);
	}

	layoutItems();
	if (isVisible())
		refreshStates();
}

void PanelView::layoutItems()
{
	const QSize size = m_runPage->size();
	m_placeholder->setGeometry(QRect(QPoint(), size).adjusted(8, 8, -8, -8));
	const grid::Metrics m{size, m_panel.rows, m_panel.cols, m_panel.theme.spacing};
	for (Item &item : m_items)
		item.widget->setGeometry(grid::pixelRect(m, item.area));
}

bool PanelView::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == m_runPage && event->type() == QEvent::Resize)
		layoutItems();
	return QWidget::eventFilter(watched, event);
}

void PanelView::refreshStates()
{
	if (!frontendReady())
		return;
	for (Item &item : m_items) {
		if (item.button && item.stateType)
			item.button->setState(item.stateType->state(item.stateSettings.get()));
	}
}

void PanelView::tick()
{
	if (!frontendReady())
		return;
	refreshStates();
	for (Item &item : m_items)
		if (item.typeWidget && item.widgetType)
			item.widgetType->tick(item.typeWidget, item.widgetSettings.get());
}

void PanelView::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	tick();
	m_pollTimer.start();
}

void PanelView::hideEvent(QHideEvent *event)
{
	QWidget::hideEvent(event);
	m_pollTimer.stop();
}

void PanelView::changeEvent(QEvent *event)
{
	QWidget::changeEvent(event);
	/* Unstyled cells use palette colors resolved at build time; follow
	 * OBS theme switches. */
	if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
		QMetaObject::invokeMethod(this, &PanelView::rebuild, Qt::QueuedConnection);
}

QString PanelView::cellIdAt(const QPoint &globalPos) const
{
	const QPoint local = m_runPage->mapFromGlobal(globalPos);
	for (const Item &item : m_items)
		if (item.widget->geometry().contains(local))
			return item.cellId;
	return {};
}

void PanelView::contextMenuEvent(QContextMenuEvent *event)
{
	if (m_editMode)
		return;
	Runtime *rt = Runtime::get();
	if (!rt)
		return;

	QMenu menu(this);
	const QString cellId = cellIdAt(event->globalPos());
	if (!m_panel.id.isEmpty()) {
		if (!cellId.isEmpty())
			menu.addAction(T("Editor.EditCell"), this, [this, cellId]() { editCell(cellId); });
		menu.addAction(T("View.EditLayout"), this, [this]() { setEditMode(true); });
	}
	menu.addAction(T("View.OpenEditor"), this, [this, rt]() { rt->openEditor(m_panelId); });

	if (m_showSwitchMenu) {
		QMenu *sw = menu.addMenu(T("View.SwitchTo"));
		for (const Panel &p : rt->store().panels()) {
			if (!p.showInSwitcher || !rt->isPanelActive(p))
				continue;
			QAction *a = sw->addAction(p.name, this, [rt, id = p.id]() { rt->switchToPanel(id); });
			a->setCheckable(true);
			a->setChecked(p.id == m_panelId);
		}
		sw->setEnabled(!sw->isEmpty());
	}
	menu.exec(event->globalPos());
}

void PanelView::modifyPanel(const std::function<bool(Panel &)> &fn)
{
	Runtime *rt = Runtime::get();
	const Panel *current = rt ? rt->store().panel(m_panelId) : nullptr;
	if (!current)
		return;
	Panel copy = *current;
	if (fn(copy))
		rt->store().updatePanel(copy); /* triggers rebuild via panelChanged */
}

void PanelView::editCell(const QString &cellId)
{
	const Cell *cell = m_panel.cell(cellId);
	if (!cell)
		return;
	CellEditorDialog dlg(*cell, this);
	if (dlg.exec() != QDialog::Accepted)
		return;
	const Cell edited = dlg.cell();
	modifyPanel([&](Panel &p) { return edit::applyCellEdit(p, edited); });
}

void PanelView::createCell(const QRect &area, CellKind kind)
{
	Cell cell = edit::makeCell(area, kind);
	CellEditorDialog dlg(cell, this);
	if (dlg.exec() != QDialog::Accepted)
		return;
	Cell created = dlg.cell();
	created.setArea(area);
	modifyPanel([&](Panel &p) {
		if (!grid::isFree(p, area, QString()))
			return false;
		p.cells.append(created);
		return true;
	});
	m_canvas->setSelectedCell(created.id);
}

} // namespace cp
