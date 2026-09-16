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
| Legendary power slots | Phase 6 | Medium | Yes | Unique legendary powers | Kanai's Cube has three slots below its grid for extracted powers. Levski's Roar deliberately has none until the powers exist. |
| Sweep the wiki for typed numbers | Content | Small | No | | SWEPT 2026-09-16: every numeric claim in the prose was checked against the source. One was wrong - the monument's recipe count, typed as "seventeen" on four pages while CraftingRecipeCount has been 18 since socket extraction joined the list. Seven more were right but typed, and now come through data.js: the name pool (64 x 64), the loot-level bonuses, the monster combat offsets, Torment's multiplier, Hel's cap, the ethereal bonus and the runeword count. Still typed, and still correct: the rune ladder and its 13% value climb, the socket footprint table, the tier openings, the HUD's pixel sizes, the monument's town tile, and the 59 Diablo II runewords - which history.html's 1.8.11 row contradicts at 61. The older note follows. LOCATED 2026-08-19, UPDATED 2026-09-13. Numeric claims in the hand-written wiki prose are still typed rather than derived. affixes.html lost its two when the page was rewritten for the unified affix pool at v1.11.106, but that rewrite added new ones - the affix limits, the 4,096-name pool - and mechanics.html's Smart Loot section types four drops in five. The older list remains: classes.html's 'a Sorcerer's 250 Magic against a Barbarian's zero', sockets.html's rune climb, Hel cap, flag rates and '370 words', monsters.html's '+15 in Nightmare, +30 in Hell' and 'default 2.0, range 1.1 to 5.0', and ui.html's grid and icon-cell sizes. Each needs routing through BuildWiki.ps1 so it cannot drift. |
| Point 10 of the socket directive | Directive | ? | ? | Never stated | The nine-point socket list ended with an empty "10." It has stayed blank across four messages. |
| Extract legendary powers | Phase 6 | Large | Yes | Unique legendary powers | Kanai's Cube's own centrepiece - Archive of Tal Rasha - and the one recipe of the nine that could not be adopted at v1.9.17. It needs powers that CHANGE a skill rather than its numbers (the "Unique legendary powers" row), and it needs somewhere to keep which powers a character has extracted, which is save format. Pairs with the "Legendary power slots" row: the slots and the extraction are one feature split across two rows and should be built together. |
| The 18 inert class-tree rows | Content | Medium | No | | RE-COUNTED 2026-09-13 from the wiki's own data: 144 of 162 tree rows are implemented - the inert-skill plan that closed at v1.9.191 took it from 66, and cold damage made the Sorcerer's ice page live. The 18 left wear a red X: Barbarian - Double Throw, Throwing Mastery, Spear Mastery, Increased Stamina; Sorcerer - Static Field, Thunder Storm, Meteor; Rogue - Decoy, Poison Javelin, Plague Javelin; Bard - Epic Solo, Resonance, Echoing Song, Perfect Harmony, Ode to Glory, Legendary Ballad; Monk - Reed in the Wind, Counterstroke. Each wants a mechanic this engine does not have yet rather than wiring. |
| The 66 inert passives | Content | Large | No | | RE-COUNTED 2026-09-13: 42 of the 108 Diablo III-style passives are live, and all of them have their glyph art - every icon shipped at v1.9.261, which retired this row's art blocker. The 66 still inert are Paladin 11, Barbarian 8, Sorcerer 11, Rogue 10, Bard 16, Monk 10. The original row's warning stands for the ones it named: the resource passives (wrath, fury, hatred, discipline, spirit, arcane power) have no resource to modify here and need a substitute or the red X they wear today. |
| Centralize readied-skill mutation in setters | Systems | Medium | No | | Twenty sites across eight files assign _pRSpell / _pLRSpell directly, including load and character-creation paths where scheduling a save would be actively wrong. v1.9.58 wired the four USER-ACTION sites the fourth audit named, so the "Auto Save on Skill Change" option now means what its name says - but the pattern that let those four drift is still there, and the next assignment site will be a coin flip again. Real setters that own the assignment AND the persistence would end it; the care needed is telling a player action apart from a load.
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
| Skill synergies | Balance | Medium | No | | D2-style: investing in one skill strengthens a related one, so a tree reads as a build rather than a shopping list. |
| Telemetry-driven balance pass | Balance | Medium | No | One uninterrupted play session | READ BACK TWICE, and still not possible - which is itself the finding. The 2026-08-21 read (v1.9.4) fixed three defects in the instrument. The 2026-08-25 read found the file had gained ONE row since: a debug marker. Every kill in it predates the clock fix, so 84% of times-to-kill are missing and missing in a biased direction; and after dropping town rows (nothing drops in town - see telemetry.h's cleaning rules) the drop sample is 46 items, which cannot measure a 5% tier rate. The instrument is sound and wired; it has never been fed. One real session at depth unblocks this row, per-difficulty zone identity, and sizing the milestone list. |
| Enchanting - single-affix reroll | Systems | Medium | Maybe | | Reroll one affix's value on a rare or better, at a vendor or shrine. Stays non-breaking if kept to a numeric reroll. |
| Paragon-style post-cap progression | Systems | Medium | New file | | Experience past level 99 converts into small permanent bonuses instead of being wasted. |
| Per-affix perfect-roll indicator | Systems | Medium | Yes | | Show which affixes rolled at their maximum. Needs a per-affix flag the item record does not carry. |
| Transmogrification | Systems | Medium | Yes | | Wear one item's stats with another's appearance. Needs a second cursor id per item. |
| Unique legendary powers | Systems | Large | Yes | | D3-style: a unique that changes how a skill behaves, not just its numbers. |
| Randomized bonus dungeon | Systems | Large | No | Zone pipeline | A Nephalem-Rift-style randomised descent built from existing tilesets and rosters. |

| More milestones | Systems | Small | No | | Eight shipped at v1.9.20 and the list is the cheapest content in the game to extend - each is one condition and one enum value, and the mask has 24 spare bits. Obvious candidates: reach Torment, complete a named set, socket a six-socket item, salvage N items, kill every named unique. Worth doing once the telemetry says how fast the twenty-signet cap is actually reached. |
| PlayerPack's header comment is out of date | Directive | Small | No | | It says pfile.cpp's ReadHero "only accepts a file whose size matches sizeof(PlayerPack) exactly", and warns that anything wanting space should hunt for reserved bytes. ReadHero has accepted a chunked TAIL since 1.6.27 - `read >= sizeof(*pPack)` with the excess handed to ApplyHeroChunks - which is where new per-character state has belonged ever since. The stale comment nearly sent Phase 2 into a struct growth that would have broken every hero file. Correct it before it misdirects someone else. |
| Growing charms with a PRIVATE history | Systems | Medium | Yes | | What v1.9.22 deliberately did not do. Its charms grow with the CHARACTER's milestones, so two copies are worth the same and no per-item state was needed - which is why that phase cost no format bump. A charm with its own history (it grew because YOU carried it through those fights) is a different and arguably better feeling, and it needs a byte on the item. Worth doing only alongside another change that already pays a bump, and only if play says the shared growth reads as flat. |
| A real uberquest AREA | Phase 4 | Large | No | Zone 1 proves the pipeline | What v1.9.23 could not do. Its three encounters are ARENAS - one room, one fight - which is the right first version and is not Fauztinville. An encounter with a place you travel through needs a level of its own, which is the tileset pipeline and genuinely blocked on art. The mechanism is already built and waiting: a Sealed Map, a set level, a placed boss and a guaranteed reward, all of which would work unchanged on a real map. |
| More named encounters | Systems | Small | No | | Three shipped at v1.9.23 and a fourth costs one row in Places[], one in the generator, and nothing else - but there are only three arena .dun files, so a fourth needs somewhere to run. Cheap the moment any new set level exists, for any reason. |
| The game does not exit cleanly | Directive | Small | No | | Observed twice on 2026-08-30, same signature both times: the process outlives its window - MainWindowHandle 0, empty title, CPU frozen (767s with a zero delta measured over three seconds), ~37 MB resident - and sits there indefinitely still holding oracool.mpq open. The next build then fails its atomic replace with MoveFileExW error 5 until the process is killed by hand. NOT corruption: the transactional packer left the previous archive byte-identical and cleaned up its shadow on both failures, which is the first time that path has been exercised by a real lock rather than an injected one. Likely a thread that is never joined, or an SDL subsystem left initialised on the shutdown path. |
| Belt slot numbers | Directive | Small | No | | UPDATED 2026-09-13. Half shipped: the Menu and Portal buttons have drawn labels since v1.9.289 - a gold M and a blue TP, with shadows. The 1-4 hotkey numbers on the four item slots are still not drawn. The old condition, 'only matters if the plateless HUD is kept', no longer applies; the HUD plate has been rebuilt several times since. |


| An item record that reads the previous version | Systems | Medium | No | | Found 2026-09-13. LoadHeroItems, LoadStash and LoadInventoryTabs compare OracoolItemFormatVersion for EXACT equality, so any item-format bump makes existing heroes, stashes and tabs refuse to load. A reader that accepts the previous version - reading a new tail only when the stored version says it is there - turns the per-item 'Save: Yes' rows into ordinary changes. It unblocks the next two rows and lowers the cost of the base-pool treasure classes, the perfect-roll indicator, transmogrification and charms with a private history. |
| Keep Mystic Orbs through a rebuild | Systems | Medium | Yes | An item record that reads the previous version | Since v1.11.104 every crafting rebuild refuses an item carrying a Mystic Orb: the item records how many orbs it took, not which, so a reroll could never put them back. Recording which orbs - one byte each, six at most - would let a rebuild re-apply them instead of refusing. |
| Affix storage past three per table | Systems | Medium | Yes | An item record that reads the previous version | v1.11.105 made the affix limit one flat count per quality, but storage is still three prefixes and three suffixes by table of origin - so a rare cannot roll four prefixes, and a primal's six are always three and three. Widening or merging the two arrays is an item-format change, and RepairOracoolAffixesIfCorrupted would have to learn which table each stored affix came from. |
| Base type on shop rows | Directive | Small | No | A screenshot of the shop | Audit finding #16, 2026-09-13. Magic and rare items are named from the pool now ('Rotting Bane'), so a shop row no longer says it is a helm - only its icon does. Adding the base type lengthens a row that also right-aligns the price, and whether the two collide can only be seen, not tested. |
| The Barbarian's Rage | Systems | Large | ? | User decision | User, 2026-09-13: 'I will probably replace mana with Rage further down the road.' A resource built by fighting instead of mana. Recorded so stat and loot work keeps treating Magic as not the Barbarian's axis in the meantime - Smart Loot already does. |
| Spells never miss - the verdict | Balance | Small | No | The user's verdict from play | A TRIAL since v1.11.052: SpellsNeverMiss in player.h skips the spell hit roll against monsters, the Diablo II rule. The user is judging Sorcerer power in play and may revert it; keeping or reverting is one constant. |
| Paper-doll heroes | Content | Large | No | AI art tools good enough | PARKED 2026-09-11 by the user: no art capacity or budget for Diablo II-style layered heroes. Prerendered sheets, fallbacks and overlay icons stand in until the tools can carry it. |
| The Necromancer | Content | Large | Yes | | ASKED 2026-09-16. The seventh hero, in the Bard's place - the Bard was hidden rather than deleted at v1.12.006 for exactly this ("I want to introduce Necromancer in his place later"). Wants his own three pages of tree rows and their glyphs, a body to wear (the parked paper-doll problem - the Bard's own sheets are the obvious stand-in), and raising the dead, which the Companion feature (v1.12.018) now carries: skeletons and a golem are companion definitions rather than new engine work. Save format because a new HeroClass enters the hero file's class field and the class-tree investment block. |
| The Ancients, the Barbarian's own | Phase 6 | Medium | No | | ASKED 2026-09-16. Korlic, Talic and Madawc exist as Companions since v1.12.018, called by Ancestral Call: warrior sheets in grey, 20 s +2 a level, each at 35% of the hero's blow, with Leap, Whirlwind and Hammer Toss. What they are NOT yet is the Barbarian's landmark: bodies of their own rather than borrowed warrior sheets, three distinct weapons and voices, the D3 "they fight beside you until you fall" feel, and a place in the Barbarian's tree that reads as a summoning rather than one more active. Their numbers are first guesses and want a play session. |
| Phalanx, the Paladin's companions | Phase 6 | Medium | No | | ASKED 2026-09-16. The Paladin has no summon row at all. Diablo III's Phalanx calls a line of shield-bearers or bowmen that charge through what stands in front of them. The Companion feature carries most of it - a guard role that draws blows, a group summoned together, a charge ability - so the work is the skill row itself (which page, which tier, its glyph), the definitions, and a charge that moves several companions as one. |

