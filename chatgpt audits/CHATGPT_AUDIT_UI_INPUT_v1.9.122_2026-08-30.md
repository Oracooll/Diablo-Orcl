# UI, geometry, and input audit: v1.9.122

**Audited commit:** 2c3e8d15d0fdc475562024bfc9b2feee9eb8852a  
**Date:** 2026-08-30

## UI-01 — High — The revived XP bar is not integrated with the current HUD/input contract

v1.9.119 restored the XP bar's drawing in Source/qol/xpbar.cpp but deliberately changed no caller.
That one-file approach missed three contracts that the old bar used to satisfy.

### UI-01A — The default plateless HUD gives an 8-pixel bar only a 6-pixel gap

The XP bar overwrites the top two pixel rows of the belt backing whenever HUD Plate Art is off,
which is the default.

The geometry is resolution-independent because all of these values are bottom-anchored:

~~~text
middle HUD height                       = 274 * 356 / 1505 = 64 px
XP counter bottom relative to HUD top  = 12 px
plateless belt top relative to HUD top = 64 - 46 = 18 px
available vertical gap                 = 18 - 12 = 6 px
XP bar height                          = 8 px
overlap                                = 2 px
~~~

Source/qol/xpbar.cpp:74-81 sees the negative spare space and clamps the bar's top to gapTop. It
does not and cannot “pin to the belt's top edge” as its comment claims; the 8-pixel rectangle then
extends two pixels into the belt.

Source/engine/render/scrollrt.cpp:2065-2079 draws belt backing/items first and the XP bar after
them, so the bar visibly overpaints those two rows rather than being hidden behind the belt.

Evidence:

- Source/options.cpp:1495 defaults HUD Plate Art to false.
- Source/oracool/hud_layout.cpp:43-70 defines the 64-pixel middle HUD.
- Source/oracool/hud_layout.cpp:92-98 defines a 46-pixel plateless belt backing.
- Source/oracool/hud_layout.cpp:372-387 bottom-aligns that backing.
- Source/oracool/xp_counter.cpp:25 and 31 define the 20-pixel counter and its bottom at HUD top +12.
- Source/qol/xpbar.cpp:39-45 defines the 8-pixel bar.

Repair by establishing at least an 8-pixel non-overlapping gap in both HUD modes. Do not merely
hide the overlap with draw order. Export GetXPBarRect and test:

- HUD Plate Art on and off;
- 960x720 plus minimum supported resolution;
- bar bottom <= belt top;
- counter bottom <= bar top;
- horizontal span equals the intended belt run.

### UI-01B — The bar draws on top of chat input

The vanilla XP bar returned early while talkflag was set. The restored implementation checks only
the Experience Bar option and MyPlayer.

Source/engine/render/scrollrt.cpp:2076-2079 draws DrawTalkPan first and DrawXPBar second. Therefore
opening chat does not hide the bar; it paints the gold strip over the chat panel.

Evidence:

- Current Source/qol/xpbar.cpp:157-169 has no talkflag condition.
- The DevilutionX 1.5.5 baseline's DrawXPBar began with:
  “if experienceBar is off OR talkflag, return.”
- The current HUD code already hides the middle plate while chat covers the same band
  (Source/engine/render/scrollrt.cpp:2050-2056).

Restore the talkflag guard, or gate DrawXPBar from the same draw-state boolean that controls the
rest of that HUD band. Add a render-state test that the XP bar is absent while chat is active.

### UI-01C — The visible bar has no tooltip and lets clicks fall through to the world

Source/qol/xpbar.cpp:172-174 still implements CheckXPBarInfo as an unconditional false. The old
implementation hit-tested the bar and showed Level, Experience, Next Level, remaining XP, and a
maximum-level message.

The new rectangle is also missing from Source/oracool/hud_layout.cpp:432-445
IsPointOverHudChrome. It lies between the counter and belt, so neither existing rectangle covers
it. A left or right click on the visible bar can therefore become a walk, attack, or cast in the
world behind it. Hovering never produces the former XP details.

Repair with one public, option-aware XP-bar rectangle used by:

- DrawXPBar;
- CheckXPBarInfo;
- IsPointOverHudChrome;
- geometry and input-routing tests.

The hit target should exist only when the bar is actually visible, including the talkflag and
player/max-level policy. Restore the old tooltip content with current formatting helpers.

