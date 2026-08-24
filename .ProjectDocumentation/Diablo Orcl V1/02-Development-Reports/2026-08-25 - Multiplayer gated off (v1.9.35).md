# Multiplayer gated off at both doors (v1.9.35)

**Date:** 2026-08-25
**Version:** 1.9.35
**Tests:** 512/514 — the two standing baseline failures.
**Closes:** external audit findings #1 (P0), #7 (P1), #9 (P1).

## The decision

An external audit found three defects that all live in multiplayer. The user's call: gate the mode
off rather than repair it.

That is the right trade, and #9 is the reason. The first two are memory-safety bugs with known
shapes — bound the parsers, zero the packet — and they could have been fixed in an afternoon. The
third cannot:

> The network item schema carries no sockets, tiers, custom affixes, ethereal state or orb count,
> and player packets carry no tree investment.

An Oracool hero shared over a network would arrive at the other end **as a different character**,
with base items in place of every socketed, runeworded, tiered or orb-fed one. Fixing that means a
separately versioned protocol carrying every field this fork has added since 1.6 — a large piece of
work for a mode the project does not ship. Refusing to enter the mode closes all three at once, at
the door instead of at each parser.

## What was actually done

`oracool::MultiplayerEnabled()`, a `constexpr` policy predicate returning false. Deliberately **not**
the same question as the existing `IsSinglePlayer()`, which reports the current game's mode; this one
is a statement about what the build will do.

Two entry points, because there are exactly two places `gbIsMultiplayer` becomes true:

1. **`InitMultiPlayerMenu` (menu.cpp)** — the UI door. Now shows an explanatory dialog and returns
   to the menu. It returns `true`, which is "this menu is finished, go back"; `false` would end the
   outer loop and quit the game.
2. **`InitMulti` (multi.cpp)** — the door that matters for the memory-safety findings. This is the
   path that calls `RegisterNetEventHandlers` and starts accepting packets. The refusal sits before
   that and before `Players.resize`, so nothing opens a socket and no half-built multiplayer state
   is left behind. **The unbounded parsers the audit found are never reached, because nothing is
   ever received.**

The main menu has omitted Multi Player since the single-player trim, and that was never a gate — it
made the mode unreachable by clicking and did nothing at all about the code behind it. A command-line
argument, a debug command or any future caller could still have walked straight in.

## What was deliberately not done

The multiplayer code is not deleted, and should not be. `gbIsMultiplayer` still exists and the
several dozen `!gbIsMultiplayer` guards across the codebase still read correctly — they describe a
mode that can no longer be entered, which is exactly what they should say if it ever is again.

Tests that set `gbIsMultiplayer` directly are unaffected: this gates the two entry points, not the
flag. 512/514 confirms it.

## Audit findings this does NOT close

Stated so nobody reads "three closed" as "the audit is handled":

- **#2** (crafted or corrupt saves reaching unchecked array indices) stands at full severity. A
  corrupt save needs no attacker, and single-player has plenty of them.
- **#3** (stack quantities destroyed), **#4** (stat points truncated to `uint8_t`), **#5** (gold
  overflow) and **#10** (packaging can omit `oracool.mpq`) are all single-player data-loss paths and
  are next.
- **#6** (waypoint revisit crash on levels 17–24) was fixed at v1.9.34.
