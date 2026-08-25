# Skill and Spell Reference

**Generated from `Source/oracool/class_tree.cpp` — do not hand-edit.** Every class-tree skill in the
game, in icon-strip order. Regenerate after adding rows; the frame number here IS the cell index in
that class's icon strip and the slot index in the save file, so the two must never disagree.

Companion to *Asset Brief — Colourful Skill Icons*, which carries the technical specification for
drawing these. This file is the CONTENT: what each icon has to depict.

**Frame** is the 0-based cell in the class's strip (`ui\<class>_tree_icons.png`).
**Built** says whether the skill currently does anything. An unbuilt skill still needs an icon —
it is listed and described in-game, drawn with a red X over it.


## Paladin — `ui\paladin_tree_icons.png`, 49 frames

Pages: 0 Combat Skills · 1 Offensive Auras · 2 Defensive Auras · 3 Passive Skills

| Frame | Page | Tier | Kind | Built | Name | What it depicts |
|---:|---|---:|---|---|---|---|
| 0 | Combat Skills | 0 | Active | — | **Sacrifice** | Strike for heavy bonus damage and wound yourself for a share of it. Not yet built. |
| 1 | Combat Skills | 0 | Active | yes | **Smite** | Bash with your shield: it always connects and briefly stuns. A shield is mandatory. |
| 2 | Combat Skills | 0 | Active | — | **Holy Bolt** | A bolt of holy energy that sears the undead. Withdrawn: it collided with this engine's own Holy Bolt spell. |
| 3 | Combat Skills | 1 | Active | yes | **Zeal** | Strike several times in one furious burst. Each invested pair of points adds a strike, up to five. |
| 4 | Combat Skills | 1 | Active | yes | **Charge** | Rush an enemy and land a running blow. |
| 5 | Combat Skills | 2 | Active | — | **Vengeance** | Adds fire, lightning and cold damage to your attack. Not yet built; this engine also has no cold. |
| 6 | Combat Skills | 3 | Active | yes | **Blessed Hammer** | Looses a spinning hammer that wheels outward through anything in its path. |
| 7 | Combat Skills | 4 | Active | — | **Conversion** | Turns an enemy to your side. Withdrawn pending design work: its Berserk behaviour was wrong. |
| 8 | Combat Skills | 5 | Active | yes | **Fist of the Heavens** | Calls down a bolt from the sky, which bursts into holy energy where it lands. |
| 9 | Offensive Auras | 0 | Aura | yes | **Might** | Increases the damage you deal. |
| 10 | Offensive Auras | 1 | Aura | yes | **Holy Fire** | Wreathes your weapon in flame, adding fire damage to every blow. |
| 11 | Offensive Auras | 1 | Aura | yes | **Thorns** | Returns damage to whatever strikes you. This engine's thorns is a flat return, so points light it rather than growing it. |
| 12 | Offensive Auras | 2 | Aura | yes | **Blessed Aim** | Steadies your hand, raising your chance to hit. |
| 13 | Offensive Auras | 3 | Aura | yes | **Concentration** | Raises damage and steadies you against interruption. |
| 14 | Offensive Auras | 3 | Aura | — | **Holy Freeze** | Chills nearby enemies and adds cold damage. Inert: this engine has no cold and no slow. |
| 15 | Offensive Auras | 4 | Aura | yes | **Holy Shock** | Charges your weapon, adding lightning damage to every blow. |
| 16 | Offensive Auras | 4 | Aura | yes | **Sanctuary** | Hallows the ground you stand on: nearby undead break and flee from you. Champions are too proud to run. |
| 17 | Offensive Auras | 5 | Aura | yes | **Fanaticism** | Drives you to strike faster, harder and truer. |
| 18 | Offensive Auras | 5 | Aura | yes | **Conviction** | Strips the resistances of every enemy near you, and at five points begins to break their immunities down into mere resistances. |
| 19 | Defensive Auras | 0 | Aura | yes | **Prayer** | Mends your wounds steadily as you walk. |
| 20 | Defensive Auras | 0 | Aura | yes | **Resist Fire** | Hardens you against fire. |
| 21 | Defensive Auras | 1 | Aura | yes | **Defiance** | Raises your armour class. |
| 22 | Defensive Auras | 1 | Aura | yes | **Resist Cold** | Hardens you against cold. No cold exists here, so it wards against magic instead. |
| 23 | Defensive Auras | 2 | Aura | — | **Cleansing** | Shortens poison and curses. Inert: this engine tracks no duration for either. |
| 24 | Defensive Auras | 2 | Aura | yes | **Resist Lightning** | Hardens you against lightning. |
| 25 | Defensive Auras | 3 | Aura | yes | **Vigor** | Quickens your stride: you run instead of walking, wherever you are. |
| 26 | Defensive Auras | 4 | Aura | yes | **Meditation** | Restores your mana steadily as you walk. |
| 27 | Defensive Auras | 5 | Aura | — | **Redemption** | Consumes the fallen for life and mana. Inert: it needs the corpse-handling pass. |
| 28 | Defensive Auras | 5 | Aura | yes | **Salvation** | Wards you against fire, lightning and magic alike. |
| 29 | Combat Skills | 2 | Active | yes | **Hammer of Faith** | A heavy swing whose force splashes over everything around your target. |
| 30 | Combat Skills | 3 | Active | yes | **Blessed Shield** | Hurls your shield at a crowd, striking several of them before it returns. A shield is mandatory. |
| 31 | Passive Skills | 0 | Passive | — | **Heavenly Strength** | Bear a two-handed weapon in your main hand and a shield in the other. Not yet built. |
| 32 | Passive Skills | 0 | Passive | — | **Fervor** | One-handed weapons swing faster and your cooldowns come round sooner. Not yet built. |
| 33 | Passive Skills | 0 | Passive | — | **Vigilant** | Your wounds close faster and every blow that is not steel hurts less. Not yet built. |
| 34 | Passive Skills | 1 | Passive | — | **Righteousness** | Your opening strikes build wrath faster, and you hold more of it. Not yet built. |
| 35 | Passive Skills | 1 | Passive | — | **Insurmountable** | Every blow you turn aside feeds your wrath. Not yet built. |
| 36 | Passive Skills | 1 | Passive | — | **Fanaticism** | Your simplest attacks land faster than a measured swing would. Not yet built. |
| 37 | Passive Skills | 2 | Passive | — | **Indestructible** | Once a minute a killing blow leaves you standing, stronger and drinking life. Not yet built. |
| 38 | Passive Skills | 2 | Passive | — | **Holy Cause** | Your weapon bites deeper, and holy damage mends you as it burns. Not yet built. |
| 39 | Passive Skills | 2 | Passive | — | **Wrathful** | Spent wrath returns to you as life. Not yet built. |
| 40 | Passive Skills | 3 | Passive | — | **Divine Fortress** | The shield you hide behind becomes armour you wear. Not yet built. |
| 41 | Passive Skills | 3 | Passive | — | **Lord Commander** | Your mount, your bombardment and your phalanx all answer sooner and hit harder. Not yet built. |
| 42 | Passive Skills | 3 | Passive | — | **Hold Your Ground** | You no longer dodge at all, and block far more. Not yet built. |
| 43 | Passive Skills | 4 | Passive | — | **Long Arm of the Law** | Every law you declare holds its power longer. Not yet built. |
| 44 | Passive Skills | 4 | Passive | — | **Iron Maiden** | What strikes you is returned with far greater interest. Not yet built. |
| 45 | Passive Skills | 4 | Passive | — | **Renewal** | Each blow turned aside returns a measure of life. Not yet built. |
| 46 | Passive Skills | 5 | Passive | — | **Finery** | Every gem set into your gear lends you strength. Not yet built. |
| 47 | Passive Skills | 5 | Passive | — | **Blunt** | Justice and the blessed hammer fall heavier. Not yet built. |
| 48 | Passive Skills | 5 | Passive | — | **Towering Shield** | Every skill worked through your shield strikes harder and readies sooner. Not yet built. |

