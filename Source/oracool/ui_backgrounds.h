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
#include "engine/point.hpp"
#include "engine/size.hpp"

namespace devilution::oracool {

/**
 * @brief Where a point marked in a background's own pixels ends up on screen.
 *
 * For art that carries a position in it - the character screens' painting has a stone dais, and the
 * hero preview stands on it. The backgrounds are cover-cropped and scaled to the window, so a spot
 * measured in the source is not a spot on screen until it has been through the same transform; this
 * runs the very same CropForScreen the painting does, so the two cannot disagree.
 *
 * @param artSize the source image's full size, as measured
 * @param pointInArt the spot, in that image's pixels
 */
Point MapBackgroundPointToScreen(Size artSize, Point pointInArt);

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
	// ChooseHero - the six classes around a campfire - was here. Removed rather than left unwired on
	// the user's call ("doesn't fit the D2R style"); the class list is back on HeroSelect's painting
	// with the other two character screens. An entry with no caller is what this enum had before, and
	// it took an audit to notice: `ui\choose_hero_bg.png` shipped for a slot nothing ever asked for.
	// The asset is still in Packaging/resources/oracool_assets/ui and is now unreferenced.
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
