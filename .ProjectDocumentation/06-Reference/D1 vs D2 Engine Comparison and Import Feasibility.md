---
date: 2026-08-16
area: Engine analysis / roadmap reference
---

# D1 vs D2 Engine Comparison and Import Feasibility

Requested by the user: "do a detailed comparison between d1 and d2 engines and point all the
advantages of d2 engine and lets analize which of these advantages can be introduced in this game."

The baseline here is not vanilla Diablo 1 but **DevilutionX plus everything Orcl V1 has already
built** - which matters, because a surprising number of "D2 advantages" are already in this fork.

## 1. The two engines at a glance

| Dimension | D1 (DevilutionX / this fork) | D2 |
|---|---|---|
| Simulation | 20 ticks/sec fixed-step lockstep | 25 fps server-style frame loop |
| Movement | Tile-hop: a unit occupies exactly one tile; walking animates between tiles | Finer sub-tile grid; continuous-looking paths, walk AND run |
| World | One town + sequential dungeon levels, 112x112 tiles, one level simulated at a time | Five acts, large outdoor zones + dungeons, waypoint web |
| Rendering | 8-bit palette, 256 colours, lighting = palette-shift tables | Still 8-bit palette at heart, but with translucency layers, perspective mode, 800x600 |
| Characters | 3 (+3) classes, shared spell pool from books | 7 classes, 30-skill trees each, synergies, skill points |
| Items | Base + 1 prefix + 1 suffix; magic/unique | Magic/rare/set/unique/runeword; up to 3+3 affixes; sockets, gems, runes, jewels, charms, ethereal, crafting |
| Monsters | Fixed types per level range, quest uniques | Type + variant system, random champions/uniques with affix auras, per-difficulty immunities |
| Difficulty | Normal/NM/Hell as stat multipliers | Full re-run of the world with new immunities, penalties, drop tiers |
| Economy | Gold, 4 vendors | Gold + gambling + crafting + trade + hirelings to equip |

## 2. Where this fork has ALREADY imported D2's advantages

Worth stating before planning anything new:

- **Random champion/unique monsters** - D2's proudest monster feature. Our lesser-unique system
  (affixes, tints, minion packs, scaled stats, INI-tunable rate) IS this, built 1.6.0.
- **Multi-affix item tiers** - Rare (1-2+1-2), Buffed Unique (2-3+2-3), Primal (3+3 perfect) map
  directly onto D2's rare/unique/D2R-primal ladder, with our own extension-record save format.
- **Base-item tier ladders** - the eight material product lines are D2's normal/exceptional/elite
  idea, taken further (8 tiers x 9 slots).
- **Stash + tabbed inventory** - D2's storage, exceeded (10 backpack pages).
- **Waypoint web** - 25 waypoints with a scrolling panel.
- **Convenience layer** - free Town Portal on a button and hotkey, XP percentages, autosave,
  gold cap in the hundreds of millions, item labels, sort buttons: all at-or-past D2 QoL.
- **Smooth high-FPS rendering** - DevilutionX already decouples render framerate from the 20-tick
  sim with animation interpolation; missiles already fly at arbitrary angles in fixed-point
  sub-tile steps. The "D2 smoothness" gap is smaller than memory suggests.
- **Translucency** - paletteTransparencyLookup exists and is used (tooltips, plates, ghosts).

## 3. D2 advantages NOT yet here, ranked by feasibility in this engine

### Tier A - natural fits (the machinery mostly exists)

1. **Sockets, gems, runes, runewords.** The tiered-item extension records (OracoolAffix arrays)
   are exactly the right save-format shape: add socket count + socketed-item list, a drop/insert
   UI interaction, and a runeword recipe table. The single most-loved D2 system, and the one this
   codebase is best pre-adapted for. *Medium-large effort, no art blocker (gem icons can be cut
   from existing sheets or user art).*
2. **Charms** - passive items that work from the backpack. CalcPlrInv already recomputes all
   bonuses from containers on every change; extending its scan to flagged backpack items is a
   small, sharply-scoped change. *Small-medium.*
3. **Skill points / deeper skill trees.** Per-class skills, unlock levels, per-skill scaling
   (Zeal's ladder) all exist; what is missing is player-allocated points and a tree UI. Points are
   per-character persistent state → PlayerPack, per the standing rule. *Medium. Pairs well with
   respec (trivial once points exist).*
4. **Crafting recipes (Horadric Cube style).** A recipe window is exactly the kind of self-contained
   UI this fork builds routinely (waypoint menu pattern); recipes = item-combination table.
   *Medium.*
5. **Magic find / gold find stats.** New affix type + one hook in the drop-quality roll (we already
   own that code path via TrySpawnOracoolSetItem and the lesser-affix second roll). *Small.*
6. **Ethereal items.** A flag, a stat bonus, a repair refusal - _iOracoolBroken already walks this
   exact path. *Small.*
7. **Gambling.** Wirt is halfway there; a real unidentified-purchase roll is a store-path change,
   an area already hardened by the audit. *Small-medium.*
8. **Per-difficulty monster immunities.** monstdat already carries IMMUNE_ masks; making them
   difficulty-dependent is data + one resolve function. *Small.*
9. **Run toggle (with or without stamina).** No run sprites exist in D1 art - but Furious Charge
   already proves walk-speed boosting works and reads fine. A run toggle = the same speed scaling,
   always-on, optional stamina drain bar. *Medium, art-free.*

### Tier B - possible, expensive, needs a strong reason

10. **Hirelings.** The golem proves a player-owned combat unit works (AI, targeting, save).
    A real hireling adds equipment, persistence, town interaction. *Large; the equipment half is
    the expensive part.*
11. **Bigger dungeons.** MAXDUNX/MAXDUNY and every parallel array, automap, minimap, save format.
    Doable but touches everything; and 112x112 already fits this game's pacing. *Large.*
12. **Treasure-class drop tables.** A rebuild of drop selection into layered tables. The current
    hooks work; this is a refactor whose visible payoff is tuning convenience. *Medium-large,
    mostly invisible.*

### Tier C - architecturally wrong for this engine (recommend against)

13. **Free sub-tile movement.** Everything - pathing, collision, melee range, wire commands, saves,
    every monster AI - assumes one-unit-one-tile. D2 itself is still grid-based underneath; the
    felt difference is mostly run speed and finer interpolation, which Tier A #9 buys at 1% of the
    cost. *Weeks of work, permanent regression risk, poor return.*
14. **Large seamless outdoor acts.** Art-blocked (no outdoor tileset exists) before it is even
    code-blocked. *Not practical.*
15. **True 32-bit / alpha rendering, perspective mode.** The palette renderer is load-bearing for
    every art asset and every lighting table; the green-ramp injection shows the palette can be
    taught new tricks far cheaper than replacing it. *Not worth it.*
16. **Client-server netcode.** V1 is single-player by decree; nothing to gain.

## 4. Strategic note on save formats

Tier A items 1-3 all want new per-character or per-item persistent state. The Charge-needs-its-own-
SpellID break is already queued for a fresh session. **Batch the breaks**: when that format break
lands, reserve fields for socket count, socketed items, skill points, and charm flags in the same
version bump, even if the features land later. One break instead of four.

## 5. Recommended import order

1. Sockets + gems + runewords (the crown jewel, best machinery fit)
2. Charms (small, immediately felt)
3. Skill points + respec (deepens what already exists)
4. MF/GF affixes + gambling (economy loop)
5. Crafting window (ties 1-4 together)
6. Run toggle (feel)
7. Hirelings (only if the appetite is still there)
