---
date: 2026-08-14
version: 1.5.18
area: Front end / focus model, input, button layout
---

# Four Rules for the Front End

> 1. ALL CLICKABLE MENU ITEMS TO BE REACHABLE WITH THE KEYBOARD AND THE PENTAGRAM SELECTOR.
> 2. ALL SELECTION ACTIONS TO REQUIRE DOUBLE CLICK OR ENTER IF REACHED WITH PENTAGRAM. SINGLE CLICK
>    DOESNT INITIATE THE ACTION THE BUTTON CARIES UNLESS CLICKED TWICE.
> 3. DEVIDE THE 960PX SCREEN INTO 4 EQUAL WIDTH VERTICAL ZONES. THE BOTTOM OF EACH ZONE, IN THE
>    MIDDLE, TO BE CONSIDERED A BUTTON DESIGNATION. BUTTONS OK, CANCEL LIVE IN SECTION 2 AND 3 ON
>    EVERY FRON END SCREEN.
> 4. SHOULD MORE BUTTONS BE REQUIRED BY A SCREEN - THEY WILL OCCUPY BOTTOMS OF ZONE 1 AND 4. NEW HERO
>    TAKES 1, DELETE TAKES 4

Rules as standing law rather than as a change request, so this pass is mostly about putting them
somewhere a future screen inherits them, instead of applying them four times by hand.

## Rule 1 — one focus ring instead of three

The shared focus model only ever knew about a **list**. `SelectedItem` indexes `gUiList` and nothing
else; a `UiArtTextButton` was a click target and no more. Screens that needed a button focused had
each grown their own answer:

| Screen | Its private ring |
|---|---|
| Character list | `HeroActionButtons[4]`, `SelectedActionButton`, `ActionRowFocusable`, `NextEnabledActionButton`, `HeroActionRowNavigation`, `ResetActionRowFocus` |
| Delete prompt | `AnswerButtons[2]`, `SelectedAnswer`, `SelyesnoNavigation` |
| Message box | `OkButton`, plus a `DrawFocusSelector` every frame |
| **Difficulty picker** | **none - its OK and CANCEL were mouse-only** |

Three copies of one idea, and the fourth screen that needed it went without. That is what a rule
about *all* clickable items cannot be built on, so the ring moved into `diabloui.cpp`:

```cpp
std::vector<UiArtTextButton *> gUiButtons;  // filled by UiInitList, left to right
int SelectedButton = -1;                    // -1: the list has focus
```

`UiInitList` collects the buttons out of whatever the screen hands it. A screen written tomorrow
satisfies rule 1 by existing rather than by remembering to.

Three things came free with the move:

- **No pointer-catching.** Each of the three files used to grab `.get()` off every button as it built
  it, and reset the array before rebuilding the screen. `UiInitList` already runs at exactly those
  moments, so the bookkeeping is not moved, it is deleted.
- **The controller and the touch pad.** The private rings matched `SDLK_` symbols through
  `UiPollAndRender`'s event hook, so their buttons answered a keyboard and nothing else. The shared
  one is written against `MenuAction`, which `GetMenuActions` already folds all three input devices
  into. **`MenuAction_LEFT` and `MenuAction_RIGHT` existed and reached nothing in the front end** -
  they are what walks the row now.
- **Left-to-right by geometry.** `gUiButtons` is sorted by `rect.x`, not left in push order - the row
  IS spatial, and selgame pushes its OK and CANCEL *before* its list for an unrelated hit-testing
  reason.

`Delete` still steps over itself while greyed out, because the shared version reads
`UiFlags::ElementDisabled` - the same flag selhero was already setting on it.

The pentagrams on a focused button are drawn from inside `Render(const UiArtTextButton &)`. That is
also a fix: all three files drew them by hand *before* `UiPollAndRender`, each carrying a comment
about the halo being repainted by the background. `gUiItems` is rendered last in a frame, so drawing
from inside that pass makes the problem they each worked around not arise.

## Rule 2 — the first click only points

Lists already came close: vanilla focused a row on the first click and acted on a genuine
double-click. But look at the condition it hung on -

```cpp
if (gfnListFocus != nullptr && SelectedItem != index) { ...focus... }
else if (gfnListFocus == nullptr || event.button.clicks >= 2) { ...act... }
```

`gfnListFocus == nullptr` is a shortcut straight to the action, and **the main menu, the class list
and the difficulty picker all pass nullptr**. On exactly the screens the rule is about, one click
started a game.

Replaced with arming - a pointer to whatever the last click marked:

```cpp
const void *ArmedItem = nullptr;
std::size_t ArmedIndex = 0;
```

First click on an unarmed thing marks it and moves the pentagrams there. Second click on the *same*
thing acts. A genuine double-click needs no special case - it is those two clicks arriving quickly -
and a pair of slow clicks works too, which the strict `clicks >= 2` reading would have refused.

Any keyboard or controller action disarms, so the two clicks have to be consecutive: a click left
half-finished on one screen cannot complete against whatever occupies that spot on the next.

`dbClickTimer` went with the double-click it timed. The SDL1 and SDL2 paths differed only in how they
asked "was that a double-click", so they are now one path.

Enter is untouched, and is still one keypress everywhere - including the vanilla error box, whose OK
also arms now (`UiClickArms`, the one exported piece) but whose own loop has always ended on Enter or
Escape. An error dialog cannot become hard to dismiss.

## Rules 3 and 4 — zones, not a centred strip

