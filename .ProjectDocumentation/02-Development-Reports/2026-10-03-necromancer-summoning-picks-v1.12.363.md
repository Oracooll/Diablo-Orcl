# 2026-10-03 - Necromancer Summoning picks (v1.12.363)

**Date:** 2026-10-03. Debug only. All 923 tests pass. The Debug `diablo.ini` was unchanged by ctest. Commit acdad716.

The user's notes come from the Necromancer Skill Cards page, db `picks`: 11 docs, all on the Summoning sheet. The snapshot is in scratchpad `cards/db363` and is now the applied state in `cards/db/necromancer`.

## Applied
- **Golems:** the vanilla golem through a real tint on the lit palette (`TintedTable`), in a new branch of `DrawMonster` for minions.
  - Clay: (184,120,78).
  - Blood: (216,30,36).
  - Iron: (132,146,168), with the new `Tint::Sheen`. Each palette entry glints with its own phase and pace, so the highlights wander at random.
  - Code: `GolemLook` and `MinionBodyLook` in `oracool/minions`. The 8-bit path keeps the old ramps.
- **Skeletal mages:**
  - They now use the regular skeleton bodies by rank.
  - Each is tinted in its element with `Tint::Shimmer`, a slow ±12% swing of the same hue: fire orange, lightning blue, poison green, bone white.
  - All four bolts fly as the bone tooth, tinted per element (bone untinted). Each lands with its own impact as before.
- **Frenzy of the Dead:**
  - The motes over each minion are gone.
  - `DrawMinionFrenzyRings` puts Holy Fire's ground ring under every frenzied minion, recoloured `hue::FrenzyOrange`, colour-cycled, at 50% of the Paladin ring (48x24).
  - Nothing is drawn on the Necromancer.
- **Sounds:** through `tools/skill_sound_slots.csv` and `skill_sound_card_picks.json`, with the generator re-run.
  - Golem Impact = `golma1`, played by `OnMinionBlow` on a landed blow. No golem Impact cue played before.
  - Fire Golem Cast = `elementl`.
  - Fire Golem Impact = `fbolt1`.

## Waiting on the user
- **Fire Golem:** "We must use Elemental sprite but we need to reduce its movement speed a bit and we need to come up with some attacking animation. Propose something."
- **The constraint:** the Elemental is only a missile sheet (`missiles\firerun`, 8 facings x 12 frames, 96 px). It has no stand, attack or death frames, and no monster entry.
- Proposals went to the user.

## Notes
- The renders (preview `OracoolPreview.DISABLED_Necro363SummoningLooks`) are not in-game screenshots. Check the ring's foot under a walking minion in play.
- `frenzy_of_the_dead.png` is still loaded but no longer drawn.
