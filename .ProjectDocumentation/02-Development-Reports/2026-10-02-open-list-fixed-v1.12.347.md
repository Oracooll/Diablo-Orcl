# 2026-10-02 - The open audit list, fixed (v1.12.347)

**Date:** 2026-10-02. Debug only. All 915 tests pass, including the shuffled runs. The Debug `diablo.ini` was unchanged by ctest. Commit b10d3254.

## The user's verdicts
- **Barbarian mana text:** FIX.
- **Fist of the Heavens:** FIX.
- **Blessed Hammer through walls:** STAYS AS IT IS.
- **Repeated set bonuses:** "if it is an affix that can be accumulated - accumulate it."
- **Everything else on the list:** FIX.

The work ran in six agents at once, followed by one build.

## Barbarian
- **Tooltips:** mana-only powers (+Mana, mana curse, mana steal, no mana) are hidden from a Rage viewer's item, shop, gem, rune and runeword lines.
  - Mana to Life and Life to Mana stay, because each also moves life.
  - The advanced-stats panel still prints its mana rows.
- **Shrines:**
  - Cost of Wisdom (Fascinating, Sacred, Ornate) teaches and takes nothing from a Barbarian, and says so.
  - Divine gives him Full Healing in place of Full Mana.
  - He gets his own hover text on the mana shrines.

## Skills
- **Fist of the Heavens:** 32 distinct ring bolts; the four doubled axis bolts are gone. Each monster is struck once per ring, through a per-cast ring id (`ClaimFistRingTarget`). That gives the tooltip's 60%.
- **Madawc:** his hammers never miss (`Missile::neverMisses`).
- **Shoulder Gate:** the rush stops before the first enemy on the line and stuns that enemy (`RushEndToward`). It pays nothing when the stun fails.
- **Charge:** refused when the walk is more than one tile longer than the straight line (`ChargePathIsDirect`).
- **Closed doors:** they blocked missiles but not the skills' sight. The new `SightLineClear` is used by the area bow skills, targeting, Strafe, Radiance, Decompose and Bone Prison.
- **Meteor:** the impact burst is back to about 0.5 s.

## Set bonuses
- **Values that add up:** life and mana steal, and armour vs undead and vs demons, now sum over every source, held in new unsaved `_pI*` totals.
  - Crimson Compact and Leoric's Court pay 8% at the top of their ladders.
  - Dawnwarden pays +40 vs undead.
- **On/off repeats:** six rungs that repeated an effect the hero already had now grant a bonus that adds instead:
  - Ashen 3: damage taken -2.
  - Stormcrow 3: +6 Dexterity.
  - Iron Root 6: +20 armour.
  - Clockwork 3: +10 to-hit.
  - Clockwork 4: damage taken -2.
  - Lost Cartographer 4: +25 life.
- **Tests:** they reject a rung that repeats an on/off power of its own set, and check that stacked steal sums.

## Gamepad (the round 87 package)
- **Backpack pages:** LB/RB turn pages 2-10, a D-pad tab row reaches them, and X and Y work on those pages with page 1's target checks.
- **Belt:** the belt cursor uses `GetBeltSlotRect` and covers slots 1-4 only.
- **Floating windows:** A reaches the Cube, workshop and runeword book with the pack open.
- **Class tree:** it answers A, and B no longer closes everything.
- **Quick-spell menu:** it honours aura and left-button bindings.
- **Abilities window:** it has arrow and D-pad focus with a gold frame, and Enter clicks.
- **Not done:** the touch pad's primary button and the quest log still map B to Space.

## Items and loot
- **Recast:** keeps a set piece's oils, using the new `MeasureOracoolSetPieceOilWork`.
- **The D2 crafts:** priced at craft time (`RepriceOracoolItemFromRecord`).
- **Craft names:** a reroll or a reforge drops the craft word.
- **Belt refill:** potions only.
- **Workshop wells:** an empty transmute refuses quietly and keeps the bench.
- **Hero sheet:** scroll and staff damage are shown at the level the cast uses.
- **Object loot:** chests, barrels, racks and theme rooms draw from the floor's level, as monsters do. They used to draw from twice that.

## Rifts and difficulty
- **Guardian Rift keystone:**
  - The keystone spent on entry is owed back after an exit or crash inside an unfinished rift. It is saved in hero chunk 21 and refunded on the next load.
  - A keystone lying on the rift floor comes home with the hero. The rift stays open if he has no room for it.
- **Torment:**
  - No monster is immune to all three schools; rows authored all-three keep magic and fire immunity and resist lightning.
  - The test also caught a promotion bug: a Hell row resisting all three and immune to lightning came out immune to everything. A resistance bit beside its own school's immunity no longer counts as an answer.

## Controls and engine
- **Press and release:** these now act on release inside, as the button default requires:
  - the red X close buttons;
  - the burger menu icons;
  - the belt's Menu, Portal and Run slots;
  - the skill picker cells;
  - the shop's fallback row;
  - every class tree control.
- **Tint tables:** cached, 16 entries.
- **Refresh Until:** with no timeout it stops at 2,000 refreshes.
- **Telemetry:** rotation retries 5 minutes after a refusal.
- **Loading:** level and save files are bounds-checked on load.

## Tests
- Two text tests (runes, jewels) now set no viewer. Under shuffle they inherited a Barbarian as `MyPlayer` and correctly lost the mana lines.

## Needs a play pass
- Every gamepad change.
- The Abilities focus frame.
- The press/release feel.
- Shoulder Gate.
- The Fist ring's look.
- The keystone refund.
- The Barbarian's shrine texts.
