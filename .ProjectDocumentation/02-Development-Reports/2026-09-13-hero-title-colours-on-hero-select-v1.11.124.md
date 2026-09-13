# Hero title colours on the hero select screen

2026-09-13 — v1.11.124

## Why

> "colors of hero titles in hero select screen are not ok. check them out."

## Cause

The title is drawn in two places: the hero stats panel in play, and the stats column on the hero select screen. Three
of the five rungs wore vanilla file colours — Adventurer `ColorWhite`, Slayer `ColorBlue`, Conqueror
`ColorWhitegold`. Those are `fonts\white.trn`, `blue.trn` and `whitegold.trn`, index remaps that only mean white, blue
and white gold through a **level** palette. The hero select screen draws on the **menu** palette
(`ui_art\diablo.pal`), where the same indices are other colours — the same trap `yellow.trn` fell into on 2026-08-15
(yellow in play, pink in the menus). Champion (`ColorYellow3`) and Sanctified (`ColorBeige2`) were already values and
were not affected.

## Fix

Three new colours defined by **value**, so they read the same on either palette:

| Rung | Colour | Top value |
|---|---|---|
| Adventurer | `ColorTitleWhite` | 244,244,244 |
| Slayer | `ColorTitleBlue` | 140,156,255 |
| Conqueror | `ColorTitleWhitegold` | 242,222,168 |

Each is a 16-shade band on the consumables' recipe (top value held across three shades, then the magic-damage
ramp's ratios). Wiring: UiFlags indices 36–38, three `text_color` entries, `ColorTranslations` 43 → 46 (null — no
file), `RgbDefinedColors` rows, `GetColorFromFlags` cases. `oracool/hero_title.cpp` uses them.

## Tests

`OracoolAudit.HeroTitlesFollowTheHardestDiabloKill` now expects the value colours and fails if any rung wears a
level-palette file colour (White, Blue, Whitegold, Red, Yellow, Black).

## For the user to look at

The hero select screen: select heroes with different Diablo kills — Adventurer white, Slayer blue, Champion yellow,
Conqueror white gold, Sanctified beige. The same colours on the hero stats screen in play.
