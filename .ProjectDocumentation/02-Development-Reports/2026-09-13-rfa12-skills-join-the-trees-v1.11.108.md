# The 162 RfA-12 skills join the class trees, with placeholder letters and their level-up stats

2026-09-13 — v1.11.108

## Why

RfA-12 asked ChatGPT for ideas to fill the empty cells on every class's three ability pages; Claude
wrote its own set in parallel, the user picked one proposal per cell on the comparison page, and the
result is `Resources/ChatGPT RfA/RfA-12 - Final Skill List.md` (122 Claude, 40 ChatGPT). RfA-13 asks for
their glyphs. User: "in the mean time implement the skills with no icons, just placeholder letters."

This is the foundation unit. Every skill now exists in the game: on its page, in its cell, taking points,
saved, with a tooltip and an icon. What each one DOES comes in the units after it.

## What changed

**162 rows, appended.** Generated from the final list's CSV (`scratchpad/gen_skills.js`) rather than
typed: an enum entry and a `Skills[]` row per skill, each at the END of its class block. A row's position
within its class is its icon-strip frame and its `_pClassTreeInvestment` slot, so appending is what keeps
every saved point where it was paid.

| Class | Rows before | Added | Rows now |
|---|---|---|---|
| Paladin | 48 | 24 | 72 |
| Barbarian | 49 | 24 | 73 |
| Sorceress | 48 | 24 | 72 |
| Rogue | 49 | 24 | 73 |
| Bard | 39 | 33 | 72 |
| Monk | 39 | 33 | 72 |

`ClassTreeSkillCount` 272 -> 434. Kinds per page: auras on the Paladin's aura pages and the Bard's
Melody (songs are auras in this engine: one plays at a time), passives on Combat Masteries and where the
design says passive, actives everywhere else. Every new row is `SpellID::Invalid` and **not yet
implemented**, so it draws struck out, takes no points and contributes nothing - the inert-row rule the
tree has always had, and `EveryInertRowContributesNothing` still holds it.

**The per-class cap, 64 -> 96.** `MaxSkillsPerClass` and `Player::_pClassTreeInvestment` grew together
(the static_asserts in `class_tree.h` demand it). The save chunk is count-prefixed, so a 64-entry hero
loads into the first 64 slots. `Writehero.pfile_write_hero` re-baselined: the chunk is 32 bytes longer,
and the comment log there says so.

**Placeholder letters.** `hud_art.cpp`: a skill whose index runs past its class strip (every RfA-12
skill, until batch 31) draws the plate a glyph would sit on and one or two capital letters - the first
letters of its first two words, skipping "of/the/and" (Wrath of the Heavens -> WH; a one-word name
gives two letters, Cleave -> Cl). All four tree-icon paths do it: the Abilities grid, the wells, the
pickers and the origin-drawn icon. `EveryClassTreeStripHasAFrameForEveryOneOfItsSkills` now requires the
strip to cover every row BEFORE the RfA-12 block - the truncation it was written to catch - since the
block itself is waiting on RfA-13.

**The level-up stat engine.** Every RfA-12 skill carries one character stat that grows with rank. A
generated table (`LevelUpStats[]`, 162 entries, the list's own wording in a comment on each) and
`ApplyLevelUpStat` cover all 21 channels the list uses. Where it applies:

- an **aura** (and song): while it burns, and it goes out with it;
- an **active**: for as long as points sit in it - what learning the skill made of you, not a buff;
- a **passive**: with its points, like every Diablo II passive.

Life and mana go through the 1/64 shift the engine keeps them in. The tooltip line is its own
(`LevelUpStatLine`) because `DescribeBonusTotals` prints life and mana in those raw units. All of it is
gated on `implemented`, so the stat switches on skill by skill as each main effect is built.

## Known, and not changed here

- **Aura hotkeys store absolute ordinals** (tag 14). The Paladin block grew by 24, so a Barbarian-onward
  hero's saved aura hotkeys now point at the wrong row and are dropped by the class check on load - the
  lit aura itself uses the class-relative tag 13 and survives. V1 always starts a New Game, and the
  bindings are one re-bind each.
- **Animosity, Astral Presence and Exalted Soul** add `totals.mana += 20` - that is 20/64 of a mana
  point, not twenty. Found while checking units for the Life/Mana channels; not this unit's change.

## What is next

- The 48 skills that need no spell id (16 Paladin auras, 11 Bard songs, 21 passives): main effects.
- The 114 actives and warcries: `SpellID` is `int8_t` with 126 of 128 ids used, so they need the enum
  widened first. Checked: `PackReadiedSpell` stores id+1 in a byte, which fits up to 254, and the
  skill-investment chunk's count byte fits 240 - so the saves survive a widening to 240 ids; `SpellMask`
  needs two more words.

## Tests

29 class-tree, strip and save tests pass after the two re-baselines; the full suite before them was
729/733, the four failures being exactly those two tests and their shuffled runs.
