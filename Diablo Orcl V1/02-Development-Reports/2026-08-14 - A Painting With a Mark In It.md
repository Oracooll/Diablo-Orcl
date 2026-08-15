---
date: 2026-08-14
version: 1.5.28
area: Front end / character screens, background art
---

# A Painting With a Mark In It

> i have delivered two pictures in oracool.mpq. one is to be used as background for hero select. the
> other one has a green circle on it - i want you to make it so that the preview sprites of heros
> land on that spot.

## Reading the mark

Two 1916x821 copies of the same image: a gothic cathedral with a circular stone dais in the
foreground. One clean, one with the spot marked.

The mark was **measured, not eyeballed** - every pixel with G > 90 and G more than 50 above both R
and B:

| | |
|---|---|
| green pixels | 4,324 |
| bounding box | x 898..1015, y 623..668 |
| centroid | **(957, 646)** |

An ellipse (118x46) rather than a circle, because it is a mark on the FLOOR drawn in the painting's
perspective - the middle of the dais. Only the centroid is kept: the ellipse's size describes the
dais, not the character, and reading a scale out of it would be inventing a requirement.

## Getting from art pixels to screen pixels

Backgrounds are cover-cropped and scaled to the window, so a spot measured in the source is not a
spot on screen until it has been through the same transform. Rather than re-derive that arithmetic
next to the original, `CropForScreen` is now reachable as `oracool::MapBackgroundPointToScreen`. A
second copy would have been three lines and would have drifted the first time either changed - and
"the character is somewhere near the dais" is exactly the kind of wrong that is hard to notice.

At 1916x821 the crop is height-bound at every aspect this project targets (full 821 rows used,
cropped horizontally, centred), so the mark lands at:

- **960x720** → (480, 567)
- **1280x720** → (639, 567)

Horizontally the screen's centre, because the mark is one pixel off the source's own centre.

## Landing the figure on it

`DrawHeroPreview` already draws the figure **centred across its area, standing on the area's bottom
edge**. So nothing in the preview code had to learn about backgrounds - the area is aimed instead:

```cpp
Rectangle HeroPreviewRect()
{
    const Point ground = HeroPreviewGroundPoint();
    const int top = HeroPreviewTop();
    constexpr int Width = 440;
    return { { ground.x - Width / 2, top }, { Width, std::max(0, ground.y - top) } };
}
```

It used to be "everything left of the list and its scrollbar" - the right answer for a screen with no
painting, where the figure went in the empty half. The painting has a place for it now, and that
place is the middle.

## The size, which did change

Worth stating plainly rather than leaving to be noticed. Measured from the vault's own sprite
exports, the warrior's south-facing stand has an ink box of **55x82** (union over the animation).
`PreviewScaleFor` takes the largest whole-number scale that fits the area:

| | area height | scale | figure |
|---|---:|---:|---:|
| before | 329 (title+8 → the action row) | 4 | 220x328 |
| **now** | 280 (title+8 → the dais) | **3** | **165x246** |

The dais is 49px higher than the old standing line, and the area's top is already as high as it can
go without running into the screen title - so 3 is the largest scale that both lands on the mark and
leaves the title alone. Verified by compositing a real sprite frame onto the cropped background at
these numbers: feet on the mark, top at y=321 against the title's 237..279, clear of the character
list at x 620 and the button row at y 628.

That is one step down from the size set when the user asked for the preview "twice bigger". It is
recoverable - the constraint is the title, not the mark - by moving the title up or dropping it,
which the painting may not need. Not done here because it was not asked for.

## Also in this pass

- `UiBackground::HeroSelect` stops sharing `hero_settings_bg.png` and gets `ui\hero_select_bg.png`.
  Settings keeps the old painting.
- The temporary "no painting" toggle from 1.5.22 is **gone** - it existed while there was no art to
  show, and there is now. Its one lasting lesson is kept as a comment where it was learned: pinning
  `ui_art\diablo.pal` is the front end's job and only LOOKS like the background's, which is what put
  the black holes back through the logo's fire at 1.5.22 and cost a bug report at 1.5.24.

