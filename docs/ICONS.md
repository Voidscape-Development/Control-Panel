# Icons

Buttons can show an icon from three sources. All are chosen in the cell
inspector's **Icon** picker.

| Source | Reference stored in the panel | Tinted to the cell colors | Portable when exported |
| --- | --- | --- | --- |
| Material Symbols | `material:<style>/<name>[?fill=always\|active]` | yes | yes |
| Built-in set | `builtin:<id>` | yes | yes |
| Image file | the file path | no (drawn as-is) | no (needs the file) |

## Material Symbols

[Material Symbols](https://fonts.google.com/icons) by Google: 3,940 icons
in three styles (`outlined`, `rounded`, `sharp`), each with an outline and a
filled glyph, at weight 400 on the 24 px design grid.

The picker can search names and Google's tags ("dictation" finds `mic`) and
filter by category. You choose the style and how the icon fills:

| Fill | Reference suffix | Shows |
| --- | --- | --- |
| Outline | none | the outline glyph |
| Filled | `?fill=always` | the filled glyph |
| Filled when lit | `?fill=active` | outline normally, filled while the button is active |

Example: `material:rounded/mic?fill=active`.

If an icon is missing (for example a panel shared from a newer release), the
cell simply shows no icon. Settings are never rewritten.

### How they ship

The icons are **data files installed next to the plugin**, not compiled into
it:

```
data/material-symbols/
  outlined.pack  rounded.pack  sharp.pack   glyph path data per style
  index.pack                                names, categories, tags, popularity
  LICENSE                                   Apache License 2.0
  README.md                                 provenance (version, sources, date)
```

Each `.pack` is JSON compressed with zlib and framed for Qt's `qUncompress`
(a 4-byte big-endian length, then the zlib stream). A style holds
`{"name": ["outline path", "filled path"]}` for the viewBox `0 -960 960 960`.
Style packs are only loaded the first time an icon of that style is drawn.
The packs total about 2.7 MB.

### Updating to a newer release

```sh
python3 tools/update-material-symbols.py
```

The script downloads the pinned `@material-symbols/svg-400` npm release and
checks its sha512 integrity. It reads the archive in memory without
extracting it, and checks that every SVG is a single path on the expected
grid and that all three styles contain the same icons. It then joins
Google's Material Symbols metadata, mapping legacy lowercase categories onto
the current ones, and writes the packs. To move to a new release, bump
`PACKAGE_VERSION` and `PACKAGE_INTEGRITY` in the script, run it, review the
summary and commit `data/material-symbols/`. `tests/test-icons.cpp` reads the
committed packs, so a format mismatch fails CI.

### Licensing

Material Symbols are licensed under the **Apache License 2.0**
(`data/material-symbols/LICENSE`). Control Panel is **GPL-3.0-or-later**,
which can include Apache-2.0 material. The icons are kept as separate,
unmodified data files with their license and attribution alongside.

## Future: optional download of newer icon releases (not implemented)

Material Symbols is **offline-only** for now: everything comes from the files
installed with the plugin, and the plugin never contacts Google or any other
server. A later version could add an **opt-in** download of newer releases.
Notes for whoever picks it up:

- **Off by default.** Make it an explicit setting, or a "Check for newer
  icons" button in the picker, never an automatic background request. Show
  the destination host before the first download.
- **Same format, separate location.** Download the same `.pack` files into
  the plugin config directory (`obs_module_config_path`) and prefer them over
  the bundled ones when present and valid. The bundled set stays the
  fallback, so a failed or partial download can never break the picker.
- **Integrity.** Publish packs from this repository's releases with a signed
  or checksummed manifest (version, file sizes, sha256). Don't build them on
  the user's machine from Google's endpoints, which have no stable versioned
  URL for the metadata.
- **Atomic swap.** Download to a temporary directory, verify everything, then
  rename into place. `MaterialLibrary::open()` already rejects unreadable
  packs.
- **Network from the UI thread.** Use `QNetworkAccessManager` asynchronously,
  or OBS's `obs_frontend`/curl helpers on a worker thread. Never block the UI
  thread.
- **Reference stability.** References store icon names. Google sometimes
  renames icons, so keep the last bundled release's name list and map known
  renames, or keep showing the old glyph from the bundled pack.

## Built-in set

28 single-color SVGs in `src/ui/icons/`, compiled into the plugin. They are
original artwork for this project, under the plugin's license.
