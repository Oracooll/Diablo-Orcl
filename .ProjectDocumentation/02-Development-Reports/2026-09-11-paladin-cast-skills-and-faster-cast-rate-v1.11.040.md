# The Paladin's three cast skills, and Faster Cast Rate (v1.11.040)

**Date:** 2026-09-11
**Branch:** renderer-32bit (default), local commit

## The request

The user asked for these changes:

- "Blessed Hammer needs to act as spell in a sense that it should be cast with the cast speed of paladin and trigger fire spell hero animation."
- "Blessed Shield also needs to act as spell but it needs to trigger magic spell hero animation."
- "Fist of the Heavens also needs to act as spell but it needs to trigger lightning spell hero animation."
- "We need to introduce Faster Cast Rate affix in the game to make it possible to increase casting animation/speed of spells."

## Before

The three skills bypassed both the attack and the spell action. `CheckPlrSpell`'s Paladin branch called `CastRangedPaladinSkill` on the click's own tick. That spent mana and spawned the missile at once, with no hero animation.

Neither attack speed nor cast speed could touch them. Nothing in the game changed cast speed anyway: `StartSpell` passed 0 skipped frames, so a spell's length was fixed by the class's animation table (the Warrior's cast frame is 14, the Sorcerer's 8).

## The three skills are cast

**The click.** It queues a real spell, the same commands every spell sends:
- `CMD_SPELLID` on the monster in range;
- `CMD_SPELLXY` at the cursor tile with shift held.

The hero plays the spell animation. `CastSpell` then hands the skill to `CastRangedPaladinSkill` at the cast frame, before the missile table (whose rows hold `MissileID::Null`) is read. Mana leaves at the cast frame, inside the cast function as before, so `ConsumeSpell` is not reached.

**The animation.** `oracool::PaladinCastAnimation` gives Hammer fire, Shield magic and Fist lightning. `GetPlayerGraphicForSpell` prefers it over the element, so the rows' element (`Magic`) is untouched.

**Refusals.** They are still asked at the click, by `CanStartRangedPaladinSkill`: unlocked (level and shield), mana, and missile-pool room. So a refusal still falls back to the swing, as before, rather than playing an animation that ends in nothing. The cast function re-checks at the cast frame.

**Hold to repeat.** The click now records `LastMouseButtonSpell` and its type, so a held button repeats the cast. The instant path never recorded them.

**Sound, one per cast.**
- Blessed Shield's and Blessed Hammer's spell rows are silent (`SFX_NONE`). Their missiles already carry the release cue: the hammer's `IS_CAST2` and the shield's own.
- Fist keeps `IS_CAST2` at the start of the cast, because its descent has no launch sound.

## Faster Cast Rate

A new power, `IPL_FASTCAST`, appended after `IPL_MOVESPEED_CURSE`. It follows Movement Speed's route exactly, so no existing item or save changes:

| Piece | Where |
|---|---|
| Rolled | `TryAddFasterCastToDrop` on the drop tail, into the item's own affix record, never the vanilla tables. It runs after Movement Speed in `FinalizeFreshDrop`. |
| Items | Normal or magic rings, amulets and helms (5..15%) and staves (10..30%). One in twelve, with the item's level pulling the floor up to double. Never on uniques or tiered items. |
| Stored | Not a field: re-derived from the affix records on load (`loadsave.cpp`), so the item format did not grow. `IsOracoolAffixTypeValid` now accepts up to `IPL_FASTCAST`. |
| Summed | `Item::_iPLFastCast`, then `ItemBonusTotals::fastCast`, then `Player::_pIFastCast` (`CalcPlrItemVals`). |
| Shown | The tiered-affix printer, the set-bonus printer, a plain/magic item line ("+N% faster cast rate"), and the stat sheet. |
| Applied | `StartSpell` passes `oracool::CastFrameSkip(_pSFNum, _pIFastCast)` as the skipped frames, spread before the cast frame. |

**The formula.** +X% casts in `castFrame × 100 / (100 + X)` ticks, rounded. The skip is never more than `castFrame − 1`, because `DoSpell` fires *on* that frame.

| FCR | Warrior (14) | Sorcerer (8) |
|---|---|---|
| +20% | 12 ticks | 7 ticks |
| +50% | 9 ticks | 5 ticks |
| +100% | 7 ticks | 4 ticks |

It applies to every spell: the three Paladin casts, book spells, scrolls, staves and warcries.

**Not yet.** Uniques, sets and runewords cannot grant it. A unique or set power would need the field stored (format bump) or re-derived from its definition, the same open question Movement Speed has (`movement_speed_percent` is still Inert for uniques).

## Tests

ctest **706/706**, and no golden moved (pack fixtures, writehero). Three new tests:
- `FasterCastRateSkipsCastFramesAndNeverTheCastItself`: the formula at the table's points; never the cast frame for any class up to +5000%; a worn ring reaching `_pIFastCast`.
- `FasterCastRateRollsOnTheDropTailIntoTheItemsOwnRecord`: the staff and ring ranges, the record and field agreeing, and never on a sword or a unique.
- `PaladinCastSkillsTakeTheirOwnSpellAnimation`: the three animations, other spells untouched, and the melee four and Charge staying swings.

No test drives `CheckPlrSpell` or a full cast through `DoSpell`: the input path has no harness.

## Verification

Debug and Release built, and RTM was refreshed with exe 1.11.040. **Not seen in play.**

**To check:**
- Blessed Hammer: a fire cast, then the hammer leaves at the cast frame.
- Blessed Shield: a magic cast.
- Fist of the Heavens: a lightning cast, with the mace falling at the cast frame.
- Holding the button repeats the cast.
- An unaffordable cast swings instead.
- Faster Cast Rate on a found ring, helm, amulet or staff: in the tooltip, on the stat sheet, and visibly shortening the cast.

**Timing change.** The skills now cost the Paladin's cast animation, 14 frames at 20 per second (0.7 s) before the missile leaves, where they used to be instant. Faster Cast Rate is what buys that back.
