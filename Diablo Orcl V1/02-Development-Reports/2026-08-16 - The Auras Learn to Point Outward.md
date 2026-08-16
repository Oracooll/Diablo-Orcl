---
date: 2026-08-16
version: 1.7.24
area: Megaplan Phase 3.4 - the monster-facing aura pass
---

# The Auras Learn to Point Outward

Every aura in this fork so far points **inward**. It reaches the character sheet through the bonus
providers and never touches anything else. A whole family of skills points the other way —
Conviction strips the enemy, Sanctuary repels the undead, the warcries frighten them — and every one
of those rows has been sitting inert across four class trees, each with the same sentence in its
description: *"it needs the monster-facing pass."*

This is that pass.

## The design decision that is the whole unit

The obvious implementation walks nearby monsters each tick and **writes to them**: drop their
resistance, raise their armour, set a flag.

That is a trap this project has already paid for twice. `Monster::resistance` and its neighbours are
**saved**. Anything written into them has to be un-written when the aura is switched off, when the
player walks away, when the character dies, when the level unloads, when a save is reloaded
mid-field — and every one of those is a place to forget. The vault's own audit checklist exists
because of exactly this class of mistake: *where does it live, is it saved, does it hold still.*

So **nothing is stored**. An aura field is computed *from* the player's live state — which aura is
lit, how many points are in it, where the player is standing — at the moment a question is asked.
Walk out of range and the answer changes by itself, because there was never any state to restore.

It is the same move the bonus providers made for the character sheet, pointed the other way.

The one exception is honest and marked as such: **repulsion has to push**. There is no "point of
use" at which to ask a fleeing monster a question, so Sanctuary runs in the tick hook. It rides
`MonsterGoal::Retreat` — the channel `M_FallenFear` has always used for the Fallen — and re-sets it
every tick while the undead is still in the field, so walking away ends it with nothing to undo.

## Conviction

`Monster::isImmune` and `Monster::isResistant` are the entire resistance seam: two functions, four
call sites. Both now read through one accessor, which returns the monster's own bits untouched when
no field is on it — so the ordinary case, which is every monster in the game every frame, costs one
comparison.

The ladder, and the reason it is a design rather than a delete button:

- **Plain resistances are stripped first.** A shallow Conviction neutralises a resistant monster.
- **Only then are immunities stepped down INTO plain resistances**, at five points. A deep
  Conviction turns an immune monster into a merely resistant one.

Doing it in that order is what keeps a deep Conviction from erasing an immunity outright — the
demoted bits arrive *after* the stripping has happened. An immunity is a design statement about what
a monster is; a strong aura should erode it, not delete it. A test walks every immunity at every
investment level and asserts the result is never nothing.

The demotion itself is the function Phase 3.3 wrote yesterday for Nightmare's middle rung. Two
systems, one step-down rule, one place.

## Sanctuary

Nearby undead break and run. Champions do not — letting an aura walk a unique out of the room would
make the fight the player came for un-fightable, which is the same reasoning that keeps Shield
Bash's stun off uniques.

## What this unlocks, and what it does not

**Now live:** Conviction and Sanctuary, the two Paladin rows that named this pass by name.

**Still inert, honestly:** Holy Freeze needs a slow, and monster movement is paced by the animation,
which lives on the shared `CMonster` — the same wall that killed the "Fleet" affix and that only
sprite *scale* has ever escaped. The Barbarian's warcries and the Monk's Temple Bell are **actives**
rather than auras: the repulsion machinery they want now exists, but they need a cast path, which is
a different unit.

**Phase 3.4's other half** — aura-carrying champion packs, a Fanaticism pack or a Might pack — is
now a data question rather than an engineering one. The field query is written and the seam it hooks
is proven; a champion aura is the same computation with a monster as its source instead of the
player.

## Verified

**417 tests, the same two pre-existing failures.** Two new ones, both on the pure ladder rather than
the world: Conviction never erasing an immunity at any investment level, and the radius starting at
zero, never shrinking as points go in, and never growing past the screen.

The exhaustive inert-row test from yesterday did its job silently here: Conviction and Sanctuary
flipping to `implemented = true` removed them from its sweep automatically, and the remaining ~88
still assert byte-identical-to-nothing.
