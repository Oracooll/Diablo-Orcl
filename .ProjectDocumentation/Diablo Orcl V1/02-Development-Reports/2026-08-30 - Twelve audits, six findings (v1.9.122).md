# Twelve audits, six findings (v1.9.122)

**Date:** 2026-08-30
**Version:** 1.9.122
**Trigger:** user, away for a few hours: "do a bunch of audits and fix what you find."
**Tests:** 577/579. Two new regression tests added; the two standing baseline failures unchanged.

Twelve audits, run in one pass. Six produced findings, four of which are fixed here. Six produced
nothing, and those are recorded too - a clean audit is only useful if the next one can see it was
run.

Two of the six findings were mine, from the two builds immediately before this one. That is the
honest headline: the highest-yield place to audit was the most recent work.

---

## Fixed

### 1. The F-key badge reported a key for things that cannot hold one

*Introduced v1.9.121, yesterday.*

An unbound hotkey slot holds `SpellID::Invalid`. So does every quick-list entry that is not a
spell - the two basic attacks, every aura. `AssignedFKeyNumber` compared the two to each other,
matched on the first empty slot, and returned its number. Both attack icons in the right-button
quick list wore an **"F1" badge that nothing had put there**, and every aura would have worn one
too.

Guarded inside the helper, not at the call sites. There are two call sites now - the Abilities
window and the quick lists - and the next one would have made the same assumption.

Pinned by `OracoolAudit.AnUnbindableEntryReportsNoHotkey`, which was checked against the defect
reintroduced: it fails with the guard removed and passes with it in.

### 2. The crafting window drew two close buttons

*Introduced v1.9.120, yesterday.*

`scrollrt` draws the red X for whichever left-panel content is open, from
`GetLeftPanelContentRect()`. The crafting window IS that rect, so the X I added inside
`DrawCraftingMenu` landed exactly on top of the one already there, and the click test I added was
dead code - `diablo.cpp`'s `LeftMouseDown` consumes that click before the crafting router ever
sees it.

The comment at the central call says it exists so the button "cannot end up in five slightly
different places". I put it in a sixth. Both removed.

### 3. The event log let clicks and hovers through to the world

*Pre-existing, and the largest hole of the set.*

The log fills the entire column under the mini-map - roughly 306 x 640 at the played resolution -
and appeared in **none** of the rejection lists: not `IsPointOverFloatingWindow`, not
`IsPointOverHudChrome`, not `IsOverLeftPanel`. Clicking it walked the character; hovering through
it highlighted whatever stood behind.

It was the least obvious hole precisely because of how it fails. A window that swallows a click it
should have acted on looks broken immediately; a window that lets a click through looks like the
player misclicked.

It now exports `GetEventLogWindowRect()`, built from the same three helpers the draw uses, and
joins `IsPointOverFloatingWindow`.

One thing had to be checked before trusting that fix. The log runs down to within a mini-map's
margin of the screen bottom, and the belt row is at the bottom centre; had the two rects
overlapped, the fix would have traded a stray walk for **dead belt cells**, which is the worse bug
and a silent one. They do not overlap, and
`OracoolAudit.TheEventLogDoesNotCoverTheBeltRow` now pins that - with an assertion that both rects
are non-empty first, so it cannot pass for the wrong reason.

### 4. The log's wheel branch stole the wheel from three gated ones

The branch was `else if (oracool::IsEventLogOpen())` - ungated by cursor position - and it sat
**above** the waypoint list, the character sheet and the spell book, all three of which are gated
on the cursor being over them. So with the log open:

- scrolling over the character sheet scrolled the log;
- scrolling over the spell book scrolled the log;
- the dungeon zoom was dead everywhere on screen.

The file states its own convention in the waypoint branch: gate on the panel, "so the wheel still
zooms the dungeon everywhere else while it is open". The log is the one window a player leaves open
for a whole session, which is what made an ungated branch cost the most here. It is gated on its
rect now - which only became possible because finding 3 gave it a rect.

### 5. A superseded window still fully implemented

The basic-attack quick list (`oracool/attack_skills`) was the two-icon popup from 2026-08-18. The
skill picker superseded it on 2026-08-20 - its own header says so - and nothing has called
`OpenAttackQuickList`, `CloseAttackQuickList`, `IsAttackQuickListOpen`, `DrawAttackQuickList` or
`CheckAttackQuickListClick` since.

It was not merely unused. It kept its own open/close state and its own click router, so a single
stray call would have put a second, unreachable popup on screen over the live one. Deleted, with a
note in the header saying where the gesture went. The rest of the file - `DrawLmbSkillWell`,
`DrawRmbSkillWell`, `BasicAttackIcon`, `AttackIconDisplayOrder` - is live and untouched.

---

## Found, not fixed

### 6. The mini-map has the same hole the event log had

`GetMiniMapScreenRect()` appears in no rejection list either. Clicking the mini-map walks the
character; hovering through it highlights monsters behind it.

