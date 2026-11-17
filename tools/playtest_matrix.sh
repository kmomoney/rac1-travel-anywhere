#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs every play-test route on one disc (PLAYTEST.md, "The routes") through tools/playtest.sh,
# then reads the results back with tools/playtest_analyze.py.
#
#   tools/playtest_matrix.sh ntsc|pal OUT
#
# OUT receives each run's NAME.jsonl (the port's trace), NAME.log (the game's log, including the
# mod's trace) and, for the runs with pictures, shots/NAME_<frame>.png. DATA is the disc's data
# folder (default ~/OpenRAC/active/rac1/data for ntsc; required for pal), OPENRAC the port (default
# ../OpenRAC), and LEVELS the level export the pictures need (default
# $OPENRAC/assets/port/rac1/ntsc, which the PAL port can draw with too).
set -u

HERE=$(cd "$(dirname "$0")/.." && pwd)
DISC=$1
OUT=$2
PT=$HERE/tools/playtest.sh
OPENRAC=${OPENRAC:-$HERE/../OpenRAC}
LEVELS_DIR=${LEVELS:-$OPENRAC/assets/port/rac1/ntsc}
MOD='{"combo": "l1_r1_select", "trace": true}'
mkdir -p "$OUT/shots"
unset LEVELS

# The game memory the routes write, at each disc's addresses: the planet list's second entry
# (gGalaxyList + 4), the planets' flags (level_available), Novalis's done items (the saved mission
# state + 16), the hero's health and his x position (y follows it).
if [ "$DISC" = pal ]; then
    : "${DATA:?give the data folder of the PAL disc in DATA}"
    GAL=13D61C AVAIL=13DE48 NOVA=14C160 HP=1416F8 POS=13F4D0
else
    DATA=${DATA:-$HOME/OpenRAC/active/rac1/data}
    GAL=13D514 AVAIL=13DD40 NOVA=14C060 HP=1415F8 POS=13F3D0
fi
export DISC DATA OPENRAC

hex() { printf %X $((0x$1 + $2)); }
# Prints a combo route: L1 and R1 held from 20 frames before frame F, Select with them at F.
combo() { echo "c$(($1 - 20)):C00:20,c$1:C01:3,c$(($1 + 3)):C00:2"; }

OPEN="c30:0:60:128:0,c100:C00:20,c120:C01:5,c125:C00:5,c340:1000:5"
NOVALIS_DONE="300:$NOVA:FFFFFFFF:300,300:$(hex $NOVA 4):FFFFFFFF:300"
NOVALIS_DONE="$NOVALIS_DONE,300:$(hex $NOVA 8):FFFFFFFF:300,300:$(hex $NOVA 12):FFFFFFFF:300"
UNLOCK="c5:$GAL:1:4000,c5:$(hex $GAL 4):2:4000,c5:$AVAIL:1010100:4000"
# Unlocks Aridia after Kerwan; holds Ratchet by Kerwan's crates at (276, 91) for 12 frames.
ARIDIA="c5:$GAL:2:9000,c5:$AVAIL:1010000:9000"
CRATES="c240:$POS:438A0000:12,c240:$(hex $POS 4):42B60000:12"
# A route: run 200 frames over the loose bolts, wrench the crates open, open the menu at 380.
BREAK="c30:0:200:128:0,c270:8000:3,c290:8000:3,c310:8000:3,c330:8000:3,$(combo 380)"

# Open on foot and back out: Kerwan, Veldin, Novalis (its dropship scene marked done).
LEVELS=$LEVELS_DIR $PT "$OUT" open3 3 "$MOD" "ROUTE=$OPEN" CF=600 \
    OPENRAC_SHOT=50:"$OUT"/shots/open3_ OPENRAC_SHOT_FROM=1300
LEVELS=$LEVELS_DIR $PT "$OUT" open0 0 "$MOD" "ROUTE=$OPEN" CF=600 \
    OPENRAC_SHOT=50:"$OUT"/shots/open0_ OPENRAC_SHOT_FROM=1300
LEVELS=$LEVELS_DIR $PT "$OUT" open1 1 "$MOD" "ROUTE=$OPEN" CF=600 OPENRAC_WRITE="$NOVALIS_DONE" \
    OPENRAC_SHOT=50:"$OUT"/shots/open1_ OPENRAC_SHOT_FROM=550
# Mid-air: hold Cross, press the combo while he rises, back out.
$PT "$OUT" air3 3 "$MOD" "ROUTE=c30:4000:12,c34:C00:6,c40:C01:3,c43:C00:2,c240:1000:5" CF=400
# Travel from Kerwan to Novalis (Novalis and Aridia unlocked), through to the arrival.
LEVELS=$LEVELS_DIR START="100:4000:5,k30:8:3,S30:8:3" $PT "$OUT" travel 3 "$MOD" \
    "ROUTE=c60:C00:20,c80:C01:5,c85:C00:5,c160:40:3,c200:4000:3,c260:4000:3" CF=2800 \
    OPENRAC_WRITE="$UNLOCK" OPENRAC_SHOT=100:"$OUT"/shots/travel_ OPENRAC_SHOT_FROM=1300
