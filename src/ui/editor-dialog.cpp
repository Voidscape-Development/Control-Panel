/*
Control Panel for OBS Studio
Copyright (C) 2026 Voidscape Development

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "editor-dialog.hpp"
#include "cell-inspector.hpp"
#include "edit-ops.hpp"
#include "grid-canvas.hpp"
#include "properties-form.hpp"
#include "../core/runtime.hpp"
#include "../model/grid.hpp"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace cp {

namespace {
QPointer<EditorDialog> g_editor;

QToolButton *barButton(const QString &text, const QString &tip, QWidget *parent)
{
	auto *b = new QToolButton(parent);
	b->setText(text);
	b->setToolTip(tip);
	b->setAccessibleName(tip);
	return b;
}

QScrollArea *scrolled(QWidget *content, QWidget *parent)
{
	auto *scroll = new QScrollArea(parent);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(content);
	return scroll;
}
} // namespace

void EditorDialog::open(QWidget *parent, const QString &panelId)
{
	if (!g_editor) {
		g_editor = new EditorDialog(parent);
		g_editor->setAttribute(Qt::WA_DeleteOnClose);
	}
	if (!panelId.isEmpty()) {
		for (int i = 0; i < g_editor->m_working.size(); ++i)
			if (g_editor->m_working[i].id == panelId)
				g_editor->selectPanel(i);
	}
	g_editor->show();
	g_editor->raise();
	g_editor->activateWindow();
}

void EditorDialog::closeInstance()
{
	if (g_editor) {
		g_editor->m_dirty = false; /* shutting down: don't prompt */
		g_editor->close();
	}
}

