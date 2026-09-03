#pragma once

#include "levels/gendung.h"

namespace devilution {

extern _difficulty nDifficulty;

void selgame_GameSelection_Init();
void selgame_GameSelection_Focus(int value);
void selgame_GameSelection_Select(int value);
void selgame_GameSelection_Esc();

/**
 * @brief Tells the difficulty gate which character it is gating, at the moment that character is
 * chosen or created.
 *
 * Step one is picking a hero; step two is the difficulty screen. The level has to be read between
 * the two, and it cannot be read from gSaveNumber there: in single-player that global is not written
 * until the character dialog returns, which is after step two has been and gone. See heroLevel in
 * selgame.cpp for the report this comes from.
 */
void selgame_SetHeroLevel(int level);

/**
 * @brief The character level @p value needs, or 0 if it is open to anyone. Reads the multiplayer
 * flag and the single-player toggle, so it answers for whichever screen is up.
 */
int DifficultyLevelRequirement(int value);

/** @brief Whether the level last handed to selgame_SetHeroLevel clears @p value's requirement. */
bool IsDifficultyUnlocked(int value);
/**
 * Oracool: selgame_Diff_Focus is gone. It rewrote the single description panel as the selection moved,
 * and the difficulty screen now shows all four blurbs at once - see selgame_Difficulty_Init.
 */
void selgame_Diff_Select(int value);
void selgame_Diff_Esc();
void selgame_GameSpeedSelection();
void selgame_Speed_Focus(int value);
void selgame_Speed_Select(int value);
void selgame_Speed_Esc();
void selgame_Password_Init(int value);
void selgame_Password_Select(int value);
void selgame_Password_Esc();

} // namespace devilution
