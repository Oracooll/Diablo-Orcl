# Rim-and-glow item backings and the tooltip card

2026-09-26 — v1.12.190

## Why

The user showed two Diablo IV screenshots and asked whether the game could look like them:

- an inventory where each item sits on one panel with a coloured rim and an inner glow;
- a tooltip laid out as a card.

After an offline mock (`backing_preview.ps1`), the user said: "build the backings and the tooltips but keep them easily rollback-able, if i dont like them".

The user's colour decisions:

- Plain items get a grey backing. This reverses 2026-08-16's "no backing".
- Uniques stay gold, not Diablo IV's salmon.
- Salmon goes to Primal ("we might use salmon for primals").

## Rollback

Both looks sit behind two new Oracool options, both on by default:

- **Rim and Glow Item Backings** (`itemBackingRimGlow`)
- **Item Tooltip Card** (`itemTooltipCard`)

Turning either off restores the previous look exactly: its code is untouched and runs whenever the option is off. Both are listed in the Settings menu and written to diablo.ini under the ITEM LOOKS heading.

## Backings (`inv.cpp`)

`InvDrawSlotBack` branches first, when the option is on and the surface is 32-bit.

- **`RimGlowHue`** picks the rim colour, in the old precedence order:

  | Item | Rim colour |
  |---|---|
  | While inspecting another player | orange |
  | Runeword | teal `#30C0B0` |
  | Socketed | silver `#C8D0DC` (quiet) |
  | Ethereal | `#A070E0` |
  | Rare | lemon `#F2DC4A` |
  | Buffed Unique / Unique | gold `#D4A23C` |
  | Primal | salmon `#E8826A` |
  | Set | `#4CB050` |
  | Magic | `#4F7BE8` |
  | Plain | grey `#9A9A9A` (quiet) |
  | Gold coins | no backing |

- **`DrawRimGlowBacking`** paints the whole footprint as one piece:
  - a 1 px dark gutter on the edge;
  - a 1 px rim in the hue, with the corners cut;
  - a dark fill in the hue, with a grain hashed from the screen position;
  - a glow fading in from the rim, reaching up to 12 px (a fifth of the smaller side on small items).

  "Quiet" rims (plain, socketed) get a darker fill and a weaker glow. There are no cell lines under an item. The body slots use the same drawing.

## Tooltip card (`oracool/cursor_tooltip.cpp`)

`DrawCursorTooltip` hands item hovers to `DrawCardTooltip` when the option is on. The item printer is untouched: `BuildCard` sorts the printed lines by what they are.

- **Title.** The name in its tier colour, at font 24 when it fits in 250 px, else 12.
- **Subtitle.** The type line, tier and item level, joined with bullets, in Gray5.
- **Headline.** "armor: N" or "damage: a-b" becomes a big number at font 24, the label in 12 on its baseline, and the durability beside it.
- **Body.** Everything else, left-aligned, keeping per-line colours, tails and runs. Socketed stones' lines get a small gold diamond.
- **Footer.** Every "Required…" line, centred in a darker band.
- **Picture.** The item's own sprite, top right (`HoveredCardItem`: container, worn, belt or ground).
- **Plate.** The world behind is kept at 18% under a warm vertical gradient (10% in the footer). A 1 px black edge, a 1 px gold line lit from the top, and a dark inner line. A gold divider fades out at both ends.

The equipped-item comparison cards sit beside the hovered one, as the panels did.

## Verified

- The v1.12.190 Debug build is clean, and the full suite passes 847 of 847.
- `OracoolPreview.DISABLED_ItemBackingsAndCards` (run by name) renders both looks through the game's own code, fonts and sprites into `item_backings_preview.png` and `item_card_preview.png` in the test directory. It showed that `ColorUiSilverDark` draws red in this fork, so the card's grey is Gray5.
- `pcursitem` and `pcursstashitem` are now `DVL_API_FOR_TEST`, so the preview can reset them.
- Not yet seen in the game.
