---
date: 2026-08-16
version: 1.7.13
area: Diablo II's Paladin skill tree - three pages, twenty-nine skills
---

# The Paladin Gets His Tree

The user supplied "Paladin Skill Tree.png" - Diablo II's real Paladin tree - and asked for it in
the Abilities window with as much of D2's behaviour as this engine allows. It is in.

## The art

29 icons cut from the sheet (tools/CutPaladinTree.ps1) into `ui\paladin_tree_icons.png`, 56px
cells, strip order = enum order so the two cannot drift. Two things the cut had to handle: the
skill names are printed under each emblem and are NOT wanted (the tree draws names as text, and a
baked name can never be translated), so the crops are per-row y-bands measured by scanning for
rows containing any non-green pixel; and a flat chroma key left a green halo one pixel wide around
every white emblem, so partially-green pixels get proportional alpha AND have their green channel
pulled down to max(R,B) - standard spill suppression. The emblems now sit on the window's dark
fill with no glow.

D2's Combat page has a tenth skill, Holy Shield. The sheet has no icon for it, so it is not
listed rather than listed with borrowed art.

## The tree replaces the invented auras

The 24-aura list that shipped as data in v1.1.80 and gained effects yesterday in v1.7.12 was built
from an early art delivery and never matched Diablo II. It is gone - `oracool/auras` deleted, the
Auras sheet deleted (it was hidden behind `ClassAbilitySheetsHidden` anyway, so in practice the
v1.7.12 activation work was never reachable in play). `oracool/paladin_tree` is the Paladin's
system now.

## Gating: tiers, and an honest omission

Every skill sits in one of D2's six tiers - character level 1, 6, 12, 18, 24, 30 - and that tier
is the whole gate. D2 also has a per-skill prerequisite graph including cross-tree links. That
graph is deliberately **not** reproduced: rebuilding it from memory would mean inventing edges and
presenting them as D2's. The tiers are certain, they are D2's primary gate, and they already make
the page read as a progression. It is one field per row if it is wanted later.

## Investment: one accessor, two stores

Points come from Phase 2.1's pool. Where a tree skill has a spell slot, its points live in
`_pSkillInvestment` keyed by SpellID - so they flow into `GetSpellLevel` and therefore into every
ladder that already scales with spell level, for free (invest in Holy Bolt and the engine's own
Holy Bolt gets stronger; invest in Zeal and the strike ladder grows). The twenty auras have no
slot, so theirs live in a new `_pPaladinAuraInvestment[20]`, persisted by a new chunk (tag 5).
`PaladinTreeInvestment()` hides which store a skill uses.

Those five spell slots are resolved at CALL time, not stored in the table: they are owned by
`oracool/paladin_skills.h`, and reading another translation unit's table during this one's static
initialisation is an initialisation-order gamble.

## The pages

Three sheets in the Abilities window, Paladin only, laid out as a real grid - three columns by six
tiers, from each skill's own (tier, column) rather than from a running index, because the pages
are sparse and a running index would close the gaps and destroy the shape. A test asserts no two
skills share a cell, which is the grid's one structural invariant.

Two click targets per cell, so neither action can be taken while reaching for the other: the
**counter under the icon** spends a point into it (it turns gold and grows a "+" only while you
hold a point that cell can take); the **icon itself** readies an active skill on whichever mouse
button clicked it, or lights an aura.

## What actually works

Six of nine combat skills act: Smite (our shield bash - D2's Smite is exactly that), Zeal, Charge,
Blessed Hammer, Fist of the Heavens, and Holy Bolt, which points at the engine's own spell.
Sacrifice, Vengeance and Conversion are listed, described, and say so.

Fourteen auras act, scaling with invested points, through the Phase 0 provider seam: Might,
Holy Fire, Thorns, Blessed Aim, Concentration, Holy Shock, Fanaticism, Resist Fire, Defiance,
Resist Cold, Resist Lightning, Salvation - plus **Prayer** and **Meditation**, which regenerate
life and mana on the per-tick hook beside the engine's own drain effects, and **Vigor**, which is
D2's movement aura in an engine with no walk-speed modifier: it holds on the same double-speed
frame skip the run toggle uses, so it is the fourth consumer of that one mechanism.

Resist Cold is the one deliberate remap - no cold exists here, so it wards against magic, D1's
third resistance, and its own description says so.

Five auras are **completely inert**: Holy Freeze (no cold, no slow), Sanctuary and Conviction
(monster-facing, needs its own pass), Cleansing (no poison or curse duration exists), Redemption
(needs corpse handling). A test asserts their totals are byte-identical to no aura at all, so a
point spent there cannot be quietly reported as buying something. Their rows say "No effect yet",
and unbuilt skills draw greyed exactly like unearned ones - a bright icon that does nothing is
the lie.

An aura also refuses to light with nothing invested in it, which is what stops a free buff
masquerading as a paid one.

## Verified

**405 tests, the usual two pre-existing failures.** New pins: every skill is on exactly one page,
no two share a grid cell, investment respects class/level/pool/cap, castable skills reach
GetSpellLevel while auras do not touch the spell array, auras need a point before they burn,
effects grow with points, the five inert auras leak nothing, Vigor runs only when paid for, and
the whole aura state round-trips through the chunk tail. Golden hero hash re-baselined as
documented change #7.

## Next

Sacrifice and Vengeance are the two combat skills the engine can support without new subsystems
(the melee latch and the elemental damage channels both exist). Conviction and Sanctuary want the
monster-facing pass. Holy Shield needs art.
