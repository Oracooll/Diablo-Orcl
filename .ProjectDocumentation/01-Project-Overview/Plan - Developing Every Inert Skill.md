# Plan: developing every inert skill, round after round

Written 2026-09-03, at v1.9.181, on the user's instruction: *"make a plan to develop all
undeveloped skills/spells/auras in the game and start developing them round after round."*

---

## 1. The real inventory

Counted from `Source/oracool/class_tree.cpp` rather than estimated, because the last three counts in
this project's notes were all wrong in the same direction.

| Class | Rows | Live | **Inert** |
|---|---:|---:|---:|
| Paladin | 31 | 24 | **7** |
| Sorcerer | 30 | 17 | **13** |
| Rogue | 30 | 3 | **27** |
| Barbarian | 30 | 7 | **23** |
| Bard | 21 | 8 | **13** |
| Monk | 39 | 8 | **31** |
| **Total** | **181** | **67** | **114** |

An inert row is listed, described, priced in points and struck through with a red X. It is not a
stub: nothing about it works, and `implemented = false` is what says so honestly in the UI.

By kind, the 114 are roughly **64 actives, 13 passives, 6 auras** in the five parsed classes, plus
the Monk's 31, which are **22 passives and 9 actives**.

## 2. The principle the order follows

**A round is a MECHANISM plus the rows it unlocks.** Not a class, not a page, not a numbered batch.

That is the whole strategy, and it comes from what the Paladin cost: Zeal, Hammer of Faith and
Shield Bash took one shared melee hook (`oracool/paladin_melee.cpp`) and then three cheap rows on top
of it. The seven rows that came after cost a fraction of the first. Every family below has the same
shape — one absent capability holding back eight to thirty rows.

So the rounds are ordered by **rows unlocked per mechanism built**, with two tie-breakers: art
already in hand wins, and a mechanism another round will need is built by the round that needs it
least.

## 3. The rounds

### Round 0 — the Cold art ✔ DONE (v1.9.181)

Thirteen sheets audited and ingested; `MissileFileData::LoadGFX` learned to read PNGs. Nothing casts
them yet. This round exists in the list because the next one depends on it and because it is the
proof the art pipeline works end to end.

### Round 1 — Cold damage ✔ DONE (v1.9.182)

`DamageType::Cold`, the chill (a monster loses every other tick), and Ice Bolt end to end. Found the
spell-id ceiling on the way: the four spell masks were uint64, so id 64 was the last spell that could
exist, and Ice Bolt was 59.

### Round 2 — the rest of the cold page ✔ DONE (v1.9.184)

Opened with the mechanism the ceiling demanded: `SpellMask`, 128 bits, low word persisted exactly as
before. Then nine rows: Ice Blast and Glacial Spike (freeze), Frost Nova, Blizzard, Frozen Orb, the
three armours (a shell the caster wears, and a reaction to whatever hits them), and Cold Mastery
(+6% cold damage a rank; pierces resistance from rank 3, ignores it from rank 6). The undead resist
cold on every difficulty, so Mastery has something to master from the first cathedral level.

Also found and fixed: a `SpellType::Skill` cast paid no mana at all, so Ice Bolt had been free for a
day. Tree skills with a price now pay it; vanilla's free skills stay free.

**Deferred to Round 3, deliberately:** Cold, Ice and Freezing Arrow. They are Rogue rows and they
are arrow *modifiers* — Round 3's mechanism — and building that mechanism for three rows now and
seven more later would be building it twice.

Not done, and worth knowing: the armours give no armour-class bonus yet (they react; they do not
harden), `ice_ground.png` and `ice_armor_break.png` are in the archive but nothing draws them, and
cold has monster *resistance* but no *immunity* — the data byte has no bit left for one.

### Round 3 — arrow modifiers ✔ DONE (v1.9.185)

