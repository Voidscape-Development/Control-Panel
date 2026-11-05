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

#include "obs-util.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QJsonDocument>

#include <algorithm>
#include <vector>

namespace cp {

QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

const char *Tc(const char *key)
{
	return obs_module_text(key);
}

QString utf8(const char *s)
{
	return s ? QString::fromUtf8(s) : QString();
}

DataPtr makeData(obs_data_t *adopt)
{
	return DataPtr(adopt, [](obs_data_t *d) { obs_data_release(d); });
}

DataPtr dataFromJson(const QJsonObject &json)
{
	obs_data_t *d = nullptr;
	if (!json.isEmpty())
		d = obs_data_create_from_json(QJsonDocument(json).toJson(QJsonDocument::Compact).constData());
	return makeData(d ? d : obs_data_create());
}

QJsonObject dataToJson(obs_data_t *data)
{
	if (!data)
		return {};
	const char *json = obs_data_get_json(data);
	return json ? QJsonDocument::fromJson(QByteArray(json)).object() : QJsonObject();
}

SourcePtr sourceByUuid(const char *uuid)
{
	if (!uuid || !*uuid)
		return nullptr;
	return SourcePtr(obs_get_source_by_uuid(uuid));
}

QString sourceUuid(const obs_source_t *source)
{
	return source ? utf8(obs_source_get_uuid(source)) : QString();
}

QString sourceNameByUuid(const char *uuid)
{
	SourcePtr s = sourceByUuid(uuid);
	return s ? utf8(obs_source_get_name(s.get())) : QString();
}

QString currentSceneCollection()
{
	char *name = obs_frontend_get_current_scene_collection();
	QString result = utf8(name);
	bfree(name);
	return result;
}

QStringList sceneCollections()
{
	QStringList result;
	char **names = obs_frontend_get_scene_collections();
	for (char **n = names; n && *n; ++n)
		result.append(utf8(*n));
	bfree(names);
	return result;
}

namespace {

bool matches(obs_source_t *source, SourceFilter filter)
{
	const bool isScene = obs_source_is_scene(source);
	const uint32_t flags = obs_source_get_output_flags(source);
	switch (filter) {
	case SourceFilter::Scenes:
		return isScene;
	case SourceFilter::Inputs:
		return !isScene && obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT;
	case SourceFilter::All:
		return isScene || obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT;
	case SourceFilter::Audio:
		return !isScene && (flags & OBS_SOURCE_AUDIO);
	case SourceFilter::Media:
		return !isScene && (flags & OBS_SOURCE_CONTROLLABLE_MEDIA);
	}
	return false;
}

} // namespace

void addMissingItem(obs_property_t *list, const char *uuid)
{
	const QString missing = T("MissingSource").arg(utf8(uuid).left(8));
	const size_t idx = obs_property_list_add_string(list, missing.toUtf8().constData(), uuid);
	obs_property_list_item_disable(list, idx, true);
}

void fillSourceList(obs_property_t *list, SourceFilter filter, const char *currentUuid)
{
	struct Entry {
		std::string name;
		std::string uuid;
	};
	struct Ctx {
		SourceFilter filter;
		std::vector<Entry> entries;
	} ctx{filter, {}};

	auto collect = [](void *param, obs_source_t *source) {
		auto *c = static_cast<Ctx *>(param);
		const char *name = obs_source_get_name(source);
		if (name && *name && matches(source, c->filter))
			c->entries.push_back({name, obs_source_get_uuid(source)});
		return true;
	};

	if (filter != SourceFilter::Scenes)
		obs_enum_sources(collect, &ctx);
	if (filter == SourceFilter::Scenes || filter == SourceFilter::All)
		obs_enum_scenes(collect, &ctx);

	std::sort(ctx.entries.begin(), ctx.entries.end(), [](const Entry &a, const Entry &b) {
		return QString::compare(QString::fromStdString(a.name), QString::fromStdString(b.name),
					Qt::CaseInsensitive) < 0;
	});
	ctx.entries.erase(std::unique(ctx.entries.begin(), ctx.entries.end(),
				      [](const Entry &a, const Entry &b) { return a.uuid == b.uuid; }),
			  ctx.entries.end());

	obs_property_list_clear(list);
	obs_property_list_add_string(list, "", "");
	bool found = !currentUuid || !*currentUuid;
	for (const Entry &e : ctx.entries) {
		obs_property_list_add_string(list, e.name.c_str(), e.uuid.c_str());
		found = found || e.uuid == currentUuid;
	}
	if (!found)
		addMissingItem(list, currentUuid);
}

obs_property_t *addSourceList(obs_properties_t *props, const char *key, const char *desc, SourceFilter filter,
			      obs_data_t *settings)
{
	obs_property_t *p = obs_properties_add_list(props, key, desc, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	fillSourceList(p, filter, settings ? obs_data_get_string(settings, key) : nullptr);
	return p;
}

obs_property_t *addModeList(obs_properties_t *props, const char *onLabel, const char *offLabel)
{
	obs_property_t *p =
		obs_properties_add_list(props, "mode", Tc("Mode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, Tc("Mode.Toggle"), "toggle");
	obs_property_list_add_string(p, onLabel, "on");
	obs_property_list_add_string(p, offLabel, "off");
	return p;
}

Mode modeOf(obs_data_t *settings)
{
	const char *m = obs_data_get_string(settings, "mode");
	if (strcmp(m, "on") == 0)
		return Mode::On;
	if (strcmp(m, "off") == 0)
		return Mode::Off;
	return Mode::Toggle;
}

} // namespace cp
