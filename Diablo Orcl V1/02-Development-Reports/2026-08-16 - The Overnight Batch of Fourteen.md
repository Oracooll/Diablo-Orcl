---
date: 2026-08-16
version: 1.6.24
area: Combat fixes / HUD / item identity
---

# The Overnight Batch of Fourteen

The user filed fourteen points in one message and went to bed - "rectify all in one go one in a few
but automatically". Two needed no code (Zeal confirmed working as designed; its 5-hit ceiling stays
under observation for a possible nerf to 4). The rest, in the order they mattered:

## The crash that could not be reproduced - found anyway

The screenshot said `clx_sprite.hpp:63... value_.data_ != nullptr`, which reads as line 63 until you
notice the dialog wrapped a digit: it is line **630**, the intrusive-optional assert on
`OptionalOwnedClxSpriteSheet` - someone dereferenced a sprite sheet that was never loaded.

The someone was Shield Bash. Its swing wears the BLOCK animation, but `LoadPlrGFX` silently refuses
to load the block sheet when `_pBlockFlag` is off. In the gap between arming the skill and the swing
resolving - swap the shield off your arm and click - StartAttack requested an animation whose sheet
did not exist, and `spritesForDirection` walked into the empty optional. "While i was trying to hit
with an item" fits exactly: an item on the cursor, mid-shuffle, mid-fight.

Fix at the single authority: `IsShieldBashSwing` now answers false when the block sheet would not be
loaded (`_pBlockFlag`/`_pBFrames`), so that swing plays the ordinary attack animation instead - the
same availability rule LoadPlrGFX applies, asked one step earlier. Everything animation already asks
this function (graphic choice, hit frame, tempo), so nothing can disagree.

## RMB with Regular Attack did nothing

Selecting Regular Attack clears `_pRSpell` - the basic attack IS the engine's no-spell state - but
`CheckPlrSpell`'s first act on an invalid spell is to say "I don't have a spell ready" and return.
The one ability the RMB well always offers was the one ability the button could never throw. New
`RightMouseBasicAttack`: LeftMouseCmd's attack dispatch without its pickup/operate branches - talk
to towners, swing at monsters (melee or ranged), attack in place under shift, walk when nothing is
there. Hold-to-repeat works through the same `LastMouseButtonAction` values the left button sets.

## Shield Bash - tempo, stun, and who it works on

Three changes to the same skill:

- **Tempo**: the block animation is 2-6 frames against an attack's 16-20, so each shove was over in
  a third of a second and holding the button read as a jackhammer. The bash's Block graphic is now
  stretched (ticks per frame) so the whole shove lasts about one regular attack; a real raised-shield
  block still plays at vanilla speed.
- **Stun**: 25 ticks → **40 ticks (2 seconds)** - "a couple of seconds", as specified.
- **Exemptions**: no stun on scripted uniques, lesser uniques, or Diablo (who is placed as a plain
  MT_DIABLO and is the one boss `isUnique()` cannot see). No mana charged for the refusal - mana
  follows effect.

## Zeal without mana is a regular attack now - and the wells say so in pink

The chain and the mana charge were already gated, but the FIRST swing compressed on the strength of
the armed latch alone - an unaffordable Zeal still looked like Zeal, one fast swing, free, forever.
`ZealSwingSkipFrames` now asks `CanUsePaladinSkill` first.

And the visible half of the same request: **SkillPlateTint::Pink** (the PAL16_BEIGE ramp, the colour
the user has always called pink, free since the plates went green). Any skill or spell on the LMB/RMB
wells that cannot currently be performed - mana, shield, town - wears the pink plate until it can.
Grey keeps meaning "not learned"; pink means "not right now".

## Item identity, spelled out

- **Tier line directly below the name**, above the damage stats, for every piece of equipment:
  "basic item", "magic item", "rare item", "unique item", "primal item" - each in the name's own
  colour. The tiered labels moved up from below the affix block; basic and magic say so out loud for
  the first time. Equipment only - potions don't announce themselves.
- **Inventory backings**: basic items draw NO backing at all now (the beige wash said nothing and
  cost contrast); rare items keep their subtle yellow, magic blue, unique/primal as before.

## The XP blinker moved home and learned percentages

The "+N" flash left its old spot under the mini-map and now sits directly above the XP counter,
sharing its centre. It reads `+312 (0.7%)` - the gain as a fraction of the whole current level, one
decimal place because a single kill is routinely under 1% and "+312 (0%)" would answer the question
with a shrug. Captured at kill time, so a level-up mid-flash cannot skew it.

## Town Portal on T

`TownPortal` keymap action, default **T**, rebindable in the keymapping settings - the same free,
at-your-feet, single-player cast the belt's Portal button throws, so the key and the button can
never disagree.

## The new items sort with the armor now

The user suspected missing sell prices and broken sorting. The audit: every one of the 143 product-
line rows has a real base value (40 → ~5,000, a smooth curve), Griswold's single-player sell filter
takes anything with a positive value, and the sort-by-value formula prices them like everything
else - selling was never actually broken. Sorting was: `StashSortCategoryRank` predates the six new
worn types, so every pauldron and greave fell through to "Others" and sorted in among the potions.
They now form their own band after Shields, before Jewelry.

## State

Build 1.6.24, clean compile, **368/370** - the usual two.
