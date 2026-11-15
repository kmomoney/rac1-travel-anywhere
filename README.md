# Travel anywhere

Opens the ship's planet select, the **Galactic Map** Ratchet gets at his ship, from anywhere you
play in the first Ratchet & Clank on [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s native port.
Hold **L1 and R1** and press **Select**.

It is the game's own menu: it lists the planets the ship would list (the ones the story has
unlocked, in the order you found them), shows each one's missions and gold bolts, and plays its
infobot. What you choose does what it does at the ship:

- **another planet** (Cross, then *Blast Off*): the game saves to the memory card, the ship leaves
  its pad, flies there and lands, exactly as when you leave from the ship;
- **back out** (Triangle, Start or Select on the planet list, or the combo again): the menu closes
  and you are where you were, as you were: the same spot, the same move (mid-jump stays mid-jump),
  the same camera. Your ship does not land and you are not moved to it;
- **the planet you are on** (Cross on it, then *Blast Off*): what the game does at the ship, the
  ship lands again on its pad (Ratchet and Clank climbing out), then play goes on where you stood.

How the game's menu works and how the mod opens it, with the evidence from the decompilations:
[DESIGN.md](DESIGN.md). The plan and where it stands: [PLAN.md](PLAN.md). How it was play-tested:
[PLAYTEST.md](PLAYTEST.md).

**Discs:** Ratchet & Clank PAL v2.00 (SCES-50916) and NTSC-U v1.00 (SCUS-97199), the two the other
mods for the first game support. Others are refused, with a reason, in the launcher.

## Controls

| | |
|---|---|
| **L1 + R1, then Select** | Opens the planet select (setting *Buttons*). Hold both shoulder buttons first, then press Select. |
| Select alone | The game's map, as ever. |
| L1, R1 | Look and crouch, as ever, also while you hold them for the combo. |
| In the menu | The game's buttons: Up and Down choose a planet, Cross goes to its page, Cross again blasts off, Circle plays its infobot, Triangle goes back or closes the menu. On the planet list Select, Start and the combo close it too. |
| A key | Optional (setting *Keyboard key*). With the keyboard the combo is Shift + Q + Backspace on the port's default keys. |

The combo's Select is the mod's: the game never sees it, so it opens the planet select instead of
the map. Outside play (in a menu, a cutscene, while a planet loads) the mod leaves every button to
the game, so the combo is just L1, R1 and Select there; in the planet select itself that Select
closes it.

## Where it does not open

In play it opens almost anywhere: standing, running, mid-jump, swimming, grinding, hanging from a
ledge; boss fights are not refused either. Where it would break the game, or where the game itself
would not let you fly, it stays shut and a line at the top of the picture says why:

| Where | The line | Why |
|---|---|---|
| A cutscene, a movie, the ship's own landing, take-off or flight, a menu, a dialog, a vendor, a planet loading | (nothing: the buttons are the game's there) | Not play |
| The first moments after any of those | *not yet* | The game's own pause waits as long (8 ticks) |
| Ratchet dying, drowning, falling off the world or respawning | *not while Ratchet is down* | The game puts him back at a checkpoint next |
| Clank on his own (Orxon's start, ...) | *Clank cannot fly the ship* | The game never lets Clank fly: at the ship he gets a scene instead |
| Giant Clank | *not as Giant Clank* | His Start opens his own screen |
| The hoverboard, a vehicle, a turret, the ship-flying sections | *not while riding* | The game's map is shut there too |
| A planet whose ship you cannot board yet (Novalis, before its dropship scene), or with no ship | *the ship is not here* | The game's own rule: the ship ignores you while the story hides it |
| Orxon's story scenes at the ship, a take-off already on its way | *the ship is busy* | The game is about to fly it itself |
| A weapon change, an item's cancellation, a dialog about to open, the game's menu about to reopen | *not now* | Moments the game's own pause and map wait out |
| The memory card saving or loading | *not while the memory card is busy* | The travel saves to it |
| The debug menu stepping the game a tick at a time | *not while the debug menu steps* | The game's map waits too |

The line shows for about two and a half seconds on a port of mod interface 1.9 or later (an older
port shows none; the menu stays shut all the same).

## Settings

| Setting | Default | |
|---|---|---|
| **Buttons** | L1 + R1, then Select | Or L2 + R2, then Select; or *Off* (the key alone, if one is set). Each player's own in a party. |
| **Keyboard key** | none | A key that opens it too. Pick one the game's controls do not use (the port's defaults use WASD, IJKL, Space, E, F, R, Q, Shift, 1, 3, C, V, Enter, Backspace and the arrows). |
| **Trace** | off | For bug reports: a line in the game's log at each press of the buttons with what the mod decided and everything it read to decide it, and at each step of its menu. Also `OPENRAC_TRAVEL_ANYWHERE_TRACE=1`. |

