#include "oracool/levski_roar.h"

#include "oracool/stonegate.h"  // IsStonegateObject: the other stand in town
#include "oracool/wirt_cart.h" // IsWirtCartObject: and the fourth one

#include <SDL.h>

#include <fmt/format.h>

#include <cmath>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "engine/render/blit_impl.hpp" // AverageRgb (v1.11)
#include "diablo.h" // CloseOtherShopSurfaces - one shop surface at a time
#include "cursor.h"
#include "engine/trn.hpp" // GetInfravisionTRN - the unusable-item grey, at 3x
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "engine/palette.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h"
#include "items.h"
#include "objects.h"

#include "oracool/badge.h"
#include "oracool/crafting.h"
#include "oracool/cursor_tooltip.h" // ShowPanelStringsAsHintCard - the buttons' card
#include "oracool/recipe_list.h"
#include "oracool/event_log.h"
#include "oracool/hud_art.h" // DrawLoosePng, DrawRedCross - the painted skin and its states
#include "oracool/levski_roar_skin.h"
#include "oracool/levski_cube_skin.h" // the Cube host's own painting (RfA-20 batch 43b)
#include "oracool/book_frame.h" // the painted tall frame the recipe book wears
#include "oracool/ornate_border.h"
#include "oracool/salvage.h"
#include "oracool/shop_grid.h" // the shop's tab column, kept beside Griswold's Salvage page (2026-09-21)
#include "oracool/skill_sounds.h"
#include "oracool/socket_overlay.h"
#include "oracool/ui_sound.h"
#include "oracool/window_close.h"
#include "oracool/workshop.h"
#include "player.h"
#include "qol/stash.h" // AutoPlaceItemInStash - the grid's way back when the pack is full
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

/**
 * @brief Darkens @p rect by a QUARTER: each pixel blended half-and-half with its own half-black.
 *
 * DrawHalfTransparentRectTo is one pass through the black-blend table, about 50%, and the idle
 * plates under it read too dark (user, 2026-09-07: "reduce the darkening by half"). No table gives
 * 25% directly, but two the engine already has compose to it: paletteTransparencyLookup[0][c] is
 * c at half brightness, and paletteTransparencyLookup[c][that] is the midpoint between c and it -
 * three quarters of c. Exact for the palette's own arithmetic, no dither.
 */
void DrawQuarterDarkenRect(const Surface &out, const Rectangle &rect)
{
	for (int y = 0; y < rect.size.height; y++) {
		const int sy = rect.position.y + y;
		if (sy < 0 || sy >= out.h())
			continue;
		for (int x = 0; x < rect.size.width; x++) {
			const int sx = rect.position.x + x;
			if (sx < 0 || sx >= out.w())
				continue;
			// A quarter darker: the pixel averaged with its own half. Exact on the 32-bit screen
			// (v1.11), the nearest-index approximation on an 8-bit surface.
			if (out.isIndexed()) {
				uint8_t &p = *out.at<uint8_t>(sx, sy);
				p = paletteTransparencyLookup[p][paletteTransparencyLookup[0][p]];
			} else {
				uint32_t &c = *out.at<uint32_t>(sx, sy);
				c = AverageRgb(c, (c >> 1) & 0x7F7F7F7Fu);
			}
		}
	}
}

bool WindowOpen = false;
bool RecipeBookOpen = false;
/**
 * @brief Whose book the window is open on (Levski's Cube, 2026-09-20). The Cube object opens the
 * Cube's; Griswold's Forge tab, Ogden's table and Gillian's hearth open theirs. The window is one,
 * the recipes shown are the host's - see RecipeBelongsTo.
 */
TransmuteHost WindowHost = TransmuteHost::Cube;
/** @brief The Cube skin's recipe list, scrolled in whole lines (the bezel has eight line positions). */
int CubeListScroll = 0;
/** @brief The painted Cube UI's button held down (levski_skin::Transmute / Recipes) or -1, and the one under the cursor last frame. */
int PressedCubeButton = -1;
/** @brief The X and the Transmute button pressed, acted on at the release (audit, 2026-09-29). */
bool PressedCloseButton = false;
bool PressedTransmute = false;
int LastHoverCubeButton = -1;

/**
 * The pressed-button flash. User request, 2026-08-20: "Make some visual feedback when i click on
 * levskis buttons."
 *
 * Held as a button index plus an expiry tick rather than a bool, for the same reason the inventory
 * SORT button is: these buttons act on mouse-DOWN and nothing here polls a mouse-up, so a flag
 * would either linger until the next click or need a second owner to clear it. The index is the
 * salvage tier 0-6, then Transmute and Recipes - one mechanism, every button on the panel.
 */
constexpr int ButtonFlashNone = -1;
constexpr int ButtonFlashTransmute = SalvageTierCount;
constexpr int ButtonFlashRecipes = SalvageTierCount + 1;
int ButtonFlashIndex = ButtonFlashNone;
uint32_t ButtonFlashUntil = 0;
/** Long enough to see, short enough not to read as a mode change - the SORT button's number. */
constexpr uint32_t ButtonFlashMs = 170;
/** A pale gold fill: the same ramp the border is cut from, near its light end. */
constexpr uint8_t ButtonFlashColor = PAL16_YELLOW + 4;

bool ButtonFlashActive(int index)
{
	return ButtonFlashIndex == index && SDL_GetTicks() < ButtonFlashUntil;
}

void FlashButton(int index)
{
	ButtonFlashIndex = index;
	ButtonFlashUntil = SDL_GetTicks() + ButtonFlashMs;
}

/** The transmute slots, indexed by an item's top-left cell. Live only while the window is open - see the header's note on why
 * this is deliberately not save state. */
Item GridItems[LevskiGridSlots];

/**
 * Which cell each item anchors to, and which cells it covers.
 *
 * GridItems is indexed by the item's TOP-LEFT cell, and GridCells[c] holds that anchor's index + 1
 * for every cell the item covers (0 = free) - the same shape as Player::InvGrid, deliberately, so
 * the rules are the ones the player already knows from the backpack and the stash.
 *
 * The first version had neither array: twelve 56x56 boxes, one item each, footprint ignored (user,
 * 2026-08-19: "Why do they fit a whole armor in one? All this makes 0 sense."). It was wrong twice
 * over - a 56px box is two inventory cells square while a 2x3 armour is 56x84, so the armour never
 * fitted the box it was drawn in; and treating a rune and a breastplate as the same "one box" made
 * the cube's capacity mean nothing. A 3x4 cube holds ONE armour, or twelve runes.
 */
int8_t GridCells[LevskiGridSlots];

// Geometry. The window is sized from the grid rather than the other way round, so changing the
// grid's dimensions cannot leave the panel the wrong shape.
//
// The cell is the INVENTORY's cell, exactly - not a size of this window's choosing. Item sprites
// are cut to a whole number of 28px cells, so any other size would either crop them or leave them
// swimming, and the drag the player already knows from the stash would stop lining up.
// THE PAINTED SKIN's cell, not the inventory's (user, 2026-09-04: "Place it as Levski's Interface").
// The painting's grid cells are ~185px; the window is drawn at the scale that makes them CellSize
// (28 since 2026-09-04 - "regular game size"; 84 for the day before) and items at ItemScale, which is
// 1 now and sends the grid through the ordinary DrawItem path - see levski_roar_skin.h. The
// footprint rules are untouched: a 2x3 armour still covers 2x3 cells, and at 1x they are the same cells.
constexpr int CellSize = levski_skin::CellSize;
constexpr int ItemScale = CellSize / InventorySlotSizeInPixels.width;
static_assert(ItemScale * InventorySlotSizeInPixels.width == CellSize, "the skin's cell is not a whole multiple of the item cell");
constexpr int SlotGap = 6;
constexpr int Padding = 14;
constexpr int HeaderHeight = 30;
constexpr int ButtonHeight = 26;
constexpr int GridWidth = LevskiGridColumns * CellSize;
constexpr int GridHeight = LevskiGridRows * CellSize;

/**
 * @brief The salvage column: seven "salvage all X" buttons, stacked down the right of the grid.
 *
 * User request, 2026-08-20: "add salvage all whites, magic, rare, uniques, primal, set, ethereal
 * buttons on levski ui. increase it's ui window and add these in placeholder gold boxes."
 *
 * A COLUMN beside the grid rather than a row beneath it, and that is the window's shape deciding:
 * seven buttons wide enough to read would be over 700px in a row, half the screen. Stacked, they
 * cost 140px of width and reuse height the 3x4 grid already occupies.
 *
 * They began as gold-bordered placeholder boxes, as asked. They wear the Levski skin's own stone
 * plates now (the levski_* idle/hover/pressed PNGs); the box is only the fallback when that art is
 * absent. Only DrawSalvageButtons knows the difference - the rects and the routing never changed.
 */
constexpr int SalvageColumnWidth = 140;
constexpr int SalvageButtonHeight = 24;
constexpr int SalvageButtonGap = 4;
constexpr int SalvageColumnGap = 10;

/**
 * @brief The window is the painted skin, at the size the cutter chose - see levski_roar_skin.h,
 * which tools/CutLevskiRoarSkin.ps1 generates from the same pass that writes the art.
 *
 * Nothing here is measured by hand any more. The quest-log frame this window wore for a day
 * (v1.9.201) and the sized-from-content window before it are both gone: the painting carries the
 * frame, the title, the grid well and every button plate with its label, so the code draws STATE
 * on top of it and nothing else.
 */
constexpr Size RoarFrameSize = levski_skin::WindowSize;
constexpr const char *LevskiBackgroundAsset = "ui\\levski_bg.png";

/** @brief Lines the recipe list shows on the list skins, at a 20 px pitch. */
constexpr int ListLines = 8;
constexpr int ListPitch = 20;

/**
 * @brief The geometry of a LIST skin: a painting (or the user's canvas) with the grid, a recipe list
 * in a bezel, one TRANSMUTE button and the X. Two wear it: the Cube's painting (RfA-20 batch 43b,
 * measured into levski_cube_skin.h) and the artisans' canvas (user, 2026-09-20: "Use this canvas as UI
 * screen when i initiate Mystic (gilian) and Jeweller (ogden) special artisan abilities. Fill the black
 * background with an interface you draw in code and i will later fill with proper asset"). Griswold's
 * Forge keeps the Roar's painting, whose salvage plates are his.
 */
struct ListSkinGeometry {
	Size window;
	Point gridOrigin;
	Rectangle transmute;
	Rectangle list;
	Rectangle track;
	Rectangle close;
	const char *background;
	/** True for the canvas: the wells, the bezel and the button plate are drawn in code, as placeholders for art. */
	bool codeDrawn;
	/** The RECIPE BOOK button (the user's painted Cube UI, 2026-09-20); empty on the bezel skins, which list the recipes in the window. */
	Rectangle recipes;
	/** Where the title is drawn in the game's own font (user: "for the title use vanilla font proper size instead of prerendered title"). */
	Rectangle title;
	/** The 340x720 pages dock in the shop panel's place and wear the shared side panel when they have no art of their own. */
	bool docked = false;
	/** How many recipe rows the list shows. The bezel skins fit eight; the Cube's recipe PAGE fits fifteen. */
	int lines = ListLines;
	/**
	 * Whether this page has the transmute grid at all.
	 *
	 * False on the Cube's Recipes tab (2026-09-21), which is the full window given over to the list.
	 * The grid's CONTENTS survive the switch - GridItems is window state, not page state - so a
	 * player who laid three items out, read a formula and came back finds them where they left them.
	 * Without this the cells would be drawn at the window's top-left corner, gridOrigin being {0,0}.
	 */
	bool hasGrid = true;
	/**
	 * The painted frame's OPENING, darkened under the recipe list (user, 2026-09-21: "Lay a
	 * transparent dark layer under the recipes"); empty on a page that wants none.
	 *
	 * Carried by the page rather than written as constants the draw reads, because the two recipe
	 * pages do NOT share it: Levski's frame opens at y 306 and Ogden's at 299, measured on each.
	 * A shared constant would have darkened seven rows of Ogden's painted moulding.
	 */
	Rectangle darkLayer = { { 0, 0 }, { 0, 0 } };
	/**
	 * Whether the item grids' slot art is laid in this page's cells (2026-09-22).
	 *
	 * True everywhere the cells are a painted WELL, which is every page that has ever had a grid -
	 * and false on the open tesseract, whose cells are a violet hologram hanging in the air (user:
	 * "dont apply grid texture to levskis hologram grid. leave it as it is"). A stone slot laid in
	 * one would put masonry inside light.
	 */
	bool slotArt = true;
	/**
	 * Whether the TRANSMUTE button is part of the painting (2026-09-22).
	 *
	 * The tabbed pages wear Griswold's plate and glyph over their transmute rect, which is right
	 * where the rect is a recess the art left empty for it. The open tesseract is not: its button is
	 * the glowing diamond the painter put there (user: "as transmute button we will use the diamond
	 * who is 72 pixels bellow the cube grid"), so laying a stone plate on it would cover the very
	 * thing being pressed. A painted button answers with light instead of a plate: brighter under
	 * the cursor, brighter still while held.
	 */
	bool paintedTransmute = false;
};

constexpr ListSkinGeometry CubeGeometry {
	cube_skin::WindowSize, cube_skin::GridOrigin, cube_skin::TransmuteRect, cube_skin::RecipeListRect,
	cube_skin::ScrollTrackRect, cube_skin::CloseRect, cube_skin::BackgroundAsset, false
};
/**
 * The canvas (Resources\02. Oracooll Assets\Interface Canvas.png -> ui\artisan_canvas.png): 320x352, the ornate frame around
 * a black interior x 22..297, y 25..326. Inside it: the title band, the 3x4 grid at the left, the recipe
 * list at the right with its track, the button centred under them.
 */
constexpr const char *ArtisanCanvasAsset = "ui\\artisan_canvas.png";
constexpr ListSkinGeometry ArtisanGeometry {
	{ 320, 352 }, { 30, 66 }, { { 89, 296 }, { 142, 26 } }, { { 124, 66 }, { 162, ListLines * ListPitch } },
	{ { 290, 66 }, { 4, ListLines * ListPitch } }, { { 296, 5 }, { 18, 18 } }, ArtisanCanvasAsset, true
};
/**
 * The USER'S Cube UI (2026-09-20, Resources\Levski's Cube UI): a 320x352 background with the grid painted in
 * (silver rules at x 115/144/173/202, y 106/135/164/193/222 - the item cell's 28 px inside a 29 px pitch, the
 * Roar's own pitch), a title band, the TRANSMUTE and RECIPE BOOK buttons as their own paintings (778x143 and
 * 532x106, scaled by tools/ScalePainting.ps1 to 208x38 and 142x28 - the sizes in the user's assembled sample,
 * "Levski Cube Full Design 2.png", 1198x1313 = the canvas at 3.74x). No bezel: the RECIPE BOOK button opens the
 * tall book beside the window, as the Roar's plate did. The title is the game's 24 px gold in the sample's band.
 * Buttons brighten a notch under the cursor and sink 2 px down-left while pressed (feedback_button_press_and_sound).
 */
constexpr const char *CubeCanvasAsset = "ui\\cube_canvas.png";
constexpr const char *CubeTransmuteAsset = "ui\\cube_transmute.png";
constexpr const char *CubeRecipeBookAsset = "ui\\cube_recipebook.png";
constexpr int CubeHoverBrightenPercent = 115;
constexpr Displacement CubeButtonSink { -2, 2 };
constexpr ListSkinGeometry CubeCanvasGeometry {
	{ 320, 352 }, { 116, 107 }, { { 56, 249 }, { 208, 38 } }, { { 0, 0 }, { 0, 0 } },
	{ { 0, 0 }, { 0, 0 } }, { { 296, 5 }, { 18, 18 } }, CubeCanvasAsset, false,
	{ { 89, 293 }, { 142, 28 } }, { { 56, 30 }, { 208, 40 } }
};

/**
 * Griswold's SALVAGE window (user, 2026-09-21, Resources\Griswold The Blacksmith Shop UI\Griswold Salvage Tab UI): a 320x352 forge painting with a
 * "Salvage Results" plate and a dark results box painted in; seven tier icons cut from the user's sheet (2172x724,
 * seven 271x277 tiles) and resampled to 56x56, placed as the user's assembled sample has them - row one White,
 * Magic, Rare, Unique at y 78; row two Set, Primal, Ethereal at y 137 (measured by diffing the sample against the
 * bare background). The title "Salvage" is the game's 30 px gold, as the Cube's. Icons brighten a notch under the
 * cursor, sink 2 px down-left while pressed, and sound titlemov on entry and press; an icon with nothing in the
 * pack to salvage sits under the quarter shade. NO grid and NO recipes: when this window lost its grid Griswold's
 * gear recipes moved to the Cube's book (crafting.cpp HostOfRecipe).
 */
constexpr const char *SalvageCanvasAsset = "ui\\salvage_canvas.png";
/** In SalvageTier order: White, Magic, Rare, Unique, Primal, Set, Ethereal. */
constexpr const char *SalvageIconAssets[SalvageTierCount] = {
	"ui\\salvage_white.png", "ui\\salvage_magic.png", "ui\\salvage_rare.png", "ui\\salvage_unique.png",
	"ui\\salvage_primal.png", "ui\\salvage_set.png", "ui\\salvage_ethereal.png"
};
/**
 * The Salvage window has TWO pages (user, 2026-09-21: "i want to assemble a new Salvage page for griswold shop, one
 * which is 340x720 size to fit nicely with the rest of his UI windows"). The TALL one is the shop panel's own size
 * and dock - the forge painting (Resources\Griswold The Blacksmith Shop UI\Griswold Salvage Tab UI\Griswold's Salvage UI Full Size.png, 862x1824, resampled 1:1 into
 * 340x720) with the seven icons laid across the line where the lit forge gives way to the dark floor, a dark gold
 * frame under them for the results, and everything clear of OrbClearanceBottom so the life orb is never covered.
 * The 320x352 page of v1.12.096 stays as the fallback when the tall painting is missing.
 */
