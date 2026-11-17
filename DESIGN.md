# rac1-travel-anywhere: design

How Ratchet & Clank opens the ship's planet select, what each way out of it does, and how the mod
opens the same menu from anywhere in play. The evidence is rac1-decomp (PAL v2.00 names and
addresses, `src/`, `include/names/`, `config/names.tsv`), the NTSC decomp OpenRAC carries
(`OpenRAC/games/rac1/ntsc-decomp`, US names and addresses) and the C OpenRAC generates from both
(`OpenRAC/port/build/release/games/rac1-{pal,ntsc}/gen`), which spells each global the way the game
reads it, relocated or not. Function names are the NTSC decomp's unless said otherwise; the PAL name
and address follow in parentheses where the mod uses them.

## 1. The ship's travel, as the game does it

**The trigger.** The parked ship is a moby whose update is `update_moby_proximity_interaction`
(US `FUN_L00_0028e180`, `src/overlays/shared/mobys/update_moby_proximity_interaction.c`). The level
binds it at load (`load_level_gameplay`: when the level's ship position is positive it creates the
ship's class there and stores it as the ship record's moby, `level_render_state.player`, PAL
`D_0013E130`, US `D_0013E030`). Each tick it checks whether Ratchet stands within 4 units of a point
beside the ship; then it shows help message 0x53E4 and, on Triangle (`pad_pressed & 0x10`), either

- on Orxon with Clank alone (`hero.vehicle_mode == 1`): puts the ship in state 0x2A and plays scene
  9, then the follow-up scenes 4, 5, movie 0xB and 6, and only then asks for the take-off itself;
- anywhere else: sets `ship_level_exit_requested` (PAL `D_L00_0015F710`, US `D_L00_0015F630`) and
  clears the record's skip word.

A ship whose flag bit 0 is set (`moby->flags & 1`) never sees the hero: the proximity word is
cleared every tick. `deactivate_recorded_moby` sets that bit (and bit 1), and so do the ship's own
sequences while they play.

**The take-off.** `handle_pause_input` (the level loop's update in game mode 0, PAL
`func_L00_00299250`) finds the request, clears it and, with the hero alive, calls
`prepare_level_transition_with_help_cleanup` (PAL `func_L00_0028FFB0`): music paused, help cleared,
then `initialize_level_transition_with_audio(0)` (PAL `func_L00_0028FCA0`, `EnterShipMode`), which
sets the game mode to 6, puts the hero in state 0x64 (`hero_set_state(100, 2)`), hides the parked
ship (`flags |= 1`) and mobys of classes 0x4A and 0xCB, loads the take-off's audio and scene chunks.
In mode 6 the level loop runs `update_resident_gameplay_state` (PAL `func_00230A90`,
`SpaceFlightUpdate`) each tick; the record's state is 0, the take-off playback.

**The menu.** At the playback's end (`render_sequence.time >= end`) the state-0 branch shows the ship
again, releases the scene's objects and opens the menu:

```c
pause_all_sounds(0);      /* PauseAllSounds, PAL func_00219C70, US FUN_00218d78 */
menu_system.state = 0xE;  /* PAL D_001D5F70, US D_001D5BF0 */
space_load_in_progress[0] = 1;
```

`PauseAllSounds(mode)` is the game's menu opener (the pause menu is `PauseAllSounds(0)`, the map on
Select `PauseAllSounds(10)`): it pauses sound group 0x1D and the music, sets the game mode to 3,
stores `mode` as the menu's state, clears its close request, and sets `selected_level_index` (PAL
`D_001A0414`, US `D_001A0314`) to the level being played. One thing first: while
`camera_position_publication_suppressed` (PAL `D_0018C42C`, US `D_0018C32C`) is set it only notes
`pause_blocked_by_camera` and returns, with the sounds already paused. Its argument is the state the
ship's two writes leave, so `PauseAllSounds(14)` is the same thing.

