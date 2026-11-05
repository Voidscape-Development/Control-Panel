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

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTabWidget;

namespace cp {

class CellInspector;
class ColorButton;
class GridCanvas;

/*
 * Full panel editor. Edits a working copy of every panel; nothing reaches
 * the live docks until Apply/OK, and Cancel discards everything.
 */
class EditorDialog : public QDialog {
	Q_OBJECT

public:
	/* Show the (single) editor, optionally selecting a panel. */
	static void open(QWidget *parent, const QString &panelId = QString());
	static void closeInstance();

protected:
	void closeEvent(QCloseEvent *event) override;
	void reject() override;

private:
	explicit EditorDialog(QWidget *parent);

	Panel *current();
	void loadFromStore();
	void refreshPanelList();
	void selectPanel(int index);
	void loadPanelSettings();
	void panelEdited(bool refreshCanvas = true);
	void setDirty(bool dirty);
	bool apply();
	bool confirmDiscard();

	void addPanel();
	void duplicatePanel();
	void removePanel();
	void importPanels();
	void addTemplate(const QString &resource);
	void exportPanels(bool all);

	void createCell(const QRect &area, CellKind kind);
	void selectCell(const QString &id);

	QList<Panel> m_working;
	int m_current = -1;
	bool m_dirty = false;
	bool m_loading = false;

	QListWidget *m_panelList;
	GridCanvas *m_canvas;
	QSpinBox *m_rows;
	QSpinBox *m_cols;
	QTabWidget *m_tabs;
	CellInspector *m_inspector;

	/* panel settings */
	QLineEdit *m_name;
	QComboBox *m_collection;
	QCheckBox *m_showAsDock;
	QCheckBox *m_showInSwitcher;
	QSpinBox *m_spacing;
	QSpinBox *m_radius;
	ColorButton *m_themeBg;
	ColorButton *m_themeFg;
	ColorButton *m_themeActiveBg;
	ColorButton *m_themeActiveFg;
	QSpinBox *m_themeFont;

	QDialogButtonBox *m_buttons;
};

} // namespace cp
