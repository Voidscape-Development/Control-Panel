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
 * Built-in action types.
 *
 * Every scene, source, filter and source-owned hotkey is referenced by UUID
 * (settings keys end in "_uuid"), never by name, so renames can't break a
 * panel. UUIDs belong to a scene collection: a panel meant for one
 * collection should be bound to it.
 */

#include "runtime.hpp"
#include "type-registry.hpp"

#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <algorithm>
#include <functional>
#include <vector>

namespace cp {
namespace {

const char *kScene = "scene_uuid";
const char *kSource = "source_uuid";
const char *kFilter = "filter_uuid";

/* ===================================================================== */
/* Hotkey                                                                */
/* ===================================================================== */

/* Hotkey ids change every session, so a hotkey is persisted as
 * registerer type + owner + hotkey name. The owner of a source hotkey is the
 * source UUID. Outputs, encoders and services have no UUID in libobs; their
 * internal (non user-facing, stable) object name is used instead. Frontend
 * hotkeys have no owner. */
struct HotkeyRef {
	int type = OBS_HOTKEY_REGISTERER_FRONTEND;
	QString owner;
	QString name;

	QString encode() const { return QStringLiteral("%1|%2|%3").arg(type).arg(owner, name); }
	static HotkeyRef decode(const QString &s)
	{
		HotkeyRef r;
		const int a = s.indexOf(QLatin1Char('|'));
		const int b = s.lastIndexOf(QLatin1Char('|'));
		if (a < 0 || b <= a)
			return r;
		r.type = s.left(a).toInt();
		r.owner = s.mid(a + 1, b - a - 1);
		r.name = s.mid(b + 1);
		return r;
	}
};

struct HotkeyOwner {
	QString key;     /* persisted: UUID for sources, internal name otherwise */
	QString display; /* shown in the editor */
};

HotkeyOwner hotkeyOwner(obs_hotkey_t *key)
{
	void *reg = obs_hotkey_get_registerer(key);
	switch (obs_hotkey_get_registerer_type(key)) {
	case OBS_HOTKEY_REGISTERER_SOURCE: {
		obs_source_t *s = obs_weak_source_get_source(static_cast<obs_weak_source_t *>(reg));
		HotkeyOwner o{sourceUuid(s), s ? utf8(obs_source_get_name(s)) : QString()};
		obs_source_release(s);
		return o;
	}
	case OBS_HOTKEY_REGISTERER_OUTPUT: {
		obs_output_t *out = obs_weak_output_get_output(static_cast<obs_weak_output_t *>(reg));
		const QString n = out ? utf8(obs_output_get_name(out)) : QString();
		obs_output_release(out);
		return {n, n};
	}
	case OBS_HOTKEY_REGISTERER_ENCODER: {
		obs_encoder_t *e = obs_weak_encoder_get_encoder(static_cast<obs_weak_encoder_t *>(reg));
		const QString n = e ? utf8(obs_encoder_get_name(e)) : QString();
		obs_encoder_release(e);
		return {n, n};
	}
	case OBS_HOTKEY_REGISTERER_SERVICE: {
		obs_service_t *sv = obs_weak_service_get_service(static_cast<obs_weak_service_t *>(reg));
		const QString n = sv ? utf8(obs_service_get_name(sv)) : QString();
		obs_service_release(sv);
		return {n, n};
	}
	default:
		return {};
	}
}

struct HotkeyEntry {
	obs_hotkey_id id;
	HotkeyRef ref;
	QString ownerName;
	QString description;
};

std::vector<HotkeyEntry> enumHotkeys()
{
	std::vector<HotkeyEntry> list;
	obs_enum_hotkeys(
		[](void *data, obs_hotkey_id id, obs_hotkey_t *key) {
			auto *out = static_cast<std::vector<HotkeyEntry> *>(data);
			const HotkeyOwner owner = hotkeyOwner(key);
			HotkeyEntry e;
			e.id = id;
			e.ref.type = obs_hotkey_get_registerer_type(key);
			e.ref.owner = owner.key;
			e.ref.name = utf8(obs_hotkey_get_name(key));
			e.ownerName = owner.display;
			e.description = utf8(obs_hotkey_get_description(key));
			out->push_back(e);
			return true;
		},
		&list);
	return list;
}

bool findHotkey(const HotkeyRef &ref, HotkeyEntry *out)
{
	for (const HotkeyEntry &e : enumHotkeys()) {
		if (e.ref.type == ref.type && e.ref.owner == ref.owner && e.ref.name == ref.name) {
			*out = e;
			return true;
		}
	}
	return false;
}

QString hotkeyDisplay(const HotkeyEntry &e)
{
	if (e.ownerName.isEmpty())
		return e.description;
	return QStringLiteral("%1: %2").arg(e.ownerName, e.description);
}

class HotkeyAction : public ActionType {
public:
	QString id() const override { return QStringLiteral("hotkey"); }
	QString name() const override { return T("Action.Hotkey"); }
	QString category() const override { return T("Category.General"); }

