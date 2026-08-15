---
date: 2026-08-15
version: 1.5.50
area: Spell descriptions, hover popup, hover outline
---

# Fifty-One Sentences Nobody Wrote

Third slice of the abilities batch: the hover description panel and the gold hover outline, plus the
content both needed and did not have.

## The content that did not exist

> I guess all spells have description, so we need a big enough pop-up window with description when i
> hover over the spells.

They do not. The engine has no spell description text of any kind - no field on `SpellData`, no
table, nothing. The original game gave you a name, a mana cost and a spell level and left the rest to
the printed manual, and this fork never added any. The auras, Barbarian skills and Charge/Zeal all
read as though spells must have them too, but those three sets were authored deliberately in earlier
passes.

So the popup could not be built until fifty-one sentences existed. They are now in
`oracool/spell_descriptions.cpp`, and **they are written, not recovered**. Each was checked against
the missile the spell actually fires in this codebase rather than against memory of the game - which
is how the Hellfire three came out accurately rather than plausibly:

| Spell | What it really does |
|---|---|
| Mana | restores a random amount, scaling with character AND spell level, doubled for a Sorcerer |
| the Magi | fills mana to maximum outright |
| the Jester | rolls a random spell from a table of ten |

Three entries deliberately promise nothing. Doom Serpents, Blood Ritual and Invisibility carry
`MissileID::Null` in both slots, so casting them does nothing at all; they say "An unfinished
incantation. Nothing answers it yet." rather than describing an effect the player can immediately
test and find missing. They are in the book on the user's explicit call with substance to follow.

The table is indexed positionally by `SpellID` with a `static_assert` on its size, so a spell added
to the enum without a sentence here fails the build rather than showing blank.

## One question, answered once

The outline and the popup both need to know which row the cursor is on, and answering that twice
would be two chances to disagree - particularly on the Skills sheet, whose rows are not all the same
height. `DrawHoverFeedback` walks the rows exactly as the draw loop does, accumulating heights, and
feeds both.

Draw order is deliberate and split: the outline is drawn BEFORE the rows, into the clipped content
subregion, so it frames the icon and text rather than striking through them and so it cannot escape
the scrolling area. The panel is drawn into the full surface at the end, because it must cover
whatever it overlaps and must be free to extend past the window's edges.

`DrawHoverPanel` sizes itself to the WRAPPED text rather than to a fixed box, so a one-line spell and
a three-line one both look deliberate, and flips to the other side of the row when there is no room
to the right - it never leaves the screen and never covers the thing it describes.

The outline colour is `MidHighlightColor` (198), the lit gold already sampled from the window's own
frame. `OuterColor` (202) is the more literal reading of "subtle", but these rows sit on a
half-transparent panel over the dungeon and 202 barely separates from the fill.

Applied to every Abilities sheet and to the waypoint list, which was the other list the user named.

## Changed

- `Source/oracool/spell_descriptions.{h,cpp}` - new. Fifty-one authored sentences.
- `Source/oracool/ornate_border.{h,cpp}` - `DrawHoverOutline`, `DrawHoverPanel`.
- `Source/panels/spell_book.cpp` - `DrawHoverFeedback`.
- `Source/oracool/waypoint_menu.cpp` - the outline on its hovered row.

## Verified

- Build clean, `static_assert` confirms 52 description slots against `SpellID::LAST + 1`.
- **352/354** - the standing baseline.
- **Not seen in play.** Neither the panel's placement nor the outline's weight has been looked at on
  screen, and both are the kind of thing that wants looking at.

## Still open

The common yellow icon background - the last item of the batch. It needs the vanilla spell-icon
plate extracted before the 38x38 strips can be composited onto it.
