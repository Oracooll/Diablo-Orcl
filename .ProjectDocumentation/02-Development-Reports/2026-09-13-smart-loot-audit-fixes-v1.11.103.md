# Smart Loot, rebuilt after its first audit

2026-09-13 — v1.11.103

## Why

The audit of v1.11.102 found seven problems in Smart Loot. The user settled the design questions:

- **D2**: Smart Loot must not change rarity.
- **D3**: only main stats count — "not even FCR as all classes will profit from it. Main stats mainly."

All seven are fixed here.

| # | Problem in v1.11.102 | Status |
|---|---|---|
| 1 | Chests and barrels lost gold and potions: that pool is gold 74% of the time, gold scored 0, any equipment candidate replaced it | fixed |
| 2 | Monster "best of three" was nearly always best of one: re-rolls went through the nothing/gold gates, ~9 in 10 wasted | fixed |
| 3 | Rarity inflation: whole items were generated and compared, and tiered items carry more stat affixes, so they won | fixed |
| 4 | Base-level inflation: a weighted SUM of requirements always preferred the deepest base | fixed |
| 5 | Slot starvation: rings and amulets ask for no main stat and lost nearly every comparison | fixed |
| 6 | Score ignored life, resistances, damage, FCR | **as intended** (D3) |
| 7 | No single-player guard | fixed |

## The design now

`SmartLootAimBase(first, player, drawCandidate)` is the only entry point, and both drop sites call it.

1. **Gold, consumables and multiplayer drops pass straight through**, checked *before* any randomness
   is consumed, so the RNG stream is exactly as it would have been. (#1, #7)
2. **It chooses the BASE, before anything is rolled onto it.** Quality — magic, rare, unique, primal —
   is rolled afterwards, exactly once, by the ordinary path. Aiming cannot change rarity because it
   never sees a rolled item. (#3, D2)
3. **Candidates come from an equipment-only pool** (`RndEquipmentForMonsterLevel`,
   `RndEquipmentForCurrentLevel`): the real drop pools with their nothing and gold outcomes removed.
   (#2)
4. **Candidates are drawn for the SAME SLOT** the blind roll chose. Aiming changes which helm drops,
   never whether a helm does. (#5)
5. **The score is a SHARE**: a weighted average of which main stats a base asks for, not how much.
   A Cap and a Great Helm that both want only Strength score the same. (#4)

## #5 needed a second attempt — the test found it

The first rework fixed #5 with scoring alone: bases that ask for no main stat score a neutral 333, the
value a balanced class gives anything. The new real-pool test disagreed:

```
Expected: (barbarianJewellery) > (blindJewellery * 0.35), actual: 0.0245 vs 0.0259
blind 0.0742 aimed 0.0245
```

A Barbarian's rings and amulets still fell to a third of their share. A neutral 333 loses to a
Strength base's 822, and Strength bases make up most of the pool. No scoring change fixes this, because
the problem is comparing *across* slots at all.

So aiming now stays **within the slot** — which is also how Diablo III's Smart Loot behaves: it biases
what is on an item, not which slot drops. Rings and amulets ask for no main stat, so all candidates
tie and aiming correctly leaves them alone.

## Test

`OracoolSmartLoot.AimsTheBaseOnTheRealPoolsAndLeavesGoldRarityAndOtherClassesAlone`, in
`items_test.cpp`, drives the real pool functions. It replaces the v1.11.102 test, which drew uniformly
from a hand-built list and so passed while all five pool problems were live.

1. Gold and a healing potion pass through unchanged, draw no candidates, and consume no randomness.
2. Multiplayer drops are unchanged and draw no candidates.
3. The candidate pool returns only equipment (3000 draws).
4. Scoring is a share: every no-requirement base scores exactly neutral for Barbarian and Sorcerer;
   every Strength-only base scores the same for the Barbarian; the Bard scores everything neutral.
5. Lift on the real pool: Sorcerer Magic-leaning drops beat blind by 1.5×, Barbarian Strength-leaning by
   1.2×; the Bard's drops stay within 25% of blind.
6. **The slot mix does not move**: for Barbarian and Sorcerer, every slot's share stays within
   max(1 point, 20%) of blind.
7. Nothing is locked out: every base a blind draw finds at least 40 times in 40,000, an aimed Sorcerer
   draw finds too.

**Proven by reverting the slot restriction** (candidates drawn from any slot, no slot check):

```
slot 5 moved for class 5
slot 6 moved for class 5
slot 5 moved for class 2
slot 6 moved for class 2
```

Ring (5) and amulet (6) shares move for Barbarian (5) and Sorcerer (2) — the exact starvation above.

## Verification

- Debug: **731 tests, 0 failed.**
- Release built and linked; `DiabloOrcl RTM\DiabloOrcl.exe` refreshed.
- No asset changed, so no MPQ repack.

## Worth knowing

- A problem that looked like a namespace issue on the way — "ambiguous call" to the new pool helpers —
  went away when their definitions moved to the end of `items.cpp`. The anonymous namespace near their
  first home closed cleanly, so the exact cause was not pinned down.
- Aiming is weaker than in v1.11.102 by design. It no longer turns a ring into a helm or a magic item
  into a rare, and those were most of the old version's visible effect. What remains is honest: within
  a slot, a Sorcerer finds staves and a Barbarian finds Strength gear more often.

## What to look at in play

1. A Sorcerer's two-handed weapon drops should lean toward staves; helms and armour change little,
   because few of them ask for Magic.
2. Chests and barrels should give gold and potions as often as they did before v1.11.102.
3. Rare, unique and primal drops should feel as frequent as before v1.11.102 — not more.
