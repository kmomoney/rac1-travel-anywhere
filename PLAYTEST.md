# rac1-travel-anywhere: the play-test

The mod played in OpenRAC's own rac1 ports (built from OpenRAC `develop` at `6dc9cade`, mod
interface 1.14), on the player's own disc data, NTSC-U v1.00 and PAL v2.00, by script. No person
drove these runs: the port's development switches did. Every run below is of version 0.1.1 (the
0.1.0 routes rerun as regressions, with the restart's routes added).

## How

`tools/playtest.sh` runs one route, hidden (`OPENRAC_HIDDEN`) and silent (the dummy audio driver).
It starts a level directly (`OPENRAC_DIRECT` with `--level`), skips what plays at its start with
Start (`k30:8:3`), presses the pad on frames counted from control (`OPENRAC_PRESS` `cF:` entries;
L1 `400`, R1 `800`, Select `1`, Triangle `1000`, Cross `4000`, Square `8000`, Up `10`, Down `40`),
writes game memory where a route needs it (`OPENRAC_WRITE`) and writes the port's per-tick trace
(`OPENRAC_TRACE`: the level, the game mode, the hero's position, rotation and state, the camera).
The mod's own trace (`"trace": true`) says what it decided at each press. Pictures come from
`OPENRAC_SHOT`, with `--levels` pointing at OpenRAC's level export (`assets/port/rac1/ntsc`), which
the port needs to draw the world (the PAL port draws with it too); the port's moby log
(`OPENRAC_MOBY_LOG`) lists the mobys near Ratchet. `tools/playtest_analyze.py` reads a run back:
`--modes` (each change of level and game mode), `--around` (each menu that play follows: the last
tick before it against the first after it), `--same` (two runs, tick by tick), `--mobys` (a class
of moby near a point, at frames of the moby log).

```sh
tools/playtest_matrix.sh ntsc build-playtest/ntsc                                       # every route below
DATA=~/Projects/OpenRAC/build/pal-test tools/playtest_matrix.sh pal build-playtest/pal  # the PAL disc
tools/playtest.sh /tmp/pt open3 3 '{"combo": "l1_r1_select", "trace": true}' \
    ROUTE=c30:0:60:128:0,c100:C00:20,c120:C01:5,c125:C00:5,c340:1000:5 CF=600   # one of them
python3 tools/playtest_analyze.py --around /tmp/pt/open3.jsonl
```

A direct start is a new game in that level: its planet list holds the levels the start unlocked
(Kerwan alone on Kerwan, Novalis alone on Novalis, none on Veldin, where the game never lists the
planet itself). The travel route unlocks Novalis and Aridia by writing the game's planet list and
flags (`gGalaxyList` NTSC `0x13D514`, PAL `0x13D61C`: 1 then 2; `level_available` NTSC `0x13DD40`,
PAL `0x13DE48`: levels 1 to 3). On Novalis a new game keeps the ship hidden until its dropship
scene (DESIGN.md section 4), so the Novalis route marks that level's done-items row done (the
saved mission state, NTSC `0x14C060`, PAL `0x14C160`, 16 bytes of 0xFF written while the level
loads), as a later visit has it: the intro scene and the dropship are then past and the ship is
there to board. The death route sets the hero's health (NTSC `0x1415F8`, PAL `0x1416F8`) to 0 for
one frame: the game then plays his dying state (0x3D) and respawns him.

## The routes

