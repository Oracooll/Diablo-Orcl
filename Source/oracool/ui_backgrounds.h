/**
 * @file oracool/ui_backgrounds.h
 *
 * Oracool: user requests (2026-08-13) - full-screen paintings behind the front-end screens,
 * "cropped into that image according to the resolution of the monitor of the player".
 *
 * The engine's own backgrounds are 640x480 .pcx plates drawn centred, with black bars around them at
 * every resolution this edition offers (the curated list starts at 960x720 and runs to 3440x1440 -
 * see options.cpp); the settings screen had no background at all, only `UiLoadBlackBackground`. The
 * masters here are 1680x720, which is exactly the 21:9 entry in that same list, so each is a master
 * rather than a fixed image: scaled to COVER the screen and then centre-cropped, which fills every
 * listed resolution from one asset.
 *
 * The palettes are NOT touched. `LoadBackgroundArt` loads the palette out of the .pcx it reads and
 * `UiLoadBlackBackground` loads `ui_art\diablo.pal`; the logo, the focus arrows, the cursor and every
 * font colour on these screens index into whichever is current, so shipping our own would recolour
 * all of them. The art quantizes into the palette the screen already has. That works better than it
 * sounds - `mainmenu.pcx` is a near-black plate using five entries out of a full 256-colour palette,
 * the other 251 are a general-purpose spread, and the mean per-pixel error comes out around 6/255.
 * (The two front-end palettes differ in 18 bytes out of 768, so this holds for both.)
 */
#pragma once

#include <memory>
#include <vector>

#include "DiabloUI/ui_item.h"

namespace devilution::oracool {

/**
 * @brief Which painting to draw. One cache slot each, so screens do not evict one another.
 *
 * Named for the SCREEN rather than the artwork, because that is what a caller knows - and the two
 * are not one-to-one: Settings and HeroSelect draw the same file. They still need separate slots,
 * because a slot caches the quantized result against the palette it was built under and those two
 * screens load different palettes (`ui_art\diablo.pal` and `ui_art\selhero.pcx`'s). Sharing one slot
 * would re-quantize 1.2 megapixels on every move between them.
 */
enum class UiBackground : size_t {
	MainMenu,
	Settings,
	HeroSelect,
	/** The class-selection screen's own painting: the six classes around a campfire. */
	ChooseHero,
	/**
	 * The difficulty picker. Reached in single-player straight after choosing a character, and drawn
	 * by selgame.cpp - which is the multiplayer file, but this screen is single-player's too (see
	 * SelheroLoadSelect's note on the difficulty-selection hack).
	 */
	Difficulty,

	LAST = Difficulty,
};

/**
 * @brief Adds background @p which, cropped to the current resolution, as an item of @p vecDialog.
 *
 * @return false if the asset is missing or could not be prepared, in which case the caller must fall
 * back to whatever it did before - a screen with no background at all is a black screen.
 *
 * Must be called AFTER the screen has loaded its palette (`LoadBackgroundArt` or
 * `UiLoadBlackBackground`), which is what puts it in `orig_palette`; quantizing before that would
 * match against whatever palette the previous screen left behind.
 *
 * Each background's prepared sprite is cached across visits and only rebuilt if the resolution or
 * that screen's palette actually changes, so the one-off cost of scaling and quantizing 1.2
 * megapixels is paid once per screen per session rather than on every return to it.
 *
 * @p atFront inserts at the head of @p vecDialog instead of appending. The hero-select screen adds
 * and removes its background around modal dialogs by erasing `begin()`, so its background has to be
 * the first item rather than merely an early one.
 */
bool AddUiBackground(std::vector<std::unique_ptr<UiItemBase>> *vecDialog, UiBackground which,
    bool atFront = false);

} // namespace devilution::oracool