The mechanism is the Paladin's latch, for a bow: a readied bow skill is *shot*, not cast — the
click becomes the ordinary ranged attack with a latch naming the skill, and `DoRangeAttack` looses
the skill's arrow(s) in place of the plain one. One missile family, five elements, the skill riding
in `var5` deciding what the arrow does when it stops. Ten rows: Magic, Fire, Cold, Multiple Shot,
Exploding, Ice, Guided (cannot miss), Strafe (nearest enemies in view), Immolation (leaves a fire
wall), Freezing (freezes the 3×3 where it stops). The last two Cold-pack sheets — `frost_arrow` and
`freezing_burst` — are in use, so all thirteen are.

Not done: no ammunition in this engine, so "costs no ammunition" is every arrow's; Guided does not
turn in flight, it simply cannot miss; Strafe fires its volley at once rather than in a sequence.

### (Round 1 as planned) — Cold damage · unlocks 13 rows · art READY

The engine has four damage types and none of them is cold. Everything the Sorceress's first page
promises, and three of the Rogue's arrows, is waiting on the same missing noun.

Build: `DamageType::Cold`; a cold resistance on the player and on monsters; **chill** (slowed) and
**freeze** (stopped) as monster states with a duration; the missile plumbing to carry them.
Then **Ice Bolt** end to end as the first user of all of it.

Unlocks: Ice Bolt, Ice Blast, Glacial Spike, Frost Nova, Blizzard, Frozen Orb, Frozen/Shiver/Chilling
Armor, Cold Mastery, Cold/Ice/Freezing Arrow.

### Round 2 — the rest of the cold line · 12 rows · art READY

Every remaining Sorceress cold row plus the three armours, each one a use of Round 1's foundation
and one of the thirteen sheets. The armours need a self-buff with a hit reaction, which Round 4
also wants — built here, where there are three customers for it instead of one.

### Round 3 — arrow modifiers · 10 rows

The Rogue's whole first page is "an arrow, but". One hook where a fired arrow's missile and damage
are chosen, and ten rows become data: Magic, Fire, Cold, Ice, Exploding, Immolation, Freezing,
Guided, Multiple Shot, Strafe. The same shape as the melee latch the Paladin already uses.

### Round 4 — melee attack modifiers ✔ DONE (v1.9.186)

`oracool::ArmMeleeSkill` already exists and already carries a skill into `DoAttack`. It was written
for the Paladin's three and it is the mechanism the Barbarian's ten actives and the Monk's nine
strikes need. Round 4 generalises it: a per-swing effect with a cost, a condition and a result.

Unlocks: Bash, Stun, Double Swing, Concentrate, Frenzy, Whirlwind, Berserk, Leap, Leap Attack,
Double Throw; Sweeping Reed, Breaking Current, Vaulting Strike, Wheel of Heaven, Seven Reeds, Open
Palm, Hundred Fists, Radiant Palm, Purifying Breath.

**Shipped:** seventeen of the nineteen, in `oracool/melee_skills.{h,cpp}` — a second latch beside
the Paladin's rather than a rewrite of it. Every skill is a profile (damage bonus and per-rank, extra
blows and their share) plus at most one reaction: knockback (Bash, Open Palm), stagger through
`StunMonster` with uniques exempt (Stun, Breaking Current), all-around strike at a share (Whirlwind,
Wheel of Heaven), the two tiles beside the target at full (Sweeping Reed), a kill that splashes its
blow onto every neighbour (Radiant Palm). Double Swing, Frenzy, Seven Reeds and Hundred Fists are
extra blows; Concentrate and Berserk are pure multipliers. The three leaps go through the engine's
own `MissileID::Teleport`, clamped to four tiles (+1 every three ranks); Leap Attack and Vaulting
Strike swing with their bonus when the target is adjacent and leap when it is not. Mana is paid
once per swing and only when the skill did something; an unaffordable skill is a plain swing.
The Barbarian's Berserk is `SpellID::BerserkBlow` because `SpellID::Berserk` is the vanilla spell
the Bard's row already uses. **Held back:** Double Throw and Purifying Breath — the first is a
thrown-weapon skill with no throwing weapons in the game, the second a breath cone that belongs
with Round 6's timed effects. Not built by design: Concentrate's uninterruptibility, Berserk's
defence penalty, Frenzy's speed, Whirlwind's travel.

