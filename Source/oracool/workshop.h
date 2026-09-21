/**
 * @file oracool/workshop.h
 *
 * Oracool: the artisans' WORKSHOPS - a 340x720 page in the shop panel's place, docked and sized like
 * every other of Griswold's windows, with its tabs in the column beside it.
 *
 * Gillian's MYSTIC WORKSHOP is the user's own plan (2026-09-21, the Artisans page's Gillian card, and
 * his verdicts in that page's `mystic` collection):
 *
 *  - **Reroll Affix.** A 2x3 slot that holds exactly one item whatever its size; the item's own rolled
 *    affixes listed and selectable; a Reroll button. The roll offers a MENU - the affix as it stands
 *    beside three alternatives - and the player picks one ("Go with Menu"). The pool is not restricted
 *    to the chosen affix's family: "That doesnt exclude also increasing the current affix if such rolls
 *    in the pools of options". The first reroll LOCKS the slot, and only it can be rerolled afterwards.
 *  - **Imbue.** The same slot; the shards already on the item listed; Imbue takes one from the pack,
 *    Remove takes the selected one off, Cleanse takes them all - "User has to be able to Cleanse an Item
 *    or Remove particular Imbuement. All cost increasing amounts of gold."
 *
 * The Jeweller's workshop is Ogden's recipe book on the same 340x720 page - see the docked list skin in
 * levski_roar.cpp - so this module is the Mystic's two tabs plus the frame both wear.
 *
 * ART: none yet. The canvas is the shared 340x720 side panel and every control is drawn in code, as a
 * placeholder the user will paint over ("Use placeholder canvas and ui buttons. I will supply assets
 * later"), the way the artisan canvas stood in for the Cube's window.
 *
 * THE RISING COST is per item and lives for the game, in this file's own table keyed by the item's seed.
 * It is deliberately NOT an item field yet: that is an item-format bump, and the user's V1 always starts
 * a new game, so a per-game count is honest until the format is bumped for other reasons.
 */
#pragma once

#include <cstdint>

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief Whose workshop is open. The Jeweller's page is the transmute window's docked skin; this is the Mystic's. */
enum class WorkshopHost : uint8_t {
	Mystic,
};

/** @brief Opens the workshop on @p host, closing whatever else shares the slot. */
void OpenWorkshop(WorkshopHost host);
/** @brief Closes it and gives the held item back to the pack; refuses (and says so) when the pack is full. */
void CloseWorkshop();
bool IsWorkshopOpen();

/** @brief The page's rect, docked where the shop panel sits. Empty when it is closed. */
Rectangle GetWorkshopRect();

/** @brief Whether @p position is on the page or on its tab column - what a click router must ask. */
bool IsPointOverWorkshop(Point position);

void DrawWorkshop(const Surface &out);
/** @brief Routes a click. True when the workshop consumed it. */
bool CheckWorkshopClick(Point position);
/**
 * @brief LeftMouseUp: the pressed button acts, and only if the release lands inside it
 * (feedback_button_press_and_sound, the release rule of v1.12.102).
 */
void ReleaseWorkshopButton();
/** @brief The hover text for whatever is under the cursor. True when it wrote one. */
bool SetWorkshopHoverInfoString();

/** @brief Game teardown: the window, the item it holds and the per-item counters are all this file's statics. */
void ResetWorkshopForNewGame();

} // namespace devilution::oracool
