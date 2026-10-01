"""Absolute Zero sheet from the user's vortex painting (2026-10-01).

20 frames of 1024x512 - the 8-tile hit diamond at 1x - in one row. The vortex grows out of the hero's feet, spins on
the floor plane, carries an outward frost wave and a cyan<->violet drift (the colour cycling), and dissolves at the end.
The engine draws CLX: no partial alpha and at most 255 colours a sheet, so alpha is an ordered dither and the colours
are one shared palette quantised here (the loader then keeps them exactly).
"""
import math
import sys

import numpy as np
from PIL import Image

SRC, OUT, PREVIEW = sys.argv[1], sys.argv[2], sys.argv[3]
W, H, N = 1024, 512, 20

src = np.asarray(Image.open(SRC).convert("RGB")).astype(np.float32) / 255.0
sh, sw, _ = src.shape
lum = src.max(axis=2)

# The eye: the centroid of the brightest pixels in the middle third.
mid = np.zeros_like(lum, dtype=bool)
mid[sh // 4: 3 * sh // 4, sw // 3: 2 * sw // 3] = True
ys, xs = np.where(mid & (lum > 0.97))
cx, cy = float(xs.mean()), float(ys.mean())
# The painted ellipse's half axes from the content's extent around the eye.
ys2, xs2 = np.where(lum > 0.16)
ax = max(cx - xs2.min(), xs2.max() - cx)
ay = max(cy - ys2.min(), ys2.max() - cy)
print(f"eye ({cx:.0f},{cy:.0f}) half axes {ax:.0f} x {ay:.0f}")

# Output grid in circle space: u, v in [-1, 1] across the frame's ellipse (512 x 256 half axes).
oy, ox = np.mgrid[0:H, 0:W].astype(np.float32)
u = (ox + 0.5 - W / 2) / (W / 2)
v = (oy + 0.5 - H / 2) / (H / 2)
r = np.sqrt(u * u + v * v)
ang = np.arctan2(v, u)

BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32)
bayer = (np.tile(BAYER, (H // 4, W // 4)) + 0.5) / 16.0


def sample(img, x, y):
    """Bilinear sample with black outside."""
    x0 = np.floor(x).astype(int)
    y0 = np.floor(y).astype(int)
    fx = (x - x0)[..., None]
    fy = (y - y0)[..., None]
    out = np.zeros(x.shape + (3,), dtype=np.float32)
    for dy, wy in ((0, 1 - fy), (1, fy)):
        for dx, wx in ((0, 1 - fx), (1, fx)):
            xi = x0 + dx
            yi = y0 + dy
            ok = (xi >= 0) & (xi < sw) & (yi >= 0) & (yi < sh)
            px = np.zeros(x.shape + (3,), dtype=np.float32)
            px[ok] = img[yi[ok], xi[ok]]
            out += px * wx * wy
    return out


def ease_out(t):
    return 1 - (1 - t) ** 3


frames_rgb, frames_alpha = [], []
for f in range(N):
    t = f / N
    # Growth: from 30% to full in the first 6 frames, then a slow breath.
    grow = 0.30 + 0.70 * ease_out(min(f / 6.0, 1.0))
    grow *= 1.0 + 0.025 * math.sin(2 * math.pi * t * 2)
    # Spin on the floor plane: a third of a turn over the whole cast, winding inward.
    theta = -t * (2 * math.pi / 3)
    # Back to the painting's circle space, rotated and scaled.
    rr = r / grow
    a = ang - theta
    sx = cx + rr * np.cos(a) * ax
    sy = cy + rr * np.sin(a) * ay
    rgb = sample(src, sx, sy)

    # Colour cycling: an outward frost wave (brightness) and a cyan<->violet drift turning with the arms.
    wave = 0.5 + 0.5 * np.sin(2 * math.pi * (rr * 2.2 - t * 3.0))
    bright = 0.80 + 0.45 * wave
    drift = np.sin(a * 2 + rr * 4 - 2 * math.pi * t * 2)
    cyan = np.array([0.80, 1.10, 1.15], dtype=np.float32)
    violet = np.array([1.05, 0.85, 1.20], dtype=np.float32)
    mixw = (0.5 + 0.5 * drift)[..., None]
    tint = cyan * mixw + violet * (1 - mixw)
    rgb = np.clip(rgb * tint * bright[..., None], 0, 1)
    # The eye flares at the cast and settles.
    eye = np.exp(-(rr / 0.10) ** 2) * (1.0 - 0.6 * t)
    rgb = np.clip(rgb + eye[..., None] * np.array([0.6, 0.8, 1.0], dtype=np.float32), 0, 1)

    # Alpha from brightness: the dark navy between the arms lets the floor through.
    L = rgb.max(axis=2)
    alpha = np.clip((L - 0.20) / 0.35, 0, 1)
    # Soft rim, and the dissolve over the last five frames.
    alpha *= np.clip((1.0 - rr) / 0.12, 0, 1)
    fade = 1.0 if f < N - 5 else 1.0 - (f - (N - 5) + 1) / 5.0
    alpha *= fade
    frames_rgb.append(rgb)
    frames_alpha.append(alpha > bayer)

# One palette for the whole sheet, from the opaque pixels of every frame.
opaque = np.concatenate([fr[m] for fr, m in zip(frames_rgb, frames_alpha)], axis=0)
rng = np.random.default_rng(1)
pick = opaque[rng.choice(len(opaque), size=min(len(opaque), 400_000), replace=False)]
side = int(math.ceil(math.sqrt(len(pick))))
pad = np.zeros((side * side, 3), dtype=np.float32)
pad[: len(pick)] = pick
pad[len(pick):] = pick[0]
pal_img = Image.fromarray((pad.reshape(side, side, 3) * 255).astype(np.uint8)).quantize(colors=250, method=Image.Quantize.MEDIANCUT)

sheet = np.zeros((H, W * N, 4), dtype=np.uint8)
for f, (rgb, m) in enumerate(zip(frames_rgb, frames_alpha)):
    q = Image.fromarray((rgb * 255).astype(np.uint8)).quantize(palette=pal_img, dither=Image.Dither.NONE).convert("RGB")
    block = np.zeros((H, W, 4), dtype=np.uint8)
    block[..., :3] = np.asarray(q)
    block[..., 3] = np.where(m, 255, 0)
    block[~m, :3] = 0
    sheet[:, f * W:(f + 1) * W] = block

colours = {tuple(c) for c in sheet[sheet[..., 3] == 255][:, :3]}
print("colours", len(colours))
Image.fromarray(sheet, "RGBA").save(OUT, optimize=True)

# A preview: frames over a dark dungeon-ish floor, 4 per row at half size, and an animated GIF.
floor = np.zeros((H, W, 3), dtype=np.uint8)
check = ((np.floor((ox / 64) + (oy / 32)) + np.floor((ox / 64) - (oy / 32))) % 2).astype(np.uint8)
floor[...] = np.where(check[..., None] == 1, [46, 40, 36], [36, 31, 28])
gif = []
for f in range(N):
    block = sheet[:, f * W:(f + 1) * W]
    comp = np.where(block[..., 3:4] == 255, block[..., :3], floor)
    gif.append(Image.fromarray(comp.astype(np.uint8)).resize((W // 2, H // 2), Image.NEAREST))
grid = Image.new("RGB", (W // 2 * 4, H // 2 * 5))
for f, im in enumerate(gif):
    grid.paste(im, ((f % 4) * (W // 2), (f // 4) * (H // 2)))
grid.save(PREVIEW + ".png")
gif[0].save(PREVIEW + ".gif", save_all=True, append_images=gif[1:], duration=50, loop=0)
print("ok")