## Barbarian — `ui\barb_tree_icons.png`, 49 frames

Pages: 0 Combat Skills · 1 Combat Masteries · 2 Warcries · 3 Passive Skills

| Frame | Page | Tier | Kind | Built | Name | What it depicts |
|---:|---|---:|---|---|---|---|
| 0 | Combat Skills | 0 | Active | — | **Bash** | A heavy blow that knocks the target back. Not yet built. |
| 1 | Combat Skills | 1 | Active | — | **Leap** | Vault over anything in the way. Not yet built: it needs new movement work. |
| 2 | Combat Skills | 1 | Active | — | **Double Swing** | Strike with both weapons at once. Not yet built. |
| 3 | Combat Skills | 2 | Active | — | **Stun** | A blow that leaves the target reeling. Not yet built. |
| 4 | Combat Skills | 2 | Active | — | **Double Throw** | Hurl both thrown weapons at once. Inert: this engine has no thrown weapons. |
| 5 | Combat Skills | 3 | Active | — | **Leap Attack** | Leap onto a distant enemy and strike on landing. Not yet built. |
| 6 | Combat Skills | 3 | Active | — | **Concentrate** | A focused blow you cannot be jolted out of. Not yet built. |
| 7 | Combat Skills | 4 | Active | — | **Frenzy** | Each kill drives the next blow faster. Not yet built. |
| 8 | Combat Skills | 5 | Active | — | **Whirlwind** | Spin through a crowd striking everything. Not yet built: it needs new movement work. |
| 9 | Combat Skills | 5 | Active | — | **Berserk** | Trade all defence for a devastating magical blow. Not yet built. |
| 10 | Combat Masteries | 0 | Passive | yes | **Sword Mastery** | Sharpens your aim and your blow with any sword held. |
| 11 | Combat Masteries | 0 | Passive | yes | **Axe Mastery** | Sharpens your aim and your blow with any axe held. |
| 12 | Combat Masteries | 0 | Passive | yes | **Mace Mastery** | Sharpens your aim and your blow with any mace or club held. |
| 13 | Combat Masteries | 1 | Passive | yes | **Pole Arm Mastery** | Sharpens your aim and your blow with a staff - this engine's nearest pole arm. |
| 14 | Combat Masteries | 1 | Passive | — | **Throwing Mastery** | Mastery of thrown weapons. Inert: this engine has none. |
| 15 | Combat Masteries | 1 | Passive | — | **Spear Mastery** | Mastery of spears. Inert: this engine has no spear type. |
| 16 | Combat Masteries | 2 | Passive | — | **Increased Stamina** | Lengthens your wind. Inert: this engine tracks no stamina. |
| 17 | Combat Masteries | 3 | Passive | yes | **Iron Skin** | Toughens your hide, raising armour class. |
| 18 | Combat Masteries | 4 | Passive | yes | **Increased Speed** | You run rather than walk, wherever you are. |
| 19 | Combat Masteries | 5 | Passive | yes | **Natural Resistance** | Hardens you against fire, lightning and magic alike. |
| 20 | Warcries | 0 | Active | — | **Howl** | Sends nearby enemies fleeing. Inert: it needs the monster-facing pass. |
| 21 | Warcries | 0 | Active | — | **Find Potion** | Searches a corpse for a potion. Inert: it needs the corpse-handling pass. |
| 22 | Warcries | 1 | Active | — | **Taunt** | Goads an enemy into charging you. Inert: it needs the monster-facing pass. |
| 23 | Warcries | 1 | Active | — | **Shout** | A bellow that hardens you. Inert: buffs with a duration have no home here yet. |
| 24 | Warcries | 2 | Active | — | **Find Item** | Searches a corpse for loot. Inert: it needs the corpse-handling pass. |
| 25 | Warcries | 3 | Active | — | **Battle Cry** | A cry that weakens what hears it. Inert: it needs the monster-facing pass. |
| 26 | Warcries | 4 | Active | — | **Battle Orders** | A shout that swells life and mana. Inert: buffs with a duration have no home here yet. |
| 27 | Warcries | 4 | Active | — | **Grim Ward** | Raises a corpse as a totem of terror. Inert: it needs the corpse-handling pass. |
| 28 | Warcries | 5 | Active | — | **War Cry** | A shout that stuns everything near. Inert: it needs the monster-facing pass. |
| 29 | Warcries | 5 | Active | — | **Battle Command** | A command that deepens every other skill. Inert: buffs with a duration have no home here yet. |
| 30 | Passive Skills | 0 | Passive | — | **Pound of Flesh** | Healing taken from the fallen leaves you mending and quickened, and it stacks. Not yet built. |
| 31 | Passive Skills | 0 | Passive | — | **Ruthless** | You fall far harder on the wounded. Not yet built. |
| 32 | Passive Skills | 0 | Passive | — | **Nerves of Steel** | A killing blow leaves you barely standing but briefly untouchable. Not yet built. |
| 33 | Passive Skills | 1 | Passive | — | **Weapons Master** | Each family of weapon lends its own gift - damage, precision, speed or fury. Not yet built. |
| 34 | Passive Skills | 1 | Passive | — | **Inspiring Presence** | Your shouts hold twice as long and leave everyone near you mending. Not yet built. |
| 35 | Passive Skills | 1 | Passive | — | **Berserker Rage** | Near the height of your fury you strike far harder. Not yet built. |
| 36 | Passive Skills | 2 | Passive | — | **Bloodthirst** | Every point of fury you spend is paid back in life. Not yet built. |
| 37 | Passive Skills | 2 | Passive | — | **Animosity** | Fury comes faster and you can hold more of it. Not yet built. |
| 38 | Passive Skills | 2 | Passive | — | **Superstition** | Magic and missiles hurt less, and being struck by them stokes your fury. Not yet built. |
| 39 | Passive Skills | 3 | Passive | — | **Tough as Nails** | Your armour and the harm you return are both greatly increased. Not yet built. |
| 40 | Passive Skills | 3 | Passive | — | **No Escape** | What you throw and what you hurl lands harder on the distant. Not yet built. |
| 41 | Passive Skills | 3 | Passive | — | **Relentless** | Badly wounded, your skills cost half, your healing doubles and blows land softer. Not yet built. |
| 42 | Passive Skills | 4 | Passive | — | **Brawler** | Surrounded by three or more, everything you do hurts more. Not yet built. |
| 43 | Passive Skills | 4 | Passive | — | **Juggernaut** | What would hold you fast holds you half as long, and may give you back your life. Not yet built. |
| 44 | Passive Skills | 4 | Passive | — | **Unforgiving** | Your fury no longer ebbs when the fighting stops - it rises. Not yet built. |
| 45 | Passive Skills | 5 | Passive | — | **Boon of Bul-Kathos** | Your earthquake, your ancients and your berserking all return far sooner. Not yet built. |
| 46 | Passive Skills | 5 | Passive | — | **Earthen Might** | Splitting the ground fills you with fury. Not yet built. |
| 47 | Passive Skills | 5 | Passive | — | **Sword and Board** | Behind a shield you take far less harm and spend far less fury. Not yet built. |
| 48 | Passive Skills | 6 | Passive | — | **Rampage** | Every kill lends you strength, and it stacks high. Not yet built. |

