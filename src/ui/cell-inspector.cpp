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

#include "cell-inspector.hpp"
#include "icon-picker.hpp"
#include "icons.hpp"
#include "properties-form.hpp"
#include "../core/type-registry.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QScrollArea>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace cp {

namespace {

QToolButton *toolButton(const QString &text, const QString &tip, QWidget *parent)
{
	auto *b = new QToolButton(parent);
	b->setText(text);
	b->setToolTip(tip);
	b->setAccessibleName(tip);
	return b;
}

} // namespace

CellInspector::CellInspector(QWidget *parent) : QWidget(parent)
{
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);

	m_empty = new QLabel(T("Inspector.NoSelection"), this);
	m_empty->setAlignment(Qt::AlignCenter);
	m_empty->setWordWrap(true);
	m_empty->setEnabled(false);
	outer->addWidget(m_empty);

	m_body = new QWidget(this);
	outer->addWidget(m_body);
	auto *layout = new QVBoxLayout(m_body);
	layout->setContentsMargins(0, 0, 0, 0);

	/* ---- general ---- */
	auto *general = new QGroupBox(T("Inspector.Cell"), m_body);
	auto *gform = new QFormLayout(general);

	m_kind = new QComboBox(general);
	gform->addRow(T("Inspector.Kind"), m_kind);

	auto *labelRow = new QWidget(general);
	auto *lh = new QHBoxLayout(labelRow);
	lh->setContentsMargins(0, 0, 0, 0);
	m_label = new QLineEdit(labelRow);
	m_suggest = toolButton(QStringLiteral("↺"), T("Inspector.SuggestLabel"), labelRow);
	lh->addWidget(m_label, 1);
	lh->addWidget(m_suggest);
	gform->addRow(T("Inspector.Label"), labelRow);

	m_showLabel = new QCheckBox(T("Inspector.ShowLabel"), general);
	gform->addRow(QString(), m_showLabel);

	/* Icon: opens the searchable picker (Material Symbols, built-ins, file).
	 * The small clear button removes the icon without opening it. */
	auto *iconRow = new QWidget(general);
	auto *ih = new QHBoxLayout(iconRow);
	ih->setContentsMargins(0, 0, 0, 0);
	m_icon = new QToolButton(iconRow);
	m_icon->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_icon->setIconSize(QSize(20, 20));
	m_icon->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_icon->setAccessibleName(T("Inspector.Icon"));
	auto *clearIcon = toolButton(QStringLiteral("✕"), T("Inspector.ClearIcon"), iconRow);
	ih->addWidget(m_icon, 1);
	ih->addWidget(clearIcon);
	gform->addRow(T("Inspector.Icon"), iconRow);

	connect(m_icon, &QToolButton::clicked, this, [this]() {
		IconPickerDialog dlg(m_cell.iconPath, this);
		if (dlg.exec() == QDialog::Accepted)
			setIcon(dlg.selectedRef());
	});
	connect(clearIcon, &QToolButton::clicked, this, [this]() { setIcon(QString()); });
	layout->addWidget(general);

	/* ---- style ---- */
	auto *styleBox = new QGroupBox(T("Inspector.Style"), m_body);
	auto *sform = new QFormLayout(styleBox);
	m_bg = new ColorButton(styleBox);
	m_fg = new ColorButton(styleBox);
	m_activeBg = new ColorButton(styleBox);
	m_activeFg = new ColorButton(styleBox);
	m_fontSize = new QSpinBox(styleBox);
	m_fontSize->setRange(0, kMaxFontSize);
	m_fontSize->setSpecialValueText(T("Inspector.Inherit"));
	m_fontSize->setSuffix(QStringLiteral(" pt"));
	sform->addRow(T("Style.Background"), m_bg);
	sform->addRow(T("Style.Text"), m_fg);
	sform->addRow(T("Style.ActiveBackground"), m_activeBg);
	sform->addRow(T("Style.ActiveText"), m_activeFg);
	sform->addRow(T("Style.FontSize"), m_fontSize);
	layout->addWidget(styleBox);

	/* ---- actions ---- */
	m_actionsBox = new QGroupBox(T("Inspector.Actions"), m_body);
	auto *av = new QVBoxLayout(m_actionsBox);
	m_actions = new QListWidget(m_actionsBox);
	m_actions->setMaximumHeight(130);
	av->addWidget(m_actions);

	auto *bar = new QHBoxLayout();
	m_addAction =
		toolButton(QStringLiteral("+ ") + T("Inspector.AddAction"), T("Inspector.AddAction"), m_actionsBox);
	m_addAction->setPopupMode(QToolButton::InstantPopup);
	m_addAction->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_removeAction = toolButton(QStringLiteral("−"), T("Inspector.RemoveAction"), m_actionsBox);
	m_upAction = toolButton(QStringLiteral("▲"), T("Inspector.MoveUp"), m_actionsBox);
	m_downAction = toolButton(QStringLiteral("▼"), T("Inspector.MoveDown"), m_actionsBox);
	bar->addWidget(m_addAction);
	bar->addStretch();
	bar->addWidget(m_upAction);
	bar->addWidget(m_downAction);
	bar->addWidget(m_removeAction);
	av->addLayout(bar);

	m_delayRow = new QWidget(m_actionsBox);
	auto *dform = new QFormLayout(m_delayRow);
	dform->setContentsMargins(0, 0, 0, 0);
	m_delay = new QSpinBox(m_delayRow);
	m_delay->setRange(0, 10 * 60 * 1000);
	m_delay->setSingleStep(100);
	m_delay->setSuffix(QStringLiteral(" ms"));
	m_delay->setToolTip(T("Inspector.DelayTip"));
	dform->addRow(T("Inspector.Delay"), m_delay);
	av->addWidget(m_delayRow);

	m_actionForm = new PropertiesForm(m_actionsBox);
	av->addWidget(m_actionForm);

	auto *iform = new QFormLayout();
	m_indicator = new QComboBox(m_actionsBox);
	m_indicator->setToolTip(T("Inspector.IndicatorTip"));
	iform->addRow(T("Inspector.Indicator"), m_indicator);
	av->addLayout(iform);
	layout->addWidget(m_actionsBox);

	/* ---- widget ---- */
	m_widgetBox = new QGroupBox(T("Inspector.WidgetSettings"), m_body);
	auto *wv = new QVBoxLayout(m_widgetBox);
	m_widgetForm = new PropertiesForm(m_widgetBox);
	wv->addWidget(m_widgetForm);
	layout->addWidget(m_widgetBox);
	layout->addStretch();

	/* ---- wiring ---- */
	connect(m_kind, &QComboBox::activated, this, [this]() {
		const QString data = m_kind->currentData().toString();
		if (data == QLatin1String("button")) {
			m_cell.kind = CellKind::Button;
		} else if (data == QLatin1String("label")) {
			m_cell.kind = CellKind::Label;
		} else {
			const QString type = data.mid(int(strlen("widget:")));
			if (m_cell.kind != CellKind::Widget || m_cell.widgetType != type)
				m_cell.widgetSettings = QJsonObject();
			m_cell.kind = CellKind::Widget;
			m_cell.widgetType = type;
		}
		loadFromCell();
		commit();
	});
	connect(m_label, &QLineEdit::textEdited, this, [this](const QString &t) {
		m_cell.label = t;
		m_labelAuto = t.isEmpty();
		commit();
	});
	connect(m_suggest, &QToolButton::clicked, this, [this]() { applySuggestedLabel(true); });
	connect(m_showLabel, &QCheckBox::toggled, this, [this](bool v) {
		m_cell.showLabel = v;
		commit();
	});
	auto bindColor = [this](ColorButton *b, QColor Style::*field) {
		connect(b, &ColorButton::colorChanged, this, [this, field](const QColor &c) {
			if (m_loading)
				return;
			m_cell.style.*field = c;
			commit();
		});
	};
	bindColor(m_bg, &Style::background);
	bindColor(m_fg, &Style::text);
	bindColor(m_activeBg, &Style::activeBackground);
	bindColor(m_activeFg, &Style::activeText);
	connect(m_fontSize, &QSpinBox::valueChanged, this, [this](int v) {
		if (m_loading)
			return;
		m_cell.style.fontSize = v;
		commit();
	});

	connect(m_actions, &QListWidget::currentRowChanged, this, [this](int row) {
		if (!m_loading)
			selectAction(row);
	});
	connect(m_removeAction, &QToolButton::clicked, this, &CellInspector::removeAction);
	connect(m_upAction, &QToolButton::clicked, this, [this]() { moveAction(-1); });
	connect(m_downAction, &QToolButton::clicked, this, [this]() { moveAction(1); });
	connect(m_delay, &QSpinBox::valueChanged, this, [this](int v) {
		if (m_loading || m_currentAction < 0 || m_currentAction >= m_cell.actions.size())
			return;
		m_cell.actions[m_currentAction].delayMs = v;
		m_actions->item(m_currentAction)->setText(actionText(m_currentAction));
		commit();
	});
	connect(m_actionForm, &PropertiesForm::changed, this, &CellInspector::actionSettingsChanged);
	connect(m_indicator, &QComboBox::activated, this, [this]() {
		m_cell.indicator = m_indicator->currentData().toInt();
		commit();
	});
	connect(m_widgetForm, &PropertiesForm::changed, this, [this]() {
		m_cell.widgetSettings = dataToJson(m_widgetForm->settings());
		commit();
	});

	/* Rebuild the add-action menu and kind list when plugins come and go. */
	if (TypeRegistry *reg = TypeRegistry::instance())
		connect(reg, &TypeRegistry::typesChanged, this, [this]() {
			if (m_hasCell)
				loadFromCell();
		});

	guardWheel(this);
	clearCell();
}

