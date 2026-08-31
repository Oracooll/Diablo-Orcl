# Reading the telemetry back for the first time

**Version:** 1.9.4
**Date:** 2026-08-21

The plan put the telemetry pass first because the CSV had been recording since Phase 0.9 and had
never once been read back. It has now. **It does not yet support a balance pass, and finding that
out is the result.**

## What was there

`build/x64-Debug/Saved_Games/balance_telemetry.csv`, 101 KB, 21 sessions, **915 data rows**: 360
kills, 552 pickups, 3 deaths.

## Three defects, found by reading a write-only file

### 1. The header was written before every row

914 header lines against 915 data rows - the file was twice the size it should be.

```cpp
FILE *file = std::fopen(path.c_str(), "ab");
if (std::ftell(file) == 0) { ...write header... }
```

In **append** mode the stream position is not the file size. MSVC leaves it at 0 until the first
write - the write is forced to the end regardless - so the test was true on every call. Fixed with
an `fseek(file, 0, SEEK_END)` before asking.

Nothing was lost; every row is present and a parser can skip the repeats. But it survived five
months for the obvious reason: **a write-only file is never wrong until someone reads it.**

### 2. 84% of kills had no time-to-kill, and the gap was biased

Only **57 of 360** kills carried a time. The clock started in `M_StartHit`, which is the monster's
STAGGER reaction - a monster killed outright by the first blow never reaches it.

So the missing 303 were not random: **the fastest kills are exactly the ones that skip the stagger**,
which means the headline metric was blank precisely where it would have been most interesting. The
57 that did record average 5.7s with a median of 1.3s, and that median is inflated by survivorship.

The clock now starts in `ApplyMonsterDamage`, where damage lands.

### 3. Debug spawns and real drops were the same row

Pickups by tier read: **484 basic, 16 rare, 13 unique, 39 primal**. An inverted rarity ladder, and
alarming for about a minute - until you remember the session had run `givepset`, `giveitemset` and
`givecharms`. Every debug-spawned item is picked up, and a pickup row cannot tell where the item
came from.

The console now emits a `debug` event naming the command, recorded BEFORE the command runs so a
crashing command still leaves its mark - which is exactly the session you would most want to discard.
The analysis rule is simple: **drop every session containing a `debug` row.**

Tagging the session rather than the item is the honest granularity. What is contaminated is not one
pickup; it is the whole session's economy.

## What the data cannot yet say

Applying that rule to the existing file leaves very little. The 21 sessions are testing sessions -
dungeon levels 1-4, player levels 1-9 plus one debug-levelled 50, three deaths. There is no
progression in it to tune against.

So the numbers this pass was meant to settle are **still estimates**: whether 3% is right for a named
set piece, whether 8% is right for a tier item, whether the deepest runes ever appear in a real
session. The instrument is fixed; it now needs a session.

## What to do next

**One uninterrupted play session with the console untouched**, ideally deep enough to reach
Nightmare, would produce the first usable dataset. Everything in the balance half of the Pipeline
waits on that rather than on code.

Deleting the existing CSV before that run is optional - the `debug` rows make it separable either
way - but a clean file is easier to reason about.
