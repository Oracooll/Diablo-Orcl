# ChatGPT multi-audit handoff: v1.9.122

**Date:** 2026-08-30  
**Audience:** Claude / the next implementation pass  
**Audited version:** ORACOOL_VERSION 1.9.122  
**Audited commit:** 2c3e8d15d0fdc475562024bfc9b2feee9eb8852a  
**Last code commit:** f760a9c91f6bad046435388b5685f2860b565ae8  
**Comparison point:** the prior ChatGPT v1.9.97 audit at 6076b80  

This handoff records a fresh set of independent passes over gameplay, persistence, UI/input, and
build/release behavior. The audit began on v1.9.121; another process committed v1.9.122 during the
work. Every open source finding below was rechecked against final v1.9.122. The final commit adds
only a development report after the v1.9.122 code commit.

No game source was changed by this audit. Only these Markdown reports were added. The pre-existing
untracked .claude directory was not inspected or modified.

## Executive result

The highest-confidence findings are:

| ID | Severity | Finding | Report |
|---|---:|---|---|
| GP-01 | High | The moved Stash Chest and the local new-game player spawn now occupy the same solid town tile, {56,67}. | GAMEPLAY |
| PO-01 | High | SaveOptions deletes the whole Oracool INI section and fails to rewrite six registered options, including the new remembered mouse-button spells. | PERSISTENCE_OPTIONS |
| UI-01 | High | The revived XP bar is incompletely integrated: it overwrites two belt rows in the default HUD, draws over chat, has no tooltip, and allows world clicks through it. | UI_INPUT |
| BR-01 | High | BuildReleasePackage.ps1 constructs its verifier temp filename before assigning runId, restoring a cross-process temp-file race. | BUILD_RELEASE_QA |
| REL-01 | Release blocker | Source and Debug are v1.9.122, but the Release executable and both Release MPQs are still v1.9.114-era artifacts. | BUILD_RELEASE_QA |
| PO-02 | Medium | Balance Telemetry is declared and consumed but omitted from GetEntries and SaveOptions, so it is permanently enabled at its default and cannot be loaded or changed. | PERSISTENCE_OPTIONS |
| UI-02 | Medium | Quick-list F-key binding uses hover state cached by the previous draw; motion or wheel plus F1-F8 in one SDL event batch can bind the old cell. | UI_INPUT |
| UI-03 | Medium | F1-F8 still edit bindings from the Abilities window, contrary to the v1.9.121 requirement that assignment happen from quick lists, not Abilities. | UI_INPUT |
| GP-02 | Medium / decision | Zeal has three conflicting contracts: two different UI descriptions and an implementation that grants every Zeal point to every Paladin melee hit. | GAMEPLAY |
| BR-02 | Medium | The final ZIP verifier collects entry names but checks only the count; a wrong same-sized entry set passes. | BUILD_RELEASE_QA |
| QA-01 | Medium | Serial and parallel test runs are both 577/579. The same dungeon golden and timedemo remain red, leaving no fully green regression signal. | BUILD_RELEASE_QA |
| UI-04 | Decision | The mini-map remains a visible click-through overlay. v1.9.122 explicitly left this for a product decision. | UI_INPUT |

## Verification performed

- Rebuilt the current x64-Debug tree successfully at v1.9.122. The executable's ProductVersion is
  1.9.122.
- Ran the full CTest suite serially: **577/579 passed**.
- Ran the full CTest suite with eight-way parallelism: **577/579 passed**, with no extra
  order-dependent or shared-state failures.
- Re-ran Drlg_l1.CreateL5Dungeon_diablo_3_844660068 alone and twice in one process. It failed the
  same way both times, ruling out parallelism and preceding-test contamination.
- Verified the current Debug archives byte-for-byte with the repository verifier:
  devilutionx.mpq verified 188 entries and oracool.mpq verified 416 entries.
- Inspected the current Debug and Release artifact version resources and timestamps.
- Compared every entry returned by OracoolOptions::GetEntries with every Oracool member referenced by
  SaveOptions. There are 67 registered entries, 62 manually serialized members, and six registered
  entries absent from the serializer. Balance Telemetry is an additional declaration absent from
  both registration and serialization.
- Simulated cross-file/new-mechanic paths rather than reviewing functions in isolation: town spawn
  plus object placement, SDL event draining plus draw-cached hover, XP geometry plus draw order and
  world-input rejection, and option load plus startup canonical rewrite.

## Suggested fix order

1. Fix PO-01 and PO-02 together. The manual canonical serializer is silently deleting player
   choices, and its duplication will keep creating this class of bug.
2. Fix GP-01 before another build is handed to players. It is a deterministic fresh-character
   glitch, not a rare edge case.
3. Integrate the XP bar through one exported rectangle shared by drawing, tooltip, click rejection,
   and geometry tests; restore the chat guard at the same time.
4. Move runId creation above every temporary filename, rebuild Release v1.9.122, and run the
   packaging preflight.
5. Make quick-list hotkey hit-testing event-current and remove or deliberately retain the Abilities
   binding route after resolving the written requirement.
6. Decide the Zeal and mini-map contracts, then pin those decisions in tests and player-facing text.
7. Restore a green test baseline by root-causing the level-3 golden and replacing or regenerating
   the timedemo hero fixture.

## Report set

- CHATGPT_AUDIT_GAMEPLAY_v1.9.122_2026-08-30.md
- CHATGPT_AUDIT_PERSISTENCE_OPTIONS_v1.9.122_2026-08-30.md
- CHATGPT_AUDIT_UI_INPUT_v1.9.122_2026-08-30.md
- CHATGPT_AUDIT_BUILD_RELEASE_QA_v1.9.122_2026-08-30.md

Each detailed report includes evidence, a deterministic reproduction or simulation, recommended
repair shape, and regression-test suggestions.