EditorDialog::EditorDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(T("Editor.Title"));
	setWindowFlag(Qt::WindowMaximizeButtonHint);
	resize(1180, 680);

	auto *root = new QVBoxLayout(this);
	auto *split = new QSplitter(Qt::Horizontal, this);
	root->addWidget(split, 1);

	/* ---- left: panel list ---- */
	auto *left = new QWidget(split);
	auto *lv = new QVBoxLayout(left);
	lv->setContentsMargins(0, 0, 0, 0);
	lv->addWidget(new QLabel(T("Editor.Panels"), left));
	m_panelList = new QListWidget(left);
	m_panelList->setDragDropMode(QAbstractItemView::InternalMove);
	m_panelList->setAccessibleName(T("Editor.Panels"));
	lv->addWidget(m_panelList, 1);
	auto *lbar = new QHBoxLayout();
	auto *add = barButton(QStringLiteral("+"), T("Editor.AddPanel"), left);
	auto *dup = barButton(QStringLiteral("⧉"), T("Editor.DuplicatePanel"), left);
	auto *del = barButton(QStringLiteral("−"), T("Editor.RemovePanel"), left);
	auto *io = barButton(T("Editor.ImportExport"), T("Editor.ImportExport"), left);
	io->setPopupMode(QToolButton::InstantPopup);
	auto *ioMenu = new QMenu(io);
	ioMenu->addAction(T("Editor.Import"), this, &EditorDialog::importPanels);
	ioMenu->addAction(T("Editor.ExportSelected"), this, [this]() { exportPanels(false); });
	ioMenu->addAction(T("Editor.ExportAll"), this, [this]() { exportPanels(true); });
	ioMenu->addSeparator();
	QMenu *templates = ioMenu->addMenu(T("Editor.Templates"));
	templates->addAction(T("Template.StreamingEssentials"), this, [this]() {
		addTemplate(QStringLiteral(":/control-panel/templates/streaming-essentials.json"));
	});
	templates->addAction(T("Template.StreamingEssentialsMaterial"), this, [this]() {
		addTemplate(QStringLiteral(":/control-panel/templates/streaming-essentials-material.json"));
	});
	io->setMenu(ioMenu);
	lbar->addWidget(add);
	lbar->addWidget(dup);
	lbar->addWidget(del);
	lbar->addStretch();
	lbar->addWidget(io);
	lv->addLayout(lbar);

	/* ---- center: canvas ---- */
	auto *center = new QWidget(split);
	auto *cv = new QVBoxLayout(center);
	cv->setContentsMargins(0, 0, 0, 0);
	auto *cbar = new QHBoxLayout();
	m_rows = new QSpinBox(center);
	m_cols = new QSpinBox(center);
	for (QSpinBox *s : {m_rows, m_cols}) {
		s->setRange(1, kMaxGridSize);
		s->setKeyboardTracking(false);
	}
	cbar->addWidget(new QLabel(T("Editor.Rows"), center));
	cbar->addWidget(m_rows);
	cbar->addWidget(new QLabel(T("Editor.Cols"), center));
	cbar->addWidget(m_cols);
	cbar->addStretch();
	auto *hint = new QLabel(T("Editor.Hint"), center);
	hint->setEnabled(false);
	cbar->addWidget(hint);
	cv->addLayout(cbar);
	m_canvas = new GridCanvas(center);
	cv->addWidget(m_canvas, 1);

	/* ---- right: inspectors ---- */
	m_tabs = new QTabWidget(split);
	m_tabs->setMinimumWidth(380);
	m_inspector = new CellInspector();
	m_tabs->addTab(scrolled(m_inspector, m_tabs), T("Editor.CellTab"));

	auto *panelPage = new QWidget();
	auto *pv = new QVBoxLayout(panelPage);
	auto *general = new QGroupBox(T("Editor.PanelGeneral"), panelPage);
	auto *gform = new QFormLayout(general);
	m_name = new QLineEdit(general);
	m_collection = new QComboBox(general);
	m_collection->setEditable(true);
	m_collection->setToolTip(T("Editor.CollectionTip"));
	m_showAsDock = new QCheckBox(T("Editor.ShowAsDock"), general);
	m_showInSwitcher = new QCheckBox(T("Editor.ShowInSwitcher"), general);
	gform->addRow(T("Editor.Name"), m_name);
	gform->addRow(T("Editor.Collection"), m_collection);
	gform->addRow(QString(), m_showAsDock);
	gform->addRow(QString(), m_showInSwitcher);
	pv->addWidget(general);

	auto *theme = new QGroupBox(T("Editor.Theme"), panelPage);
	auto *tform = new QFormLayout(theme);
	m_spacing = new QSpinBox(theme);
	m_spacing->setRange(0, kMaxSpacing);
	m_spacing->setSuffix(QStringLiteral(" px"));
	m_radius = new QSpinBox(theme);
	m_radius->setRange(0, 64);
	m_radius->setSuffix(QStringLiteral(" px"));
	m_themeBg = new ColorButton(theme);
	m_themeFg = new ColorButton(theme);
	m_themeActiveBg = new ColorButton(theme);
	m_themeActiveFg = new ColorButton(theme);
	m_themeFont = new QSpinBox(theme);
	m_themeFont->setRange(0, kMaxFontSize);
	m_themeFont->setSpecialValueText(T("Editor.AutoFit"));
	m_themeFont->setSuffix(QStringLiteral(" pt"));
	tform->addRow(T("Editor.Spacing"), m_spacing);
	tform->addRow(T("Editor.Radius"), m_radius);
	tform->addRow(T("Style.Background"), m_themeBg);
	tform->addRow(T("Style.Text"), m_themeFg);
	tform->addRow(T("Style.ActiveBackground"), m_themeActiveBg);
	tform->addRow(T("Style.ActiveText"), m_themeActiveFg);
	tform->addRow(T("Style.FontSize"), m_themeFont);
	pv->addWidget(theme);

	auto *hotkeyInfo = new QLabel(T("Editor.HotkeyInfo"), panelPage);
	hotkeyInfo->setWordWrap(true);
	hotkeyInfo->setEnabled(false);
	pv->addWidget(hotkeyInfo);
	pv->addStretch();
	guardWheel(panelPage);
	m_tabs->addTab(scrolled(panelPage, m_tabs), T("Editor.PanelTab"));

	split->setStretchFactor(0, 0);
	split->setStretchFactor(1, 1);
	split->setStretchFactor(2, 0);
	split->setCollapsible(1, false);
	split->setSizes({180, 520, 400});

	m_buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
	root->addWidget(m_buttons);

	/* ---- wiring ---- */
	connect(m_buttons, &QDialogButtonBox::accepted, this, [this]() {
		if (apply())
			accept();
	});
	connect(m_buttons, &QDialogButtonBox::rejected, this, &EditorDialog::reject);
	connect(m_buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &EditorDialog::apply);

	connect(add, &QToolButton::clicked, this, &EditorDialog::addPanel);
	connect(dup, &QToolButton::clicked, this, &EditorDialog::duplicatePanel);
	connect(del, &QToolButton::clicked, this, &EditorDialog::removePanel);
	connect(m_panelList, &QListWidget::currentRowChanged, this, [this](int row) {
		if (!m_loading)
			selectPanel(row);
	});
	connect(m_panelList->model(), &QAbstractItemModel::rowsMoved, this, [this]() {
		/* Reorder the working copy to match the list (switcher order). */
		QList<Panel> reordered;
		for (int i = 0; i < m_panelList->count(); ++i) {
			const QString id = m_panelList->item(i)->data(Qt::UserRole).toString();
			for (const Panel &p : m_working)
				if (p.id == id)
					reordered.append(p);
		}
		const QString currentId = current() ? current()->id : QString();
		m_working = reordered;
		for (int i = 0; i < m_working.size(); ++i)
			if (m_working[i].id == currentId)
				m_current = i;
		setDirty(true);
	});

	auto onGridSize = [this]() {
		Panel *p = current();
		if (!p || (m_rows->value() == p->rows && m_cols->value() == p->cols))
			return;
		if (edit::resizeGrid(this, *p, m_rows->value(), m_cols->value()))
			panelEdited();
		QSignalBlocker b1(m_rows), b2(m_cols);
		m_rows->setValue(p->rows);
		m_cols->setValue(p->cols);
	};
	connect(m_rows, &QSpinBox::valueChanged, this, onGridSize);
	connect(m_cols, &QSpinBox::valueChanged, this, onGridSize);

	connect(m_canvas, &GridCanvas::selectionChanged, this, &EditorDialog::selectCell);
	connect(m_canvas, &GridCanvas::createRequested, this, &EditorDialog::createCell);
	connect(m_canvas, &GridCanvas::geometryRequested, this, [this](const QString &id, const QRect &area) {
		Panel *p = current();
		Cell *c = p ? p->cell(id) : nullptr;
		if (c && grid::isFree(*p, area, id)) {
			c->setArea(area);
			panelEdited();
		}
	});
	connect(m_canvas, &GridCanvas::editRequested, this, [this](const QString &id) {
		selectCell(id);
		m_canvas->setSelectedCell(id);
		m_tabs->setCurrentIndex(0);
	});
	connect(m_canvas, &GridCanvas::deleteRequested, this, [this](const QString &id) {
		if (Panel *p = current(); p && edit::removeCell(*p, id)) {
			panelEdited();
			selectCell(QString());
		}
	});
	connect(m_canvas, &GridCanvas::duplicateRequested, this, [this](const QString &id) {
		Panel *p = current();
		if (!p)
			return;
		const QString newId = edit::duplicateCell(*p, id);
		if (newId.isEmpty()) {
			QMessageBox::information(this, T("Editor.Title"), T("Editor.NoRoom"));
			return;
		}
		panelEdited();
		m_canvas->setSelectedCell(newId);
		selectCell(newId);
	});
	connect(m_inspector, &CellInspector::cellChanged, this, [this](const Cell &cell) {
		if (Panel *p = current(); p && edit::applyCellEdit(*p, cell))
			panelEdited();
	});

	/* Panel settings. */
	connect(m_name, &QLineEdit::textEdited, this, [this](const QString &t) {
		if (Panel *p = current()) {
			p->name = t;
			if (QListWidgetItem *item = m_panelList->item(m_current))
				item->setText(t);
			panelEdited(false);
		}
	});
	connect(m_collection, &QComboBox::currentTextChanged, this, [this]() {
		Panel *p = current();
		if (!p || m_loading)
			return;
		const int idx = m_collection->currentIndex();
		const bool isAll = idx == 0 && m_collection->currentText() == m_collection->itemText(0);
		p->collection = isAll ? QString() : m_collection->currentText().trimmed();
		panelEdited(false);
	});
	connect(m_showAsDock, &QCheckBox::toggled, this, [this](bool v) {
		if (Panel *p = current(); p && !m_loading) {
			p->showAsDock = v;
			panelEdited(false);
		}
	});
	connect(m_showInSwitcher, &QCheckBox::toggled, this, [this](bool v) {
		if (Panel *p = current(); p && !m_loading) {
			p->showInSwitcher = v;
			panelEdited(false);
		}
	});
	connect(m_spacing, &QSpinBox::valueChanged, this, [this](int v) {
		if (Panel *p = current(); p && !m_loading) {
			p->theme.spacing = v;
			panelEdited();
		}
	});
	connect(m_radius, &QSpinBox::valueChanged, this, [this](int v) {
		if (Panel *p = current(); p && !m_loading) {
			p->theme.cornerRadius = v;
			panelEdited();
		}
	});
	auto bindThemeColor = [this](ColorButton *b, QColor Style::*field) {
		connect(b, &ColorButton::colorChanged, this, [this, field](const QColor &c) {
			if (Panel *p = current(); p && !m_loading) {
				p->theme.style.*field = c;
				panelEdited();
			}
		});
	};
	bindThemeColor(m_themeBg, &Style::background);
	bindThemeColor(m_themeFg, &Style::text);
	bindThemeColor(m_themeActiveBg, &Style::activeBackground);
	bindThemeColor(m_themeActiveFg, &Style::activeText);
	connect(m_themeFont, &QSpinBox::valueChanged, this, [this](int v) {
		if (Panel *p = current(); p && !m_loading) {
			p->theme.style.fontSize = v;
			panelEdited();
		}
	});

	/* Changes made elsewhere (a dock's edit mode, renames) while we have no
	 * unsaved edits: follow them. */
	if (Runtime *rt = Runtime::get()) {
		auto follow = [this]() {
			if (!m_dirty)
				loadFromStore();
		};
		connect(&rt->store(), &PanelStore::panelsReset, this, follow);
		connect(&rt->store(), &PanelStore::panelChanged, this, follow);
	}

	loadFromStore();
}

