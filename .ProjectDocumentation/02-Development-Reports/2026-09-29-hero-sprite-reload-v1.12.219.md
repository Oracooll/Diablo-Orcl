# 2026-09-29 - Equipping a weapon changes the hero's sprite again (v1.12.219)

**Date:** 2026-09-29. Debug only. The user: "Close barb axe note. There is some bug where i can equip a weapon but the hero sprite doesnt change from fist to axe for some reason but reequiping fixes it. ... You check your code and if you find something - great."

## Cause

v1.12.201 (2026-09-27) changed five `CalcPlrInv(player, true)` calls in `inv.cpp` to `CalcPlrInv(player, false)`. Its report gives no reason; it reads like an accidental replace-all while `ReseatOutgrownItems` (which rightly uses `false`) was added.

`loadgfx == false` makes `CalcPlrItemVals` record the new look (`_pgfxnum`, `_pGearLook`) without reloading the sheets. The next call that did reload then saw no change, so the hero kept the old sheets until the weapon was re-equipped by hand (`CheckInvPaste` passes `true`). Sheets not loaded yet, such as the attack sheet, loaded for the new look, so stand/walk and attack could disagree. That matches the note's first report of sword attack frames with an axe in hand.

The five paths:
- **AutoEquip:** right-click equip, auto-equip on pick-up, a purchase, taking from the stash;
- **CheckInvCut:** lifting a worn item off;
- **Imbuing and socketing** an equipped item;
- **TransferItemToStash:** of a worn item.

## Fix

- **The five calls:** `true` again.
- **Levski's Mend** (`crafting.cpp`): also `true`, since mending a broken weapon in the hand makes it count again, which changes the look.

The other `false` callers are the load, death and new-game paths, where no reload is wanted.

## Test

Debug build and ctest: 878/878 passed. Not seen in play yet.
