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
| Sweep the wiki for typed numbers | Content | Small | No | | LOCATED 2026-08-19, not yet derived. Eleven typed numeric claims survive in the generated pages, and every one spot-checked against source is currently CORRECT - so this is about them being typed, not wrong. The list: affixes.html "143 shipped entries" and "up to 3 prefixes + 3 suffixes"; classes.html "a Sorcerer's 250 Magic against a Barbarian's zero"; sockets.html "climbs 13% a rung", "by 20% ... capped at 60%", "fixed 3%/5% flags", "370 words"; monsters.html "+15 in Nightmare, +30 in Hell", "default 2.0, range 1.1 to 5.0"; ui.html "10 x 7 grid, ten tabs, thirteen equipment slots", "56 x 56 icon cell". Verified against gems.cpp (Hel 20, cap 60), GenRunes.ps1 (1.13 climb), playerdat.cpp (Sorcerer 250, Barbarian 0), runes_effects.inc. The remaining work is routing each through BuildWiki.ps1 so they cannot drift.
| Point 10 of the socket directive | Directive | ? | ? | Never stated | The nine-point socket list ended with an empty "10." It has stayed blank across four messages. |
| The 107 remaining uniques | Content | Medium | No | Needs NEW base items, not just wiring | CORRECTED 2026-08-21 by the affix audit, and the count corrected again 2026-08-22 by counting the table: unique_items_data.inc holds 143 rows, not 253, which is also what the wiki reports. All 143 shipped uniques already have a valid base - the old "needs UITYPE values on existing armour bases" was wrong. The 107 absent ones ride bases this engine does not have at all: shoulder mantles, reliquaries, cloaks, battle cloaks, spears, pikes, war lutes, arcane foci, war quivers, canticles. So the cheap half is not cheap; each needs a base item authored first. |
| Extract legendary powers | Phase 6 | Large | Yes | Unique legendary powers | Kanai's Cube's own centrepiece - Archive of Tal Rasha - and the one recipe of the nine that could not be adopted at v1.9.17. It needs powers that CHANGE a skill rather than its numbers (the "Unique legendary powers" row), and it needs somewhere to keep which powers a character has extracted, which is save format. Pairs with the "Legendary power slots" row: the slots and the extraction are one feature split across two rows and should be built together. |
| The Sorcerer's thin tree | Content | Medium | No | New rows need icon-strip art | PARTLY FIXED at v1.9.2, from 1 live row to 4: Enchant now burns on every blow, and Fire and Lightning Mastery each grant their element's damage plus its resistance. Those three were the only rows this engine had a channel for. The other 13 stay inert honestly - TEN are the cold page and there is no cold damage type at all, and Static Field, Lightning Storm and Meteor have no analogue. A fuller tree means NEW rows, and every row needs an icon in her strip, so the rest is blocked on art rather than on code. |
| The 97 unbuilt class-tree rows | Content | Large | No | | Of 163 tree skills across six classes, 66 are implemented. The rest are listed with a red X and do nothing. |
| More monster variants | Phase 3 | Small | No | | The four that exist (Ashen, Stormtouched, Hollow, Feral) are two resistances and two body types, and Hell's roster is now all four - so the deepest floors have nothing the shallowest do not. A fifth and sixth would give the deep rosters something of their own. Any new trait must be a channel an ordinary monster already has, which is what kept the first four to resistance and life/damage. |
| Zone 1, the recolour zone | Phase 4 | Large | No | | Hellfire's own trick: new palette, retinted tileset, new roster, new waypoints, new entrance. Validates the whole pipeline with zero AI-art risk. |
| Zone 2, first generated tileset | Phase 4 | Large | No | Zone 1 proves the pipeline | The first zone built from user art through the tileset pipeline. |
| Zone quest chains | Phase 4 | Medium | No | A zone to put them in | Each new zone gets a quest in the D1 style - a voice, a horror, a reward. |
| Treasure classes for the BASE item pool | Phase 5 | Large | Yes | | What v1.9.13 deliberately could not do. A class redistributes the additive hooks - socketables and set pieces - but the base item a monster drops still comes from the seeded pool, which is save format: UnPackItem replays an item's seed through it to recover the index. Making a zone drop different WEAPONS means a per-zone pool, which means the replay needs to know which zone an item came from, which means a field on the item. That is the format bump, and it should be paid once alongside whatever else needs one. |
| Boss art and a boss health bar | Phase 5 | Small | No | User's assets | The bosses shipped at v1.9.14 wearing an existing monster's sprite scaled to Colossal, which is what kept them unblocked. What they do NOT have is a silhouette of their own or a health bar that reads differently from a champion's - both are presentation rather than mechanics, and both are the kind of thing that makes a fight feel like an event. |
| Per-difficulty ZONE identity | Phase 5 | Medium | No | | What v1.9.16 did not do. The difficulty now changes the champion pool, the variant density and the loot RATE - but every zone changes by the same multiplier, so the Caves are the rune place on all four difficulties and nothing about WHERE you farm shifts as you climb. Rotating the treasure classes' majorities per difficulty would make a re-run re-map the world rather than re-scale it. Needs the telemetry read back first: rotating what a zone gives is the kind of change that is either the best idea in the backlog or deeply annoying, and taste is not enough to tell which. |
| Seasonal or challenge characters | Phase 5 | Medium | New file | | A checkbox at creation and a ladder file. Single-player friendly. |
| Hirelings | Phase 6 | Large | Yes | | A persistent companion off the golem framework. Equipping them is the expensive half. |
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

