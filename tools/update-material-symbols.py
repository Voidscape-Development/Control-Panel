#!/usr/bin/env python3
"""
Build the Material Symbols icon packs shipped with Control Panel.

Downloads a pinned release of the Material Symbols SVGs (weight 400; outlined,
rounded and sharp, each with a filled variant) and Google's Material Symbols
metadata (categories and search tags), validates every icon and writes:

    data/material-symbols/outlined.pack   } one per style: zlib-compressed JSON
    data/material-symbols/rounded.pack    } {"name": ["path", "filled path"]}
    data/material-symbols/sharp.pack      }
    data/material-symbols/index.pack      names, categories, tags, popularity
    data/material-symbols/LICENSE         Apache License 2.0 (the icons' license)
    data/material-symbols/README.md       provenance and attribution

Packs use Qt's qCompress framing (4-byte big-endian length + zlib stream) so
the plugin can read them with qUncompress. They are data files installed next
to the plugin, not compiled into it.

Usage:  python3 tools/update-material-symbols.py [--out data/material-symbols]

To update: bump PACKAGE_VERSION and PACKAGE_INTEGRITY (from
`npm view @material-symbols/svg-400@<version> dist.integrity`), run the
script, check the summary and commit the regenerated files.
"""

import argparse
import base64
import datetime
import hashlib
import io
import json
import re
import struct
import sys
import tarfile
import urllib.request
import zlib
from pathlib import Path

PACKAGE = "@material-symbols/svg-400"
PACKAGE_VERSION = "0.48.3"
PACKAGE_INTEGRITY = (
    "sha512-Zzt/m3ITfPnQWjdVl5JwHgeWaJLETMYBRvZqs8qXn0tNO+uAuZK/SaDzWEOIxkxZPIEVJpCXpKnVfQAjHshsAQ=="
)
TARBALL_URL = f"https://registry.npmjs.org/{PACKAGE}/-/svg-400-{PACKAGE_VERSION}.tgz"
METADATA_URL = "https://fonts.google.com/metadata/icons?key=material_symbols&incomplete=true"

STYLES = ("outlined", "rounded", "sharp")
VIEWBOX = "0 -960 960 960"
SVG_RE = re.compile(
    r'^<svg xmlns="http://www.w3.org/2000/svg" width="\d+" height="\d+" '
    r'viewBox="(?P<vb>[^"]+)"><path d="(?P<d>[A-Za-z0-9.,\- ]+)"/></svg>\s*$'
)

# Google's metadata mixes the current title-case categories with legacy
# lowercase ones; ~half the icons only have legacy ones. Map them across.
LEGACY_CATEGORIES = {
    "action": "Actions",
    "alert": "UI actions",
    "av": "Audio&Video",
    "communication": "Communicate",
    "content": "Actions",
    "device": "Hardware",
    "editor": "Text",
    "file": "Actions",
    "hardware": "Hardware",
    "home": "Home",
    "image": "Images",
    "maps": "Maps",
    "navigation": "UI actions",
    "notification": "Communicate",
    "places": "Travel",
    "search": "UI actions",
    "social": "Social",
    "toggle": "UI actions",
}


