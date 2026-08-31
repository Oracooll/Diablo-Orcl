---
title: 2026-08-09 - Rename Warrior to Paladin
date: 2026-08-09
tags: [dev-report, gameplay]
summary: Renamed the Warrior class's display name to Paladin across the game — character creation, character panel, Discord status, and the Warrior-flavored Oracool options — without touching the internal enum, asset paths, or ini keys.
---

# Rename Warrior to Paladin

## Context

First item picked up off [[Idea-Backlog]]'s Tier 1 — Trivial. That entry flagged one open question: whether this stays a pure display-name change, or also implies Paladin-themed abilities once the Skills/skill trees idea exists. Since Skills doesn't exist yet, proceeded with the cosmetic-only interpretation — the only one currently buildable.

## What changed

- [`Source/DiabloUI/hero/selhero.cpp:159`](../../Source/DiabloUI/hero/selhero.cpp) — the class-selection screen's list item: `_("Warrior")` → `_("Paladin")`.
- [`Source/playerdat.cpp:146`](../../Source/playerdat.cpp) — the `PlayersData` table's `className` field for `HeroClass::Warrior`: `N_("Warrior")` → `N_("Paladin")`. This one field is read by three separate consumers that all picked up the rename automatically: the character panel (`panels/charpanel.cpp:126`), Discord rich presence (`discord/discord.cpp:91`), and the multiplayer player-hover tooltip (`control.cpp:1258`).
- [`Source/options.cpp`](../../Source/options.cpp) — updated every user-facing string describing the two Warrior-flavored Oracool options (Furious Charge, Warrior/Paladin Splash Damage Range) to say "Paladin": both the in-game options-menu labels/descriptions and the auto-generated `diablo.ini` comment blocks (including renaming the `; ----- WARRIOR SPLASH DAMAGE -----` comment banner to `PALADIN SPLASH DAMAGE`).

## Why these boundaries

Investigated every other "Warrior" occurrence in `Source/` (22 files) before deciding what to touch:

- **`Source/player.h`'s `HeroClass::Warrior` enum value** — left unchanged. Renaming a C++ identifier used in ~20 internal comparison sites (`_pClass == HeroClass::Warrior`) across `player.cpp`, `spells.cpp`, `monster.cpp`, `missiles.cpp`, etc. would be a large, risky refactor with zero user-visible benefit — the enum's internal name doesn't need to match its display string.
- **`playerdat.cpp`'s `classPath` field (`"warrior"`)** — left unchanged. Confirmed via `player.cpp:2133` that this string builds the actual sprite/animation asset file paths (`plrgfx/warrior/...`). Changing it without renaming the corresponding MPQ asset folders would break sprite loading entirely.
- **The literal ini keys** (`"Furious Charge"`, `"Warrior Splash Damage Range"` as the first constructor argument to each `OptionEntryBoolean`/`OptionEntryIntRange`, and as the `key` parameter passed to the ini-comment-writing lambdas) — left unchanged. These are what `diablo.ini` actually stores on disk; renaming them would silently drop existing users' saved settings for these two options back to default. Only the translatable `N_()`-wrapped display labels/descriptions changed.
- **`itemdat.cpp`'s `N_("Warrior's")` item affix** (e.g. "Warrior's Sword") — left unchanged. This is generic magic-item affix flavor text unrelated to the playable class, present in vanilla Diablo independent of any class system.
- **`textdat.cpp`'s Farnham dialogue** ("Warriors would go to a place...") — left unchanged. Generic narrative use of the word, not a reference to the specific class.
- **Internal code comments** (e.g. `player.h`'s "Warriors/barbarians get between 1/4 and 1/2 life restored per potion") — left unchanged. Not user-facing, and in that specific case groups two separate classes (`Warrior` and `Barbarian`) by shared behavior rather than naming the one class being renamed.

## Verification

- Bumped `ORACOOL_VERSION` to 1.0.2 (per the standing rebuild policy), reconfigured, and rebuilt both `build/x64-Debug` and `build/x64-Release` — both compiled and linked cleanly, only pre-existing unrelated `C4267` warnings.
- Launched `DiabloOrcl.exe`, navigated Single Player → New Hero → Choose Class, and confirmed visually: the class list now reads **Paladin / Rogue / Sorcerer**, and selecting it shows the Warrior's original base stats (STR 30 / MAG 10 / DEX 20 / VIT 25) — confirming the rename changed only the label, not the underlying class.

## Not done / deliberately left alone

No abilities, stats, or skills changed — this was scoped as a pure rename per the backlog entry's recommended interpretation. If Paladin-specific theming (auras, holy-elemental skills) is wanted later, that's a separate, much larger piece of work gated behind the Skills/skill trees idea, not an extension of this change.

## Related

- [[Idea-Backlog]]
- [[2026-08-09 - Project Documentation Overhaul]]
