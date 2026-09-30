# 2026-09-30 - Whole-code audit, round 8 (v1.12.233)

**Date:** 2026-09-30. Debug only. The audit continues at the user's word.

Round 8 ran six read-only tracks:
1. item tooltips against the real effects;
2. crafting and the Cube, second pass;
3. the Sorcerer and the Rogue;
4. the Monk and the Necromancer;
5. quests and townspeople;
6. gamepad controls.

Every finding was verified against the code first.

## Fixed: tooltips and item rolls

- **About 55 expansion uniques** printed their stats through fields other powers also write. The Quiet Sun read "+16 to
  all attributes" and "Resist All: +41%" where it gives 5 and 15. Fixed-value attribute and resistance rows now print
  their own value, as set pieces do; ranged rows keep the old printer. The shop's set-piece line prints the same way.
- **A negative durability row** reads "decreased durability". On a base with no durability (Vesper Bell, Salt Heart) it
  prints nothing, since it does nothing. The tooltip sweep's row count skips it there.
- **Runeword attack-speed flags** are named: seven weapon words carry Fast Attack and printed no speed line.
- **Fire and lightning weapon damage** (and fire and lightning arrows) no longer roll together on one item. Each power
  zeroes the other's fields, so a "Flaming ... of Lightning" sword was priced for both and dealt one.

## Fixed: crafting

- **Recolour turned a gem stack into one gem.** The whole stack now changes colour.
- **Ogden's Upgrade at the top of a ladder** (Perfect gem, Radiant jewel) took three and gave one of the same back. It
  is now refused.
- **Reforge, Ennoble, Recast and Consecrate** were free repairs; they bypassed Mend. They now keep the wear and the
  broken flag.
  - Ennoble also keeps the base tier's numbers; it wrote Normal damage under a tier name.
  - Recast and Consecrate keep empty sockets, which were lost even when bought with Punch Sockets.
- **Gillian's bench** tries every recipe the grid answers alone before lending from the pack. A Cleanse had become an
  Enrich.
- **Consecrate** respects the item's depth. A level-3 ring could become the deepest set's ring. Recast stays within one
  set, so it is not gated.
- **Reforge** refuses set pieces, as Awaken and Reroll Uniques do.

## Fixed: classes

- **Rogue:**
  - Gnat Sting's second release frame fired the armed bow skill again and charged again.
  - The elemental bow skills (Magic, Fire, Cold, Exploding, Ice, Immolation, Freezing Arrow) now carry the bow's +% and
    the Strength part. The missile path adds those to physical hits only, so these arrows landed at the bare weapon
    dice.
- **Sorcerer:**
  - Static Field and Thunder Storm need a line of sight.
  - Shiver Armor's counter respects cold resistance.
- **Necromancer:**
  - The corpse table (100 bodies) now replaces its farthest body when full. New kills were left unraisable.
  - Confuse and Frailty skip uniques, as Terror does. They paid Essence and replaced the curse the unique already had.
  - The area curses, Soul Harvest, Army of the Dead and Death Mark's burst need a line of sight.
  - A Revived body this floor cannot take no longer holds back the rest of the army.

## Fixed: quests and townspeople

- **The quest log's reveal mode** (on by default) lists every quest this game can hold that is not finished.
  - Testing INIT alone dropped a quest the moment vanilla set it active without logging it: the lair entered before
    Ogden's talk, Lazarus killed before Cain.
  - It hid the Jersey while he teased.
  - It listed quests that cannot exist here: the Wandering Trader, whichever of the cow and the farmer is absent, and
    Theo with its option off.
- **Ogden's "!"** stays lit while his queued speech waits.
- **The Complete Nut's "!"** no longer stays lit for as long as the rune bomb is carried.

## Fixed: gamepad

- **Town objects** can be operated from the pad: the stash chest, the waypoint, the Cube, the Monument.
- **Inventory pages 2-10** turn from the pad; the tab's release was never sent.
- **A spell beside a townsperson** talks to him instead of doing nothing. The talk used the pad's (-1,-1) cursor.

## Not changed (open)

- **Gamepad, larger items:**
  - Cancel opens the Abilities window instead of closing the fork's windows.
  - No face button readies a skill in the Abilities window.
  - With the inventory open, Primary cannot reach the Cube or the workshop.
  - The D-pad does not move the shop grid.
  - Pad quick-spells ignore aura and left-button bindings.
  - Belt snapping lands on the vanilla slot positions.

  The summary asked the user whether pad play matters.
- **Decay's tooltip** prints the combined damage total; Crystalline prints through the same path.
- **The Mourning Token** grants fire and lightning only, against its "all resistances" design.
- **Shrunken heads** have no "Necromancer only" line.
- **Hunter's Mark** does not raise the RfA-12 bow skills' damage. Storm Crucible and Ride the Lightning strike along an
  8-way ray, not the segment.
- **Recasting Decoy** does not move it. Firestorm's fireballs fly from the hero.
- **Crafting:**
  - Zod plus Free the Sockets is a repeatable repair.
  - Make Ethereal halves Tempering's durability.
  - Reroll Rares keeps a Blood craft's name.
  - Unbind checks only the first wearable.
  - Bulk salvage destroys stones in magic-and-better items (by design, only whites are protected).
- **Attract's lured kills** give no experience.

## Tests

- **Updated:** the tooltip sweep's unique-row rule skips a durability row on a base with no durability.

Debug build and ctest: 884/884. The Debug `diablo.ini` was unchanged through the run. Nothing seen in play.
