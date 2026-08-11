/**
 * @file automap.h
 *
 * Interface of the in-game map overlay.
 */
#pragma once

#include <cstdint>

#include "engine.h"
#include "engine/displacement.hpp"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "levels/gendung.h"
#include "utils/attributes.h"

namespace devilution {

enum MapExplorationType : uint8_t {
	/** unexplored map tile */
	MAP_EXP_NONE,
	/** map tile explored in vanilla - compatibility reasons */
	MAP_EXP_OLD,
	/** map explored by a shrine */
	MAP_EXP_SHRINE,
	/** map tile explored by someone else in multiplayer */
	MAP_EXP_OTHERS,
	/** map tile explored by current player */
	MAP_EXP_SELF,
};

/** Specifies whether the automap is enabled. */
extern DVL_API_FOR_TEST bool AutomapActive;
/**
 * @brief Oracool: specifies whether the small always-corner mini-map overlay is enabled. Mutually
 * exclusive with AutomapActive - DoAutoMap (control.cpp) cycles TAB through no map -> mini-map ->
 * full map -> no map, rather than the vanilla plain on/off toggle.
 */
extern DVL_API_FOR_TEST bool MiniMapActive;
/** Tracks the explored areas of the map. */
extern uint8_t AutomapView[DMAXX][DMAXY];
/** Specifies the scale of the automap. */
extern DVL_API_FOR_TEST int AutoMapScale;
extern DVL_API_FOR_TEST Displacement AutomapOffset;
/** @brief Oracool: the mini-map's own scale, independent of AutoMapScale - see MiniMapScaleMin/Max. */
extern DVL_API_FOR_TEST int MiniMapScale;
/** @brief Oracool: the mini-map's own pan offset, independent of AutomapOffset. */
extern DVL_API_FOR_TEST Displacement MiniMapOffset;

/** @brief Oracool: the most zoomed-out the mini-map can go (ALT+Wheel down / ALT+middle-click limit). */
constexpr int MiniMapScaleMin = 6;
/** @brief Oracool: the most zoomed-in the mini-map can go (ALT+Wheel up / ALT+middle-click limit). */
constexpr int MiniMapScaleMax = 30;

inline int AmLine(int x)
{
	assert(x >= 4 && x <= 64);
	assert((x & (x - 1)) == 0);
	return AutoMapScale * x / 100;
}

/**
 * @brief Initializes the automap.
 */
void InitAutomapOnce();

/**
 * @brief Loads the mapping between tile IDs and automap shapes.
 */
void InitAutomap();

/**
 * @brief Displays the automap.
 */
void StartAutomap();

/**
 * @brief Scrolls the automap upwards.
 */
void AutomapUp();

/**
 * @brief Scrolls the automap downwards.
 */
void AutomapDown();

/**
 * @brief Scrolls the automap leftwards.
 */
void AutomapLeft();

/**
 * @brief Scrolls the automap rightwards.
 */
void AutomapRight();

/**
 * @brief Increases the zoom level of the automap.
 */
void AutomapZoomIn();

/**
 * @brief Decreases the zoom level of the automap.
 */
void AutomapZoomOut();

/**
 * @brief Oracool: jumps the automap's zoom straight to whichever limit (50 or 200) it isn't
 * already at or closer to - used by CTRL+middle-click, only while the automap is open.
 */
void ToggleAutomapZoom();

/**
 * @brief Oracool: scrolls the mini-map's own view leftwards/rightwards/up/downwards -
 * independent of the full automap's AutomapUp/Down/Left/Right - used by ALT+Arrow keys.
 */
void MiniMapUp();
void MiniMapDown();
void MiniMapLeft();
void MiniMapRight();

/**
 * @brief Oracool: resets the mini-map's pan offset back to centered-on-character - used by
 * ALT+` (the key next to 1).
 */
void RecenterMiniMap();

/**
 * @brief Oracool: increases/decreases the mini-map's own zoom level, clamped to
 * MiniMapScaleMin/MiniMapScaleMax - used by ALT+Wheel.
 */
void MiniMapZoomIn();
void MiniMapZoomOut();

/**
 * @brief Oracool: jumps the mini-map's zoom straight to whichever limit (MiniMapScaleMin or
 * MiniMapScaleMax) it isn't already at or closer to - used by ALT+middle-click.
 */
void ToggleMiniMapZoom();

/**
 * @brief Renders the automap to the given buffer.
 */
void DrawAutomap(const Surface &out);

/**
 * @brief Oracool: renders a small, heavily zoomed-out automap into a fixed corner of the screen,
 * on top of the live game view rather than replacing it. Shares all of DrawAutomap's tile/player
 * rendering via DrawAutomapCore - see automap.cpp.
 */
void DrawMiniMap(const Surface &out);

/**
 * @brief Oracool: the mini-map's actual on-screen rectangle (its diamond content's bounding box,
 * positioned exactly where DrawMiniMap draws it - not the MiniMapSize zoom-level constant) -
 * exposed so other UI can align itself against the mini-map's real edges instead of duplicating
 * its position/size formula. The event log window uses this to match the mini-map's width and
 * both its left and right edges, and to anchor its own top boundary below the mini-map's bottom.
 */
Rectangle GetMiniMapScreenRect();

/**
 * @brief Updates automap explorer at point if value is higher than existing.
 */
void UpdateAutomapExplorer(Point map, MapExplorationType explorer);

/**
 * @brief Marks the given coordinate as within view on the automap.
 */
void SetAutomapView(Point tile, MapExplorationType explorer);

/**
 * @brief Resets the zoom level of the automap.
 */
void AutomapZoomReset();

} // namespace devilution
