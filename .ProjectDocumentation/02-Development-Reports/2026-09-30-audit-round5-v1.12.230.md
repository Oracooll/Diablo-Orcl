# 2026-09-30 - Whole-code audit, round 5 (v1.12.230)

**Date:** 2026-09-30. Debug only. The audit continues at the user's word.

Round 5 ran six read-only tracks:
1. a sweep for stale references into the monster array;
2. a sweep for state that outlives a game;
3. a second pass on items.cpp;
4. a second pass on the inventory, belt and stash;
5. a second pass on the fork's missiles and skill effects;
6. a second pass on the game loop and level transitions.

Every finding was verified against the code first.

## Fixed: losses

- **Killing Diablo saved nothing.** The ending cleared the "game running" flag inside the tick, and every save refuses
  once it is clear, so the save after the game loop never ran. Lost with it:
  - the kill itself, which is the hero-select title and the next difficulty's unlock;
  - Diablo's experience;
  - anything since the last autosave.

  The ending now saves before it clears the flag.
- **Items on extra inventory pages could be lost or used by mistake.** The hovered-item index for pages 2-10 was
  reset only while hovering the open inventory with an empty hand, so it outlived a pick-up or a closed inventory.
  - A shift+right-click then split the stack under the old index and wrote the part over the item on the cursor. That
    item was gone.
  - A right-click on the ground drank or salvaged whatever sat at that index. After the inventory closed, that meant
    page one's item.

  The index now resets every frame, and a split refuses while something is held.
- **Banked gold was counted twice.** A pile moved to the stash with Ctrl+click stayed in the pack's gold total, so a
  purchase could take the stash below zero. The pack's total is now recounted. Levski's Cube refuses gold, which it
  had the same problem with.

## Fixed: items

- **An ethereal Torment scythe wrapped its damage past 255.** A Deathbringer read "141-50", and every blow landed at the
  minimum. Both the ethereal boost and the tier scaling are now held to the byte the damage fields are.
- **Oils repaired ethereal items.** Ethereal items cannot be repaired except by the Cube's Mend. The two durability
  oils are now refused on them, and the oil stays on the cursor.
- **Necromancer base drops rolled like a replay.** Wands, scythes and heads could never be Rare, Buffed Unique or
  Primal, never had a base tier, and dropped their uniques about ten times too often. They now roll as the fresh drops
  they are.

## Fixed: skills

- **RfA-12 skills and Command the Dead in town.** In town the monster map holds townspeople, and the slots it points at
  are stale: golem bodies at 1 HP and the last floor's monsters.
  - Chill Touch at Griswold killed a golem slot.
  - Ice Needle through Cain could strike, and eventually kill, a unique left over from the last floor.
  - These skills now find nothing in town.
- **Walking monsters were struck twice.** A walking monster holds two tiles, and Chill Touch, the Teeth fan, Aegis Slam
  and Bone Wall each struck it on both. It is now struck once.
- **Area skills hit through walls** (Absolute Zero, Earthshaker Cry, Thousand Reeds, Poison Nova, Death Nova, Flame
  Ring), as Frost Nova did before round 3. They now need a clear line.
- **Ice Arrow** no longer freezes the hero's own companions.
- **The hero sheet's Blessed Shield and Hammer damage** now includes Towering Shield and Blunt.

## Fixed: slots reused and statics

- **Command the Dead's focus** ended only with its timer. A skeleton raised into the dead target's slot wore the
  Commanded sigil, and a hostile there drew the whole army. The focus now ends with the monster.
- **The passives' element marks** (Conflagration, Exposure) passed to the slot's next monster. They are now cleared
  with the slot.
- **The Weapon Throw latch outlived its swing.** A throw interrupted by a hit fired at the next skill's hit frame, even
  on another floor or in the next game. It then threw the weapon at an old tile and spent the throw's cost.
  - Every swing latch is now cleared when the hero is hit and when a game ends.
  - Arming a melee or Paladin skill drops a pending throw.
- **The Rift Monument menu** joined the new-game close sweep.

## Fixed: rifts

- **Monsters in a rift's theme rooms** kept their floor stats at any tier. They were filled after the tier scaling ran.
  They are now scaled too.

## Tried and reverted

- **Town Portal scrolls from chests.** They still spawn, although the HUD's Portal button replaced them. Swapping them
  for a potion changes what an existing scroll becomes when it is rebuilt from its seed, and the pack test's golden
  item caught that. Left as it was; the scroll is only clutter.

## Not changed (open)

- **Gillian's rework** can re-roll the base armour of items from Griswold's Magic tab and Wirt's Shop tab. Their
  generators draw the base before the armour, and the rework assumes the armour comes first. The fix touches how those
  shops seed their items.
- **Adria's fixed Town Portal slot** (clutter).
- **Slot reuse, narrow windows:**
  - an acid puddle whose dead source's slot is taken by a minion changes sides;
  - a Stone Curse shatter on a reused slot stops early;
  - pack followers can re-leash to whatever takes their dead leader's slot.
- **Cold Mastery** is not applied to the RfA-12 cold skills, Vengeance's cold, or Shiver Armor's retaliation.
- **Arc and Chi Wave** pick their first target at the cursor with no range or line-of-sight check.
- **Earthshaker Cry** is paid in an empty room (possibly deliberate).
- **A Nephalem Rift that closes on a dead hero** sends the corpse to town. Respawn in Town still works.

Debug build and ctest: 882/882. Nothing seen in play.
