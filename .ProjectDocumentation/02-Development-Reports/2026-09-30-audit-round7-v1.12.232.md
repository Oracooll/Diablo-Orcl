# 2026-09-30 - Whole-code audit, round 7 (v1.12.232)

**Date:** 2026-09-30. Debug only. The audit continues at the user's word.

Round 7 ran six read-only tracks:
1. the never-played endgame (Sealed Maps, keystones, signets, Dread bosses);
2. the skill and missile data tables;
3. the item data tables;
4. lighting, vision and the automap;
5. a second pass on saves and loads;
6. a second pass on the Paladin and Barbarian code.

Every finding was verified against the code first.

## Fixed: lost progress

- **Two of three Sealed Maps spawned no boss and ate the map.** The boss is a lesser unique of the encounter's monster
  type:
  - the Ring of Mourning used the Flesh Clan, whose only unique is Gharbad, a quest speaker that is filtered out;
  - the Ember Vault used the Horned Demon, which has no unique at all.

  Both arenas were empty, and their charms could never be earned. They now use the Fire Clan (Bloodgutter) and the
  Obsidian Lord (Blackstorm, Grimspike): the same sprites, each with a unique. A new test,
  `OracoolAudit.EveryNamedEncounterHasABossToPlace`, holds it.
- **Rune spell levels were lost every New Game.** The hero file (`PlayerPack`) carries spell levels for ids 0-46 only,
  and the five runes (47-51) have had books since v1.5.49. A rune read from its book came back known at level 0 and
  uncastable. A new hero chunk (tag 19, `HeroChunkSpellLevels`) carries every level. The hero golden hash was
  re-baselined for it on purpose.
- **A buff or aura ending at low life killed the hero.** The recalculation cut current life by the bonus. Battle Orders
  running out, or Endurance put out by an F-key, took a wounded hero to 0.
  - A new `CalcPlrInvKeepingLife` caps life by the new maximum (D2's rule) where a bonus ends on its own.
  - Taking an item off keeps vanilla's rule.

## Fixed: skills

- **Plain Leap next to a monster** swung in place for 10 Rage with the leap's cues. It now always leaps.
- **Cleave, Sweep and Sweeping Reed** struck a walking monster twice on a diagonal facing, and paid double Rage. It is
  now struck once.
- **The war cries:**
  - They reached through walls; they now need a line of sight.
  - War Cry's magic ignored magic immunity; it now respects immunity and resistance.
- **The Paladin's auras** no longer burn a monster Conversion turned.

## Fixed: lighting

- **Revisit lights.** This was the open item from the audit passes. Light ids are slots in a 32-entry pool, freed
  lazily. On a revisit, a throwaway monster population is generated and then replaced by the saved one, and the
  throwaway set handed out its own lights. The results:
  - stray glows on empty floor;
  - loaded champions and Luminous monsters walking dark, or sharing a spell's light.

  Now the throwaway set takes no lights (`SuppressMonsterLights`), and `RelightLoadedMonsters` lights the loaded
  uniques and Luminous monsters after the load. A unique's corpse no longer glows on a revisit; that was already broken
  (its light came from the throwaway set).
- **Lightning Rod and Faraday Ring** free the light of a missile they delete. They also no longer delete a charging
  beast's carrier, which froze the monster, unhittable, until the level was left.
- **The mini-map's portal marker** draws from the live portal, on its own level. It showed another floor's tile,
  because of an upstream precedence slip in `PortalOnLevel`.
- **Returning to town with a full light pool** no longer writes `Lights[-1]`.

## Fixed: items

- **Bone-tier gear** asked Required Level 1. Its floor came from the Necromancer's Bone Wand, whose name starts the same
  way. The floor is now 13 for Bone armour; the wand and the scythe ask their own drop level.
- **Relic and Reliquary bases** now roll as amulets. They dropped as stat-less white items, and their 13 uniques were
  about three times rarer than intended.
- **Wyrmhide Arsenal's rungs** keep their fire damage. The lightning power zeroed the fire fields on the shared scratch
  item.

## Fixed: minor

- **Arenas** show their encounter's name on the automap.
- **The reward room's discarded item** clears its tile. Otherwise its tile pointed at the reward, and a later save
  could throw.

## Not changed (open)

- **Tooltips read accumulated fields.** A unique with all-attributes plus strength, or all-resist plus fire, prints the
  sum. A negative durability prints "high durability". Round 8's tooltip track takes this up.
- **Mourning Token** grants fire and lightning only, though its design says all resistances.
- **Charge's clock** runs on real time (`SDL_GetTicks`), so the cooldown ticks while paused.
- **War cry buff refresh:** a recast stores the new rank without recalculating.
- **A Golem left on a floor** comes back with to-hit capped at 255 (saved as a byte).
- **Design, for the user:**
  - Dread bosses in rifts never drop Sealed Maps.
  - Dying in an arena forfeits the map.
  - The Guardian Rift clock starts when the keystone is turned.
- **Tables are append-only:** the Necromancer's unique ids and the set-cursor positions depend on the tables' lengths.

## Tests

- **New:** `OracoolAudit.EveryNamedEncounterHasABossToPlace` and
  `OracoolHeroChunks.BookSpellLevelsPastPlayerPackRoundTrip`.
- **Re-baselined:** `Writehero.pfile_write_hero` (the new chunk).

Debug build and ctest: 884/884. The Debug `diablo.ini` was unchanged through the run (same md5). Nothing seen in play.
