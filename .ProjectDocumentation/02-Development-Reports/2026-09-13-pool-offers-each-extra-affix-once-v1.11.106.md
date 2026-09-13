# The pool offers each extra affix once, so depth no longer floods loot with Movement Speed

2026-09-13 — v1.11.106

## Why

An overnight audit after v1.11.105 MEASURED how often the new pool affixes appear, instead of assuming
it. A temporary test rolled 20,000 magic and 20,000 Rare items per case:

| ilvl | item | magic Move Speed | magic curse | magic Faster Cast | Rare Move Speed | Rare Faster Cast |
|---|---|---|---|---|---|---|
| 10 | helm | 5.22% | 0.00% | 5.24% | 13.16% | 13.19% |
| 10 | ring | 5.29% | 0.00% | 5.51% | 13.59% | 13.96% |
| 10 | staff | — | — | 4.67% | — | 12.96% |
| 30 | helm | 7.26% | 1.15% | 7.16% | 14.65% | 14.40% |
| 30 | ring | 6.91% | 1.24% | 6.84% | 14.60% | 14.70% |
| 30 | staff | — | — | 5.83% | — | 14.79% |
| 50 | helm | **15.01%** | 2.35% | **15.63%** | **18.86%** | **18.67%** |
| 50 | ring | **20.83%** | 3.08% | **20.92%** | **20.78%** | **20.57%** |
| 50 | staff | — | — | **13.99%** | — | **18.83%** |

For reference, the removed drop tail was flat: roughly 6% Movement Speed, 2% curse and 8% Faster Cast on
an eligible drop, at every depth.

The v1.11.105 report called this "a balance change to watch". That undersold it: at item level 50 one
magic ring in five carried Movement Speed, and a fifth of Rares did at every depth.

## The cause was the banding, not the model

`DrawUnifiedAffix` offered every eligible row. Each pool type has six level bands, so:

- **At depth**, the window [ilvl/2, ilvl] admits only the deepest vanilla rows, but two of each pool type's
  bands. The pool's share of a shrinking candidate list grew.
- **On Rares**, the guaranteed picks ignore level limits entirely, so all six bands of each type competed
  at once - six candidates for Movement Speed against one or two for a typical vanilla type.

## The fix

**One candidate per pool power type per draw**, at the strongest band the item's level reaches - or, for a
guaranteed pick below every band, the gentlest one. Each type now competes like a single vanilla row
however many bands it has, and a deeper item still gets the stronger band.

## After the fix

The same measurement, same seed:

| ilvl | item | magic Move Speed | magic curse | magic Faster Cast | Rare Move Speed | Rare Faster Cast |
|---|---|---|---|---|---|---|
| 10 | helm | 5.22% | 0.00% | 5.24% | 5.12% | 5.08% |
| 10 | ring | 5.29% | 0.00% | 5.51% | 5.16% | 5.41% |
| 10 | staff | — | — | 4.91% | — | 4.71% |
| 30 | helm | 3.81% | 1.21% | 4.01% | 4.68% | 4.48% |
| 30 | ring | 4.01% | 1.29% | 3.62% | 4.12% | 4.13% |
| 30 | staff | — | — | 3.27% | — | 4.67% |
| 50 | helm | 8.82% | 2.63% | 9.33% | 7.82% | 7.62% |
| 50 | ring | 12.57% | 3.62% | 12.43% | 8.38% | 8.48% |
| 50 | staff | — | — | 7.60% | — | 7.33% |

Rares fell from 13-21% to 4-8%, and deep magic items from 15-21% to 8-13% - back around the drop tail's
6-8%. Some depth effect remains, because the vanilla candidate list really does shrink at item level 50;
magic rings there are the high point at 12.6%. That is within reach of the old design rather than a flood,
but it is the number to watch in play.

The curse is still absent at item level 10: its bands start at 1, 12 and 30, and the window [5,10] holds
none. The drop tail rolled it at every depth. Left as is - a shallow item escaping a curse is harmless.

## Golden data

The candidate list changed, so seeds pick differently again:

- **Pack corpus**: 13 rows regenerated through the temporary dumper; 61/61 pack tests pass.
- **`Writehero.pfile_write_hero`**: the packed Rogue's totals moved again - Strength 104→97, Magic 80→103,
  Dexterity 260→276, damage mod 91→93, HP 13824→17408, Mana 14624→16800, magic and fire resist 48/46→0/0.
  The file hash did not move. All three resistances are still below the soft cap.

## Also in this change

Stale comments describing Movement Speed and Faster Cast as drop-tail rolls were corrected in itemdat.h,
items.cpp and two test headers. Mentions of the drop tail for Magic Find, Gold Find, sockets and ethereal
are still accurate and were left alone.

An audit of the name pool against every authored item name found no collisions: none of the 4,096 pool
names equals a unique, set or base name, and no authored two-word name is built from pool words alone.

## A test made deterministic

The first full run failed only in `oracool_audit_test_shuffled`: the v1.11.104 rebuild test expected
Awaken to keep the ethereal bargain, and in that shuffled order the rebuilt Primal rolled "of the ages" -
Indestructible. `MakeItemEthereal` declines an indestructible item, correctly: ethereal's whole price is
durability, and such an item has none to pay. The test now seeds its RNG and asserts that rule explicitly
- ethereal kept, unless the result is indestructible, in which case it must NOT be ethereal.

## Verification

- Debug: 733 tests - 732 passed on the full run, and the one shuffled failure passes after the test fix,
  in normal order and in two further shuffled runs of the whole audit suite.
- Release built and linked; `DiabloOrcl RTM\DiabloOrcl.exe` refreshed.
- No save format change; no asset change.
