# 2026-10-03 - Dev notes: the hero sheet and timed elixirs (v1.12.360-362)

**Date:** 2026-10-03. Debug only. All tests pass at each build (915, then 916). The Debug `diablo.ini` was unchanged by ctest.

| Version | Commit |
|---|---|
| v1.12.360 | e1e58915 |
| v1.12.362 | 7bfb9572 |

v1.12.361 was a working build that a crashing new test stopped. Its changes ship in v1.12.362.

## v1.12.360: the hero sheet (four dev notes)
- **No scrolling:** the grouped sheet always fits its window. `MaxScrollOffset` is 0 and asserted, and the scrollbar is gone. The list layout (with `heroSheetGrouped` off) still scrolls.
- **Necromancer, Resist Cold:** the Essence frame pushed Resist Cold 18 px past the stash-grid line, where it was cropped.
  - The right column's gap is now the widest up to 6 px that fits: 4 for him, 6 for every other class.
  - No 1 px cut was needed elsewhere.
- **XP bar:** its frame and 10% ticks are white.
- **Readied skill rows:** "left button" / "right button" became "LMB" / "RMB".
- **Preview:** `OracoolPreview.DISABLED_HeroSheet` also writes `hero_sheet_necro.png`.

## v1.12.362: timed elixirs
The user first asked for a countdown. Elixirs here were vanilla's permanent +1 base point, so there was nothing to count down. The user then set the rules: "+5 point. to last 5 minutes ... stack 10 consumes ... at 11th consume hero says I cant do this. when barb tries to consume magic elixir to refuse ... countdown icon to be the elixir potion icon."

- **The module (`oracool/elixirs`):** each stat has its stacks and ticks on the Player (`_pElixirStacks` / `_pElixirTicks`).
  - Never saved. Cleared in `InitPlayer` for a new game; kept through death.
  - Each drink adds +5 on the item side (in `CalcPlrItemVals`) and resets that stat's clock to 5:00. The whole stack ends together, recalculated with `CalcPlrInvKeepingLife`.
  - Magic still refills mana and Vitality still refills life.
- **Refusal:** `RefuseElixir` is asked in `UseInvItem` (backpack and belt) and in the stash before the item is used up. It refuses the 11th of a kind, or Magic for a Barbarian.
  - The local hero says "I can't do that" and a log line explains why. The elixir is kept.
  - Only the local hero speaks: a test's stand-in hero has no sound to play, and that crashed v1.12.361's test.
- **Countdown:** the column beside the mini-map has one row per stat, with the elixir's 28x28 inventory icon, an `xN` stack badge, and the seconds left.
- **Tooltip:** two rows, at the user's request: "Temporary boost +5 to <stat> for 5 min." / "Stacks up to 10 times."
- **Hero sheet:** the readied rows read "LMB: damage" (user: "replace (space space dash space space), with (:space)").
- **Test:** `OracoolElixirs.StackToTenThenRefuseAndABarbarianRefusesMagic`.

## Not seen on screen yet
- The elixir rows in the countdown column.
- The hero sheet in game.
