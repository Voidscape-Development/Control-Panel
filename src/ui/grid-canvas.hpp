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

#include "../model/grid.hpp"

#include <QWidget>

namespace cp {

/*
 * Interactive layout surface (inspired by the Looking Glass multiview
 * editor). Draw on empty space to create a cell, drag a cell to move it,
 * drag its edges or corners to resize. Keyboard: arrows move, Shift+arrows
 * resize, Delete removes, Enter edits, Ctrl+D duplicates.
 *
 * The canvas never mutates the model: it emits requests and the owner
 * applies them and calls setPanel() again. That keeps one source of truth
 * whether it's used in the editor dialog or in a dock's edit mode.
 */
class GridCanvas : public QWidget {
	Q_OBJECT

public:
	explicit GridCanvas(QWidget *parent = nullptr);

	void setPanel(const Panel &panel);
	const Panel &panel() const { return m_panel; }

	QString selectedCell() const { return m_selected; }
	void setSelectedCell(const QString &id);

	/* Names shown for widget cells, keyed by widget type id. */
	void setWidgetTypeNames(const QHash<QString, QString> &names) { m_widgetNames = names; }

	QSize sizeHint() const override { return QSize(480, 320); }
	QSize minimumSizeHint() const override { return QSize(120, 80); }

signals:
	void selectionChanged(const QString &cellId);
	void createRequested(const QRect &area, CellKind kind);
	void geometryRequested(const QString &cellId, const QRect &area);
	void editRequested(const QString &cellId);
	void deleteRequested(const QString &cellId);
	void duplicateRequested(const QString &cellId);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void leaveEvent(QEvent *event) override;

private:
	enum class Drag { None, Create, Move, Resize };

	grid::Metrics metrics() const;
	QPoint cellAt(const QPoint &px) const;
	QRect pixelRect(const QRect &area) const;
	Qt::Edges resizeEdgesAt(const QPoint &px, QString *cellId) const;
	void updateCursor(const QPoint &px);
	void updatePreview(const QPoint &px);
	void select(const QString &id);

	Panel m_panel;
	QString m_selected;
	QHash<QString, QString> m_widgetNames;

	Drag m_drag = Drag::None;
	bool m_dragMoved = false;
	QPoint m_pressPx;
	QPoint m_pressCell;
	QString m_dragCell;
	QRect m_origArea;
	Qt::Edges m_edges;
	QRect m_preview;
	bool m_previewValid = false;
	QPoint m_hoverCell{-1, -1};
};

} // namespace cp
