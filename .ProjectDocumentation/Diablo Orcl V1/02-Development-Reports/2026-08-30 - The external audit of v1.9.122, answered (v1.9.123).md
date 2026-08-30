# The external audit of v1.9.122, answered (v1.9.123)

**Date:** 2026-08-30
**Version:** 1.9.123
**Trigger:** four ChatGPT audit reports dropped into `chatgpt audits/`, targeting commit `2c3e8d1`.
**Tests:** 580/582. Five new tests; the two standing baseline failures unchanged.

A good audit. Five of its twelve findings were real and are fixed; three are decisions rather than
defects and are left for the user; one report referenced by the index is missing from the folder.

Every finding below was **reproduced here before being fixed** — the audit's arithmetic was not
taken on trust — and every fix is pinned by a test that was checked against the defect.

---

## Fixed

### GP-01 — a new character was created standing inside the Stash Chest

The worst of the set, and it was mine, from yesterday.

The town spawn sits at `{56,67}` *because* the chest was at `{55,67}` — its comment says "next to
the Stash Chest". Play-test item #13 moved the chest one tile toward 4-5 o'clock, which put it on
`{56,67}`: the spawn tile. The chest is solid, so every new character began the game inside it.

Nothing said so. The chest's own placement guard **logs** the collision and then places it anyway,
and the collision it checks is `dObject` — the player is not an object.

The spawn follows the chest to the tile it vacated. That keeps the two adjacent, keeps the original
intent, and lands on floor known to be clear because the chest stood on it until yesterday.
`town.cpp`'s ENTRY_MAIN camera moved with it; those two must always name the same tile.

The town-objects test had a `TownFurnitureDoesNotShareTiles` case that compared furniture only with
furniture. It now also asserts that no object at all occupies the spawn — the version that does not
need updating when a fourth piece of furniture arrives.

### PO-01 — six registered options deleted on every launch

`SaveOptions` skips the Oracool category, **deletes** the whole INI section, and rebuilds it by hand
so it can be grouped and commented. The hand-written list had drifted six entries behind the
registered ones.

That is not "unsaved". `SaveOptions` runs at startup, so those six keys were destroyed on every
launch and reverted to their constructor defaults. Confirmed independently by diffing `GetEntries`
against the members the serializer touches: 66 registered, 61 written, six missing —
`dungeonZoomLevel`, `gameSpeedReadout`, `panelDocking`, `vendorTieredStockChance`, and **both**
`lastReadiedSpell` slots.

So v1.9.118's remembered LMB/RMB could never survive a restart, which was its entire purpose.

Fixed with a **backstop loop**, not six more `setBoolean` lines: the hand-maintained list *is* the
defect — it has to be extended by hand for every new option, and nothing failed when it was not.
Anything missed now lands in the file uncommented instead of being lost. This needed a public
`OptionEntryBase::GetKey()`, the symmetric accessor `OptionCategoryBase` has always had.

The finding had a second half worth stating separately: **nothing called `SaveOptions` when a
readied spell changed**, and nothing calls it on exit. It runs at startup, from the settings
screens, and on a fullscreen toggle. So even with the backstop, the value would only have reached
disk if the player happened to open Settings before quitting. `RememberReadiedSpells` now writes
when the pair actually changes — cheap, because that is a handful of times per session.

`OracoolOptions.EveryRegisteredEntrySurvivesASaveRoundTrip` runs the real `SaveOptions` against a
redirected config path — never the user's own `diablo.ini` — and asserts the resulting file names
every registered entry. With the backstop disabled it names exactly the six above.

### PO-02 — Balance Telemetry could not be loaded, saved, or switched off

Declared, and consumed by `oracool/telemetry.cpp`, but never in `GetEntries` — so it was never
loaded from the INI, never written, and never listed in Settings. Permanently on at its default.

Registering it is the whole fix: loading walks that list, and the PO-01 backstop now writes it.

