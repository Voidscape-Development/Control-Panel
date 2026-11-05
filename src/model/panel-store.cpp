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

#include "panel-store.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QTimer>

namespace cp {

PanelStore::PanelStore(QObject *parent) : QObject(parent) {}

PanelStore::~PanelStore()
{
	if (m_savePending)
		saveNow();
}

bool PanelStore::load(const QString &path, QString *error)
{
	m_path = path;
	m_panels.clear();
	m_switcherPanel.clear();
	m_globalHotkeys = QJsonObject();
	m_bindingHidden.clear();

	QFile file(path);
	if (!file.exists()) {
		emit panelsReset();
		return true;
	}
	if (!file.open(QIODevice::ReadOnly)) {
		if (error)
			*error = file.errorString();
		emit panelsReset();
		return false;
	}

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (!doc.isObject()) {
		if (error)
			*error = parseError.errorString();
		/* Keep the unreadable file for manual recovery instead of letting
		 * the next save overwrite it. */
		file.close();
		const QString aside = path + QStringLiteral(".invalid");
		QFile::remove(aside);
		QFile::rename(path, aside);
		emit panelsReset();
		return false;
	}

	const QJsonObject root = doc.object();
	for (const QJsonValue v : root.value(QStringLiteral("panels")).toArray()) {
		Panel p = Panel::fromJson(v.toObject());
		sanitize(p);
		if (panel(p.id))
			p.id = newId();
		m_panels.append(p);
	}
	m_switcherPanel = root.value(QStringLiteral("switcherPanel")).toString();
	m_globalHotkeys = root.value(QStringLiteral("hotkeys")).toObject();
	for (const QJsonValue v : root.value(QStringLiteral("bindingHiddenDocks")).toArray())
		if (panel(v.toString()))
			m_bindingHidden.append(v.toString());
	emit panelsReset();
	return true;
}

QByteArray PanelStore::serialize() const
{
	QJsonObject root;
	root.insert(QStringLiteral("version"), kFormatVersion);
	QJsonArray arr;
	for (const Panel &p : m_panels)
		arr.append(p.toJson());
	root.insert(QStringLiteral("panels"), arr);
	root.insert(QStringLiteral("switcherPanel"), m_switcherPanel);
	if (!m_globalHotkeys.isEmpty())
		root.insert(QStringLiteral("hotkeys"), m_globalHotkeys);
	if (!m_bindingHidden.isEmpty())
		root.insert(QStringLiteral("bindingHiddenDocks"), QJsonArray::fromStringList(m_bindingHidden));
	return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool PanelStore::saveNow(QString *error)
{
	m_savePending = false;
	if (m_path.isEmpty())
		return false;

	QDir().mkpath(QFileInfo(m_path).absolutePath());

	/* Keep the last good file as a backup before replacing it. */
	if (QFile::exists(m_path)) {
		const QString bak = m_path + QStringLiteral(".bak");
		QFile::remove(bak);
		QFile::copy(m_path, bak);
	}

	QSaveFile file(m_path);
	if (!file.open(QIODevice::WriteOnly) || file.write(serialize()) < 0 || !file.commit()) {
		if (error)
			*error = file.errorString();
		return false;
	}
	return true;
}

void PanelStore::scheduleSave()
{
	if (!m_autoSave || m_savePending)
		return;
	m_savePending = true;
	QTimer::singleShot(0, this, [this]() {
		if (m_savePending)
			saveNow();
	});
}

const Panel *PanelStore::panel(const QString &id) const
{
	for (const Panel &p : m_panels)
		if (p.id == id)
			return &p;
	return nullptr;
}

const Panel *PanelStore::findPanel(const QString &idOrName) const
{
	if (const Panel *p = panel(idOrName))
		return p;
	for (const Panel &p : m_panels)
		if (p.name.compare(idOrName, Qt::CaseInsensitive) == 0)
			return &p;
	return nullptr;
}

void PanelStore::setPanels(QList<Panel> panels)
{
	for (Panel &p : panels) {
		sanitize(p);
		if (const Panel *old = panel(p.id))
			p.hotkeys = old->hotkeys;
	}
	m_panels = std::move(panels);
	if (!m_switcherPanel.isEmpty() && !panel(m_switcherPanel))
		m_switcherPanel.clear();
	scheduleSave();
	emit panelsReset();
}

void PanelStore::updatePanel(Panel updated)
{
	for (Panel &p : m_panels) {
		if (p.id != updated.id)
			continue;
		sanitize(updated);
		updated.hotkeys = p.hotkeys;
		if (p == updated)
			return;
		p = std::move(updated);
		scheduleSave();
		emit panelChanged(p.id);
		return;
	}
}

QString PanelStore::addPanel(Panel p)
{
	sanitize(p);
	if (panel(p.id))
		p.id = newId();
	if (p.name.trimmed().isEmpty())
		p.name = QStringLiteral("Panel");
	p.name = uniqueName(p.name, m_panels);
	m_panels.append(p);
	scheduleSave();
	emit panelsReset();
	return p.id;
}

void PanelStore::removePanel(const QString &id)
{
	for (int i = 0; i < m_panels.size(); ++i) {
		if (m_panels[i].id == id) {
			m_panels.removeAt(i);
			if (m_switcherPanel == id)
				m_switcherPanel.clear();
			scheduleSave();
			emit panelsReset();
			return;
		}
	}
}

void PanelStore::setPanelHotkeys(const QString &id, const QJsonObject &hotkeys)
{
	for (Panel &p : m_panels) {
		if (p.id == id && p.hotkeys != hotkeys) {
			p.hotkeys = hotkeys;
			scheduleSave();
		}
	}
}

void PanelStore::setSwitcherPanel(const QString &id)
{
	if (m_switcherPanel == id)
		return;
	m_switcherPanel = id;
	scheduleSave();
	emit switcherPanelChanged(id);
}

void PanelStore::setGlobalHotkeys(const QJsonObject &hotkeys)
{
	if (m_globalHotkeys == hotkeys)
		return;
	m_globalHotkeys = hotkeys;
	scheduleSave();
}

void PanelStore::setBindingHiddenDocks(const QStringList &ids)
{
	if (m_bindingHidden == ids)
		return;
	m_bindingHidden = ids;
	scheduleSave();
}

/* ---- import / export ------------------------------------------------ */

QByteArray PanelStore::exportPanels(const QList<Panel> &panels)
{
	QJsonObject root;
	root.insert(QStringLiteral("format"), QStringLiteral("obs-control-panel"));
	root.insert(QStringLiteral("version"), kFormatVersion);
	QJsonArray arr;
	for (Panel p : panels) {
		/* Bindings are machine specific; don't leak them into shares. */
		p.hotkeys = QJsonObject();
		arr.append(p.toJson());
	}
	root.insert(QStringLiteral("panels"), arr);
	return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

QList<Panel> PanelStore::parsePanels(const QByteArray &json, QString *error)
{
	QList<Panel> result;
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
	if (doc.isNull()) {
		if (error)
			*error = parseError.errorString();
		return result;
	}

	QJsonArray arr;
	if (doc.isObject() && doc.object().contains(QStringLiteral("panels")))
		arr = doc.object().value(QStringLiteral("panels")).toArray();
	else if (doc.isObject())
		arr.append(doc.object()); /* a single bare panel */
	else
		arr = doc.array();

	const int version = doc.isObject() ? doc.object().value(QStringLiteral("version")).toInt(1) : 1;
	if (version > kFormatVersion) {
		if (error)
			*error = QStringLiteral("File was made by a newer version of Control Panel (format %1).")
					 .arg(version);
		return result;
	}

	for (const QJsonValue v : arr) {
		if (!v.isObject())
			continue;
		Panel p = Panel::fromJson(v.toObject());
		sanitize(p);
		result.append(p);
	}
	if (result.isEmpty() && error)
		*error = QStringLiteral("No panels found in file.");
	return result;
}

QString PanelStore::uniqueName(const QString &base, const QList<Panel> &existing)
{
	auto taken = [&](const QString &name) {
		for (const Panel &p : existing)
			if (p.name.compare(name, Qt::CaseInsensitive) == 0)
				return true;
		return false;
	};
	if (!taken(base))
		return base;
	for (int n = 2;; ++n) {
		const QString candidate = QStringLiteral("%1 (%2)").arg(base).arg(n);
		if (!taken(candidate))
			return candidate;
	}
}

QList<Panel> PanelStore::prepareImport(QList<Panel> incoming, const QList<Panel> &existing)
{
	QSet<QString> panelIds;
	QSet<QString> cellIds;
	for (const Panel &p : existing) {
		panelIds.insert(p.id);
		for (const Cell &c : p.cells)
			cellIds.insert(c.id);
	}

	QList<Panel> accepted = existing;
	QList<Panel> result;
	for (Panel p : incoming) {
		if (p.id.isEmpty() || panelIds.contains(p.id))
			p.id = newId();
		panelIds.insert(p.id);
		for (Cell &c : p.cells) {
			if (c.id.isEmpty() || cellIds.contains(c.id))
				c.id = newId();
			cellIds.insert(c.id);
		}
		if (p.name.trimmed().isEmpty())
			p.name = QStringLiteral("Imported Panel");
		p.name = uniqueName(p.name, accepted);
		p.hotkeys = QJsonObject();
		accepted.append(p);
		result.append(p);
	}
	return result;
}

} // namespace cp
