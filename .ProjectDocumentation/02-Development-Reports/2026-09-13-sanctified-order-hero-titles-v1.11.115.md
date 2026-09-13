# The Sanctified Order: hero titles for defeating Diablo

2026-09-13 — v1.11.115

## Why

> "we need to distinguish heroes who have defeated diablo in different difficulties. D2 does it nice with
> a certain title after each kill. i suggest we introduce Title stat in hero screen and in hero select
> screen to honour these achievements."

Three sets were proposed; the user chose the Sanctified Order with two names changed: "go with
Sanctified Order, but switch wanderer with adventurer, and exalted with Conqueror. put the title in the
hero stats screen between name and class row."

## The titles

| Hardest Diablo kill | Title | Colour |
|---|---|---|
| none | **Adventurer** | white |
| Normal | **Slayer** | blue |
| Nightmare | **Champion** | rare yellow (YL-3) |
| Hell | **Conqueror** | unique gold |
| Torment | **Sanctified** | primal (BE-2) |

The colours are the item-quality ladder, weakest to strongest, so a player reads the rank the way they
already read loot.

## How

- **No new save data.** `Player::pDiabloKillLevel` already records the hardest difficulty Diablo fell on
  (monster.cpp raises it to difficulty + 1 on the kill), is saved with the hero, and reaches the hero select
  screen as `_uiheroinfo::herorank` (pfile.cpp). The title is derived from it.
- `oracool/hero_title.{h,cpp}`: `HeroTitleFor` and `HeroTitleColorFor`, one table of five rungs. A
  `static_assert` ties the table to `DIFF_LAST`, so a new difficulty cannot ship without its title; a
  kill level past the ladder clamps to Sanctified.
- **Hero stats screen** (`charpanel.cpp`): a Title row between Name and Class. The row list already
  scrolls, so the added row cannot push anything off the panel.
- **Hero select screen** (`hero_stats_column.cpp`): a Title row at the top of the stats column, above
  Class, in the rung's colour. The column already drops its spacing and then its last rows at small window
  sizes, so it still never overflows.

## Tests

`HeroTitlesFollowTheHardestDiabloKill` — every rung's name and colour, Torment as the last rung, and the
clamp.

## For the user to look at

- A hero who has not killed Diablo: "Title: Adventurer" in white on both screens.
- After a Normal kill the next sheet reads Slayer; the hero select screen shows it too.
