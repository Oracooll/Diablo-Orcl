---
date: 2026-08-13
version: 1.1.63
area: UI / input routing
severity: gameplay-affecting
---

# Three Windows Routing Clicks Through a Rect They Outgrew

## The report

> Make sure RESET is clickable. Now it isn't. Click lands on ground and moves character.

Followed by the rule this establishes:

> Area of UI screens should in no case permit clicks on the ground!

## What was actually wrong

`LeftPanel` is the vanilla left-hand slot: `{ {0,0}, SidePanelSize }` where
`SidePanelSize = { 320, 352 }` (control.h:35). Every click over the left panel was routed by

```cpp
if (GetLeftPanelContent() != LeftPanelContent::None && GetLeftPanel().contains(MousePosition))
```

in `LeftMouseDown` (diablo.cpp). Miss that rect and the click falls through the whole `else if`
chain to the world handler, which walks the player.

Three of the four things that can occupy the left panel have since been rebuilt as **340x720**
windows — the character sheet, the quest log and the waypoint list — while the routing rect stayed
320x352. Everything below y=352, or right of x=320, was drawn as UI and treated as ground:

| Window | Dead region | Effect |
|---|---|---|
| Character sheet | RESET at y 516 | Reported: click walks the player |
| Character sheet | + buttons span x 291-332 | Only the part left of x=320 responds |
| Waypoint list | entries 8-16 (rows start at y 101, 35px each) | Nine of seventeen warps unclickable; click walks the player |
| Quest log | any row below y=352 | Selection fails; click walks the player |

The inventory was never affected — when it grew, it was given its own exported
`oracool::GetInventoryPanelRect()` and the router tests that. That is the pattern the other three
never got.

## The fix

`GetLeftPanelContentRect()` (control.cpp), switching on the same `GetLeftPanelContent()` that
already decides what to draw and what to route to:

```cpp
switch (GetLeftPanelContent()) {
case LeftPanelContent::Character:    return GetCharacterPanelRect();
case LeftPanelContent::QuestLog:     return GetQuestLogPanelRect();
case LeftPanelContent::WaypointMenu: return oracool::GetWaypointMenuRect();
case LeftPanelContent::Stash:        return LeftPanel;   // still genuinely 320x352
case LeftPanelContent::None:         break;
}
```

Content and rect now come out of one switch, so a window cannot be drawn in one place and
hit-tested in another. `IsOverLeftPanel(Point)` wraps it for callers.

Two rects had to be exported to make this possible: `GetQuestLogPanelRect()` (quests.h) and
`oracool::GetWaypointMenuRect()` (waypoint_menu.h). Both were previously file-local constants.

### Call sites moved onto it

- `diablo.cpp` `LeftMouseDown` — the routing gate. This is the reported bug.
- `cursor.cpp` `CheckCursMove` — hover. Without this the cursor would still highlight monsters and
  items *through* the lower two-thirds of an open window, inviting exactly the click the router now
  refuses. A silent half-fix would have been worse than none.
- `controls/touch/event_handlers.cpp` — touch movement gating, same authority.

## Note for the spellbook pass

`GetRightPanel()` is the mirror of this problem waiting to happen. It is still 320x352 and the
spellbook still draws at `SidePanelSize.width` (spell_book.cpp:111), so today they agree. The
moment the spellbook becomes a 340x720 window it must get its own exported rect and a
`GetRightPanelContentRect()` alongside it, or it will reproduce this bug exactly — with the
inventory as the one window that already does it correctly.

## Verification

Debug config builds clean at `1.1.63`.

Manual checks wanted, since this is input routing and cannot be asserted:
1. Character sheet: RESET clicks and resets; all four + buttons respond across their full width.
2. Waypoint list: entries 8-16 warp rather than walking the player.
3. Quest log: rows below the halfway point select.
4. Clicking empty space anywhere inside any of the three windows does **nothing** — no walking.
5. Hovering a monster standing behind the lower half of an open window does not highlight it.