## Shipped, so not listed above

**Audited 2026-09-13 against v1.11.106**, the first pass since 2026-09-01. Seven rows had shipped without being moved: **Levski's Roar's window art** (the painted skin, 2026-09-04/05); **the stash chest's real art** (the sarcophagus, back on 2026-09-08); **the 107 remaining uniques** (all 250 live on eighteen new bases at v1.11.031); **cold damage** (the fifth DamageType, Round 1 of the inert-skill plan on 2026-09-03, with its missiles and chill); **the Sorcerer's thin tree** (cold made her ice page live - three rows remain and moved into the inert-rows row); **movement-speed affixes** (IPL_MOVESPEED at v1.10.003, an ordinary pool affix since v1.11.105); and **the stash sharing the hero's transaction** (hero and stash published as one transaction, v1.9.59-60). The tree and passive rows were re-counted from the wiki's own data rather than retyped, and seven new rows came out of the loot and save work of 2026-09-11 to 2026-09-13.

**"Retire the standalone Crafting window"** is gone from the table, and not because it was built -
because its premise was. The row existed to end a real problem: the same recipes ran from two
larders, the monument's grid and the backpack, and it proposed removing the burger entry (blocked on
recutting `menu_icons.png`, since the row order must match the sheet).

The user resolved it the other way across 2026-08-31. First the two recipe lists were merged so both
books showed all seventeen (v1.9.140), then backpack crafting was DELETED outright - the path, not
just its button - leaving Levski's Roar the only place a recipe can produce an item (v1.9.142). The
burger window stays, as a reading room: seventeen recipes, a line under the title saying where they
are crafted, and a click that answers with the monument's name rather than an item.