struct SalvageLayout {
	Size window;
	const char *background;
	Rectangle title;
	Rectangle results;
	Rectangle close;
	Rectangle icons[SalvageTierCount];
	/** The eighth plate: salvage ONE item, chosen with the hammer (2026-09-21). Empty on a page that has none. */
	Rectangle itemIcon;
	/** The tall page docks with the shop panel (bottom-left); the small one is centred like the other artisan windows. */
	bool docked;
	/** And draws the dark gold frame around its results area. */
	bool goldFrame;
};

/**
 * Griswold's redesigned canvas, shared with his shop tabs (user, 2026-09-21: "We replace the canvas for all his
 * tabs, including the new Salvage tab"). One painting behind every door of his shop, so moving between the shelves
 * and the Salvage page no longer changes the room. His old Salvage-only painting (ui\salvage_canvas_tall.png) is
 * still in the archive and is no longer read.
 */
constexpr const char *SalvageTallCanvasAsset = "ui\\griswold_canvas.png";
/**
 * Measured on the resampled painting: the lit forge ends and the bare floor begins about y 378, so the first icon row
 * overlaps it by thirty pixels. Four across then three, at the 66 px pitch the 320 page used, centred in the canvas's
 * 22..317 opening. The results frame ends at 612 - six pixels above the inventory grid's own floor (GridBottom 618),
 * which is where OrbClearanceBottom puts the top of the orbs.
 */
constexpr SalvageLayout SalvageTallPage {
	{ 340, 720 }, SalvageTallCanvasAsset,
	// NO title (user, 2026-09-21: "We remove the Salvage title from Salvage tab"). An empty rect rather than an
	// empty string, so the draw skips it outright and nothing reserves the band.
	{ { 0, 0 }, { 0, 0 } },
	{ { 30, 486 }, { 280, 126 } }, // the results, inside the gold frame; its title rides the top border
	{ { 316, 5 }, { 18, 18 } },
	// 4x2 at the row's own 66 px pitch, split by whether the plate asks first (user, 2026-09-21): row one is the
	// hammer and the three cheap tiers, which act at once; row two is the four dear ones, which ask.
	{ { { 109, 344 }, { 56, 56 } }, // White - "All Basics", second of the top row
	    { { 175, 344 }, { 56, 56 } }, // Magic
	    { { 241, 344 }, { 56, 56 } }, // Rare
	    { { 43, 410 }, { 56, 56 } },  // Unique - the lower row asks before it destroys
	    { { 175, 410 }, { 56, 56 } }, // Primal
	    { { 109, 410 }, { 56, 56 } }, // Set
	    { { 241, 410 }, { 56, 56 } } },// Ethereal
	{ { 43, 344 }, { 56, 56 } },   // the hammer that takes one item, first of the top row
	true, true
};

/** The first page (v1.12.096), kept for a build without the tall painting. */
constexpr SalvageLayout SalvageSmallPage {
	{ 320, 352 }, SalvageCanvasAsset,
	{ { 22, 30 }, { 276, 40 } },
	{ { 30, 214 }, { 260, 106 } },
	{ { 296, 5 }, { 18, 18 } },
	{ { { 32, 78 }, { 56, 56 } }, { { 98, 78 }, { 56, 56 } }, { { 163, 78 }, { 56, 56 } }, { { 230, 78 }, { 56, 56 } },
	    { { 132, 137 }, { 56, 56 } }, { { 49, 137 }, { 56, 56 } }, { { 214, 137 }, { 56, 56 } } },
	{ { 0, 0 }, { 0, 0 } }, // the small page has no room for the eighth plate
	false, false
};

/** @brief The Salvage page Griswold's window wears, or nullptr (another host, or no painting at all). */
const SalvageLayout *SalvagePage()
{
	if (WindowHost != TransmuteHost::Smith)
		return nullptr;
	if (GetLoosePngSize(SalvageTallPage.background).width > 0)
		return &SalvageTallPage;
	if (GetLoosePngSize(SalvageSmallPage.background).width > 0)
		return &SalvageSmallPage;
	return nullptr;
}
int PressedSalvageIcon = -1;
int LastHoverSalvageIcon = -1;
/**
 * The message under the painted "Salvage Results" plate (user, 2026-09-21: "1 message per salvage button press:
 * X (type) Items destroyed / (Icon frame 60x60 with 56x56 high res version of salvaged material sprite) X (material
 * name) Salvaged. Font in colour of item type. Message stays until another salvage icon is pressed ... horizontally
 * and vertically aligned in middle of Salvage Results area"; "icon frame to have 1px outline border on the outside
 * of these 60x60px. frame color - according to salvaged item"). The sprites are the 28 px material icons doubled
 * (ui\salvage_mat_<tier>.png) until a painted 56 px set exists.
 */
struct SalvageMessageState {
	bool shown = false;
	SalvageTier tier = SalvageTier::White;
	int items = 0;
	int materials = 0;
};
SalvageMessageState SalvageMessage;
/** In SalvageTier order, as the icons: White, Magic, Rare, Unique, Primal, Set, Ethereal. */
constexpr const char *SalvageItemIconAsset = "ui\\salvage_item.png";
/**
 * @brief The four dear tiers ask before they destroy (user, 2026-09-21: "These actions require confirmation ... Are
 * you sure you want to destroy all (item type) items?"). The three cheap ones and the hammer act at once.
 */
bool SalvageTierNeedsConfirm(SalvageTier tier)
{
	return tier == SalvageTier::Unique || tier == SalvageTier::Set || tier == SalvageTier::Primal || tier == SalvageTier::Ethereal;
}

/** @brief The tier whose question is standing in the results frame, or -1; and the button held down in it. */
int PendingConfirmTier = -1;
int PressedConfirmButton = -1;
constexpr int ConfirmButton = 0;
constexpr int CancelButton = 1;
constexpr Size ConfirmButtonSize { 100, 28 };
constexpr int ConfirmButtonGap = 20;
constexpr int ConfirmButtonBottomGap = 16;
constexpr uint32_t ConfirmGreenRgb = 0x64A064;
constexpr uint32_t CancelRedRgb = 0xC04030;
/** Defined further down, beside the drawing they belong to; the release hook above them needs both. */
Rectangle SalvageConfirmButtonRect(const Rectangle &results, int which);
void RunSalvageTier(SalvageTier tier);
/** @brief Whether the hammer is armed to break ONE item down; it lives only while this window is open. */
bool SalvageItemCursorArmed = false;

constexpr const char *SalvageMaterialSprites[SalvageTierCount] = {
	"ui\\salvage_mat_white.png", "ui\\salvage_mat_magic.png", "ui\\salvage_mat_rare.png", "ui\\salvage_mat_unique.png",
	"ui\\salvage_mat_primal.png", "ui\\salvage_mat_set.png", "ui\\salvage_mat_ethereal.png"
};
/** "4 Rare Items destroyed" - the tier as an adjective; SalvageTierName has the plural button labels ("Whites"). */
constexpr const char *SalvageTierAdjectives[SalvageTierCount] = {
	N_("White"), N_("Magic"), N_("Rare"), N_("Unique"), N_("Primal"), N_("Set"), N_("Ethereal")
};
/** The seven item-type colours (Item::getTextColor's ladder) and their RGB for the frame's outline. */
constexpr UiFlags SalvageTierColors[SalvageTierCount] = {
	UiFlags::ColorWhite, UiFlags::ColorBlue, UiFlags::ColorYellow3, UiFlags::ColorWhitegold,
	UiFlags::ColorBeige2, UiFlags::ColorOracoolGreen, UiFlags::ColorGray7
};
constexpr uint32_t SalvageTierRgb[SalvageTierCount] = { 0xCCCCCC, 0x9FA5C6, 0xF0EC00, 0xDDC47E, 0xCA9E9E, 0x64A064, 0x737373 };
constexpr Size SalvageFramePlate { 60, 60 };
constexpr int SalvageFrameGap = 8;

/** @brief Whether Griswold's window wears the user's painted Salvage UI (the Roar painting is the missing-file fallback). */
bool SalvageSkin()
{
	return SalvagePage() != nullptr;
}


/**
 * The artisans' WORKSHOP page (user, 2026-09-21: "Build Jeweller workshop with placeholder canvas and ui buttons.
 * I will supply assets later"): the same 340x720 page and dock as Griswold's windows, with the grid, the recipe
 * bezel, the track and the button drawn in code on the shared side panel until the art arrives at
 * ui\artisan_workshop.png.
 */
constexpr const char *ArtisanWorkshopAsset = "ui\\artisan_workshop.png";
constexpr ListSkinGeometry ArtisanWorkshopGeometry {
	{ 340, 720 }, { 30, 96 }, { { 99, 300 }, { 142, 26 } }, { { 134, 96 }, { 168, ListLines * ListPitch } },
	{ { 306, 96 }, { 4, ListLines * ListPitch } }, { { 316, 5 }, { 18, 18 } }, ArtisanWorkshopAsset, true,
	{ { 0, 0 }, { 0, 0 } }, { { 22, 30 }, { 296, 36 } }, true
};
/**
 * Levski's Cube at the family size (user, 2026-09-21: "Move levski's cube to 340x720 UI. Use placeholders for now.
 * Put as many tabs as necessary"): the same docked page as the artisans', so the three windows line up. ONE page is
 * enough - the bezel lists all seven of the Cube's recipes at once, so there is nothing for a second tab to hold
 * until the Powers slots are built (section III of the plan, parked at D3/D4).
 */
constexpr const char *CubeWorkshopAsset = "ui\\cube_workshop.png";
constexpr ListSkinGeometry CubeWorkshopGeometry {
	{ 340, 720 }, { 30, 96 }, { { 99, 300 }, { 142, 26 } }, { { 134, 96 }, { 168, ListLines * ListPitch } },
	{ { 306, 96 }, { 4, ListLines * ListPitch } }, { { 316, 5 }, { 18, 18 } }, CubeWorkshopAsset, true,
	{ { 0, 0 }, { 0, 0 } }, { { 22, 30 }, { 296, 36 } }, true
};
/**
 * LEVSKI'S CUBE, TWO TABS (user, 2026-09-21: "Tab Cube - ... / Tab Recipes - ...").
 *
 * Two painted 340x720 pages of one window, switched by a two-tab column beside it - the vendors'
 * own column, because the Cube docks in exactly the shop panel's rect and a second kind of tab
 * beside the same window would be two answers to one question.
 *
 * MEASURED on the art, both pages:
 *   Cube page   - the grid's painted frame runs x 118..224, y 406..541, with a 3x4 well inside it:
 *                 rules at x 156/185/214 and y 444/473/502, which is the Roar's own 28px cell on a
 *                 29px pitch, so the grid's origin is (128,416) and NOTHING about the transmute
 *                 logic changes. The painting simply draws the wells the code used to.
 *   Recipe page - one ornate frame, band y 289..628 with its opening at y 306..618, x 29..310.
 */
constexpr const char *CubePageCanvasAsset = "ui\\cube_page_canvas.png";
constexpr const char *CubeRecipesCanvasAsset = "ui\\cube_recipes_canvas.png";
/**
 * OGDEN'S CUBE TAB (user, 2026-09-21: "for ogden missing recipes we will need Cube tab canvas [...]
 * Use the transmute button as with Levski's Cube. Same behaviour").
 *
 * His jeweller's table with the SAME grid frame painted into it - measured, and identical to
 * Levski's to the pixel: frame x 118..224, y 406..541, rules at x 156/185/214 and y 444/473/502. So
 * it shares CubePageGeometry's every number and differs only in which painting goes down, which is
 * why "same behaviour" costs one asset and one line of the table.
 *
 * This is where his four recipes that need an item in hand are worked - Free the Sockets, Temper
 * Jewels, Punch Sockets and Recolour Gems. His Gems and Runes pages are collections, and none of
 * those four acts on a collection.
 */
constexpr const char *OgdenCubeCanvasAsset = "ui\\ogden_cube_canvas.png";
/**
 * TRANSMUTE is Griswold's Refresh plate (user, 2026-09-21: "Use Griswold Refresh button as Transmute
 * button here"), frame and glyph both - the same two files his shop draws, not copies of them.
 *
 * "Placed under the cube, dead center, 4px away": the painted grid frame spans x 118..224, so its
 * centre is 171 and a 34px plate starts at 154; its foot is y 541, so four pixels of air puts the
 * plate's top at 546. Both derived from the measurement above rather than typed in, which is what
 * makes a recut canvas move the button with it.
 */
constexpr const char *CubeTransmuteFrameAsset = "ui\\shop_button_frame.png";
// The Cube's OWN transmute icon since 2026-09-22 (Resources\Levski's Cube UI\Transmute Icon.png,
// 112x112, reduced 4:1 to the 28px every glyph on these plates is). Griswold's Refresh glyph stood
// in for it while the plate was new - it was the nearest thing to hand, and it said "reroll" on a
// button that transmutes.
constexpr const char *CubeTransmuteGlyphAsset = "ui\\shop_glyph_transmute.png";
constexpr int CubeGridFrameLeft = 118;
constexpr int CubeGridFrameRight = 224;
constexpr int CubeGridFrameBottom = 541;
constexpr int CubeTransmuteSize = 34;
constexpr int CubeTransmuteClearance = 4;
constexpr Rectangle CubePageTransmuteRect {
	{ (CubeGridFrameLeft + CubeGridFrameRight + 1) / 2 - CubeTransmuteSize / 2,
	    CubeGridFrameBottom + 1 + CubeTransmuteClearance },
	{ CubeTransmuteSize, CubeTransmuteSize }
};
/**
 * THE OPEN TESSERACT (user, 2026-09-22: "replace levski's cube with this new asset [...] as transmute
 * button we will use the diamond who is 72 pixels bellow the cube grid").
 *
 * A man facing the opened cube, with the grid floating above it as a VIOLET HOLOGRAM and the
 * transmute diamond glowing in the tesseract below. MEASURED off the painting, not estimated:
 *
 *   the hologram's rules stand at x 123/124, 152, 180, 207/208 and y 103/104, 132, 160, 188,
 *   215/216 - three columns by four rows of 28px cells on a 28px pitch, origin (124,104). That is
 *   the Roar's own cell and the Roar's own pitch, so not one line of the transmute logic moves;
 *
 *   the diamond's vertices sit at x 143 and 190, y 288 and 322 - and its TOP lands at exactly
 *   216 + 72, which is the user's "72 pixels bellow the cube grid" to the pixel. Two independent
 *   measurements agreeing is what says the grid reading is right as well.
 *
 * The button is the diamond's bounding box, 48 by 35. It is wider than it is tall because the shape
 * is, and a square over it would take in the tesseract's ribs on either side.
 */
constexpr Point CubeTesseractGridOrigin { 124, 104 };
constexpr Rectangle CubeDiamondRect { { 143, 288 }, { 48, 35 } };
constexpr ListSkinGeometry CubePageGeometry {
	{ 340, 720 }, CubeTesseractGridOrigin, CubeDiamondRect, { { 0, 0 }, { 0, 0 } },
	{ { 0, 0 }, { 0, 0 } }, { { 316, 5 }, { 18, 18 } }, CubePageCanvasAsset, false,
	{ { 0, 0 }, { 0, 0 } }, // no RECIPE BOOK plate: the tab beside the window is how the list is reached now
	{ { 0, 0 }, { 0, 0 } }, // and no title - the painting is a portrait, as every canvas this day
	true, ListLines, /*hasGrid=*/true, { { 0, 0 }, { 0, 0 } },
	// NO SLOT ART (user: "dont apply grid texture to levskis hologram grid. leave it as it is").
	// The cells are a hologram, not a well: a stone slot laid in one would put masonry inside light.
	/*slotArt=*/false,
	/*paintedTransmute=*/true
};
/** @brief Ogden's, the same page down to the pixel with his table painted behind it instead. */
constexpr ListSkinGeometry OgdenCubePageGeometry {
	{ 340, 720 }, { 128, 416 }, CubePageTransmuteRect, { { 0, 0 }, { 0, 0 } },
	{ { 0, 0 }, { 0, 0 } }, { { 316, 5 }, { 18, 18 } }, OgdenCubeCanvasAsset, false,
	{ { 0, 0 }, { 0, 0 } }, { { 0, 0 }, { 0, 0 } }, true
};
/**
 * The recipe page: the list inside the painted frame, on a dark layer (user, 2026-09-21: "Put the
 * list with recipes within the Frame. Lay a transparent dark layer under the recipes").
 *
 * Fifteen rows at the list's own 20px pitch is 300 of the opening's 312, which leaves six pixels of
 * air top and bottom. The track rides the opening's right edge; the rows stop short of it.
 */
constexpr int CubeRecipeOpeningTop = 306;
constexpr int CubeRecipeOpeningBottom = 618;
constexpr int CubeRecipeOpeningLeft = 29;
constexpr int CubeRecipeOpeningRight = 310;
constexpr int CubeRecipeLines = 15;
constexpr int CubeRecipeListTop = CubeRecipeOpeningTop
    + (CubeRecipeOpeningBottom - CubeRecipeOpeningTop + 1 - CubeRecipeLines * ListPitch) / 2;
constexpr ListSkinGeometry CubeRecipesGeometry {
	{ 340, 720 }, { 0, 0 }, { { 0, 0 }, { 0, 0 } },
	{ { CubeRecipeOpeningLeft + 6, CubeRecipeListTop }, { CubeRecipeOpeningRight - CubeRecipeOpeningLeft - 11 - 8, CubeRecipeLines * ListPitch } },
	{ { CubeRecipeOpeningRight - 5, CubeRecipeListTop }, { 4, CubeRecipeLines * ListPitch } },
	{ { 316, 5 }, { 18, 18 } }, CubeRecipesCanvasAsset, false,
	{ { 0, 0 }, { 0, 0 } }, { { 0, 0 }, { 0, 0 } }, true, CubeRecipeLines, /*hasGrid=*/false,
	{ { CubeRecipeOpeningLeft, CubeRecipeOpeningTop },
	    { CubeRecipeOpeningRight - CubeRecipeOpeningLeft + 1, CubeRecipeOpeningBottom - CubeRecipeOpeningTop + 1 } }
};