void CellInspector::clearCell()
{
	m_hasCell = false;
	m_cell = Cell();
	m_actionForm->clear();
	m_widgetForm->clear();
	m_body->hide();
	m_empty->show();
}

void CellInspector::setCell(const Cell &cell)
{
	const bool sameCell = m_hasCell && cell.id == m_cell.id;
	m_cell = cell;
	m_hasCell = true;
	if (!sameCell) {
		m_currentAction = cell.actions.isEmpty() ? -1 : 0;
		m_labelAuto = cell.label.isEmpty() || cell.label == suggestedLabel();
	}
	loadFromCell();
}

void CellInspector::commit()
{
	if (!m_loading && m_hasCell)
		emit cellChanged(m_cell);
}

void CellInspector::rebuildKindCombo()
{
	m_kind->clear();
	m_kind->addItem(T("Kind.Button"), QStringLiteral("button"));
	m_kind->addItem(T("Kind.Label"), QStringLiteral("label"));
	if (TypeRegistry *reg = TypeRegistry::instance()) {
		m_kind->insertSeparator(m_kind->count());
		for (const WidgetTypePtr &w : reg->widgets())
			m_kind->addItem(w->name(), QStringLiteral("widget:") + w->id());
	}
	QString current = cellKindToString(m_cell.kind);
	if (m_cell.kind == CellKind::Widget) {
		current = QStringLiteral("widget:") + m_cell.widgetType;
		if (m_kind->findData(current) < 0)
			m_kind->addItem(T("Missing").arg(m_cell.widgetType), current);
	}
	m_kind->setCurrentIndex(m_kind->findData(current));
}

