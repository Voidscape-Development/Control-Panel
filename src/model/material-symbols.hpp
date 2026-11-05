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
 * Reader for the Material Symbols packs in data/material-symbols (built by
 * tools/update-material-symbols.py). The index is loaded on first use; a
 * style's glyphs are loaded the first time an icon of that style is drawn.
 *
 * Offline only: everything comes from the files installed with the plugin.
 * See docs/ICONS.md for the planned optional download of newer releases.
 */

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace cp {

struct MaterialIcon {
	QString name;  /* e.g. "fiber_manual_record" */
	QString label; /* e.g. "Fiber manual record" */
	QStringList categories;
	QStringList tags;
	int popularity = 0;
	QString haystack; /* lowercased name + tags, for search */
};

class MaterialLibrary {
public:
	static const QStringList &styles(); /* outlined, rounded, sharp */

	/* Point at a directory containing the packs. Returns false (and stays
	 * unavailable) if the index can't be read. */
	bool open(const QString &dir, QString *error = nullptr);
	bool isAvailable() const { return !m_icons.isEmpty(); }
	QString version() const { return m_version; }

	/* All icons, most popular first. */
	const QList<MaterialIcon> &icons() const { return m_icons; }
	QStringList categories() const;
	bool contains(const QString &name) const { return m_byName.contains(name); }
	const MaterialIcon *icon(const QString &name) const;

	/* Indexes into icons() matching every word of query (in name or tags)
	 * and the category (empty = any). Name matches rank first. */
	QList<int> search(const QString &query, const QString &category = QString()) const;

	/* SVG path data for an icon (viewBox "0 -960 960 960"), or empty. */
	QByteArray path(const QString &style, const QString &name, bool filled);

	/* A complete SVG document for an icon, or empty. */
	QByteArray svg(const QString &style, const QString &name, bool filled);

	/* Read a qCompress-framed JSON file. Exposed for tests. */
	static QByteArray readPack(const QString &file, QString *error = nullptr);

private:
	bool loadStyle(const QString &style);

	QString m_dir;
	QString m_version;
	QList<MaterialIcon> m_icons;
	QHash<QString, int> m_byName;
	QHash<QString, QHash<QString, QPair<QByteArray, QByteArray>>> m_glyphs; /* style -> name -> (outline, fill) */
	QHash<QString, bool> m_styleFailed;
};

} // namespace cp