/**
 * OGDEN'S RECIPE PAGE (user, 2026-09-21: "here is ogden recipe tab").
 *
 * His jeweller's table under one ornate frame. MEASURED, and NOT Levski's: the band runs y 289..298
 * against Levski's 289..305, so the opening starts at **299** rather than 306 and is 320 tall to his
 * 313. Sixteen rows at the list's 20px pitch instead of fifteen.
 *
 * Seven pixels is exactly the kind of difference that survives an eyeball and shows up as a dark
 * band over painted moulding - which is why the opening is measured per page and carried on the
 * page rather than shared.
 */
constexpr const char *OgdenRecipesCanvasAsset = "ui\\ogden_recipes_canvas.png";
constexpr int OgdenRecipeOpeningTop = 299;
constexpr int OgdenRecipeOpeningBottom = 618;
constexpr int OgdenRecipeLines = 16;
constexpr int OgdenRecipeListTop = OgdenRecipeOpeningTop
    + (OgdenRecipeOpeningBottom - OgdenRecipeOpeningTop + 1 - OgdenRecipeLines * ListPitch) / 2;
constexpr ListSkinGeometry OgdenRecipesGeometry {
	{ 340, 720 }, { 0, 0 }, { { 0, 0 }, { 0, 0 } },
	{ { CubeRecipeOpeningLeft + 6, OgdenRecipeListTop }, { CubeRecipeOpeningRight - CubeRecipeOpeningLeft - 11 - 8, OgdenRecipeLines * ListPitch } },
	{ { CubeRecipeOpeningRight - 5, OgdenRecipeListTop }, { 4, OgdenRecipeLines * ListPitch } },
	{ { 316, 5 }, { 18, 18 } }, OgdenRecipesCanvasAsset, false,
	{ { 0, 0 }, { 0, 0 } }, { { 0, 0 }, { 0, 0 } }, true, OgdenRecipeLines, /*hasGrid=*/false,
	{ { CubeRecipeOpeningLeft, OgdenRecipeOpeningTop },
	    { CubeRecipeOpeningRight - CubeRecipeOpeningLeft + 1, OgdenRecipeOpeningBottom - OgdenRecipeOpeningTop + 1 } }
};

/** @brief The Cube's two tabs, in column order. */
enum class CubeTab { Cube, Recipes };
constexpr int CubeTabCount = 2;
CubeTab OpenCubeTab = CubeTab::Cube;
/** @brief The tab being held down, or -1. Acts on the release inside itself, like every button here. */
int PressedCubeTab = -1;
int LastHoverCubeTab = -1;

/**
 * @brief The page behind tab @p index for whichever host this window is open on, or nullptr.
 *
 * Two hosts have tabs. LEVSKI'S CUBE has both of its pages painted, so its Recipes tab is the user's
 * recipe panel. OGDEN has only his Cube page painted; his Recipes tab keeps the page it already had,
 * the shared workshop canvas with the list in its bezel, until a canvas for it arrives.
 *
 * Gated on the art, as every canvas this day is: a tab that leads to a page with no painting is
 * worse than no tabs, so a host short of its file keeps the single page it had before.
 */
const ListSkinGeometry *TabbedPageAt(int index)
{
	if (WindowHost == TransmuteHost::Cube
	    && GetLoosePngSize(CubePageCanvasAsset).width > 0
	    && GetLoosePngSize(CubeRecipesCanvasAsset).width > 0)
		return index == 1 ? &CubeRecipesGeometry : &CubePageGeometry;
	if (WindowHost == TransmuteHost::Tavern && GetLoosePngSize(OgdenCubeCanvasAsset).width > 0) {
		// His recipe page when it is installed; the page he had before when it is not, so the tab
		// never leads somewhere unpainted.
		if (index == 1)
			return GetLoosePngSize(OgdenRecipesCanvasAsset).width > 0 ? &OgdenRecipesGeometry : &ArtisanWorkshopGeometry;
		return &OgdenCubePageGeometry;
	}
	return nullptr;
}

/** @brief Whether this window wears tabbed pages at all. */
bool CubeTabbedPages()
{
	return TabbedPageAt(0) != nullptr;
}

/** @brief The list skin the window wears right now, or nullptr for the Roar's painting. */
const ListSkinGeometry *ListSkin()
{
	// The user's two painted pages first (2026-09-21), and only when BOTH are installed: a tab that
	// leads to a missing canvas is worse than no tabs, so the pair is all-or-nothing.
	if (CubeTabbedPages())
		return OpenCubeTab == CubeTab::Recipes ? &CubeRecipesGeometry : &CubePageGeometry;
	// The canvas for the Cube too (user, 2026-09-20: "Use same canvas and in code drawn interface for the
	// UI of Levski's Cube"); batch 43b's painting stays measured in levski_cube_skin.h, unworn.
	// The Cube moved to the 340x720 page with the artisans (2026-09-21); its 320x352 painting stays in the archive.
	if (WindowHost == TransmuteHost::Cube)
		return &CubeWorkshopGeometry;
	// The user's own Cube UI first (2026-09-20: "build Levski's Cube UI with assets from this folder").
	if (WindowHost == TransmuteHost::Cube && GetLoosePngSize(CubeCanvasGeometry.background).width > 0)
		return &CubeCanvasGeometry;
	// Ogden and Gillian work at the 340x720 page since 2026-09-21; the Cube keeps its own painting.
	if (WindowHost == TransmuteHost::Tavern || WindowHost == TransmuteHost::Barmaid)
		return &ArtisanWorkshopGeometry;
	if (WindowHost == TransmuteHost::Cube || WindowHost == TransmuteHost::Tavern || WindowHost == TransmuteHost::Barmaid) {
		if (GetLoosePngSize(ArtisanGeometry.background).width > 0)
			return &ArtisanGeometry;
		if (WindowHost == TransmuteHost::Cube && GetLoosePngSize(CubeGeometry.background).width > 0)
			return &CubeGeometry;
	}
	return nullptr;
}

/** @brief Whether the window wears a list skin (the name predates the artisans' canvas). */
bool CubeSkin()
{
	return ListSkin() != nullptr;
}

/**
 * @brief The Cube's RECIPE page opening, or an empty rect on any other page.
 *
 * The page's dark layer IS its opening - it is the rect measured on the painted frame - so the list
 * and the wheel both ask this rather than each deriving it. Empty on the grid page, which has no
 * list at all.
 */
Rectangle CubeRecipeOpening()
{
	const ListSkinGeometry *skin = ListSkin();
	if (skin == nullptr || skin->hasGrid || skin->darkLayer.size.width == 0)
		return Rectangle { { 0, 0 }, { 0, 0 } };
	const Rectangle window = GetLevskiRoarRect();
	return Rectangle { window.position + Displacement { skin->darkLayer.position.x, skin->darkLayer.position.y },
		skin->darkLayer.size };
}

/**
 * @brief Whether the page on screen has the transmute grid.
 *
 * False only on the Cube's Recipes tab. Asked by CellAt, which is the single chokepoint every hover
 * and every click on a cell goes through - so one test here takes the grid out of the hit map, and
 * the draw's own gate takes it off the screen. Two places, not twelve, and they cannot disagree
 * about which cells exist because one of them is the whole hit test.
 */
bool PageHasGrid()
{
	const ListSkinGeometry *skin = ListSkin();
	return skin == nullptr || skin->hasGrid;
}

/** @brief Whether the window wears the user's painted Cube UI: painted buttons, no bezel, the tall book on demand. */
bool PaintedButtons()
{
	const ListSkinGeometry *skin = ListSkin();
	return skin != nullptr && skin->recipes.size.width > 0;
}

Size CurrentFrameSize()
{
	if (const SalvageLayout *page = SalvagePage(); page != nullptr)
		return page->window;
	const ListSkinGeometry *skin = ListSkin();
	return skin != nullptr ? skin->window : RoarFrameSize;
}

/** @brief One of the ten painted plates, in window space. Under a list skin only the close X and
 * TRANSMUTE exist; every other plate is an empty rect, which contains nothing and draws nothing. */
Rectangle ButtonRect(const Rectangle &window, int index)
{
	if (const SalvageLayout *page = SalvagePage(); page != nullptr) {
		if (index == levski_skin::Close) {
			// The SHARED corner on the docked page (user, 2026-09-21: "Its X on the upper corner is not
			// located properly"). Griswold's shop panel puts its X at GetWindowCloseButtonRect's corner
			// and the Salvage page sits in exactly that rect, so a hand-authored one three pixels left
			// and two down read as a different window wearing the same frame. The small painted page
			// keeps its own, which is measured against art the shared corner knows nothing about.
			if (page->docked)
				return GetWindowCloseButtonRect(window);
			return Rectangle { window.position + Displacement { page->close.position.x, page->close.position.y }, page->close.size };
		}
		if (index >= levski_skin::SalvageFirst && index < levski_skin::SalvageFirst + SalvageTierCount) {
			const Rectangle &r = page->icons[index - levski_skin::SalvageFirst];
			return Rectangle { window.position + Displacement { r.position.x, r.position.y }, r.size };
		}
		return Rectangle { { 0, 0 }, { 0, 0 } }; // no Transmute, no Recipes on the Salvage window
	}
	if (const ListSkinGeometry *skin = ListSkin(); skin != nullptr) {
		if (index == levski_skin::Close) {
			const Rectangle &c = skin->close;
			return Rectangle { window.position + Displacement { c.position.x, c.position.y }, c.size };
		}
		if (index == levski_skin::Transmute) {
			const Rectangle &t = skin->transmute;
			return Rectangle { window.position + Displacement { t.position.x, t.position.y }, t.size };
		}
		if (index == levski_skin::Recipes) {
			const Rectangle &r = skin->recipes;
			return Rectangle { window.position + Displacement { r.position.x, r.position.y }, r.size };
		}
		return Rectangle { { 0, 0 }, { 0, 0 } };
	}
	const Rectangle &r = levski_skin::ButtonRects[index];
	return Rectangle { window.position + Displacement { r.position.x, r.position.y }, r.size };
}

// CubeLineRect, CurrentListLines, CubeListMaxScroll and CubeListRecipeAt are GONE (2026-09-22).
//
// All four served the fixed-row bezel list: a row was ListPitch tall, a page showed `lines` of them,
// and the scroll counted rows. The shared recipe list wraps each explanation to as many lines as it
// needs, so a recipe is a block of its own height and the scroll counts pixels - there is no row to
// index. Their work is now RecipeListMaxScroll and RecipeListHitTest, which run the draw's own
// arithmetic rather than a parallel model of it.
/** @brief The host's recipes in book order. */
std::vector<int> CubeListRecipes();

Rectangle CloseButtonRect(const Rectangle &window)
{
	if (SalvageSkin())
		return ButtonRect(window, levski_skin::Close);
	return ButtonRect(window, levski_skin::Close);
}

/**  How wide the book may be: all the room left of the window, capped, never overlapping it.
 *
 * A constant 420 was wrong twice - first drawn off the left edge, then clamped to x=0 where it sat
 * ON TOP of Levski's own window and covered its title (user screenshot, 2026-08-19). The room to the
 * left of a centred window is what it is; the book has to fit that, not assume it. */
int RecipeBookWidthFor(const Rectangle &window)
{
	// The painted tall frame's width (2026-09-05): a painting cannot be narrower for a narrow
	// screen, so the book is its frame's size and slides to x=0 when the room left of the window
	// runs out - the clamp GetLevskiRecipeBookRect already does.
	(void)window;
	return BookFrameSize(BookFrame::Tall).width;
}

/** @brief The frame's clear core: where the book's title, rows and clips live (the bezel is outside it). */
Rectangle RecipeBookInner(const Rectangle &page)
{
	return BookFrameCore(BookFrame::Tall, page);
}
/**
 * The panel's ground, opaque.
 *
 * Two half-transparent passes came first and were not enough (user screenshot, 2026-08-19): the
 * town read straight through the grid, and worse, it read through UNEVENLY - the four cells over
 * the lit doorway glowed while the rest sat black, so the grid looked like four different
 * materials. Half-transparency composites against whatever is behind it, and what is behind this
 * window is a moving, unevenly lit town.
 *
 * So: a solid fill. Every other window in the game sits on painted art and hides what is under it
 * completely; this one had no art then, and "no art" should still mean "not a window you can see
 * through". It has its painting now (ui\levski_bg.png), so this ground only draws when the skin
 * fails to load - see DrawLevskiRoar.
 *
 * The indices are DrawOrnateBorder's own, with its measured RGB in the comments - not a `PAL16_x +
 * n` expression. PAL16_GRAY + 12/15 was the first attempt and came out near-white (user screenshot,
 * 2026-08-19): the top of the palette is the UI's white end, not the dark end of a grey ramp, so
 * the arithmetic that reads sensibly - "a high offset is a dark shade", per the palette header's
 * own dark-blue example - is simply false for that one ramp. Naming proven indices removes the
 * guess. Both live in the shared upper half (128-255), identical across town and all four
 * tilesets, so the fill cannot recolour itself by level.
 */
constexpr uint8_t PanelFillColor = 204; // (57, 49, 29) - the border's own shadow tone, dark stone
/** The grid cells, near-black against the panel's dark stone - so a slot reads as a recessed well
 * waiting for a stone rather than as a square someone drew on the stone (user screenshot,
 * 2026-08-19: the cells were the same colour as the panel and read as decoration). */
constexpr uint8_t SlotFillColor = 223; // (15, 5, 0)

void DrawPanelGround(const Surface &out, const Rectangle &rect, uint8_t fill = PanelFillColor)
{
	FillRect(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, fill);
	DrawOrnateBorder(out, rect);
}

/** @brief The recipe book's lines, pre-wrapped to its own text width - the formulas are long
 * enough that "1 socketed item -> the item, emptied, and its stones back" ran off the panel and
 * the last line was sliced by the bottom edge. */
/**
 * @brief The recipe the Transmute button will run, or -1 for "whatever is ready".
 *
 * WHY THIS EXISTS. Until v1.9.18 the monument auto-picked - first the lowest-numbered ready recipe,
 * then the one consuming the most grid slots. Both worked while the recipes had disjoint inputs.
 * Neither survives the tier ladder: Ennoble Rares and Reroll Rares take the SAME target and the
 * SAME material at different counts, and a reagent stack of five sits in ONE slot, so nearly every
 * item recipe ties at two slots and the tie-break decides for the player.
 *
 * So the player decides. Clicking a recipe in the book selects it; clicking it again clears the
 * selection back to automatic. Not persisted - the grid is not either, and a crafting station that
 * remembers a mode across sessions is a mode you can forget you set.
 */
int SelectedRecipe = -1;

/** @brief Whose recipe book the open window shows (Levski's Cube, 2026-09-20). */
/** @brief How far the recipe book is scrolled, in pixels. Clamped on every draw. */
int RecipeBookScroll = 0;

std::vector<int> CubeListRecipes()
{
	std::vector<int> recipes;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		if (RecipeBelongsTo(i, WindowHost))
			recipes.push_back(i);
	}
	return recipes;
}

std::string RecipeBookText(int width)
{
	std::string page;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		if (i > 0)
			page += '\n';
		page += _(CraftingRecipeName(i));
		page += '\n';
		page += WordWrapString(_(CraftingRecipeInputs(i)), width - Padding * 2, GameFont12);
		page += '\n';
	}
	return page;
}

/**
 * @brief Where each recipe's block sits in @p page, scroll already applied.
 *
 * ONE geometry, read by the draw and by the click. The rows are not a fixed height - each formula
 * wraps to as many lines as it needs - so a click handler that divided by a row height would drift
 * further out of step with every recipe added, and would drift silently.
 */
struct RecipeRow {
	int top;
	int height;
};

std::vector<RecipeRow> RecipeBookRows(const Rectangle &page)
{
	const Rectangle inner = RecipeBookInner(page);
	std::vector<RecipeRow> rows;
	rows.reserve(CraftingRecipeCount);
	const int textWidth = inner.size.width - Padding * 2;
	int y = inner.position.y + Padding + HeaderHeight - RecipeBookScroll;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		// A recipe of another host's book is a zero-height row: the vector stays indexed by recipe
		// (every reader does rows[i]) and the draw and click loops skip it (Levski's Cube, 2026-09-20).
		if (!RecipeBelongsTo(i, WindowHost)) {
			rows.push_back({ y, 0 });
			continue;
		}
		const int lineHeight = GetLineHeight(_(CraftingRecipeName(i)), GameFont12);
		const std::string formula = WordWrapString(_(CraftingRecipeInputs(i)), textWidth, GameFont12);
		const int formulaLines = static_cast<int>(std::count(formula.begin(), formula.end(), '\n')) + 1;
		const int height = lineHeight + lineHeight * formulaLines + 6;
		rows.push_back({ y, height });
		y += height;
	}
	return rows;
}

/** @brief How far the book can scroll before the last recipe's foot reaches the panel's. */
int RecipeBookMaxScroll(const Rectangle &page)
{
	const Rectangle inner = RecipeBookInner(page);
	if (inner.size.height <= 0)
		return 0;
	// Measured from the UNSCROLLED layout, so the answer does not depend on where the book already
	// is - a max that moved with the offset is how a scroll runs away from its own bound.
	const int saved = RecipeBookScroll;
	RecipeBookScroll = 0;
	const std::vector<RecipeRow> rows = RecipeBookRows(page);
	RecipeBookScroll = saved;
	if (rows.empty())
		return 0;
	const int contentBottom = rows.back().top + rows.back().height;
	const int visibleBottom = inner.position.y + inner.size.height - Padding;
	return std::max(0, contentBottom - visibleBottom);
}

Point GridOrigin(const Rectangle &window)
{
	// Where the skin paints its wells: the Roar's and the Cube's share (26,106); the canvas has its own.
	const ListSkinGeometry *skin = ListSkin();
	const Point origin = skin != nullptr ? skin->gridOrigin : levski_skin::GridOrigin;
	return window.position + Displacement { origin.x, origin.y };
}

