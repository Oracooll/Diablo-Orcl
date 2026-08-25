/**
 * @file oracool/named_encounters.h
 *
 * Oracool: D2MXL-to-ORCL Phase 4 - named encounters with fixed rewards.
 *
 * Median XL's uberquests: a specific hard fight, in a specific place, with a **known** reward. What
 * turns farming into a destination rather than a lottery.
 *
 * ## Why this is not the tileset pipeline
 *
 * It was scoped as a Large blocked on art, because "a place" sounded like a new dungeon. The place
 * already exists three times over: SL_ARENA_CHURCH, SL_ARENA_HELL and SL_ARENA_CIRCLE_OF_LIFE are
 * set levels that load from shipped .dun files in three different tilesets, each with an exit
 * trigger already wired back to town. Entering one is two lines, and populating one is exactly what
 * every quest set level does - AddMonsterType, then place.
 *
 * So this phase costs no art, no tileset and no DRLG work. What it adds is the FRAME: a way in that
 * is not a debug command, a way to know the reward before you go, and a reward worth going for.
 *
 * ## The frame
 *
 *  - A **Sealed Map** drops from a Dread boss - one per encounter, a generated item like every
 *    other family here. It is CONSUMED when used, so how often you can run an encounter is bounded
 *    by drops rather than by a cooldown nobody can see.
 *  - Using it in town opens the encounter. The map's own description says what it opens and what it
 *    pays, which is where "known reward" lives - cheaper and clearer than wiring the quest log, and
 *    a player who has never seen one still knows before they go.
 *  - The floor holds ONE Dread boss and its escort, and nothing else. The encounter is the fight; a
 *    room of ordinary monsters would dilute it.
 *  - The reward is a **unique charm**, guaranteed on the kill. Three encounters give three signature
 *    charms, which makes the three-charm active cap a decision rather than an inventory rule.
 *
 * ## The honest limit
 *
 * Three rooms with one fight each is the right first version and it is not Fauztinville. A real
 * uberquest AREA needs the zone pipeline, which is still blocked on art. This is the mechanism
 * landing early, in the cheapest place that can hold it.
 */
#pragma once

#include <cstdint>

#include "levels/gendung.h"
#include "monstdat.h"

namespace devilution {
struct Player;
struct Monster;
} // namespace devilution

namespace devilution::oracool {

/** @brief The three encounters. */
enum class NamedEncounter : uint8_t {
	/** The Church arena - the shallowest, and the one a mid-level character can attempt. */
	SunkenChapel,
	/** The Circle of Life arena. */
	RingOfMourning,
	/** The Hell arena - the deepest. */
	EmberVault,
	LAST = EmberVault,
};

constexpr int NamedEncounterCount = static_cast<int>(NamedEncounter::LAST) + 1;

/** @brief @p encounter's display name, untranslated. */
const char *NamedEncounterName(NamedEncounter encounter);

/** @brief The set level @p encounter runs on. */
_setlevels NamedEncounterLevel(NamedEncounter encounter);

/** @brief The tileset that set level must be loaded with - see the risk note in the plan. */
dungeon_type NamedEncounterDungeon(NamedEncounter encounter);

/** @brief The monster type whose sprites the encounter's boss borrows. */
_monster_id NamedEncounterMonster(NamedEncounter encounter);

/**
 * @brief The dungeon floor @p level counts as for the area-level ladder, if it is an arena.
 *
 * Exists so CurrentAreaLevel stops sending all three encounters to its floor-1 default.
 */
bool NamedEncounterFloorForSetLevel(_setlevels level, int &floor);

/** @brief The Sealed Map item that opens @p encounter. */
int NamedEncounterMapItem(NamedEncounter encounter);

/** @brief The unique charm @p encounter pays on the kill. */
int NamedEncounterReward(NamedEncounter encounter);

/** @brief The encounter @p mapIdx opens, or nullopt if it is not a Sealed Map. */
bool EncounterForMapItem(int mapIdx, NamedEncounter &out);

/** @brief The encounter running on the current set level, or false if none is. */
bool CurrentNamedEncounter(NamedEncounter &out);

/**
 * @brief Opens @p encounter from town. False if the player is not somewhere it can be opened from.
 *
 * Sets setlvltype before StartNewLvl, which is not optional - the /arena command does the same and
 * that is the tell: without it the level loads with the wrong tileset.
 */
bool EnterNamedEncounter(Player &player, NamedEncounter encounter);

/**
 * @brief Rolls a Sealed Map from @p monster's death. Does nothing unless it is a Dread boss.
 *
 * The way IN, and until 2026-08-26 it did not exist. NamedEncounterMapItem had no production
 * caller at all: the maps are excluded from ordinary generation on purpose - they are not pool
 * items - and the replacement hook this file's own header describes was never written. Every
 * named encounter was therefore unreachable in normal play, and the only thing that had ever
 * opened one was a debug command.
 *
 * Called from the monster loot hook beside the other Oracool families, and for the same reason
 * they are there rather than in the seeded pool.
 */
void TrySpawnSealedMap(const Monster &monster, bool sendmsg);

/** @brief Whether @p monster is the boss of the encounter currently running. */
bool IsNamedEncounterBoss(const Monster &monster);

/** @brief Pays @p encounter's reward at @p monster's feet. Called once, on the boss's death. */
void AwardNamedEncounter(const Monster &monster);

} // namespace devilution::oracool