| Run | Level | Route (frames from control) |
|---|---|---|
| open3, open0, open1 | Kerwan, Veldin, Novalis | run forward 60; L1 + R1 from 100, Select at 120: the menu; Triangle at 340: back out |
| air3 | Kerwan | Cross held 12 from 30 (a jump); L1 + R1 from 34, Select at 40, mid-jump; Triangle at 240 |
| travel | Kerwan | the combo at 80; Down at 160 (Novalis), Cross at 200 (its page), Cross at 260 (Blast Off); arrival on Novalis, its scenes skipped |
| land | Kerwan | run 60; the combo at 120; Cross at 240 (Kerwan's page), Cross at 300 (Blast Off): the restart |
| death | Kerwan | health 0 at 100; the combo at 110, 160 and 210 (dying), at 260 (respawned); Triangle at 400 |
| scene | Novalis | Novalis's intro scene not skipped; the combo at window frames 900 and 1500, in the scene |
| clank | Orxon | Clank's start (no O2 Mask); the combo at 80 |
| noship | Novalis | a new game's first visit, its ship hidden until the dropship scene; the combo at 120 |
| farland | Kerwan | run 200; the combo at 300; Cross at 420, Cross at 480 (Blast Off on Kerwan): the restart |
| land0 | Veldin | the combo at 120; Cross at 240, Cross at 300 (Blast Off on Veldin, not in the ship's list) |
| resets | Kerwan | run 200 over three loose bolts; put by the crates at (276, 91) at 240; Square at 270, 290, 310, 330 (three crates broken); the combo at 380; Cross at 480, Cross at 540 (the restart); the port's moby log every 100 frames |
| roundtrip | Kerwan | the same up to the menu, then Down (Aridia, unlocked after Kerwan), Cross, Cross; on Aridia the combo at 2500, Up (Kerwan), Cross, Cross; back on Kerwan; the moby log |
| toggle | Kerwan | the combo at 120 (opens), the combo again at 260 |
| same_base, same_mod | Kerwan | no combo: run 90, jump, L1 30, R1 30, L1 + R1 30 (no Select), Select alone (the map) at 360, Triangle at 420, Start at 480 (pause), Start at 540, run left 60, jump; without the mod, then with it |

## What it showed

Every route gave the same result on both discs: the same decisions in the mod's trace, the same
positions to the trace's four decimals, the same frames from control (c) but where the flight
between planets ends in a level's load in travel, resets and roundtrip: 18 to 62 frames sooner on
PAL (in land and farland on the same frame). Where the two differ the tables say so; window frames
(f) are NTSC-U's.

### Opening on foot, and backing out (open3, open0, open1)

| Level | The trigger | The menu | Back out (Triangle at 340) |
|---|---|---|---|
| Kerwan | `open` (mode 0, 150 ticks in play, state 0x1: L1 looks) | Galactic Map, "Metropolis Kerwan", the cursor on it | play again at c341: position (263.9811, 106.59, 54.7344), rotation, state 0x1 and camera (263.9811, 105.4151, 56.3696) the same as at c120, the tick before the menu; the camera's forward the same on the next tick |
| Veldin | `open` | Galactic Map, the list empty (a new game has found no planet; Veldin is never listed) | the same: (136.4801, 116.9818, 31.2344), camera (135.1171, 116.5158, 32.8655) |
| Novalis | `open` (its dropship scene marked done) | "Tobruk Crater Novalis" | the same: (160.6307, 140.6861, 60.0), camera (161.1037, 139.6166, 61.7771) |

The mod's trace for each: `combo pressed in play`, `trigger: open (...)`, `planet select open`,
`backed out on planet N: the map's close, play goes on`, `menu over: play goes on in place`. The
game mode went 0, 3 (219 window frames), 0, and never 6: the ship did not land.

On Veldin a Gadgetron HelpDesk message shows after the menu. It is the game's own: the same route
without the mod shows it at frame 1400, where the mod's run has the menu up.

Novalis before its dropship scene (noship, a new game's first visit): `trigger: no-ship` (ship 0),
`notice shown: Ship travel: the ship is not here`, nothing opened, the game mode 0 throughout. The
game keeps the parked ship hidden and unboardable then (DESIGN.md section 4).

### Mid-air (air3)

Opened at c40, rising in a jump (state 7, z 55.2363, climbing about 0.14 a tick). Backed out at
c241: state 7, z 55.2363, the same camera; the jump went on at once (z 55.3655 at c242), topped at
56.28 and landed at 54.5, as a jump does.

### A travel (travel): Kerwan to Novalis

The list showed Metropolis Kerwan, Tobruk Crater Novalis and Outpost X11 Aridia; Down put the
cursor on Novalis; its page showed "Planet Novalis, Tobruk Crater", Exit / Play Infobot / Blast Off.
Blast Off at c260:

| | NTSC-U | PAL |
|---|---|---|
| The mod's trace | `ShipTravelTo(1) on planet 3: the ship's travel` | `menu over: the level changed` (the port does not call the hook on PAL: below) |
| The ship's flight out (mode 6 on Kerwan) | from c261 | from c261 |
| Novalis loaded, its landing | c1275 | c1255 |
| Play on Novalis (after its intro scene, skipped) | c2056, at (162.8337, 135.7076, 60.0) | c2036, the same place |

The pictures: the parked ship rises off its pad on Kerwan and flies off (the camera on the pad),
the flight between planets, Novalis's landing with Ratchet and Clank climbing out, then play on
Novalis.

### The planet he is on: the level anew (land, farland)

land opens the menu at c120 after a run of 60 frames (at (263.9811, 106.59, 54.7344), a few steps
on from the level's start), presses Cross on Metropolis Kerwan at c240 (its page) and Blast Off at c300.
farland runs 200 frames first, along the path to (263.9789, 119.9286, 54.9802), and presses the
same at c300, c420 and c480. The mod's trace, for both:

```
planet 3 chosen, the one he is on: the game's travel to it
ShipTravelTo(-1) on planet 3: the ship's travel to it, the level anew
restart: the load level set to planet 3
```

On PAL the middle line is missing (the port calls no hook there: "Found on the way") and the
third comes from the next pad poll; the menu then ends with `menu over: play again`.

| | land | farland |
|---|---|---|
| Blast Off | c300 | c480 |
| The ship's flight out, then the flight between planets (mode 6) | from c301 | from c481 |
| Kerwan loaded again, its arrival landing (the hero's state 0x64) | c1335, at (263.982, 102.0925, 54.5) | c1495, the same place |
| Play | c2072, at (263.982, 101.3446, 54.5), state 0x65 | c2232, the same place and state |

That is the level's start: each run's own first arrival on Kerwan (the direct start's landing,
window frames 446 to 1183, PAL 388 to 1125) ends at (263.982, 101.3446, 54.5) in state 0x65, the same 737 frames
after its landing began, as the restart's does. The pictures: the parked ship rises off its pad
and flies off, the flight between planets to "Planet Kerwan", the ship coming down onto Kerwan's
pad and Ratchet jumping out, then Ratchet at the level's start, at the foot of its first walkway. In farland the bolts he picked up on the way
show in the corner before the menu; after it the loose bolts lie on the path again (below).

0.1.0 played the game's landing here instead (mode 6 from c301 to c800) and left him where he
stood; Veldin's first visit still does (below).

### Veldin's first visit (land0)

A new game's first minutes on Veldin: the ship's list is empty, and can never hold Veldin (level 0
ends it). The combo at c120 opened the Galactic Map with its list empty; Cross at c240 opened the
planet's page and Cross at c300 was Blast Off. The mod's trace: `menu closing for planet 0: the
ship's landing (not in the ship's list)`, then `ShipTravelTo(0) on planet 0: the ship's landing`
(NTSC-U; PAL: `menu over: play again`). The game's landing, as in 0.1.0: mode 6 from c301 to c800,
the landing at the ship's pad, then play at (131.5004, 115.0198, 31.4331) in state 0x65 and, at c840,
at (131.9346, 115.3588, 31.4283), 0.2 from where he stood (132.09, 115.48, 31.4266).

### What a restart resets (resets, roundtrip)

resets runs 200 frames along Kerwan's first path, over its loose bolts, is put by three crates at
(276, 91) at c240 (by writing his position) and breaks them with the wrench (Square at c270, c290,
c310, c330), then restarts: the combo at c380, Cross at c480, Blast Off at c540. roundtrip does the
same up to the menu, then flies to Aridia instead (unlocked by the route), opens the menu there at
c2500 and flies back to Kerwan: an arrival from another planet, to hold the restart against. The
port's moby log (every 100 window frames, the mobys within 40 of Ratchet) counts the crates within
1.5 of (276, 93.3, 55) and the loose bolts within 8 of (264, 117, 55.1); the port logs the bolts
Ratchet carries when they change.

| | Before (the moby log's first frame in play) | Before the menu | Back on Kerwan: the restart (resets) | Back on Kerwan: from Aridia (roundtrip) |
|---|---|---|---|---|
| The three crates | 3 | broken (c270 to c330) | 0: still broken | 0: still broken |
| The three loose bolts on the path | 3 | picked up | 3, where they lay | 3, where they lay |
| Bolts carried | 0 | 24 on NTSC-U, 22 on PAL, once the crates' bolts reach him (some were still flying to him when the menu opened, and came during the flight out) | the same: 24, 22 | one more, from Aridia: 25, 23 |
| Ratchet | the level's start | by the crates, (276.5745, 94.0424, 54.5) | the level's start, (263.982, 101.3446, 54.5), state 0x65 | the same |
| Kerwan's arrival landing, then play | | | c1577, c2314 (PAL c1515, c2252) | c3713, c4450 (PAL c3695, c4432) |

roundtrip's Aridia: its landing at c1575 (PAL c1515), play at c2312 (PAL c2252) at (210.9139,
169.7973, 25.299). The crates gave a different number of bolts on each disc; the rest is the same
on both.

So a restart resets what an arrival from another planet resets, and the same way: the level is
placed again from its file, the crates the game recorded as broken stay broken, the loose bolts are
back, and what Ratchet carries stays. Enemies were not part of the routes: by the code a moby with
a save id records its death and stays gone after any arrival, a restart's too (DESIGN.md section
1, "What an arrival restores").

### Refusals

| Run | Where | The mod's trace | What happened |
|---|---|---|---|
| death | dying (health 0 at c100) | `trigger: down` at c110 (hp 0, the dying state not yet entered), c160 and c210 (state 0x3D); `notice shown: Ship travel: not while Ratchet is down` | Nothing opened, and Select reached no map. He was in his dying state from c116 and respawned at c242; the combo at c260 opened the menu (`open`, hp 4) and Triangle backed out with everything the same |
| scene | Novalis's intro cutscene (mode 2, frames 570 to 2102) | `combo outside play (level 1, mode 2): the game's`, twice | Nothing opened; the scene played on to control. The buttons were left to the game |
| clank | Orxon, Clank's start (vehicle mode 1) | `trigger: clank` | Nothing opened; "Clank cannot fly the ship" |

The combo pressed in the planet select itself closes it, as Select does on its list (toggle: open
at c120; at c260 `combo outside play (level 3, mode 3): the game's`, then `backed out on planet 3`;
play again at c261 with the position, rotation, state and camera of c120). The camera's forward on
the next tick is one step on from c120's, not the same: the camera was moving then (L1 held since
c100 swings it in behind him), and its move goes on where it stopped.