	obs_properties_t *properties(obs_data_t *settings) const override
	{
		obs_properties_t *props = obs_properties_create();
		obs_property_t *list = obs_properties_add_list(props, "hotkey", Tc("Action.Hotkey"),
							       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

		std::vector<HotkeyEntry> entries = enumHotkeys();
		/* Frontend hotkeys first, then per owner, alphabetically. */
		std::sort(entries.begin(), entries.end(), [](const HotkeyEntry &a, const HotkeyEntry &b) {
			if (a.ownerName.isEmpty() != b.ownerName.isEmpty())
				return a.ownerName.isEmpty();
			return QString::localeAwareCompare(hotkeyDisplay(a), hotkeyDisplay(b)) < 0;
		});

		const QString current = utf8(obs_data_get_string(settings, "hotkey"));
		bool found = current.isEmpty();
		obs_property_list_add_string(list, "", "");
		for (const HotkeyEntry &e : entries) {
			if (e.description.isEmpty())
				continue;
			const QString key = e.ref.encode();
			obs_property_list_add_string(list, hotkeyDisplay(e).toUtf8().constData(),
						     key.toUtf8().constData());
			found = found || key == current;
		}
		if (!found) {
			const QString missing = T("Missing").arg(HotkeyRef::decode(current).name);
			const size_t idx = obs_property_list_add_string(list, missing.toUtf8().constData(),
									current.toUtf8().constData());
			obs_property_list_item_disable(list, idx, true);
		}
		return props;
	}

	void execute(obs_data_t *settings) const override
	{
		HotkeyEntry e;
		if (!findHotkey(HotkeyRef::decode(utf8(obs_data_get_string(settings, "hotkey"))), &e)) {
			obs_log(LOG_WARNING, "Hotkey '%s' not found", obs_data_get_string(settings, "hotkey"));
			return;
		}
		obs_hotkey_trigger_routed_callback(e.id, true);
		obs_hotkey_trigger_routed_callback(e.id, false);
	}

	QString defaultLabel(obs_data_t *settings) const override
	{
		HotkeyEntry e;
		if (findHotkey(HotkeyRef::decode(utf8(obs_data_get_string(settings, "hotkey"))), &e))
			return e.description;
		return {};
	}
};

/* ===================================================================== */
/* Scenes                                                                */
/* ===================================================================== */

class SceneSwitchAction : public ActionType {
public:
	QString id() const override { return QStringLiteral("scene_switch"); }
	QString name() const override { return T("Action.SceneSwitch"); }
	QString category() const override { return T("Category.Scenes"); }

	obs_properties_t *properties(obs_data_t *settings) const override
	{
		obs_properties_t *props = obs_properties_create();
		addSourceList(props, kScene, Tc("Scene"), SourceFilter::Scenes, settings);
		obs_property_t *t = obs_properties_add_list(props, "target", Tc("Target"), OBS_COMBO_TYPE_LIST,
							    OBS_COMBO_FORMAT_STRING);
		obs_property_list_add_string(t, Tc("Target.Program"), "program");
		obs_property_list_add_string(t, Tc("Target.Preview"), "preview");
		obs_property_set_long_description(t, Tc("Target.Description"));
		return props;
	}

