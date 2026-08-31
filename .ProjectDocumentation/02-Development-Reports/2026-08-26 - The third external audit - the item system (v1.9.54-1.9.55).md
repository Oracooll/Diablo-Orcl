# The third external audit: the item system (v1.9.54–1.9.55)

Ten findings — 1 critical, 6 high, 2 medium, 1 low — all in item generation, crafting and the
per-character side tables. Every one verified against the code before anything changed. **All ten
were real**, which is now two accurate audits in a row after the first one's false claim.

---

## The critical one, and why it hid so well

`GetItemAttrs` is the only reset an item receives on its way through `SetupAllItems`. It reset the
**base** fields — type, damage, armour, durability — and never touched a single `_iPL*` bonus.
`SaveItemPower` applies almost all of them with `+=`.

A dropped item was safe, because a drop rolls into a brand new `Item`. But Reforge, Reroll, Enrich,
Awaken and Ennoble all reroll the **same object**, so each pass added its affixes on top of every
affix that item had ever carried. The tooltip lists only the current roll.

**A rare rerolled twenty times was wearing twenty rares** — unbounded, invisible, and saved.

It hid because every observable surface was honest. The affix record was cleared, the tooltip was
correct, the item's name was correct, and the only wrong thing was the character sheet, where
nobody attributes a number to an item they rerolled an hour ago.

Cleared at that one seam rather than in each recipe: there are five recipes today and the sixth
would have been written without the line. Also cleared — because each is its own bug — the fire and
lightning ranges, charges, spell level, value multipliers, the special-effect flag words, and
`_iOracoolEthereal` / `_iOracoolBroken`, which outlived the stats they modify. A rerolled item
stayed "ethereal" and "broken" while its durability had just been reset: no stats, unrepairable.

### The consequence the audit predicted

Mystic Orbs write into the same `_iPL*` fields, and the item records only **how many** orbs it has
taken, not which. So the orbs cannot be replayed. Before the fix, rerolling an orbed item
**duplicated** its orb bonuses; after it, the reroll would **delete** investment paid for with a
capped, permanent resource.

Neither is acceptable and a ledger is a save-format change, so orbed items are refused by the reroll
recipes. The orbs stay and the recipe declines.

## The rest

| # | Finding | Shape of it |
|---|---|---|
| 2 | Set pieces accepted by unique recipes | `MakeSetItem` marks them `ITEM_QUALITY_UNIQUE`, reasonably — they are named objects with fixed powers |
| 3 | Progression bled between heroes | Side tables keyed by player **slot**; the select screen previews every save through `Players[0]` |
| 4 | Set drops skipped finalization | No seed, no item level, no base tier, no ethereal roll |
| 5 | Recast destroyed socketed stones | The one reroll selector without a socket check |
| 6 | Mend did not unbreak | Restored durability, left `_iOracoolBroken` |
| 7 | Refine recipes ate the surplus | Matched on slots, consumed whole slots |
| 8 | Broken set pieces paid set bonuses | "Worn" was not "working" |
| 9 | Two scans ignored the nine extra tabs | The shared iterator already existed |
| 10 | Set powers absent from stores and telemetry | Routed through affix arrays that are empty for sets |

**#3 was worse than reported.** `ApplyHeroChunks` returns *early* when there is no tail, so a legacy
hero, an empty tail and a tail rejected for bad magic all inherited the previous character's
milestones and spent signets. The reset is now at the top of that function, before every early
return — those returns are exactly the paths a caller-side reset would have been forgotten on — and
in `CreatePlayer`, because `player = {}` clears the Player and not the side tables.

**#4 and #7 were both "the same job done twice, differently."** Set pieces were finished three ways
at three sites; refine recipes counted units correctly in the backpack and slots at the monument.
Both are now one implementation.

---

## Where I departed from the audit

For **#8**, the recommended measure was to require the piece to be *stat-active* (`_iStatFlag`).
I used `_iOracoolBroken` alone.

`_iStatFlag` is not a stored fact. It is **computed during `CalcPlrItemVals`**, and its first
assignment there is literally `_iStatFlag = !_iOracoolBroken` before requirement checks refine it.
Reading it from the set walk would make set bonuses depend on whether that pass had reached the item
yet — an ordering hazard that fails toward silently **losing** bonuses, which is worse than the bug.
Three existing tests said so within a minute of trying it.

The broken flag is written once, at the moment of breaking, and is the signal `_iStatFlag` derives
from anyway.

## What the harness could and could not reach

The audit's verification note was right that the existing tests use clean fixtures, and right that
real rolled items are the way to catch these. It also ran into a limit worth recording:
**`ReforgeOracoolItem` needs live dungeon state and faults in a bare test binary.**

So the critical bug is tested at `GetItemAttrs` instead — which is not a compromise. That call is
the *only* reset `SetupAllItems` performs, so "does it leave a previous life's bonuses behind" is
the bug stated directly, and the test plants bonuses and reads them straight back out.

Three fixes were verified by **reintroducing the bug**: the accumulation reset, the hero-progression
reset, and the refine surplus (restoring the slot-clearing leaves five runes where eight should
survive).

Still out of reach here: socketed-Recast and real-break-then-Mend as end-to-end sequences. Both are
now correct by construction and by reading, and neither is asserted.

## Verification

546/549, the two standing baseline failures. The suite grew 546 → 549 across the two commits.
