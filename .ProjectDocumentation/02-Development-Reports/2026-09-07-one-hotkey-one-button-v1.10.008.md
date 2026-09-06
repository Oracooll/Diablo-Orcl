# A hotkey belongs to one skill on one button (v1.10.008)

**Date:** 2026-09-07
**Request:** "hotkeying has problem. i assigned F2 to a skill in lmb picker but that didnt remove it from the skill who used to use it in rmb picker. fix this. a hot key can only be assigned to a single skill on a single picker. hard rule."

## The fault

The 2026-08-18 rule ("one key, one skill, one button") was enforced on the SKILL side only: binding a key swept that skill off every key on both buttons (`ClearSpellFromHotkeys`), but left the KEY's other side alone. F2-left and F2-right could therefore name two different skills, and the in-play handler carried a shift tiebreak to pick between them.

## The fix

One exported routine, `BindAbilityHotkey(player, slot, spell, leftButton)` in spell_book.cpp, now does every bind from both the quick lists and the Abilities window. It first empties the key on the right button, the left button and the aura array (`ClearHotkeySlotOnBothButtons`), then sweeps the skill off every other key, then writes the one binding. The same key on the same skill on the same button still unbinds. The aura bind path uses the same slot clear. The shift tiebreak stays only for saves made before today, where two sides can still exist.

## Also

Bumping to 1.10.008 broke the build: the patch number is written with three digits and reaches C++ as a literal, where a leading zero means octal and 008 is not a number (001-007 happened to be valid octals). CMakeLists.txt now strips leading zeros from the three parts before they become macros.

## Tests

New: F2 on Firebolt right, then F2 on Healing left empties the right side; moving Healing to F3 empties F2; a spell on a key evicts an aura and the other button's skill; same key same skill unbinds. Suite 688/688.
