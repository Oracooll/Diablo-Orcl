/**
 * @file xpbar.cpp
 *
 * Adds XP bar QoL feature
 *
 * Oracool: HUD art pass, compact redesign (2026-08-10) - the XP bar is retired entirely. The
 * user's second plate design dropped the XP groove ("Exp bar is gone. I will use the exp counter
 * under the minimap" - the Oracool XP Counter, Source/oracool/xp_counter.cpp, already shows the
 * same per-level progress on demand). All entry points stay as no-ops rather than being unwired
 * from their call sites (scrollrt.cpp, control.cpp's CheckPanelInfo, diablo.cpp's Init/Free), so
 * bringing a bar back later - e.g. if a future plate design reintroduces a groove - is a one-file
 * change here rather than a call-site hunt.
 */
#include "xpbar.h"

namespace devilution {

void InitXPBar()
{
}

void FreeXPBar()
{
}

void DrawXPBar(const Surface & /*out*/)
{
}

bool CheckXPBarInfo()
{
	return false;
}

} // namespace devilution
