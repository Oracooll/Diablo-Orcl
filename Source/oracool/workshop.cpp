#include "oracool/workshop.h"

#include <SDL.h>
#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "diablo.h" // CloseOtherShopSurfaces - one shop surface at a time
#include "cursor.h"
#include "engine/palette.h"
#include "engine/random.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h"
#include "items.h"
#include "oracool/event_log.h"
#include "oracool/gems.h"
#include "oracool/hud_art.h"
#include "oracool/imbuement.h"
#include "oracool/levski_roar.h"
#include "oracool/crafting.h"
#include "oracool/ornate_border.h"
#include "oracool/recipe_list.h"
#include "oracool/runewords.h"
#include "oracool/shop_grid.h"
#include "oracool/ui_sound.h"
#include "oracool/window_close.h"
#include "player.h"
#include "qol/stash.h"
#include "effects.h"
#include "oracool/skill_sounds.h"
#include "stores.h"
#include "towners.h" // GetTowner - the artisan the window belongs to, for the walk-away
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

constexpr Size PageSize { 340, 720 };
/** Gillian's painted canvas (user, 2026-09-21); the shared side panel stands in until it is in the archive. */
constexpr const char *MysticCanvasAsset = "ui\\mystic_workshop.png";
/**
 * Ogden's own table (user, 2026-09-21), with the collection's frame painted into it. The shared
 * workshop canvas stays as the fallback, so a build short of the file still draws a whole window.
 */
constexpr const char *OgdenCanvasAsset = "ui\\ogden_canvas.png";
constexpr const char *JewellerCanvasAsset = "ui\\artisan_workshop.png";

/**
 * THE COLLECTION BOARD (user, 2026-09-21: "Make an invisible 30x30, 7x5 grid and fill it with all
 * types of Gems the same order as you use for the stash Sort function").
 *
 * MEASURED on his canvas: the painted frame's rules stand at x 61..63 and 276..278, its ornate bands
 * at y 429..438 and 591..600, so the opening is x 64..275 by y 439..590 - 212 by 152. A 7x5 board of
 * 30px cells is 210 by 150, which centres inside it with a pixel to spare on every side.
 *
 * INVISIBLE, as asked: no rules and no wells are drawn. The frame is the painting's, and what sits
 * inside it is icons on their own.
 *
 * Seven columns by five rows is not an arbitrary fit - it is the stash's own gem layout. SortStash
 * places a gem at `{ type, GemTopRow + quality }`: column by GemType (Amethyst, Diamond, Emerald,
 * Ruby, Sapphire, Topaz, Skull), row by GemQuality (Chipped up to Perfect). Seven types, five
 * qualities, thirty-five cells, thirty-five gems. The runes take the same board by ladder position,
 * which is how the stash orders them too, and 33 of them leave the last two cells empty.
 */
/**
 * The shared canvas's opening, as every 340x720 window uses it.
 *
 * Declared HERE, above the board geometry, because the collection title spans it - and it used to be
 * declared two hundred lines further down, which is a compile error the moment anything up here
 * reads it.
 */
constexpr int InnerLeft = 22;
constexpr int InnerRight = 317;

constexpr int BoardOpeningLeft = 64;
constexpr int BoardOpeningTop = 439;
constexpr int BoardOpeningWidth = 212;
constexpr int BoardOpeningHeight = 152;

/**
 * @brief One tab's board: how many cells, and how big, inside that same opening.
 *
 * PER TAB since 2026-09-22 (user: "Use different invisible grid in the Jewels Tab. In that tab we
 * need 5x3 grid, not 7x5 [...] filling it completely"). The gems and runes need thirty-five cells
 * and get 30px ones; the jewels need fifteen and can spend the whole opening on them, so their cells
 * are 42x50 - the opening divided by five and by three, to the pixel.
 */
struct BoardShape {
	int columns;
	int rows;
	int cellWidth;
	int cellHeight;
};
constexpr BoardShape GemBoard { 7, 5, 30, 30 };
constexpr BoardShape JewelBoard { 5, 3, BoardOpeningWidth / 5, BoardOpeningHeight / 3 };
static_assert(GemBoard.columns * GemBoard.rows >= 35, "the board no longer holds every gem");
static_assert(JewelBoard.columns * JewelBoard.rows >= 15, "the board no longer holds every jewel");

// ShapeFor is defined below, with the Tab enum it switches on.

/** @brief The board's top-left for @p shape, centred in the frame's opening. */
Point BoardOriginFor(const BoardShape &shape)
{
	return { BoardOpeningLeft + (BoardOpeningWidth - shape.columns * shape.cellWidth) / 2,
		BoardOpeningTop + (BoardOpeningHeight - shape.rows * shape.cellHeight) / 2 };
}

/** @brief The widest board's left edge - what the row above is aligned to, so it cannot move per tab. */
constexpr Point BoardOrigin {
	BoardOpeningLeft + (BoardOpeningWidth - GemBoard.columns * GemBoard.cellWidth) / 2,
	BoardOpeningTop + (BoardOpeningHeight - GemBoard.rows * GemBoard.cellHeight) / 2
};
constexpr int BoardColumns = GemBoard.columns;
constexpr int BoardCellPx = GemBoard.cellWidth;
constexpr int BoardMaxSlots = 35;

/**
 * The row above the frame: the collection's name between two arrow plates, four pixels clear of the
 * painted band at y 429 (the clearance every window this day uses).
 *
 * The plates are GRISWOLD'S - his 34x34 button frame with his sell arrow turned a quarter turn, up
 * for the upgrade and down for the downgrade (user: "Flush left put the Sell Icon from Griswold
 * pointing down [...] Flush with right grid border put Sell Icon from Griswold pointing Up"). Flush
 * means flush with the BOARD's edges, not the frame's, so they line up with the icons beneath them.
 */
constexpr int BoardButtonSize = 34;
constexpr int BoardRowClearance = 4;
constexpr int BoardButtonTop = 429 - BoardRowClearance - BoardButtonSize;
constexpr Rectangle BoardDowngradeRect { { BoardOrigin.x, BoardButtonTop }, { BoardButtonSize, BoardButtonSize } };
constexpr Rectangle BoardUpgradeRect {
	{ BoardOrigin.x + BoardColumns * BoardCellPx - BoardButtonSize, BoardButtonTop },
	{ BoardButtonSize, BoardButtonSize }
};
/**
 * FORTY pixels above the arrow row (user, 2026-09-22: "Move Gem Collection and Rune Collection
 * titles 40px upwards"). Only the title moves; the two plates keep their four-pixel clearance over
 * the painted band, so the name now stands clear of them on the open stone above.
 */
// SEVENTY: forty on 2026-09-22, then thirty more the same day ("move titles 30px upwards").
constexpr int BoardTitleLift = 70;
/**
 * The title spans the canvas's whole opening, not the gap between the two arrow plates.
 *
 * It was that gap - about 108px - and "Gem Collection" at FontSize24 does not fit in it, so the name
 * was cut off at both ends (user, 2026-09-22: "Titles are not fully visible. increase their text box
 * to fit to width of canvas, without overlaping the canvas frame"). InnerLeft/InnerRight are the
 * painted frame's own inner edges, so the box is as wide as the canvas allows and no wider.
 *
 * It can span the full width safely only because the title now sits seventy pixels above the plates
 * rather than on their line.
 */
constexpr Rectangle BoardTitleRect {
	{ InnerLeft, BoardButtonTop - BoardTitleLift },
	{ InnerRight - InnerLeft + 1, BoardButtonSize }
};
/**
 * The question under the grid, and its two answers (user, 2026-09-21: "Runes upgrades and downgrade
 * to ask for confirmation, just like the salvaging. Put confirmation message and buttons under the
 * grid").
 *
 * Everything here stays LEFT OF x 175, which is where the health orb's own rect begins on a 960-wide
 * screen. Griswold's Refresh-until plate ends at 154 and the stash's gold at 165 for the same
 * reason: below OrbClearanceBottom the window shares the screen with the orb, and a button drawn
 * under a sphere is a button the player cannot press.
 */
constexpr Rectangle BoardMessageRect { { 22, 602 }, { 295, 20 } };

/**
 * GRISWOLD'S GOLD (user, 2026-09-22: "Remove the current gold counter from all tabs and replace with
 * Griswold style gold counter") - the pile with the number beneath it and no "Gold:" label, because
 * the pile says what the number is. His own file, at his own left edge.
 *
 * At the FOOT of the window rather than his y, because this window's own frame runs to y 600 and his
 * grid ends at 618: the two windows put the pair under their grid, and that is a different number in
 * each. Left of x 175 like everything else down here - see the orb note below.
 */
constexpr const char *BoardGoldIconAsset = "ui\\shop_gold_icon.png";
constexpr Point BoardGoldIconAt { 27, 626 };
constexpr int BoardGoldIconHeight = 28;
// EIGHTY wide, not Griswold's 140: his number has the whole width under his grid, and this one
// shares its line with the confirmation at x 110. Eight digits at FontSize12 come to about seventy.
constexpr Rectangle BoardGoldCountRect { { 25, BoardGoldIconAt.y + BoardGoldIconHeight - 1 }, { 80, 16 } };

/**
 * The two answers sit BESIDE the gold and stacked, not in a row under it.
 *
 * Everything here stays LEFT OF x 175, which is where the health orb's own rect begins on a 960-wide
 * screen. Griswold's Refresh-until plate ends at 154 and the stash's gold at 165 for the same
 * reason: below OrbClearanceBottom the window shares the screen with the orb, and a button drawn
 * under a sphere is a button the player cannot press. That leaves 110..172 for the pair, which is
 * one button wide - so they stack rather than sitting side by side.
 */
constexpr Size BoardConfirmSize { 62, 24 };
constexpr int BoardConfirmLeft = 110;
constexpr int BoardConfirmTop = 626;
constexpr int BoardConfirmGap = 4;
constexpr Rectangle BoardConfirmRect { { BoardConfirmLeft, BoardConfirmTop }, BoardConfirmSize };
constexpr Rectangle BoardCancelRect {
	{ BoardConfirmLeft, BoardConfirmTop + BoardConfirmSize.height + BoardConfirmGap }, BoardConfirmSize
};
static_assert(BoardCancelRect.position.x + BoardCancelRect.size.width < 175,
    "the confirmation runs under the health orb - keep it left of the orb's rect");
static_assert(BoardGoldCountRect.position.x + BoardGoldCountRect.size.width <= BoardConfirmLeft,
    "the gold readout runs into the confirmation beside it");

/**
 * OGDEN'S CRAFT BENCH and his RECIPE PAGE (user, 2026-09-22, items 6 and 7).
 *
 * Both canvases were measured when they arrived on 2026-09-21 and both are reused here exactly:
 *  - the cube page's grid frame runs x 118..224, y 406..541, with rules at x 156/185/214 and y
 *    444/473/502 - the Roar's own 28px cell on a 29px pitch, so the grid is 3x4 at (128,416);
 *  - the recipe page's frame opens at x 29..310, y 299..618.
 *
 * The Transmute plate is GRISWOLD'S Refresh frame and glyph at the grid frame's centre, four pixels
 * below its foot - the same plate and the same derivation Levski's Cube uses, because the user asked
 * for that button by name on both.
 */
constexpr const char *OgdenCubeCanvasAsset = "ui\\ogden_cube_canvas.png";
constexpr const char *OgdenRecipesCanvasAsset = "ui\\ogden_recipes_canvas.png";
/**
 * GILLIAN'S PAGES (user, 2026-09-22).
 *
 * MEASURED, and her recipe frame is Ogden's to the pixel: the band runs y 289..298 and 619..628, so
 * the opening is x 29..310, y 299..618 - the same numbers, which is why her list needs no geometry
 * of its own.
 *
 * THREE PAGES, THREE PAINTINGS (user, 2026-09-22: "use Gillian Single Item Frame for tabs 1 and 2 -
 * where we need to place only one item", "use Gillian Multy-Item Frame for tab 3 where we need
 * multiple items in one grid").
 *
 * All three are the same room. They differ ONLY in what stands in the small frame at the top right:
 * nothing on the single-item file, a painted 3x4 well on the multi-item one, and the recipe file has
 * no small frame at all. So her Reroll and Imbue tabs, which hold one item whatever its footprint,
 * wear the empty frame; her Craft tab, which needs a grid, wears the well; and the big frame beneath
 * carries the list and the message board on every one of them.
 *
 * The small frame MEASURED: borders x 207..216 and 304..313, y 145..154 and 270..280, so the opening
 * is x 217..303, y 155..269 - 87 by 115.
 *
 * The WELL measured on the multi-item file: bright rules at x 245 and 274, y 183, 212 and 241, which
 * is a 3x4 of 28px cells on a 29px pitch starting at (217,155) - the Roar's own cell, exactly as
 * Ogden's well is, just moved up into her frame. Her craft grid is therefore his code with a
 * different origin rather than a second grid.
 */
constexpr const char *GillianRecipesCanvasAsset = "ui\\gillian_recipes_canvas.png";
/** The empty small frame: ONE item, any footprint (Reroll and Imbue). */
constexpr const char *GillianFrameCanvasAsset = "ui\\gillian_frame_canvas.png";
/** The same room with a 3x4 well painted into that frame (Craft). */
constexpr const char *GillianCraftCanvasAsset = "ui\\gillian_craft_canvas.png";

/** @brief The small frame's opening - her one-item bench, and the box her craft well sits in. */
constexpr Rectangle MysticFrameRect { { 217, 155 }, { 87, 115 } };
/** @brief The small frame's painted band, outer edges - borders x 207..216 / 304..313, y 145..154 / 270..280. */
constexpr int MysticFrameBandLeft = 207;
constexpr int MysticFrameBandBottom = 280;

constexpr int CraftColumns = 3;
constexpr int CraftRows = 4;
constexpr int CraftSlots = CraftColumns * CraftRows;
constexpr int CraftPitch = 29;
constexpr int CraftCellPx = 28;
/** Ogden's well, painted low on his cube canvas; hers is the small frame - see CraftGridOriginFor. */
constexpr Point CraftGridOrigin { 128, 416 };
constexpr Point MysticCraftGridOrigin { MysticFrameRect.position.x, MysticFrameRect.position.y };
static_assert(CraftColumns * CraftPitch - 1 <= MysticFrameRect.size.width
        && CraftRows * CraftPitch - 1 <= MysticFrameRect.size.height,
    "her craft grid no longer fits the frame her canvas paints it in");
constexpr int CraftFrameLeft = 118;
constexpr int CraftFrameRight = 224;
constexpr int CraftFrameBottom = 541;
constexpr int CraftPlateSize = 34;
constexpr Rectangle CraftTransmuteRect {
	{ (CraftFrameLeft + CraftFrameRight + 1) / 2 - CraftPlateSize / 2, CraftFrameBottom + 1 + 4 },
	{ CraftPlateSize, CraftPlateSize }
};
// The Cube's own transmute icon, shared with it (2026-09-22). Griswold's Refresh glyph stood in
// while the plate was new and said "reroll" on a button that transmutes.
constexpr const char *CraftTransmuteGlyphAsset = "ui\\shop_glyph_transmute.png";

constexpr int RecipeOpeningLeft = 29;
constexpr int RecipeOpeningTop = 299;
constexpr int RecipeOpeningRight = 310;
constexpr int RecipeOpeningBottom = 618;

constexpr const char *BoardButtonFrameAsset = "ui\\shop_button_frame.png";
constexpr const char *BoardUpGlyphAsset = "ui\\shop_glyph_arrow_up.png";
constexpr const char *BoardDownGlyphAsset = "ui\\shop_glyph_arrow_down.png";

