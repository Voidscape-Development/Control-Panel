# Control Panel extension API

Other OBS plugins can add their own **action types** (things a button can do)
and **widget types** (custom grid cells) to Control Panel. Users then pick them
in the editor exactly like the built-in ones, with a settings form generated
from your `obs_properties_t`.

Everything goes through [`include/control-panel-api.h`](../include/control-panel-api.h).
Copy that single header into your plugin. There is **no link-time dependency**:
every call goes through the global OBS proc handler, so your plugin still loads
when Control Panel is not installed, and each helper then returns `false`.

A complete working example is in
[`examples/example-extension`](../examples/example-extension/example-extension.cpp).

## Lifecycle

```c
void obs_module_post_load(void)       /* Control Panel's procs exist by now */
{
	if (control_panel_get_api_version() < 1)
		return;                        /* not installed */
	control_panel_register_action_type(&my_action);
	control_panel_register_widget_type(&my_widget);
}

void obs_module_unload(void)
{
	control_panel_unregister_type("my-plugin.my-action");
	control_panel_unregister_type("my-plugin.my-widget");
}
```

| Rule | Why |
| --- | --- |
| Register from `obs_module_post_load()` | Module load order isn't defined, but every module's `obs_module_load` has run by then. |
| Call from the UI thread only | Registration from other threads is rejected and logged. `obs_module_post_load`/`obs_module_unload` are on the UI thread. |
| Ids must contain a `.` (e.g. `my-plugin.flash`) | Dot-less ids are reserved for built-ins. Ids are stored in user configs, so never change them. |
| Info structs are copied | They (and the `id` string) can live on the stack. |
| `type_data` ownership | Passes to Control Panel on success and is released with `free_type_data` when you unregister or when Control Panel shuts down. On failure it stays yours. |
| Settings objects are borrowed | Don't release them or keep them after the callback returns. |

Unregistering a type doesn't touch user data: cells using it show a
placeholder (buttons get a red "unavailable" dot) and work again when the type
comes back.

## Action types

```c
struct control_panel_action_info info = {0};
info.size = sizeof(info);               /* required: enables forward compatibility */
info.id = "my-plugin.flash";            /* required */
info.get_name = my_name;                /* required: shown in the "Add action" menu */
info.execute = my_execute;              /* required */
info.get_properties = my_properties;    /* optional: editor form */
info.get_defaults = my_defaults;        /* optional */
info.get_state = my_state;              /* optional: lights up the button */
info.get_default_label = my_label;      /* optional: suggested button text */
info.type_data = my_data;               /* optional */
info.free_type_data = my_free;          /* optional */
control_panel_register_action_type(&info);
```

- `get_properties(type_data, settings)` receives the settings being edited, so
  lists can be filtered on other values. Modified callbacks
  (`obs_property_set_modified_callback`) work like in OBS's own property
  views. Supported property kinds: bool, int, float, text, path, list, color,
  button and group.
- `get_state` is polled about four times a second while the button is visible.
  Keep it cheap. Return `CONTROL_PANEL_STATE_ON`/`OFF` for toggles, `NONE` for
  one-shot actions and `UNAVAILABLE` when the action can't run (the button is
  dimmed). If your state changes outside a poll, call
  `control_panel_request_refresh()` (safe from any thread).
- `get_default_label` returns a `bstrdup`'d string (Control Panel `bfree`s it)
  used to auto-label new buttons, e.g. the name of the selected scene.

A button runs its actions top to bottom. Users can put a delay before any
step, so `execute` should do its work and return rather than block.

## Widget types

For anything that isn't "do something on click": meters, previews, counters.

```c
struct control_panel_widget_info info = {0};
info.size = sizeof(info);
info.id = "my-plugin.meter";
info.get_name = meter_name;
info.create = meter_create;            /* returns a QWidget*, parent is a QWidget* */
info.update = meter_update;            /* optional; NULL = destroy and re-create on change */
info.tick = meter_tick;                /* optional; ~4x/s while visible */
info.destroy = meter_destroy;          /* optional; NULL = Control Panel deletes the widget */
info.get_properties = meter_properties;
info.get_defaults = meter_defaults;
control_panel_register_widget_type(&info);
```

The widget is placed in a frame that paints the cell background and sets the
palette text color and font from the cell style, so plain Qt widgets pick up
the user's styling automatically. Your plugin must be built against the same
Qt major version as OBS, which is already a requirement for any Qt plugin.

All of a panel's widgets are destroyed when OBS sends
`OBS_FRONTEND_EVENT_EXIT`, before any module unloads, so `destroy` always runs
while your plugin is still fully alive.

## Versioning

`CONTROL_PANEL_API_VERSION` is currently `1`. New optional fields are only ever
appended to the info structs. Control Panel copies `min(info.size, its own
size)` bytes, so:

- an older plugin (smaller struct) keeps working with a newer Control Panel:
  the missing fields are treated as `NULL`;
- a newer plugin (larger struct) works with an older Control Panel, which
  ignores fields it doesn't know about.

Check `control_panel_get_api_version()` before relying on fields added in later
versions.

## Proc reference

These are what the header's helpers call. You only need them if you can't use
the header (for example from a script).

| Proc | Parameters |
| --- | --- |
| `control_panel_get_api_version` | `out int version` |
| `control_panel_register_action_type` | `in ptr info, in int api_version, out bool success` |
| `control_panel_register_widget_type` | `in ptr info, in int api_version, out bool success` |
| `control_panel_unregister_type` | `in string id, out bool success` |
| `control_panel_request_refresh` | none |
