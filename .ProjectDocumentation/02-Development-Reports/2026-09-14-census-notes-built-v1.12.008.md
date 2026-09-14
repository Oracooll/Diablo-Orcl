# The census notes: ten skills adjusted to the engine, and RfA-16

2026-09-14 — v1.12.008

## Why

The user left notes on ten rows in the Orcl Skill Census. Asked to act on them, they said:

> "go ahead with the RfA and the rest"

## What each note asked, and what was built

| Hero | Row | Note | Built |
|---|---|---|---|
| Barbarian | Double Throw | "remove Double Throw ... keep single weapon throw ... using normal attack animation" | **Weapon Throw**: a new `SpellID::WeaponThrow`; see below. The row keeps its position, glyph and investment. |
| Barbarian | Throwing Mastery | "keep single weapon throw" | +10% damage to Weapon Throw, +6% per level (`PassiveDamageDealtPercent`) |
| Sorcerer | Static Field | "work like DMG aura of Paladin. Similar to Holy Fire." | **An aura** (`Kind::Aura`). It pulses every `HolyPulseTicks` over Holy Fire's reach and strips 4% of remaining life (+1% per level, 20% max, half on uniques) as lightning. |
| Sorcerer | Thunder Storm | same note | **An aura.** Every `HolyPulseTicks` a bolt hits one enemy within 6 tiles for 1–20 lightning (+10 per level). |
| Sorcerer | Meteor | "seems doable" | `SpellID::Meteor`, a cast. A ground field falls in 1 s, then deals 20–40 fire (+8–12 per level) in radius 2, then burns 3 s. |
| Rogue | Decoy | "Summon recolored clone. Use Golem mechanic" | `SpellID::Decoy`: the Golem slot with no damage or to-hit and twice the life, lasting 15 s (+1 per level) |
| Rogue | Poison Javelin | "This engine has Acid, so use Acid." | `SpellID::PoisonJavelin`. The first enemy in line takes 60% of a weapon blow as acid (+5% per level), and the ground under it takes acid for 3 s. |
| Rogue | Plague Javelin | same | `SpellID::PlagueJavelin`: an acid cloud at the first enemy or the aim point, 4–8 acid/s (+2–3 per level) for 5 s in radius 2 |
| Rogue | Custom Engineering | "Diablo Hellfire introduces trap runes. Use their mechanics." | Rune traps the Rogue sets gain +3 spell levels (`AddRune`), and half the time the rune is not used up (`ConsumeScroll`). |
| Rogue | Grenadier | "Use Magic Star mechanics for granades." | Every fourth arrow loosed also lobs a grenade, the engine's Fireball at a quarter of the Rogue's level (`DoRangeAttack` → `OnPassiveArrowLoosed`). No "Magic Star" missile exists in the engine. |

## Weapon Throw

`oracool/weapon_throw.{h,cpp}` is a new module.

1. **The click.** A readied Weapon Throw makes the click the ordinary attack, in place, toward the cursor
   (`CMD_SATTACKXY`). It needs a sword or axe in hand and no bow.
2. **The latch.** A latch holds the target, the same shape as the melee and bow latches.
3. **The hit frame.** `DoAttack` asks `ThrowArmedWeapon`. That launches the engine's arrow, which flies with the
   wielder's own damage range (`ProcessArrow`), and settles the price. The swing itself strikes nothing.
4. **Clearing the latch.** Like the other latches, it is cleared where `diablo.cpp` clears them for a plain attack.

**Rage.** It is a new Barbarian skill, so it needs a Rage role. It is a spender at 10, like his other attacks.
The user can change this in `RageCost`.

## Engine changes

- **`MAX_SPELLS` 240 → 245** for the five new ids.
  - Each gets a `SpellsData` row, an icon frame and a spell band.
  - This is still under the 254 ceiling the one-byte readied-spell chunk allows.
- **`AuraFiles` 57 → 59.** It gains `static_field` and `thunder_storm`. `LoadAura` already tolerates a missing file,
  so both auras work with no ring until RfA-16 batch 34 arrives.
- **`aura_field.cpp`:** `ProcessStaticField`, `ProcessThunderStorm` and a shared `AuraTargetsWithin`.

## Placeholder visuals, and RfA-16

`Resources/ChatGPT RfA/RfA-16 - Missiles, Rings and Glyphs for the Census Skills.md` requests three batches:

- **Batch 34:** the two Sorceress aura rings.
- **Batch 35:** eight missile and effect sheets, listed with their current placeholders:

| Sheet | Skill | Placeholder until delivered |
|---|---|---|
| thrown sword, thrown axe | Weapon Throw | an arrow |
| acid javelin, acid cloud | Poison and Plague Javelin | the warcry floor shockwave |
| meteor, meteor impact | Meteor | the warcry floor shockwave |
| thunder bolt | Thunder Storm | the warcry floor shockwave |
| grenade | Grenadier | a fireball |

- **Batch 36:** six glyphs for the rows whose idea changed: Weapon Throw, Crusader's Stride, Sanctified,
  Toughness, Mana Attunement, Serene Mind.

No sheet was requested for Decoy. A true recoloured clone of the Rogue needs hero animations as a monster sprite
set, which is the parked hero body-sprite work.

## Tests

- **New:** `OracoolCensusNotes` checks that all ten rows are built and have the right spell ids and aura kinds,
  that Weapon Throw costs 10 Rage, and that Custom Engineering's rune bonus appears only while slotted.
- **Updated counts:**
  - aura rings 57 → 59;
  - inert floor >25 → >10 (17 rows stay inert: the hidden Bard's 15 and the two retired);
  - built Passive Skills rows 97 → 99.
- **`Writehero` re-baselined.** The skill-investment chunk grew five bytes with `MAX_SPELLS`. This chunk is
  count-prefixed, exactly as in 1.9.182–1.11.111.

## Census after this build

- 324 built
- 36 reworded to fit the engine (each with its note)
- 0 needing engine work
- 2 retired
- 72 hidden (Bard)
