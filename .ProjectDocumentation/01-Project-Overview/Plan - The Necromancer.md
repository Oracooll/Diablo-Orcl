# Plan - The Necromancer

**Revision 2, 2026-09-17**, written against v1.12.029. Revision 1 listed six open decisions; the user
answered them in the artifact *The Road to Necromancy* (https://claude.ai/artifact/85RWSQWqGn9CK2EhR1gsWq),
and this revision is rebuilt on those answers. Revision 1 is in git (`ae32fe54`).

---

## 0. What is decided

| # | Question | The user's answer | Consequence |
|---|---|---|---|
| D1 | Class slot | **A new one** (recommended) | `HeroClass::Necromancer` = 6. The Bard stays hidden and intact. |
| D2 | Body | **The Sorcerer's, dyed** (recommended) | His three armour tiers are measured and dyed with `oracool/hero_look`; the shield-follows-item look already covers these sheets. |
| D3 | Resource | **"Dual orb (Demon Hunter D3 style) - Mana/Essence (dark green) used for casting curses. Auto refills rather quickly."** | Two pools in one orb. Mana pays for everything it pays for today; **Essence**, dark green, pays for curses and refills quickly by itself. New HUD work and a second pay path - section 3. |
| D4 | Army | **"Upgrade the game engine ... increase this 4 number to a lot more - as many as in D2."** | The army stops being an optional last phase and becomes the FIRST engine phase, so the Summoning page is designed for a crowd from the start - section 4. |
| D5 | Skill pages | **Diablo II's three: Summoning, Poison & Bone, Curses** (recommended) | 54 tree rows + 18 passives; the user edits the rows in a ledger. |
| D6 | Item bases | **"Scythes AND wands and shrunken heads."** | Three new item families: types, bases, icons, ground tumbles, uniques, drop and vendor rules - section 6. The largest addition to the plan. |

Reading of D3, to be confirmed (Q8 below): Essence is a SECOND pool beside mana, not a replacement for it,
and only curses draw on it.

---

## 1. New questions the answers raise

Added to the artifact as D7-D11. None blocks the first two phases.

| # | Question | Recommendation |
|---|---|---|
| D7 | **How many minions, exactly?** "As many as D2" is a formula there: skeletons and mages each 1 + rank/3 (about 8 at high rank), one golem, Revive up to rank (10-20). | Skeletons 8, Mages 8, Golem 1, Revived 10 at full rank = **27**, engine cap 32. |
| D8 | **Essence details**: what it pays for, how big, how fast. | Curses only; pool 100; refills 0 to full in about 5 seconds; no potion touches it; not saved. |
| D9 | **How deep is each new item family?** The Orcl shields are a 16-rung ladder; vanilla families are 6-10. | 8 bases per family (24 bases), level 1 to 45 - each needs an icon and a tumble. |
| D10 | **Who can use them?** | Anyone can equip wands and scythes (they are weapons); shrunken heads are his alone, like the Bard's Canticle was. |
| D11 | **What does a shrunken head look like in his hand?** It sits in the shield slot, and the sheets only know how to draw a shield there. | Draw the NO-shield sheet while a head is held: `sprite_mix` already knows a body with an empty shield hand. |

---

### Answered 2026-09-17 (second round)

| # | The user's answer | Consequence |
|---|---|---|
| D7 | **8 Skeletons, 8 Mages, 1 Golem, 10 Revived = 27** (recommended) | Minion pool of 32. Counts grow with rank: 1 at rank 1, +1 every 3 ranks. |
| D8 | **Curses AND the corpse skills**; "pool of 100. fills within 20 seconds." | Essence is his death-magic pool: curses, Corpse Explosion, Revive. 5 a second is SLOW against D2's habit of chaining Corpse Explosions, so prices decide the feel - proposed: Corpse Explosion 10, a curse 25, Revive 35. Raising skeletons, bone and poison stay on mana. |
| D9 | **8 bases per family** (recommended) | 24 bases, 48 images in art request B. |
| D10 | **Wands and scythes for anyone; heads his alone** (recommended) | Heads are a class-only base, hidden from every other class's drops the way the Bard's were. |
| D11 | **"light shield with head decal on it (if you can put one on the shield asset)."** | A held head is drawn as the LIGHT tier's shield whatever armour he wears - that is  as it stands ( for head items). The decal is new work: the mixer knows the shield's pixels per frame (it subtracts them), so a mark can be stamped on the frames where the face shows and the shield re-dyed bone in true colour. At game scale a head on a 20-pixel shield is a pale mark, not a portrait - to be judged on a contact sheet before it is committed to, as the shield swap was. |

## 2. The class itself (data and plumbing) - unchanged from revision 1

Traced through `HeroClass::Bard` (18 files) and `enum_size<HeroClass>` (5 files).

- `player.h` enum; `playerdat.cpp` `PlayersData`, `PlayersSpriteData`, `PlayersAnimData`, `herosounds` rows
  (Sorcerer's body and voice); `player.cpp` `GetPlayerSpriteClass` and six class switches.
- `oracool/sprite_import.cpp` `ClassSpriteFolder`, `oracool/hero_preview.cpp` `SpriteClassFor`,
  `engine/trn.cpp` `GetClassTRN`; `oracool/hero_look` - his dye per armour tier.
- Class branches: `items.cpp` (starting gear, class bonuses, 3 sites), `inv.cpp` (3), `spells.cpp` (2),
  `missiles.cpp` (7), `objects.cpp` (9: shrines and class speech), `effects.cpp` (speech routing).
- Front end and HUD: `selhero.cpp`, `diabloui.cpp` portrait order, `oracool/hud_art.cpp` (2 art switches),
  character and stat sheets, glossary.
- Save: class byte; `oracool/hero_chunks.cpp` (class, first tree index, count) table. Additive, no break.

---

## 3. Essence and the dual orb (D3)

- `oracool/essence.{h,cpp}`, modelled on `oracool/rage`: a transient whole-number pool, `MaxEssence`,
  `ProcessEssenceTick` (refill - the opposite sign of Rage's drain), `ClassUsesEssence`.
- **One pay path.** `CanPaySkill` / `SettleSkill` (already the only door for skill costs since the Rage work)
  learn a third currency: a row is priced in mana or in Essence, never both. Tooltips say "Essence Cost".
- **The orb.** The right orb is split down the middle, Demon Hunter style: mana blue on one half, Essence dark
  green on the other, each half filling by its own pool. `oracool/hud_art` draws the mana liquid as an ARGB
  layer already and tints a copy for Rage; the split is two clipped draws of two tints. The numbers readout
  becomes two lines. NOTE the palette has no green for indexed art (recorded 2026-09-12) - this works only
  because the HUD is true colour.
- Character sheet, hero-select preview stats and the stat sheet gain the second resource.

---

## 4. The army (D4) - now the first engine phase

**What the code already does** (checked for this revision - better than revision 1 assumed):
`Monster::isPlayerMinion()` is a FLAG test (`MFLAG_GOLEM` without `MFLAG_BERSERK`), not a slot test.
`UpdateEnemy` scans every active monster and already stops minions fighting each other. Berserk and the
Barbarian's turning warcry already make ORDINARY-slot monsters fight for the player. The enemy encoding
(`monster.cpp` 5251-5264) is general. So "a friendly monster outside the golem slots" exists today.

**What is tied to the four golem slots and has to go:**
- Companion INSTANCES are an array of 3 bound to slots 1-3 (`MaxCompanions`); the Golem spell owns slot 0 by
  player number. Minions need a pool: `MaxMinions` = 32 bodies taken from the ordinary `Monsters[]` range.
- Level generation fills up to `MaxMonsters` (200) less a margin of 10. It must leave the minion pool free
  (raise `MaxMonsters` to 232, or reserve 32 of the 200 - measured choice, the arrays are static).
- Spawn / holding cell / release (`SpawnCompanionBody`, `ReleaseCompanionBody`, `GolemHoldingCell`), the
  level-change carry-over, `LevelHasGolemSlots` (town and set levels), the three `for i < MAX_PLRS` loops.
- Ownership and credit: a golem is "player N's" by index. Minions carry an owner field; their kills pay the
  owner (the Companion blows already do this by being the OWNER's blows).

**What a crowd needs that three bodies did not:**
- Moving through them: the hero already walks through companions (`CompanionsMakeWay`); twenty-seven bodies in
  a one-tile Cathedral corridor must also yield to EACH OTHER and never wall the hero in.
- Formation: rings around the owner by kind (warriors out front, mages behind), not three fixed offsets.
- Cost: `CompanionAi` per minion per tick, path-finding for 27; a budget (stagger the thinking across ticks).
- HUD: a panel of groups with counts ("Skeletons 6/8"), not three named portraits. Stances apply per group.
- Friendly fire: the hero's and the minions' missiles pass through minions (the rule exists for companions).
- Cursor: minions are not targets and do not steal clicks.

---

## 5. Skills: 72 rows on three D2 pages (D5)

`oracool/class_tree`: a `NECROMANCER_FIRST` block appended after the Monk's (no existing index moves), 72
rows, range `static_assert`s, `LevelUpStat` channels, new `SpellID`s (the writehero hash moves once,
deliberately), passives through `oracool/passives.h`. Curses are priced in Essence.

Engine systems the pages need, beyond the army:

1. **Monster curses - Medium.** One curse per monster: kind, strength, ticks. Hooks in `ApplyMonsterDamage`
   (Amplify, Decrepify, Lower Resist), damage dealt (Weaken), `UpdateEnemy` (Attract, Confuse, Dim Vision,
   Terror), on-hit reflection (Iron Maiden) and healing (Life Tap). Speed curses fight the shared-animation
   constraint recorded at the aura-field work, so they slow by skipping a monster's ticks. Overhead marker and
   a line on the health bar. Cast as an area at the cursor, like the warcries.
2. **Damage over time, generalised - Small.** Deep Wounds' bleed becomes one channel with an element; poison
   is then data. Poison tint through the lit-TRN variant draw path.
3. **Corpses a skill can use - Medium.** `dCorpse` knows where a body lies, not what it was. A capped per-level
   table filled at `MonsterDeath` (type, level, position), consumed by Raise Skeleton, Revive and Corpse
   Explosion. Not saved.
4. **Minions drawn as monsters on any floor - Medium.** Skeleton CL2s always loaded (as `MT_GOLEM` is, via
   `AddMonsterType(..., PLACE_SPECIAL)`); a Companion definition that binds a monster type's animations rather
   than hero sheets. Revive binds the dead monster's own type, loaded on that floor by definition. Golem kinds
   are TRN recolours. An exclusion list for Revive (bosses, Diablo, types whose AI makes no sense friendly).
5. **Bone and poison missiles - Medium, art-bound.** Teeth, Bone Spear, Poison Nova, Corpse Explosion, Bone
   Wall / Prison. `PngOnly` with placeholder fallbacks, as RfA-16 landed.
6. **Bone Armor - Small.** Ice Armor's shell draw plus Mana Shield's interception, as an absorb pool.

---

## 6. Wands, scythes and shrunken heads (D6)

Per family: an `ItemType` (APPENDED - the value is written into the tiered-item save extension), an
`item_equip_type`, `ItemData` rows for the ladder, `UITYPE`s (new ones - the standing rule is never to add
uniques to an existing `UITYPE`), icons in the cursor sheet, ground tumbles (through `FinishOracoolDrop` - a
drop built without the tumble is invisible until reload), affix eligibility, Smart Loot slot and class aim,
vendor stock (Adria for wands and heads, Griswold for scythes), salvage class, sockets by footprint, tier
ladders, item-level ceilings, and tests.

- **Wands** - one-handed caster weapon. Swung with the Sorcerer's mace-class sheets. Carries +skills,
  faster cast, Essence or mana affixes; may hold spell charges like a staff.
- **Scythes** - two-handed. Swung with the staff-class sheets (the only two-handed swing the Sorcerer's art
  has). Damage-leaning, with a bonus to summons or poison.
- **Shrunken heads** - the off-hand. Shield slot, no block. His alone (D10). Drawn as the no-shield sheet
  (D11).
- Uniques and sets: a first tranche per family (the unique-expansion package's "recommended: Bard" uniques on
  shared bases are candidates to re-tag for him).
- The item format version is bumped if `ItemType` growth requires it, per the standing rule (never protect a
  save format); loaders still reject any other version, so this is a break for existing heroes unless the
  "reads the previous version" Roadmap card lands first. Flagged, not decided here.

---

## 7. Art and sound (two requests)

| RfA | Contents |
|---|---|
| A - with the skill ledger | 72 glyphs; ~8 missile sheets; ~10 curse markers; hero portrait; class HUD art |
| B - with the item families | 24 base icons + 24 tumbles (D9), plus icons for the first uniques |
| none | Body (Sorcerer's, dyed), voice (Sorcerer's), minions (the game's skeletons and golem), the orb (tinted in code) |

---

## 8. Build order

| Phase | Contents | Size | Then you can |
|---|---|---|---|
| N1 | The class exists: tables, dyed Sorcerer body, hero select, starting gear, 72 inert rows | Medium | Play a Necromancer who fights like a bare Sorcerer |
| N2 | Essence and the dual orb | Medium | See both pools; nothing spends Essence yet |
| N3 | Skill ledger; you fix the 72 rows; RfA A goes out | Small | Read and edit his whole kit |
| N4 | **The army engine**: minion pool of 32, crowd movement, formation, group panel, think budget | Large | Summon a debug crowd of 27 and walk a corridor with it |
| N5 | Summoning page: corpse table, minion graphics, Skeletons, Mages, Golems, Revive, masteries | Large | Play a summoner |
| N6 | Poison & Bone page: DoT channel, Bone Armor, the missiles | Medium | Play the caster page |
| N7 | Curses page on Essence: the curse system | Medium | All three pages live |
| N8 | The 18 passives | Medium | The complete kit |
| N9 | Wands, scythes, shrunken heads; RfA B | Large | Find and wear his gear |
| N10 | Art intake, both requests | Small | His final look |
| N11 | Play pass, numbers, Roadmap and census | Small | Built |

N1-N3 can start now. N4 must precede N5. N6, N7 and N9 are independent of each other and of N5.

---

## 9. Risks

- **N4 in a Cathedral corridor.** Twenty-seven bodies in a one-tile hallway is the test that decides whether
  "as many as D2" is pleasant in D1's maps. It is why N4 ends in a debug crowd BEFORE any summoning skill exists.
- **Think cost**: 27 path-finders a tick in a Debug build. Budgeted, measured in N4.
- **Monster pool**: 232 static monsters, or 32 taken from the level's 200 - the second thins late floors.
- **Revive** inherits the dead type's AI; special AIs need an exclusion list.
- **Speed curses** versus shared animation timing.
- **Item format**: three new `ItemType`s may force a version bump that orphans existing heroes (section 6).
- **Art lead time**: 72 glyphs plus 48 item images is the largest request yet; everything runs on placeholders.
- **Scope**: this is now the largest single feature in the mod - a class, an engine change, a resource, three
  item families. Eleven phases, several of them Large.
