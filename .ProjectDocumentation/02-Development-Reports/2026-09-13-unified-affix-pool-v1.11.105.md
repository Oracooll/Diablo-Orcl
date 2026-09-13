# One affix pool for every tier, and Movement Speed and Faster Cast are affixes

2026-09-13 — v1.11.105

## Why

Audit findings #8, #9, #10 and #13, after the user set the model:

> "i would like to move to D3 style. We call all possible item bonuses affixes and an item can have any
> combo of them within its limit of affixes."

v1.11.100 made the LIMIT flat. The ROLLERS were still split: a magic item rolled vanilla's one prefix
one time in four and one suffix two times in three; a Rare always got at least one prefix and one suffix
and at most two of either; and Movement Speed and Faster Cast arrived from drop-tail rolls after the
item was finished, outside every limit and with no price.

| # | Finding | Now |
|---|---|---|
| 8 | Rollers were not "any combo" | one pool for every tier |
| 9 | Movement Speed / Faster Cast lived outside the affix library | OracoolPoolRows, ordinary pool affixes |
| 10 | Drop-tail affixes always stored as suffixes, blocked by storage with budget left | drop tail removed; storage by table |
| 13 | Price ignored drop-tail affixes | pool rows priced like vanilla rows |

## The pool

`DrawUnifiedAffix` draws from the prefix table, the suffix table and `OracoolPoolRows` together, applying
once the rules both old rollers applied separately: item type, level band, only-good, the running
good/evil theme, and no power type twice. Prefix rows keep their PLDouble weighting.

**OracoolPoolRows** (items.cpp) holds the affixes that are not vanilla rows:

- Movement Speed — six level bands, 10-13% up to 25-30%, on armour, rings and amulets
- the slow curse — three bands, 10-14% to 16-20%, PLOk false so only-good rolls never take it
- Faster Cast — six bands on rings, amulets and helms (5-7% to 12-15%), six on staves (10-13% to 25-30%)

Magnitudes and item types match what the drop tail rolled. Prices follow the vanilla life rows of similar
strength; the curse is priced like vanilla curses (no value, negative multiplier).

They are NOT appended to ItemPrefixes/ItemSuffixes: those tables' indices are load-bearing — the
non-Hellfire gating in IsPrefixValidForItemType is written in index ranges, the staff prefix roll picks by
index, and RepairOracoolAffixesIfCorrupted walks them.

## Counts keep their odds

- **Magic**: 2 affixes one time in six, otherwise 1 — vanilla's both-halves odds. The only-good coin is
  unchanged, so a magic item is as likely to be cursed as ever.
- **Rare**: 2 guaranteed, plus two 30% chances — 2, 3 or 4, as before.
- **Buffed Unique**: 4 guaranteed, plus two 30% chances — 4, 5 or 6.
- **Primal**: 6, all at maximum.

Guaranteed affixes still ignore level limits, so a ring in a narrow level window still gets its minimum.

## What "any combination" cannot yet mean

Storage is still three prefixes and three suffixes, **by the table each affix came from**, for two reasons:

1. `RepairOracoolAffixesIfCorrupted` runs on every load and checks the prefix array against the prefix
   table and the suffix array against the suffix table. A suffix-table affix stored as a prefix could be
   "repaired" into a wrong value.
2. Widening either array is an item format change, and the loaders compare that version for exact
   equality — a bump would reject existing hero items, stash and tabs.

So a Rare may now roll three prefixes and one suffix, none and three, or Movement Speed beside two
suffixes — but not four from one table. A Primal's six are still three and three.

Magic items keep table affixes in the vanilla `_iPrePower` / `_iSufPower` pair (in either slot, whichever
table they came from) and pool-row affixes in the record, because the loader re-derives Movement Speed
and Faster Cast from the record alone.

## Removed

`TryAddMovementSpeedToDrop`, `TryAddFasterCastToDrop`, their calls in FinalizeFreshDrop, the unused
`OracoolHasFreeAffixSlot`, and the per-table `SelectRarePrefixCandidate` / `SelectRareSuffixCandidate`.

## A cause found on the way

`GetItemPower` lives inside an anonymous namespace in items.cpp. Exporting it through items.h made every
call ambiguous — the same thing that happened to the Smart Loot pool helpers in v1.11.103, whose cause
was not pinned down then. The definition is now closed out of that namespace.

## Also in this change

