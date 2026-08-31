---
date: 2026-08-19
version: 1.8.8
area: Sockets v2 - points 1-4 and 9 of the nine-point directive
---

# Sockets Are Measured, Not Guessed

First unit of the Sockets v2 directive. The design for all nine points is in the vault
([[Sockets v2 - 33 Runes, Footprint Sockets, and the Runeword Table]]); this ships the four that
were fully specified, plus the colour correction.

## The rule that replaced three rules

An item's socket ceiling is now **the number of 28x28 inventory cells it occupies**. That is the
same footprint the backpack grid already uses to place the item, which is the point: a cap derived
from `GetInventorySize` cannot disagree with what the player is looking at, whereas the old flat
cap of 3 was a guess that happened to fit the three launch runewords.

| Footprint | Cap | Hosts |
|---|---|---|
| 1x1 | 1 | rings, amulets |
| 1x2 / 2x1 | 2 | gloves, boots, belts, bracers, small weapons |
| 2x2 | 4 | shields, helms, shoulders, most one-handers |
| 2x3 | 6 | body armour, two-handers, staves |

`Item::MaxItemSockets` 3 -> 6, and the fixed item extension record grows six bytes with it. That is
a real format break rather than a tail extension, so `OracoolItemFormatVersion` goes to **7** and an
older save is rejected on load. `StashVersion` deliberately stays at 6: version 6 already embeds
the item-format byte and checks it, which is exactly the case that rule was added for - the two no
longer have to be remembered together.

## Two exclusions removed, one kept

**Base tier no longer disqualifies.** The old rule refused any item carrying a Nightmare/Hell/
Torment tier, which made the deeper base strictly worse raw material than a Normal one. That is
backwards: the deeper base is the better host. Tier and sockets are independent axes, exactly as
tier and quality already were.

**Jewelry is in.** Rings and amulets have no basic versions at all - a ring is magic or better by
construction - so "basic only" meant "never" for them. They now socket at any quality. Being 1x1
they take exactly one, which keeps a socketed ring a choice rather than a second equipment slot.

**Quality stays exclusive everywhere else.** A magic sword has already been rolled on; letting it
socket too would leave the white sword with no role, and the white sword being the raw material is
the whole design.

## The count roll

Weights are declared for the full six - **60 / 25 / 8 / 4 / 2 / 1** - then truncated at the item's
own cap and renormalised. The renormalisation is the part that matters: without it a 1x2 glove
would roll against all six weights and every draw above two would clamp down onto two, turning a
60/25 split into something closer to 60/40. The weights are parsed out of items.cpp by the wiki
generator, so retuning them retunes the page.

The roll still happens where it always did - on the drop paths, after `SetupAllItems`, never inside
the seeded replay. That is the drop-pool lesson and nothing here touched it.

## Point 9: primal is orange

The wiki's quality table claimed gold for Primal and for Buffed Unique. `Item::getTextColor` says
otherwise: Primal is **orange** (true cyan was never available - `UiFlags` is a fully packed 32-bit
flag enum with no free bit, and no cyan `.trn` ships in the game data, so orange stands in and
happens to match Diablo 3's Primal Ancient convention) and Buffed Unique is **whitegold**. Only the
vanilla unique table is gold. Two new tag colours render it correctly.

## Verified

**448 tests, the usual two** (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`,
`Timedemo.WarriorLevel1to2`).

`OracoolGems.SocketsOnlyOnPlainEquipment` failed on the first run, correctly - it pinned the tier
exclusion this change removes. Rewritten to pin the new behaviour, and joined by
`SocketCapIsTheItemFootprint`: a 2x3 plate at six, a 1x1 ring at one, jewelry socketing at magic,
and every cap inside the record's width.

One process note worth recording: a build invoked from a fresh PowerShell call has no MSVC
environment and does nothing, while ctest happily re-runs the previous binaries and reports a
result that looks real. The environment, the build and the test run have to be one invocation.

## Still open in this directive

- **Points 6 and 7** - the 33 runes and the runeword table - are designed but unbuilt. Note for
  whoever picks it up: `IsOracoolRuneIdx` is a single range check (`EL..SOL`), and item indices are
  positional save format, so the 28 new runes append at the end and that predicate becomes two
  ranges, the same shape the Luck/Greed charms already needed.
- **Points 5 and 8** - extraction and the crafting move - are blocked on Levski's Roar, which does
  not exist yet and whose recipe table was to be approved first.
- **Point 10** of the directive is empty. Nothing assumed for it.
