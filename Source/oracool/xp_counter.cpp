#include "oracool/xp_counter.h"

#include <algorithm>

#include <cstdint>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "control.h" // talkflag - the bar hides while chat is open
#include "diablo.h" // MousePosition - the counter shows only under the cursor
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "monster.h"
#include "multi.h"
#include "options.h"
#include "oracool/hud_layout.h"
#include "oracool/oracool.h"
#include "oracool/ui_sound.h" // PlayUiMoveSound - the bar clicks like the HUD's buttons
#include "qol/xpbar.h" // GetXPBarRect - the bar is the counter's hit target and its anchor
#include "player.h"
#include "playerdat.hpp"
#include "utils/format_int.hpp"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

constexpr int CounterHeight = 20;


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
	// Since 2026-09-05 the click target IS the XP bar (user: "hide the xp counter. show it when
	// hovering over xp bar and when clicking on xp bar show dungeon exp pool"), padded two pixels
	// above and below so an 8px strip is not a test of aim. The counter's text is drawn ABOVE the
	// bar - see GetCounterDrawRect - and only while the cursor is here or the button is held.
	const Rectangle bar = GetXPBarRect();
	constexpr int Pad = 2;
	return Rectangle { { bar.position.x, bar.position.y - Pad }, { bar.size.width, bar.size.height + 2 * Pad } };
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
	// Directly above the bar, which is the click target now; the plate's width so a ten-digit
	// remainder is not clipped. Nothing else is drawn in this strip since the bar moved down to the
	// belt, so the text has the air above the belt run to itself.
	const Rectangle bar = GetXPBarRect();
	const Rectangle clickRect { { bar.position.x, bar.position.y - 2 - CounterHeight }, { bar.size.width, CounterHeight } };
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

		// The kill's own formula (2026-09-07), so the counter and the health bar quote one number.
		total += KillExperienceFor(player, static_cast<int>(monster.level(sgGameInitInfo.nDifficulty)),
		    static_cast<int>(monster.exp(sgGameInitInfo.nDifficulty)));
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
	// Audit fix (2026-08-16): the XP counter always guarded max level before asking, but the gain
	// blinker (its Phase-0-era new caller) did not - at the level cap this indexed one past the
	// end of ExpLvlsTbl. The bounds live HERE now, the single authority, so no caller can repeat
	// the mistake: at or past the cap there is no "next level" and the span is a harmless 1.
	if (player._pLevel < 1 || player._pLevel >= static_cast<int>(MaxCharacterLevel))
		return 1;
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

	// Hidden unless asked for (user, 2026-09-05: "hide the xp counter. show it when hovering over xp
	// bar and when clicking on xp bar show dungeon exp pool"). Hover shows the ordinary readout;
	// holding the bar down swaps it for the monsters' pool, as the counter itself used to.
	if (!IsHeld && !GetCounterRect().contains(MousePosition))
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
	// The bar's gates first (round 59 audit): with the bar switched off the counter's strip ate clicks over the world.
	if (!IsPointOverXpBar(mousePosition))
		return false;
	return MyPlayer->_pLevel < MaxCharacterLevel; // nothing is drawn at max level, so the strip isn't there to click
}

bool IsPointOverXpBar(Point mousePosition)
{
	if (!*sgOptions.Gameplay.experienceBar || talkflag || MyPlayer == nullptr)
		return false;
	return GetCounterRect().contains(mousePosition);
}

bool CheckXpCounterButtonClick(Point mousePosition)
{
	// A click on the bar sounds like the HUD's buttons (dev note, 2026-09-27: "titlemov sound on hover and click on exp
	// bar"), and it is the bar's: it no longer walks the hero when the counter's button is switched off.
	if (IsPointOverXpBar(mousePosition)) {
		PlayUiMoveSound();
		if (!*sgOptions.Oracool.xpCounter || !*sgOptions.Oracool.remainingMonsterXpButton || !IsSinglePlayer() || MyPlayer->_pLevel >= MaxCharacterLevel)
			return true;
	}
	if (!*sgOptions.Oracool.xpCounter || !*sgOptions.Oracool.remainingMonsterXpButton || !IsSinglePlayer())
		return false;

	const Player &player = *MyPlayer;
	if (player._pLevel >= MaxCharacterLevel)
		return false;

	if (!IsPointOverXpBar(mousePosition)) // the bar's gates: no bar, nothing to hold (round 59 audit)
		return false;

	IsHeld = true;
	return true;
}

void ReleaseXpCounterButton()
{
	IsHeld = false;
}

} // namespace devilution::oracool