| More milestones | Systems | Small | No | | Eight shipped at v1.9.20 and the list is the cheapest content in the game to extend - each is one condition and one enum value, and the mask has 24 spare bits. Obvious candidates: reach Torment, complete a named set, socket a six-socket item, salvage N items, kill every named unique. Worth doing once the telemetry says how fast the twenty-signet cap is actually reached. |
| PlayerPack's header comment is out of date | Directive | Small | No | | It says pfile.cpp's ReadHero "only accepts a file whose size matches sizeof(PlayerPack) exactly", and warns that anything wanting space should hunt for reserved bytes. ReadHero has accepted a chunked TAIL since 1.6.27 - `read >= sizeof(*pPack)` with the excess handed to ApplyHeroChunks - which is where new per-character state has belonged ever since. The stale comment nearly sent Phase 2 into a struct growth that would have broken every hero file. Correct it before it misdirects someone else. |
| D2MXL Phase 3: Growing charms | Systems | Medium | No | | Plan - D2MXL to ORCL, Phase 3. A charm that gains stats as milestones are met rather than being fixed at drop. Needs per-item state for the same reason orbs did - and the byte was already paid at v1.9.19, which is why this was planned then and built later rather than the other way round. Wants Phase 2's milestones to grow against. |
| D2MXL Phase 4: Named encounters | Phase 5 | Large | No | | Plan - D2MXL to ORCL, Phase 4. Median XL's uberquests: a specific hard fight, in a specific place, with a KNOWN reward. Most of the machinery landed in the v1.9.7-1.9.16 line - bosses, treasure classes, tinting, affix pools - so what is missing is the fixed-reward half and somewhere to put it. The only phase of the four that wants new CONTENT rather than new mechanism. |

## Shipped, so not listed above

**Signets of Learning and milestones** shipped at v1.9.20 - D2MXL-to-ORCL Phase 2. Permanent stat
points with a lifetime cap of 20, paid by eight one-time milestones across depth, combat and the
item systems. The point goes to the UNSPENT pool so the player chooses where it lands.

The plan said `PlayerPack`; checking that first found it cannot grow for free - its reserved bytes
were repurposed years ago and its own header records that growing it at 1.5.0 broke every character
then existing. Both values ride the hero CHUNK TAIL instead, which is tagged, length-prefixed and
forward-compatible, so the phase breaks no save and bumps no version. `Writehero.pfile_write_hero`
was re-baselined for the two added chunks, which is the deliberate kind of move that test exists to
make you notice.

