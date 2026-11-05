/*
Control Panel for OBS Studio - public extension API
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
 * Copy this header into your plugin to add new action types (things a
 * button can do) and widget types (custom grid cells) to Control Panel.
 *
 * There is no link-time dependency: every call goes through the global OBS
 * proc handler, so your plugin loads fine whether or not Control Panel is
 * installed. All helpers return false when Control Panel is not available.
 *
 * Rules:
 *  - Register from obs_module_post_load() (Control Panel is guaranteed to have
 *    loaded its procs by then). Unregister from obs_module_unload().
 *  - Every callback is invoked on the OBS UI (Qt main) thread.
 *  - Info structs are copied (including the id string) during registration,
 *    so they may live on the stack. type_data is owned by Control Panel after
 *    a successful registration and is released via free_type_data (if set)
 *    when the type is unregistered or Control Panel shuts down. On failure
 *    ownership stays with the caller.
 *  - Ids must be globally unique. Prefix them with your plugin name, e.g.
 *    "my-plugin.do-thing". Ids without a '.' are reserved for built-ins.
 *  - Settings objects passed to callbacks are owned by Control Panel; do not
 *    release them and do not keep them past the call.
 *  - Widget types hand Control Panel a QWidget*. Your plugin must be built
 *    against the same Qt major version as OBS (which plugins already must).
 *
 * See docs/API.md for a full walkthrough.
 */

#pragma once

#include <obs.h>
#include <callback/calldata.h>
#include <callback/proc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONTROL_PANEL_API_VERSION 1

/* State reported by an action, used to light up a button. */
enum control_panel_state {
	/* The action has no on/off notion (e.g. "save replay"). */
	CONTROL_PANEL_STATE_NONE = 0,
	CONTROL_PANEL_STATE_OFF = 1,
	CONTROL_PANEL_STATE_ON = 2,
	/* The action cannot currently run (missing source, output not configured, ...). */
	CONTROL_PANEL_STATE_UNAVAILABLE = 3,
};

/*
 * An action is one step in a button's action list.
 *
 * Required: size, id, get_name, execute.
 */
struct control_panel_action_info {
	/* Must be sizeof(struct control_panel_action_info). Allows new fields to
	 * be appended in later API versions without breaking older plugins. */
	size_t size;

	/* Unique, stable identifier persisted in user configs. Never change it. */
	const char *id;

	/* Opaque pointer passed back to every callback. */
	void *type_data;
	void (*free_type_data)(void *type_data);

	/* Localized display name, e.g. "Toggle Blur Filter". */
	const char *(*get_name)(void *type_data);

	/* Optional. Properties shown in the editor for this action. The settings
	 * currently being edited are passed so lists can be filtered. Modified
	 * callbacks (obs_property_set_modified_callback) are honored. */
	obs_properties_t *(*get_properties)(void *type_data, obs_data_t *settings);

	/* Optional. Fill defaults into settings via obs_data_set_default_*. */
	void (*get_defaults)(void *type_data, obs_data_t *settings);

	/* Perform the action. */
	void (*execute)(void *type_data, obs_data_t *settings);

	/* Optional. Polled a few times per second while the button is visible.
	 * Keep it cheap. NULL means CONTROL_PANEL_STATE_NONE. */
	enum control_panel_state (*get_state)(void *type_data, obs_data_t *settings);

	/* Optional. Suggested button label for these settings, e.g. the scene
	 * name. Return a string allocated with bstrdup (Control Panel bfree()s it)
	 * or NULL for no suggestion. */
	char *(*get_default_label)(void *type_data, obs_data_t *settings);
};

/*
 * A widget type is a whole grid cell rendered by your plugin (a meter, a
 * chat preview, ...). Buttons with action lists are built in; use an action
 * type instead if all you need is "do something on click".
 *
 * Required: size, id, get_name, create.
 */
struct control_panel_widget_info {
	size_t size; /* sizeof(struct control_panel_widget_info) */
	const char *id;

	void *type_data;
	void (*free_type_data)(void *type_data);

	const char *(*get_name)(void *type_data);
	obs_properties_t *(*get_properties)(void *type_data, obs_data_t *settings);
	void (*get_defaults)(void *type_data, obs_data_t *settings);

	/* Create the cell widget. parent is a QWidget*; return a QWidget* (or
	 * NULL on failure). The widget is resized by Control Panel to fill its
	 * grid cell. */
	void *(*create)(void *type_data, obs_data_t *settings, void *parent);

	/* Optional. Settings changed in the editor. If NULL the widget is
	 * destroyed and re-created instead. */
	void (*update)(void *type_data, void *widget, obs_data_t *settings);

	/* Optional. Called about four times per second while visible. */
	void (*tick)(void *type_data, void *widget, obs_data_t *settings);

	/* Optional. Destroy the widget. If NULL Control Panel deletes the QWidget. */
	void (*destroy)(void *type_data, void *widget);
};

/* ------------------------------------------------------------------------ */
/* Helpers. These are static inline so the header is all you need.          */

static inline bool control_panel_call_(const char *proc, calldata_t *cd)
{
	proc_handler_t *ph = obs_get_proc_handler();
	return ph && proc_handler_call(ph, proc, cd);
}

/* Returns the API version Control Panel implements, or 0 if not installed. */
static inline int control_panel_get_api_version(void)
{
	calldata_t cd = {0, 0, 0, 0};
	int version = 0;
	if (control_panel_call_("control_panel_get_api_version", &cd))
		version = (int)calldata_int(&cd, "version");
	calldata_free(&cd);
	return version;
}

static inline bool control_panel_register_action_type(const struct control_panel_action_info *info)
{
	calldata_t cd = {0, 0, 0, 0};
	bool ok = false;
	calldata_set_ptr(&cd, "info", (void *)info);
	calldata_set_int(&cd, "api_version", CONTROL_PANEL_API_VERSION);
	if (control_panel_call_("control_panel_register_action_type", &cd))
		ok = calldata_bool(&cd, "success");
	calldata_free(&cd);
	return ok;
}

static inline bool control_panel_register_widget_type(const struct control_panel_widget_info *info)
{
	calldata_t cd = {0, 0, 0, 0};
	bool ok = false;
	calldata_set_ptr(&cd, "info", (void *)info);
	calldata_set_int(&cd, "api_version", CONTROL_PANEL_API_VERSION);
	if (control_panel_call_("control_panel_register_widget_type", &cd))
		ok = calldata_bool(&cd, "success");
	calldata_free(&cd);
	return ok;
}

/* Unregister an action or widget type. Cells using it show a placeholder
 * until the type is registered again; their settings are preserved. */
static inline bool control_panel_unregister_type(const char *id)
{
	calldata_t cd = {0, 0, 0, 0};
	bool ok = false;
	calldata_set_string(&cd, "id", id);
	if (control_panel_call_("control_panel_unregister_type", &cd))
		ok = calldata_bool(&cd, "success");
	calldata_free(&cd);
	return ok;
}

/* Ask all panels to re-query action states right away (instead of waiting
 * for the next poll), e.g. after your plugin's state changed. */
static inline void control_panel_request_refresh(void)
{
	calldata_t cd = {0, 0, 0, 0};
	control_panel_call_("control_panel_request_refresh", &cd);
	calldata_free(&cd);
}

#ifdef __cplusplus
}
#endif