## Sorceress — `ui\sorc_tree_icons.png`, 48 frames

Pages: 0 Cold Spells · 1 Lightning Spells · 2 Fire Spells · 3 Passive Skills

| Frame | Page | Tier | Kind | Built | Name | What it depicts |
|---:|---|---:|---|---|---|---|
| 0 | Cold Spells | 0 | Active | — | **Ice Bolt** | A shard of ice that chills what it hits. Inert: this engine has no cold damage. |
| 1 | Cold Spells | 0 | Active | — | **Frozen Armor** | Armour of ice that freezes attackers. Inert: no cold, no freeze. |
| 2 | Cold Spells | 1 | Active | — | **Frost Nova** | A ring of ice bursting outward. Inert: no cold damage. |
| 3 | Cold Spells | 1 | Active | — | **Ice Blast** | A shard that freezes its target solid. Inert: no cold, no freeze. |
| 4 | Cold Spells | 2 | Active | — | **Shiver Armor** | Armour that answers blows with ice. Inert: no cold damage. |
| 5 | Cold Spells | 3 | Active | — | **Glacial Spike** | A spike that shatters into freezing shards. Inert: no cold damage. |
| 6 | Cold Spells | 4 | Active | — | **Blizzard** | Ice falls across a wide area. Inert: no cold damage. |
| 7 | Cold Spells | 4 | Active | — | **Chilling Armor** | Armour that answers ranged attacks in kind. Inert: no cold damage. |
| 8 | Cold Spells | 5 | Active | — | **Frozen Orb** | An orb that wanders, shedding ice. Inert: no cold damage. |
| 9 | Cold Spells | 5 | Passive | — | **Cold Mastery** | Pierces cold resistance. Inert: there is no cold to master. |
| 10 | Lightning Spells | 0 | Active | yes | **Charged Bolt** | Looses a spray of erratic bolts. Points raise this engine's Charged Bolt. |
| 11 | Lightning Spells | 1 | Active | — | **Static Field** | Strips a share of the life from everything near. Inert: no analogue exists here. |
| 12 | Lightning Spells | 1 | Active | yes | **Telekinesis** | Works objects and gathers items at a distance. Points raise this engine's Telekinesis. |
| 13 | Lightning Spells | 2 | Active | yes | **Nova** | A ring of lightning bursting outward. Points raise this engine's Nova. |
| 14 | Lightning Spells | 2 | Active | yes | **Lightning** | A bolt that strikes in a line. Points raise this engine's Lightning. |
| 15 | Lightning Spells | 3 | Active | yes | **Chain Lightning** | A bolt that leaps between enemies. Points raise this engine's Chain Lightning. |
| 16 | Lightning Spells | 3 | Active | yes | **Teleport** | Step instantly to a place you can see. Points raise this engine's Teleport. |
| 17 | Lightning Spells | 4 | Active | — | **Thunder Storm** | A storm that strikes on its own as you fight. Inert: no analogue exists here. |
| 18 | Lightning Spells | 4 | Active | yes | **Energy Shield** | Mana takes the damage your life would. Points raise this engine's Mana Shield. |
| 19 | Lightning Spells | 5 | Passive | yes | **Lightning Mastery** | Your blows carry lightning, and lightning troubles you less. Not D2's spell scaling: this engine deepens a spell by its LEVEL, and has no per-element channel to raise. |
| 20 | Fire Spells | 0 | Active | yes | **Fire Bolt** | A bolt of flame. Points raise this engine's Fire Bolt. |
| 21 | Fire Spells | 0 | Passive | yes | **Warmth** | Your mana returns of its own accord. |
| 22 | Fire Spells | 1 | Active | yes | **Inferno** | A gout of flame from your hands. Points raise this engine's Inferno. |
| 23 | Fire Spells | 2 | Active | yes | **Blaze** | Leaves fire in your wake. Mapped onto this engine's Flame Wave, the nearest rolling fire it has. |
| 24 | Fire Spells | 2 | Active | yes | **Fire Ball** | A bursting ball of flame. Points raise this engine's Fireball. |
| 25 | Fire Spells | 3 | Active | yes | **Fire Wall** | A wall of flame across the ground. Points raise this engine's Fire Wall. |
| 26 | Fire Spells | 3 | Passive | yes | **Enchant** | Your weapon burns: every blow carries fire. A passive rather than a cast buff, since a tree skill with no spell slot has no way to be cast. |
| 27 | Fire Spells | 4 | Active | — | **Meteor** | Calls a burning rock down from the sky. Inert: no analogue exists here. |
| 28 | Fire Spells | 5 | Passive | yes | **Fire Mastery** | Fire burns for you and less against you. Not D2's spell scaling: this engine deepens a spell by its LEVEL, and has no per-element channel to raise. |
| 29 | Fire Spells | 5 | Active | yes | **Hydra** | Sets a fire-breathing head to guard a spot. Mapped onto this engine's Guardian, which is the same idea. |
| 30 | Passive Skills | 0 | Passive | — | **Power Hungry** | You deal far more harm to what is far away. Not yet built. |
| 31 | Passive Skills | 0 | Passive | — | **Blur** | Everything that strikes you strikes softer. Not yet built. |
| 32 | Passive Skills | 0 | Passive | — | **Evocation** | Every cooldown you carry comes round sooner. Not yet built. |
| 33 | Passive Skills | 1 | Passive | — | **Glass Cannon** | You hit much harder and are much easier to hit back. Not yet built. |
| 34 | Passive Skills | 1 | Passive | — | **Prodigy** | Your simplest spells give back arcane power as you cast them. Not yet built. |
| 35 | Passive Skills | 1 | Passive | — | **Astral Presence** | You hold more arcane power and recover it faster. Not yet built. |
| 36 | Passive Skills | 2 | Passive | — | **Illusionist** | A heavy blow resets your escapes and speeds your step. Not yet built. |
| 37 | Passive Skills | 2 | Passive | — | **Cold Blooded** | What you have chilled takes more harm from every source. Not yet built. |
| 38 | Passive Skills | 2 | Passive | — | **Conflagration** | What you set alight becomes easier to strike truly. Not yet built. |
| 39 | Passive Skills | 3 | Passive | — | **Paralysis** | Your lightning may stun everything it touches. Not yet built. |
| 40 | Passive Skills | 3 | Passive | — | **Galvanizing Ward** | Go unharmed a moment and a ward forms around you. Not yet built. |
| 41 | Passive Skills | 3 | Passive | — | **Temporal Flux** | Arcane harm slows what it touches to a crawl. Not yet built. |
| 42 | Passive Skills | 4 | Passive | — | **Dominance** | Every kill lays another shell of shielding over you. Not yet built. |
| 43 | Passive Skills | 4 | Passive | — | **Arcane Dynamo** | Five simple spells charge the next great one. Not yet built. |
| 44 | Passive Skills | 4 | Passive | — | **Unstable Anomaly** | A killing blow throws up a vast ward and scatters what stands near. Not yet built. |
| 45 | Passive Skills | 5 | Passive | — | **Unwavering Will** | Stand still a moment and your armour, your wards and your damage all rise. Not yet built. |
| 46 | Passive Skills | 5 | Passive | — | **Audacity** | You deal far more harm to whatever is close enough to touch. Not yet built. |
| 47 | Passive Skills | 5 | Passive | — | **Elemental Exposure** | Striking with a new element leaves the target more open to all of them. Not yet built. |