In mode 3 the level loop runs `update_pause_menu` (PAL `func_L00_00277A88`). Its first tick calls
`initialize_pause_menu`, which for state 14 picks the level-transition page, the planet select
(state 15; `planet_select_page`, `src/ui/menus/initialize_menu_preview_scene.c`), with the menu's
small buffers, `initialize_render_archive_transfer` and the map image's VRAM: the same set-up as
state 10, the map Select opens in play.

**The list.** The planet list's enter function (`initialize_selection_entries_and_index`, US
`FUN_L00_0027f448`, `BuildGalaxyMenu`) builds the list from `gGalaxyList` (PAL `D_0013D618`, US
`D_0013D510`), the planets in the order they were discovered, up to the first 0, and puts the cursor
on the level being played. Planets join that list only through `UnlockPlanet` (the story, infobots).
Level 0 (Veldin) can never be listed: 0 ends the list. The menu offers the game's own planets and
nothing else.

**The pages and their buttons.** On the list (`update_menu_entry_actions`, flag `SETS_LEVEL`):
Select, R3 or Start close the menu with `selected_level_index` set back to the planet being played;
Triangle does the same (the page has no back page); Cross on a planet, or on the galaxy map
(`update_level_selection_map_screen`), goes to the confirm page (state 16, `planet_confirm_page`:
the planet's missions, its gold bolts, "Exit / Play Infobot / Blast Off"). There
(`handle_menu_action_buttons`, in this order): Triangle goes back to the list; Start closes with the
planet being played; Cross closes with the planet chosen; Circle closes with close request 3, which
plays the planet's infobot movie and opens the menu again on that page afterwards (the page waits in
the menu's `travel_page`, +0xF0, which `handle_pause_input` reopens it from).

**The close.** A page update that asks to close makes `update_pause_menu` set the close request to
2 in states 15 and 16 (1 anywhere else); `close_pause_menu` waits until the menu has run 10 ticks,
puts the hero's items back, sets state 20 and a timer of 2. Two ticks later the restore branch puts
the saved camera back and then, by the close request: 1 sets game mode 0 (play goes on); 2 calls
`prepare_target_level_transition(transition_target_level)` (PAL `func_L00_00290030`,
`ShipTravelTo`, the SDK's `rac1.ship.travel_to`); 3 to 7 play movies, scenes or the slideshow. After
any but 2 it unpauses the music. `ShipTravelTo(dest)`:

- `dest` the level being played: `initialize_level_transition_with_audio(8)`, the **landing**: the
  ship comes down onto its pad and Ratchet and Clank climb out. Its playback ends with play where
  the hero stood: the record's landing word (+0x2C), which `prepare_level_entry_transition` sets at
  every level's start and nothing clears, takes the branch that leaves him there (the other puts him
  at the record's `hero_start_pos`, +0x60). His landing state (0x65) then steps him a little and
  back. At the ship, where he boarded, that is beside it.
- another level: `memcard_save_data(0, dest)` (the game saves to the card), the next level set
  (the level loop's load level: PAL `D_L00_0015F6A4`, US `D_L00_0015F5C0`, level 0's copy of the
  executable's `0x0015F6E4`, which OpenRAC's own travel and Quit Game also set; -1 there is the
  title), the record's state 3. State 3 flies the parked ship off its pad (it rises over 120 ticks, then
  follows the level's flight path with the camera on it); at the path's end the level loop ends and
  `do_space_transition` plays the flight between planets and loads `dest`, whose own landing plays.

So at the ship, backing out (Triangle, Start, Select, R3 on the list; Start or Triangle then Triangle
on the confirm page) and choosing the planet you are on both land the ship again where it stands,
Ratchet climbing out beside it. From anywhere else that same landing plays at the ship's pad and play
then goes on where Ratchet stood (mod 0.1.0, PLAYTEST.md).

**The save on travel.** `memcard_save_data(slot, level)` (PAL `memcard_Save`, `func_0020BFC8`; US
`FUN_0020b178`; the SDK's `rac1.save.request_autosave`) first records the level being played: its
objects' positions, its map coverage, and its level-data record (`record_index` = the level played:
its kills and done items). Then, only for a level of 0 or more, it makes that level the save's resume
level for the moment the game data is written, and marks it visited. A level under 0 leaves the save
resuming on the level being played. It does nothing unless a memory card is in use with a save
chosen (a game started from a save or saved once).

**What an arrival restores.** `load_level_gameplay` places the level's mobys anew from its file, but
skips each placed moby whose kill bit is recorded (`Moby_RecordDeath`: a moby with a save id, a
crate's break) or whose mission is done (the level's 16 bytes of done items, the saved mission state,
copied in at the load), and a crate looted on an earlier visit gives half its bolts. Loose bolts
lying in the level are placed again. What the player carries (bolts, items, gold bolts, planets)
is the game's, not the level's, and stays. Story scenes of a first visit play only on the first
(`level_visit_state`).

## 2. What the mod does

**Opening.** At the end of a tick in which the trigger came (on_frame, after the level loop's
update), if the rules of section 4 allow it, the mod calls `PauseAllSounds(14)`: the menu opens on
the planet select exactly as the take-off's end opens it, from the next tick on, and every page,
list, picture and button is the game's. The level loop resets its count of ticks in the game mode
(cinematic_mode_frames, PAL `D_L00_0015F6AC`, US `D_L00_0015F5C8`) in the tick the mode changes; the
mod's change comes after that tick's check, so the mod writes the 0 itself. The take-off is not
played (the hero is wherever he is), and the mod does not touch `space_load_in_progress`, the take-off
renderer's flag, which is the level's own outside the ship's sequences.

**Backing out.** The game would land the ship, Ratchet and Clank climbing out at its pad. The
mod's menu instead plays on where he was, as the map does: while the menu closes (state 20, close
request 2, two ticks before the restore), if the last press that could close it was not Cross on
the confirm page and the planet selected is the one being played, the mod changes the close request
to 1. The restore then
takes the map's branch: game mode 0, the saved camera, the music unpaused. Nothing of the hero was
updated while the menu was up, so his position, rotation, state and the camera are those of the tick
it opened in (PLAYTEST.md). The mod checks at each pad poll and at each tick's end; should both miss
(they cannot, by the loop's order: a poll and a tick's end come between state 20 and the restore),
its `rac1.ship.travel_to` hook does the same by hand (game mode 0, close request 1) and calls no
travel.

**Which press closed it.** The mod watches pad 0's report while its menu is up (game mode 3) and the
page the report reaches: the page's state, or during a page switch the incoming page's on the
switch's last tick (the tick its screens next update), and no page in the tick a switch starts. Cross
on the confirm page (with neither Triangle nor Start, which the game weighs first) marks a choice;
any closing button on the list, and Triangle or Start on the confirm page, unmark it.

**Choosing another planet.** The game's `ShipTravelTo` runs, the hook only noting it. A planet
other than the one being played is always the game's travel, chosen or not: the game's own
selection decides, never the mod's reading of the presses.

**Choosing the planet he is on** (0.1.1). Ratchet starts the level anew, as on any arrival: the
mod sends the game down its own travel branch for this planet, by changing two words it reads.

1. While the menu closes after Cross (Blast Off) on that planet's page, the mod sets the menu's
   `transition_target_level` (PAL `D_L00_001842F0 + 0x224`, US `D_L00_001841F0 + 0x224`), the
   planet the restore hands `ShipTravelTo`, to -1. `ShipTravelTo(-1)` is not the level played, so
   the game takes its travel branch: `memcard_save_data(0, -1)`, which saves resuming on the level
   played (what a trip to it would save), the load level set to -1, the ship's flight out of the
   level (state 3).
2. As soon as that branch has run (game mode 6 and the load level -1), the mod puts the load level
   back to the planet he is on: in its `rac1.ship.travel_to` hook right after the game's code, or
   at the next pad poll or tick's end (on PAL, where the port skips the hook). The level loop reads
   it only when the ship's flight out has ended, several seconds later: `do_space_transition` then
   plays the flight between planets and loads the level again, whose own arrival landing plays and
   puts him at its start.

Nothing is reimplemented: the save, the flight out, the flight, the load and the arrival are the
game's own, as for any other planet. A level the ship's list does not hold (`gGalaxyList`, PAL
`D_0013D618`, US `D_0013D510`) is not travelled to: the game never flies there. That is Veldin's
first visit, level 0, which the list can never hold (0 ends it); there Blast Off stays the game's
landing again. The restart resets what any arrival resets (section 1, "What an arrival restores";
PLAYTEST.md shows it for Kerwan against a trip to Aridia and back): broken crates and recorded kills
stay, loose bolts and the rest of the level's mobys are placed again, Ratchet starts at the level's
start through its arrival landing, and what he carries stays.

**The menu's end.** The mod forgets its menu when `ShipTravelTo` runs, when the level changes, or
when play is back (mode 0) with no page waiting to reopen (the infobot movie's round trip keeps it).
A restart's level number does not change: its menu ends at the hook (NTSC) or when play is back on
the level loaded again (PAL).

**The hook on PAL.** OpenRAC's PAL build (develop at `6dc9cade`) puts a host function of its own
over `ShipTravelTo` for its custom ship destinations (`port/game/rac1-pal/host/destinations.c`,
`func_L00_00290030`), and with no destination set it calls the game's code directly
(`func_L00_00290030__game`) rather than through the hook point (`func_L00_00290030__hooked`). So
on the PAL disc `rac1.ship.travel_to` never reaches a mod (this one, the multiplayer mod). The
NTSC build calls the hook. The fix belongs to OpenRAC (calling `__hooked` in both places), not to
this mod.

It is safe for this mod, because nothing it must do happens in the hook:

- **Backing out** is the close request turned from 2 to 1 while the menu closes, at a pad poll or
  a tick's end; the game then never calls `ShipTravelTo` at all. Between the close (state 20,
  timer 2) and the restore come a whole loop pass with its poll and its vertical blank, so the
  change always lands first. The hook's own back-out is only a second line behind it, which PAL
  lacks; every PAL back-out route took the first (PLAYTEST.md).
- **A travel or the landing** is the game's own `ShipTravelTo` whether a hook sees it or not: the
  mod only notes it there. On PAL the game runs it unobserved, exactly as at the ship.
- **A restart** steers the restore's target before `ShipTravelTo` (no hook needed), and puts the
  load level back at the first pad poll or tick's end after the travel branch, long before the level
  loop's end reads it. The hook only does it a tick sooner. The PAL restart routes go that way
  (`restart: the load level set to planet 3`).
- **The menu's end** is also seen without the hook: the level changing (a travel) or play coming
  back with no page waiting (a landing, a back-out). The PAL travel and landing routes end their
  menus that way (`menu over: the level changed`, `menu over: play again`).

What PAL loses is the trace line naming the travel, and the second line of the back-out and of
the restart.

## 3. Why these hook points and calls

| Considered | Used | Why |
|---|---|---|
| `PauseAllSounds` (game function, both discs' builds name it) | yes | The game's own opener; the ship calls it, with its state 14 |
| `rac1.ship.travel_to` | yes | Where the ship's travel or landing starts; the only way to another planet by ship. The mod observes it, puts a restart's load level back at once, and backs out there if the earlier steer missed. A call carrying the planet he is on rather than the steered -1 (a steer missed, or a mod ahead of it in the chain changed the planet) stays the game's landing: the mod never forces a trip the game was not asked for. On PAL the port does not call it today (section 2); nothing depends on it |
| `rac1.save.request_autosave` | no | A restart passes -1, for which the game's save already resumes on the level played: nothing to change |
| `on_pad` | yes | The trigger, read from the report before the game reads it; the combo's last button is let go in the report, so the game never sees it (no map) |
| `on_frame` | yes | The open at a tick's end; the notice's timing; the menu's close |
| `play_notice` (1.9) | when the port has it | A refusal's reason at the top of the picture; checked at run time, so `api` stays 1.2 |
| `input()` (1.2) | for the key | The keyboard key setting |
| `rac1.pad.process_report` | no | `on_pad` sees the same report earlier, and the mod changes only its own button |
| `play_travel` (1.8) | no | The port's Quit-Game path: it ends the level and flies to a level the mod would have to choose. Not the game's menu, no save to the card, no ship leaving its pad |
| `ui_pause_item` (1.9) | no | An item under Pause, Options that opens a page of the mod's (a copy of the Camera options page). Reaching the planet select from it means closing one menu and opening another, three levels down; a button combination is one step and never touches the pause menu |

## 4. Where it does not open

The mod asks only in a level's play (game mode 0): elsewhere it does not take the combo's button and
the game sees every press. In play it takes the button and then opens the menu, or says on screen
why not. The rules, in the order the mod checks them (`ta_may_open`, `src/rac1/travel.h`):

| Rule | Reason |
|---|---|
| A level loaded, its loop not ending (`level_loop_exit_requested`, PAL `D_L00_0015F650`, US `D_L00_0015F570`), the hero bound to his moby | Level loads, a travel or Quit Game on its way |
| Game mode 0 | 1 a movie, 2 a cutscene, 7 a slideshow; 6 the ship's own landing, take-off and flight; 3 a menu (pause, map, this one), 4 a dialog, 5 a vendor, -1 the debug menu |
| 8 ticks in play | The game's pause and map wait as long after any of those (`cinematic_mode_frames >= 8`) |
| Health above 0, not falling off the world (`fall_fade_pending`), no dying, drowning, fatal-fall, death-descent or respawn state (0x3D, 0x47, 0x57, 0x62, 0x68, 0x69, 0x6A, 0x6D-0x6F, 0x77, 0x7B-0x7D, 0x7F, 0x80, 0x82) | Death and respawn: the level puts him back at a checkpoint next |
| Not Clank (vehicle mode 1) | The game never flies Clank: on Orxon, Triangle at the ship plays scene 9 instead. Clank's Orxon start is one such section |
| Not Giant Clank (2) | Start opens his own screen (`mode_freeze_init(4)`), not the pause menu |
| Not another vehicle (3), the hoverboard (`on_hoverboard`, states 0x3E, 0x6B, 0x6C) or a ride or turret (0x32) | The game's map refuses 0x32; the hoverboard's control mode 0x16 too |
| Not states 0x72, 0x1D, 0x64, control mode 0x16, a weapon change (`items[0].switch_lock == 2`) | The moments the game's own pause and map wait out (0x64 is the ship's own, in its sequences) |
| The ship there to board: the record's moby, flag bit 0 clear | The game's boarding rule. On a planet with no ship the flight-out would have nothing to fly; where the story keeps the ship away (Novalis before its dropship scene: `deactivate_recorded_moby` every tick) the game lets nobody board |
| Not the ship's story scenes (its moby in state 0x2A), no take-off asked for | Orxon's scenes at the ship; a take-off already on its way |
| No dialog next (`mode_freeze_flags`, PAL `D_0015EFB4` & 0x401, US `D_0015EEB4` & 1), no menu waiting to reopen (+0xF0) | The game opens those next tick; the pause menu would take the dialog's place |
| The memory card idle (PAL `memory_card_state` +0xDC < 3 and +0xE4 < 0, as the PAL pause and map want; US +0xD4 and +0xDC, the level loop's card icon) | A card write under way; the travel itself saves |
| The camera not holding the pause off | `PauseAllSounds` would pause the sounds and open nothing |
| The debug menu not stepping (`gDebugFreeCamLvl` +8 bit 0x10, +0xEC) | The game's map waits too |

**Not refused:** mid-air, swimming, grinding, hanging from a ledge, looking (L1, which the default
combo holds), crouching, hurt: the game's pause and map open in all of them, and backing out plays on
as after them (mid-air is play-tested). Boss fights are not refused either: the game's own pause
menu offers Save Game and Quit Game in them, a travel out of one is leaving the level as Quit Game
leaves it (the fight starts over on the next visit), and backing out goes on with the fight. The
fight fought as Giant Clank is refused with him, and the ship-flying sections ride (state 0x32).

## 5. The two discs

`src/rac1/discs.h` holds one row per disc. The executable's globals below 0x15F000 are read as they
are; the executable's others through `relocate_data` (a level program carries its own copy: PAL
`D_001D5F70` relocates to level 0's `D_L00_001BA070`, the copy `update_pause_menu` reads, and a run
watching both read the same words, PLAYTEST.md); level 0's through `relocate_level_data(0, ...)`.

| Field | PAL v2.00 | NTSC-U v1.00 | Seen in |
|---|---|---|---|
| hero | `0x0013F450` | `0x0013F350` | `handle_pause_input` (both builds' generated C) |
| ship record | `0x0013E130` | `0x0013E030` | `update_resident_gameplay_state`; OpenRAC `boot.c` (`SHIP_STATE` +0x20) |
| current level | `0x0015EE84` | `0x0015ED84` | `PauseAllSounds`, `handle_pause_input` |
| dialog flags, mask | `0x0015EFB4`, 0x401 | `0x0015EEB4`, 1 | `handle_pause_input` |
| memory card, state, pending | `0x0013D390`, +0xDC, +0xE4 | `0x0013D290`, +0xD4, +0xDC | PAL `handle_pause_input`; US `run_level_main_loop` |
| menu (relocated) | `0x001D5F70` | `0x001D5BF0` | `PauseAllSounds`, `update_resident_gameplay_state` |
| the ship's list | `0x0013D618` | `0x0013D510` | `unlock_planet`, `initialize_selection_entries_and_index` |
| camera's hold (relocated) | `0x0018C42C` | `0x0018C32C` | `PauseAllSounds` |
| game mode (level 0) | `0x0015F6A8` | `0x0015F5C4` | the level loop |
| ticks in the mode (level 0) | `0x0015F6AC` | `0x0015F5C8` | the level loop |
| loop's end (level 0) | `0x0015F650` | `0x0015F570` | the level loop |
| take-off asked (level 0) | `0x0015F710` | `0x0015F630` | `handle_pause_input` |
| debug menu (level 0) | `0x0016C158` | `0x0016C058` | `handle_pause_input` |
| the menu's target (level 0) | `0x00184514` | `0x00184414` | `update_pause_menu`'s restore, `update_menu_entry_actions` |
| load level (level 0) | `0x0015F6A4` | `0x0015F5C0` | `ShipTravelTo`'s travel branch |
| menu opener | `func_00219C70` | `FUN_00218d78` | both builds' function tables |

The structures' offsets are the same on both discs (`src/rac1/rac1.h`): the hero's state +0x2084,
control mode +0x208C, vehicle mode +0x20A4, `fall_fade_pending` +0x20B1, `on_hoverboard` +0x20B2,
health +0x22A8, his moby +0x2080, the item slot's switch lock +0x10AC; a moby's state +0x20 and flags
+0x34; the menu's state +0, next page +8, close request +0xC, timer +0x14, waiting page +0xF0; a
page's state +0x3C. The PAL v2.00 handle_pause_input adds the card check to the pause and the map
that the US v1.00 one lacks; the mod applies it on both, from each disc's own fields.

## 6. Limits

- The take-off is not played: the menu appears over the level where Ratchet stands. After a planet
  is chosen the camera cuts to the parked ship leaving its pad, as the game's state 3 shows it, and
  Ratchet is not seen leaving.
- On Veldin the list holds only the planets the story has added (none in a new game's first
  minutes) and never Veldin itself: there the planet he is on cannot be travelled to, and Blast Off
  on it lands the ship again, play going on where Ratchet stood (the game's landing).
- "The planet you are on" is a whole trip: the ship's flight out of the level, the flight between
  planets, the load and the arrival landing, some 30 seconds, as for any other planet; the game saves
  to the card on the way, as it does for any trip.
- The restart's travel branch runs with -1 as its planet for a tick: a mod that takes
  `rac1.ship.travel_to` before this one (the multiplayer mod, loaded after it) sees -1 there.
- A party (the multiplayer mod): only the host's travel takes the party (the multiplayer mod's
  rule); a member's choice of another planet, or of its own for a restart, is the multiplayer mod's
  to turn into a landing. Not play-tested;
  on PAL the multiplayer mod's own `rac1.ship.travel_to` is skipped by the port (section 2).
