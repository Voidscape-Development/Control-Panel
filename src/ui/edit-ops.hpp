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

/* Layout edits shared by the editor dialog and a dock's edit mode. */

#include "../model/panel-model.hpp"

class QWidget;

namespace cp::edit {

Cell makeCell(const QRect &area, CellKind kind);

/* Copy a cell into the first free slot that fits it. Returns the new id or
 * an empty string if there's no room. */
QString duplicateCell(Panel &panel, const QString &cellId);

bool removeCell(Panel &panel, const QString &cellId);

/* Apply inspector edits but keep the geometry the model already has (the
 * inspector's copy may predate a drag on the canvas). */
bool applyCellEdit(Panel &panel, const Cell &edited);

/* Resize the grid; asks for confirmation if cells would be removed or
 * clipped. Returns false if the user cancelled. */
bool resizeGrid(QWidget *parent, Panel &panel, int rows, int cols);

} // namespace cp::edit
