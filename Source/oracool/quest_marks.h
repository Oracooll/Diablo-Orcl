/**
 * @file oracool/quest_marks.h
 *
 * Oracool: the gold ! over a townsperson who has something for you (user, 2026-09-12: "Add a gold
 * glowing ! over the heads of who i should speak to").
 *
 * ## Why the conditions are mirrored rather than asked
 *
 * Whether a towner has news is only ever known by RUNNING its talk function - every one of them starts
 * a quest, takes an item or plays a speech as it decides (towners.cpp). There is no way to ask without
 * doing, so TownerHasQuestNews repeats each function's own gate, in the same order, with the line it
 * came from beside it. The pair is pinned by a test, and the rule for changing either is to change both.
 *
 * It answers for the LOCAL player, because the mark is drawn over the local player's town.
 */
#pragma once

#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/surface.hpp"

namespace devilution {
struct Towner;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief Whether @p towner would do something for the local player right now: give a quest, take the
 * item it wants, or hand over a reward. False for idle chatter and for a shop with nothing to say.
 */
bool TownerHasQuestNews(const Towner &towner);

/**
 * @brief Draws the mark over @p towner when it has news. @p position is the sprite's bottom-left corner
 * and @p sprite the frame being drawn, which together give the top of the head.
 *
 * The glow is a pulse, not a second piece of art: the mark brightens and rises by a pixel about twice a
 * second, which reads as glowing and costs no asset.
 */
void DrawTownerQuestMark(const Surface &out, const Towner &towner, Point position, ClxSprite sprite);

} // namespace devilution::oracool