Panel *EditorDialog::current()
{
	return m_current >= 0 && m_current < m_working.size() ? &m_working[m_current] : nullptr;
}

void EditorDialog::loadFromStore()
{
	Runtime *rt = Runtime::get();
	const QString currentId = current() ? current()->id : QString();
	const QString cellId = m_canvas->selectedCell();
	m_working = rt ? rt->store().panels() : QList<Panel>();
	refreshPanelList();

	int idx = m_working.isEmpty() ? -1 : 0;
	for (int i = 0; i < m_working.size(); ++i)
		if (m_working[i].id == currentId)
			idx = i;
	m_current = -1;
	selectPanel(idx);
	if (current() && current()->cell(cellId)) {
		m_canvas->setSelectedCell(cellId);
		selectCell(cellId);
	}
	setDirty(false);
}

void EditorDialog::refreshPanelList()
{
	m_loading = true;
	m_panelList->clear();
	for (const Panel &p : m_working) {
		auto *item = new QListWidgetItem(p.name, m_panelList);
		item->setData(Qt::UserRole, p.id);
	}
	m_loading = false;
}

void EditorDialog::selectPanel(int index)
{
	m_current = index;
	m_loading = true;
	m_panelList->setCurrentRow(index);
	m_loading = false;

	Panel *p = current();
	m_canvas->setEnabled(p != nullptr);
	m_rows->setEnabled(p != nullptr);
	m_cols->setEnabled(p != nullptr);
	m_tabs->setEnabled(p != nullptr);

	QHash<QString, QString> widgetNames;
	if (TypeRegistry *reg = TypeRegistry::instance())
		for (const WidgetTypePtr &w : reg->widgets())
			widgetNames.insert(w->id(), w->name());
	m_canvas->setWidgetTypeNames(widgetNames);
	m_canvas->setPanel(p ? *p : Panel());
	m_canvas->setSelectedCell(QString());
	m_inspector->clearCell();
	loadPanelSettings();
}