**Not fixed, because the trade is real and it is a design call, not a defect call.** The log is a
window you open and close; the mini-map is a permanent overlay across the top-right. Making it
swallow clicks means a monster in that corner becomes unclickable whenever the mini-map is on. The
standing rule ("area of UI screens should in no case permit clicks on the ground") argues for
fixing it; the cost in combat argues for leaving it. That is the user's call.

The same shape exists, less sharply, for the stash and quest-log wheel branches, which are also
ungated and also sit above gated ones. Those are vanilla DevilutionX behaviour rather than
Oracool's own convention being broken, so they were left alone.

---

## Clean audits

Recorded so the next pass does not repeat them.

- **Options never read.** All 52 `Oracool` option entries have a consumer outside `options.*`.
- **Auto-save triggers.** All fourteen `ScheduleAutoSaveFor*` functions have live call sites. The
  `ItemBreak` one has exactly one, inside `BreakOrRemoveEquipment` - which is correct, and is what
  the `WearDurabilityPoint` consolidation preserved rather than dropped.
- **Left-button hotkey persistence.** `_pSplLHotKey`/`_pSplLTHotKey` are packed and unpacked by
  `hero_chunks`. They are value-initialised to `SpellID::Null` rather than filled with `Invalid`
  like the right array, but `IsValidSpell` rejects both, so nothing rides on it.
- **`CloseAllWindows` completeness.** Every window is reachable, most via `ClosePanels()`.
- **IPL token mapping.** Every `IPL_` token used by the set, unique and orb tables is handled by
  `SaveItemPower`; no row claims `Power`/`Approx` while mapping to `IPL_INVALID`, and none marked
  `Inert` maps to a live token.
- **Tier-parameter ranges.** Every `IPL_STEALLIFE`/`IPL_STEALMANA` row across `itemdat.cpp` and the
  three generated `.inc` tables uses param1 3 or 5 - the only two values that do anything.
  `IPL_FASTATTACK` is within 1..4, `IPL_FASTRECOVER` within 1..3.
- **`param2 < param1` rows.** There are 38 of them, and every one is on a power that ignores the
  rolled value (`FASTATTACK`, `FASTRECOVER`, `FASTBLOCK`, `ONEHAND`, `INVCURS` all read `param1`
  directly). This matters because `RndPL` returns **param2** under `ForcePerfectAffixRoll`, so such
  a row on a rolled power would make a perfect roll worse than an ordinary one. None does.
- **`ForcePerfectAffixRoll` leakage.** Saved and restored around its one use, with an existing test.
- **Salvage entry points.** All three - `SalvageAllInBackpack`, `AnySalvageableInBackpack`,
  `TrySalvageOnPickup` - go through `SalvageMatches`, so the socketed-white protection cannot be
  missed by one path again.
- **Hover suppression coverage.** `CheckCursMove` excludes the HUD chrome, the shop, the inventory,
  the stash, the spell book, the left panel and the floating windows. Complete, apart from finding 6.

---

## Still unexplained: `Drlg_l1.CreateL5Dungeon_diablo_3_844660068`

This has been carried as a "standing baseline failure" since at least v1.9.26 (2026-08-22) without
anyone establishing *why*. It deserves better than that label, so here is what was ruled out:

- `Source/levels/drlg_l1.cpp` is **byte-identical to the 1.5.5 baseline** - the generator itself is
  untouched.
- `GetLevelType` moved into the zone registry, but the registry's rows reproduce the old ladder
  exactly, town included. Level 3 is Cathedral in both.
- `randomizeQuests` still defaults to false, so `InitialiseQuestPools` does not run in the test.
- `quests.cpp`'s fork changes touch the log-reveal and the cow-quest choice; none moves a quest's
  level or activation.
- The fixture `test/fixtures/diablo/3-844660068.dun` is present and the same size as its siblings.
- Levels 2 and 4 of the same suite, same seed style, same `TestInitGame`, **pass** - so it is not a
  global init or type-mapping problem. It is specific to level 3.

The failure is a whole-layout divergence (first mismatch at tile 1x0) plus both view positions, so
it is a different dungeon, not a perturbed one. Settling it properly needs a baseline build in a
worktree to confirm whether it ever passed in this repo. That is the next step, and it was not taken
here because it is a build-and-bisect job rather than a read.

`Timedemo.WarriorLevel1to2` needs no such investigation: a timedemo replays recorded input against
changed gameplay, and this fork has changed a great deal of it.

---

## Verification

Full suite after every change: **577/579**, the two above. The two new tests were both checked for
vacuity - the badge test by reintroducing the defect and confirming a red run, the overlap test by
asserting both rects are non-empty before comparing them.

Nothing here is visual, so nothing here needs a screenshot - except one confirmation worth making
in passing: with the log open, the wheel should now zoom the dungeon everywhere except over the log
itself.
