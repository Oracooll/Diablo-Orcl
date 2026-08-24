# Reading the telemetry back, a second time (v1.9.33)

**Date:** 2026-08-25
**Version:** 1.9.33
**Tests:** 512/514 — the two standing baseline failures.
**Status:** analysis only; one comment block and two documents changed.

Asked to read the balance telemetry back and tune from it. I read it. **It still cannot support a
balance pass, and the reason is not the instrument — it is that nothing has been played into it.**

## The headline

The file was read back once before, on 2026-08-21 (v1.9.4), which fixed three defects and concluded
it needed "one uninterrupted play session". Since that read the file has gained **one row**: a debug
marker written on 2026-08-23.

    balance_telemetry.csv   103 KB   916 data rows   22 sessions
    at the v1.9.4 read:               915 data rows

So this pass measured the same data as the last one, plus a marker.

It also confirmed the fix landed: the time-to-kill clock now starts in `ApplyMonsterDamage`
(`monster.cpp:3934`), not in the stagger reaction. The instrument is sound and wired. It has simply
never been fed.

## What the data can and cannot say

| | |
|---|---|
| Kills | 360 — but **all predate the 21 Aug clock fix**. 16% carry a time, and the missing 84% is missing in a *biased* direction: a one-shot kill never reached the old start point, so the fastest kills are exactly the absent ones. Unusable. |
| Pickups | 552 raw. After cleaning: **46 real items** plus 91 gold rows. |
| Deaths | 3. Burning Dead (dlvl 2), Shadow Beast (dlvl 3), Firebat (dlvl 21 at plvl 8). |
| Depth | dlvl 1–4, plus one dlvl 21 excursion. Player level 1–9, plus a debug-levelled 50. |

Forty-six items cannot measure a 5% tier rate. Two of them were above basic. That is a sample where
a single lucky drop moves the estimate by two percentage points.

## A second cleaning rule, and why rule one missed it

The v1.9.4 pass established rule one: **drop any session containing a `debug` row**, because what a
console command contaminates is the economy rather than one item.

That rule flagged exactly one session here. It should have flagged more:

**415 of 552 pickups are at dungeon level 0 — town.** 344 of those are gems and runes in even counts
of four per grade, bench-spawned to test the socket system. They were not conjured straight into the
pack, so no `debug` row exists for them; they were dropped on the town floor and collected, which
goes through `InvGetItem` like any other ground pickup. Both telemetry call sites are legitimate —
this is not a code defect.

The rule that catches them is about the data, not the session: **nothing drops in town, so a `pickup`
at `level == 0` was put there by the player.** Without it the drop sample is three quarters noise and
reads as an inverted rarity ladder — which is precisely the wrong conclusion, and the same shape of
wrong conclusion the debug rule was invented to prevent.

Both rules are now written into `telemetry.h` beside the column list, along with the date before
which time-to-kill is unusable. Deriving them twice is the failure this records against.

## The settings that produced the data are not the settings now shipped

Worth knowing before anyone reaches for these numbers. The `diablo.ini` beside the build during
those sessions ran:

    Rare Item Drop Chance=6        Buffed Unique Item Drop Chance=3      Monster Density=200

The file supplied on 2026-08-24 and made default at v1.9.32 says:

    Rare Item Drop Chance=20       Buffed Unique Item Drop Chance=5      Monster Density=100

Three of the five loot knobs differ, one of them by more than 3x. So even a clean sample from these
sessions would be measuring a configuration nobody is shipping or playing.

## What would make this answerable

One session, at depth, on a build from v1.9.4 or later, without console commands. Nothing else. The
row that says so has been corrected — it claimed the file "has never been read back", which stopped
being true four days ago.
