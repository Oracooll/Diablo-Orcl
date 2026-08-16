# Third-party components

## Derived small fonts (sizes 11, 10, 9 and 8)

`Packaging/resources/assets/fonts/{8,9,10,11}-*.clx` and `fonts/tr/{8,9,10,11}-00.clx`, together
with the engine support for `GameFont11`/`GameFont10`/`GameFont9`/`GameFont8` and the matching
`UiFlags::FontSize11`/`FontSize10`/`FontSize9`/`FontSize8`, come from a third-party overlay
(`fonts-8-11.zip`) that derives the four smaller sizes from DevilutionX's own Font 12 assets.

The overlay is itself a modification of DevilutionX 1.5.5 (commit
`7223eeac9e8274fbf665b4de86fda26d3b22c52f`). Its notices are reproduced here because its licence
requires that they travel with any copy.

### Licence

The overlay is distributed under the **Sustainable Use License, Version 1.0**. In summary, and
without replacing the licence text itself:

- Use or modification is permitted for personal, internal or non-commercial purposes.
- It may be distributed or provided to others only free of charge and for non-commercial purposes.
- Licensing, copyright and other notices must not be altered, removed or obscured.
- Anyone who receives any part of the software must also receive those terms.
- Modified copies must carry a prominent notice stating that they have been modified.

The full text ships with the overlay archive in the MPQ folder (`fonts-8-11.zip`, `LICENSE.md`).
This project is a personal, non-commercial mod, which is the use the licence permits.

### Modifications made when integrating it

The overlay's own README instructs the reader to copy it over a **clean** DevilutionX 1.5.5
checkout. That was deliberately **not** done here, because three of the four source files it
carries are ones this fork has modified, and a straight copy would have reverted that work:

- `Source/DiabloUI/ui_flags.hpp` — the overlay's copy is stock-based and would have removed this
  fork's added colour bits (`ColorUiGold` through `ColorOrange`, and `ColorOracoolYellow`,
  `ColorOracoolYellowDark`, `ColorOracoolGreen`). Instead the four `FontSize*` flags were added at
  bits 35-38, above this fork's colour bits rather than replacing them.
- `Source/engine/render/text_render.cpp` — the overlay's copy has 19 colour translations; this
  fork has 22 (three added for the yellow focus glow and green set-item text). Only the three
  metric arrays were widened, from six entries to ten.
- `Source/engine/render/text_render.hpp` — `GameFont11`/`10`/`9`/`8` appended after
  `FontSizeDialog` so the original five keep their enum values, and the four flag checks added
  after the existing ones so a caller setting no size flag still resolves to `GameFont12`.
- `CMake/Assets.cmake` — only the new font entries were added. This fork packages Unicode rows
  `1f1`, `1f3` and `1f5` for Fonts 12 and 24 (and `30-e0`) that stock does not, plus the three
  `gamedialog*.trn` files whose absence used to turn every in-game fatal error into a misleading
  font-loading message. Those entries are untouched, and the small sizes list the same extra rows
  so they do not silently lose glyphs the large sizes have.

The derived CLX font data itself is used unmodified.

### Usage

```cpp
DrawString(out, text, rect, { UiFlags::FontSize10 | UiFlags::ColorWhite });
```

`GetLineWidth` and `WordWrapString` accept `GameFont8` through `GameFont11`.
