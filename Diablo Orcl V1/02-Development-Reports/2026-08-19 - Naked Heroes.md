# Naked Heroes

**Version:** 1.7.98
**Date:** 2026-08-19
**Tests:** 445/447 (the two standing baseline failures)

A new game-wide rule: heroes start with nothing at all.

## The rule

No weapon, no shield, no armour, no potions, no gold. Both mouse buttons on the bare fist.

INI-toggleable as `Naked Heroes` in the Oracool block, **ON by default**.

## Where it lives

One early return in `CreatePlrItems` (`Source/items.cpp`). Everything above that point already clears
the body, the grid, the inventory and the belt, so being naked is simply stopping there - before the
per-class gear switch, before the two healing potions, and before the 100-gold stack that the switch
is followed by.

The fists need no code. `CreatePlayer` has readied nothing on either button since the six vanilla
class skills were retired in 1.7.97, and `oracool::BasicAttackIcon` already reports **Fist Attack**
rather than Regular whenever the hands are empty - which they now are, from the first frame.

## Read once, at creation

Deliberately. The option decides what a character was *born* with, not what they are allowed to
carry: turning it off later re-equips nobody, and turning it on strips nobody. That also keeps it out
of every hot path - one read, at the one moment it means anything.

## Files

- `Source/options.h` / `Source/options.cpp` - the entry, its ini comment block and its place in the
  Oracool list
- `Source/items.cpp` - the early return in `CreatePlrItems`
- `test/player_test.cpp` - `AssertPlayer` now expects an empty bag, an empty belt, an empty body and
  zero gold
- `test/loadsave_test.cpp` - that fixture turns the option OFF in `SetUp`: they are save round-trip
  tests and need a real item to tag and reload

## To look at in game

Roll a new hero of any class. Empty everywhere, both wells showing the fist, zero gold on the
character sheet. Then set `Naked Heroes=0` in diablo.ini and roll another - the vanilla starting kit
comes back for that character, and the naked one stays naked.
