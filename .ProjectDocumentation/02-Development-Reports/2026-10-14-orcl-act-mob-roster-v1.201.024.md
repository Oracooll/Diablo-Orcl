# Orcl Act: the Mob Roster applied (v1.201.024)

Branch orcl-act, 2026-10-14. Source: the Woods Levels Designer's tab 2 (db `settings/current`, 45 ticks + notes).

## What changed
- **Woods roster** (`oracool/woods.cpp` `WoodsKinds`): only the 40 ticked non-boss kinds walk the woods and their caves. A floor
  takes the kinds whose own band holds its rung; where fewer than four do (rungs 15-16 mostly), the four nearest.
- **Scaling** (`ScaleWoodsMonster`): a kind living above the rung gains 12% life and damage per floor between (plus a little
  armour). Applied after placement, and to raised skeletons, minions and clones.
- **Boss** (`WoodsBossKind`, `PlaceWoodsBoss` in monster.cpp): the last floor's boss is the Skeleton King, the Butcher, the
  Reaper or Diablo, picked from floor 4's seed (re-rolled every new game, fixed within one). He stands 6+ tiles in from the way
  on. He's drawn at Diablo's height (128 / 219 / 176 / 100%) and set to the rung from his own floors (up or down, floor 25%),
  then ×3 life and ×1.25 damage. All-three immunity is demoted to resistances, as rift guardians are. He replaces the Dread boss
  on that floor only.
- **Guards**: a woods Diablo neither pans the camera nor ends the game; the boss settles no quest (CheckQuestKill).
- **oldboned** carries a "use as boss" note but was left unticked, and it is Hellfire's unused kind, so it isn't in.

## Verification
- OracoolAudit.WoodsRosterHasFourKindsOnEveryRung now checks that all four bosses come up across seeds, that the boss's kind is
  loaded on every rung, that the King has a skeleton to raise, and that the roster keeps 4 scatter kinds.
- ctest: 1066/1066 passed (Debug). Release built to C:\Diablo Orcl\x64-Release-OrclAct (MPQs, ini and saves copied in).
- Not yet seen on screen.
