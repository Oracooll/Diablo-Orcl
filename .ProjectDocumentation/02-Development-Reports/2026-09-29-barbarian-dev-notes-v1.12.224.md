# 2026-09-29 - Seventeen dev notes, mostly the Barbarian (v1.12.224)

**Date:** 2026-09-29. Debug only. The user: "check dev notes and process." All seventeen notes were logged from a
v1.12.223 play session and are archived in `development-archive.md` with what was done; the inbox is empty.

## Right button only

`RightButtonOnly` (oracool/whirlwind.h, renamed in v1.12.223) now covers, besides Whirlwind and Earthquake:
- **Leap, Ground Stomp, Rend** - the user counts them as spells;
- **every active on the Barbarian's Warcries page** (`BarbarianWarcriesPage`, found through the class tree), the
  cries, Find Potion, Find Item and Grim Ward - "warcries to be rmb only".

The left button's menu shows them on the red plate and readies them on the right.

## Rend, a cast

"rend to be considered spell similar to d3 ... to play magic cast sprite animation and to apply to all mobs within range
as a curse. use the war cry anymation as visual effect ... increase its scale to the range":
- **Spell data:** an untargeted cast through the cry missile, as Ground Stomp's; out of `IsMeleeSpell`, its melee bonus,
  its swing effect and its Blood Star flash.
- **Effect:** everything within 3 tiles (`ReachTiles`) bleeds for 4 seconds at its old bleed, 3 a second, +2 a level.
- **Look:** a war cry ring grown to the reach (`ReachRing`: 60% of the sheet a tile, 180% here), blood red, every cast.

## Rings and animations

- **Earthquake:** war cry rings every 4 ticks for its four seconds, grown to its 3-tile reach, in its molten brown cycle.
  The v1.12.223 Blue Flare went, and with it `SpinMissile` and `SpinPingPongClxList`; the rotation note was superseded.
- **Ground Stomp:** its ring on every stomp, at 200% (floor point 29px).
- **War cries:** the ring goes out on every cast; a cry that finds no one still fizzles (`AddWarcry`).
- **Hammer of the Ancients:** Holy Bolt's burst, 100%, infrared.
- **Whirlwind:** only the magic cast frames with the cloud whole (13-18 of 20, as shares), forward and back. Two axes and
  two swords circle him in the cloud (`DrawWhirlwindBlades`): the inventory's small axe and short sword at 36%, turned
  through 16 steps by the new `TurnedClxList`, once round every 0.6 s and end over end every 0.3 s, the near half drawn
  after him and the far half before.

## Battle Command

"battle command doesnt seem to increase lvl of skills". It did raise the actives: its +1 lands in `_pISplLvlAdd`, and
an active's rank is its spell level. Nothing showed it, and the passives never had it:
- `ClassTreeBonusRanks` / `ClassTreeRank`: points plus 1 while it lasts, used by the tree totals (auras, passives), the
  RfA-12 passives (`PointsIfOn`) and Throwing Mastery. Level-up stats stay the points spent.
- `ClassTreeShownRank`: the Abilities window, the skill menu and the wells show the working rank (items included).
- The hover says "(+1 from Battle Command)" beside "from items".

## Smaller notes

- **Weapon Throw:** 5 Rage.
- **Passive learn sound:** every passive without a learn or start cue plays `readbook.wav` (was the UI click for the
  eight RfA-12 passives on the Combat Masteries sheet).
- **Find Potion:** Find Item's sound, recorded as a pick on the Barbarian Skill Cards page (db) and regenerated.
- **Resistance bars:** 9 parts to the 90 cap (`DrawSheetBar` segments).
- **XP bar (character window):** 11px (two thirds of 16), centred in its row; 2px pale gold frame and marks.
- **Passives in the menus:** already out since v1.12.201; noted.

## Tests

- `OracoolRage.WhirlwindIsHeldOnTheRightButton`: the new right-only set, Rend no longer a melee swing.
- `OracoolRage.EveryBarbarianActiveIsTheUsersPick` and the census note test: Weapon Throw 5.
- New `OracoolWarcries.BattleCommandDeepensEveryLearnedSkill`, `OracoolSpriteScale.TurnedListTurnsOneSpriteThroughAWholeTurn`.

Debug build and ctest: 882/882 passed. A leftover game process (no window, idle) was stopped before the build. Not
seen in play: the Whirlwind blades and Rend's cast want a look first.
