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

#include "material-symbols.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace cp {

const QStringList &MaterialLibrary::styles()
{
	static const QStringList s = {QStringLiteral("outlined"), QStringLiteral("rounded"), QStringLiteral("sharp")};
	return s;
}

QByteArray MaterialLibrary::readPack(const QString &file, QString *error)
{
	QFile f(file);
	if (!f.open(QIODevice::ReadOnly)) {
		if (error)
			*error = f.errorString();
		return {};
	}
	const QByteArray data = qUncompress(f.readAll());
	if (data.isEmpty() && error)
		*error = QStringLiteral("Corrupt pack: %1").arg(file);
	return data;
}

bool MaterialLibrary::open(const QString &dir, QString *error)
{
	m_dir = dir;
	m_icons.clear();
	m_byName.clear();
	m_glyphs.clear();
	m_styleFailed.clear();

	const QByteArray raw = readPack(QDir(dir).filePath(QStringLiteral("index.pack")), error);
	if (raw.isEmpty())
		return false;
	const QJsonObject root = QJsonDocument::fromJson(raw).object();
	m_version = root.value(QStringLiteral("version")).toString();

	for (const QJsonValue v : root.value(QStringLiteral("icons")).toArray()) {
		const QJsonObject o = v.toObject();
		MaterialIcon icon;
		icon.name = o.value(QStringLiteral("n")).toString();
		if (icon.name.isEmpty())
			continue;
		QString label = icon.name;
		label.replace(QLatin1Char('_'), QLatin1Char(' '));
		label[0] = label[0].toUpper();
		icon.label = label;
		for (const QJsonValue c : o.value(QStringLiteral("c")).toArray())
			icon.categories.append(c.toString());
		for (const QJsonValue t : o.value(QStringLiteral("t")).toArray())
			icon.tags.append(t.toString());
		icon.popularity = o.value(QStringLiteral("p")).toInt();
		icon.haystack = (label + QLatin1Char(' ') + icon.tags.join(QLatin1Char(' '))).toLower();
		m_icons.append(icon);
	}

	std::stable_sort(m_icons.begin(), m_icons.end(),
			 [](const MaterialIcon &a, const MaterialIcon &b) { return a.popularity > b.popularity; });
	for (int i = 0; i < m_icons.size(); ++i)
		m_byName.insert(m_icons[i].name, i);

	if (m_icons.isEmpty() && error)
		*error = QStringLiteral("No icons in index");
	return !m_icons.isEmpty();
}

QStringList MaterialLibrary::categories() const
{
	QSet<QString> set;
	for (const MaterialIcon &i : m_icons)
		for (const QString &c : i.categories)
			set.insert(c);
	QStringList list(set.begin(), set.end());
	list.sort(Qt::CaseInsensitive);
	return list;
}

const MaterialIcon *MaterialLibrary::icon(const QString &name) const
{
	const int i = m_byName.value(name, -1);
	return i < 0 ? nullptr : &m_icons[i];
}

QList<int> MaterialLibrary::search(const QString &query, const QString &category) const
{
	const QStringList words = query.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
	const QString joined = words.join(QLatin1Char(' '));

	/* Rank: exact name, name prefix, word in name, tag match. Within a
	 * rank, keep popularity order (icons() is already sorted by it). */
	QList<int> ranks[4];
	for (int i = 0; i < m_icons.size(); ++i) {
		const MaterialIcon &icon = m_icons[i];
		if (!category.isEmpty() && !icon.categories.contains(category))
			continue;
		bool all = true;
		for (const QString &w : words) {
			if (!icon.haystack.contains(w)) {
				all = false;
				break;
			}
		}
		if (!all)
			continue;
		if (words.isEmpty()) {
			ranks[0].append(i);
			continue;
		}
		const QString label = icon.label.toLower();
		if (label == joined)
			ranks[0].append(i);
		else if (label.startsWith(joined))
			ranks[1].append(i);
		else if (std::all_of(words.begin(), words.end(), [&](const QString &w) { return label.contains(w); }))
			ranks[2].append(i);
		else
			ranks[3].append(i);
	}
	return ranks[0] + ranks[1] + ranks[2] + ranks[3];
}

bool MaterialLibrary::loadStyle(const QString &style)
{
	if (m_glyphs.contains(style))
		return true;
	if (m_styleFailed.value(style) || !styles().contains(style) || m_dir.isEmpty())
		return false;

	const QByteArray raw = readPack(QDir(m_dir).filePath(style + QStringLiteral(".pack")));
	const QJsonObject root = QJsonDocument::fromJson(raw).object();
	if (root.isEmpty()) {
		m_styleFailed.insert(style, true);
		return false;
	}
	QHash<QString, QPair<QByteArray, QByteArray>> glyphs;
	glyphs.reserve(root.size());
	for (auto it = root.begin(); it != root.end(); ++it) {
		const QJsonArray pair = it.value().toArray();
		glyphs.insert(it.key(), {pair.at(0).toString().toLatin1(), pair.at(1).toString().toLatin1()});
	}
	m_glyphs.insert(style, glyphs);
	return true;
}

QByteArray MaterialLibrary::path(const QString &style, const QString &name, bool filled)
{
	if (!loadStyle(style))
		return {};
	const auto pair = m_glyphs.value(style).value(name);
	return filled ? pair.second : pair.first;
}

QByteArray MaterialLibrary::svg(const QString &style, const QString &name, bool filled)
{
	const QByteArray d = path(style, name, filled);
	if (d.isEmpty())
		return {};
	return QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 -960 960 960\"><path d=\"") +
	       d + QByteArrayLiteral("\"/></svg>");
}

} // namespace cp
