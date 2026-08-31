---
date: 2026-08-16
version: 1.7.25
area: Megaplan Phase 3.4, second half - aura-carrying champion packs
---

# What the Champion Lends Its Pack

The megaplan calls this one *"D2's scariest idea"*, and it is right about why: the frightening thing
in Diablo II was never the champion, it was the champion's **pack**. An ordinary skeleton standing
next to a Might boss hits like something several floors deeper, and you find that out by walking
into it.

Yesterday's aura field made this a data question rather than an engineering one. This is the data.

## Reusing the affix instead of inventing a column

A champion already carries a `LesserUniqueAffix`, and that value is already saved. So the aura a
champion radiates is **derived from the affix it already has** — no new per-monster state, no save
change, and every existing champion in every existing save gains its pack aura the moment it loads.

It also makes each affix say more than it did. An affix used to describe the champion alone; now
three of them describe the champion and three describe the whole group:

| Affix | Lends |
|---|---|
| **Relentless** | Might — the one that will not be knocked back drives its pack forward with it |
| **Fortified** | Defiance — the armoured one shelters what stands beside it |
| Warded, Vampiric, Thunderous, Colossal | nothing; a resistance, a life-steal, a death burst and a size are all personal |

Keeping four of them personal is deliberate. If every affix lent something, the six would blur into
"champion nearby, numbers bigger". Half lending and half not is what makes reading the champion's
name worth doing.

## Same rule as yesterday: a query, never a write

`Monster::minDamage` and `armorClass` are saved, so lending by writing into them would need
un-writing on every path where the pack breaks up — the champion dies, the monster wanders off, the
level unloads. Instead the lending is computed at the six places those values are **read**: three
where a monster's damage is taken for a swing (melee, ranged, special), and three where its armour
is taken for the player's hit roll (melee, missile, ranged).

Pull a monster away from its leader and it weakens on the spot, because there was never anything
stored to undo. That is also the intended counterplay, and it is why the pack radius is **tighter
than a player aura at full investment** — the edge has to be somewhere the player can reach.

## Two details worth stating

**The strongest of each kind, not the sum.** Two champions in one room lend the better Might and the
better Defiance, not both added together. A floor that rolls two packs into the same corridor should
be harder, not exponential.

**Clamped, not wrapped.** These land in `uint8_t` fields. The Torment difficulty block already had
to learn this lesson: a wrap makes a *stronger* pack come out unpredictably weaker, which is the
worst kind of bug because it looks like balance.

## Verified

**418 tests, the same two pre-existing failures.** One new test, and it pins the rule rather than
the world — `Monsters` and `ActiveMonsterCount` are not exported to the test binary, the same
limitation Phase 3.3 hit with `MonstersData`, so the scan is thin and the rule it calls is pure:

- Exactly two of the six affixes lend; the other four are asserted silent.
- The pack has an edge, it is beyond arm's reach but well inside "pull it away from the leader".
- Raising damage never makes it smaller, checked over every base value 0-255 at four percentages.

## Where this leaves Phase 3

3.2 (Colossal), 3.3 (difficulty resistances) and 3.4 (both halves) are done. **3.1 remains blocked**
on Phase 4 creating a zone to wire a roster into.

The natural follow-up is legibility rather than mechanics: a player currently learns that Relentless
means "and its friends hit harder" by dying to it. The champion's name is already on screen, so the
hook exists — a line in the hover panel would close it.