	void defaults(obs_data_t *settings) const override
	{
		obs_data_set_default_string(settings, "target", "program");
	}

	static bool toPreview(obs_data_t *settings)
	{
		return strcmp(obs_data_get_string(settings, "target"), "preview") == 0 &&
		       obs_frontend_preview_program_mode_active();
	}

	void execute(obs_data_t *settings) const override
	{
		SourcePtr scene = sourceByUuid(obs_data_get_string(settings, kScene));
		if (!scene || !obs_source_is_scene(scene.get()))
			return;
		if (toPreview(settings))
			obs_frontend_set_current_preview_scene(scene.get());
		else
			obs_frontend_set_current_scene(scene.get());
	}

	bool hasState() const override { return true; }
	control_panel_state state(obs_data_t *settings) const override
	{
		SourcePtr scene = sourceByUuid(obs_data_get_string(settings, kScene));
		if (!scene)
			return CONTROL_PANEL_STATE_UNAVAILABLE;
		SourcePtr current(toPreview(settings) ? obs_frontend_get_current_preview_scene()
						      : obs_frontend_get_current_scene());
		return current.get() == scene.get() ? CONTROL_PANEL_STATE_ON : CONTROL_PANEL_STATE_OFF;
	}

	QString defaultLabel(obs_data_t *settings) const override
	{
		return sourceNameByUuid(obs_data_get_string(settings, kScene));
	}
};

class StudioTransitionAction : public ActionType {
public:
	QString id() const override { return QStringLiteral("studio_transition"); }
	QString name() const override { return T("Action.StudioTransition"); }
	QString category() const override { return T("Category.Scenes"); }
	void execute(obs_data_t *) const override
	{
		if (obs_frontend_preview_program_mode_active())
			obs_frontend_preview_program_trigger_transition();
	}
	bool hasState() const override { return true; }
	control_panel_state state(obs_data_t *) const override
	{
		return obs_frontend_preview_program_mode_active() ? CONTROL_PANEL_STATE_NONE
								  : CONTROL_PANEL_STATE_UNAVAILABLE;
	}
	QString defaultLabel(obs_data_t *) const override { return name(); }
};

/* ===================================================================== */
/* Sources                                                               */
/* ===================================================================== */

/* Calls fn for every item of a scene, descending into groups. */
void forEachItem(obs_scene_t *scene, const std::function<bool(obs_sceneitem_t *)> &fn)
{
	struct Ctx {
		const std::function<bool(obs_sceneitem_t *)> *fn;
		bool stop = false;
	} ctx{&fn};
	auto cb = [](obs_scene_t *, obs_sceneitem_t *item, void *param) {
		auto *c = static_cast<Ctx *>(param);
		if (!(*c->fn)(item)) {
			c->stop = true;
			return false;
		}
		if (obs_sceneitem_is_group(item))
			obs_sceneitem_group_enum_items(
				item,
				[](obs_scene_t *, obs_sceneitem_t *child, void *p) {
					auto *cc = static_cast<Ctx *>(p);
					if (!(*cc->fn)(child))
						cc->stop = true;
					return !cc->stop;
				},
				param);
		return !c->stop;
	};
	obs_scene_enum_items(scene, cb, &ctx);
}

/* The scene item showing source_uuid inside scene_uuid. Not ref'd: valid
 * while the scene lives, used synchronously on the UI thread only. */
obs_sceneitem_t *findItem(obs_data_t *settings)
{
	SourcePtr sceneSource = sourceByUuid(obs_data_get_string(settings, kScene));
	obs_scene_t *scene = sceneSource ? obs_scene_from_source(sceneSource.get()) : nullptr;
	const char *want = obs_data_get_string(settings, kSource);
	if (!scene || !*want)
		return nullptr;
	obs_sceneitem_t *found = nullptr;
	forEachItem(scene, [&](obs_sceneitem_t *item) {
		if (strcmp(obs_source_get_uuid(obs_sceneitem_get_source(item)), want) == 0) {
			found = item;
			return false;
		}
		return true;
	});
	return found;
}

void fillSceneItems(obs_property_t *list, const char *sceneUuid, const char *currentUuid)
{
	obs_property_list_clear(list);
	obs_property_list_add_string(list, "", "");

	std::vector<std::pair<std::string, std::string>> items; /* name, uuid */
	SourcePtr sceneSource = sourceByUuid(sceneUuid);
	if (obs_scene_t *scene = sceneSource ? obs_scene_from_source(sceneSource.get()) : nullptr) {
		forEachItem(scene, [&](obs_sceneitem_t *item) {
			obs_source_t *src = obs_sceneitem_get_source(item);
			const std::string uuid = obs_source_get_uuid(src);
			for (const auto &existing : items)
				if (existing.second == uuid)
					return true; /* same source twice in a scene */
			items.emplace_back(obs_source_get_name(src), uuid);
			return true;
		});
	}

	bool found = !currentUuid || !*currentUuid;
	for (const auto &[name, uuid] : items) {
		obs_property_list_add_string(list, name.c_str(), uuid.c_str());
		found = found || uuid == currentUuid;
	}
	if (!found)
		addMissingItem(list, currentUuid);
}

class SourceVisibilityAction : public ActionType {
public:
	QString id() const override { return QStringLiteral("source_visibility"); }
	QString name() const override { return T("Action.SourceVisibility"); }
	QString category() const override { return T("Category.Sources"); }

