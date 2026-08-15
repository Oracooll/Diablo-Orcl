---
date: 2026-08-15
version: 1.6.12
area: Self-audit / Abilities window, XP, objects, sync layer
---

# Audit Round Eight — The Clean Round

Continuation of the standing audit. After round seven's nine store fixes, this round took the
remaining systems with the same shape of risk — and found them already sound. One stale comment was
the only edit. A clean round is a result too: it says where the codebase's discipline actually lives,
and it marks the audit's yield curve hitting the floor.

## Audited and found sound

**The Abilities window** (panels/spell_book.cpp, ~1,500 lines) — the prime suspect, because it is the
fork's biggest UI rewrite and full of the same click-to-row math the stores had. It turned out to
have exactly the discipline the stores lacked, on every axis:

| Store bug family | Abilities window equivalent |
|---|---|
| Stale selection restored over a rebuilt list | Rows rebuilt from scratch on every click AND every draw — there is no held selection to go stale |
| Unclamped scroll offset feeding index math | `UpdateScrollBounds()` clamps per-sheet offsets before the click math runs, same call the draw path makes |
| Row index derived past the list | Mixed-height sheet walks the same enumeration the draw uses; uniform sheets bound `rowIndex >= rowCount` |
| Sparse-array scan overrunning | No sparse scans — builders write dense arrays sized by `MaxSkillSheetRows`, bounded by construction |

**`AddPlrExperience`** — the fork's three hooks (XP gain indicator, autosave-on-gain, counter) are
all correctly placed after the clamps and gated on actual gain; the level-up loop and max-level clamp
are vanilla-intact; the XP counter's remaining-XP mirror of the level-difference formula matches.

**objects.cpp's fork additions** — `OperateWaypoint` bounds `_oVar1` through `IsWaypointUnlocked`'s
own range check; the Stash Chest's no-sync design is documented by its own postmortem (the loopback
double-operate crash) and remains correct; `AddStashChestObject` guards its tile against collision
and logs rather than clobbering.

**msg.cpp / sync.cpp under the 13-slot growth** — `OnChangePlayerItems`/`OnDeletePlayerItems` bound
`bLoc` against the widened `NUM_INVLOC`; the belt handler bounds against `MaxBeltItems` and writes
only for remote players, which single-player loopback never produces; sync.cpp carries both an assert
and a runtime skip at the same bound.

## The one edit

`OperateWaypoint`'s comment still described `_oVar1` as "1-16 = that dungeon level" — stale since the
25-waypoint extension at v1.5.0 gave Hellfire's Nest and Crypt levels 17-24 their sigils. Comment
now matches the code, which was always right.

## Standing after eight rounds

**Twenty-one fixes** across the audit (1.6.5 → 1.6.12), two of them same-day regressions of my own.
The unswept remainder is now genuinely thin: draw-only geometry that self-evidences on screen,
data-only tables that gate nothing, the debug commands, and vanilla systems the fork never touched.
Further rounds should be driven by play reports rather than by reading — the reading has caught up
with the code.

## State

**354/356** — `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, red
before any of this work began.
