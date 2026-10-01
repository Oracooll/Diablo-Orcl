# 2026-10-01 - Torment picker, Diablo II Strength, audit round 50 (v1.12.285)

**Date:** 2026-10-01. Debug only. 899 tests pass; the Debug `diablo.ini` was unchanged by ctest; oracool.mpq was repacked. v1.12.284 was an intermediate build whose two expectation tests were then updated.

## Torment picker (user, 2026-10-01)
Choosing Torment on the difficulty screen now opens an 800x600 window with eight columns. Each column is one multiplier of Hell: x1.5, x2, x2.5, x3, x3.5, x4, x4.5 and x5.

- **Art:** the user's design, scaled from 1448x1086 to 800x600 (`ui/torment_select_bg.png`). It sits centred, lifted 12 px above the OK / Cancel row so those stay in zones 2 and 3.
- **The pentagram:** the big spinning one (`ui_art\focus42`, 42 px, 8 frames) turns in the chosen column's circle. The circle centres were measured on the asset; the inner ring is about 52 px across. While focus is on OK or Cancel it is dimmed but still turning, marking what OK will take.
- **Front-end rules:**
  - The columns are wordless buttons in the shared focus ring, so the arrow keys walk them, then OK and Cancel.
  - A first click marks a column; a second click, Enter or OK takes it.
  - Esc or Cancel goes back to the difficulty screen.
- **What the choice sets:** `Oracool.tormentDifficultyMultiplier`, which every Torment formula already reads (monster life, damage, armour, to-hit, level, XP and gold). The pentagram opens on the last choice.
- **Shared changes in `diabloui.cpp`:**
  - the button ring sorts row by row, so one-row screens are unchanged;
  - a button with no words draws no end pentagrams;
  - `UiFocusedButton`, `UiFocusButton` and `UiDrawFocusPentagram` are exported.
- **Rendered:** `OracoolPreview.DISABLED_TormentPicker` draws the art at its screen spot with a pentagram frame in all eight circles. Each sits centred inside its ring.

## Strength multiplies the weapon (user, 2026-10-01)
- **The new field:** `_pDamageMod` (a flat level x stat / k) is replaced by `_pStatDamageBasisPoints`: stat x (100 / k)% of the weapon's bare roll.
- **One adjustable number:** `StatDamageTenthsPercentPerPoint` (player.h) scales it. 10 means 1% a point, as in Diablo II.
- **Each class keeps its weights:**
  - Rogue: Strength + Dexterity, 0.5% each;
  - Monk: Strength + Dexterity, 0.67% each, halved with a weapon that is not a staff;
  - Barbarian: axe or mace 1.33%, bow 0.33%, plus Vitality 1% without a shield;
  - everyone else: 1%.
- **Bows:** a bow outside the Rogue still takes half.
- **Level** no longer multiplies the bonus.
- **Every blow** reads `StatDamage(player, weaponRoll)`: melee, arrows, the class melee skills, Paladin ranged, the RfA-12 weapon blows and companions. The sheet and hero select do too.
- **Test:** `StatDamage_IsAShareOfTheWeaponRoll`.
- **To watch in play:** late-game damage at moderate Strength falls compared with the flat rule. If it is too low, raise the one constant rather than touching monsters.

## Audit round 50 (regression review of v1.12.283)
- **Repel mark:** a bat or sneak repel ended by Taunt or the Siren no longer leaves its mark behind.
- **Zero blows:** a 0 monster-vs-monster blow wakes and flinches again. Negative blows are still clamped to 0.
- **Dropped uniques:** a unique dropped from a full floor is logged.
- **Multiplayer header:** base attributes are clamped at 255 rather than wrapped (multiplayer only).
