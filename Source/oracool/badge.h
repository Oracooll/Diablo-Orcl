/**
 * @file oracool/badge.h
 *
 * Oracool: the one way a badge is drawn - a hotkey, a rank, any short label stuck to an icon.
 *
 * User, 2026-08-31: "let's introduce a new redesign in skills/spells/auras badges (hotkey/level) -
 * put a dark, transparent backing on the badge, and use white font. it will make badges more
 * visible. use this template everywhere badges exist."
 *
 * WHY A BACKING AT ALL. A badge is a few glyphs laid directly over an icon, and an icon is the one
 * surface in the game guaranteed to be busy - that is what makes it an icon. Before this, each site
 * solved the legibility problem its own way and none of them solved it well: the Abilities window
 * drew bare coloured text, the speedbook drew white text with a one-pixel outline, and the skill
 * picker drew whitegold on whatever the art happened to be underneath. All three vanish over a light
 * patch of sprite. A dark plate under the glyphs is the fix that does not depend on the art.
 *
 * WHY ONE MODULE. Four sites drew badges four ways, which is how they came to disagree in the first
 * place. The same argument as oracool/window_close.h, and the same shape: a helper every caller uses
 * rather than a convention every caller remembers.
 *
 * THE COLOUR IS ALMOST ALWAYS WHITE. The Abilities window used red for the left button and yellow
 * for the right; that distinction now lives entirely in which CORNER the badge sits in, and in the
 * assignment rings, which keep their colours. One badge colour is what lets the plate be one colour.
 *
 * The one exception, and it is a WARNING rather than a category: a staff's remaining charges go red
 * below ten (user, 2026-09-03: "0-9 charges to use red font. 10 and over - regular white font").
 * That is the badge telling you the thing it labels is about to run out, which no corner can say.
 */
#pragma once

#include "DiabloUI/ui_flags.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "engine/surface.hpp"
#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

/** @brief Where on the host rect a badge sits.
 *
 * The two TOP corners are the hotkey positions - left for the left button, right for the right -
 * and BottomRight is the rank, everywhere a rank is shown (user, 2026-09-02: "move skell level
 * badges to botom right corner of icons"). BottomLeft and BottomCentre are unused as of that change;
 * BottomCentre is where the rank sat between 2026-08-20 and then. */
enum class BadgeCorner : uint8_t {
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
	BottomCentre,
};

/** @brief Pixels of plate either side of the glyphs, and above and below them. */
constexpr int BadgePadX = 3;
constexpr int BadgePadY = 1;

/** @brief The plate @p text needs at @p font, glyphs plus padding. */
Size BadgeSize(string_view text);

/**
 * @brief Draws @p text as a badge in @p corner of @p host. Returns the plate's rect.
 *
 * The plate is sized to the text rather than fixed, so a two-character badge does not carry a
 * three-character plate - at 13px across an icon corner, that difference is most of the icon.
 *
 * Clamped INTO @p host: a badge wider than its host is pinned to the host's edge rather than hanging
 * off it, because the thing it is labelling is what the player is looking at.
 */
Rectangle DrawBadge(const Surface &out, Rectangle host, BadgeCorner corner, string_view text,
    UiFlags color = UiFlags::ColorWhite);

} // namespace devilution::oracool
