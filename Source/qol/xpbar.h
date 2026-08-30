/**
 * @file xpbar.h
 *
 * Adds XP bar QoL feature
 */
#pragma once

#include "engine/rectangle.hpp"

namespace devilution {

struct Surface;

void InitXPBar();
void FreeXPBar();

/**
 * @brief The bar's screen rect, derived from the XP counter above it and the belt below.
 *
 * Exported 2026-08-30 (external audit UI-01) so the drawing, the geometry tests and any future hit
 * test all read ONE rect. It was file-local, which is how an 8px bar came to be drawn into a 6px
 * gap with nothing able to notice.
 */
Rectangle GetXPBarRect();

void DrawXPBar(const Surface &out);
bool CheckXPBarInfo();

} // namespace devilution