**Mystic Orbs** shipped at v1.9.19 - D2MXL-to-ORCL Phase 1. Eight consumables adding one fixed small
stat to an item, capped at six per ITEM rather than per orb type, applied by dropping one onto a
backpack item through the gem paste path. They cost `OracoolItemFormatVersion` 8 -> 9, which is the
first per-item value in this fork that is not derived from a seed: a player decision has nowhere to
be recomputed from. The same bump carried `IPL_MAGICFIND`, closing the gap `IPL_GOLDFIND` closed one
axis over - magic find had been an `ItemBonusTotals` figure that only a CHARM could contribute to.

**Four of Kanai's Cube's recipes** were adopted at v1.9.17: Reforge Gear, Ennoble Rares, Recast Set
Pieces and Recolour Gems, taking Levski's Roar to nine. All four consume a SALVAGE MATERIAL, which
until then had no consumer anywhere in the game - the seven dropped, stacked, sorted into their
stash row and were never spent. Which material pays for what is deliberate: uniques fund reforging,
rares fund ennobling, set pieces fund recasting.

Three of the Cube's were skipped and the reasons recorded so they are not re-litigated: extracting
a legendary power needs powers that do not exist yet plus save storage (its own row above),
Caldesann's augments are a format bump, and Law of Kulle's level-requirement removal would make the
Hel rune pointless.

The nine recipes also forced a selection rule. "Lowest-numbered ready recipe" was fine while the
inputs were disjoint; with four recipes all eating "one item plus a reagent" it silently locked
reforge reagents out of anything socketed. It is now MOST SLOTS WINS - the monument runs the recipe
that uses the most of what you put in front of it, so putting in only what a recipe needs is how you
choose it.

**Difficulty re-runs that mean something** shipped at v1.9.16, and auditing the row first found two
of its three asks already done: immunities answered to the difficulty from Phase 3.3, and drop tiers
do so through `TierForItem`'s item-level key. What was missing was the affix pool - all six champion
modifiers were available from the first floor of Normal - plus a fourth thing the row did not name:
a treasure class is chosen by dungeon type and a re-run walks the same floors, so the Cathedral in
Torment paid exactly what the Cathedral in Normal paid. Now the pool grows per rung (Normal's three
need no gear to answer; resistances arrive at Nightmare, Vampiric at Hell), the loot rate scales
100/115/130/150, and the variant density climbs 15/19/23/28.

**Temper Jewels** shipped at v1.9.15 - a fifth recipe, three identical jewels into one of the next
grade, Radiant excluded. Its own recipe and its own `IsJewel` rather than a widened Refine Gems: the
gem recipe decomposes an index as (type, quality) and the jewel ladder is (family, grade), so a
shared one would have to work out which decomposition applied. The ladder is one addition because
the ids are grade-major, which `gems.cpp` now static_asserts rather than trusts.

**Endgame bosses** shipped at v1.9.14 - `oracool/endgame_boss.cpp`. One per floor, guaranteed from
the first Hell floor and 25% below that, built on the champion path at 800% health / 200% damage /
+10 armour / 6 escorts, always Colossal, paying 6x its floor's treasure class. Marked by one new
value on the already-saved `lesserAffix` byte (`Dread`, which is never rollable), with a SECOND
trait derived from `lesserNameSeed` - so a boss has two modifiers and still costs no save change.

**Treasure classes** shipped at v1.9.13 - `oracool/treasure_class.cpp`. Six zone tables, each giving
one socketable family a clear majority of the draw (Cathedral gems, Catacombs charms, Caves runes,
Hell jewels, Nest charms, Crypt runes) plus a socketable and set-piece rate that climbs with depth,
multiplied by what the monster is worth: ordinary 1x, champion 2x, unique 4x.

The row above records what it deliberately does NOT do and why the remaining half is a Large with a
save cost, so that limit is not rediscovered as a bug later.

**Jewels** shipped at v1.9.9 - fifteen of them, five families (Fervor, Focus, Aegis, Ruin, Warding)
across three grades at 60/100/160 percent, dropping at 1% off the socketable hook. The row asked for
"rolled affixes rather than a fixed effect" and that is deliberately NOT what shipped:
`Item::_iSocketed` stores a socket's contents as a bare `uint16_t` base index, so per-instance rolls
would have meant a new per-socket record in the item extension - `OracoolItemFormatVersion` to 9 and
a migration for every existing hero - to buy variety rather than depth. Fifteen fixed indices buy
most of that variety for **no save change at all**. If rolled jewels are still wanted they are a new
row, not this one, and they pay the format cost once and knowingly.

