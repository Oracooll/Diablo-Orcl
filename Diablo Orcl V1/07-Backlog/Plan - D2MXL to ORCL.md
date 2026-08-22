# Plan — D2MXL to ORCL

Adopting Median XL's content mechanisms into Diablo Orcl V1.

**Written:** 2026-08-22, at v1.9.18
**Status:** Phase 1 planned to implementation level; Phases 2–4 scoped.

---

## The idea being borrowed

Median XL's insight is that D2's endgame reduces to "farm the same bosses for a fixed unique list".
It replaces that with **many small permanent decisions** — things you apply to items, things you
earn once, challenges that pay a specific known reward.

That is the part worth taking. Not the numbers, not the skill trees (we have our own), and not the
item inflation — the *mechanisms* that make gear you already own into a project.

**A caution recorded up front:** this plan is written from knowledge of the mod, not from a copy of
it. Median XL has changed enormously across its versions, and any specific value here is our own
tuning rather than a quotation. Where a number appears it is a starting point for the telemetry to
correct, not a fact about another game.

---

## What we already have, so is not in this plan

| Median XL | Ours |
|---|---|
| Full skill-tree overhaul | class trees — 161 skills across 6 classes |
| Sacred tier above elite | `BaseItemTier` — Normal/Nightmare/Hell/Torment on every base |
| Expanded runewords | 370 runewords, 33 runes |
| Harder difficulties, denser monsters | the difficulty ladder (v1.9.16) and the density dial |
| Token respec | `RespecCost`, priced in gold |
| Cube recipe sprawl | 17 recipes on Levski's Roar |

## What is deliberately excluded

