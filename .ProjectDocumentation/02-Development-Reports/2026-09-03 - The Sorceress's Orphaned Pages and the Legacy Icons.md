# The Sorceress's orphaned pages, and the legacy icons (v1.9.194)

Two user reports, 2026-09-03.

## 1. "his lightning and fire spells abilities screen seem orphaned"

Literally orphaned. The 2026-08-20 rule — "Spells cant be affected by skill points, only by books"
— was enforced by *filtering book rows out of the page entirely*. Thirteen of the Sorceress's rows
are book spells, so:

| Page | Cells filled before | Now |
|---|---|---|
| Lightning | 3 of 10 (Static Field, Thunder Storm, Lightning Mastery) | 10 |
| Fire | 4 of 10 (Warmth, Enchant, Meteor, Fire Mastery) | 10 |

The rows are **listed again and still take no points**, which is what the rule actually asks for.
A book row now shows:

- its **legacy icon**, green when the spell is learned and grey when it is not;
- the **spell's own level** in the rank corner, from the books, or nothing when unlearned;
- its F-key badge, so it can be bound from the page like anything else;
- a tooltip reading "Spell level N" and "Raised by books, not by skill points";
- and on a click, a red line naming the row and saying the same thing, instead of silence.

The same rows return elsewhere for free: the Rogue's Golem, the Bard's Charm and Sonic Barrier,
the Monk's Search.

Twelve row descriptions still promised "Points raise this engine's X." They now say "This engine's
X, raised by its books rather than by skill points" — the sentence and the behaviour agree again.

## 2. "use legacy icons for legacy spells everywhere"

The wells and the speedbook asked the class-tree icon strip first, for any spell with a tree row.
Since the Sorceress's book rows *are* tree rows, Fire Bolt, Lightning, Nova, Teleport, Guardian,
Mana Shield, Charged Bolt, Telekinesis, Flame Wave, Inferno and Fire Wall were drawn from the new
art in the well and from the engine's own sheet in the speedbook — one spell, two faces.

`IsLegacySpell` in spelldat.h is the split, and the boundary is the enum itself: every id below
`SpellID::Charge` shipped with Diablo or Hellfire, and Charge is the first thing this fork
appended. Nothing has ever been inserted in the middle, so it is one comparison.

Applied in three places, which is all of them: the LMB/RMB wells and the skill picker
(`TryDrawSkillSpellIcon`), the speedbook's large plate (`TryDrawSkillSpellIconLarge`), and the
Abilities page's own cells (`DrawTreeCell`). The tint ramp still colours the plate, so a readied
legacy spell keeps the well's state colour — only the picture changed.

## Tests

- `OracoolClassTree.TheSorceressBookRowsAreListedAndStillTakeNoPoints` — both pages carry an
  arsenal again, named rows are on their own pages, and a book row still refuses a point while a
  cold row still takes one.
- `OracoolSpellArt.LegacySpellsAreTheOnesTheOriginalGameShipped` — sixteen legacy spells on one
  side of the split, twelve of this fork's on the other.
- `OracoolClassTree.EveryPageIsPopulatedAndGridPositionsAreUnique` updated: every row is on exactly
  one page now, with no exceptions, and no two share a cell.

622/623, the standing baseline.

## To look at in play

Open a Sorceress's Abilities window on the Lightning and Fire pages: ten filled cells each, the
learned spells green with their book level in the corner and the unlearned ones grey. Ready Fire
Bolt to a button and check the well shows the same icon the speedbook does.
