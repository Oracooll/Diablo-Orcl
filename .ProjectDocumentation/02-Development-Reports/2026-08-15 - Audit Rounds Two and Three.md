---
date: 2026-08-15
version: 1.6.7
area: Self-audit, continued
---

# Audit Rounds Two and Three

Continuation of the standing audit ("keep auditing until i stop you"). Round two concentrated on the
shift-cast path added at 1.6.3 — the newest input code — and the dead-mode click routing; round three
swept the timer-driven systems (autosave, event log), the palette machinery, the waypoint save
round-trip, and the density options. Five more fixes shipped across 1.6.6 and 1.6.7.

## Fixed in 1.6.6

**Shift+Charge bought a stationary swing at full price.** The shift branch started the dash and spent
the 10 mana while sending `CMD_SATTACKXY` — a swing *in place*. The dash is a walk-speed boost; there
was no walk to boost. Shift means "act without moving", and for a skill whose whole identity is
movement, that leaves the swing — so the swing, unpaid, is what shift now gets.

**Shift+LMB over an item quietly stopped casting.** The interaction-wins rule gated the cast
unconditionally, but interaction wins on a *plain* click — shift's meaning is "cast, no matter what",
and vanilla's own shift-click already ignores items to swing in place. The gate is now
`isShiftHeld || !IsInteractableUnderCursor()`.

**Dying to a lesser unique blamed the borrowed champion.** The death-source note passed
`monster.name()`. Third caller collapsed onto `GetMonsterDisplayName` — the health bar, the kill log
and the death screen now cannot disagree about who a champion is.

**A corpse could open the inventory.** The dead-mode branch routed clicks to the *entire* HUD menu
row, so a dead player could toggle the inventory or Abilities window — which then drew but could not
be clicked, since `LeftMouseDown` returns before in-window handling while dead. Vanilla dead mode
allowed Game Menu and chat only; the row now dispatches only Game Menu while dead.

## Fixed in 1.6.7

**The Lesser Unique Density help text lied by a day.** "100 is one" stopped being true at 1.6.1, when
the base became 2–6 packs by difficulty. Now reads "Multiplies the champion packs a dungeon level
hosts. 100 is the base 2-6 by difficulty."

## Audited and found sound

| System | What was checked | Verdict |
|---|---|---|
| Waypoint persistence | Pack/unpack round-trip, all 4 difficulties, 24-bit mask, `static_assert` on width | Sound |
| Autosave | `SaveGame()` → `NotifyGameSaved()` resets the interval clock (loadsave.cpp:3067); `SaveOnExit`'s deliberate bypass of `IsSafeToSave` is correct and documented | Sound |
| Event log | Bounded ring buffer (200), scroll reset on open, geometry tied to one rect source | Sound |
| Cursor tooltip | Clamped with an explicit hi<lo guard before `std::clamp` | Sound |
| Divine TRN | Rebuilds in place on palette change; missiles hold a pointer to static storage, so no dangling; index 0 self-mapped | Sound |
| Missile TRN draw path | `Monsters[missile._misource]` is unreachable for player missiles — `_miUniqTrans` is only ever set when `micaster == TARGET_PLAYERS` | Sound |
| Thunderous trap-source | `-1` source: `IsTrap()` skips the unique-TRN lookup, `AddNovaBall` has its own `_misource < 0` branch | Sound |
| Scatter clamp | Density multiplier applies before the `MaxMonsters - 10` clamp; a negative remainder just skips the loop | Sound |
| Town casting | All skills castable in town by design; town damage suppression makes them cosmetic; matches vanilla's own mana-for-nothing behaviour on townsfolk | Sound, consistent |
| `CalcPlrInv` mask rebuild | Runs during unpack after `_pLevel`/`_pClass` are set; readied-spell validation still runs after it | Sound |

## The running tally

Across the whole audit (1.6.5 → 1.6.7): **nine fixes**, of which two were regressions introduced by
my own fixes earlier the same day. The pattern from the 1.6.5 report still holds — every real bug was
an assumption about scope or lifetime that quietly stopped being true — and round two added its
corollary: **the fix that adds a caller must re-check the callee's assumptions**, because that is
exactly how both regressions happened.

## State

**354/356** — `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, red before
any of this work began.
