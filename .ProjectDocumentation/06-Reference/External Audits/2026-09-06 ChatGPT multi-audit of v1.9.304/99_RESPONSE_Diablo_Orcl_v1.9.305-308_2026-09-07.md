# Response to the ChatGPT multi-audit of v1.9.304 (fixes in v1.9.305 - v1.9.308)

Date: 2026-09-07. The seven reports arrived in `Oracool.MPQ\ChatGPT Audits` and were filed here. Every finding was verified against the source before anything changed; the reports' line numbers and mechanisms were accurate throughout.

| ID | Audit priority | Verified | Outcome |
|---|---|---|---|
| INV-01 stack merges committed before placement succeeds | P1 | Confirmed, both faults | Fixed v1.9.307: belt, backpack and stash placement are all-or-nothing (probe, then commit); the extra-tab fallback gets the remainder. Four regression tests. |
| SKL-01 `GetSpellLevel` rejects ids 64-125 | P1 | Confirmed | Fixed v1.9.305: bounded by MAX_SPELLS, book level added only where the 64-wide store reaches. Test covers 63/64/125, +skill items, and every active tree row. |
| WCR-01 warcry state survives monster slot reuse | P2 | Confirmed | Fixed v1.9.306: cleared in InitMonster and DeleteMonster. Test through DeleteMonsterList with a control slot. |
| SAV-01 socket entries beyond the count stay active | P2 | Confirmed | Fixed v1.9.306: `Item::normalizeSockets()` after load. Test. |
| NET-01 net pack copies 126 bytes through a 64-byte field | P3 (dormant) | Confirmed | Fixed v1.9.305: 64 copied, packet tail zeroed, layout unchanged. Canary test. |
| SAV-02 negative shift in `GetSpellBitmask` | P3 | Confirmed | Fixed v1.9.305: empty mask outside 1..127. Test. |
| SKL-02 per-tick clamp could index past the book store | latent | Confirmed | Fixed v1.9.305: guarded. |
| RW-01 runeword host quality not enforced | P3 | Confirmed | Fixed v1.9.306: plain normal, untiered hosts only; base tier stays eligible. Test. |
| QA-02 inv_test reuses the Player and globals | harness | Confirmed | Fixed v1.9.308: fresh Player per test; the multiplayer gold case pins MaxGold. 8 shuffled repeats clean. |
| QA-04 writehero deletes the shared per-process save dir | harness | Confirmed | Fixed v1.9.308: one directory per test, previous path restored; the seam test that wrote into the LIVE Saved_Games folder is isolated. 5 shuffled repeats clean. |
| WCR-02 Redemption cadence phase | P4 | Confirmed | Not changed: the phase of the first pulse is the whole effect. |
| SAV-03 full-save loader trusts class and inventory count | P3 | Not examined in depth | Open. The compact hero/net unpackers have bounds tests; the full loader does not mirror them. Worth a pass. |
| UI-01 F-key binds the previous draw's hover | P3 | Plausible from the code's own comment | Open: needs the shared hit-test the report describes, and a screenshot-free way to judge it. |
| DROP-01 chests, racks, corpses and Find Item skip the drop tail | decision | Confirmed path split | Decided and fixed v1.9.309: the user chose "all fresh drops get the drop tail"; `FinalizeFreshDrop` is the one funnel, from the monster path and SetupBaseItem. |
| QA-01 oracool_audit_test archive/audio/cursor lifetime | harness | Not reproduced here | Open: needs a suite-level RAII owner for archives, audio and cursor; larger change. |
| QA-03 player_test leaks animation and skill state | harness | Not reproduced here | Open: same shape as QA-02, in a file this pass did not touch. |

Not adopted: the report's suggested CI lane for whole-binary shuffle/repeat runs is sound and cheap; it is a build-system change the user should choose to add.

Every fix built in `C:\Diablo Orcl\x64-Debug`; the suite went from 636 to 646 cases, 645 passing, the standing `Drlg_l1` fixture mismatch the only failure throughout.
