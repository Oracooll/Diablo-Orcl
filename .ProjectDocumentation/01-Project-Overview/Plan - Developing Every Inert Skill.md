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

### Round 4 — melee attack modifiers · ~19 rows

`oracool::ArmMeleeSkill` already exists and already carries a skill into `DoAttack`. It was written
for the Paladin's three and it is the mechanism the Barbarian's ten actives and the Monk's nine
strikes need. Round 4 generalises it: a per-swing effect with a cost, a condition and a result.

Unlocks: Bash, Stun, Double Swing, Concentrate, Frenzy, Whirlwind, Berserk, Leap, Leap Attack,
Double Throw; Sweeping Reed, Breaking Current, Vaulting Strike, Wheel of Heaven, Seven Reeds, Open
Palm, Hundred Fists, Radiant Palm, Purifying Breath.

### Round 5 — passives that are already possible · ~35 rows · CHEAPEST

`oracool/stat_sheet.h`'s `BonusProvider` already sums bonuses from items, auras and investment. Most
inert passives are one line of that: Dodge, Avoid, Evade, Pierce, the three masteries, Increased
Stamina, and twenty-two of the Monk's.

Deliberately NOT first, despite being cheapest per row. A passive is invisible - it changes a number
on a sheet - and thirty-five invisible rows going live in one round is thirty-five things that can be
subtly wrong with nothing on screen to show it. It goes after the rounds that produce something a
player can see, so the sheet can be read against effects that are known good.

### Round 6 — timed shouts and songs · ~18 rows

A buff with a duration, a radius and a stack rule. The Barbarian's nine cries and the Bard's songs
and auras are one mechanism with two vocabularies.

### Round 7 — javelins and throwing · 7 rows

The Rogue's third page. A thrown weapon that consumes ammunition and can carry a charge: Jab, Power
Strike, Poison Javelin, Impale, Charged Strike, Lightning Bolt, Plague Javelin, Fend, Lightning
Strike, Lightning Fury. Needs poison, which nothing else in the list needs - which is exactly why it
is late.

### Round 8 — the Paladin's remaining seven · 7 rows

Sacrifice, Holy Bolt, Vengeance, Conversion, Holy Freeze, Cleansing, Redemption. Small, and Holy
Freeze wants Round 1's chill, so it cannot be earlier than it looks.

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