### Nothing without the trigger (same_base, same_mod)

A route of running, a jump, L1 held, R1 held, L1 and R1 held together with no Select, Select alone
(the game's map opened at c360, Triangle closed it at c421), Start (the pause menu, c480 to c541),
a run to the left and a jump, without the mod and with it: **749 ticks from control in both, 0
differ** (the level, the game mode, the hero's position, rotation and state, the camera's
position and forward), on each disc.

## Found on the way

- **OpenRAC's PAL build skips `rac1.ship.travel_to`.** `port/game/rac1-pal/host/destinations.c`
  replaces `func_L00_00290030` for the editor's custom destinations and, with none set, calls
  `func_L00_00290030__game`, past the hook point (`__hooked`). The mod's PAL runs show it: the
  travel, the landing and the restart happen, its hook never runs (`menu over: the level
  changed`, `menu over: play again` where NTSC-U says `ShipTravelTo(...)`; the restart's load
  level put back from the pad poll instead). Every result is the same as on NTSC-U: the mod does
  nothing in that hook that it needs (DESIGN.md section 2, why it is safe). The multiplayer mod's
  travel rule does need it. Not changed in OpenRAC.
- A restart keeps the level's number: the port's trace shows level 3 throughout and the port logs
  no `now playing level`. The load shows in the hero's state (the arrival landing's 0x64, which
  only a level's start plays), in the mobys the frame holds (the trace's `mb`) and in the
  pictures.
