#include "oracool/xp_counter.h"

#include <algorithm>

#include <cstdint>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "monster.h"
#include "multi.h"
#include "options.h"
#include "oracool/hud_layout.h"
#include "oracool/oracool.h"
#include "player.h"
#include "playerdat.hpp"
#include "utils/format_int.hpp"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

constexpr int CounterHeight = 20;

// Oracool: user request (2026-08-11) - moved from the row under the mini-map to sit just above the
// belt's numbered cells (1-4), whose "1".."4" labels are baked into the plate art. The offset is
// measured DOWN from the plate's top edge, so a larger value sits lower; tuned by eye in play
// (started 14px higher, at -2).
constexpr int CounterOffsetBelowPlateTop = 12;

// Oracool: user request - press and hold the XP Counter to see this instead of the normal
// remaining-to-next-level readout. Cleared unconditionally on mouse-up (see
// ReleaseXpCounterButton/diablo.cpp's LeftMouseUp), so it can never get stuck true if the mouse
// is released somewhere else, dragged off the button, or a menu interrupts the click.
bool IsHeld = false;

/**
 * @brief The strip directly above the belt's four numbered cells, spanning them end to end - not
 * the exact width of whichever text happens to be showing (that changes between the two things
 * this counter can display). A generous, fixed click target is easier to hit than the precise
 * bounds of a handful of digits.
 */
Rectangle GetCounterRect()
{
	// Belt cells 1..4 are visible indices 1-4; span from the left edge of the first to the right
	// edge of the last so the readout is centred over the numbered run specifically, not the whole
	// plate (which also carries the Menu and Portal buttons).
	const Rectangle first = GetBeltSlotRect(1);
	const Rectangle last = GetBeltSlotRect(4);
	const int left = first.position.x;
	const int width = last.position.x + last.size.width - left;
	const int bottom = GetMiddleHudRect().position.y + CounterOffsetBelowPlateTop;
	return Rectangle { { left, bottom - CounterHeight }, { width, CounterHeight } };
}

/**
 * @brief The box the text is DRAWN into, as opposed to the one that is clicked.
 *
 * Oracool: user report - a high-level character showed "220,000,000 / 10". Not a wrong number: the
 * real string is "220,000,000 / 100%" and DrawString was clipping it, because the click target
 * spans only belt cells 1-4 (~150px) and a ten-digit remainder plus a three-digit percentage needs
 * more than that.
 *
 * Widening the click target instead would have pushed it out over the plate's two skill wells and
 * let it swallow clicks meant for them, so the two rects are separate. They share a centre - belt
 * cells 1-4 are themselves centred on the plate - so the readout does not shift.
 */
Rectangle GetCounterDrawRect()
{
	const Rectangle clickRect = GetCounterRect();
	const Rectangle plate = GetMiddleHudRect();
	return Rectangle { { plate.position.x, clickRect.position.y }, { plate.size.width, clickRect.size.height } };
}

// LevelExperienceSpan moved to the public GetLevelExperienceSpan below (the XP gain blinker needs
// the same denominator for its percentage - user request, 2026-08-16).

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

Rectangle GetXpCounterDrawRect()
{
	return GetCounterDrawRect();
}

uint64_t GetLevelExperienceSpan(const Player &player)
{
	const uint64_t levelStart = ExpLvlsTbl[player._pLevel - 1];
	const uint64_t levelEnd = ExpLvlsTbl[player._pLevel];
	return (levelEnd > levelStart) ? (levelEnd - levelStart) : 1; // guard against a zero divisor
}

void DrawXpCounter(const Surface &out)
{
	if (!*sgOptions.Oracool.xpCounter)
		return;

	const Player &player = *MyPlayer;
	if (player._pLevel >= MaxCharacterLevel)
		return;

	// Oracool: user request (2026-08-11) - both readouts carry a percentage, expressed against the
	// experience gap between this level and the next. Normally that reads how much of the current
	// level is still to go ("2,000 / 100%" on a freshly-levelled character); while held, it reads
	// how much of a full level the monsters still alive on this floor are worth - so at a glance
	// you can tell whether clearing the level will level you up.
	const uint64_t span = GetLevelExperienceSpan(player);
	const uint64_t value = IsHeld
	    ? CalcRemainingMonsterXp(player)
	    : (ExpLvlsTbl[player._pLevel] - player._pExperience);
	const uint64_t percent = value * 100 / span;
	const std::string text = StrCat(FormatInteger(value), " / ", FormatInteger(percent), "%");
	const UiFlags color = IsHeld ? UiFlags::ColorWhite : UiFlags::ColorGold;

	// Centred in the strip above belt cells 1-4. The box is a fixed span rather than the text's own
	// width, so the readout stays put as digits come and go - and spans the whole plate rather than
	// just the cells, so a ten-digit remainder is not clipped (see GetCounterDrawRect).
	DrawString(out, text, GetCounterDrawRect(), { UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize12 | color });
}

bool IsPointOverXpCounter(Point mousePosition)
{
	if (!*sgOptions.Oracool.xpCounter)
		return false;
	if (MyPlayer->_pLevel >= MaxCharacterLevel)
		return false; // nothing is drawn at max level, so the strip isn't there to click
	return GetCounterRect().contains(mousePosition);
}

bool CheckXpCounterButtonClick(Point mousePosition)
{
	if (!*sgOptions.Oracool.xpCounter || !*sgOptions.Oracool.remainingMonsterXpButton || !IsSinglePlayer())
		return false;

	const Player &player = *MyPlayer;
	if (player._pLevel >= MaxCharacterLevel)
		return false;

	if (!GetCounterRect().contains(mousePosition))
		return false;

	IsHeld = true;
	return true;
}

void ReleaseXpCounterButton()
{
	IsHeld = false;
}

} // namespace devilution::oracool
