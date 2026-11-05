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

/*
 * Cell icons. An icon reference is either
 *   "builtin:<id>"  one of the icons compiled into the plugin. Single color,
 *                   tinted to the cell's text color (or active text color
 *                   when lit). Portable: exported panels keep their icons.
 *   a file path     PNG/JPG/SVG/... drawn as-is. SVGs are rendered at the
 *                   exact size so they stay sharp.
 */

#include <QColor>
#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QString>

namespace cp::icons {

struct BuiltinIcon {
	const char *id;
	const char *nameKey; /* locale key */
};

const QList<BuiltinIcon> &builtins();

QString builtinRef(const QString &id);
bool isBuiltin(const QString &ref);
/* Display text for a reference: the icon's name or the file name. */
QString displayName(const QString &ref);

/* Render an icon to fit `size` (logical px), keeping aspect ratio. tint is
 * applied to built-in icons only. Results are cached. Returns a null pixmap
 * if the reference can't be loaded. */
QPixmap render(const QString &ref, const QSize &size, qreal devicePixelRatio, const QColor &tint);

/* A QIcon for menus and lists. */
QIcon menuIcon(const QString &ref, const QColor &tint);

} // namespace cp::icons
