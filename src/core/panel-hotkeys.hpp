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

#include <obs.h>

#include <QObject>
#include <QString>

#include <functional>
#include <map>
#include <memory>

namespace cp {

/*
 * Registers OBS frontend hotkeys (Settings > Hotkeys):
 *   per panel: "Show panel" and "Show in switcher"
 *   global:    "Next panel" / "Previous panel" in the switcher
 * Bindings are persisted in the panel config because OBS doesn't save
 * plugin-registered frontend hotkeys by itself.
 */
class PanelHotkeys : public QObject {
	Q_OBJECT

public:
	explicit PanelHotkeys(QObject *parent = nullptr);
	~PanelHotkeys() override;

	/* Copy current bindings into the store (call before shutdown too). */
	void saveBindings();

signals:
	void stepSwitcherRequested(int delta);

private:
	struct Binding {
		obs_hotkey_id id = OBS_INVALID_HOTKEY_ID;
		std::function<void()> fn;
	};
	struct PanelEntry {
		QString name;
		std::unique_ptr<Binding> show;
		std::unique_ptr<Binding> switcher;
	};

	void sync();
	std::unique_ptr<Binding> registerHotkey(const QString &name, const QString &description,
						std::function<void()> fn, const QJsonArray &saved);
	static void unregisterHotkey(std::unique_ptr<Binding> &b);
	static void onHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *key, bool pressed);
	static void onBindingsChanged(void *data, calldata_t *cd);

	std::map<QString, PanelEntry> m_panels; /* move-only values */
	std::unique_ptr<Binding> m_next;
	std::unique_ptr<Binding> m_prev;
	bool m_savePending = false;
};

} // namespace cp
