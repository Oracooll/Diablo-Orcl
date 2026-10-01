"""The Sorcerer's fire redesign sheets (user, 2026-10-01).

Input: "Resources/02. Oracooll Assets/Sorc Skills/Fire Spells/" and the game's own ui/aura_holy_fire.png.
Output, in Packaging/resources/oracool_assets/missiles/:
  ember_mine.png     the user's mine A: 8 frames of 128x64 on a green screen, keyed, the stone locked to one colour
                     across frames (its per-frame palettes made it shimmer), at half size (the user's 50% pick): 8 x 64x32;
  furnace_mouth.png  the user's furnace, 8 rows (S, SW, W, NW, N, NE, E, SE - the engine's order), one 448x320 frame each,
                     the floor at (224, 160);
  furnace_flame.png  its flame, the same rows and anchor, three tiles along each facing;
  ashen_ring.png     aura_holy_fire at 448x224 - the 3 tiles round Ashen Brand's cursor - its soft glow an ordered dither
                     (the engine draws no partial alpha; the game fades it in and out with the missile's alpha).

Usage: python tools/BuildFireSheets.py <Fire Spells folder> <oracool_assets folder>
Needs Pillow and numpy.
"""
import os
import sys

import numpy as np
from PIL import Image

SRC, ASSETS = sys.argv[1], sys.argv[2]
MISSILES = os.path.join(ASSETS, "missiles")
BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32) / 16.0 + 1 / 32.0


def key(rgb):
    """The green screen out and its fringe despilled: RGBA."""
    a = rgb.astype(int)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    greenness = g - np.maximum(r, b)
    vis = greenness <= 60
    a[..., 1] = np.where(greenness > 0, np.maximum(r, b), g)
    out = np.zeros(a.shape[:2] + (4,), np.uint8)
    out[..., :3] = np.clip(a, 0, 255)
    out[..., 3] = np.where(vis, 255, 0)
    out[~vis, :3] = 0
    return out


def save(rgba, name, limit=255):
    path = os.path.join(MISSILES, name)
    opaque = rgba[..., 3] >= 128
    colours = {tuple(p) for p in rgba[..., :3][opaque]}
    if len(colours) > limit:
        q = Image.fromarray(np.ascontiguousarray(rgba[..., :3])).quantize(limit - 5, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert("RGB")
        rgba = rgba.copy()
        rgba[..., :3] = np.where(opaque[..., None], np.asarray(q), 0)
        colours = {tuple(p) for p in rgba[..., :3][opaque]}
    rgba[..., 3] = np.where(opaque, 255, 0)
    Image.fromarray(rgba, "RGBA").save(path, optimize=True)
    print(f"{name}: {rgba.shape[1]}x{rgba.shape[0]}, {len(colours)} colours")


# Ember Mine: keyed, locked, halved.
frames = [key(np.asarray(Image.open(os.path.join(SRC, "Ember Mine", "A Idle 8", f"ember_mine_a_idle_{i:02d}.png")).convert("RGB"))) for i in range(1, 9)]
F = np.stack(frames).astype(float)
vis = F[..., 3] > 0
rgb = np.where(vis[..., None], F[..., :3], np.nan)
with np.errstate(all="ignore"):
    med = np.nanmedian(rgb, 0)
    spread = np.nanmax(rgb, 0) - np.nanmin(rgb, 0)
stable = (vis.sum(0) >= len(frames) - 1) & (np.nan_to_num(spread, nan=999).max(-1) <= 40)
half = []
for f in F:
    f[stable, :3] = np.round(med[stable])
    f[stable, 3] = 255
    # Pillow resizes RGBA premultiplied on its own, so the keyed black never bleeds into the edge (round 66 audit: doing it
    # here as well brightened the edge pixels twice).
    small = Image.fromarray(np.clip(f, 0, 255).astype(np.uint8), "RGBA").resize((64, 32), Image.LANCZOS)
    half.append(np.asarray(small).copy())
print("ember mine: locked", int(stable.sum()), "pixels")
save(np.concatenate(half, 1), "ember_mine.png")

# Furnace Mouth: the furnace and the flame, rows as delivered.
base = os.path.join(SRC, "Furnace Mouth", "Final Assets")
save(key(np.asarray(Image.open(os.path.join(base, "furnace_mouth_B_8row_448x2560.png")).convert("RGB"))), "furnace_mouth.png")
save(key(np.asarray(Image.open(os.path.join(base, "dragon_breath_B_8row_448x2560.png")).convert("RGB"))), "furnace_flame.png")

# Ashen Brand's ring: the holy-fire aura at the curse's 3 tiles, its glow dithered.
ring = np.asarray(Image.open(os.path.join(ASSETS, "ui", "aura_holy_fire.png")).convert("RGBA").resize((448, 224), Image.LANCZOS)).astype(float)
alpha = ring[..., 3] / 255.0
threshold = np.tile(BAYER4, (224 // 4, 448 // 4))
keep = alpha > threshold * 0.9
out = np.zeros(ring.shape, np.uint8)
out[..., :3] = np.where(keep[..., None], np.clip(ring[..., :3], 0, 255), 0).astype(np.uint8)
out[..., 3] = np.where(keep, 255, 0)
save(out, "ashen_ring.png")
