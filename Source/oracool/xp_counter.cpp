#include "oracool/xp_counter.h"

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "options.h"
#include "player.h"
#include "playerdat.hpp"
#include "utils/format_int.hpp"

namespace devilution::oracool {

namespace {

// Oracool: matches the LOG button's/Game Clock's own height/row (event_log.cpp, game_clock.cpp)
// so all three sit level with each other in the row just below the mini-map.
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
	const std::string text = FormatInteger(remaining);

	// Oracool: sized to the actual rendered text every frame (rather than a fixed box) so it never
	// clips no matter how many digits (or thousands separators, see FormatInteger) the remaining-XP
	// value needs - at level 99 the widest case, a fresh level 1 character, needs 11 digits plus 3
	// separators, but staying dynamic means it's correct at any length without guessing a max up front.
	const int textWidth = GetLineWidth(text, GameFont12, 1);
	const Rectangle miniMap = GetMiniMapScreenRect();
	const Point position { miniMap.position.x + miniMap.size.width / 2 - textWidth / 2, miniMap.position.y + miniMap.size.height + 1 };
	const Rectangle rect { position, { textWidth, CounterHeight } };
	DrawString(out, text, rect, { UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::ColorGold });
}

} // namespace devilution::oracool