/**
 * GILLIAN'S SERVICES AS ICON PLATES (user, 2026-09-22), on Griswold's 34x34 frame like every other
 * service button in the game. The icons are the user's own assignment:
 *
 *   Reroll  - his Refresh glyph        Imbue   - his Recharge glyph
 *   Cleanse - his Repair glyph         Remove  - his Sell arrow, turned to point RIGHT
 *
 * THE PRICE IS DRAWN, not hidden in a hover hint ("Prices to be visible, not hover text"). That was
 * the one thing the wide word-buttons did better - REROLL carried its cost on its face - and it is
 * the reason this conversion waited for a decision rather than being inferred from the other
 * vendors, none of whom charge for anything. So each plate gets a line beneath it, and the row is
 * laid out with room for that line rather than packed as tightly as the plates alone would allow.
 */
constexpr Size ServiceIconSize { 34, 34 };
constexpr int ServiceIconGap = 28;
constexpr int ServicePriceHeight = 14;
constexpr int ServicePriceGap = 2;
// A DIE (2026-09-22), not Griswold's restock arrows. She borrowed his because his was the nearest
// thing on the shelf, and the two services are not the same: his refreshes a shop's stock, hers
// gambles one affix on this item. A die says the second and the arrows say the first.
constexpr const char *RerollGlyphAsset = "ui\\shop_glyph_reroll.png";
// HER OWN FILE since 2026-09-22, holding the star her Imbue has always worn. It was Griswold's
// Recharge glyph, shared with him at the user's word; his became a fuel pump that day ("use the
// pump instead of the star for recharge"), and a fuel pump is not what working a shard into an item
// looks like. The picture on her button did not change - only which file it comes out of.
constexpr const char *ImbueGlyphAsset = "ui\\shop_glyph_imbue.png";
constexpr const char *RemoveGlyphAsset = "ui\\shop_glyph_arrow_right.png";
constexpr const char *CleanseGlyphAsset = "ui\\shop_glyph_repair.png";
constexpr Rectangle TitleRect { { 22, 26 }, { 296, 40 } };
constexpr Rectangle CloseRect { { 316, 5 }, { 18, 18 } };

/**
 * THE ICON ROW SITS BESIDE THE SMALL FRAME (user, 2026-09-22: "move the icons of tabs 1 2 3 of
 * gillian flush with lower border of smaller frame, 6px away from smaller frame, distributed
 * parallel to the top bezel of the bigger frame").
 *
 * Three instructions, one place:
 *  - FLUSH WITH THE LOWER BORDER: the plates' feet sit on y 280, the frame's outer lower edge;
 *  - 6PX AWAY: the rightmost plate ends six pixels clear of x 207, the frame's outer left edge;
 *  - PARALLEL TO THE BIG FRAME'S TOP BEZEL: a horizontal row, spread across the floor left of the
 *    frame rather than packed against it.
 *
 * There is no room UNDER the small frame - its foot is at 280 and the big frame's bezel begins at
 * 289, eight pixels - so beside it is the only reading of all three at once, and it is the reading
 * that puts the row on the same line as the thing it acts on.
 *
 * THE PRICE GOES ABOVE THE PLATE HERE, which it does nowhere else. Below would put it at y 282..295
 * and the big frame's painted bezel runs 289..298: a number half over the moulding is the one thing
 * every canvas this week has been laid out to avoid.
 */
constexpr int MysticIconRowClearance = 6;
/** The rightmost pixel the row may use: six clear of the small frame's left border. */
constexpr int MysticIconRowRight = MysticFrameBandLeft - MysticIconRowClearance - 1;
constexpr int MysticIconRowTop = MysticFrameBandBottom - ServiceIconSize.height + 1;
constexpr int MysticPriceRowTop = MysticIconRowTop - ServicePriceGap - ServicePriceHeight;
static_assert(MysticFrameBandBottom < 289, "the icon row has walked into the big frame's top bezel");
static_assert(MysticIconRowRight - InnerLeft + 1 >= 3 * ServiceIconSize.width,
    "the floor beside the frame no longer holds her three-plate row");

/**
 * THE BIG FRAME carries the list and the message board on every one of her pages.
 *
 * Her list was at (104,84) and her board at (30,336), laid out for a window that drew its own walls.
 * Both now sit inside the painted opening - x 29..310, y 299..618, the same frame her recipes use -
 * with the list at the top of it and the board beneath, on one dark layer.
 */
constexpr int ListLineHeight = 20;
constexpr int ListLines = 6;
constexpr Rectangle ListRect { { 33, 305 }, { 274, ListLines * ListLineHeight } };
/** The message area: the alternatives menu, the refusals and the last thing that happened. */
constexpr Rectangle BoardRect { { 33, 433 }, { 274, 180 } };
constexpr int BoardLineHeight = 20;

// The tab column's own geometry is GONE (2026-09-22): TabRect asks GetSideTabRect now, so the
// column is the shop's rather than a copy of it that had drifted to a 27px width and a 96px top.

constexpr uint8_t FrameGold = PAL16_YELLOW + 10; // the item grid's own outline gold (v1.12.094)
constexpr uint8_t PlateFill = PAL16_GRAY + 14;
constexpr uint8_t PlateEdge = PAL16_GRAY + 6;
constexpr uint32_t GreenRgb = 0x64A064;
constexpr uint32_t RedRgb = 0xC04030;
constexpr int HoverBrightenPercent = 115;
constexpr Displacement PressSink { -2, 2 };

/** Every tab either workshop can show; TabsFor says which of them a host has, in column order. */
enum class Tab : uint8_t {
	Reroll,
	Imbue,
	Gems,
	Runes,
	Jewels,
	/**
	 * Ogden's bench for the recipes his collection boards cannot run (user, 2026-09-22: "There must
	 * be another tab - Craft [...] where user performs other Ogden crafting recipes, not possible in
	 * his other tabs").
	 *
	 * The boards climb ladders - three gems for one better gem - and that is all they can do. Free
	 * the Sockets, Recolour Gems and Punch Sockets each act on an ITEM you put in front of him, so
	 * they need a grid to put it in.
	 */
	Craft,
	Recipes,
};
constexpr int MaxTabs = 5;

/** Every control the page can hold. The press sinks one of these; the release runs it. */
enum class Control : uint8_t {
	None,
	// The tab slots must stay CONTIGUOUS and in order: every handler derives the slot index as
	// `control - Control::Tab0`, so a value inserted among them silently renumbers the column.
	Tab0,
	Tab1,
	Tab2,
	Tab3,
	Tab4, // Ogden's fifth, since the Craft tab (2026-09-22)
	Reroll,
	Imbue,
	Remove,
	Cleanse,
	Option0,
	Option1,
	Option2,
	Option3,
	Upgrade,
	Downgrade,
	/** The rune ladder's two answers (2026-09-21) - see PendingStep. */
	ConfirmStep,
	CancelStep,
	/** Ogden's craft bench (2026-09-22): Griswold's Refresh plate under the grid. */
	Transmute,
	Close,
};

// MaxTabs was a dead constant until this (audit, 2026-09-22). It now guards the exact mistake made
// while adding the Craft tab: a tab with no control to press it, or a Tab value inserted among the
// slots so that `control - Control::Tab0` no longer indexes the column. Both are silent at runtime -
// the tab simply does nothing, or the wrong one opens.
static_assert(static_cast<int>(Control::Tab4) - static_cast<int>(Control::Tab0) + 1 == MaxTabs,
    "the tab controls no longer match MaxTabs - a tab would have no control to press, "
    "or the slots are no longer contiguous from Tab0");
constexpr int OptionCount = 4; // the affix as it stands, and three alternatives

bool WindowOpen = false;
WorkshopHost Host = WorkshopHost::Mystic;
Tab OpenTab = Tab::Reroll;
/** The one item on the bench. Returned to the pack when the window closes. */
Item Bench;
/**
 * @brief The craft grid, indexed by an item's TOP-LEFT cell.
 *
 * FOOTPRINTS since 2026-09-22 (user: "i placed a 2x3 item in the smaller frame in Craft tab and it
 * shrunk down to 1 slot only. Fix it. Size remains 2x3 when placed there [...] they still leave 6
 * unoccupied slots for ingredients").
 *
 * It was one item per cell whatever its size, which is the BENCH's rule applied twelve times - and
 * it was wrong the moment the grid became visible, because an item drawn to fit a 28px cell is an
 * item shrunk to a twelfth of itself. A 2x3 takes six of the twelve cells now and leaves six, which
 * is exactly the arithmetic the user did.
 *
 * The array is still indexed by anchor and still handed to CanCraftFromLevskiGrid unchanged: the
 * cells an item covers are recorded in CraftCells beside it, not in this array, so the recipes see
 * the same twelve-slot bag of items they always have.
 *
 * NEVER persisted - returned to the player when the window closes, exactly as the Cube's grid is, so
 * a crafting station stays out of the save format entirely.
 */
std::array<Item, CraftSlots> CraftGrid {};
/**
 * @brief cell -> the anchor covering it, plus one. Zero for a free cell.
 *
 * The Cube's GridCells by another name and for the same reason: a 2x3 occupies six cells and every
 * one of them has to name the same item, or the outlined item and the described item are two
 * different items.
 */
std::array<int8_t, CraftSlots> CraftCells {};
/**
 * @brief The kind the board has picked, by ITEM ID rather than by row (2026-09-21).
 *
 * The list it replaced was built from what the pack held, so a row index meant "the Nth kind you
 * own" - and that moved under the player the moment a conversion emptied a stack. The board shows
 * every kind whether owned or not, so the identity of the selection is the item itself and nothing
 * about it changes when the counts do.
 */
int SelectedStockIdx = -1;
/**
 * @brief The rune step waiting on an answer: +1 up, -1 down, 0 for no question standing.
 *
 * Only the RUNES ask (user, 2026-09-21: "Runes upgrades and downgrade to ask for confirmation, just
 * like the salvaging"), and for the same reason salvage asks: a rune is the scarcest thing in the
 * game and a misclick on Zod cannot be undone. Gems and jewels act at once.
 *
 * Withdrawn by anything that changes what the question was about - picking another kind, changing
 * tab, closing the window - so a standing question can never be answered for a different rune than
 * the one it named.
 */
int PendingStep = 0;
/** @brief The recipe page's scroll, in PIXELS - the lines are not a fixed height once wrapped. */
int RecipeScroll = 0;
int SelectedRow = -1;
Control Pressed = Control::None;
Control LastHovered = Control::None;
std::string Board; // what the page is saying right now

/** The alternatives the last Reroll rolled: [0] is the affix as it stands, [1..3] the offers. */
bool OfferOpen = false;
int OfferSlot = -1;
std::array<OracoolAffix, OptionCount> Offers {};

/**
 * @brief The item's own counters (Item::_iOracoolRerolls and friends, item format 15). Until 2026-09-27 they were a
 * per-game table keyed by seed, and going back to the menu - which keeps the hero and every item - reset the doubling
 * price and freed the lock (audit; user: "fix all four").
 */
struct ItemCounters {
	uint8_t rerolls = 0;
	uint8_t removals = 0;
	/** The affix slot the first reroll locked, or -1: only that one may be rerolled afterwards (the user agreed). */
	int8_t lockedAffix = -1;
};

ItemCounters CountersOf(const Item &item)
{
	return { item._iOracoolRerolls, item._iOracoolRemovals, item._iOracoolLockedAffix };
}

/** @brief Gold: a base that doubles with every attempt on THIS item, capped so it stays payable. */
int PriceFor(int base, int attempts)
{
	int price = base;
	for (int i = 0; i < attempts && price < 2000000; i++)
		price *= 2;
	return std::min(price, 2000000);
}

int RerollPrice(const Item &item)
{
	if (item.isEmpty())
		return 0;
	const int level = std::max<int>(1, item._iOracoolItemLevel);
	return PriceFor(500 * level, CountersOf(item).rerolls);
}

int RemovePrice(const Item &item)
{
	if (item.isEmpty())
		return 0;
	const int level = std::max<int>(1, item._iOracoolItemLevel);
	return PriceFor(250 * level, CountersOf(item).removals);
}

int CleansePrice(const Item &item)
{
	// Everything at once, so it is priced as everything: one removal's price per shard on the item.
	if (item.isEmpty())
		return 0;
	const ImbuementLedger ledger = CaptureImbuements(item);
	return RemovePrice(item) * std::max<int>(1, ledger.count);
}

Rectangle PageRect()
{
	return Rectangle { { 0, BottomDockedTop(PageSize.height) }, PageSize };
}

Rectangle Panel(const Rectangle &rect)
{
	const Rectangle page = PageRect();
	return Rectangle { page.position + Displacement { rect.position.x, rect.position.y }, rect.size };
}

/**
 * @brief One tab in the column beside the page - GRISWOLD'S column, since 2026-09-22.
 *
 * The user asked for his tabs here ("Tabs are code drawn - replace them with tab design from
 * Griswald - using button assets and rotated text"), and this window docks in exactly the shop
 * panel's rect, so the tabs must be exactly the shop's tabs: a column of its own at a slightly
 * different top or width would be the same furniture in the wrong place, which is the difference the
 * eye catches flipping between a vendor and Ogden.
 *
 * The local TabTop/TabSize/TabGap this replaced are gone with it.
 */
Rectangle TabRect(int index)
{
	return GetSideTabRect(index);
}

/** @brief The tabs @p host shows, in column order. */
std::vector<Tab> TabsFor(WorkshopHost host)
{
	if (host == WorkshopHost::Mystic)
		return { Tab::Reroll, Tab::Imbue, Tab::Craft, Tab::Recipes };
	// Ogden's tables (user, 2026-09-21): "a list of all Gem types with the number the user curently owns of each
	// and clicking on certain type provides Upgrade/Downgrade options", the same for runes, and his jewels beside
	// them; his socket recipes are a tab away in his book.
	return { Tab::Gems, Tab::Runes, Tab::Jewels, Tab::Craft, Tab::Recipes };
}

const char *TabName(Tab tab)
{
	switch (tab) {
	case Tab::Reroll:
		return N_("Reroll");
	case Tab::Imbue:
		return N_("Imbue");
	case Tab::Gems:
		return N_("Gems");
	case Tab::Runes:
		return N_("Runes");
	case Tab::Jewels:
		return N_("Jewels");
	case Tab::Craft:
		return N_("Craft");
	case Tab::Recipes:
		break;
	}
	return N_("Recipes");
}

/** @brief Whether @p tab is one of Ogden's stock lists, and what it lists. */
bool IsStockTab(Tab tab)
{
	return tab == Tab::Gems || tab == Tab::Runes || tab == Tab::Jewels;
}

/**
 * @brief Which board @p tab draws: the gems' and runes' 7x5 of 30px cells, or the jewels' 5x3.
 *
 * Defined HERE rather than beside the shapes, because it switches on Tab and the enum is declared
 * further down the file than the geometry is.
 */
BoardShape ShapeFor(Tab tab)
{
	return tab == Tab::Jewels ? JewelBoard : GemBoard;
}

bool StockMatches(Tab tab, int idx)
{
	switch (tab) {
	case Tab::Gems:
		return IsOracoolGemIdx(idx);
	case Tab::Runes:
		return IsOracoolRuneIdx(idx);
	case Tab::Jewels:
		return IsOracoolJewelIdx(idx);
	default:
		return false;
	}
}


/** @brief How many of a kind the ladder asks for a step up: three stones or jewels, two runes. */
int StepUpCost(Tab tab)
{
	return tab == Tab::Runes ? 2 : 3;
}

/** @brief The kind one step up @p tab's ladder, or 0 at the top. */
int StepUp(Tab tab, int idx)
{
	switch (tab) {
	case Tab::Gems:
		return NextGemQuality(static_cast<uint16_t>(idx));
	case Tab::Runes:
		return IsTopRune(static_cast<uint16_t>(idx)) ? 0 : NextRune(static_cast<uint16_t>(idx));
	case Tab::Jewels:
		return NextJewelGrade(static_cast<uint16_t>(idx));
	default:
		return 0;
	}
}