## UI-02 — Medium — Quick-list F-key binding can target the previously hovered cell

### Observation

The quick-list F1-F8 feature stores HoveredPickerSpell during DrawSkillPicker and reads that cached
value later during key input. SDL input is drained as a batch before the next draw. Mouse motion,
wheel scrolling, and F-key input can therefore be processed against different geometry in one
batch.

### Evidence

- Source/oracool/skill_picker.cpp:39-40 describes HoveredPickerSpell as the value “as of the last
  draw.”
- Source/oracool/skill_picker.cpp:293-295 returns that cached value.
- Source/oracool/skill_picker.cpp:326-343 changes PickerScroll without invalidating or recomputing
  the cached hover.
- Source/oracool/skill_picker.cpp:345-359 clears hover only when drawing begins.
- Source/oracool/skill_picker.cpp:447-454 assigns hover during drawing.
- Source/panels/spell_book.cpp:1322-1334 binds GetSkillPickerHoveredSpell.
- Source/diablo.cpp:1244-1267 handles keydown and mouse motion as independent events.
- Source/diablo.cpp:1465-1474 drains every FetchMessage event before drawing again.

### Deterministic event simulation

1. Render the picker with spell A under the cursor. Cached hover becomes A.
2. Before the next frame, enqueue a mouse-wheel event that scrolls the list and then F1.
3. The wheel changes PickerScroll, so spell B is now geometrically under the cursor.
4. F1 is handled in the same event-drain loop before DrawSkillPicker runs.
5. GetSkillPickerHoveredSpell still returns A, so A is bound even though the visible/current cell
   is B.

The same stale-target shape exists for a mouse-motion event followed immediately by F1.

### Recommended repair

Create one pure geometry query such as SkillPickerSpellAt(Point) that builds the current entries,
uses the current PickerScroll, and returns the current cell. Use it for drawing, clicks, and F-key
binding.

Simply invalidating hover on motion/wheel is safer than the current behavior but makes a legitimate
move-then-F1 gesture do nothing until another frame. Computing from current state gives both safety
and responsiveness.

Add an event-order regression test for:

- draw A, scroll to B, press F1 before redraw;
- draw A, move outside, press F1 before redraw;
- clipped cells above/below the viewport;
- attack/aura entries remaining unbindable.

## UI-03 — Medium — The Abilities window still assigns F1-F8

### Observation

The v1.9.121 requirement and its development report say F1-F8 are assignable from quick lists, “not
from the abilities windows.” The old Abilities editing branch remains fully active.

### Evidence

- Source/panels/spell_book.cpp:1341-1367 checks sbookflag, reads HoveredAbilitySpell, removes or
  writes a binding, schedules an autosave, and redraws.
- Source/panels/spell_book.hpp:43-47 still documents Abilities-window binding.
- Source/diablo.cpp:976-978 routes F1-F8 to HandleAbilityFKey while panels are open.

### Reproduction

1. Open Abilities.
2. Hover a bindable skill/spell.
3. Press F1, or Shift+F1 for the left button.
4. The old branch assigns/removes the binding, despite the new quick-list-only rule.

Remove the Abilities mutation branch if the requirement is literal. If Abilities should still show
badges, keep display lookup separate from assignment. Add a test that pressing F1 over Abilities
does not change either hotkey array, while the same gesture over the appropriate quick list does.

## UI-04 — Product decision — The mini-map remains a click-through overlay

v1.9.122 fixed the event log's click-through but intentionally left the mini-map unresolved.
GetMiniMapScreenRect is declared at Source/automap.h:160 and built at Source/automap.cpp:1270, but
it is absent from both IsPointOverHudChrome and IsPointOverFloatingWindow.

Current behavior:

- hovering through the map can highlight a monster behind it;
- clicking the map can walk, attack, or cast into the world;
- blocking it would make the top-right world area unclickable while the permanent map is enabled.

This needs a documented product choice. Possible alternatives:

- block all world interaction over the mini-map, following the existing “UI must not click through”
  rule;
- allow full click-through and document the overlay as intentionally non-interactive;
- add a modifier or temporary transparency/pass-through mode;
- let clicks through only when the map opacity is below a chosen threshold.

Whichever behavior is selected should have an explicit routing test so it does not oscillate in
future audits.
