---
date: 2026-08-16
status: COMPLETE - five units plus two re-sweep units shipped (v1.7.14 - v1.7.19)
area: Asset intake from the MPQ folder's root drop zone
---

# MPQ Drop Zone — Introduction Plan

> **Closed 2026-08-16.** Every file in the drop zone is consumed. Unit 1 shipped as v1.7.14 (the
> gem quality ladder); units 2-5 shipped together as v1.7.15 (the tree generalized to four classes,
> and the Barbarian, Sorceress and Rogue sheets filling it), with v1.7.16 retiring the invented
> Barbarian list the real tree superseded. Reports: "Seven Gems, Five Qualities" and
> "Four Trees, One Grid".
>
> The honest backlog the trees leave behind is the set of engine capabilities the inert rows are
> waiting on: a **cold damage channel** (the whole Sorceress Cold page, Holy Freeze, the Rogue's
> ice arrows), a **monster-facing aura pass** (Conviction, Sanctuary, the warcries, Inner Sight),
> **missile work** (every bow skill), **movement work** (Leap, Whirlwind), **corpse handling**
> (Redemption, Find Item, Find Potion, Grim Ward) and **buffs with a duration** (Shout, Battle
> Orders, Battle Command). Each unlocks a named group of rows rather than one skill, which is the
> argument for building the capability rather than the skill.

