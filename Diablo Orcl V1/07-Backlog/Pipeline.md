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
| Levski's Roar - window art | Directive | Small | No | User's assets | HALF DONE at v1.8.63. The MONUMENT has its art - objects\orclroar.cel, a statue on a stepped plaza, built by tools/MonumentCel.cs and no longer borrowing the Anvil of Fury's rock stand. The WINDOW still wears the ordinary ornate border every other panel uses, which is a deliberate placeholder rather than a design. |
| The stash chest - real art | Directive | Small | No | User's assets | Back on vanilla chest3.cel at v1.8.63. Two packs have been tried and rejected: the Grand Reliquary read as a building, and the second pack's three states are drawn at visibly different scales, so the chest changes size as it opens. A shared cut box cannot fix that - it pins the frame, not what is inside it. The next pack needs its three states drawn at one scale. The engine side is intact and parked: uncommenting ApplyStashChestGraphics and its SyncObjectAnim twin, together, is the whole job. |
| Legendary power slots | Phase 6 | Medium | Yes | Unique legendary powers | Kanai's Cube has three slots below its grid for extracted powers. Levski's Roar deliberately has none until the powers exist. |
| Retire the standalone Crafting window | Directive | Small | No | menu_icons.png recut | Directive point 8 is HALF done. Levski's Roar shipped with the recipes, but the burger menu's own Crafting window was left in place, so the same recipes run from two larders - the monument's grid and the backpack. Removing the burger entry shifts every icon after it, because the row order must match menu_icons.png exactly, so the sheet has to be recut first. |
| Object-instance test harness | Content | Medium | No | | Nothing in the suite asserts against a placed object after AddObject, which is why Levski's Roar shipped unclickable through three green runs. A harness that builds town and checks placement, selectability and operate routing would have caught it. |
| Sweep the wiki for typed numbers | Content | Small | No | | LOCATED 2026-08-19, not yet derived. Eleven typed numeric claims survive in the generated pages, and every one spot-checked against source is currently CORRECT - so this is about them being typed, not wrong. The list: affixes.html "143 shipped entries" and "up to 3 prefixes + 3 suffixes"; classes.html "a Sorcerer's 250 Magic against a Barbarian's zero"; sockets.html "climbs 13% a rung", "by 20% ... capped at 60%", "fixed 3%/5% flags", "370 words"; monsters.html "+15 in Nightmare, +30 in Hell", "default 2.0, range 1.1 to 5.0"; ui.html "10 x 7 grid, ten tabs, thirteen equipment slots", "56 x 56 icon cell". Verified against gems.cpp (Hel 20, cap 60), GenRunes.ps1 (1.13 climb), playerdat.cpp (Sorcerer 250, Barbarian 0), runes_effects.inc. The remaining work is routing each through BuildWiki.ps1 so they cannot drift.
| Salvaging, and the seven materials | Directive | Medium | New file | | Break unwanted items into White Scales, Magic Powder, Rare Fibres, Unique Encrustments, Primal Vines, Ethereal Imbueities and Set Engravings. Materials are player-scoped, so they can live in their own absent-tolerant file rather than breaking the hero format. |
| Point 10 of the socket directive | Directive | ? | ? | Never stated | The nine-point socket list ended with an empty "10." It has stayed blank across four messages. |
| The 107 remaining uniques | Content | Medium | No | Needs UITYPE values on existing armour bases | 143 of 250 uniques ship with sprites. The cheap 66 of the remainder need only base-item wiring. |
| Named set drops | Content | Medium | No | | Three gaps left after the 73 rungs shipped: some set items cannot spawn, sets do not drop as sets, and there is no gold mechanic tied to them. |
| The 97 unbuilt class-tree rows | Content | Large | No | | Of 163 tree skills across six classes, 66 are implemented. The rest are listed with a red X and do nothing. |
| TRN recolour monster variants | Phase 3 | Medium | No | | Recoloured versions of existing monsters wired into per-zone rosters - the cheapest possible bestiary multiplier. |
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
| Health globes | Balance | Small | No | | DEFERRED 2026-08-19 at the user's request - "skip the health globes for our project for now". Skipped, not dropped; do not offer it again unasked. Monsters drop globes that heal on pickup, shifting part of the healing loop out of the potion menu. |
| Movement-speed affixes | Balance | Medium | Yes | | RESIZED 2026-08-19, was Small/Save-No. There is no walk-speed item power in the codebase at all - no IPL_FASTERWALK, no speed field on the player. It needs (1) a new `item_effect_type` appended before IPL_INVALID, (2) a SaveItemPower mapping audited against the delivered token, (3) affix table rows with their own level bands, and (4) a movement model: devilutionX walks in animation frames per step, not a scalar, so "faster walk" has to be expressed as a frame count and will interact with the run toggle already shipped. The save flag is Yes because a new power on an item changes what SaveItemPower writes.
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

The 1.8.3x line added: gold auto-place across the full 10x7 backpack (it had still been walking the vanilla
10x4), per-difficulty monster immunities with Torment hardening Hell's resistances rather than repeating them,
and the class trees' cast cues. v1.8.35 added the resistance soft cap: 75 is now where returns
start diminishing rather than where they stop, with a hard ceiling of 90 and a per-difficulty
penetration penalty subtracted before any cap. v1.8.36 finished the scale-variant entry: the Colossal
champion affix had shipped in Phase 3.2, and ordinary monsters can now be born Runt or Giant, derived
from the level seed rather than stored. v1.8.37 closed the sound entry: impact cues now fire from two
hooks - the missile carries its skill from CastSpell to the moment it lands, and melee skills ring in
ApplyMeleeSkillOnHit. All seven cue events in the package are now wired. v1.8.38 shipped MPQ Unit B: the burger-menu, portal and
level-up icons rebuilt as three-state strips from the drop-zone packages by toolsCutHudStateIcons.ps1. v1.8.40 shipped MPQ Unit E from hud-plate-v3.png, which - unlike the
limestone package - is drawn on the same 1505-wide source grid the layout already uses, so every slot
rect held and only PlateSrcSize.height moved.

The 1.6 to 1.8 lines took these out of the backlog: Levski's Roar with its 3x4 grid and recipe book, socket extraction, waypoints, autosave-only play, the HUD rebuild,
skill trees and respec, the run toggle, charms, item tiers, set items, sockets and gems, all 33
runes, 370 runewords, the crafting window, gambling at Wirt, ethereal items, Magic and Gold Find,
lesser uniques with affixes, and the alvl/mlvl/ilvl ladder.
