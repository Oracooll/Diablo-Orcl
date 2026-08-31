---
title: 2026-08-10 - HUD Overhaul Phase 1 - Structural Skeleton
date: 2026-08-10
tags: [dev-report]
summary: First implementation pass of the approved three-piece HUD redesign - corner-locked Health/Mana orbs, a centered LMB/belt/RMB row with a 6-slot repurposed belt (Menu popup, Town Portal button), XP bar beneath the row, and a cursor-following tooltip replacing the old black info box. Placeholder art throughout; polished assets come later.
---

# HUD Overhaul Phase 1 - Structural Skeleton

## Context

With the waypoint system closed out ([[2026-08-10 - Waypoint System Complete]]) and both Tier 1 backlog items confirmed done, the user pivoted from feature work to V1's founding purpose - the 960x720 UI overhaul (see [[V0 to V1 Fork]]). The target design was worked out interactively and approved via an HTML mockup, then planned in detail (three parallel research passes plus a design pass) before any code was touched. Per the user's explicit instruction - *"can we implement in the game this raw design and then replace gradually with approved assets?"* - this pass builds the real, working structural skeleton reusing existing sprites as placeholders. No new art was created.

## The design (user-approved)

The old fixed 640x128 bottom panel is replaced by three independently-anchored pieces:

1. **Health Orb** - locked to the screen's bottom-left corner regardless of resolution.
2. **Mana Orb** - locked to the bottom-right corner.
3. **Middle HUD** - horizontally centered: `[LMB skill button] [6-slot belt] [RMB skill button]`, with the XP bar directly beneath spanning the row.

The belt's 6 visible slots repurpose the underlying 8-wide array: slot 1 is a **Menu button** (opens a popup consolidating everything the old 8 panel buttons did, plus two mini-map controls), slots 2-5 are the real potion/scroll slots, and slot 6 is a **permanent free Town Portal** button (Diablo 3 style, single-player only). The old black description box is gone, replaced by a tooltip that follows the mouse cursor.

Two scope decisions confirmed with the user during planning: the RMB button hosts the *existing* readied-spell/speedbook indicator (relocated, not new scope), and the LMB/RMB "assign a spell and cast by clicking" mechanic is deferred to a later phase - the LMB button is a visual placeholder for now.

## What changed

**New files:**
- `Source/oracool/hud_layout.h/.cpp` - single source of truth for all new HUD geometry (orb anchors, middle-HUD rect, skill-button rects, belt slot-role constants `BeltMenuSlotIndex`/`BeltTownPortalSlotIndex`/`IsRealBeltItemSlot()`), plus `MigrateHiddenBeltSlots()`.
- `Source/oracool/hud_menu.h/.cpp` - the belt Menu popup (10 entries: Character, Quests, Automap, Game Menu, Inventory, Spellbook, Chat, Friendly Fire, Recenter Mini-Map, Toggle Mini-Map Zoom - the first 8 are a straight relocation of the old `CheckBtnUp` switch bodies), plus the Town Portal belt button and its `CastTownPortalAtFeet()`. Pattern-matched on `waypoint_menu.cpp` - own open/close bool, NOT wired into `gmenu.cpp`'s pausing singleton, so gameplay keeps running while it's open.
- `Source/oracool/cursor_tooltip.h/.cpp` - draws `InfoString`/`InfoColor` in a translucent box at the cursor (+16,+16 offset, clamped on-screen), with `GetPrevCursorTooltipRect()` for the <=640-wide dirty-rect path.

**Orbs** (`control.cpp`, `scrollrt.cpp`): `DrawFlaskUpper`/`DrawFlaskLower` take a `Point panelAnchor` parameter instead of reading `GetMainPanel()` internally - their internal math is untouched, callers just pass `GetHealthOrbAnchor()`/`GetManaOrbAnchor()`. Flask value text and the dirty-rect blit regions follow the same anchors.

