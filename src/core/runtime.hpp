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

#include "control-panel-api.h"
#include "../model/panel-store.hpp"
#include "type-registry.hpp"

#include <QObject>

namespace cp {

/* Index of the action that drives a button's lit state, or -1. */
int indicatorIndex(const Cell &cell);

/* Text a cell shows: its label, or for an unlabeled button the label its
 * first action suggests (e.g. the scene name). */
QString effectiveLabel(const Cell &cell);

/* Current state of a button cell (evaluates its indicator action). */
control_panel_state evaluateCellState(const Cell &cell);

/*
 * Process-wide plugin state. Lives from obs_module_load to obs_module_unload
 * and is only touched from the UI thread.
 */
class Runtime : public QObject {
	Q_OBJECT

public:
	static Runtime *get();
	static void create();
	static void destroy();

	PanelStore &store() { return m_store; }

	/* True between FRONTEND_EVENT_FINISHED_LOADING and FRONTEND_EVENT_EXIT.
	 * Frontend output/scene APIs (obs_frontend_get_recording_output, ...)
	 * must not be called outside this window: OBS hasn't created (or has
	 * already destroyed) what they return, and they crash. All action state
	 * polling and widget ticks are gated on this. */
	bool isFrontendReady() const { return m_frontendReady; }
	void setFrontendReady(bool ready);

	QString currentCollection() const { return m_collection; }
	void setCurrentCollection(const QString &name);
	bool isPanelActive(const Panel &panel) const { return panel.isBoundTo(m_collection); }

	/* Runs the cell's action list. Returns false if the cell doesn't exist
	 * or isn't a button. */
	bool triggerCell(const QString &panelId, const QString &cellId);

	void requestRefresh() { emit refreshRequested(); }
	void showPanel(const QString &panelId) { emit showPanelRequested(panelId); }
	void switchToPanel(const QString &panelId) { emit switchPanelRequested(panelId); }

	/* Open the editor, optionally with a panel selected. Set by the UI. */
	void openEditor(const QString &panelId = QString()) { emit editorRequested(panelId); }

signals:
	void refreshRequested();
	void collectionChanged(const QString &name);
	void cellTriggered(const QString &panelId, const QString &cellId);
	void showPanelRequested(const QString &panelId);
	void switchPanelRequested(const QString &panelId);
	void editorRequested(const QString &panelId);

private:
	Runtime();
	~Runtime() override;

	static void onSourceRename(void *data, calldata_t *cd);
	void handleRename(const SourceRename &rename);

	PanelStore m_store;
	QString m_collection;
	bool m_frontendReady = false;
};

} // namespace cp
