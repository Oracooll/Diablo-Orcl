# The second external audit, answered in full (v1.9.48–1.9.53)

Ten high-priority findings plus a dozen smaller ones, worked in the order the audit recommended.
Every finding was **verified against the code before anything was changed** — the previous external
audit contained a claim that was simply false, so none of these were taken on trust.

**This audit was accurate.** Nine of the ten high-priority findings confirmed outright; the tenth
(#5) confirmed once traced. One detail was imprecise: #10 cites a "320×352 rectangle" in spell
validation, and no such literal exists — but its substance (no authoritative UI rejection in
`RightMouseDown`) was correct and is the more important half.

---

## 1 — Save atomicity, held items, shutdown routes (v1.9.48)

**A failed save destroyed the previous one and reported success.** `MpqWriter::WriteFile` called
`RemoveHashEntry` *first* and wrote afterwards, and both callers discarded the result. Now written
under a temporary name and renamed over the target. Safe to rename because the stored bytes are not
keyed to the filename — blocks never set the encrypted flag, so `WriteFileContents`' vestigial
`Hash(filename, 3)` has no effect. **Checked before relying on it.**

**Exiting while holding an item deleted it.** `HoldItem` is in neither `PlayerPack` nor
`SaveHeroItems` — only `SaveGame`'s world snapshot, which the exit path deliberately does not write.
Now put down first (inventory → belt → stash), and said out loud if all three are full.

**Two ways to leave without saving.** `SaveOnExit` was gated on the Auto Save *preference*, so
turning off an option that reads "save every N minutes" silently meant "never save". And Alt+F4 and
window-close bypassed it entirely. `diablo_quit` saves only on a **clean** exit — `appfat` arrives
there too, and writing a character out of a state the game has just declared broken is how a crash
becomes a corrupt save.

## 2 — Named encounters, end to end (v1.9.49)

The whole D2MXL Phase 4 chain was **dead in normal play**: `NamedEncounterMapItem` had one caller, a
test. Sealed Maps now drop from Dread bosses; arenas open fresh every time (leaving marked them
visited, so map two walked you back to the corpse); the guaranteed charm is guaranteed even on a
full floor; and all three arenas carry their own depth instead of falling through to floor 1, which
had them paying floor-1 loot.

## 3 — Aura lifecycle and knockback (v1.9.50)

**Relentless and Implacable did the reverse of what they said.** `MFLAG_KNOCKBACK` lives in the
monster-hits-*player* path. Both granted an offensive power nobody designed and neither granted the
immunity both advertised. Now a predicate at `M_GetKnockback`, the one place a player pushes a
monster.

**A corpse kept its aura**, healing zero hit points in `PM_DEATH`. One guard in
`GetActiveClassAura` — the single function the tick, the totals, the ring and the audio all ask.

**Putting an aura out left its bonuses on.** `ToggleClassAura`'s one caller recalculated; all four
`ClearClassAuraForRightButton` callers did not. That asymmetry is the argument for putting it inside
the function.

## 4 — One authority for UI hit-testing (v1.9.51)

Three findings, one mistake: comparing against the vanilla **320×352** slot while the window is
**340×720**. Right-click cast through windows, ground labels stayed live under them, and the
controller could not reach the lower stash rows. Fixed with `IsOverAnyInterface`, **composed** from
four predicates that already existed and were each already authoritative — nothing was missing
except somewhere that asked all four.

## 5 — The remainder (v1.9.52–1.9.53)

Integer scaling divided by zero on startup when the saved resolution exceeded the monitor — a crash
before the first frame. Reflection-killed Vampiric and Devouring monsters drank afterwards, ending
in death mode with positive health. Paladin ranged skills spent mana then discarded `AddMissile`'s
null. Touch players could slot a passive and never unslot one. The passive page's instruction was 61
characters in a 246×18 box that clips. Skill and passive changes scheduled no save. The periodic
autosave serialised a world snapshot nothing ever reads. Linux packaging still had the `OPTIONAL`
that was removed on Windows, and the MPQ helper hardcoded the Debug tree.

Finally, `GetActiveListIndex` answered **0** when the stored resolution matched nothing — silently
moving the selection to the shortest entry. It now picks the nearest height, which is what the
player actually chose.

---

## What testing found that reading did not

Three times, the test was the thing that was wrong:

1. **A test that could not fail.** The aura-bonus test asserted on a fresh `ApplyClassTreeToTotals`
   and **passed with the fix removed** — that function recomputes from scratch and can never observe
   a stale cache, which was the entire bug. Rewritten against `player._pIBonusDam`. Found by
   injection, not by review.
2. **A predicate that was too loose.** Treating `_pHitPoints <= 0` as death declared every test
   fixture a corpse. "No health left" and "health never set up" are different states.
3. **An assertion about the wrong thing.** The hero round-trip test failed on `_pStrength`, which is
   derived while `PackPlayer` stores the base. The code was right.

Four fixes were verified by **reintroducing the bug**: the aura class guard, the passive read
validation, the map drop rate, and the aura recalculation.

## What is NOT covered

Said plainly rather than implied:

- The encounter tests call `TrySpawnSealedMap` directly, so they prove the function behaves and
  **not** that the loot hook still calls it.
- `IsOverAnyInterface` reaches into live HUD state and faults in a bare harness — a first version of
  its test proved that by crashing. The test pins the *premise* (those windows outgrow
  `SidePanelSize`); the routing needs play.
- Most of batch 5 is unreachable from the harness: display mode needs SDL, the lifesteal guard and
  the missile pool need a running game.
- The audit asked for **failure-injection tests for disk-full and short-write conditions**. Not
  done. That needs a seam for injecting write failures that does not exist, and it is the right next
  piece of work on this code.

## Verification

543/545, the two standing baseline failures (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`,
`Timedemo.WarriorLevel1to2`). Suite grew 537 → 545.
