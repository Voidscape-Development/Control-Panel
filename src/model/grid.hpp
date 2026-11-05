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

/*
 * Grid math shared by the run-time view and the layout editor. Areas are in
 * grid units: QRect(col, row, colSpan, rowSpan).
 */

#include "panel-model.hpp"

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QStringList>

#include <optional>

namespace cp::grid {

/* ---- occupancy ---- */

bool inBounds(const Panel &panel, const QRect &area);

/* area is inside the grid and does not overlap any cell except ignoreCellId. */
bool isFree(const Panel &panel, const QRect &area, const QString &ignoreCellId);

/* Index of the cell covering (row, col), or -1. */
int cellIndexAt(const Panel &panel, int row, int col);

/* First free area of the given size, scanning row by row. */
std::optional<QPoint> findFreeSlot(const Panel &panel, int colSpan = 1, int rowSpan = 1);

/* Ids of cells that would be clipped or removed by resizing the grid. */
QStringList cellsAffectedByResize(const Panel &panel, int rows, int cols);

/* Resize the grid. Cells sticking out are clipped; cells that start outside
 * are removed. Returns the ids of removed cells. */
QStringList resize(Panel &panel, int rows, int cols);

/* Normalized area spanned by two grid points (inclusive). */
QRect areaBetween(const QPoint &a, const QPoint &b);

/* ---- pixel geometry ---- */

struct Metrics {
	QSize size; /* widget size in px */
	int rows = 1;
	int cols = 1;
	int spacing = 0; /* gap between cells and outer margin, px */
};

/* Pixel rectangle for a grid area. Rounding remainders are spread across
 * tracks so cells line up exactly with no accumulating drift. */
QRect pixelRect(const Metrics &m, const QRect &area);

/* Grid cell (col, row) under a pixel position. Gaps belong to the preceding
 * track; positions outside are clamped onto the grid. */
QPoint cellAtPixel(const Metrics &m, const QPoint &px);

} // namespace cp::grid
