# Reachability audit of every added item

**Date:** 2026-08-21 (against v1.8.99)
**Question:** of the hundreds of items this fork has added, which can a player actually obtain?

Not "does it exist in the table" - that was answered before. This asks whether each one has a path
into a player's hands, and at what depth.

## Verdict: the item families are healthy. One set is broken.

### What is reachable

| Family | Count | Path | Deepest gate |
|---|---:|---|---:|
| Worn types x material tiers | 134 | `TrySpawnOracoolSetItem`, 8% | 60 |
| Gems | 35 | `TrySpawnOracoolGem`, 3% | 29 |
| Runes | 33 | same hook, 2% | 60 |
| Charms (stat + salvage) | 15 | same hook, 1%, and vendors | 25 |
| Oracool uniques | 143 | the ordinary unique roll on vanilla bases | UIMinLvl 38 |
| Named set pieces | 92 of 94 | `TrySpawnNamedSetPiece`, 3% (new today) | by piece level |
| Salvage materials | 7 | salvaging only, `IDROP_NEVER` **by design** | n/a |

The ceiling is `MaxAreaLevel` = **96** (24 floors x 4 difficulty blocks), so every gate above is
comfortably reachable. The deepest runes need banded 60, which means Hell or Torment - a design
choice, not a gap.

### The pool-exclusion rule holds completely

The seeded droppable pool is save format: growing it re-routes every seeded item recreation. Of
**214** Oracool rows, **207** are droppable and **every one** is excluded from that pool and routed
through its own hook instead. Zero uncovered. The seven salvage materials are `IDROP_NEVER` and never
enter it at all.

I expected to find the salvage materials leaking into the pool - they shipped after the exclusion
line was written, and nothing names them there. They do not: `IDROP_NEVER` is tested first.

### Two ceilings checked and sound

- `MaxUniqueItems` is **512** with a `static_assert`, against 253 rows. The old vanilla trap - 128
  written into three unrelated places - is properly fixed.
- No Oracool unique rides a quest-only base. All 143 sit on droppable vanilla bases; the 10 vanilla
  uniques that do ride `IDROP_NEVER` bases are the quest drops, which is correct.

---

## THE ERROR: Leoric's Fallen Court cannot be completed

**Thirteen pieces, two of which cannot be built** - a `relic` and a `cloak`, slots this fork has never
implemented. So the most a player can wear is **11**.

**Its top bonus rung requires 13.** That rung is unreachable by construction, and nothing in the game
says so. Every other set is complete now that rings and amulets resolve (73 -> 92 buildable, today).

This is the only genuinely broken item content the audit found.

---

## Plan

### 1. Leoric's Fallen Court - pick one

| Option | Cost | Consequence |
|---|---|---|
| **A. Re-band the ladder** - move the top rung from 13 pieces to 11 | Tiny, data only | Set becomes completable today. Two designed items stay unobtainable. |
| **B. Re-base the two pieces** onto slots that exist (relic -> amulet, cloak -> torso) | Small | All 13 obtainable. Two items wear a slot the designer did not choose, and the set gains a second amulet/torso, which the distinct-piece rule handles but the fantasy does not. |
| **C. Build the relic and cloak slots** | Large - new equip locations, paperdoll positions, `ILOC_` values, art | Correct, and see the synergy below. |

**Recommended: A now, C later.** A is a one-line data change that makes fifteen of fifteen sets
completable; C is real work that should be scheduled on its own merits, not as a bug fix.

### 2. The synergy worth knowing before scheduling C

The 107 absent uniques need ten base families that do not exist: shoulder mantle, **reliquary**,
**cloak**, battle cloak, spear, pike, war lute, arcane focus, war quiver, canticle.

**Reliquary and cloak are the same two slots Leoric's Fallen Court needs.** Building them once
serves both - it completes the last broken set AND unlocks a share of the 107. That makes C
considerably better value than it looks in isolation, and it is the argument for doing it properly
rather than re-basing.

### 3. Not errors, but decisions waiting

- Sets drop piece by piece; there is no "drops as a set" mechanic and no gold mechanic tied to them.
- The deepest runes require Hell or Torment. Intended as far as I can tell, but never stated.
- **Latent:** `CheckUnique` does `if (!IsUniqueAvailable(j)) break;`, and `IsUniqueAvailable` is
  `gbIsHellfire || i <= 89`. The fork forces Hellfire so the break never fires - but launched with
  `--diablo` it would silently drop all 143 Oracool uniques, and the `break` means it cuts by index
  rather than per item. Worth a comment at minimum.
