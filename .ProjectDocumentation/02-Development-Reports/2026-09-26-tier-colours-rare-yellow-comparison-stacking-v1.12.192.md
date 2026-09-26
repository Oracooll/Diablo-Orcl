# Tier colours, the rare yellow's dots, stacked comparisons, half-transparent backings

2026-09-26 — v1.12.192

## What was asked

Three dev notes, archived with their outcomes, and one request in chat: "tier hell text should be gold, tier torment text should be salmon".

## What changed

- **Tier colours** (`oracool/item_tiers.cpp` `TierColor`):
  - Hell is `ColorGold6`, the books' gold, defined by value. It was `ColorUiGold`, the MENU palette's gold file, which drew blue in play; the preview render showed "Tier Hell" in blue on both the card and the old panel.
  - Torment is the new `ColorSalmon`.
- **`ColorSalmon`**: a new value colour at index 39. It needed entries in:
  - `ui_flags.hpp`;
  - `text_render.hpp`;
  - the translation table (now 47 entries);
  - `RgbDefinedColors` (salmon `0xE8826A` on the consumables' recipe);
  - the UiFlags-to-colour switch.
- **The rare yellow's ramp** (`ColorYellow3`): the bottom six of its 16 shades were all `0x191900`, which gave the black dots in rare names. The ramp is now on the recipe: the top held across three shades, then even steps down. Every rare name in the game changes, not only the card.
- **Comparison panels** (`oracool/cursor_tooltip.cpp` `PlaceComparison`) try right, then left, then under and over each equipped panel already placed. They take the first spot with no overlap, else the least overlap. Both the card and the old panel use it.
- **Backings at 50%** (`inv.cpp` `DrawRimGlowBacking`): every pixel is blended over the slot at `OpacityPercent` = 50.

## Verified

- The v1.12.192 Debug build compiles cleanly. Two test programs were blocked by Smart App Control and relinked.
- The full suite passes, 847 of 847.
- The `OracoolPreview` render shows:
  - the rare name in clean yellow;
  - "Tier Hell" in gold;
  - the half-transparent backings.
- Torment's salmon was not in the preview sample, and nothing here has been seen in the game yet.