The row was 800px centred on the screen, quartered. It is now the **window**, quartered:
`gnScreenWidth / 4` - 240px at 960, and it scales, so the rule holds at every resolution rather than
only the one it was written for. The two 80px dead margins are gone and each button's cell grew from
200 to 240, which the longest label (New Hero) had been closest to outgrowing.

The indices were reordered to the rule. Reading them in order now gives the row as it appears:

| Zone | Was | Now |
|---|---|---|
| 1 | OK | **New Hero** |
| 2 | Delete | **OK** |
| 3 | New Hero | **Cancel** |
| 4 | Cancel | **Delete** |

Two screens were naming the right cells for the wrong reason and are now honest about it:

- **The delete prompt** put Yes and No in `DeleteButtonIndex` and `NewHeroButtonIndex` - which under
  the old numbering happened to be columns 2 and 3. Yes *is* this screen's OK and No is its Cancel,
  so they take those two indices now and land in the same place for a reason that survives the next
  renumbering.
- **The message box** quartered the row into *thirds* to put its lone OK dead centre. Under the zone
  rule OK has a place and does not move to suit how many buttons stand beside it; here there simply
  are none.

### The one thing that had to be refused

`HeroListX` was "centred over the Cancel button" - and Cancel moved to zone 3. Following that
literally would have dragged the 320px list column 180px left, and `HeroPreviewRect` is *everything
to that column's left*, so it would have taken 180px off the character preview - which was its own
request, twice ("twice bigger").

The relationship was already dead anyway: a zone is 240px and the column is 320, so it cannot sit
inside one. The column is now anchored where it already was, at the 4px its scrollbar has always had
at the window's edge - `960 - 4 - 12 - 4 - 320 = 620`, the exact number the old expression produced.
Written as an anchor instead of as a coincidence.

### The bug that shipped anyway (1.5.18)

> double click doesnt initiate action. only enter does. fix it.

Arming was correct; something was clearing it between the two clicks. `HandleMenuAction` began with an
unconditional `DisarmClicks()` — reasonable-looking, and wrong, because **`UiPollAndRender` calls that
function every frame** with `GetMenuHeldUpDownAction()`, which returns `MenuAction_NONE` whenever
nothing is held:

```cpp
HandleMenuAction(GetMenuHeldUpDownAction());   // UiPollAndRender, once per frame
```

So the disarm ran once per *frame* rather than once per *input*. The first click armed, the next frame
wiped it, and the second click found nothing armed and could only arm again. Nothing could ever be
clicked into — Enter was the only way to act, which is exactly the report.

Fixed by returning on `MenuAction_NONE` before anything else happens. The lesson is narrower than
"guard your defaults": **this function is on the frame path, not only the input path**, and its name
does not say so. Anything added above the switch runs 60 times a second.

## Two regressions caught before they shipped

Both found by reasoning through the flows rather than by the build, which was clean either way.

**The name box lost its arrow keys.** `MenuAction_LEFT`/`RIGHT` are also how the text caret moves -
unclaimed menu actions fall through to `HandleTextInputEvent`. The name screen has no list, so the
new "no list, focus the first button" rule would have put focus on OK at once and eaten both arrows.
Fixed by treating the text field as the thing above the row: no auto-focus on a screen with text
input, Down reaches the buttons from the field, Up returns to it, and Left/Right are only claimed
while focus is actually on a button.

**A click could land on an empty or disabled row.** The bound check `index > SelectedItemMax` passes
for index 0 on an empty list (`SelectedItemMax` is 0 there too) - the same underflow `UiInitList`
already guards from the other side. And the disabled-row check sat only on the branch that *acted*,
so a click could still move the rule onto a locked difficulty. Both are checked before anything else
now.

## Files

- `Source/DiabloUI/diabloui.cpp` / `.h` - `gUiButtons`, `SelectedButton`, `ArmedItem`, the rewritten
  `HandleMenuAction`, arming in both mouse handlers, focus drawn in `Render(const UiArtTextButton &)`.
  `UiListSelectorHidden` and `DrawFocusSelector` are internal now; `UiClickArms` is exported in their
  place.
- `Source/DiabloUI/hero/hero_layout.h` - zones, the four indices, `HeroButtonRect`.
- `Source/DiabloUI/hero/selhero.cpp` - private ring deleted; `HeroListX` re-anchored.
- `Source/DiabloUI/selyesno.cpp`, `selok.cpp` - private rings deleted, buttons into zones 2 and 3.
- `Source/DiabloUI/button.cpp` - arming for the vanilla dialog button.
- `Source/DiabloUI/multi/selgame.cpp` - `BlockWidth` no longer borrows the button row's old width.

## Verification

Debug config builds clean at `1.5.18`; full suite **352/354**, the two known pre-existing failures.

The suite cannot see any of this - there is no front-end input test - which is how 1.5.17 shipped
with clicking broken and a clean 352/354 beside it. Worth remembering before trusting the number on
an input change.

Not seen in game, and this one wants a real pass at the controls rather than a look. Worth walking:

- Main menu: one click marks an entry, the second starts it. Enter still works on the marked one.
- Character list: Down off the last character reaches New Hero / OK / Cancel / Delete; Left and Right
  walk them; Delete is skipped while greyed out; Up returns to the list.
- **Difficulty picker: its OK and CANCEL answer the keyboard at all now** - they never did.
- Name box: typing works, **Left and Right still move the caret**, Down reaches OK and Cancel.
- Delete prompt and message box: something is marked the moment they open.
- Every screen's OK sits in zone 2 and Cancel in zone 3, in the same place on each.
