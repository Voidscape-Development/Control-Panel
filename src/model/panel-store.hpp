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

#include "panel-model.hpp"

#include <QByteArray>
#include <QObject>
#include <QStringList>

namespace cp {

/*
 * Owns the global list of panels and persists it as JSON.
 *
 * All mutation goes through this class so views can observe changes. Writes
 * are coalesced and atomic (QSaveFile), and the previous file is kept as
 * "<file>.bak" so a crash or bad edit can be recovered by hand.
 */
class PanelStore : public QObject {
	Q_OBJECT

public:
	static constexpr int kFormatVersion = 1;

	explicit PanelStore(QObject *parent = nullptr);
	~PanelStore() override;

	/* Load from path (missing file = empty store). Subsequent saves go to
	 * the same path. Returns false and fills error on a parse failure; the
	 * store is then empty and the broken file is left untouched. */
	bool load(const QString &path, QString *error = nullptr);
	bool saveNow(QString *error = nullptr);
	QString path() const { return m_path; }

	/* Disable automatic saving (used by tests). */
	void setAutoSave(bool enabled) { m_autoSave = enabled; }

	const QList<Panel> &panels() const { return m_panels; }
	const Panel *panel(const QString &id) const;
	/* Lookup by id, then by case-insensitive name. */
	const Panel *findPanel(const QString &idOrName) const;

	/* Replace everything (the editor's Apply). Hotkey bindings of panels
	 * that still exist are kept from the current state. */
	void setPanels(QList<Panel> panels);

	/* Replace one panel (in-place edits). No-op if the id is unknown. */
	void updatePanel(Panel panel);

	QString addPanel(Panel panel);
	void removePanel(const QString &id);

	/* Store hotkey bindings without notifying views. */
	void setPanelHotkeys(const QString &id, const QJsonObject &hotkeys);

	QString switcherPanel() const { return m_switcherPanel; }
	void setSwitcherPanel(const QString &id);

	/* Bindings of hotkeys not tied to a panel (e.g. next/previous). */
	QJsonObject globalHotkeys() const { return m_globalHotkeys; }
	void setGlobalHotkeys(const QJsonObject &hotkeys);

	/* Docks that were open when their panel's collection binding hid them;
	 * they are reopened when the collection becomes active again. */
	QStringList bindingHiddenDocks() const { return m_bindingHidden; }
	void setBindingHiddenDocks(const QStringList &ids);

	/* ---- import / export ---- */

	static QByteArray exportPanels(const QList<Panel> &panels);
	/* Accepts both export files and a full config file. */
	static QList<Panel> parsePanels(const QByteArray &json, QString *error);
	/* Make imported panels safe to add: fresh ids where they collide with
	 * existing ones, unique names, no hotkey bindings. */
	static QList<Panel> prepareImport(QList<Panel> incoming, const QList<Panel> &existing);
	/* Returns a name not used by any panel in existing ("Name (2)"...). */
	static QString uniqueName(const QString &base, const QList<Panel> &existing);

	QByteArray serialize() const;

signals:
	/* Panels added, removed, reordered or replaced wholesale. */
	void panelsReset();
	void panelChanged(const QString &id);
	void switcherPanelChanged(const QString &id);

private:
	void scheduleSave();

	QList<Panel> m_panels;
	QString m_switcherPanel;
	QJsonObject m_globalHotkeys;
	QStringList m_bindingHidden;
	QString m_path;
	bool m_autoSave = true;
	bool m_savePending = false;
};

} // namespace cp
