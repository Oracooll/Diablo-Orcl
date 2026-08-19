# Reaudit of v1.8.17 Through v1.8.38

**Version:** 1.8.39
**Date:** 2026-08-19
**Scope:** the 29 commits since `32a4f2e` (v1.8.16), the last audited point.
**Tests:** 470 total, 468 passing. The two standing baseline failures only.

Everything below was checked against the source, not against the dev reports that described it.

## Two findings, both fixed

### 1. Four files had their line endings silently rewritten

`.gitattributes` opens with a one-line instruction: `# Do not let git change line endings.` My
editing tools ignored it. Four files that were fully CRLF came out fully LF:

| File | CR bytes before | after |
|---|---|---|
| `Source/qol/stash.cpp` | 1163 | 0 |
| `test/inv_test.cpp` | 1244 | 0 |
| `Source/spells.cpp` | 366 | 0 |
| `Source/cursor.h` | 99 | 0 |

Nothing was functionally wrong - the compiler does not care - but the history was ruined. The real
change to `stash.cpp` in that range is **two lines**; the committed diff was **2326**. Reviewing any
of those four commits meant reading a full-file rewrite to find a one-line edit, and a genuine defect
introduced alongside would have been invisible in the noise.

That is the same failure shape as the `Pipeline.md` corruption found earlier today, and it went
unnoticed for the same reason: a diff stat that looks plausible is not a diff anyone read.

Restored to CRLF. `git diff 32a4f2e` on those four files now reads 2, 14, 9 and 109 lines - the
actual work.

**The rule this leaves:** on this repo, in-place `perl -i` and `sed -i` rewrite line endings on
CRLF files. Either preserve them explicitly or use the editing tools that do. Worth checking
`git diff --stat` against expectation before every commit, not after.

### 2. A branch that is now unreachable, kept on purpose

`GetResistInfo` colours negative resistance red. Since v1.8.35 `ApplyResistanceCurve` floors at zero,
so nothing can reach it.

Deleting it was the wrong call. `player_resistance.h` records removing that floor as a **one-line
change** if negative resistance is ever wanted, and this is the branch that would have to return with
it - deleting it turns that one-line change into a two-file change for no gain. Kept, with the reason
written beside it so the next reader does not "clean it up".

## Checked and clean

**The impact-cue latch.** `_miHitFlag` is reset to `false` in exactly one place - the
blocked-by-tile branch of `CheckMissileCol`, which runs *after* the monster-hit branch in the same
call. So a monster standing on a blocked tile could let one missile ring twice. It is a rare
geometry, and the mixer's 80ms retrigger drop absorbs it. Noted rather than guarded: a second latch
field to cover it would cost more than the case is worth.

**Missiles loaded from a save.** `LoadMissile` builds its missile with `Missile missile = {}`, which
applies the default member initializers - so `oracoolSkill` comes back as `0xFF`, not `0`. This is
the failure the new test pins, and the load path independently satisfies it. A missile in flight
across a save rings nothing, as designed.

**Monster-fired missiles never ring a skill cue.** `CurrentCastSkill()` is `None` except inside
`CastSpell`, which is player-only (`Players[id]`). Traps, monster attacks and town portals all stamp
`None`.

**Nested casts.** `mAddProc` can call `AddMissile` recursively - chain lightning and similar - and
those children inherit the open scope, which is correct: they are the same cast. `CastSpell` has no
early return between `BeginSkillCast` and `EndSkillCast`, so the scope cannot leak.

**`glSeedTbl[currlevel]` in `GetMonsterSize`.** This was the crash candidate: an out-of-range
`currlevel` would read past a 25-entry array on every monster draw. `interfac.cpp:408` sets
`currlevel = setlvlnum` on quest levels, and `_setlevels` tops out at `SL_ARENA_CIRCLE_OF_LIFE` = 8.
In range on every path.

**Resistance bounds.** `ApplyResistanceCurve` returns `[0, 90]` for every input over
`raw` -200..400 on all four difficulties, which the new exhaustive test pins; the three player fields
are `int8_t` and hold it comfortably. Difficulty is fixed for a session, so no recomputation gap.

**Save format.** Nothing in the range writes a new field to disk. The resistance change moves the
*values* two pinned tests assert (already handled and named), the monster size is derived rather than
stored, and `Missile::oracoolSkill` is deliberately not persisted - `SaveMissile` still writes its
fixed 180 bytes.

## One thing left as-is, deliberately

`PrintItemDetails` prints "MAX" on an item whose own resistance roll reaches 75. With the player cap
now 90, that reads oddly - but it is a statement about the *item's* roll ceiling, not the player's
total, and it is vanilla behaviour on a vanilla string. Changing it means deciding what an item's
"max" means under a soft cap, which is a design question, not an audit fix.

## What this audit could not check

The HUD close buttons, the Space master-closer, Levski's grid, the three refreshed icons and every
sound cue are all things only a screenshot or a speaker can confirm. They are listed in their own
reports as look-and-listen items. This pass covers what the source can answer.

## Files

- `Source/qol/stash.cpp`, `Source/spells.cpp`, `Source/cursor.h`, `test/inv_test.cpp` - CRLF restored.
- `Source/panels/charpanel.cpp` - why the red branch stays.