	obs_properties_t *properties(obs_data_t *settings) const override
	{
		obs_properties_t *props = obs_properties_create();
		obs_property_t *scene = addSourceList(props, kScene, Tc("Scene"), SourceFilter::Scenes, settings);
		obs_property_t *source = obs_properties_add_list(props, kSource, Tc("Source"), OBS_COMBO_TYPE_LIST,
								 OBS_COMBO_FORMAT_STRING);
		fillSceneItems(source, obs_data_get_string(settings, kScene), obs_data_get_string(settings, kSource));
		obs_property_set_modified_callback(scene, [](obs_properties_t *ps, obs_property_t *, obs_data_t *s) {
			fillSceneItems(obs_properties_get(ps, kSource), obs_data_get_string(s, kScene),
				       obs_data_get_string(s, kSource));
			return true;
		});
		addModeList(props, Tc("Mode.Show"), Tc("Mode.Hide"));
		return props;
	}
	void defaults(obs_data_t *settings) const override { obs_data_set_default_string(settings, "mode", "toggle"); }

	void execute(obs_data_t *settings) const override
	{
		if (obs_sceneitem_t *item = findItem(settings))
			obs_sceneitem_set_visible(item, applyMode(modeOf(settings), obs_sceneitem_visible(item)));
	}
	bool hasState() const override { return true; }
	control_panel_state state(obs_data_t *settings) const override
	{
		obs_sceneitem_t *item = findItem(settings);
		if (!item)
			return CONTROL_PANEL_STATE_UNAVAILABLE;
		return obs_sceneitem_visible(item) ? CONTROL_PANEL_STATE_ON : CONTROL_PANEL_STATE_OFF;
	}
	QString defaultLabel(obs_data_t *settings) const override
	{
		return sourceNameByUuid(obs_data_get_string(settings, kSource));
	}
};

void fillFilters(obs_property_t *list, const char *sourceUuid, const char *currentUuid)
{
	obs_property_list_clear(list);
	obs_property_list_add_string(list, "", "");
	std::vector<std::pair<std::string, std::string>> filters; /* name, uuid */
	if (SourcePtr source = sourceByUuid(sourceUuid)) {
		obs_source_enum_filters(
			source.get(),
			[](obs_source_t *, obs_source_t *filter, void *p) {
				static_cast<std::vector<std::pair<std::string, std::string>> *>(p)->emplace_back(
					obs_source_get_name(filter), obs_source_get_uuid(filter));
			},
			&filters);
	}
	bool found = !currentUuid || !*currentUuid;
	for (const auto &[name, uuid] : filters) {
		obs_property_list_add_string(list, name.c_str(), uuid.c_str());
		found = found || uuid == currentUuid;
	}
	if (!found)
		addMissingItem(list, currentUuid);
}

SourcePtr findFilter(obs_data_t *settings)
{
	SourcePtr filter = sourceByUuid(obs_data_get_string(settings, kFilter));
	return filter && obs_source_get_type(filter.get()) == OBS_SOURCE_TYPE_FILTER ? std::move(filter) : nullptr;
}

class FilterToggleAction : public ActionType {
public:
	QString id() const override { return QStringLiteral("filter_toggle"); }
	QString name() const override { return T("Action.FilterToggle"); }
	QString category() const override { return T("Category.Sources"); }

