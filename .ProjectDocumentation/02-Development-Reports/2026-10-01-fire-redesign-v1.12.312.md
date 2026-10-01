# 2026-10-01 - The Sorcerer's fire redesign (v1.12.312)

**Date:** 2026-10-01. Debug only. 902 tests pass; the Debug `diablo.ini` was unchanged by ctest. oracool.mpq repacked. Commit 54afd8f0.

The user reviewed previews on the Sorcerer Skill Cards page (Fire tab). The only input on them was Ember Mine at 50%, so the rest was built as previewed.

## Skills
- **Meteor shower:**
  - The cast makes one field per rock: 12-16 rocks at random open tiles within 5 tiles of the cursor.
  - Each field's clock starts at `-delay-1`. It shows the fall at clock 0 and lands a second later, so the landings spread over a second.
  - Each rock is a 1-tile blast at the old damage, and burns 1 tile.
  - `MaxFields` went from 32 to 64 to hold a shower beside the other fields.
- **Funeral Spiral** (was Funeral Star):
  - 12 Fireball sheets on art carriers (var2 index, var5 field stamp, var6 `FuneralSpiralMark`) are moved by `TickField` along `angle = phase + 0.14 t`, `radius = 12 + 4 t` (y halved for the iso floor).
  - Each faces the spiral's tangent by sixteenths. The first monster a ball meets takes the skill's damage, and the ball bursts in `BigExplosion`.
  - Each ball carries a light. The stand-still channel and both RfA-27 sheets are gone.
- **Ashen Brand curse:**
  - Every monster within 3 tiles of the cursor gets the brand for 4 s, +0.5 s per level. Any branded monster that dies bursts (`ApocalypseBoom`, the vanilla fire explosion).
  - `AshenRing` is the game's `aura_holy_fire` at 448x224 with its glow dithered. It fades in over 6 ticks and out over 8, by `oracoolAlpha` in `ProcessCensusEffect`, with a Glint.
  - The cast sound is flamwave.
- **Furnace Mouth:** the user's 8-row furnace (`FurnaceMouth`) stands for the vent's life, turned to the cast's way. The new `FurnaceFlame` is spat each pulse for 12 ticks with a Glint, and lights radius 5.
- **Ember Mine:** the user's mine A. The stone is locked to its median colour across frames (anti-wobble), halved to 64x32.
- **Renames:**
  - Blaze becomes Flame Wave.
  - Hydra becomes Fire Hydra, and the Guardian spell is renamed Fire Hydra game-wide, so books and staves follow.
  - Funeral Star becomes Funeral Spiral.

## Tools
- `tools/BuildFireSheets.py` builds `ember_mine.png`, `furnace_mouth.png`, `furnace_flame.png` and `ashen_ring.png`.
