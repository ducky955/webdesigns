"""Generate icon.png, the 16x16 padlock icon shown by shells like Cesium.

Usage: python3 tools/make_icon.py   (needs Pillow: pip install pillow)
"""

from pathlib import Path

from PIL import Image

BG = (16, 18, 26)        # matches COL_BG in src/ui.c
SHACKLE = (150, 160, 184)
BODY = (58, 128, 246)    # matches COL_ACCENT
BODY_EDGE = (34, 84, 180)
HOLE = (16, 18, 26)

# One character per pixel: . background, s shackle, b body, e body edge, k keyhole
PIXELS = [
    "................",
    ".....ssssss.....",
    "....ss....ss....",
    "...ss......ss...",
    "...s........s...",
    "...s........s...",
    "...s........s...",
    "..eeeeeeeeeeee..",
    "..ebbbbbbbbbbe..",
    "..ebbbbkkbbbbe..",
    "..ebbbbkkbbbbe..",
    "..ebbbbbkbbbbe..",
    "..ebbbbbkbbbbe..",
    "..ebbbbbbbbbbe..",
    "..eeeeeeeeeeee..",
    "................",
]

COLORS = {".": BG, "s": SHACKLE, "b": BODY, "e": BODY_EDGE, "k": HOLE}


def main() -> None:
    assert len(PIXELS) == 16 and all(len(row) == 16 for row in PIXELS)
    img = Image.new("RGB", (16, 16))
    for y, row in enumerate(PIXELS):
        for x, ch in enumerate(row):
            img.putpixel((x, y), COLORS[ch])
    out = Path(__file__).resolve().parent.parent / "icon.png"
    img.save(out)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