### Round 5 — passives that are already possible ✔ DONE (v1.9.187)

`oracool/stat_sheet.h`'s `BonusProvider` already sums bonuses from items, auras and investment. Most
inert passives are one line of that: Dodge, Avoid, Evade, Pierce, the three masteries, Increased
Stamina, and twenty-two of the Monk's.

Deliberately NOT first, despite being cheapest per row. A passive is invisible - it changes a number
on a sheet - and thirty-five invisible rows going live in one round is thirty-five things that can be
subtly wrong with nothing on screen to show it. It goes after the rounds that produce something a
player can see, so the sheet can be read against effects that are known good.

**Shipped:** forty-five rows, in two homes. The sheet rows went where every sheet passive already
lives, `ApplyPassive` in class_tree.cpp: Divine Fortress, Tough as Nails, Perfectionist, Harmony,
Superstition, Glass Cannon, Holy Cause, Animosity, Astral Presence, Exalted Soul, Fanaticism, Fervor.
The rule rows - a chance, a condition read at the moment of a blow - went into a new
`oracool/passives.{h,cpp}` with seven hooks, each asked from one engine site: damage taken
(ApplyPlrDamage: Blur, Sixth Sense, Vigilant, Sword and Board, Relentless, Unwavering Will), damage
dealt (PlrHitMonst and MonsterMHit: Ruthless, Ambush, Brawler, Determination, Steady Aim, Audacity,
Power Hungry, Cold Blooded, Cull the Weak, Relentless Assault, Single Out, Rampage, Cadence), the
slips (MonsterAttackPlayer and PlayerMHit: Dodge, Evade, Avoid), Pierce (CheckMissileCol), the
once-a-minute saves (ApplyPlrDamage: Indestructible, Nerves of Steel, Awareness, Near Death
Experience), the returns (Leech on hit, Bloodthirst and Transcendence on mana spent, Requiem on a
kill nearby), and the tick (stillness for Unwavering Will and Brooding, Rampage's stacks, the save's
cooldown). Fleet Footed is one more reason IsClassTreeRunActive says yes. Every row's sentence now
states its number. **Held back**, and the rows still say "Not yet built": everything that needs
fury, wrath, hatred, spirit, cooldowns, songs, traps, grenades, rockets, mounts, laws, gems-in-gear
counting, a block report, or an attack-speed number the engine has no channel for - 36 Passive
Skills page rows, plus the Bard's two masteries and Throwing/Spear Mastery and Increased Stamina,
which are inert by engine and say so.

### Round 6 — timed shouts and songs ✔ DONE (v1.9.188)

A buff with a duration, a radius and a stack rule. The Barbarian's nine cries and the Bard's songs
and auras are one mechanism with two vocabularies.

**Shipped:** twenty-one rows, in `oracool/warcries.{h,cpp}`. A cry is cast like a spell — it has a
SpellID, a mana price, the cast animation and sound — and its one shared missile
(`MissileID::Warcry`) calls `CastWarcry` and is gone. What the cry then does is one of three
things: a timed buff on the caster kept in a per-player table that feeds the sheet through the
existing aura provider and forces a recompute when it starts and ends (Shout, Battle Orders,
Battle Command, Purifying Breath, Vengeance; Slow Missiles and Tranquility are timed but not on
the sheet); a timed debuff on the monsters that heard it, asked about at the point of use
(Battle Cry, Inner Sight — every player to-hit roll now reads `EffectiveMonsterArmor`, and
`MonsterAttackPlayer` reads the damage and aim debuffs); or an immediate reaction — Howl and
Daze on Sanctuary's own retreat channel, Taunt waking and turning everything in earshot, War Cry,
Lullaby, the Bard's Shout, Sound Shock and Temple Bell staggering through `StunMonster`. The
Bard's three song-auras and the Paladin's Holy Freeze are the same vocabulary held rather than
shouted: lit through the existing aura toggle, asked through a new `AuraPointsOn` beside
Conviction's query — Discord strips armour, Weaken blunts aim and chills, Dirge of Dread weakens
and repels, Holy Freeze chills. Uniques hold their ground against every repel and stagger.
**Held back:** Find Potion, Find Item, Grim Ward, Ode to Glory (corpses — Round 9), Decoy (an
entity), Cleansing and Redemption (no durations or corpses to work on).

