# Carrying a Cast to the Moment It Lands

**Version:** 1.8.37
**Date:** 2026-08-19
**Tests:** 470 total, 468 passing. The two standing baseline failures only
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`). Two new tests.

## The decision this entry was waiting on

The cast cue shipped this morning in one hook, because `StartSpell` knows the spell. Impact had no
equivalent: `Missile` carries no `SpellID`, so a resolved hit could not be traced back to the tree row
that caused it. The entry named two ways out - put an id on the missile record, or add a call to each
of the 66 skill implementations - and said the choice was the work.

**The id goes on the missile.** One place to set it, one place to read it, and every skill built from
here on gets its cue for free rather than needing to remember. 66 call sites would have been 66
chances to forget, and the count grows with every skill built out of the 97 unbuilt tree rows.

## What killed the objection to it

`Missile` is persisted in the level save at a fixed 180 bytes each, so a new field looks like a save
break on an entry marked Save: No.

It is not, because **the field is not saved** - exactly like `oracoolTrn`, the divine-TRN pointer this
fork already added to `Missile` for the same reason. A missile caught in flight across a save loses
its skill id and its impact rings nothing. That is a fraction of a second of one sound, and it is the
sound package's own "missing IDs must fail softly" rule rather than an exception to it.

So: `uint8_t oracoolSkill = 0xFF`, defaulting to `ClassTreeSkill::None`.

## Two hooks, because there are two kinds of skill

**Missiles.** `CastSpell` is where a spell's missiles are actually created - not `StartSpell`, which
only begins the animation; the missiles arrive later, when the animation reaches its action frame and
lands back in `CastSpell`. It opens a scope:

```cpp
oracool::BeginSkillCast(oracool::ClassTreeSkillForSpell(player._pClass, spl));
```

`AddMissile` stamps whatever is current onto every missile it makes. A scoped value rather than a
parameter because `AddMissile` has a dozen call sites that have nothing to do with class skills -
traps, monster attacks, town portals - and threading an id through all of them to serve the few that
care is the worse trade.

**Melee.** Zeal, Shield Bash, Hammer of Faith and the rest never make a missile. They resolve in
`ApplyMeleeSkillOnHit`, which is the single point a skill-armed swing connects, and rings there.

## The latch, and the one it does not need

The missile hook fires on the FIRST monster a missile hits and not on later ones. `_miHitFlag` was
already the "has hit something" latch, so the transition is free - and a piercing missile that rang
once per victim would be a machine gun.

The melee hook needs no latch at all: a swing lands there exactly once per connected blow. Stating
that rather than copying the missile guard across is the point - a latch that guards nothing is a
line the next reader has to disprove.

A multi-missile spell like Nova still fires one cue per bolt. The mixer drops any retrigger inside
80ms, which collapses them into the single sound the player hears - the same mechanism the module's
header already relies on for the contract's 70ms floor.

## Two tests, and one of them is the loud one

- **A missile nobody cast carries no skill.** The default is `0xFF`, not `0`. Had it been `0`, every
  trap, every monster arrow and every town portal in the game would ring the impact cue of whichever
  skill sits first in the tree enum. This is the failure that would have been immediately obvious in
  play and completely invisible in a green test run.
- **The cast scope closes behind itself.** A scope left open would attribute every later missile to
  the last skill the player cast, and the cue would fire on someone else's hit.

The cues themselves need audio and a running game, so they are a listening item rather than a test.

## All seven events are now wired

| Event | Cues | Where it rings |
|---|---|---|
| Learn | 31 | `InvestClassTreePoint` |
| Cast | 92 | `StartSpell` (v1.8.34) |
| Impact | 64 | `CheckMissileCol` and `ApplyMeleeSkillOnHit` |
| Arrive | 3 | `CheckMissileCol`, as a summon's own impact |
| Start / Loop / Stop | 38 each | `ToggleClassAura`, `RefundClassTreePoint` |

The entry that said *"the skills swing silently"* is closed. Cues that still do not play belong to
skills that do not exist yet - 97 of the 163 tree rows - and they will ring the day those rows are
built, with no sound work required.

## Files

- `Source/missiles.h` - the non-persisted `oracoolSkill` field.
- `Source/missiles.cpp` - `AddMissile` stamps it; `CheckMissileCol` rings on first hit.
- `Source/spells.cpp` - `CastSpell` opens and closes the scope.
- `Source/oracool/skill_sounds.h` / `.cpp` - `BeginSkillCast` / `EndSkillCast` / `CurrentCastSkill`.
- `Source/oracool/paladin_melee.cpp` - the melee impact cue.
- `test/oracool_audit_test.cpp` - two new tests.
- `Diablo Orcl V1/07-Backlog/Pipeline.md` - entry moved to Shipped. 35 rows.

## To listen for

Cast Fist of the Heavens or Blessed Hammer at something and the hit should now speak, separately from
the cast. Then swing Zeal into a pack - the melee path - and confirm it rings per connected blow
rather than once per burst. And the negative: walk into a fire trap, or let a Skeleton archer shoot
you. Neither should make any skill sound at all. That last one is the check worth doing first,
because it is the one that would be wrong loudly.