## Other mods

- **fast-travel** and **better-pause-menu** (QoL Hub): both travel to unlocked planets from the
  pause menu through OpenRAC's own travel call (`play_travel`, what the pause menu's Quit Game
  does with a planet in place of the title): the level ends and the game flies to the planet, with
  no save to the card and no ship leaving its pad. This mod opens the game's own Galactic Map
  instead, with the ship's save, flight and landing. They take no hook point of this mod's and none
  of its buttons, so they work together: use whichever you like for a trip. Their pages live in the
  pause menu, which the combo does not open over, and the planet select has no way into theirs.
- **debug-menu**: opens the game's debug menu on R3, which the combo does not use. While its menu
  is open, or while it steps the game a tick at a time, the planet select does not open.
- **multiplayer**: in a party only the host's ship takes the party to another planet (its rule): a
  member who picks another planet here lands its ship again where it stands. Backing out is never
  seen as a travel. Not play-tested in a party. On PAL, OpenRAC's port today calls the game's ship
  travel past the hook point both mods take (DESIGN.md section 2): this mod does not need it, the
  multiplayer mod's travel rule does.
- **rac1-enhanced-movement** (strafe on L2 or R2) and **lock-strafe**: the default combo holds L1
  and R1, which they leave as look and crouch. With *Buttons* on L2 + R2, holding them strafes
  first, which is harmless.
- **keyboard-mouse**, **button-swap**: the mod loads after them, so the combo is read on the buttons
  they press or swap.

No conflict is declared with any of them.

## Known limits

- The take-off is not played: the menu opens over the level where you stand. After *Blast Off* the
  picture cuts to the parked ship leaving its pad; Ratchet is not seen boarding.
- Choosing the planet you are on plays the landing at the ship's pad, then play goes on where you
  stood: the game's landing ends so. At the ship that is beside it; from afar it is a cut.
- On Veldin the list holds only the planets the story has added (none in a new game's first
  minutes): the game never lists Veldin itself, so there the menu can only land the ship or back
  out.
- Every scripted play-test started a level directly (OpenRAC's `--level`), with planets unlocked by
  writing the game's planet list; nothing was played from a memory card save, in a party, or on a
  boss fight (PLAYTEST.md).

## Building

```sh
python3 tools/fetch_sdk.py                 # the SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build && ctest --test-dir build --output-on-failure
python3 tools/cpp_style.py                 # layout (clang-format and the blank-line rules)
python3 tools/cpp_style.py --comments      # control blocks with no comment above them
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py --check openrac-mod.json
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/rac1_travel_anywhere -o dist
```

To try a build, add `build/package/rac1_travel_anywhere` in the launcher's **Mods** tab.
`tools/playtest.sh` plays a scripted route in OpenRAC's port, hidden, and
`tools/playtest_analyze.py` reads its trace back (PLAYTEST.md).

## Notes

- **0.1.0**: the first version, PAL v2.00 and NTSC-U v1.00, play-tested by script on both
  (PLAYTEST.md).

Nothing from the game's discs is in this repository: only addresses, offsets and numbers worked out
from the decompilations. GPL-3.0-or-later.
