---
date: 2026-08-14
version: 1.5.19
area: Display / window state, resolution changes
---

# The Flag That Stopped the Picture

> there is a bug in devilutionx - everytime i change resolution or switch fit to screen on/off the
> game rendering freezes. the game keeps runing though. i need to ESC my way out of the game and
> restart it to apply new graphics settings.

## Reading the symptom

"Rendering freezes, the game keeps running" is a narrow description. Input works, the menus advance
under the frozen picture (the user navigates out by feel), and a restart clears it. So the loop runs,
the frame is computed, and something throws it away at the last step.

There is exactly one thing in the codebase that does that persistently - `RenderPresent`:

```cpp
if (!gbActive) {
    LimitFrameRate();
    return;
}
```

`gbActive` is written in precisely three places: `true` once at startup, and then only from window
events - `SDL_WINDOWEVENT_HIDDEN`/`MINIMIZED` clear it, `SHOWN`/`EXPOSED`/`RESTORED` set it. There is
no third source, and nothing ever checks whether it is still true.

That is fine while the only thing moving the window is the person using it. It is not fine across a
resolution change, because **the game moves the window itself**: `ResizeWindow` calls
`SDL_SetWindowSize`, `SDL_SetWindowFullscreen`, `SDL_SetWindowDisplayMode`, and `ReinitializeRenderer`
below it destroys and rebuilds the renderer. All of that generates hide/show/minimise traffic of its
own. If the last message of it to be processed says "hidden", the flag is stuck false and the picture
never updates again - which is exactly the report, including why a restart fixes it.

## Three changes, one of which is the actual fix

**1. Ask the window, do not trust the last message about it.** New `SyncWindowActiveState()` sets
`gbActive` from `SDL_GetWindowFlags` - a fact rather than a recollection - and logs when the two
disagreed, so the disagreement can be seen rather than assumed:

```cpp
const Uint32 flags = SDL_GetWindowFlags(ghMainWnd);
const bool active = (flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) == 0;
```

Called at the end of `ResizeWindow` and `SetFullscreenMode`.

**2. Make it self-healing - this is the change that actually fixes it.** Calling the sync at the end
of `ResizeWindow` is *not enough on its own*, and it is worth being clear about why: the events the
resize generated are still sitting in the queue at that moment. One dequeued a frame later puts the
flag straight back. So `RenderPresent` re-asks whenever it is about to skip:

```cpp
if (!gbActive)
    SyncWindowActiveState();
if (!gbActive) { LimitFrameRate(); return; }
```

A genuinely minimised window still reports minimised and still skips, so nothing is lost. A stale
flag costs one frame instead of the rest of the session. The check only runs while not rendering.

**3. Only this window's events may speak for this window.** Both handlers - `MainWndProc` and the
front end's copy in `UiHandleEvents` - switched on `event.window.event` without ever looking at
`event.window.windowID`. Events queued for a window that has since been destroyed are still
delivered, and this codebase really does destroy and recreate its window: `FreeRenderer` does so on
Windows when the outgoing renderer was Direct3D 9, as a workaround for an SDL VSYNC-timer bug
(libsdl-org/SDL#5099). A `HIDDEN` belonging to the window we just replaced would clear the flag for
the one we just built.

## What I could not establish from the code

Which of the three window calls emits the message that sticks - and whether the D3D9 window
recreation is involved at all. It is guarded on `frameRateControl != VerticalSync`, and VerticalSync
is the default on Windows, so on a default configuration that path does **not** run and change 3 is
hygiene rather than the cure. Chasing that further needs a run, not a re-read.

That is what the log line in change 1 is for. If the freeze is gone, the fix is right for the wrong
reason at worst. If it is not gone, the log says whether `gbActive` was the culprit at all - and if
no correction is ever logged, it was not, and the next place to look is the surfaces rather than the
flag.

## Files

- `Source/utils/display.cpp` / `.h` - `SyncWindowActiveState`, called from `ResizeWindow` and
  `SetFullscreenMode`.
- `Source/engine/dx.cpp` - the self-healing check in `RenderPresent`.
- `Source/init.cpp`, `Source/DiabloUI/diabloui.cpp` - the `windowID` guard in both window-event
  handlers.

## Verification

Debug config builds clean at `1.5.19`; full suite **352/354**, the two known pre-existing failures.

**The suite proves nothing here** - there is no display test, and the failure only exists against a
real window manager. This one has to be confirmed by changing the resolution in the settings menu and
watching whether the screen keeps drawing. If it does not, `diablo.log` should now say what the flag
was doing.
