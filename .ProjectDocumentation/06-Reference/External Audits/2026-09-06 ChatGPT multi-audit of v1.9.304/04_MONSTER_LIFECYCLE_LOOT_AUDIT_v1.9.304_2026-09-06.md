# Monster lifecycle, warcry, and loot-path audit

Snapshot: Diablo Orcl v1.9.304, source `c659976d69906489c34c28c636b477f14f424bb6`

## WCR-01 - warcry state survives deletion and attaches to a new monster in the same slot

Priority: P2  
Confidence: high static confidence; add the executable slot-reuse test described below  
Reachability: normal play has dynamic monster creation (skeletons, golems, doppelgangers)

### Evidence

- Warcry debuffs live in a global `std::array<Debuff, MaxMonsters>` at `Source/oracool/warcries.cpp:83-94`.
- The only identity is `monster.getId()`, which is the index into `Monsters`.
- Battle Cry and Inner Sight populate the sidecar at lines `314-317` and `370-373`.
- Conversion stores a timer and sets `MFLAG_BERSERK | MFLAG_GOLEM` at lines `415-433`.
- Every tick, `ProcessWarcriesTick` decrements all sidecar slots at `Source/oracool/warcries.cpp:524-547`. Expiring conversion clears flags on `Monsters[i]`, regardless of whether it is still the original monster.
- `ClearWarcries` clears the sidecar only on level reset at lines `604-609`.
- `DeleteMonster` at `Source/monster.cpp:845-854` removes an active ID but does not clear its sidecar entry.
- `AddMonster` at `Source/monster.cpp:3974-3981` reuses the next ID in `ActiveMonsters` and calls `InitMonster`; `InitMonster` cannot clear the private warcry array.
- Normal dynamic paths include `AddSkeleton`/`SpawnSkeleton` at `Source/monster.cpp:1890-1912`, doppelganger creation at `3987-3998`, golems, and other `AddMonster` callers.

### Failure simulations

Debuff inheritance:

1. Apply Battle Cry to monster A in slot N.
2. Kill A before the debuff expires; `DeleteMonster` makes N reusable.
3. Spawn monster B in slot N on the same level.
4. `EffectiveMonsterDamagePercent/Armor` reads `Debuffs[N]`; B inherits A's remaining penalties.

Conversion expiry corruption:

1. Convert A in slot N and then kill/delete it.
2. Reuse N for B before the old conversion timer ends.
3. The tick at `warcries.cpp:545-546` clears `MFLAG_BERSERK | MFLAG_GOLEM` on B. This can remove legitimate flags from the new occupant, not merely end an inherited visual effect.

### Repair

Expose `ClearWarcryStateForMonster(Monster&)` (or index) and call it whenever a slot is initialized and when it is deleted. Clearing on initialization is essential; it protects all creation paths. Clearing on deletion makes stale state impossible to observe between phases.

A generation-tagged sidecar is safer if this pattern grows: increment a per-slot generation in `InitMonster`, store the generation with every debuff, and ignore entries whose generation no longer matches.

Regression test:

1. Initialize A in a known slot.
2. Add damage/armor debuff and conversion timer.
3. Delete A and initialize B in that exact slot.
4. Assert B has zero debuff, retains its own flags, and remains unchanged after ticking beyond A's old timers.
5. Repeat with a live unaffected monster in a neighboring slot to catch off-by-one clearing.

## WCR-02 - Redemption cadence is process-global and never reset

Priority: P4 polish/hardening  
Confidence: confirmed

`ProcessWarcriesTick` uses a function-static `redemptionClock` at `Source/oracool/warcries.cpp:584-585`. `ClearWarcries`, `ClearWarcryBuffs`, new-game initialization, aura switching, and player switching do not reset it.

The effect is small: the first Redemption pulse after enabling the aura can arrive anywhere from immediately to almost one second later, depending on old process history. A new character inherits the cadence phase. Store cadence per player/aura or reset it when Redemption becomes active/inactive and on new-game teardown. Add a deterministic “first pulse after one full second” test.

## DROP-01 - non-monster loot bypasses Oracool drop finalization

Priority: product decision before code change  
Confidence: confirmed path split

Monster `SpawnItem` constructs an item and then performs the complete new-feature tail at `Source/items.cpp:4611-4618`:

1. Magic/Gold Find upgrade
2. socket roll
3. ethereal roll
4. noteworthy-drop logging

Generic creation functions do not call that tail:

- `CreateRndItem` delegates directly to `SetupBaseItem` at `Source/items.cpp:4626-4631`.
- `CreateTypeItem` does the same at `4648-4659`.
- Chests use `CreateRndItem` at `Source/objects.cpp:2286-2291`.
- Sarcophagi/corpses use it at `2455` and `3487`.
- Armor stands and weapon racks use `CreateTypeItem` at `3502-3508` and `3662`.
- Barrels use `CreateRndItem` at `3841`.
- The new Find Item warcry uses `CreateRndItem` at `Source/oracool/warcries.cpp:397-403`.

Consequences if the intended rule is “all freshly dropped loot”:

- Chest/rack/corpse/barrel and Find Item equipment cannot roll the new socket or ethereal features.
- Player Magic Find does not upgrade those equipment drops.
- Gold Find does not affect container gold.
- Those drops miss the same noteworthy logging path, making telemetry incomplete by source.

This may instead be an intentional monster-only economy. The source comments currently say “drop tail” and “dropped item,” which is broader than the implementation. Decide and document the rule before changing it.

If all true loot should participate, introduce one `FinalizeFreshDrop(item, source, level, player)` funnel after seeded setup. Give quest items, vendors, crafting outputs, and save reconstruction explicit opt-outs; never place unseeded random finalization inside `SetupAllItems`, because replay must remain deterministic.

Regression matrix: normal monster, unique monster, chest, barrel, rack, sarcophagus/corpse, Find Item, quest item, vendor item, crafted item, reconstructed save item. Assert exactly which sources receive MF, GF, sockets, ethereal, and logging.

## Clean lifecycle observations

- New-game/level paths explicitly clear many newer systems: autosave state, gradual healing, mouse hold, auras, grids, waypoints, Furious Charge, crafting/HUD state, Zeal, chills, passives, and warcry level state.
- Ordinary attack paths clear class-specific armed attack latches.
- The focused Rogue-arrow, melee, and warcry test suites all passed.

Those checks narrowed the remaining lifecycle issue to per-monster sidecar identity and the low-grade Redemption cadence leak.
