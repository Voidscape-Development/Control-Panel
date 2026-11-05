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
 * Icon references as stored in a cell's "icon" field:
 *
 *   builtin:<id>                       icon compiled into the plugin
 *   material:<style>/<name>            Material Symbols, outline glyph
 *   material:<style>/<name>?fill=always   ... always the filled glyph
 *   material:<style>/<name>?fill=active   ... filled while the button is lit
 *   anything else                      path to an image file
 *
 * <style> is outlined, rounded or sharp. Built-in and Material icons are
 * single-color and tinted to the cell's text color; they're portable
 * because they don't depend on files on the user's machine.
 */

#include <QString>

namespace cp {

struct IconRef {
	enum class Kind { None, Builtin, Material, File };
	enum class Fill { Outline, Always, Active };

	Kind kind = Kind::None;
	QString name;  /* builtin id or Material icon name */
	QString style; /* Material style */
	Fill fill = Fill::Outline;
	QString path; /* File */

	static IconRef parse(const QString &ref);
	QString toString() const;

	bool isTinted() const { return kind == Kind::Builtin || kind == Kind::Material; }
	bool filledWhen(bool active) const { return fill == Fill::Always || (fill == Fill::Active && active); }

	static IconRef builtin(const QString &id);
	static IconRef material(const QString &style, const QString &name, Fill fill = Fill::Outline);
	static IconRef file(const QString &path);
};

} // namespace cp
