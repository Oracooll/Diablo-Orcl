/**
 * @file oracool/ui_sound.h
 *
 * Oracool: the two interface clicks, so every fork control answers with the same sound vanilla's own
 * menus use. The pair is taken from vanilla, not chosen: the pause menu (gmenu.cpp), the quest log
 * (quests.cpp) and the stores (stores.cpp) all play IS_TITLEMOV to move and IS_TITLSLCT to act.
 *
 * Call these only on a path that is otherwise silent. A control whose action already makes a sound
 * (coins, an item landing, a skill's own cue) keeps that sound and gets nothing from here - two
 * sounds for one click reads as two things having happened.
 *
 * In game only: PlaySFX reads MyPlayer, so the front end (DiabloUI) uses its own UiPlayMoveSound.
 */
#pragma once

#include "effects.h"
#include "player.h"

namespace devilution::oracool {

/** @brief Switching, toggling, closing, selecting a row. */
inline void PlayUiMoveSound()
{
	// PlaySfxPriv reads MyPlayer before it asks whether there is any audio at all, and some tests
	// drive these handlers (the close button, the monument) without a player. Always set in play.
	if (MyPlayer != nullptr)
		PlaySFX(IS_TITLEMOV);
}

/** @brief Confirming, opening, choosing. */
inline void PlayUiSelectSound()
{
	if (MyPlayer != nullptr) // see PlayUiMoveSound
		PlaySFX(IS_TITLSLCT);
}

} // namespace devilution::oracool
