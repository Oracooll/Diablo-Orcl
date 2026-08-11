---
title: 2026-08-09 - ClearUI Debug Command
date: 2026-08-09
tags: [dev-report]
summary: Added a "clearui" debug command that hides every ambient HUD element (bottom panel, mini-map, Event Log, Game Clock, XP Counter, durability icon, item labels, monster health bars, floating numbers, FPS counter, and the cursor) for a genuinely clean screenshot.
---

# ClearUI Debug Command

## Context

The user asked for a way to take a clean screenshot with no UI elements at all. The existing `hideui` command (see its own earlier dev history) only ever hid the bottom control panel (orbs, belt, buttons, XP bar) - everything else (mini-map, Event Log button/window, Game Clock, XP Counter and its gain indicator, the durability warning icon, item name labels, monster health bars, floating damage numbers, the FPS counter, and the mouse cursor) is drawn in a separate, unrelated code path in `DrawView()`/`DrawAndBlit()` and was never gated by anything.

## What changed

- **`Source/debug.h`**: new `extern bool DebugClearUi;`, documented as a broader, independent sibling to `DebugHideUi`.
- **`Source/debug.cpp`**: `bool DebugClearUi = false;`, `DebugCmdClearUi()` (toggle + `RedrawEverything()`), registered as the `clearui` console command.
- **`Source/engine/render/scrollrt.cpp`**: gated every ambient HUD draw call behind `!DebugClearUi` (mirroring the existing `#ifdef _DEBUG` / bare-`{block}` idiom used for `DebugHideUi`, required because `debug.cpp` - and therefore the actual `DebugClearUi` variable definition - only exists in Debug builds at all):
  - Mini-map (`DrawMiniMap`)
  - Event Log button/window, Game Clock, XP Counter, XP gain indicator
  - Item name labels, monster health bars, floating damage numbers
  - Durability warning icon (`DrawDurIcon`)
  - FPS counter
  - The mouse cursor (both `DrawCursor` call sites)
  - The bottom HUD panel, which `hideui` already hid - `clearui` now hides it too, via `if (!DebugHideUi && !DebugClearUi)`

Bumped `ORACOOL_VERSION` to `1.0.42` and rebuilt the Debug config clean.

## Why

`DrawMonsterWallOutlines()` was deliberately left ungated - its own existing comment states it's "always called to drain the queue every frame," meaning skipping the call (not just skipping what it draws) would let that internal queue build up unbounded while `clearui` stays on. Everything else in this pass is a pure draw call with no side effect, safe to skip outright.

Left untouched, matching `hideui`'s own original scope decision: anything the player opened themselves - inventory, character panel, quest log, spellbook, Stash, an open store dialog, or the full-screen automap (TAB). Those already have their own close controls, and force-closing them would be a much bigger behavior change than "hide the ambient HUD."

## Verification

Debug build completed with no errors. Not yet manually tested in-game by the user.

## Related

- (hideui's own original dev history, from earlier in the project, predates this vault's per-topic report naming)
