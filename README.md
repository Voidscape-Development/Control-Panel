# Control Panel for OBS Studio

Build your own grids of buttons, toggles and status displays, and keep them
docked next to your preview. Make one panel for replays, another for filters,
another for scene switching, and show whichever fits what you're doing right
now.

- **Several panels.** Each panel can be its own OBS dock (float it, tab it,
  snap it), and there's also a combined **Control Panels** dock with a dropdown
  to switch between panels.
- **Grid layout editor.** Inspired by the Looking Glass multiview editor:
  drag across empty squares to create a cell, drag cells to move them, drag
  their edges to span several rows or columns. Use the full editor
  (**Tools → Control Panels…**) or edit in place from a dock's right-click menu.
- **Buttons that do several things.** Each button runs an ordered list of
  actions with an optional delay before any step, for example "save replay,
  wait 500 ms, switch to the replay scene".
- **Live state.** Buttons light up while what they control is on (recording,
  a filter enabled, a source visible, a mic muted...) and dim with a red dot
  when their target is missing.
- **Styling.** Panel-wide defaults and per-cell overrides for background,
  text, active colors and font size. Unstyled cells follow the OBS theme.
- **Icons.** Search Google's Material Symbols library (3,940 icons, three
  styles, outline or filled), use the built-in set, or your own images.
  Icons follow the cell's colors and can fill in when a button lights up.
- **Per-collection panels.** Bind a panel to a scene collection and it only
  appears while that collection is loaded.
- **Hotkeys.** Every panel gets "Show" and "Show in switcher" hotkeys, plus
  next/previous panel hotkeys for the switcher (Settings → Hotkeys).
- **Import / export** panels as JSON to back them up or share them.
- **Remote control** through obs-websocket (Stream Deck, Touch Portal,
  scripts).
- **Extensible.** Other plugins can register their own action and widget types
  through a small C API. See [docs/API.md](docs/API.md).

## Cell types

| Kind | What it does |
| --- | --- |
| Button | Runs its action list on click. Can light up from one of its actions. |
| Label | Static text, for section headers. |
| Status display | Stream time, recording time, replay buffer and virtual camera status, clock, CPU usage, FPS. |
| Volume fader | Volume slider for an audio source (same curve as the OBS mixer), with an optional mute button. |
| Plugin widgets | Anything another plugin registers. |

## Built-in actions

| Category | Actions |
| --- | --- |
| General | Trigger any OBS hotkey, show another panel, take a screenshot |
| Scenes | Switch scene (to program, or to preview in Studio Mode), Studio Mode transition |
| Sources | Show/hide/toggle a source in a scene, enable/disable/toggle a filter, media play/pause/restart/stop/next/previous |
| Audio | Mute/unmute/toggle |
| Outputs | Start/stop/toggle streaming, recording, replay buffer, virtual camera and Studio Mode; pause recording; save replay |

**Trigger Hotkey** lists every registered OBS hotkey (frontend, sources,
outputs and other plugins), so anything you can bind a key to can go on a
panel.

### Scenes and sources are referenced by UUID

Scenes, sources, filters and source-owned hotkeys are stored by their OBS UUID,
never by name, so renaming anything in OBS never breaks a button. Hotkeys
owned by outputs, encoders or services, which have no UUID, use their
internal object name, which users never see or change.

OBS gives each scene collection its own UUIDs. A button that targets a
specific scene or source therefore only works in the collection it was set
up in; elsewhere it shows as unavailable (dimmed, red dot). Bind such panels
to their scene collection. Panels made only of output controls (stream,
record, replay buffer...) and status displays work everywhere.

## Icons

Buttons can show an icon above their label, or on its own with **Show label**
turned off. Click **Icon** in the cell inspector to open the picker:

- **Material Symbols**: Google's library of 3,940 icons in Outlined, Rounded
  and Sharp styles. Search by name or tag ("dictation" finds the microphone)
  and filter by category. Each icon can be shown as an outline, filled, or
  **filled when lit** (outline normally, filled while the button is active).
- **Built-in**: 28 original icons made for this plugin.
- **From file**: your own PNG, JPG, SVG and others, drawn as-is.

