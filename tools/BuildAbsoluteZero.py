"""Absolute Zero's missile sheet from the user's painted vortex frames (2026-10-01).

Input: "Resources/02. Oracooll Assets/Sorc Skills/Absolute Zero/Absolute Zero.png" - eight vortex frames, two across and
four down, on a green screen. They are variations rather than a clean turn (frame to frame they rotate -6..+11 degrees),
so each is keyed, centred on its eye and fitted to one ellipse, and the sheet adds an even spin on top: the painted
differences read as the vortex churning, the spin as its motion.

Output: 20 frames of 1024x512 in one row - the 8-tile hit diamond at 1x, the eye at the frame's centre. Grows out of the
feet over the first frames, spins with the arms trailing, a mild frost wave rolls outward, and it dissolves at the end.
The engine draws CLX: no partial alpha and at most 255 colours a sheet, so alpha is an ordered dither and the colours
are one shared palette quantised here (the true-colour loader then keeps them exactly).

Usage: python tools/BuildAbsoluteZero.py <painted sheet> <out absolute_zero.png> [preview stem]
Needs Pillow and numpy.
"""
import math
import sys

import numpy as np
from PIL import Image, ImageFilter

SRC, OUT = sys.argv[1], sys.argv[2]
PREVIEW = sys.argv[3] if len(sys.argv) > 3 else None
W, H, N = 1024, 512, 20
COLS, ROWS = 2, 4
SPIN_DEGREES = 120  # over the whole cast
FIT = 0.97  # the painted ellipse's share of the frame

sheet_in = Image.open(SRC).convert("RGB")
src = np.asarray(sheet_in).astype(np.float32)
SH, SW, _ = src.shape

# The green screen out: a pixel's greenness above its other channels keys it, edges are despilled, and keyed pixels go
# black - black is "no pixel" for the luminance alpha below.
r, g, b = src[..., 0], src[..., 1], src[..., 2]
greenness = g - np.maximum(r, b)
keep = 1.0 - np.clip((greenness - 20.0) / 80.0, 0.0, 1.0)
src[..., 1] = np.where(greenness > 0, np.maximum(r, b), g)
src = src * keep[..., None] / 255.0
lum = src.max(axis=2)

