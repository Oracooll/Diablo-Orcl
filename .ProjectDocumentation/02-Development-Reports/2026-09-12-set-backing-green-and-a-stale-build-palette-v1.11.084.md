# The set backing was orange, and so was a palette nobody had re-staged

2026-09-12 — v1.11.084

## What was asked

> backing of set items to be light green, not orange as it is now.

## The reported bug

`InvDrawSlotBack` set `colorBlock = PAL8_ORANGE` for the Set tier and let the palette supply the
colour. That worked only while the fork injected a green ramp over entries 152-159. The injection
came **out** on 2026-09-10 because it was breaking the dungeon fires, and `engine/palette.h` states
the consequence in as many words:

> the green that was injected over them (2026-08-15) came out again on 2026-09-10 — so on an indexed
> surface this ramp is ORANGE. The 32-bit screen draws the green as values (see the three consumers).

There were **four** consumers. This one was never converted, so set backings had been drawing orange
ever since, with a comment above them still saying "the green this fork injected over PAL8_ORANGE".

Identical in shape to the grey fire-damage text fixed two versions ago: a colour chosen by a **name**
whose meaning moved underneath it, defended by a comment written when the name was still true. Third
instance today, counting the stale `vcvars` path and the stale `ART` root.

The fix draws it as a value — `0x64A064`, the second entry of `ColorOracoolGreen`'s band and the
exact value the runeword border beside it already uses, so the game has one green rather than a
second one invented here. The fill now goes through `FillRectRgb` for every tier: on an indexed
surface nothing changes, and on the 32-bit screen every other tier passes the colour its own index
already resolves to, so it is a no-op for them and the one place a tier can name a colour the
palette cannot give it.

## The bigger find: tools\town.pal was stale

Checking what the old orange actually looked like meant reading `tools\town.pal`, the palette every
CEL build matches against — and its entries 152-159 were **`140,190,140 / 100,160,100 / 62,130,62
/ …`**: the injected green ramp, file dated 16 August.

The real palette, `Resources/00-original-game-art/palettes/levels/towndata/town.pal`, has
`254,190,160 / 255,140,87 / 254,105,36 / …` — orange.

So for a month every icon cut by `build_item_icons.cmd` was palette-matched against a palette **the
running game does not have**. Any source pixel near green matched into the orange minis and drew
orange in play. `tools\town.pal` is gitignored, so it is a local file with no history and nothing to
notice it had gone stale.

Measured, not assumed. Re-staging from the original and re-cutting the sheet:

| | |
|---|---|
| `oracool_items.cel` bytes changed | **21,113** (2.50%) |
| pixels in indices 152-159, before | **9,465** |
| after | **2,085** (the ones genuinely nearest to orange) |

So roughly 7,400 icon pixels had been landing in that ramp because the palette claimed green there.

The other three CELs were checked and are clean — `orclroar.cel`, `orclstash.cel` and
`orclwayp.cel` use indices 152-159 **zero** times, so nothing else needs rebuilding.

### The durable part

Repairing the file locally would have fixed nothing for the next clone, and the staleness is
invisible by construction. `build_item_icons.cmd` now **re-stages `tools\town.pal` from the original
on every run**, and fails loudly with the path named if the original is missing rather than cutting
icons against whatever happens to be there. A copy is cheap; there is no version of this worth
optimising into a test-and-skip.

Four other CEL builders (`build_levski_roar_cel.cmd`, `build_stash_cel.cmd`,
`build_waypoint_cel.cmd`, `build_reliquary_cel.cmd`) read the same path and have the same exposure.
They are not touched here because their outputs are provably clean, but they are the next place this
bites.

## Verification

- **723/723 tests pass.**
- Both archives repacked (466 files) — a normal build does not repack them.
- Debug and Release build clean; RTM refreshed.
- Palette re-staging confirmed by re-reading the file after the build prints its own line.

## What to look for in play

A set item in the inventory, stash or belt should now sit on a green backing rather than an orange
one. `0x64A064` is a medium-light green, deliberately lighter than the deep shades the other tiers
wear; `ColorOracoolGreen`'s band has a lighter `0x8CBE8C` above it if it wants to be paler still,
which is a one-value change.

Item icons are also worth a glance. 2.5% of the sheet moved, and anything whose art had green in it
— the Set Engravings salvage material, the emerald gems — was drawing orange there and now draws its
true nearest colour.
