# Pipeline

Everything floated and not expressly discarded, and nothing that has already shipped. This file is
the SOURCE for the wiki's Pipeline page - `tools/BuildWiki.ps1` parses the table below, so editing
here is how the page changes. Add a row to add an idea; delete a row only when the user drops it,
or move it to the Shipped note at the bottom when it lands.

Columns: `Name | Group | Size | Save | Blocked | Summary`

- **Size** - Small (a session), Medium (a few units), Large (a phase of its own).
- **Save** - whether it breaks the hero/stash format. "No" means old characters survive.
- **Blocked** - what has to happen first. Empty means it can start today.

The old `Idea-Backlog.md` is superseded by this file and kept only for its history; a dozen of its
"not started" entries had shipped by 1.8.11 without being moved.

| Name | Group | Size | Save | Blocked | Summary |
|---|---|---|---|---|---|
| Levski's Roar - real art | Directive | Small | No | User's assets | The monument and its window ship on placeholders: OBJ_STAND (the Anvil of Fury's rock stand) and the ordinary ornate border. Both swap out when the real assets arrive; neither choice constrains the swap. |
| Legendary power slots | Phase 6 | Medium | Yes | Unique legendary powers | Kanai's Cube has three slots below its grid for extracted powers. Levski's Roar deliberately has none until the powers exist. |
| Retire the standalone Crafting window | Directive | Small | No | menu_icons.png recut | Directive point 8 is HALF done. Levski's Roar shipped with the recipes, but the burger menu's own Crafting window was left in place, so the same recipes run from two larders - the monument's grid and the backpack. Removing the burger entry shifts every icon after it, because the row order must match menu_icons.png exactly, so the sheet has to be recut first. |
| Object-instance test harness | Content | Medium | No | | Nothing in the suite asserts against a placed object after AddObject, which is why Levski's Roar shipped unclickable through three green runs. A harness that builds town and checks placement, selectability and operate routing would have caught it. |
| Sweep the wiki for typed numbers | Content | Small | No | | Four hand-typed tables have been found inside the generated wiki, one per audit. Worth one pass that finds the rest rather than another four audits. |
| Salvaging, and the seven materials | Directive | Medium | New file | | Break unwanted items into White Scales, Magic Powder, Rare Fibres, Unique Encrustments, Primal Vines, Ethereal Imbueities and Set Engravings. Materials are player-scoped, so they can live in their own absent-tolerant file rather than breaking the hero format. |
| Point 10 of the socket directive | Directive | ? | ? | Never stated | The nine-point socket list ended with an empty "10." It has stayed blank across four messages. |
| The 107 remaining uniques | Content | Medium | No | Needs UITYPE values on existing armour bases | 143 of 250 uniques ship with sprites. The cheap 66 of the remainder need only base-item wiring. |
| Named set drops | Content | Medium | No | | Three gaps left after the 73 rungs shipped: some set items cannot spawn, sets do not drop as sets, and there is no gold mechanic tied to them. |
| The 97 unbuilt class-tree rows | Content | Large | No | | Of 163 tree skills across six classes, 66 are implemented. The rest are listed with a red X and do nothing. |
| Cast and impact skill sounds | Content | Small | No | | The skills swing silently. 305 sounds are already in the archive. |
| MPQ Unit E - bottom HUD plate | Art | Small | No | | The 1536x1024 limestone master replacing the 356x64 middle_hud.png. Standing note: do not add a procedural bottom offset. |
| MPQ Unit B - icon refreshes | Art | Small | No | | New burger-menu, portal and level-up icons from the drop zone. |
| TRN recolour monster variants | Phase 3 | Medium | No | | Recoloured versions of existing monsters wired into per-zone rosters - the cheapest possible bestiary multiplier. |
| Scale variants and the Colossal affix | Phase 3 | Small | No | | Giant and runt monsters through the sprite scaler that already ships, plus a Colossal lesser-unique affix. |
| Per-difficulty immunities | Phase 3 | Small | No | | The resistance data exists; make it difficulty-aware so Hell and Torment demand real resistance gear. |
| Aura-carrying champion packs | Phase 3 | Medium | No | | Fanaticism and Might packs - D2's scariest idea, and cheap here because the aura and lesser-unique systems both exist. |
| Zone 1, the recolour zone | Phase 4 | Large | No | | Hellfire's own trick: new palette, retinted tileset, new roster, new waypoints, new entrance. Validates the whole pipeline with zero AI-art risk. |
| Zone 2, first generated tileset | Phase 4 | Large | No | Zone 1 proves the pipeline | The first zone built from user art through the tileset pipeline. |
| Zone quest chains | Phase 4 | Medium | No | A zone to put them in | Each new zone gets a quest in the D1 style - a voice, a horror, a reward. |
| Treasure-class drop tables | Phase 5 | Medium | No | | Zone- and boss-specific drop tables, so a particular place is worth farming for a particular thing. |
| Endgame bosses | Phase 5 | Medium | No | Treasure classes | Scaled, tinted, affix-loaded versions of existing monsters guarding the best tables. Built entirely from machinery that already exists. |
| Difficulty re-runs that mean something | Phase 5 | Medium | No | | New immunities, new lesser-affix pools and new drop tiers per difficulty, so Nightmare is not just Normal with bigger numbers. |
| Seasonal or challenge characters | Phase 5 | Medium | New file | | A checkbox at creation and a ladder file. Single-player friendly. |
| Hirelings | Phase 6 | Large | Yes | | A persistent companion off the golem framework. Equipping them is the expensive half. |
| Jewels | Phase 6 | Medium | Yes | | A socketable with rolled affixes rather than a fixed effect - the third socket family after gems and runes. |
| Set bonus system | Phase 6 | Medium | Yes | | Real set bonuses for wearing several pieces. The green text colour is already reserved for it. |
| Resistance soft cap and penetration | Balance | Small | No | | A 75% soft cap with sharply reduced returns past it, plus per-difficulty resistance penalties, so resistance gear matters at endgame. |
| Health globes | Balance | Small | No | | Monsters drop globes that heal on pickup, shifting part of the healing loop out of the potion menu. |
| Movement-speed affixes | Balance | Small | No | | The run toggle shipped; the affix that makes boots worth choosing did not. |
| Skill synergies | Balance | Medium | No | | D2-style: investing in one skill strengthens a related one, so a tree reads as a build rather than a shopping list. |
| Telemetry-driven balance pass | Balance | Medium | No | | The CSV has been collecting kills and drops since Phase 0.9 and has never been read back. Drop rates, monster scaling and the tier weights are all tunable from it. |
| Enchanting - single-affix reroll | Systems | Medium | Maybe | | Reroll one affix's value on a rare or better, at a vendor or shrine. Stays non-breaking if kept to a numeric reroll. |
| Paragon-style post-cap progression | Systems | Medium | New file | | Experience past level 99 converts into small permanent bonuses instead of being wasted. |
| Per-affix perfect-roll indicator | Systems | Medium | Yes | | Show which affixes rolled at their maximum. Needs a per-affix flag the item record does not carry. |
| Transmogrification | Systems | Medium | Yes | | Wear one item's stats with another's appearance. Needs a second cursor id per item. |
| Unique legendary powers | Systems | Large | Yes | | D3-style: a unique that changes how a skill behaves, not just its numbers. |
| Diablo 2-style shop interface | Systems | Medium | No | | A grid shop with tabs rather than the vanilla scrolling list. |
| Randomized bonus dungeon | Systems | Large | No | Zone pipeline | A Nephalem-Rift-style randomised descent built from existing tilesets and rosters. |

## Shipped, so not listed above

The 1.6 to 1.8 lines took these out of the backlog: Levski's Roar with its 3x4 grid and recipe book, socket extraction, waypoints, autosave-only play, the HUD rebuild,
skill trees and respec, the run toggle, charms, item tiers, set items, sockets and gems, all 33
runes, 370 runewords, the crafting window, gambling at Wirt, ethereal items, Magic and Gold Find,
lesser uniques with affixes, and the alvl/mlvl/ilvl ladder.