void EditorDialog::loadPanelSettings()
{
	Panel *p = current();
	m_loading = true;
	QSignalBlocker b1(m_rows), b2(m_cols);
	m_rows->setValue(p ? p->rows : 1);
	m_cols->setValue(p ? p->cols : 1);
	m_name->setText(p ? p->name : QString());

	m_collection->clear();
	m_collection->addItem(T("Editor.AllCollections"));
	m_collection->addItems(sceneCollections());
	if (p && !p->collection.isEmpty()) {
		if (m_collection->findText(p->collection) < 0)
			m_collection->addItem(p->collection);
		m_collection->setCurrentText(p->collection);
	} else {
		m_collection->setCurrentIndex(0);
	}

	m_showAsDock->setChecked(p ? p->showAsDock : true);
	m_showInSwitcher->setChecked(p ? p->showInSwitcher : true);
	m_spacing->setValue(p ? p->theme.spacing : 4);
	m_radius->setValue(p ? p->theme.cornerRadius : 4);
	const Style s = p ? p->theme.style : Style();
	m_themeBg->setColor(s.background);
	m_themeFg->setColor(s.text);
	m_themeActiveBg->setColor(s.activeBackground);
	m_themeActiveFg->setColor(s.activeText);
	m_themeFont->setValue(s.fontSize);
	m_loading = false;
}

