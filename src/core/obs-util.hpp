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

#include <obs.h>

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace cp {

/* Localized string from data/locale. */
QString T(const char *key);
/* Same, for use as const char* in OBS property APIs (owned by OBS). */
const char *Tc(const char *key);

/* Shared, ref-counted obs_data_t. makeData adopts the reference passed in. */
using DataPtr = std::shared_ptr<obs_data_t>;
DataPtr makeData(obs_data_t *adopt);
DataPtr dataFromJson(const QJsonObject &json);
QJsonObject dataToJson(obs_data_t *data);

/* RAII for strong source references. */
struct SourceDeleter {
	void operator()(obs_source_t *s) const { obs_source_release(s); }
};
using SourcePtr = std::unique_ptr<obs_source_t, SourceDeleter>;
SourcePtr sourceByName(const char *name);
inline SourcePtr sourceByName(const QString &name)
{
	return sourceByName(name.toUtf8().constData());
}

QString currentSceneCollection();
QStringList sceneCollections();

QString utf8(const char *s);

/* ---- property helpers ---- */

enum class SourceFilter {
	Scenes, /* scenes only */
	Inputs, /* non-scene sources */
	All,    /* scenes and inputs */
	Audio,  /* inputs with audio */
	Media,  /* inputs with controllable media */
};

/* Fills a string list property with source names (sorted). If the current
 * value isn't present (e.g. a source of another collection), it is kept as a
 * "missing" entry so the setting isn't silently lost. */
void fillSourceList(obs_property_t *list, SourceFilter filter, const char *current);
obs_property_t *addSourceList(obs_properties_t *props, const char *key, const char *desc, SourceFilter filter,
			      obs_data_t *settings);

/* Adds a "mode" list: toggle / on / off with custom labels. */
obs_property_t *addModeList(obs_properties_t *props, const char *onLabel, const char *offLabel);

enum class Mode { Toggle, On, Off };
Mode modeOf(obs_data_t *settings);
inline bool applyMode(Mode m, bool current)
{
	return m == Mode::Toggle ? !current : m == Mode::On;
}

} // namespace cp
