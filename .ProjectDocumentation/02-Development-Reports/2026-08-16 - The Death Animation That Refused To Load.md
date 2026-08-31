---
date: 2026-08-16
version: 1.7.21
area: Crash on death; three unguarded sprite derefs; rare item backing
---

# The Death Animation That Refused To Load

A user crash on death: `assertion failed ... value_.data_ != nullptr`, `clx_sprite.hpp:599`. Line
599 is the intrusive-optional assert on **`OptionalClxSpriteList`** — which narrows it to
`AnimationInfo::currentSprite()`, someone drawing an entity whose animation holds no sprites.

## The cause

`LoadPlrGFX` had this:

```cpp
case player_graphic::Death:
    if (animWeaponId != PlayerWeaponGraphic::Unarmed)
        return;          // loads nothing, says nothing
    szCel = "dt";
```

The death animation is only authored unarmed, and all **four** places that start one clear the
weapon nibble first — `StartPlayerKill`, `InitPlayer`, and the two multiplayer sync paths. So the
refusal looked like a harmless assertion about callers that always held.

It is not the only caller. `CalcPlrItemVals` rebuilds the *current* animation whenever `_pgfxnum`
changes:

```cpp
player_graphic graphic = player.getGraphic();   // PM_DEATH  ->  Death
LoadPlrGFX(player, graphic);                    // refuses: a sword is still in hand
sprites = player.AnimationData[Death].spritesForDirection(dir);   // empty
player.AnimInfo.changeAnimationData(sprites, ...);                // binds nothing
```

`StartPlayerKill` clears the weapon nibble; the very next equipment recalculation puts it back,
because it recomputes `_pgfxnum` from what the corpse is still wearing. Death animation requested,
refused, bound empty, and the next frame dereferenced it.

Fixed the way the Shield Bash block-sheet crash was fixed at v1.6.24 — **at the single authority,
by asking for the sheet that exists** rather than making every caller remember to. Nothing else
reads the weapon for this graphic: the frame count comes from `_pDFrames`, and
`GetPlayerSpriteWidth` returns `spriteData.death` whatever the weapon is.

## Three derefs with no guard

The crash was only fatal because nothing between the empty bind and the screen checked. Three sites
dereferenced a sprite list that `LoadPlrGFX` can legitimately leave empty — it still declines three
graphics outright (Attack and Hit in town, Block without the block flag):

- `NewPlrAnim` — `(*sprites)[0]`, in the preview-sprite comparison. Only reached right after a
  click, which fits "it happened when I died" exactly.
- `DrawPlayer` — `player.AnimInfo.currentSprite()`.
- The monster draw caller. This one is quietly funny: `DrawMonster` **already** logs and returns on
  an empty sprite list, but it never got the chance, because the caller reads `currentSprite()`
  about fifty lines earlier. That check has been dead code the whole time.

All three now skip the frame and log. A missing sheet should cost a drawn entity, not the session.

## A use-after-free of my own, from v1.7.20

`ClearMonsterScaleCache` dropped the scaled sprite data while sized monsters were still bound to it,
leaving a dangling pointer rather than an empty optional — the one failure no null check anywhere
could have caught. It now unbinds sized monsters **before** freeing, so the worst case is a monster
that draws nothing until it rebinds, which the guards above handle.

## Rare items were gold, not yellow

The user: *"make rare items backing yellow. now is gold."* Correct, and `engine/palette.h` had
already written down why — it says in as many words that "the pale `PAL16_YELLOW` substitutes read
as unique-gold". Rare was using exactly that ramp, and so was Buffed Unique, so the two tiers were
also indistinguishable from each other.

Rare now rides `PAL8_YELLOW`, the saturated mini-ramp the rare **name** colour comes from, so the
backing and the name finally agree. Buffed Unique keeps `PAL16_YELLOW`, which is the unique-gold it
was always meant to be. The mini-ramps are eight shades where the PAL16 ramps are sixteen, so the
offset is halved — at the full offset it would have run off the end into `PAL8_ORANGE`, which this
fork overwrote with the injected green ramp.

## Verified

**412 tests, the same two pre-existing failures.**

No test covers the death fix, and adding one would have been theatre: the suite runs with
`HeadlessMode = true`, under which `LoadPlrGFX` returns at its first line, so any such test could
only ever skip. This one is verified by playing and dying.
