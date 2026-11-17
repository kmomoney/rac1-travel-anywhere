#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reads play-test runs (tools/playtest.sh) back from the port's per-tick trace (OPENRAC_TRACE).

    python3 tools/playtest_analyze.py --modes RUN.jsonl
    python3 tools/playtest_analyze.py --around RUN.jsonl
    python3 tools/playtest_analyze.py --same A.jsonl B.jsonl
    python3 tools/playtest_analyze.py --mobys RUN.log CLASS X Y Z RADIUS FRAME [FRAME ...]

--modes lists each change of the level (lv) and its game mode (m: 0 play, 1 a movie, 2 a cutscene,
3 a menu, 4 a dialog, 5 a vendor, 6 the ship's landing, take-off or flight), with the frame (f), the
frames from control (c), the hero's state (hs) and position (hp), and the run's last tick.

--around compares, for each menu (a stretch of game mode 3) that play follows in the same level,
the last tick of play before it with the first after it: the hero's position, rotation and state,
the camera's position, and its forward from the second tick after (the first still has the menu's
basis, as after any of the game's menus, until the camera's next update). "same" means equal to
the trace's four decimals.

--same counts the ticks from control where two runs differ in the level, the game mode, the hero's
position, rotation or state, or the camera, and shows the first. It compares runs of the mod with no
trigger pressed against runs without the mod.

--mobys reads the port's moby log (OPENRAC_MOBY_LOG=N:RADIUS, in the run's log) and counts, at each
FRAME it logged, the mobys of CLASS within RADIUS of (X, Y, Z): crates (500) or loose bolts (13)
still there before and after a level loads again. FRAME may be `first` (the first frame logged at or
after control) or `last` (the run's last).
"""
import json
import math
import re
import sys


def rows(path):
    out = []
    for line in open(path):
        line = line.strip()
        if line:
            out.append(json.loads(line))
    return out


def modes(path):
    rs = rows(path)
    prev = None
    for r in rs:
        key = (r.get("lv"), r.get("m"))
        if key != prev:
            print(f"f {r['f']:5} c {str(r.get('c')):>5}  level {key[0]:>2} mode {key[1]:>2}  state {r.get('hs')}  hero {r.get('hp')}")
            prev = key
    last = rs[-1]
    print(f"f {last['f']:5} c {str(last.get('c')):>5}  (last)  level {last.get('lv')} mode {last.get('m')}  state {last.get('hs')}  hero {last.get('hp')}")


def around(path):
    rs = rows(path)
    found = 0
    i = 1
    while i < len(rs):
        # A menu opening out of play.
        if rs[i].get("m") == 3 and rs[i - 1].get("m") == 0:
            before = rs[i - 1]
            j = i
            while j < len(rs) and rs[j].get("m") == 3:
                j += 1
            # Play after it, in the same level, with a second tick for the camera's forward.
            if j + 1 < len(rs) and rs[j].get("m") == 0 and rs[j].get("lv") == before.get("lv"):
                after, settled = rs[j], rs[j + 1]
                found += 1
                print(f"menu from f {before['f']} (c {before.get('c')}) to f {after['f']} (c {after.get('c')}), "
                      f"{after['f'] - before['f'] - 1} frames, level {before.get('lv')}")
                for name, a, b in (
                    ("position", before.get("hp"), after.get("hp")),
                    ("rotation", before.get("hr"), after.get("hr")),
                    ("state", before.get("hs"), after.get("hs")),
                    ("camera", before.get("cam"), after.get("cam")),
                    ("forward", before.get("fw"), settled.get("fw")),
                ):
                    print(f"  {name:8} {'same' if a == b else 'DIFFERS'}  {a}  ->  {b}")
            i = j
        i += 1
    if not found:
        print("no menu that play follows in the same level")


def same(path_a, path_b):
    a = {r["c"]: r for r in rows(path_a) if r.get("c") is not None}
    b = {r["c"]: r for r in rows(path_b) if r.get("c") is not None}
    keys = ("lv", "m", "hp", "hr", "hs", "cam", "fw")
    both = sorted(set(a) & set(b))
    differ = [c for c in both if any(a[c].get(k) != b[c].get(k) for k in keys)]
    print(f"{len(both)} ticks from control in both, {len(differ)} differ "
          f"(only in one: {len(set(a) ^ set(b))})")
    if differ:
        c = differ[0]
        print(f"first at c {c}:")
        for k in keys:
            if a[c].get(k) != b[c].get(k):
                print(f"  {k}: {a[c].get(k)}  vs  {b[c].get(k)}")


def mobys(path, cls, x, y, z, radius, frames):
    seen = {}
    control = 0
    for line in open(path):
        c = re.search(r"control at frame (\d+)", line)
        if c and not control:
            control = int(c.group(1))
        m = re.match(r"\[info\] moby (\d+) class (\d+) at \S+ state \d+ .* pos (\S+) (\S+) (\S+)", line)
        if m and int(m.group(2)) == cls:
            px, py, pz = (float(m.group(i)) for i in (3, 4, 5))
            near = math.dist((px, py, pz), (x, y, z)) <= radius
            seen.setdefault(int(m.group(1)), []).append((px, py, pz) if near else None)
    logged = sorted(seen)
    for f in frames:
        if f == "first":
            f = next((g for g in logged if g >= control), None)
        elif f == "last":
            f = logged[-1] if logged else None
        else:
            f = int(f)
        if f not in seen:
            print(f"f {f}: not logged")
            continue
        here = [p for p in seen[f] if p is not None]
        print(f"f {f}: {len(here)} of class {cls} within {radius} of ({x}, {y}, {z})")


def main(argv):
    if len(argv) == 3 and argv[1] == "--modes":
        modes(argv[2])
    elif len(argv) == 3 and argv[1] == "--around":
        around(argv[2])
    elif len(argv) == 4 and argv[1] == "--same":
        same(argv[2], argv[3])
    elif len(argv) >= 9 and argv[1] == "--mobys":
        mobys(argv[2], int(argv[3]), float(argv[4]), float(argv[5]), float(argv[6]), float(argv[7]),
              argv[8:])
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