Worth noting against my own audit the same day: I checked every option for a **reader** and found
none missing. I never checked for **registration**. Two different questions, and only one of them
was asked.

### UI-01A — the XP bar painted over the belt

The bar is 8px. With plate art off — **the default** — the band between the XP counter and the belt
is 6px. An 8px rectangle centred in a 6px gap overhangs by two pixels, and the bar is drawn after
the belt, so it overpainted the belt backing rather than being hidden behind it.

The old comment claimed the clamp "pins the bar to the belt's top edge instead". It could not:
clamping the *top* of a fixed-height rect cannot shorten it, so the bottom simply went past the
belt. The height is now what the gap allows.

The rect is exported. It was file-local, which is how an 8px bar came to be drawn into a 6px gap
with nothing able to notice — the same lesson as the event log yesterday, in the other direction.
`OracoolAudit.TheXpBarFitsBetweenTheCounterAndTheBelt` checks both HUD modes, and fails on the
default one before the fix (676 vs 674, exactly the 2px the audit predicted).

**One thing for the user to look at.** The consequence is that in the default HUD the bar is 6px,
not the 8px that was asked for. The gap genuinely is not big enough. Making it 8px everywhere means
moving the XP counter or the belt, which is HUD geometry that has been iterated on carefully, so I
have not touched it. Say the word and the band can be widened.

### UI-01B — the bar drew over the chat panel

The vanilla bar began with a `talkflag` guard; the revival dropped it, and `DrawTalkPan` runs before
`DrawXPBar`. Restored.

### GP-02 (descriptions half) — Zeal was described three different ways

- `paladin_skills.cpp`: "+1% chance to hit **thereafter**" — implying the bonus starts past the
  strike cap. It does not; every invested point pays.
- `class_tree.cpp`: "each invested **pair** of points adds a strike, up to **five**" — the pre-nerf
  ladder, left untouched by the change that replaced it.
- The code: two strikes at unlock, one more per point to a cap of four, +1% per point throughout.

Both now match the code and each other. The accuracy half had also shipped untested, which is why
neither text was caught; `OracoolAudit.ZealToHitLadder` pins the ladder, the unlock gate, and that a
non-Paladin never receives it.

---

## Left for the user

### GP-02 (scope half) — who does Zeal's accuracy bonus belong to?

`ZealToHitBonus` is added in `PlrHitMonst` for every Paladin melee hit, without asking whether Zeal
is the readied skill. Ordinary swings and the other melee skills get it too.

The request — *"every level of zeal add up to 4 strikes per hit, but also add +1% chance to hit"* —
does not settle whether that bonus belongs to **Zeal** or to the **Paladin**. Narrowing it is a
balance change. If it should be Zeal's alone, the change is small and the test above is where to
pin it.

### UI-03 — should the Abilities window *stop* binding F-keys?

The audit reads *"we are making F1-F8 hotkeys assignable from the quicklists, not from the abilities
windows"* as a requirement to **remove** the Abilities route. It can equally be read as naming where
the new gesture lives. Both windows binding the same keys is coherent today — one key, one skill,
one button is enforced across both. Removing a working route on an ambiguous reading is not an
audit's call.

### UI-04 — the mini-map click-through

Already recorded yesterday as the user's decision, for the same reason: fixing it makes a permanent
overlay swallow combat clicks in the top-right corner.

---

## Deferred, with reasons

### UI-02 — the quick-list hover is one frame stale

True by construction, and documented as such: the picker reads the hovered cell out of its draw, the
way the Abilities window does. Motion and an F-key arriving in one SDL batch can therefore bind the
previously hovered cell.

Not fixed here. The honest fix is to compute the hovered cell on demand, which means extracting the
cell walk that the draw and the click router each already do — a three-way refactor of live click
routing that I cannot verify by screenshot while the user is away. It is rare (it needs both events
in one batch), self-correcting on the next frame, and worth doing deliberately rather than at the
end of a long session.

