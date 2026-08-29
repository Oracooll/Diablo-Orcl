# The play-test list of ten (v1.9.101-1.9.102)

**Date:** 2026-08-28
**Versions:** 1.9.101 (items 1-7, 10), 1.9.102 (items 8-9)
**Trigger:** user bug list of ten, with three screenshots.

Seven were real bugs with a single identifiable cause each. Three were requests. Every one is
below, with what was actually wrong rather than what it looked like.

## 1. Right-click did not sell from inventory tabs 2-10

`pcursinvitem` is deliberately **-1** for an extra tab: its encoding is a tab-1 list index that the
legacy drag/drop code assumes, so `CheckInvHLight` refuses to publish one for an item that is not in
`InvList` and sets `pcursinvtabidx`/`pcursinvtabitem` instead. Every single-shot cursor action -
Identify, Repair, Recharge, Oil - already reads that pair. The sale gesture did not, so it silently
did nothing on nine tenths of the backpack.

`ShopSellInventoryItem` itself needed no change: it resolves through `GetActiveInvListItem`, which
reads whichever tab is displayed, and the hovered tab is always the displayed one.

## 2. The aura ring trailed the hero at 3 and 9 o'clock

The engine draws a player from the tile whose `dPlayer` entry is **positive**. The three walk
helpers in `player.cpp` disagree about which tile that is:

| Helper | Directions | Positive tile |
|---|---|---|
| `WalkSouthwards` | S, SE, SW | reassigns `position.tile` to the destination |
| `WalkNorthwards` | N, NE, NW | marks the destination NEGATIVE; `position.tile` stays positive |
| `WalkSideways` | **E and W** | marks `position.tile` negative, `position.future` positive, and never reassigns |

The ring anchored at `position.tile`. For the two sideways directions - due East and due West,
exactly the 3 and 9 o'clock reported - that is one whole tile behind the sprite. The other six were
right by coincidence rather than by agreement, which is why this went months without showing.

It now asks `dPlayer` the same question the renderer answers, so the ring cannot disagree with the
sprite whatever a walk helper does next.

## 3. The Levski grid lost the stack badge

That grid drew its sprites with a bare `ClxDraw` and so never drew any of the three things the
shared `DrawItem` adds on top: the stack count, the red X over a broken item, and the greyscale tint
on gear the character cannot use. A stack of five looked like a stack of one. The inventory, the
belt and the worn slots all go through `DrawItem`; this fourth place items are shown had quietly
opted out.

## 4. A crafted item came out red

Nothing in the crafting path sets `_iStatFlag`. It is normally written by `CalcPlrInv`, which walks
the worn slots and the backpack and has no idea the transmute grid exists. So a fresh item sat there
with the flag clear, `DrawItem` read that as "cannot use" and tinted it through the infravision TRN,
which is red. It corrected itself the moment the item reached the backpack and `CalcPlrInv` ran -
which is exactly why it was only ever seen once per item. Every item the transmute leaves behind is
now stamped with `CanUseItem`.

## 5. Ctrl+click now moves items to and from the Levski grid

With the stash open, ctrl+click already moved the hovered item into it. With Levski's Roar open it
fell through to `CheckInvCut`, whose ctrl arm is vanilla's **drop it on the floor** - the one thing
that gesture must not do beside an open crafting window.

Both directions now work, and the placement is asked before the removal: the grid is packed by
footprint, so "is a slot free" and "does this fit" are different questions and a refusal has to
leave the item where it was.

The hovered-cell resolution lives in `inv.cpp` rather than in `levski_roar.cpp`, because that file
cannot include the inventory layout header - its own `GridWidth` and `CellSize` collide with it.
Only the placement crosses the boundary.

## 6. The XP counter and stat-points button were hidden by open windows

Two separate causes for one symptom.

The XP counter and its per-kill flash were grouped with the mini-map, the clock and the log, all of
which are suppressed while a right-hand window occupies their corner. These two are not in that
corner at all - they draw on the belt plate, dead centre of the main panel, which no window covers.
They were only there because they were added at the same time.

