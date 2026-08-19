/**
 * @file oracool/monster_scale.h
 *
 * Oracool: Megaplan Phase 3.2 - monsters that are actually a different size.
 *
 * Phase 0.6 built `oracool/sprite_scale` and left it as "a tool with tests"; this is the wiring it
 * was built for. A Colossal champion is not a recolour and not a bigger health bar - it is a bigger
 * creature, drawn from real scaled sprite data, so the renderer, the outline pass and the corpse all
 * see honest dimensions.
 *
 * ## Why the cache is keyed by (level monster type, percent) and not by monster
 *
 * `CMonster::anims` is SHARED by every monster of a type - that is the same fact that killed the
 * "Fleet" affix, because animation timing lives there too (see lesser_uniques.cpp). Scaling is the
 * one property that can escape it, because the individual monster's `animInfo` binds a sprite LIST,
 * and nothing says that list has to be the shared one.
 *
 * Giving each monster its own copy would be correct and wasteful: six animations of eight directions
 * apiece, per champion. Keying on (type, percent) means every Colossal skeleton on a floor shares
 * one scaled sheet, which is one allocation per distinct size a floor actually uses.
 *
 * The cache holds owned sprite data that the shared `LevelMonsterTypes` blob does not own, so it
 * must be dropped at exactly the moment those sprites are - see ClearMonsterScaleCache, called from
 * FreeMonsters and InitLevelMonsters.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "monster.h"
#include "utils/attributes.h"

namespace devilution::oracool {

/**
 * @brief The sizes a monster can be born at.
 *
 * Deliberately a small closed set rather than a free percentage: a size has to be recognisable
 * across a dark room at a glance, and three of them are already at the limit of what reads as
 * "that one is different" rather than "the renderer is wrong".
 */
enum class MonsterSize : uint8_t {
	Normal,
	/** An ordinary monster born small. Rank and file, not a champion. */
	Runt,
	/** An ordinary monster born large. Still not a champion - it wears no affix and no colour. */
	Giant,
	/** A champion that fills its tile and then some. Only the Colossal affix produces this. */
	Colossal,
	LAST = Colossal,
};

/** @brief What @p size scales sprites to, in percent. Normal is 100 and never allocates. */
unsigned MonsterSizePercent(MonsterSize size);

/** @brief The size @p monster is: its affix if it has one, otherwise its birth roll. */
MonsterSize GetMonsterSize(const Monster &monster);

/**
 * @brief The size an ORDINARY monster is born at, derived rather than stored.
 *
 * No new field on Monster, and therefore no save-format change - which is the whole reason it is
 * derived. A monster's size has to survive a save and reload unchanged (a runt that grew back on
 * load would read as a rendering bug), and the two ways to get that are to store it or to compute
 * it from things already stored. The level seed and the monster's own index are both already
 * saved and both already stable, so the second way costs nothing.
 *
 * NOT the cosmetic RNG stream, despite being purely cosmetic: `CosmeticRnd` is seeded from the wall
 * clock and never saved, so it is precisely the wrong tool for a value that must be reproducible.
 *
 * ## Two rolls, and why it is not one
 *
 * The type rolls FIRST for whether it has an odd size on this floor at all, and only then does each
 * individual roll for whether it is one. That shape is a memory decision as much as a flavour one:
 * the scale cache holds one owned copy of six animations per (type, size), so a single roll per
 * monster would let one floor's skeletons demand a runt sheet AND a giant sheet on top of the
 * normal one. Deciding per type caps it at one extra sheet per type, the same order of cost the
 * Colossal affix already pays.
 *
 * It also reads better. "The skeletons down here run small" is a place with a character; a random
 * scatter of sizes across every type at once is just noise.
 *
 * @param levelSeed the dungeon seed for the floor the monster is on.
 * @param typeIndex the monster's `levelType`.
 * @param monsterId the monster's own index.
 */
DVL_API_FOR_TEST MonsterSize OrdinaryMonsterSize(uint32_t levelSeed, size_t typeIndex, size_t monsterId);

/**
 * @brief The scaled animation @p monster should bind for @p graphic, or nullptr for normal size.
 *
 * Builds the scaled data on first use for this (type, size) pair and shares it thereafter. The
 * returned pointer stays valid until ClearMonsterScaleCache, which is why callers must not hold it
 * across a level change.
 */
const AnimStruct *GetScaledAnim(const Monster &monster, MonsterGraphic graphic);

/**
 * @brief The scaled corpse sprites for @p monster's corpse id, or nullopt for normal size.
 *
 * Separate from GetScaledAnim because a corpse is not one of the six animations - it lives in the
 * global Corpses table, keyed by corpseId, and is drawn long after the monster it came from stopped
 * being drawn. Without this a Colossal champion would shrink at the moment it died.
 */
OptionalClxSpriteListOrSheet GetScaledCorpse(const Monster &monster);

/** @brief Drops every scaled sheet. Called wherever the sprites they were scaled from are freed. */
void ClearMonsterScaleCache();

} // namespace devilution::oracool
