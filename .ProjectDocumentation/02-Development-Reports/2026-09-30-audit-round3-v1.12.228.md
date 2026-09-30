# 2026-09-30 - Whole-code audit, round 3 (v1.12.228)

**Date:** 2026-09-30. Debug only. The audit continues at the user's word: "dont stop auditing until you hit the 5 hour
tokens limit or until there are no bugs left in the code".

Round 3 ran six read-only tracks:
1. missiles and spell damage;
2. monsters;
3. vendors;
4. input and the cursor;
5. quests, objects and shrines;
6. options and the game's meta layer.

Every finding was verified against the code first.

## Fixed: options and saving

- **Settings reset on every load since v1.12.199.** The INI loader clamped every entry to its list of values, including
  entries that have no list. Volume, gamma, the tick rate and the last hero picked all went back to their defaults. It
  now clamps only list entries.
- **A torn INI.** The INI is written to a temp file and swapped in only after a clean write and close. The "changed" flag
  clears only on success.
- **The Speed slider** spans exactly the game's speed range, and the menu slider stops at its last step.
- **Death with an item on the cursor** keeps the item in single-player: inventory, then the belt (potions only), then
  the stash. Only if all three are full does it fall to the ground.
- **A refused hero write** is no longer followed by the rest of the save being published over it.
- **Closing the window** (the X on the game window) saves first, as Quit does.

## Fixed: missiles

- **Warp in town** crashed the game (a fatal "invalid leveltype" error). It now fizzles in town.
- **Frozen Orb** burst at the caster's feet on most casts. Its wall test read "did not change tile" as "hit a wall", and a
  half-speed orb often does not change tile. It now bursts when it is actually stopped.
- **Bone Spirit and the Elemental** homed on the hero's own minions and companions. They now skip them, and in town
  (where the monster map holds townspeople) they find nothing.
- **Stone Curse** could petrify the hero's minions and companions. It now skips them and fizzles in town.
- **Berserk** fizzles in town.
- **Golem in town** now fizzles properly instead of being paid for and doing nothing.
- **Frost Nova** no longer hits through walls.
- **The Necromancer's Bone Spirit tooltip** said "half the target's remaining life"; the game takes a third, and the text
  now says so.

## Fixed: input and the HUD

- **The event log took clicks while hidden.** The log hides behind the inventory, the Abilities window, Advanced Stats,
  chat and the full map, but its rect still swallowed clicks, the wheel and hover. The companion and minion headers had
  the same problem. There is now one predicate, `IsCornerHudShown`, shared by the draw and the click rects.
- **Targeting scrolls fired through windows.** A Teleport or Fire Wall cursor clicked over a window fired at the last
  world tile hovered and spent the scroll. Over any window it is now put away; a right click puts it away too.
- **The health and mana orbs and the two points icons** are now interface. Clicks on them walked the hero, cast, or
  dropped the held item; monsters behind them lit up on hover.
- **The stat level-up icon** is routed before the held-item drop, so it no longer drops the item.
- **Gamepad:**
  - A on a shop tab buys again. Return on a shop tab opens chat since 2026-09-25, and the pad's A arrives as Return, so
    nothing could be bought with a pad. The keyboard keeps the chat behaviour.
  - A under Advanced Stats no longer picks up the hidden item under it.

## Fixed: monsters

- **Taunt and War Cry on quest speakers.** The cries reached monsters the engine treats as unhittable: Gharbad, Zhar or
  the Warlord still talking, charging beasts, fading Counselors.
  - Taunt made Gharbad an attacker nothing could hurt, and left Lachdanan and Snotspill unable to talk (blocking Veil and
    Banner).
  - War Cry could kill Lachdanan.
  - The cries now use the engine's own `isPossibleToHit`.
- **A monster killed on the blow that killed the hero** was stood back up with 0 life, if reflect, Thorns, Iron Maiden
  or Shiver Armor killed it. It could not be targeted and could not die. It now stays dead.
- **A raised minion's body.** A monster type added mid-level (Raise Skeleton's body) kept the corpse id its slot held on
  the previous floor, so it left another monster's corpse. It now has none until corpses are set up.

## Fixed: quests, objects and shrines

- **Rift chests used quest set-level loot:** always full, always magic. That gave 14-37 magic items a rift. Rift chests
  now roll as floor chests. Leoric's tomb and the other quest levels keep their reward chests.
- **Rift shrines** paid by the set-level id (9 or 10); a Sparkling shrine gave 9000 experience at any tier. They now pay
  by the rung the rift's tier stands on.
- **Na-Krul:** a story book on floors 21-23 read after his death set his quest active again. It no longer does.
- **Ogden's quest speech:**
  - It could be lost: Esc out of the menu, click him again, and one queued speech overwrote the other. They now queue.
  - It could carry into the next game. It is now cleared when stores reset.
- **The Enchanted shrine** now raises Charge (spell 52), the last id, which it lowered but never raised. Its hover text
  no longer says "multiplayer only"; it works in single-player.
- **The Rift Monument's lookup** is town-only. In a dungeon the same object slot can be the Magic Rock's stand.

## Fixed: vendors

- **Adria's staves used up uniques.** A plain staff that rolled a unique on a restock spent that unique's one drop for
  the rest of the game. The unique flags are now saved and restored, as on the magic shelf.
- **New stock came worn.** Two causes, both fixed:
  - A shop item built like a drop was worn to 25-75%. The Rare shelf, the magic shelf, Wirt's slots and Adria's staves
    now sell whole. Wirt's gamble is a drop and stays as it was.
  - A tiered base's maximum durability was scaled but its current durability only clamped, so a new Nightmare, Hell or
    Torment item sold at 80, 67 or 57%. Current durability now scales with the maximum.

## Not changed (open)

- **Latent double kill:** fixed in v1.12.229 (a monster already dying cannot be killed again).
- **The waypoint re-placement** never keeps its tile (its own object blocks it) and does not check for items. Harmless.
- **Levski's Cube animation** is only driven from a function that never runs in town; it doesn't matter while the Cube
  is one frame.
- **The last item on a shop tab:** fixed in v1.12.229 (the shop moves to the Basic tab).
- **Sell hammer:** fixed in v1.12.229 (the same gate as the other two sale paths).
- **The Sold tab** hides items past its 10x16 grid.
- **HUD controls act on the press, not the release:** the burger menu, the wells, the points frame and the red X
  (a design change, not a fix).
- **Rift summons** raised by the Skeleton King are not scaled to the tier.
- **Known from earlier rounds:** see the round 2 report's open list.

## Tests

- **Strengthened:** `OracoolDurability.TieringAnItemWithHighDurabilityDoesNotZeroIt` now also checks that a whole item
  stays whole.
- **Updated:** two event-log tests close the panels that hide the corner HUD before reading the log's rect. The shuffled
  run caught one test leaving a panel open.

Debug build and ctest: 882/882. Nothing seen in play.
