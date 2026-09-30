# 2026-09-30 - Whole-code audit, round 6 (v1.12.231)

**Date:** 2026-09-30. Debug only. The audit continues at the user's word.

Round 6 ran six read-only tracks:
1. a sweep for monster-array reads in town, plus the remaining slot reuse;
2. level generation;
3. quality-of-life features and sound;
4. the spell-casting core;
5. monster, minion and companion AI;
6. settings, menus and the keymapper.

Every finding was verified against the code first.

## Fixed: ctest was overwriting the player's settings

- **The test exes share `C:\Diablo Orcl\x64-Debug` with the game**, and on Windows the config path is the exe's
  folder. Any test that reached `SaveOptions` wrote the test process's defaults over the player's `diablo.ini`. Paths
  that reach it: a tree point's autosave hook, `gamemenu_off`, the run toggle.
- **The result:** no key bindings, `Width=0`, an empty language, gamepad deadzone 0.
  - The live file carried these fingerprints, written seven minutes after a build.
  - Every test run since the fork's tests reached `SaveOptions` did this.
- **The fix:** `test/main.cpp` now gives every test process its own config and pref folder under `%TEMP%`, and removes
  it at exit.
- **Checked:** after a full ctest, the Debug folder's `diablo.ini` is byte-for-byte unchanged (same md5).
- **Restored:** the player's settings were copied back from the release folder's `diablo.ini` (2026-09-26: 1280x720,
  English, the key bindings, deadzone 0.07). The damaged file is kept as `diablo.ini.test-overwritten.bak`. Anything
  changed in the Debug build since the 26th needs setting again.

## Fixed: crashes and stuck states

- **A scrolling text whose voice file is missing divided by zero.** Since the fork stopped raising an error for a
  missing sound, a zero length reached the text-speed division. It is latent on a full install and a crash on one
  without `hfvoice.mpq`. It now falls back to the no-sound estimate.
- **A pack member shattered in stone left its unique unable to walk again.** Its death skipped the pack update, so
  `packSize` stayed one too high and `DirOK` refused every step. A leader shattered in stone left its pack tied to the
  slot the next summon takes. Now:
  - a petrified death updates the pack;
  - `ReleaseMinions` frees separated followers too.
- **A dead companion never left.** At the end of its death animation its body keeps the Death mode, and the tick only
  looked at the mode. The companion stayed on the HUD, held its golem slot, and stood up again on the next level. It is
  now released.
- **A named encounter's boss was placed with town's tile data.** A set level loaded its tile properties after its
  monsters. The Sealed Map boss could stand in a wall or outside the arena. The properties now load first.
- **The Ring of Mourning** is built of Hell tiles and was loaded as the Catacombs. Its row is now `DTYPE_HELL`.

## Fixed: town and reused slots

- **A missile's side is fixed when it is fired.** A dead acid beast's puddle became the army's when a skeleton was
  raised into its slot. It stopped hurting the hero and burned his enemies instead.
- **Bow skills in town** made the hero blink and still charged the mana. They are now refused, as melee attacks
  already were.
- **The Guardian, Chain Lightning, Apocalypse and Telekinesis** no longer target townspeople. In town the monster map
  holds their ids.

## Fixed: skills

- **Leap mid-walk snapped back.** The walk's last frame put the hero on the step's target tile, and the leap was still
  paid for. The walk is now stopped first. A leap is refused in hit recovery or a block.
- **The Paladin's cast skills** are re-checked at the cast. A second click on the last mana played a whole cast that
  ended in nothing.
- **Tree skills short of mana, Rage or Essence** now say so; they failed without a word.
- **Bow skills** check room for the arrows before taking the mana.
- **All twelve F-key slots** are cleared of refunded skills, not just the first eight; Quick Cast reads all twelve.

## Fixed: monsters

- **Thunderous** now lays its ring. It fired one spark at its own tile, which sat on the loot for thirteen seconds.
- **Skeletal Mages in Hold** shoot at range again. Their reach was 1.
- **An enemy champion's Might** no longer lifts the Necromancer's skeletons.

## Fixed: QoL and menus

- **Auto-pickup:**
  - Past the adjacent ring it needs a clear line; it took gold from behind walls.
  - It is no longer stuck on an item whose request was dropped. This happened with the two halves of a split stack.
- **Settings:** option descriptions in the long categories (Diablo Orcl, Keymapping, Game) were drawn below the
  screen. The list cap had been sized for vanilla's list top.
- **Respawn In Town** works while the hero is still falling.
- **Under the open game menu** only the HUD's Menu cell answers. A portal clicked through the pause was cast the moment
  the menu closed.
- **Held R, W and J** no longer auto-repeat. R rewrote the INI thirty times a second.
- **The tick rate** is clamped to the speed band when a game starts. A 0 in the INI divided by zero.
- **Panel Gamma's description** now says it takes effect after a restart.

## Not changed (open)

- **Auto-pickup** takes back an item the player just dropped. That is the user's call; the round 6 summary asked.
- **Leap Attack and Vaulting Strike** charge the leap and the strike separately, and the strike needs a second click.
  This may be deliberate.
- **RfA-12 bow skills with no bow** play a full cast, then refuse.
- **The skill picker's F-key bind** guesses the kind (staff, scroll, book) instead of taking the hovered cell's.
- **A waypoint can land on the Poisoned Water entrance**, which is not a trigger.
- **The Stone Curse shatter on a reused slot** stops early (cosmetic).

## Tests

- **Test harness:** config and pref paths are sandboxed per process (`test/main.cpp`).

Debug build and ctest: 882/882. Nothing seen in play.
