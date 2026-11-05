/*
Example extension for Control Panel for OBS Studio
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
 * A minimal plugin that adds one action type and one widget type to
 * Control Panel. It loads fine without Control Panel installed: the
 * registration helpers simply return false.
 *
 *   example.flag     - action: toggles a named on/off flag and lights up
 *                      while it's on (shows get_state + get_default_label)
 *   example.counter  - widget: a button that counts clicks (shows a Qt
 *                      widget type with create/update/destroy)
 */

#include "control-panel-api.h"

#include <obs-module.h>

#include <QPushButton>

#include <map>
#include <string>

OBS_DECLARE_MODULE()

namespace {

/* ---- action: example.flag ------------------------------------------- */

struct FlagState {
	std::map<std::string, bool> flags;
};

const char *flagName(void *)
{
	return "Example: Toggle Flag";
}

obs_properties_t *flagProperties(void *, obs_data_t *)
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_text(props, "flag", "Flag name", OBS_TEXT_DEFAULT);
	return props;
}

void flagDefaults(void *, obs_data_t *settings)
{
	obs_data_set_default_string(settings, "flag", "demo");
}

void flagExecute(void *data, obs_data_t *settings)
{
	auto *state = static_cast<FlagState *>(data);
	bool &flag = state->flags[obs_data_get_string(settings, "flag")];
	flag = !flag;
	blog(LOG_INFO, "[example-extension] flag '%s' is now %s", obs_data_get_string(settings, "flag"),
	     flag ? "on" : "off");
}

control_panel_state flagState(void *data, obs_data_t *settings)
{
	auto *state = static_cast<FlagState *>(data);
	auto it = state->flags.find(obs_data_get_string(settings, "flag"));
	return it != state->flags.end() && it->second ? CONTROL_PANEL_STATE_ON : CONTROL_PANEL_STATE_OFF;
}

char *flagLabel(void *, obs_data_t *settings)
{
	return bstrdup(obs_data_get_string(settings, "flag"));
}

void flagFree(void *data)
{
	delete static_cast<FlagState *>(data);
}

/* ---- widget: example.counter ---------------------------------------- */

const char *counterName(void *)
{
	return "Example: Click Counter";
}

obs_properties_t *counterProperties(void *, obs_data_t *)
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_text(props, "prefix", "Prefix", OBS_TEXT_DEFAULT);
	return props;
}

void counterDefaults(void *, obs_data_t *settings)
{
	obs_data_set_default_string(settings, "prefix", "Clicks");
}

void applyCounterText(QPushButton *b, obs_data_t *settings)
{
	b->setText(QStringLiteral("%1: %2")
			   .arg(QString::fromUtf8(obs_data_get_string(settings, "prefix")))
			   .arg(b->property("count").toInt()));
}

void *counterCreate(void *, obs_data_t *settings, void *parent)
{
	auto *button = new QPushButton(static_cast<QWidget *>(parent));
	button->setProperty("count", 0);
	button->setProperty("prefix", QString::fromUtf8(obs_data_get_string(settings, "prefix")));
	applyCounterText(button, settings);
	QObject::connect(button, &QPushButton::clicked, button, [button]() {
		button->setProperty("count", button->property("count").toInt() + 1);
		button->setText(QStringLiteral("%1: %2")
					.arg(button->property("prefix").toString())
					.arg(button->property("count").toInt()));
	});
	return button;
}

void counterUpdate(void *, void *widget, obs_data_t *settings)
{
	auto *button = static_cast<QPushButton *>(widget);
	button->setProperty("prefix", QString::fromUtf8(obs_data_get_string(settings, "prefix")));
	applyCounterText(button, settings);
}

} // namespace

bool obs_module_load(void)
{
	return true;
}

void obs_module_post_load(void)
{
	if (control_panel_get_api_version() < 1) {
		blog(LOG_INFO, "[example-extension] Control Panel not installed; nothing to register");
		return;
	}

	control_panel_action_info flag = {};
	flag.size = sizeof(flag);
	flag.id = "example.flag";
	flag.type_data = new FlagState();
	flag.free_type_data = flagFree;
	flag.get_name = flagName;
	flag.get_properties = flagProperties;
	flag.get_defaults = flagDefaults;
	flag.execute = flagExecute;
	flag.get_state = flagState;
	flag.get_default_label = flagLabel;
	if (!control_panel_register_action_type(&flag))
		flagFree(flag.type_data); /* ownership stays with us on failure */

	control_panel_widget_info counter = {};
	counter.size = sizeof(counter);
	counter.id = "example.counter";
	counter.get_name = counterName;
	counter.get_properties = counterProperties;
	counter.get_defaults = counterDefaults;
	counter.create = counterCreate;
	counter.update = counterUpdate;
	control_panel_register_widget_type(&counter);

	blog(LOG_INFO, "[example-extension] registered with Control Panel API v%d", control_panel_get_api_version());
}

void obs_module_unload(void)
{
	control_panel_unregister_type("example.flag");
	control_panel_unregister_type("example.counter");
}
