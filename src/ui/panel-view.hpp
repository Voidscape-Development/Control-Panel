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

#pragma once

#include "../core/type-registry.hpp"
#include "../model/panel-model.hpp"

#include <QPointer>
#include <QTimer>
#include <QWidget>

class QLabel;
class QSpinBox;
class QStackedWidget;

namespace cp {

class CellButton;
class GridCanvas;

/*
 * Shows one panel. Run mode lays out live cells on the grid and polls action
 * state while visible; edit mode swaps in a GridCanvas that edits the stored
 * panel directly. Several views may show the same panel (its own dock and
 * the switcher), each rebuilding from the store when it changes.
 */
class PanelView : public QWidget {
	Q_OBJECT

public:
	explicit PanelView(QWidget *parent = nullptr);
	~PanelView() override;

	void setPanelId(const QString &id);
	QString panelId() const { return m_panelId; }

	void setEditMode(bool on);
	bool editMode() const { return m_editMode; }

	/* Allow the "Switch panel" items in the context menu (switcher only). */
	void setShowSwitchMenu(bool on) { m_showSwitchMenu = on; }

	QSize sizeHint() const override { return QSize(320, 200); }
	QSize minimumSizeHint() const override { return QSize(40, 30); }

signals:
	void editModeChanged(bool on);

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void changeEvent(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	struct Item {
		QString cellId;
		QRect area;
		QWidget *widget = nullptr; /* CellButton or frame */
		CellButton *button = nullptr;
		/* indicator */
		ActionTypePtr stateType;
		DataPtr stateSettings;
		/* widget cells */
		WidgetTypePtr widgetType;
		DataPtr widgetSettings;
		QWidget *typeWidget = nullptr;
	};

	void rebuild();
	void destroyItems();
	void layoutItems();
	void refreshStates();
	void tick();

	void editCell(const QString &cellId);
	void createCell(const QRect &area, CellKind kind);
	void modifyPanel(const std::function<bool(Panel &)> &fn);
	QString cellIdAt(const QPoint &globalPos) const;

	QString m_panelId;
	Panel m_panel;
	QList<Item> m_items;
	bool m_editMode = false;
	bool m_showSwitchMenu = false;

	QStackedWidget *m_stack;
	QWidget *m_runPage;
	QLabel *m_placeholder;
	QWidget *m_editPage;
	GridCanvas *m_canvas;
	QSpinBox *m_rows;
	QSpinBox *m_cols;
	QTimer m_pollTimer;
};

} // namespace cp
