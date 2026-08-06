#include "oracool/xp_counter.h"

#include <algorithm>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "monster.h"
#include "multi.h"
#include "options.h"
#include "oracool/oracool.h"
#include "player.h"
#include "playerdat.hpp"
#include "utils/format_int.hpp"

namespace devilution::oracool {

namespace {

// Oracool: matches the LOG button's/Game Clock's own height/row (event_log.cpp, game_clock.cpp)
// so all three sit level with each other in the row just below the mini-map.
constexpr int CounterHeight = 20;

// Oracool: user request - press and hold the XP Counter to see this instead of the normal
// remaining-to-next-level readout. Cleared unconditionally on mouse-up (see
// ReleaseXpCounterButton/diablo.cpp's LeftMouseUp), so it can never get stuck true if the mouse
// is released somewhere else, dragged off the button, or a menu interrupts the click.
bool IsHeld = false;

/**
 * @brief The row just below the mini-map, spanning its full width - not the exact width of
 * whichever text happens to be showing (DrawXpCounter sizes that separately, and it changes
 * between the two things this counter can show). A generous, fixed click target is easier to hit
 * than the precise bounds of a handful of digits.
 */
Rectangle GetButtonRect()
{
	const Rectangle miniMap = GetMiniMapScreenRect();
	return Rectangle { { miniMap.position.x, miniMap.position.y + miniMap.size.height + 1 }, { miniMap.size.width, CounterHeight } };
}

/**
 * @brief Sums, over every monster currently alive on this level, the same experience the player
 * would actually be awarded for killing it right now - mirroring AddPlrExperience's own
 * level-difference formula (Source/player.cpp) exactly, including its zero floor. Excludes player
 * minions (golems), which never grant experience when they die (see MonsterDeath).
 */
uint64_t CalcRemainingMonsterXp(const Player &player)
{
	uint64_t total = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &monster = Monsters[ActiveMonsters[i]];
		if (monster.hitPoints <= 0 || monster.isPlayerMinion())
			continue;

		const int monsterLevel = static_cast<int>(monster.level(sgGameInitInfo.nDifficulty));
		const int monsterExp = static_cast<int>(monster.exp(sgGameInitInfo.nDifficulty));
		const int64_t clampedExp = static_cast<int64_t>(monsterExp * (1 + (monsterLevel - player._pLevel) / 10.0));
		total += static_cast<uint64_t>(std::max<int64_t>(clampedExp, 0));
	}
	return total;
}

} // namespace

void DrawXpCounter(const Surface &out)
{
	if (!*sgOptions.Oracool.xpCounter)
		return;

	const Player &player = *MyPlayer;
	if (player._pLevel >= MaxCharacterLevel)
		return;

	const std::string text = IsHeld
	    ? FormatInteger(CalcRemainingMonsterXp(player))
	    : FormatInteger(ExpLvlsTbl[player._pLevel] - player._pExperience);
	const UiFlags color = IsHeld ? UiFlags::ColorWhite : UiFlags::ColorGold;

	// Oracool: sized to the actual rendered text every frame (rather than a fixed box) so it never
	// clips no matter how many digits (or thousands separators, see FormatInteger) the displayed
	// value needs - at level 99 the widest case, a fresh level 1 character, needs 11 digits plus 3
	// separators, but staying dynamic means it's correct at any length without guessing a max up front.
	const int textWidth = GetLineWidth(text, GameFont12, 1);
	const Rectangle miniMap = GetMiniMapScreenRect();
	const Point position { miniMap.position.x + miniMap.size.width / 2 - textWidth / 2, miniMap.position.y + miniMap.size.height + 1 };
	const Rectangle rect { position, { textWidth, CounterHeight } };
	DrawString(out, text, rect, { UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize12 | color });
}

bool CheckXpCounterButtonClick(Point mousePosition)
{
	if (!*sgOptions.Oracool.xpCounter || !*sgOptions.Oracool.remainingMonsterXpButton || !IsSinglePlayer())
		return false;

	const Player &player = *MyPlayer;
	if (player._pLevel >= MaxCharacterLevel)
		return false;

	if (!GetButtonRect().contains(mousePosition))
		return false;

	IsHeld = true;
	return true;
}

void ReleaseXpCounterButton()
{
	IsHeld = false;
}

} // namespace devilution::oracool