- The level's own first arrival, which every direct start plays, is the measure of "the level's
  start": the restart's arrival matches it in place, state and length on both discs.
- The port draws a mod's notice in its own overlay, which `OPENRAC_SHOT` pictures leave out; the
  mod's trace says whether the port took it (`notice shown: ...`).
- Writing the hero's health word kills him for real (his dying state, then the game's respawn);
  teleporting him off the world for a few frames only brings an instant respawn, with no window
  to test in.

## Not covered

- A playthrough from a memory card save, a party (the multiplayer mod), a boss fight, the
  hoverboard, Giant Clank, swimming and grinding (refused or allowed by the rules of DESIGN.md
  section 4, and the load test, but not played).
- Circle on a planet's page (its infobot movie, after which the game opens the menu again): the
  load test plays the round trip; the runs use `--skip-movies`.
- The keyboard key: the load test only (the scripted runs press the pad).
- The restart's save. Every run is a direct start with no memory card in use (`card 0` in the
  trace), where `memcard_save_data` does nothing; that it saves resuming on the level played, as a
  trip there would, is from the code of both discs (DESIGN.md section 1, "The save on travel").
- Enemies killed, or a mission done, before a restart (from the code: section above), and a
  restart on another planet than Kerwan (the mod reads nothing of the planet but its number and
  whether the ship's list holds it).
- Another mod hooking `rac1.ship.travel_to` (the multiplayer mod's travel rule) during a restart:
  it is handed -1, the game's own value for no planet, then the load level is put back (DESIGN.md
  section 6).
- Windows and macOS builds: the CI workflow builds them and runs the unit and load tests (green for
  0.1.0 on GitHub); none was played.

## The pictures

`tools/playtest_matrix.sh` writes each run's `OPENRAC_SHOT` frames to `OUT/shots/` (the runs of
this page: `build-playtest/ntsc/shots` and `build-playtest/pal/shots`, which git ignores), with
contact sheets of them beside (`build-playtest/<disc>/sheets/`, each tile a frame, its number on it).
None is in the repository: they show the game.
