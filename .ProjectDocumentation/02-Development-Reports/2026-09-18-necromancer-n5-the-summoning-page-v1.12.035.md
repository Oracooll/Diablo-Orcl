# The Necromancer, phase N5: the Summoning page (v1.12.035)

**Date:** 2026-09-18 - Debug only - **814 of 814 tests**. N4 (v1.12.034) is still awaiting the user's look in play; the
user said "proceed with n5" without reporting on it. The ledger had no verdicts on the Summoning page, so the rows
were built as written on 2026-09-17.

A leftover `DiabloOrcl.exe` (started 10:09 today) blocked the linker mid-way; it was stopped per the standing rule.
If that was a live session, it was v1.12.034 and nothing of it is lost but the moment.

## The page, all eighteen rows live

Actives (13 new `SpellID`s, cast through rfa12_actives' door into `oracool/necro_summoning`):

- **Raise Skeleton / Raise Skeletal Mage** - need a corpse within 3 tiles of the cursor; take it (the body leaves the
  floor). 1 at rank 1, +1 every 3 ranks, 8 at most (`RaisedCountAtRank`). The body climbs with rank: the four
  skeleton axemen / bowmen weakest to strongest at ranks 1, 4, 7, 10. Mages shoot fire, lightning, poison (acid) and
  bone (arrow) in turn - the palette's own monster missiles, so every one hits monsters through the faction rule.
- **Clay / Blood / Iron / Fire Golem** - one golem of any kind, a new one replaces the old. The Golem body, recoloured
  through `RampTranslation` (tan / red / grey / orange - the companions' recolour, now exported). Habits: Clay's blows
  chill 2 s; Blood returns a quarter of its blows' damage to itself and its owner; Iron gives a third of every blow
  it takes back to the striker; Fire burns everything beside it for half a blow once a second and DRINKS fire.
- **Revive** - a revivable corpse within 3 (no uniques, story monsters, golems or `Availability::Never` types), as it
  was plus 5%/rank life, for 3 minutes + 30 s per point of Lasting Bond. Priced in Essence (35) - the first row that is.
- **Command the Dead** - the companions' focus (`FocusCompanionsOn`) for 6+rank seconds. **Gather the Dead** - every
  body beyond 2 tiles placed beside the owner. **Dark Mending** - every minion within 8 heals 20+4r %.
  **Frenzy of the Dead** - 10 s, blows +40+4r % (through `MinionDamagePercent` at the monster's swing).
  **Unholy Offering** - the nearest minion to the cursor dies; the hero heals 30+3r % (75 max) of its full life.
  **Army of the Dead** - six magic pulses half a second apart within 2 tiles of the cursor.

Passives, read when they matter: Skeleton Mastery and Bone Plating at the raise, Golem Mastery at the shaping,
Summon Resist at every blow (fire / lightning / magic, 20% + 5%/point, 75 max), Lasting Bond at the Revive.

## The engine underneath

- **`oracool/corpses`** - a per-level table (100) written at every enemy death beside `AddCorpse`: position, type,
  level and the dead one's numbers. Taking one clears the floor sprite unless another body lies on the tile.
  Cleared with the level. A revisited floor keeps its sprites but has no table.
- **`oracool/minions` grew**: `MinionSpec` carries a missile (ranged), a `GolemKind`, a lifetime and a ramp;
  hooks `MinionDamageTaken` (ApplyMonsterDamage), `MinionDamagePercent` (MonsterAttack), `OnMinionBlow` /
  `OnMinionStruck` (MonsterAttackMonster); Gather / Heal / Frenzy / Sacrifice for the skills; timed records and
  the Fire Golem's pulse in `ProcessMinions`. The debug `army` now raises a Firebolt-throwing mage rank and an Iron
  Golem too.
- **The shared brain** (`CompanionOrders`) carries the missile a ranged one shoots.

## The ceilings the page broke, and what replaced them

Spell ids passed 254 (13 new ones on 246). Three things stored a spell in one byte, each guarded by a static_assert
or - worse - not:

1. `SpellMask` held 256 bits: a **fifth word** (ids 257-320), all operators, `GetSpellBitmask`.
2. The readied pair and the F-key hotkeys were saved as id+1 in a byte: **`HeroChunkReadiedSpells16` (15),
   `HeroChunkSpellHotkeys16` (16), `HeroChunkSpellHotkeysLeft16` (17)** - two bytes an entry, written after the
   one-byte forms and winning over them; the byte forms now store 0 rather than a wrapped id for a spell past 254.
   The remembered-spell options use the 16-bit form.
3. **Not asserted anywhere**: `HeroChunkSkillPoints`' one-byte COUNT wrapped `MAX_SPELLS` 259 to **3**, and the
   round-trip tests found every investment past the third spell gone. **`HeroChunkSkillPoints16` (18)** with a u16
   count; tag 1 still written with its first 255 entries for older builds. This is the kind of thing the "don't
   protect save formats" rule is for: the writehero hash moved once, deliberately, and the test says why.

Per-spell tables that assert their size (`SpellBand`, `SpellITbl`, `SpellsData`) got their thirteen rows.

## For the user's look in play

Necromancer, points in Raise Skeleton: kill something, cast at the body - it vanishes and a skeleton stands; the
count climbs with rank. A golem of each kind (colour, habit). Revive on a fallen monster (Essence 35, a timer on the
panel is NOT drawn yet - the Revived just falls apart after three minutes). Mages fire four kinds of bolt. Command,
Gather, Mending, Frenzy, Offering, Army. Then the stairs with a mixed army. A screenshot is the only verification.