void EditorDialog::panelEdited(bool refreshCanvas)
{
	if (refreshCanvas)
		if (Panel *p = current())
			m_canvas->setPanel(*p);
	setDirty(true);
}

void EditorDialog::setDirty(bool dirty)
{
	m_dirty = dirty;
	m_buttons->button(QDialogButtonBox::Apply)->setEnabled(dirty);
	setWindowTitle(dirty ? T("Editor.Title") + QStringLiteral(" *") : T("Editor.Title"));
}

bool EditorDialog::apply()
{
	Runtime *rt = Runtime::get();
	if (!rt)
		return false;
	if (!m_dirty)
		return true;
	for (Panel &p : m_working)
		if (p.name.trimmed().isEmpty())
			p.name = T("Editor.Untitled");
	/* Avoid re-entering loadFromStore while we push our own state. */
	m_dirty = false;
	rt->store().setPanels(m_working);
	loadFromStore();
	return true;
}

bool EditorDialog::confirmDiscard()
{
	if (!m_dirty)
		return true;
	const auto r = QMessageBox::question(this, T("Editor.Title"), T("Editor.Unsaved"),
					     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
	if (r == QMessageBox::Save)
		return apply();
	return r == QMessageBox::Discard;
}

void EditorDialog::reject()
{
	if (confirmDiscard())
		QDialog::reject();
}

void EditorDialog::closeEvent(QCloseEvent *event)
{
	if (event->spontaneous()) {
		/* The user closed the window: they can still change their mind. */
		if (confirmDiscard())
			event->accept();
		else
			event->ignore();
		return;
	}

	/* Closed programmatically, i.e. OBS is shutting down. Cancelling can't
	 * stop that, so only offer Save or Discard, and always close. */
	if (m_dirty &&
	    QMessageBox::question(this, T("Editor.Title"), T("Editor.UnsavedOnExit"),
				  QMessageBox::Save | QMessageBox::Discard, QMessageBox::Save) == QMessageBox::Save)
		apply();
	m_dirty = false;
	event->accept();
}

/* ---- panel list actions --------------------------------------------- */

void EditorDialog::addPanel()
{
	Panel p;
	p.id = newId();
	p.name = PanelStore::uniqueName(T("Editor.NewPanel"), m_working);
	m_working.append(p);
	refreshPanelList();
	selectPanel(m_working.size() - 1);
	setDirty(true);
	m_tabs->setCurrentIndex(1);
	m_name->setFocus();
	m_name->selectAll();
}

void EditorDialog::duplicatePanel()
{
	Panel *src = current();
	if (!src)
		return;
	Panel copy = *src;
	copy.id = newId();
	copy.hotkeys = QJsonObject();
	for (Cell &c : copy.cells)
		c.id = newId();
	copy.name = PanelStore::uniqueName(src->name, m_working);
	m_working.insert(m_current + 1, copy);
	refreshPanelList();
	selectPanel(m_current + 1);
	setDirty(true);
}

void EditorDialog::removePanel()
{
	Panel *p = current();
	if (!p)
		return;
	if (QMessageBox::question(this, T("Editor.RemovePanel"), T("Editor.RemoveConfirm").arg(p->name)) !=
	    QMessageBox::Yes)
		return;
	m_working.removeAt(m_current);
	refreshPanelList();
	selectPanel(std::min(m_current, int(m_working.size()) - 1));
	setDirty(true);
}

void EditorDialog::importPanels()
{
	const QString path = QFileDialog::getOpenFileName(this, T("Editor.Import"), QString(), T("Editor.FileFilter"));
	if (path.isEmpty())
		return;
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(this, T("Editor.Import"), f.errorString());
		return;
	}
	QString error;
	const QList<Panel> parsed = PanelStore::parsePanels(f.readAll(), &error);
	if (parsed.isEmpty()) {
		QMessageBox::warning(this, T("Editor.Import"), T("Editor.ImportFailed").arg(error));
		return;
	}
	const QList<Panel> prepared = PanelStore::prepareImport(parsed, m_working);
	m_working.append(prepared);
	refreshPanelList();
	selectPanel(m_working.size() - prepared.size());
	setDirty(true);
}

