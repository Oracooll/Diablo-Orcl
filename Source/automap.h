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
 * @brief Oracool: the mini-map's actual on-screen pixel width (its diamond content's bounding
 * box, not the MiniMapSize zoom-level constant) - exposed so other UI that wants to visually
 * match the mini-map's width (the event log window) doesn't have to duplicate the formula.
 */
int GetMiniMapWidth();

/**
 * @brief Oracool: the y-coordinate just past the mini-map's bottom edge (its screen-space margin
 * plus its diamond content's height) - exposed so other UI can anchor itself relative to where
 * the mini-map actually ends on screen (the event log window's top boundary).
 */
int GetMiniMapBottom();

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
