---
date: 2026-08-16
version: 1.6.30
area: Megaplan Phase 0 - foundations
---

# Phase Zero, Complete

Executed autonomously overnight per the user's instruction ("execute the entire plan without
stopping as if i am asleep"). All nine foundation units landed, each built, tested (368/370
baseline held throughout, plus eleven new tests), committed and pushed separately: v1.6.25 through
v1.6.30, commits `2ad1a8b..479706c`.

## What landed

- **0.3 Named RNG streams** (`oracool/rng_streams`): an independent cosmetic generator and
  MainSeedGuard, closing the Thunderous/SpawnLoot bug class by construction. Two tests pin it.
- **0.4 Bonus-provider stat aggregation** (`oracool/stat_sheet`): CalcPlrItemVals' accumulation is
  a provider walk - per-ENTITY context (hireling-ready), condition hook (set-bonus-ready),
  equipment and rage as the first two providers. Semantics pinned by test before extraction.
- **0.1 The chunk tail** (`oracool/hero_chunks`): the hero file is PlayerPack + an optional OEXT
  tag/length/payload tail. Unknown tags skip; torn tails reject whole; pre-tail heroes load
  unchanged. First chunks: SkillPoints (Phase 2's fields, persisted from day one) and Waypoints64.
  **This is the last planned save-format growth** - future state appends a chunk.
- **0.2 Zone-ready layout**: MaxWaypointSlots 25 -> 64, once, generously. And two audit findings
  that SHRANK the plan: the queued Charge-SpellID break had already happened in an earlier session
  (id 52, MAX_SPELLS 59, both readied slots persist - memory updated), and entity caps/level state
  need no format work at all (counts are dynamic, level files are keyed by number), so new zones
  are purely additive.
- **0.5 Zone registry** (`oracool/zone_registry`): the world as a data table; GetLevelType's
  if-ladder is gone; Drlg golden tests prove nothing moved.
- **0.6 Sprite scaler** (`oracool/sprite_scale`): load-time nearest-neighbour CLX resampling that
  decodes the CL2 runs themselves - index 0 is the opaque SHADOW colour, so transparency comes
  from run structure, never a colour key (the first cut had the fill/literal alphabet backwards;
  the test caught it). Phase 3 wires it into Colossal/runt/boss variants.
- **0.7 v1 Recolor-zone mechanism**: `tools/PaletteVariant.ps1` (luminance-preserving tint
  variants of any .pal - a frost Cathedral proved it) + a paletteOverride field on zone rows,
  consulted by LoadRndLvlPal after the vanilla roll so deterministic streams never move. The full
  MIN/TIL authoring tool deliberately waits for Phase 4.2, where the AI art it serves exists -
  reserving format bytes and tools against guessed requirements is the anti-pattern the audit
  checklist exists to catch.
- **0.8 Iteration tools**: `dumpdungeon` (dPiece matrix to CSV - generated layouts inspectable
  with no client), `reloadassets` (PNG art hot-reload in seconds).
- **0.9 Balance telemetry** (`oracool/telemetry`): balance_telemetry.csv beside the saves - kills
  with time-to-kill, deaths with source, pickups with tier and value. Per-row open/flush/close so
  crashes lose nothing. Play sessions are tuning DATA now.

## The one golden-file re-baseline

Writehero's SHA256 moved (documented change #5 in its comment log): the hero blob now carries the
chunk tail. Deliberate, and - if the design holds - final.

## Next

Phase 1 (the item endgame): sockets, gems, runes, runewords discoverable in-game, charms in their
own pouch, ethereal, MF/GF, gambling, crafting. The chunk tail and the provider seam were built
for exactly this.
