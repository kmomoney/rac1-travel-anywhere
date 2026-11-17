# rac1-travel-anywhere: the plan

A native OpenRAC mod that opens Ratchet & Clank's (rac1) own ship planet select, the Galactic Map,
from anywhere in play, built as `openrac-mods/docs/GUIDE.md` describes. This file is the plan: what
was decided, how the mod is built, and where the work stands. The evidence is in `DESIGN.md`, the
play-test in `PLAYTEST.md`, what players read in `README.md`.

## Status (2026-11-16)

| Phase | State |
|---|---|
| 0. Repository | Done: `~/Projects/rac1-travel-anywhere`, its own git repository on `master`, from OpenRAC's `sdk/template`; SDK 1.14.4 pinned as rac1-enhanced-movement pins it (`sdk.lock`, `tools/fetch_sdk.py`); its `cpp_style.py` and CI workflow (`.github/workflows/build.yml`) copied |
| 1. Research | Done: DESIGN.md. The ship's trigger, take-off, menu (`PauseAllSounds` in state 14), list (`gGalaxyList`), pages and buttons, close and `ShipTravelTo`; what backing out does at the ship (the landing again); the hook points and calls the SDK offers; both discs' addresses from the decompilations and the C OpenRAC generates |
| 2. Decisions and unit tests | Done: `src/rac1/travel.h` (the combo, when it is the mod's, every refusal, the page a press reaches, the session's presses, the close); `travel` test |
| 3. Game layer and load test | Done: `src/rac1/game.cpp`, `discs.h` (PAL v2.00 and NTSC-U rows), `rac1.h`; the `load` test on both rows, the `manifest` test |
| 4. Build and checks | Done: 3 of 3 tests, `cpp_style.py` and `--comments` clean, `package_mod.py --check` ok, from a clean build |
| 5. Play-test, NTSC-U | Done by script in OpenRAC's port (PLAYTEST.md, `tools/playtest_matrix.sh`, 14 runs): open and back out on Veldin, Novalis and Kerwan (pictures), mid-air, the combo closing the menu again, a travel Kerwan to Novalis to arrival (pictures), the planet he is on, near and far from the ship (pictures), refusals while dying, in a cutscene, in Clank's Orxon start and on Novalis before its ship is there, and a route without the trigger tick for tick as without the mod |
| 6. Play-test, PAL v2.00 | Done, the same matrix and the same results (PLAYTEST.md). Found on the way: OpenRAC's PAL build calls the game's `ShipTravelTo` past the hook point, so `rac1.ship.travel_to` never fires on PAL; the mod does not depend on it (DESIGN.md section 2). Reported here, not changed in OpenRAC |
| 7. Docs and commit | Done: README (controls, settings, limits, other mods, notes), DESIGN, PLAYTEST, this file; committed on `master`, locally only |
| 8. Release (0.1.0) | Done on your go-ahead: github.com/kmomoney/rac1-travel-anywhere, tag `rac1-travel-anywhere-v0.1.0` released from `8451a37` |
| 9. The planet he is on restarts the level (0.1.1) | Done on your request, committed locally (not pushed, tagged or released): Blast Off on the planet Ratchet is on travels there through the game's own travel branch (the restore's target steered to -1, the load level put back), so the level starts anew as on arrival; backing out unchanged; Veldin's first visit, never in the ship's list, keeps the landing. `travel` and `load` tests extended, the play-test matrix (17 runs) rerun on both discs with the restart near and far from the ship, Veldin's landing, what a restart resets against a trip to Aridia and back, and every 0.1.0 route as a regression: the same results on both (PLAYTEST.md) |

## 1. Decisions

| | Decided |
|---|---|
| Id, name, folder | `rac1-travel-anywhere`, "Travel anywhere", `~/Projects/rac1-travel-anywhere`. No mod in the openrac-mods index has a like id; `fast-travel` is the nearest name |
| The menu | The game's own planet select, opened as the ship's take-off opens it (`PauseAllSounds(14)`): only the planets the game lists, every page and button the game's, progression untouched |
| Another planet | The game's `ShipTravelTo`: the save to the card, the ship leaving its pad, the flight, the landing |
| The planet he is on | 0.1.1: the game's travel to it, as to any other planet (the save, the flight out, the flight, the load, the arrival landing): the level starts anew, as you asked ("selecting the current level should reset them to the beginning of the level"). The game's own path, steered: `ShipTravelTo` is handed -1 (its travel branch; its save then resumes on the level played) and the load level is put back to the planet. 0.1.0 played the game's landing and left Ratchet where he stood. A planet the ship's list does not hold (Veldin's first visit) keeps the landing |
| Backing out | Not the game's (which lands the ship): the menu's close becomes the map's close, which plays on where he was, as he was |
| Trigger | A button combination, L1 + R1 then Select (or L2 + R2 then Select, or off), plus an optional keyboard key. Not a pause menu entry: the SDK's (`ui_pause_item`) is an Options item opening a page of the mod's, three levels down, and reaching the planet select from it would mean closing one menu to open another. Select alone keeps the map; L1 and R1 keep looking and crouching; the combo's Select is the mod's only in play |
| Where it opens | In play only, and not where it could break the game or where the game itself would not fly (DESIGN.md section 4); a notice says why |
| Hook points | `rac1.ship.travel_to` only, plus `on_pad` and `on_frame`: SDK 1.14.4, no OpenRAC change |
| api | `1.2`, for the keybind setting; `play_notice` (1.9) is used where the port has it |
| Multiplayer | `gameplay`: a travel changes the world for a party. Each setting is the player's own (`own`): the buttons and the key are preferences, the trace a debugging aid |
| Other mods | No conflicts (README, "Other mods"); `load.after` keyboard-mouse and button-swap so the combo is read on the buttons they press |
| Discs | PAL v2.00 and NTSC-U v1.00, both play-tested |

## 2. How it is built

- `src/mod.cpp`: the entry point, rac1 only.
- `src/rac1/travel.h`: the decisions in plain C, no game memory.
- `src/rac1/game.cpp`: reads the game into them, opens the menu, rewrites a back-out's close,
  observes `ShipTravelTo`.
- `src/rac1/discs.h`, `src/rac1/rac1.h`: the addresses per disc, the offsets both share.
- `tests/`: `travel_test.c`, `load_test.c` (both rows), `manifest_test.py`.
- `tools/playtest.sh`, `tools/playtest_analyze.py`: the scripted play-test and its reading.

## 3. Open questions for later

- A party: the multiplayer mod's travel rules with this menu, not play-tested.
- A real playthrough from a memory card save (every run here starts a level directly).
- Listing it in the openrac-mods index (`index/rac1-travel-anywhere.json`) once it is released.
