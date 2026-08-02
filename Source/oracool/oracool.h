#pragma once

#include "multi.h"

namespace devilution::oracool {

inline constexpr char EditionName[] = "Diablo Oracool Edition";

inline bool IsSinglePlayer()
{
	return !gbIsMultiplayer;
}

} // namespace devilution::oracool
