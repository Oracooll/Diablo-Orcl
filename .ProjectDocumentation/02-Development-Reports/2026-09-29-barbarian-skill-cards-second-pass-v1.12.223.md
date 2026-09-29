# 2026-09-29 - The Barbarian Skill Cards page, second pass (v1.12.223)

**Date:** 2026-09-29. Debug only. The user: "now check all my barb inputs on his artefact."

## What was new on the page

The page's db `picks` held 54 documents, 14 more than the 40 applied in v1.12.222 (compared against that pass's
snapshot, file for file). None of the 40 had changed. The 14 new ones:
- 5 animation picks, one with a comment (Earthquake);
- 9 sound picks (10 sound changes: Berserk has two).

## The combat masteries

The user asked first that the Combat Masteries (all passive) stay out of the left- and right-button menus. They already
were: the picker has left out every passive tree row since v1.12.201 (`skill_picker.cpp` BuildEntries), and the
Abilities window binds only a row with a spell of its own. Nothing changed; the user had not seen them there, only
assumed it.

## Sounds

`cards/mkcardpicks.js` re-read the page (35 Barbarian card picks now), and `GenVanillaSkillSounds.js` regenerated
`skill_sounds_data.inc`: 480 cues, 141 user picks. All ten had a slot already.

| Skill | Event | Was | Now |
|---|---|---|---|
| Berserk | Cast | swing | swing2 (heavy swing) |
| Berserk | Impact | blsimpt | items\invaxe |
| Bloodcall | Cast | fatc\fatca1 | zombie\zombiea1 ("Zombie moan") |
| Earthquake | Cast | swing2 | cast2 |
| Find Item | Cast | invgrab | magic1 |
| Howl | Cast | rhino\rhinoa1 | mega\megas1 (Slayer bellow) |
| Intimidate | Cast | rhino\rhinoa2 | mega\megaa1 (Slayer roar) |
| Split Ranks | Cast | black\blacka2 | black\blacka1 |
| Taunt | Cast | falspear\phalla1 | fat\fata2 (Overlord roar 2) |
| Threatening Shout | Cast | gargoyle\gargoa1 | mega\megas1 |

## Animations

- **Berserk:** a Holy Bolt burst at 75%, infrared, on a landed blow (`melee_skills.cpp`, beside Frenzy's).
- **War Cry / Earthshaker Cry:** the cry's ring at 200% / 150% (`warcries.cpp` AddWarcry, `ScaleMissile` with the
  ring's floor point, 80px up its 160px cell, so its centre stays on the floor).
- **Earthquake** - the pick (Blue Flare Explosion, `ex_blu2`, 150%, infrared) and its comment: "this animation should
  loop back and forth for 4 seconds, while rotating and colorcycling with brown color. This is a spell so it should play
  magic casting sprite sheet when initiated by player. to be assignable to right key only. Backing to be red on lmb well
  and lmb menu."
  - **The flare:** the quake's cracked ground is replaced by vanilla's Blue Flare Explosion at 150%, on the floor for
    the four seconds. New `oracool::SpinPingPongClxList` (sprite_scale) builds 36 square sprites: the sheet's frames
    forward and back, turned a whole turn over the 36. New `SpinMissile` (missiles.cpp) puts them on the missile, 2 ticks
    a sprite: a forward-and-back every 1.8 s, a turn every 3.6 s. Its centre sits 16px above the tile's bottom
    (AddWarcryRing's rule). The sheet is a monster's, loaded on the cast (`LoadMonsterOwnedArt`).
  - **Colour:** the comment's brown colour cycling, over the dropdown's infrared: `Tint::Earthquake`, v1.12.211's brown
    to dark orange and back with bands rolling through. The dropdown has no cycling brown.
  - **Fallbacks:** the RfA-27 cracked ground while the flare cannot load; the pulse ring while neither can.
  - **Magic cast sheet:** already so. Earthquake is a spell of MagicType Magic, cast through StartSpell, and
    GetPlayerGraphicForSpell gives it player_graphic::Magic. Nothing changed.
  - **Right button only:** `WhirlwindRightButtonOnly` is now `RightButtonOnly`, true for Whirlwind and Earthquake. The
    same paths hold it: the menu readies it on the right, the Abilities window likewise, a left hotkey binds nothing, a
    saved left binding is not restored.
  - **Red plate:** in the left button's menu both now wear the red (Blocked) plate, not the grey one auras keep; the LMB
    well draws a right-only spell red, should one ever be there.

## Tests

- `OracoolRage.WhirlwindIsHeldOnTheRightButton`: Earthquake is right-button only too.
- New `OracoolSpriteScale.SpinPingPongPlaysForwardAndBackWhileTurning`: 8 sprites of a 3-frame sheet play frames
  0 1 2 1 0 1 2 1, each on one square canvas, a quarter turn standing the bar up.

Debug build and ctest: 881/881 passed. Not seen in play: the Earthquake flare especially wants a look (size, speed of
the turn, and how the brown reads on a blue sheet).

## The page

Cards `build.js` SKILL_FX gained Berserk's burst, Earthquake's flare (the cracked ground dropped) and the two rings'
sizes; the Barbarian page republished as Version 3.
