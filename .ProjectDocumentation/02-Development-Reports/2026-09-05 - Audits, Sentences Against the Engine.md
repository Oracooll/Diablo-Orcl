# Audits, Sentences Against the Engine (v1.9.263)

**Date:** 2026-09-05 · **Request:** "i am away. do a bunch of audits."

## 1. Skill sentences versus the facts the modules roll

`tools/AuditSkillSentences.pl` reads the Skills table and the `--skill-facts` dump and compares every number a sentence states with the number the module prints at rank 1 and 2: damage bonus and its per-rank climb, strike counts, durations, chances, sweep shares. 71 skills checked.

**16 flagged, all fixed:**

| Skill | Sentence said | Module rolls | Fix |
|---|---|---|---|
| Bash, Power Strike | +33% | +30% | sentence |
| Whirlwind, Wheel of Heaven | 67% | 66% | sentence |
| Charged Strike, Lightning Strike | +20% | +20%, +5% per rank | sentence names the climb |
| Tranquility | 12 s | 13 s at rank 1 (12 + rank) | sentence |
| Battle Command, Battle Orders, Grim Ward, Purifying Breath, Conversion, Vengeance | durations in words | seconds | digits, "+N per rank" |
| Frozen / Shiver / Chilling Armor | "for a while" | 24 s, +4 per rank | sentence, in class_tree and cold.cpp |
| Frozen Armor | freezes | facts said "chill" | **the facts were wrong**: Frozen Armor freezes (uniques chilled), Shiver chills and bites with an Ice Bolt, Chilling chills near or far |

The "+33%" and "67%" came from my own fraction-to-percent pass this morning, which rounded "a third again" and "two thirds" arithmetically while the code says 30 and 66. The audit re-run is clean: 71 checked, 0 flagged.

## 2. Every strip frame

`tools/AuditGlyphStrips.ps1` classifies every frame of the six class strips and the attack strip as glyph (the two colours on transparency, bbox inside 8..47), legacy or empty. Result: 273 frames, 255 glyph, 18 legacy, 0 empty, 0 out of bounds; both attack frames glyph. The 18 legacy frames are exactly the rows that ride engine spells and draw the engine's icon on the page anyway.

## 3. Key defaults

Every keymapper default letter is bound once (A, S, D new; B C F G I L P Q R T V W X Z as before). The dev ini's `CancelAction=A` and `DisplaySpells=A` are pad-mapper rows, not keyboard.

## 4. Smaller findings, fixed

- `--skill-facts` had no help line; it has one.
- The skill-well static_assert message still pointed at `CutAttackIcons.ps1`; it names the glyph strips now.
- `tools/ApplyPaladinGlyphDraft.ps1`, the eleven-glyph pilot, is retired; `BuildGlyphStrips.ps1` supersedes it.

## Not changed, worth knowing

- The comparison panel does not open for a SHOP item: the shop grid has no container hover of its own. It would be one more branch in `HoveredContainerItem` if wanted.
- `IsHoverHeadingLine` matches the translated heading prefix ("Current Skill Level") against builders that translate the whole "Current Skill Level: {:d}"; a translation that renders the two differently would lose the gold. English is unaffected.

## Verification

Debug build clean; 625/626 with the standing `Drlg_l1` failure. The hover matrix was regenerated from the corrected sentences and the re-dumped facts and republished.
