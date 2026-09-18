# The Necromancer, phase N11 - numbers, Roadmap and census (v1.12.041)

**Date:** 2026-09-18
**Plan:** [[Plan - The Necromancer]] section 8, phase N11 "Play pass, numbers, Roadmap and census"
**Build:** Debug v1.12.041 - 819 of 819 tests pass, no new warnings

The last phase of the plan has two halves. The play pass is the user's: N4 to N10 are all in the build and
none has been seen in play yet. This report is the other half - the four placeholders N10 left behind are
closed, every number the class runs on is written down in one place for the pass, the Roadmap card and the
Orcl Skill Census carry the class.

## The four placeholders, closed

| Was | Now |
|---|---|
| `BoneHitBurst` registered, never spawned | `BoneStrike` (rfa12_actives) shows the batch-38 burst on the struck monster's tile: every bone blow - Teeth, Splinters, Spear, Spikes, Wall, Prison, Storm, Death Nova - ends in it. Nothing spawns while the sheet is not in the archive. |
| Bone Spirit flew as the book spell's `sklball` | The book spell's missile and brain still fly (it hunts what it was aimed at, and finds another within 19 if that falls), but dressed in the delivered skull when `bone_spirit` is in the archive: `ProcessBoneSpirit` knows the sheet, turns it by `GetDirection16` (sixteen facings) and ends it in the bone-hit burst instead of `sklball`'s ninth, bursting, set. |
| No Essence on the character sheet | An "Essence" row under Mana, current beside maximum in the orb's green (`ColorOracoolGreen`). `CharRow` grew a last field, `visible`; a hidden row takes no height and draws nothing, and the layout is redone when the set of visible rows changes, so it is on the Necromancer's sheet alone. |
| The Revived had no clock on the army panel | The Revived row reads "3/10  2:47" - the time the first of them has left (`spec.ticksLeft`, the soonest). Untimed groups keep "n/cap". |

## The numbers sheet

Everything below is read from the code at v1.12.041, not from the plan. `r` is the skill's rank (tree
investment, 30 at most; a further rank of an active needs a level each), `p` the points in a tree passive.
Poison is `DamageType::Acid` at so many whole points a second.

### Summoning

| Skill | Pays | At rank r |
|---|---|---|
| Raise Skeleton | 6 mana (-30% with Commander of the Risen Dead) | 1 + (r-1)/3 skeletons, 8 at rank 22. Body: Skeleton axeman at 1, Tomb at 4, Raging at 7, Burning at 10. Life 20+6r (+8 a Skeleton Mastery point), damage (2+r/2)-(5+r) (+1/+2 a point), to-hit 60+4r (+2), AC 10+2r (+4 a Bone Plating point). |
| Raise Skeletal Mage | 8 mana (same discount) | Same count rule. Bowman bodies on the same ladder. Life 14+6r, damage (1+r/2)-(4+r), AC 6+2r; the four elements in turn - Firebolt, Charged Bolt, Acid, bone (Arrow). |
| Clay Golem | 10 mana | Life 100+30r, damage (4+2r)-(10+3r), AC 20+3r; a blow that lands chills 2 s. |
| Blood Golem | 14 mana | Life 80+25r, damage (6+2r)-(14+3r), AC 15+2r; its blows heal it and you. |
| Iron Golem | 18 mana | Life 120+35r, damage (8+3r)-(16+4r), AC 40+4r; returns a third of every blow it takes. |
| Fire Golem | 22 mana | Life 90+25r, damage (5+2r)-(12+3r), AC 15+2r; burns what stands beside it; fire heals it for half the damage. |
| Golem Mastery | tree | +15% life a point, +3 to-hit a point; golem to-hit 70+4r. |
| Revive | 35 Essence | 1 + (r-1)/3 revived, 10 at rank 28. Life = the corpse's, +5% a rank; 180 s, +30 s a Lasting Bond point, +25% with Extended Servitude. |
| Command the Dead | 2 mana | every minion turns on the target for 6+r s. |
| Gather the Dead | 4 mana | everything beyond 2 tiles is placed within 4 of you. |
| Dark Mending | 8 mana | every minion within 8 heals 20+4r % of its life. |
| Frenzy of the Dead | 10 mana | 10 s, +40+4r % minion damage. |
| Unholy Offering | 4 mana | the nearest minion to the cursor is unmade; you heal 30+3r % (75 at most) of its life. |
| Army of the Dead | 24 mana | six pulses over 3 s, each (4+2r)+d(5+2r) magic to everything within 2 of the cursor. |
| Summon Resist | tree | fire, lightning, magic on minions -20%, -5% more a point, 75% at most. |
| Bone Plating | tree | +4 AC a point on the next raised. |
| Skeleton Mastery, Lasting Bond | tree | as above. |