# Each frame: its cell, its eye (the peak of a heavily blurred luminance) and its ellipse's half axes.
blurred = np.asarray(Image.fromarray((lum * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(18))).astype(np.float32)
frames_src = []
for row in range(ROWS):
    for col in range(COLS):
        x0, x1 = col * SW // COLS, (col + 1) * SW // COLS
        y0, y1 = row * SH // ROWS, (row + 1) * SH // ROWS
        cell = blurred[y0:y1, x0:x1]
        # Peak in the middle half only, so a bright rim never wins.
        h, w = cell.shape
        inner = np.full_like(cell, -1)
        inner[h // 4: 3 * h // 4, w // 4: 3 * w // 4] = cell[h // 4: 3 * h // 4, w // 4: 3 * w // 4]
        ey, ex = np.unravel_index(np.argmax(inner), inner.shape)
        ys, xs = np.where(lum[y0:y1, x0:x1] > 0.12)
        ax = max(ex - xs.min(), xs.max() - ex)
        ay = max(ey - ys.min(), ys.max() - ey)
        frames_src.append((x0 + ex, y0 + ey, ax, ay))
        print(f"frame {len(frames_src) - 1}: eye ({x0 + ex},{y0 + ey}) half axes {ax} x {ay}")


def bilinear(x, y):
    x = np.clip(x, 0, SW - 1.001)
    y = np.clip(y, 0, SH - 1.001)
    x0 = np.floor(x).astype(int)
    y0 = np.floor(y).astype(int)
    fx = (x - x0)[..., None]
    fy = (y - y0)[..., None]
    return (src[y0, x0] * (1 - fx) * (1 - fy) + src[y0, x0 + 1] * fx * (1 - fy)
            + src[y0 + 1, x0] * (1 - fx) * fy + src[y0 + 1, x0 + 1] * fx * fy)


# Which way the arms wind: the angle shift that carries an inner ring onto an outer one. Arms trail the spin, so the
# vortex turns against the outward winding.
def ring(i, rad):
    ex, ey, ax, ay = frames_src[i]
    t = np.radians(np.arange(360))
    xs = ex + rad * np.cos(t) * ax
    ys = ey + rad * np.sin(t) * ay
    p = bilinear(xs, ys).max(axis=1)
    return (p - p.mean()) / (p.std() + 1e-6)


winding = []
for i in range(len(frames_src)):
    a, b2 = ring(i, 0.35), ring(i, 0.55)
    winding.append(max(range(-90, 91), key=lambda s: float((np.roll(a, s) * b2).mean())))
outward = float(np.median(winding))
spin_sign = -1.0 if outward > 0 else 1.0
print(f"arms wind {outward:+.0f} deg outward (screen angle, y down); the spin turns {'counter-' if spin_sign < 0 else ''}clockwise")

oy, ox = np.mgrid[0:H, 0:W].astype(np.float32)
u = (ox + 0.5 - W / 2) / (W / 2 * FIT)
v = (oy + 0.5 - H / 2) / (H / 2 * FIT)
rad = np.sqrt(u * u + v * v)
ang = np.arctan2(v, u)
BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32)
bayer = (np.tile(BAYER, (H // 4, W // 4)) + 0.5) / 16.0


def ease_out(t):
    return 1 - (1 - t) ** 3


rgbs, masks = [], []
for f in range(N):
    t = f / N
    ex, ey, ax, ay = frames_src[f % len(frames_src)]
    grow = 0.30 + 0.70 * ease_out(min(f / 5.0, 1.0))
    theta = spin_sign * math.radians(SPIN_DEGREES) * t
    rr = rad / grow
    a = ang - theta
    rgb = bilinear(ex + rr * np.cos(a) * ax, ey + rr * np.sin(a) * ay)
    rgb[rr > 1.0] = 0
    # The frost wave: a mild brightness band rolling outward, the painting's own colours kept.
    wave = 0.5 + 0.5 * np.sin(2 * math.pi * (rr * 2.0 - t * 3.0))
    rgb = np.clip(rgb * (0.88 + 0.30 * wave)[..., None], 0, 1)
    # The eye flares at the cast and settles.
    eye = np.exp(-(rr / 0.09) ** 2) * (0.8 - 0.6 * t)
    rgb = np.clip(rgb + eye[..., None] * np.array([0.55, 0.75, 1.0], dtype=np.float32), 0, 1)
    # Alpha from brightness - the dark navy between the arms lets the floor through - a soft rim, the closing dissolve.
    alpha = np.clip((rgb.max(axis=2) - 0.18) / 0.32, 0, 1)
    alpha *= np.clip((1.0 - rr) / 0.10, 0, 1)
    if f >= N - 5:
        alpha *= 1.0 - (f - (N - 5) + 1) / 6.0
    rgbs.append(rgb)
    masks.append(alpha > bayer)

# One palette for the whole sheet, from every frame's opaque pixels.
opaque = np.concatenate([c[m] for c, m in zip(rgbs, masks)], axis=0)
rng = np.random.default_rng(1)
pick = opaque[rng.choice(len(opaque), size=min(len(opaque), 400_000), replace=False)]
side = int(math.ceil(math.sqrt(len(pick))))
pad = np.repeat(pick[:1], side * side, axis=0)
pad[: len(pick)] = pick
palette = Image.fromarray((pad.reshape(side, side, 3) * 255).astype(np.uint8)).quantize(colors=250, method=Image.Quantize.MEDIANCUT)

out = np.zeros((H, W * N, 4), dtype=np.uint8)
for f, (rgb, m) in enumerate(zip(rgbs, masks)):
    q = np.asarray(Image.fromarray((rgb * 255).astype(np.uint8)).quantize(palette=palette, dither=Image.Dither.NONE).convert("RGB"))
    block = out[:, f * W:(f + 1) * W]
    block[..., :3] = np.where(m[..., None], q, 0)
    block[..., 3] = np.where(m, 255, 0)
print("colours", len({tuple(c) for c in out[out[..., 3] == 255][:, :3]}))
Image.fromarray(out, "RGBA").save(OUT, optimize=True)

if PREVIEW:
    check = ((np.floor(ox / 64 + oy / 32) + np.floor(ox / 64 - oy / 32)) % 2)[..., None]
    floor = np.where(check == 1, np.array([46, 40, 36]), np.array([36, 31, 28])).astype(np.uint8)
    shots = []
    for f in range(N):
        block = out[:, f * W:(f + 1) * W]
        shots.append(Image.fromarray(np.where(block[..., 3:4] == 255, block[..., :3], floor).astype(np.uint8)).resize((W // 2, H // 2), Image.NEAREST))
    grid = Image.new("RGB", (W // 2 * 4, H // 2 * 5))
    for f, im in enumerate(shots):
        grid.paste(im, ((f % 4) * (W // 2), (f // 4) * (H // 2)))
    grid.save(PREVIEW + ".png")
    shots[0].save(PREVIEW + ".gif", save_all=True, append_images=shots[1:], duration=50, loop=0)
print("ok")
