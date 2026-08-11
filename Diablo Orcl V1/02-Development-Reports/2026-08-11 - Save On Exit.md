---
title: 2026-08-11 - Save On Exit
date: 2026-08-11
tags: [dev-report, save-system]
summary: Leaving via "Main Menu" or "Exit Game" now saves the character. Closes a gap where opening the menu to quit actively blocked the autosave your last actions had earned. Saves on death too, with health restored.
---

# Save On Exit

## The gap was worse than "no save on quit"

Two things compounded.

`RunGameLoop`'s exit-path `pfile_write_hero()` is wrapped in `if (gbIsMultiplayer)`, so single-player
never saved on the way out at all.

The scheduled autosave could not cover for it either, and not by accident: `IsSafeToSave()`
requires `!gmenu_is_active()`. Opening the in-game menu to choose "Main Menu" therefore
**blocks** any autosave your last actions had scheduled - and then the loop exits and it is
discarded. So the failure was not merely "quitting does not save"; quitting specifically threw
away a save that was already pending.

## Where it hooks

`GamemenuReturnToMainMenu` and `gamemenu_quit_game` both funnel through `GamemenuNewGame`, so one
call there covers both entries, in both the living and the on-death menu variants.

Ordering is load-bearing. `GamemenuNewGame` sets every player's `_pmode` to `PM_QUIT` and clears
`MyPlayerIsDead`; saving after that would persist a quitting player rather than the one who was
just playing. `SaveOnExit()` runs first.

## Why it does not reuse IsSafeToSave

`SaveOnExit()` deliberately does not call the shared predicate. It requires `!gmenu_is_active()`,
which is false by construction here - reusing it would have made the whole feature a silent no-op
that looks implemented. Its other conditions (no store open, cursor not holding an item, not
paused) exist to avoid saving a mid-interaction state that play would resume from; the game is
ending here and only the character is kept, so none of them apply.

Kept: single-player only, `gbRunGame`, a non-null player, and not during demo playback or
recording.

## Saving on death

Verified rather than assumed: saving a dead character does **not** damage the file.
`UnPackPlayer` (pack.cpp) floors it -

```cpp
if ((int)(player._pHPBase & 0xFFFFFFC0) < 64)
    player._pHPBase = 64;
```

HP is stored in 64ths, so a dead hero loads at 1 HP, not as a corpse. (`InitPlayerGFX` does load
the Death graphic at `_pHitPoints >> 6 == 0`, which is what a missing floor would have produced.)

Health is restored to full on the death-save anyway. 1 HP is a miserable restart for no design
reason, and the alternative - skipping the save on death, as originally proposed - would have let
a player quit on death to roll back to the last autosave and dodge the penalty entirely. Saving
with health restored means death costs exactly what death costs.

## Character progress only

`SaveGame()` is `pfile_write_hero(true)` + `sfile_write_stash()`. `SaveOnExit()` passes **false**.

What the `true` adds is the world snapshot: current dungeon level and position, monsters, objects,
ground items, in-progress quest state, portals. **None of it is ever read back in V1
single-player** - `LoadGame()` is unreachable because every character always starts a fresh
dungeon (see [[2026-08-09 - Character-Only Persistence, No Continue]]).

Everything durable travels with the hero regardless, which was checked rather than assumed:

| Data | Where it lives | Saved? |
|---|---|---|
| Stats, XP, inventory, belt, equipment, gold | `PlayerPack` | yes |
| Waypoint unlock table | `PlayerPack` (bitmask per difficulty, pack.cpp) | yes |
| Extra inventory tabs | own sub-file, alongside `SaveHeroItems` | yes |
| Stash | own file, `sfile_write_stash()` | yes |
| Dungeon/monsters/ground items/quests/portals | game-data half | no - never read |

## Verification

Debug build clean, `ORACOOL_VERSION` 1.0.89, all test suites pass except the pre-existing
`drlg_l1_test`. Needs a play-test: quit via both menu entries and confirm progress survives, and
die, quit, and confirm the character returns alive at full health with the death's costs intact.

## Related

- [[2026-08-09 - Character-Only Persistence, No Continue]]
- [[2026-08-09 - Autosave-Only Play, Part 1]]
