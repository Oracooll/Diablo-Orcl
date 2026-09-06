# Skills, combat, spell-capacity, and skill-picker audit

Snapshot: Diablo Orcl v1.9.304, source `c659976d69906489c34c28c636b477f14f424bb6`

## SKL-01 - every skill with SpellID 64 or greater has effective rank zero

Priority: P1  
Confidence: confirmed  
Scope: 62 IDs, 64 through 125

### Evidence

- `MAX_SPELLS` is 126 in `Source/spelldat.h:21`.
- Legacy learned-book levels remain `Player::_pSplLvl[64]` in `Source/player.h:436`.
- Tree investment correctly has 126 entries: `Player::_pSkillInvestment[MAX_SPELLS]` at `Source/player.h:444`.
- `Player::GetSpellLevel` at `Source/player.h:787-797` rejects a spell when its numeric ID is greater than or equal to `sizeof(_pSplLvl)`. That is 64.
- The function returns before reading the 126-wide `_pSkillInvestment` array.

The boundary is visible in the current enum/table:

- IDs 59-63 (`IceBolt`, `IceBlast`, `GlacialSpike`, `FrostNova`, `Blizzard`) work.
- ID 64 (`FrozenOrb`) is the first broken entry.
- IDs 65-67 are the new cold armours.
- IDs 68-77 are Rogue arrow skills.
- IDs 78-94 are class melee skills.
- IDs 95-117 include warcries, songs, and related class skills.
- IDs 118-125 include late javelin/spear/corpse skills.

### Player-visible split brain

`ClassTreeInvestment` reads `_pSkillInvestment` directly at `Source/oracool/class_tree.cpp:1134-1145`, and `InvestClassTreePoint` increments it at `1179-1187`. The skill tree and `WellRank` (`Source/oracool/attack_skills.cpp:93-101`) can therefore display the invested rank correctly.

Mechanics use `GetSpellLevel`:

- Generic queued spells capture it at `Source/player.cpp:410`.
- Rogue arrows read it at `Source/oracool/rogue_arrows.cpp:155-179`.
- Melee skills read it at `Source/oracool/melee_skills.cpp:84-107` and clamp to at least 1, making affected skills behave permanently as rank 1.
- Warcries read it at `Source/oracool/warcries.cpp:113-120` and likewise behave permanently as rank 1.
- Mana pricing calls it through `GetManaAmount` at `Source/spells.cpp:175-177`.

Thus a player can spend several points and see the higher rank in the UI while duration, damage, radius, mana scaling, or other effects remain at rank 0/1.

### Repair

The book-level store and the tree-investment store need separate bounds:

```cpp
const size_t index = static_cast<size_t>(spell);
if (spell == SpellID::Invalid || index >= MAX_SPELLS)
    return 0;
const int bookLevel = index < std::size(_pSplLvl) ? _pSplLvl[index] : 0;
return std::max<int>(_pISplLvlAdd + bookLevel + _pSkillInvestment[index], 0);
```

Confirm the design choice that global `+skill levels` applies to new tree skills. The existing implementation/commentary implies that it should.

### Regression tests

- Explicit boundary cases at ID 63, ID 64, and ID 125.
- A naked player with one point in Frozen Orb must return effective level 1.
- Rank 3 Rogue arrow, melee skill, and warcry must each feed rank 3 into their mechanic and mana calculation.
- Iterate every active class-tree row with a real SpellID; after one investment, effective level must be positive and equal to the intended rank modifiers.
- Ensure legacy books below 64 and item `+spell level` still compose correctly.

## NET-01 - multiplayer player packets overrun `_pSplLvl`

Current priority: P3 because multiplayer is hard-disabled  
Future priority: release blocker before multiplayer is re-enabled  
Confidence: confirmed static memory-safety defect

- `PlayerNetPack::pSplLvl` has `MAX_SPELLS` (126) bytes at `Source/pack.h:153`.
- Player has only 64 bytes at `Source/player.h:436`.
- `PackNetPlayer` loops to 126 at `Source/pack.cpp:293-294`, reading 62 bytes beyond `_pSplLvl`.
- `UnPackNetPlayer` loops to 126 at `Source/pack.cpp:608-609`, writing 62 bytes beyond `_pSplLvl` into adjacent Player fields, including new skill state.
- Packet paths remain wired through `Source/multi.cpp:361-365` and `857-899`.
- `oracool::MultiplayerEnabled()` is currently constant false at `Source/oracool/oracool.h:50-53`, so normal released play cannot reach this today.

`NetPackTest.UnPackNetPlayer_valid` passing does not make the copy safe. An intra-object overwrite can evade AddressSanitizer, and that test has no canaries on fields adjacent to `_pSplLvl`.

Repair by copying only `std::size(player._pSplLvl)` unless the Player representation is deliberately widened with save migration. Add a `static_assert` tying packet and source extents if equality is required, and tests that seed all adjacent fields before pack/unpack and assert they do not change.

## SKL-02 - a future book-enabled ID above 63 would make the per-tick validator index out of bounds

Priority: P3 hardening  
Confidence: confirmed latent invariant

`ValidatePlayer` runs every player tick (`Source/player.cpp:3384`). Its loop at `Source/player.cpp:1626-1632` iterates to `MAX_SPELLS` and indexes `_pSplLvl[b]` when `GetSpellBookLevel(b) != -1`. All current IDs above 63 have book level -1, so the dangerous access is short-circuited today. A future high-ID book spell would silently make this an ordinary per-tick out-of-bounds read/write.

Make the clamp loop range over `std::size(_pSplLvl)`, or explicitly split legacy learned books from tree skills. Add a table invariant test: no book-enabled spell may live beyond the learned-book store unless the representation is widened.

## UI-01 - F-key assignment can use the previous frame's hover

Priority: P3  
Confidence: static/timing-dependent; add an event-order regression test

`HoveredPickerSpell` and `HoveredPickerAura` are described as the values “as of the last draw” in `Source/oracool/skill_picker.cpp:40-49`. They are cleared and recomputed only during drawing at `435-552`. The F-key handler consumes those cached values at `Source/panels/spell_book.cpp:1396-1424`.

If the mouse moves from skill A to skill B and an F-key event is processed before the next draw, the handler can bind A. Moving off a cell and pressing immediately can also bind the last cell instead of doing nothing.

Prefer a shared hit-test/layout function that resolves the current `MousePosition` during the key event. If keeping cached draw state, update it on mouse motion and invalidate it before input dispatch when layout/scroll changes.

Regression test: draw with A hovered, move the current mouse position to B without drawing, dispatch F1, and assert B is bound. Repeat after moving outside all cells and after scrolling.

## Clean checks in this area

- Thirty skill-point/class-tree focused tests passed.
- Three cold/chill/art tests passed.
- Three Rogue-arrow/melee/warcry mapping and mechanic tests passed.
- The skill diagnostic export had 142 rows, all with five columns and no duplicate full rows: Barbarian 38, Bard 8, Monk 22, Paladin 20, Rogue 36, Sorceress 18.

These clean checks validate table shape and many baseline mechanics; they do not exercise the effective-rank boundary at 64 or monster-slot reuse.
