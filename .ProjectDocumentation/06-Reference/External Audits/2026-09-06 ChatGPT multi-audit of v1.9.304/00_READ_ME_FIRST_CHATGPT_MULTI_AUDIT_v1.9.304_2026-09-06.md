# Diablo Orcl v1.9.304 - multi-audit index and fix order

Date: 2026-09-06 (Europe/Sofia)  
Runtime audited: `C:\Diablo Orcl\x64-Debug`  
Source audited: `C:\Users\hroga\OneDrive\2. Personal Files\Software\Diablo\Diablo Orcl V1`  
Source HEAD: `c659976d69906489c34c28c636b477f14f424bb6` (`oracool-v1-main`)  
Executable: `DiabloOrcl.exe`, file/product version `1.9.304`  
Executable SHA-256: `EAFDBE229DAC70DA3D6E596F558D7A056B8FC29F11C9CA2648251104CF0DF23A`

## Outcome

This pass found two high-priority, normally reachable gameplay defects, one normally reachable lifecycle defect, two persistence-hardening gaps, one dormant multiplayer memory-corruption defect, and several test-harness isolation failures. The most urgent issue is deterministic item duplication when a stack merge is followed by a placement failure or extra-tab fallback. The broadest issue is that effective skill rank is forced to zero for every spell ID from 64 through 125, covering most recently added class mechanics.

No game source, build output, asset, save, or MPQ was modified by this audit. Only these Markdown reports were created.

## Fix first

| Order | ID | Priority | Confidence | Finding |
|---:|---|---|---|---|
| 1 | INV-01 | P1 | Confirmed | Stack placement mutates partial destinations before success is known; failure and extra-tab fallback duplicate units. |
| 2 | SKL-01 | P1 | Confirmed | `Player::GetSpellLevel` rejects IDs 64-125 because it bounds the lookup by the old 64-byte book-level array. |
| 3 | WCR-01 | P2 | High | Warcry debuffs are keyed only by monster array slot and survive deletion; a newly spawned monster can inherit the prior occupant's debuff or conversion expiry. |
| 4 | SAV-01 | P2 | Confirmed hardening gap | Loading trusts valid socket IDs beyond `_iSocketCount`; all six entries continue to grant effects and can be extracted. |
| 5 | NET-01 | P3 now / release blocker before multiplayer | Confirmed | Network packing copies 126 bytes to/from a 64-byte player field, reading and writing across adjacent player state. Multiplayer is currently disabled. |
| 6 | SAV-02 | P3 | Confirmed hardening gap | `GetSpellBitmask` shifts by a negative count for Null/Invalid spell IDs; a malformed readied-scroll/charges save can reach it. |
| 7 | RW-01 | P3 | Contract drift | Runeword activation does not enforce its documented plain-normal, non-tiered host invariant. Normal generation mostly protects it today. |
| 8 | UI-01 | P3 | Timing-dependent, static | F-key assignment reads hover state from the previous draw, so a fast move-and-press can bind the prior skill. |
| 9 | DROP-01 | Decision needed | Confirmed path split | Monster loot gets MF/GF, sockets, ethereal rolls, and logging; chests, racks, corpses, and Find Item bypass that finalization tail. |

Priority meaning used here: P1 = fix before the next public build; P2 = important correctness issue; P3 = hardening, dormant, or lower-frequency issue. `Decision needed` means the behavior is real but product intent is not established.

## Test and package status

- Full CTest discovery: 636 cases; 634 passed, 1 failed, 1 intentionally skipped.
- The failure is `Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, the already-known inherited golden-map mismatch. It is not evidence of a new Oracool regression.
- The skipped case is `Timedemo.WarriorLevel1to2`, intentionally obsolete.
- Seventy-three focused tests covering gems, runewords, skills, class trees, cold mechanics, arrows, melee skills, warcries, stacks, tabs, and stash all passed.
- `oracool.mpq` verified all 409 manifest entries against its source tree.
- `devilutionx.mpq` verified all 188 manifest entries against its source tree.
- Runtime diagnostic exports contained 142 well-formed skill rows and 370 well-formed, uniquely named runeword rows.
- The live save directory contained no zero-length saves and no `.tmp`, `.partial`, `.previous`, or `.bak` residue at inspection time.

Passing tests do not contradict INV-01 or SKL-01. Current stack tests leave room for the remainder, and current class tests often assert the investment ledger/UI rather than the effective rank returned to mechanics.

## Report map

1. `01_INVENTORY_STASH_ATOMICITY_AUDIT_v1.9.304_2026-09-06.md` - duplication traces, affected callers, fix shape, regression matrix.
2. `02_SKILLS_COMBAT_UI_AUDIT_v1.9.304_2026-09-06.md` - rank cutoff, dormant network overwrite, lifecycle/UI boundary observations.
3. `03_SAVE_SOCKET_RUNEWORD_INTEGRITY_AUDIT_v1.9.304_2026-09-06.md` - socket normalization, invalid spell masks, save bounds, runeword contract.
4. `04_MONSTER_LIFECYCLE_LOOT_AUDIT_v1.9.304_2026-09-06.md` - monster-slot reuse and split loot finalization.
5. `05_RUNTIME_BUILD_ASSET_AUDIT_v1.9.304_2026-09-06.md` - binary identity, test baseline, archive and generated-data validation.
6. `06_TEST_HARNESS_STRESS_AUDIT_v1.9.304_2026-09-06.md` - order-dependent crashes and state leakage hidden by per-test CTest processes.

## Claude implementation order and acceptance gates

### Patch A - transactional stack placement

Make belt, inventory/extra-tab, and stash placement all-or-nothing. A failed call must leave every destination byte-for-byte unchanged. When placement succeeds after merging, place only the remainder, never the original item. Add conservation assertions/tests: `sum(after) - sum(before) == incoming`, and on failure `after == before`.

### Patch B - separate book levels from effective skill levels

Keep `_pSplLvl[64]` only as the legacy book-level store if save compatibility requires it. Bound that one access by its actual size, but bound `_pSkillInvestment` by `MAX_SPELLS`. Test IDs 63, 64, and 125 explicitly and then iterate every active class-tree spell row.

### Patch C - clear sidecar state when reusing monster slots

Expose a reset function for the warcry sidecar and invoke it whenever a monster slot is initialized or deleted. A generation/token scheme is even safer if other sidecars follow this pattern. Test: debuff/convert monster A, delete A, initialize B in the same slot, tick past A's old timer, and assert B remains untouched.

### Patch D - persistence normalization and network bounds

Normalize socket entries at and beyond `_iSocketCount` to `EmptySocket`, and make consumers iterate only the declared bounded count. Guard `GetSpellBitmask` for numeric IDs outside 1..`MAX_SPELLS - 1`. Replace the network `MAX_SPELLS` copy with the true source extent or deliberately widen/migrate the source representation; add compile-time size checks and adjacent-field canaries.

### Patch E - fix the test process, then add the new tests

Reconstruct global players in each fixture, restore every changed global, make archive/audio/cursor lifetime explicit, and make isolated save paths unique per test or restore/recreate them after each guard. Add one CI lane that runs each test executable as a whole process with shuffle/repeat, in addition to per-case CTest discovery.

## Audit limitations

The executable exposes useful noninteractive diagnostics but not a scripted full game session. Gameplay findings were therefore established through code-path tracing, boundary-state simulation, package verification, and the shipped tests. WCR-01 and UI-01 should receive a small executable regression test during the fix; DROP-01 requires a product decision before changing behavior.
