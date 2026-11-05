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

#include "panel-model.hpp"
#include "grid.hpp"

#include <QSet>
#include <QUuid>

#include <algorithm>

namespace cp {

namespace {

QString colorToString(const QColor &c)
{
	return c.isValid() ? c.name(QColor::HexArgb) : QString();
}

QColor colorFromJson(const QJsonValue &v)
{
	const QString s = v.toString();
	if (s.isEmpty())
		return QColor();
	QColor c(s);
	return c.isValid() ? c : QColor();
}

void putColor(QJsonObject &obj, const char *key, const QColor &c)
{
	if (c.isValid())
		obj.insert(QLatin1String(key), colorToString(c));
}

} // namespace

/* ---- Style ---------------------------------------------------------- */

Style Style::mergedOver(const Style &base) const
{
	Style r = *this;
	if (!r.background.isValid())
		r.background = base.background;
	if (!r.text.isValid())
		r.text = base.text;
	if (!r.activeBackground.isValid())
		r.activeBackground = base.activeBackground;
	if (!r.activeText.isValid())
		r.activeText = base.activeText;
	if (r.fontSize <= 0)
		r.fontSize = base.fontSize;
	return r;
}

bool Style::isEmpty() const
{
	return !background.isValid() && !text.isValid() && !activeBackground.isValid() && !activeText.isValid() &&
	       fontSize <= 0;
}

QJsonObject Style::toJson() const
{
	QJsonObject o;
	putColor(o, "background", background);
	putColor(o, "text", text);
	putColor(o, "activeBackground", activeBackground);
	putColor(o, "activeText", activeText);
	if (fontSize > 0)
		o.insert(QStringLiteral("fontSize"), fontSize);
	return o;
}

Style Style::fromJson(const QJsonObject &o)
{
	Style s;
	s.background = colorFromJson(o.value(QStringLiteral("background")));
	s.text = colorFromJson(o.value(QStringLiteral("text")));
	s.activeBackground = colorFromJson(o.value(QStringLiteral("activeBackground")));
	s.activeText = colorFromJson(o.value(QStringLiteral("activeText")));
	s.fontSize = std::clamp(o.value(QStringLiteral("fontSize")).toInt(0), 0, kMaxFontSize);
	return s;
}

bool Style::operator==(const Style &o) const
{
	return background == o.background && text == o.text && activeBackground == o.activeBackground &&
	       activeText == o.activeText && fontSize == o.fontSize;
}

/* ---- Theme ---------------------------------------------------------- */

QJsonObject Theme::toJson() const
{
	QJsonObject o;
	o.insert(QStringLiteral("style"), style.toJson());
	o.insert(QStringLiteral("spacing"), spacing);
	o.insert(QStringLiteral("cornerRadius"), cornerRadius);
	return o;
}

Theme Theme::fromJson(const QJsonObject &o)
{
	Theme t;
	t.style = Style::fromJson(o.value(QStringLiteral("style")).toObject());
	t.spacing = std::clamp(o.value(QStringLiteral("spacing")).toInt(t.spacing), 0, kMaxSpacing);
	t.cornerRadius = std::clamp(o.value(QStringLiteral("cornerRadius")).toInt(t.cornerRadius), 0, 64);
	return t;
}

bool Theme::operator==(const Theme &o) const
{
	return style == o.style && spacing == o.spacing && cornerRadius == o.cornerRadius;
}

/* ---- Action --------------------------------------------------------- */

QJsonObject Action::toJson() const
{
	QJsonObject o;
	o.insert(QStringLiteral("type"), type);
	o.insert(QStringLiteral("settings"), settings);
	if (delayMs > 0)
		o.insert(QStringLiteral("delayMs"), delayMs);
	return o;
}

Action Action::fromJson(const QJsonObject &o)
{
	Action a;
	a.type = o.value(QStringLiteral("type")).toString();
	a.settings = o.value(QStringLiteral("settings")).toObject();
	a.delayMs = std::clamp(o.value(QStringLiteral("delayMs")).toInt(0), 0, 10 * 60 * 1000);
	return a;
}

bool Action::operator==(const Action &o) const
{
	return type == o.type && settings == o.settings && delayMs == o.delayMs;
}

/* ---- Cell ----------------------------------------------------------- */

QString cellKindToString(CellKind kind)
{
	switch (kind) {
	case CellKind::Label:
		return QStringLiteral("label");
	case CellKind::Widget:
		return QStringLiteral("widget");
	case CellKind::Button:
		break;
	}
	return QStringLiteral("button");
}

CellKind cellKindFromString(const QString &str)
{
	if (str == QLatin1String("label"))
		return CellKind::Label;
	if (str == QLatin1String("widget"))
		return CellKind::Widget;
	return CellKind::Button;
}

void Cell::setArea(const QRect &r)
{
	col = r.x();
	row = r.y();
	colSpan = r.width();
	rowSpan = r.height();
}

QJsonObject Cell::toJson() const
{
	QJsonObject o;
	o.insert(QStringLiteral("id"), id);
	o.insert(QStringLiteral("row"), row);
	o.insert(QStringLiteral("col"), col);
	o.insert(QStringLiteral("rowSpan"), rowSpan);
	o.insert(QStringLiteral("colSpan"), colSpan);
	o.insert(QStringLiteral("kind"), cellKindToString(kind));
	o.insert(QStringLiteral("label"), label);
	o.insert(QStringLiteral("showLabel"), showLabel);
	if (!iconPath.isEmpty())
		o.insert(QStringLiteral("icon"), iconPath);
	if (!style.isEmpty())
		o.insert(QStringLiteral("style"), style.toJson());

	switch (kind) {
	case CellKind::Button: {
		QJsonArray arr;
		for (const Action &a : actions)
			arr.append(a.toJson());
		o.insert(QStringLiteral("actions"), arr);
		o.insert(QStringLiteral("indicator"), indicator);
		break;
	}
	case CellKind::Widget:
		o.insert(QStringLiteral("widgetType"), widgetType);
		o.insert(QStringLiteral("widgetSettings"), widgetSettings);
		break;
	case CellKind::Label:
		break;
	}
	return o;
}

Cell Cell::fromJson(const QJsonObject &o)
{
	Cell c;
	c.id = o.value(QStringLiteral("id")).toString();
	c.row = o.value(QStringLiteral("row")).toInt(0);
	c.col = o.value(QStringLiteral("col")).toInt(0);
	c.rowSpan = o.value(QStringLiteral("rowSpan")).toInt(1);
	c.colSpan = o.value(QStringLiteral("colSpan")).toInt(1);
	c.kind = cellKindFromString(o.value(QStringLiteral("kind")).toString());
	c.label = o.value(QStringLiteral("label")).toString();
	c.showLabel = o.value(QStringLiteral("showLabel")).toBool(true);
	c.iconPath = o.value(QStringLiteral("icon")).toString();
	c.style = Style::fromJson(o.value(QStringLiteral("style")).toObject());

	for (const QJsonValue v : o.value(QStringLiteral("actions")).toArray()) {
		Action a = Action::fromJson(v.toObject());
		if (!a.type.isEmpty())
			c.actions.append(a);
	}
	c.indicator = o.value(QStringLiteral("indicator")).toInt(kIndicatorAuto);
	if (c.indicator < kIndicatorNone || c.indicator >= c.actions.size())
		c.indicator = kIndicatorAuto;

	c.widgetType = o.value(QStringLiteral("widgetType")).toString();
	c.widgetSettings = o.value(QStringLiteral("widgetSettings")).toObject();
	return c;
}

bool Cell::operator==(const Cell &o) const
{
	return id == o.id && area() == o.area() && kind == o.kind && label == o.label && showLabel == o.showLabel &&
	       iconPath == o.iconPath && style == o.style && actions == o.actions && indicator == o.indicator &&
	       widgetType == o.widgetType && widgetSettings == o.widgetSettings;
}

/* ---- Panel ---------------------------------------------------------- */

int Panel::indexOfCell(const QString &cellId) const
{
	for (int i = 0; i < cells.size(); ++i)
		if (cells[i].id == cellId)
			return i;
	return -1;
}

const Cell *Panel::cell(const QString &cellId) const
{
	const int i = indexOfCell(cellId);
	return i < 0 ? nullptr : &cells[i];
}

Cell *Panel::cell(const QString &cellId)
{
	const int i = indexOfCell(cellId);
	return i < 0 ? nullptr : &cells[i];
}

const Cell *Panel::findCell(const QString &idOrLabel) const
{
	if (const Cell *c = cell(idOrLabel))
		return c;
	for (const Cell &c : cells)
		if (c.label.compare(idOrLabel, Qt::CaseInsensitive) == 0)
			return &c;
	return nullptr;
}

QJsonObject Panel::toJson() const
{
	QJsonObject o;
	o.insert(QStringLiteral("id"), id);
	o.insert(QStringLiteral("name"), name);
	o.insert(QStringLiteral("rows"), rows);
	o.insert(QStringLiteral("cols"), cols);
	o.insert(QStringLiteral("theme"), theme.toJson());
	o.insert(QStringLiteral("collection"), collection);
	o.insert(QStringLiteral("showAsDock"), showAsDock);
	o.insert(QStringLiteral("showInSwitcher"), showInSwitcher);
	QJsonArray arr;
	for (const Cell &c : cells)
		arr.append(c.toJson());
	o.insert(QStringLiteral("cells"), arr);
	if (!hotkeys.isEmpty())
		o.insert(QStringLiteral("hotkeys"), hotkeys);
	return o;
}

Panel Panel::fromJson(const QJsonObject &o)
{
	Panel p;
	p.id = o.value(QStringLiteral("id")).toString();
	p.name = o.value(QStringLiteral("name")).toString();
	p.rows = o.value(QStringLiteral("rows")).toInt(p.rows);
	p.cols = o.value(QStringLiteral("cols")).toInt(p.cols);
	p.theme = Theme::fromJson(o.value(QStringLiteral("theme")).toObject());
	p.collection = o.value(QStringLiteral("collection")).toString();
	p.showAsDock = o.value(QStringLiteral("showAsDock")).toBool(true);
	p.showInSwitcher = o.value(QStringLiteral("showInSwitcher")).toBool(true);
	for (const QJsonValue v : o.value(QStringLiteral("cells")).toArray())
		p.cells.append(Cell::fromJson(v.toObject()));
	p.hotkeys = o.value(QStringLiteral("hotkeys")).toObject();
	return p;
}

bool Panel::operator==(const Panel &o) const
{
	return id == o.id && name == o.name && rows == o.rows && cols == o.cols && theme == o.theme &&
	       collection == o.collection && showAsDock == o.showAsDock && showInSwitcher == o.showInSwitcher &&
	       cells == o.cells && hotkeys == o.hotkeys;
}

QString newId()
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

int sanitize(Panel &panel)
{
	if (panel.id.isEmpty())
		panel.id = newId();
	panel.rows = std::clamp(panel.rows, 1, kMaxGridSize);
	panel.cols = std::clamp(panel.cols, 1, kMaxGridSize);

	QList<Cell> kept;
	QSet<QString> ids;
	int dropped = 0;
	Panel probe = panel;
	probe.cells.clear();

	for (Cell c : panel.cells) {
		c.colSpan = std::max(1, c.colSpan);
		c.rowSpan = std::max(1, c.rowSpan);
		if (c.id.isEmpty() || ids.contains(c.id))
			c.id = newId();

		/* Clip spans that stick out of the grid, drop cells fully outside. */
		const QRect clipped = c.area() & QRect(0, 0, panel.cols, panel.rows);
		if (clipped.isEmpty() || clipped.topLeft() != c.area().topLeft() ||
		    !grid::isFree(probe, clipped, QString())) {
			++dropped;
			continue;
		}
		c.setArea(clipped);
		ids.insert(c.id);
		probe.cells.append(c);
		kept.append(c);
	}
	panel.cells = kept;
	return dropped;
}

} // namespace cp
