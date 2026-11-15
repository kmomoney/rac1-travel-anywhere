#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# One scripted play-test run of the mod in OpenRAC's rac1 port, hidden and silent: the port's own
# development switches (OpenRAC docs/port/RAC1_PAL_TESTING.md, DEBUGGING.md, REGRESSION.md) start a
# level, press the pad on a schedule counted from control, write game memory (OPENRAC_WRITE: a
# planet unlocked, the hero's health) and write one JSON line a tick (OPENRAC_TRACE: the game mode
# m, the hero's position hp, rotation hr, state hs, the camera's position cam and forward fw).
# PLAYTEST.md has the routes and what each run showed.
#
#   tools/playtest.sh OUT NAME LEVEL SETTINGS [VAR=VALUE...]
#
# OUT       a scratch folder (made): the run's NAME.jsonl, NAME.log and its plugins list
# SETTINGS  none (no mods), or the mod's settings as JSON: '{"combo": "l1_r1_select", ...}'
# VAR=VALUE more of the port's switches (OPENRAC_WRITE=..., OPENRAC_SHOT=..., ...), and ROUTE=
#           (OPENRAC_PRESS entries after the level's start) and CF= (frames after control) to
#           replace the standard route
#
# The port and its data: DISC (ntsc, the default, or pal: the program openrac-rac1-DISC), OPENRAC (an
# OpenRAC checkout with port/build/release built, default ../OpenRAC), DATA (the disc's data folder as
# the launcher or tools/extractor.py writes it, default ~/OpenRAC/active/rac1/data), PACKAGE (the
# mod's package folder to run, default this checkout's build/package/rac1_travel_anywhere). LEVELS
# (an export of the levels, OpenRAC's assets/port/rac1/ntsc) makes the port draw the world, for
# OPENRAC_SHOT pictures. START (default "100:4000:5,k30:8:3": Cross at the title, then Start every
# 30 frames to skip what plays until control) replaces the presses before control.
set -euo pipefail

HERE=$(cd "$(dirname "$0")/.." && pwd)
DISC=${DISC:-ntsc}
OPENRAC=${OPENRAC:-$HERE/../OpenRAC}
DATA=${DATA:-$HOME/OpenRAC/active/rac1/data}
START=${START:-100:4000:5,k30:8:3}
OUT=$1
NAME=$2
LEVEL=$3
SETTINGS=$4
shift 4

# The standard route, in frames from control (masks: 400 L1, 800 R1, 1 Select, 1000 Triangle,
# 4000 Cross, 8 Start): L1 and R1 held from 60, Select with them at 80 (the planet select opens),
# Triangle at 300 (back out), then 200 frames of play.
ROUTE="c60:C00:20,c80:C01:5,c85:C00:5,c300:1000:5"
CF=500

# The route and its length from the arguments.
for kv in "$@"; do
    case $kv in
        ROUTE=*) ROUTE=${kv#ROUTE=} ;;
        CF=*) CF=${kv#CF=} ;;
    esac
done

mkdir -p "$OUT"
MODS=(--no-mods)

# The mod from this checkout's build, listed with the settings asked for.
if [ "$SETTINGS" != none ]; then
    mkdir -p "$OUT/plugins-$NAME"
    ln -sfn "${PACKAGE:-$HERE/build/package/rac1_travel_anywhere}" "$OUT/plugins-$NAME/rac1-travel-anywhere"
    printf '{"format": 1, "mods": [{"id": "rac1-travel-anywhere", "enabled": true, "settings": %s}]}\n' \
        "$SETTINGS" > "$OUT/plugins-$NAME/plugins.json"
    MODS=(--plugins "$OUT/plugins-$NAME/plugins.json")
fi

EXTRA=()
if [ -n "${LEVELS:-}" ]; then
    EXTRA=(--levels "$LEVELS")
fi

# The switches of the port's own: the ones given, then the route's.
VARS=()
for kv in "$@"; do
    case $kv in
        ROUTE=* | CF=*) ;;
        *) VARS+=("$kv") ;;
    esac
done

env "${VARS[@]}" SDL_AUDIODRIVER=dummy OPENRAC_AUDIO_TICKED=1 OPENRAC_HIDDEN=1 OPENRAC_UNCAPPED=1 \
    OPENRAC_DIRECT=1 OPENRAC_PRESS="$START,$ROUTE" OPENRAC_CONTROL_FRAMES="$CF" \
    OPENRAC_TRACE="$OUT/$NAME.jsonl" \
    timeout 600 "$OPENRAC/port/build/release/openrac-rac1-$DISC" --data "$DATA" --window --no-card \
    --level "$LEVEL" --skip-movies --frames 20000 "${MODS[@]}" "${EXTRA[@]}" > "$OUT/$NAME.log" 2>&1 || true

echo "$NAME: $(grep -c . "$OUT/$NAME.jsonl") ticks, control $(grep -c 'control at' "$OUT/$NAME.log")"
