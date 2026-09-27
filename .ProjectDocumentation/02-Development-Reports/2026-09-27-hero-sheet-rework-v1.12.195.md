# Hero sheet rework: the user's frames, stat buttons, odds bars

2026-09-27 — v1.12.195

## Why

The four dev notes on the v1.12.194 grouped sheet, then about thirty rounds of changes the user asked for one render at a time. Every round was drawn by the game's own code in `OracoolPreview.DISABLED_HeroSheet`, sometimes with a green line at y=617 (the stash grid's bottom row) laid over the image, before anything was built. The game exe stayed at v1.12.194 until the user said "make a build".

## Frames and spacing

- **Frame art:**
  - The frames first used the vanilla stat field (`data\boxleftend/boxmiddle/boxrightend.clx`), cut to any size.
  - I exported those as PNGs, and the user redrew them.
  - `ui\stat_field_left/middle/right.png` (5x25, 284x25, 5x25) are now the frame: 2px of gold at the top and right, plus a 2px light lip at the bottom and left.
  - `advanced_stats.cpp` loads them in true colour and stretches only the middle rows. Vanilla's CLX is the fallback.
- **Text:** the ink (glyph plus shadow) sits 2px inside the frame. `FieldInkTop` is 4 and `FieldInkBottom` is 6. Text rects are 22 tall with the ink 6 rows down, because a shorter rect clipped the glyphs' feet.
- **Shadow:** frames cast a shadow 3px down and left. RESET (while it lasted), the toggle's old button and the frames all use `DrawSheetShadow`.
- **Inset:** content sits 4px inside the canvas frame (x 26 to 313).
- **Gaps:** 6px everywhere.

## Layout

- **Header:** name, title, "Level N Class", a 16px XP bar, the experience, and the remaining experience. Each is on its own row, 6px apart.
- **Advanced Stats toggle:**
  - A 25x25 +/- button (`ui\sheet_toggle_plus/minus.png`) sits in the header's top-right corner, 4px inside the frame.
  - It sinks when pressed and acts on release.
  - It replaced the ADVANCED STATS button.
- **Right column:**
  - Both mouse-button boxes, then Armor class and To hit (each with an odds bar), Life and Mana, and the four resistances (each with a bar).
  - The frames stretch evenly so the column ends on the stash grid's bottom row (`GroupedColumnsBottom = oracool::GridBottom - GroupedContentTop`). Each frame's contents are centred in the extra height.
- **Left column:**
  - Four attribute frames and two points frames: stat points on the left, skill points on the right (the Abilities window's side).
  - They are one height, 6px apart, and reach the same line. Leftover pixels go into the gaps.
- **Removed:**
  - The aura row, which the readied button's box already covered.
  - The resistance cap strip, which moved to Advanced Stats as "Resistances capped at 90%" plus the difficulty penalty in red.
  - RESET on the grouped sheet; the list sheet keeps it.
- **Advanced Stats:**
  - Plain shadowed text, with gold titles and 10px above each title.
  - Docked flush with the sheet's right edge. It closes with the sheet and still puts the inventory or Abilities window away.

## Stat buttons

- **Attribute frame:**
  - The name is centred.
  - Below it are "Now" and "Base" with a hairline between them, then the two values.
  - Under each column is a wide button cut from the user's `ui\stat_point_button.png`: - under Now, + under Base. The border is kept and the stone is cropped rather than squeezed.
- **Button behaviour:**
  - The buttons sink 2px down and left, which leaves the sunk face 2px inside the frame.
  - Hover brightens them. When held they show the idle colour.
  - The +/- is drawn in code.
- **Always active:**
  - A click does 1 point, ctrl does 5 and shift does 10, in either direction.
  - - uses the new `RefundStatPoints(player, attribute, count)` in player.cpp. It only takes back points spent on that stat (`_pStatPtsSpent*`), in single player, for the local hero.
  - Tested in `Player.RefundStatPoints_TakesBackOnlySpentPointsOfOneStat`.
- **Hit rects:**
  - `ChrDecBtnsRect[4]` and `chrDecBtn[4]` (control.h) pair with `ChrBtnsRect` and `chrbtn`.
  - The list sheet resizes `ChrBtnsRect` back to vanilla's 41x22.

## Bars

- **Look:** every bar has a 1px grey frame with cut corners, ticks every 10%, and a colour cycle (a band of lighter and darker colour flowing along, 48px per band, 1.6s per loop).
- **Resistances:** the bar is full at 90% (the cap) and drawn in the label's colour: magic gold, fire red, lightning yellow, cold blue. A negative value fills red from the right, down to the -100 floor.
- **Odds bars (`oracool/combat_odds`):**
  - Armor class shows the chance that the last monster to land a melee blow on the hero hits them now, from green to red.
  - To hit shows the hero's chance now against the last monster they swung at or shot, from red to green, clamped 5 to 95.
  - The monster's side of the game's own formula is frozen at the blow and the hero's side is read live, so gear changes move the bar.
  - The colour runs red, then yellow, then green.
  - Hovering either box names the monster and the chance, through `SetCharacterSheetHoverInfoString` in `UpdateInfoString`.
  - The hooks are in `MonsterAttackPlayer`, `PlrHitMonst` (not cleave neighbours) and the arrow branch of `MonsterMHit`. A new game clears them.

## Tests

- The v1.12.195 Debug build is clean. The full suite passes, 850 of 850, including the shuffled audit suite.
- New: `Player.RefundStatPoints_TakesBackOnlySpentPointsOfOneStat`.
- **Preview (`DISABLED_HeroSheet`):**
  - It now draws a named, levelled Paladin in Hell, with negative resistances and a known attacker and target.
  - It also draws a second render with Advanced Stats shut and zero skill points (`hero_sheet_shut.png`), and 16 frames of the bar cycle (`hero_bars_NN.png`).
  - It prints the two hover texts.
  - Drawing the cursor tooltip itself asserts in a bare test process, so the texts are printed instead.
- **Exports for the preview:** `chrflag`, `chrbtn`, `chrDecBtn`, `ChrBtnsRect`, `pChrButtons`, `LoadCelListOrSheet`, and the test hook `oracool::SheetBarClockOverrideMs`.
- **Assets:** six PNGs are added to `Packaging/resources/oracool_assets/ui`, and oracool.mpq is repacked in the Debug tree.
- **Not yet seen in the game:**
  - The toggle's and buttons' press feel.
  - The odds bars in real combat.
  - Monster missiles don't feed the Armor class bar; only melee blows do.
