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

#pragma once

#include "../model/panel-model.hpp"

#include <QDialog>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QToolButton;

namespace cp {

class ColorButton;
class PropertiesForm;

/* Edits one cell. Works on its own copy and reports every change through
 * cellChanged(); the owner decides where the result goes. */
class CellInspector : public QWidget {
	Q_OBJECT

public:
	explicit CellInspector(QWidget *parent = nullptr);

	void setCell(const Cell &cell);
	const Cell &cell() const { return m_cell; }
	void clearCell();

signals:
	void cellChanged(const Cell &cell);

private:
	void rebuildKindCombo();
	void loadFromCell();
	void commit();

	void refreshActionList();
	void selectAction(int index);
	void addAction(const QString &typeId);
	void moveAction(int delta);
	void removeAction();
	void actionSettingsChanged();
	void refreshIndicatorCombo();
	void applySuggestedLabel(bool force);
	QString suggestedLabel() const;
	QString actionText(int index) const;
	void setIcon(const QString &ref);
	void refreshIconButton();

	Cell m_cell;
	bool m_hasCell = false;
	bool m_loading = false;
	bool m_labelAuto = false;
	int m_currentAction = -1;

	QWidget *m_body;
	QLabel *m_empty;
	QComboBox *m_kind;
	QLineEdit *m_label;
	QToolButton *m_suggest;
	QCheckBox *m_showLabel;
	QToolButton *m_icon;

	ColorButton *m_bg;
	ColorButton *m_fg;
	ColorButton *m_activeBg;
	ColorButton *m_activeFg;
	QSpinBox *m_fontSize;

	QGroupBox *m_actionsBox;
	QListWidget *m_actions;
	QToolButton *m_addAction;
	QToolButton *m_removeAction;
	QToolButton *m_upAction;
	QToolButton *m_downAction;
	QSpinBox *m_delay;
	QWidget *m_delayRow;
	PropertiesForm *m_actionForm;
	QComboBox *m_indicator;

	QGroupBox *m_widgetBox;
	PropertiesForm *m_widgetForm;
};

/* Modal wrapper used by a dock's in-place edit mode. */
class CellEditorDialog : public QDialog {
	Q_OBJECT

public:
	CellEditorDialog(const Cell &cell, QWidget *parent = nullptr);
	Cell cell() const;

private:
	CellInspector *m_inspector;
};

} // namespace cp
