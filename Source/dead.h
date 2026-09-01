/**
 * @file dead.h
 *
 * Interface of functions for placing dead monsters.
 */
#pragma once

#include <array>
#include <cstdint>

#include "engine.h"
#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "utils/attributes.h"

namespace devilution {

static constexpr unsigned MaxCorpses = 31;

struct Corpse {
	OptionalClxSpriteListOrSheet sprites;
	int frame;
	uint16_t width;
	uint8_t translationPaletteIndex;

	/**
	 * @brief Returns the sprite list for a given direction.
	 *
	 * @param direction One of the 16 directions. Valid range: [0, 15].
	 * @return ClxSpriteList
	 */
	/**
	 * @brief The sprite list for @p direction, or nullopt when this corpse has no graphics.
	 *
	 * Oracool audit (2026-08-16): guarded to match AnimStruct::spritesForDirection. A Corpse entry
	 * with no sprites is reachable between a level being torn down and the next one's graphics
	 * loading, and dCorpse still names it during that window.
	 */
	[[nodiscard]] OptionalClxSpriteList spritesForDirection(Direction direction) const
	{
		if (!sprites)
			return std::nullopt;
		return sprites->isSheet() ? sprites->sheet()[static_cast<size_t>(direction)] : sprites->list();
	}
};

extern DVL_API_FOR_TEST Corpse Corpses[MaxCorpses];
extern DVL_API_FOR_TEST int8_t stonendx;

DVL_API_FOR_TEST void InitCorpses();
void AddCorpse(Point tilePosition, int8_t dv, Direction ddir);
void MoveLightsToCorpses();

} // namespace devilution
