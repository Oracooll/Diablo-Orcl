# Ninety-Two Cast Cues, and a Backlog File That Had Been Corrupt for Three Commits

**Version:** 1.8.34
**Date:** 2026-08-19
**Tests:** 459 total, 457 passing. The two standing baseline failures only
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).

## The entry was half true

The Pipeline said *"Cast and impact skill sounds - the skills swing silently. 305 sounds are already
in the archive."* Checking before starting, as the last three entries have taught:

`Source/oracool/skill_sounds.{h,cpp}` and a 315-line generated `skill_sounds_data.inc` have been in
the tree for some time. The 304 WAVs are cut, the manifest is joined to the class tree at build time
by `tools/GenSkillSounds.ps1`, and the loader, the cache, the aura loop handle and the set-completion
baseline all exist and are commented at length. So "the skills swing silently" was not describing a
missing system.

It was describing missing CALL SITES. Grepping every caller outside the module itself found exactly
three:

| Event | Cues in data | Wired |
|---|---|---|
| Learn | 31 | yes - `InvestClassTreePoint` |
| Start / Loop / Stop | 38 each | yes - `ToggleClassAura`, `RefundClassTreePoint` |
| Cast | **92** | **no** |
| Impact | **64** | **no** |
| Arrive | **3** | **no** |

159 cues were being loaded, indexed and never played. The entry was right about the silence and
wrong about the cause, which is why it read as a content job and was actually a wiring job.

## One hook for ninety-two cues

`StartSpell` (`Source/player.cpp:327`) is the accepted-activation point for every spell and skill in
the game: the queue has been re-validated, the animation is starting, `player.executedSpell` is about
to be set. Every tree skill with a slot passes through it, including the melee-latched Paladin
skills - Zeal and Shield Bash are ordinary `SpellID`s with a latch inside their handler, not a
separate cast path. So one hook there is true for all six trees at once, rather than six
implementations each remembering to ring.

The reverse lookup already existed - `ClassTreeSkillForSpell(heroClass, spell)`, written for the HUD
wells - so nothing new had to be built to get from the spell being cast back to the tree row.

```cpp
const oracool::ClassTreeSkill castSkill = &player == MyPlayer
    ? oracool::ClassTreeSkillForSpell(player._pClass, player.queuedSpell.spellId)
    : oracool::ClassTreeSkill::None;
if (!oracool::PlaySkillSound(castSkill, oracool::SkillSoundEvent::Cast))
    PlaySfxLoc(GetSpellData(player.queuedSpell.spellId).sSFX, player.position.tile);
```

Two decisions worth naming:

**The vanilla sSFX is a fallback, not a casualty.** 92 of 163 tree rows have a cast cue. Replacing
the line outright would have silenced the other 71 to give 92 their voice. The class cue wins where
it exists and the old noise stays everywhere else.

**`PlaySkillSound` now returns `bool`.** That is the whole reason - a caller with a fallback has to
know whether the cue rang, and the alternative was a second `FindSound` scan at the call site
answering the same question. Existing callers ignore the return and are unaffected.

The local-player guard is there because `PlaySkillSound` is deliberately non-spatial (its own header
says so): correct for your own cast, wrong for anyone else's. V1 is single-player, so this is a
guard rather than a feature.

## What impact needs, and why it is not in this build

Impact has no equivalent single hook. `Missile` carries no `SpellID` - checked `missiles.h`, the only
mentions are in `GetDamageAmt` signatures - so a resolved hit cannot be traced back to the tree row
that caused it. Two ways out, both real work rather than a call site:

1. put a spell id (or skill id) on the missile record and ring from the resolution path, which pays
   once and covers every missile-based skill, or
2. a per-skill call inside each implementation, which is 66 edits and grows with every skill built.

Choosing between those is the job. The Pipeline entry is now `Skill impact sounds`, still Small, with
that choice written into it - so whoever picks it up starts from the decision rather than from
"the skills swing silently."

## The backlog file had been corrupt since the 16th

Reading `Pipeline.md` to check the sound entry, its first line turned out to be three table rows
concatenated together, sitting ABOVE the `# Pipeline` header - the movement-speed row, the wiki-
numbers row and the gold-auto-place row, joined by ` || `. The originals were all still in the table
below, unedited.

So three of the last four Pipeline "edits" had not edited anything. Each one appended to that stray
line, and the last commit's diff - `1 insertion, 1 deletion` - is exactly what a rewrite of one long
line looks like, which is why it read as clean. `tools/BuildWiki.ps1` parses the table by header
position, so the generated Pipeline page has been showing the OLD text of all three entries.

Repaired properly this build: the stray line removed, the movement-speed and wiki-numbers rows
replaced in place, the gold row and the per-difficulty-immunities row moved to the Shipped note
(both landed in v1.8.32 and v1.8.33), and the sound row rewritten. 39 rows, all under the header.

The lesson is narrower than "check your edits": a one-line diff on a file whose rows are one line
each is indistinguishable from a correct edit and from a catastrophic one. Verifying a Markdown table
edit means reading the table back, not reading the diff stat.

## Files

- `Source/player.cpp` - the cast hook in `StartSpell`, and the `skill_sounds.h` include.
- `Source/oracool/skill_sounds.h` / `.cpp` - `PlaySkillSound` returns whether it rang.
- `Diablo Orcl V1/07-Backlog/Pipeline.md` - structural repair plus four row changes.
- `ORACOOL_VERSION` - 1.8.34.

## To listen for

Cast a tree skill that has a cue - Bash, Berserk, Concentrate, Double Swing on a Barbarian are all in
the first page of the data - and it should now speak with its own voice rather than the generic
`IS_CAST2`. Cast one WITHOUT a cue and the old sound must still be there; that is the half of this
change that fails silently if it is wrong.