### Round 7 — javelins and throwing ✔ DONE (v1.9.189)

The Rogue's third page. A thrown weapon that consumes ammunition and can carry a charge: Jab, Power
Strike, Poison Javelin, Impale, Charged Strike, Lightning Bolt, Plague Javelin, Fend, Lightning
Strike, Lightning Fury. Needs poison, which nothing else in the list needs - which is exactly why it
is late.

**Shipped:** eight of the ten. This engine has no javelin and no spear, and no poison, and the
round was built on what it does have. The six THRUSTS — Jab, Power Strike, Impale, Charged
Strike, Fend, Lightning Strike — are melee skills on Round 4's latch: a thrust is a swing with a
rule on it, and every one of them is a profile plus at most one reaction in
`oracool/melee_skills.cpp` (Jab three blows, Impale a double blow, Fend the Rogue's spin at four
fifths, Power Strike a lightning charge on the blow, Charged Strike the engine's own Charged Bolts
thrown off toward the target, Lightning Strike the engine's Chain Lightning launched through it).
The two THROWN rows — Lightning Bolt and Lightning Fury — are spells riding the engine's Lightning
and Nova at the rank, and their sentences say the bolt carries itself because nothing else would.
**Held back:** Poison Javelin and Plague Javelin (no poison), and the Barbarian's Double Throw (no
thrown weapons); all three rows say why.

### Round 8 — the Paladin's remaining seven ✔ DONE (v1.9.190)

Sacrifice, Holy Bolt, Vengeance, Conversion, Holy Freeze, Cleansing, Redemption. Small, and Holy
Freeze wants Round 1's chill, so it cannot be earlier than it looks.

**Shipped:** three rows; Vengeance and Holy Freeze had already gone live in Round 6. Sacrifice is
a melee-latch thrust (Round 4's shape): two and a half times the blow, a fifth more a rank, and a
twelfth of what it dealt taken from the Paladin's own life, never the last point. Holy Bolt is the
tree's OWN bolt — `SpellID::HolyBoltSkill` riding `MissileID::HolyBolt` at the rank — under a
SpellID of its own, so it cannot collide with the book spell again, which is what withdrew it in
August. Conversion is a targeted cry through Round 6's missile: one enemy near the cursor takes
the engine's Berserk flags for twenty seconds, two more a rank, and gives them back when the clock
runs out — the clock being what the first attempt lacked. **Held back:** Cleansing (nothing on a
player here has a duration to shorten) and Redemption (corpses); both rows still say why.

### Round 9 — the one-offs

Decoy, Grim Ward, Find Item, Find Potion, Inner Sight, Slow Missiles, Taunt. Each is its own
mechanism serving one row, which is the definition of last.

## 4. What every round owes

Not negotiable, and written here so a round cannot quietly skip one:

1. **A row stops being inert only when it WORKS.** `implemented = true` is a promise the red X was
   keeping honestly; flipping it early is worse than leaving it.
2. **A test per mechanism**, not per row. The rows are data; the mechanism is the code.
3. **The tooltip's numbers come from the same function the effect does.** The fork has been bitten
   three times by a sheet reporting a number the game does not use.
4. **A dev report per round** in `02-Development-Reports`, and the pipeline row updated.
5. **The user plays it before the round is called done.** A screenshot is the only verification for
   anything drawn.

## 5. Sequencing note

Rounds 1-2 are art-led: the sheets are in hand and idle. Rounds 3-4 are mechanism-led and each
unlocks more rows than any other single piece of work available. Round 5 is the largest single jump
in row count and the least visible, which is why it sits in the middle rather than at either end.