`IsLevelUpButtonVisible` hid the stat-points button behind `chrflag`, the store, the stash and the
quest log, because in **vanilla** it sat in the top-left where those windows open. It does not live
there any more. The skill-point frame beside it never had those gates and has never been in anyone's
way, which is the evidence that the gates were about the old position rather than about the button.

## 7. The RMB well had no tooltip

It had a complete one - naming the aura, the skill, the spell and its level, the scroll count, the
staff charges - and threw it away one statement later. The well is inside `GetMiddleHudRect`, so the
belt hover pass ran over the same pixel, and `CheckInvHLight` opens by calling `ClearPanelStrings`.
The LMB well returns after building its tooltip; this one fell through. One `return`.

## 8. More in the skill, spell and aura hovers

Two halves, and the scope is stated honestly because half of it is not possible today.

**The skill picker** named the entry and stopped. It now shows the block its own sheet already
builds: `BuildSpellStatBlock` for a spell (mana, damage now, damage at the next level) and
`ClassTreeEffectLine` for a tree row. Called, not reimplemented - a second copy would be a second
place for the next formula change to be forgotten.

**Class-tree rows** now carry magnitudes, but only where a magnitude is modelled:

- every aura's **radius**, now and after the next point, from `AuraRadiusForPoints` - silent when
  the next point does not move it, since the radius steps every second point and caps at eight;
- **Conviction's** immunity-break threshold, which is a step rather than a curve.

The remaining rows have bespoke effects with no per-rank formula behind them, and several are still
`implemented == false`. Inventing a number for those would be worse than the silence: a tooltip that
quotes a bonus the code does not apply is a bug that reads as a feature. Those rows keep their
description, which is what states the effect today. Giving them real numbers means authoring
per-rank effects for the tree, which is a design job rather than a display one.

## 9. The level requirement never moved

`ClassTreeEffectLine` printed `ClassTreeTierMinLevel(tier)` - the tier's floor, a constant - while
the gate that actually refuses a point is `RankRequiredLevel(tierLevel, invested + 1)`, the Rule of
Rangs. A tier-1 skill with nine points in it still advertised "Requires level 1" while silently
demanding level 10 for the tenth, and the button did nothing with no explanation on the tooltip.

It now names the **next point's** price, which is the only number a player standing in front of the
button can act on, and says "Fully invested" where there is no next point to price.

## 10. The white salvage charm spared socketed whites

A socketed white base is the one thing a runeword can be built in, and the white charm is the button
most likely to be pressed without looking - a pack full of grey drops is exactly what it exists to
clear, and the socketed base among them is worth more than the rest together.

`SalvageMatches` is now one predicate shared by the collect pass and the "is this button live" test,
which used to spell the same condition out separately - two places for a protection to be added to
only one of, and a button that lights up over stock it then refuses is worse than one that never
lit. Scoped to White as asked; the higher tiers are not swept blind in the same way.

## Verification

Suite **574/576** throughout, the two standing baseline failures
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).

No automated test: every one of these is a hover, a draw or a click path, and the honest note is the
standing one - a screenshot is the only verification for anything drawn over the world. What to look
at in game:

1. Right-click an item on tab 3 with Griswold open.
2. Walk due east and due west; the ring should stay centred on the hero.
3. Put a stack of potions in the Levski grid - the count should be on the icon.
4. Transmute something and pick it up - no red tint.
5. Ctrl+click between the backpack and the grid, both ways; nothing should reach the floor.
6. Open the inventory - the XP counter and the stat button should stay.
7. Hover the RMB well.
8. Hover a spell and an aura in the quick list, and an aura on the tree page.
9. Put a point in a tier-1 skill and re-read the requirement line.
10. Socket a white item, then press Salvage Whites.

Not pushed: GitHub Actions minutes are exhausted until roughly 2026-09-01.
