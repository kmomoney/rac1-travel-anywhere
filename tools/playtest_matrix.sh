#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The play-test's whole set of routes on one disc (PLAYTEST.md, "The routes"), each through
# tools/playtest.sh, then read back with tools/playtest_analyze.py.
#
#   tools/playtest_matrix.sh ntsc|pal OUT
#
# OUT gets each run's NAME.jsonl (the port's trace), NAME.log (the game's log, the mod's trace in
# it) and, for the runs with pictures, shots/NAME_<frame>.png. The disc's data comes from DATA
# (for ntsc, default ~/OpenRAC/active/rac1/data; for pal it must be given), the port from OPENRAC
# (default ../OpenRAC), the level export the pictures need from LEVELS (default
# $OPENRAC/assets/port/rac1/ntsc, which the PAL port draws with too).
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

# The game memory the routes write, at each disc's address: the planet list's second entry
# (gGalaxyList + 4), the planets' flags (level_available), Novalis's row of done items (the saved
# mission state + 16), the hero's health.
if [ "$DISC" = pal ]; then
    : "${DATA:?give the data folder of the PAL disc in DATA}"
    GAL=13D61C AVAIL=13DE48 NOVA=14C160 HP=1416F8
else
    DATA=${DATA:-$HOME/OpenRAC/active/rac1/data}
    GAL=13D514 AVAIL=13DD40 NOVA=14C060 HP=1415F8
fi
export DISC DATA OPENRAC

hex() { printf %X $((0x$1 + $2)); }
# L1 and R1 from 20 frames before F, Select with them at F.
combo() { echo "c$(($1 - 20)):C00:20,c$1:C01:3,c$(($1 + 3)):C00:2"; }

OPEN="c30:0:60:128:0,c100:C00:20,c120:C01:5,c125:C00:5,c340:1000:5"
NOVALIS_DONE="300:$NOVA:FFFFFFFF:300,300:$(hex $NOVA 4):FFFFFFFF:300"
NOVALIS_DONE="$NOVALIS_DONE,300:$(hex $NOVA 8):FFFFFFFF:300,300:$(hex $NOVA 12):FFFFFFFF:300"
UNLOCK="c5:$GAL:1:4000,c5:$(hex $GAL 4):2:4000,c5:$AVAIL:1010100:4000"

# Open on foot and back out: Kerwan, Veldin, Novalis (its dropship scene marked done).
LEVELS=$LEVELS_DIR $PT "$OUT" open3 3 "$MOD" "ROUTE=$OPEN" CF=600 \
    OPENRAC_SHOT=50:"$OUT"/shots/open3_ OPENRAC_SHOT_FROM=1300
LEVELS=$LEVELS_DIR $PT "$OUT" open0 0 "$MOD" "ROUTE=$OPEN" CF=600 \
    OPENRAC_SHOT=50:"$OUT"/shots/open0_ OPENRAC_SHOT_FROM=1300
LEVELS=$LEVELS_DIR $PT "$OUT" open1 1 "$MOD" "ROUTE=$OPEN" CF=600 OPENRAC_WRITE="$NOVALIS_DONE" \
    OPENRAC_SHOT=50:"$OUT"/shots/open1_ OPENRAC_SHOT_FROM=550
# Mid-air: Cross held, the combo while he rises, back out.
$PT "$OUT" air3 3 "$MOD" "ROUTE=c30:4000:12,c34:C00:6,c40:C01:3,c43:C00:2,c240:1000:5" CF=400
# A travel, Kerwan to Novalis (Novalis and Aridia unlocked), to the arrival.
LEVELS=$LEVELS_DIR START="100:4000:5,k30:8:3,S30:8:3" $PT "$OUT" travel 3 "$MOD" \
    "ROUTE=c60:C00:20,c80:C01:5,c85:C00:5,c160:40:3,c200:4000:3,c260:4000:3" CF=2800 \
    OPENRAC_WRITE="$UNLOCK" OPENRAC_SHOT=100:"$OUT"/shots/travel_ OPENRAC_SHOT_FROM=1300
# The planet he is on, after a run away from the start: the landing.
LEVELS=$LEVELS_DIR $PT "$OUT" land 3 "$MOD" \
    "ROUTE=c30:0:60:128:0,c100:C00:20,c120:C01:5,c125:C00:5,c240:4000:3,c300:4000:3" CF=1300 \
    OPENRAC_SHOT=100:"$OUT"/shots/land_ OPENRAC_SHOT_FROM=1300
# Down: health 0 at 100, the combo while he dies (110, 160, 210) and once respawned (260).
$PT "$OUT" death 3 "$MOD" "ROUTE=$(combo 110),$(combo 160),$(combo 210),$(combo 260),c400:1000:5" \
    CF=600 OPENRAC_WRITE=c100:$HP:0:1
# A cutscene: Novalis's intro, not skipped, the combo twice in it (window frames).
START="100:4000:5,880:C00:20,900:C01:3,903:C00:2,1480:C00:20,1500:C01:3,1503:C00:2" \
    $PT "$OUT" scene 1 "$MOD" "ROUTE=c10:0:1" CF=100
# Clank's start on Orxon.
$PT "$OUT" clank 10 "$MOD" "ROUTE=$(combo 80)" CF=200
# Novalis as a new game has it, its ship hidden until the dropship scene.
$PT "$OUT" noship 1 "$MOD" "ROUTE=$(combo 120)" CF=200
# The planet he is on after a long run from the start (200 frames).
$PT "$OUT" farland 3 "$MOD" "ROUTE=c30:0:200:128:0,$(combo 300),c420:4000:3,c480:4000:3" CF=1500
# The combo pressed again in the planet select: it closes it, as Select does there.
$PT "$OUT" toggle 3 "$MOD" "ROUTE=$(combo 120),$(combo 260)" CF=400
# No trigger: running, a jump, L1, R1, both, Select alone (the map), the pause menu, a run and a
# jump, without the mod and with it.
SAME="c30:0:90:128:0,c130:4000:5,c200:400:30,c250:800:30,c300:C00:30,c360:1:5,c420:1000:5"
SAME="$SAME,c480:8:5,c540:8:5,c600:0:60:0:128,c680:4000:5"
$PT "$OUT" same_base 3 none "ROUTE=$SAME" CF=750
$PT "$OUT" same_mod 3 '{"combo": "l1_r1_select"}' "ROUTE=$SAME" CF=750

# What they showed.
A=$HERE/tools/playtest_analyze.py
for run in open3 open0 open1 air3 travel land death scene clank noship farland toggle; do
    echo "=== $run"
    grep "\[mod rac1-travel-anywhere\]" "$OUT/$run.log" | grep -v "opens with" | sed 's/.*\] //'
    python3 "$A" --modes "$OUT/$run.jsonl"
    case $run in open* | air3 | death | toggle) python3 "$A" --around "$OUT/$run.jsonl" ;; esac
done
echo "=== same"
python3 "$A" --same "$OUT/same_base.jsonl" "$OUT/same_mod.jsonl"
# Contact sheets of the pictures (needs Pillow).
python3 "$HERE/tools/playtest_sheet.py" "$OUT/shots" "$OUT/sheets"
