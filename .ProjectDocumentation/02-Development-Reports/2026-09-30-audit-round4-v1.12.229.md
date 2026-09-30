# 2026-09-30 - Whole-code audit, round 4 (v1.12.229)

**Date:** 2026-09-30. Debug only. The audit continues at the user's word.

Round 4 ran six read-only tracks:
1. the front end and hero looks;
2. rendering;
3. the hero sheets;
4. the other game windows;
5. file IO;
6. the player core.

Every finding was verified against the code first. Three items from round 3's open list were fixed in the same build
(listed at the end).

## Fixed: two crashes

- **A champion's corpse was drawn through a reused monster slot.** The corpse read its colours and scaled body from the
  dead monster's slot every frame. That slot is freed when the death animation ends, and the next summon takes it.
  - A Raise Skeleton after a champion's death drew the body through a null colour table, which crashes.
  - Other summons drew it with the wrong body or colours.
  - The corpse now copies its look when it is laid.
- **A save writer whose staging failed crashed on its first lookup.** A disk-full or locked-file failure leaves the writer
  inert with no hash table. The autosave's routine "remove the empty extra-tabs record" then read that null table. An
  inert writer's lookups now find nothing.

## Fixed: saves and files

- **The new save is flushed to disk before it replaces the old one.** The rename was durable but the data was only in
  the OS cache, so a power cut just after a save could leave the hero zero-filled with the old save gone. This covers
  the hero archive, the stash and the INI.
- **The sprite mixer's worker thread** read the palette the main thread reloads when the menu opens. That is a data race
  that could bake wrong colours into the look cache. It now reads its own copy, filled once.
- **Telemetry and the debug dungeon dump** open their files through the UTF-8 opener, so a non-ASCII profile name works.

## Fixed: the player

- **Charge snapped onto its target.** Its walk skip started every chained step on the walk's last frame, so the hero
  crossed the whole approach in one tick. It now takes 2 ticks a tile (0.1 s), as the dev note asked.
- **Damage to a dead hero.** A Drain Life item kept hitting through the death animation. Each tick added another death
  to the event log and to telemetry, and a cheat-death passive coming off cooldown could set life on the corpse.
- **The "a quarter of every blow lands" floor** now holds for the sum of all the passives. Battle Hardened stacked past
  it to 95%. Rathma's Shield keeps its deliberate 100%.
- **Near Death Experience** restores mana and never lowers a fuller pool.

## Fixed: the hero sheet

- **Melee skill damage:**
  - The skill's percent multiplied the Strength bonus, which the game adds after the skill. A Berserk Barbarian read
    100+ too high.
  - Whirlwind showed the full swing, not its share.
  - Smite showed plain weapon damage.
  - The 19 RfA-12 melee skills (Cleave, Hammer of the Ancients, Tiger Claw...) showed a dash.
- **Taking back Magic** on an empty orb no longer leaves mana negative.
- **The hero-select screen's Damage** matches the sheet. It showed the bare weapon.
- **The list sheet** shows cold resistance.

## Fixed: the front end

- **The new-hero preview** no longer shows the focused saved hero's shield and sword on the class list.
- **A freed UI list** was read by the next screen (the delete prompt, the name box); the pointer is now cleared.
- **The Necromancer's gamepad name prefill** has its own names; it read the Paladin's.
- **A missing hero sheet file** leaves the preview empty instead of ending the game (a shareware Sorcerer or Necromancer
  row, or a Monk save without hfmonk.mpq). The shareware refusal now covers the Necromancer.

## Fixed: windows and rendering

- **Right clicks and hover went through the skill picker, the runeword book and the crafting book** into the inventory
  or the Abilities window underneath. That drank potions, equipped items and refunded tree points.
- **The waypoint list opened invisibly under Levski's Cube**, which shares its rect. The Cube and the workshop now
  close first; if either refuses (a full pack), the list does not open.
- **The Rift Monument's keys** no longer open a rift while the game is paused.
- **The skill picker's scroll** is re-clamped every frame, so a list that shrinks does not hide its top rows.
- **The runeword book** is closed on a new game.
- **An undrawable corpse** no longer hides everything else on its tile (items, the hero, monsters).
- **Frozen and converted monsters** no longer go dark in full light.
- **The orb clip** covers the waypoint list and the crafting page.

## Fixed: from round 3's open list

- **The last item on a shop tab:** the shop now moves to the Basic tab instead of closing the grid.
- **The Sell hammer:** it uses the same gate as the other two sale paths.
- **A monster could be killed twice** (XP and loot paid twice). This was reachable only by a Bard skill.

## Not changed (open)

- **Press versus release:** the Abilities window's tabs and tree cells, the crafting book's buttons and the skill
  picker's cells act on the press. This is a design change across three windows, not a fix.
- **The Cube's salvage Confirm/Cancel** can't be answered from the keyboard (Enter to confirm, Escape to cancel only the
  question).
- **The Rift Monument menu** closes nothing behind it. Its 20 px overlap with the inventory and the sheet leaks hover
  and right clicks.

## Tests

- **Extended:** the writer staging-failure test now also calls `HasFile`, `RemoveHashEntry` and `RemoveHashEntries` on
  the inert writer.

Debug build and ctest: 882/882. Nothing seen in play.