Rectangle CellRect(const Rectangle &window, int cell)
{
	// Stepped by the PAINTED pitch, sized as the item cell (2026-09-05, the 1:1 painting): the
	// grid is painted at 29 and the item sprite is 28, so the cell sits inside its painted square
	// with the rule around it rather than the painting being squeezed to make the two agree.
	const Point origin = GridOrigin(window);
	return Rectangle { { origin.x + (cell % LevskiGridColumns) * levski_skin::GridPitch, origin.y + (cell / LevskiGridColumns) * levski_skin::GridPitch },
		{ CellSize, CellSize } };
}

/**
 * @brief The cell's whole painted square - the 29px pitch, rules included - for hit-testing.
 *
 * CellRect is the 28px ITEM cell, which leaves a one-pixel seam between neighbours where a click
 * landed on nothing (user, 2026-09-05: "i am having some difficulty placing my items exactly where
 * i want them"). The seam belongs to the cell it borders, so the hit rect is the pitch square.
 */
Rectangle CellHitRect(const Rectangle &window, int cell)
{
	const Point origin = GridOrigin(window);
	return Rectangle { { origin.x + (cell % LevskiGridColumns) * levski_skin::GridPitch, origin.y + (cell / LevskiGridColumns) * levski_skin::GridPitch },
		{ levski_skin::GridPitch, levski_skin::GridPitch } };
}

/** @brief The cell under @p position, or -1. */
int CellAt(const Rectangle &window, Point position)
{
	if (!PageHasGrid())
		return -1; // the Recipes tab is the whole window: there are no cells under the pointer
	for (int cell = 0; cell < LevskiGridSlots; cell++) {
		if (CellHitRect(window, cell).contains(position))
			return cell;
	}
	return -1;
}

/**
 * @brief Where a HELD item of @p size lands when dropped at @p position: its top-left cell.
 *
 * The backpack's rule, exactly (inv.cpp FindTargetSlotUnderItemCursor): the cursor carries the
 * item by its CENTRE, so the cell under the cursor is the item's middle cell, not its corner. This
 * grid used to take the clicked cell as the top-left, which put a 2x3 armour one cell right and
 * one down from where it was drawn under the cursor - the difficulty the user reported. An even
 * size has no middle cell, so the half the cursor is in decides, with the same 14px probe. Clamped
 * to the grid, so dropping near an edge slides the item in rather than refusing.
 */
int TargetAnchorUnderItemCursor(const Rectangle &window, Point position, Size size)
{
	const int hot = CellAt(window, position);
	if (hot < 0)
		return -1;
	if (size.width <= 1 && size.height <= 1)
		return hot;
	constexpr int HalfCell = levski_skin::CellSize / 2;
	Displacement offset { (size.width - 1) / 2, (size.height - 1) / 2 };
	const Rectangle hotRect = CellHitRect(window, hot);
	if (size.width % 2 == 0 && hotRect.contains(position + Displacement { HalfCell, 0 }))
		offset.deltaX++;
	if (size.height % 2 == 0 && hotRect.contains(position + Displacement { 0, HalfCell }))
		offset.deltaY++;
	const int row = std::clamp(hot / LevskiGridColumns - offset.deltaY, 0, LevskiGridRows - size.height);
	const int column = std::clamp(hot % LevskiGridColumns - offset.deltaX, 0, LevskiGridColumns - size.width);
	return row * LevskiGridColumns + column;
}

/**
 * @brief The ANCHOR of the item under the cursor, or -1. The grid's answer to pcursinvitem.
 *
 * Anchor rather than cell, because that is what identifies an ITEM here: a 2x3 armour occupies six
 * cells and GridCells[c] holds its anchor + 1 in every one of them, so hovering any part of it has
 * to name the same item. Both the draw and the tooltip ask this, which is what keeps the outlined
 * item and the described item from ever being two different items.
 */
int HoveredAnchor()
{
	if (!WindowOpen)
		return -1;
	const int cell = CellAt(GetLevskiRoarRect(), MousePosition);
	if (cell < 0 || GridCells[cell] == 0)
		return -1;
	return GridCells[cell] - 1;
}

/** @brief Whether an item of @p size can sit with its top-left at @p anchor. */
bool FitsAt(int anchor, Size size)
{
	const int column = anchor % LevskiGridColumns;
	const int row = anchor / LevskiGridColumns;
	if (column + size.width > LevskiGridColumns || row + size.height > LevskiGridRows)
		return false;
	for (int y = 0; y < size.height; y++) {
		for (int x = 0; x < size.width; x++) {
			if (GridCells[(row + y) * LevskiGridColumns + column + x] != 0)
				return false;
		}
	}
	return true;
}

void MarkCells(int anchor, Size size, int8_t value)
{
	const int column = anchor % LevskiGridColumns;
	const int row = anchor / LevskiGridColumns;
	for (int y = 0; y < size.height; y++) {
		for (int x = 0; x < size.width; x++)
			GridCells[(row + y) * LevskiGridColumns + column + x] = value;
	}
}

/**
 * @brief Puts @p item in the grid, preferring @p preferredAnchor. True if it found room.
 *
 * @p preferredAnchor of -1, or one the item does not fit at, falls back to the first cell it does
 * fit at - so a click that lands slightly off still does what the player meant, rather than nothing.
 */
bool PlaceInGrid(const Item &item, int preferredAnchor)
{
	const Size size = GetInventorySize(item);
	int anchor = (preferredAnchor >= 0 && FitsAt(preferredAnchor, size)) ? preferredAnchor : -1;
	for (int candidate = 0; anchor < 0 && candidate < LevskiGridSlots; candidate++) {
		if (FitsAt(candidate, size))
			anchor = candidate;
	}
	if (anchor < 0)
		return false;
	GridItems[anchor] = item;
	MarkCells(anchor, size, static_cast<int8_t>(anchor + 1));
	return true;
}

/**
 * @brief Collects the non-empty items of @p source into @p out, largest footprint first.
 *
 * Largest first is the placement order, not merely a tidy one: a 2x3 placed after four runes may
 * find no run of free cells left, while the runes always fit around it. Both the simulation and the
 * real rebuild sort this way, from this one function, so the answer and the act cannot disagree.
 */
int CollectLargestFirst(const Item *source, int sourceCount, Item *out)
{
	int count = 0;
	for (int i = 0; i < sourceCount; i++) {
		if (!source[i].isEmpty())
			out[count++] = source[i];
	}
	std::sort(out, out + count, [](const Item &a, const Item &b) {
		const Size sa = GetInventorySize(a);
		const Size sb = GetInventorySize(b);
		return sa.width * sa.height > sb.width * sb.height;
	});
	return count;
}

/**
 * @brief Rebuilds the occupancy map from GridItems. False if something could not be placed.
 *
 * The recipes rewrite GridItems in place - three gems become one, a socketed item becomes an item
 * plus its stones - without any idea of footprints, and the result's sizes are not the inputs'. So
 * after a transmute the map is re-derived rather than patched: collect what is there, clear, and
 * re-place. Anchors may move, which is correct; the alternative is a stone drawn over a helmet.
 *
 * The return value exists because PlaceInGrid CAN fail - twelve array slots is not twelve free
 * cells - and for a long time its failure was discarded, which turned "no room" into an item that
 * quietly stopped existing. Nothing here can put the item anywhere else, so the honest thing is to
 * report the failure and let the caller undo the whole transmute (see the Transmute button).
 */
bool RebuildGridOccupancy()
{
	Item items[LevskiGridSlots];
	const int count = CollectLargestFirst(GridItems, LevskiGridSlots, items);
	for (Item &slot : GridItems)
		slot.clear();
	for (int8_t &cell : GridCells)
		cell = 0;
	bool allPlaced = true;
	for (int i = 0; i < count; i++) {
		if (!PlaceInGrid(items[i], -1))
			allPlaced = false;
	}
	return allPlaced;
}

Rectangle TransmuteButtonRect(const Rectangle &window)
{
	return ButtonRect(window, levski_skin::Transmute);
}

Rectangle RecipeButtonRect(const Rectangle &window)
{
	return ButtonRect(window, levski_skin::Recipes);
}

/** @brief Salvage button @p index, counting down the column from the top. */
Rectangle SalvageButtonRect(const Rectangle &window, int index)
{
	return ButtonRect(window, levski_skin::SalvageFirst + index);
}

/**
 * @brief Hands everything in the grid back to the player. True when the grid is empty afterwards.
 *
 * A full backpack is the one case that has to be handled rather than assumed away, and the answer
 * is to REFUSE THE CLOSE rather than to drop on the floor: there is no exported "drop this item
 * here" call, and inventing one to solve a UI problem is how a stone ends up on a floor the player
 * has already left. Keeping the window open loses nothing and says why.
 */
bool ReturnGridToPlayer()
{
	Player &player = *MyPlayer;
	bool allReturned = true;
	for (Item &item : GridItems) {
		if (item.isEmpty())
			continue;
		// The backpack, else the stash (audit, 2026-09-27) - the workshop's rule since v1.12.189. Leaving the game
		// closes this window and then clears the grid: with only the backpack, a full one lost everything staged.
		if (AutoPlaceItemInInventory(player, item, true) || AutoPlaceItemInStash(player, item, true))
			item.clear();
		else
			allReturned = false;
	}
	// Rebuild rather than patch: a partial return leaves some items behind, and their occupancy has
	// to match what is actually still in the grid.
	RebuildGridOccupancy();
	return allReturned;
}

} // namespace


bool LevskiGridCanHold(const Item *items, int count)
{
	// More items than array slots cannot be held whatever their sizes, and the scratch arrays below
	// are exactly LevskiGridSlots long.
	if (count > LevskiGridSlots)
		return false;

	// The real grid arrays are borrowed as the scratch space and put back afterwards. Ugly, but it
	// is what makes this the SAME packing that will actually run: a separate simulation with its
	// own occupancy map is a second implementation, and a second implementation of "does it fit"
	// is exactly how a check comes to disagree with the thing it is checking.
	Item savedItems[LevskiGridSlots];
	int8_t savedCells[LevskiGridSlots];
	std::copy(std::begin(GridItems), std::end(GridItems), savedItems);
	std::copy(std::begin(GridCells), std::end(GridCells), savedCells);

	Item ordered[LevskiGridSlots];
	const int orderedCount = CollectLargestFirst(items, count, ordered);

	for (Item &slot : GridItems)
		slot.clear();
	for (int8_t &cell : GridCells)
		cell = 0;
	bool allFit = true;
	for (int i = 0; i < orderedCount; i++) {
		if (!PlaceInGrid(ordered[i], -1))
			allFit = false;
	}

	std::copy(std::begin(savedItems), std::end(savedItems), GridItems);
	std::copy(std::begin(savedCells), std::end(savedCells), GridCells);
	return allFit;
}

bool HandleLevskiRecipeBookScroll(int notches)
{
	if (WindowOpen && CubeSkin() && !PaintedButtons()) {
		// The bezel list scrolls a line per notch, and only while the cursor is on the window - the
		// wheel elsewhere still belongs to whatever is under it.
		if (!GetLevskiRoarRect().contains(MousePosition))
			return false;
		const Rectangle opening = CubeRecipeOpening();
		if (opening.size.width == 0)
			return false; // the grid page has no list to scroll
		const int maxScroll = RecipeListMaxScroll(opening, CubeListRecipes());
		if (maxScroll <= 0)
			return false; // nothing to move: let the wheel fall through
		constexpr int PixelsPerNotch = 20;
		CubeListScroll = std::clamp(CubeListScroll - notches * PixelsPerNotch, 0, maxScroll);
		return true;
	}
	if (!WindowOpen || !RecipeBookOpen)
		return false;
	const Rectangle book = GetLevskiRecipeBookRect();
	// Only with the cursor on the book, as the bezel list above: it took the wheel from the Abilities list, the event log
	// and the dungeon zoom (round 25 audit).
	if (!book.contains(MousePosition))
		return false;
	const Rectangle bookInner = RecipeBookInner(book);
	if (bookInner.size.height <= 0)
		return false;
	// A wheel notch moves about one recipe's worth. Bounded at BOTH ends, for the reason recorded
	// on the skill picker's own scroll: without the upper bound the wheel pushes the list past its
	// last row and the panel goes blank, which reads as a crash rather than as the end of a list.
	constexpr int PixelsPerNotch = 40;
	RecipeBookScroll = std::clamp(RecipeBookScroll - notches * PixelsPerNotch, 0, RecipeBookMaxScroll(book));
	return true;
}

const Item *HoveredLevskiGridItem()
{
	const int anchor = HoveredAnchor();
	return anchor < 0 ? nullptr : &GridItems[anchor];
}

bool SetLevskiHoverInfoString()
{
	// The controls first (2026-09-05): icon plates carry no label, so the info panel says what each
	// one does while the cursor is on it. The salvage line names the BACKPACK, because that is what
	// SalvageAllInBackpack acts on - not the grid the cursor is next to.
	if (WindowOpen) {
		const Rectangle window = GetLevskiRoarRect();
		if (CubeSkin()) {
			// The recipe under the cursor, asked of the shared list rather than of a row table
			// (2026-09-22): the lines are wrapped now, so a recipe is a block of them and its height
			// depends on its own text. RecipeListHitTest runs the draw's own arithmetic backwards,
			// which is the only way the two cannot drift.
			//
			// The hint still says what the page cannot: whether the grid can actually run it.
			if (const Rectangle opening = CubeRecipeOpening(); opening.size.width > 0) {
				const int recipe = RecipeListHitTest(opening, CubeListRecipes(), CubeListScroll, MousePosition);
				if (recipe >= 0) {
					const bool ready = CanCraftFromLevskiGrid(GridItems, recipe);
					SetPanelString(_(CraftingRecipeName(recipe)), ready ? UiFlags::ColorGold : UiFlags::ColorWhitegold);
					AddPanelString(_(CraftingRecipeInputs(recipe)), UiFlags::ColorWhite);
					ShowPanelStringsAsHintCard(); // the vendors' card, as every button here (dev note, 2026-09-27)
					return true;
				}
			}
		}
		if (const SalvageLayout *page = SalvagePage(); page != nullptr && page->itemIcon.size.width > 0) {
			const Rectangle rect { window.position + Displacement { page->itemIcon.position.x, page->itemIcon.position.y }, page->itemIcon.size };
			if (rect.contains(MousePosition)) {
				SetPanelString(_("Salvage an Item"), UiFlags::ColorWhitegold);
				AddPanelString(_("Click for the hammer, then click any item in your pack."), UiFlags::ColorWhite);
				AddPanelString(_("It is destroyed and its materials are yours."), UiFlags::ColorWhite);
				ShowPanelStringsAsHintCard();
				return true;
			}
		}
		for (int i = levski_skin::Close + 1; i < levski_skin::ButtonCount; i++) {
			if (i >= levski_skin::SalvageFirst && WindowHost != TransmuteHost::Smith)
				continue; // not drawn there, so not hoverable (Griswold's plates)
			if (!ButtonRect(window, i).contains(MousePosition))
				continue;
			// Transmute - the painted diamond on the Cube, the plate in the artisans' books - wears the gold card
			// (dev note, 2026-09-28: "add golden tooltip to transmute buttons, including the diamond in levski's
			// cube"). Silent since 2026-09-12, when a one-line tooltip only repeated the word on the plate; the card
			// says what a press would do: the recipe it would run, the one picked that is not ready, or how to begin.
			if (i == levski_skin::Transmute) {
				SetPanelString(_("Transmute"), UiFlags::ColorWhitegold);
				const int ready = SelectedRecipe >= 0 ? SelectedRecipe : FirstReadyLevskiRecipeFor(GridItems, WindowHost);
				if (ready >= 0 && CanCraftFromLevskiGrid(GridItems, ready)) {
					AddPanelString(StrCat(_("Makes: "), _(CraftingRecipeName(ready))), UiFlags::ColorGold);
					AddPanelString(_("Click to transmute what is in the grid."), UiFlags::ColorWhite);
				} else if (SelectedRecipe >= 0) {
					AddPanelString(StrCat(_(CraftingRecipeName(SelectedRecipe)), _(" is not ready")), UiFlags::ColorRed);
					AddPanelString(_(CraftingRecipeInputs(SelectedRecipe)), UiFlags::ColorWhite);
				} else {
					AddPanelString(_("Put a recipe's ingredients in the grid, then click here."), UiFlags::ColorWhite);
				}
				ShowPanelStringsAsHintCard();
				return true;
			}
			// A card with a body, as the vendors' are: the title alone was the whole tooltip.
			if (i == levski_skin::Recipes) {
				SetPanelString(_("Recipes"), UiFlags::ColorWhitegold);
				AddPanelString(_("Opens the recipe book."), UiFlags::ColorWhite);
			} else {
				const char *tier = SalvageTierName(static_cast<SalvageTier>(i - levski_skin::SalvageFirst));
				SetPanelString(StrCat("Salvage all ", _(tier), " in backpack"), UiFlags::ColorWhitegold);
				AddPanelString(_("Every item of this quality on every backpack page is destroyed, and its materials are yours."), UiFlags::ColorWhite);
			}
			ShowPanelStringsAsHintCard();
			return true;
		}
	}

	const int anchor = HoveredAnchor();
	if (anchor < 0)
		return false;

	// The stash's own three lines, and deliberately those exact three (see CheckStashHLight): the
	// name through SetPanelString so the tier colour is recorded as line 0's, then the full block for
	// an identified item and the durability line for one that is not. Written the same way so an item
	// reads identically wherever the player is looking at it - which is the whole of the request.
	const Item &item = GridItems[anchor];
	SetPanelString(item.getName(), item.getTextColor());
	if (item._iIdentified)
		PrintItemDetails(item);
	else
		PrintItemDur(item);
	return true;
}

