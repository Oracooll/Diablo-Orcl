# 2026-10-04 - RTM dev notes: spear look, chill tint, town portals, rift boss roster (v1.12.377)

**Date:** 2026-10-04. Debug only. All 940 tests pass. The Debug `diablo.ini` was unchanged by ctest (md5 b3392ce4). Commit 10d6ce0b. The MPQ was repacked for the new portal sheets.

The source is the first batch of notes filed from an RTM build: `development.md` in `DiabloOrcl RTM/Saved_Games` (PrefPath). There were four notes. They are archived in `development-archive.md`, tagged `| RTM`, and the RTM inbox file is emptied. No save file or the ini was touched.

## 1. A spear wears the staff body
"wearing spear as a weapon to load staff wielding sprites sheets."
- **items.cpp `CalcPlrItemVals`:** a held Spear (`UITYPE_SPEAR`, an `ItemType::Sword`) or Pike (`UITYPE_PIKE`, an `ItemType::Axe`) counts as `ItemType::Staff` for the body choice only. It wears `PlayerWeaponGraphic::Staff`, or `MaceShield` beside a shield, the same rule a staff follows.
- **player.cpp `SwingWearsStaffLook`:** returns false while the staff sheet is already worn, so the javelin skills' staff-look swap does nothing then.
- **Test:** the shield-body test in oracool_audit_test checks that both bases wear Staff alone and MaceShield with a buckler.
- **Side effect:** the swing timing is now the staff sheet's.

## 2. A chilled hero is tinted
"if i am slowed down by a cold hit tint me cooled - light blue."
- **scrollrt.cpp `DrawPlayerTinted`:** when none of the existing tints applies and `oracool::IsPlayerChilled`, the hero draws through `TintedTable(..., Tint::Ice)`, a steady light-blue glaze.
- **Priority:** Shadow Step, Astral, the cold armours and Conduit still win.
- **missile_tint.cpp `Tint::Ice`:** now leaves pure black alone, as `Electric` does, so the hero's shadow stays a shadow.
  - Nothing else drew through `Tint::Ice`; the armours use Glint.

## 3. No portal shadows in town
"dont cast portal shadows over monuments - we have two in town, so - no portal shadows in town."
- **tools/BuildRiftPortals.ps1:**
  - `portal_town.png` is built with `$stripShadow` again; v1.12.370 had put the shadow back.
  - Two new sheets: `portal_gold_town.png` and `portal_purple_town.png`, with the gold and purple recipes plus `$stripShadow`.
  - The existing gold, purple and red sheets came out byte-identical.
- **misdat:** `MissileGraphicID::RiftPortalGoldInTown` and `RiftPortalPurpleInTown` are appended before `None`, so saved missile anim ids keep their values.
- **missiles.cpp `AddRiftPortal`:** in town it calls `SetMissAnim` onto the town sheet. `SetMissDir` keeps the graphic, so the switch to the standing loop stays on it.
  - The Guardian violet draw is keyed by `_mitype` and is unaffected.
- **Unchanged:** a rift's exit portal and every dungeon portal (blue CL2, red, the rift sheets) keep the shadow.

## 4. Rift boss roster: proposed, not built
"suggest a nice big roster of rift bosses."
- **Today:** a coin flip in `OpenRift` picks Leoric or the Butcher for a Nephalem Rift, and Diablo or Na-Krul for a Guardian Rift.
- **The page:** **Rift Boss Roster**, https://claude.ai/artifact/19y6u5hv47hXB6XdcLaRZv (capability db, collection `verdicts/<id>`).
  - **32 bosses:** 20 Nephalem and 12 Guardian, all on existing vanilla and Hellfire sprites.
  - **Per boss:** size 140-170%, a tint, life/damage/armour multipliers on top of the tier scaling, walk and attack speed in ticks per frame, resists, a signature trick taken from an existing behaviour, an escort, home tilesets and a tier band.
  - **Engine steps B1-B8:** a boss table, spawning any type, a free scale percent, a per-monster glaze, per-boss speeds, signatures, escorts, names.
  - **Four open questions:** keep the current four faces; the 170% cap; bosses outside their home; a phase 2 with new art.
- **Next:** read the verdicts with ArtifactData `list verdicts` before building.

## To check in play
- A spear and a pike in hand, with and without a shield, on each armour.
- A cold hit: the light-blue hero while slowed.
- Town: the town portal and both rift portals in the monument, with no dark shadow on the monument.
