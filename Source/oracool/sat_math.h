#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace devilution::oracool {

/**
 * @brief @p value * @p percent / 100, computed in 64 bits and clamped to int.
 *
 * Round 27 audit: a high-level Fireball is half a million points before the <<6 of the 1/64 units, and the damage
 * passives' "dam * percent / 100" wrapped int from about +40% - the hit landed hundreds of thousands short or long, and
 * Life Tap's share of it came out negative and drained the Necromancer. Truncates toward zero exactly as the int
 * expression does, so every value that did not overflow is unchanged.
 */
inline int PercentOfSat(int value, int percent)
{
	const int64_t result = static_cast<int64_t>(value) * percent / 100;
	return static_cast<int>(std::clamp<int64_t>(result, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
}

/** @brief @p value plus @p percent of it, as PercentOfSat: the "dam += dam * percent / 100" of the damage passives. */
inline int AddPercentSat(int value, int percent)
{
	const int64_t result = static_cast<int64_t>(value) + static_cast<int64_t>(value) * percent / 100;
	return static_cast<int>(std::clamp<int64_t>(result, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
}

} // namespace devilution::oracool
