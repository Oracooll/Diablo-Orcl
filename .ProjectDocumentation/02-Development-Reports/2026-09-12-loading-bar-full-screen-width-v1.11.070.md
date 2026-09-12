# The loading bar spans the screen

**Version:** 1.11.070
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "make it full screen width"

Following v1.11.069, which put the bar on the floor at 15px with the red-to-green gradient.

## The change

The bar's track is now the surface's own width, and its length is the progress as a **fraction** of
that:

```cpp
const int trackWidth = out.w();
const int fillWidth = static_cast<int>(sgdwProgress) * trackWidth / static_cast<int>(MaxProgress);
SDL_Rect rect = MakeSdlRect(out.region.x, out.region.y + out.h() - ProgressHeight, fillWidth, ProgressHeight);
```

At full progress `fillWidth` is exactly `out.w()`, so the bar reaches edge to edge on any resolution
rather than stopping wherever 534 authored pixels happened to scale to.

## What that deleted

The bar no longer rides the painting in either axis, and that collapsed the whole placement
apparatus:

- **The two branches** of `DrawCutsceneForeground` became one. There was a 32-bit path that mapped
  the bar onto the painting's centred 4:3 core and an 8-bit path that used the UI rectangle; neither
  has anything left to compute.
- **`BarPos`** - the per-screen top-left corner, authored at 640x480 as `{53,37}`, `{53,421}`,
  `{53,37}` - is gone. v1.11.069 had already stopped reading its y; nothing reads its x now.
- **`CutsceneRgbSourceWidth` / `CutsceneRgbSourceHeight`** are gone with it, along with their four
  assignments. Only the bar ever read them, to find the painting's 4:3 core. `CutsceneRgbRect` stays:
  the painting itself is still drawn through it.
- **`GetUIRectangle()`** is no longer called here.

`progress_id` survives all of it, because `BarColor` is still indexed by it on the 8-bit path.

The gradient is untouched: it already spanned the whole track, so "the whole track" simply became
wider and the ramp now reads across the full screen.

## Verification

Debug and Release build clean - with no unused-variable warnings, which is what confirms the removed
globals really were dead rather than merely unreferenced from this function. **718/718** tests pass.
RTM updated.