void EditorDialog::addTemplate(const QString &resource)
{
	QFile f(resource);
	if (!f.open(QIODevice::ReadOnly))
		return;
	QString error;
	const QList<Panel> parsed = PanelStore::parsePanels(f.readAll(), &error);
	if (parsed.isEmpty())
		return;
	/* Fresh ids every time, so a template can be added more than once. */
	const QList<Panel> prepared = PanelStore::prepareImport(parsed, m_working);
	m_working.append(prepared);
	refreshPanelList();
	selectPanel(m_working.size() - prepared.size());
	setDirty(true);
}

void EditorDialog::exportPanels(bool all)
{
	QList<Panel> panels;
	if (all)
		panels = m_working;
	else if (Panel *p = current())
		panels.append(*p);
	if (panels.isEmpty())
		return;

	const QString suggested =
		(all ? QStringLiteral("control-panels") : panels.first().name) + QStringLiteral(".json");
	const QString path = QFileDialog::getSaveFileName(
		this, all ? T("Editor.ExportAll") : T("Editor.ExportSelected"), suggested, T("Editor.FileFilter"));
	if (path.isEmpty())
		return;
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly) || f.write(PanelStore::exportPanels(panels)) < 0)
		QMessageBox::warning(this, T("Editor.Export"), f.errorString());
}

/* ---- cells ---------------------------------------------------------- */

void EditorDialog::createCell(const QRect &area, CellKind kind)
{
	Panel *p = current();
	if (!p || !grid::isFree(*p, area, QString()))
		return;
	Cell cell = edit::makeCell(area, kind);
	p->cells.append(cell);
	panelEdited();
	m_canvas->setSelectedCell(cell.id);
	selectCell(cell.id);
}

void EditorDialog::selectCell(const QString &id)
{
	Panel *p = current();
	const Cell *cell = p ? p->cell(id) : nullptr;
	if (!cell) {
		m_inspector->clearCell();
		return;
	}
	m_inspector->setCell(*cell);
	m_tabs->setCurrentIndex(0);
}

} // namespace cp
