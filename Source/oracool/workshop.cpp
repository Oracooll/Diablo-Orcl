#include "oracool/workshop.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
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
#include "oracool/runewords.h"
#include "oracool/shop_grid.h"
#include "oracool/ui_sound.h"
#include "oracool/window_close.h"
#include "player.h"
#include "qol/stash.h"
#include "effects.h"
#include "oracool/skill_sounds.h"
#include "stores.h"
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
constexpr int BoardTitleLift = 40;
constexpr Rectangle BoardTitleRect {
	{ BoardDowngradeRect.position.x + BoardButtonSize, BoardButtonTop - BoardTitleLift },
	{ BoardUpgradeRect.position.x - BoardDowngradeRect.position.x - BoardButtonSize, BoardButtonSize }
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

constexpr int CraftColumns = 3;
constexpr int CraftRows = 4;
constexpr int CraftSlots = CraftColumns * CraftRows;
constexpr int CraftPitch = 29;
constexpr int CraftCellPx = 28;
constexpr Point CraftGridOrigin { 128, 416 };
constexpr int CraftFrameLeft = 118;
constexpr int CraftFrameRight = 224;
constexpr int CraftFrameBottom = 541;
constexpr int CraftPlateSize = 34;
constexpr Rectangle CraftTransmuteRect {
	{ (CraftFrameLeft + CraftFrameRight + 1) / 2 - CraftPlateSize / 2, CraftFrameBottom + 1 + 4 },
	{ CraftPlateSize, CraftPlateSize }
};
constexpr const char *CraftTransmuteGlyphAsset = "ui\\shop_glyph_refresh.png";

constexpr int RecipeOpeningLeft = 29;
constexpr int RecipeOpeningTop = 299;
constexpr int RecipeOpeningRight = 310;
constexpr int RecipeOpeningBottom = 618;
constexpr int RecipeLinePitch = 20;

constexpr const char *BoardButtonFrameAsset = "ui\\shop_button_frame.png";
constexpr const char *BoardUpGlyphAsset = "ui\\shop_glyph_arrow_up.png";
constexpr const char *BoardDownGlyphAsset = "ui\\shop_glyph_arrow_down.png";
/** The shared canvas's opening, as every 340x720 window uses it. */
constexpr int InnerLeft = 22;
constexpr int InnerRight = 317;
constexpr Rectangle TitleRect { { 22, 26 }, { 296, 40 } };
constexpr Rectangle CloseRect { { 316, 5 }, { 18, 18 } };
/** The one-item slot: 2x3 inventory cells, and it holds one item whatever its size (user, 2026-09-21). */
constexpr Size SlotCells { 2, 3 };
constexpr int CellPx = INV_SLOT_SIZE_PX;
constexpr Rectangle SlotRect { { 30, 84 }, { SlotCells.width * CellPx, SlotCells.height * CellPx } };
/** The list beside the slot: the affixes on the Reroll tab, the shards on the Imbue tab. */
constexpr int ListLineHeight = 20;
constexpr int ListLines = 6;
constexpr Rectangle ListRect { { 104, 84 }, { 206, ListLines * ListLineHeight } };
/** The two rows of buttons under them, and the gold line under those. */
constexpr Size ButtonSize { 136, 30 };
constexpr int ButtonGap = 12;
constexpr int ButtonRowTop = 224;
constexpr int ButtonRowPitch = 38;
constexpr Rectangle GoldRect { { 30, 306 }, { 280, 18 } };
/** The message area: the alternatives menu, the refusals and the last thing that happened. */
constexpr Rectangle BoardRect { { 30, 336 }, { 280, 276 } };
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
constexpr int OptionCount = 4; // the affix as it stands, and three alternatives

bool WindowOpen = false;
WorkshopHost Host = WorkshopHost::Mystic;
Tab OpenTab = Tab::Reroll;
/** The one item on the bench. Returned to the pack when the window closes. */
Item Bench;
/**
 * @brief Ogden's craft grid: ONE item per cell, whatever its size.
 *
 * Deliberately simpler than the Cube's, which packs multi-cell footprints and keeps a separate
 * cell->anchor map. This is the BENCH's rule (2026-09-21, "a 2x3 slot that holds exactly one item
 * whatever its size") applied twelve times, and it is the format the crafting API already wants:
 * CanCraftFromLevskiGrid takes an Item array indexed by anchor, and a footprint map is a drawing
 * nicety on top of that rather than something the recipes read.
 *
 * NEVER persisted - returned to the player when the window closes, exactly as the Cube's grid is, so
 * a crafting station stays out of the save format entirely.
 */
std::array<Item, CraftSlots> CraftGrid {};
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
int SelectedRow = -1;
Control Pressed = Control::None;
Control LastHovered = Control::None;
std::string Board; // what the page is saying right now

/** The alternatives the last Reroll rolled: [0] is the affix as it stands, [1..3] the offers. */
bool OfferOpen = false;
int OfferSlot = -1;
std::array<OracoolAffix, OptionCount> Offers {};

/**
 * @brief The per-item counters, keyed by the item's seed and alive for the game (see the header).
 *
 * The seed is what makes an item that item - the crafting recipes reroll from it - so two items cannot
 * share a row, and an item taken away and brought back keeps its price.
 */
struct ItemCounters {
	uint32_t seed = 0;
	uint8_t rerolls = 0;
	uint8_t removals = 0;
	/** The affix slot the first reroll locked, or -1: only that one may be rerolled afterwards (the user agreed). */
	int8_t lockedAffix = -1;
};
std::vector<ItemCounters> Counters;

ItemCounters &CountersFor(const Item &item)
{
	for (ItemCounters &row : Counters) {
		if (row.seed == item._iSeed)
			return row;
	}
	Counters.push_back(ItemCounters { item._iSeed, 0, 0, -1 });
	return Counters.back();
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
	return PriceFor(500 * level, CountersFor(item).rerolls);
}

int RemovePrice(const Item &item)
{
	if (item.isEmpty())
		return 0;
	const int level = std::max<int>(1, item._iOracoolItemLevel);
	return PriceFor(250 * level, CountersFor(item).removals);
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
		return { Tab::Reroll, Tab::Imbue, Tab::Recipes };
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
	if (PlaceItemInWorld(std::move(made), player.position.tile) != 0)
		return Landing::Ground;
	return Landing::Nowhere;
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

/** @brief One of the two button rows: @p row 0 is the tab's own actions, @p row 1 the second pair. */
Rectangle ButtonRect(int row, int column, int columns)
{
	const Rectangle page = PageRect();
	const int span = columns * ButtonSize.width + (columns - 1) * ButtonGap;
	const int left = page.position.x + InnerLeft + (InnerRight - InnerLeft + 1 - span) / 2;
	return Rectangle { { left + column * (ButtonSize.width + ButtonGap), page.position.y + ButtonRowTop + row * ButtonRowPitch }, ButtonSize };
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
	case Control::Reroll:
		return OpenTab == Tab::Reroll && !OfferOpen ? ButtonRect(0, 0, 1) : Rectangle { { 0, 0 }, { 0, 0 } };
	case Control::Imbue:
		return OpenTab == Tab::Imbue ? ButtonRect(0, 0, 2) : Rectangle { { 0, 0 }, { 0, 0 } };
	case Control::Remove:
		return OpenTab == Tab::Imbue ? ButtonRect(0, 1, 2) : Rectangle { { 0, 0 }, { 0, 0 } };
	case Control::Cleanse:
		return OpenTab == Tab::Imbue ? ButtonRect(1, 0, 1) : Rectangle { { 0, 0 }, { 0, 0 } };
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
		return OpenTab == Tab::Craft ? Panel(CraftTransmuteRect) : Rectangle { { 0, 0 }, { 0, 0 } };
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

/** @brief Gives the bench back to the pack. False - and the item stays - when there is no room. */
bool ReturnBench()
{
	if (Bench.isEmpty())
		return true;
	if (!AutoPlaceItemInInventory(*MyPlayer, Bench, true))
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

/** @brief A placeholder button: a dark plate in a gold frame with a gold label, pressed, hovered or idle. */
void DrawPlateButton(const Surface &out, Control control, string_view label, bool enabled)
{
	const Rectangle rect = ControlRect(control);
	if (rect.size.width == 0)
		return;
	const bool hovered = rect.contains(MousePosition);
	const Rectangle face { rect.position + (Pressed == control ? PressSink : Displacement { 0, 0 }), rect.size };
	FillRect(out, face.position.x + 1, face.position.y + 1, face.size.width - 2, face.size.height - 2, PlateFill);
	OutlineRect(out, face, enabled ? FrameGold : PlateEdge);
	DrawString(out, label, face,
	    { (enabled ? UiFlags::ColorGold : UiFlags::ColorWhitegold) | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	if (hovered)
		BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, HoverBrightenPercent);
}

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

void DrawBench(const Surface &out)
{
	const Rectangle slot = Panel(SlotRect);
	FillRect(out, slot.position.x, slot.position.y, slot.size.width, slot.size.height, PlateFill);
	OutlineRect(out, slot, FrameGold);
	// The cell lines inside it, so it reads as the 2x3 the user asked for.
	for (int x = CellPx; x < slot.size.width; x += CellPx)
		FillRect(out, slot.position.x + x, slot.position.y, 1, slot.size.height, PAL16_GRAY + 9);
	for (int y = CellPx; y < slot.size.height; y += CellPx)
		FillRect(out, slot.position.x, slot.position.y + y, slot.size.width, 1, PAL16_GRAY + 9);
	if (Bench.isEmpty())
		return;
	const ClxSprite sprite = GetInvItemSprite(Bench._iCurs + CURSOR_FIRSTITEM);
	const Point topLeft { slot.position.x + (slot.size.width - static_cast<int>(sprite.width())) / 2,
		slot.position.y + (slot.size.height - static_cast<int>(sprite.height())) / 2 };
	DrawItem(Bench, out, { topLeft.x, topLeft.y + static_cast<int>(sprite.height()) - 1 }, sprite);
}

void DrawRerollList(const Surface &out)
{
	const ItemCounters &counters = Bench.isEmpty() ? ItemCounters {} : CountersFor(Bench);
	for (int row = 0; row < ListLines; row++) {
		const Rectangle rect = ListRowRect(row);
		if (Bench.isEmpty() || row >= Bench._iOracoolAffixCount) {
			if (row == 0 && Bench.isEmpty())
				DrawString(out, _("no item on the bench"), rect, { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter });
			continue;
		}
		const bool locked = counters.lockedAffix >= 0 && counters.lockedAffix != row;
		const bool selected = SelectedRow == row;
		if (selected)
			FillRect(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, PlateFill);
		const StringOrView line = PrintOracoolAffixPower(Bench._iOracoolAffixes[row], Bench);
		DrawString(out, line.str(), Rectangle { { rect.position.x + 4, rect.position.y }, { rect.size.width - 8, rect.size.height } },
		    { (locked ? UiFlags::ColorWhitegold : (selected ? UiFlags::ColorGold : UiFlags::ColorWhite)) | UiFlags::FontSize12 | UiFlags::VerticalCenter });
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

		// The counter, bottom left, on its own half-transparent ground so it reads over any icon.
		const std::string text = StrCat(std::min(count, 99));
		const int width = GetLineWidth(text, GameFont12) + 4;
		const Rectangle box { { cell.position.x, cell.position.y + cell.size.height - 12 }, { width, 12 } };
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

// ---------------------------------------------------------------------------------------------
// The window
// ---------------------------------------------------------------------------------------------

void OpenWorkshop(WorkshopHost host)
{
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

void CloseWorkshop()
{
	if (!WindowOpen)
		return;
	if (!ReturnBench()) {
		LogEvent(std::string(_("Your pack is full - the bench keeps what it holds.")), UiFlags::ColorRed);
		return;
	}
	// The craft grid too (2026-09-22). It is not a container: nothing may be left standing on it when
	// the window shuts, or a player who closed the window on three gems would have to guess where
	// they went. The window stays OPEN when there is nowhere to put them, which is the same answer
	// the bench above gives.
	if (!ReturnCraftGrid(*MyPlayer)) {
		LogEvent(std::string(_("Your pack and stash are full - the bench keeps what it holds.")), UiFlags::ColorRed);
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
	Bench.clear();
	// A new game starts with an empty bench. Not RETURNED - there is no player to return it to by
	// the time this runs - so it is simply dropped, which is what the Cube's grid does too.
	for (Item &item : CraftGrid)
		item.clear();
	Counters.clear();
	Board.clear();
}

/** @brief Craft cell @p slot, in screen space. */
Rectangle CraftSlotRect(int slot)
{
	const Rectangle page = PageRect();
	return Rectangle { { page.position.x + CraftGridOrigin.x + (slot % CraftColumns) * CraftPitch,
	                       page.position.y + CraftGridOrigin.y + (slot / CraftColumns) * CraftPitch },
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
	for (Item &item : CraftGrid) {
		if (item.isEmpty())
			continue;
		if (AutoPlaceItemInInventory(player, item, true) || AutoPlaceItemInStash(player, item, true)) {
			item.clear();
			continue;
		}
		all = false;
	}
	return all;
}

/** @brief Ogden's recipes, in book order - the ones HostOfRecipe hands him. */
std::vector<int> OgdenRecipes()
{
	std::vector<int> recipes;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		if (RecipeBelongsTo(i, TransmuteHost::Tavern))
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
void DrawCraftPage(const Surface &out)
{
	for (int slot = 0; slot < CraftSlots; slot++) {
		const Item &item = CraftGrid[slot];
		if (item.isEmpty())
			continue;
		const Rectangle cell = CraftSlotRect(slot);
		const ClxSprite sprite = GetInvItemSprite(item._iCurs + CURSOR_FIRSTITEM);
		// Fitted to the cell rather than drawn at 1:1: a craft grid takes items of every footprint,
		// and a 2x3 sword drawn at its natural size would cover half the board. See DrawSpriteToFit.
		DrawSpriteToFit(out, cell, sprite);
		if (cell.contains(MousePosition))
			DrawHoverOutline(out, cell);
	}

	// Griswold's Refresh plate as Transmute, as on Levski's Cube.
	const Rectangle rect = ControlRect(Control::Transmute);
	const bool ready = FirstReadyLevskiRecipeFor(CraftGrid.data(), TransmuteHost::Tavern) >= 0;
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
void DrawRecipesPage(const Surface &out)
{
	const Rectangle page = PageRect();
	const Rectangle opening { page.position + Displacement { RecipeOpeningLeft, RecipeOpeningTop },
		{ RecipeOpeningRight - RecipeOpeningLeft + 1, RecipeOpeningBottom - RecipeOpeningTop + 1 } };
	DrawThemedFill(out, opening, 2);

	const std::vector<int> recipes = OgdenRecipes();
	int y = opening.position.y + 6;
	const int width = opening.size.width - 12;
	for (const int recipe : recipes) {
		if (y + 2 * RecipeLinePitch > opening.position.y + opening.size.height)
			break; // a half-drawn recipe reads as a fault, not as "there is more"
		DrawString(out, _(CraftingRecipeName(recipe)),
		    Rectangle { { opening.position.x + 6, y }, { width, RecipeLinePitch } },
		    { UiFlags::ColorGold | UiFlags::FontSize12 | UiFlags::VerticalCenter });
		y += RecipeLinePitch;
		DrawString(out, _(CraftingRecipeInputs(recipe)),
		    Rectangle { { opening.position.x + 14, y }, { width - 8, RecipeLinePitch } },
		    { UiFlags::ColorWhite | UiFlags::FontSize12 | UiFlags::VerticalCenter });
		y += RecipeLinePitch;
	}
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
	if (Host != WorkshopHost::Mystic) {
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
	const bool painted = Host != WorkshopHost::Mystic && GetLoosePngSize(OgdenCanvasAsset).width > 0;
	if (!painted) {
		DrawString(out, Host == WorkshopHost::Mystic ? _("Mystic Workshop") : _("Jeweller's Tables"), Panel(TitleRect),
		    { UiFlags::ColorGold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}
	DrawTabColumn(out);
	// The bench belongs to the MYSTIC's two tabs and nowhere else (2026-09-22). "Not a stock tab" was
	// close enough while those were the only other pages; with Craft and Recipes here it would draw
	// her 2x3 slot over Ogden's cube grid and his recipe list.
	if (OpenTab == Tab::Reroll || OpenTab == Tab::Imbue)
		DrawBench(out);

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
		DrawPlateButton(out, Control::Reroll, StrCat(_("REROLL"), Bench.isEmpty() ? "" : StrCat("  ", FormatInteger(RerollPrice(Bench)))), ready);
	} else {
		DrawImbueList(out);
		DrawPlateButton(out, Control::Imbue, _("IMBUE"), !Bench.isEmpty());
		DrawPlateButton(out, Control::Remove, StrCat(_("REMOVE"), Bench.isEmpty() ? "" : StrCat("  ", FormatInteger(RemovePrice(Bench)))), SelectedRow >= 0);
		DrawPlateButton(out, Control::Cleanse, StrCat(_("CLEANSE"), Bench.isEmpty() ? "" : StrCat("  ", FormatInteger(CleansePrice(Bench)))), !Bench.isEmpty());
	}

	// Griswold's pair, on every tab (user, 2026-09-22): the pile, with the number beneath it and no
	// "Gold:" in front of it. The centred label that stood at y 306 is gone with the words.
	if (GetLoosePngSize(BoardGoldIconAsset).width > 0)
		DrawLoosePng(out, BoardGoldIconAsset, page.position + Displacement { BoardGoldIconAt.x, BoardGoldIconAt.y });
	DrawString(out, FormatInteger(static_cast<int>(TotalPlayerGold())), Panel(BoardGoldCountRect),
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter | UiFlags::Shadowed });
	if (IsStockTab(OpenTab)) {
		// The old message panel runs y 336..612 and the collection board sits at 439..590 INSIDE it,
		// so on these tabs it is not drawn at all - it would be a grey slab over every icon. The
		// message moves under the grid with the question, which is where the user put them.
		DrawBoardMessage(out);
	} else {
		DrawBoard(out);
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

bool SetWorkshopHoverInfoString()
{
	if (!WindowOpen)
		return false;
	if (Panel(SlotRect).contains(MousePosition) && !Bench.isEmpty()) {
		SetPanelString(Bench.getName(), Bench.getTextColor());
		const std::string count = ImbueCountLine(Bench);
		if (!count.empty())
			AddPanelString(count, UiFlags::ColorWhite);
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
		// The MYSTIC keeps the hand-off: her recipes have no page of their own here, and sending her
		// to the shared book is better than a tab that shows nothing.
		if (tabs[slot] == Tab::Recipes && Host == WorkshopHost::Mystic) {
			CloseWorkshop();
			if (!IsWorkshopOpen())
				OpenLevskiWindowFor(TransmuteHost::Barmaid);
			break;
		}
		OpenTab = tabs[slot];
		SelectedRow = -1;
		SelectedStockIdx = -1;
		PendingStep = 0;
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
		ItemCounters &counters = CountersFor(Bench);
		if (SelectedRow < 0 || SelectedRow >= Bench._iOracoolAffixCount) {
			SetBoard(std::string(_("Choose an affix from the list first.")));
			break;
		}
		if (counters.lockedAffix >= 0 && counters.lockedAffix != SelectedRow) {
			SetBoard(std::string(_("This item is settled: only the affix she first worked can be rerolled.")));
			break;
		}
		const int price = RerollPrice(Bench);
		if (static_cast<int>(TotalPlayerGold()) < price) {
			SetBoard(std::string(_("You do not have enough gold.")));
			break;
		}
		TakePlrsMoney(price);
		counters.lockedAffix = static_cast<int8_t>(SelectedRow);
		counters.rerolls = static_cast<uint8_t>(std::min(20, counters.rerolls + 1));
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
		ItemCounters &counters = CountersFor(Bench);
		counters.removals = static_cast<uint8_t>(std::min(20, counters.removals + 1));
		const ShardDefinition &def = ShardDef(static_cast<ShardKind>(ledger.kinds[SelectedRow]));
		ImbuementLedger kept;
		for (int i = 0; i < ledger.count; i++) {
			if (i != SelectedRow)
				kept.kinds[kept.count++] = ledger.kinds[i];
		}
		StripImbuements(Bench);
		RestoreImbuements(Bench, kept);
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
		// HIS recipes only: FirstReadyLevskiRecipeFor is asked for TransmuteHost::Tavern, so a grid
		// that happens to satisfy one of Griswold's or Gillian's does nothing here. The bench is
		// Ogden's, and a recipe running at the wrong artisan's window would be a bug that looked like
		// a feature.
		const int recipe = FirstReadyLevskiRecipeFor(CraftGrid.data(), TransmuteHost::Tavern);
		if (recipe < 0) {
			SetBoard(std::string(_("Nothing on the bench makes anything.")));
			break;
		}
		const std::string result = TransmuteLevskiGridWith(CraftGrid.data(), recipe);
		SetBoard(result);
		if (!IsTransmuteRefusal(result) && !PlayUiEventSound(UiEventSound::Transmute))
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
	if (const int slot = CraftSlotAt(position); slot >= 0) {
		Player &player = *MyPlayer;
		if (!player.HoldItem.isEmpty()) {
			if (!CraftGrid[slot].isEmpty()) {
				SetBoard(std::string(_("That cell is taken.")));
				return true;
			}
			CraftGrid[slot] = player.HoldItem;
			player.HoldItem.clear();
			NewCursor(CURSOR_HAND);
			PlaySFX(ItemInvSnds[GetItemDropAnimIndex(CraftGrid[slot]._iCurs)]);
		} else if (!CraftGrid[slot].isEmpty()) {
			player.HoldItem = CraftGrid[slot];
			CraftGrid[slot].clear();
			NewCursor(player.HoldItem._iCurs + CURSOR_FIRSTITEM);
			PlaySFX(IS_IGRAB);
		}
		Board.clear();
		return true;
	}

	// The bench is the MYSTIC's, on her two tabs only - the same tightening the draw got.
	if ((OpenTab == Tab::Reroll || OpenTab == Tab::Imbue) && Panel(SlotRect).contains(position)) {
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
