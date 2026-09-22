# Socketed is a kind, and the chest is gilded — v1.12.159

2026-09-22

Two requests, and both turned out to be about a rule living in the wrong place.

## "Punching sockets should make it a socketed item"

> when i punch socket/sockets on basic item it should become socketed item - proper font color
> and all specs such item comes with.

**The item data was already right.** `PunchSocketsRecipe` sets `_iSocketCount`, writes
`Item::EmptySocket` into each new socket and calls `normalizeSockets()` — which is strictly more
than the natural drop roller (`TryAddSocketsToDroppedItem`) does, and that one only sets the
count. The `Sockets: x/y` row in the description and the dark-grey inventory backing both key off
`_iSocketCount` and were already appearing.

**The colour was the gap, and it was never about punching.** The rule "a plain socketed item
reads GR-5" was written into `itemlabels.cpp` — into the *floor*, not into the item. So a
socketed base read grey on the ground and turned **white** the moment it was picked up: in the
inventory, on the cursor, in the stash, and at the top of its own description.

Punching just made it visible, because that is the one moment an item changes while the player is
watching it. A socketed **drop** had exactly the same two colours.

The rule now lives in `Item::getTextColor()`, beside the other kinds, and the ground label asks
for it. Ethereal still supersedes it (an ethereal plain item is GR-7) and quality still wins over
both — a magic or tiered item never reaches that block, which is what keeps the colour telling the
qualities apart. The label keeps only what is genuinely its own: the `[N]` suffix.

`itemlabels.cpp` had also kept a *copy* of the conditions to decide when to override
`getTextColor()`. That is the other half of the same bug — two places deciding one thing — and it
is gone.

## "Recolor the stash chest to look more goldish"

> recolor stash chest to look more goldish, not that tomb grey.

### The wrong road, briefly

I started building a TRN: extracted `levels\towndata\town.pal`, intending to remap every index
onto the nearest gold entry in the palette. The user stopped it — *"we dont use palettes anymore.
you keep forgeting that."* Correct: the screen has been 32-bit since v1.11, and a recolour is
colour **values**. A palette remap would also have landed most of the sarcophagus on whatever
yellows the town palette happens to carry.

### The right one

`oracool::SpriteColours` is the mod's own answer to exactly this — it exists because hero sheets
needed it — and it brings the lighting with it. An index given a colour of its own is darkened
arithmetically by as much as the light table darkens its fallback, so the chest falls into shadow
at the pace the stone beside it does. `DrawSpriteWithColours` is the lit path with a palette of
its own, not a second way of drawing an object, and on an indexed target (the golden tests,
offscreen work) it falls back to the plain index draw.

Every index is given its own colour and keeps **itself** as the fallback, so an indexed target
draws the chest exactly as it always did. The table is built from whatever palette is live and
rebuilt when `PaletteRgbGeneration` moves.

Only the sarcophagus is gilded. A build without the private archive puts vanilla `chest3.cel`
here instead, and gilding the vanilla chest would be a surprise rather than the thing that was
asked for.

### The cast was measured, and the first one was wrong

Gold is (255, 200, 80) — luminance 203, hence the three ratios — applied luminance-preserving and
in proportion to how grey the source is.

The first attempt gated it at `saturation * 3`: full cast below a third saturation, nothing above
it. **The sarcophagus is not grey at all.** Decoding its five frames out of `orclstash.cel` and
rendering them against the town palette showed blue-purple stone, so that gate let the gold reach
the lit edges and left the body exactly the colour the user was complaining about.

A straight taper — `mix = 255 - saturation` — casts a grey whole, leaves a pure hue alone, and
gives everything between the share of gold it is not already coloured.

## The link failure in between

v1.12.157 did not link. `StashChestColoursFor` was defined beside `ApplyStashChestGraphics`, where
it reads as if it belongs — but that spot is inside `namespace oracool`, which opens two hundred
lines above it and closes eight hundred below. So the definition was
`devilution::oracool::StashChestColoursFor` while `objects.h` declares
`devilution::StashChestColoursFor`.

Third time this session a definition has landed in a namespace it looked like it was outside of
(v1.12.130, v1.12.140). The rule, now written at the site: check where the block **closes**, not
where the neighbouring function appears to sit.

## Files

- `Source/items.h` — the socketed colour, in `getTextColor`
- `Source/qol/itemlabels.cpp` — the duplicate conditions removed
- `Source/objects.cpp` / `objects.h` — `GoldCast`, `StashChestColours`, `StashChestColoursFor`
- `Source/engine/render/scrollrt.cpp` — one branch in `DrawObject`

Built clean, 832/832. No asset changes.

## What to look at in play

1. Punch sockets into a basic sword. Its name goes grey in the inventory, on the cursor and at the
   top of its description — and stays grey when dropped.
2. A socketed item that *drops* should now read the same grey in the inventory as it does on the
   floor. It did not before.
3. The stash chest in town, lid closed and lid open. Gold, and darkening with the light like the
   stone around it.
