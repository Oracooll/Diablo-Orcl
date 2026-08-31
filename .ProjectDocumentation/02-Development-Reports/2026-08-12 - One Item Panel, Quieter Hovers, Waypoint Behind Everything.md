---
title: 2026-08-12 - One Item Panel, Quieter Hovers, Waypoint Behind Everything
date: 2026-08-12
tags: [dev-report]
summary: Three play-test findings. The waypoint moves to a new floor draw pass so it stops painting over the player; monsters stop populating the cursor tooltip; and the fixed "item stats" box merges with the tooltip into one cursor-following panel sized to its contents.
---

# One Item Panel, Quieter Hovers, Waypoint Behind Everything

Three things the user found play-testing 1.1.3.

## 1. The waypoint drew on top of the player

`_oPreFlag` looked like the answer and was not. It only orders an object against **its own tile's** contents - the renderer runs it once before that tile's characters and once after. The waypoint's new sprite is 144x106, roughly three tile-rows tall, so anything standing on a tile *behind* it - drawn earlier in the back-to-front sweep - was painted over by something that visually belongs on the floor. This is the ordinary fate of any object drawn much larger than its tile; the placeholder magic circle at 79x38 was small enough never to hit it.

Three ways out were available: shrink the art, give the object a multi-tile footprint so the engine's own large-object path orders it, or move it out of the per-tile loop entirely. The third is both the smallest change and the honest one - it *is* a floor platform.

`DrawObject`'s `bool pre` becomes an `ObjectDrawPass` enum with three values, and `IsFloorPassObject` routes `OBJ_WAYPOINT` to the new `Floor` pass, which runs inside `DrawFloor` - so the entire viewport's floor sweep, including the waypoint, completes before `DrawTileContent` draws a single character. The cost, accepted deliberately: the platform's pillars also pass under anything standing in front of them, which is how floor decals behave everywhere else.

`_oPreFlag` is left set on the object. Nothing else reads it, and leaving it matches the magic circles this object was modelled on.

## 2. Monsters no longer feed the cursor tooltip

Hovering a monster put its name, type and kill count under the cursor - while the health bar across the top of the screen was already showing the same name. Two readouts for one hover, one of them following the mouse around mid-fight.

The dungeon-monster branch of `UpdateInfoString` is simply gone. `PrintMonstHistory` and `PrintUniqueHistory` are deliberately not called and their results discarded - they are the only source of the "Kills:" line, so not calling them is the fix. Town NPCs keep their branch: no health bar names them, and their names are useful.

Both functions are now unreachable. Left in place rather than deleted, as a separate cleanup.

## 3. One item panel instead of a tooltip and a box

Item information was appearing in two places at once: the cursor tooltip carried the name and stat lines as outlined floating text, while a separate fixed box - vanilla's `data\textbox2` frame, pinned beside the inventory - carried the affix powers for unique, magic and Oracool-tiered items. Two locations, one of them nowhere near the cursor, and the fixed box wasted most of its 271x330 frame on whitespace because its layout assumed ten lines whatever the item had.

They are now one panel, and it is the tooltip that survived.

**Content.** The affix lines moved into the same panel-string list every other line of item detail already goes into, via a new `AddItemPowerPanelStrings`. Magic items are absent from it on purpose: `PrintItemDetails` was already printing their prefix and suffix powers, and the old box was showing them a second time in a different place - so folding the box in removes a duplicate rather than creating one.

**Rendering.** `DrawCursorTooltip` gains a panel mode: when the hover is an item, it pads the text, darkens the area behind it with two half-transparent passes (one leaves the floor tiles reading straight through the stat lines) and draws a 2px muted-gold border - palette index 194, the same index the engine outlines hovered objects with, and a match for the HUD's bronze trim. Non-item hovers keep the outlined-text-no-plate look. The box is sized to the measured text either way, so it is only ever as large as its contents.

**Deciding it is an item.** Derived from the existing hover globals - `pcursitem`, `pcursinvitem`, `pcursstashitem`, `ActiveTabItemHovered` - rather than from a new flag set by the hover code. All four are already cleared and repopulated once per frame by the cursor pass, so the state cannot go stale, and nothing new has to be threaded through `inv.cpp`, `stash.cpp` and `control.cpp` for something all three already record. `pcursinvitem` covers the inventory grid, the equipment slots and the belt; `ActiveTabItemHovered` covers the extra tabs, whose items have no `pcursinvitem` encoding at all.

A held item is deliberately excluded - it is one line, and a plate trailing a dragged item would be in the way.

**Removed:** `DrawUniqueInfo`, `DrawUniqueInfoWindow`, `ShowUniqueItemInfoBox`, `curruitem`, and the draw call in `DrawAndBlit`. `pSTextBoxCels` stays - the stores still use it for their own box.

### A latent clamp bug the panel would have reached

The tooltip clamped its origin with `std::clamp(origin.x, 0, gnScreenWidth - width)`. That upper bound goes negative for a box wider than the screen, and `std::clamp` is undefined when `hi < lo`. A one-line tooltip could never get there; a padded, bordered, multi-line item panel plausibly could. Both bounds are now floored at 0, and the unclipped `UnsafeDrawBorder2px` only runs when the box actually fits.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.5** (1.1.4 was the first pass; the clamp fix took it to 1.1.5), version string confirmed in the exe. Tests **347/349** - the same two pre-existing failures as the last two reports (`Drlg_l1` stairs, `Timedemo`'s pre-save-break demo character), no new ones.

Needs a play-test on all three: that the waypoint now passes behind the player and behind monsters standing anywhere near it; that hovering a monster is silent apart from the health bar; and that the item panel reads well at both extremes - a plain unidentified sword and a fully-affixed tiered item - in the inventory, the stash, and on the dungeon floor.

## Related

- [[2026-08-11 - Waypoint Sprite and the First CEL Encoder]]
- [[2026-08-11 - Level Up Icon Under the Clock]]
