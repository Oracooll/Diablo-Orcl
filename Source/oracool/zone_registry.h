/**
 * @file oracool/zone_registry.h
 *
 * Oracool: Megaplan Phase 0.5 - the world's zones as one data table.
 *
 * "A new dungeon zone" used to mean editing a ladder of `if (level <= N)`s in GetLevelType, plus
 * parallel ladders for music, monster rosters and entrances scattered across the engine. This
 * registry is the single authority those ladders migrate onto: one row describes one zone, and
 * Phase 4's "a zone a week" cadence is filling in a row.
 *
 * Migration is deliberately incremental: GetLevelType is the first consumer (proven identical over
 * every level by test); the other ladders move here as the zones they describe start varying. A
 * row's future fields (tileset override, palette, roster, music, entrance, quest hooks) get added
 * WHEN a first zone needs them - speculative columns would just be guesses with names.
 *
 * Levels remain identified by their currlevel number, and the per-level save files ("perml05",
 * "templ21", ...) are keyed by that number - which is why a NEW zone is additive: it claims fresh
 * level numbers past 24, its waypoints claim fresh bits in the 64-bit mask (hero_chunks.h), and
 * nothing existing moves.
 */
#pragma once

#include <cstdint>

#include "levels/gendung.h"

namespace devilution::oracool {

struct ZoneDefinition {
	/** @brief First and last currlevel this zone spans, inclusive. */
	uint8_t firstLevel;
	uint8_t lastLevel;
	dungeon_type levelType;
	/** @brief Untranslated display name; callers wrap in _() where it reaches the screen. */
	const char *name;
	/**
	 * @brief Optional .pal path replacing the level type's normal palette roll - the recolor-zone
	 * mechanism (Phase 4.1: Hellfire's own Crypt-is-recolored-Cathedral trick). nullptr = the
	 * vanilla per-type palette selection in LoadRndLvlPal. Variant palettes are produced by
	 * tools/PaletteVariant.ps1.
	 */
	const char *paletteOverride;
};

/** @brief The palette override for @p level's zone, or nullptr when the vanilla roll applies. */
const char *GetZonePaletteOverride(int level);

/** @brief The zone containing @p level, or nullptr for a level no zone claims. */
const ZoneDefinition *FindZoneForLevel(int level);

/** @brief Number of registered zones (for iteration). */
int GetZoneCount();

/** @brief Zone by registry index [0, GetZoneCount()). */
const ZoneDefinition &GetZone(int index);

} // namespace devilution::oracool
