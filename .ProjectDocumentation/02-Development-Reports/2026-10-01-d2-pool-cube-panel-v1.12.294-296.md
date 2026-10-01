# 2026-10-01 - Diablo II damage pool, the Cube's whole panel, class pages (v1.12.294-296)

**Date:** 2026-10-01. Debug only. 900 tests pass; the Debug `diablo.ini` was unchanged by ctest; oracool.mpq was repacked at v1.12.295.

## v1.12.294 - the user's decisions
- **Strength in Diablo II's pool.** `PooledWeaponDamage(player, roll, poolPercent, statShare)` = the bare roll x (100% + item +% + the stat share + every skill/passive +%), plus the flat bonus.
  - **Where it applies:** the swing (skill bonus, follow-up and Whirlwind shares, passives), arrows (passives pooled, not re-applied), PvP, companions, `FullBlow`/`WeaponBlow`, Paladin ranged, the rogue arrow quotes, the sheet, the readied-skill line and hero select.
  - **Still multiplying:** "X% weapon damage" shares, and the passives that RfA-12 `Strike` applies.
  - **Test:** `PooledWeaponDamage_AddsEveryPercentIntoOnePool`.
- **A failed floor save** is caught (`BeginSaveAttempt` / `SaveAttemptFailed` in `pfile_save_level`). On failure:
  - `ForgetUnsavedLevel` marks the floor not visited and drops its temp snapshot;
  - a red SAVE FAILED line and an on-screen message say so.
- **Death Mark:** its burst takes the body (`TakeCorpseOf`).
- **Vanilla uniques:** on fresh drops, a vanilla unique needs a stamped item level of at least its UIMinLvl.
- **Kept as they are:** silent no-target fizzles, and Charge's unarmed dash.

## v1.12.295 - the opened Cube
- **The held frame:** the opened state holds the last opening frame (19). Its panel is whole and carries a painted 3x4 grid; every opened still was cropped at the top. Previewed on the Levski's Cube Frames page and approved.
- **The live view:** it draws only the window's items, in 12 px cells from (46, 19) on the painted grid. The glaze and rules are gone (user: the painted grid is enough).
- **Waypoint sigils** keep 3 tiles away from active quest entrances (round 57 audit: the Poisoned Water entrance).

## v1.12.296
- `DrawLevskiCubeItemsAt` is split out.
- `DISABLED_LevskiCubeItemsOnFrame19` renders real item pictures on frame 19 for the user to check the fit.

## Audit round 57 (rolled into v1.12.294-295)
- **Regression review of v1.12.292-293:** the build script was back to CRLF, and two tool comments were fixed.
- **Quests and level generation (4th pass):** found no unfinishable quest. Fixed the waypoint sigil on quest entrances. Left as they are:
  - rift quest-room shapes (cosmetic);
  - a cursor quest item lost on a rift death with every container full.

## Class Skill Cards pages
- **All six republished** from a fresh export, with current skill texts.
- **The Sorcerer page** shows the cold dev notes (v1.12.271-272):
  - Chill Touch at 100%;
  - Frost Nova at 200% with cycling colour;
  - Glacial Spike at 75% and its shatter at 50%;
  - Blizzard's shards at 50%;
  - Frozen Orb at 50% with Glint (a new picker tint);
  - Frozen Sentinel's shards at 50%, its body the Guardian sheet in ice blue;
  - Ice Lance as a 200% ice-blue arrow;
  - Whiteout as Fire Wall at 50% in ice blue.
- **Pick databases:** every pick is already in the game (Sorcerer 37, Paladin 20, Barbarian 55). The Rogue, Monk and Necromancer pages have no picks yet.
