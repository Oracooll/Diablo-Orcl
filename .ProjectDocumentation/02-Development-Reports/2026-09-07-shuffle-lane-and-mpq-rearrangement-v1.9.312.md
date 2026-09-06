# The whole-binary shuffle lane; the rearranged Oracool.MPQ (v1.9.312)

**Date:** 2026-09-07. The user: "add the shuffle repeat CI lane too" and "look at oracool.mpq. i have rearranged it a bit."

## The lane

`test/CMakeLists.txt` registers, beside every discovered case, one CTest entry per test binary that runs it WHOLE, shuffled, three times (`--gtest_shuffle --gtest_repeat=3 --gtest_brief=1`), labelled `shuffle`. Select it alone with `ctest -L shuffle`; a plain `ctest` runs it too, so the Linux workflow's existing ctest step picks it up. gtest prints the seed, so a failure is reproducible with `--gtest_random_seed`.

Its first run caught two more binaries the audit had not: `path_test` (the solidity and walkability cases wrote `dPiece`, `dObject`, `SOLData` and `Objects[0]` and never put them back, so the long-path cases found different paths) and `pack_test` (cases set `gbIsSpawn`/`gbIsHellfire` for themselves and left them; the next case's item validation ran in the wrong mode). A `PathTest` fixture zeroes the grids, the table and the object; both pack fixtures reset the three mode flags. Three lane runs clean; the only red is `drlg_l1_test_shuffled`, the standing fixture mismatch's twin.

## The MPQ

The user folded `01-in-use`, `03-concepts`, `04-references` and `05-asset-plan` into `02-source-art` and renamed `99-original-game-art` to `00-original-game-art`. Eight tools named the old folders and were repointed: build_inventory_assets.cmd, build_waypoint_cel.cmd, BuildWaypointPanel.ps1, CutBurgerMenuIcon.ps1, CutTownPortalIcon.ps1, BrightenPanelBg.ps1, build_asset_studio.cmd and the Asset Studio form. `CutClassSilhouette.ps1`'s reference sheet went with 04-references; its outputs are shipped, so it is annotated rather than repointed. The README's Layout block and the tool tables describe the new tree.

Suite 681/683: the standing dungeon-generation failure and its shuffled twin.