**Belt** (`inv.cpp`, `diablo.cpp`, `multi.cpp`): `DrawInvBelt` renders 6 slots (Menu icon, 4 items, TP icon); every hit-test path (`FindTargetSlotUnderItemCursor`, `CheckInvCut`'s scan, `CheckInvHLight`) and both auto-place functions (`MergeStackableItemIntoBelt`, `AutoPlaceItemInBelt`) skip non-real slots; `UseInvItem` has a defense-in-depth guard; belt hotkeys are now 1-4 mapping to `SpdList[1..4]`. `MigrateHiddenBeltSlots()` runs once per session after `pfile_read_player_from_save` (in `multi.cpp`'s `InitSingle`) - items stranded in slots 0/5/6/7 by older saves move to slots 1-4, overflow to inventory, and anything unplaceable stays put with a `LogEvent` entry. **`MaxBeltItems`/`SpdList[8]`/`InvRect[]` are untouched - save format unchanged.**

**Town Portal button**: `CastTownPortalAtFeet()` sends the same `CMD_SPELLXY` wire command the normal cast path uses, targeted at the player's own tile, skipping `CheckPlrSpell`'s cursor-dependent guards. Gated to single-player and out-of-town, matching the existing always-memorized zero-mana TP mechanic it rides on.

**RMB button**: `DrawSpell` (`panels/spell_list.cpp`) and both its hit-tests (`DoPanBtn` click, `CheckPanelInfo` hover) now read `GetRmbSkillButtonRect()` instead of hardcoded panel offsets. The `'s'` hotkey and shift-click-to-clear behaviors are unchanged.

**XP bar** (`qol/xpbar.cpp`): centered on `GetMiddleHudRect()` and positioned just beneath the row, for both drawing and hover info.

**Tooltip**: `DrawInfoBox` split - the string-population half (load-bearing for every hover system) stays as `UpdateInfoString()`; the box-drawing half is `oracool::DrawCursorTooltip()`, called just before `DrawCursor` so it never renders under the cursor sprite.

**Removed** (superseded, deleted outright): the 640px panel background blit (`DrawCtrlPan`), all 8 panel buttons and their machinery (`PanBtnPos`, `PanelButtons`, `PanelButtonIndex`, `panbtndown`, `DrawCtrlBtns`, `ClearPanBtn`, `CheckBtnUp`, `control_check_btn_press`, `SetButtonStateDown`, `PanBtnStr`/`PanBtnHotKey`, the `panel8bu`/`p8but2` sprite loads, and `mainpanel.cpp`'s button-label baking/`PanelButtonDown` sheet). `DoPanBtn` kept its name but now only handles the RMB skill button. The dead-player click path and touch handlers were rewired accordingly; `ClosePanels()` and `PressEscKey()` close the new popup.

## Known placeholder-pass rough edges (deliberate)

- All visuals are reused sprites/text labels ("M", "TP", plain text list for the Menu popup) - the whole point of this pass; polished art replaces them gradually.
- The orbs still draw their flask-fill from the old panel image buffer, so they carry a slice of panel behind them until dedicated orb art exists.
- The belt still blits its old strip background, and slots 6/7's InvRect area is simply unused.
- The tooltip uses the old info box's fixed 288x64 size rather than sizing to its text - fine for readability, revisit with the art pass.
- Mini-map directional panning stays keybind-only (Alt+arrows); the popup got Recenter and Zoom Toggle, where one-shot clicks make sense.

## Verification

Debug build compiles clean (v1.0.55, three checkpoint builds along the way). Not yet play-tested - the user will test manually (launch it yourself when ready). Test focus: orb positions at 960x720 and one wider resolution, belt clicks/hotkeys 1-4, every Menu popup entry, TP button in a dungeon, RMB speedbook at its new position, tooltip following the cursor over items/monsters/NPCs, and a save that had items in old belt slots 5-8 (migration + event log entry).

## Related

- [[2026-08-10 - Waypoint System Complete]]
- [[V0 to V1 Fork]]
- [[Idea-Backlog]] (HUD entry updated to reflect this pass)