	obs_properties_t *properties(obs_data_t *settings) const override
	{
		obs_properties_t *props = obs_properties_create();
		/* The source only narrows the filter list; the filter UUID alone
		 * identifies the filter. */
		obs_property_t *source = addSourceList(props, kSource, Tc("Source"), SourceFilter::All, settings);
		obs_property_t *filter = obs_properties_add_list(props, kFilter, Tc("Filter"), OBS_COMBO_TYPE_LIST,
								 OBS_COMBO_FORMAT_STRING);
		fillFilters(filter, obs_data_get_string(settings, kSource), obs_data_get_string(settings, kFilter));
		obs_property_set_modified_callback(source, [](obs_properties_t *ps, obs_property_t *, obs_data_t *s) {
			fillFilters(obs_properties_get(ps, kFilter), obs_data_get_string(s, kSource),
				    obs_data_get_string(s, kFilter));
			return true;
		});
		addModeList(props, Tc("Mode.Enable"), Tc("Mode.Disable"));
		return props;
	}
	void defaults(obs_data_t *settings) const override { obs_data_set_default_string(settings, "mode", "toggle"); }

	void execute(obs_data_t *settings) const override
	{
		if (SourcePtr filter = findFilter(settings))
			obs_source_set_enabled(filter.get(),
					       applyMode(modeOf(settings), obs_source_enabled(filter.get())));
	}
	bool hasState() const override { return true; }
	control_panel_state state(obs_data_t *settings) const override
	{
		SourcePtr filter = findFilter(settings);
		if (!filter)
			return CONTROL_PANEL_STATE_UNAVAILABLE;
		return obs_source_enabled(filter.get()) ? CONTROL_PANEL_STATE_ON : CONTROL_PANEL_STATE_OFF;
	}
	QString defaultLabel(obs_data_t *settings) const override
	{
		return sourceNameByUuid(obs_data_get_string(settings, kFilter));
	}
};

class AudioMuteAction : public ActionType {
public:
	QString id() const override { return QStringLiteral("audio_mute"); }
	QString name() const override { return T("Action.AudioMute"); }
	QString category() const override { return T("Category.Audio"); }

	obs_properties_t *properties(obs_data_t *settings) const override
	{
		obs_properties_t *props = obs_properties_create();
		addSourceList(props, kSource, Tc("Source"), SourceFilter::Audio, settings);
		addModeList(props, Tc("Mode.Mute"), Tc("Mode.Unmute"));
		return props;
	}
	void defaults(obs_data_t *settings) const override { obs_data_set_default_string(settings, "mode", "toggle"); }
	void execute(obs_data_t *settings) const override
	{
		if (SourcePtr s = sourceByUuid(obs_data_get_string(settings, kSource)))
			obs_source_set_muted(s.get(), applyMode(modeOf(settings), obs_source_muted(s.get())));
	}
	bool hasState() const override { return true; }
	/* Lit while muted, like the mixer's mute indicator. */
	control_panel_state state(obs_data_t *settings) const override
	{
		SourcePtr s = sourceByUuid(obs_data_get_string(settings, kSource));
		if (!s)
			return CONTROL_PANEL_STATE_UNAVAILABLE;
		return obs_source_muted(s.get()) ? CONTROL_PANEL_STATE_ON : CONTROL_PANEL_STATE_OFF;
	}
	QString defaultLabel(obs_data_t *settings) const override
	{
		return sourceNameByUuid(obs_data_get_string(settings, kSource));
	}
};

class MediaControlAction : public ActionType {
public:
	QString id() const override { return QStringLiteral("media_control"); }
	QString name() const override { return T("Action.MediaControl"); }
	QString category() const override { return T("Category.Sources"); }