void CellInspector::loadFromCell()
{
	m_loading = true;
	m_empty->hide();
	m_body->show();

	rebuildKindCombo();
	const bool button = m_cell.kind == CellKind::Button;
	const bool widget = m_cell.kind == CellKind::Widget;

	m_label->setText(m_cell.label);
	m_suggest->setVisible(button);
	m_showLabel->setChecked(m_cell.showLabel);
	m_showLabel->setVisible(button);
	refreshIconButton();
	m_icon->setEnabled(button);

	m_bg->setColor(m_cell.style.background);
	m_fg->setColor(m_cell.style.text);
	m_activeBg->setColor(m_cell.style.activeBackground);
	m_activeFg->setColor(m_cell.style.activeText);
	m_activeBg->setEnabled(button);
	m_activeFg->setEnabled(button);
	m_fontSize->setValue(m_cell.style.fontSize);

	/* Add-action menu, grouped by category. */
	auto *menu = new QMenu(m_addAction);
	if (TypeRegistry *reg = TypeRegistry::instance()) {
		QString category;
		QMenu *sub = nullptr;
		for (const ActionTypePtr &a : reg->actions()) {
			if (a->category() != category || !sub) {
				category = a->category();
				sub = menu->addMenu(category);
			}
			const QString id = a->id();
			sub->addAction(a->name(), this, [this, id]() { addAction(id); });
		}
	}
	delete m_addAction->menu();
	m_addAction->setMenu(menu);

	m_actionsBox->setVisible(button);
	m_widgetBox->setVisible(widget);
	m_loading = false;

	if (button) {
		refreshActionList();
		selectAction(m_currentAction);
		refreshIndicatorCombo();
	} else {
		m_actionForm->clear();
	}

	if (widget) {
		TypeRegistry *reg = TypeRegistry::instance();
		WidgetTypePtr type = reg ? reg->widget(m_cell.widgetType) : nullptr;
		if (type) {
			m_widgetForm->setSource([type](obs_data_t *s) { return type->properties(s); },
						settingsFor(type.get(), m_cell.widgetSettings));
		} else {
			m_widgetForm->clear();
		}
	} else {
		m_widgetForm->clear();
	}
}

