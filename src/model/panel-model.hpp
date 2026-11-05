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
 * Plain value types describing panels. This layer deliberately has no OBS
 * dependency: action/widget settings are kept as JSON objects and only turned
 * into obs_data_t by the runtime. That keeps copies cheap and alias-free (the
 * editor works on a copy of the whole model) and lets the model be unit
 * tested with nothing but Qt.
 */

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QRect>
#include <QString>

namespace cp {

constexpr int kMaxGridSize = 32;
constexpr int kMaxSpacing = 32;
constexpr int kMaxFontSize = 96;

/* Visual style. Every field is optional: an invalid color or a zero font
 * size means "inherit" (cell -> panel theme -> OBS palette). */
struct Style {
	QColor background;
	QColor text;
	QColor activeBackground;
	QColor activeText;
	int fontSize = 0; /* points; 0 = inherit, or auto-fit at the top level */

	/* Returns a copy where unset fields are taken from base. */
	Style mergedOver(const Style &base) const;
	bool isEmpty() const;

	QJsonObject toJson() const;
	static Style fromJson(const QJsonObject &obj);

	bool operator==(const Style &o) const;
	bool operator!=(const Style &o) const { return !(*this == o); }
};

struct Theme {
	Style style;          /* defaults for every cell of the panel */
	int spacing = 4;      /* px between cells and around the grid */
	int cornerRadius = 4; /* px */

	QJsonObject toJson() const;
	static Theme fromJson(const QJsonObject &obj);
	bool operator==(const Theme &o) const;
};

/* One step of a button's action list. */
struct Action {
	QString type;         /* action type id, e.g. "scene_switch" */
	QJsonObject settings; /* type specific, as produced by obs_data_get_json */
	int delayMs = 0;      /* wait before this step runs */

	QJsonObject toJson() const;
	static Action fromJson(const QJsonObject &obj);
	bool operator==(const Action &o) const;
};

enum class CellKind {
	Button, /* runs an action list on click, can light up */
	Label,  /* static text / section header */
	Widget, /* rendered by a registered widget type */
};

QString cellKindToString(CellKind kind);
CellKind cellKindFromString(const QString &str);

constexpr int kIndicatorAuto = -1; /* first action that reports a state */
constexpr int kIndicatorNone = -2;

struct Cell {
	QString id; /* stable, used by the API and obs-websocket */
	int row = 0;
	int col = 0;
	int rowSpan = 1;
	int colSpan = 1;

	CellKind kind = CellKind::Button;
	QString label;
	bool showLabel = true;
	QString iconPath;
	Style style;

	/* Button */
	QList<Action> actions;
	int indicator = kIndicatorAuto;

	/* Widget */
	QString widgetType;
	QJsonObject widgetSettings;

	QRect area() const { return QRect(col, row, colSpan, rowSpan); }
	void setArea(const QRect &r);

	QJsonObject toJson() const;
	static Cell fromJson(const QJsonObject &obj);
	bool operator==(const Cell &o) const;
};

struct Panel {
	QString id;
	QString name;
	int rows = 3;
	int cols = 4;
	Theme theme;

	/* Scene collection binding. Empty = available in every collection. */
	QString collection;
	bool showAsDock = true;
	bool showInSwitcher = true;

	QList<Cell> cells;

	/* Saved hotkey bindings, keyed by hotkey role ("show", "switch"). Owned
	 * by the hotkey manager, persisted here so they survive restarts. */
	QJsonObject hotkeys;

	int indexOfCell(const QString &cellId) const;
	const Cell *cell(const QString &cellId) const;
	Cell *cell(const QString &cellId);

	/* Finds a button by id first, then by (case insensitive) label. */
	const Cell *findCell(const QString &idOrLabel) const;

	bool isBoundTo(const QString &activeCollection) const
	{
		return collection.isEmpty() || collection == activeCollection;
	}

	QJsonObject toJson() const;
	static Panel fromJson(const QJsonObject &obj);
	bool operator==(const Panel &o) const;
};

QString newId();

/* Ensure sane values after loading untrusted JSON: clamps grid size, spans and
 * spacing, assigns missing ids, drops cells that are outside the grid or
 * overlap an earlier cell. Returns the number of cells dropped. */
int sanitize(Panel &panel);

} // namespace cp