A sweep of `Oracool.MPQ\` (the "MPQ folder") on 2026-08-16. The **root** is the drop zone: the user
puts new art there, and everything below it (`01-in-use`, `02-source-art`, `03-concepts`,
`04-references`, `05-asset-plan`, `99-original-game-art`) is either already consumed, reference, or
the vanilla extraction.

## Root inventory

| File | Added | Status |
|---|---|---|
| `Paladin Skills.png` | 08-15 | **Done** — cut to `ui\paladin_skill_icons.png` (v1.1.x) |
| `Fist and Regular Attacks.png` | 08-15 | **Done** — cut to `ui\attack_icons.png` |
| `Runes.png` | 08-16 08:58 | **Done** — 5 of 33 cut (v1.7.12); 28 spare for higher rungs |
| `Paladin Skill Tree.png` | 08-16 10:37 | **Done** — 29 icons, three pages (v1.7.13) |
| `Barb Skill Tree.png` | 08-16 10:41 | **NEW** → unit 3 |
| `Gems.png` | 08-16 10:56 | **NEW** → unit 1 |
| `Sorc Skill Tree.png` | 08-16 10:56 | **NEW** → unit 4 |
| `Rogue Skill Tree.png` | 08-16 11:02 | **NEW** → unit 5 |
| `README.md` | 08-13 | vault documentation, not an asset |

All four new sheets are 1448×1086 green-screen, the same format as the Paladin tree — so the
`tools/CutPaladinTree.ps1` approach (per-row y-bands to exclude the printed labels, green **spill
suppression** rather than a flat key) transfers directly.

## What the new sheets contain

**Gems.png** — 7 gem types × 5 quality tiers, as pixel art:
Amethyst, Diamond, Emerald, Ruby, Sapphire, Topaz, Skull, each in chipped / flawed / normal /
flawless / perfect. This is Diablo II's complete gem progression, and it supersedes the five gem
icons cut from a phone screenshot in v1.7.8.

**Barb Skill Tree.png** — 30 skills: Combat Skills, Combat Masteries, Warcries (10 each).
**Sorc Skill Tree.png** — 30 skills: Cold Spells, Lightning Spells, Fire Spells (10 each).
**Rogue Skill Tree.png** — 30 skills: Bow & Crossbow, Passive & Magic, Javelin & Spear (10 each).

Each is three pages of ten, exactly the shape `oracool/paladin_tree` already implements.

## The units, in order

### Unit 1 — Gems: seven types, five qualities (v1.7.14)

The largest gameplay gain of the four, and self-contained. Today there are five gems at one
quality each, wearing icons cut from a screenshot. This replaces them with the real art and opens
D2's whole gem economy:

- 35 gem items (7 types × 5 qualities), replacing the current 5. Amethyst and Diamond are new
  types; the existing five become the **normal** (middle) quality of their type.
- Effects scale by quality — a chipped Ruby is a starter drop, a perfect Ruby is a prize.
- **The crafting recipe becomes D2's own**: three gems of the same type and quality make one of the
  next quality. The current "three same gems → random rune" recipe is wrong for D2 and is replaced;
  runes keep their own path (two identical runes → next rung).
- Drops weight toward low quality early and high quality deep, so the ladder is walked rather than
  skipped.

Risk: 35 new item indices moves `IDI_LAST` and needs the pack tests extended — the same additive
pattern the 73 armour items used, and all new indices stay pool-excluded and drop via their own
hook, so seeded item generation is untouched.

### Unit 2 — Generalize the tree (v1.7.15)

`oracool/paladin_tree` is written for one class. Before three more trees land it becomes
`oracool/class_tree`: the same grid, gating, two-store investment and hover machinery, with the
class's skill table and icon strip as data. No new content — a pure seam, with the Paladin's
behaviour pinned by its existing tests so the refactor is provably neutral.

Doing this BEFORE the other trees is the whole point: three copies of the Paladin page code would
be three places to fix every future bug.

### Unit 3 — Barbarian tree (v1.7.16)

30 skills. Supersedes `oracool/barb_skills` (18 invented skills, hidden behind
`ClassAbilitySheetsHidden`) exactly as the Paladin tree superseded the invented auras.

Engine fit: the masteries are the easy half — weapon masteries and Increased Speed / Iron Skin /
Natural Resistance map straight onto the bonus-provider totals. Warcries need a shout radius and a
monster-facing pass. Whirlwind and Leap need movement work.

### Unit 4 — Sorceress tree (v1.7.17)

30 skills, and the **best engine fit of the three**: Diablo I already has Fire Bolt, Fireball,
Fire Wall, Inferno, Lightning, Chain Lightning, Nova, Charged Bolt, Teleport, Flame Wave and
Elemental as real, working spells. Most of that page is a wiring job rather than new mechanics —
invest a point, the existing spell's level rises through `GetSpellLevel`, exactly as Holy Bolt
already does on the Paladin's page.

Cold is the known gap: this engine has no cold damage channel and no chill, so the Cold page needs
either new damage fields or honest inert rows. Decide when the unit starts, from how the Fire and
Lightning pages land.

### Unit 5 — Rogue tree (v1.7.18)

30 skills, the weakest engine fit: bow skills, javelin throws and dodge/pierce passives have the
fewest D1 analogues. The passives map onto the totals; the multishot and guided-arrow skills need
missile work. Last on purpose.

## A note on cutting the three remaining sheets

An attempt to generalize `CutPaladinTree.ps1` into a self-measuring `CutClassTree.ps1` was made and
**abandoned**: it kept failing with `[System.Object[]] does not contain a method named
'op_Subtraction'` somewhere in the band scan, and chasing a PowerShell type-coercion bug was not
worth the time against a method that already works twice over. The script was deleted rather than
committed broken.

**Use the proven two-step instead**, which is how both `Paladin Skill Tree.png` and `Gems.png` were
done:

1. Run a small inline scan (Bash/PowerShell one-off, not a committed tool) that prints the y-bands
   of non-green rows and, inside each band, the x-spans of non-green columns. Read the numbers.
2. Write those rectangles into a table-driven cut script modelled on `CutPaladinTree.ps1`, which is
   the reference implementation - including its green **spill suppression** (proportional alpha
   plus pulling the green channel down to max(R,B)), without which every white emblem keeps a
   one-pixel green halo on the window's dark fill.

Two traps the Gems unit already paid for, worth re-reading before the next cut: PowerShell's `[int]`
cast **rounds** rather than truncates (it put icons in the wrong preview row), and a green subject
on a green key cannot be separated by any threshold - it needs a pre-processed black backdrop and
the dark-mode extractor.

## Re-sweep units

The drop zone kept filling after this plan closed. Two later sweeps found three more drops, each
shipped as its own unit under the same rules.

### Unit 6 — Bard tree (v1.7.17) and the font pack (v1.7.18)

The Bard's 21 songs, seven per discipline, inheriting the aura machinery whole — one song plays at a
time, which is both what a bard does and what the Paladin's aura code already enforced. Then
`fonts-8-11.zip`, **merged rather than copied**: its README says to overlay a clean 1.5.5 checkout,
and three of its four source files are ones this fork had modified. See "Four Smaller Fonts, Merged
Not Copied" and `docs/THIRD_PARTY.md`.

### Unit 7 — Monk tree (v1.7.19)

`monk-skill-tree-package.zip`, the best-prepared drop yet: a design doc with per-rank numbers, an
icon brief, and 21 pre-cut chroma-green PNGs, so no sheet measuring was needed at all. It is the
**sixth and last** class, closing the tree system.

Its design asked for two things the grid did not have, and both were built rather than trimmed:

- A **seventh tier**, unlocking at character level 36. The first six thresholds are unchanged, so no
  existing skill moved. `ClassTreeTierCount` now names the number everywhere.
- **Per-skill rank caps** — five ranks for skills 1-6, one for each branch capstone. Added as a
  `maxRank` field where **0 means "the usual cap"**, because appending a field to a positional
  aggregate leaves 140 existing rows at zero. Read through `ClassTreeMaxRank()`, never directly.

Its layout is also the first that is not a grid: each branch is a ladder of seven, one per tier, all
in the middle column. The tier gate does the "requires the previous skill" work on its own.

## Standing rules for every unit

- Cut with the Paladin tree's script pattern; strip order = enum order.
- Anything the engine cannot do is **listed, described and inert**, never approximated — and pinned
  by a test asserting it contributes nothing.
- Every unit: build, full suite, version bump, dev report, commit, push.
