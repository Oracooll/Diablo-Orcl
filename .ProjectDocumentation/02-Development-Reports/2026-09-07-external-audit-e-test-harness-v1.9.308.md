# External audit (ChatGPT, 2026-09-06), patch E: test-harness isolation (v1.9.308)

**Date:** 2026-09-07

- **QA-04.** The isolated save directory was per PROCESS; every test in one executable shares a pid, so the first guard to finish deleted the folder the next test still pointed at. One directory per test now (pid plus suite and name), and the guard restores the previous path. The seam-failure test had no guard at all and wrote `seam_test.sv` into the player's LIVE Saved_Games folder under a shuffled run; it is isolated. writehero 5 shuffled repeats clean; no leftover folders.
- **QA-02.** `InvTest::SetUp` resized the player vector without reconstructing the element, so belt, gold and grids leaked between cases; a fresh Player per test. The multiplayer gold case inherited the single-player `MaxGold` of 100,000,000 and its pile absorbed everything; it pins the multiplayer cap. inv_test 8 shuffled repeats clean.

The full response to the audit, finding by finding, is filed beside the reports under `06-Reference/External Audits/`. Suite 645/646.