namespace {

/**
 * @brief The Transmute button's work, run on its release (audit, 2026-09-29: it ran on the press, against the game-wide
 * rule that a button acts on a release inside it).
 */
bool RunLevskiTransmute()
{
	// TRANSACTIONAL. The recipes rewrite GridItems with no idea of footprints, and freeing
	// sockets is the one that gives back more than it takes - so the repack afterwards can find
	// it has nowhere to put something. Before this snapshot the repack simply dropped whatever
	// would not fit, and a rune or a jewel stopped existing with no message. TransmuteLevskiGrid
	// pre-checks the footprints now, so a rollback here should be unreachable; it stays because
	// "should be unreachable" is not a guarantee to stake a player's stones on, and the next
	// recipe added will not remember to ask.
	Item snapshotItems[LevskiGridSlots];
	int8_t snapshotCells[LevskiGridSlots];
	std::copy(std::begin(GridItems), std::end(GridItems), snapshotItems);
	std::copy(std::begin(GridCells), std::end(GridCells), snapshotCells);

	// Asked BEFORE the transmute, because the transmute reports what it MADE and a refusal made
	// nothing. A selected recipe that cannot run has to say so out loud - a Transmute button
	// that silently does nothing is the exact ambiguity this fork has shipped twice already.
	if (SelectedRecipe >= 0 && !CanCraftFromLevskiGrid(GridItems, SelectedRecipe)) {
		LogEvent(StrCat("Levski's Cube: ", _(CraftingRecipeName(SelectedRecipe)), " is not ready"));
		return true;
	}
	// With no recipe picked, the readiest recipe of THIS host's book - never another host's.
	const int recipe = SelectedRecipe >= 0 ? SelectedRecipe : FirstReadyLevskiRecipeFor(GridItems, WindowHost);
	const std::string result = recipe >= 0 ? TransmuteLevskiGridWith(GridItems, recipe) : std::string {};
	if (!RebuildGridOccupancy()) {
		std::copy(std::begin(snapshotItems), std::end(snapshotItems), GridItems);
		std::copy(std::begin(snapshotCells), std::end(snapshotCells), GridCells);
		LogEvent("Levski's Roar: not enough room - nothing was transmuted");
		return true;
	}
	// STAMP USABILITY on everything the transmute left behind (user, 2026-08-28: "picking up a
	// crafted item the first time colors it in RED").
	//
	// It was red because _iStatFlag was false. Nothing in the crafting path ever set it - the
	// recipes build items and hand them back, and the flag is normally written by CalcPlrInv,
	// which walks the worn slots and the backpack and has no idea this grid exists. So a fresh
	// item sat here with the flag clear, DrawItem read that as "the character cannot use this"
	// and tinted it through the infravision TRN, which is red. It corrected itself the moment
	// the item reached the backpack and CalcPlrInv ran over it, which is exactly why it was only
	// ever seen once per item.
	for (Item &item : GridItems) {
		if (!item.isEmpty())
			item._iStatFlag = MyPlayer->CanUseItem(item);
	}
	if (!result.empty())
		LogEvent(StrCat("Levski's Cube: ", result));
	// Salvage's sound for a transmute that MADE something. The room refusals consumed nothing
	// and stay as quiet as the other refusals above; crafting.cpp owns their wording.
	// The Cube's own flash (RfA-20 batch 43d) on its book; the artisans keep the salvage-era sound.
	if (!result.empty() && !IsTransmuteRefusal(result)
	    && !(WindowHost == TransmuteHost::Cube && PlayUiEventSound(UiEventSound::CubeTransmute))
	    && !PlayUiEventSound(UiEventSound::Transmute))
		PlaySFX(IS_ISHIEL); // the old stand-in, if the transmute sound is not in the archive
	return true;
}

} // namespace

bool IsLevskiRoarOpen() { return WindowOpen; }
void ReleaseLevskiButtons()
{
	// What was pressed, read before it is cleared below: the Cube's controls act only on a release inside the control
	// pressed (audit, 2026-09-29 - the X, Recipes, Transmute, the salvage plates and the hammer acted on the press, and a
	// bulk salvage plate destroyed every matching item with no way to slide off it).
	const bool closePressed = std::exchange(PressedCloseButton, false);
	const bool transmutePressed = std::exchange(PressedTransmute, false);
	const int cubeButtonPressed = PressedCubeButton;
	const int salvageIconPressed = PressedSalvageIcon;
	// The Cube's tabs spring back with everything else, and the page turns only if the release landed
	// back inside the tab that was pressed. Always cleared, so a press that outlived its window
	// cannot turn a page later.
	if (const int tab = PressedCubeTab; tab >= 0) {
		PressedCubeTab = -1;
		if (WindowOpen && CubeTabbedPages() && GetSideTabRect(tab).contains(MousePosition)) {
			const auto wanted = static_cast<CubeTab>(tab);
			if (wanted != OpenCubeTab) {
				OpenCubeTab = wanted;
				CubeListScroll = 0; // a page that opens mid-list looks like it lost the first recipes
				PlayUiSelectSound();
			}
		}
	}
	// Griswold's tab column beside the Salvage page springs back with everything else on it, and the shelf it
	// was pressed on opens only if the release landed back inside the same tab.
	if (const SalvageLayout *page = SalvagePage(); WindowOpen && page != nullptr && page->docked) {
		const TalkID tab = TakeReleasedShopTab();
		if (tab != TalkID::None && tab != TalkID::SmithTransmute) {
			CloseLevskiRoar();
			if (!WindowOpen)
				StartStore(tab);
			PressedCubeButton = -1;
			PressedSalvageIcon = -1;
			PressedConfirmButton = -1;
			return;
		}
	}
	PressedCubeButton = -1;
	PressedSalvageIcon = -1; // Griswold's salvage icons spring back too (2026-09-21)
	if (WindowOpen) {
		const Rectangle window = GetLevskiRoarRect();
		if (closePressed) {
			if (CloseButtonRect(window).contains(MousePosition)) {
				CloseLevskiRoar();
				if (!WindowOpen)
					PlayUiMoveSound();
			}
			return;
		}
		if (salvageIconPressed == SalvageTierCount) {
			// The eighth plate arms the hammer; the next click on a backpack item breaks that item down.
			const SalvageLayout *page = SalvagePage();
			if (page != nullptr && page->itemIcon.size.width > 0) {
				const Rectangle rect { window.position + Displacement { page->itemIcon.position.x, page->itemIcon.position.y }, page->itemIcon.size };
				if (rect.contains(MousePosition)) {
					SalvageItemCursorArmed = true;
					NewCursor(CURSOR_REPAIR); // vanilla's hammer, as the user asked; the click is ours
				}
			}
			return;
		}
		if (salvageIconPressed >= 0 && salvageIconPressed < SalvageTierCount) {
			if (!SalvageButtonRect(window, salvageIconPressed).contains(MousePosition))
				return;
			if (SalvageItemCursorArmed) {
				SalvageItemCursorArmed = false; // a bulk plate takes the hammer back
				NewCursor(CURSOR_HAND);
			}
			const auto tier = static_cast<SalvageTier>(salvageIconPressed);
			PressedConfirmButton = -1;
			if (SalvageSkin() && SalvageTierNeedsConfirm(tier)) {
				// The dear tiers ask first; the question replaces whatever the frame was showing.
				PendingConfirmTier = salvageIconPressed;
				return;
			}
			PendingConfirmTier = -1; // a cheap plate answers the standing question by simply doing its own work
			RunSalvageTier(tier);
			return;
		}
		if (cubeButtonPressed == levski_skin::Recipes && RecipeButtonRect(window).contains(MousePosition)) {
			RecipeBookOpen = !RecipeBookOpen;
			return;
		}
		if (transmutePressed) {
			if (TransmuteButtonRect(window).contains(MousePosition))
				RunLevskiTransmute();
			return;
		}
	}
	// The confirmation's two buttons act on the RELEASE, and only when it lands inside the button that was
	// pressed (user, 2026-09-21: "Release of click outside the boundary of any of these buttons is considered
	// as Let Me Think a Bit More by the user") - so a release anywhere else leaves the question standing.
	if (PressedConfirmButton < 0)
		return;
	const int which = PressedConfirmButton;
	PressedConfirmButton = -1;
	const SalvageLayout *page = SalvagePage();
	if (page == nullptr || PendingConfirmTier < 0)
		return;
	const Rectangle window = GetLevskiRoarRect();
	const Rectangle results { window.position + Displacement { page->results.position.x, page->results.position.y }, page->results.size };
	if (!SalvageConfirmButtonRect(results, which).contains(MousePosition))
		return; // let me think a bit more
	const auto tier = static_cast<SalvageTier>(PendingConfirmTier);
	PendingConfirmTier = -1;
	PlayUiMoveSound();
	if (which == ConfirmButton)
		RunSalvageTier(tier);
}
bool IsSalvageItemCursorArmed()
{
	return SalvageItemCursorArmed;
}

void CancelSalvageItemCursor()
{
	SalvageItemCursorArmed = false;
}

bool UseSalvageItemCursor(Player &player, int tab, int index)
{
	SalvageItemCursorArmed = false;
	SalvageTier tier = SalvageTier::White;
	int materials = 0;
	if (!SalvageSingleItem(player, tab, index, &tier, &materials)) {
		// Say why when it is the stones: Free the Sockets takes them out first (round 26 audit).
		const Item *target = tab < 0 ? (index >= 0 && index < player._pNumInv ? &player.InvList[index] : nullptr)
		                              : (tab < Player::NumExtraInventoryTabs && index >= 0 && index < player._pNumInvTab[tab] ? &player.InvTabList[tab][index] : nullptr);
		if (target != nullptr && target->socketedCount() > 0)
			LogEvent("Its stones would be lost - use Free the Sockets first.", UiFlags::ColorWhite);
		else
			LogEvent("That cannot be salvaged.", UiFlags::ColorWhite);
		return false;
	}
	// One item, so the window's message says one - the same two lines every bulk press writes.
	SalvageMessage = { true, tier, 1, materials };
	LogEvent(StrCat("Salvaged 1 ", _(SalvageTierName(tier)), " into ", materials, " ",
	             _(AllItemsList[SalvageMaterialFor(tier)].iName)),
	    UiFlags::ColorWhitegold);
	if (!PlayUiEventSound(UiEventSound::Salvage))
		PlaySFX(IS_ISHIEL);
	return true;
}

bool IsLevskiRecipeBookOpen() { return WindowOpen && RecipeBookOpen; }

bool IsLevskiRoarObject(const Object &object)
{
	// Two stands in town since 2026-09-20: the Stonegate is the other one (oracool/stonegate.h).
	// Four since 2026-09-22 - the gate's portal arch is one, and Wirt's cart is the fourth.
	//
	// This test is "a stand that is not the others", which is the shape that has broken three times
	// in this codebase the moment a third thing appeared. It wants inverting: the Cube should be
	// found by its own identity, as the gate and the cart already are. Not done with the cart,
	// because the Cube's id would have to survive a town reload and that is a change to the Cube.
	// Until then, every new stand in town must be named here or it becomes clickable as the Cube.
	return currlevel == 0 && !setlevel && object._otype == OBJ_STAND
	    && !IsStonegateObject(object) && !IsWirtCartObject(object);
}

void ProcessLevskiCubeAnimation()
{
	// Levski's Cube (batch 43, 2026-09-20): the object's sheet has thirteen frames - twelve idle, one
	// open - where the Roar's had one. Loop the idle while the window is shut, hold the open pose
	// while it is up. The Roar's one-frame sheet (any sheet short of thirteen) is left alone.
	constexpr uint32_t IdleFrames = 12;
	constexpr uint32_t OpenFrame = 13;
	constexpr int IdleDelay = 4;
	if (currlevel != 0 || setlevel)
		return;
	for (int i = 0; i < ActiveObjectCount; i++) {
		Object &object = Objects[ActiveObjects[i]];
		if (!IsLevskiRoarObject(object) || object._oAnimLen < OpenFrame)
			continue;
		object._oAnimFlag = 0;
		// The lid parts for the CUBE's own book only, not for Griswold's or Ogden's (audit, 2026-09-20).
		if (WindowOpen && WindowHost == TransmuteHost::Cube) {
			object._oAnimFrame = OpenFrame;
			object._oAnimCnt = 0;
			return;
		}
		if (++object._oAnimCnt < IdleDelay)
			return;
		object._oAnimCnt = 0;
		const uint32_t next = object._oAnimFrame + 1;
		object._oAnimFrame = (next < 1 || next > IdleFrames) ? 1 : next;
		return;
	}
}

void ToggleLevskiRoar()
{
	if (WindowOpen) {
		CloseLevskiRoar();
		if (!WindowOpen) // a full pack refuses the close, and says so in red - that stays as it is
			PlayUiMoveSound();
		return;
	}
	// Only on the OPEN half: the close above has already run, and a toggle that shuts the Cube has no
	// business closing a counter as well (2026-09-22).
	CloseOtherShopSurfaces();
	if (IsWorkshopOpen())
		return; // a bench that refused to close keeps the slot (audit, 2026-09-27)
	// And the left panels its docked pages cover: the hero sheet stayed open under the Cube, took the wheel, and reappeared
	// when the Cube closed (round 30 audit).
	if (!TakeLeftPanelSlot(LeftPanelContent::None))
		return;
	WindowHost = TransmuteHost::Cube;
	WindowOpen = true;
	RecipeBookOpen = false;
	SelectedRecipe = -1;
	CubeListScroll = 0;
	// The lid grinding a finger's width (RfA-20 batch 43d); the select click until the sound lands.
	if (!PlayUiEventSound(UiEventSound::CubeOpen))
		PlayUiSelectSound();
}

void OpenLevskiWindowFor(TransmuteHost host)
{
	if (WindowOpen && WindowHost == host)
		return;
	if (WindowOpen) {
		CloseLevskiRoar();
		if (WindowOpen)
			return; // the close was refused (no room for the grid's items); the book stays whose it was
	}
	WindowHost = host;
	WindowOpen = true;
	RecipeBookOpen = false;
	SelectedRecipe = -1;
	CubeListScroll = 0;
	// SILENT, on purpose (user, 2026-09-21: "There is an extra sound being played when i click Salvage tab.
	// remove it"). Every way into this window has already sounded by the time it opens: the shop's tab column
	// plays titlemov at the press, StoreEnter plays titlslct before it dispatches, and the workshop's tab does
	// the same as the shop's. Adding one here made the Salvage tab answer a single click twice, which is the
	// rule ui_sound.h states - "call these only on a path that is otherwise silent" - and this path is not.
	// ToggleLevskiRoar, the Cube object in town, is NOT this function and keeps its own lid-grinding sound.
}

TransmuteHost CurrentTransmuteHost()
{
	return WindowHost;
}

void ResetLevskiRoarForNewGame()
{
	// Unconditional, and it does NOT try to give anything back - by the time this runs the player is
	// being torn down and has already been saved, so a return would go nowhere.
	//
	// Audit, 2026-08-30. GridItems and WindowOpen are file-local statics, so they live for the whole
	// PROCESS, not the game. Leaving a game does not close this window: "Main Menu" and "Exit Game"
	// both funnel through GamemenuNewGame, which saves the character and clears gbRunGame without
	// closing anything, and CloseLevskiRoar is allowed to REFUSE while the pack is full. So the
	// window stayed open and the grid stayed full into the next character started in the same
	// session - which showed them someone else's items and let them take them out.
	//
	// CloseLevskiRoar is attempted before the save (see GamemenuNewGame), so anything that fits in
	// the backpack is kept and persisted. This is the backstop for what did not fit.
	for (Item &slot : GridItems)
		slot.clear();
	for (int8_t &cell : GridCells)
		cell = 0;
	WindowOpen = false;
	RecipeBookOpen = false;
	RecipeBookScroll = 0;
	CubeListScroll = 0;
	// The tab and the pressed buttons too: the next hero's first Cube opened on the last one's Recipes tab (round 22).
	OpenCubeTab = CubeTab::Cube;
	PressedCubeTab = -1;
	LastHoverCubeTab = -1;
	PressedCubeButton = -1;
	LastHoverCubeButton = -1;
	SalvageMessage = {};
	PressedSalvageIcon = -1;
	LastHoverSalvageIcon = -1;
	PendingConfirmTier = -1;
	PressedConfirmButton = -1;
	if (SalvageItemCursorArmed) {
		SalvageItemCursorArmed = false;
		if (pcurs == CURSOR_REPAIR)
			NewCursor(CURSOR_HAND);
	}
}

void CloseLevskiRoar()
{
	if (!WindowOpen)
		return;
	if (!ReturnGridToPlayer()) {
		// Once every few seconds at most (audit, 2026-09-27): a walk-away asks every tick.
		static uint32_t lastLogged = 0;
		const uint32_t now = SDL_GetTicks();
		if (lastLogged == 0 || now - lastLogged >= 5000) {
			lastLogged = now;
			LogEvent(std::string(_("Your pack and stash are full - Levski's Roar keeps what it holds.")), UiFlags::ColorRed);
		}
		return;
	}
	for (int8_t &cell : GridCells)
		cell = 0;
	WindowOpen = false;
	RecipeBookOpen = false;
	// The Cube reopens on its Cube tab (2026-09-21). A window that remembers which page it was on is
	// a window that sometimes opens on the recipe list when the player came to transmute - and the
	// grid they filled last time is emptied above, so there would be nothing to come back to.
	OpenCubeTab = CubeTab::Cube;
	PressedCubeTab = -1;
	LastHoverCubeTab = -1;
	PressedCubeButton = -1;
	LastHoverCubeButton = -1;
	PressedSalvageIcon = -1;
	LastHoverSalvageIcon = -1;
	PendingConfirmTier = -1;
	PressedConfirmButton = -1;
	if (SalvageItemCursorArmed) {
		SalvageItemCursorArmed = false;
		if (pcurs == CURSOR_REPAIR)
			NewCursor(CURSOR_HAND);
	}
}

bool PlaceItemInLevskiGrid(const Item &item)
{
	// The grid is packed by FOOTPRINT, so "is a slot free" and "does this fit" are different
	// questions and only PlaceInGrid answers the second. Callers must place before they remove.
	return WindowOpen && PlaceInGrid(item, -1);
}

