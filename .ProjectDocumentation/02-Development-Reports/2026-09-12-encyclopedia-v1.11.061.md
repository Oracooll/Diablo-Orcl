# The wiki becomes an encyclopedia: portraits, per-difficulty monsters, per-tier items

**Version:** 1.11.061
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "update the wiki with all new changes - area levels, monster levels, item levels, a legend of all
> possible monsters with a screenshot of it, with their stats per level/per difficulty. a legend of
> all possible items with their stats per tier and a picture. i want a complete encyclopedia of
> anything a player can encounter in this game, complete with picture and stats per diffuculty."
> "also add a complete encyclopedia for all possible Skills/Auras/Spells with a description of what
> they do and what gain they get per lvl."

## The art decision

Pictures mean Blizzard art, which [[feedback-no-blizzard-assets-shipped]] forbids committing. The
user was shown that history - the 2026-09-07 reversal, the verbatim-vs-derivative line, the pending
purge list, and that origin is a public GitHub repo - and chose to proceed anyway:

> "making a wiki with private sprites is not a crime as we are not profiting off of this activity, so
> use intelectuall property as you like, regarding wiki. internet is full of WIKI pages with IP
> assets. It is a norm."

So the carve-out is **the wiki only**. oracool.mpq and the release zips are unchanged. The extracts
are deliberately small - one frame per monster family, not the sheets - so the whole encyclopedia's
art is 0.45 MB rather than the ~200 MB that `/sprite-export/` is banned for.

## The pictures

`tools/oracool_art_export.cpp` already existed and already knew how to do this; it simply had never
been run for the wiki. Built as its own target (it is `EXCLUDE_FROM_ALL`) and run for the `monsters`,
`items` and `ui` categories.

A new `tools/BuildEncyclopediaArt.ps1` step cuts what the pages need:

- **Monsters.** The `_n` idle sheet is one row per direction, frames left to right. The frame width
  is `MonsterData.width` and the row height is the sheet height over eight directions - both checked
  to divide evenly before anything is cut, rather than guessed. Row 0 is south, the frame that faces
  the reader. **54 families, 0.12 MB.** Three families (`worm`, `golem`, `darkmage`) have no art in
  these archives and fall back to a label.
- **Items.** The cursor sheets, filed by index. **240 icons, 0.33 MB.**

Two facts worth keeping. **Sprites are per family, not per monster**: all four zombies share
`zombie\zombie` and differ only by palette, so 137 monsters draw on 54 portraits and a family
resemblance in the pictures is the truth. And **the fork's own items have no picture yet**: 271 of
the 452 bases carry `ORACOOL_*` icon names that live in Oracool's own sheets rather than vanilla
`objcurs`, so they are guarded out and show nothing instead of a broken image.

## The data the wiki never had

`tools/BuildWiki.ps1` gained four parses, all read from source rather than transcribed:

| Added | From | Why it was needed |
|---|---|---|
| `art`, `resist`, `resistHell`, special damage, special to-hit | `monstdat.cpp` | per-difficulty resistances and a picture key |
| `monsterScaling` | `monster.cpp`, `options.cpp` | the HP/damage/to-hit/armour/XP/level ladders |
| `desc` on every spell | `oracool/spell_descriptions.cpp` | **never parsed before** - the page could quote a spell's mana cost but not say what it did |
| `skillGains` | `oracool/class_tree.cpp` | the `Scaled(points, base, perPoint)` triples |
| `cursIndex` | `itemdat.h` | items name their icon; the icons are filed by number |

`skillGains` is parsed per `case Skill::X:` precisely because the alternative already exists and has
already failed: `tools/BuildSkillsWorkbook.ps1` transcribed the same table by hand and now states a
cap of 98 where the source says 30. Skills whose ladder is not a `Scaled()` call - the Holy pulses,
Thorns, Cleansing, Conviction, the Paladin's `*PercentAt` tables - emit nothing rather than a guess;
their own description text carries the figure.

## The pages

- **Monsters** - a bestiary under the summary table: portrait, class, floors, and a four-row block
  giving level, hit points, damage, special damage, to-hit, armour, XP and resistances for Normal,
  Nightmare, Hell and Torment. Every number is computed from `monsterScaling` in the page, so a
  retune moves the wiki with it. Resistances follow the real rule: Nightmare demotes Hell's
  immunities, Hell uses them as authored, Torment promotes them but always leaves one school
  un-immune. Torment is shown at the default x2.0 and says so.
- **Items** - an icon column, and a compendium giving every weapon and piece of armour its damage,
  armour, requirements, durability and value at all four tiers, with `ApplyBaseTier`'s own rounding
  (round-half-up, byte clamps, and 255 durability left alone as the indestructible sentinel).
- **Spells** - a "What it does" column.
- **Skills** - a "Gain per level" column.

`tools/BundleWiki.ps1` inlines the new `wiki/encyclopedia/` root ahead of the asset gallery, so a
growing gallery can never push the encyclopedia's own pictures out of the 7 MB budget. The bundle
went from 121 to 415 inlined sprites, 9.85 MB.

## Verification

The in-app browser cannot open `file://` paths and is not signed in to view the private artifact, so
the live page was **not** verified by eye - the user should look. What was checked: every page's
inline script passes `node --check` (the blank-section failure mode), the bundle contains all 54
monster and 240 item data URIs, and the parsed values were spot-checked against source
(`monsterScaling` to-hit 85/120 and armour 50/80, Torment default 2.0, a monster row's resistance
columns read against the raw line in `monstdat.cpp`).

One parse bug was caught this way and fixed: the Torment multiplier regex first matched the
**range's first entry** and reported x1.1 instead of the x2.0 default, because the default sits
between the description and the allowed-value list.

## Still open

- **Icons for the fork's own 271 items** (gems, runes, jewels, charms, the new equipment families).
  Their art is in `oracool_assets` with `*_curs.inc` / `*_icon_specs.txt` cut lists; wiring it is its
  own pipeline.
- **Per-level gains for the non-`Scaled()` skills**, which would mean parsing `aura_field.cpp` and
  the Paladin `*PercentAt` tables.
- The `runewords` reader still parses nothing (pre-existing; empty at HEAD too).