/** @brief The kind one step DOWN, or 0 at the bottom - the walk back up from the foot of the ladder. */
int StepDown(Tab tab, int idx)
{
	if (tab == Tab::Runes) {
		for (size_t i = 1; i < RuneLadderSize(); i++) {
			if (RuneAtLadderPosition(i) == idx)
				return RuneAtLadderPosition(i - 1);
		}
		return 0;
	}
	// Gems and jewels: the rung whose step up lands on this one.
	for (int candidate = 0; candidate < static_cast<int>(IDI_LAST) + 1; candidate++) {
		if (StockMatches(tab, candidate) && StepUp(tab, candidate) == idx)
			return candidate;
	}
	return 0;
}

/**
 * @brief The item the board's cell @p slot stands for on @p tab, or 0 for a cell that stands for
 * nothing (the runes' last two, the jewels' unused columns).
 *
 * THE STASH'S OWN ORDER, in every case, because the user asked for it by name and because a player
 * who has sorted their stash has already learned this arrangement:
 *  - gems: column by GemType, row by GemQuality - SortStash's `{ type, GemTopRow + quality }`;
 *  - runes: ladder position, read left to right and down, as the stash's rune block reads;
 *  - jewels: grade-major five to a row, the block shape the stash gives them.
 */
int BoardSlotItem(Tab tab, int slot)
{
	const BoardShape shape = ShapeFor(tab);
	const int column = slot % shape.columns;
	const int row = slot / shape.columns;
	switch (tab) {
	case Tab::Gems:
		if (column >= static_cast<int>(GemTypeCount) || row >= static_cast<int>(GemQualityCount))
			return 0;
		return GemIndexFor(static_cast<GemType>(column), static_cast<GemQuality>(row));
	case Tab::Runes:
		return slot < static_cast<int>(RuneLadderSize()) ? RuneAtLadderPosition(slot) : 0;
	case Tab::Jewels: {
		// Five families across, three grades down - the stash's block, left-aligned on this board.
		if (column >= static_cast<int>(JewelFamilyCount) || row >= static_cast<int>(JewelGradeCount))
			return 0;
		return IDI_ORACOOL_JEWEL_FERVOR_FLAWED + row * static_cast<int>(JewelFamilyCount) + column;
	}
	default:
		return 0;
	}
}

int CountInPack(const Player &player, int idx)
{
	int total = 0;
	const auto add = [&](const Item *list, int count) {
		for (int i = 0; i < count; i++) {
			if (!list[i].isEmpty() && list[i].IDidx == idx)
				total += std::max(1, list[i].stackCount());
		}
	};
	add(player.InvList, player._pNumInv);
	for (int tab = 0; tab < Player::NumExtraInventoryTabs; tab++)
		add(player.InvTabList[tab].data(), player._pNumInvTab[tab]);
	return total;
}

/** @brief Takes exactly @p count of @p idx out of the pack, stacks and tabs included. */
void TakeFromPack(Player &player, int idx, int count)
{
	int owed = count;
	for (int i = player._pNumInv - 1; i >= 0 && owed > 0; i--) {
		if (player.InvList[i].isEmpty() || player.InvList[i].IDidx != idx)
			continue;
		const int units = std::max(1, player.InvList[i].stackCount());
		if (units <= owed) {
			owed -= units;
			player.RemoveInvItem(i, false);
		} else {
			player.InvList[i].setStackCount(units - owed);
			owed = 0;
		}
	}
	for (int tab = 0; tab < Player::NumExtraInventoryTabs && owed > 0; tab++) {
		for (int i = player._pNumInvTab[tab] - 1; i >= 0 && owed > 0; i--) {
			Item &item = player.InvTabList[tab][i];
			if (item.isEmpty() || item.IDidx != idx)
				continue;
			const int units = std::max(1, item.stackCount());
			if (units <= owed) {
				owed -= units;
				RemoveExtraTabItem(player, tab, i);
			} else {
				item.setStackCount(units - owed);
				owed = 0;
			}
		}
	}
}

/**
 * THE STASH COUNTS TOO (user, 2026-09-22: "Gems and Runes tabs must be able to see also runes and
 * gems inside Stash, not just in Backpack").
 *
 * The board asked CountInPack, which walks the backpack and its tabs and stops there - so a player
 * whose gems were all in the stash saw a board of red zeroes and greyed arrows while standing on a
 * hoard of them. Everything below reads and spends BOTH stores, pack first.
 *
 * Pack first is deliberate rather than arbitrary: it is what the player is carrying, it is what they
 * can see without opening another window, and spending it keeps the stash as the deeper store.
 */
int CountInStash(int idx)
{
	int total = 0;
	for (const Item &item : Stash.stashList) {
		if (!item.isEmpty() && item.IDidx == idx)
			total += std::max(1, item.stackCount());
	}
	return total;
}

int CountOwned(const Player &player, int idx)
{
	return CountInPack(player, idx) + CountInStash(idx);
}

/** @brief Takes up to @p count of @p idx out of the stash, stacks included. */
void TakeFromStash(int idx, int count)
{
	int owed = count;
	// BACKWARDS, because RemoveStashItem erases from the list and every index after it shifts down.
	for (int i = static_cast<int>(Stash.stashList.size()) - 1; i >= 0 && owed > 0; i--) {
		Item &item = Stash.stashList[i];
		if (item.isEmpty() || item.IDidx != idx)
			continue;
		const int units = std::max(1, item.stackCount());
		if (units <= owed) {
			owed -= units;
			Stash.RemoveStashItem(static_cast<StashStruct::StashCell>(i));
		} else {
			item.setStackCount(units - owed);
			owed = 0;
		}
	}
}

/** @brief Takes @p count of @p idx from wherever the player keeps them - the pack, then the stash. */
void TakeOwned(Player &player, int idx, int count)
{
	const int fromPack = std::min(count, CountInPack(player, idx));
	if (fromPack > 0)
		TakeFromPack(player, idx, fromPack);
	if (count > fromPack)
		TakeFromStash(idx, count - fromPack);
}

/** @brief Where a made item ended up, so the board can say so under the grid. */
enum class Landing : uint8_t {
	Backpack,
	Stash,
	Ground,
	Nowhere,
};

/**
 * @brief Puts one @p idx where there is room: the backpack, else the stash, else the floor.
 *
 * The user's order exactly (2026-09-22): "land in backpack if there is enough space, otherwize land
 * in Stash [...] If both full - drop on ground." The floor is the last resort rather than a refusal,
 * because the materials have already been spent by the time this is called - refusing here would
 * either swallow them or need the whole step undone, and a dropped item is at the player's feet.
 */
Landing GiveOwned(Player &player, int idx)
{
	Item made;
	InitializeItem(made, static_cast<_item_indexes>(idx));
	GenerateNewSeed(made);
	made._iIdentified = true;
	made.updateRequiredStatsCacheForPlayer(player);
	if (AutoPlaceItemInInventory(player, made, true))
		return Landing::Backpack;
	if (AutoPlaceItemInStash(player, made, true))
		return Landing::Stash;
	// On a FREE tile beside him (audit, 2026-09-27): PlaceItemInWorld on his own tile overwrote whatever lay there, so each
	// further step orphaned the last drop - and it answered 0, read here as "nowhere", for a real placement at Items[0].
	if (ActiveItemCount >= MAXITEMS)
		return Landing::Nowhere;
	DropItemBesidePlayer(player, std::move(made));
	return Landing::Ground;
}

/** @brief Puts @p count of @p idx into the pack. Returns how many actually fitted. */
int GiveToPack(Player &player, int idx, int count)
{
	int placed = 0;
	for (; placed < count; placed++) {
		Item made;
		InitializeItem(made, static_cast<_item_indexes>(idx));
		GenerateNewSeed(made);
		made._iIdentified = true;
		made.updateRequiredStatsCacheForPlayer(player);
		if (!AutoPlaceItemInInventory(player, made, true))
			break;
	}
	return placed;
}

// ButtonRect is GONE (2026-09-22). It placed the 136x30 word-buttons in one or two rows; every one
// of them is a 34px icon plate now, laid out by ServiceIconRect below.

/**
 * @brief Icon @p index of a row of @p count, centred in the canvas opening.
 *
 * The PLATE's rect. Its price line hangs below it and is not part of the button - a click belongs to
 * the plate, and a rect that included the number would make the price itself pressable.
 */
/**
 * @brief Where the one-item bench is - the frame her single-item canvas paints, on her Reroll and
 * Imbue tabs.
 *
 * Her Craft tab has no bench any more: the multi-item canvas paints a 3x4 well in that same frame,
 * so Craft uses CraftSlotRect like Ogden's does. One function, because the draw, the hover and the
 * click must agree about where the bench is - and they are three different places in this file.
 */
Rectangle BenchSlotRect()
{
	return Panel(MysticFrameRect);
}

/**
 * @brief Icon @p index of a row of @p count, on the floor beside the small frame.
 *
 * DISTRIBUTED across that floor, as asked, rather than laid out from a fixed gap: the first plate
 * starts at the canvas's own left edge, the last ends six pixels clear of the frame, and the rest
 * are spaced evenly between them.
 *
 * A ROW OF ONE STANDS AT THE RIGHT END (user, 2026-09-22: "bring transmute icon 6px away from
 * smaller frame of gillian in craft tab. i dont want it so far away"). It was centred in the band,
 * which put Craft's Transmute and Reroll's plate seventy pixels from the frame they act on. The row
 * is ANCHORED at the six-pixel clearance and distributes leftward, so a lone plate simply is that
 * anchor - and it lands exactly where Imbue's third plate stands, which is the one place on this
 * page the eye is already used to finding a button.
 *
 * ServiceIconGap survives only as the width the price line may overhang into; it no longer places
 * anything.
 */
Rectangle ServiceIconRect(int index, int count)
{
	const Rectangle page = PageRect();
	const int anchor = MysticIconRowRight + 1 - ServiceIconSize.width;
	const int travel = anchor - InnerLeft;
	// Rounded rather than truncated, so the last plate lands exactly on the six-pixel clearance
	// instead of a pixel or two inside it.
	const int left = count <= 1
	    ? anchor
	    : InnerLeft + (index * travel + (count - 1) / 2) / (count - 1);
	return Rectangle { { page.position.x + left, page.position.y + MysticIconRowTop }, ServiceIconSize };
}

Rectangle OptionRect(int index)
{
	const Rectangle board = Panel(BoardRect);
	return Rectangle { { board.position.x + 4, board.position.y + 26 + index * (BoardLineHeight + 6) }, { board.size.width - 8, BoardLineHeight + 2 } };
}

Rectangle ListRowRect(int row)
{
	const Rectangle list = Panel(ListRect);
	return Rectangle { { list.position.x, list.position.y + row * ListLineHeight }, { list.size.width, ListLineHeight } };
}

/** @brief The rect of @p control right now, or an empty one when the page is not showing it. */
Rectangle ControlRect(Control control)
{
	switch (control) {
	case Control::Tab0:
	case Control::Tab1:
	case Control::Tab2:
	case Control::Tab3:
	case Control::Tab4: {
		const int slot = static_cast<int>(control) - static_cast<int>(Control::Tab0);
		return slot < static_cast<int>(TabsFor(Host).size()) ? TabRect(slot) : Rectangle { { 0, 0 }, { 0, 0 } };
	}
	case Control::Close:
		return Panel(CloseRect);
	// Icon plates since 2026-09-22. Her Imbue tab's three sit in ONE row now; they were two on the
	// first row and Cleanse alone on a second, which a 34px plate makes unnecessary.
	case Control::Reroll:
		return OpenTab == Tab::Reroll && !OfferOpen ? ServiceIconRect(0, 1) : Rectangle { { 0, 0 }, { 0, 0 } };
	case Control::Imbue:
		return OpenTab == Tab::Imbue ? ServiceIconRect(0, 3) : Rectangle { { 0, 0 }, { 0, 0 } };
	case Control::Remove:
		return OpenTab == Tab::Imbue ? ServiceIconRect(1, 3) : Rectangle { { 0, 0 }, { 0, 0 } };
	case Control::Cleanse:
		return OpenTab == Tab::Imbue ? ServiceIconRect(2, 3) : Rectangle { { 0, 0 }, { 0, 0 } };
	case Control::Option0:
	case Control::Option1:
	case Control::Option2:
	case Control::Option3:
		return OfferOpen ? OptionRect(static_cast<int>(control) - static_cast<int>(Control::Option0)) : Rectangle { { 0, 0 }, { 0, 0 } };
	// The two arrow plates on the board's own row, flush with its edges - up on the right, down on
	// the left (user, 2026-09-21). They were a pair of wide word-buttons in the middle of the page.
	case Control::Upgrade:
		return IsStockTab(OpenTab) ? Panel(BoardUpgradeRect) : Rectangle { { 0, 0 }, { 0, 0 } };
	case Control::Downgrade:
		return IsStockTab(OpenTab) ? Panel(BoardDowngradeRect) : Rectangle { { 0, 0 }, { 0, 0 } };
	// The two answers exist only while a question is standing, so a stale click cannot find them.
	case Control::ConfirmStep:
		return PendingStep != 0 ? Panel(BoardConfirmRect) : Rectangle { { 0, 0 }, { 0, 0 } };
	case Control::CancelStep:
		return PendingStep != 0 ? Panel(BoardCancelRect) : Rectangle { { 0, 0 }, { 0, 0 } };
	case Control::Transmute:
		// Ogden's sits under his painted well; hers stands ABOVE her small frame, on the same line
		// and in the same place as Reroll's plate on the tab before it, so the button does not jump
		// as the player moves between her pages.
		if (OpenTab != Tab::Craft)
			return Rectangle { { 0, 0 }, { 0, 0 } };
		return Host == WorkshopHost::Mystic ? ServiceIconRect(0, 1) : Panel(CraftTransmuteRect);
	case Control::None:
		break;
	}
	return Rectangle { { 0, 0 }, { 0, 0 } };
}

/** @brief Whether the bench holds something the Mystic will touch at all. */
bool BenchIsWorkable(std::string &why)
{
	if (Bench.isEmpty()) {
		why = _("Put an item on the bench.");
		return false;
	}
	if (Bench._itype == ItemType::Gold || Bench._iClass == ICLASS_QUEST) {
		why = _("She will not work on that.");
		return false;
	}
	if (!Bench._iIdentified) {
		why = _("Identify it first.");
		return false;
	}
	if (GetActiveRuneword(Bench) != nullptr) {
		why = _("A runeword holds this item together.");
		return false;
	}
	return true;
}

void SetBoard(std::string text)
{
	Board = std::move(text);
}

/**
 * @brief Gives the bench back to the pack, or to the stash when the pack is full - as the craft grid's
 * items go. False - and the item stays - when neither has room. The stash since v1.12.189 (external
 * audit, SAVE-01): leaving the game closes this window before the exit save, and a full pack must not
 * be the reason an item is left on a bench that is not saved.
 */
bool ReturnBench()
{
	if (Bench.isEmpty())
		return true;
	if (!AutoPlaceItemInInventory(*MyPlayer, Bench, true) && !AutoPlaceItemInStash(*MyPlayer, Bench, true))
		return false;
	Bench.clear();
	return true;
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------

void OutlineRect(const Surface &out, const Rectangle &rect, uint8_t color)
{
	FillRect(out, rect.position.x, rect.position.y, rect.size.width, 1, color);
	FillRect(out, rect.position.x, rect.position.y + rect.size.height - 1, rect.size.width, 1, color);
	FillRect(out, rect.position.x, rect.position.y, 1, rect.size.height, color);
	FillRect(out, rect.position.x + rect.size.width - 1, rect.position.y, 1, rect.size.height, color);
}

void OutlineRectRgb(const Surface &out, const Rectangle &rect, uint32_t rgb, uint8_t fallback)
{
	FillRectRgb(out, rect.position.x, rect.position.y, rect.size.width, 1, rgb, fallback);
	FillRectRgb(out, rect.position.x, rect.position.y + rect.size.height - 1, rect.size.width, 1, rgb, fallback);
	FillRectRgb(out, rect.position.x, rect.position.y, 1, rect.size.height, rgb, fallback);
	FillRectRgb(out, rect.position.x + rect.size.width - 1, rect.position.y, 1, rect.size.height, rgb, fallback);
}

// DrawPlateButton is GONE with it. It drew the code-drawn placeholder plate these windows wore
// before the painted art arrived - "a placeholder button", as its own comment said - and its last
// four callers became icon plates on 2026-09-22.

void DrawTabColumn(const Surface &out)
{
	const std::vector<Tab> tabs = TabsFor(Host);
	for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
		const bool active = tabs[i] == OpenTab;
		// Held down: the face sinks and springs back on the release, like every other button here (user,
		// 2026-09-21: "make all tab buttons on all vendors sinkable on click"). The hit test stays on the
		// unsunk rect, so a tab cannot slide out from under a pointer that has not moved.
		const bool held = Pressed == static_cast<Control>(static_cast<int>(Control::Tab0) + i);
		// Griswold's own tab, drawn by his own helper (2026-09-22): the vanilla dialog button laid on
		// its side with the label reading down it, the sink on the press, the hover. The plate and the
		// stack of capitals this replaced were a code-drawn stand-in from before that art existed, and
		// the difference showed the moment the two windows sat in the same slot.
		DrawSideTab(out, i, _(TabName(tabs[i])), active, held);
	}
}

