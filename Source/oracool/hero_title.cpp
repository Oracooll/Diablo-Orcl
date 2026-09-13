#include "oracool/hero_title.h"

#include <algorithm>
#include <array>

#include "levels/gendung.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

struct TitleRung {
	const char *name;
	UiFlags color;
};

/** @brief Indexed by pDiabloKillLevel: no kill, then one rung per difficulty (user, 2026-09-13). */
constexpr std::array<TitleRung, 5> Rungs { {
	{ N_("Adventurer"), UiFlags::ColorWhite },
	{ N_("Slayer"), UiFlags::ColorBlue },
	{ N_("Champion"), UiFlags::ColorYellow3 },
	{ N_("Conqueror"), UiFlags::ColorWhitegold },
	{ N_("Sanctified"), UiFlags::ColorBeige2 },
} };
// A difficulty added to the ladder needs a rung, or its conquerors would read one title short.
static_assert(Rungs.size() == static_cast<size_t>(DIFF_LAST) + 2, "one title per difficulty, plus the hero who has not killed Diablo");

const TitleRung &RungFor(uint8_t diabloKillLevel)
{
	// Clamped: the kill level is a saved byte, and a value past the ladder is still a hero who beat
	// the hardest difficulty there is.
	return Rungs[std::min<size_t>(diabloKillLevel, Rungs.size() - 1)];
}

} // namespace

const char *HeroTitleFor(uint8_t diabloKillLevel)
{
	return RungFor(diabloKillLevel).name;
}

UiFlags HeroTitleColorFor(uint8_t diabloKillLevel)
{
	return RungFor(diabloKillLevel).color;
}

} // namespace devilution::oracool