## Rogue — `ui\rogue_tree_icons.png`, 49 frames

Pages: 0 Bow & Crossbow · 1 Passive & Magic · 2 Javelin & Spear · 3 Passive Skills

| Frame | Page | Tier | Kind | Built | Name | What it depicts |
|---:|---|---:|---|---|---|---|
| 0 | Bow & Crossbow | 0 | Active | — | **Magic Arrow** | An arrow of pure force that costs no ammunition. Not yet built. |
| 1 | Bow & Crossbow | 0 | Active | — | **Fire Arrow** | An arrow wrapped in flame. Not yet built. |
| 2 | Bow & Crossbow | 1 | Active | — | **Cold Arrow** | An arrow that chills. Inert: this engine has no cold damage. |
| 3 | Bow & Crossbow | 1 | Active | — | **Multiple Shot** | Looses a fan of arrows at once. Not yet built. |
| 4 | Bow & Crossbow | 2 | Active | — | **Exploding Arrow** | An arrow that bursts where it lands. Not yet built. |
| 5 | Bow & Crossbow | 2 | Active | — | **Ice Arrow** | An arrow that freezes its target. Inert: no cold, no freeze. |
| 6 | Bow & Crossbow | 3 | Active | — | **Guided Arrow** | An arrow that hunts its target. Not yet built. |
| 7 | Bow & Crossbow | 4 | Active | — | **Strafe** | Looses at every enemy in view in turn. Not yet built. |
| 8 | Bow & Crossbow | 4 | Active | — | **Immolation Arrow** | An arrow that leaves a burning pool. Not yet built. |
| 9 | Bow & Crossbow | 5 | Active | — | **Freezing Arrow** | An arrow that freezes everything near where it lands. Inert: no cold. |
| 10 | Passive & Magic | 0 | Active | — | **Inner Sight** | Lights nearby enemies and strips their defence. Inert: it needs the monster-facing pass. |
| 11 | Passive & Magic | 0 | Passive | yes | **Critical Strike** | A chance to strike for double. This engine has no critical roll, so it raises your damage instead. |
| 12 | Passive & Magic | 1 | Passive | — | **Dodge** | A chance to slip a blow while standing. Inert: no avoidance roll exists here. |
| 13 | Passive & Magic | 2 | Active | — | **Slow Missiles** | Slows what is thrown at you. Inert: it needs the monster-facing pass. |
| 14 | Passive & Magic | 2 | Passive | — | **Avoid** | A chance to slip a missile. Inert: no avoidance roll exists here. |
| 15 | Passive & Magic | 3 | Passive | yes | **Penetrate** | Sharpens your aim with anything you wield. |
| 16 | Passive & Magic | 3 | Active | — | **Decoy** | A double of yourself to draw fire. Not yet built. |
| 17 | Passive & Magic | 4 | Passive | — | **Evade** | A chance to slip a blow while moving. Inert: no avoidance roll exists here. |
| 18 | Passive & Magic | 5 | Active | yes | **Valkyrie** | Calls a warrior to fight beside you. Mapped onto this engine's Golem, which is the same idea. |
| 19 | Passive & Magic | 5 | Passive | — | **Pierce** | Your missiles carry on through. Not yet built. |
| 20 | Javelin & Spear | 0 | Active | — | **Jab** | A rapid flurry of thrusts. Not yet built. |
| 21 | Javelin & Spear | 1 | Active | — | **Power Strike** | A thrust charged with lightning. Not yet built. |
| 22 | Javelin & Spear | 1 | Active | — | **Poison Javelin** | A javelin trailing venom. Inert: this engine has no poison. |
| 23 | Javelin & Spear | 2 | Active | — | **Impale** | A savage thrust that wears the weapon. Not yet built. |
| 24 | Javelin & Spear | 2 | Active | — | **Charged Strike** | A thrust that throws off charged bolts. Not yet built. |
| 25 | Javelin & Spear | 3 | Active | — | **Lightning Bolt** | Turns a thrown javelin into a bolt. Not yet built. |
| 26 | Javelin & Spear | 3 | Active | — | **Plague Javelin** | A javelin trailing a cloud of pestilence. Inert: this engine has no poison. |
| 27 | Javelin & Spear | 4 | Active | — | **Fend** | Strikes every enemy around you in one motion. Not yet built. |
| 28 | Javelin & Spear | 5 | Active | — | **Lightning Strike** | A thrust whose lightning leaps onward. Not yet built. |
| 29 | Javelin & Spear | 5 | Active | — | **Lightning Fury** | A javelin that bursts into many bolts. Not yet built. |
| 30 | Passive Skills | 0 | Passive | — | **Thrill of the Hunt** | What your heavier shots strike is slowed almost to a stop. Not yet built. |
| 31 | Passive Skills | 0 | Passive | — | **Tactical Advantage** | Every evasion leaves you running far faster. Not yet built. |
| 32 | Passive Skills | 0 | Passive | — | **Blood Vengeance** | You hold more hatred, and the fallen restore both hatred and discipline. Not yet built. |
| 33 | Passive Skills | 1 | Passive | — | **Steady Aim** | With nothing close to you, everything you do hurts more. Not yet built. |
| 34 | Passive Skills | 1 | Passive | — | **Cull the Weak** | You fall harder on anything already slowed. Not yet built. |
| 35 | Passive Skills | 1 | Passive | — | **Night Stalker** | Your opening shots build hatred faster. Not yet built. |
| 36 | Passive Skills | 2 | Passive | — | **Brooding** | Stand still and your wounds close faster and faster. Not yet built. |
| 37 | Passive Skills | 2 | Passive | — | **Hot Pursuit** | Landing a blow leaves you moving faster. Not yet built. |
| 38 | Passive Skills | 2 | Passive | — | **Archery** | Each kind of bow lends its own gift - damage, precision or hatred. Not yet built. |
| 39 | Passive Skills | 3 | Passive | — | **Numbing Traps** | Anything you have slowed strikes back far weaker. Not yet built. |
| 40 | Passive Skills | 3 | Passive | — | **Perfectionist** | Your discipline goes further and your armour and wards are stronger. Not yet built. |
| 41 | Passive Skills | 3 | Passive | — | **Custom Engineering** | Your traps and sentries last twice as long and you may set more. Not yet built. |
| 42 | Passive Skills | 4 | Passive | — | **Grenadier** | Your grenades hit harder, burst wider, and one falls when you do. Not yet built. |
| 43 | Passive Skills | 4 | Passive | — | **Sharpshooter** | Every moment you do not land a telling blow makes the next one likelier. Not yet built. |
| 44 | Passive Skills | 4 | Passive | — | **Ballistics** | Your rockets hit twice as hard and sometimes seek their mark. Not yet built. |
| 45 | Passive Skills | 5 | Passive | — | **Leech** | Every blow you land returns life. Not yet built. |
| 46 | Passive Skills | 5 | Passive | — | **Ambush** | You fall far harder on the unwounded. Not yet built. |
| 47 | Passive Skills | 5 | Passive | — | **Awareness** | Once a minute a killing blow makes you vanish and mends you instead. Not yet built. |
| 48 | Passive Skills | 6 | Passive | — | **Single Out** | Anything that has strayed from its fellows is far easier to strike truly. Not yet built. |

