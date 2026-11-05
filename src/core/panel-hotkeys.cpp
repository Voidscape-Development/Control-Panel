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

#include "panel-hotkeys.hpp"
#include "runtime.hpp"

#include <QJsonArray>
#include <QPointer>
#include <QSet>

namespace cp {

namespace {

QJsonArray toJson(obs_data_array_t *array)
{
	if (!array)
		return {};
	DataPtr wrapper = makeData(obs_data_create());
	obs_data_set_array(wrapper.get(), "b", array);
	return dataToJson(wrapper.get()).value(QStringLiteral("b")).toArray();
}

QJsonArray saveHotkey(obs_hotkey_id id)
{
	obs_data_array_t *array = obs_hotkey_save(id);
	QJsonArray json = toJson(array);
	obs_data_array_release(array);
	return json;
}

void loadHotkey(obs_hotkey_id id, const QJsonArray &json)
{
	if (json.isEmpty())
		return;
	DataPtr wrapper = dataFromJson(QJsonObject{{QStringLiteral("b"), json}});
	obs_data_array_t *array = obs_data_get_array(wrapper.get(), "b");
	obs_hotkey_load(id, array);
	obs_data_array_release(array);
}

} // namespace

PanelHotkeys::PanelHotkeys(QObject *parent) : QObject(parent)
{
	Runtime *rt = Runtime::get();
	const QJsonObject global = rt->store().globalHotkeys();
	m_next = registerHotkey(
		QStringLiteral("ControlPanel.SwitcherNext"), T("Hotkey.Next"),
		[this]() { emit stepSwitcherRequested(1); }, global.value("next").toArray());
	m_prev = registerHotkey(
		QStringLiteral("ControlPanel.SwitcherPrevious"), T("Hotkey.Previous"),
		[this]() { emit stepSwitcherRequested(-1); }, global.value("previous").toArray());

	connect(&rt->store(), &PanelStore::panelsReset, this, &PanelHotkeys::sync);
	connect(&rt->store(), &PanelStore::panelChanged, this, &PanelHotkeys::sync);
	signal_handler_connect(obs_get_signal_handler(), "hotkey_bindings_changed", &PanelHotkeys::onBindingsChanged,
			       this);
	sync();
}

PanelHotkeys::~PanelHotkeys()
{
	signal_handler_disconnect(obs_get_signal_handler(), "hotkey_bindings_changed", &PanelHotkeys::onBindingsChanged,
				  this);
	saveBindings();
	for (auto &[id, e] : m_panels) {
		unregisterHotkey(e.show);
		unregisterHotkey(e.switcher);
	}
	unregisterHotkey(m_next);
	unregisterHotkey(m_prev);
}

std::unique_ptr<PanelHotkeys::Binding> PanelHotkeys::registerHotkey(const QString &name, const QString &description,
								    std::function<void()> fn, const QJsonArray &saved)
{
	auto b = std::make_unique<Binding>();
	b->fn = std::move(fn);
	b->id = obs_hotkey_register_frontend(name.toUtf8().constData(), description.toUtf8().constData(),
					     &PanelHotkeys::onHotkey, b.get());
	loadHotkey(b->id, saved);
	return b;
}

void PanelHotkeys::unregisterHotkey(std::unique_ptr<Binding> &b)
{
	if (b && b->id != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(b->id);
	b.reset();
}

void PanelHotkeys::onHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *b = static_cast<Binding *>(data);
	Runtime *rt = Runtime::get();
	if (!rt)
		return;
	/* Hotkeys may fire off the UI thread; copy the callback so it survives
	 * the binding being unregistered before the queued call runs. */
	std::function<void()> fn = b->fn;
	QMetaObject::invokeMethod(rt, [fn]() { fn(); }, Qt::QueuedConnection);
}

void PanelHotkeys::onBindingsChanged(void *data, calldata_t *)
{
	auto *self = static_cast<PanelHotkeys *>(data);
	/* Coalesce bursts (OBS emits one per hotkey when settings are saved). */
	QPointer<PanelHotkeys> guard(self);
	QMetaObject::invokeMethod(
		self,
		[guard]() {
			if (!guard || guard->m_savePending)
				return;
			guard->m_savePending = true;
			QMetaObject::invokeMethod(
				guard.data(),
				[guard]() {
					if (guard) {
						guard->m_savePending = false;
						guard->saveBindings();
					}
				},
				Qt::QueuedConnection);
		},
		Qt::QueuedConnection);
}

void PanelHotkeys::sync()
{
	Runtime *rt = Runtime::get();
	QSet<QString> seen;
	for (const Panel &p : rt->store().panels()) {
		seen.insert(p.id);
		auto it = m_panels.find(p.id);
		if (it == m_panels.end()) {
			PanelEntry e;
			e.name = p.name;
			const QString id = p.id;
			e.show = registerHotkey(
				QStringLiteral("ControlPanel.Show.") + id, T("Hotkey.Show").arg(p.name),
				[id]() {
					if (Runtime *r = Runtime::get())
						r->showPanel(id);
				},
				p.hotkeys.value("show").toArray());
			e.switcher = registerHotkey(
				QStringLiteral("ControlPanel.Switch.") + id, T("Hotkey.Switch").arg(p.name),
				[id]() {
					if (Runtime *r = Runtime::get())
						r->switchToPanel(id);
				},
				p.hotkeys.value("switch").toArray());
			m_panels.emplace(p.id, std::move(e));
		} else if (it->second.name != p.name) {
			PanelEntry &e = it->second;
			e.name = p.name;
			obs_hotkey_set_description(e.show->id, T("Hotkey.Show").arg(p.name).toUtf8().constData());
			obs_hotkey_set_description(e.switcher->id, T("Hotkey.Switch").arg(p.name).toUtf8().constData());
		}
	}
	for (auto it = m_panels.begin(); it != m_panels.end();) {
		if (seen.contains(it->first)) {
			++it;
			continue;
		}
		unregisterHotkey(it->second.show);
		unregisterHotkey(it->second.switcher);
		it = m_panels.erase(it);
	}
}

void PanelHotkeys::saveBindings()
{
	Runtime *rt = Runtime::get();
	if (!rt)
		return;
	for (const auto &[id, e] : m_panels) {
		QJsonObject hk;
		hk.insert(QStringLiteral("show"), saveHotkey(e.show->id));
		hk.insert(QStringLiteral("switch"), saveHotkey(e.switcher->id));
		rt->store().setPanelHotkeys(id, hk);
	}
	QJsonObject global;
	if (m_next)
		global.insert(QStringLiteral("next"), saveHotkey(m_next->id));
	if (m_prev)
		global.insert(QStringLiteral("previous"), saveHotkey(m_prev->id));
	rt->store().setGlobalHotkeys(global);
}

} // namespace cp