/**
 * @brief The one-item bench: what stands in the small frame her canvas paints.
 *
 * NOTHING is drawn for an empty bench. The plate fill, the gold outline and the 2x3 of cell lines
 * this used to lay down were the bench itself, back when the window drew its own furniture; over a
 * painted frame they are a second frame inside the first, and the cell lines describe a grid the
 * painting does not have.
 *
 * AT ITS OWN SIZE, centred (user, 2026-09-22: "items keep original size when placed in that frame").
 * It was fitted to the frame, which blew a ring up to 87x115 and shrank nothing - the opposite
 * mistake to the craft grid's, in the same frame. Nothing needs fitting here: the frame is 87 by 115
 * and the largest item in the game is a 2x3 at 56 by 84.
 */
void DrawBench(const Surface &out)
{
	const Rectangle slot = BenchSlotRect();
	if (!Bench.isEmpty()) {
		const ClxSprite sprite = GetInvItemSprite(Bench._iCurs + CURSOR_FIRSTITEM);
		const Point topLeft { slot.position.x + (slot.size.width - static_cast<int>(sprite.width())) / 2,
			slot.position.y + (slot.size.height - static_cast<int>(sprite.height())) / 2 };
		DrawItem(Bench, out, { topLeft.x, topLeft.y + static_cast<int>(sprite.height()) - 1 }, sprite);
	}
	if (slot.contains(MousePosition))
		DrawHoverOutline(out, slot);
}

void DrawRerollList(const Surface &out)
{
	const ItemCounters counters = Bench.isEmpty() ? ItemCounters {} : CountersOf(Bench);
	for (int row = 0; row < ListLines; row++) {
		const Rectangle rect = ListRowRect(row);
		if (Bench.isEmpty() || row >= Bench._iOracoolAffixCount) {
			if (row == 0 && Bench.isEmpty())
				DrawString(out, _("no item on the bench"), rect, { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter });
			continue;
		}
		const bool selected = SelectedRow == row;
		if (selected)
			FillRect(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, PlateFill);
		// GREEN FOR THE ONE, RED FOR THE REST, once the item is settled (user, 2026-09-22: "when an
		// affix is rerolled, use red font for other affixes from now on as they are now untouchable.
		// use green text for the rerolled one").
		//
		// The first reroll locks this item to one affix for good - counters.lockedAffix - and until
		// now the others were merely dimmed to whitegold, which is a shade, not an answer. Red says
		// they cannot be worked; green says which one still can.
		//
		// Asked of lockedAffix rather than of the reroll count, because that field IS the rule: it
		// is what RunControl refuses on, so the colours and the refusal cannot drift apart.
		const bool settled = counters.lockedAffix >= 0;
		const UiFlags colour = settled
		    ? (row == counters.lockedAffix ? UiFlags::ColorOracoolGreen : UiFlags::ColorRed)
		    : (selected ? UiFlags::ColorGold : UiFlags::ColorWhite);
		const StringOrView line = PrintOracoolAffixPower(Bench._iOracoolAffixes[row], Bench);
		DrawString(out, line.str(), Rectangle { { rect.position.x + 4, rect.position.y }, { rect.size.width - 8, rect.size.height } },
		    { colour | UiFlags::FontSize12 | UiFlags::VerticalCenter });
		if (!selected && rect.contains(MousePosition))
			OutlineRect(out, rect, PlateEdge);
	}
}

void DrawImbueList(const Surface &out)
{
	if (Bench.isEmpty()) {
		DrawString(out, _("no item on the bench"), ListRowRect(0), { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter });
		return;
	}
	const ImbuementLedger ledger = CaptureImbuements(Bench);
	for (int row = 0; row < ListLines && row < ledger.count; row++) {
		const Rectangle rect = ListRowRect(row);
		const bool selected = SelectedRow == row;
		if (selected)
			FillRect(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, PlateFill);
		const ShardDefinition &def = ShardDef(static_cast<ShardKind>(ledger.kinds[row]));
		DrawString(out, StrCat(_(def.name), " - ", _(def.line)),
		    Rectangle { { rect.position.x + 4, rect.position.y }, { rect.size.width - 8, rect.size.height } },
		    { (selected ? UiFlags::ColorGold : UiFlags::ColorWhite) | UiFlags::FontSize12 | UiFlags::VerticalCenter });
		if (!selected && rect.contains(MousePosition))
			OutlineRect(out, rect, PlateEdge);
	}
	if (ledger.count == 0)
		DrawString(out, _("nothing imbued yet"), ListRowRect(0), { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter });
}

/** @brief Ogden's list: one row per kind the pack holds, with what the player owns of it. */
/** @brief The board cell @p slot, in screen space. */
Rectangle BoardSlotRect(int slot)
{
	const Rectangle page = PageRect();
	const BoardShape shape = ShapeFor(OpenTab);
	const Point origin = BoardOriginFor(shape);
	return Rectangle { { page.position.x + origin.x + (slot % shape.columns) * shape.cellWidth,
	                       page.position.y + origin.y + (slot / shape.columns) * shape.cellHeight },
		{ shape.cellWidth, shape.cellHeight } };
}

/** @brief How many cells the open tab's board has. */
int BoardSlotCount()
{
	const BoardShape shape = ShapeFor(OpenTab);
	return shape.columns * shape.rows;
}

/** @brief The board cell under @p position, or -1. */
int BoardSlotAt(Point position)
{
	for (int slot = 0; slot < BoardSlotCount(); slot++) {
		if (BoardSlotRect(slot).contains(position))
			return slot;
	}
	return -1;
}

const char *BoardTitle(Tab tab)
{
	switch (tab) {
	case Tab::Runes:
		return N_("Runes Collection");
	case Tab::Jewels:
		return N_("Jewels Collection");
	default:
		return N_("Gem Collection");
	}
}

/**
 * @brief The whole collection: every kind on the board, owned or not, with what the pack holds.
 *
 * Three rules, all the user's (2026-09-21):
 *  - a counter at the bottom left of each cell, white on a dark transparent ground;
 *  - RED when the count is zero;
 *  - and the icon DESATURATED for a kind not possessed, so the board reads at a glance as what has
 *    been found and what has not.
 *
 * The count is CountOwned - the backpack, its tabs AND the stash (user, 2026-09-22). It was the pack
 * alone, which showed a board of red zeroes and greyed arrows to a player whose gems were all in the
 * stash. It is still "what you have", never "what exists": the arrows can only spend what it counts.
 */
