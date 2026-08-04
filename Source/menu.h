/**
 * @file menu.h
 *
 * Interface of functions for interacting with the main menu.
 */
#pragma once

#include <cstdint>

#include "multi.h"
#include "utils/attributes.h"

namespace devilution {

extern DVL_API_FOR_TEST uint32_t gSaveNumber;

bool mainmenu_select_hero_dialog(GameData *gameData);
void mainmenu_loop();

} // namespace devilution
