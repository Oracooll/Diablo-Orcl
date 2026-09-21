---
version: v1.12.107
date: 2026-09-21
area: UI / vendors
tests: 832/832
---

# Vendor tabs sink while held and open on the release

## The ask

> make all tab buttons on all vendors sinkable on click. sink holds as long as click and springs back to
> normal on click release. opening clicked tab counts if release happens within region of button.

This is the release rule of v1.12.102 (Griswold's CONFIRM / CANCEL) and v1.12.103 (the Rift Monument's
menu) applied to the last family of buttons that still acted on the press: the vendors' tab columns.

## What changed

### The shop's column (`oracool/shop_grid.cpp`)

The column now holds two file-local values: the `TalkID` of the tab being held and the rect it was pressed
at. The rect travels with the id because the column is drawn for **two** screens - the shop itself and
Griswold's Salvage page, which is not a shop screen at all - so the release does not have to work out which
column a press came from before it can find the button again.

Three exports replace the old switch-on-click:

| Function | What it does |
| --- | --- |
| `PressShopTabAt(position, open)` | Sinks the tab under `position` and sounds the click. Nothing opens. |
| `TakeReleasedShopTab()` | The held tab **if the pointer is still inside it**, `TalkID::None` otherwise - and clears the press either way. |
| `ReleaseShopTabButton()` | The shop's own mouse-up: opens the released tab's shelf. Joined `LeftMouseUp`. |

`DrawShopTabColumn` draws the held tab's face at `{ -2, +2 }`, the same 2 px down-left sink every other
pressed button in the mod wears. The **hit test stays on the unsunk rect**, so a tab cannot slide out from
under a pointer that has not moved - which would otherwise make a perfectly still click fail its own
release test.

The tab you are already on presses and springs back like the others; it simply has nothing to open.

### The right button

A right click on a tab still switches at once. There is no `RightMouseUp` in this engine to spring the tab
back with, so a right-pressed tab would stay sunk until the next left click landed somewhere else.
`CheckShopTabColumnClick` takes the `rightClick` flag `CheckShopGridClick` already had and branches on it.

### Griswold's Salvage page (`oracool/levski_roar.cpp`)

The column beside the Salvage page presses through the same helper. `ReleaseLevskiButtons` asks
`TakeReleasedShopTab()` first, while the docked page is open, and closes the page for the shelf the release
landed on - so the page's icons, its confirmation buttons and its tabs all now answer to one rule.

### The artisans' workshops (`oracool/workshop.cpp`)

Gillian's and Ogden's tabs already ran on the release - they are ordinary `Control::Tab0..Tab3` values and
`ReleaseWorkshopButton` has decided them since v1.12.104. What they did not do was **show** it, so
`DrawTabColumn` now draws the held tab's plate, its outline, its stacked label and its hover brighten at the
sink, hit test unmoved.

## Coverage

Every vendor's tabs are the one shared column (`ShopTabsFor`): Griswold's ten, Adria's three, Pepin's,
Wirt's two, and Griswold's Salvage page beside it. The workshops' tabs are the other column. Both now sink
on the press and act only on a release inside the button that was pressed.

## Build

Debug, clean. 832/832.