# Choose the planet he is on after a short run from the start: the ship travels there and the
# level restarts.
LEVELS=$LEVELS_DIR $PT "$OUT" land 3 "$MOD" \
    "ROUTE=c30:0:60:128:0,c100:C00:20,c120:C01:5,c125:C00:5,c240:4000:3,c300:4000:3" CF=2600 \
    OPENRAC_SHOT=100:"$OUT"/shots/land_ OPENRAC_SHOT_FROM=1300
# Down: health 0 at 100, the combo while he dies (110, 160, 210) and once respawned (260).
$PT "$OUT" death 3 "$MOD" "ROUTE=$(combo 110),$(combo 160),$(combo 210),$(combo 260),c400:1000:5" \
    CF=600 OPENRAC_WRITE=c100:$HP:0:1
# A cutscene: Novalis's intro, not skipped, with the combo pressed twice during it (window frames).
START="100:4000:5,880:C00:20,900:C01:3,903:C00:2,1480:C00:20,1500:C01:3,1503:C00:2" \
    $PT "$OUT" scene 1 "$MOD" "ROUTE=c10:0:1" CF=100
# Clank's start on Orxon.
$PT "$OUT" clank 10 "$MOD" "ROUTE=$(combo 80)" CF=200
# Novalis as a new game starts it, with the ship hidden until the dropship scene.
$PT "$OUT" noship 1 "$MOD" "ROUTE=$(combo 120)" CF=200
# Choose the planet he is on after a long run from the start (200 frames).
LEVELS=$LEVELS_DIR $PT "$OUT" farland 3 "$MOD" \
    "ROUTE=c30:0:200:128:0,$(combo 300),c420:4000:3,c480:4000:3" CF=2800 \
    OPENRAC_SHOT=100:"$OUT"/shots/farland_ OPENRAC_SHOT_FROM=1500
# Veldin's first visit, which the ship's list never holds: Blast Off there is the game's landing.
LEVELS=$LEVELS_DIR $PT "$OUT" land0 0 "$MOD" "ROUTE=$(combo 120),c240:4000:3,c300:4000:3" CF=1300 \
    OPENRAC_SHOT=100:"$OUT"/shots/land0_ OPENRAC_SHOT_FROM=1300
# What a restart resets: collect loose bolts, break crates, then Blast Off on Kerwan. Then the same
# followed by a trip to Aridia and back, to compare with an arrival from another planet.
$PT "$OUT" resets 3 "$MOD" "ROUTE=$BREAK,c480:4000:3,c540:4000:3" CF=2800 \
    OPENRAC_WRITE="$CRATES" OPENRAC_MOBY_LOG=100:40
START="100:4000:5,k30:8:3,S30:8:3" $PT "$OUT" roundtrip 3 "$MOD" \
    "ROUTE=$BREAK,c440:40:3,c480:4000:3,c540:4000:3,$(combo 2500),c2560:10:3,c2600:4000:3,c2660:4000:3" \
    CF=4900 OPENRAC_WRITE="$CRATES,$ARIDIA" OPENRAC_MOBY_LOG=100:40
# Pressing the combo again in the planet select closes it, as Select does there.
$PT "$OUT" toggle 3 "$MOD" "ROUTE=$(combo 120),$(combo 260)" CF=400
# No trigger: running, a jump, L1, R1, both, Select alone (the map), the pause menu, a run and a
# jump, once without the mod and once with it.
SAME="c30:0:90:128:0,c130:4000:5,c200:400:30,c250:800:30,c300:C00:30,c360:1:5,c420:1000:5"
SAME="$SAME,c480:8:5,c540:8:5,c600:0:60:0:128,c680:4000:5"
$PT "$OUT" same_base 3 none "ROUTE=$SAME" CF=750
$PT "$OUT" same_mod 3 '{"combo": "l1_r1_select"}' "ROUTE=$SAME" CF=750

# Summarise each run.
A=$HERE/tools/playtest_analyze.py
for run in open3 open0 open1 air3 travel land death scene clank noship farland toggle land0 resets \
    roundtrip; do
    echo "=== $run"
    grep "\[mod rac1-travel-anywhere\]" "$OUT/$run.log" | grep -v "opens the planet select" | sed 's/.*\] //'
    python3 "$A" --modes "$OUT/$run.jsonl"
    case $run in open* | air3 | death | toggle) python3 "$A" --around "$OUT/$run.jsonl" ;; esac
    case $run in resets | roundtrip)
        grep "\] bolts " "$OUT/$run.log" | tail -1
        python3 "$A" --mobys "$OUT/$run.log" 500 276 93.3 55 1.5 first last
        python3 "$A" --mobys "$OUT/$run.log" 13 264 117 55.1 8 first last
        ;;
    esac
done
echo "=== same"
python3 "$A" --same "$OUT/same_base.jsonl" "$OUT/same_mod.jsonl"
# Contact sheets of the pictures (needs Pillow).
python3 "$HERE/tools/playtest_sheet.py" "$OUT/shots" "$OUT/sheets"
