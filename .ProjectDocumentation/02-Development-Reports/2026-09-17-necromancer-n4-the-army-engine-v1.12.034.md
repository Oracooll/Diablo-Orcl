# The Necromancer, phase N4: the army engine (v1.12.034)

**Date:** 2026-09-17 - Debug only. **Tests:** 810; the full parallel run passed 809 and failed
`OracoolOptions.EveryRegisteredEntrySurvivesASaveRoundTrip` once ("SaveOptions wrote nothing to the redirected
path"), which passes when run alone and touches nothing this build changed - a file-I/O flake under `-j 8`.

N3 (ledger + RfA-17 draft) stands delivered; the user said "go. start n4".

## The idea

A minion is an ORDINARY monster in an ORDINARY slot with `MFLAG_GOLEM` set. The engine already treats that flag as
"fights for the heroes" everywhere it matters (UpdateEnemy, every damage path, the cursor, loot, XP), and the
Companion brain in monster.cpp (`CompanionAi`) already fights from a set of ORDERS rather than from a slot. So the
army is mostly bookkeeping: `oracool/minions.{h,cpp}`.

## What was built

- **The pool.** `MaxEnemyMonsters` 200 (what `MaxMonsters` was), `MaxMinionBodies` 32, `MaxMonsters` 232 - the
  army sits ABOVE the enemies, so a floor keeps every monster it had. Level generation caps
  (`MaxEnemyMonsters - 10`) and every runtime enemy spawn (`EnemyMonsterRoomLeft`, which leaves minion bodies out
  of the count) are unchanged in effect; only `AddMinionBody` may use the rest. 232 < 255, the limit of
  `Monster::enemy`. The plan's "measured choice" was decided by this: reserving 32 of the 200 would have thinned
  dense floors for every hero.
- **Records, not slots.** Up to 32 records {owner, group, spec, life, place in ring}; a slot-to-record table
  answers "is this a minion" in O(1). `DeleteMonster` releases the record with the slot, so a reused slot can
  never inherit an owner. Groups and caps are D7's: Skeletons 8, Mages 8, Golem 1, Revived 10.
- **Sprites on demand.** A minion's monster type is loaded when the first one is raised, not with every level: a
  type added at level load counts against the level's sprite budget and would change which monsters a floor
  rolls, for every hero.
- **The brain.** `GetCompanionOrders` falls through to `GetMinionOrders`; `ProcessMonsters` sends a minion to
  `MinionAi` instead of its native AI. Shared stance (J key / panel header). Looser leash than a companion's
  (6 / settle 4 / regroup 12 in Follow) because thirty bodies cannot stand within three tiles. A minion flinches
  like a monster, so anything but standing or walking is left to finish.
- **Thinking budget.** An idle minion thinks on one tick in three, its own third (slot id + tick); walking and
  fighting are never held up.
- **Formation.** Rings by group - golem at 1 tile, skeletons 2, mages 3, Revived 4 - spread evenly round the
  ring. Rings, not a "front": a hero here turns on the spot, and a front would swing thirty bodies each time.
- **The corridor.** The hero walks through his minions as through companions (`CompanionMakesWay`). A minion
  with something to fight, blocked by an IDLE minion, trades tiles with it (`MinionTradesPlaces`); idlers never
  trade with each other, so nothing ping-pongs. Beyond the regroup distance a minion reappears beside its owner.
  A minion with nothing to fight clears `MFLAG_TARGETS_MONSTER` AND resets `enemy` to 0 - without the flag
  ProcessMonsters reads `enemy` as a player index.
- **Stairs.** `SaveLevel` first withdraws every minion body (`WithdrawMinionsForLevelSave`), the living keeping
  their life in their records; stored, they would return on the next visit as ownerless friendly monsters of a
  type that visit never loaded. The army re-forms around its owner on the next floor, four bodies a tick. Not in
  town. `ForgetMinions` with `ForgetCompanions` on a new game.
- **Credit.** A minion's kill tags its owner (its slot says nothing about whose it is).
- **Panel.** "Army: <stance>" with a row per group: name, count/cap, a life bar for the group.
- **Debug command** `army` (27 skeleton-bodied minions across the four groups, scaled to hero level),
  `army 12`, `army 0`.

## Not in this build

Ranged minions (the brain's bow path shoots arrows; mages are N5), the skills that raise anything (N5), minion
curses/auras. All minions are melee skeleton bodies for now.

## For the user's look in play

In a Debug game, in the dungeon: open the debug console and type `army`. Then: walk a one-tile Cathedral
corridor and through doors with it; fight a pack (do they engage, does the back rank get through); take the
stairs down and up (the army re-forms; the floor you left has no stray skeletons when you return); J cycles the
stance; the panel counts; frame rate with 27 bodies; `army 0`. A screenshot is the only verification.
