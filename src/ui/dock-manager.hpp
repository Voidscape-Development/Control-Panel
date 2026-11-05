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

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QWidget>

class QComboBox;
class QDockWidget;

namespace cp {

class PanelView;

/* Combined dock: pick any active panel from a dropdown. */
class SwitcherWidget : public QWidget {
	Q_OBJECT

public:
	explicit SwitcherWidget(QWidget *parent = nullptr);

	void setPanel(const QString &id);
	QString panel() const;
	/* Cycle through the panels listed in the switcher. */
	void step(int delta);

private:
	void refreshList();

	QComboBox *m_combo;
	PanelView *m_view;
};

/*
 * Owns all docks. Each panel with "show as dock" gets its own OBS dock (so
 * users can float/tab/snap it); the switcher dock shows any panel. Docks of
 * panels bound to another scene collection are hidden along with their
 * Docks-menu entry and come back when that collection is loaded.
 */
class DockManager : public QObject {
	Q_OBJECT

public:
	explicit DockManager(QObject *parent = nullptr);
	~DockManager() override;

	/* Create docks. Call once OBS's main window exists. */
	void start();
	/* Remove every dock. Must run before plugins unload (FRONTEND_EVENT_EXIT)
	 * so widget types owned by other plugins are destroyed while valid. */
	void shutdown();

	/* OBS finished loading (dock state restored, collection known). */
	void markLoaded();

	/* Re-evaluate collection bindings (after the collection changed). */
	void applyBindings();

	void showPanel(const QString &id);
	void switchTo(const QString &id);
	void stepSwitcher(int delta);

private:
	struct DockEntry {
		QString dockId;
		QPointer<PanelView> view;
		QPointer<QDockWidget> dock;
		QString title;
	};

	void syncDocks();
	void addDock(const QString &panelId);
	void removeDock(const QString &panelId);
	static QDockWidget *dockOf(QWidget *content);
	void raise(QDockWidget *dock);

	QHash<QString, DockEntry> m_docks; /* by panel id */
	QPointer<SwitcherWidget> m_switcher;
	bool m_started = false;
	bool m_loaded = false; /* startup finished; new docks may be shown */
};

} // namespace cp