- **Oskills** (items granting any class's skills). Doable, but it fights the class trees, which are
  deliberately about class identity.
- **MXL's crafted-affix recipes.** We shipped the reroll ladder at v1.9.18; this would need a whole
  parallel affix table before it added anything.
- **Removing stat requirements.** That is MXL compensating for its own item inflation. We do not
  have that problem, and Hel already sells requirement relief.
- **Wholesale skill replacement.** Done, our own way.

---

# Phase 1 — Mystic Orbs

The highest content-per-line mechanism on the list, and the one that changes what a player does with
the gear they already have.

## What it is

A consumable that adds a **fixed small stat** to an item — +3 strength, +5 fire resist, +2% life
steal — with a **hard cap on how many any one item can take**. You find orbs; you decide which item
deserves them.

Three properties make it work, and all three matter:

1. **The cap is per ITEM, not per orb type.** Six orbs into one weapon and it is finished; the
   seventh has to go somewhere else. That is what makes it a decision rather than an accumulator.
2. **Small and fixed, never rolled.** An orb is a known quantity, so the player can plan. A rolled
   orb is just another affix.
3. **Permanent.** No undo. The reroll recipes already exist for people who want to gamble; orbs are
   the opposite of gambling.

## Why this engine can carry it

- `ApplyItemPower(player, item, power)` (items.cpp) is already the public door onto `SaveItemPower`,
  written for exactly this reason — the item sets use it so their stats land in the same fields,
  with the same signs and flag semantics, as every other item's. An orb is a fourth caller.
- The salvage materials already have a drain (the recipes), so orbs get a *second* sink of a
  different shape rather than being the only use for anything.
- Levski's Roar exists, has a selectable recipe list, and has room for one more.

## The save cost, stated plainly

An orb count has to live on the item, and there is nowhere derived to put it — unlike the monster
variants or the boss traits, this is a player decision and cannot be recomputed from a seed.

`OracoolItemFormatVersion` is **8** and is an explicitly versioned extension record with an
exact-match check that refuses stale saves cleanly. Adding one byte takes it to **9**.

That is cheap *here specifically*: V1 always starts a New Game, so no existing character has to
migrate. It is still the one irreversible decision in this phase and should be paid once — see
Phase 3, which wants the same byte.

## Design

### The orbs

Start with **eight**, one per stat channel that already exists and reads clearly:

| Orb | Grants | Notes |
|---|---|---|
| Orb of Might | +3 Strength | |
| Orb of Grace | +3 Dexterity | |
| Orb of Insight | +3 Magic | |
| Orb of Vigour | +3 Vitality | |
| Orb of Warding | +5 to all resistances | the one most likely to need tuning down |
| Orb of Fury | +2 damage | flat, so it reads on any weapon |
| Orb of Fortune | +5% magic find | |
| Orb of Avarice | +8% gold find | rides `IPL_GOLDFIND`, added v1.9.5 |

Generated the way every other family here is — `tools/GenMysticOrbs.ps1`, one walk emitting ids,
table rows, cursor ids, frame sizes, icon specs and the effect table, so they cannot drift. Same
discipline as GenRunes/GenJewels.

### The cap

`MaxOrbsPerItem = 6`, stored as one `uint8_t` on `Item`.

Six because it is the same number as `MaxItemSockets`, and because a cap the player can hold in
their head is worth more than a tuned one they cannot.

### Applying one

**Ride the existing paste path.** `inv.cpp:671` already routes a gem dropped onto a socketed
backpack item into `TrySocketGem` — the whole insertion UI, riding the paste path's own target
resolution. An orb dropped onto a backpack item takes the same seam.

That gives us, for free: backpack-only (so orbing worn gear means carrying it first, the same beat
of friction socketing has), correct target resolution, and no new window.

```
TryApplyMysticOrb(Item &target, const Item &held) -> bool
    reject unless held is an orb
    reject unless target is weapon or armour
    reject if target._iOracoolOrbCount >= MaxOrbsPerItem
    ApplyItemPower(*MyPlayer, target, orbPowerFor(held.IDidx))
    target._iOracoolOrbCount++
```

### Showing it

One line in the item description, under the tier: `Mystic Orbs: 3 / 6`. The player needs to know
what is left before they spend, and the description panel is where this fork explains itself.

### Where orbs come from

**Not a new drop family.** Two sources, both existing:

1. **The socketable draw.** A fifth family beside gem/rune/jewel/charm in the treasure classes, so
   which zone favours orbs is a table entry rather than a new hook.
2. **A Levski's Roar recipe** — 8 White Scales → 1 random orb. White Scales come from salvaging
   plain items, which is the material with the weakest sink today.

## Verification

- Orbs applied past the cap are refused, and the refusal says why.
- The count survives a save/load round trip — `Writehero.pfile_write_hero` must move, and that is
  expected exactly once, at the format bump.
- Every orb's power reaches `ItemBonusTotals` through the ordinary equipment provider, asserted by
  equipping an orbed item rather than by reading the field back.
- Every orb in the enum is reachable from the drop table (the dead-code-wearing-a-name check that
  found gaps in the jewels and the boss traits).
- An orb cannot be applied to a socketable, a potion, or gold.

**Needs eyes:** the description line and the paste interaction. Neither is verifiable from tests.

---

# Phase 2 — Signets of Learning and Milestone Challenges

Two mechanisms, one unit, because they share their storage and **neither touches the item format**.

## Signets of Learning

A rare drop granting a **permanent character stat point**, with a lifetime cap (start at 20).
Progression that survives your gear entirely.

Per-character counters belong in `PlayerPack` — the memory note is explicit about this, and the pack
already carries the readied-spell encoding and the per-difficulty waypoint bitmask in repurposed
inert bytes. A signet count and a spent-total is the same pattern.

The cap is the whole design. Without it this is just a slower level-up; with it, it is a finite
resource you can exhaust and then must live with.

## Milestone challenges

"Reach level 30 and kill a Dread boss → this reward." A bitmask in `PlayerPack`, same as the
waypoint unlocks.

Value: it gives the levelling curve punctuation, and it gives the endgame bosses (v1.9.14) a reason
to exist beyond their treasure class.

---

# Phase 3 — Growing Charms

A charm that gains stats as milestones are met, rather than being fixed when it drops.

We have charms *and* the active-cap rule that makes charm choice interesting, so a charm that grows
competes for a slot that already matters.

**Rides Phase 1's byte.** This needs per-item state for exactly the same reason orbs do, and it
should not pay a second format bump — which is the argument for planning it now even though it is
built later.

---

# Phase 4 — Named Encounters with Fixed Rewards

Median XL's uberquests: a specific hard fight, in a specific place, with a **known** reward. This is
what turns farming into a destination rather than a lottery.

Most of the machinery landed in the last week — endgame bosses, per-zone treasure classes, TRN
tinting, the lesser-affix pools. What is missing is the fixed-reward half and somewhere to put it.

Scoped last because it is the only phase here that wants new *content* (a place, a fight, a named
item) rather than new *mechanism*.

---

## Order, and why

1. **Mystic Orbs** — pays the format bump, and is the mechanism that changes the most about ordinary
   play for the least code.
2. **Signets + challenges** — no format cost at all, and they cover the progression axis orbs do not.
3. **Growing charms** — reuses the byte from 1 and the milestones from 2, so it is cheapest last.
4. **Named encounters** — needs content, and benefits from having 1–3 as rewards to hand out.

## The standing risk

Every phase here adds a permanent, non-undoable player decision. That is the point of the mechanism
and it is also the way to make a game frustrating. The telemetry has been collecting since Phase 0.9
and has still never been read back against a real session — **these are the numbers it exists to
correct**, and the orb cap in particular should be treated as provisional until it has been.