So the two larders are one larder, the icon sheet never needed recutting, and nothing is left of the
row but this note.

**The Diablo 2-style shop interface** shipped across v1.9.25-1.9.31, and it was scoped Medium when
it was two features wearing one name. The STRUCTURAL half - one door per vendor, everything behind
it as tabs - landed at 1.9.25 and changed no transaction. The VISUAL half took five more versions.

The grid is the stash's geometry, 10x16 at 28px, which is 280x448 and does not fit the vanilla
592x292 store box - so the shop became a 340x720 panel in the top-left slot. Selling is a drag from
the inventory, which now opens alongside it; the old Sell screen became a Sold tab that offers back
what the vendor bought at the price it paid; Repair, Repair all and Recharge became icon buttons you
drop an item on.

The transactions were NOT rewritten. Every one derived which item from `stextvhold + ((stextlhold -
stextup) / 4)` - the text list's scroll position, and the arithmetic behind the 1.8.90 crash. They
were split into `...At(idx)` forms and the grid hands an index to a bridge that runs the tab's own
Enter handler, so every afford/room/stale-row guard still has exactly one copy.

Three audit passes found seven defects, four of them mine and invisible from a green build: items
drawing above the grid (ClxDraw renders upward, and the inventory only gets away with the same
anchor because it marks an item's BOTTOM-left cell); a sixth of Griswold's stock silently unbuyable
once the stocks were raised to fill the grid (fixed by paging, which also exposed a pre-existing
unbounded walk in `SortVendor`); Pepin buying anything Griswold buys; and the shop losing its panel
slot to five other windows that draw over it while it went on swallowing the clicks.

**None of it has been played.** The drag, the icons at 20px, and how many pages a real stock fills
are the three things no test can reach.

**Named encounters** shipped at v1.9.23 - D2MXL-to-ORCL Phase 4, and the last of the four. Three
sealed destinations on the three existing arena set levels: a Sealed Map drops from a Dread boss,
opens the encounter in town and is consumed; the floor holds one Dread boss and nothing else; the
reward is a guaranteed unique charm named on the map itself.

Scoped Large and blocked on art, and it was neither: the arenas already load from shipped .dun files
in three tilesets with exit triggers wired, entry is two lines, and populating a set level is what
every quest level already does. The multiplayer gate turned out to be policy inside TextCmdArena
rather than anything structural.

**THE WHOLE D2MXL-TO-ORCL PROGRAMME IS NOW SHIPPED** - Mystic Orbs, Signets and milestones, growing
charms, named encounters. None of it has been played.

**Growing charms** shipped at v1.9.22 - D2MXL-to-ORCL Phase 3. Three charms whose value scales with
claimed milestones, base deliberately below the fixed charm covering the same stat so the choice is
"good now" against "better later". The plan budgeted a format bump for per-item state; it was not
needed, because milestones already live in the chunk tail and the charm's power is a pure function
of them. The trade - growth belongs to the character, so two copies are worth the same - is recorded
as its own row above rather than left implicit.

**The Signet as a droppable item** shipped at v1.9.21 - champions and better only, 3% times what the
monster is worth, with the cap refusal in UseInvItem beside the spell-book gate so a signet used at
the cap is never consumed.

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