## Files

- `Packaging/resources/oracool_assets/ui/hero_select_bg.png` (and the Debug build's `assets/ui`).
- `Source/oracool/ui_backgrounds.h/.cpp` - `MapBackgroundPointToScreen`, the new slot path.
- `Source/DiabloUI/hero/selhero.cpp` - `HeroSelectGroundInArt`, `HeroPreviewGroundPoint`,
  `HeroPreviewRect`, and the retirement of the hide toggle.

## Verification

Debug config builds clean at `1.5.26`; full suite **352/354**, the two known pre-existing failures.

Placement was checked by construction rather than by eye - the mock composites the background through
the game's own crop and puts a sprite at the computed rect. The pose in that mock is wrong (the PNG
export's row order is not the direction order I assumed); only the position and size are meaningful.

Not seen in game. Worth confirming: the character stands centred on the dais at both 960x720 and
1280x720, and every class lands at the same spot - a taller or wider class changes the figure, not
where its feet are.

**For a packaged build the asset still needs to go into oracool.mpq.** The Debug build reads the
loose copy in `build\x64-Debug\assets\ui`, which is why this works now without a repack.

## The delete prompt gets the same treatment (1.5.27–1.5.28)

> i like it. now, copy this back ground to delete screen, remove the long text string asking you if
> you are sure and increase the size of the previews to the same as in the hero select and place it
> in the same position. also when you are done, sort the background image where its place is, dont
> leave it in root mpq folder.

**Lower first.** `HeroPreviewGroundNudgeY = 25` puts the feet at y 592 instead of the measured 567.
An offset rather than an edit to `HeroSelectGroundInArt`, so the measurement stays a measurement -
folding it in would leave a number agreeing with neither the art nor the screen, and the next person
to re-measure the ellipse would find a mismatch with nothing to explain it.

**The painting.** The delete prompt takes `UiBackground::HeroSelect` - the SLOT, not a new one, so
the two screens share one cached quantized copy of a 1916x821 image rather than holding two. Safe
because `AddUiBackground` pins `ui_art\diablo.pal` before it quantizes, so every slot is built
against the same palette, which is the only thing a slot caches against. (The enum's old note about
Settings and HeroSelect needing separate slots for different palettes predates that pin.)

**The figure, in one place.** `HeroPreviewRect` and the dais mark moved from selhero.cpp into
`hero_layout.h`. "The same position and size as hero select" is now a fact both screens read from one
definition, rather than two sets of numbers that agree until one is edited. `FigureRect`,
`MessageLineHeight` and `MessageToFigureGap` are gone - nothing on this screen is positioned off how
long a piece of text came out any more.

**The sentence, and the line I did not cross.** "Are you sure you want to delete the character
'X'?" is gone; the title says what the screen is and the figure is the character it means. But the
**name stays**, alone, in gold under the title. Without it this prompt cannot tell two characters of
one class apart, and it is the one screen in the front end where picking the wrong one cannot be
undone. Removing the name was not asked for - it would only have been a side effect of removing the
sentence it was buried in.

That changed the interface rather than parsing around it: `UiSelHeroYesNoDialog`'s second parameter
is `heroName` now and the caller passes `selhero_heroInfo.name` directly. Formatting a sentence only
to pull the name back out of it would have been a parser that worked in English and quietly stopped
working in every other language.

Checked rather than assumed: the name sits at y 303..345 and the figure's head lands at 346 for the
tallest ink measured (82px) and 391..400 for the per-class stand frames sampled from the vault -
clear in every case, so the name is not going to end up behind a helmet.

**Filed.** Both images out of the vault's root and into `02-source-art/ui-backgrounds/`, following
the folder's existing convention:

- `hero-select-background-master-21x9.png` (the clean plate that ships)
- `hero-select-background-master-21x9-ground-mark.png` (the marked copy - kept, because the numbers
  in `hero_layout.h` cite it and a measurement with no source is a magic number)

The vault root holds only `README.md` again.

Debug config builds clean at `1.5.28`; full suite **352/354**.
