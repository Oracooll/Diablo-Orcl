# Audit of the Day's Work

**Date:** 2026-08-18
**Scope:** v1.7.71 → v1.7.86, seventeen commits — the MPQ sweep units A/C/D, the sixteen-item review
list, and the startup fix.
**Method:** re-read every change against the code it touched, rather than against the commit message
that described it.

Four findings. One is a wrong diagnosis I shipped as a fix.

---

## 1. Item 7 was NOT fixed. The change was inert.

**Severity: high — the reported bug may still be present.**

v1.7.80 rewired seven Paladin tree rows from `SpellID::Invalid` to real ids (Smite→ShieldBash, Zeal,
Charge, Blessed Hammer, Fist of the Heavens, Hammer of Faith, Blessed Shield), on the reasoning that
the click handler's `!IsValidSpell(ClassTreeSpellId(hit))` guard was rejecting them.

That reasoning was wrong. `ClassTreeSpellId` (class_tree.cpp:585) consults `BorrowedPaladinSkill`
**first** and returns `GetPaladinSkillData(*borrowed).spellId` — and all seven are in that switch
(`:563–583`). They were **already** returning valid ids. The table values I edited are shadowed and
never read for these rows.

The edit is harmless — the values agree with what the borrowed path returns — but it fixed nothing.

**What actually gates those clicks** is the next clause, `IsClassTreeSkillUnlocked`, which for a
borrowed row defers to `IsPaladinSkillUnlocked` (`:699–700`). That carries its own minimum level
**and a shield requirement**, and the comment above it describes the user's exact symptom:

> "The visible half was Smite: the Combat Skills page lit it at level 1 with no shield in hand, let
> it be clicked onto a mouse button, and the button then did nothing"

Requirements: Smite level 8 **and a shield**, Charge 12, Blessed Hammer 18, Fist of the Heavens 30.

So if the character was below those levels or had no shield equipped, the cells refuse — **silently**,
which is indistinguishable from "clicks do nothing". v1.7.81 added a fallback for *unbuilt* rows only;
**locked** rows still return without a word, deliberately.

**Recommended:** give a locked row the same courtesy an unbuilt one now gets — say why. A message
("Smite requires level 8 and a shield") or a lock glyph on the cell. Until then item 7 should not be
considered closed.

---

## 2. Seven files had their line endings rewritten wholesale

**Severity: medium — no runtime effect, real review and history cost.**

My `awk`/`perl` whole-file rewrites converted CRLF to LF across files where I changed only a handful
of lines:

| File | CR bytes lost |
|---|---|
| `Source/objects.cpp` | 5,661 |
| `Source/options.cpp` | 2,308 |
| `Source/options.h` | 1,084 |
| `Source/player.h` | 1,015 |
| `Source/plrmsg.cpp` | 137 |
| `Source/panels/mainpanel.cpp` | 90 |
| `Source/panels/spell_book.hpp` | 88 |

`.gitattributes` says `* -text` — *"Do not let git change line endings"* — so these are real committed
byte changes, not a checkout artifact. `objects.cpp` shows **11,386 changed lines for a ~20-line
edit**; `git blame` on those files now attributes every line to today.

This is not cosmetic. **It is why the startup bug got through**: a one-character error inside an
11k-line diff is unreviewable, so nothing — not the build, not the suite, not me — was in a position
to catch `ctrlpan\talkpanl` losing a backslash.

**Recommended:** restore CRLF on those seven files in one dedicated commit, so the history says
"whitespace only" plainly. And stop rewriting whole files to change three lines.

---

## 3. The message history now paints over open side panels

**Severity: medium — visual, intermittent.**

v1.7.85 moved `DrawPlrMsg` into the minimap's column and dropped the old
`IsLeftPanelOpen`/`IsRightPanelOpen` avoidance, which the old bottom-left placement needed.

But the inventory is `{gnScreenWidth - 340, 0}`, 340×720 — flush top-right, full height — and the
minimap sits **inside** that footprint at `gnScreenWidth - 314 → gnScreenWidth - 8`. Draw order is
`DrawInv` at scrollrt.cpp:1480, `DrawPlrMsg` at `:1545`, so the history draws **on top of** an open
inventory, stash, quest log, character sheet or Abilities window.

Messages last ten seconds, so this shows up as text intermittently crossing an open panel rather than
as a permanent defect.

**Recommended:** suppress the history while a 340-wide panel is open, or move it below the panel's
bottom edge. Worth confirming against the screen first — the minimap already shares that footprint,
so whatever it does may be the precedent to follow.

---

## 4. Verified sound, no action

- **`_pSplLHotKey` / `_pSplLTHotKey` initialisation.** The new arrays are never explicitly cleared,
  unlike `_pSplHotKey` (loadsave.cpp:2401). Traced: `UnPackPlayer` does `player = {}` (pack.cpp:403)
  *before* `ApplyHeroChunks` runs, so they zero-initialise to `SpellID::Null` — which `IsValidSpell`
  rejects (`spl > SpellID::Null`). No leak between characters, no garbage binding. Safe, but it is
  safe because of a wipe elsewhere rather than by its own statement; a `std::fill` beside the
  existing one would make it say so.
- **Chunk compatibility.** `HeroChunkSpellHotkeysLeft` is a new tag, and both hotkey chunks are
  count-prefixed and clamped with `std::min`, so a 6-slot hero loads into the 8-slot build and an old
  build skips the unknown tag. The `Writehero` re-baseline was correct and documented.
- **Grid bezel clearance.** `GridFrameWidth` is 6 with a `static_assert` that the fallback bevel can
  never exceed it, and both orb-clearance asserts compile — so the inventory grid and the stash's
  sixteenth row provably still clear the orbs.
- **Asset paths.** Every string literal in the files touched today was re-checked for the collapsed
  escape that caused the startup crash. `ctrlpan\talkpanl` was the only one.

---

## Suite

445 of 447 throughout, the two standing baseline failures. `Writehero` moved once, deliberately, for
the new save chunk.

## What this says about the day

Two of the three real findings trace to the same habit: rewriting whole files with stream tools
instead of making targeted edits. That produced the line-ending churn, the churn hid the backslash,
and the backslash was the crash. The wrong diagnosis in finding 1 is separate — that was reasoning
from a plausible-looking guard without following `ClassTreeSpellId` into its first branch.
