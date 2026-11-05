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
  text, active colors and font size, plus icons. Unstyled cells follow the
  OBS theme.
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

Sources are referenced by name, so a panel works across scene collections that
use the same names. Renaming a source or filter updates the panels that are
active for the current collection.

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
tests/                        unit tests for src/model
examples/example-extension/   sample plugin adding an action and a widget type
docs/API.md                   extension API guide
```

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
