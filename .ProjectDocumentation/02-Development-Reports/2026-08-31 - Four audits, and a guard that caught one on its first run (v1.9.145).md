# Four audits, and a guard that caught one on its first run (v1.9.145)

**Date:** 2026-08-31
**Version:** 1.9.145
**Tests:** 595/596 (the standing `Drlg_l1.CreateL5Dungeon_diablo_3_844660068`); the suite gained a test

Four audits run while the user was away, each a different angle. Three found something.

---

## Audit 1 — dead exports in `Source/oracool`

Every function declared in an `oracool/*.h` was counted across all `.cpp` in `Source` and `Test`.
**Twenty-two are defined and never called from anywhere.**

The first pass over this was wrong and worth recording: it excluded each module's own `.cpp`, which
flagged `SocketRequirementReductionPercent` (Hel's requirement cut) as dead. It is not — it is
reached through `EffectiveRequirement`, which both the item panel and the equip check call. Narrowing
the count to "appears once in all of `Source` and `Test`, i.e. only its own definition" is what turned
a list of 40 guesses into a list of 22 facts.

The one with real consequences:

### `ui/waypoint_panel.png` ships, loads, quantizes, and is never drawn

407 KB, packed into `oracool.mpq`, loaded by `hud_art.cpp`, re-quantized on every palette change —
and `DrawWaypointPanelArt` has no caller. Git says why: the waypoint list was wired to it at
**v1.1.42**, then v1.1.43 ("uniform background") switched the menu to the shared
`HasSidePanelArt()/DrawSidePanelArt()` path. The dedicated art has been orphaned ever since.

Left in place deliberately — using it again is a visible design decision, not a cleanup. **The
choice is: wire it back into the waypoint menu, or delete the asset and its two functions.** Right
now it is paying rent in the archive, in memory and in every palette swap for nothing.

The other 21 are mostly layout and predicate helpers exported when they could have been file-static
(`EquipRect`, `GetTabCellOrigin`, `TabColumnX`, `GetLmbSkillIconOrigin`…). Two are worth a second
look when their features are next touched: `zone_registry::GetZone`/`GetZoneCount` (a registry
nothing reads) and `spell_ranks::CanLearnSpell`.

---

## Audit 2 — the empty-collection guard, and what it caught immediately

Yesterday's audit found three wiki readers that had been finding nothing and saying nothing. Rather
than fix three and hope, `BuildWiki.ps1` now **names every collection that comes back empty**, right
before `data.js` is written. Not fatal — an empty backlog is legal, and `pipeline` is listed as
allowed-empty with a reason — but never silent again.

**Its first run found a fourth:**

```
WARNING: PARSED NOTHING: encounters - a reader found no rows. Its page will publish blank.
```

`EncounterPlace` gained a `floor` field at **v1.9.49** (2026-08-26) — the audit fix that stopped the
three arenas falling through `CurrentAreaLevel`'s "floor = 1" default. The wiki's regex still
expected the name straight after the monster type, so it matched nothing, `$encPlaces` was empty, and
the loop that fills the table `break`s on the first iteration when it is. **The named-encounters
table on the Core mechanics page has published blank for five days.**

Fixed, and the floor is now captured rather than skipped — it is precisely what a reader of that
table wants to know — so the page gained a **Counts as floor** column. Verified in the browser:

| Encounter | Where | Counts as floor | Opened by | Reward |
|---|---|---|---|---|
| The Sunken Chapel | Cathedral | 4 | Map of the Sunken Chapel | Chapel Reliquary |
| The Ring of Mourning | Catacombs | 8 | Map of the Ring of Mourning | Mourning Token |
| The Ember Vault | Hell | 16 | Map of the Ember Vault | Ember Seal |

The other parsed counts were checked against their sources rather than assumed: 273 skills matches
`ClassTreeSkillCount`, 143 uniques and 94 set items match what the art folder shipped, 59 spells is
`MAX_SPELLS`.

---

## Audit 3 — the statics sweep, continued

Yesterday covered the windows. This pass walked **every** file-local static in `Source/oracool` and
asked the same question of each: does it hold per-game state, and does anything clear it?

Most were already handled, and that is worth recording as much as the misses —
`signets.cpp`'s `ClaimedMask`/`ConsumedCount` (per-character, reset and packed since the 2026-08-26
audit), `skill_sounds`' completion baseline, Zeal's chain, gradual healing.

One gap left: **`furious_charge.cpp`**. `DashActive` and `CooldownActive` are keyed to
`SDL_GetTicks` and nothing resets them, so quitting mid-charge — or inside the three seconds after
one — hands the next character in the same session a dash already running, or a Furious Charge that
reports itself on cooldown with the icon half-filled before they have swung at anything.

Both self-expire on their own timers, so the leak is bounded by `CooldownDurationMs` rather than
permanent. Hardening in the spirit of `ResetZealChain`, not a reported fault: two assignments to make
the window zero instead of three seconds.

The start times are deliberately **not** zeroed — both readers gate on the Active flag first, so a
stale timestamp behind a cleared flag is unreachable, and zeroing it would make the next
`SDL_GetTicks()` subtraction read as an enormous elapsed time to anyone in a debugger.

---

## Audit 4 — the unique-affix table, which claimed a test it did not have

`unique_affixes.cpp` says of its 45-row table: *"ALPHABETICAL, and the test enforces it."*

**There was no such test.** The table had no coverage of any kind — on the one file the project's own
standing rule exists because of ("never trust a package's IPL column; audit every delivered stat
token against `SaveItemPower`" — three wrong mappings in three days). A comment claiming a guard is
worse than no comment: it is the reason nobody went looking.

`OracoolAudit.TheUniqueAffixTableHoldsItsOwnRules` now pins:

- alphabetical order, checked against the previous row so the message names the pair;
- no duplicate tokens (`FindUniqueAffix` returns the first, so a second would be dead);
- an **Inert** row must carry `IPL_INVALID` and a note — an inert row naming a power is the dangerous
  shape, because it reads as implemented;
- an **Approx** row must name a power *and* say what was traded away;
- a **Power** row must name a power;
- `FindUniqueAffix` resolves each token to its own row, and an unknown token to `nullptr`.

It also gives **`IsUniqueAffixLive` its first caller**. It was written as the diagnostic for "does
the wearer actually feel this", exported — and then called by nothing, neither game nor test. It is
now asserted to agree with the fidelity column on all 45 rows.

The table itself was found **clean**: no ordering breaks, no duplicates, no inert row naming a power.
The guard was checked by breaking the table on purpose (`all_resist` → `zzz_all_resist`) and
confirming it goes red with *"the table is not alphabetical at row 2"*, then reverting.

---

## Open, for a decision

1. **`ui/waypoint_panel.png`** — wire it back into the waypoint menu, or delete it with
   `DrawWaypointPanelArt`/`HasWaypointPanelArt`?
2. **The other 20 dead exports** — mostly layout helpers that want to be file-static. Cheap to sweep,
   but it touches twenty files for no behaviour change; worth doing only alongside other work in
   those files.
3. Still outstanding from yesterday: `tools\RenameProjectFolder.ps1` needs a run from outside the
   folder, and `build\x64-Release` is 1.82 GB of stale tree.
