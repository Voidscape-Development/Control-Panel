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

#include "core/panel-hotkeys.hpp"
#include "core/runtime.hpp"
#include "core/type-registry.hpp"
#include "core/websocket-vendor.hpp"
#include "ui/dock-manager.hpp"
#include "ui/editor-dialog.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QAction>
#include <QMainWindow>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

namespace {

cp::DockManager *g_docks = nullptr;
cp::PanelHotkeys *g_hotkeys = nullptr;

QWidget *mainWindow()
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

void handleCollectionRename(cp::Runtime &rt)
{
	/* Keep bindings pointing at a renamed collection. */
	const QString oldName = rt.currentCollection();
	const QString newName = cp::currentSceneCollection();
	if (oldName.isEmpty() || oldName == newName)
		return;
	for (cp::Panel p : rt.store().panels()) {
		if (p.collection == oldName) {
			p.collection = newName;
			rt.store().updatePanel(p);
		}
	}
	rt.setCurrentCollection(newName);
}

void onFrontendEvent(obs_frontend_event event, void *)
{
	cp::Runtime *rt = cp::Runtime::get();
	if (!rt)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		rt->setCurrentCollection(cp::currentSceneCollection());
		rt->setFrontendReady(true);
		if (g_docks)
			g_docks->markLoaded();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		rt->setCurrentCollection(cp::currentSceneCollection());
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_RENAMED:
		handleCollectionRename(*rt);
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		rt->setFrontendReady(false);
		/* Tear down UI while OBS and every other plugin are still fully
		 * alive: widgets of plugin-provided types must be destroyed by
		 * their owners before those plugins unload. */
		if (g_hotkeys)
			g_hotkeys->saveBindings();
		cp::EditorDialog::closeInstance();
		if (g_docks)
			g_docks->shutdown();
		rt->store().saveNow();
		break;
	default:
		/* State changes: refresh lit buttons immediately instead of
		 * waiting for the next poll. */
		rt->requestRefresh();
		break;
	}
}

} // namespace

bool obs_module_load(void)
{
	cp::TypeRegistry::create();
	cp::registerBuiltinActions(*cp::TypeRegistry::instance());
	cp::registerBuiltinWidgets(*cp::TypeRegistry::instance());

	cp::Runtime::create();
	cp::Runtime *rt = cp::Runtime::get();

	char *path = obs_module_config_path("panels.json");
	QString error;
	if (!rt->store().load(QString::fromUtf8(path), &error))
		obs_log(LOG_ERROR, "Could not read %s (%s); it was moved aside to panels.json.invalid", path,
			qUtf8Printable(error));
	bfree(path);

	/* Other plugins register their types from obs_module_post_load, which
	 * runs after every module's obs_module_load. */
	cp::registerApiProcs();

	g_docks = new cp::DockManager();
	g_docks->start();

	g_hotkeys = new cp::PanelHotkeys();
	QObject::connect(g_hotkeys, &cp::PanelHotkeys::stepSwitcherRequested, g_docks, &cp::DockManager::stepSwitcher);

	QObject::connect(rt, &cp::Runtime::editorRequested, rt,
			 [](const QString &panelId) { cp::EditorDialog::open(mainWindow(), panelId); });

	auto *action = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(obs_module_text("Menu.Editor")));
	QObject::connect(action, &QAction::triggered, rt, []() { cp::EditorDialog::open(mainWindow()); });

	obs_frontend_add_event_callback(onFrontendEvent, nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s, API %d)", PLUGIN_VERSION, CONTROL_PANEL_API_VERSION);
	return true;
}

void obs_module_post_load(void)
{
	cp::websocket::start();
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	cp::websocket::stop();
	cp::disableApiProcs();

	delete g_hotkeys;
	g_hotkeys = nullptr;
	delete g_docks; /* already shut down at EXIT; this is a no-op fallback */
	g_docks = nullptr;

	cp::Runtime::destroy();
	/* Releases plugin type_data still registered (via free_type_data). */
	cp::TypeRegistry::destroy();

	obs_log(LOG_INFO, "plugin unloaded");
}