	obs_properties_t *properties(obs_data_t *settings) const override
	{
		obs_properties_t *props = obs_properties_create();
		addSourceList(props, kSource, Tc("Source"), SourceFilter::Media, settings);
		obs_property_t *c = obs_properties_add_list(props, "command", Tc("Media.Command"), OBS_COMBO_TYPE_LIST,
							    OBS_COMBO_FORMAT_STRING);
		obs_property_list_add_string(c, Tc("Media.PlayPause"), "play_pause");
		obs_property_list_add_string(c, Tc("Media.Restart"), "restart");
		obs_property_list_add_string(c, Tc("Media.Stop"), "stop");
		obs_property_list_add_string(c, Tc("Media.Next"), "next");
		obs_property_list_add_string(c, Tc("Media.Previous"), "previous");
		return props;
	}
	void defaults(obs_data_t *settings) const override
	{
		obs_data_set_default_string(settings, "command", "play_pause");
	}
	void execute(obs_data_t *settings) const override
	{
		SourcePtr s = sourceByUuid(obs_data_get_string(settings, kSource));
		if (!s)
			return;
		const char *cmd = obs_data_get_string(settings, "command");
		if (strcmp(cmd, "play_pause") == 0) {
			const obs_media_state st = obs_source_media_get_state(s.get());
			if (st == OBS_MEDIA_STATE_PLAYING)
				obs_source_media_play_pause(s.get(), true);
			else if (st == OBS_MEDIA_STATE_ENDED || st == OBS_MEDIA_STATE_STOPPED ||
				 st == OBS_MEDIA_STATE_NONE)
				obs_source_media_restart(s.get());
			else
				obs_source_media_play_pause(s.get(), false);
		} else if (strcmp(cmd, "restart") == 0) {
			obs_source_media_restart(s.get());
		} else if (strcmp(cmd, "stop") == 0) {
			obs_source_media_stop(s.get());
		} else if (strcmp(cmd, "next") == 0) {
			obs_source_media_next(s.get());
		} else if (strcmp(cmd, "previous") == 0) {
			obs_source_media_previous(s.get());
		}
	}
	bool hasState() const override { return true; }
	control_panel_state state(obs_data_t *settings) const override
	{
		SourcePtr s = sourceByUuid(obs_data_get_string(settings, kSource));
		if (!s)
			return CONTROL_PANEL_STATE_UNAVAILABLE;
		if (strcmp(obs_data_get_string(settings, "command"), "play_pause") != 0)
			return CONTROL_PANEL_STATE_NONE;
		return obs_source_media_get_state(s.get()) == OBS_MEDIA_STATE_PLAYING ? CONTROL_PANEL_STATE_ON
										      : CONTROL_PANEL_STATE_OFF;
	}
	QString defaultLabel(obs_data_t *settings) const override
	{
		return sourceNameByUuid(obs_data_get_string(settings, kSource));
	}
};

/* ===================================================================== */
/* Outputs                                                               */
/* ===================================================================== */

/* Start/stop/toggle for frontend-managed outputs and modes. */
class OutputAction : public ActionType {
public:
	struct Spec {
		const char *id;
		const char *nameKey;
		const char *onKey;
		const char *offKey;
		std::function<bool()> active;
		std::function<void(bool)> set;
		std::function<bool()> available; /* may be null */
	};

