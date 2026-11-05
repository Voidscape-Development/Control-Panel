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

#include "control-panel-api.h"
#include "../model/panel-model.hpp"

#include <QAbstractButton>
#include <QPalette>
#include <QPixmap>

class QPainter;

namespace cp {

/* Fully resolved colors for drawing a cell: cell style over panel theme over
 * the current Qt palette (so panels follow the OBS theme by default). */
struct ResolvedStyle {
	QColor background;
	QColor text;
	QColor activeBackground;
	QColor activeText;
	int fontSize = 0; /* 0 = auto-fit */
	int radius = 4;
	bool explicitBackground = false;
};

ResolvedStyle resolveStyle(const Cell &cell, const Theme &theme, const QPalette &palette);

QPixmap loadIcon(const QString &path);

struct CellVisual {
	CellKind kind = CellKind::Button;
	QString text;
	QPixmap icon;
	ResolvedStyle style;
};

CellVisual visualFor(const Cell &cell, const Theme &theme, const QPalette &palette);

/* Draws a cell (background, icon, auto-fitted text). Used by the run-time
 * button and by the layout editor so both look identical. */
void paintCell(QPainter &p, const QRect &rect, const CellVisual &v, control_panel_state state, bool hovered,
	       bool pressed, const QFont &baseFont);

/* The clickable button of a Button cell. */
class CellButton : public QAbstractButton {
	Q_OBJECT

public:
	explicit CellButton(QWidget *parent = nullptr);

	void setVisual(const CellVisual &visual);
	void setState(control_panel_state state);
	control_panel_state state() const { return m_state; }

	QSize sizeHint() const override { return QSize(64, 48); }
	QSize minimumSizeHint() const override { return QSize(8, 8); }

protected:
	void paintEvent(QPaintEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;

private:
	CellVisual m_visual;
	control_panel_state m_state = CONTROL_PANEL_STATE_NONE;
	bool m_hovered = false;
};

} // namespace cp
