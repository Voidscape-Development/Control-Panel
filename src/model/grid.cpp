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

#include "grid.hpp"

#include <algorithm>

namespace cp::grid {

bool inBounds(const Panel &panel, const QRect &area)
{
	return area.isValid() && QRect(0, 0, panel.cols, panel.rows).contains(area);
}

bool isFree(const Panel &panel, const QRect &area, const QString &ignoreCellId)
{
	if (!inBounds(panel, area))
		return false;
	for (const Cell &c : panel.cells) {
		if (c.id == ignoreCellId && !ignoreCellId.isEmpty())
			continue;
		if (c.area().intersects(area))
			return false;
	}
	return true;
}

int cellIndexAt(const Panel &panel, int row, int col)
{
	for (int i = 0; i < panel.cells.size(); ++i)
		if (panel.cells[i].area().contains(col, row))
			return i;
	return -1;
}

std::optional<QPoint> findFreeSlot(const Panel &panel, int colSpan, int rowSpan)
{
	for (int r = 0; r + rowSpan <= panel.rows; ++r)
		for (int c = 0; c + colSpan <= panel.cols; ++c)
			if (isFree(panel, QRect(c, r, colSpan, rowSpan), QString()))
				return QPoint(c, r);
	return std::nullopt;
}

QStringList cellsAffectedByResize(const Panel &panel, int rows, int cols)
{
	const QRect bounds(0, 0, cols, rows);
	QStringList ids;
	for (const Cell &c : panel.cells)
		if (!bounds.contains(c.area()))
			ids.append(c.id);
	return ids;
}

QStringList resize(Panel &panel, int rows, int cols)
{
	panel.rows = std::clamp(rows, 1, kMaxGridSize);
	panel.cols = std::clamp(cols, 1, kMaxGridSize);
	const QRect bounds(0, 0, panel.cols, panel.rows);

	QStringList removed;
	for (int i = int(panel.cells.size()) - 1; i >= 0; --i) {
		Cell &c = panel.cells[i];
		if (!bounds.contains(c.area().topLeft())) {
			removed.prepend(c.id);
			panel.cells.removeAt(i);
			continue;
		}
		c.setArea(c.area() & bounds);
	}
	return removed;
}

QRect areaBetween(const QPoint &a, const QPoint &b)
{
	return QRect(QPoint(std::min(a.x(), b.x()), std::min(a.y(), b.y())),
		     QPoint(std::max(a.x(), b.x()), std::max(a.y(), b.y())));
}

namespace {

/* Start offset and length of track i out of n sharing `extent` px with
 * `spacing` px gaps (and the same gap as an outer margin). */
struct Track {
	int start;
	int length;
};

Track track(int extent, int n, int spacing, int i)
{
	n = std::max(1, n);
	const int inner = std::max(0, extent - spacing * (n + 1));
	const int a = static_cast<int>(static_cast<long long>(inner) * i / n);
	const int b = static_cast<int>(static_cast<long long>(inner) * (i + 1) / n);
	return {spacing * (i + 1) + a, b - a};
}

int trackAt(int extent, int n, int spacing, int px)
{
	for (int i = 0; i < n - 1; ++i) {
		const Track next = track(extent, n, spacing, i + 1);
		if (px < next.start)
			return i;
	}
	return std::max(0, n - 1);
}

} // namespace

QRect pixelRect(const Metrics &m, const QRect &area)
{
	const int c0 = std::clamp(area.left(), 0, m.cols - 1);
	const int c1 = std::clamp(area.right(), 0, m.cols - 1);
	const int r0 = std::clamp(area.top(), 0, m.rows - 1);
	const int r1 = std::clamp(area.bottom(), 0, m.rows - 1);

	const Track left = track(m.size.width(), m.cols, m.spacing, c0);
	const Track right = track(m.size.width(), m.cols, m.spacing, c1);
	const Track top = track(m.size.height(), m.rows, m.spacing, r0);
	const Track bottom = track(m.size.height(), m.rows, m.spacing, r1);

	return QRect(QPoint(left.start, top.start),
		     QSize(right.start + right.length - left.start, bottom.start + bottom.length - top.start));
}

QPoint cellAtPixel(const Metrics &m, const QPoint &px)
{
	return QPoint(trackAt(m.size.width(), m.cols, m.spacing, px.x()),
		      trackAt(m.size.height(), m.rows, m.spacing, px.y()));
}

} // namespace cp::grid