**Set bonuses** shipped with the 73 rungs across 15 sets: `ApplySetBonusesToTotals` feeds the same
`ItemBonusTotals` every other provider does, `ActiveSetBonus` answers how far up a ladder you are,
and `ForEachEarnedSetBonus` lets the description panel name each rung in the reserved green.

**Named set drops** closed all three of its gaps across v1.8.99-1.9.5: every one of the 94 pieces can
now spawn (the last two were re-slotted at v1.9.1 rather than given new equipment slots),
`TrySpawnNamedSetPiece` drops them at 3%, and the gold mechanic landed as a real item power -
`IPL_GOLDFIND` with `Item::_iPLGoldFind` behind it, which is what finally lets the Rat King's Court
pay 40/50/60% gold find up its ladder. What remains is narrower and has a row of its own: sets drop
as loose pieces, never as sets.

**Recoloured monster variants** shipped at v1.9.7 and their **per-zone rosters** at v1.9.11. Each
dungeon type offers a subset - the Cathedral only the two body variants, Hell all four, town none -
so a roster is something a player can learn about a place. `VariantForSeed(seed, dungeon)` is the
whole rule and reads no globals, which is what makes it testable without building a level.

**Sets drop as sets** shipped at v1.9.11, though not by making a set drop whole. The pick is
weighted instead: `1 + held*3` for a missing piece of a set you already hold pieces of, floor weight
1 otherwise, where held means worn OR carried OR stashed. A piece you already hold stays at the
floor rather than dropping to zero, so a piece sold by mistake is not gone for good. The RNG stream
is unchanged - one `GenerateRnd` call, exactly as the uniform pick made.

**Aura-carrying champion packs** shipped in Phase 3.4 and the row survived until 2026-08-21 - the
fourth stale row found that week. `oracool/aura_field.h` has the whole thing: `PackAuraFrom` maps an
affix to what it lends at a distance, `PackAuraOn` answers for one monster, a champion lends to its
neighbours but never to itself, and a dead one lends nothing. **Relentless** carries Might (a damage
percentage) and **Fortified** carries Defiance (flat armour); Vampiric, Thunderous and Colossal are
personal and lend nothing, which is what keeps the six distinct. It is consumed in four places -
`MonsterAttack`'s damage roll and three armour checks across melee and missiles.

Note the design constraint recorded there, because it kills the obvious next idea: there can be no
Fanaticism-style SPEED aura. Monster movement is paced by the animation, and animation timing lives
on the shared `CMonster` rather than the individual, so making one champion faster would make every
monster of its type faster with it.

The 1.8.7x-1.8.9x line closed **Salvaging and the seven materials** in full, and then some: the seven
materials with their own stackable items and sprites, the seven buttons on Levski's Roar, a row of
their own in the stash SORT, seven Charms of Salvaging that convert a drop at pickup, jewellery
salvaging, and a sweep across every backpack page rather than the displayed one. Also in that line:
the skill picker on the LMB/RMB wells, the Abilities window becoming points-only, and the rule that
books raise spells while points raise skills.

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

v1.8.65 shipped the object-instance test harness: test/oracool_town_objects_test.cpp, 14 tests that
build town's object pool and assert placement, selectability and operate routing on what comes out.
Both of the historical failures it targets were reintroduced deliberately to watch it go red before
it was trusted. It cannot check SPRITES - HeadlessMode short-circuits the whole graphics path - so
whether an object wears the right art is still a question only the screen answers, and that is
stated at the top of the file rather than papered over.

The 1.6 to 1.8 lines took these out of the backlog: Levski's Roar with its 3x4 grid and recipe book, socket extraction, waypoints, autosave-only play, the HUD rebuild,
skill trees and respec, the run toggle, charms, item tiers, set items, sockets and gems, all 33
runes, 370 runewords, the crafting window, gambling at Wirt, ethereal items, Magic and Gold Find,
lesser uniques with affixes, and the alvl/mlvl/ilvl ladder.
