# The played configuration becomes the default (v1.9.32)

**Date:** 2026-08-24
**Version:** 1.9.32
**Tests:** 512/514 — the two standing baseline failures.
**Status:** built, not played. Applies to future releases; v1.9.31 as shipped still has the old defaults.

The user supplied their working `diablo.ini` and asked for it to become the shipped default.

## Method: diff, do not transcribe

The file has 230 settable keys. Transcribing it would have meant changing 230 defaults, most of them
to the value they already had, and quietly reclassifying session state as configuration. Instead
every key was matched against its declaration and only the differences were touched.

**Nine options actually differed.** Everything else in that file — including all eighteen customised
keybindings — was already the shipped default.

| Option | Was | Now |
|---|---|---|
| Auto Oil Pickup | off | **on** |
| Adria Refills Mana | off | **on** |
| Griswold Premium Ignore Affix Level Limits | on | **off** |
| Griswold Premium Ignore Price Limits | on | **off** |
| Hellfire Intro | Once | **Off** |
| Auto Pickup Range | 5 | **3** |
| Unique Item Drop Multiplier | 25 | **5** |
| Buffed Unique Item Drop Chance | 10% | **5%** |
| Primal Item Drop Chance | 5% | **1%** |

The last four matter most: the shipped loot defaults were **five times more generous** than what the
game is actually being played and tuned at. Anyone starting from the defaults was playing a
different game from the one being balanced.

The two Griswold Premium "ignore the limits" switches went the other way — they were on by default,
which quietly disables the affix-level and price ceilings for everybody. Off is the honest default;
they remain available.

## What was deliberately not copied

- **Session state**, not settings: `LastSinglePlayerHero`, `SItem`, `Previous Game ID`,
  `Previous Host`, the audio `Device`, the controller `Mapping`.
- **The `[NetMsg]` quick messages**, which are the user's own Bulgarian phrases. V1 is single-player,
  so they are unreachable anyway, and the shipped English defaults are the right thing for a build
  other people run.

## One thing I was wrong about on the way

`Sound Volume=0` and `Music Volume=0` read like a muted machine, and I was ready to flag shipping a
silent game. They are already the default: `VOLUME_MAX` is **0** and `VOLUME_MIN` is -1600. Checking
beat assuming by a wide margin.

## Verification

The same comparison that found the nine was re-run afterwards and reports every boolean and integer
option matching the supplied file. That is a check of the declarations rather than of a generated
ini — writing one would mean starting the game, which is not done here.

Screen size, audio rates, game speed, gamma, controller deadzone, the intro/splash modes, the farmer
quest mode and floating-number style were all confirmed to match already.
