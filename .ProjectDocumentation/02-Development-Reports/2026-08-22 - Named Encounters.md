# Named Encounters

**Version:** v1.9.22 -> v1.9.23
**Date:** 2026-08-22
**Phase:** D2MXL-to-ORCL Phase 4 — the last of the four

## The estimate was wrong, in the good direction

Phase 4 was scoped **Large, blocked on art**, because "a specific place" sounded like the tileset
pipeline. Checking the engine before writing anything found that wrong three times over:

- **`SL_ARENA_CHURCH`, `SL_ARENA_HELL`, `SL_ARENA_CIRCLE_OF_LIFE`** are set levels that already load
  from shipped `.dun` files, in three different tilesets, each with an exit trigger already wired
  back to town.
- **Entry is two lines** — `setlvltype = ...; StartNewLvl(...)` — already proven by `/arena`.
- **Populating one is what every quest set level does**: `AddMonsterType`, then place. And
  `InitMonsters` runs on set levels — only the *scatter* half sits behind `if (!setlevel)`, which
  turned out to be exactly the hook this needed.
- **The boss already existed** at v1.9.14.

The first risk in the plan was "are the arenas multiplayer-gated?" They are not: the gate is a
policy check inside `TextCmdArena`, the text command. The three places the level machinery is
arena-aware — cutscene selection, the exit trigger, the death-drop rule — are all structural.

So: **no new art, no tileset, no DRLG work.**

## What shipped

Three encounters, each a sealed destination:

| Encounter | Arena | Reward |
|---|---|---|
| The Sunken Chapel | Church, Cathedral tiles | Chapel Reliquary — +60 life |
| The Ring of Mourning | Circle of Life, Catacombs tiles | Mourning Token — +18% fire and lightning resist |
| The Ember Vault | Hell arena, Hell tiles | Ember Seal — +40% magic find |

A **Sealed Map** drops, is used in town, and is **consumed** — so how often you can run an encounter
is bounded by drops rather than by a cooldown nobody can see. The floor holds **one Dread boss and
its escort and nothing else**: the encounter is the fight, and a room of ordinary monsters would
dilute the one thing the player came for. The reward is **guaranteed**, because the map said what it
pays and a roll would make that a lie.

**The "known reward" lives on the map's own description** — it names the encounter it opens *and*
the charm its guardian carries. Cheaper and clearer than wiring the quest log, and a player who has
never seen one still knows before they spend it.

The rewards are charms, so they obey the **three-charm active cap**. That is what makes three
signature rewards a decision rather than three free bonuses — each is big in one stat and offers
nothing else, so carrying one costs a slot rather than filling one.

## The bug writing the test caught

**The Sealed Maps were not excluded from the seeded droppable pool.** That pool is save format —
`UnPackItem` replays an item's seed through the same walk to recover its index — so three new items
would have joined it and re-routed every existing item's recreation.

The reward charms were covered by `IsOracoolCharmIdx`; the maps answered to **no family predicate at
all**. The exclusion is one OR-chain, and a new family that forgets to join it fails silently and
catastrophically. The test now asserts every encounter item answers TRUE to at least one predicate,
which is the shape that would have caught it.

## Two things found while wiring

- **`UseItem` cannot open a map.** It is handed the misc id, not the item, and three maps share one
  misc id. The dispatch lives in `UseInvItem` beside the signet's cap gate — which is also where the
  refusal has to be, since that function consumes the item *after* `UseItem` returns.
- **`PrintItemOil(char iDidx)` is misleadingly named** — it has switched on `_iMiscId` and covered
  every misc item for a long time. It now takes the `Item`, because a map's line must name *which*
  encounter it opens. That also removed a latent narrowing: the parameter was `char`, one appended
  `IMISC_` value away from silently wrapping.

## The honest limit

Three rooms with one fight each is the right first version and it is **not** Fauztinville. A real
uberquest *area* still needs the zone pipeline, which is genuinely blocked on art. This is the
mechanism landing early, in the cheapest place that can hold it.

Icon strip 489 -> 495 frames; `oracool.mpq` repacked.

## What to look at in game — this one especially

Almost none of this is testable. A Dread boss must drop a map; the map must open the right arena in
the right tileset; the boss must actually appear in a room with no authored spawn points; the exit
must return you to town; and the reward must land at its feet.

**The arena entering at all, in single-player, is the assumption everything rests on.**
