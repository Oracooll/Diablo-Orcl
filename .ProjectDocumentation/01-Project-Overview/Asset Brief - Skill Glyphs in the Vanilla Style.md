# Asset Brief - Skill Glyphs in the Vanilla Style

**Date:** 2026-09-05 · One ChatGPT brief for every skill, spell, aura, passive and cry this fork introduced - 255 glyphs - to be drawn in the style of Diablo's own spell icons and placed by the game on the vanilla 56x56 plate (the blank frame 26 of the spell icon sheet). The 18 rows that are vanilla spells keep vanilla's icons and are not listed.

Attach `99-original-game-art/png/spelicon/spelicon_frame00.png`, `04.png`, `19.png` (Fire Bolt, Fireball, Flash - white glyph on the gold plate) and `spelicon_frame26.png` (the blank plate) as the style reference. Send the STYLE block once and the list in batches of a page each; ask for a contact sheet per batch and single files.

## The measurements (from the vanilla 56px sheet)

- Frame 56 x 56. The plate's bevel takes the outer 8 px; the glyph lives in the middle **40 x 40** (x 8-47, y 8-47). Vanilla glyphs span 35-43 px wide and 26-44 tall inside that box, never touching the bevel.
- The glyph is FLAT white, one colour - (243,243,243) - with no shading, no gradient, no anti-aliasing: hard pixel edges, as palette art.
- Every glyph carries a hard black shadow of its own silhouette, offset about **2 px to the left and 1 px up** (vanilla's light comes from the lower right). Black, (12,7,7) to (20,11,0), one colour, no blur.
- A few glyphs use two mid greys - (184,184,184) and (133,133,133) - for a second plane (a blade's edge, an inner ring). Two greys at most; most glyphs are pure white and black.
- The small 37 x 38 sheet is the same glyph at 30 x 32. Not needed: the game scales the 56 for the wells.

## STYLE block (send with every batch)

```text
Draw skill icons in the exact style of Diablo 1's spell icons (reference attached): a flat, single-colour
WHITE pictogram - bold, simple, readable at 56 px, no shading, no gradients, no outlines, no anti-aliasing,
hard pixel edges - with a hard BLACK shadow of the same silhouette offset 2 px left and 1 px up, and nothing
else. Transparent background: NO plate, NO frame, NO bevel - the game draws the gold plate itself and puts
the glyph on it. At most two extra greys (184 and 133) for a second plane when a shape needs it.

FORMAT: one PNG per icon, 56 x 56, true alpha, the glyph centred in the middle 40 x 40 (leave 8 px clear on
every side). Name each file exactly as listed (kebab-case). Also give a contact sheet of the batch, glyphs
at 56 on a mid-grey ground, labelled, so I can check them before you write the files.

Subject: the pictogram should say what the skill DOES in one shape - a weapon, a hand, a bolt, a ring, a
figure - the way Fire Bolt is a comet and Flash is a hand. No text, no numbers, no class emblems.
```

## Paladin

### Paladin - COMBAT SKILLS (11)

| File | Name | Kind | What it does |
|---|---|---|---|
| `sacrifice.png` | Sacrifice | Active | A blow two and a half times as hard, a fifth more a rank, that costs you a twelfth of what it dealt. It cannot take your last point of life. |
| `smite.png` | Smite | Active | Bash with your shield: it always connects and briefly stuns. A shield is mandatory. |
| `holy-bolt.png` | Holy Bolt | Active | A bolt of holy energy that sears the undead, at the rank. The tree's own bolt, beside the book's. |
| `zeal.png` | Zeal | Active | Strike several times in one furious burst. Skill levels 1, 3 and 5 each add a strike, and every skill level adds +1% chance to hit. |
| `charge.png` | Charge | Active | Rush an enemy and land a running blow. |
| `vengeance.png` | Vengeance | Active | Your blows burn and crackle for thirty seconds, five more a rank: fire and lightning on every hit. Cold has no place on the weapon sheet, so it is not added. |
| `hammer-of-faith.png` | Hammer of Faith | Active | A heavy swing whose force splashes over everything around your target. |
| `blessed-hammer.png` | Blessed Hammer | Active | Looses a spinning hammer that wheels outward through anything in its path. |
| `blessed-shield.png` | Blessed Shield | Active | Hurls your shield at a crowd, striking several of them before it returns. A shield is mandatory. |
| `conversion.png` | Conversion | Active | Turns one enemy near the cursor to your side for twenty seconds, two more a rank. Uniques and the magic-immune refuse. |
| `fist-of-the-heavens.png` | Fist of the Heavens | Active | Calls down a bolt from the sky, which bursts into holy energy where it lands. |

### Paladin - OFFENSIVE AURAS (10)

| File | Name | Kind | What it does |
|---|---|---|---|
| `might.png` | Might | Aura | Increases the damage you deal. |
| `holy-fire.png` | Holy Fire | Aura | Wreathes your weapon in flame, adding fire damage to every blow. |
| `thorns.png` | Thorns | Aura | Returns damage to whatever strikes you. This engine's thorns is a flat return, so points light it rather than growing it. |
| `blessed-aim.png` | Blessed Aim | Aura | Steadies your hand, raising your chance to hit. |
| `concentration.png` | Concentration | Aura | Raises damage and steadies you against interruption. |
| `holy-freeze.png` | Holy Freeze | Aura | A cold that chills everything standing in it, slowing its step and its swing. |
| `holy-shock.png` | Holy Shock | Aura | Charges your weapon, adding lightning damage to every blow. |
| `sanctuary.png` | Sanctuary | Aura | Hallows the ground you stand on: nearby undead break and flee from you. Champions are too proud to run. |
| `fanaticism.png` | Fanaticism | Aura | Drives you to strike faster, harder and truer. |
| `conviction.png` | Conviction | Aura | Strips the resistances of every enemy near you, and at five points begins to break their immunities down into mere resistances. |

### Paladin - DEFENSIVE AURAS (10)

| File | Name | Kind | What it does |
|---|---|---|---|
| `prayer.png` | Prayer | Aura | Mends your wounds steadily as you walk. |
| `resist-fire.png` | Resist Fire | Aura | Hardens you against fire. |
| `defiance.png` | Defiance | Aura | Raises your armour class. |
| `resist-cold.png` | Resist Cold | Aura | Hardens you against cold. No cold exists here, so it wards against magic instead. |
| `cleansing.png` | Cleansing | Aura | Shortens poison and curses. Inert: this engine tracks no duration for either. |
| `resist-lightning.png` | Resist Lightning | Aura | Hardens you against lightning. |
| `vigor.png` | Vigor | Aura | Quickens your stride: you run instead of walking, wherever you are. |
| `meditation.png` | Meditation | Aura | Restores your mana steadily as you walk. |
| `redemption.png` | Redemption | Aura | Once a second the nearest corpse in the field is consumed for a fiftieth of your life and mana, a hundredth more a point. |
| `salvation.png` | Salvation | Aura | Wards you against fire, lightning and magic alike. |

### Paladin - PASSIVE SKILLS (18)

| File | Name | Kind | What it does |
|---|---|---|---|
| `heavenly-strength.png` | Heavenly Strength | Passive | Bear a two-handed weapon in your main hand and a shield in the other. Not yet built. |
| `fervor.png` | Fervor | Passive | With a one-handed weapon in hand you swing faster. |
| `vigilant.png` | Vigilant | Passive | Every blow that is not steel - fire, lightning, magic - hurts a fifth less. |
| `righteousness.png` | Righteousness | Passive | Your opening strikes build wrath faster, and you hold more of it. Not yet built. |
| `insurmountable.png` | Insurmountable | Passive | Every blow you turn aside feeds your wrath. Not yet built. |
| `fanaticism.png` | Fanaticism | Passive | Your simplest attacks swing faster. |
| `indestructible.png` | Indestructible | Passive | Once a minute a killing blow leaves you standing at a third of your life instead. |
| `holy-cause.png` | Holy Cause | Passive | Your weapon bites a tenth deeper. |
| `wrathful.png` | Wrathful | Passive | Spent wrath returns to you as life. Not yet built. |
| `divine-fortress.png` | Divine Fortress | Passive | Behind a shield your armour is a quarter greater. |
| `lord-commander.png` | Lord Commander | Passive | Your mount, your bombardment and your phalanx all answer sooner and hit harder. Not yet built. |
| `hold-your-ground.png` | Hold Your Ground | Passive | You no longer dodge at all, and block far more. Not yet built. |
| `long-arm-of-the-law.png` | Long Arm of the Law | Passive | Every law you declare holds its power longer. Not yet built. |
| `iron-maiden.png` | Iron Maiden | Passive | What strikes you is returned with far greater interest. Not yet built. |
| `renewal.png` | Renewal | Passive | Each blow turned aside returns a measure of life. Not yet built. |
| `finery.png` | Finery | Passive | Every gem set into your gear lends you strength. Not yet built. |
| `blunt.png` | Blunt | Passive | Justice and the blessed hammer fall heavier. Not yet built. |
| `towering-shield.png` | Towering Shield | Passive | Every skill worked through your shield strikes harder and readies sooner. Not yet built. |

## Barbarian

### Barbarian - COMBAT SKILLS (10)

| File | Name | Kind | What it does |
|---|---|---|---|
| `bash.png` | Bash | Active | A heavy blow: a third again as hard, a tenth more a rank, and it knocks the target back. |
| `leap.png` | Leap | Active | Vault to the spot under the cursor, over anything in the way - four tiles, a tile further every three ranks. |
| `double-swing.png` | Double Swing | Active | Two blows in one swing, the second at three quarters, a twentieth more a rank. |
| `stun.png` | Stun | Active | A blow that leaves the target reeling for a second and a half, a fifth longer a rank. Uniques shrug it off. |
| `double-throw.png` | Double Throw | Active | Hurl both thrown weapons at once. Inert: this engine has no thrown weapons. |
| `leap-attack.png` | Leap Attack | Active | Leap onto a distant enemy; the blow you land there is half again as hard, a tenth more a rank. |
| `concentrate.png` | Concentrate | Active | A focused blow half again as hard, a tenth more a rank. The steadiness half is not built yet. |
| `frenzy.png` | Frenzy | Active | Two blows in one swing, both at full force, a tenth more a rank. |
| `whirlwind.png` | Whirlwind | Active | Every swing strikes everything around you, at two thirds, a twentieth more a rank. You stand your ground rather than travelling. |
| `berserk.png` | Berserk | Active | A blow twice as hard, a fifth more a rank. The defence you would trade for it is not taken yet. |

### Barbarian - COMBAT MASTERIES (10)

| File | Name | Kind | What it does |
|---|---|---|---|
| `sword-mastery.png` | Sword Mastery | Passive | Sharpens your aim and your blow with any sword held. |
| `axe-mastery.png` | Axe Mastery | Passive | Sharpens your aim and your blow with any axe held. |
| `mace-mastery.png` | Mace Mastery | Passive | Sharpens your aim and your blow with any mace or club held. |
| `pole-arm-mastery.png` | Pole Arm Mastery | Passive | Sharpens your aim and your blow with a staff - this engine's nearest pole arm. |
| `throwing-mastery.png` | Throwing Mastery | Passive | Mastery of thrown weapons. Inert: this engine has none. |
| `spear-mastery.png` | Spear Mastery | Passive | Mastery of spears. Inert: this engine has no spear type. |
| `increased-stamina.png` | Increased Stamina | Passive | Lengthens your wind. Inert: this engine tracks no stamina. |
| `iron-skin.png` | Iron Skin | Passive | Toughens your hide, raising armour class. |
| `increased-speed.png` | Increased Speed | Passive | You run rather than walk, wherever you are. |
| `natural-resistance.png` | Natural Resistance | Passive | Hardens you against fire, lightning and magic alike. |

### Barbarian - WARCRIES (10)

| File | Name | Kind | What it does |
|---|---|---|---|
| `howl.png` | Howl | Active | A howl that sends everything in earshot running, four tiles and a tile more a rank. Uniques hold their ground. |
| `find-potion.png` | Find Potion | Active | Search a corpse near the cursor. Half the time, a twentieth more a rank, it yields a potion - rarely a full one. The corpse is used up. |
| `taunt.png` | Taunt | Active | A goad that wakes everything in earshot and turns it on you. |
| `shout.png` | Shout | Active | A bellow that hardens you: half again your armour, a tenth more a rank, for forty seconds and five more a rank. |
| `find-item.png` | Find Item | Active | Search a corpse near the cursor. A quarter of the time, a twentieth more a rank, it yields an item. The corpse is used up. |
| `battle-cry.png` | Battle Cry | Active | A cry that leaves what hears it a quarter weaker in blow and in armour for twenty-four seconds. |
| `battle-orders.png` | Battle Orders | Active | A shout that swells your life and mana by twenty, ten more a rank, for forty seconds and five more a rank. |
| `grim-ward.png` | Grim Ward | Active | Raise a corpse near the cursor as a totem of terror: for twenty seconds, two more a rank, everything but the uniques that comes near it runs. |
| `war-cry.png` | War Cry | Active | A shout that strikes everything in earshot for four to eight a rank and leaves it reeling for two seconds. Uniques shrug off the reeling. |
| `battle-command.png` | Battle Command | Active | A command that deepens every skill you have by a rank for thirty seconds, five more a rank. |

### Barbarian - PASSIVE SKILLS (19)

| File | Name | Kind | What it does |
|---|---|---|---|
| `pound-of-flesh.png` | Pound of Flesh | Passive | Healing taken from the fallen leaves you mending and quickened, and it stacks. Not yet built. |
| `ruthless.png` | Ruthless | Passive | You fall two fifths harder on anything below a third of its life. |
| `nerves-of-steel.png` | Nerves of Steel | Passive | Once a minute a killing blow leaves you standing at a third of your life instead. |
| `weapons-master.png` | Weapons Master | Passive | Each family of weapon lends its own gift - damage, precision, speed or fury. Not yet built. |
| `inspiring-presence.png` | Inspiring Presence | Passive | Your shouts hold twice as long and leave everyone near you mending. Not yet built. |
| `berserker-rage.png` | Berserker Rage | Passive | Near the height of your fury you strike far harder. Not yet built. |
| `bloodthirst.png` | Bloodthirst | Passive | Half of every point of mana you spend returns as life. |
| `animosity.png` | Animosity | Passive | You hold twenty more mana. |
| `superstition.png` | Superstition | Passive | Fire, lightning and magic all find you a tenth harder to harm. |
| `tough-as-nails.png` | Tough as Nails | Passive | Your armour is a quarter greater. |
| `no-escape.png` | No Escape | Passive | What you throw and what you hurl lands harder on the distant. Not yet built. |
| `relentless.png` | Relentless | Passive | Below a third of your life, every blow lands a quarter softer. |
| `brawler.png` | Brawler | Passive | With three or more enemies pressing close, everything you do hurts a fifth more. |
| `juggernaut.png` | Juggernaut | Passive | What would hold you fast holds you half as long, and may give you back your life. Not yet built. |
| `unforgiving.png` | Unforgiving | Passive | Your fury no longer ebbs when the fighting stops - it rises. Not yet built. |
| `boon-of-bul-kathos.png` | Boon of Bul-Kathos | Passive | Your earthquake, your ancients and your berserking all return far sooner. Not yet built. |
| `earthen-might.png` | Earthen Might | Passive | Splitting the ground fills you with fury. Not yet built. |
| `sword-and-board.png` | Sword and Board | Passive | Behind a shield you take three tenths less harm. |
| `rampage.png` | Rampage | Passive | Every kill lends a twentieth more damage for five seconds, stacking five high. |

## Sorceress

### Sorceress - COLD SPELLS (10)

| File | Name | Kind | What it does |
|---|---|---|---|
| `ice-bolt.png` | Ice Bolt | Active | A shard of ice that damages and chills what it hits, halving its speed for two seconds. |
| `frozen-armor.png` | Frozen Armor | Active | Armour of ice: for a while, whatever strikes you in melee is frozen in place. |
| `frost-nova.png` | Frost Nova | Active | A ring of ice bursting out from you, chilling and damaging everything near. |
| `ice-blast.png` | Ice Blast | Active | A heavier shard that freezes its target solid for a moment. Uniques are chilled instead. |
| `shiver-armor.png` | Shiver Armor | Active | Armour of ice: for a while, whatever strikes you in melee is chilled and cut by cold. |
| `glacial-spike.png` | Glacial Spike | Active | A spear of ice that freezes what it strikes and shatters, chilling everything beside it. |
| `blizzard.png` | Blizzard | Active | Ice falls over an area for a few seconds, chilling and damaging whatever stands in it. |
| `chilling-armor.png` | Chilling Armor | Active | Armour of ice: for a while, whatever hits you - near or far - is chilled and answered with an ice bolt. |
| `frozen-orb.png` | Frozen Orb | Active | An orb that drifts toward its mark shedding ice bolts, then bursts into a ring of them. |
| `cold-mastery.png` | Cold Mastery | Passive | Every rank adds 6% to all cold damage. From rank 3 a resisting monster keeps only half its protection; from rank 6, none. |

### Sorceress - LIGHTNING SPELLS (3)

| File | Name | Kind | What it does |
|---|---|---|---|
| `static-field.png` | Static Field | Active | Strips a share of the life from everything near. Inert: no analogue exists here. |
| `thunder-storm.png` | Thunder Storm | Active | A storm that strikes on its own as you fight. Inert: no analogue exists here. |
| `lightning-mastery.png` | Lightning Mastery | Passive | Your blows carry lightning, and lightning troubles you less. Not D2's spell scaling: this engine deepens a spell by its LEVEL, and has no per-element channel to raise. |

### Sorceress - FIRE SPELLS (4)

| File | Name | Kind | What it does |
|---|---|---|---|
| `warmth.png` | Warmth | Passive | Your mana returns of its own accord. |
| `enchant.png` | Enchant | Passive | Your weapon burns: every blow carries fire. A passive rather than a cast buff, since a tree skill with no spell slot has no way to be cast. |
| `meteor.png` | Meteor | Active | Calls a burning rock down from the sky. Inert: no analogue exists here. |
| `fire-mastery.png` | Fire Mastery | Passive | Fire burns for you and less against you. Not D2's spell scaling: this engine deepens a spell by its LEVEL, and has no per-element channel to raise. |

### Sorceress - PASSIVE SKILLS (18)

| File | Name | Kind | What it does |
|---|---|---|---|
| `power-hungry.png` | Power Hungry | Passive | You deal a fifth more harm to anything five tiles away or further. |
| `blur.png` | Blur | Passive | Everything that strikes you strikes a sixth softer. |
| `evocation.png` | Evocation | Passive | Every cooldown you carry comes round sooner. Not yet built. |
| `glass-cannon.png` | Glass Cannon | Passive | You hit fifteen percent harder and wear a tenth less armour. |
| `prodigy.png` | Prodigy | Passive | Your simplest spells give back arcane power as you cast them. Not yet built. |
| `astral-presence.png` | Astral Presence | Passive | You hold twenty more mana. |
| `illusionist.png` | Illusionist | Passive | A heavy blow resets your escapes and speeds your step. Not yet built. |
| `cold-blooded.png` | Cold Blooded | Passive | Anything chilled or frozen takes a tenth more harm from you. |
| `conflagration.png` | Conflagration | Passive | What you set alight becomes easier to strike truly. Not yet built. |
| `paralysis.png` | Paralysis | Passive | Your lightning may stun everything it touches. Not yet built. |
| `galvanizing-ward.png` | Galvanizing Ward | Passive | Go unharmed a moment and a ward forms around you. Not yet built. |
| `temporal-flux.png` | Temporal Flux | Passive | Arcane harm slows what it touches to a crawl. Not yet built. |
| `dominance.png` | Dominance | Passive | Every kill lays another shell of shielding over you. Not yet built. |
| `arcane-dynamo.png` | Arcane Dynamo | Passive | Five simple spells charge the next great one. Not yet built. |
| `unstable-anomaly.png` | Unstable Anomaly | Passive | A killing blow throws up a vast ward and scatters what stands near. Not yet built. |
| `unwavering-will.png` | Unwavering Will | Passive | Stand still a moment and blows land a fifth softer on you while yours land a tenth harder. |
| `audacity.png` | Audacity | Passive | You deal fifteen percent more harm to anything within two tiles. |
| `elemental-exposure.png` | Elemental Exposure | Passive | Striking with a new element leaves the target more open to all of them. Not yet built. |

## Rogue

### Rogue - BOW & CROSSBOW (10)

| File | Name | Kind | What it does |
|---|---|---|---|
| `magic-arrow.png` | Magic Arrow | Active | An arrow of pure force: your bow damage as magic, plus a little a rank. |
| `fire-arrow.png` | Fire Arrow | Active | An arrow wrapped in flame: your bow damage as fire, plus a little a rank. |
| `cold-arrow.png` | Cold Arrow | Active | An arrow sheathed in frost: your bow damage as cold, and it chills what it hits. |
| `multiple-shot.png` | Multiple Shot | Active | Looses a fan of arrows at once - two, and one more every two ranks. |
| `exploding-arrow.png` | Exploding Arrow | Active | A fire arrow that bursts where it stops, burning the tiles around it. |
| `ice-arrow.png` | Ice Arrow | Active | A frost arrow that freezes what it hits solid for a moment. |
| `guided-arrow.png` | Guided Arrow | Active | An arrow that cannot miss. |
| `strafe.png` | Strafe | Active | One arrow at each enemy in view, nearest first - three, and one more every two ranks. |
| `immolation-arrow.png` | Immolation Arrow | Active | A fire arrow that leaves a wall of flame burning where it stops. |
| `freezing-arrow.png` | Freezing Arrow | Active | A frost arrow that freezes everything around where it stops. |

### Rogue - PASSIVE & MAGIC (9)

| File | Name | Kind | What it does |
|---|---|---|---|
| `inner-sight.png` | Inner Sight | Active | Reveals the weak points of everything in earshot: a third of its armour gone, two percent more a rank, for twenty seconds. |
| `critical-strike.png` | Critical Strike | Passive | A chance to strike for double. This engine has no critical roll, so it raises your damage instead. |
| `dodge.png` | Dodge | Passive | A chance to slip a blow while standing: a tenth, a twenty-fifth more a rank, two fifths at most. |
| `slow-missiles.png` | Slow Missiles | Active | For twenty seconds, four more a rank, half the arrows aimed at you turn aside, a twentieth more a rank. |
| `avoid.png` | Avoid | Passive | A chance to slip an arrow: a tenth, a twenty-fifth more a rank, two fifths at most. |
| `penetrate.png` | Penetrate | Passive | Sharpens your aim with anything you wield. |
| `decoy.png` | Decoy | Active | A double of yourself to draw fire. Not yet built. |
| `evade.png` | Evade | Passive | A chance to slip a blow while moving: a tenth, a twenty-fifth more a rank, two fifths at most. |
| `pierce.png` | Pierce | Passive | Your arrows may carry on through what they strike: fifteen percent, a twentieth more a rank, three fifths at most. |

### Rogue - JAVELIN & SPEAR (10)

| File | Name | Kind | What it does |
|---|---|---|---|
| `jab.png` | Jab | Active | Three quick thrusts in one motion, the second and third at half, a twentieth more a rank. |
| `power-strike.png` | Power Strike | Active | A thrust a third again as hard, a twentieth more a rank, with one to four lightning a rank on top of it. |
| `poison-javelin.png` | Poison Javelin | Active | A javelin trailing venom. Inert: this engine has no poison. |
| `impale.png` | Impale | Active | A savage thrust twice as hard, a fifth more a rank. |
| `charged-strike.png` | Charged Strike | Active | A thrust a fifth harder that throws off two charged bolts toward the target, one more every two ranks. |
| `lightning-bolt.png` | Lightning Bolt | Active | Hurl a bolt of lightning that races along the ground toward the target, at the rank. No javelin exists here; the bolt carries itself. |
| `plague-javelin.png` | Plague Javelin | Active | A javelin trailing a cloud of pestilence. Inert: this engine has no poison. |
| `fend.png` | Fend | Active | Every swing also strikes everything around you, at four fifths, a twentieth more a rank. |
| `lightning-strike.png` | Lightning Strike | Active | A thrust a fifth harder whose lightning leaps on from the target to the next enemy, and the next. |
| `lightning-fury.png` | Lightning Fury | Active | Hurl lightning that bursts outward in every direction at once, at the rank. |

### Rogue - PASSIVE SKILLS (19)

| File | Name | Kind | What it does |
|---|---|---|---|
| `thrill-of-the-hunt.png` | Thrill of the Hunt | Passive | What your heavier shots strike is slowed almost to a stop. Not yet built. |
| `tactical-advantage.png` | Tactical Advantage | Passive | Every evasion leaves you running far faster. Not yet built. |
| `blood-vengeance.png` | Blood Vengeance | Passive | You hold more hatred, and the fallen restore both hatred and discipline. Not yet built. |
| `steady-aim.png` | Steady Aim | Passive | With nothing within three tiles of you, everything you do hurts a fifth more. |
| `cull-the-weak.png` | Cull the Weak | Passive | You fall a fifth harder on anything chilled or frozen. |
| `night-stalker.png` | Night Stalker | Passive | Your opening shots build hatred faster. Not yet built. |
| `brooding.png` | Brooding | Passive | Stand still a moment and your wounds close, a hundredth of your life a second. |
| `hot-pursuit.png` | Hot Pursuit | Passive | Landing a blow leaves you moving faster. Not yet built. |
| `archery.png` | Archery | Passive | Each kind of bow lends its own gift - damage, precision or hatred. Not yet built. |
| `numbing-traps.png` | Numbing Traps | Passive | Anything you have slowed strikes back far weaker. Not yet built. |
| `perfectionist.png` | Perfectionist | Passive | Your armour is a tenth greater and every resistance ten points higher. |
| `custom-engineering.png` | Custom Engineering | Passive | Your traps and sentries last twice as long and you may set more. Not yet built. |
| `grenadier.png` | Grenadier | Passive | Your grenades hit harder, burst wider, and one falls when you do. Not yet built. |
| `sharpshooter.png` | Sharpshooter | Passive | Every moment you do not land a telling blow makes the next one likelier. Not yet built. |
| `ballistics.png` | Ballistics | Passive | Your rockets hit twice as hard and sometimes seek their mark. Not yet built. |
| `leech.png` | Leech | Passive | Every blow you land returns three hundredths of its damage as life. |
| `ambush.png` | Ambush | Passive | You fall two fifths harder on anything above three quarters of its life. |
| `awareness.png` | Awareness | Passive | Once a minute a killing blow leaves you standing at a third of your life instead. |
| `single-out.png` | Single Out | Passive | Anything with no fellow within two tiles takes a quarter more harm from you. |

## Bard

### Bard - MELODY (7)

| File | Name | Kind | What it does |
|---|---|---|---|
| `melody-of-life.png` | Melody of Life | Aura | A song that mends your wounds as it plays. |
| `battle-hymn.png` | Battle Hymn | Aura | A song that sharpens your aim and your blow. |
| `song-of-swiftness.png` | Song of Swiftness | Aura | A song that quickens your strikes and your stride - you run rather than walk. |
| `song-of-fortitude.png` | Song of Fortitude | Aura | A song that hardens your guard and your wards. |
| `dirge-of-dread.png` | Dirge of Dread | Aura | A dirge that leaves what hears it fifteen percent weaker, two more a point, and sends all but the uniques fleeing. |
| `lullaby.png` | Lullaby | Active | A song that leaves everything in earshot asleep on its feet for four seconds, half a second more a rank, until it is struck. Uniques do not sleep. |
| `epic-solo.png` | Epic Solo | Passive | Mastery that empowers every Melody song. Inert: there is no per-page channel here. |

### Bard - HARMONY (6)

| File | Name | Kind | What it does |
|---|---|---|---|
| `sound-shock.png` | Sound Shock | Active | A burst of sound through the three tiles ahead, for four to ten and two to four more a rank, that staggers what it strikes. |
| `shout.png` | Shout | Active | A shout that leaves everything within three tiles reeling for a second, a fifth more a rank. Uniques shrug it off. |
| `discord.png` | Discord | Aura | A discord that strips a fifth of the armour from what hears it, two percent more a point. |
| `resonance.png` | Resonance | Passive | Your blows amplify your next song. Inert: no such carry-over exists here. |
| `echoing-song.png` | Echoing Song | Passive | Your songs reach further and last longer. Inert: songs here have neither range nor duration. |
| `perfect-harmony.png` | Perfect Harmony | Passive | Mastery that empowers every Harmony skill. Inert: there is no per-page channel here. |

### Bard - POETRY (6)

| File | Name | Kind | What it does |
|---|---|---|---|
| `daze.png` | Daze | Active | A verse that sends everything in earshot stumbling off in a direction of its own. Uniques keep their feet. |
| `inspiration.png` | Inspiration | Aura | A verse that returns your mana as it plays. |
| `tale-of-heroes.png` | Tale of Heroes | Aura | A verse that lends you a hero's strength and grace. |
| `weaken.png` | Weaken | Aura | A drone that blunts the aim of what hears it by twenty, two more a point, and slows its step. |
| `ode-to-glory.png` | Ode to Glory | Active | Raises a fallen ally to fight on. Inert: it needs the corpse-handling pass. |
| `legendary-ballad.png` | Legendary Ballad | Passive | Mastery that empowers every Poetry skill. Inert: there is no per-page channel here. |

### Bard - PASSIVE SKILLS (18)

| File | Name | Kind | What it does |
|---|---|---|---|
| `perfect-pitch.png` | Perfect Pitch | Passive | A song held without a wrong note strikes truer the longer it runs. Not yet built. |
| `crescendo.png` | Crescendo | Passive | Each verse of a song hits harder than the one before it, and it stacks. Not yet built. |
| `sustain.png` | Sustain | Passive | Your songs hold their power well after you stop playing them. Not yet built. |
| `countermelody.png` | Countermelody | Passive | A second song may play beneath the first at half its strength. Not yet built. |
| `rhythm.png` | Rhythm | Passive | Striking in time with your song quickens your hand. Not yet built. |
| `refrain.png` | Refrain | Passive | A song that has run its course begins again at no cost. Not yet built. |
| `encore.png` | Encore | Passive | Falling silent leaves the last song ringing a while longer. Not yet built. |
| `cadence.png` | Cadence | Passive | Every third blow lands on the beat, half again as hard. |
| `timbre.png` | Timbre | Passive | Your songs reach far further from you. Not yet built. |
| `virtuoso.png` | Virtuoso | Passive | Your songs cost far less to hold. Not yet built. |
| `dissonance.png` | Dissonance | Passive | What your songs touch strikes back weaker. Not yet built. |
| `improvisation.png` | Improvisation | Passive | Switching songs costs nothing and briefly grants both. Not yet built. |
| `chorus.png` | Chorus | Passive | Every ally within earshot lends your songs strength. Not yet built. |
| `overture.png` | Overture | Passive | The first song of a fight begins at its full power. Not yet built. |
| `reverberation.png` | Reverberation | Passive | Your songs echo, striking a second time for less. Not yet built. |
| `stagecraft.png` | Stagecraft | Passive | Being struck while playing does not break the song. Not yet built. |
| `requiem.png` | Requiem | Passive | Each enemy that falls within four tiles mends a fiftieth of your life. |
| `magnum-opus.png` | Magnum Opus | Passive | Hold one song long enough and it becomes something greater. Not yet built. |

## Monk

### Monk - WAY OF THE STAFF (7)

| File | Name | Kind | What it does |
|---|---|---|---|
| `sweeping-reed.png` | Sweeping Reed | Active | Sweep your staff through the three tiles ahead - the target and both beside it - at full force. |
| `breaking-current.png` | Breaking Current | Active | A focused strike a third again as hard that leaves the target reeling for a second. Uniques shrug the stagger off. |
| `reed-in-the-wind.png` | Reed in the Wind | Passive | Staff blocks carry you aside. Inert: this engine exposes no block-chance channel. |
| `vaulting-strike.png` | Vaulting Strike | Active | Vault onto a distant foe; the blow you land there is half again as hard, a tenth more a rank. |
| `wheel-of-heaven.png` | Wheel of Heaven | Active | Every swing strikes everything around you, at two thirds, a twentieth more a rank. |
| `seven-reeds.png` | Seven Reeds | Active | Three blows in one swing, one more every three ranks up to seven, each at three fifths. |
| `master-of-the-long-staff.png` | Master of the Long Staff | Passive | Your mastery of the staff empowers every Way of the Staff skill. With a staff in hand: +10% damage and a sharper aim. |

### Monk - WAY OF THE BODY (7)

| File | Name | Kind | What it does |
|---|---|---|---|
| `open-palm.png` | Open Palm | Active | An open-hand strike a fifth harder, a tenth more a rank, that drives the enemy back a tile. |
| `flowing-step.png` | Flowing Step | Passive | Move through battle with greater speed. One point makes you run rather than walk; the evade half needs an avoidance roll this engine has not got. |
| `iron-robe.png` | Iron Robe | Passive | Discipline hardens your body while you wear light armour or none at all. Unarmoured: armour class by level, and blows land lighter. Light armour keeps half. Mail and plate switch it off. |
| `counterstroke.png` | Counterstroke | Passive | A block empowers your next blow. Inert: nothing here reports a block to build on. |
| `purifying-breath.png` | Purifying Breath | Active | Centre yourself: twenty to every resistance, five more a rank, for thirty seconds and five more a rank. |
| `hundred-fists.png` | Hundred Fists | Active | Four blows in one swing, one more every two ranks up to seven, each at half. |
| `perfect-vessel.png` | Perfect Vessel | Passive | Your mastery of the body empowers every Way of the Body skill: a tenth more life, and you shake off hits faster. |

### Monk - WAY OF THE SPIRIT (5)

| File | Name | Kind | What it does |
|---|---|---|---|
| `healing-mantra.png` | Healing Mantra | Aura | Restore life to yourself over time. Held like an aura rather than cast, so it mends you for as long as it plays. |
| `temple-bell.png` | Temple Bell | Active | A tone that strikes every undead in earshot for three to six a rank, staggers it and drives it back. |
| `radiant-palm.png` | Radiant Palm | Active | A strike a fifth harder, a tenth more a rank; an enemy it kills erupts, dealing the blow again to everything beside it. |
| `tranquility.png` | Tranquility | Active | A sanctuary about you for twelve seconds and one more a rank: what stands beside you is slowed, and a fiftieth of your life returns each second. |
| `enlightenment.png` | Enlightenment | Passive | Your mastery of spirit empowers every Way of the Spirit skill: a tenth more mana, and ten points of every resistance. |

### Monk - PASSIVE SKILLS (18)

| File | Name | Kind | What it does |
|---|---|---|---|
| `resolve.png` | Resolve | Passive | What you strike strikes back weaker for a while. Not yet built. |
| `fleet-footed.png` | Fleet Footed | Passive | You run at all times. |
| `exalted-soul.png` | Exalted Soul | Passive | You hold twenty more mana. |
| `transcendence.png` | Transcendence | Passive | Half of every point of mana you spend returns as life. |
| `chant-of-resonance.png` | Chant of Resonance | Passive | Your mantras cost far less to invoke. Not yet built. |
| `seize-the-initiative.png` | Seize the Initiative | Passive | Striking the unwounded quickens your hand. Not yet built. |
| `the-guardian-s-path.png` | The Guardian's Path | Passive | Two weapons lend you evasion; one great staff lends you spirit. Not yet built. |
| `sixth-sense.png` | Sixth Sense | Passive | Everything that is not steel - fire, lightning, magic - hurts a quarter less. |
| `determination.png` | Determination | Passive | Every enemy pressing close makes you hit a twentieth harder, to a fifth. |
| `relentless-assault.png` | Relentless Assault | Passive | You fall three tenths harder on anything frozen or reeling. |
| `beacon-of-ytar.png` | Beacon of Ytar | Passive | Every cooldown you carry comes round sooner. Not yet built. |
| `alacrity.png` | Alacrity | Passive | Your spirit-building strikes come faster. Not yet built. |
| `harmony.png` | Harmony | Passive | Every resistance fifteen points higher. |
| `combination-strike.png` | Combination Strike | Passive | Rotating your strikes makes each of them stronger. Not yet built. |
| `near-death-experience.png` | Near Death Experience | Passive | Once a minute a killing blow restores a third of your life and mana instead. |
| `unity.png` | Unity | Passive | Every ally under your mantra lends you strength. Not yet built. |
| `momentum.png` | Momentum | Passive | Cover enough ground and your next blows land far harder. Not yet built. |
| `mythic-rhythm.png` | Mythic Rhythm | Passive | Every third building strike charges the spender that follows. Not yet built. |

## Also

| File | Name | What it does |
|---|---|---|
| `regular-attack.png` | Regular Attack | The basic weapon swing, the LMB well's resting picture. |
| `fist-attack.png` | Fist Attack | The same with no weapon in hand. |

## What will happen with the delivery

- Each class strip (`ui*_tree_icons.png`, cut by the tree-icon cutters) is regenerated from the files: the glyph composited on frame 26 at 56, one cell per skill in tree order. The wells, the Abilities window and the picker all draw from those strips already, so no draw code changes.
- The plate's tint (green invested, red unspent, grey locked, pink unusable, orange staff) is applied by the game through the palette, as it is to vanilla's own icons - so the glyphs must be white and black only, or the tint will not take.
