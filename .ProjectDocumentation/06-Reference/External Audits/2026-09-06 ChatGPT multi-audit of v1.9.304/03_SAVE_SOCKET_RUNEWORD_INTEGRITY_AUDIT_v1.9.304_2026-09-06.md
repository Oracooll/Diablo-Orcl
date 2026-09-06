# Save, socket, and runeword integrity audit

Snapshot: Diablo Orcl v1.9.304, source `c659976d69906489c34c28c636b477f14f424bb6`

## SAV-01 - valid socket entries beyond the declared count remain active

Priority: P2 robustness/exploit hardening  
Confidence: confirmed  
Normal creation reachability: not found; reachable through a malformed, damaged, or edited save

### Evidence

An Item stores a declared socket count plus six physical entries:

- `_iSocketCount` and `_iSocketed[MaxItemSockets]` are at `Source/items.h:737-739`.
- `LoadItemData` clamps the count at `Source/loadsave.cpp:455-457`.
- It then validates all six stored IDs at `Source/loadsave.cpp:458-470`, but it does not clear valid IDs in positions at or beyond the declared count.
- `Item::socketedCount()` scans all six entries at `Source/items.h:750-759`.
- Worn socket bonuses scan all six at `Source/oracool/stat_sheet.cpp:116-129`.
- Hel/Zod and other socket effects scan all six in `Source/oracool/gems.cpp`, including lines `462-491`.
- Tooltips report the all-six filled count at `Source/items.cpp:6248-6254`.
- Levski extraction counts and returns all six at `Source/oracool/crafting.cpp:789-821`.
- Save writes the declared count and all entries again at `Source/loadsave.cpp:1389-1392`, preserving the inconsistency.

### Concrete malformed record

Set `_iSocketCount = 1` and store six valid gem/rune IDs in `_iSocketed`. Loading accepts all six. The item can display `Sockets: 6/1`, receive all six effects, satisfy a runeword sequence using data outside the declared range, and yield all six stones when freed.

### Repair

After loading and validating the array:

1. Clamp `_iSocketCount` to `MaxItemSockets` as now.
2. Set every entry with index `>= _iSocketCount` to `EmptySocket`.
3. Prefer making every reader iterate only `[0, _iSocketCount)`, with the count bounded locally.
4. Normalize before saving too, so an in-memory inconsistency cannot be persisted.

Regression cases: count 0 with six valid IDs; count 1 with six valid IDs; count 6; count 255; invalid IDs both inside and outside the declared range. Assert tooltip count, bonuses, runeword activation, Zod/Hel behavior, extraction output, and round-trip bytes.

## SAV-02 - invalid spell IDs can produce an undefined shift

Priority: P3 save hardening  
Confidence: confirmed primitive, crafted-save reachability

`GetSpellBitmask` at `Source/spells.h:72-80` computes:

```cpp
const int index = static_cast<int8_t>(spellId) - 1;
return SpellMask { 1ULL << index, 0 }; // whenever index < 64
```

`SpellID::Null` makes `index == -1`; `SpellID::Invalid` is negative as well. Shifting by a negative count is undefined behavior.

Most callers validate or use constants. The save path does not fully guarantee that:

- `LoadPlayer` reads raw `_pRSpell` and `_pRSplType` at `Source/loadsave.cpp:566-567`.
- `IsReadiedSpellValid` at `Source/spells.cpp:49-65` calls `GetSpellBitmask(_pRSpell)` when the type says Scroll or Charges.
- `CalcPlrInv` calls `EnsureValidReadiedSpell` at `Source/items.cpp:3822-3826`.
- `LoadPlayer` invokes `CalcPlrInv` at `Source/loadsave.cpp:755`.

A malformed pair such as `Invalid + Scroll` can therefore reach the shift before the selection is cleared.

Make `GetSpellBitmask` total over its enum input: return an empty mask unless the numeric ID is in 1..`MAX_SPELLS - 1`. Also validate the readied ID/type pair on load. Add runtime tests for Null, Invalid, negative casts, `MAX_SPELLS`, and the largest valid ID.

## SAV-03 - full game-load player bounds are trusted before use

Priority: P3 robustness  
Confidence: confirmed validation omissions; requires damaged or deliberately rewritten save payload

- `_pClass` is read directly at `Source/loadsave.cpp:597`.
- If `_pBaseToBlk` is zero, that class immediately indexes `PlayersData` at line 611 with no range check.
- `_pNumInv` is read directly at line 692 with no clamp to `InventoryGridCells`.
- `CalcPlrInv` runs at line 755, and ordinary loops later use `_pNumInv` as the bound (for example `Source/player.cpp:1602` and `Source/inv.cpp:2192`).

The compact hero/network unpacker has many explicit bounds tests, but this full save loader does not mirror them. If archive authentication guarantees these bytes can never be corrupt, document that precondition. Otherwise reject or normalize the class and inventory count before any table/array access.

Tests should mutate one decoded field at a time: class -1 and 127; inventory count -1, 41, and `INT_MAX`; then require a controlled load failure or safe clamp, never an array access.

## RW-01 - active runeword derivation omits the documented base-quality invariant

Priority: P3 defense in depth  
Confidence: confirmed contract mismatch; normal generation currently prevents most cases

`Source/oracool/runewords.h` documents a runeword as a fully socketed, plain normal-quality, non-tiered item. `GetActiveRuneword` at `Source/oracool/runewords.cpp:97-121` checks item existence, socket count, host type, fill count, and rune order, but never checks:

- `item._iMagical == ITEM_QUALITY_NORMAL`
- `item._iOracoolTier == OracoolItemTier::None`

`TrySocketGem` at `Source/oracool/gems.cpp:730-743` also checks only the held stone family and whether the target has an open socket.

Ordinary drop/crafting rules make a magic or quality-tiered socket host difficult to create today, so this is not reported as a normal duplication/exploit path. It is still safer for the derived truth function itself to enforce its contract, especially for old/corrupt saves and future crafting paths.

Do not reject `_iOracoolBaseTier`: Normal/Nightmare/Hell/Torment base tiers are a separate feature and are documented as eligible. Reject magical quality and `_iOracoolTier`, not the base-tier field.

Add tests that the same valid rune sequence activates on a plain host but not on magic, rare, set, buffed-unique, or primal hosts. Decide whether insertion itself should be refused or merely never activate a word; keep tooltip/crafting behavior consistent.

## Known persistence design risk, not a new finding

The project backlog already records that stash and hero are separate archives. A failure between writing them can duplicate an item transferred across that boundary. This audit did not relabel that accepted backlog item as new. A journal/transaction ID shared by hero and stash remains the robust long-term solution.

## Clean checks

- The live save directory had nonzero hero/stash files and no temporary/backup residue at inspection time.
- Seventeen focused gem/runeword tests passed.
- Archive verification and generated runeword-table checks were clean.
- The findings above concern inconsistent or adversarial boundaries that current happy-path tests do not construct.
