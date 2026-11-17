#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks openrac-mod.json against the code (GUIDE.md section 12, the manifest tests).

    python3 tests/manifest_test.py

- id and version: the ones in src/version.h.
- games: the discs src/rac1/discs.h has rows for.
- settings: every key game.cpp reads, with the type it reads it as. The combo's choices are the
  ones combo_choice() knows, and its default is the one it falls back to. The key is a keybind of
  one key, empty by default. Every setting is each player's own (the trigger is a preference, the
  trace a debugging aid), while the mod itself is gameplay (a travel changes the world for a party).
- hooks: the ids of the hook point types game.cpp takes (openrac/games/rac1.hpp names them).
- api: 1.2, the interface that has the keybind setting.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# The hook point types game.cpp takes, by their permanent ids.
HOOK_IDS = {
    "Ship_TravelTo": "rac1.ship.travel_to",
}
TYPES = {"bool": {"bool"}, "std::string": {"choice", "keybind"}}


def main() -> int:
    manifest = json.loads((ROOT / "openrac-mod.json").read_text())
    version_h = (ROOT / "src" / "version.h").read_text()
    discs_h = (ROOT / "src" / "rac1" / "discs.h").read_text()
    game = (ROOT / "src" / "rac1" / "game.cpp").read_text()
    errors: list[str] = []

    def expect(ok: bool, message: str) -> None:
        if not ok:
            errors.append(message)

    # The id and version the library reports.
    expect(f'"{manifest["id"]}"' in version_h, f"id {manifest['id']} is not version.h's")
    expect(f'"{manifest["version"]}"' in version_h, f"version {manifest['version']} is not version.h's")
    expect(manifest["id"].startswith("rac1-"), "the id starts with rac1-")

    # The manifest's discs are the rows in discs.h.
    rows = set(re.findall(r'\{\s*"(\w+)",', discs_h))
    expect(set(manifest["games"]["rac1"]) == rows, f"games {manifest['games']['rac1']} against discs.h's {sorted(rows)}")
    expect(list(manifest["games"]) == ["rac1"], "the mod is for rac1 only")

    # In a party every player needs the mod, but each player's settings are their own.
    expect(manifest.get("multiplayer") == "gameplay", "multiplayer: gameplay (a travel changes the world)")
    expect(manifest.get("api") == "1.2", "api 1.2: the keybind setting")

    # The settings game.cpp reads, and their types.
    read = {key: t for t, key in re.findall(r'setting<(bool|std::string)>\("(\w+)"\)', game)}
    settings = {s["key"]: s for s in manifest["settings"]}
    expect(set(read) == set(settings), f"settings {sorted(settings)} against game.cpp's {sorted(read)}")
    for key, t in read.items():
        s = settings.get(key)
        if s is None:
            continue
        expect(s["type"] in TYPES[t], f"{key}: game.cpp reads a {t}, the manifest says {s['type']}")
        expect(s.get("multiplayer") == "own", f"{key}: each player's own")
        expect(bool(s.get("description")), f"{key}: a description")

    # The combo's choices are the ones combo_choice() knows, and the default is its fallback.
    combo = settings.get("combo", {})
    values = [c["value"] for c in combo.get("choices", [])]
    known = set(re.findall(r'value == "(\w+)"', game))
    expect(set(values) - {combo.get("default")} == known, f"combo choices {values} against combo_choice()'s {sorted(known)}")
    expect(combo.get("default") == "l1_r1_select", "the combo's default is L1 + R1, then Select")
    expect("return TA_COMBO_L1_R1_SELECT;" in game, "combo_choice() falls back to L1 + R1")

    # The key: one bind, empty by default.
    key = settings.get("key", {})
    expect(key.get("type") == "keybind" and key.get("binds") == 1, "key: a keybind of one key")
    expect(key.get("default") == "", "key: none by default")

    # The trace is off by default.
    expect(settings.get("trace", {}).get("default") is False, "trace: off by default")

    # The hook points game.cpp takes.
    taken = {HOOK_IDS[name] for name in re.findall(r"hook<hooks::(\w+)>", game)}
    expect(set(manifest.get("hooks", [])) == taken, f"hooks {manifest.get('hooks')} against game.cpp's {sorted(taken)}")

    for e in errors:
        print("manifest:", e)
    print("manifest_test:", "FAILED" if errors else "ok")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