void DrawCollectionBoard(const Surface &out)
{
	const Player &player = *MyPlayer;
	// THE SLOT FACE (user, 2026-09-22: "also apply it to all vendors grids which might be a bit
	// bigger like ogden and gillian's grids"). His boards are the two grids in the game that are not
	// 28px - gems at 30x30, jewels at 42x50 - and DrawSlotBackground scales the art to whichever it
	// is handed. Every cell, INCLUDING the ones standing for nothing: the board's shape is the
	// board's shape, and a hole where the runes run out would read as a fault in the painting.
	for (int slot = 0; slot < BoardSlotCount(); slot++)
		DrawSlotBackground(out, BoardSlotRect(slot));

	for (int slot = 0; slot < BoardSlotCount(); slot++) {
		const int idx = BoardSlotItem(OpenTab, slot);
		if (idx == 0)
			continue; // a cell standing for nothing: the runes' last two, the jewels' spare columns
		const Rectangle cell = BoardSlotRect(slot);
		const int count = CountOwned(player, idx);

		const ClxSprite sprite = GetInvItemSprite(AllItemsList[idx].iCurs + CURSOR_FIRSTITEM);
		if (OpenTab == Tab::Jewels) {
			// EIGHTY PER CENT of the jewels' roomier cell (user, 2026-09-22: "You can increase their
			// icon size to fit 80% on new grid size"). 80% of 42x50 is 33x40, and a square 28px sprite
			// fitted into that comes out 33 - bigger than its natural size, which is the point.
			//
			// DrawSpriteToFit rather than DrawSpriteScaled: the latter takes an integer factor, so it
			// could only offer 28 or 56 here. See ornate_border.h.
			constexpr int IconPercent = 80;
			const Rectangle icon { { cell.position.x + cell.size.width * (100 - IconPercent) / 200,
				                       cell.position.y + cell.size.height * (100 - IconPercent) / 200 },
				{ cell.size.width * IconPercent / 100, cell.size.height * IconPercent / 100 } };
			DrawSpriteToFit(out, icon, sprite);
		} else {
			// The gems' and runes' 30px cell is barely larger than the 28px sprite, so it is drawn at
			// its natural size: fitting it to 80% would make these icons SMALLER, not bigger.
			const Point topLeft { cell.position.x + (cell.size.width - static_cast<int>(sprite.width())) / 2,
				cell.position.y + (cell.size.height - static_cast<int>(sprite.height())) / 2 };
			ClxDraw(out, { topLeft.x, topLeft.y + static_cast<int>(sprite.height()) - 1 }, sprite);
		}
		if (count == 0) {
			// The same white-hue pass every inactive plate in this mod wears, so "you have none of
			// these" looks the same here as "this button does nothing" does at Griswold's.
			TintRectRgb(out, cell.position.x, cell.position.y, cell.size.width, cell.size.height,
			    0xFFFFFFu, /*brightnessPercent=*/70, /*floorPercent=*/0, PAL16_GRAY);
		}

		// The counter, bottom RIGHT since 2026-09-22 (user: "move the badges of items in ogden shops
		// from bottom left to bottom right"), on its own half-transparent ground so it reads over any
		// icon. The box is as wide as the number needs, so the right edge is the fixed one now and
		// the box grows leftward as the count reaches three figures.
		const std::string text = StrCat(std::min(count, 99));
		const int width = GetLineWidth(text, GameFont12) + 4;
		const Rectangle box { { cell.position.x + cell.size.width - width, cell.position.y + cell.size.height - 12 },
			{ width, 12 } };
		DrawHalfTransparentRectTo(out, box.position.x, box.position.y, box.size.width, box.size.height);
		DrawString(out, text, box,
		    { (count == 0 ? UiFlags::ColorRed : UiFlags::ColorWhite) | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });

		if (idx == SelectedStockIdx)
			OutlineRect(out, cell, FrameGold);
		else if (cell.contains(MousePosition))
			DrawHoverOutline(out, cell);
	}

	DrawString(out, _(BoardTitle(OpenTab)), Panel(BoardTitleRect),
	    { UiFlags::ColorGold | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
}

/**
 * @brief One of the two arrow plates: Griswold's frame, his sell arrow turned a quarter turn.
 *
 * Sinks on the press and springs back on the release like every button in the mod, and greys when
 * the selection cannot take the step - nothing above Perfect, nothing below Chipped, or not enough
 * in the pack to pay for it.
 */
/**
 * @brief One of Gillian's services: Griswold's plate, her icon, and its PRICE drawn beneath it.
 *
 * The price line is why this exists rather than reusing DrawBoardArrow. It is drawn under the plate,
 * never in a hover hint (user, 2026-09-22), and it is the one thing the wide word-buttons did better
 * than an icon can - so the row is laid out around it.
 *
 * Red when the purse cannot cover it: a greyed plate says "not now" and the number says why, which
 * between them is the whole answer without a click.
 */
void DrawServiceIcon(const Surface &out, Control control, const char *glyph, bool enabled, int price)
{
	const Rectangle rect = ControlRect(control);
	if (rect.size.width == 0)
		return;
	const bool hovered = rect.contains(MousePosition);
	const Rectangle face { rect.position + (Pressed == control ? PressSink : Displacement { 0, 0 }), rect.size };
	if (GetLoosePngSize(BoardButtonFrameAsset).width > 0)
		DrawLoosePng(out, BoardButtonFrameAsset, face.position);
	else
		DrawOrnateBorder(out, face);
	if (const Size size = GetLoosePngSize(glyph); size.width > 0) {
		DrawLoosePng(out, glyph, { face.position.x + (face.size.width - size.width) / 2,
		                             face.position.y + (face.size.height - size.height) / 2 });
	}
	if (!enabled) {
		TintRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height,
		    0xFFFFFFu, /*brightnessPercent=*/70, /*floorPercent=*/0, PAL16_GRAY);
	} else if (hovered) {
		BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, HoverBrightenPercent);
	}

	// ABOVE the plate, not below it (2026-09-22). The row's feet are flush with the small frame's
	// lower border at y 280 and the big frame's painted bezel begins at 289, so a line under the
	// plate would sit half on the moulding.
	//
	// Wider than the plate by the row's own gap, so eight digits overhang into the air between icons
	// rather than being clipped by a 34px box - but never past the canvas's opening.
	//
	// The overhang is SYMMETRIC and shrinks to whatever both sides can spare, because the leftmost
	// plate of the row stands on the canvas's own inner edge: a box clamped on one side only would
	// still be centred on itself, and its number would sit a few pixels right of the plate it prices.
	const Rectangle page = PageRect();
	const int overhang = std::max(0, std::min({ ServiceIconGap / 2,
	                                  rect.position.x - (page.position.x + InnerLeft),
	                                  (page.position.x + InnerRight) - (rect.position.x + rect.size.width - 1) }));
	const Rectangle line { { rect.position.x - overhang, page.position.y + MysticPriceRowTop },
		{ rect.size.width + 2 * overhang, ServicePriceHeight } };
	const bool afford = price <= 0 || static_cast<int>(TotalPlayerGold()) >= price;
	DrawString(out, price > 0 ? FormatInteger(price) : std::string { _("Free") }, line,
	    { (afford ? UiFlags::ColorWhitegold : UiFlags::ColorRed) | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
}

void DrawBoardArrow(const Surface &out, Control control, bool enabled)
{
	const Rectangle rect = ControlRect(control);
	if (rect.size.width == 0)
		return;
	const bool hovered = rect.contains(MousePosition);
	const Rectangle face { rect.position + (Pressed == control ? PressSink : Displacement { 0, 0 }), rect.size };
	if (GetLoosePngSize(BoardButtonFrameAsset).width > 0)
		DrawLoosePng(out, BoardButtonFrameAsset, face.position);
	else
		DrawOrnateBorder(out, face);
	const char *glyph = control == Control::Upgrade ? BoardUpGlyphAsset : BoardDownGlyphAsset;
	if (const Size size = GetLoosePngSize(glyph); size.width > 0) {
		DrawLoosePng(out, glyph, { face.position.x + (face.size.width - size.width) / 2,
		                             face.position.y + (face.size.height - size.height) / 2 });
	} else {
		DrawString(out, control == Control::Upgrade ? "^" : "v", face,
		    { UiFlags::ColorGold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}
	if (!enabled) {
		TintRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height,
		    0xFFFFFFu, /*brightnessPercent=*/70, /*floorPercent=*/0, PAL16_GRAY);
	} else if (hovered) {
		BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, 115);
	}
}

/**
 * @brief What the board is saying, under the grid: the standing question, or the last thing done.
 *
 * The question names the rune and both sides of the trade, because "are you sure?" over a board of
 * thirty-five icons does not say which one is about to be spent.
 */
void DrawBoardMessage(const Surface &out)
{
	if (PendingStep != 0 && SelectedStockIdx > 0) {
		const int made = PendingStep > 0 ? StepUp(OpenTab, SelectedStockIdx) : StepDown(OpenTab, SelectedStockIdx);
		const int cost = PendingStep > 0 ? StepUpCost(OpenTab) : 1;
		const std::string question = made == 0
		    ? std::string(_("There is no rune that way."))
		    : fmt::format(fmt::runtime(_("Spend {:d} {:s} for 1 {:s}?")), cost,
		        _(AllItemsList[SelectedStockIdx].iName), _(AllItemsList[made].iName));
		DrawString(out, question, Panel(BoardMessageRect),
		    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
		// The TWO answers, and only those: this loop draws a YES/NO plate per entry, so anything else
		// in it renders as a "NO" box wherever its rect happens to be.
		for (const Control which : { Control::ConfirmStep, Control::CancelStep }) {
			const Rectangle rect = ControlRect(which);
			const bool confirm = which == Control::ConfirmStep;
			const Rectangle face { rect.position + (Pressed == which ? PressSink : Displacement { 0, 0 }), rect.size };
			FillRect(out, face.position.x + 1, face.position.y + 1, face.size.width - 2, face.size.height - 2, PlateFill);
			OutlineRect(out, face, FrameGold);
			DrawString(out, confirm ? _("YES") : _("NO"), face,
			    { (confirm ? UiFlags::ColorWhitegold : UiFlags::ColorRed) | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
			if (rect.contains(MousePosition))
				BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, 115);
		}
		return;
	}
	if (!Board.empty()) {
		DrawString(out, Board, Panel(BoardMessageRect),
		    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}
}

void DrawBoard(const Surface &out)
{
	const Rectangle board = Panel(BoardRect);
	OutlineRect(out, board, FrameGold);
	if (OfferOpen) {
		DrawString(out, _("Choose one:"), Rectangle { { board.position.x + 6, board.position.y + 4 }, { board.size.width - 12, BoardLineHeight } },
		    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter });
		for (int i = 0; i < OptionCount; i++) {
			const Rectangle rect = OptionRect(i);
			const bool hovered = rect.contains(MousePosition);
			const Rectangle face { rect.position + (Pressed == static_cast<Control>(static_cast<int>(Control::Option0) + i) ? PressSink : Displacement { 0, 0 }), rect.size };
			FillRect(out, face.position.x + 1, face.position.y + 1, face.size.width - 2, face.size.height - 2, PlateFill);
			OutlineRectRgb(out, face, i == 0 ? RedRgb : GreenRgb, i == 0 ? PAL16_RED + 4 : PAL16_GRAY + 6);
			const StringOrView line = PrintOracoolAffixPower(Offers[i], Bench);
			DrawString(out, i == 0 ? StrCat(_("Keep"), ": ", line.str()) : std::string(line.str()),
			    Rectangle { { face.position.x + 6, face.position.y }, { face.size.width - 12, face.size.height } },
			    { UiFlags::ColorWhite | UiFlags::FontSize12 | UiFlags::VerticalCenter });
			if (hovered)
				BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, HoverBrightenPercent);
		}
		return;
	}
	if (Board.empty())
		return;
	const std::string wrapped = WordWrapString(Board, board.size.width - 12, GameFont12);
	const int lineHeight = GetLineHeight(wrapped, GameFont12);
	const int lines = static_cast<int>(std::count(wrapped.begin(), wrapped.end(), '\n')) + 1;
	DrawString(out, wrapped, Rectangle { { board.position.x + 6, board.position.y + 8 }, { board.size.width - 12, lines * lineHeight } },
	    { UiFlags::ColorWhite | UiFlags::FontSize12 | UiFlags::AlignCenter });
}

} // namespace

bool WorkshopLockAllowsReroll(const Item &item, int row)
{
	// The first reroll settles the item on that affix; a lock past the item's affixes (a Cube recipe took some away)
	// locks nothing.
	const int locked = item._iOracoolLockedAffix;
	return locked < 0 || locked >= item._iOracoolAffixCount || locked == row;
}

int WorkshopRerollPrice(const Item &item)
{
	return RerollPrice(item);
}

int WorkshopRemovePrice(const Item &item)
{
	return RemovePrice(item);
}

// ---------------------------------------------------------------------------------------------
// The window
// ---------------------------------------------------------------------------------------------

void OpenWorkshop(WorkshopHost host)
{
	// The slot is shared with the counters, the stash and the Cube, and only one of them may have it
	// (2026-09-22). Before the flags below, so a refused close - a bench with items it cannot hand
	// back - leaves the old window up rather than being overwritten by this one's state.
	CloseOtherShopSurfaces();
	// And if one of them refused - a bench or a Cube still holding what it cannot hand back - this one does not open over
	// it (audit, 2026-09-27): the comment above promised as much, but the flags were set anyway, two windows up at once.
	if (WindowOpen || IsLevskiRoarOpen())
		return;
	Host = host;
	WindowOpen = true;
	OpenTab = TabsFor(host).front();
	SelectedRow = -1;
	OfferOpen = false;
	Pressed = Control::None;
	Board.clear();
	PlayUiSelectSound();
}

// Defined below, with the rest of the craft bench, and declared HERE at oracool scope rather than in
// the anonymous namespace above: the definition sits between the two anonymous blocks, so a
// declaration inside one of them is a different function and the link fails on it.
bool ReturnCraftGrid(Player &player);

/**
 * @brief The refused close's red line, at most once every few seconds (audit, 2026-09-27): walking away asks to close every
 * tick, and a bench that cannot be emptied said so every tick - two lines a tick, the 200-line log gone in seconds.
 */
void LogRefusedClose(const std::string &text)
{
	static uint32_t lastLogged = 0;
	const uint32_t now = SDL_GetTicks();
	if (lastLogged != 0 && now - lastLogged < 5000)
		return;
	lastLogged = now;
	LogEvent(text, UiFlags::ColorRed);
}

void CloseWorkshop()
{
	if (!WindowOpen)
		return;
	if (!ReturnBench()) {
		LogRefusedClose(std::string(_("Your pack and stash are full - the bench keeps what it holds.")));
		return;
	}
	// The craft grid too (2026-09-22). It is not a container: nothing may be left standing on it when
	// the window shuts, or a player who closed the window on three gems would have to guess where
	// they went. The window stays OPEN when there is nowhere to put them, which is the same answer
	// the bench above gives.
	if (!ReturnCraftGrid(*MyPlayer)) {
		LogRefusedClose(std::string(_("Your pack and stash are full - the bench keeps what it holds.")));
		return;
	}
	WindowOpen = false;
	OfferOpen = false;
	Pressed = Control::None;
	SelectedRow = -1;
	// The board's pick and any standing rune question go with the window (audit, 2026-09-22). They
	// were left behind: ResetWorkshopForNewGame clears them and this does not, so a question asked
	// and walked away from was still standing - and already answered "yes" once - when the window
	// next opened. The two functions have near-identical bodies, which is how one substitution
	// patched the wrong one.
	SelectedStockIdx = -1;
	PendingStep = 0;
	RecipeScroll = 0;
}

bool IsWorkshopOpen()
{
	return WindowOpen;
}

WorkshopHost CurrentWorkshopHost()
{
	return Host;
}

Rectangle GetWorkshopRect()
{
	return WindowOpen ? PageRect() : Rectangle { { 0, 0 }, { 0, 0 } };
}

bool IsPointOverWorkshop(Point position)
{
	if (!WindowOpen)
		return false;
	if (PageRect().contains(position))
		return true;
	for (int i = 0; i < static_cast<int>(TabsFor(Host).size()); i++) {
		if (TabRect(i).contains(position))
			return true;
	}
	return false;
}

void ResetWorkshopForNewGame()
{
	WindowOpen = false;
	OfferOpen = false;
	Pressed = Control::None;
	SelectedRow = -1;
	SelectedStockIdx = -1;
	PendingStep = 0;
	RecipeScroll = 0;
	Bench.clear();
	// A new game starts with an empty bench. Not RETURNED - there is no player to return it to by
	// the time this runs - so it is simply dropped, which is what the Cube's grid does too.
	for (Item &item : CraftGrid)
		item.clear();
	CraftCells = {};
	Board.clear();
}

/** @brief Where the open host's craft well is painted - low on Ogden's canvas, high in hers. */
Point CraftGridOriginFor()
{
	return Host == WorkshopHost::Mystic ? MysticCraftGridOrigin : CraftGridOrigin;
}

/** @brief Craft cell @p slot, in screen space. */
Rectangle CraftSlotRect(int slot)
{
	const Rectangle page = PageRect();
	const Point origin = CraftGridOriginFor();
	return Rectangle { { page.position.x + origin.x + (slot % CraftColumns) * CraftPitch,
	                       page.position.y + origin.y + (slot / CraftColumns) * CraftPitch },
		{ CraftCellPx, CraftCellPx } };
}

/** @brief The craft cell under @p position, or -1. */
int CraftSlotAt(Point position)
{
	if (OpenTab != Tab::Craft)
		return -1;
	for (int slot = 0; slot < CraftSlots; slot++) {
		if (CraftSlotRect(slot).contains(position))
			return slot;
	}
	return -1;
}

/** @brief The screen rect the item at @p anchor covers - one cell per cell of its footprint. */
Rectangle CraftItemRect(int anchor, Size size)
{
	const Rectangle first = CraftSlotRect(anchor);
	return Rectangle { first.position,
		{ (size.width - 1) * CraftPitch + CraftCellPx, (size.height - 1) * CraftPitch + CraftCellPx } };
}

/** @brief The ANCHOR of the item under @p position, or -1. The grid's answer to pcursinvitem. */
int CraftAnchorAt(Point position)
{
	const int cell = CraftSlotAt(position);
	if (cell < 0 || CraftCells[cell] == 0)
		return -1;
	return CraftCells[cell] - 1;
}

/** @brief Whether an item of @p size can sit with its top-left at @p anchor. */
bool CraftFitsAt(int anchor, Size size)
{
	const int column = anchor % CraftColumns;
	const int row = anchor / CraftColumns;
	if (column + size.width > CraftColumns || row + size.height > CraftRows)
		return false;
	for (int y = 0; y < size.height; y++) {
		for (int x = 0; x < size.width; x++) {
			if (CraftCells[(row + y) * CraftColumns + column + x] != 0)
				return false;
		}
	}
	return true;
}

void CraftMarkCells(int anchor, Size size, int8_t value)
{
	const int column = anchor % CraftColumns;
	const int row = anchor / CraftColumns;
	for (int y = 0; y < size.height; y++) {
		for (int x = 0; x < size.width; x++)
			CraftCells[(row + y) * CraftColumns + column + x] = value;
	}
}

/**
 * @brief Puts @p item in the grid, preferring @p preferredAnchor. True if it found room.
 *
 * A preferred anchor of -1, or one the item does not fit at, falls back to the first cell it DOES
 * fit at - so a click that lands a cell off still does what the player meant rather than nothing.
 */
bool PlaceInCraftGrid(const Item &item, int preferredAnchor)
{
	const Size size = GetInventorySize(item);
	int anchor = (preferredAnchor >= 0 && CraftFitsAt(preferredAnchor, size)) ? preferredAnchor : -1;
	for (int candidate = 0; anchor < 0 && candidate < CraftSlots; candidate++) {
		if (CraftFitsAt(candidate, size))
			anchor = candidate;
	}
	if (anchor < 0)
		return false;
	CraftGrid[anchor] = item;
	CraftMarkCells(anchor, size, static_cast<int8_t>(anchor + 1));
	return true;
}

/** @brief Lifts the item at @p anchor out of the grid, freeing every cell it covered. */
Item TakeFromCraftGrid(int anchor)
{
	Item taken = CraftGrid[anchor];
	CraftMarkCells(anchor, GetInventorySize(taken), 0);
	CraftGrid[anchor].clear();
	return taken;
}

/**
 * @brief Rebuilds the occupancy map from CraftGrid. False if something could not be placed.
 *
 * The recipes rewrite CraftGrid in place - four powders and a sword become a sword - with no idea
 * of footprints, and the result's size is not the inputs'. So after a transmute the map is
 * re-derived rather than patched: collect what is there largest first, clear, and re-place. Anchors
 * may move, which is correct; the alternative is a gem drawn over a helmet.
 *
 * LARGEST FIRST is the placement order, not merely a tidy one: a 2x3 placed after four gems may
 * find six free cells that are not six free cells in a row.
 *
 * The return value exists because placement CAN fail - twelve array slots is not twelve free cells -
 * and the Cube's own comment records what happens when that failure is discarded: "no room" becomes
 * an item that quietly stops existing. The caller undoes the whole transmute instead.
 */
bool RebuildCraftOccupancy()
{
	std::array<Item, CraftSlots> items {};
	int count = 0;
	for (const Item &slot : CraftGrid) {
		if (!slot.isEmpty())
			items[count++] = slot;
	}
	std::sort(items.begin(), items.begin() + count, [](const Item &a, const Item &b) {
		const Size sa = GetInventorySize(a);
		const Size sb = GetInventorySize(b);
		return sa.width * sa.height > sb.width * sb.height;
	});
	for (Item &slot : CraftGrid)
		slot.clear();
	CraftCells = {};
	bool allPlaced = true;
	for (int i = 0; i < count; i++) {
		if (!PlaceInCraftGrid(items[i], -1))
			allPlaced = false;
	}
	return allPlaced;
}

/**
 * @brief Hands the craft grid back. False when something had nowhere to go, so the window stays open.
 *
 * The same contract as ReturnBench: a crafting station is not a container, so nothing may be left
 * here when the window closes. Backpack first, then the stash - the order everything else in this
 * window now uses.
 */
bool ReturnCraftGrid(Player &player)
{
	bool all = true;
	for (int anchor = 0; anchor < CraftSlots; anchor++) {
		Item &item = CraftGrid[anchor];
		if (item.isEmpty())
			continue;
		if (AutoPlaceItemInInventory(player, item, true) || AutoPlaceItemInStash(player, item, true)) {
			// The cells it covered go with it. A cleared item that left its footprint behind would
			// be twelve slots' worth of invisible walls the next item could not be put down on.
			CraftMarkCells(anchor, GetInventorySize(item), 0);
			item.clear();
			continue;
		}
		all = false;
	}
	return all;
}

/** @brief This artisan's recipes, in book order - the ones HostOfRecipe hands them. */
std::vector<int> HostRecipes()
{
	const TransmuteHost host = Host == WorkshopHost::Mystic ? TransmuteHost::Barmaid : TransmuteHost::Tavern;
	std::vector<int> recipes;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		if (RecipeBelongsTo(i, host))
			recipes.push_back(i);
	}
	return recipes;
}

/**
 * @brief The Craft bench: the grid over his cube canvas, and Griswold's plate under it.
 *
 * The canvas paints the wells, so nothing is drawn for an empty cell - what goes down here is the
 * items in it, the plate, and a line saying what the grid can currently make.
 */
/** @brief Rework Charms - the one recipe of hers whose second input is an ITEM, not a material. */
constexpr int ReworkCharmsRecipe = 2;

/** @brief A charm from the pack, copied, with its id - for the recipe that needs a second one. */
bool FindPackCharm(const Player &player, Item &out, int &idx)
{
	const auto scan = [&](const Item *list, int count) {
		for (int i = 0; i < count; i++) {
			if (list[i].isEmpty() || !IsOracoolCharmIdx(list[i].IDidx))
				continue;
			out = list[i];
			idx = list[i].IDidx;
			return true;
		}
		return false;
	};
	if (scan(player.InvList, player._pNumInv))
		return true;
	for (int tab = 0; tab < Player::NumExtraInventoryTabs; tab++) {
		if (scan(player.InvTabList[tab].data(), player._pNumInvTab[tab]))
			return true;
	}
	return false;
}

/**
 * @brief Her first recipe the GRID - alone, or with what the pack can lend it - can run, or -1.
 *
 * Her well holds twelve cells since 2026-09-22, so the grid is asked FIRST and on its own, exactly
 * as Ogden's and the Cube's are. A player who lays out every input gets the plain contract every
 * other crafting station in the mod has.
 *
 * THE PACK STILL LENDS (user, 2026-09-22: "yes, pull reagents from the pack"), as a second question
 * asked only when the grid cannot answer the first. That is her window's own habit already - Imbue
 * "takes the first shard your pack can spare" - and it costs a player who does place the reagents
 * nothing at all.
 *
 * Either way the answer comes from CanCraftFromLevskiGrid rather than from this function
 * re-deciding what a recipe needs, or her bench and the monument would drift apart on what counts
 * as craftable.
 *
 * @p reagentIdx and @p reagentCount come back as what the PACK owes - zero when the grid paid for
 * itself - and @p reagentSlot as the scratch cell the loan was put in, which is the one cell the
 * caller must not copy back.
 */
int FindMysticRecipe(const Player &player, std::array<Item, CraftSlots> &scratch, int &reagentIdx, int &reagentCount, int &reagentSlot)
{
	reagentIdx = 0;
	reagentCount = 0;
	reagentSlot = -1;
	for (const int recipe : HostRecipes()) {
		// The grid as it stands. A copy, because the predicate takes a mutable grid and nothing may
		// be spent while the question is still being asked.
		scratch = CraftGrid;
		if (CanCraftFromLevskiGrid(scratch.data(), recipe))
			return recipe;
		// NEVER slot 0: that is where TransmuteLevskiGridWith leaves the result, and the caller has
		// to be able to skip the loan's cell when it writes the scratch grid back. A loan sitting in
		// the cell the result comes out of makes those two the same cell.
		int free = -1;
		for (int i = 1; i < CraftSlots && free < 0; i++) {
			if (scratch[i].isEmpty())
				free = i;
		}
		if (free < 0)
			continue; // a full grid has nowhere to put a loan
		int wantIdx = 0;
		int wantCount = 0;
		const int material = CraftingRecipeReagentItem(recipe);
		const int count = CraftingRecipeReagentCount(recipe);
		if (count > 0) {
			if (CountOwned(player, material) < count)
				continue;
			// ONE stack is enough: FindGridReagents sums stackCount across the slots it finds, so a
			// single item carrying the whole count satisfies it exactly as N separate ones would.
			InitializeItem(scratch[free], static_cast<_item_indexes>(material));
			scratch[free].setStackCount(count);
			wantIdx = material;
			wantCount = count;
		} else if (recipe == ReworkCharmsRecipe) {
			// Charms do not stack, so the second one is an item rather than a count - and it is
			// COPIED rather than rebuilt from its id, so whatever the recipe reads off it is real.
			Item charm;
			int charmIdx = 0;
			if (!FindPackCharm(player, charm, charmIdx))
				continue;
			scratch[free] = charm;
			wantIdx = charmIdx;
			wantCount = 1;
		} else {
			continue; // nothing the pack could add would change the answer
		}
		if (!CanCraftFromLevskiGrid(scratch.data(), recipe))
			continue;
		reagentIdx = wantIdx;
		reagentCount = wantCount;
		reagentSlot = free;
		return recipe;
	}
	return -1;
}

void DrawCraftPage(const Surface &out)
{
	// Named like the collection boards, and on THEIR line (user, 2026-09-22: "put a crafting title in
	// craft tab"). The shared rect is what stops the name jumping up or down as the player moves
	// between his tabs.
	DrawString(out, _("Crafting"), Panel(BoardTitleRect),
	    { UiFlags::ColorGold | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });

	// THE SLOT FACE in each of the twelve cells, under the items (user, 2026-09-22). Both hosts:
	// the well is painted into each canvas, and this is what sits inside it.
	for (int slot = 0; slot < CraftSlots; slot++)
		DrawSlotBackground(out, CraftSlotRect(slot));

	// ONE grid, both hosts (2026-09-22). Hers was a one-item bench while her canvas painted an empty
	// frame; the multi-item painting puts the same 3x4 well in that frame, so the only difference
	// left between her craft page and his is where the well is - see CraftGridOriginFor.
	//
	// AT ITS OWN SIZE, over the cells it occupies (user: "items keep original size when placed in
	// that frame"). It was fitted to a single 28px cell, which shrank a 2x3 sword to a twelfth of
	// itself and made every item on the board the same size as a gem.
	const int hoveredAnchor = CraftAnchorAt(MousePosition);
	for (int anchor = 0; anchor < CraftSlots; anchor++) {
		const Item &item = CraftGrid[anchor];
		if (item.isEmpty())
			continue;
		const Rectangle footprint = CraftItemRect(anchor, GetInventorySize(item));
		const ClxSprite sprite = GetInvItemSprite(item._iCurs + CURSOR_FIRSTITEM);
		// Centred in the footprint and drawn by the inventory's own routine, so the grey for gear
		// the character cannot use, the red X on a broken item and the stack count all come with it.
		const Point topLeft { footprint.position.x + (footprint.size.width - static_cast<int>(sprite.width())) / 2,
			footprint.position.y + (footprint.size.height - static_cast<int>(sprite.height())) / 2 };
		DrawItem(item, out, { topLeft.x, topLeft.y + static_cast<int>(sprite.height()) - 1 }, sprite);
		if (anchor == hoveredAnchor)
			DrawHoverOutline(out, footprint);
	}

	// Griswold's Refresh plate as Transmute, as on Levski's Cube.
	const Rectangle rect = ControlRect(Control::Transmute);
	bool ready = false;
	if (Host == WorkshopHost::Mystic) {
		std::array<Item, CraftSlots> scratch {};
		int idx = 0;
		int count = 0;
		int loanSlot = -1;
		ready = FindMysticRecipe(*MyPlayer, scratch, idx, count, loanSlot) >= 0;
	} else {
		ready = FirstReadyLevskiRecipeFor(CraftGrid.data(), TransmuteHost::Tavern) >= 0;
	}
	const Rectangle face { rect.position + (Pressed == Control::Transmute ? PressSink : Displacement { 0, 0 }), rect.size };
	if (GetLoosePngSize(BoardButtonFrameAsset).width > 0)
		DrawLoosePng(out, BoardButtonFrameAsset, face.position);
	else
		DrawOrnateBorder(out, face);
	if (const Size glyph = GetLoosePngSize(CraftTransmuteGlyphAsset); glyph.width > 0) {
		DrawLoosePng(out, CraftTransmuteGlyphAsset,
		    { face.position.x + (face.size.width - glyph.width) / 2,
		        face.position.y + (face.size.height - glyph.height) / 2 });
	}
	if (!ready) {
		TintRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height,
		    0xFFFFFFu, /*brightnessPercent=*/70, /*floorPercent=*/0, PAL16_GRAY);
	} else if (rect.contains(MousePosition)) {
		BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, HoverBrightenPercent);
	}
}