## Bard — `ui\bard_tree_icons.png`, 39 frames

Pages: 0 Melody · 1 Harmony · 2 Poetry · 3 Passive Skills

| Frame | Page | Tier | Kind | Built | Name | What it depicts |
|---:|---|---:|---|---|---|---|
| 0 | Melody | 0 | Aura | yes | **Melody of Life** | A song that mends your wounds as it plays. |
| 1 | Melody | 0 | Aura | yes | **Battle Hymn** | A song that sharpens your aim and your blow. |
| 2 | Melody | 1 | Aura | yes | **Song of Swiftness** | A song that quickens your strikes and your stride - you run rather than walk. |
| 3 | Melody | 1 | Aura | yes | **Song of Fortitude** | A song that hardens your guard and your wards. |
| 4 | Melody | 2 | Aura | — | **Dirge of Dread** | Weakens enemies and sends them fleeing. Inert: it needs the monster-facing pass. |
| 5 | Melody | 2 | Active | — | **Lullaby** | Puts enemies to sleep. Inert: this engine has no sleep state. |
| 6 | Melody | 5 | Passive | — | **Epic Solo** | Mastery that empowers every Melody song. Inert: there is no per-page channel here. |
| 7 | Harmony | 0 | Active | — | **Sound Shock** | A burst of sonic force in front of you. Not yet built. |
| 8 | Harmony | 0 | Active | — | **Shout** | A shout that stuns. Inert: it needs the monster-facing pass. |
| 9 | Harmony | 1 | Active | yes | **Sonic Barrier** | A barrier that drinks the damage meant for you. Points raise this engine's Mana Shield. |
| 10 | Harmony | 1 | Aura | — | **Discord** | Strips enemy defence. Inert: it needs the monster-facing pass. |
| 11 | Harmony | 2 | Passive | — | **Resonance** | Your blows amplify your next song. Inert: no such carry-over exists here. |
| 12 | Harmony | 2 | Passive | — | **Echoing Song** | Your songs reach further and last longer. Inert: songs here have neither range nor duration. |
| 13 | Harmony | 5 | Passive | — | **Perfect Harmony** | Mastery that empowers every Harmony skill. Inert: there is no per-page channel here. |
| 14 | Poetry | 0 | Active | — | **Daze** | Sets an enemy wandering and striking at random. Inert: it needs the monster-facing pass. |
| 15 | Poetry | 0 | Active | yes | **Charm** | Turns a monster to your side. Rides this engine's Berserk, which does exactly that. |
| 16 | Poetry | 1 | Aura | yes | **Inspiration** | A verse that returns your mana as it plays. |
| 17 | Poetry | 1 | Aura | yes | **Tale of Heroes** | A verse that lends you a hero's strength and grace. |
| 18 | Poetry | 2 | Aura | — | **Weaken** | Blunts enemy aim and slows their step. Inert: it needs the monster-facing pass. |
| 19 | Poetry | 2 | Active | — | **Ode to Glory** | Raises a fallen ally to fight on. Inert: it needs the corpse-handling pass. |
| 20 | Poetry | 5 | Passive | — | **Legendary Ballad** | Mastery that empowers every Poetry skill. Inert: there is no per-page channel here. |
| 21 | Passive Skills | 0 | Passive | — | **Perfect Pitch** | A song held without a wrong note strikes truer the longer it runs. Not yet built. |
| 22 | Passive Skills | 0 | Passive | — | **Crescendo** | Each verse of a song hits harder than the one before it, and it stacks. Not yet built. |
| 23 | Passive Skills | 0 | Passive | — | **Sustain** | Your songs hold their power well after you stop playing them. Not yet built. |
| 24 | Passive Skills | 1 | Passive | — | **Countermelody** | A second song may play beneath the first at half its strength. Not yet built. |
| 25 | Passive Skills | 1 | Passive | — | **Rhythm** | Striking in time with your song quickens your hand. Not yet built. |
| 26 | Passive Skills | 1 | Passive | — | **Refrain** | A song that has run its course begins again at no cost. Not yet built. |
| 27 | Passive Skills | 2 | Passive | — | **Encore** | Falling silent leaves the last song ringing a while longer. Not yet built. |
| 28 | Passive Skills | 2 | Passive | — | **Cadence** | Every third blow lands on the beat and hits far harder. Not yet built. |
| 29 | Passive Skills | 2 | Passive | — | **Timbre** | Your songs reach far further from you. Not yet built. |
| 30 | Passive Skills | 3 | Passive | — | **Virtuoso** | Your songs cost far less to hold. Not yet built. |
| 31 | Passive Skills | 3 | Passive | — | **Dissonance** | What your songs touch strikes back weaker. Not yet built. |
| 32 | Passive Skills | 3 | Passive | — | **Improvisation** | Switching songs costs nothing and briefly grants both. Not yet built. |
| 33 | Passive Skills | 4 | Passive | — | **Chorus** | Every ally within earshot lends your songs strength. Not yet built. |
| 34 | Passive Skills | 4 | Passive | — | **Overture** | The first song of a fight begins at its full power. Not yet built. |
| 35 | Passive Skills | 4 | Passive | — | **Reverberation** | Your songs echo, striking a second time for less. Not yet built. |
| 36 | Passive Skills | 5 | Passive | — | **Stagecraft** | Being struck while playing does not break the song. Not yet built. |
| 37 | Passive Skills | 5 | Passive | — | **Requiem** | Each enemy that falls near you mends you a little. Not yet built. |
| 38 | Passive Skills | 5 | Passive | — | **Magnum Opus** | Hold one song long enough and it becomes something greater. Not yet built. |

