# Third Audit of the 1.8.x Loot Mechanics

**Version:** 1.8.7
**Date:** 2026-08-19
**Tests:** 445/447 (the two standing baseline failures)

A third pass, deliberately aimed at ground the first two did not cover: the persistence side of the
1.8.x changes rather than the generation side.

One finding. Everything else checked out.

## B1 - FIXED: a stale tab file could misparse silently

The item record grew twice this version - the ilvl byte (format 5) and the base-tier byte (format 6).
Three containers embed item records, and each is versioned:

| Container | Guard | Verdict |
|---|---|---|
| Hero save (`heroitems`) | `OracoolItemFormatVersion` on the record | Correct |
| Stash | Version 6 embeds the item format AND `IsStashSizeValid` checks the total against today's record size, before any item is read | Correct - a stale file is rejected with a message |
| Inventory tabs | Version 3 embeds the item format... but version **2** was still accepted and parsed with today's format, with no total-size guard | **Was wrong** |

A version-2 tabs file would drift two bytes per item through the stream and read later tabs as
garbage. The per-item validation and the count/grid plausibility checks bound the damage, but nothing
detects it - it is the one silent-misparse path left in the save code.

Version 2 is no longer accepted. Unlike the stash, this costs nothing: an unreadable tab file simply
leaves the extra tabs empty, which is the same thing an absent file does.

Practically unreachable for you - the file is rewritten by every autosave and you start new heroes -
but it is the kind of thing that only stays unreachable until it isn't.

## B2 - checked, correct

- **Stash rejection path**: `IsStashSizeValid` computes the expected size from *today's* record and
  runs BEFORE any item is read, so a stale version-5 stash fails cleanly with its red message rather
  than misparsing.
- **Band boundaries**: ilvl 24 lands in Normal, 25 in Nightmare; floor 24 of Torment is exactly
  `MaxAreaLevel` (96) and maps to block 3. No off-by-one at any of the four seams.
- **Field widths**: `_pSplLvl`, `_pSkillInvestment` and `_pClassTreeInvestment` are all `uint8_t` and
  the new cap of 98 fits with room; `_iOracoolItemLevel` tops out at 96 in a byte.
- **Quest set-levels**: `CurrentAreaLevel` maps each of the four to its quest's own `_qlevel` through
  the same ladder, so a lair is as deep as the floor it hangs off, on every difficulty. An
  unrecognised set-level falls to floor 1 - the least rewarding answer, which is the right default.

## Files

- `Source/loadsave.cpp` - version 2 tab files rejected