/**
 * @brief His recipe page: the list inside the painted frame, on a dark layer.
 *
 * Two lines a recipe - the name in gold, what it takes in white - because a name alone ("Punch
 * Sockets") does not say what to put on the bench. The dark layer is the same two passes Levski's
 * recipe page uses, for the same reason: one left the names competing with the floor behind them.
 */
/**
 * @brief Where this artisan's recipe list is drawn.
 *
 * OGDEN has a painted recipe frame and the list goes inside it. GILLIAN does not - her canvas is a
 * portrait with open floor and no frame at all - so hers is drawn on the floor her window already
 * uses for text, the same rect her offers menu occupies, ending clear of the message line at 602.
 *
 * The dark layer is what makes either readable, so the absence of a painted frame costs her nothing
 * but the moulding. Inventing a frame in code for her would be the one thing the other windows have
 * all stopped doing.
 */
/**
 * @brief The big frame's opening - x 29..310, y 299..618 - on every page of either window.
 *
 * ONE rect since 2026-09-22. It was the recipe page's alone, with Gillian falling back to open floor
 * when her recipe painting had not landed; her Reroll, Imbue and Craft tabs now stand on the same
 * frame, and all four of Ogden's already did.
 */
Rectangle FrameOpeningRect()
{
	const Rectangle page = PageRect();
	return Rectangle { page.position + Displacement { RecipeOpeningLeft, RecipeOpeningTop },
		{ RecipeOpeningRight - RecipeOpeningLeft + 1, RecipeOpeningBottom - RecipeOpeningTop + 1 } };
}

Rectangle RecipeOpeningRect()
{
	return FrameOpeningRect();
}

void DrawRecipesPage(const Surface &out)
{
	const Rectangle opening = RecipeOpeningRect();
	DrawThemedFill(out, opening, 2);
	// The SHARED list (2026-09-22): the Cube's page, Ogden's and Gillian's all draw through this one
	// function, so "behave identically" is a property of the code rather than three loops kept in step.
	DrawRecipeList(out, opening, HostRecipes(), RecipeScroll);
}