## Monk — `ui\monk_tree_icons.png`, 39 frames

Pages: 0 Way of the Staff · 1 Way of the Body · 2 Way of the Spirit · 3 Passive Skills

| Frame | Page | Tier | Kind | Built | Name | What it depicts |
|---:|---|---:|---|---|---|---|
| 0 | Way of the Staff | 0 | Active | — | **Sweeping Reed** | Sweep your staff through enemies in a wide arc. Not yet built: it needs the multi-tile melee arc. |
| 1 | Way of the Staff | 1 | Active | — | **Breaking Current** | A focused strike that breaks armour and interrupts. Inert: it needs the monster-facing pass. |
| 2 | Way of the Staff | 2 | Passive | — | **Reed in the Wind** | Staff blocks carry you aside. Inert: this engine exposes no block-chance channel. |
| 3 | Way of the Staff | 3 | Active | — | **Vaulting Strike** | Vault over danger onto a distant foe. Not yet built: it needs new movement work. |
| 4 | Way of the Staff | 4 | Active | — | **Wheel of Heaven** | Spin your staff, striking all around you. Not yet built. |
| 5 | Way of the Staff | 5 | Active | — | **Seven Reeds** | A rapid chain of staff blows. Not yet built; the Paladin's Zeal burst is the nearest machinery. |
| 6 | Way of the Staff | 6 | Passive | yes | **Master of the Long Staff** | Your mastery of the staff empowers every Way of the Staff skill. With a staff in hand: +10% damage and a sharper aim. |
| 7 | Way of the Body | 0 | Active | — | **Open Palm** | An open-hand strike that drives the enemy back. Not yet built. |
| 8 | Way of the Body | 1 | Passive | yes | **Flowing Step** | Move through battle with greater speed. One point makes you run rather than walk; the evade half needs an avoidance roll this engine has not got. |
| 9 | Way of the Body | 2 | Passive | yes | **Iron Robe** | Discipline hardens your body while you wear light armour or none at all. Unarmoured: armour class by level, and blows land lighter. Light armour keeps half. Mail and plate switch it off. |
| 10 | Way of the Body | 3 | Passive | — | **Counterstroke** | A block empowers your next blow. Inert: nothing here reports a block to build on. |
| 11 | Way of the Body | 4 | Active | — | **Purifying Breath** | Centre yourself against the elements. Inert: the cleansing half needs status effects this engine has not got. |
| 12 | Way of the Body | 5 | Active | — | **Hundred Fists** | A storm of unarmed strikes on one enemy. Not yet built. |
| 13 | Way of the Body | 6 | Passive | yes | **Perfect Vessel** | Your mastery of the body empowers every Way of the Body skill: a tenth more life, and you shake off hits faster. |
| 14 | Way of the Spirit | 0 | Active | yes | **Inner Sight** | Reveal nearby objects, traps and treasure. Deepens the Monk's own Search: every point holds the sight longer. |
| 15 | Way of the Spirit | 1 | Aura | yes | **Healing Mantra** | Restore life to yourself over time. Held like an aura rather than cast, so it mends you for as long as it plays. |
| 16 | Way of the Spirit | 2 | Active | — | **Temple Bell** | A tone that staggers and repels the undead. Inert: it needs the monster-facing pass. |
| 17 | Way of the Spirit | 3 | Active | yes | **Spirit Ward** | Surround yourself with a barrier against magic. Rides this engine's Mana Shield, which drinks the blow into your mana; every point makes it drink deeper. |
| 18 | Way of the Spirit | 4 | Active | — | **Radiant Palm** | Marks an enemy to erupt when it falls. Not yet built. |
| 19 | Way of the Spirit | 5 | Active | — | **Tranquility** | A sanctuary that slows enemies and restores allies. Inert: it needs a ground-effect pass. |
| 20 | Way of the Spirit | 6 | Passive | yes | **Enlightenment** | Your mastery of spirit empowers every Way of the Spirit skill: a tenth more mana, and ten points of every resistance. |
| 21 | Passive Skills | 0 | Passive | — | **Resolve** | What you strike strikes back weaker for a while. Not yet built. |
| 22 | Passive Skills | 0 | Passive | — | **Fleet Footed** | You move faster at all times. Not yet built. |
| 23 | Passive Skills | 0 | Passive | — | **Exalted Soul** | You hold more spirit and recover it faster. Not yet built. |
| 24 | Passive Skills | 1 | Passive | — | **Transcendence** | Every point of spirit you spend returns as life. Not yet built. |
| 25 | Passive Skills | 1 | Passive | — | **Chant of Resonance** | Your mantras cost far less to invoke. Not yet built. |
| 26 | Passive Skills | 1 | Passive | — | **Seize the Initiative** | Striking the unwounded quickens your hand. Not yet built. |
| 27 | Passive Skills | 2 | Passive | — | **The Guardian's Path** | Two weapons lend you evasion; one great staff lends you spirit. Not yet built. |
| 28 | Passive Skills | 2 | Passive | — | **Sixth Sense** | Everything that is not steel hurts you far less. Not yet built. |
| 29 | Passive Skills | 2 | Passive | — | **Determination** | Every enemy pressing close makes you hit harder. Not yet built. |
| 30 | Passive Skills | 3 | Passive | — | **Relentless Assault** | You fall harder on anything blinded, frozen or reeling. Not yet built. |
| 31 | Passive Skills | 3 | Passive | — | **Beacon of Ytar** | Every cooldown you carry comes round sooner. Not yet built. |
| 32 | Passive Skills | 3 | Passive | — | **Alacrity** | Your spirit-building strikes come faster. Not yet built. |
| 33 | Passive Skills | 4 | Passive | — | **Harmony** | A ward against one element becomes a lesser ward against all of them. Not yet built. |
| 34 | Passive Skills | 4 | Passive | — | **Combination Strike** | Rotating your strikes makes each of them stronger. Not yet built. |
| 35 | Passive Skills | 4 | Passive | — | **Near Death Experience** | Once a minute a killing blow restores your life and spirit instead. Not yet built. |
| 36 | Passive Skills | 5 | Passive | — | **Unity** | Every ally under your mantra lends you strength. Not yet built. |
| 37 | Passive Skills | 5 | Passive | — | **Momentum** | Cover enough ground and your next blows land far harder. Not yet built. |
| 38 | Passive Skills | 5 | Passive | — | **Mythic Rhythm** | Every third building strike charges the spender that follows. Not yet built. |