def fetch(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": "control-panel-asset-builder"})
    with urllib.request.urlopen(req, timeout=120) as r:
        return r.read()


def verify_integrity(data: bytes) -> None:
    algo, expected = PACKAGE_INTEGRITY.split("-", 1)
    actual = base64.b64encode(hashlib.new(algo, data).digest()).decode()
    if actual != expected:
        sys.exit(f"Integrity mismatch for {TARBALL_URL}: got {algo}-{actual}")


def qcompress(raw: bytes) -> bytes:
    return struct.pack(">I", len(raw)) + zlib.compress(raw, 9)


def read_icons(tarball: bytes) -> dict:
    """{style: {name: [path, filled_path]}} read in memory, never extracted."""
    styles = {s: {} for s in STYLES}
    with tarfile.open(fileobj=io.BytesIO(tarball), mode="r:gz") as tar:
        for member in tar:
            if not member.isfile():
                continue
            parts = member.name.split("/")
            if len(parts) != 3 or parts[0] != "package" or parts[1] not in STYLES or not parts[2].endswith(".svg"):
                continue
            name = parts[2][:-4]
            filled = name.endswith("-fill")
            if filled:
                name = name[:-5]
            if not re.fullmatch(r"[a-z0-9_]+", name):
                sys.exit(f"Unexpected icon name: {member.name}")
            svg = tar.extractfile(member).read().decode("utf-8")
            m = SVG_RE.match(svg)
            if not m or m.group("vb") != VIEWBOX:
                sys.exit(f"Unexpected SVG structure in {member.name}")
            entry = styles[parts[1]].setdefault(name, ["", ""])
            entry[1 if filled else 0] = m.group("d")
    for style, icons in styles.items():
        incomplete = [n for n, (o, f) in icons.items() if not o or not f]
        if incomplete:
            sys.exit(f"{style}: icons missing a variant: {incomplete[:10]}")
    if not (styles["outlined"].keys() == styles["rounded"].keys() == styles["sharp"].keys()):
        sys.exit("Styles contain different icon sets")
    return styles


def build_index(names: set) -> list:
    text = fetch(METADATA_URL).decode("utf-8")
    meta = json.loads(text.split("\n", 1)[1])  # strip the )]}' XSSI prefix
    by_name = {i["name"]: i for i in meta["icons"]}
    index = []
    for name in sorted(names):
        info = by_name.get(name, {})
        cats = []
        for c in info.get("categories", []):
            c = LEGACY_CATEGORIES.get(c, c)
            if c not in cats:
                cats.append(c)
        tags = sorted({t.lower() for t in info.get("tags", [])} - {name.replace("_", " ")})
        index.append({"n": name, "c": cats, "t": tags, "p": info.get("popularity", 0)})
    missing = [n for n in names if n not in by_name]
    if missing:
        print(f"note: {len(missing)} icons have no metadata (searchable by name only)")
    return index


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default="data/material-symbols", type=Path)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    print(f"Downloading {PACKAGE}@{PACKAGE_VERSION} ...")
    tarball = fetch(TARBALL_URL)
    verify_integrity(tarball)
    styles = read_icons(tarball)

    for style, icons in styles.items():
        raw = json.dumps(dict(sorted(icons.items())), separators=(",", ":")).encode()
        (args.out / f"{style}.pack").write_bytes(qcompress(raw))

    print("Downloading Material Symbols metadata ...")
    index = build_index(set(styles["outlined"]))
    raw = json.dumps({"version": PACKAGE_VERSION, "icons": index}, separators=(",", ":")).encode()
    (args.out / "index.pack").write_bytes(qcompress(raw))

    with tarfile.open(fileobj=io.BytesIO(tarball), mode="r:gz") as tar:
        license_text = tar.extractfile("package/LICENSE").read()
    (args.out / "LICENSE").write_bytes(license_text)

    today = datetime.date.today().isoformat()
    categories = sorted({c for i in index for c in i["c"]})
    (args.out / "README.md").write_text(
        f"""# Material Symbols

These icons are **Material Symbols** by Google, licensed under the
[Apache License 2.0](LICENSE). They are unmodified apart from repackaging:
only the SVG path data is kept, compressed into one file per style.

| | |
| --- | --- |
| Icons | {len(index)} per style (outlined, rounded, sharp), each with a filled variant |
| Weight / grade / size | 400 / 0 / 24 px design grid |
| SVG source | npm `{PACKAGE}@{PACKAGE_VERSION}` ({PACKAGE_INTEGRITY[:20]}...) |
| Metadata | {METADATA_URL} (fetched {today}) |
| Upstream | https://github.com/google/material-design-icons |

Categories: {", ".join(categories)}.

Generated by `tools/update-material-symbols.py`; do not edit by hand.
""",
        encoding="utf-8",
    )

    total = sum((args.out / f"{s}.pack").stat().st_size for s in STYLES) + (args.out / "index.pack").stat().st_size
    print(f"Wrote {len(index)} icons x {len(STYLES)} styles x 2 fills to {args.out} ({total / 1e6:.2f} MB)")


if __name__ == "__main__":
    main()
