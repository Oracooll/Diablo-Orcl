"""Ball Lightning's ball and the lightning strikes it, Faraday Ring and Lightning Rod shoot (user, 2026-10-01).

Input: "Resources/02. Oracooll Assets/Sorc Skills/Ball Lightning/" -
  lightning-mesh-ball-organic-72f/lightning-mesh-organic-72f-black.png  72 frames of 64x64, 12 across and 6 down, a
      seamless 3-second loop, white on black;
  lightning-strike-portfolio[-extra-12]/*-black.png  24 strike pieces, white on black, every one pointing right and
      entering at x=0, y=32: short-01..08 (64x64, a close discharge), chain-01..08 (64x64, a body that also leaves at
      its right edge, y=32), long-01..04 (128x64, a long body) and long-impact-01..04 (128x64, a body ending in the
      fork that hits).

Output, in Packaging/resources/oracool_assets/missiles/:
  ball_lightning.png          72 frames of 64x64 in one row;
  lightning_strike_short.png  8 rows (variants) of 32 frames (angles), 96x96;
  lightning_strike_chain.png  8 rows of 32, 96x96;
  lightning_strike_long.png   4 rows of 32, 144x144;
  lightning_strike_impact.png 4 rows of 32, 144x144.

The engine cannot turn a sprite, so every piece is turned here: frame a of a row is the piece pointing at screen angle
a * 360/32 degrees, clockwise from east (screen y grows down), turned about the centre of its axis (w/2, 32) - which
lands on the frame's centre. The game (missiles.cpp, AddLightningStrike) lays the pieces end to end along the exact
line and picks the nearest angle for each.

Colour: the grey is a brightness, mapped to an ice-blue ramp (deep blue glow, sky-blue body, white core). The engine
draws CLX: no partial alpha, so the faint glow is an ordered dither and black is no pixel. One colour table a sheet.

Usage: python tools/BuildBallLightning.py <Ball Lightning folder> <out folder>
Needs Pillow and numpy.
"""
import math
import os
import sys

import numpy as np
from PIL import Image

SRC, OUT = sys.argv[1], sys.argv[2]
ANGLES = 32
LEVELS = 48  # brightness steps on the ramp: well inside a sheet's 255 colours

BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32) / 16.0 + 1 / 32.0
SOLID = 0.34  # brightness from which a pixel is always drawn
FAINT = 0.07  # brightness below which it never is; between the two, the dither


def ramp(level):
    """Brightness 0..1 -> RGB on the ice-blue ramp."""
    stops = [(0.0, (16, 34, 120)), (0.35, (40, 96, 220)), (0.65, (110, 180, 255)), (0.85, (200, 230, 255)), (1.0, (255, 255, 255))]
    for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
        if level <= t1:
            k = (level - t0) / (t1 - t0)
            return tuple(int(round(a + (b - a) * k)) for a, b in zip(c0, c1))
    return stops[-1][1]


RAMP = [ramp(i / (LEVELS - 1)) for i in range(LEVELS)]


def colourise(lum):
    """Brightness array (0..1) -> RGBA: ramp colour, binary alpha with the glow dithered."""
    h, w = lum.shape
    threshold = np.tile(BAYER4, (h // 4 + 1, w // 4 + 1))[:h, :w]
    keep = (lum >= SOLID) | ((lum >= FAINT) & ((lum - FAINT) / (SOLID - FAINT) > threshold))
    idx = np.clip(np.round(lum * (LEVELS - 1)), 0, LEVELS - 1).astype(int)
    lut = np.array(RAMP, dtype=np.uint8)
    out = np.zeros((h, w, 4), dtype=np.uint8)
    out[..., :3] = lut[idx]
    out[..., 3] = np.where(keep, 255, 0)
    out[~keep, :3] = 0
    return out


def load_grey(path):
    return np.asarray(Image.open(path).convert("L")).astype(np.float32) / 255.0


def rotate(grey, theta, cell):
    """The piece turned to screen angle theta about (w/2, 32), bilinear, on a cell x cell canvas centred there."""
    h, w = grey.shape
    cx, cy = w / 2.0, 32.0
    c = (cell - 1) / 2.0
    ys, xs = np.mgrid[0:cell, 0:cell].astype(np.float32)
    dx, dy = xs - c, ys - c
    ct, st = math.cos(theta), math.sin(theta)
    sx = dx * ct + dy * st + cx - 0.5
    sy = -dx * st + dy * ct + cy - 0.5
    x0, y0 = np.floor(sx).astype(int), np.floor(sy).astype(int)
    fx, fy = sx - x0, sy - y0
    pad = np.pad(grey, 1)

    def at(yy, xx):
        inside = (xx >= -1) & (xx <= w) & (yy >= -1) & (yy <= h)
        return np.where(inside, pad[np.clip(yy + 1, 0, h + 1), np.clip(xx + 1, 0, w + 1)], 0.0)

    return (at(y0, x0) * (1 - fx) * (1 - fy) + at(y0, x0 + 1) * fx * (1 - fy) + at(y0 + 1, x0) * (1 - fx) * fy
            + at(y0 + 1, x0 + 1) * fx * fy)


def pieces(kind):
    found = []
    for folder in ("lightning-strike-portfolio", "lightning-strike-portfolio-extra-12"):
        d = os.path.join(SRC, folder)
        found += [os.path.join(d, f) for f in sorted(os.listdir(d)) if f.startswith(kind + "-") and f.endswith("-black.png")
                  and (kind != "long" or not f.startswith("long-impact"))]
    return found


def save(rgba, name):
    path = os.path.join(OUT, name)
    Image.fromarray(rgba, "RGBA").save(path, optimize=True)
    colours = {tuple(p[:3]) for p in rgba.reshape(-1, 4) if p[3] == 255}
    print(f"{name}: {rgba.shape[1]}x{rgba.shape[0]}, {len(colours)} colours")
    assert len(colours) <= 255


# The ball: 72 frames, one row.
sheet = load_grey(os.path.join(SRC, "lightning-mesh-ball-organic-72f", "lightning-mesh-organic-72f-black.png"))
ball = np.zeros((64, 64 * 72, 4), dtype=np.uint8)
for i in range(72):
    r, c = divmod(i, 12)
    # A touch brighter than the strikes (gamma 0.8): its mesh is mostly faint lines, and dim it read as a dark blue knot.
    ball[:, 64 * i:64 * (i + 1)] = colourise(sheet[64 * r:64 * (r + 1), 64 * c:64 * (c + 1)] ** 0.8)
save(ball, "ball_lightning.png")

# The strikes: a row per variant, a frame per angle.
for kind, cell, out in (("short", 96, "lightning_strike_short.png"), ("chain", 96, "lightning_strike_chain.png"),
                        ("long", 144, "lightning_strike_long.png"), ("long-impact", 144, "lightning_strike_impact.png")):
    files = pieces(kind)
    assert len(files) == (8 if kind in ("short", "chain") else 4), (kind, files)
    rgba = np.zeros((cell * len(files), cell * ANGLES, 4), dtype=np.uint8)
    for row, f in enumerate(files):
        grey = load_grey(f)
        for a in range(ANGLES):
            turned = rotate(grey, 2 * math.pi * a / ANGLES, cell)
            rgba[cell * row:cell * (row + 1), cell * a:cell * (a + 1)] = colourise(turned)
    save(rgba, out)
