---
date: 2026-08-15
version: 1.6.8
area: Self-audit, concluded
---

# Audit Rounds Four and Five

The tail of the standing audit. Round four swept the small always-on systems (gradual healing, readied
spells, the belt guards); round five was a terminating pass over everything that remained — the
waypoint menu's geometry, the draw-only modules, the spell-table sizing.

## Fixed in 1.6.8

**A heal that outlived its hero.** Gradual healing's pending amount and tick counters are file-scope,
and nothing reset them. Drink a potion, quit to the main menu inside the ~3-second drip window, load a
*different* character: the remainder heals the new one. Small in magnitude — a fraction of one potion —
but health appearing from a previous hero's potion is exactly the kind of leak that surfaces months
later as an unreproducible report. `ResetGradualHealing()` now runs from `StartGame` beside
`ResetAutoSave()`, which exists for precisely the same reason.

This is the same species as everything else this audit caught: **session state whose lifetime nobody
had checked.** The autosave clock got a reset when it was built; the healing drip did not.

**A comment that aged out.** `readied_spells.cpp` justified its byte-packing with "MAX_SPELLS is 53" —
59 since Charge and the six Paladin skills. The safety claim survives at 59; the number was simply
from an older world.

## Audited and found sound in these rounds

| Area | What was checked |
|---|---|
| Belt slot guards | Every `SpdList` access in inv.cpp traced: the paste path receives only pre-guarded slots, the hover path filters with `IsRealBeltItemSlot` before indexing, use/refill paths are guarded at entry |
| Waypoint menu | Scroll offset clamped on every mutation; `MouseToEntry` rejects the inter-row gap and bounds the index; unlock table indexed by difficulty 0-3 into `[4]` |
| Readied-spell packing | +1 encoding keeps Null and "empty" distinct; type re-derived on load by design so an item-bound type can never resurrect without its item |
| Spell table sizing | `_pSplLvl[64]` and the `uint64_t` bitmask both hold MAX_SPELLS 59; largest packed byte is 59 |
| hud_art.cpp (995 lines) | No per-frame allocation, no per-frame file loads; all sprite lists optional-guarded |
| Draw-only modules | ui_backgrounds, ornate_border, hero_preview all guard missing art |
| Gradual healing arithmetic | `remaining / ticks` self-corrects rounding; drains rather than banks while dead; caps at max HP/mana |

## The audit in total (1.6.5 → 1.6.8)

**Ten fixes over five rounds**, from a starting point of "audit your code as i am resting":

1. Tint repainting every scripted unique (regression, 1 hour old)
2. Four quest bosses spawnable as lesser uniques (Na-Krul state corruption possible)
3. Kill log naming the borrowed champion
4. Thunderous shifting the loot RNG
5. Shift+Charge charging mana for a stationary swing (regression from the shift fix)
6. Shift+LMB over an item not casting
7. Death screen naming the borrowed champion
8. Dead players operating the full HUD menu
9. Stale density option text
10. The gradual-heal leak across characters

Two of the ten were regressions introduced by the same day's earlier fixes — both by adding a caller
without re-checking the callee's assumptions. Every one of the ten, and all four bugs the user
reported before the audit began, reduce to one sentence: **an assumption about scope or lifetime that
was true when written and quietly stopped being true.** That sentence is the cheapest review checklist
this project owns now.

## What was NOT audited

The stash and Tabbed Inventory sync internals (pack/sync/msg) — covered by `pack_test` and
`writehero_test`, which pass. Vanilla systems the fork never touched. The data-only aura and Barbarian
skill tables, which gate nothing yet.

## State

**354/356** — `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, red before
any of this work began. Four commits pushed this audit: 1.6.5, 1.6.6, 1.6.7, 1.6.8.