**Staves are named from the pool (#14).** `GetStaffPower` still built vanilla's "{Prefix} {Staff} of {Spell}",
the last magic name made out of its affix. It now takes a pool name when a prefix rolls; the unidentified
name keeps the spell, and the tooltip lists it.

**The shop summary line lists pool affixes.** A magic item's one-line affix summary in a store read only the
vanilla pair. Store items never got the old drop-tail rolls, but they can roll Movement Speed and Faster
Cast from the pool now, and those live in the record — so the line now appends the record's entries too.

## Tests

Rewritten or added:

- `RareItemTest.UnifiedAffixes_AnyCombinationWithinTheLimitAndMovementSpeedIsAnAffix` — over 3000 Rare
  helms: 2-4 affixes, never more than 3 per table, at least one roll with a table left EMPTY and one with
  THREE from one table (both impossible under the old per-slot guarantee), Movement Speed and Faster Cast
  both reachable; a Rare sword never rolls either; magic helms stay at 1-2 affixes, the record agrees with
  `_iPLMoveSpeed`, a magic helm carrying Movement Speed has a price, and only-good rolls never take the curse.
- `OracoolAffixBudget.EveryTierRollsWithinItsAffixLimit` — replaces the hand-set budget test with real rolls:
  4000 magic helms at the reported item's level band, and 1500 each of Rare, Buffed Unique and Primal.
- `OracoolAudit.MovementSpeedIsAPoolAffixKeptInTheRecord`, `MovementSpeedCurseRollsAndReadsBelowTheWalk` and
  `FasterCastRateIsAPoolAffixKeptInTheRecord` — the three drop-tail tests, now over real magic rolls, with the
  same ranges and item-type rules.
- The Rare and Buffed Unique per-slot guarantees became totals (at least 2, at least 4); the Primal 3+3
  guarantee is unchanged and still holds, because storage forces it.

## Golden data re-baselined, and why each is legitimate

Magic items are rebuilt from their seeds by the replay, and the replay now rolls different affixes, so
three pieces of golden data moved. None is a save-format change.

- **Pack corpus** — 81 rows for the new magic rolls, then 7 more when staves took pool names. Regenerated by
  a temporary dumper that printed only the rows whose unpacked item differed, restored before commit.
- **`NetPackTest.UnPackNetPlayer_invalid_iBonusAC`** — tampered with the chest's armour percent by `++`. The
  bonus is the item's AC times its percent over 100, floored, and a magic item's bonuses only count once
  identified; the new roll made that +1 invisible. The test now identifies the item, checks that alone still
  validates, and adds a full 100% — exactly the item's own AC — so it tests tampering, not rounding.
- **`Writehero.pfile_write_hero`** — eight item-derived totals on the packed Rogue (Strength 124→104, Dexterity
  281→260, damage mod 101→91, HP 16640→13824, resistances 89/16/90 → 48/46/0). The golden file hash did NOT
  move: the hero blob carries seeds, not rolled stats. All three resistances are now below the soft cap, so
  this assertion no longer exercises it — `OracoolAudit.ResistanceReturnsDiminishPastTheSoftCap` does.

## Worth knowing

- **Loot changes.** Magic items now take prefixes about as often as suffixes, and Movement Speed and Faster
  Cast are as common as any other pool type rather than a flat one-in-twelve — rarer on some items, more
  common on Rares. That is the D3 model working, but it is a balance change to watch in play.
- **Existing items are untouched.** Single-player saves store full records, so everything already found keeps
  its affixes (decision D4), including any item that was over its limit.
- Two PowerShell traps cost failed builds tonight and are now in memory: here-strings drop the final newline,
  and `String.IndexOf` in PowerShell 5.1 is culture-sensitive with `\n` — it left orphan braces after four
  spliced tests. Every later splice used ordinal regex.

## Proof by reverting

Each claim was reverted on its own and the tests had to catch it.

**Pool rows switched off** (Movement Speed and Faster Cast never offered):

```
Movement Speed never rolled on a Rare helm - it is not in the pool
Faster Cast never rolled on a Rare helm - it is not in the pool
Movement Speed never rolled on a magic helm
Movement Speed never rolled on a magic ring
Faster Cast never rolled on a magic staff
Faster Cast never rolled on a magic ring
```

**A magic item allowed a third affix:**

```
a magic item rolled 3 affixes
a magic helm rolled over its limit on roll 0
```

That second proof took three attempts, and the first two are worth recording:

1. Run together with the first revert, the affix-limit test *passed* — not a gap, but the two reverts
   interfering: with pool rows off, a magic item's third draw had nowhere to go (both vanilla slots full,
   the record's only candidates gone), so it could never exceed two.
2. Run alone, both tests crashed with `0xc0000409` (stack buffer overrun) instead of failing an assertion.
   `GetItemPower` kept picked types in a two-slot array, so a third pick wrote past its end. Production
   never asks for three, so this was not a live bug — but it was a rule held up by an accident.

So `GetItemPower`'s loop is now bounded by the array as well as by the count, as the tiered roller's
`apply()` already was, and the proof widened the array to let a third affix through cleanly.

## Verification

- Debug: **733 tests, 0 failed** — after the hardening, on the source being committed.
- Release built and linked; `DiabloOrcl RTM\DiabloOrcl.exe` refreshed.
- No save format change; no asset change, so no MPQ repack.

## Not in this change

- **#16, shop rows showing only the name.** A shop row draws the name and the price on one line with no
  explicit truncation, so adding the base type ("Rotting Bane (Great Helm)") could run into the price.
  Layout over the world cannot be verified without a screenshot, so this is left as a proposal rather
  than shipped blind.
- **Stale comments.** A handful of comments still describe Movement Speed and Faster Cast as drop-tail
  rolls. Fixing them touches itemdat.h, which rebuilds almost everything, so they follow in a separate
  cleanup commit.

## What to look at in play

1. Magic items: some should now carry two prefixes or two suffixes, and Movement Speed or Faster Cast
   beside an ordinary affix — never more than two affixes.
2. Rares: some should have three affixes of one kind and none of the other.
3. A magic staff should show a two-word name once identified, and "Staff of <Spell>" before.
4. In a shop, a magic item carrying Movement Speed should list it on its summary line.
