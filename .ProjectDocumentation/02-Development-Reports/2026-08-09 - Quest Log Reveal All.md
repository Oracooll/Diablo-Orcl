---
title: 2026-08-09 - Quest Log Reveal All
date: 2026-08-09
tags: [dev-report, gameplay]
summary: Every quest available in a session now appears in the quest log from the start, without touching any of the underlying quest-trigger mechanics — completing the "Autosave-only play" specification's last deferred item.
---

# Quest Log Reveal All

## Context

The last deferred piece of [[2026-08-09 - Autosave-Only Play, Part 1]]'s 5-point specification: "all quests should be unlocked in the questlog" from the start of a session, not progressively revealed as vanilla does. The earlier report flagged this as needing a real investigation before touching it, since `_qlog` (quest-log visibility) looked tightly coupled with NPC dialogue gating. Picked up per the project owner's instruction, referencing the Belzebub mod (source unavailable) as a mod known to have handled this well as a guide for the *goal*, not a source to copy from directly.

## Investigation

Traced exactly what `_qactive`/`_qlog` gate throughout the codebase before writing any code, since the earlier report's caution turned out to be partly right and partly wrong:

- **`Quest::IsAvailable()`** (`quests.cpp:984`) — gates whether a quest's dungeon content (monsters, objects) spawns. Checks `currlevel == _qlevel` only — completely independent of `_qactive`. Forcing `_qactive` early wouldn't have affected this either way.
- **`DrawQuestLog()`** (`quests.cpp:862`, pre-existing) — requires `_qactive == QUEST_ACTIVE && _qlog` for a quest to appear in the log's list. This is the actual gate that needed to change.
- **NPC dialogue gating** (`stores.cpp:1644` etc.) — also requires `_qactive == QUEST_ACTIVE && _qlog`. Quest completion/progression logic (e.g. `TalkToDeadguy` in `towners.cpp`) generally re-derives its own state from `_qvar1`/`_qvar2`, not from what `_qactive` was before it runs, and idempotently re-sets `_qactive = QUEST_ACTIVE` when the real trigger fires regardless of prior state.
- **The actual danger, found in `ResyncQuests()`** (`quests.cpp:693-697`): the Mushroom quest's tome item spawn is explicitly gated on `_qactive == QUEST_INIT && _qvar1 == QS_INIT`. Forcing `_qactive` to `QUEST_ACTIVE` before the player reaches that level would make this check permanently false — the quest item would never spawn, and the quest would become uncompletable. Several other quests have similar `_qactive == QUEST_INIT` preconditions gating their first mechanical step (`quests.cpp:599-638`, `towners.cpp:311/334/385/406/502/546`).

This confirmed the original caution was justified for a "force `_qactive` early" approach, but also revealed the actual fix: **don't touch `_qactive`/`_qlog` at all** — only widen what `StartQuestlog()` (the function that builds the log's display list) includes.

## What changed

[`Source/quests.cpp`](../../Source/quests.cpp) — `StartQuestlog()` now also includes quests still in `QUEST_INIT` (not yet discovered) in the log's list, gated by a new option and single-player only:

```cpp
const bool revealUndiscovered = !gbIsMultiplayer && *sgOptions.Oracool.questLogRevealAll;

for (auto &quest : Quests) {
    if ((quest._qactive == QUEST_ACTIVE && quest._qlog)
        || (revealUndiscovered && quest._qactive == QUEST_INIT)) {
        ...
    }
}
```

Quests excluded by this game's quest-pool randomization stay `QUEST_NOTAVAIL` and correctly don't appear — only quests that are actually part of this game's world show up. `QuestlogEnter()` (pressing a log entry) already just plays that quest's intro flavor text via `InitQTextMsg(quest._qmsg)` for anything before `FirstFinishedQuest` — a read-only action, so including `QUEST_INIT` entries there doesn't mutate any state either.

Added a new Oracool option, `Quest Log Reveal All` (`Source/options.h`/`options.cpp`), defaulting to enabled, following the project's "everything independently configurable" convention — matching the in-game options menu label/description and a `diablo.ini` comment block.

## Why this is the safe design

Every quest's actual trigger — finding the right object, talking to the right NPC, reaching the right level, spawning its quest item — still runs exactly as vanilla intends, completely unaffected by this change, because nothing about `_qactive` or `_qlog` themselves changed. This is purely a display-list change: the log now previews everything available this session, but progressing or completing a quest still requires the same in-game discovery the mechanics were built around. This matches the project owner's own framing of quests as being "just for fun and to enrich the dungeon exploration" — the log is a checklist of what's out there, not a shortcut past finding it.

## Verification

- Bumped `ORACOOL_VERSION` to 1.0.5, rebuilt both `build/x64-Debug` and `build/x64-Release` — both compiled and linked cleanly (66/66 objects on Release), only pre-existing unrelated warnings.
- Launched `DiabloOrcl.exe` (20-second warned launch, per the standing rule) and started a fresh Normal game with the existing "Oracool" character. Opened the quest log (QUESTS button): all **24 quests** appeared immediately, including deep-game ones vanilla would never reveal at level 1 (Diablo, Archbishop Lazarus, Na-Krul, The Defiler, Warlord of Blood) — confirming the reveal-all logic works and every quest label renders correctly with no broken/placeholder text.

## Not done / deliberately left alone

Didn't add any visual distinction between "discovered" and "not yet discovered" quests in the log (same rendering for both) — the project owner didn't ask for that level of polish, and vanilla's `PrintQLString` doesn't have a slot for a third visual state without deeper UI work. A reasonable follow-up if wanted later, not implemented now.

## Related

- [[2026-08-09 - Autosave-Only Play, Part 1]]
- [[2026-08-09 - Character-Only Persistence, No Continue]]
- [[Idea-Backlog]]
