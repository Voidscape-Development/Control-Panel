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

#include "runtime.hpp"

#include <plugin-support.h>

#include <QPointer>
#include <QTimer>

#include <vector>

namespace cp {

namespace {

Runtime *g_runtime = nullptr;

/*
 * Executes a snapshot of an action list. Settings are resolved up front so
 * edits made while a delayed sequence is running don't affect it.
 */
class ActionSequence : public QObject {
public:
	struct Step {
		ActionTypePtr type;
		DataPtr settings;
		int delayMs = 0;
	};

	ActionSequence(std::vector<Step> steps, QObject *parent) : QObject(parent), m_steps(std::move(steps)) {}

	void start() { runFrom(0, false); }

private:
	void runFrom(size_t index, bool delayDone)
	{
		for (size_t i = index; i < m_steps.size(); ++i) {
			const Step &step = m_steps[i];
			if (step.delayMs > 0 && !(delayDone && i == index)) {
				QTimer::singleShot(step.delayMs, this, [this, i]() { runFrom(i, true); });
				return;
			}
			step.type->execute(step.settings.get());
		}
		deleteLater();
	}

	std::vector<Step> m_steps;
};

} // namespace

int indicatorIndex(const Cell &cell)
{
	if (cell.kind != CellKind::Button || cell.indicator == kIndicatorNone)
		return -1;
	if (cell.indicator >= 0)
		return cell.indicator < cell.actions.size() ? cell.indicator : -1;

	TypeRegistry *reg = TypeRegistry::instance();
	if (!reg)
		return -1;
	for (int i = 0; i < cell.actions.size(); ++i) {
		ActionTypePtr type = reg->action(cell.actions[i].type);
		if (type && type->hasState())
			return i;
	}
	return -1;
}

QString effectiveLabel(const Cell &cell)
{
	if (!cell.label.isEmpty() || cell.kind != CellKind::Button)
		return cell.label;
	TypeRegistry *reg = TypeRegistry::instance();
	if (!reg)
		return {};
	for (const Action &a : cell.actions) {
		ActionTypePtr type = reg->action(a.type);
		if (!type)
			continue;
		const QString suggested = type->defaultLabel(settingsFor(type.get(), a.settings).get());
		if (!suggested.isEmpty())
			return suggested;
	}
	return {};
}

control_panel_state evaluateCellState(const Cell &cell)
{
	const int idx = indicatorIndex(cell);
	TypeRegistry *reg = TypeRegistry::instance();
	Runtime *rt = Runtime::get();
	if (idx < 0 || !reg || !rt || !rt->isFrontendReady())
		return CONTROL_PANEL_STATE_NONE;
	ActionTypePtr type = reg->action(cell.actions[idx].type);
	if (!type)
		return CONTROL_PANEL_STATE_UNAVAILABLE;
	DataPtr settings = settingsFor(type.get(), cell.actions[idx].settings);
	return type->state(settings.get());
}

Runtime *Runtime::get()
{
	return g_runtime;
}

void Runtime::create()
{
	if (!g_runtime)
		g_runtime = new Runtime();
}

void Runtime::destroy()
{
	delete g_runtime;
	g_runtime = nullptr;
}

Runtime::Runtime()
{
	signal_handler_connect(obs_get_signal_handler(), "source_rename", &Runtime::onSourceRename, this);
}

Runtime::~Runtime()
{
	signal_handler_disconnect(obs_get_signal_handler(), "source_rename", &Runtime::onSourceRename, this);
	m_store.saveNow();
}

void Runtime::setFrontendReady(bool ready)
{
	if (m_frontendReady == ready)
		return;
	m_frontendReady = ready;
	if (ready)
		emit refreshRequested();
}

void Runtime::setCurrentCollection(const QString &name)
{
	if (m_collection == name)
		return;
	m_collection = name;
	emit collectionChanged(name);
}

bool Runtime::triggerCell(const QString &panelId, const QString &cellId)
{
	const Panel *panel = m_store.panel(panelId);
	const Cell *cell = panel ? panel->cell(cellId) : nullptr;
	TypeRegistry *reg = TypeRegistry::instance();
	if (!cell || cell->kind != CellKind::Button || !reg || !m_frontendReady)
		return false;

	std::vector<ActionSequence::Step> steps;
	for (const Action &a : cell->actions) {
		ActionTypePtr type = reg->action(a.type);
		if (!type) {
			obs_log(LOG_WARNING, "Skipping action of unknown type '%s' in '%s'", qUtf8Printable(a.type),
				qUtf8Printable(cell->label));
			continue;
		}
		steps.push_back({type, settingsFor(type.get(), a.settings), a.delayMs});
	}

	emit cellTriggered(panelId, cellId);
	if (!steps.empty())
		(new ActionSequence(std::move(steps), this))->start();

	/* Most actions change state; reflect it without waiting for the poll. */
	QTimer::singleShot(50, this, &Runtime::requestRefresh);
	return true;
}

void Runtime::onSourceRename(void *data, calldata_t *cd)
{
	auto *self = static_cast<Runtime *>(data);
	obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	SourceRename rename;
	rename.oldName = utf8(calldata_string(cd, "prev_name"));
	rename.newName = utf8(calldata_string(cd, "new_name"));
	if (source && obs_source_get_type(source) == OBS_SOURCE_TYPE_FILTER) {
		rename.isFilter = true;
		if (obs_source_t *parent = obs_filter_get_parent(source))
			rename.filterParent = utf8(obs_source_get_name(parent));
	}
	if (rename.oldName.isEmpty() || rename.oldName == rename.newName)
		return;

	/* May fire on any thread. */
	QPointer<Runtime> guard(self);
	QMetaObject::invokeMethod(
		self,
		[guard, rename]() {
			if (guard)
				guard->handleRename(rename);
		},
		Qt::QueuedConnection);
}

void Runtime::handleRename(const SourceRename &rename)
{
	TypeRegistry *reg = TypeRegistry::instance();
	if (!reg)
		return;

	/* Only panels in use by the current collection: a panel bound to another
	 * collection may legitimately refer to a different source of that name. */
	const QList<Panel> panels = m_store.panels();
	for (Panel panel : panels) {
		if (!isPanelActive(panel))
			continue;
		bool changed = false;
		for (Cell &cell : panel.cells) {
			for (Action &action : cell.actions) {
				ActionTypePtr type = reg->action(action.type);
				if (!type)
					continue;
				DataPtr settings = dataFromJson(action.settings);
				if (type->sourceRenamed(settings.get(), rename)) {
					action.settings = dataToJson(settings.get());
					changed = true;
				}
			}
			if (cell.kind == CellKind::Widget) {
				WidgetTypePtr type = reg->widget(cell.widgetType);
				DataPtr settings = dataFromJson(cell.widgetSettings);
				if (type && type->sourceRenamed(settings.get(), rename)) {
					cell.widgetSettings = dataToJson(settings.get());
					changed = true;
				}
			}
		}
		if (changed)
			m_store.updatePanel(panel);
	}
}

} // namespace cp