void DrawWorkshop(const Surface &out)
{
	if (!WindowOpen)
		return;
	const Rectangle page = PageRect();
	// The user's painted canvas, or the shared 340x720 side panel until it lands.
	// Ogden's own table when it is installed (2026-09-21); the shared workshop canvas otherwise, and
	// the side panel under that - so a build short of any of them still draws a whole window.
	// ONE CANVAS PER PAGE for Ogden (2026-09-22): his collection tabs wear the table with the board's
	// frame, his Craft tab the cube page, his Recipes tab the recipe frame. Each falls back to the
	// one before it, so a build short of a file still draws a whole window rather than a blank tab.
	const char *canvas = MysticCanvasAsset;
	if (Host == WorkshopHost::Mystic) {
		// ONE CANVAS PER PAGE for her too (user, 2026-09-22). Her Reroll and Imbue tabs hold one item
		// and wear the empty small frame; her Craft tab holds a grid and wears the well painted into
		// that same frame; her Recipes tab wears the frame alone. Each falls back to the one before
		// it, so a build short of a file still draws a whole window rather than a blank tab.
		if (OpenTab == Tab::Recipes && GetLoosePngSize(GillianRecipesCanvasAsset).width > 0)
			canvas = GillianRecipesCanvasAsset;
		else if (OpenTab == Tab::Craft && GetLoosePngSize(GillianCraftCanvasAsset).width > 0)
			canvas = GillianCraftCanvasAsset;
		else if (GetLoosePngSize(GillianFrameCanvasAsset).width > 0)
			canvas = GillianFrameCanvasAsset;
	} else {
		canvas = GetLoosePngSize(OgdenCanvasAsset).width > 0 ? OgdenCanvasAsset : JewellerCanvasAsset;
		if (OpenTab == Tab::Craft && GetLoosePngSize(OgdenCubeCanvasAsset).width > 0)
			canvas = OgdenCubeCanvasAsset;
		else if (OpenTab == Tab::Recipes && GetLoosePngSize(OgdenRecipesCanvasAsset).width > 0)
			canvas = OgdenRecipesCanvasAsset;
	}
	if (GetLoosePngSize(canvas).width > 0) {
		DrawLoosePng(out, canvas, page.position);
	} else if (HasSidePanelArt()) {
		DrawSidePanelArt(out, page.position);
	} else {
		DrawThemedFill(out, page);
		DrawOrnateBorder(out, page);
	}
	// NO title over a portrait (audit, 2026-09-22). The stash, Griswold's tabs and all four vendor
	// canvases lost theirs on 2026-09-21 for the same reason: the painting has the man in it, and a
	// name printed over his own room says nothing the picture does not. Ogden's window was the last
	// one still doing it, and only because his canvas arrived a day later than the rule.
	//
	// The placeholder canvases KEEP their title - there the interior is drawn in code and the band
	// is empty stone, so the window would otherwise have nothing naming it at all.
	// Any PAINTED canvas loses the title; only the shared code-drawn placeholder keeps one, because
	// there the band is empty stone and the window would have nothing naming it at all.
	//
	// Gillian was the last window in the game still printing a name over a portrait (2026-09-22).
	// The stash, Griswold's tabs, all four vendors and Ogden lost theirs on 2026-09-21 for the same
	// reason - her canvas has her standing in her own workshop, and "Mystic Workshop" over it says
	// nothing the picture does not. Asked of the CANVAS rather than the host, so a future painting
	// gets the rule for free.
	const bool painted = canvas != JewellerCanvasAsset && GetLoosePngSize(canvas).width > 0;
	if (!painted) {
		DrawString(out, Host == WorkshopHost::Mystic ? _("Mystic Workshop") : _("Jeweller's Tables"), Panel(TitleRect),
		    { UiFlags::ColorGold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}
	DrawTabColumn(out);
	// THE DARK LAYER under everything her big frame carries (2026-09-22). Her list and her message
	// board used to stand on the window's own stone; they stand inside a painting now, and a painting
	// is not a background text can be read against. The same two passes her recipe page has always
	// used, laid once for whatever the page puts on it.
	//
	// Her Recipes tab lays its own, because it lays it under a list that scrolls.
	if (Host == WorkshopHost::Mystic && OpenTab != Tab::Recipes)
		DrawThemedFill(out, FrameOpeningRect(), 2);
	// The bench belongs to the MYSTIC's two tabs and nowhere else (2026-09-22). Her Craft tab had one
	// while her canvas painted an empty frame; the well painted into that frame took its place.
	if (OpenTab == Tab::Reroll || OpenTab == Tab::Imbue) {
		// And a dark layer in the small frame too (user, 2026-09-23 dev note: "on reroll and imbue
		// tab of gillian put a dark transparent lay[er] in the lit little frame"). The painting lights
		// that opening, and an item laid on a lit ground loses its edges; the same two passes the
		// big frame takes, under the item.
		DrawThemedFill(out, BenchSlotRect(), 2);
		DrawBench(out);
	}

	if (IsStockTab(OpenTab)) {
		// The board, and the two arrow plates on the row above it (2026-09-21). The list this
		// replaced showed only what the pack held; the board shows the whole collection.
		DrawCollectionBoard(out);
		const int picked = SelectedStockIdx;
		const int held = picked > 0 ? CountOwned(*MyPlayer, picked) : 0;
		const int up = picked > 0 ? StepUp(OpenTab, picked) : 0;
		const int down = picked > 0 ? StepDown(OpenTab, picked) : 0;
		DrawBoardArrow(out, Control::Upgrade, up != 0 && held >= StepUpCost(OpenTab));
		DrawBoardArrow(out, Control::Downgrade, down != 0 && held >= 1);
	} else if (OpenTab == Tab::Craft) {
		DrawCraftPage(out);
	} else if (OpenTab == Tab::Recipes) {
		DrawRecipesPage(out);
	} else if (OpenTab == Tab::Reroll) {
		DrawRerollList(out);
		const bool ready = !Bench.isEmpty() && Bench._iOracoolAffixCount > 0 && SelectedRow >= 0;
		DrawServiceIcon(out, Control::Reroll, RerollGlyphAsset, ready, Bench.isEmpty() ? 0 : RerollPrice(Bench));
	} else {
		DrawImbueList(out);
		// Imbue takes the first shard the pack can spare and costs nothing, so its line reads "Free"
		// rather than a zero - a zero beside two real prices looks like a bug in the pricing.
		DrawServiceIcon(out, Control::Imbue, ImbueGlyphAsset, !Bench.isEmpty(), 0);
		DrawServiceIcon(out, Control::Remove, RemoveGlyphAsset, SelectedRow >= 0, Bench.isEmpty() ? 0 : RemovePrice(Bench));
		DrawServiceIcon(out, Control::Cleanse, CleanseGlyphAsset, !Bench.isEmpty(), Bench.isEmpty() ? 0 : CleansePrice(Bench));
	}

	// GOLD ONLY WHERE GOLD IS SPENT (user, 2026-09-22: "If there is no gold cost to ogden services, i
	// dont see a point of showing the gold counter").
	//
	// They are right, and the code agrees: the only prices in this window are RerollPrice,
	// RemovePrice and CleansePrice, all three the MYSTIC's. Ogden's ladders and his bench take
	// materials and give items back; no path through his tabs reads the player's purse. A readout
	// that never changes is furniture.
	//
	// Asked of the HOST rather than the tab, because it is the artisan who charges: every one of her
	// pages can spend gold and none of his can.
	if (Host == WorkshopHost::Mystic) {
		if (GetLoosePngSize(BoardGoldIconAsset).width > 0)
			DrawLoosePng(out, BoardGoldIconAsset, page.position + Displacement { BoardGoldIconAt.x, BoardGoldIconAt.y });
		DrawString(out, FormatInteger(static_cast<int>(TotalPlayerGold())), Panel(BoardGoldCountRect),
		    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}
	// The big message panel belongs to the MYSTIC's two tabs, which is where its offers menu lives.
	// Named rather than written as "not a stock tab" (audit, 2026-09-22): its rect is y 336..612 and
	// it OUTLINES itself before any early return, so on every other page it drew a gold box through
	// whatever was there - the collection board at 439..590, the craft grid at 416..541, the recipe
	// list at 299..618. That is the third time this session a gate phrased as "not the other thing"
	// broke the moment a third thing existed; this one says which tabs it means.
	if (Host == WorkshopHost::Mystic && OpenTab != Tab::Recipes) {
		// Her Craft tab joins the two that always had it (2026-09-22): its big frame would otherwise
		// stand empty under the well, and what the bench refuses - "Nothing on the bench makes
		// anything" - has to be readable somewhere.
		DrawBoard(out);
	} else {
		DrawBoardMessage(out);
	}
	DrawWindowCloseButtonAt(out, Panel(CloseRect));

	// The hover sound, once as the cursor arrives on a control.
	Control hoveredNow = Control::None;
	for (const Control control : { Control::Tab0, Control::Tab1, Control::Tab2, Control::Tab3, Control::Tab4, Control::Reroll,
	         Control::Imbue, Control::Remove, Control::Cleanse, Control::Upgrade, Control::Downgrade,
	         Control::Option0, Control::Option1, Control::Option2, Control::Option3,
	         Control::ConfirmStep, Control::CancelStep, Control::Transmute }) {
		const Rectangle rect = ControlRect(control);
		if (rect.size.width > 0 && rect.contains(MousePosition)) {
			hoveredNow = control;
			break;
		}
	}
	if (hoveredNow != Control::None && hoveredNow != LastHovered)
		PlayUiMoveSound();
	LastHovered = hoveredNow;
}

void UpdateWorkshopState()
{
	if (!WindowOpen)
		return;
	// THE VENDORS' OWN WALK-AWAY (user, 2026-09-22: "make sure gillians tabs auto close on walkaway.
	// They should behave like the rest tabs of vendors - like shop i believe you described it not
	// like windows").
	//
	// They are right about the distinction and it was a real gap: every shop tab has closed at three
	// tiles since 2026-09-21, and these two windows had no distance check at all - walk to the other
	// end of town and Gillian's bench was still open over the world.
	//
	// THREE tiles, the same constant the counters use, read from the towner rather than from where
	// the window was opened: the artisan is the place, and standing next to her is what keeps her
	// page up.
	const Towner *towner = GetTowner(Host == WorkshopHost::Mystic ? TOWN_BMAID : TOWN_TAVERN);
	if (towner == nullptr)
		return;
	constexpr int WalkAwayTiles = 3;
	if (MyPlayer->position.tile.WalkingDistance(towner->position) <= WalkAwayTiles)
		return;
	// CloseWorkshop may refuse - a bench or craft grid with nowhere to give its items back says so
	// and stays up. That is the same answer it gives the close button, and it is deliberate: walking
	// away must not destroy what is sitting on the bench.
	CloseWorkshop();
}

bool HandleWorkshopScroll(int notches)
{
	// Only the recipe page scrolls, and only while it is the page on screen - so the wheel keeps
	// zooming the dungeon everywhere else in this window, which is what it did before.
	if (!WindowOpen || OpenTab != Tab::Recipes)
		return false;
	const int maxScroll = RecipeListMaxScroll(RecipeOpeningRect(), HostRecipes());
	if (maxScroll <= 0)
		return false; // nothing to scroll: let the wheel fall through rather than swallow it
	constexpr int PixelsPerNotch = 20;
	RecipeScroll = std::clamp(RecipeScroll - notches * PixelsPerNotch, 0, maxScroll);
	return true;
}

bool SetWorkshopHoverInfoString()
{
	if (!WindowOpen)
		return false;
	// The bench, on the two tabs that have one. Asked of the TAB as well as the rect, because the
	// craft well now shares that rect: without the gate, an item left on the bench would name itself
	// under a cell holding something else.
	if ((OpenTab == Tab::Reroll || OpenTab == Tab::Imbue)
	    && BenchSlotRect().contains(MousePosition) && !Bench.isEmpty()) {
		SetPanelString(Bench.getName(), Bench.getTextColor());
		const std::string count = ImbueCountLine(Bench);
		if (!count.empty())
			AddPanelString(count, UiFlags::ColorWhite);
		return true;
	}
	// The craft cells, both hosts. They had no hover text at all, which was survivable while the only
	// grid was Ogden's low-painted well; hers stands in the frame her bench used to, where the player
	// has every reason to expect a name.
	if (const int anchor = CraftAnchorAt(MousePosition); anchor >= 0) {
		SetPanelString(CraftGrid[anchor].getName(), CraftGrid[anchor].getTextColor());
		return true;
	}
	if (ControlRect(Control::Reroll).contains(MousePosition)) {
		SetPanelString(_("Reroll the chosen affix"), UiFlags::ColorWhitegold);
		AddPanelString(_("She offers the affix as it stands and three others; you choose one."), UiFlags::ColorWhite);
		AddPanelString(_("The first reroll settles which affix this item may ever reroll."), UiFlags::ColorWhite);
		return true;
	}
	if (ControlRect(Control::Imbue).contains(MousePosition)) {
		SetPanelString(_("Imbue"), UiFlags::ColorWhitegold);
		AddPanelString(_("Takes the first shard your pack can spare and works it in. Free."), UiFlags::ColorWhite);
		return true;
	}
	if (ControlRect(Control::Remove).contains(MousePosition)) {
		SetPanelString(_("Remove one imbuement"), UiFlags::ColorWhitegold);
		AddPanelString(_("The shard is destroyed; the rest stay. The price rises each time."), UiFlags::ColorWhite);
		return true;
	}
	if (ControlRect(Control::Cleanse).contains(MousePosition)) {
		SetPanelString(_("Cleanse the item"), UiFlags::ColorWhitegold);
		AddPanelString(_("Every shard off at once, and none comes back."), UiFlags::ColorWhite);
		return true;
	}
	// The board and its two arrows (audit, 2026-09-22). They had NO hover text at all: the wide
	// buttons they replaced carried "UPGRADE 3 -> 1" on their faces, so the ratio was readable
	// without asking. A 34px glyph plate says nothing, which left the price of a step - and, on the
	// runes, the only warning before a Zod is spent - nowhere on the screen.
	if (IsStockTab(OpenTab)) {
		for (const Control which : { Control::Upgrade, Control::Downgrade }) {
			if (!ControlRect(which).contains(MousePosition))
				continue;
			const bool up = which == Control::Upgrade;
			SetPanelString(up ? _("Upgrade") : _("Downgrade"), UiFlags::ColorWhitegold);
			AddPanelString(up
			        ? fmt::format(fmt::runtime(_("{:d} of the chosen kind for 1 of the next.")), StepUpCost(OpenTab))
			        : std::string(_("1 of the chosen kind for 1 of the one below.")),
			    UiFlags::ColorWhite);
			if (!up)
				AddPanelString(_("The step down is a price, not a refund."), UiFlags::ColorWhite);
			if (SelectedStockIdx <= 0)
				AddPanelString(_("Choose a kind from the board first."), UiFlags::ColorRed);
			return true;
		}
		const int slot = BoardSlotAt(MousePosition);
		if (slot >= 0) {
			const int idx = BoardSlotItem(OpenTab, slot);
			if (idx != 0) {
				const int held = CountOwned(*MyPlayer, idx);
				SetPanelString(_(AllItemsList[idx].iName), UiFlags::ColorWhitegold);
				AddPanelString(held > 0
				        ? fmt::format(fmt::runtime(_("You carry {:d}.")), held)
				        : std::string(_("You carry none of these.")),
				    held > 0 ? UiFlags::ColorWhite : UiFlags::ColorRed);
				return true;
			}
		}
	}
	return false;
}

namespace {

/** @brief Rolls the menu: the affix as it stands, then three the pool offers at this item's level. */
void RollOffers(int slot)
{
	Offers[0] = Bench._iOracoolAffixes[slot];
	std::array<item_effect_type, OptionCount + Item::MaxOracoolAffixes> exclude {};
	int excludeCount = 0;
	for (int i = 0; i < Bench._iOracoolAffixCount; i++) {
		if (i != slot)
			exclude[excludeCount++] = Bench._iOracoolAffixes[i].type;
	}
	int rolled = 1;
	for (int attempt = 0; attempt < 24 && rolled < OptionCount; attempt++) {
		OracoolAffix drawn;
		if (!RollOracoolAffixFor(*MyPlayer, Bench, drawn, exclude.data(), excludeCount))
			break;
		bool already = false;
		for (int i = 1; i < rolled; i++) {
			if (Offers[i].type == drawn.type && Offers[i].param1 == drawn.param1)
				already = true;
		}
		if (already)
			continue;
		Offers[rolled++] = drawn;
	}
	// A pool too thin to fill the menu simply offers fewer: the rest repeat the affix as it stands,
	// which reads as "nothing better was on the wheel" rather than as an empty row.
	for (int i = rolled; i < OptionCount; i++)
		Offers[i] = Offers[0];
	OfferSlot = slot;
	OfferOpen = true;
}

/** @brief The release's work for one control. */
/**
 * @brief Takes the selected kind one rung up or down the ladder.
 *
 * ONE for one either way DOWN (user, 2026-09-21: "runes downgrade ratio 1:1. you dont get two of the
 * lower. sorry. price of conversion"). It gave TWO before, and the button said so - "DOWNGRADE 1 ->
 * 2" - which made the ladder a free pump on the runes: two down and one up is a net gain when up
 * costs two. At 1:1 the step down is what the user calls it, a price.
 *
 * Up still costs what the ladder asks: three gems or jewels, two runes.
 *
 * The sounds are the user's: a gem hitting the floor for the step up, and the glass break for the
 * step down, which destroys what it takes. IS_SHATTER is sfx\misc\shatter.wav - in diabdat all
 * along, never referenced by this engine until 2026-09-21.
 */
void RunStep(bool up)
{
	Player &player = *MyPlayer;
	const int idx = SelectedStockIdx;
	if (idx <= 0)
		return;
	const int made = up ? StepUp(OpenTab, idx) : StepDown(OpenTab, idx);
	if (made == 0) {
		SetBoard(std::string(up ? _("Nothing stands above this one.") : _("Nothing stands below this one.")));
		return;
	}
	const int cost = up ? StepUpCost(OpenTab) : 1;
	if (CountOwned(player, idx) < cost) {
		SetBoard(fmt::format(fmt::runtime(_("You need {:d} of those.")), cost));
		return;
	}
	TakeOwned(player, idx, cost);
	const Landing where = GiveOwned(player, made);
	CalcPlrInv(player, true);
	// WHERE IT WENT, under the grid (user, 2026-09-22: "Under the grid frame is where you will inform
	// about where the new item landed. Sent to Backpack, Sent to Stash"). The name comes with it: the
	// board has thirty-five cells and "Sent to Stash" alone does not say what was sent.
	// Explicitly constructed: _() hands back a string_view here, which fmt takes but std::string will
	// not implicitly adopt.
	const std::string name { _(AllItemsList[made].iName) };
	switch (where) {
	case Landing::Backpack:
		SetBoard(fmt::format(fmt::runtime(_("{:s} - sent to Backpack")), name));
		break;
	case Landing::Stash:
		SetBoard(fmt::format(fmt::runtime(_("{:s} - sent to Stash")), name));
		break;
	case Landing::Ground:
		SetBoard(fmt::format(fmt::runtime(_("{:s} - both full, dropped at your feet")), name));
		break;
	case Landing::Nowhere:
		// Nothing took it, not even the floor. Said plainly rather than silently swallowed.
		SetBoard(fmt::format(fmt::runtime(_("{:s} - nowhere to put it")), name));
		break;
	}
	PlaySFX(up ? IS_FROCK : IS_SHATTER);
}

void RunControl(Control control)
{
	switch (control) {
	case Control::Tab0:
	case Control::Tab1:
	case Control::Tab2:
	case Control::Tab3:
	case Control::Tab4: {
		const std::vector<Tab> tabs = TabsFor(Host);
		const int slot = static_cast<int>(control) - static_cast<int>(Control::Tab0);
		if (slot >= static_cast<int>(tabs.size()) || tabs[slot] == OpenTab)
			break;
		// OGDEN'S RECIPES STAY HERE (user, 2026-09-22: "Tab recipes must not lead to Levski Cube -
		// must lead to Ogden's recipe canvas and list recipes"). The tab used to close this window and
		// open the Levski page, which is a different window with a different painting and its own tab
		// column - leaving the player two clicks from the board they started on.
		//
		// GILLIAN'S STAY TOO, since 2026-09-22. She kept the hand-off only because she had no page of
		// her own; she has one now, drawn on her painting's floor, so no tab in this window leads out
		// of it any more. That was the session's firmest rule and she was the last exception to it.
		// THE BENCH DOES NOT TRAVEL (2026-09-22). Only Reroll and Imbue draw it, so an item left on
		// it while the player moved to Craft or Recipes would be invisible until the window closed -
		// and on her Craft tab it would be invisible UNDER the well that now shares its frame. It
		// goes back to the pack on the way out, and when there is nowhere to put it the tab does not
		// change, which is the answer CloseWorkshop gives to the same question.
		if (tabs[slot] != Tab::Reroll && tabs[slot] != Tab::Imbue && !Bench.isEmpty()) {
			if (!ReturnBench()) {
				SetBoard(std::string(_("Your pack and stash are full - the bench keeps what it holds.")));
				break;
			}
		}
		OpenTab = tabs[slot];
		SelectedRow = -1;
		SelectedStockIdx = -1;
		PendingStep = 0;
		RecipeScroll = 0;
		OfferOpen = false;
		Board.clear();
		break;
	}
	case Control::Close:
		CloseWorkshop();
		break;
	case Control::Reroll: {
		std::string why;
		if (!BenchIsWorkable(why)) {
			SetBoard(why);
			break;
		}
		if (Bench._iOracoolAffixCount == 0) {
			SetBoard(std::string(_("This item has no affix to reroll.")));
			break;
		}
		if (SelectedRow < 0 || SelectedRow >= Bench._iOracoolAffixCount) {
			SetBoard(std::string(_("Choose an affix from the list first.")));
			break;
		}
		// A lock past the item's affixes (a Cube recipe took some away since) locks nothing.
		if (!WorkshopLockAllowsReroll(Bench, SelectedRow)) {
			SetBoard(std::string(_("This item is settled: only the affix she first worked can be rerolled.")));
			break;
		}
		const int price = RerollPrice(Bench);
		if (static_cast<int>(TotalPlayerGold()) < price) {
			SetBoard(std::string(_("You do not have enough gold.")));
			break;
		}
		TakePlrsMoney(price);
		Bench._iOracoolLockedAffix = static_cast<int8_t>(SelectedRow);
		Bench._iOracoolRerolls = static_cast<uint8_t>(std::min<int>(Item::MaxWorkshopAttempts, Bench._iOracoolRerolls + 1));
		RollOffers(SelectedRow);
		break;
	}
	case Control::Option0:
	case Control::Option1:
	case Control::Option2:
	case Control::Option3: {
		if (!OfferOpen)
			break;
		const int pick = static_cast<int>(control) - static_cast<int>(Control::Option0);
		OfferOpen = false;
		if (pick == 0) {
			SetBoard(std::string(_("Kept as it was.")));
			break;
		}
		std::array<OracoolAffix, Item::MaxOracoolAffixes> affixes {};
		int count = 0;
		for (int i = 0; i < Bench._iOracoolAffixCount; i++)
			affixes[count++] = i == OfferSlot ? Offers[pick] : Bench._iOracoolAffixes[i];
		if (!RebuildOracoolItemWithAffixes(*MyPlayer, Bench, affixes.data(), count)) {
			SetBoard(std::string(_("The work would not take.")));
			break;
		}
		SetBoard(StrCat(_("Reworked"), ": ", std::string(PrintOracoolAffixPower(Offers[pick], Bench).str())));
		if (!PlayUiEventSound(UiEventSound::ShardImbue))
			PlayUiSelectSound();
		break;
	}
	case Control::Imbue: {
		std::string why;
		if (!BenchIsWorkable(why)) {
			SetBoard(why);
			break;
		}
		if (!CanReceiveShard(Bench)) {
			SetBoard(std::string(_("This item will take no more shards.")));
			break;
		}
		// The first shard in the pack this item can still take.
		Player &player = *MyPlayer;
		for (int i = 0; i < player._pNumInv; i++) {
			const ShardDefinition *def = FindShardByItem(player.InvList[i].IDidx);
			if (def == nullptr || !CanReceiveShardKind(Bench, def->kind))
				continue;
			if (!TryImbue(player, Bench, player.InvList[i]))
				continue;
			const std::string name { std::string(_(def->name)) };
			// ONE shard: they stack, and removing the item took the whole stack for the one worked in (audit, 2026-09-27).
			if (player.InvList[i].stackCount() > 1)
				player.InvList[i].setStackCount(player.InvList[i].stackCount() - 1);
			else
				player.RemoveInvItem(i, false);
			CalcPlrInv(player, true);
			SetBoard(StrCat(name, " ", _("worked in.")));
			if (!PlayUiEventSound(UiEventSound::ShardImbue))
				PlayUiSelectSound();
			return;
		}
		SetBoard(std::string(_("No shard in your pack fits this item.")));
		break;
	}
	case Control::Remove: {
		std::string why;
		if (!BenchIsWorkable(why)) {
			SetBoard(why);
			break;
		}
		const ImbuementLedger ledger = CaptureImbuements(Bench);
		if (SelectedRow < 0 || SelectedRow >= ledger.count) {
			SetBoard(std::string(_("Choose an imbuement from the list first.")));
			break;
		}
		const int price = RemovePrice(Bench);
		if (static_cast<int>(TotalPlayerGold()) < price) {
			SetBoard(std::string(_("You do not have enough gold.")));
			break;
		}
		TakePlrsMoney(price);
		Bench._iOracoolRemovals = static_cast<uint8_t>(std::min<int>(Item::MaxWorkshopAttempts, Bench._iOracoolRemovals + 1));
		const ShardDefinition &def = ShardDef(static_cast<ShardKind>(ledger.kinds[SelectedRow]));
		ImbuementLedger kept;
		for (int i = 0; i < ledger.count; i++) {
			if (i != SelectedRow)
				kept.kinds[kept.count++] = ledger.kinds[i];
		}
		// Current durability kept as it was (audit, 2026-09-27): the strip clamps it to the lowered maximum and the restore
		// adds Tempering back to BOTH, so taking out any shard repaired a worn item - an ethereal one too.
		const int durabilityBefore = Bench._iDurability;
		StripImbuements(Bench);
		RestoreImbuements(Bench, kept);
		Bench._iDurability = std::min(durabilityBefore, Bench._iMaxDur);
		SelectedRow = -1;
		SetBoard(StrCat(_(def.name), " ", _("drawn out and destroyed.")));
		if (!PlayUiEventSound(UiEventSound::ShardImbue))
			PlayUiSelectSound();
		break;
	}
	case Control::Cleanse: {
		std::string why;
		if (!BenchIsWorkable(why)) {
			SetBoard(why);
			break;
		}
		const ImbuementLedger ledger = CaptureImbuements(Bench);
		if (ledger.count == 0) {
			SetBoard(std::string(_("Nothing to cleanse.")));
			break;
		}
		const int price = CleansePrice(Bench);
		if (static_cast<int>(TotalPlayerGold()) < price) {
			SetBoard(std::string(_("You do not have enough gold.")));
			break;
		}
		TakePlrsMoney(price);
		StripImbuements(Bench);
		SelectedRow = -1;
		SetBoard(StrCat(_("Cleansed"), ": ", static_cast<int>(ledger.count), " ", _("shards gone.")));
		if (!PlayUiEventSound(UiEventSound::ShardImbue))
			PlayUiSelectSound();
		break;
	}
	case Control::Upgrade:
	case Control::Downgrade: {
		const bool up = control == Control::Upgrade;
		if (SelectedStockIdx <= 0) {
			SetBoard(std::string(_("Choose a kind from the board first.")));
			break;
		}
		// The runes ASK; the gems and jewels act. See PendingStep.
		if (OpenTab == Tab::Runes) {
			PendingStep = up ? 1 : -1;
			PlayUiSelectSound();
			break;
		}
		RunStep(up);
		break;
	}
	case Control::ConfirmStep: {
		const int step = PendingStep;
		PendingStep = 0;
		if (step != 0)
			RunStep(step > 0);
		break;
	}
	case Control::CancelStep:
		// "Let me think a bit more" - the question goes and nothing else changes, the selection
		// included, so the player can answer it again without hunting for the rune a second time.
		PendingStep = 0;
		PlayUiSelectSound();
		break;
	case Control::Transmute: {
		// THE WHOLE BENCH, BEFORE (2026-09-22). A recipe rewrites the grid with no idea of
		// footprints - four powders and a sword become a sword - so the occupancy map has to be
		// re-derived afterwards, and re-deriving it can fail: twelve array slots are not twelve free
		// cells once items take more than one each. The snapshot is what lets that failure undo the
		// transmute instead of losing whatever could not be laid out. The Cube learned this the hard
		// way; see RebuildCraftOccupancy.
		const std::array<Item, CraftSlots> benchBefore = CraftGrid;
		const std::array<int8_t, CraftSlots> cellsBefore = CraftCells;
		const auto restoreBench = [&benchBefore, &cellsBefore]() {
			CraftGrid = benchBefore;
			CraftCells = cellsBefore;
		};
		// HIS recipes only: FirstReadyLevskiRecipeFor is asked for TransmuteHost::Tavern, so a grid
		// that happens to satisfy one of Griswold's or Gillian's does nothing here. The bench is
		// Ogden's, and a recipe running at the wrong artisan's window would be a bug that looked like
		// a feature.
		if (Host == WorkshopHost::Mystic) {
			Player &player = *MyPlayer;
			std::array<Item, CraftSlots> scratch {};
			int reagentIdx = 0;
			int reagentCount = 0;
			int reagentSlot = -1;
			const int mine = FindMysticRecipe(player, scratch, reagentIdx, reagentCount, reagentSlot);
			if (mine < 0) {
				SetBoard(std::string(_("Nothing on the bench makes anything.")));
				break;
			}
			// The loan as it went in, to see afterwards how much of it the recipe really used.
			const Item loan = reagentSlot >= 0 ? scratch[reagentSlot] : Item {};
			const std::string made = TransmuteLevskiGridWith(scratch.data(), mine);
			if (IsTransmuteRefusal(made)) {
				SetBoard(made);
				break;
			}
			// What the loan's cell holds now (audit, 2026-09-27). The recipe may have spent the well's own units first
			// and only part of the loan - the pack paid the whole loan anyway - or spent it all and put its RESULT in
			// that cell, which the writeback below skipped, losing what was made. So: the loan still there means only
			// the difference was spent and the rest never left the pack; anything else there is the recipe's, and goes
			// onto the well.
			const bool loanRemains = reagentSlot >= 0 && !scratch[reagentSlot].isEmpty()
			    && scratch[reagentSlot].IDidx == loan.IDidx && scratch[reagentSlot]._iSeed == loan._iSeed;
			const int loanSpent = reagentSlot < 0 ? 0
			    : loanRemains                   ? std::max(0, loan.stackCount() - scratch[reagentSlot].stackCount())
			                                    : reagentCount;
			// The scratch grid was a COPY of her well, made to ask the predicate its question, and
			// any reagent the PACK lent it was a copy too. Both are settled HERE, after the work
			// succeeded and never before, so a refusal cannot cost the player anything:
			//  - the copy is written back over the real well, which is what spends what was in it;
			//  - the loan's cell is skipped, because that item was never on the well;
			//  - and the pack pays for the loan exactly as the test was passed.
			for (int i = 0; i < CraftSlots; i++) {
				if (i != reagentSlot || !loanRemains)
					CraftGrid[i] = scratch[i];
			}
			if (!RebuildCraftOccupancy()) {
				restoreBench();
				SetBoard(std::string(_("There is no room on the bench for what that would make.")));
				break;
			}
			if (loanSpent > 0)
				TakeOwned(player, reagentIdx, loanSpent);
			CalcPlrInv(player, true);
			SetBoard(made);
			if (!PlayUiEventSound(UiEventSound::Transmute))
				PlayUiSelectSound();
			break;
		}
		const int recipe = FirstReadyLevskiRecipeFor(CraftGrid.data(), TransmuteHost::Tavern);
		if (recipe < 0) {
			SetBoard(std::string(_("Nothing on the bench makes anything.")));
			break;
		}
		const std::string result = TransmuteLevskiGridWith(CraftGrid.data(), recipe);
		if (IsTransmuteRefusal(result)) {
			// A refusal leaves the grid as it was, so the map is still right; restore anyway rather
			// than trust that, because "leaves it as it was" is a property of another file.
			restoreBench();
			SetBoard(result);
			break;
		}
		if (!RebuildCraftOccupancy()) {
			restoreBench();
			SetBoard(std::string(_("There is no room on the bench for what that would make.")));
			break;
		}
		SetBoard(result);
		if (!PlayUiEventSound(UiEventSound::Transmute))
			PlayUiSelectSound();
		break;
	}
	case Control::None:
		break;
	}
}

} // namespace

bool CheckWorkshopClick(Point position)
{
	if (!WindowOpen)
		return false;
	if (!IsPointOverWorkshop(position))
		return false;

	// Every control presses here and RUNS on the release - the standing rule since v1.12.102.
	for (const Control control : { Control::Tab0, Control::Tab1, Control::Tab2, Control::Tab3, Control::Tab4, Control::Close,
	         Control::Reroll, Control::Imbue, Control::Remove, Control::Cleanse, Control::Upgrade, Control::Downgrade,
	         Control::Option0, Control::Option1, Control::Option2, Control::Option3,
	         Control::ConfirmStep, Control::CancelStep, Control::Transmute }) {
		const Rectangle rect = ControlRect(control);
		if (rect.size.width == 0 || !rect.contains(position))
			continue;
		Pressed = control;
		PlayUiMoveSound();
		return true;
	}

	// The bench takes an item from the cursor and gives it back to an empty hand.
	Player &player = *MyPlayer;
	// The bench is not DRAWN on a stock tab (see DrawWorkshop), so it must not be clickable there
	// either (audit, 2026-09-22). Its rect sat live under the Gems, Runes and Jewels pages, which is
	// every tab Ogden has: a held item dropped in that corner went onto an invisible bench and came
	// back only when the window closed. Griswold's Salvage page learned this same lesson on
	// 2026-09-21 - a control that is not drawn must not be hit-tested.
	// Ogden's craft cells: a held item goes down, an item already there comes up. One item per cell,
	// whatever its footprint - see CraftGrid.
	if (const int cell = CraftSlotAt(position); cell >= 0) {
		Player &player = *MyPlayer;
		// The ANCHOR, not the cell: a 2x3 answers to any of its six cells, so a click on the blade
		// of a sword picks up the sword rather than finding an empty slot beside it.
		const int anchor = CraftAnchorAt(position);
		if (!player.HoldItem.isEmpty()) {
			if (anchor >= 0) {
				SetBoard(std::string(_("That cell is taken.")));
				return true;
			}
			// The cell under the cursor is preferred and the first that fits is the fallback, so an
			// item too tall to start here still goes down somewhere rather than refusing silently.
			if (!PlaceInCraftGrid(player.HoldItem, cell)) {
				SetBoard(std::string(_("There is no room on the bench for that.")));
				return true;
			}
			PlaySFX(ItemInvSnds[GetItemDropAnimIndex(player.HoldItem._iCurs)]);
			player.HoldItem.clear();
			NewCursor(CURSOR_HAND);
		} else if (anchor >= 0) {
			player.HoldItem = TakeFromCraftGrid(anchor);
			NewCursor(player.HoldItem._iCurs + CURSOR_FIRSTITEM);
			PlaySFX(IS_IGRAB);
		}
		Board.clear();
		return true;
	}

	// The bench is the MYSTIC's, on her two one-item tabs only - the same tightening the draw got.
	// Her Craft tab lost its bench to the painted well above (2026-09-22) and is hit-tested by
	// CraftSlotAt like Ogden's. Every OTHER page has no bench and must not hit-test one: a control
	// that is not drawn must not be clickable, which is the rule an invisible bench under Ogden's
	// collection tabs taught this file already.
	if ((OpenTab == Tab::Reroll || OpenTab == Tab::Imbue)
	    && BenchSlotRect().contains(position)) {
		if (!player.HoldItem.isEmpty()) {
			if (!Bench.isEmpty()) {
				SetBoard(std::string(_("The bench holds one item at a time.")));
				return true;
			}
			Bench = player.HoldItem;
			player.HoldItem.clear();
			NewCursor(CURSOR_HAND);
			SelectedRow = -1;
			OfferOpen = false;
			Board.clear();
			PlaySFX(ItemInvSnds[GetItemDropAnimIndex(Bench._iCurs)]);
		} else if (!Bench.isEmpty()) {
			player.HoldItem = Bench;
			Bench.clear();
			NewCursor(player.HoldItem._iCurs + CURSOR_FIRSTITEM);
			SelectedRow = -1;
			OfferOpen = false;
			PlaySFX(IS_IGRAB);
		}
		return true;
	}

	if (IsStockTab(OpenTab)) {
		// A cell picks its KIND, and clicking the picked one again clears it - the rule every list
		// in these windows follows. A cell standing for nothing absorbs the click and does nothing,
		// because it is still inside the frame.
		const int slot = BoardSlotAt(position);
		if (slot >= 0) {
			const int idx = BoardSlotItem(OpenTab, slot);
			if (idx != 0) {
				SelectedStockIdx = SelectedStockIdx == idx ? -1 : idx;
				PendingStep = 0; // a new pick withdraws whatever question was standing
				PlayUiSelectSound();
			}
		}
		return true;
	}
	// A list row selects; the lists are the tab's own.
	if (!OfferOpen && !Bench.isEmpty()) {
		const int rows = OpenTab == Tab::Reroll ? Bench._iOracoolAffixCount : CaptureImbuements(Bench).count;
		for (int row = 0; row < ListLines && row < rows; row++) {
			if (!ListRowRect(row).contains(position))
				continue;
			SelectedRow = SelectedRow == row ? -1 : row;
			PlayUiSelectSound();
			return true;
		}
	}
	return true; // the page's stone absorbs everything else
}

void ReleaseWorkshopButton()
{
	const Control pressed = Pressed;
	Pressed = Control::None;
	if (pressed == Control::None || !WindowOpen)
		return;
	const Rectangle rect = ControlRect(pressed);
	if (rect.size.width == 0 || !rect.contains(MousePosition))
		return; // released off the button: let me think a bit more
	RunControl(pressed);
}

} // namespace devilution::oracool
