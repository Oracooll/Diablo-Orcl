# The 114 RfA-12 actives are built: all 162 new skills now work

2026-09-13 — v1.11.112

## Why

The last unit of the RfA-12 implementation. v1.11.108 put the 162 skills in the trees, v1.11.109 built
the 48 that need no spell slot, v1.11.110 widened `SpellID`, and v1.11.111 gave the 114 actives their ids.
This builds those 114: every new skill on every class page can now be learned, readied, cast or swung,
and grants its level-up stat.

## How it is built

One new module, `oracool/rfa12_actives.{h,cpp}`, two ways in - decided by each skill's `SpellsData` row:

- **Swung (20).** `MissileID::Null`. The click becomes the ordinary attack with a latch naming the skill,
  exactly Round 4's mechanism on a latch of its own (`ArmRfa12Melee`), so `ClassMeleeSkill` is untouched.
  `PlrHitMonst` adds `Rfa12MeleeDamagePercent`; `DoAttack` calls `ApplyRfa12MeleeOnSwing` beside the
  Round 4 hook. Arming the Round 4 latch drops this one, so only one is ever armed. Mana is paid when the
  swing does something, as every tree skill pays.
- **Cast (94, the Rogue's 8 bow skills among them).** `MissileID::Warcry`: the engine does the cast
  animation, the mana check and the aim, and `AddWarcry` now calls `CastRfa12Active` when the spell is not
  a cry. False fizzles the cast and costs nothing. Bow skills refuse without a bow in hand. Only the
  shouts leave the cry's floor ring.

What a skill leaves behind is state in the module, per player and per monster slot:

- **Buffs** on the hero (Iron Will, Conduit, Saga, Astral Projection on the sheet; Static Charge,
  Retribution, Evasion, Bloodcall, Feedback, Chord of Warding, Clarity, Rallying Cry, Immolate, Music of
  the Spheres off it).
- **Marks** on monsters: Judgment, Oathbrand, Hunter's Mark, Frostbite, Tragedy, Satire, Ashen Brand,
  Exploding Palm, Cinder Touch's burn, Elegy, Dissonant Thread, and a few cooldowns. Bleeding, blocked
  regeneration, scent and a taken corpse reuse `rfa12_effects`' marks through four small setters.
- **Ground effects**, up to 32 at once: Earthquake, Brittle Ground, Frozen Sentinel, Whiteout, Ball
  Lightning, Lightning Rod, Faraday Ring, Storm Crucible, Ember Mine, Furnace Mouth, Firestorm, Tuning
  Fork, Ancestral Court. Plus the per-hero sequences: Wrath of the Heavens' pillars, Funeral Star's
  channel, Storm Crucible's first conductor, Staff of Echoes' second blow, Hunter's Claim, the summon's
  thirty seconds, and the landing of the three leaps that strike when they arrive.

Every question the engine asks goes through `oracool/rfa12_effects.h`, which forwards to the module. New
hook sites were added for what the 48 did not need: Chord of Warding's absorb in `ApplyPlrDamage`,
Mantra of Evasion beside Dodge, Satire in `EffectiveResistances`, Hunter's Claim at the top of
`MonsterMHit`, Feedback after both player-damage paths of `PlayerMHit`, Frostbite's extra cold on every
cold missile, and the buffs beside the warcries' in the sheet provider. State is cleared with the
effects' own, and the hero's buffs at a new game beside the passive clocks.

The 114 rows now carry descriptions with their real numbers, and are built.

## Built on what the engine has

Where a skill's idea needed a system this engine lacks, it was built on the nearest real one, and the
row describes what was built:

- **Visuals.** No new art: the shouts and bursts use the cry's floor ring; Ball Lightning throws the
  engine's own Charged Bolts, Frozen Sentinel its Ice Bolts, Firestorm its Fireballs. The bow skills
  resolve at the cast frame rather than loosing a visible arrow.
- **Summons** (Ancestral Call, Spirit Guardian) are the engine's Golem at the skill's rank, dismissed after
  thirty seconds.
- **Leaps and steps** are the engine's Teleport.
- **Clasp of Ruin** holds the enemy in place rather than dragging it past you; **Harpoon** drags it one
  step, the engine's knockback turned round.
- **Deafening Roar** holds monsters still rather than silencing only casters - the engine has no
  "cannot cast" state.
- **Symphony of War** (v1.11.109) and **Grand Finale** read "a song is playing" as the one song a Bard can
  play.

## Tests

The RfA-12 shape test now requires all 114 actives built on their own spell ids (and still exactly the 48
rules).

One existing test moved with the build, and it was the test's data rather than the game:
`NetPackTest.UnPackNetPlayer_doesNotSpillPastTheBookLevels` wrote canary bytes into every skill-investment
slot of the unpacking player. From v1.11.112 a byte in an RfA-12 active's slot is a real investment with a
level-up stat, so the recomputed sheet no longer matched the packing player's and the unpack refused the
packet before the spill check ran. The canaries are now zero from the first RfA-12 id, and all 240 slots are
still checked untouched. (The net pack has never carried skill investment, so a tree stat could always
differ between peers; V1 is single-player.)

Full suite: 740/740.
