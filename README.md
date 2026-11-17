# Travel anywhere

Open the ship's planet select from anywhere in the first Ratchet & Clank, on
[OpenRAC](https://github.com/OpenRAC/OpenRAC)'s native port. Hold **L1 + R1** and press
**Select** to get the game's own Galactic Map, the same menu as at Ratchet's ship. Pick a planet
and the ship flies you there, or back out and carry on right where you were.

## Install

In the OpenRAC launcher, open **Mods**, choose **Add from a link** and paste:

```
https://github.com/kmomoney/rac1-travel-anywhere
```

It works with the **PAL v2.00** (SCES-50916) and **NTSC-U v1.00** (SCUS-97199) discs. With any
other disc, the launcher says why it won't load.

## What it does

The menu lists the planets the story has unlocked, with each one's missions and gold bolts, and
plays their infobots, exactly as at the ship. What you choose decides what happens:

| You choose | What happens |
|---|---|
| **Another planet** (Cross, then *Blast Off*) | The same trip as from the ship: the game saves to your memory card, the ship takes off and flies you there. |
| **Back out** (Triangle, Start, Select or the combo again) | The menu closes and you carry on exactly where you were, even mid-jump. The ship doesn't land and you aren't moved. |
| **The planet you're on** (Cross, then *Blast Off*) | A full trip back to the same planet, with the save, the flight and the landing. The level restarts from its beginning. |

Restarting resets the level the same way arriving from another planet does. Loose bolts are back,
but anything the game records as done stays done (broken crates stay broken). You keep everything
you carry: bolts, weapons, gadgets and unlocked planets.

## Controls

| Button | What it does |
|---|---|
| **L1 + R1 + Select** | Opens the planet select. Hold L1 and R1, then press Select. Change it with the *Buttons* setting. |
| Select alone | Opens the map, as usual. |
| L1, R1 | Look and crouch, as usual, even while you hold them for the combo. |
| In the menu | Up and Down pick a planet. Cross opens its page, and Cross again is *Blast Off*. Circle plays its infobot. Triangle goes back, or closes the menu from the planet list. |
| Keyboard | With OpenRAC's default keys, the combo is Left Shift + Q + Backspace. You can also set a single key with the *Keyboard key* setting. |

The combo only works in normal play. In menus, cutscenes and loading screens, the buttons do what
they always do. In play, the game never sees the combo's Select, so the map doesn't open with it.

## When it won't open

It opens almost anywhere in play, mid-jump included. Swimming, grinding, hanging from a ledge and
boss fights aren't blocked either. It stays shut at moments where travelling could break the game,
or where the game itself wouldn't let you fly, and a message at the top of the screen says why.

| When | Why |
|---|---|
| A cutscene, a menu, a dialog, a vendor, loading, or the ship's own take-off, flight or landing | Not normal play: the buttons work as usual and no message shows. |
| A split second after any of those, during a weapon switch, or just before a dialog opens | The game's own pause menu waits too. Try again a moment later. |
| Ratchet dying, drowning, falling off the world or respawning | He's about to go back to a checkpoint. |
| Playing as Clank or Giant Clank | Clank can't fly the ship, and Giant Clank has his own screen. |
| Riding the hoverboard, a vehicle or a turret, or flying the ship | The game's map is off there too. |
| No ship to board: a planet without one, or one the story keeps hidden (Novalis before the dropship scene) | The trip needs the ship. |
| The ship about to take off (Orxon's scenes at the ship, or a take-off you already started) | The game is already sending it off. |
| The memory card saving or loading | The trip saves to it. |
| The debug menu stepping frames | The game's map waits too. |

Older OpenRAC versions show no message, but the menu still stays shut.

## Settings

Change these in the launcher. They take effect the next time you start the game.

| Setting | Default | What it does |
|---|---|---|
| **Buttons** | L1 + R1 + Select | Or L2 + R2 + Select, or *Off* to use only the keyboard key. In a party, each player picks their own. |
| **Keyboard key** | None | A key that also opens the planet select. Pick one the game's controls don't use. The defaults use WASD, IJKL, Space, E, F, R, Q, Left Shift, 1, 3, C, V, Enter, Backspace and the arrow keys. |
| **Trace** | Off | For bug reports. Writes what the mod sees and decides to the game's log. |

## Other mods

No conflicts are declared with any of these.

- **fast-travel** and **better-pause-menu**: both travel from the pause menu. This mod opens the
  ship's own menu instead, with the ship's usual save, flight and landing. Use whichever you like.
- **debug-menu**: it uses R3, which the combo doesn't. The planet select won't open while the
  debug menu is open or stepping frames.
- **multiplayer**: in a party, the multiplayer mod lets only the host's trips move the party. Not
  yet tested together. On the PAL disc, OpenRAC currently skips the multiplayer mod's ship rule,
  for this menu and at the ship alike ([DESIGN.md](DESIGN.md), section 2).
- **rac1-enhanced-movement** and **lock-strafe**: they strafe on L2 or R2. The default combo uses
  L1 + R1, so they don't clash. If you set *Buttons* to L2 + R2, Ratchet also strafes while you
  hold them.
- **keyboard-mouse** and **button-swap**: this mod loads after them, so the combo follows the
  buttons they press or swap.

## Known limits

- The take-off isn't shown. After *Blast Off*, the view cuts to the parked ship leaving its pad,
  and you don't see Ratchet board.
- Restarting the level takes a full trip, about half a minute, as flying to any planet does.
- On Veldin at the start of the game, the list only has the planets you've unlocked (none at
  first), and never Veldin itself, because the ship can't fly there. *Blast Off* on Veldin lands
  the ship again and you carry on where you stood.
- It has only been tested by script, each run starting a level directly. It hasn't been tested
  from a memory card save, in a party or in a boss fight. See [PLAYTEST.md](PLAYTEST.md).

## Release notes

- **0.1.1**: Choosing the planet you're on now restarts the level with a full trip there. It used
  to land the ship and leave you where you stood (Veldin still does).
- **0.1.0**: First release, for the PAL v2.00 and NTSC-U v1.00 discs.

## For developers

How the game's menu works and how the mod opens it: [DESIGN.md](DESIGN.md). The plan:
[PLAN.md](PLAN.md). How it was tested: [PLAYTEST.md](PLAYTEST.md).

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
`OPENRAC_TRAVEL_ANYWHERE_TRACE=1` turns the trace on without the setting. `tools/playtest.sh` plays
a scripted route in OpenRAC's port, hidden, and `tools/playtest_analyze.py` reads its trace back.

No game data is in this repository: only addresses, offsets and numbers worked out from the
decompilations. License: GPL-3.0-or-later.