### REL-01 — Release artifacts are v1.9.114-era

Correct, and deliberately not acted on: the standing instruction is Debug only unless Release is
asked for. Flagged rather than built.

### BR-01, BR-02, QA-01 — the packaging report is missing

`CHATGPT_MULTI_AUDIT_INDEX` lists `CHATGPT_AUDIT_BUILD_RELEASE_QA_v1.9.122_2026-08-30.md` in its
report set, and that file is **not in the folder** — only the three others arrived. The index's
one-line summaries are all there is to go on:

- **BR-01** — `BuildReleasePackage.ps1` builds its verifier temp filename before assigning `runId`,
  restoring a cross-process temp-file race.
- **BR-02** — the final ZIP verifier collects entry names but checks only the count, so a wrong
  same-sized entry set passes.

Both sound plausible and both are cheap to check, but acting on a one-line summary of a report I
cannot read is guessing. If the file turns up, they are quick.

### QA-01 / the level-3 golden

The audit reports the same 577/579 and the same two failures, and confirms independently that
`Drlg_l1.CreateL5Dungeon_diablo_3_844660068` fails alone and twice in one process — ruling out
parallelism and test contamination, which matches my own finding yesterday.

Yesterday's investigation went further and is worth recording here, since the audit lists
root-causing it as step 7:

- Level 3 is the **only** Diablo Cathedral test that actually places a quest set piece. Level 1 has
  no quest; the level-2 test explicitly sets `Q_BUTCHER` to `QUEST_NOTAVAIL`; one of the two level-4
  tests explicitly *enables* `Q_LTBANNER` and **passes**. So the set-piece path itself works.
- Instrumenting `LoadQuestSetPieces` confirmed it: level 2 loads nothing, level 3 loads
  `skngdo.dun` and the layout diverges.
- It is not `diabdat.mpq` being present locally. Moving it aside and re-running gives the identical
  failure (the file was restored immediately).
- Every input was measured and matches vanilla: `gbIsHellfire=0`, `gbIsMultiplayer=0`,
  `setlevel=0`, `leveltype=CATHEDRAL`, King Leoric available, `pOriginalCathedral=1`.
- Every file on the path is byte-identical or behaviourally identical to the 1.5.5 baseline:
  `drlg_l1.cpp` unchanged; `gendung.cpp`'s only change reproduces the old `GetLevelType` ladder
  exactly; `themes.cpp` unchanged; `random.cpp` unchanged; `QuestsData`, `Quest::IsAvailable`,
  `InitQuests`' loop and `DRLG_CheckQuests` all identical; `test/fixtures/**` including
  `skngdo.dun` tracked and unchanged.

With identical code, identical inputs and identical data, the output cannot differ. **The failure is
therefore inherited from the vendored 1.5.5 baseline, not caused by any Oracool change** — most
likely a fixture/source version skew at import time.

Stated with its limit: I could not demonstrate this directly. A baseline worktree configured and
compiled, but the resulting binary would not load, and chasing that is toolchain plumbing rather
than signal. So this is an argument from exhaustive equivalence, not an observation. If a direct
demonstration is wanted, that is the remaining step.

---

## Verification

Full suite after every change: **580/582**. Five tests added across the two audit rounds today, each
checked against its own defect:

| Test | Fails when |
|---|---|
| `AnUnbindableEntryReportsNoHotkey` | the `IsValidSpell` guard is removed |
| `TheEventLogDoesNotCoverTheBeltRow` | asserts both rects non-empty first, so it cannot pass vacuously |
| `EveryRegisteredEntrySurvivesASaveRoundTrip` | the backstop is disabled — names exactly the six |
| `TheXpBarFitsBetweenTheCounterAndTheBelt` | before the height clamp, on the default HUD |
| `ZealToHitLadder` | new coverage; the ladder had none |

Nothing here needs a screenshot except one thing worth a glance: the XP bar is now 6px in the
default HUD rather than 8px, for the reason under UI-01A.
