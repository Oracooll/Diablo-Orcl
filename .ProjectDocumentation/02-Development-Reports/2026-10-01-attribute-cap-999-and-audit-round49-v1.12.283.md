# 2026-10-01 - Base attributes to 999, and audit round 49 (v1.12.283)

**Date:** 2026-10-01. Debug only. 898 tests pass; the Debug `diablo.ini` was unchanged by ctest. v1.12.282 was an intermediate build of the cap lift alone (not committed separately).

## The 255 cap lifted (user, 2026-10-01)
The user approved the redesign's steps 1-4 with a three-digit ceiling: 999.

- **One ceiling:** `MaxBaseAttribute = 999` (player.h). It replaces every literal 255 for base attributes:
  - `ModifyPlrStr/Mag/Dex/Vit`, `CheckStats`, `CalcStatDiff`, `StatPointsToSpend`;
  - the per-tick clamp in player.cpp;
  - the + buttons (mouse, keyboard and touch);
  - the sheet's gold colour at the cap;
  - the elixirs;
  - the debug `maxstats` command.
- **Hero file:** `PlayerPack` is untouched. Its bytes now clamp at 255 rather than wrapping. The full values ride in a new chunk, `HeroChunkBaseAttributes` (tag 20: four u16), applied after unpacking and clamped to the ceiling. Older heroes load as before.
- **Amulet and ring conversions:** `_iPLHP` and `_iPLMana` are widened to int32. Two bytes wrapped past about 1023 base mana or 1279 base life.
- **Unchanged:** item requirements stay one byte. Multiplayer message paths are untouched (V1 is single-player).
- **Tests:**
  - `BaseAttributesPast255RoundTrip`;
  - `StatPointsToSpend` now checks 255 is not a cap and 999 is;
  - the writehero golden is re-baselined with a dated note.
- **Rendered:** the hero sheet preview now also draws every base at 999 with totals of 1149/999/1024/1200 (`hero_sheet_999.png`). Four digits fit the boxes.

**Open for the user:** Strength as a D2-style percentage bonus (explained in chat). With the flat Diablo I formula, 999 Strength at level 99 adds +989 damage to every blow, or +1318 for a Barbarian with an axe.

## Audit round 49

### Regression review of v1.12.281
- **Bone Storm:**
  - only the local hero's storm is carried across a level change;
  - it ends on arrival in town;
  - a respawn clears it.
- **Ride the Lightning** strikes the line it actually flew.
- **A doc comment** is back on `FireGolemsBurn`.

### Monster AI (4th pass)
- **PlaceGroup:**
  - no longer wraps its count when the floor already stands past `totalmonsters`, as a rift guardian's pack placed mid-game can;
  - places and undoes through `ActiveMonsters` slots, so it never overwrites a live monster.
- **`GetUniqueMonstPosition`** is bounded at 5000 tries. A full floor drops the champion instead of hanging.
- **Monster-vs-monster blows** can no longer be negative. A Hollow magma demon's second blow had healed its target and still made it flinch.
- **Repels:**
  - `StartRepelRetreat` replaces four hand-written repel sites;
  - bats and sneaks now take the repel's steps. Howl at rank 5 and up had been cancelled on the spot, and bats only sidestepped once.

### Texts against code (3rd pass)
- **Stated caps and growth** added to: Inner Sight, Slow Missiles, Multiple Shot, Strafe, Seven Reeds, Hundred Fists, Battle Cry, War Cry, Find Potion, Find Item and Cleansing.
- **Power Strike:** "1 to 4 lightning damage a rank".
- **Steal powers:** mana and life steal say "per melee hit".
- **Advanced Stats:** magic find past 75 shows "(75% counts)".
- **Charm of Legend:** its growth line says what it does (Rare chance). Fixed in the generator too.
- **`IPL_ONEHAND`:** prints "one-handed", not "one handed sword".

### Not changed
- Cadence's bonus now reaches no weapon burst. That is a design call, and it is consistent with "the swing's own".
- `encounter_charms.inc` is unused.
- Some package-mapping notes still call movement speed inert.
