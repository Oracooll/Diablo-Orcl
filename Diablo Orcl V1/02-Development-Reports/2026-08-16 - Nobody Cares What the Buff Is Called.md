# Nobody Cares What the Buff Is Called

**Version:** 1.7.54
**Date:** 2026-08-16
**Files:** `Source/items.cpp`

---

## The note

> i don;t know what rung means but if it is the name of the extra affixe - hide those. they just take extra rows and nobody cares abot them. we, players, care about the buff not the fancy name for it.

Two things in one sentence, and both land.

**The first is about me.** "Rung" is my word, not the game's. I used it in code, in commit messages, in reports and to the user's face for two days without ever having defined it. It reads as jargon because it *is* jargon — a private metaphor for "one tier of the set bonus ladder" that never made it out of my own head. The comments in `items.cpp` now say "tier". The data structure keeps `SetBonusDefinition`, which was always the honest name.

**The second is about the design.** One version ago the tooltip printed this:

```
  (2) Warmth of the Reliquary
      +15% fire resist, +5 vit
  (3) Procession Unbroken
      +10% all resist, fast hit recovery
```

Two rows per tier, one of which is a proper noun the player has no use for. Ten rows for the Ashen Saint's ladder. Now:

```
  (2) +15% fire resist, +5 vit
  (3) +10% all resist, fast hit recovery
```

Five.

---

## Worth recording, because I nearly defended the names

The names had a real function *in the previous version*, and it would have been easy to argue for keeping them on that basis. When 45 of 73 tiers granted nothing, the name was the only content a tier had — the whole point of the inert-row rule was that a player who earned "Cinderbrand" should at least be told they earned *something*.

But that reasoning expired the moment the tiers got real stats. A name earns its row when it is the only thing there. Once "+4-12 fire damage, +10% to hit" is on the line, "Cinderbrand" is decoration sitting on top of the information, and the player is paying a screen row for it.

The names are **not deleted**. They stay in `item_set_bonus_overrides.txt`, which is organised by them; in the generated table; and in the test failure messages, where "`SET_IRON_ROOT` tier 'Bark of Ages' compiles to nothing" is far easier to act on than "`SET_IRON_ROOT` tier 5". They are a developer's index into the content. They were never player-facing information.

---

## A side effect worth having

The bonus block halved, which finally puts **Leoric's Fallen Court** inside the screen. Thirteen item rows plus seven tiers was 13 + 14 = 27 rows under the item's own block; it is now 13 + 7 = 20. The one-line-per-tier format was chosen in 1.7.53 specifically because two lines each would overflow — this makes it comfortable rather than marginal.

---

## The fallback

```cpp
if (granted.empty())
    granted = _(rung.name);
```

Unreachable with the current data: the generator refuses an empty tier, and `EverySetBonusStatHasText` refuses a tier carrying a stat that renders as nothing. But the symptom if either guard were ever removed would be a bare `  (4)` with empty space after it, and falling back to the name is better than falling back to nothing.

---

## Verification

- Debug build clean at 1.7.54.
- Full suite **441 of 443** — the two failures are the standing baseline pair, unchanged.
- No data or generator changes; this is presentation only, so the 1.7.53 guards and tests all still apply as written.
