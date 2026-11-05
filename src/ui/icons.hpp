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
 * Cell icons; see model/icon-ref.hpp for the reference syntax.
 *
 *   builtin:<id>        icons compiled into the plugin
 *   material:...        Material Symbols, from data files installed with
 *                       the plugin (offline; see docs/ICONS.md)
 *   a file path         PNG/JPG/SVG/... drawn as-is
 *
 * Built-in and Material icons are single-color and tinted to the cell's text
 * color (or active text color when lit).
 */

#include "../model/icon-ref.hpp"

#include <QColor>
#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QString>

namespace cp {
class MaterialLibrary;
}

namespace cp::icons {

struct BuiltinIcon {
	const char *id;
	const char *nameKey; /* locale key */
};

const QList<BuiltinIcon> &builtins();

/* The Material Symbols library, opened from the plugin's data directory on
 * first use. Check isAvailable(): the data files may be missing. */
MaterialLibrary &material();

/* Human-readable description of a reference, e.g. "Mic (Rounded, filled)". */
QString displayName(const QString &ref);

/* Render an icon to fit `size` (logical px), keeping aspect ratio. `active`
 * selects the filled Material glyph for "fill when lit" icons. Cached;
 * returns a null pixmap if the reference can't be drawn. */
QPixmap render(const QString &ref, const QSize &size, qreal devicePixelRatio, const QColor &tint, bool active = false);

/* A QIcon for menus, lists and buttons. */
QIcon menuIcon(const QString &ref, const QColor &tint);

} // namespace cp::icons
