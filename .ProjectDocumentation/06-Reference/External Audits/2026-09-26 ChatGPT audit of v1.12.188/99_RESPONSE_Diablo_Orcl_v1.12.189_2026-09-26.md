# Response to the ChatGPT audit of v1.12.188 (fixes in v1.12.189)

Date: 2026-09-26. The brief and the seven reports are filed beside this response. The originals, with the evidence folder, are in `Resources\ChatGPT Code Audits`.

Every finding was checked against the source before anything changed. The reports' file and line references and their mechanisms were accurate in all ten findings.

| ID | Audit priority | Verified | Outcome |
|---|---|---|---|
| ITEM-01 stash sort loses items when the new layout cannot fit | P1 | Confirmed. The sort cleared the stash and ignored failed placements. The fix claimed at v1.11.102 had never been made. | Fixed v1.12.189. `SortStash` keeps the stash as it was and counts units (one per item, a stack's count for a stack). If the sorted stash holds fewer, it puts the original back and returns false. The Sort button then says "The stash is too full to sort". The autosave is scheduled only after a sort that succeeded. |
| SAVE-01 workshop bench and craft grid are neither saved nor reset | P1 | Confirmed. The Levski Cube got this protection on 2026-08-30. The workshop, added later, never did. | Fixed v1.12.189. The exit path closes the workshop before the exit save, as it does the Cube. Autosave waits while the workshop is open. `FreeGame` resets it. The bench now falls back to the stash when the pack is full, as the craft grid already did. |
| SKL-01 Essence-priced skills check and pay mana | P2 | Confirmed. `CheckSpell` and `ConsumeSpell` sent only Rage users through the payment facade. | Fixed v1.12.189. `SkillPaysThroughFacade` also sends the 17 Essence rows through `CanPaySkill`/`SettleSkill`. Mana stays as it was for every other skill, and the price is settled once, in `ConsumeSpell`. |
| SKL-02 a deleted monster slot keeps its chill and freeze | P2 | Confirmed | Fixed v1.12.189. The new `ClearColdStateForMonster` clears both timers in `DeleteMonster` and `InitMonster`. The level-wide reset stays. |
| ITEM-02 the Mystic rebuild turns a 3-9 damage range into 3-3 | P2 | Confirmed. The record holds the roll and the row's price multiplier, not the row. Several rows can share a type. | Fixed v1.12.189. The rebuild finds the record's row: its range holds the roll and its multiplier matches. Nine powers that write two separate fields (the four elemental ones, Fireball, set damage, the two armour-and-life/mana powers, and Spell) replay that row's pair. Every other power keeps the single pinned value. The record format is unchanged. The audit's own probe used a synthetic record (param2 = 9) that no drop produces, so the regression uses real rows recorded as a drop records them. |
| WORLD-01 a closed rift leaves a town portal pointing into it | P2 | Confirmed | Fixed v1.12.189. `EndRiftAndItsPortals` closes every open portal whose set level is a rift level, then ends the rift. It runs when the monument closes and when the hero comes home through the way home. Portals into ordinary floors are untouched. |
| WORLD-02 guardian identity is not scoped to the rift level | P2 (likely) | Confirmed by inspection | Fixed v1.12.189. `IsRiftGuardian` requires `InRift()`. A rift level left and re-entered keeps its monsters, so the real guardian is still recognised. |
| UI-01 Escape closes the workshop only through the monument menu's branch | P3 | Confirmed. The call was even mis-indented inside the other branch. | Fixed v1.12.189. The workshop has its own Escape branch, which spends the key even when the close refuses. |
| UI-02 the workshop is missing from the shared click rejection | P2 | Confirmed | Fixed v1.12.189. `IsPointOverFloatingWindow` asks `IsPointOverWorkshop`, which covers the page and its tabs. |
| QA-01 green helper tests miss the production boundaries | P3 | Agreed | The audit's probes are now `OracoolAuditV188.*` in `oracool_audit_test`: eight tests, one per finding above except SAVE-01, plus both Essence directions in one. The inconclusive bench fixture was not adopted. SAVE-01 is covered by its wiring, which mirrors the Cube's. A full session fixture (save, exit, reload) is still open. |

The seven tests taken from the audit's probes failed on v1.12.188. All eight pass on v1.12.189, and the full suite passes 847 of 847. The guardian test only covers the case with no guardian spawned. The stash test takes about 35 seconds in Debug.

Still open, as the audit itself listed:
- the session-level beta matrix: save and reload with staged items, a full rift traversal, and every window at every resolution;
- a whole-session save/exit/reload fixture.
