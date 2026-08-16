---
date: 2026-08-16
version: 1.7.17
area: The Bard's skill tree - a fifth class, and a correction to the Paladin's Conversion
---

# The Bard Gets Her Songs

A re-sweep of the MPQ drop zone turned up `Bard Skill Trees.png`, and unlike the four D2 sheets
this one is the **user's own design**, with the effects written out on the sheet itself. That
removed the whole guessing problem the other trees had: nothing here was reconstructed from
memory.

Three disciplines of seven — Melody, Harmony, Poetry — so 21 skills rather than 30. The tree
machinery took a fifth class without any structural change; only the enum grew.

## The cut

The Bard's sheet is laid out unlike the others: its icon strip is three green panels, one per
discipline, each holding two rows (four icons then three), and **every icon has its name printed
underneath in the same white as the icon**. No colour test separates a label from an emblem, so the
crops came from a vertical ink profile instead — the emblem bands are the tall dense runs at
y 707-803 and y 868-969, and the label bands fall in the gaps between them and are excluded by
construction. Six band lines in the cut table rather than three, ordered so the strip comes out in
enum order.

## Songs are auras, and that was free

The Bard's working skills are songs, and a bard plays one song at a time. That is exactly the rule
the Paladin's aura machinery already enforces, and that machinery was written generic over
`ClassTreeSkill` rather than over the Paladin — so the Bard's songs became `Kind::Aura` and
inherited one-at-a-time activation, the invest-before-you-light rule, and the gold ring on the
burning row, with no new code. The generalization paying for itself a second time.

Eight of twenty-one act:

- **Melody of Life** and **Inspiration** regenerate life and mana on the per-tick hook. They are
  the Bard's Prayer and Meditation, so they share those branches rather than repeating them.
- **Battle Hymn** sharpens aim and blow; **Song of Fortitude** raises armour and all three wards;
  **Tale of Heroes** lends strength and dexterity.
- **Song of Swiftness** quickens strikes and stride — the fifth consumer of the run toggle's frame
  skip, which is now how three different classes get their movement skill.
- **Sonic Barrier** rides Mana Shield: a barrier that drinks the damage meant for you is the same
  idea under a different name.
- **Charm** rides Berserk (see below).

The rest are inert and say so: the monster-facing ones (Dirge of Dread, Shout, Discord, Daze,
Weaken), the ones needing machinery that does not exist (Lullaby has no sleep state, Ode to Glory
needs corpse handling, Resonance and Echoing Song need carry-over and duration), and the three
page masteries, which have no per-page channel to deepen.

The descriptions say "and allies". This is single-player, so in practice that means you — worth
knowing before the numbers look small.

## A correction: Conversion was never impossible

Wiring the Bard's **Charm** turned up that this engine's `Berserk` spell sets `MFLAG_GOLEM` on its
target — the monster becomes a player-side minion and gets a damage boost. That is precisely
"turns a monster to fight for you".

Which means the Paladin's **Conversion**, shipped in v1.7.13 with the row "Not yet built: this
engine has no charmed-monster state", was wrong. The state exists; I had not looked. That row now
rides Berserk too and is marked implemented. The claim was checkable and I did not check it — the
inert-row rule only stays honest if the reasons behind it are true, so this is worth recording
rather than quietly fixing.

## Verified

**408 tests, the usual two pre-existing failures.** The grid invariant test now walks all five
trees, which is what caught the Bard being in the enum before it was in the page builder — 140
skills accounted for, none on two pages, no two sharing a cell.

## Next

The font pack (`fonts-8-11.zip`), whose licence the user has approved. It must be **merged**, not
copied: its README assumes a clean 1.5.5 checkout, and its `ui_flags.hpp` would revert the colour
bits this fork added.