Material Symbols and built-in icons take on the cell's text color and switch
to the active text color when the button lights up. They're stored by name
(e.g. `material:rounded/mic?fill=active`), so panels that use them can be
exported and shared. Everything works offline: the Material Symbols ship as
data files with the plugin. See [docs/ICONS.md](docs/ICONS.md) for the format,
how to update to a newer release, and notes on a possible optional download.

## Templates

**Import / Export → Add from Template** in the editor adds a ready-made panel:

- **Streaming Essentials (built-in icons)**
- **Streaming Essentials (Material Symbols)**: the same panel with rounded
  Material icons that fill in when lit.

Both have stream, record, pause, virtual camera, replay buffer, save replay,
Studio Mode, transition and screenshot buttons, plus stream/record timers, a
clock, CPU and FPS. They use no scene or source references, so they work with
any setup. Both are also available as importable files in
[`examples/panels/`](examples/panels/).

## Where things are stored

Panels are global (not part of a scene collection) and live in
`plugin_config/obs-control-panel/panels.json` inside the OBS config folder.
Every save is atomic and keeps the previous version as `panels.json.bak`. If
the file can't be read it is moved to `panels.json.invalid` rather than
overwritten.

## obs-websocket

Vendor name: `obs-control-panel`. `panel` accepts a panel id or name and `cell`
accepts a cell id or label. Every response includes `success` and, on failure,
`error`.

| Request | Data | Response |
| --- | --- | --- |
| `GetPanelList` | | `panels: [{panelId, panelName, rows, cols, collection, active}]` |
| `GetPanel` | `panel` | `panel`: the full panel definition |
| `GetCellStates` | `panel` | `cells: [{cellId, label, state}]` where `state` is `on`, `off`, `none` or `unavailable` |
| `TriggerCell` | `panel`, `cell` | |
| `ShowPanel` | `panel` | |
| `SetSwitcherPanel` | `panel` | |

Event `CellTriggered`: `{panelId, panelName, cellId, label}`.

Example (obs-websocket v5 `CallVendorRequest`):

```json
{
  "requestType": "CallVendorRequest",
  "requestData": {
    "vendorName": "obs-control-panel",
    "requestType": "TriggerCell",
    "requestData": { "panel": "Replay", "cell": "Save + Switch" }
  }
}
```

## Building

Requires OBS Studio 30.0 or newer (built against 31.1.1 by default) and Qt 6.
This repository is based on the
[OBS plugin template](https://github.com/obsproject/obs-plugintemplate), so the
usual presets and CI workflows apply:

```sh
cmake --preset ubuntu-x86_64      # or macos / windows-x64
cmake --build --preset ubuntu-x86_64
```

### Tests

The model layer (panels, grid math, persistence, import/export) has no OBS
dependency and is unit tested with Qt Test:

```sh
cmake -S tests -B build_tests
cmake --build build_tests
ctest --test-dir build_tests --output-on-failure
```

## Project layout

```
include/control-panel-api.h   public extension API (header only)
src/model/                    panels, cells, grid math, storage (Qt Core only)
src/core/                     OBS integration: type registry, built-in actions
                              and widgets, runtime, hotkeys, obs-websocket
src/ui/                       docks, panel view, layout canvas, editor, inspector
src/ui/icons/                 built-in icon set (SVG, compiled into the plugin)
data/material-symbols/        Material Symbols packs (Apache-2.0 data files)
tools/                        asset scripts (update-material-symbols.py)
tests/                        unit tests for src/model
examples/example-extension/   sample plugin adding an action and a widget type
examples/panels/              importable example panels
docs/API.md                   extension API guide
docs/ICONS.md                 icon references, packs, updating, licensing
```

## License

Control Panel is licensed under the **GNU General Public License v3.0 or
later**. See [LICENSE](LICENSE).

It includes [Material Symbols](https://fonts.google.com/icons) by Google,
licensed under the Apache License 2.0, shipped unmodified as separate data
files in `data/material-symbols/` together with that license. The vendored
`deps/obs-websocket-api/obs-websocket-api.h` is from obs-websocket
(GPL-2.0-or-later).
