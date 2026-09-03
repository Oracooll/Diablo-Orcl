#pragma once

#include <cstdint>

#include "utils/attributes.h"

namespace devilution {

/**
 * @brief Which of the two character screens is up - and so which set of difficulty thresholds the
 * gate in selgame.cpp applies.
 *
 * DVL_API_FOR_TEST because the difficulty-gate test sets it: WINDOWS_EXPORT_ALL_SYMBOLS exports
 * functions from the test's shared library but not DATA, so without this a test can call the gate
 * and not say which screen it is answering for.
 */
extern DVL_API_FOR_TEST bool selhero_isMultiPlayer;
extern bool selhero_endMenu;

void selhero_Init();
void selhero_List_Init();

} // namespace devilution
