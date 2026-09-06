# Reach and hit: a held click on a monster swings the Paladin's melee skill on arrival (v1.10.011)

**Date:** 2026-09-07
**Request:** "something is wrong with the reach-and-hit mechanic of holding a left click over a monster. i hold left click over a monster and many time my hero reaches the mob but doesnt start attacking."

## The cause

With a Paladin skill on the button and the monster out of the skill's range, CheckPlrSpell (player.cpp) sent a plain walk to the monster's tile and recorded the hold as a Walk. RepeatMouseAction then repeated a WALK, not the skill: the hero arrived beside the monster, RepeatWalk saw the walk's target already reached, and nothing ever asked the skill again. The plain attack and the Barbarian's and Monk's melee skills never had this - they go through CMD_ATTACKID, the engine's own walk-then-swing, which re-lays the path to the monster every tick and swings at arrival.

## The fix

Out of range on a monster: a MELEE Paladin skill (Zeal, Hammer of Faith, Shield Bash) now takes CMD_ATTACKID with the skill armed, the same road the other classes' melee skills take. A RANGED one (Charge, Blessed Shield, Fist of the Heavens, Blessed Hammer) still walks, but the hold is recorded as a cast on that monster, so a held button asks again every step and fires the moment the range is met. A click on bare ground is still a walk.

The 2026-08-15 rule ("melee skills only initiate when clicked on monsters within range, else - move command") is kept in its intent: the hero moves there. It no longer forgets why.

## Tests

No unit test: the path runs through the cursor globals and the network queue, which the suite does not stand up. Suite 690/690 unchanged. Needs the user's hand on the mouse: hold left click on a distant monster with Zeal readied and with Blessed Hammer readied.
