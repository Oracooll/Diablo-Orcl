---
date: 2026-08-16
version: 1.7.2
area: Megaplan Phase 1 - the item endgame (first three units)
---

# The Socket Economy

Phase 1's heart, shipped as three units in one autonomous run (v1.7.0-1.7.2), directly on the
Phase 0 foundations - the provider seam took all three systems without a single change to
CalcPlrItemVals, and none of it cost a save break.

## v1.7.0 - Sockets and gems

Sockets roll on plain, tierless NORMAL equipment only (25% of drops; 1-3 weighted 60/30/10) -
"basic item" is now the raw material of the endgame, the exact role white items played in D2's
runeword culture. Five gems (Ruby/Sapphire/Topaz/Emerald/Skull) with HOST-dependent effects:
fire damage in a sword, fire resist in a helm, bigger resist in a shield. Insertion is the paste
path itself: drop a gem on a socketed backpack item and it goes in, permanently. The item record
grew behind OracoolItemFormatVersion 3 / StashVersion 4, bumped in lockstep per the recorded
stash lesson. The socket roll lives in the DROP paths, never inside seed-replayed SetupAllItems -
the drop-pool lesson, applied before it could bite.

## v1.7.1 - Charms, with D2's lesson learned

Four backpack passives (Vigor +20 life, Embers/Storms +15% resists, Fortune +12% to-hit) - but
only the FIRST THREE in reading order are live. The active cap is the pouch, economically: it
prevents the charm-rack backpack D2 taught everyone to hate, without waiting for pouch art. The
description carries both the effect and the rule.

## v1.7.2 - Runes and runewords, discoverable in-game

Five runes, three launch words (Steel / Lore / Ancient's Pledge - one per host category). The
improvement over D2: every rune's description TEACHES the words it belongs to - the recipes drop
with the runes. Runeword state is fully derived from socket contents (exact sequence, exact
count, right host), so it costs the format nothing and cannot desync; completion renames the item
and logs the moment.

## The seams doing their jobs

- **stat_sheet providers**: sockets, charms, and runeword bonuses are three new provider entries.
  CalcPlrItemVals was not touched again after Phase 0.4.
- **Pool exclusion**: all fourteen new items joined AllItemsList without moving a single seeded
  recreation - the pack golden tests stayed green through every unit.
- **The drop hook**: one draw covers the family - 3% gem, 1% charm, 2% rune - depth-gated by
  iMinMLvl, tunable against the telemetry CSV.

## Play-test notes for the user

- Everything drops from monsters now; `drop ruby`, `drop tir rune`, `drop charm` etc. work in the
  debug console for instant testing.
- Gem/charm/rune ICONS are stand-ins (Blood Stone / Magic Rock) until art ships through the icon
  pipeline - they all LOOK like grey rocks for now. Function first, faces later.
- Balance levers all in one place: drop rates in TrySpawnOracoolGem, socket odds in
  TryAddSocketsToDroppedItem, gem numbers in gems.cpp's table, words in runewords.cpp's.

## Remaining in Phase 1

Ethereal items, Magic/Gold Find affixes, gambling at Wirt, and the crafting window - then Phase 2.

**State: 379/381** - the usual two, plus nine new contract tests across the three units.
