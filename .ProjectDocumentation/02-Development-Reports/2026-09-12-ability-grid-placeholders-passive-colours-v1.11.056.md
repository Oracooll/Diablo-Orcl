# v1.11.056 - every ability page is a 3x6 grid of slots; passives are gold when earned, green when assigned

2026-09-12. The user:
- "i want you to put empty place holders (skill slot without white icon) for skills in each available skill slot on each ability tree for every hero. i want every ability tree to have 3x6 skills."
- "also make unlocked passive skills gold, and the assigned ones green."

## The grid

`panels/spell_book.cpp`.
- **`TreeGridTiers = 6`** is new. With `TreeColumns = 3`, every page (the three skill trees and Passive Skills, for every class) is a three-by-six grid.
- **Empty slots.** `DrawTreePage` first marks which (tier, column) cells hold a skill. It then draws every other cell as an empty slot: the drop shadow, the slot bezel, and `DrawClassTreeIconOutlined` with `skillIndex = -1`. That is exactly how an empty passive slot in the four-slot band is drawn: the frame, with no white icon.
- **Placeholders are drawn only.** `TreeCellAt` walks the skills, so a click or hover on an empty slot finds nothing: no tooltip, no invest, no slotting.
- **Page height.** `TotalListHeight` used to measure a page by its deepest skill. It is now never shorter than the grid, so a sparse page still shows its bottom row of empty slots.
- **The seventh tier.** Two rows stand below the grid, because `ClassTreeTierCount` is still 7: the Barbarian's **Rampage** and the Rogue's **Single Out**, each the nineteenth passive on its page. A full 3x6 page holds 18. They are left where they are for the user to decide: drop one, move it, or keep a seventh row on those two pages. `OracoolClassTree.EveryPageFitsTheThreeBySixGrid` pins that exactly these two sit outside the grid, so a new row placed outside it fails a test.

## Passive colours

- **Tree cells.** A Passive Skills row that is earned now has the **gold** plate (`Ready`), and one that is slotted has the **green** plate. It used to be light grey when earned and gold when slotted. Locked stays red. Every other page keeps its old coding.
- **Slot band.** A filled slot there is green too.

**The green is drawn as values.** `SkillPlateTint::Green` is new, and `ApplyPlateTint` sends it to `SetSpellTransGreen`. That is the value-drawn green (`SplGreenOverride` through `ClxDrawRgbMap`), because the palette has had no green since v1.11.023. Two fixes were needed for it to show:
- `DrawLargeSpellIconCentredIn`, the 56px plate the Abilities window draws, called `ClxDrawTRN` directly. On its own that would have painted the green plate as fire orange, since `PAL8_GREEN` is 152 again. It now goes through `DrawSpellSprite`, which uses the value table while green is active.
- `SetSpellTransRed` and `SetSpellTransWhite` did not clear `SplGreenActive`, while `SetSpellTrans` and `SetSpellTransDarkGrey` did. A red or white plate drawn after a green one would have kept the green's value overrides. The window draws the three colours cell after cell, so both now clear it.

`DrawSkillTintOutline`'s switch gains `Green` with the default ring, because the palette has no green to ring with.

## Verification

Debug and Release built, ctest **714/714**. **Not seen in play.** This is drawn over the world, and only a screenshot verifies it.

**To check:**
- Every tree page shows 18 cells, with empty frames where no skill is. The Barbarian's and Rogue's passive pages add Rampage / Single Out below.
- Passive Skills: earned passives gold, slotted ones green (in the grid and in the four slots), locked red.
- Other pages unchanged.
