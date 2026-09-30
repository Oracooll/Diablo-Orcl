# 2026-09-30 - Sorcerer Skill Cards: Cold Spells picks applied (v1.12.270)

**Date:** 2026-09-30. Debug only. The user asked: "look at my sorc artefact and apply the inputs in Coold Spells. The other
two spell families are not done yet."

The Sorcerer page's `picks` collection held 37 documents, all on the Cold Spells sheet. There were no comments.
- **Sounds:** 32 sound picks differed from the game. All are applied through `tools/skill_sound_card_picks.json`
  (`cards/mkcardpicks.js`) and the regenerated `skill_sounds_data.inc`.
- **Animations:** 2 animation picks changed the size.
- **Unchanged:** Nova's lightning pick (Nova is not a Cold spell) and the unchanged Frozen Armor and Frozen Orb cast picks.

## Sounds

| Skill | Cast / Start | Impact | Stop |
|---|---|---|---|
| Absolute Zero | Explosion (Hellfire) | Fire barrel blast *(new place)* | |
| Blizzard | Generic cast (cast6) | Fire barrel blast | |
| Brittle Ground | Generic cast (cast6) | Skeleton crumbles | |
| Chill Touch | Fire arrow | Bone Spirit impact *(new place)* | |
| Chilling Armor | | | Mana Shield |
| Frost Nova | | Bone Spirit impact | |
| Frostbite | Fire arrow | Bone Spirit impact *(new place)* | |
| Frozen Armor | Fade out (invisibility) | | Fade out (invisibility) |
| Frozen Orb | | Fire arrow | |
| Frozen Sentinel | Fire arrow | Bone Spirit impact *(new place)* | |
| Glacial Spike, Ice Blast | Firebolt | Bone Spirit impact | |
| Ice Bolt, Ice Lance, Ice Needle | Fire arrow | Bone Spirit impact | |
| Shiver Armor | Elemental | Bone Spirit impact *(new place)* | Elemental |
| Whiteout | Generic cast (cast6) | Fire barrel blast *(new place)* | |

Six Impact places were new: the game played no sound there before. Each got a row in `tools/skill_sound_slots.csv` and a
call where the effect lands:
- **Chill Touch** and **Frostbite:** on the struck monster.
- **Absolute Zero:** when it catches anything.
- **Whiteout:** once a step that strikes anything.
- **Frozen Sentinel:** each bolt the sentinel fires carries a flag (`Missile::sentinelBolt`) and sounds the cue where it
  lands.
- **Shiver Armor:** on the striker it answers.

## Animations

- **Absolute Zero's burst:** drawn at 200%.
- **Chill Touch's frost cone:** drawn at 125%.

`cards/build.js` records both in SKILL_FX. The Sorcerer page was rebuilt and republished, so its "in the game" values now
show these picks.

## Tests

No new tests. Debug build and ctest: 892/892. Not heard or seen in play.
