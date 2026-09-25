# Fourth /dev batch: gold for the selected, book buttons, Enter, Wirt's rares — v1.12.175

2026-09-25

> check dev notes and process

Ten notes (eight items), coded in full and built once.

## One face for every tab and book button

`shop_grid.cpp`'s vanilla button (the player's own `ui_art\but_sml`, decoded once) now decodes a GOLD
set of its three faces beside the grey — the same luminance through a warm ramp (R 125%, G 98%,
B 42%). `DrawVanillaButton(..., golden)` picks the set; the vendors' tab column and `DrawSideTab` pass
`golden = active`. `DrawVendorButtonBacking(out, rect, selected, hovered)` is the public, flat form for
other windows: gold+lit selected, grey rest under the pointer, grey pressed otherwise; false with no
archive button, so each caller keeps its old drawing as the fallback.

The user asked whether this rule was given before: yes, for the inventory's tab plates on
2026-09-06 (v1.9.290, "grey for inactive, gold for active") — never for the vendors'.

- **Crafting book:** the three book buttons on it, labels shadowed, white on gold.
- **Runeword book:** the ten slot keys on it; a new title row — `TitleButtonRect(i)`, 28×22, 6px
  apart, flush with the Weapon key — holds the chest (the Possible-Runewords toggle, the old yellow X,
  drawn with `DrawTabGlyph` open while lit) and rune-count filters 2–6 (`RuneCountSelected`, an OR,
  applied in `PassesFilters` beside the slot and rune filters).

## Damaging auras on the sheet

`DamagingAuraOnButton` (charpanel): Holy Fire / Freeze / Shock and Sanctuary give their element and
one pulse's range at the invested points (`HolyPulseDamage`, `SanctuaryDamage` — the tooltip's own
numbers). `ReadiedSlotColor` returns the element's colour for both rows; the value row reads
"On, min-max".

## Enter in a shop opens the chat line

`diablo.cpp`: `StoreEnter` only when the store screen is NOT a shop tab; with a tab open Enter falls
to `control_type_message`. The grid's own `ActivateShopGridSelection` stays for any other caller.

## Wirt's rares

`RollBoyShopSlot`: slot % 3 == 1 is `CreateRareVendorItem` (up to twenty tries, then his blue roll),
slot % 3 == 2 Orcl magic gear, the rest his own table.

## Charms get a page

`SortStash` pulls charms out of the gear run into their own vector, sorted by kind then quality, and
seats them on the first empty page after the gear — before the socketables and consumables claim
"the first empty page".

Debug build and tests at the end of the batch.
