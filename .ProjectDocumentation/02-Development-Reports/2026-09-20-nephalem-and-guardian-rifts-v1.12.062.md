# 2026-09-20 - Nephalem and Guardian Rifts (v1.12.062)

**Date:** 2026-09-20 - Debug only - green: 827/827 (build 38; 36 and 37 caught an unexported test symbol and the item record size). The plan page
(https://claude.ai/artifact/TJkuDBeQjnxZgn27csdGNM) answered by the user: every recommended option except
r8 (normal loot inside a Guardian Rift too) and r10 (death leaves the rift open until its clock runs out).
Phases P1-P7 built in one pass; P8 (audit) and P9 (play) remain.

## What a rift is now

- **The floor (P1).** Two new set levels, `SL_RIFT_NEPHALEM` and `SL_RIFT_GUARDIAN` (levels/gendung.h, inside
  the NUMLEVELS-long visited arrays; `SL_LAST_ARENA` keeps the arena list in control.cpp honest). `LoadSetMap`
  sends them to `oracool::BuildRiftLevel`, which runs `CreateDungeon` on the rift's own seed with `currlevel`
  temporarily set to a quest-free floor of the rift's tileset (2, 7, 11, 13, 18, 22), installs no stairs,
  loads the palette, the tile properties and the themes (seeded, so a revisit computes the rooms the saved
  objects sit in), the object sprites, and on a fresh build the objects; `FinishRiftLevel` adds the theme
  rooms after the items. A revisit (the hero died and came back) restores everything else through the
  engine's ordinary level save. `CurrentAreaLevel` answers the rift's tier. `oracool/rift.cpp` holds the
  state: kind, tier, seed, tileset, guardian, the bar, the clock. Nothing is saved; a new game resets it.
- **The roster (P2).** `GetLevelMTypes` for a rift: the four golem bodies a .dun would have given, the
  guardian's type (and a skeleton type for the King to raise), then up to five types from the WHOLE game whose
  floor band contains the tier's rung or its Hive/Crypt twin (`RiftAcceptsMonster`), under the usual image
  budget. `PlaceRiftMonsters` fills the floor as InitMonsters fills a dungeon floor (champions, a Dread boss,
  the scatter at the density dial) and then scales everything by `RiftScalePercent`: +3% life and damage per
  tier above what the current difficulty pays on that rung - a Nephalem Rift scales nothing.
- **The bar and the guardian (P3).** Kills credit the bar (1 / 3 champion or variant / 5 unique), sized at
  70% of the floor's total credit; at 100% `SpawnRiftGuardian` places him on the nearest open ring three
  tiles out from the hero: the Skeleton King, the Butcher or Na-Krul through `PrepareUniqueMonst`, Diablo as
  the plain `MT_DIABLO` body, none of them wearing a variant. His death marks the rift done, drops the pile
  (`TreasureBonusFor` pays a rift guardian a Dread boss's six), the keystone, and lays a `WM_DIABRTNLVL`
  trigger where he fell with the rift's portal drawn on it. In a rift Diablo's death is an ordinary death
  (no pan, no ending, no `DiabloDeath`) and `CheckQuestKill` settles nothing.
- **The door (P4).** `ToggleStonegate`: closed -> a free Nephalem Rift at the deepest floor's tier (r3, r6);
  open -> closed and the rift ended. Walking onto the tile in front of the gate (south, +1,+1) enters -
  `ProcessRiftPortal` in town asks `TryEnterRiftFromTown`, which waits until the hero has LEFT the tile once
  (they stand beside the gate to click it). Town rebuilt with a rift open relights the portal silently; a
  cleared rift ends there.
- **Keystones (P5).** `IDI_ORACOOL_KEYSTONE` ("Guardian Keystone", never in a pool, misc id
  `IMISC_ORACOOL_KEYSTONE`, batch 42's icon as `ICURS_ORACOOL_KEYSTONE`, the sheet's last frame), its tier in
  `_iOracoolRiftTier` (item format 13). A Nephalem guardian drops one at the rift's tier; a Guardian guardian
  drops the next tier's - one up, three up with more than half the clock left, none out of time
  (`NextKeystoneTier`). Using it in town opens a Guardian Rift at its tier and lights the violet portal;
  outside town it is refused and kept.
- **The clock (P6).** Fifteen minutes, ticking everywhere while the rift stands (r10); `CheckSpell` refuses
  the town portal inside a Guardian Rift before any cost. Loot is normal throughout (r8).
- **Edges (P7).** Golem slots on the rift floor; summons follow. `/rift nephalem` and `/rift guardian
  [tier]` open and enter from town.
- **HUD.** `DrawRiftHud` under the mini-map: the rift's name, the percentage or the guardian's name or
  "cleared", the clock, and a bar filled in the portal's colour.

## Tests

Five new: the set-level ids fit the visited arrays and are not arenas; the scale is flat at the floor and
+3%/tier above; the Nephalem tier is the deepest floor reached; the roster band takes the side-step twins;
the kill credit weights; the keystone arithmetic and the item row.

## Not verified in play

Everything. First things to look at: click the gate, walk into the gold portal, the floor's mix and the bar,
the guardian rising, the pile and the keystone, the way home; then use the keystone and watch the clock.