Rectangle GetLevskiRoarRect()
{
	if (!WindowOpen)
		return Rectangle { { 0, 0 }, { 0, 0 } };
	// Centred on the play area, like the other operable-object windows - unless the recipe book is
	// open and the room to the left is short of it (audit, 2026-09-08: at 960 wide the centred window
	// leaves 287px and the 420px book clamped to x=0 covered the whole item grid). Then the window
	// slides right exactly as far as the book needs, and no further than the screen allows.
	const Size frame = CurrentFrameSize();
	if (const ListSkinGeometry *skin = ListSkin(); skin != nullptr && skin->docked)
		return Rectangle { { 0, BottomDockedTop(frame.height) }, frame };
	// Griswold's tall Salvage page docks where the shop panel does - bottom-left, same size (2026-09-21).
	if (const SalvageLayout *page = SalvagePage(); page != nullptr && page->docked)
		return Rectangle { { 0, BottomDockedTop(frame.height) }, frame };
	int x = (gnScreenWidth - frame.width) / 2;
	if (RecipeBookOpen) {
		const int needed = BookFrameSize(BookFrame::Tall).width + SlotGap;
		if (x < needed)
			x = std::min(needed, std::max(0, static_cast<int>(gnScreenWidth) - frame.width));
	}
	// CENTRED vertically (user, 2026-08-27: "Levski's Roar should be middle of screen"). It sat a
	// third of the way down before, and was briefly bottom-docked by a rule that was never meant for
	// it - the docking rule is about the side panels.
	const int y = std::max(0, (static_cast<int>(gnScreenHeight) - frame.height) / 2);
	return Rectangle { { x, y }, frame };
}

bool IsPointOverLevski(Point position)
{
	// The window and the two side tabs beside it (user, 2026-09-27: "fix the decisions for me too"): the tabs sit OUTSIDE
	// the window's rect, so a right click on one cast or walked, and what stood under it lit up on hover.
	if (GetLevskiRoarRect().contains(position))
		return true;
	if (WindowOpen && CubeTabbedPages()) {
		for (int i = 0; i < CubeTabCount; i++) {
			if (GetSideTabRect(i).contains(position))
				return true;
		}
	}
	return false;
}

Rectangle GetLevskiRecipeBookRect()
{
	if (!IsLevskiRecipeBookOpen())
		return Rectangle { { 0, 0 }, { 0, 0 } };
	const Rectangle window = GetLevskiRoarRect();
	// Height from the WRAPPED text, not from a per-recipe row guess: the formulas wrap to two lines
	// each and the fixed 40px row left the last one sliced by the panel's bottom edge.
	const int bookWidth = RecipeBookWidthFor(window);
	const std::string page = RecipeBookText(bookWidth);
	const int textHeight = static_cast<int>(GetLineHeight(page, GameFont12) * (std::count(page.begin(), page.end(), '\n') + 1));
	// CAPPED to the screen, and scrolled inside the cap (v1.9.18). The height used to be whatever
	// the wrapped text came to, which was fine for five recipes and stopped being fine at eighteen:
	// the panel simply grew past the bottom of a 720-tall screen and the last recipes could not be
	// read at all, let alone clicked.
	// The band the book is allowed to occupy: the top of the screen down to a 100px reserve above
	// the bottom (user, 2026-08-27: "Recipe book should be next to it, in the middle between top of
	// screen and 100px row above the bottom"), and 620 tall at most (user, 2026-08-27: "recipe
	// window of levski to be 620px high and scrollable").
	//
	// On the 720-tall screens this project targets those two numbers are the same number - 720 minus
	// the reserve IS 620 - so the book fills the band exactly and starts at y=0. They are written as
	// two rules anyway because they are two rules: the reserve is about the HUD, the 620 is a size,
	// and a screen that is not 720 tall must honour both rather than whichever happened to be
	// hardcoded.
	constexpr int BottomReserve = 100;
	constexpr int MaxBookHeight = 620;
	const int band = std::max(0, static_cast<int>(gnScreenHeight) - BottomReserve);
	// The painted frame's height (2026-09-05), not the text's: a painting is one size. The band
	// still caps it on a screen shorter than the frame; the text scrolls inside whatever is left.
	(void)textHeight;
	(void)MaxBookHeight;
	const int height = std::min(BookFrameSize(BookFrame::Tall).height, band);
	// LEFT of the window by preference: opening right ran the book under the mini-map, which owns
	// the top-right corner.
	//
	// But NOT unconditionally, and the previous comment here - "there is always room on the left,
	// the window is centred" - was simply false, which a screenshot caught (user, 2026-08-19: the
	// book's title read "IPES" and every line lost its first characters off the left edge). The
	// window is centred in gnScreenWidth, so the room to its left is (gnScreenWidth - FrameSize.width)/2,
	// and at 1024 wide that is 408 against a book needing 426. Centring guarantees symmetry, not
	// space.
	//
	// So: place it left and CLAMP at the screen edge. The first fix flipped it to the right of the
	// window when the left did not fit, and that was worse (user screenshot, 2026-08-19): the right
	// is where the inventory and the mini-map live, so the book landed on top of the panel the
	// player had open. Sliding left until it touches x=0 costs at most a few pixels of overlap with
	// Levski's own window - and the book is drawn after it, so the book stays readable.
	const int x = std::max(0, window.position.x - bookWidth - SlotGap);
	// Centred in that same band, so the book sits in the space it can actually use rather than in
	// the whole screen.
	const int y = std::max(0, (band - height) / 2);
	return Rectangle { { x, y }, { bookWidth, height } };
}

namespace {

/** @brief The press-flash slot a painted button maps to, or -1 for the close button, which has none. */
int FlashIndexForButton(int button)
{
	if (button == levski_skin::Transmute)
		return ButtonFlashTransmute;
	if (button == levski_skin::Recipes)
		return ButtonFlashRecipes;
	if (button >= levski_skin::SalvageFirst)
		return button - levski_skin::SalvageFirst;
	return -1;
}

/**
 * @brief Nearest-neighbour blit of @p sprite at @p scale, top-left at @p topLeft, through @p trn if given.
 *
 * Through a scratch surface rather than a scaled CLX: the grid holds at most twelve items and is a
 * window, so the per-frame cost is nothing, and a scaled list per cursor id would be a cache to
 * invalidate. Index 0 is treated as transparent - the item art's baked shadows drop at 3x, which is
 * a smaller wrong than a black halo three pixels wide.
 */
void DrawSpriteScaled(const Surface &out, Point topLeft, ClxSprite sprite, int scale, const uint8_t *trn)
{
	const int w = static_cast<int>(sprite.width());
	const int h = static_cast<int>(sprite.height());
	if (w <= 0 || h <= 0)
		return;
	OwnedSurface scratch(w, h);
	SDL_FillRect(scratch.surface, nullptr, 0);
	ClxDraw(scratch, { 0, h - 1 }, sprite);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			uint8_t index = *scratch.at(x, y);
			if (index == 0)
				continue;
			if (trn != nullptr)
				index = trn[index];
			for (int yy = 0; yy < scale; yy++) {
				const int dy = topLeft.y + y * scale + yy;
				if (dy < 0 || dy >= out.h())
					continue;
				for (int xx = 0; xx < scale; xx++) {
					const int dx = topLeft.x + x * scale + xx;
					if (dx < 0 || dx >= out.w())
						continue;
					out.SetPixelUnchecked({ dx, dy }, index); // an index, resolved by the surface (v1.11)
				}
			}
		}
	}
}

/** @brief The tall recipe book beside the window: the Roar's plate opens it, and the painted Cube UI's RECIPE BOOK button. */
void DrawTallRecipeBook(const Surface &out)
{
	const Rectangle page = GetLevskiRecipeBookRect();
	// The painted tall frame (user, 2026-09-05): dark backing in its core, the bezel over it, the
	// red X at the frame's top-right.
	DrawBookFrame(out, BookFrame::Tall, page);
	DrawWindowCloseButton(out, page);
	const Rectangle inner = RecipeBookInner(page);
	Point cursor = inner.position + Displacement { Padding, Padding };
	const int textWidth = inner.size.width - Padding * 2;
	DrawString(out, _("Recipes"), Rectangle { cursor, { textWidth, HeaderHeight } },
	    { UiFlags::ColorGold | UiFlags::FontSize24 });
	cursor.y += HeaderHeight;
	// One wrapped block rather than two DrawStrings per recipe at a guessed row height. The name
	// keeps its own colour, so each recipe is drawn as its own pair - but both lines are measured
	// from the SAME wrapped text the panel was sized from, which is what stops the last one being
	// sliced by the bottom edge.
	// Clamped HERE, every frame, rather than only where the wheel turns - the content's height
	// changes with the window width and with how the formulas wrap, so a scroll that was legal when
	// it was set can be past the end by the time it is drawn.
	RecipeBookScroll = std::clamp(RecipeBookScroll, 0, RecipeBookMaxScroll(page));

	const int clipTop = inner.position.y + Padding + HeaderHeight;
	const int clipBottom = inner.position.y + inner.size.height - Padding;
	const std::vector<RecipeRow> rows = RecipeBookRows(page);
	for (int i = 0; i < CraftingRecipeCount; i++) {
		const RecipeRow &row = rows[i];
		// Wholly outside the visible band: skipped rather than drawn and overdrawn. A partially
		// visible row is skipped too - half a formula reads as a rendering fault, not as a hint
		// that there is more below.
		if (row.top < clipTop || row.top + row.height > clipBottom)
			continue;

		if (row.height == 0)
			continue; // another host's recipe (Levski's Cube, 2026-09-20)
		const bool ready = CanCraftFromLevskiGrid(GridItems, i);
		const bool selected = SelectedRecipe == i;
		if (selected) {
			// The selection is a filled band behind the block, because the name's colour is
			// already carrying "can this run right now" and one text colour cannot say two things.
			FillRect(out, inner.position.x + Padding - 2, row.top - 2,
			    textWidth + 4, row.height - 2, ButtonFlashColor);
		}

		Point rowCursor { inner.position.x + Padding, row.top };
		const int lineHeight = GetLineHeight(_(CraftingRecipeName(i)), GameFont12);
		DrawString(out, _(CraftingRecipeName(i)), Rectangle { rowCursor, { textWidth, lineHeight } },
		    { (selected ? UiFlags::ColorWhite : (ready ? UiFlags::ColorGold : UiFlags::ColorWhitegold)) | UiFlags::FontSize12 });
		rowCursor.y += lineHeight;

		const std::string formula = WordWrapString(_(CraftingRecipeInputs(i)), textWidth, GameFont12);
		const int formulaLines = static_cast<int>(std::count(formula.begin(), formula.end(), '\n')) + 1;
		DrawString(out, formula, Rectangle { rowCursor, { textWidth, lineHeight * formulaLines } },
		    { UiFlags::ColorWhite | UiFlags::FontSize12 });
	}
	(void)cursor;
}

/**
 * @brief The Salvage Results frame (user, 2026-09-21: "make the Salvage Results frame border 1px thick and use same
 * color you use for items sprites frame"): ONE pixel, in inv.cpp's GridFrameGold - PAL16_YELLOW + 10, the darkened
 * gold every item on every grid is outlined with since v1.12.094, so the two frames are the same frame.
 */
constexpr uint8_t SalvageFrameGold = PAL16_YELLOW + 10;
/** How far the two lines bow from the border to clear the title, and how far along it the split takes. */
constexpr int SalvageTitleBow = 9;
constexpr int SalvageTitleReach = 7;

/**
 * @brief A quarter ellipse of single pixels from (cx + sx*rx, cy) round to (cx, cy + sy*ry).
 *
 * Stepped finely enough that the pixels touch at these radii; the curve is what rounds the corners where the
 * frame's top border splits around the title and merges back.
 */
void PlotQuarterArc(const Surface &out, Point centre, int rx, int ry, int sx, int sy, uint8_t color)
{
	constexpr int Steps = 48;
	for (int i = 0; i <= Steps; i++) {
		const double t = i * (M_PI / 2) / Steps;
		const int x = centre.x + static_cast<int>(std::lround(sx * rx * std::cos(t)));
		const int y = centre.y + static_cast<int>(std::lround(sy * ry * std::sin(t)));
		FillRect(out, x, y, 1, 1, color);
	}
}

/** @brief One of the two answer buttons inside @p results: CONFIRM then CANCEL, side by side along its foot. */
Rectangle SalvageConfirmButtonRect(const Rectangle &results, int which)
{
	const int span = 2 * ConfirmButtonSize.width + ConfirmButtonGap;
	const int left = results.position.x + (results.size.width - span) / 2 + which * (ConfirmButtonSize.width + ConfirmButtonGap);
	const int top = results.position.y + results.size.height - ConfirmButtonSize.height - ConfirmButtonBottomGap;
	return Rectangle { { left, top }, ConfirmButtonSize };
}

/** @brief A 1 px rectangle outline in @p rgb - four fills, so a framed button is a stack of these. */
void OutlineRectRgb(const Surface &out, Rectangle rect, uint32_t rgb, uint8_t fallback)
{
	FillRectRgb(out, rect.position.x, rect.position.y, rect.size.width, 1, rgb, fallback);
	FillRectRgb(out, rect.position.x, rect.position.y + rect.size.height - 1, rect.size.width, 1, rgb, fallback);
	FillRectRgb(out, rect.position.x, rect.position.y, 1, rect.size.height, rgb, fallback);
	FillRectRgb(out, rect.position.x + rect.size.width - 1, rect.position.y, 1, rect.size.height, rgb, fallback);
}

/** @brief Breaks every backpack item of @p tier down and writes the window's message. The answer to a plate. */
void RunSalvageTier(SalvageTier tier)
{
	FlashButton(static_cast<int>(tier)); // fires whether or not there was anything to salvage - it acknowledges the CLICK
	int materialsMade = 0;
	const int consumed = SalvageAllInBackpack(*MyPlayer, tier, &materialsMade);
	SalvageMessage = { true, tier, consumed, materialsMade };
	if (consumed > 0) {
		LogEvent(StrCat("Salvaged ", consumed, " ", _(SalvageTierName(tier)), " into ", materialsMade, " ",
		             _(AllItemsList[SalvageMaterialFor(tier)].iName)),
		    UiFlags::ColorWhitegold);
		if (!PlayUiEventSound(UiEventSound::Salvage))
			PlaySFX(IS_ISHIEL); // the old stand-in, if the salvage sound is not in the archive
	} else {
		LogEvent(StrCat("Nothing to salvage: ", _(SalvageTierName(tier))), UiFlags::ColorWhite);
	}
}
/**
 * @brief The frame round the results, and the title ON its top border (user, 2026-09-21: "put Salvage results title
 * somewhere along the top border of the salvage results frame and make sure that top border reaches the title, slipts
 * into two lines with rounded edges to outline the Salvage results text and merges again back into one border line").
 *
 * So the top border runs in from each side, curves apart into a line above the title and a line below it, and curves
 * back together - one continuous 1 px line that opens around the words and closes again.
 */
