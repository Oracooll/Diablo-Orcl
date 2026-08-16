# The Whole Ladder, Not Just the Rung You Are On

**Version:** 1.7.52
**Date:** 2026-08-16
**Files:** `Source/items.cpp`, `Source/control.{h,cpp}`, `Source/oracool/item_sets.{h,cpp}`, `Source/oracool/cursor_tooltip.cpp`

---

## What was asked

> set affixes to be in green font. when hovering over set item i want to see below its standard affixes the following: 1. all set items listed, the missing ones in red. 2. against every set item i want to you to list in brackets with white text the type of item it is (ring, boots, helm, etc...) 3. bellow the list of items in this set i want you to list all set affixes 4. i want you to list them in order of appearance (affix on 2 collected, affix on 3 collected, affix on 4 collected, etc.) missing affixes in red.

And then, after the first mockup:

> almost spot on. Regullar affixes to be in blue.

---

## The point of it

Before this, hovering a set piece told you what that one object does. That is what a tooltip does for every other item in the game, and for every other item it is the whole story — a rare ring is a rare ring, and there is nothing else to know.

A set piece is not a whole object. It is one sixth of one, and the five you do not have are the interesting part. The old panel could not say that: it named the item, listed its stats, and stopped, leaving the player to go and look up the set somewhere the game does not have.

So the panel now carries the set's entire state. Not "you have earned Procession Unbroken" — that is the rung you are standing on, and it tells you nothing about the climb. The whole roster, the whole ladder, and colour doing the work of saying which parts are yours.

---

## The four colours

| Colour | Means |
|---|---|
| **Blue** | this piece's own affixes — the ordinary `ItemAffixColor` every other item's rolled stats use |
| **Green** | set content you have: the set name, pieces worn, rungs earned |
| **Red** | set content you do not: pieces missing, rungs unreached |
| **White** | the slot word in brackets — a label, not a value |

The blue is the correction. My first pass made the piece's own affixes green too, on the reasoning that everything on a set item is "set content". That is wrong and the user caught it: green has to *mean* something, and if it means "on a set item" it means nothing, because you are already looking at a set item. Green now means **have**, red means **have not**, and the item's own stats — which you unambiguously have, since you are holding the thing — sit in the same blue as every other item's stats in the game. The eye learns one rule instead of two.

---

## The two-run line

Requirement 2 asked for a line with two colours in it: the piece name in green or red, the slot in white brackets. `InfoString` is drawn one colour per line — `InfoStringLineColors` was built for exactly that and no further.

Rather than generalise it into a run-list, this adds the minimum the requirement actually needs:

```cpp
/** Byte offset at which a line's WHITE tail begins; 0 means the whole line is one run. */
extern std::vector<uint16_t> InfoStringLineTailStart;

void AddPanelStringSplit(std::string &&str, UiFlags color, size_t tailStart);
```

One head, one white tail, one offset. Every existing producer records 0 and draws exactly as before — the value is not even a special case in the renderer, since `tailStart > 0 && tailStart < line.size()` is false for all of them.

The offset is taken **before** the bracket is appended:

```cpp
std::string name = StrCat("  ", _(piece.name));
const size_t tailStart = name.size();
name = StrCat(name, " (", _(oracool::SetSlotDisplayName(piece.slot)), ")");
```

which is what makes it survive translation. Measuring the split by counting back from the end would break the first time a language put the bracket somewhere else; measuring it at the moment the head is complete cannot.

The renderer measures rather than assumes:

```cpp
const Rectangle tailArea { lineArea.position + Displacement { GetLineWidth(head), 0 }, ... };
```

The head is a translated item name. Its pixel width is only knowable from the font that is about to draw it.

---

## The roster and the ladder

Two new queries in `item_sets.cpp`, both small enough to state in full:

```cpp
bool IsSetPieceWorn(const Player &player, const SetItemDefinition &piece);
const char *SetSlotDisplayName(string_view slot);
```

`IsSetPieceWorn` compares `_iCurs`, because on this system **the icon is the identity** — every one of the ninety-four items has a frame of its own, so recognising a piece needs no field on `Item` and no save-format change.

`SetSlotDisplayName` exists so the bracket is player-facing rather than raw JSON: `torso` reads as **body**, `main_hand` loses its underscore. Twelve slot words, `N_()`-marked, translated at display.

The ladder is read **straight off the set's rungs**, not through `ForEachEarnedSetBonus`:

```cpp
for (int i = 0; i < set->bonusCount; i++) {
    const oracool::SetBonusDefinition &rung = oracool::ItemSetBonuses[set->firstBonus + i];
    AddPanelString(fmt::format(fmt::runtime(_("  ({:d}) {:s}")), rung.pieces, _(rung.name)),
        rung.pieces <= worn ? UiFlags::ColorOracoolGreen : UiFlags::ColorRed);
}
```

That is deliberate and it is the one place this display does **not** reuse the walk that applies the stats. `ForEachEarnedSetBonus` visits the earned rungs — that is its whole job, and it is the right function for anything that needs to agree with what is actually granted. This list wants the **unearned** ones too, which is a different question, and asking it through a function built to answer the other one would have meant inverting it at the call site. The earned/unearned test itself is still the single comparison `rung.pieces <= worn`, identical to the one in `ApplySetBonusesToTotals`, so the two cannot disagree about where the line falls.

---

## What it looks like

```
Vhal's Censergrip                       green
set item                                green
Armor: 6   Dur: 30/30                   white
+10 to dexterity                        blue
chance to hit: +8%                      blue
quick attack                            blue
fire hit damage: 1-4                    blue
Required: 25 Str                        white

Vestments of the Ashen Saint (3/6)      green
  Vhal's Blackened Halo (helm)          green + white
  Vhal's Emberguard (body)              green + white
  Vhal's Censergrip (gloves)            green + white
  Vhal's Girdle of Coals (belt)         red + white
  Vhal's Pilgrim Treads (boots)         red + white
  Censer of Saint Vhal (main hand)      red + white
  (2) Warmth of the Reliquary           green
  (3) Procession Unbroken               green
  (4) Cinderbrand                       red
  (5) Ashen Intercession                red
  (6) The Saint Walks Again             red
```

Eleven lines of set state under seven lines of item. Which is the right proportion — the item is the small part.

---

## Note on an inert rung

`(4) Cinderbrand` is a single `proc:` and does nothing in this engine yet. It is still **named** and still turns green at four pieces, because the player earned it. That is the inert-row rule the class trees established, applied here: listed, honest, never a bright number that does nothing.

---

## Verification

- Debug build clean at 1.7.52.
- Full suite: **438 of 440**. The two failures are the standing baseline pair (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`), unchanged in count and identity.
- Mockup rendered and confirmed by the user before the code was finalised, then re-rendered with the blue correction.

---

## Still open

Unchanged from `2026-08-16 - Item Sets - Continuation Plan.md`, and deferred at the user's instruction:

- 21 of 94 items have no droppable base (11 amulet, 8 ring, 1 relic, 1 cloak) — the first two want `IDI_ORACOOL_RING` / `IDI_ORACOOL_AMULET`.
- No set item drops from anything yet; `giveitemset {1-15}` is the only way in.
- 71 stat keywords resolve to nothing and contribute nothing.
