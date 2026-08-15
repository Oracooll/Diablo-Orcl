---
date: 2026-08-15
version: 1.6.12
area: Self-audit / periphery, closure
---

# Audit Round Nine — Second Clean Round, Audit Closed

The last unswept corners, and nothing needed changing in any of them. Two consecutive clean rounds
is the signal the audit was waiting for: the reading has caught up with the code everywhere it can.

## Audited and found sound

**quests.cpp's quest-log transition tracker** — the exact shape the lifetime checklist flags
(file-scope state mirroring per-game data), and the author already answered the question:
`SyncQuestLogState()` re-seeds it in `InitQuests`, so it resets per game. The gradual-healing leak's
sibling, except this one was built right.

**hud_art.cpp's orb drain arithmetic** — the one division by a player stat is guarded
(`maxValue > 0`), everything else is constant geometry pinned by static_asserts.

**sprite_import.cpp** — every dimension validated before any buffer math (null surface, zero frame
width, modulo checks against frame columns and facing rows, palette presence), and the sheet
combiner's offsets are computed from the same sizes it copies. Menu-only besides.

**capture.cpp** — pitch-aware row copies, error paths logged, and the zero-alpha palette trap
(libpng emitting a tRNS chunk that renders the whole screenshot blank) already found and documented
in place.

**The debug set-spawn commands** — previously hardened (the level clamp from the invalid-packet
episode, the stat-overflow verification at task #44); the table entries check out.

**display.cpp / dx.cpp** — carry no fork changes at all; their appearance in the working-tree
snapshot was history from v1.5.44, long committed and play-tested through the front-end era.
minitext.cpp and gmenu.cpp's small touches likewise.

## The audit, closed

Nine rounds, **twenty-one fixes**, 1.6.5 through 1.6.12:

| Rounds | Ground covered | Fixes |
|---|---|---|
| 1-5 | Lesser uniques, Paladin skills, input, timers, session state | 10 |
| 6 | Stash + tabbed inventory (load hardening) | 2 |
| 7 | Store paths (stale rows, gold overflow) | 9 |
| 8-9 | Abilities window, XP, objects, sync, periphery | 0 |

The two lessons that earned their place in memory:

1. **Scope and lifetime**: where does it live, is it saved, does it hold still - and for anything
   read from disk, does the file's shape prove its references.
2. **The fix that adds a caller must re-check the callee's assumptions** - both same-day regressions
   came from that one move.

From here, bugs come from play, not from reading. The systems that most want play-minutes: the
shift-cast paths, Zeal's burst rhythm, lesser-unique names across save/load and stairs, and every
store screen after selling or buying out a list.

## State

**354/356** - `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, red
before any of this work began. No code changed this round; the build stands at v1.6.12.
