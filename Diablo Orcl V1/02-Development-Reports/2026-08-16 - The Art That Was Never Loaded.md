---
date: 2026-08-16
version: 1.7.29
area: Assets declared but never registered - the stash background, and five class tree strips
---

# The Art That Was Never Loaded

User: *"did you import the stash background? i dont see it"*.

They were right, and chasing it turned up something considerably worse than a missing background.

## The stash

`hud_art.cpp` keeps its assets in four hand-written passes: load, a "does anything still need
quantizing" check, quantize, and reset-on-palette-change. Each names **every asset individually**.

I added `StashPanelArt`, wrote `DrawStashPanelArt` and `HasStashPanelArt` around it, and never added
it to any of the four. So its pixels were never loaded, `HasStashPanelArt()` was permanently false,
and `DrawStash` quietly took the fallback branch and drew the procedural theme — which looks
perfectly fine, which is exactly why nothing announced itself.

## The part that was worse

Sweeping every `ArtAsset` against the four passes to see whether anything else had drifted turned up
**five of the six class tree strips**: Barbarian, Sorceress, Rogue, Bard and Monk. Only the
Paladin's was ever registered.

`DrawStripIcon` opens with `EnsureLoadedAll()` and then `if (asset.rgba.empty()) return;`. Those
five strips were always empty, so five of the six classes have been drawing skill **plates with no
icons on them** — since v1.7.15, when the tree was generalised past the Paladin.

It stayed invisible for the same reason the stash did: the failure mode is a silent early return,
and a missing strip is indistinguishable from a skill that simply has no icon. The Paladin worked,
and the Paladin is the class everything gets tested on.

## The fix, and the shape of it

Not five more lines in four more places — that is the mistake, repeated. The six strips are now one
array that the four passes walk, exactly as `SilhouetteArt` has always been handled:

```cpp
ArtAsset *const ClassTreeStrips[] = {
    &PaladinTreeIconsArt, &BarbTreeIconsArt, &SorcTreeIconsArt,
    &RogueTreeIconsArt, &BardTreeIconsArt, &MonkTreeIconsArt,
};
```

Adding a seventh class is now one line in one place instead of four lines in four. `StashPanelArt`
is registered in all four passes alongside `InventoryPanelArt`.

## The sweep is repeatable

The check that found this is worth keeping in the toolbox, because the failure is silent by
construction — every declared `ArtAsset` must appear in the load, quantize and reset passes, whether
by name or through a group array. Re-running it now reports only `SilhouetteArt`, which is a false
positive: it is an array walked by `for (ArtAsset &silhouette : SilhouetteArt)` in all four places.

While there, `SilhouetteForClass` was confirmed sound for all six classes — Bard shares the Rogue's
figure (they share a sprite set), Monk deliberately has none and returns nullptr, which draws
nothing rather than indexing off the end of a four-entry array.

## Verified

**418 tests, the same two pre-existing failures.** The tests could not have caught either bug: both
live in the render path, and the suite runs `HeadlessMode`, under which none of this art loads at
all.

Worth looking at in play: the stash background should now be the painted panel rather than the
translucent theme, and the Barbarian, Sorceress, Rogue, Bard and Monk trees should have icons on
their plates for the first time.
