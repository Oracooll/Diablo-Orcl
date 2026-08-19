# Three Icons Refreshed, and the Plate That Is Not a Drop-In

**Version:** 1.8.38
**Date:** 2026-08-19
**Tests:** 470 total, 468 passing. The two standing baseline failures only. No engine change - this
is assets and one new tool.

## Unit B shipped. Unit E did not, and the reason is measured rather than guessed.

Both were asked for in one go. One of them turned out to be a different size than its entry, and this
time the difference was found by measuring the artwork before writing any code.

## Unit B: the three-state strips

The drop zone (`..\Oracool.MPQ`, a sibling of the repo - which is why earlier searches inside the
repo found nothing) holds three packages, each shipping its states as separate 1254x1254 masters. The
engine wants each as ONE strip of three cells indexed `state * cellWidth` along a single row:

| Asset | Cells | Package states |
|---|---|---|
| `ui\burger_menu_button.png` | 3 x 27x29 | inactive / hover / active |
| `ui\town_portal_icon.png` | 3 x 27x29 | inactive / hover / click |
| `ui\level_up_icon.png` | 3 x 60x61 | inactive / active - **two, not three** |

`tools\CutHudStateIcons.ps1` unpacks, fits and writes all three, to both asset trees.

**The common-box rule is inherited, not reinvented.** `CutLevelUpIcon.ps1` found this the hard way
and wrote it down: a glowing state's detected bounds are wider than the plain one's, so fitting each
state through its own box gives them different scale factors and the icon visibly twitches the
instant the cursor touches it. One box - the largest of the three, re-centred on each master's own
centre - one scale factor, one landing pixel.

Aspect is preserved and the art is centred in its cell rather than stretched to fill it. The masters
are nothing like the cells' shape: the portal content is 787x1189, taller than it is wide, against a
27x29 cell. Stretching would have been a visible distortion sold as a fit.

Level-up has two masters for three cells, so it takes the same mapping the old script chose, for the
same reason - the glow IS the hover cue:

```
state 0 (idle)    <- inactive
state 1 (hover)   <- active
state 2 (pressed) <- inactive
```

**This supersedes `CutLevelUpIcon.ps1` for that asset**, and both scripts now say so. Running the old
one afterwards would silently revert the level-up icon to the previous art at the same size with no
error - exactly the hazard that script already documents about `HudIconCut.cs`.

### Verified by looking at it

Rendered at 6x and compared against the art it replaced, rather than trusting that the numbers came
out. The portal reads as a ring of blue energy brightening through its three states; the level-up
plaque lights from dull red to a lit glow; the burger's three bars go dark, then pale, then red - and
they are noticeably **bolder** than the icon they replace, filling the cell width where the old art
had margins. That is what the artist drew, so it stands.

## Unit E: measured, resized, not shipped

The package is in the drop zone and it is not a drop-in replacement. Measuring it:

- Its content occupies **1497x297** of the 1536x1024 canvas - aspect **5.04**.
- The current `middle_hud.png` is **356x64** - aspect **5.56**.
- The master is 24bpp RGB on black, so it needs keying as well as scaling.

The aspect alone would only mean a squash. The real cost is one layer down:

`DrawMiddleHudArt` blits the asset **1:1** at its own size, and `GetMiddleHudRect` returns the
constant `PlateScreenSize`. Every element on the plate - the LMB and RMB skill wells, the six belt
cells, the Menu and Portal boxes - is placed by `ScalePlateRect` mapping **source-art coordinates**
into that rect. The limestone artwork's slots are not where the old artwork's slots were.

So Unit E is three jobs, not one:

1. crop, scale and key the new plate,
2. change `PlateScreenSize` to the new aspect,
3. **re-derive all eight slot rects from the new artwork.**

Step 3 is the one that matters. Those rects are not decoration - they are where the HUD hit-tests
clicks. Getting them wrong moves the skill buttons and belt slots away from where they are drawn,
which is the exact failure class this project has already paid for twice, and which no test can
catch: the constants would compile, the suite would stay green, and the HUD would be visibly and
unclickably wrong.

Only a screenshot proves it right, and a screenshot needs the user to run the game. Shipping an
unverified rewrite of the HUD's input geometry at the end of a long session is a bad trade, so the
entry is **resized Small -> Medium** with all three steps and the measurements written into it. It is
one focused session from done, and it should start from those numbers.

The package's standing note still holds and is recorded: *do not add a procedural bottom offset* -
the artwork is already screen-bottom aligned.

## A blocker that did NOT clear

Last session I flagged that the burger-menu pack might unblock the Directive entry *"retire the
standalone Crafting window"*, which waits on a `menu_icons.png` recut. It does not. The pack contains
the burger **button** in three states, not the menu's row icons. That entry stays blocked.

## Files

- `tools/CutHudStateIcons.ps1` - new; rebuilds all three strips from the packages.
- `tools/CutLevelUpIcon.ps1` - superseded note.
- `Packaging/resources/oracool_assets/ui/` and the `assets/ui` mirror - three PNGs.
- `Diablo Orcl V1/07-Backlog/Pipeline.md` - Unit B shipped, Unit E resized. 34 rows.

## To look at

The burger button and the portal cell sit in the belt; hover each and press it, and the three states
should be distinct without the icon shifting by a pixel between them. That last part is the whole
point of the common box, and it is the thing that would look like a rendering bug rather than a
design choice. The level-up plaque appears under the clock when attribute points are unspent.
