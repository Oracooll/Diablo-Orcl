"""Meteor's fall and impact at twice the frames (dev note, 2026-10-01: "introduce more frames to meteor shower assets. i want
the falling meteor animations to be more fluid. same for the impact rings").

Input: the delivered sheets, kept as sources in "Resources/02. Oracooll Assets/Sorc Skills/Fire Spells/Meteor/":
  meteor.png         10 frames of 96x160, the rock falling;
  meteor_impact.png  14 frames of 160x128, the burst (1-10) and the burning ring the game loops (11-14).
Output, in Packaging/resources/oracool_assets/missiles/: the same files at 20 and 28 frames.
  - The fall: an in-between frame is the earlier frame moved half way to the next along the rock's own path (its centroid),
    so the rock glides rather than cross-fading; the game plays them a tick each (the second the fall always took).
  - The impact: an in-between frame is the two neighbours cross-faded; the ring's four looped frames become eight, and the
    loop's in-between closes the ring back to its first (missiles.cpp MeteorImpactBurnFrame 21).
The engine draws no partial alpha, so a cross-fade's half-covered pixels are an ordered dither.

Usage: python tools/BuildMeteorFrames.py <Meteor source folder> <missiles folder>
Needs Pillow and numpy.
"""
import os
import sys

import numpy as np
from PIL import Image

SRC, OUT = sys.argv[1], sys.argv[2]
BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32) / 16.0 + 1 / 32.0


def frames_of(path, w):
    a = np.asarray(Image.open(path).convert("RGBA")).astype(np.float32)
    return [a[:, w * i:w * (i + 1)] for i in range(a.shape[1] // w)]


def binary(f):
    """Partial alpha to an ordered dither, colours where kept."""
    h, w = f.shape[:2]
    alpha = f[..., 3] / 255.0
    keep = alpha > np.tile(BAYER4, (h // 4 + 1, w // 4 + 1))[:h, :w]
    out = np.zeros(f.shape, np.uint8)
    rgb = np.where(alpha[..., None] > 0, f[..., :3] / np.maximum(alpha[..., None], 1e-6), 0)
    out[..., :3] = np.where(keep[..., None], np.clip(rgb, 0, 255), 0).astype(np.uint8)
    out[..., 3] = np.where(keep, 255, 0)
    return out


def blend(a, b, t=0.5):
    """Premultiplied cross-fade."""
    pa = a.copy()
    pa[..., :3] *= pa[..., 3:] / 255.0
    pb = b.copy()
    pb[..., :3] *= pb[..., 3:] / 255.0
    return pa * (1 - t) + pb * t


def shifted(f, dx, dy):
    """The frame moved by whole pixels (premultiplied, for binary())."""
    p = f.copy()
    p[..., :3] *= p[..., 3:] / 255.0
    out = np.zeros_like(p)
    h, w = p.shape[:2]
    ys, xs = slice(max(dy, 0), min(h, h + dy)), slice(max(dx, 0), min(w, w + dx))
    ys0, xs0 = slice(max(-dy, 0), min(h, h - dy)), slice(max(-dx, 0), min(w, w - dx))
    out[ys, xs] = p[ys0, xs0]
    return out


def centroid(f):
    ys, xs = np.nonzero(f[..., 3] > 0)
    return xs.mean(), ys.mean()


def premul_to_binary(p):
    return binary(p)


def save(frames, name):
    sheet = np.concatenate(frames, 1)
    opaque = sheet[..., 3] == 255
    colours = {tuple(px) for px in sheet[..., :3][opaque]}
    if len(colours) > 250:
        q = np.asarray(Image.fromarray(np.ascontiguousarray(sheet[..., :3])).quantize(245, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert("RGB"))
        sheet = sheet.copy()
        sheet[..., :3] = np.where(opaque[..., None], q, 0)
        colours = {tuple(px) for px in sheet[..., :3][opaque]}
    Image.fromarray(sheet, "RGBA").save(os.path.join(OUT, name), optimize=True)
    print(f"{name}: {len(frames)} frames, {sheet.shape[1]}x{sheet.shape[0]}, {len(colours)} colours")


# The fall: each frame, then itself moved half way toward the next one's place (the last toward where it lands).
fall = frames_of(os.path.join(SRC, "meteor.png"), 96)
out = []
for i, f in enumerate(fall):
    out.append(binary(shifted(f, 0, 0)))
    cx, cy = centroid(f)
    if i + 1 < len(fall):
        nx, ny = centroid(fall[i + 1])
    else:
        px, py = centroid(fall[i - 1])
        nx, ny = 2 * cx - px, 2 * cy - py
    out.append(binary(shifted(f, int(round((nx - cx) / 2)), int(round((ny - cy) / 2)))))
save(out, "meteor.png")

# The impact: the burst's frames with cross-faded in-betweens; the ring's loop closed onto its own first frame.
impact = frames_of(os.path.join(SRC, "meteor_impact.png"), 160)
burst, ring = impact[:10], impact[10:]
out = []
for i, f in enumerate(burst):
    out.append(binary(blend(f, f, 0)))
    nxt = burst[i + 1] if i + 1 < len(burst) else ring[0]
    out.append(binary(blend(f, nxt)))
for i, f in enumerate(ring):
    out.append(binary(blend(f, f, 0)))
    out.append(binary(blend(f, ring[(i + 1) % len(ring)])))
save(out, "meteor_impact.png")
