# Plan - The Necromancer

2026-09-17, written against v1.12.029. Requested by the user: "create the necromancer class. first of all -
make a plan with everything this build will require to be completed."

The seventh hero. The Bard was hidden at v1.12.006 "to introduce Necromancer in his place later", and the
Companion feature (v1.12.018) was built with him in mind. This document lists everything the class needs, in
the order it should be built, with the decisions that are the user's to make called out first.

---

## 0. Decisions needed before building

Each has a recommendation; the build can start on the recommended path and change later at the cost noted.

| # | Decision | Recommendation | Why / cost of changing later |
|---|---|---|---|
| D1 | **A new class slot, or the Bard's?** | A NEW `HeroClass::Necromancer` (value 6). The Bard stays hidden, as asked ("Hide them, dont remove them"). | Reusing slot 4 would turn every Bard save into a Necromancer and destroy the Bard's rows. A new value costs one more row in ~20 tables. |
| D2 | **Whose body does he wear?** | The **Sorcerer's** sheets - a robed man - dyed with `oracool/hero_look`: black and bone robes, grey skin, white hair. Free, uses what was built this week. | The Rogue/Bard sheets are a woman in leather. A body of his own is the parked paper-doll problem. Dyeing needs his three armour tiers measured the way the Warrior's light tier was. |
| D3 | **Mana, or a resource of his own?** | **Mana** for V1. | D3's Essence (built by attacks, spent by skills) is the Rage system with other numbers - `oracool/rage` would generalise - but it doubles the design work: every skill needs a generator/spender role. Can be added later without a save break (Rage is transient). |
| D4 | **How big is his army?** | **Stage it.** First within today's limit - 3 companion bodies plus the Golem's own slot. Then lift the limit as its own phase. | The engine gives summons the four "golem slots" `Monsters[0..3]`. An army of 8-10 skeletons means friendly monsters in ordinary slots: 8 index assumptions and 11 `MFLAG_GOLEM` sites to make slot-agnostic. Real engine work; should not block the class. |
| D5 | **The skill list.** | Three D2 pages - **Summoning, Poison & Bone, Curses** - 18 rows each, plus 18 D3-style passives = 72 rows, the shape every other class has. The user picks/edits the list in a ledger artifact, as for the Barbarian's Rage. | The list drives the art request (72 glyphs) and the engine systems below; changing it after the RfA goes out costs glyphs. |
| D6 | **New item bases for him?** | None for V1: he uses wands/staves/daggers that exist. | Wands, scythes and shrunken heads each need icons, tumbles and unique tables (see the Bard's War Lute and Canticle for the cost). |

---

## 1. The class itself (data and plumbing)

Everything a class is wired into, found by tracing `HeroClass::Bard` through the code (18 files) and
`enum_size<HeroClass>` (5 files). None of it is hard; all of it is mandatory, and a missed table is a crash or a
silent wrong default.

**Core tables**
- `player.h` - `HeroClass::Necromancer`, `LAST`. `CharChar` gains a letter only if he ever gets sheets of his own.
- `playerdat.cpp` - `PlayersData` row (name, `classPath` "sorceror", base and max stats, life/mana per level,
  block bonus), `PlayersSpriteData` and `PlayersAnimData` rows (copy the Sorcerer's - same body), `herosounds` row.
- `player.cpp` - `GetPlayerSpriteClass` (Necromancer -> Sorcerer), the six other class switches (frame counts,
  starting spell, per-level gains).
- `oracool/sprite_import.cpp` `ClassSpriteFolder`, `oracool/hero_preview.cpp` `SpriteClassFor`,
  `engine/trn.cpp` `GetClassTRN` ("plrgfx\necromancer.trn" - optional).
- `oracool/hero_look` - his dye, per armour tier; `oracool/sprite_mix` already covers the Sorcerer's sheets, so
  the shield-follows-item look works for him the day he exists.

**Rules with a class branch**
- `items.cpp` - starting gear (`CreatePlrItems`), class item bonuses, the class table at 8157.
- `inv.cpp` - 3 branches (the Bard's dual-wield rules; he takes none).
- `spells.cpp` - cast-speed / mana-cost classes (2 branches): he belongs with the Sorcerer.
- `missiles.cpp` - 7 class branches in damage and to-hit formulas.
- `objects.cpp` - 9 branches: shrine effects and the class-specific speech lines on books, doors and altars.
- `effects.cpp` - hero speech routing. **He has no voice**; he borrows the Sorcerer's lines.

**Front end and HUD**
- `DiabloUI/hero/selhero.cpp` - the class list, the stat preview, the spawn gate; `diabloui.cpp` portrait order.
- `oracool/hud_art.cpp` - 2 per-class art switches (portrait / class emblem).
- Character sheet, stat sheet, hero-select strings, translations glossary.

**Save**
- The hero file stores the class byte; `oracool/hero_chunks.cpp` has a table of (class, first tree index, count)
  for the class-tree investment chunk and needs his row. No format break: a new class only adds.
- `oracool/hidden_classes.h` untouched - the Bard stays hidden, the Necromancer is simply not in it.

---

## 2. The skill tree (72 rows)

`oracool/class_tree` - the one skill system. He needs:

- a `NECROMANCER_FIRST` block in `ClassTreeSkill` (54 tree + 18 passive rows), appended AFTER the Monk's so no
  existing index moves (icon-strip position and the saved investment index are the same number);
- 72 `ClassTreeSkillData` rows (name, description, page, tier, column, kind, spell id, implemented, max rank);
- `static_assert`s for his range against `MaxSkillsPerClass` (96);
- a `LevelUpStat` channel per row, a Rage-style resource line if D3 is ever taken;
- new `SpellID`s for the actives (the enum is int16 since v1.11.112; `MAX_SPELLS` and the writehero hash move);
- passives through the hooks in `oracool/passives.h`.

Proposed pages (final list is D5):

| Page | Flavour | Rows that exist in some form already |
|---|---|---|
| Summoning | Raise Skeleton, Skeletal Mage, Golems (Clay/Blood/Iron/Fire), Revive, masteries | Golem spell and its slot; Companion definitions, stances, HUD panel |
| Poison & Bone | Teeth, Bone Spear, Bone Spirit, Bone Armor, Bone Wall/Prison, Poison Dagger/Explosion/Nova, Corpse Explosion | `SpellID::BoneSpirit` and its missile; Deep Wounds' bleed (a damage-over-time on monsters); Ice Armor's shell draw |
| Curses | Amplify Damage, Weaken, Iron Maiden, Life Tap, Decrepify, Lower Resist, Dim Vision, Confuse, Attract, Terror | Warcries' area-effect pattern (`oracool/warcries`); aura fields |

---

## 3. Engine systems he needs that do not exist yet

In build order. Each is a unit of work with its own report; sizes are Small (a session) / Medium / Large.

1. **Monster curses - Medium.** A per-monster debuff state (kind, strength, ticks left), one curse at a time
   as in D2. Hooks: damage taken (`ApplyMonsterDamage` - Amplify, Decrepify), damage dealt (Weaken), movement
   and attack speed (Decrepify - note the constraint recorded at the aura-field work: animation timing lives on
   the shared `CMonster`, so SPEED changes are per type, not per monster; Decrepify needs a per-monster tick
   skip instead), target choice (`UpdateEnemy` - Attract, Confuse, Dim Vision, Terror), on-hit reflection (Iron
   Maiden), on-hit healing (Life Tap). A curse icon over the monster and a line on the health bar. Not saved
   (levels are not saved mid-state in V1).
2. **Damage over time on monsters, generalised - Small.** Deep Wounds' bleed becomes one DoT channel with an
   element, so poison is a row of data rather than a second system. Poison tint reuses the variant-recolour
   draw path (lit TRN).
3. **Corpses as something a skill can use - Medium.** `dCorpse` records where a body lies and how it is drawn,
   not what it was. Needed: find the nearest corpse in range, consume it (clear the tile's corpse), and remember
   enough about the dead (monster type, level) for Revive and for Corpse Explosion's damage. Proposed: a small
   per-level side table filled where monsters die (`MonsterDeath`), capped, not saved.
4. **Minions drawn as monsters, on any floor - Medium.** Companions today wear HERO sheets. Skeletons need the
   skeleton CL2s, and monster graphics are loaded per level only for that level's roster. Needed: always-load a
   small minion roster (as `MT_GOLEM` already is, `AddMonsterType(..., PLACE_SPECIAL)`), and a Companion
   definition that binds a monster type's animations instead of a hero sheet. Revive binds the dead monster's own
   type, which is loaded on that floor by definition. Golem variants are TRN recolours of the one Golem.
5. **Companion framework extensions - Medium.** Per-definition counts that scale with rank (2 skeletons at
   rank 1...), minions that do not expire (skeletons last until killed; Revive is timed), a panel that shows a
   group rather than three named bodies, mastery passives feeding life and damage.
6. **More than three bodies - Large (D4, its own phase).** Friendly monsters in ordinary monster slots:
   make the 8 `< MAX_PLRS` assumptions and 11 `MFLAG_GOLEM` sites ask "is this a friendly summon" rather than
   "is this index below 4"; targeting, missile friend-or-foe, the taunt hook, level change carry-over,
   the summons-need-a-golem-slot guard (town and set levels). Until then: 3 companions + the Golem.
7. **Bone and poison missiles - Medium, art-bound.** Teeth, Bone Spear, Poison Nova, Corpse Explosion blast,
   Bone Wall/Prison segments (objects or stationary monsters - see the Decoy for a body that only stands).
   All `PngOnly` missile graphics with placeholder fallbacks, the way RfA-16 landed.
8. **Bone Armor - Small.** An absorb pool with a shell drawn over the hero: Ice Armor's shell path plus
   Mana Shield's damage interception.

---

## 4. Art and sound to request (one RfA)

| Asset | Count | Notes |
|---|---|---|
| Skill glyphs | 72 | vanilla style, via `tools/BuildGlyphStrips.ps1`; frames detected by pixel colour, never bake a plate |
| Missile sheets | ~8 | Teeth, Bone Spear, Poison Nova, Poison cloud, Corpse Explosion, Bone Wall, Bone Prison, curse cast ring |
| Curse markers | ~10 | small overhead icons, one per curse (or one icon recoloured by value on the 32-bit screen) |
| Hero portrait | 1 | hero-select portrait and the HUD class art (2 switches in `hud_art.cpp`) |
| Body | 0 | Sorcerer's sheets, dyed (D2). Minions use the game's own skeleton and golem art |
| Voice | 0 | Sorcerer's lines |

No Blizzard archive is shipped; the dye and the minions are built from the player's own files at run time.

---

## 5. Balance and content around him

- `PlayersData` numbers: a caster with the lowest life, Magic as the axis; starting gear; starting skill.
- Smart Loot: aim his drops at Magic/caster bases (the class axis table).
- The unique-expansion package's "recommended: Bard" uniques on shared bases (32) are candidates for re-tagging.
- Set/unique class recommendations, `+skills` affixes if they name classes.
- Companion numbers for every minion (life, resist, damage percent, count per rank) - first guesses, then play.

---

## 6. Tests

- Class-count assumptions: every table indexed by `HeroClass` has a row (a test that walks `enum_size<HeroClass>`).
- Class tree: 54 + 18 rows, every page/tier/column cell filled once, investment index stable, inert rows inert.
- Passive sweep test extended to his 18.
- `Writehero.pfile_write_hero` re-baselined once for the new SpellIDs (deliberate, with a changelog comment).
- Curses, DoT, corpse table: unit tests with a fabricated monster (see the backpack-test pitfalls note for the traps).
- Minion graphics and missiles cannot be tested headless: contact sheets via the export tool, then the user's eyes.

---

## 7. Build order

| Phase | Contents | Size | Playable result |
|---|---|---|---|
| N1 | The class exists: enum, all tables in section 1, Sorcerer body with a dye, hero select, starting gear, Sorcerer's voice. Tree page shell with 72 inert rows. | Medium | A Necromancer who plays like a bare Sorcerer |
| N2 | Skill ledger artifact; user fixes the 72 rows (D5). RfA sent for glyphs and missiles. | Small | - |
| N3 | Bone & Poison page: DoT channel, Bone Armor, Teeth, Bone Spear, Bone Spirit, Poison skills, placeholder art. | Medium | A working caster page |
| N4 | Corpse table, Corpse Explosion; minion graphics; Raise Skeleton, Skeletal Mage, Golems, Revive within 3 bodies + Golem; companion extensions. | Large | Summoner, small army |
| N5 | Curses system and the Curses page. | Medium | Full three pages |
| N6 | The 18 passives; masteries. | Medium | Complete class |
| N7 | RfA intake: glyphs, missiles, portrait. | Small | Final look |
| N8 | The army: friendly monsters beyond the golem slots (D4). | Large | 8-10 minions |
| N9 | Play pass, numbers, Roadmap and census update. | Small | Built |

N1-N2 can start now. N3, N4 and N5 are independent of each other after N1. N8 is optional for calling the
class Built - that is the user's call (D4).

---

## 8. Risks

- **The army (D4/N8)** is the one piece that reaches deep into `monster.cpp`; everything else is additive.
- **Decrepify / speed curses** fight the shared-animation constraint; they may have to slow by skipping ticks.
- **Revive** inherits whatever a monster type's AI does when it is friendly - special AIs (Diablo, bosses,
  suicidal or fleeing types) need an exclusion list.
- **Save**: new SpellIDs move `MAX_SPELLS` again; per the standing rule the format is bumped, not protected.
- **Art lead time**: 72 glyphs is the largest single request so far; N3-N6 run on placeholders meanwhile.
- **Load cost**: an always-loaded minion roster adds sprite memory on every floor (the Golem already does).
