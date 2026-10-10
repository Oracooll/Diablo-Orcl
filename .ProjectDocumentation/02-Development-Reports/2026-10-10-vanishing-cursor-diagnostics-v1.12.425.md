# v1.12.425 - vanishing cursor: diagnostics (2026-10-10)

User report (play pass of v1.12.424): Barbarian, dungeon level 2, after the Poisoned Water quest level, walked back up the stairs to level 2 and the cursor was gone - mouse movement did nothing, nothing brought it back but a restart. Keyboard and mouse only (no controller); Hardware Cursor=1 in diablo.ini. The user believes it started with the visual upgrades (v1.12.419-424).

Ruled out by reading:
- pcurs set to CURSOR_NONE in play: only the multiplayer timeout and the game's end do that.
- ControlMode stuck on a gamepad: a real mouse motion sets it back to keyboard-and-mouse.
- The loading screen's SetHardwareCursorVisible(false): the first DrawCursor after it shows the cursor again (and rebuilds its image if a palette fade asked for that).
- The upgrade passes writing out of bounds on the water level (ApplyHeatHazeAndWater, ClassifyPiece): every write clamps to the screen region or the 64x48 scratch surface.
- No cursor, palette, hwcursor or input file was changed by the upgrades; dx.cpp only gained NoteFramePresented.

Added two Debug console commands:
- `cursorinfo` - pcurs, control mode and device, hardware cursor enabled / in use / shown, its type, and whether a rebuild is pending.
- `cursorfix` - forces keyboard-and-mouse, the hand cursor if nothing is held, rebuilds and shows the hardware cursor.

Next: the user reproduces and types `cursorinfo` the moment the cursor vanishes; the answer decides the fix.

## v1.12.426 (same day)
User: "we should not have heat waves over the water in poisoned water well. we only need that effect over lava." The pool's cycling colours read red-dominant while poisoned, so the classifier called it lava; the Poisoned Water Supply set level (SL_POISONWATER) is now always water (the moving light only, no shimmer).
