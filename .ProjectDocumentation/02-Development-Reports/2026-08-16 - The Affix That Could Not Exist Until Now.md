---
date: 2026-08-16
version: 1.7.20
area: Megaplan Phase 3.2 - the Colossal champion, and monsters that are really a different size
---

# The Affix That Could Not Exist Until Now

When the lesser-unique system shipped, the user asked for champions "shrunken in size". The design
document answered with palette and naming, and said why: *there is no scale parameter anywhere in
the CLX renderer.* Shortly after, the same wall killed a second idea — an affix called "Fleet" —
because monster movement is paced by the animation, and animation data lives on the shared
`CMonster` rather than on the individual creature.

Phase 0.6 built `oracool/sprite_scale` and left it, in its own words, "a tool with tests". This is
the wiring it was built for, and it makes size the **second** property that can escape the shared
type.

## Why it can escape, when speed cannot

`CMonster::anims` is shared by every monster of a type — that fact is what makes a per-monster
walk speed impossible without making every skeleton on the floor fast. Size gets out through a
different door: the individual monster's `animInfo` binds a sprite **list**, and nothing requires
that list to be the shared one. Give the monster a scaled list and every consumer downstream —
the renderer, the outline pass, the hit-test — sees honest dimensions with no further changes.

There turned out to be exactly **two** places a monster binds sprites: `NewMonsterAnim` in
monster.cpp, and `Monster::changeAnimationData` in monster.h. Both had to be caught. Missing the
second would have meant a Colossal champion that snapped back to normal size the first time it
merely turned to face the player.

## The cache is keyed by (type, size), not by monster

Giving each champion its own copy would be correct and wasteful — six animations of eight
directions apiece, per creature. Keying on the pair means every Colossal skeleton on a floor shares
one scaled sheet, so a floor pays one allocation per distinct size it actually uses. The scaled data
is owned by the cache and views sprites the shared blob owns, so `ClearMonsterScaleCache` is called
from both `FreeMonsters` and `InitLevelMonsters`.

**The bug I nearly cached in.** The first version built all six animations at once, on first ask.
A monster binds its Stand animation during *placement*, which can precede the load of the sprites
another animation needs — so the eager version would have stored an empty entry for whatever was
not ready, and the champion would have gone invisible the moment it attacked. It is now per-
animation and lazy, with no "already tried" flag anywhere: an unloaded source stays retryable rather
than caching a miss forever. Same reasoning applied to the corpse.

## The corpse

A Colossal champion that shrank the instant it died would be a worse bug than not scaling it at all,
and corpses are drawn from a global table keyed by `corpseId` — shared, like the animations, and
consulted long after the monster stopped being drawn. The corpse already carried a back-reference to
its monster, `translationPaletteIndex`, because it needs it for the champion's palette; the scaled
sprites now come through the same reference.

While there, the corpse's horizontal centring moved from the table's `width` to the drawn sprite's
own `width()` — which is what the live monster path had always used, and is the same number for
everything that is not scaled.

## What Colossal is

140%. Past roughly 160% the baked-in shadow separates from the feet; below about 130% it reads as
"the same monster, closer" rather than a bigger one.

The stat half is **half again as much life**, applied with the hit points topped back up — because
`ApplyLesserUniqueAffix` runs after hit points are already set, and raising only the maximum would
have spawned every Colossal champion visibly wounded. Life rather than damage on purpose: it should
take longer to bring down, not delete a player who misjudged its reach.

## Verified

**412 tests, the same two pre-existing failures.** Two new ones, pinning the properties most likely
to rot:

- An ordinary monster returns `nullptr` from the scale path immediately. `GetScaledAnim` runs from
  `changeAnimationData`, which fires whenever *any* monster turns — anything but an early exit there
  is a cost every monster in the game pays, every frame.
- Every affix has a name. The display name is affix + space + generated name, so an enum value that
  falls through the switch leaves a champion called " Malgrith the Unclean".

`LesserUniqueAffix` is persisted per monster, so `Colossal` was **appended**, never inserted. No
existing save can hold the new value, so there is nothing to migrate.

## To see one

Colossal is now one of six modifiers a lesser unique can roll, so it appears at the same rate as
Warded or Thunderous. The lesser-unique rate is the INI option added in v1.6.0 if you want them
thicker on the ground while judging how it reads.