void CellInspector::setIcon(const QString &ref)
{
	m_cell.iconPath = ref;
	refreshIconButton();
	commit();
}

void CellInspector::refreshIconButton()
{
	const QString ref = m_cell.iconPath;
	m_icon->setText(ref.isEmpty() ? T("Inspector.NoIcon") : icons::displayName(ref));
	m_icon->setIcon(ref.isEmpty() ? QIcon() : icons::menuIcon(ref, palette().color(QPalette::ButtonText)));
	m_icon->setToolTip(IconRef::parse(ref).kind == IconRef::Kind::File ? ref : QString());
}

QString CellInspector::actionText(int i) const
{
	const Action &a = m_cell.actions[i];
	TypeRegistry *reg = TypeRegistry::instance();
	ActionTypePtr type = reg ? reg->action(a.type) : nullptr;
	QString text = QStringLiteral("%1. ").arg(i + 1);
	if (!type)
		return text + T("Missing").arg(a.type);
	text += type->name();
	const QString detail = type->defaultLabel(settingsFor(type.get(), a.settings).get());
	if (!detail.isEmpty() && detail != type->name())
		text += QStringLiteral(" — ") + detail;
	if (a.delayMs > 0)
		text = QStringLiteral("⏱ %1 ms  ").arg(a.delayMs) + text;
	return text;
}

void CellInspector::refreshActionList()
{
	m_loading = true;
	m_actions->clear();
	for (int i = 0; i < m_cell.actions.size(); ++i)
		m_actions->addItem(actionText(i));
	if (m_currentAction >= m_cell.actions.size())
		m_currentAction = m_cell.actions.size() - 1;
	m_actions->setCurrentRow(m_currentAction);
	m_loading = false;

	m_removeAction->setEnabled(m_currentAction >= 0);
	m_upAction->setEnabled(m_currentAction > 0);
	m_downAction->setEnabled(m_currentAction >= 0 && m_currentAction < m_cell.actions.size() - 1);
}

void CellInspector::selectAction(int index)
{
	m_currentAction = index;
	m_removeAction->setEnabled(index >= 0);
	m_upAction->setEnabled(index > 0);
	m_downAction->setEnabled(index >= 0 && index < m_cell.actions.size() - 1);

	if (index < 0 || index >= m_cell.actions.size()) {
		m_delayRow->hide();
		m_actionForm->clear();
		return;
	}
	m_loading = true;
	m_delayRow->show();
	m_delay->setValue(m_cell.actions[index].delayMs);
	m_loading = false;

	TypeRegistry *reg = TypeRegistry::instance();
	ActionTypePtr type = reg ? reg->action(m_cell.actions[index].type) : nullptr;
	if (!type) {
		m_actionForm->clear();
		return;
	}
	m_actionForm->setSource([type](obs_data_t *s) { return type->properties(s); },
				settingsFor(type.get(), m_cell.actions[index].settings));
}

