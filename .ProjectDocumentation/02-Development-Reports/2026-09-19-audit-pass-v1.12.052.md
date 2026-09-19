# Audit pass on the day's work (v1.12.052)

**Date:** 2026-09-19 - Debug only - **819 of 819 tests** (one run, build 21). User: "Run a few audits while i am away." Three
read-only audits (backings v1.12.048-051, debug spawners v1.12.049, monster variants + venom v1.12.047), every
finding re-read in the code before the fix. Nothing seen in play.

## Backings (inv.cpp, primitive_render.cpp)

| # | Finding | Fix |
|---|---|---|
| 1 | `InvDrawSlotBack` ran vanilla's `Surface::Clip`, which takes a TOP-left and only trims a source rect nothing reads - a footprint over the left screen edge would shift right instead of being cut (latent: no layout puts a panel there) | Clip removed; the footprint is built first and refused if empty or wholly off-screen; both drawing helpers clamp themselves |
| 2 | 8-bit fallback of `TintRectRgb` shifted grey into a PAL8 eight-shade ramp (or the grey ramp itself) past the ramp's end - test surfaces only, the screen is RGB | the shade is clamped to the ramp's length (16 from PAL16_BEIGE up, 8 below) |
| 3 | the swatch cache never reset after `FreeInvGFX`, and had no bounds check against a smaller replacement `inv.cel` | reset in `FreeInvGFX`; every origin + 24 checked against the panel size, else no underlay |
| 4 | per-pixel hash + rotation in the underlay: ~130k px x 2 passes with backpack and stash open, a few ms a frame | the 32 oriented tiles are baked once at `EnsureSlotStoneTiles`; the hash runs once per 24 px tile and rows are copied |

## Debug spawners (items.cpp)

| # | Finding | Fix |
|---|---|---|
| 1 | `dropu` unnamed drew a unique's NAME and then the substring search found the first unique CONTAINING it: "Black Meridian" is inside "Black Meridian Robe", "The Long Vigil" inside "Hood of the Long Vigil" - those two could never drop and the other two dropped at double weight | the drawn INDEX is used; the search is skipped |
| 2 | `BetterRng` (`std::mt19937`) was never seeded: the first random spawn after every launch was the same item | seeded from `std::random_device` |
| 3 | `giverw`, `givesockets`, `giveethereal` walked every base row, including quest uniques' rows (`IMISC_UNIQUE`: The Undead Crown, Griswold's Edge...) - a plain helm on The Undead Crown's row wearing a runeword's name | rows with `iMiscId == IMISC_UNIQUE` skipped in all three |

Clean: item lifecycle (no slot leak on any early return), save/load (set identity by `_iCurs`, runeword by
`_iSocketed`, format 11 stores both), `ILOC_BELT`/`item_quality` exist, `givebasic` lands within a few iterations
(about 60% of worn/wielded rolls stay Normal), rune indices match what `TrySocketGem` writes.

## Monster variants and venom (monster.cpp, player.cpp, oracool/monster_variants)

| # | Finding | Fix |
|---|---|---|
| 1 | **Frenzied and Fleet did nothing.** All 138 rows in monstdat.cpp run walk and attack at rate 1, and the hook took one tick off, floored at one | frames are SKIPPED instead (`VariantSkippedFrames`, 2 each), the way the player's fast attack and run work: bounded in `NewMonsterAnim` to keep two frames, and an attack keeps its hit frame with one to spare and distributes the skip before it |
| 2 | patching `ticksPerFrame` after `setNewAnimation` would have left the distribution maths stale (walk offset, render frame) | moot with 1: the skip is passed INTO `setNewAnimation` |
| 3 | Searing/Voltaic made two `ApplyPlrDamage` calls: the physical two-thirds could kill, a cheat-death passive restore, and the elemental third kill again; every on-damaged passive rang twice | one call, `dam - elemental + resisted`, labelled with the element |
| 4 | Brutal declined only types with no special ANIMATION; ~70 of the ~85 `hasSpecial` types deal no special damage (Fallen fleeing, Scavengers eating...) so it did nothing on them | declined too when `maxDamageSpecial == 0` |
| 5 | variants were applied in `InitMonster` BEFORE a unique or champion got its identity: a Luminous light orphaned at the spawn tile when `PrepareUniqueMonst` set its own, a champion scaled from Ironhide armour (pre-existing for Hollow/Feral) | `InitMonster`/`PlaceMonster` take `ordinary` (default true); `PlaceUniqueMonst` and `PlaceLesserUniqueMonst` pass false, so no variant touches a unique or champion in the making |
| 6 | the v1.12.047 report claimed the death path frees a Luminous light; it does not (vanilla frees a monster light only on a petrified unique's death) - a lit corpse for the level, as a unique's | report and code comment corrected; no code change |
| 7 | venom ticked in town, unlike the life drain beside it | guarded on `leveltype != DTYPE_TOWN` |

Clean: venom lifetime (file-local per player, cleared in `InitPlayer`'s first-time branch, never saved, never
ticks paused or loading, floor 64 = 1 HP so it never kills, magic resist applied once), the elemental arithmetic
(total at zero resist equals the blow; a miss, dodge or block never poisons), Gilded (single death seam, MAXITEMS
checked, tumble via `FinishOracoolDrop`, no double drop), rosters (per-kind rate 7.5% Cathedral to 1% Hell, item
seeds independent).

## Not changed

- The roster test pins rosters and names only; no test pins the skip, the split or the identity ordering.
- Numbers: 2 skipped frames for both Frenzied and Fleet are first guesses, to be judged in play beside the other
  variant constants.

## Related

- [[2026-09-19-seamless-stone-backings-v1.12.051]], [[2026-09-19-single-item-debug-commands-v1.12.049]],
  [[2026-09-19-eleven-monster-variants-v1.12.047]] - the work audited.
