# 2026-09-27 - Dev notes: fourteen from play (v1.12.201)

**Date:** 2026-09-27. Debug only. The user said: "check dev notes and process".

Fourteen notes were open in `development.md`: four logged under v1.12.196 in the morning and ten from a v1.12.200 session. All
fourteen are answered and moved to `development-archive.md`, each with what was done. Thirteen are coded. One (the stretched
sheet backing) needs art, so a request was written and the code is ready for it.

## Skills

- **Shield skills level without a shield.** The shield moved from `IsPaladinSkillUnlocked` to `CanUsePaladinSkill`.
  - Smite and Blessed Shield take points and stay on their buttons without a shield.
  - A use without one says "I can't do that".
  - The tree's lock text no longer names the shield.
- **Heavenly Strength costs** -20% damage, -20% to hit and -20% attack speed while the grip is in use: a two-hander in one hand and a shield in the other.
  - Damage and to hit are applied by `ApplyPassive`.
  - The speed is a negative frame skip in `StartAttack` (`HeavenlyStrengthSwingDelayFrames`: a quarter more of the attack's frames).
- **Charge dashes at 2 ticks a tile** (0.1 s): `ChargeDashSkipFrames` = 6 on the walk's 8 frames. Slows don't reach it.
- **The right button casts the three Paladin cast skills at the ground.** Blessed Shield, Blessed Hammer and Fist of the Heavens used to walk to a monster first; they now cast like any spell (a monster out of reach is cast at).
- **Smite and Aegis Slam strike on the unarmed-with-shield attack sheet.**
  - A new `player_graphic::ShieldAttack` loads `?lu/?mu/?hu` + `at` for the Paladin and follows the armour worn.
  - `SwingsShieldAttackSheet` decides it; `MeleeHitFrame` gives the sheet's own hit frame.
  - The block sheet stays as the stand-in for when the sheet cannot load.
- **Conversion's monsters are green and never attack you.**
  - `IsMonsterConverted`; `UpdateEnemy` skips players for them.
  - `MonsterAttackPlayer`, the charge hit and their missiles refuse a player.
  - The colour is `ConvertedRgbTable`, beside the frozen look's.

## Art

- **25 directional skill sheets were mirrored.**
  - RfA-27's sixteen-direction sheets are wound counter-clockwise; the engine reads clockwise. Every facing but straight up and down pointed the wrong way.
  - I checked every PNG sixteen-direction missile sheet against Magic Arrow. The 25 from that batch had their rows reordered (row r ← row 16−r); the older sheets were already right.
  - Two were left as they are: Whiteout Wall (11 rows) and Ride the Lightning (a zigzag with no readable facing).
- **Holy Lance** now flies Guided Arrow's gold arrow from the hero through the opponent to the second tile behind it. Its damage was already right.
- **The Hive and Crypt loading screens** had been saved under each other's names since v1.11.090. The two PNGs are swapped; the code was right.
- **The 16 Orcl tier shields are 2x3** (`cursor.cpp`, and a 56x84 cut), and their icons are 17-50% bigger.
  - `ReseatOutgrownItems` runs once per game load. It moves any item that no longer fits where it was saved (backpack pages, stash) to a free spot, or to the hero's feet.
  - Socket caps follow the cell count, so these shields can roll 6.
- **The 16 tier belts** are no longer cut at 125% (they were clipped on every side). The Obsidian belt's source box was shortened; it had caught the armour below it.

## Interface

- **The XP bar** plays the menu sound on hover entry and on click, as the HUD's buttons do. A click on it no longer walks the hero.
- **The skill picker** has sections:
  - ATTACKS;
  - the Abilities window's three tree tabs, "1 - <page>" to "3 - <page>", each in that window's reading order;
  - SPELLS, SCROLLS and STAFF SPELLS, sorted as the Spells tab sorts: level band, then name.
  - One `Section` list drives the draw, the click, the hover and the scroll.
- **Sheet backing:** a strip taller than a frame is now shrunk by averaging rows. `RfA-31 - Character Sheet Frame Backing.md` asks for the three pieces at 132 pixels tall, the tallest frame, with the edges at their current thickness.

## The art folder moved

`Resources` is now `01. Blizzard Assets` / `02. Oracooll Assets` (with `01. Used` and `02. Unused`) / `03.DevilutionX Assets`.

About 50 tools, and the generated icon spec files, still name `00-original-game-art` and `01-in-use-assets`, so `tools\build_item_icons.cmd` refuses to run.

For this batch the icon sheet was cut from a scratch copy of the script with every art path mapped to the new layout (116 paths, none missing). The result was checked frame by frame against the committed sheet: 643 frames, exactly the 32 belts and shields differ, and the other 611 are byte-identical.

The tools themselves are not repointed yet.

## Tests

- New: `InvTest.ReseatOutgrownItems_MovesAnItemThatGrewIntoItsNeighbour`.
- Changed: `OracoolClassTree.EveryBorrowedPaladinSkillMatchesItsTreeTier` now asserts that the shield gates use, not points.