	explicit OutputAction(Spec spec) : m_spec(std::move(spec)) {}

	QString id() const override { return QString::fromLatin1(m_spec.id); }
	QString name() const override { return T(m_spec.nameKey); }
	QString category() const override { return T("Category.Outputs"); }

	obs_properties_t *properties(obs_data_t *) const override
	{
		obs_properties_t *props = obs_properties_create();
		addModeList(props, Tc(m_spec.onKey), Tc(m_spec.offKey));
		return props;
	}
	void defaults(obs_data_t *settings) const override { obs_data_set_default_string(settings, "mode", "toggle"); }

	void execute(obs_data_t *settings) const override
	{
		if (m_spec.available && !m_spec.available())
			return;
		const bool active = m_spec.active();
		const bool want = applyMode(modeOf(settings), active);
		if (want != active)
			m_spec.set(want);
	}
	bool hasState() const override { return true; }
	control_panel_state state(obs_data_t *) const override
	{
		if (m_spec.available && !m_spec.available())
			return CONTROL_PANEL_STATE_UNAVAILABLE;
		return m_spec.active() ? CONTROL_PANEL_STATE_ON : CONTROL_PANEL_STATE_OFF;
	}
	QString defaultLabel(obs_data_t *) const override { return name(); }

private:
	Spec m_spec;
};

bool replayBufferConfigured()
{
	obs_output_t *out = obs_frontend_get_replay_buffer_output();
	obs_output_release(out);
	return out != nullptr;
}

/* A one-shot action with an optional availability check. */
class SimpleAction : public ActionType {
public:
	SimpleAction(const char *id, const char *nameKey, const char *categoryKey, std::function<void()> run,
		     std::function<bool()> available = nullptr)
		: m_id(id),
		  m_nameKey(nameKey),
		  m_categoryKey(categoryKey),
		  m_run(std::move(run)),
		  m_available(std::move(available))
	{
	}
	QString id() const override { return QString::fromLatin1(m_id); }
	QString name() const override { return T(m_nameKey); }
	QString category() const override { return T(m_categoryKey); }
	void execute(obs_data_t *) const override
	{
		if (!m_available || m_available())
			m_run();
	}
	bool hasState() const override { return m_available != nullptr; }
	control_panel_state state(obs_data_t *) const override
	{
		return !m_available || m_available() ? CONTROL_PANEL_STATE_NONE : CONTROL_PANEL_STATE_UNAVAILABLE;
	}
	QString defaultLabel(obs_data_t *) const override { return name(); }

private:
	const char *m_id;
	const char *m_nameKey;
	const char *m_categoryKey;
	std::function<void()> m_run;
	std::function<bool()> m_available;
};

/* ===================================================================== */
/* Panels                                                                */
/* ===================================================================== */

class PanelShowAction : public ActionType {
public:
	QString id() const override { return QStringLiteral("panel_show"); }
	QString name() const override { return T("Action.PanelShow"); }
	QString category() const override { return T("Category.General"); }