void CellInspector::addAction(const QString &typeId)
{
	Action a;
	a.type = typeId;
	m_cell.actions.append(a);
	m_currentAction = m_cell.actions.size() - 1;
	refreshActionList();
	selectAction(m_currentAction);
	refreshIndicatorCombo();
	applySuggestedLabel(false);
	commit();
}

void CellInspector::moveAction(int delta)
{
	const int from = m_currentAction;
	const int to = from + delta;
	if (from < 0 || to < 0 || to >= m_cell.actions.size())
		return;
	m_cell.actions.swapItemsAt(from, to);
	if (m_cell.indicator == from)
		m_cell.indicator = to;
	else if (m_cell.indicator == to)
		m_cell.indicator = from;
	m_currentAction = to;
	refreshActionList();
	refreshIndicatorCombo();
	commit();
}

void CellInspector::removeAction()
{
	const int i = m_currentAction;
	if (i < 0 || i >= m_cell.actions.size())
		return;
	m_cell.actions.removeAt(i);
	if (m_cell.indicator == i)
		m_cell.indicator = kIndicatorAuto;
	else if (m_cell.indicator > i)
		--m_cell.indicator;
	m_currentAction = std::min(i, int(m_cell.actions.size()) - 1);
	refreshActionList();
	selectAction(m_currentAction);
	refreshIndicatorCombo();
	commit();
}

void CellInspector::actionSettingsChanged()
{
	const int i = m_currentAction;
	if (i < 0 || i >= m_cell.actions.size() || !m_actionForm->settings())
		return;
	m_cell.actions[i].settings = dataToJson(m_actionForm->settings());
	if (QListWidgetItem *item = m_actions->item(i))
		item->setText(actionText(i));
	applySuggestedLabel(false);
	commit();
}

void CellInspector::refreshIndicatorCombo()
{
	m_indicator->clear();
	m_indicator->addItem(T("Inspector.IndicatorAuto"), kIndicatorAuto);
	m_indicator->addItem(T("Inspector.IndicatorNone"), kIndicatorNone);
	TypeRegistry *reg = TypeRegistry::instance();
	for (int i = 0; i < m_cell.actions.size(); ++i) {
		ActionTypePtr type = reg ? reg->action(m_cell.actions[i].type) : nullptr;
		if (type && type->hasState())
			m_indicator->addItem(actionText(i), i);
	}
	int idx = m_indicator->findData(m_cell.indicator);
	if (idx < 0) {
		m_cell.indicator = kIndicatorAuto;
		idx = 0;
	}
	m_indicator->setCurrentIndex(idx);
}

QString CellInspector::suggestedLabel() const
{
	TypeRegistry *reg = TypeRegistry::instance();
	if (!reg)
		return {};
	for (const Action &a : m_cell.actions) {
		ActionTypePtr type = reg->action(a.type);
		if (!type)
			continue;
		const QString s = type->defaultLabel(settingsFor(type.get(), a.settings).get());
		if (!s.isEmpty())
			return s;
	}
	return {};
}

void CellInspector::applySuggestedLabel(bool force)
{
	if (m_cell.kind != CellKind::Button || (!force && !m_labelAuto))
		return;
	const QString s = suggestedLabel();
	if (s.isEmpty() || s == m_cell.label)
		return;
	m_cell.label = s;
	m_label->setText(s);
	m_labelAuto = true;
	commit();
}

/* ---- CellEditorDialog ----------------------------------------------- */

CellEditorDialog::CellEditorDialog(const Cell &cell, QWidget *parent) : QDialog(parent)
{
	setWindowTitle(T("Inspector.EditCellTitle"));
	auto *layout = new QVBoxLayout(this);
	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	m_inspector = new CellInspector(scroll);
	scroll->setWidget(m_inspector);
	layout->addWidget(scroll);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);
	m_inspector->setCell(cell);
	resize(420, 620);
}

Cell CellEditorDialog::cell() const
{
	return m_inspector->cell();
}

} // namespace cp