void DrawSalvageResultsFrame(const Surface &out, Rectangle inner, string_view title)
{
	const Rectangle f { { inner.position.x - 1, inner.position.y - 1 }, { inner.size.width + 2, inner.size.height + 2 } };
	const int left = f.position.x;
	const int right = f.position.x + f.size.width - 1;
	const int top = f.position.y;
	const int bottom = f.position.y + f.size.height - 1;
	FillRect(out, left, top, 1, f.size.height, SalvageFrameGold);
	FillRect(out, right, top, 1, f.size.height, SalvageFrameGold);
	FillRect(out, left, bottom, f.size.width, 1, SalvageFrameGold);
	if (title.empty()) {
		FillRect(out, left, top, f.size.width, 1, SalvageFrameGold);
		return;
	}
	const int half = GetLineWidth(title, GameFont12) / 2 + 7;
	const int centreX = f.position.x + f.size.width / 2;
	const int splitLeft = centreX - half;
	const int splitRight = centreX + half;
	// The border, in from each side as far as the curve.
	FillRect(out, left, top, splitLeft - SalvageTitleReach - left, 1, SalvageFrameGold);
	FillRect(out, splitRight + SalvageTitleReach, top, right - splitRight - SalvageTitleReach + 1, 1, SalvageFrameGold);
	// The two lines that hold the title between them.
	FillRect(out, splitLeft, top - SalvageTitleBow, splitRight - splitLeft + 1, 1, SalvageFrameGold);
	FillRect(out, splitLeft, top + SalvageTitleBow, splitRight - splitLeft + 1, 1, SalvageFrameGold);
	// And the four rounded corners: out of the border and up, out and down, and the same again on the way back in.
	PlotQuarterArc(out, { splitLeft, top }, SalvageTitleReach, SalvageTitleBow, -1, -1, SalvageFrameGold);
	PlotQuarterArc(out, { splitLeft, top }, SalvageTitleReach, SalvageTitleBow, -1, 1, SalvageFrameGold);
	PlotQuarterArc(out, { splitRight, top }, SalvageTitleReach, SalvageTitleBow, 1, -1, SalvageFrameGold);
	PlotQuarterArc(out, { splitRight, top }, SalvageTitleReach, SalvageTitleBow, 1, 1, SalvageFrameGold);
	DrawString(out, title, Rectangle { { splitLeft, top - SalvageTitleBow }, { splitRight - splitLeft, 2 * SalvageTitleBow } },
	    { UiFlags::ColorGold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
}
/** @brief Griswold's painted Salvage window: the forge, the title, the seven icons with the button feel, the results. */
void DrawSalvageWindow(const Surface &out, const Rectangle &window)
{
	const SalvageLayout *page = SalvagePage();
	if (page == nullptr)
		return;
	DrawLoosePng(out, page->background, window.position);
	// A page with no title rect draws no title - the tall page since 2026-09-21, where the painting says whose
	// forge this is and the tab beside it says which of his doors you came through.
	if (page->title.size.width > 0) {
		DrawString(out, _("Salvage"), Rectangle { window.position + Displacement { page->title.position.x, page->title.position.y }, page->title.size },
		    { UiFlags::ColorGold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}
	int hoveredNow = -1;
	for (int i = 0; i < SalvageTierCount; i++) {
		const Rectangle rect = SalvageButtonRect(window, i);
		const bool hovered = rect.contains(MousePosition);
		if (hovered)
			hoveredNow = i;
		const Rectangle face { rect.position + (PressedSalvageIcon == i ? CubeButtonSink : Displacement { 0, 0 }), rect.size };
		if (GetLoosePngSize(SalvageIconAssets[i]).width > 0) {
			DrawLoosePng(out, SalvageIconAssets[i], face.position);
		} else {
			FillRect(out, face.position.x, face.position.y, face.size.width, face.size.height, SlotFillColor);
			DrawString(out, _(SalvageTierName(static_cast<SalvageTier>(i))), face,
			    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
		}
		if (hovered)
			BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, CubeHoverBrightenPercent);
		else if (MyPlayer != nullptr && !AnySalvageableInBackpack(*MyPlayer, static_cast<SalvageTier>(i)))
			DrawQuarterDarkenRect(out, face); // nothing of this tier in the pack: the plate sits under a shade
	}
	if (page->itemIcon.size.width > 0) {
		// The eighth plate: no tier, so no idle shade - it is about whatever the player points at next.
		const Rectangle rect { window.position + Displacement { page->itemIcon.position.x, page->itemIcon.position.y }, page->itemIcon.size };
		const bool hovered = rect.contains(MousePosition);
		if (hovered)
			hoveredNow = SalvageTierCount;
		const Rectangle face { rect.position + (PressedSalvageIcon == SalvageTierCount ? CubeButtonSink : Displacement { 0, 0 }), rect.size };
		if (GetLoosePngSize(SalvageItemIconAsset).width > 0)
			DrawLoosePng(out, SalvageItemIconAsset, face.position);
		if (hovered || SalvageItemCursorArmed)
			BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, CubeHoverBrightenPercent);
	}
	if (hoveredNow >= 0 && hoveredNow != LastHoverSalvageIcon)
		PlayUiMoveSound();
	LastHoverSalvageIcon = hoveredNow;

	const Rectangle results { window.position + Displacement { page->results.position.x, page->results.position.y }, page->results.size };
	if (page->goldFrame)
		DrawSalvageResultsFrame(out, results, _("Salvage Results"));
	// Griswold's tabs stay in view beside his Salvage page (user, 2026-09-21: "make sure when a user clicks on
	// Salvage tab the tabs column remains visible"), drawn from the shop's own column so the two cannot drift.
	if (page->docked)
		DrawShopTabColumnFor(out, TalkID::SmithTransmute);

	// The message (user, 2026-09-21): "X Rare Items destroyed", then the material's sprite in a 60x60 plate with a
	// 1 px outline OUTSIDE it in the tier's colour and "X Rare Fibres Salvaged" beside it - all in the tier's colour,
	// the block centred in the box both ways, replaced by the next press.
	// The question the dear tiers ask, in the results frame (user, 2026-09-21). It stands until one of the two
	// buttons is pressed AND released inside itself; a release anywhere else is "let me think a bit more".
	if (PendingConfirmTier >= 0) {
		const std::string question = fmt::format(fmt::runtime(_("Are you sure you want to destroy all {:s} items?")),
		    _(SalvageTierAdjectives[PendingConfirmTier]));
		const int lineHeight = GetLineHeight(question, GameFont12);
		const std::string wrapped = WordWrapString(question, results.size.width - 16, GameFont12);
		const int lines = static_cast<int>(std::count(wrapped.begin(), wrapped.end(), '\n')) + 1;
		const int buttonsTop = SalvageConfirmButtonRect(results, ConfirmButton).position.y;
		DrawString(out, wrapped, Rectangle { { results.position.x + 8, results.position.y + (buttonsTop - results.position.y - lines * lineHeight) / 2 },
		                             { results.size.width - 16, lines * lineHeight } },
		    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter });
		for (int which = ConfirmButton; which <= CancelButton; which++) {
			const Rectangle rect = SalvageConfirmButtonRect(results, which);
			const bool hovered = rect.contains(MousePosition);
			const Rectangle face { rect.position + (PressedConfirmButton == which ? CubeButtonSink : Displacement { 0, 0 }), rect.size };
			const uint32_t rgb = which == ConfirmButton ? ConfirmGreenRgb : CancelRedRgb;
			FillRect(out, face.position.x + 1, face.position.y + 1, face.size.width - 2, face.size.height - 2, PAL16_GRAY + 14);
			OutlineRectRgb(out, face, rgb, which == ConfirmButton ? PAL16_GRAY + 6 : PAL16_RED + 4);
			DrawString(out, which == ConfirmButton ? _("CONFIRM") : _("CANCEL"), face,
			    { (which == ConfirmButton ? UiFlags::ColorOracoolGreen : UiFlags::ColorRed) | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
			if (hovered)
				BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, CubeHoverBrightenPercent);
		}
	}
	if (PendingConfirmTier < 0 && SalvageMessage.shown) {
		const int t = static_cast<int>(SalvageMessage.tier);
		const UiFlags color = SalvageTierColors[t];
		const Rectangle &box = results;
		const std::string line1 = fmt::format(fmt::runtime(_("{:d} {:s} Items destroyed")), SalvageMessage.items, _(SalvageTierAdjectives[t]));
		const std::string line2 = fmt::format(fmt::runtime(_("{:d} {:s} Salvaged")), SalvageMessage.materials, _(AllItemsList[SalvageMaterialFor(SalvageMessage.tier)].iName));
		const int lineHeight = GetLineHeight(line1, GameFont12);
		const int frameOuter = SalvageFramePlate.height + 2; // the outline sits outside the 60x60 plate
		const int blockHeight = lineHeight + 6 + frameOuter;
		const int top = box.position.y + (box.size.height - blockHeight) / 2;
		DrawString(out, line1, Rectangle { { box.position.x, top }, { box.size.width, lineHeight } },
		    { color | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
		// Wrapped beside the plate (user, 2026-09-21: "word wrapp the word Salvaged from the Frame + Icon row to fit it
		// within the Salvage results frame"): "7 Unique Encrustments" / "Salvaged" when one line will not fit next to
		// the frame; the lines sit vertically centred on the plate and the row is centred by its widest line.
		const int textRoom = box.size.width - frameOuter - SalvageFrameGap;
		const std::string wrapped = WordWrapString(line2, textRoom, GameFont12);
		std::vector<std::string> rows;
		for (size_t start = 0; start <= wrapped.size();) {
			const size_t end = wrapped.find('\n', start);
			rows.push_back(wrapped.substr(start, end == std::string::npos ? std::string::npos : end - start));
			if (end == std::string::npos)
				break;
			start = end + 1;
		}
		int textWidth = 0;
		for (const std::string &row : rows)
			textWidth = std::max(textWidth, GetLineWidth(row, GameFont12));
		const int rowWidth = frameOuter + SalvageFrameGap + textWidth;
		const int rowLeft = box.position.x + (box.size.width - rowWidth) / 2;
		const int rowTop = top + lineHeight + 6;
		FillRectRgb(out, rowLeft, rowTop, frameOuter, frameOuter, SalvageTierRgb[t], PAL16_GRAY + 4); // the 1 px outline
		FillRect(out, rowLeft + 1, rowTop + 1, SalvageFramePlate.width, SalvageFramePlate.height, PAL16_GRAY + 14); // the plate
		if (GetLoosePngSize(SalvageMaterialSprites[t]).width > 0)
			DrawLoosePng(out, SalvageMaterialSprites[t], { rowLeft + 3, rowTop + 3 }); // 56 in 60: two pixels of plate around it
		const int textBlock = lineHeight * static_cast<int>(rows.size());
		int rowY = rowTop + (frameOuter - textBlock) / 2;
		for (const std::string &row : rows) {
			DrawString(out, row, Rectangle { { rowLeft + frameOuter + SalvageFrameGap, rowY }, { textWidth + 4, lineHeight } },
			    { color | UiFlags::FontSize12 | UiFlags::VerticalCenter });
			rowY += lineHeight;
		}
	}
	DrawWindowCloseButtonAt(out, CloseButtonRect(window));
}

} // namespace

void DrawLevskiRoar(const Surface &out)
{
	if (!WindowOpen)
		return;

	const Rectangle window = GetLevskiRoarRect();
	if (SalvageSkin()) {
		DrawSalvageWindow(out, window); // Griswold's painted Salvage UI (2026-09-21): no grid, no recipes
		return;
	}
	// The painted skin. Everything the old window drew itself - frame, title, grid well, plates and
	// labels - is in the painting; what is drawn here is STATE: items in the grid, a plate under the
	// cursor or mid-press, and a plate dimmed because pressing it would do nothing.
	const ListSkinGeometry *listSkin = ListSkin();
	const bool cube = listSkin != nullptr;
	const char *skin = cube ? listSkin->background : LevskiBackgroundAsset;
	if (GetLoosePngSize(skin).width == 0)
		if (cube && listSkin->docked && HasSidePanelArt())
			DrawSidePanelArt(out, window.position); // the workshop's placeholder canvas
		else
		DrawPanelGround(out, window); // the skin did not load: the flat ground, so the window still exists
	DrawLoosePng(out, skin, window.position);

	if (cube && listSkin->codeDrawn) {
		// The artisans' canvas: its interior is black, and everything in it is drawn here as a
		// placeholder the user will paint over - the title, the twelve wells, the list's bezel and
		// the button's plate. Colours from the border's own ramp so it reads as one object.
		const char *title = WindowHost == TransmuteHost::Cube ? "Levski's Cube" : WindowHost == TransmuteHost::Tavern ? "Ogden's Table" : "Gillian's Hearth";
		DrawString(out, _(title), Rectangle { { window.position.x + 22, window.position.y + 30 }, { 276, 26 } },
		    { UiFlags::ColorGold | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::Shadowed });
		const Point origin = GridOrigin(window);
		for (int cell = 0; cell < LevskiGridSlots; cell++) {
			const Rectangle well { { origin.x + (cell % LevskiGridColumns) * levski_skin::GridPitch, origin.y + (cell / LevskiGridColumns) * levski_skin::GridPitch },
				{ levski_skin::GridPitch, levski_skin::GridPitch } };
			FillRect(out, well.position.x, well.position.y, well.size.width, well.size.height, PanelFillColor);
			FillRect(out, well.position.x + 1, well.position.y + 1, well.size.width - 2, well.size.height - 2, SlotFillColor);
		}
		const Rectangle list { window.position + Displacement { listSkin->list.position.x - 3, listSkin->list.position.y - 3 }, { listSkin->list.size.width + 6, listSkin->list.size.height + 6 } };
		FillRect(out, list.position.x, list.position.y, list.size.width, list.size.height, PanelFillColor);
		FillRect(out, list.position.x + 1, list.position.y + 1, list.size.width - 2, list.size.height - 2, SlotFillColor);
		const Rectangle track { window.position + Displacement { listSkin->track.position.x, listSkin->track.position.y }, listSkin->track.size };
		FillRect(out, track.position.x, track.position.y, track.size.width, track.size.height, PanelFillColor);
		const Rectangle plate = TransmuteButtonRect(window);
		FillRect(out, plate.position.x - 1, plate.position.y - 1, plate.size.width + 2, plate.size.height + 2, PanelFillColor);
		FillRect(out, plate.position.x, plate.position.y, plate.size.width, plate.size.height, SlotFillColor);
	}

	// THE SLOT FACE in each of the twelve cells (user, 2026-09-22: "apply this texture to all 28x28px
	// inv/stash grids game-wide", then "if levski is 29x29 scale it to 29x29").
	//
	// IT IS NOT 29. Measured, and the skins say so themselves: levski_skin::GridPitch and CellSize
	// are both 28, and cube_skin's own note reads "brass rims at x 25-26 / 53-54, interiors of 26 px
	// on a 28 px pitch - the Roar's grid exactly". The 29 is a stale line in CellRect's comment,
	// left from a painting this window has not worn since 2026-09-05.
	//
	// THE FULL CELL, rims included (user, 2026-09-22: "i would like that"). It was inset a pixel to
	// leave the painted brass rims showing, which was my caution rather than a request - the user
	// looked at it and asked for the cover. CellRect is the 28 the item sprite occupies, so the slot
	// art and the item it holds now sit on exactly the same square.
	//
	// Outside the codeDrawn block above, so a painted page gets it too - but ASKED OF THE PAGE, not
	// assumed: the open tesseract's cells are a hologram and take no slot art at all.
	if (PageHasGrid() && (listSkin == nullptr || listSkin->slotArt)) {
		for (int cell = 0; cell < LevskiGridSlots; cell++)
			DrawSlotBackground(out, CellRect(window, cell));
	}

	const int hoveredAnchor = HoveredAnchor();
	for (int anchor = 0; anchor < LevskiGridSlots && PageHasGrid(); anchor++) {
		if (GridItems[anchor].isEmpty())
			continue;
		const Item &item = GridItems[anchor];
		const Size size = GetInventorySize(item);
		const Rectangle footprint {
			CellRect(window, anchor).position,
			{ size.width * CellSize, size.height * CellSize }
		};
		const ClxSprite sprite = GetInvItemSprite(item._iCurs + CURSOR_FIRSTITEM);
		// Centred in the footprint at ItemScale - the sprite is cut to 28px cells.
		const Point topLeft {
			footprint.position.x + (footprint.size.width - sprite.width() * ItemScale) / 2,
			footprint.position.y + (footprint.size.height - sprite.height() * ItemScale) / 2
		};
		// At any other scale, what DrawItem does at 1x done by hand: the grey for gear the character cannot use, the
		// red X for a broken item, the stack count in the corner. The socket overlay is NOT drawn -
		// its dots are placed for a 1x sprite - but the hover panel still names the gems, which is
		// what the user asked for when this grid learned to hover (2026-09-03).
		const bool usable = !IsInspectingPlayer() ? item._iStatFlag : InspectPlayer->CanUseItem(item);
		if constexpr (ItemScale == 1) {
			// Regular game size (user, 2026-09-04): the ordinary item draw, with everything it
			// carries - shadows, the grey, the red X, the stack count - and the socket overlay and
			// outline the backpack gives an item under the cursor. Nothing here is a copy of it.
			const Point bottomLeft { topLeft.x, topLeft.y + sprite.height() - 1 };
			// The backpack's backing under the item (2026-09-20: the tier tint, the stone, and the grid frame the
			// user asked for on every grid but the body) - the same call, so the Cube's grid reads as the pack does.
			InvDrawSlotBack(out, bottomLeft, footprint.size, item);
			if (anchor == hoveredAnchor)
				ClxDrawOutline(out, GetOutlineColor(item, true), bottomLeft, sprite);
			DrawItem(item, out, bottomLeft, sprite);
			if (anchor == hoveredAnchor)
				DrawSocketOverlay(out, item, bottomLeft, size);
			continue;
		}
		DrawSpriteScaled(out, topLeft, sprite, ItemScale, usable ? nullptr : GetInfravisionTRN());
		if (item._iOracoolBroken)
			DrawRedCross(out, footprint);
		if (item.isStackableConsumable() && item.stackCount() > 1)
			DrawBadge(out, footprint, BadgeCorner::BottomRight, StrCat(item.stackCount()));
		if (anchor == hoveredAnchor)
			DrawColoredOutline(out, footprint, GetOutlineColor(item, true));
	}

	// The close button: the game's own red X, where the skin puts it (the painting has no plate for
	// it, and its frame's corner is not the rect's corner - see the cutter).
	DrawWindowCloseButtonAt(out, CloseButtonRect(window));

	// The Cube's two tabs, in the vendors' own column (user, 2026-09-21). Drawn from the shop's
	// helper, not a copy of it: this window sits in exactly the shop panel's rect, so a column of
	// its own would be the same furniture in a slightly different place - which is the difference
	// the eye catches when flipping between a vendor and the Cube.
	if (CubeTabbedPages()) {
		int hoveredTab = -1;
		for (int i = 0; i < CubeTabCount; i++) {
			const auto tab = static_cast<CubeTab>(i);
			if (GetSideTabRect(i).contains(MousePosition))
				hoveredTab = i;
			DrawSideTab(out, i, _(i == 0 ? "Cube" : "Recipes"), OpenCubeTab == tab, PressedCubeTab == i);
		}
		if (hoveredTab >= 0 && hoveredTab != LastHoverCubeTab)
			PlayUiMoveSound(); // titlemov on entry, as every tab and button in the mod
		LastHoverCubeTab = hoveredTab;
	}

	if (cube && listSkin->recipes.size.width > 0) {
		// The user's painted Cube UI: the title in the game's font, the two painted buttons - brighter under the
		// cursor, sunk while pressed, the entry sound as the cursor arrives - and the tall book when it is open.
		DrawString(out, _("Levski's Cube"), Rectangle { window.position + Displacement { listSkin->title.position.x, listSkin->title.position.y }, listSkin->title.size },
		    { UiFlags::ColorGold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
		int hoveredNow = -1;
		for (const int b : { static_cast<int>(levski_skin::Transmute), static_cast<int>(levski_skin::Recipes) }) {
			const Rectangle rect = ButtonRect(window, b);
			const bool hovered = rect.contains(MousePosition);
			if (hovered)
				hoveredNow = b;
			const char *asset = b == levski_skin::Transmute ? CubeTransmuteAsset : CubeRecipeBookAsset;
			const Rectangle face { rect.position + (PressedCubeButton == b ? CubeButtonSink : Displacement { 0, 0 }), rect.size };
			if (GetLoosePngSize(asset).width > 0) {
				DrawLoosePng(out, asset, face.position);
			} else {
				DrawString(out, b == levski_skin::Transmute ? _("TRANSMUTE") : _("RECIPE BOOK"), face,
				    { UiFlags::ColorGold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
			}
			if (hovered)
				BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, CubeHoverBrightenPercent);
		}
		if (hoveredNow >= 0 && hoveredNow != LastHoverCubeButton)
			PlayUiMoveSound();
		LastHoverCubeButton = hoveredNow;
		if (RecipeBookOpen)
			DrawTallRecipeBook(out);
		return;
	}

	if (cube) {
		// The Cube's painting (batch 43b): TRANSMUTE is a recess under the grid, and the recipes are
		// listed in the bezel on the right - one name per line, eight lines, the wheel scrolls them.
		// The button art (batch 43c) goes in the recess when it lands; until then the game's own
		// gold label names it, so the recess is never a blank slot.
		const Rectangle button = TransmuteButtonRect(window);
		const bool hovered = button.contains(MousePosition);
		const bool pressed = ButtonFlashActive(ButtonFlashTransmute);
		const char *buttonArt = pressed ? cube_skin::TransmuteButtonPressedAsset : cube_skin::TransmuteButtonAsset;
		if (button.size.width == 0) {
			// The Recipes tab has no Transmute at all - it is the list, nothing else.
		} else if (listSkin != nullptr && listSkin->paintedTransmute) {
			// THE DIAMOND IS THE BUTTON (2026-09-22). Nothing is drawn on it: the painting already
			// has the thing being pressed, and Griswold's plate below would sit on top of it.
			//
			// It answers with LIGHT rather than with the mod's two-pixel sink, because a sink moves
			// a painted object off the art it is part of - the tesseract's ribs would show a
			// diamond-shaped hole beside it. Brighter under the cursor, brighter still while held.
			//
			// Checked BEFORE the tabbed-page branch below, which this page also satisfies: the
			// tesseract IS a tabbed page, and the plate is what it must not get.
			constexpr int CubeDiamondPressBrightenPercent = 140;
			if (pressed)
				BrightenRectRgb(out, button.position.x, button.position.y, button.size.width, button.size.height, CubeDiamondPressBrightenPercent);
			else if (hovered)
				BrightenRectRgb(out, button.position.x, button.position.y, button.size.width, button.size.height, CubeHoverBrightenPercent);
		} else if (CubeTabbedPages()) {
			// GRISWOLD'S OWN PLATE (user, 2026-09-21: "Use Griswold Refresh button as Transmute
			// button here"), frame and glyph, the same two files his shop draws. It sinks on the
			// press like every button in the mod and centres the glyph on the glyph's own size, so
			// a redrawn icon of another size stays centred.
			const Rectangle face { button.position + (pressed ? CubeButtonSink : Displacement { 0, 0 }), button.size };
			if (GetLoosePngSize(CubeTransmuteFrameAsset).width > 0)
				DrawLoosePng(out, CubeTransmuteFrameAsset, face.position);
			else
				DrawOrnateBorder(out, face);
			if (const Size glyph = GetLoosePngSize(CubeTransmuteGlyphAsset); glyph.width > 0) {
				DrawLoosePng(out, CubeTransmuteGlyphAsset,
				    { face.position.x + (face.size.width - glyph.width) / 2,
				        face.position.y + (face.size.height - glyph.height) / 2 });
			} else {
				DrawString(out, _("T"), face,
				    { UiFlags::ColorGold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
			}
			if (hovered)
				BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, CubeHoverBrightenPercent);
		} else if (!listSkin->codeDrawn && GetLoosePngSize(buttonArt).width > 0) {
			DrawLoosePng(out, buttonArt, button.position);
		} else {
			DrawString(out, _("TRANSMUTE"), button,
			    { (pressed ? UiFlags::ColorWhitegold : UiFlags::ColorGold) | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
		}
		if (hovered && !pressed && button.size.width > 0 && !CubeTabbedPages())
			DrawHoverOutline(out, button);

		// The dark layer the user asked for under the recipes (2026-09-21: "Lay a transparent dark
		// layer under the recipes"), inside the painted frame's opening and nowhere else - the frame
		// itself stays as painted, which is the same rule every canvas this day follows.
		//
		// Half-transparent rather than a flat colour, so it darkens whatever the painting puts behind
		// it and survives a recut of the art. Twice: the floor under this frame is busy stone, and one
		// pass left the recipe names competing with it.
		if (const Rectangle &dark = listSkin->darkLayer; dark.size.width > 0) {
			DrawThemedFill(out, Rectangle { window.position + Displacement { dark.position.x, dark.position.y }, dark.size }, 2);
		}

		// THE SHARED LIST (2026-09-22): this page, Ogden's and Gillian's all draw through one
		// function, so "behave identically" is a property of the code rather than of three loops kept
		// in step. Names in gold, explanations in white and wrapped, scrolled by the wheel.
		//
		// The GRID page has no list at all - it drew nothing before either, but only because the loop
		// ran against an empty rect and zero-width rows happen to be invisible.
		if (const Rectangle opening = CubeRecipeOpening(); opening.size.width > 0)
			DrawRecipeList(out, opening, CubeListRecipes(), CubeListScroll, SelectedRecipe);
		return; // no salvage block, no recipe-book plate, no tall book: the page IS the book
	}

	// The SALVAGE title over the block (user, 2026-09-05: "Gold, with text shadow. Appropriate font
	// size"): 24px, the window title's own gold, and the same shadow the hero sheet's text wears.
	// Skipped when the skin gives it no room: the 2026-09-05 painting carries SALVAGE on its own
	// stone plate, and a second SALVAGE drawn over it would be the one thing worse than none.
	if (const Rectangle &t = levski_skin::SalvageTitleRect; t.size.height > 0) {
		DrawString(out, _("SALVAGE"), Rectangle { window.position + Displacement { t.position.x, t.position.y }, t.size },
		    { UiFlags::ColorGold | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}

	// The nine controls (2026-09-05, "3 rows of 3 icons"): GPT's 32px icon plates, three states each.
	// The painting carries NO plates for them, so the DEFAULT frame goes down at rest and the hover
	// or pressed frame replaces it while the cursor is on it or the press flash is running. Where a
	// hover file is the plain plate (HoverIsPlain), the hover is marked with the theme's outline.
	for (int i = levski_skin::Close + 1; i < levski_skin::ButtonCount; i++) {
		// Salvage is Griswold's (roadmap, 2026-09-20): the seven tier plates are drawn - and work -
		// only on his Forge. Ogden's and Gillian's books wear the same painting, whose carved cells
		// are empty, so leaving them undrawn leaves eight empty cells and no dead buttons.
		if (i >= levski_skin::SalvageFirst && WindowHost != TransmuteHost::Smith)
			continue;
		const Rectangle rect = ButtonRect(window, i);
		const bool hovered = rect.contains(MousePosition);
		const int flash = FlashIndexForButton(i);
		const bool pressed = flash >= 0 && ButtonFlashActive(flash);
		// TRANSMUTE is engraved in the painting (user, 2026-09-08: "it is integrated in the asset
		// in idle and hover state. click to blink once"): nothing at rest, the gold-lit crop of the
		// hover painting under the cursor, and a click shows the OTHER state for the flash - lit
		// when it was not, dark when it was - which is one blink either way.
		if (i == levski_skin::Transmute) {
			if (hovered != pressed)
				DrawLoosePng(out, "ui\\levski_transmute_lit.png", window.position + Displacement { levski_skin::TransmuteLitOrigin.x, levski_skin::TransmuteLitOrigin.y });
			continue;
		}
		// The eight carved cells are EMPTY in the painting, so the icon is drawn in every state:
		// the bright frame at rest, the pushed frame while the press flash runs, and the theme's
		// outline to mark the cell under the cursor.
		const std::string state = StrCat("ui\\levski_", levski_skin::ButtonStems[i], pressed ? "_pressed.png" : "_hover.png");
		DrawLoosePng(out, state.c_str(), rect.position);
		if (hovered && !pressed)
			DrawHoverOutline(out, rect);
		if (pressed || hovered)
			continue;
		// The readout the old gold-vs-whitegold label carried: a plate that would do nothing right
		// now sits under a shade, so the column still says what is worth pressing.
		// TRANSMUTE gets NO idle shade, and the line that tried to give it one was unreachable:
		// the `continue` above returns for Transmute long before this, so `idle = ready < 0` never
		// ran and FirstReadyLevskiRecipe was being called once a frame for a value nothing read
		// (found by the 2026-09-12 asset sweep; the call is gone with it).
		//
		// Left unshaded deliberately rather than moved above the continue. The eight salvage cells
		// are EMPTY in the painting, so their shade darkens a plate this code drew. TRANSMUTE is
		// engraved into the painting itself, so a shade there would darken the artwork - a different
		// effect on a different thing, and not one anybody has asked to see. If it is ever wanted it
		// needs designing against the engraving, not this one line.
		bool idle = false;
		if (i >= levski_skin::SalvageFirst)
			idle = !AnySalvageableInBackpack(*MyPlayer, static_cast<SalvageTier>(i - levski_skin::SalvageFirst));
		if (idle)
			DrawQuarterDarkenRect(out, rect); // a quarter, not a half, since 2026-09-07 - see the helper
	}

	if (RecipeBookOpen)
		DrawTallRecipeBook(out);
}

bool CheckLevskiRoarClick(Point mousePosition, bool isCtrlHeld)
{
	if (!WindowOpen)
		return false;

	// The tab column beside the Salvage page: a click on another of Griswold's tabs closes this page and opens that
	// shelf, and a click on Salvage itself is absorbed (2026-09-21). Tested before the window, since the column is
	// outside it.
	if (const SalvageLayout *page = SalvagePage(); page != nullptr && page->docked) {
		// The press only sinks the tab; the shelf opens on the release, in ReleaseLevskiButtons below
		// (user, 2026-09-21: "opening clicked tab counts if release happens within region of button").
		if (PressShopTabAt(mousePosition, TalkID::SmithTransmute))
			return true;
	}
	// The Cube's own two tabs, in the same column and by the same rule: the press only sinks the tab
	// and sounds, and the page turns on the release inside it (ReleaseLevskiButtons below). Tested
	// before the window, since the column sits OUTSIDE it - a test against the window's rect alone
	// would let every tab click fall through to the world.
	if (CubeTabbedPages()) {
		for (int i = 0; i < CubeTabCount; i++) {
			if (!GetSideTabRect(i).contains(mousePosition))
				continue;
			PressedCubeTab = i;
			PlayUiMoveSound();
			return true;
		}
	}
	const Rectangle window = GetLevskiRoarRect();
	const Rectangle book = GetLevskiRecipeBookRect();
	const Rectangle bookInner = RecipeBookInner(book); // the rows and clips are laid out from the frame's core
	const bool inWindow = window.contains(mousePosition);
	const bool inBook = RecipeBookOpen && book.contains(mousePosition);
	if (!inWindow && !inBook)
		return false; // outside both panels: the click belongs to whatever is under it

	if (inBook) {
		// The book's own X closes the book, not the window under it - each window owns its button.
		if (CheckWindowCloseButtonClick(book, mousePosition)) {
			RecipeBookOpen = false;
			return true;
		}
		// The book is a control surface now (v1.9.18): clicking a recipe SELECTS it, and clicking
		// the selected one again clears the selection. It stopped being a pure reference the moment
		// two recipes could take the same target and the same material at different costs, because
		// then no auto-pick can be the one the player meant.
		//
		// Walked through the same RecipeBookRows the draw used, so a click lands on the row that
		// was actually under the pointer even though the rows are not a fixed height.
		const std::vector<RecipeRow> rows = RecipeBookRows(book);
		const int clipTop = bookInner.position.y + Padding + HeaderHeight;
		const int clipBottom = bookInner.position.y + bookInner.size.height - Padding;
		for (int i = 0; i < CraftingRecipeCount; i++) {
			const RecipeRow &row = rows[i];
			if (row.top < clipTop || row.top + row.height > clipBottom)
				continue; // not drawn, so not clickable - the invisible-cell rule from the skill picker
			if (row.height == 0 || mousePosition.y < row.top || mousePosition.y >= row.top + row.height)
				continue;
			SelectedRecipe = (SelectedRecipe == i) ? -1 : i;
			PlayUiSelectSound();
			return true;
		}
		return true;
	}

	// Before every other control: the X is the one click that must always work, and this window
	// absorbs everything else that lands on it.
	if (CloseButtonRect(window).contains(mousePosition)) {
		// Closes on the release inside it (ReleaseLevskiButtons). Its own hit test (the skin places this X).
		PressedCloseButton = true;
		return true;
	}

	// The Cube skin's bezel: a click on a listed recipe selects it, on the selected one clears the
	// selection - the tall book's rule, on the painting's own lines.
	if (CubeSkin() && !PaintedButtons()) {
		// Asked of the shared list, like the hover above: a recipe is a wrapped BLOCK now, and
		// clicking anywhere in its paragraph picks it rather than only its title.
		if (const Rectangle opening = CubeRecipeOpening(); opening.size.width > 0) {
			const int recipe = RecipeListHitTest(opening, CubeListRecipes(), CubeListScroll, mousePosition);
			if (recipe >= 0) {
				SelectedRecipe = (SelectedRecipe == recipe) ? -1 : recipe;
				PlayUiSelectSound();
				return true;
			}
		}
	}

	// Salvage. Reports what it did, always - a button that silently does nothing because you own no
	// rares is indistinguishable from a button that is broken, and this fork has shipped that exact
	// ambiguity twice. Griswold's Forge only (the plates are not drawn on the other books).
	// The eighth plate arms the hammer; the next click on a backpack item breaks that item down (2026-09-21).
	if (const SalvageLayout *page = SalvagePage(); page != nullptr && page->itemIcon.size.width > 0) {
		const Rectangle rect { window.position + Displacement { page->itemIcon.position.x, page->itemIcon.position.y }, page->itemIcon.size };
		if (rect.contains(mousePosition)) {
			PressedSalvageIcon = SalvageTierCount; // arms the hammer on the release inside it (ReleaseLevskiButtons)
			PlayUiMoveSound();
			return true;
		}
	}
	// The standing question owns its two buttons: the press only SINKS them, and ReleaseLevskiButtons decides,
	// because the action and the dismissal both wait for a release inside the button that was pressed.
	if (const SalvageLayout *page = SalvagePage(); page != nullptr && PendingConfirmTier >= 0) {
		const Rectangle results { window.position + Displacement { page->results.position.x, page->results.position.y }, page->results.size };
		for (int which = ConfirmButton; which <= CancelButton; which++) {
			if (!SalvageConfirmButtonRect(results, which).contains(mousePosition))
				continue;
			PressedConfirmButton = which;
			PlayUiMoveSound();
			return true;
		}
	}
	for (int i = 0; WindowHost == TransmuteHost::Smith && i < SalvageTierCount; i++) {
		if (!SalvageButtonRect(window, i).contains(mousePosition))
			continue;
		PressedSalvageIcon = i; // sinks until LeftMouseUp, and acts on the release inside it (ReleaseLevskiButtons)
		if (SalvageSkin())
			PlayUiMoveSound();
		return true;
	}

	if (SalvageSkin())
		return true; // the Salvage window has no grid, no Transmute and no book: everything else on it is stone

	if (RecipeButtonRect(window).contains(mousePosition)) {
		PressedCubeButton = levski_skin::Recipes; // sinks until LeftMouseUp; the book turns on the release (ReleaseLevskiButtons)
		FlashButton(ButtonFlashRecipes);
		PlayUiMoveSound();
		return true;
	}

	if (TransmuteButtonRect(window).contains(mousePosition)) {
		FlashButton(ButtonFlashTransmute);
		PressedTransmute = true; // runs on the release inside the button (ReleaseLevskiButtons)
		if (PaintedButtons()) {
			PressedCubeButton = levski_skin::Transmute; // sinks until LeftMouseUp; the click sounds at the press
			PlayUiMoveSound();
		}
		return true;
	}

	// The grid itself: an empty hand takes an item out, a full one puts it in. Swapping is
	// deliberately absent - a click that both takes and gives is how a stone goes missing.
	Player &player = *MyPlayer;
	const int cell = CellAt(window, mousePosition);
	if (cell >= 0) {
		// CTRL sends it straight back to the backpack instead of onto the cursor (user, 2026-08-28:
		// "ctrl+click to send items to levskis grid and back to my inv grid, not drop them on the
		// ground"). The same gesture the stash uses, in the same direction: ctrl means "move it to
		// the other container", never "pick it up".
		if (isCtrlHeld && player.HoldItem.isEmpty() && GridCells[cell] != 0) {
			const int anchor = GridCells[cell] - 1;
			if (!AutoPlaceItemInInventory(player, GridItems[anchor], true)) {
				LogEvent(std::string(_("Your pack is full.")), UiFlags::ColorRed);
				return true;
			}
			PlaySFX(ItemInvSnds[GetItemDropAnimIndex(GridItems[anchor]._iCurs)]);
			MarkCells(anchor, GetInventorySize(GridItems[anchor]), 0);
			GridItems[anchor].clear();
			return true;
		}
		if (!player.HoldItem.isEmpty()) {
			// The item lands where it is DRAWN under the cursor - its centre on the clicked cell,
			// as in the backpack (TargetAnchorUnderItemCursor). If that footprint is over something,
			// PlaceInGrid finds the first cell it does fit.
			const int anchor = TargetAnchorUnderItemCursor(window, mousePosition, GetInventorySize(player.HoldItem));
			if (PlaceInGrid(player.HoldItem, anchor)) {
				// The backpack's put-down sound, as the Ctrl+click path above plays it.
				PlaySFX(ItemInvSnds[GetItemDropAnimIndex(player.HoldItem._iCurs)]);
				player.HoldItem.clear();
				NewCursor(CURSOR_HAND);
			}
		} else if (GridCells[cell] != 0) {
			// Any covered cell lifts the item, not just its anchor - clicking the bottom half of a
			// breastplate has to work, or half of every large item is dead surface.
			const int anchor = GridCells[cell] - 1;
			player.HoldItem = GridItems[anchor];
			MarkCells(anchor, GetInventorySize(GridItems[anchor]), 0);
			GridItems[anchor].clear();
			NewCursor(player.HoldItem._iCurs + CURSOR_FIRSTITEM);
			PlaySFX(IS_IGRAB); // the backpack's pick-up sound (inv.cpp)
		}
		return true;
	}

	return true; // padding and header clicks are absorbed, never passed through to the world
}

} // namespace devilution::oracool
