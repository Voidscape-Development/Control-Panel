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

#include "edit-ops.hpp"
#include "../core/type-registry.hpp"
#include "../model/grid.hpp"

#include <QMessageBox>

namespace cp::edit {

Cell makeCell(const QRect &area, CellKind kind)
{
	Cell c;
	c.id = newId();
	c.setArea(area);
	c.kind = kind;
	if (kind == CellKind::Label) {
		c.label = T("Kind.Label");
	} else if (kind == CellKind::Widget) {
		TypeRegistry *reg = TypeRegistry::instance();
		const QList<WidgetTypePtr> widgets = reg ? reg->widgets() : QList<WidgetTypePtr>();
		if (!widgets.isEmpty())
			c.widgetType = widgets.first()->id();
	}
	return c;
}

QString duplicateCell(Panel &panel, const QString &cellId)
{
	const Cell *src = panel.cell(cellId);
	if (!src)
		return {};
	std::optional<QPoint> slot = grid::findFreeSlot(panel, src->colSpan, src->rowSpan);
	if (!slot)
		slot = grid::findFreeSlot(panel, 1, 1);
	if (!slot)
		return {};
	Cell copy = *src;
	copy.id = newId();
	copy.col = slot->x();
	copy.row = slot->y();
	if (!grid::isFree(panel, copy.area(), QString())) {
		copy.colSpan = 1;
		copy.rowSpan = 1;
	}
	panel.cells.append(copy);
	return copy.id;
}

bool removeCell(Panel &panel, const QString &cellId)
{
	const int i = panel.indexOfCell(cellId);
	if (i < 0)
		return false;
	panel.cells.removeAt(i);
	return true;
}

bool applyCellEdit(Panel &panel, const Cell &edited)
{
	Cell *existing = panel.cell(edited.id);
	if (!existing)
		return false;
	const QRect area = existing->area();
	*existing = edited;
	existing->setArea(area);
	return true;
}

bool resizeGrid(QWidget *parent, Panel &panel, int rows, int cols)
{
	const QStringList affected = grid::cellsAffectedByResize(panel, rows, cols);
	if (!affected.isEmpty()) {
		Panel probe = panel;
		const int removed = grid::resize(probe, rows, cols).size();
		const QString text = T("Editor.ResizeConfirm").arg(removed).arg(affected.size() - removed);
		if (QMessageBox::question(parent, T("Editor.ResizeTitle"), text) != QMessageBox::Yes)
			return false;
	}
	grid::resize(panel, rows, cols);
	return true;
}

} // namespace cp::edit
