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

/*
 * obs-websocket vendor "obs-control-panel". Requests (all take/return JSON):
 *
 *   GetPanelList                         -> { panels: [{ panelId, panelName, rows, cols,
 *                                                        collection, active }] }
 *   GetPanel       { panel }             -> { panel: <panel definition> }
 *   GetCellStates  { panel }             -> { cells: [{ cellId, label, state }] }
 *   TriggerCell    { panel, cell }       -> {}
 *   ShowPanel      { panel }             -> {}
 *   SetSwitcherPanel { panel }           -> {}
 *
 * "panel" accepts an id or a name, "cell" an id or a label. Every response
 * carries "success" and, on failure, "error". Event: CellTriggered
 * { panelId, panelName, cellId, label }.
 */

#include "websocket-vendor.hpp"
#include "runtime.hpp"

#include <obs-websocket-api.h>
#include <plugin-support.h>

#include <QCoreApplication>
#include <QThread>

#include <functional>

namespace cp::websocket {

namespace {

obs_websocket_vendor g_vendor = nullptr;
QMetaObject::Connection g_eventConnection;

const char *stateName(control_panel_state s)
{
	switch (s) {
	case CONTROL_PANEL_STATE_ON:
		return "on";
	case CONTROL_PANEL_STATE_OFF:
		return "off";
	case CONTROL_PANEL_STATE_UNAVAILABLE:
		return "unavailable";
	default:
		return "none";
	}
}

/* Requests arrive on obs-websocket's thread; panels live on the UI thread. */
void onUiThread(const std::function<void()> &fn)
{
	QCoreApplication *app = QCoreApplication::instance();
	if (!app)
		return;
	if (QThread::currentThread() == app->thread())
		fn();
	else
		QMetaObject::invokeMethod(app, fn, Qt::BlockingQueuedConnection);
}

void respond(obs_data_t *response, const QJsonObject &json, const QString &error = QString())
{
	DataPtr data = dataFromJson(json);
	obs_data_apply(response, data.get());
	obs_data_set_bool(response, "success", error.isEmpty());
	if (!error.isEmpty())
		obs_data_set_string(response, "error", error.toUtf8().constData());
}

using Handler = std::function<void(Runtime &, obs_data_t *req, obs_data_t *resp)>;

/* Resolve "panel" from the request or fail the response. */
const Panel *requirePanel(Runtime &rt, obs_data_t *req, obs_data_t *resp)
{
	const QString key = utf8(obs_data_get_string(req, "panel"));
	const Panel *p = key.isEmpty() ? nullptr : rt.store().findPanel(key);
	if (!p)
		respond(resp, {},
			key.isEmpty() ? QStringLiteral("Missing 'panel'")
				      : QStringLiteral("Panel '%1' not found").arg(key));
	return p;
}

void registerRequest(const char *name, Handler handler)
{
	auto *heap = new Handler(std::move(handler)); /* lives for the process */
	obs_websocket_vendor_register_request(
		g_vendor, name,
		[](obs_data_t *req, obs_data_t *resp, void *priv) {
			auto *h = static_cast<Handler *>(priv);
			onUiThread([&]() {
				Runtime *rt = Runtime::get();
				if (!rt || !rt->isFrontendReady()) {
					respond(resp, {}, QStringLiteral("OBS is starting up or shutting down"));
					return;
				}
				(*h)(*rt, req, resp);
			});
		},
		heap);
}

} // namespace

void start()
{
	if (g_vendor)
		return;
	g_vendor = obs_websocket_register_vendor(kVendorName);
	if (!g_vendor) {
		obs_log(LOG_INFO, "obs-websocket not available; remote control disabled");
		return;
	}

	registerRequest("GetPanelList", [](Runtime &rt, obs_data_t *, obs_data_t *resp) {
		QJsonArray panels;
		for (const Panel &p : rt.store().panels())
			panels.append(QJsonObject{{"panelId", p.id},
						  {"panelName", p.name},
						  {"rows", p.rows},
						  {"cols", p.cols},
						  {"collection", p.collection},
						  {"active", rt.isPanelActive(p)}});
		respond(resp, {{"panels", panels}});
	});

	registerRequest("GetPanel", [](Runtime &rt, obs_data_t *req, obs_data_t *resp) {
		if (const Panel *p = requirePanel(rt, req, resp)) {
			QJsonObject json = p->toJson();
			json.remove(QStringLiteral("hotkeys"));
			respond(resp, {{"panel", json}});
		}
	});

	registerRequest("GetCellStates", [](Runtime &rt, obs_data_t *req, obs_data_t *resp) {
		const Panel *p = requirePanel(rt, req, resp);
		if (!p)
			return;
		QJsonArray cells;
		for (const Cell &c : p->cells) {
			if (c.kind != CellKind::Button)
				continue;
			cells.append(QJsonObject{{"cellId", c.id},
						 {"label", c.label},
						 {"state", stateName(evaluateCellState(c))}});
		}
		respond(resp, {{"cells", cells}});
	});

	registerRequest("TriggerCell", [](Runtime &rt, obs_data_t *req, obs_data_t *resp) {
		const Panel *p = requirePanel(rt, req, resp);
		if (!p)
			return;
		const QString key = utf8(obs_data_get_string(req, "cell"));
		const Cell *c = p->findCell(key);
		if (!c) {
			respond(resp, {}, QStringLiteral("Cell '%1' not found").arg(key));
			return;
		}
		if (!rt.triggerCell(p->id, c->id)) {
			respond(resp, {}, QStringLiteral("Cell '%1' is not a button").arg(key));
			return;
		}
		respond(resp, {});
	});

	registerRequest("ShowPanel", [](Runtime &rt, obs_data_t *req, obs_data_t *resp) {
		if (const Panel *p = requirePanel(rt, req, resp)) {
			rt.showPanel(p->id);
			respond(resp, {});
		}
	});

	registerRequest("SetSwitcherPanel", [](Runtime &rt, obs_data_t *req, obs_data_t *resp) {
		if (const Panel *p = requirePanel(rt, req, resp)) {
			rt.switchToPanel(p->id);
			respond(resp, {});
		}
	});

	if (Runtime *rt = Runtime::get()) {
		g_eventConnection = QObject::connect(
			rt, &Runtime::cellTriggered, rt, [](const QString &panelId, const QString &cellId) {
				Runtime *r = Runtime::get();
				const Panel *p = r ? r->store().panel(panelId) : nullptr;
				const Cell *c = p ? p->cell(cellId) : nullptr;
				if (!g_vendor || !c)
					return;
				DataPtr ev = dataFromJson(QJsonObject{{"panelId", panelId},
								      {"panelName", p->name},
								      {"cellId", cellId},
								      {"label", c->label}});
				obs_websocket_vendor_emit_event(g_vendor, "CellTriggered", ev.get());
			});
	}
	obs_log(LOG_INFO, "Registered obs-websocket vendor '%s'", kVendorName);
}

void stop()
{
	QObject::disconnect(g_eventConnection);
	/* obs-websocket has no vendor removal; requests check Runtime::get(). */
	g_vendor = nullptr;
}

} // namespace cp::websocket
