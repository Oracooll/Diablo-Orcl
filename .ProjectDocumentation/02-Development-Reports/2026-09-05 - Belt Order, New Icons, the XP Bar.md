# Belt Order, New Icons, the XP Bar (v1.9.219)

**Date:** 2026-09-05 · **Request** (with two 1280x720 screenshots): the orb cropping only at 4:3; burger to slot 6, portal to slot 5, potions 1-4; sweep the drop zone for new burger/portal icons and use them; XP bar closer to the belt; XP counter hidden, shown on hover, monster pool on click.

## The clip only at 4:3

`scrollrt.cpp`: the orb clip against `SidePanelContentBottom` runs only when `gnScreenWidth * 3 <= gnScreenHeight * 4 + 8`. At 960x720 the 340px panels reach the orbs; at 16:9 they do not, and the clip was costing the sphere its crown for nothing.

## Belt order

`Player::SpdList` numbering (0 menu, 1-4 items, 5 portal) is saved data and the BeltItem keys, so it stays. What moved is the CELL each slot is drawn in: `BeltCellOfSlot` in `hud_layout.h` (1-4 → 0-3, 5 → 4, 0 → 5), applied inside `GetBeltSlotRect`, which every draw call and hit-test already goes through. `GetBeltCellRect` is the raw row geometry for things that span it (the XP bar). Test pins the order.

## The icons

The sweep found `diablo-bottom-hud-2400x384-v1/icons/` back in the drop-zone root - GPT's burger and portal sets (masters, 112px sources, 28px runtime states, 84x28 strips) plus a minimal burger variant. Filed under the pack as `icons/`; the two runtime strips ship unchanged as `ui\burger_menu_button.png` and `ui\town_portal_icon.png`. Both icons are 28x28 with three states now; the burger's blink alternates hover and click instead of unlit and lit. An 88x88 charcoal tile that also arrived is filed under `textures/`, unused.

## The XP bar and counter

The cutter emits `BeltBarTop` (the painted belt bar's top edge, plate-local 67) and `GetBeltRunTop()` returns it on screen (the cells' top on the plateless row). `GetXPBarRect` hangs 1px above that line; it no longer derives from the counter. The counter is drawn only while the cursor is over the bar (padded 2px) or the bar is held, directly above the bar; holding shows the remaining-monster pool as the counter's own click used to. `IsPointOverXpCounter` and the click test both use the bar now.
