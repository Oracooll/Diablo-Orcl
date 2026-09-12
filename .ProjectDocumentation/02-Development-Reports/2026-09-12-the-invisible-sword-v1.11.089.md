# The invisible sword: a debris sweep that deleted the subject

2026-09-12 — v1.11.089

## How it was found

The fourth asset sweep of the day, run under the standing loop. The first three passes had all
checked that frame counts matched, that paths resolved, and that files existed — and this bug passes
every one of those. It had to be found by decoding the CEL and looking at the pixels.

**Knellbranch**, a level-33 sabre unique, had a completely transparent inventory icon. Frame 313 of
`oracool_items.cel` was 84 bytes: eighty-four scanlines of "28 transparent pixels" and not one
opaque pixel. The only blank frame in all 626, and the smallest by a factor of two.

## Why nothing caught it

Because the frame was *present and correctly shaped*. 28 × 84, right index, right position in the
sheet. Every frame count, every table length, every `static_assert` in `cursor.cpp` passed, and the
generator had nothing to reject. The only symptom available was an invisible sword in the inventory,
which nobody had happened to pick up.

## Three wrong theories, in order

Worth recording because each was plausible and each was disproved by measurement rather than by
argument:

1. **"The source art is missing or blank."** No — the 28 px cell has 183 pixels with some alpha, and
   rendering it shows a recognisable sabre. The master is a clean 128 × 384 painting of a thin
   elegant blade.
2. **"The master is degenerate — only 2.5% solid."** No, and this one I said out loud before
   checking properly. 2.5% is simply what a thin sword *is* on a 128 × 384 canvas. Rendering the
   master beside a healthy sibling settled it in one look.
3. **"This sprite has non-binary alpha and the others don't."** No — I scanned all 250 and **every
   one** has non-binary alpha. Not the discriminator.

What *is* unique about Knellbranch, from scanning all 250: it is the only sprite whose solid
coverage (alpha ≥ 128) is under 3% of its cell. 59 pixels of 2352.

## The actual cause: `PostProcess` pass 3

`tools/ItemIconCel.cs` runs an island sweep at final resolution that erases any connected component
smaller than `MinCellIsland = 90`, to remove the specks green-key extraction leaves behind. After
pass 2's `KeepAlpha = 56` cull, Knellbranch's blade is about 75 pixels — **one island, under the
threshold — so the sweep deleted the entire icon.**

Two defects, and the first one hid the second:

- **The sweep was 4-connected.** A 1 px *diagonal* line has no 4-connectivity at all, so every pixel
  of that blade was an island of size 1. Knellbranch is the only art in the pack that is almost
  entirely one diagonal stroke.
- **The loop bound was `d < 4`** against a 4-element neighbour array. So when I widened the arrays to
  8 neighbours as the first fix, *nothing changed* — zero bytes of the CEL moved, and the frame was
  still blank. That null result is what sent me back to look properly instead of declaring victory.

## The fix: the largest island is never debris

Both defects are repaired — `d < dx.Length` with the diagonals included — but the change that
actually matters is the rule, not the connectivity:

```
if (i == biggest || islands[i].Count >= MinCellIsland) continue;  // never erase the subject
```

The sweep now collects every island first and **unconditionally keeps the largest**, whatever
`MinCellIsland` says.

`MinCellIsland` was never wrong. It was measured against real debris and real content (57 px versus
171 px, per its own comment) and it is right for its purpose. It was being asked a question it
cannot answer — *"is this the icon?"* — when all it can answer is *"is this smaller than debris gets?"*.
Keeping the biggest component separates the two, and costs nothing on an icon whose subject is
comfortably over the threshold. A debris filter that can return an empty image is not a debris
filter.

## Verification

- `icon_knellbranch.png` preview: 0 opaque pixels before, **89 after**. Composited at 4× beside four
  neighbouring uniques and looked at: a thin blade, blue-wrapped grip, visible crossguard, and the
  neighbours unchanged and fully detailed.
- `oracool_items.cel` grew 839,843 → **840,579 bytes**, +736. One frame's worth — the fix is
  surgical, not a global re-cut.
- Both archives repacked; **724/724 tests pass**; Debug and Release clean; RTM refreshed with exe and
  archive.
- All 250 unique sources scanned for the same signature: Knellbranch is the only one under 3% solid
  coverage, so no sibling is silently eroded.

## The rest of the sweep, not addressed here

- `missiles/ice_ground.png` — shipped, 256 × 128, with no `MissileSpriteData` row. Acknowledged in
  `misdat.h:117`: "the ground patch has no spell to leave it." An asset waiting for a feature, not a
  gap.
- `Packaging/resources/assets/data/inv/oracool_items.cel` is absent from `CMake/Assets.cmake`'s
  list, so the second channel's copy is unmanaged. Harmless today — the file reaches the game through
  `oracool.mpq` — but it is load-bearing for nothing and should either be listed or dropped.
- `levski_roar.cpp:951` is unreachable: the `continue` at :937 returns for `Transmute` before the
  loop reaches it, so TRANSMUTE never wears the "this would do nothing" shade the eight salvage cells
  get, and `FirstReadyLevskiRecipe`'s result is consumed only by dead code.
- Five stale comments that misdescribe live assets (`tab_glyphs` frame size, `waypoint_icons` cell
  size, `orclstash.cel` frame count, the `ui_backgrounds` master size, "nine slots" for a 12-slot
  grid). Documentation only, but they will misdirect the next sweep, which now runs on a loop.