	obs_properties_t *properties(obs_data_t *) const override
	{
		obs_properties_t *props = obs_properties_create();
		obs_property_t *list = obs_properties_add_list(props, "panel", Tc("Panel"), OBS_COMBO_TYPE_LIST,
							       OBS_COMBO_FORMAT_STRING);
		obs_property_list_add_string(list, "", "");
		if (Runtime *rt = Runtime::get())
			for (const Panel &p : rt->store().panels())
				obs_property_list_add_string(list, p.name.toUtf8().constData(),
							     p.id.toUtf8().constData());

		obs_property_t *where = obs_properties_add_list(props, "where", Tc("PanelShow.Where"),
								OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
		obs_property_list_add_string(where, Tc("PanelShow.Dock"), "dock");
		obs_property_list_add_string(where, Tc("PanelShow.Switcher"), "switcher");
		return props;
	}
	void defaults(obs_data_t *settings) const override { obs_data_set_default_string(settings, "where", "dock"); }

	void execute(obs_data_t *settings) const override
	{
		Runtime *rt = Runtime::get();
		if (!rt)
			return;
		const QString id = utf8(obs_data_get_string(settings, "panel"));
		if (strcmp(obs_data_get_string(settings, "where"), "switcher") == 0)
			rt->switchToPanel(id);
		else
			rt->showPanel(id);
	}
	bool hasState() const override { return true; }
	control_panel_state state(obs_data_t *settings) const override
	{
		Runtime *rt = Runtime::get();
		const Panel *p = rt ? rt->store().panel(utf8(obs_data_get_string(settings, "panel"))) : nullptr;
		return p && rt->isPanelActive(*p) ? CONTROL_PANEL_STATE_NONE : CONTROL_PANEL_STATE_UNAVAILABLE;
	}
	QString defaultLabel(obs_data_t *settings) const override
	{
		Runtime *rt = Runtime::get();
		const Panel *p = rt ? rt->store().panel(utf8(obs_data_get_string(settings, "panel"))) : nullptr;
		return p ? p->name : QString();
	}
};

} // namespace

void registerBuiltinActions(TypeRegistry &reg)
{
	reg.addAction(std::make_shared<HotkeyAction>());
	reg.addAction(std::make_shared<PanelShowAction>());
	reg.addAction(std::make_shared<SceneSwitchAction>());
	reg.addAction(std::make_shared<StudioTransitionAction>());
	reg.addAction(std::make_shared<SourceVisibilityAction>());
	reg.addAction(std::make_shared<FilterToggleAction>());
	reg.addAction(std::make_shared<MediaControlAction>());
	reg.addAction(std::make_shared<AudioMuteAction>());

	reg.addAction(std::make_shared<OutputAction>(OutputAction::Spec{
		"stream", "Action.Stream", "Mode.Start", "Mode.Stop", obs_frontend_streaming_active,
		[](bool on) { on ? obs_frontend_streaming_start() : obs_frontend_streaming_stop(); }, nullptr}));
	reg.addAction(std::make_shared<OutputAction>(OutputAction::Spec{
		"record", "Action.Record", "Mode.Start", "Mode.Stop", obs_frontend_recording_active,
		[](bool on) { on ? obs_frontend_recording_start() : obs_frontend_recording_stop(); }, nullptr}));
	reg.addAction(std::make_shared<OutputAction>(OutputAction::Spec{
		"record_pause", "Action.RecordPause", "Mode.Pause", "Mode.Resume", obs_frontend_recording_paused,
		[](bool on) { obs_frontend_recording_pause(on); }, obs_frontend_recording_active}));
	reg.addAction(std::make_shared<OutputAction>(OutputAction::Spec{
		"replay_buffer", "Action.ReplayBuffer", "Mode.Start", "Mode.Stop", obs_frontend_replay_buffer_active,
		[](bool on) { on ? obs_frontend_replay_buffer_start() : obs_frontend_replay_buffer_stop(); },
		replayBufferConfigured}));
	reg.addAction(std::make_shared<OutputAction>(OutputAction::Spec{
		"virtualcam", "Action.VirtualCam", "Mode.Start", "Mode.Stop", obs_frontend_virtualcam_active,
		[](bool on) { on ? obs_frontend_start_virtualcam() : obs_frontend_stop_virtualcam(); }, nullptr}));
	reg.addAction(std::make_shared<OutputAction>(
		OutputAction::Spec{"studio_mode", "Action.StudioMode", "Mode.Enable", "Mode.Disable",
				   obs_frontend_preview_program_mode_active,
				   [](bool on) { obs_frontend_set_preview_program_mode(on); }, nullptr}));

	reg.addAction(std::make_shared<SimpleAction>("replay_save", "Action.ReplaySave", "Category.Outputs",
						     obs_frontend_replay_buffer_save,
						     obs_frontend_replay_buffer_active));
	reg.addAction(std::make_shared<SimpleAction>("screenshot", "Action.Screenshot", "Category.General",
						     obs_frontend_take_screenshot));
}

} // namespace cp
