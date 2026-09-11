/**
 * @file oracool/unique_affixes.h
 *
 * The affix vocabulary the 250-unique expansion is written in, and what each token means HERE.
 *
 * The package (Oracool.MPQ/02-source-art/unique-items/unique-item-expansion-250.zip) is data only:
 * 250 items with bases, requirements, drop bands, lore, visual briefs and exact affix tokens. It
 * declares an `enginePower` per affix and an implementation tier per item, and it is mostly right -
 * but "mostly" is why this table exists rather than the generator trusting the package's own column.
 *
 * ONE OF ITS MAPPINGS IS WRONG FOR THIS ENGINE, and it is the same shape as the bug found in the
 * item-set keywords the day before: `flat_armor` is declared as IPL_SETAC, and SaveItemPower does
 * `item._iAC = r` for that power - it OVERWRITES the base's armour rather than adding to it. 109 of
 * the 250 items carry flat_armor. Taken at the package's word, a Gothic Plate with 42 armour and
 * "+8 flat armor" would have come out of the roller with 8. A downgrade, printed as a bonus, on
 * nearly half the expansion.
 *
 * So every token is resolved through this table, and the package's `enginePower` is used only as a
 * cross-check the generator reports on. Same three fidelities the set keywords use:
 *
 *   Power  - the engine does what the token says.
 *   Approx - the engine does something adjacent. The note says what was traded away.
 *   Inert  - no channel. The note says what building it would take.
 *
 * @see oracool/item_set_stats.h for the same idea applied to the fifteen item sets, and for the
 * two bugs that made the "audit every token against SaveItemPower" rule non-negotiable here.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "itemdat.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

enum class UniqueAffixFidelity : uint8_t {
	Power,
	Approx,
	Inert,
};

struct UniqueAffixMapping {
	/** @brief The token exactly as it appears in unique-items.json. */
	const char *token;
	UniqueAffixFidelity fidelity;
	/** @brief What it compiles to, or IPL_INVALID when inert. */
	item_effect_type power;
	/** @brief For Approx, what differs. For Inert, what it would need. Developer-facing. */
	const char *note;
};

/**
 * @brief Every token the 250 items use (43), plus two the registry documents that no item uses yet,
 * plus faster_cast_rate_percent, which the generator's own table authors (2026-09-11).
 *
 * `life_steal_percent` and `mana_steal_percent` are in AFFIX_IMPLEMENTATION.md's stock table but
 * appear on none of the 250. They are mapped here anyway, with the 3-or-5 trap recorded, so the
 * next batch cannot rediscover it the hard way.
 */
constexpr size_t UniqueAffixMappingCount = 46;

extern DVL_API_FOR_TEST const UniqueAffixMapping UniqueAffixMappings[UniqueAffixMappingCount];

/** @brief The row for @p token, or nullptr if the vocabulary does not contain it. */
const UniqueAffixMapping *FindUniqueAffix(string_view token);

/** @brief Whether the token resolves to something the wearer will actually feel. */
bool IsUniqueAffixLive(string_view token);

} // namespace devilution::oracool
