# 2026-09-29 - The three Ancients (v1.12.225)

**Date:** 2026-09-29. Debug only. The user, over five messages:
- "make one of the ancients to throw weapons and one to attack with whirlwind."
- "check d2 ancients which one did what."
- "tint the three ancients in different color as they are different characters."
- "korlic to move from leap to regular melee attack."
- "madawc to toss the hammer asset we introduced for blessed hammer skill. scaled to be no bigger that regular barb axe
  toss."

## D2's three

At Arreat Summit: Talic the Defender whirls (Whirlwind), Madawc the Guardian throws (Double Throw), Korlic the
Protector leaps (Leap, Leap Attack). Ours now follow the first two; Korlic, at the user's word, is a plain fighter.

## Before

Korlic had a Leap ability; Talic's "Whirlwind" and Madawc's "Hammer Toss" were instant hits on a cooldown that dropped a
ring. Talic never spun and Madawc never threw anything visible. All three were one grey ramp.

## Now (oracool/companion)

| Ancient | Tint | Fights |
|---|---|---|
| Korlic | PAL16_BLUE | melee with his sword, no ability |
| Talic | PAL16_RED | `CompanionAttack::Whirl`: spins while anything is beside him |
| Madawc | PAL16_YELLOW | `CompanionAttack::Throw`: the Blessed Hammer's hammer from up to 6 tiles; Double Throw every 6 s |

- **Talic's spin:** when an enemy is beside him the brain starts a spin instead of a swing (`StartCompanionSpin`);
  `ProcessSpin` strikes everything beside him every 5 ticks at his share of the owner's blow, playing the hero's
  Whirlwind strike cue, and keeps going while anything is there (a second at the least). `CompanionAi` leaves him alone
  meanwhile. He is drawn as the hero's Whirlwind is: `CompanionSpinSprite` gives his magic cast sheet on its full-cloud
  frames, turning a facing a tick (`WhirlFrame`, shared from oracool/whirlwind), and `DrawCompanionBlades` the four
  circling blades (`DrawWhirlingBlades`). The cast sheet loads into its own slot (`HeroSheets::spin`), not Special:
  Special plays when a body spawns and would have made every companion arrive casting.
- **Madawc's throw:** a ranged attack on his own mace sheet's swing, as a bow companion shoots; at the release
  `ThrowHammers` launches the engine's arrow with his share of the owner's blow (the Barbarian's Weapon Throw path),
  dressed in `BlessedHammerSpin` at 75%: the thrown axe averages 27px across its frames, the hammer 35 at full size.
  Double Throw (was Hammer Toss) throws two side by side.
- **Abilities:** Leap and the instant Whirlwind are gone from the enum; `AbilityPercent` no longer boosts anything.
- **Text:** the Ancestral Call description and the tooltip say who does what.

## Tests

`OracoolCompanion` stats test: Korlic Melee, Talic Whirl, Madawc Throw. Debug build and ctest: 882/882. Not seen in
play: the spin, the hammer's size and the three tints want a look.
