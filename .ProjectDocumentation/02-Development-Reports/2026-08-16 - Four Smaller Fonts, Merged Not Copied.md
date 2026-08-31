---
date: 2026-08-16
version: 1.7.18
area: Integrating the derived font pack (sizes 11, 10, 9, 8)
---

# Four Smaller Fonts, Merged Not Copied

`fonts-8-11.zip` from the drop zone adds the four font sizes the game has never had — it jumps
from 12 straight to 24 — which is exactly what the dense panels this fork keeps building have
been short of. The user approved its licence, so it is in.

## Why it could not be installed as instructed

The overlay's README says to copy it over a **clean** DevilutionX 1.5.5 checkout. This tree is not
clean, and three of the four source files the overlay carries are ones this fork has modified.
Copying would have silently reverted real work. Measured before touching anything:

- `ui_flags.hpp` — the overlay's copy is stock-based and would have removed the colour bits this
  fork added (`ColorUiGold` through `ColorOrange`, plus the three Oracool colours at bits 32-34).
- `text_render.cpp` — the overlay has 19 colour translations; this fork has **22**, the extra three
  being the yellow focus glow and the green set-item text that the injected palette ramp depends
  on. It would also have dropped the `engine/palette.h` include and the `ColorOracoolYellow`
  handling.
- `CMake/Assets.cmake` — 18 of our lines were absent from the overlay's version, including the
  extra Unicode rows this fork packages and the three `gamedialog*.trn` entries whose absence once
  turned every in-game fatal error into a misleading font-loading message.

So this was a **merge**, taking only what the pack adds:

- `GameFont11/10/9/8` appended after `FontSizeDialog`, so the original five keep their enum values.
- The three metric arrays widened from six entries to ten. Nothing else in that file moved.
- `UiFlags::FontSize11/10/9/8` at bits **35-38** — above this fork's colour bits rather than
  replacing them, and above the contiguous `FontSize12`-`FontSizeDialog` block at bits 0-5, which
  several places mask as a unit.
- The four flag checks added **after** the existing ones in `GetFontSizeFromUiFlags`, so a caller
  that sets no size flag still resolves to `GameFont12` exactly as before.

One deliberate improvement on the pack: it lists the same Unicode rows stock packages for Font 12,
but this fork also ships rows `1f1`, `1f3` and `1f5`. The small sizes now list those too — the CLX
files were in the archive, just unlisted — so the new sizes do not silently lose glyph rows the
large ones have. 56 base files plus 4 Turkish overrides, all staged into the build.

## The licence

The pack ships under the Sustainable Use License 1.0: personal and non-commercial use, notices
preserved, terms travelling with any copy, modifications declared. `docs/THIRD_PARTY.md` records
all of that along with the exact integration changes above, which is what the "prominent notice
stating that you have modified the software" clause asks for. This repository is public, so that
file is how the terms reach anyone who takes a copy.

## Verified

**409 tests, the usual two pre-existing failures.** One new test pins the part a merge is most
likely to get wrong: every original size flag still resolves to the font it always did, each new
flag resolves to its own font, no size flag still means Font 12, and the new bits do not collide
with this fork's colour bits.

The fonts are wired but not yet *used* anywhere — nothing in the UI asks for `FontSize10` yet.
That is the natural follow-up: the tooltips, the tree's point counters and the item labels are all
places where a smaller face would buy back space.
