# v1.11.059 - a gold ! over the townspeople who have something for you

2026-09-12. The user, after the Lazarus staff turned out to need Cain rather than Pepin: "Add a gold glowing ! over the heads of who i should speak to."

## What it is

`Source/oracool/quest_marks.{h,cpp}`. A gold **!** floats over any townsperson who would actually do something if spoken to right now.

- **The glow is a pulse, not art.** A four-step triangle every half second brightens the gold to whitegold and lifts the mark a pixel with it. Nothing was drawn, imported or packed.
- **Placed on the head**, from the sprite being drawn: centred on its width, ten pixels above its top.
- **Drawn with the towner** (`scrollrt.cpp`'s town branch), so whatever hides a towner's head hides the mark too, rather than floating it over a wall.

## Who gets one, and when

Whether a towner has news is only ever known by RUNNING its talk function - each one starts a quest, takes an item or plays a speech as it decides. So `TownerHasQuestNews` mirrors each function's own gate, in the same order, with the towners.cpp line beside it.

| Towner | Marked when |
|---|---|
| Ogden | the Skeleton King's word (after level 2 or 4), or Ogden's Sign - given, or the banner in hand |
| The dying townsman | he has not told you about the Butcher yet |
| Griswold | the Magic Rock or the Anvil - given, or the item in hand |
| Adria | the fungal tome, the mushroom, or the brain and the elixir |
| Pepin | Poisoned Water (given, unlogged, or the ring owed), or the brain for the elixir |
| Gillian | the Cathedral Map, before the Crypt is entered |
| **Cain** | **the Staff of Lazarus in your pack**, and the last word after Lazarus falls |
| Lester | the rune bomb, or the Auric Amulet owed |
| The complete nut | either suit, the bomb, or any of his three teases left |
| Celia | Theodore, or her first ask |
| Wirt, Farnham, the cow | never - they carry no quest |

Cain's row is the user's own case: picking up the staff does nothing until he is told, and now the town says so.

## Tests

`OracoolQuestMarks.TheMarkFollowsTheTalkFunctionsGate` pins the pair - change the gate and the mark must follow. The dying townsman before and after he speaks; Cain with no staff, with the staff, after he has taken it, and once Lazarus is dead; Wirt never.

## Verification

Debug and Release built, ctest **716/716**. **Not seen in play** - this is drawn over the world, so only a screenshot confirms the mark's size and place.

**To check:** walk into town with the Staff of Lazarus. Cain should carry the mark and Pepin should not.
