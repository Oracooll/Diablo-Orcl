---
title: 2026-08-09 - Autosave on Waypoint Activation
date: 2026-08-09
tags: [dev-report]
summary: Added an autosave trigger for opening a waypoint sigil's travel menu, following the same per-event toggle pattern as the rest of the trigger set.
---

# Autosave on Waypoint Activation

## Context

Follow-up to [[2026-08-09 - Autosave Cheat Command and Wider Event Coverage]]: the user asked to add waypoint activation to the trigger list.

## What changed

New `Auto Save on Waypoint Activation` option ([Source/options.h](../../../Source/options.h), [Source/options.cpp](../../../Source/options.cpp)) and `oracool::ScheduleAutoSaveForWaypointActivation()` ([Source/oracool/auto_save.h](../../../Source/oracool/auto_save.h), [Source/oracool/auto_save.cpp](../../../Source/oracool/auto_save.cpp)), hooked in `OperateWaypoint()` ([Source/objects.cpp:2022](../../../Source/objects.cpp:2022)) right after `oracool::OpenWaypointMenu()`.

"Activation" here means opening a sigil's travel menu - the only waypoint interaction that currently exists (see [[2026-08-09 - Autosave-Only Play, Part 1]] and the waypoint feature's own dev history: there's no unlock-persistence system yet to distinguish "first discovery" from "reopening a known waypoint"). Once unlock persistence is built, this is the natural place to keep the hook, since it already fires on every menu-open regardless of lock state.

Bumped `ORACOOL_VERSION` to `1.0.38` and rebuilt the Debug config clean.

## Verification

Debug build completed with no errors. Not yet manually retested in-game by the user.

## Related

- [[2026-08-09 - Autosave Cheat Command and Wider Event Coverage]]
