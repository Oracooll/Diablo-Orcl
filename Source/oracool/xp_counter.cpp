#include "oracool/xp_counter.h"

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "options.h"
#include "player.h"
#include "playerdat.hpp"

namespace devilution::oracool {

namespace {

// Oracool: matches the LOG button's/Game Clock's own height/row (event_log.cpp, game_clock.cpp)
// so all three sit level with each other in the row just below the mini-map.
constexpr int CounterWidth = 60;
constexpr int CounterHeight = 20;

} // namespace

void DrawXpCounter(const Surface &out)
{
	if (!*sgOptions.Oracool.xpCounter)
		return;

	const Player &player = *MyPlayer;
	if (player._pLevel >= MaxCharacterLevel)
		return;

	const uint64_t remaining = ExpLvlsTbl[player._pLevel] - player._pExperience;

	const Rectangle miniMap = GetMiniMapScreenRect();
	const Point position { miniMap.position.x + miniMap.size.width / 2 - CounterWidth / 2, miniMap.position.y + miniMap.size.height + 1 };
	const Rectangle rect { position, { CounterWidth, CounterHeight } };
	DrawString(out, fmt::format("{:d}", remaining), rect, { UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::ColorGold });
}

} // namespace devilution::oracool
