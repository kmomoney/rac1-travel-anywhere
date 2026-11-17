#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Makes contact sheets of a play-test run's pictures (tools/playtest_matrix.sh, OPENRAC_SHOT).

    python3 tools/playtest_sheet.py OUT/shots OUT/sheets

For each run whose frames are in OUT/shots (NAME_<frame>.png), one sheet OUT/sheets/NAME.png: the
frames in order, four to a row, each 400 by 300 with its file name on it. Needs Pillow; without it,
it says so and makes none.
"""
import re
import sys
from pathlib import Path


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("playtest_sheet: no Pillow (python3 -m pip install pillow): no sheets made")
        return 0
    shots, sheets = Path(argv[1]), Path(argv[2])
    runs = {}
    for f in shots.glob("*.png"):
        m = re.fullmatch(r"(.+)_(\d+)\.png", f.name)
        if m:
            runs.setdefault(m.group(1), []).append((int(m.group(2)), f))
    sheets.mkdir(parents=True, exist_ok=True)
    w, h, cols = 400, 300, 4
    for name, frames in sorted(runs.items()):
        frames.sort()
        rows = (len(frames) + cols - 1) // cols
        sheet = Image.new("RGB", (w * cols, h * rows))
        for i, (_, f) in enumerate(frames):
            tile = Image.open(f).convert("RGB").resize((w, h))
            ImageDraw.Draw(tile).text((5, 5), f.name, fill=(255, 255, 0))
            sheet.paste(tile, ((i % cols) * w, (i // cols) * h))
        sheet.save(sheets / f"{name}.png")
        print(f"{sheets / (name + '.png')}: {len(frames)} frames")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
