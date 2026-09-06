# QA-01 and QA-03 from the external audit: the last two harness leaks (v1.9.311)

**Date:** 2026-09-07. The user: "do QA-01 and QA-03 too." Both reproduced first with whole-binary shuffled runs (CTest runs each case in its own process, which is why the suite never showed them).

## QA-01: oracool_audit_test, 17 crashes under shuffle

Two mechanisms, found by ordered experiments:

- **Divide by zero (nine class-tree tests).** After any test mounted the archives, an aura toggle found its sound files and played them into a mixer that was never initialised. Engine-side fix: `PlaySkillSound` and `ResumeClassAuraLoop` gate on `gbSndInited`, the same gate `PlaySFX` has. No device, no sound - correct in the game too.
- **Access violations (art tests, the hero-chunk aura test).** `LoadCoreArchives` called a SECOND time in one process crashed the binary - repeating the oil-tint test alone reproduced it - and `FreeCursor` at the end of one test left the next drawing from sprites that were gone. Test-side fix: `MountTestArchives` and `EnsureCursorSpritesLoaded` mount once per process; every `LoadCoreArchives`/`LoadGameArchives`/`InitCursor` call in the file goes through them and the five `FreeCursor` calls are gone. The process end frees.

Result: 248 tests pass shuffled on the audit's seed (22062) and two others.

## QA-03: player_test

`PM_DoGotHit` and the Furious Charge gate test read the player the previous test left behind (the recovery frame depends on it; the gate reads the level). A fresh `Player {}` for each, and the charge state reset. Eight shuffled repeats clean.

Suite 647/648, the standing dungeon-generation failure only. Every finding in the 2026-09-06 audit is now closed; the response table is complete.