### Poison & Bone

Bone damage is magic, ×(100+8p)% for Marrow, +5% a tile flown with Serration (50 at most), and a second's
chill with Rigor Mortis. Poison lasts +25% and bites +10% deeper a Virulence point. "a-b, +c-d" is the roll
at rank 1 and its growth a rank.

| Skill | Pays | At rank r |
|---|---|---|
| Teeth | 4 mana | the three front-arc tiles, and 2+r monsters down the line within 6; 2-5, +1-2. |
| Bone Armor | 8 mana | absorbs 20+10r damage, for a minute or until spent. |
| Poison Dagger | 4 mana | 20 s: every melee blow poisons 4 s at 2+r a second. |
| Corpse Explosion | 10 Essence | a corpse within 3 of the cursor bursts: 40+5r % (100 at most) of its maximum life, 4 to 400, physical, to everything within 2. |
| Bone Splinters | 5 mana | up to three on the line within 4; 3-6, +1-2. |
| Blight | 7 mana | a pool within 8 for 4 s; each second everything within 1 is poisoned 2 s at 2+r. |
| Bone Wall | 10 mana | five tiles across the cast within 8, 8 s; twice a second whatever stands in one takes 3-6, +1-2, and is shoved back. |
| Bone Spikes | 8 mana | everything within 1 of the cursor: 4-9, +2-3, and a second's stagger. |
| Poison Explosion | 10 Essence | the corpse bursts into poison: everything within 2 poisoned 6 s at 3+r. |
| Bone Spear | 9 mana | everything on the line within 9: 6-12, +3-4. |
| Decompose | 9 mana | one monster: 5 s at 5+2r. |
| Bone Prison | 12 mana | one monster held 3 s; each second 2-5, +1-2. |
| Bone Storm | 14 mana | 8 s following you; twice a second everything within 2 takes 2-4, +1-1. |
| Bone Spirit | 14 mana | hunts the nearest within 19 and takes A THIRD OF ITS CURRENT LIFE (the book spell's own rule; the rank changes nothing). |
| Poison Nova | 16 mana | everything within 5 poisoned 6 s at 2+r. |
| Death Nova | 18 mana | everything within 4: 6-12, +2-4 magic now, then poison 5 s at 2+r. |

### Curses

Every curse is 25 Essence and no mana; Soul Harvest is 12 mana and no Essence. A curse lasts 8+r s, +20% a
Curse Mastery point (Eternal Torment: until the monster dies). Cast on everything within 2 of the cursor, +1
a Wide Malice point (5 at most); Attract and Death Mark on the one monster under it. One curse to a
monster; a new one replaces the old, except that nothing weaker replaces Doom.

| Curse | While it holds |
|---|---|
| Amplify Damage | physical damage taken +50+5r % (100 at most). |
| Dim Vision | the monster notices nothing that is not beside it. |
| Weaken | damage dealt -33%. |
| Frailty | dies at 10+r % of its life (25 at most). Not uniques. |
| Iron Maiden | every blow it lands on you or a minion comes back at 100+25r %. |
| Terror | runs from you. Not uniques. |
| Bane | 2+r acid a second. |
| Confuse | turns on whatever is nearest (the Barbarian's berserk flags). Not uniques. |
| Life Tap | 20+2r % (50 at most) of every blow landed on it heals the one who struck - you or the minion. |
| Attract | every monster near it makes it its target. |
| Decrepify | chilled, deals a quarter less, takes a fifth more. |
| Death Mark | when it dies, a Corpse Explosion of the curse's rank. |
| Lower Resist | fire, lightning, magic and poison taken +25+3r % (70 at most). |
| Doom | damage taken +15+2r % (45 at most); cannot be displaced by a weaker curse. |
| Essence Tap (tree) | 2+2p Essence for every cursed monster that dies. |
| Soul Harvest | every cursed monster within 6 takes (6+2r)-(12+4r) magic, and each gives you 5 Essence. |

### The passives (one rank, slotted)

Stand Alone -15% damage taken with no minions, 3% less for each, nothing at five. Swift Harvesting: fast
attack with a wand or scythe. Commander of the Risen Dead: raising -30% mana. Extended Servitude: the Revived
last a quarter longer. Rigor Mortis: a second's chill on every bone blow. Overwhelming Essence: 120 Essence.
Dark Reaping: 1 mana and 1 Essence for every blow that lands. Spreading Malediction: +5% damage a cursed
monster within 6, 30 at most. Eternal Torment: curses end with the monster. Final Service: a killing blow
unmakes the army and leaves you a quarter of your life, once a floor. Grisly Tribute: a tenth of every minion
blow heals you. Draw Life: a two-hundredth of your life a second for each monster within 4, five at most.
Serration and Marrow: above. Aberrant Animator: a fifth of the blows minions take goes back. Life from Death:
a death within 6 heals a twenty-fifth of your life. Fueled by Death: a corpse consumed is +30% speed for 4 s.
Blood is Power: one Essence for every twenty-fifth of your life lost. Rathma's Shield: below a fifth of your
life, nothing harms you for 4 s, once a floor.

### Essence

100 (120 with Overwhelming Essence), full in 20 s from empty, empty at the start of every game, never saved.
Four curses drain it; then one every five seconds.

## What the sheet says to look at in the play pass

Read from the numbers alone, before any of it is seen; nothing here was changed.

1. **Bone Spirit** is the only skill on the sheet the rank does not touch, and against a boss it is the
   strongest: a third of current life a cast, at 14 mana. Diablo II's is a flat roll. If it feels wrong in
   play, the fix is a `Scale` roll in the NecroBoneSpirit case and a damage type for `ProcessBoneSpirit`.
2. **Corpse Explosion** at 40% of a Hell-floor corpse's life within 2 for 10 Essence is D2's chain reaction:
   the corpse it makes is the next cast's fuel, and a Necromancer with a full pool casts ten in a row.
3. **The curses' price**: 25 of 100, so four before the pool is dry and then one every five seconds. That is
   the plan's number (D8); it is the one to feel out first, since every curse row hangs on it.
4. **Iron Maiden** returns 125% at rank 1; with the army taking the blows it is a kill-everything button
   against melee packs. **Life Tap** on the army is the counterpart.
5. **The counts**: 8 skeletons at rank 22, 10 Revived at rank 28, of a 30-rank cap. If the army should be
   full sooner, the rule is `RaisedCountAtRank` alone.
6. **Bone Wall's five segments** are laid at the delivered 64px cell width, and the notes warn the visible
   segment is narrower: gaps or overlaps between segments are an anchor question, not a skill one.

## Roadmap and census

- The Orcl Roadmap's card "The Necromancer" is In progress with the state at v1.12.041 and what is left
  (the play pass, batch 40, batch 39).
- The Orcl Skill Census is republished from `class_tree.cpp` at v1.12.041: 506 rows of seven heroes, 391
  built as written, 41 reworded (Life from Death and Blood is Power are the Necromancer's two - no health
  globes and no cooldowns in this engine), 2 retired, 72 in the hidden Bard. The Necromancer has no inert
  rows.
- The Road to Necromancy page: phase N11 is "doing" until the play pass has been through N4-N10.

## Not seen

None of the four closures on screen: the burst's place on the struck body (cell centre, no anchor offset),
the skull's facing order against the delivered rows (0 = S, clockwise), the Essence row's fit in the sheet
(the rows below it must close up on other heroes), the clock's width on the army panel's 156px.
