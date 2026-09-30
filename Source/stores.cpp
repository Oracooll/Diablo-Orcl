/**
 * @file stores.cpp
 *
 * Implementation of functionality for stores and towner dialogs.
 */
#include "stores.h"

#include "oracool/item_tiers.h" // ScaleValueForBaseTier - a bought unique's own value
#include "oracool/gems.h"              // EffectiveRequirement - Cain's line asks what CanUseItem asks
#include "oracool/level_requirement.h" // RequiredLevel
#include "oracool/necro_items.h"       // ClassMayUseItem
#include "oracool/workshop.h"

#include "oracool/crafting.h"    // TransmuteHost (Levski's Cube, 2026-09-20)
#include "oracool/levski_roar.h" // OpenLevskiWindowFor

#include <algorithm>
#include <iterator>
#include <array>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <limits>
#include <vector>

#include <SDL.h>
#include <fmt/format.h>

#include "DiabloUI/text_input.hpp"
#include "control.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "engine/backbuffer_state.hpp"
#include "engine/load_cel.hpp"
#include "engine/random.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/trn.hpp"
#include "error.h"
#include "init.h"
#include "minitext.h"
#include "objects.h"
#include "options.h"
#include "oracool/auto_save.h"
#include "oracool/event_log.h"
#include "oracool/item_sets.h"
#include "oracool/oracool.h"
#include "oracool/crafting_menu.h"
#include "oracool/runeword_book.h"
#include "oracool/shop_grid.h"
#include "oracool/shop_toast.h"
#include "oracool/shop_tabs.h"
#include "oracool/ui_sound.h"
#include "oracool/waypoint_menu.h"
#include "oracool/skill_points.h"
#include "panels/info_box.hpp"
#include "qol/stash.h"
#include "loadsave.h" // StashFileRefused - sale gold stays out of an unreadable stash
#include "oracool/window_close.h" // the Refresh Until prompt's red X
#include "towners.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/stdcompat/string_view.hpp"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

TalkID stextflag;

int storenumh;
int8_t storehidx[StoreHoldCapacity];
/**
 * @brief Oracool Tabbed Inventory: -1 means storehidx[i] keeps its existing InvList/belt meaning;
 * 0-8 means the item at storehold[i] came from extra tab storehTabIdx[i] (displayed as tab
 * storehTabIdx[i]+2), at InvTabList position storehidx[i] within that tab.
 */
int8_t storehTabIdx[StoreHoldCapacity];
Item storehold[StoreHoldCapacity];

Item smithitem[SMITH_ITEMS];
int numpremium;
int premiumlevel;
Item premiumitems[SMITH_PREMIUM_ITEMS];

// A PAGE-FULL, not a hand-picked count (user, 2026-08-27: "Uniques items quantity to be a
// page-full. Remove Number of Uniques items from INI file. Unique shop to be only ON/OFF settable
// in the INI").
//
// Forty rather than a number derived from the grid, because the grid measures CELLS and uniques
// vary from a 1x1 ring to a 2x3 breastplate - there is no single item count that fills 160 cells.
// The shelf is defined by what fits (see PlaceStock), so this only has to be comfortably MORE than
// a page can hold; the page decides where the stock actually ends.
constexpr int CuratedShelfCapacity = 40;

/**
 * @brief One curated shelf - a page of items generated once and not refilled as they are bought.
 *
 * The unique shelf was the first, and for a while the only one, so its array and its "has it been
 * built yet" flag were two file-scope variables and its behaviour was about twenty `case
 * TalkID::SmithUniqueBuy:` labels. Rare and Set were asked for next (user, 2026-08-27), and copying
 * that twice would have meant sixty labels and three places for the same stale-row bug to be fixed
 * in two of them.
 *
 * So the shelves are indexed instead. Everything that differs between them - the INI switch, the
 * tab name, the generator - is a function OF the index; everything that does not differ is written
 * once. Adding a fourth shelf is a row in each of those three functions.
 */
struct CuratedShelfState {
	Item items[CuratedShelfCapacity];
	bool initialized;
};

CuratedShelfState CuratedShelves[static_cast<size_t>(CuratedShelf::Count)];

Item *ShelfItems(CuratedShelf shelf)
{
	// Bounded, because the index comes from a TalkID mapping and a mapping can be wrong. Reading one
	// shelf past the array would be a silent out-of-bounds walk over whatever statics follow it -
	// the same shape as the premium-stock overrun this file already had once (see SortVendor).
	assert(shelf < CuratedShelf::Count);
	if (shelf >= CuratedShelf::Count)
		return CuratedShelves[0].items;
	return CuratedShelves[static_cast<size_t>(shelf)].items;
}

/**
 * @brief The shelf @p id shows, for callers that already know it has one.
 *
 * Every call site is inside a `case TalkID::Smith{Unique,Rare,Set}Buy:` label, so the lookup cannot
 * fail today - but it is safe by COINCIDENCE of those labels, and a fourth shelf added to one switch
 * and not to CuratedShelfFor would turn seven unchecked `*optional` dereferences into undefined
 * behaviour at once. This makes that failure a wrong shelf and a debug assert instead.
 */
CuratedShelf RequireCuratedShelf(TalkID id)
{
	const std::optional<CuratedShelf> shelf = CuratedShelfFor(id);
	assert(shelf.has_value() && "a curated-shelf screen is missing from CuratedShelfFor");
	return shelf.value_or(CuratedShelf::Unique);
}

/** @brief The wording on Griswold's menu row for @p shelf. */
const char *CuratedShelfMenuLabel(CuratedShelf shelf)
{
	switch (shelf) {
	case CuratedShelf::Unique:
		return N_("Buy unique items");
	case CuratedShelf::Rare:
		return N_("Buy rare items");
	case CuratedShelf::Set:
		return N_("Buy set items");
	case CuratedShelf::Count:
		break;
	}
	return "";
}

/**
 * @brief The depth a vendor's stock is generated at: the deepest floor visited, clamped 6-16.
 *
 * Lifted out of SetupTownStores when the Refresh buttons needed it. A reroll has to regenerate a
 * shelf at the SAME depth it was first built at, and the alternative - passing the level around, or
 * recomputing the walk at each call site - is how two answers to one question come about.
 */
int VendorStockLevel()
{
	const Player &myPlayer = *MyPlayer;
	int l = myPlayer._pLevel / 2;
	if (!gbIsMultiplayer) {
		l = 0;
		for (int i = 0; i < NUMLEVELS; i++) {
			if (myPlayer._pLvlVisited[i])
				l = i;
		}
	}
	return clamp(l + 2, 6, 16);
}

Item healitem[20];

Item witchitem[WITCH_ITEMS];

int boylevel;
Item boyitem;
Item boyitems[BOY_ITEMS];
Item gambleitems[GAMBLE_ITEMS];

/**
 * @brief Oracool: user request - typing the Refresh Until target in-game instead of editing
 * diablo.ini's "Griswold Refresh Until Item Names" by hand. Deliberately reuses that exact INI
 * field as the live text buffer (see StartRefreshUntilPrompt) rather than a separate scratch
 * buffer, so what's typed here is both what GetPremiumRefreshTargets()/RefreshPremiumUntilTarget()
 * already search for and what persists to the INI on the next options save - no new save-format
 * or option needed.
 */
bool IsRefreshUntilPromptOpen;
TextInputCursorState RefreshUntilPromptCursor;
/** @brief The names as they were when the prompt opened, restored by Escape (round 11 audit, v1.12.236). */
std::string RefreshUntilNamesBeforeEdit;
std::optional<TextInputState> RefreshUntilPromptInputState;

namespace {

/** The current towner being interacted with */
_talker_id talker;

/** Is the current dialog full size */
bool stextsize;

/** Number of text lines in the current dialog */
int stextsmax;
/** Remember currently selected text line from stext while displaying a dialog */
int stextlhold;
/** Currently selected text line from stext */
int stextsel;

struct STextStruct {
	enum Type : uint8_t {
		Label,
		Divider,
		Selectable,
	};

	std::string text;
	int _sval;
	int y;
	UiFlags flags;
	Type type;
	uint8_t _sx;
	uint8_t _syoff;
	int cursId;
	bool cursIndent;

	[[nodiscard]] bool isDivider() const
	{
		return type == Divider;
	}
	[[nodiscard]] bool isSelectable() const
	{
		return type == Selectable;
	}

	[[nodiscard]] bool hasText() const
	{
		return !text.empty();
	}
};

/** Text lines */
STextStruct stext[STORE_LINES];

/** Whether to render the player's gold amount in the top left */
bool RenderGold;

/** Does the current panel have a scrollbar */
bool stextscrl;
/** Remember last scoll position */
int stextvhold;
/** Scoll position */
int stextsval;
/** Next scoll position */
int stextdown;
/** Previous scoll position */
int stextup;
/** Count down for the push state of the scroll up button */
int8_t stextscrlubtn;
/** Count down for the push state of the scroll down button */
int8_t stextscrldbtn;

/** Remember current store while displaying a dialog */
TalkID stextshold;

/** Temporary item used to hold the the item being traided */
Item StoreItem;

/** Maps from towner IDs to NPC names. */
const char *const TownerNames[] = {
	N_("Griswold"),
	N_("Pepin"),
	"",
	N_("Ogden"),
	N_("Cain"),
	N_("Farnham"),
	N_("Adria"),
	N_("Gillian"),
	N_("Wirt"),
};

constexpr int PaddingTop = 32;

// For most languages, line height is always 12.
// This includes blank lines and divider line.
constexpr int SmallLineHeight = 12;
constexpr int SmallTextHeight = 12;

// For larger small fonts (Chinese and Japanese), text lines are
// taller and overflow.
// We space out blank lines a bit more to give space to 3-line store items.
constexpr int LargeLineHeight = SmallLineHeight + 1;
constexpr int LargeTextHeight = 18;

/**
 * The line index with the Back / Leave button.
 * This is a special button that is always the last line.
 *
 * For lists with a scrollbar, it is not selectable (mouse-only).
 */
int BackButtonLine()
{
	if (IsSmallFontTall()) {
		return stextscrl ? 21 : 20;
	}
	return 22;
}

/**
 * @brief Oracool bug fix: user report - the horizontal golden divider between the premium item
 * list and the Back/Refresh/Refresh Until row disappeared whenever Refresh Until was enabled.
 * Root cause: AddItemListBackButton() only draws that divider (via AddSLine) in the non-tall-font
 * case, at BackButtonLine()-1 - the exact line index PremiumRefreshUntilLine() used to return, so
 * setting up the Refresh Until button (AddSText) on that same line silently overwrote the
 * divider's line type with a normal text line. Shifted one line earlier specifically for the
 * non-tall-font case to stay clear of it; the tall-font case never draws a divider here, so its
 * offset is unchanged.
 */
int PremiumRefreshLine()
{
	return IsSmallFontTall() ? BackButtonLine() - 2 : BackButtonLine() - 3;
}

int PremiumRefreshUntilLine()
{
	return IsSmallFontTall() ? BackButtonLine() - 1 : BackButtonLine() - 2;
}

/**
 * @brief Oracool: user request - "Sell all"'s own line index, used for StoreEnter() dispatch and
 * stextsel bookkeeping. Its *rendered* position is overridden elsewhere (ScrollSmithSell) to
 * share Back's row, flush against the right golden border, like Griswold Premium's own "Refresh"
 * button and "Repair all" - this index just needs to be a line the Sell screen's own item list
 * never uses.
 */
int SmithSellAllLine()
{
	return BackButtonLine() - 2;
}

/**
 * @brief Oracool: user request - "Repair all"'s own line index, used for StoreEnter() dispatch
 * and stextsel bookkeeping. Its *rendered* position is overridden elsewhere (ScrollSmithSell) to
 * share Back's row, flush against the right golden border, like Griswold Premium's own "Refresh"
 * button - this index just needs to be a line the Repair screen's own item list never uses.
 */
int SmithRepairAllLine()
{
	return BackButtonLine() - 2;
}

/**
 * @brief Every towner's dialog, ONE layout (user, 2026-09-24 dev note: "check all vendors dialog windows and
 * align positioning of all texts on them to be on identical places - talk to XXX, Leave XXXX, Enter shop,
 * etc..."). The title had sat on line 1, 2 or 3, "Talk to" on 8, 10 or 12, the door on 12, 14 or 15 and
 * the way out on 14, 18 or 22, worded five ways. Now: the name on 2, the question on 9, Talk on 12, the
 * door (or the one service) on 14, a second service on 16, and "Leave <name>" on 18 - for all eight.
 * Named, because the lines are addressed from the Enter handlers and every back path, not only drawn.
 */
constexpr int TownerTitleLine = 2;
constexpr int TownerPromptLine = 9;
constexpr int TownerTalkLine = 12;
constexpr int TownerDoorLine = 14;
constexpr int TownerSecondLine = 16;
constexpr int TownerLeaveLine = 18;

std::vector<TalkID> SmithMenuEntries()
{
	// ONE DOOR. This listed all seven services and had grown a new line every time one was added -
	// nine entries by v1.9.24, which stops being a menu and becomes a list you read every visit.
	//
	// They are all still there; they are TABS now (oracool/shop_tabs.h), which is how D2 and D3
	// both answer this. Gossip and leave stay out here because one is a conversation and the other
	// is the way out of the building - neither is a shop screen.
	//
	// SmithBuy is the door because it is the tab a player wants most often; the strip lets them
	// reach any of the others in one further click, which is fewer than the old menu ever managed.
	return { TalkID::Gossip, TalkID::SmithBuy, TalkID::None };
}

int SmithMenuFirstLine(size_t /*entryCount*/)
{
	// The shared layout's Talk line; the leave entry is not counted from here but sits on TownerLeaveLine.
	return TownerTalkLine;
}

int SmithMenuLine(TalkID service)
{
	if (service == TalkID::None)
		return TownerLeaveLine;
	const std::vector<TalkID> entries = SmithMenuEntries();
	auto position = std::find(entries.begin(), entries.end(), service);
	if (position == entries.end()) {
		// The service asked for is a TAB now, not a menu entry - every caller that backs out of one
		// lands here. Falling back to the first line would put the cursor on "Talk to Griswold",
		// which is not where anyone leaving the shop wants it; the DOOR is, so that anyone stepping
		// out is one click from stepping back in.
		position = std::find(entries.begin(), entries.end(), TalkID::SmithBuy);
		if (position == entries.end())
			return SmithMenuFirstLine(entries.size());
	}
	return SmithMenuFirstLine(entries.size()) + static_cast<int>(std::distance(entries.begin(), position)) * 2;
}

constexpr size_t SmithPepinPotionCount = 4;
std::array<Item, SmithPepinPotionCount> smithPepinPotions;

std::array<_item_indexes, SmithPepinPotionCount> SmithPepinPotionTypes()
{
	return { IDI_HEAL, IDI_FULLHEAL, ItemMiscIdIdx(IMISC_REJUV), ItemMiscIdIdx(IMISC_FULLREJUV) };
}

void InitializeSmithPepinPotions()
{
	const std::array<_item_indexes, SmithPepinPotionCount> potionTypes = SmithPepinPotionTypes();
	for (size_t i = 0; i < SmithPepinPotionCount; ++i) {
		// Two of the four come from ItemMiscIdIdx, which can now answer IDI_NONE rather than
		// walking off the table. An empty slot here is correct and survivable: the stock builder
		// includes all four Pepin rows unconditionally, and an EMPTY item is skipped by the draw and
		// counted by nothing, whereas InitializeItem(-1) would index AllItemsList at -1.
		if (potionTypes[i] == IDI_NONE) {
			smithPepinPotions[i].clear();
			continue;
		}
		InitializeItem(smithPepinPotions[i], potionTypes[i]);
		smithPepinPotions[i]._iStatFlag = true;
	}
}

enum class ConsumablesVendor : uint8_t {
	Pepin,
	Witch,
};

struct ConsumablesStockEntry {
	Item *item;
	ConsumablesVendor vendor;
	int vendorIndex;

	[[nodiscard]] bool isReplenishing() const
	{
		return vendor == ConsumablesVendor::Pepin || vendorIndex < 3;
	}
};

std::vector<ConsumablesStockEntry> SmithConsumablesStock()
{
	std::vector<ConsumablesStockEntry> stock;
	stock.reserve(SmithPepinPotionCount + WITCH_ITEMS);
	for (size_t i = 0; i < SmithPepinPotionCount; ++i) {
		// EMPTY entries are filtered here rather than trusted to be "skipped by the draw", which is
		// what InitializeSmithPepinPotions' comment claimed and this builder did not do (external
		// audit of v1.9.97, finding 2, additional hardening). Two of the four types come from
		// ItemMiscIdIdx, which can answer IDI_NONE; all four resolve in the current configuration,
		// so the claim was true by luck rather than by construction. Filtering at the source keeps
		// every consumer - the draw, the placement pass, the index-to-vendor mapping - agreeing on
		// one list.
		if (smithPepinPotions[i].isEmpty())
			continue;
		stock.push_back({ &smithPepinPotions[i], ConsumablesVendor::Pepin, static_cast<int>(i) });
	}
	for (int i = 0; i < WITCH_ITEMS; ++i) {
		if (!witchitem[i].isEmpty())
			stock.push_back({ &witchitem[i], ConsumablesVendor::Witch, i });
	}
	return stock;
}

/**
 * @brief The stock entry at @p index, bounded.
 *
 * The bound lives HERE, not only in the callers, because an unbounded read of either container is
 * what produced both the crash and the garbage rows (user, 2026-08-20: rows reading "Gold", "Club"
 * and "Ring of Truth" in Griswold's consumables). Neither of those items is in Adria's stock - they
 * live in the statics ADJACENT to witchitem, which is what an overrun of a fixed array looks like
 * when it does not happen to fault. The vector path faults instead, which is the crash.
 *
 * Returning a shared empty item rather than clamping to the last entry: a caller that asks for a
 * row past the end should render nothing, not a duplicate of the final row.
 */
Item &WitchStockItem(int index, bool includePepinPotions)
{
	static Item OutOfRange;
	if (index < 0)
		return OutOfRange;
	if (!includePepinPotions)
		return index < WITCH_ITEMS ? witchitem[index] : OutOfRange;
	const std::vector<ConsumablesStockEntry> stock = SmithConsumablesStock();
	if (static_cast<size_t>(index) >= stock.size())
		return OutOfRange;
	return *stock[index].item;
}

int LineHeight()
{
	return IsSmallFontTall() ? LargeLineHeight : SmallLineHeight;
}

int TextHeight()
{
	return IsSmallFontTall() ? LargeTextHeight : SmallTextHeight;
}

void CalculateLineHeights()
{
	stext[0].y = 0;
	if (IsSmallFontTall()) {
		for (int i = 1; i < STORE_LINES; ++i) {
			// Space out consecutive text lines, unless they are both selectable (never the case currently).
			if (stext[i].hasText() && stext[i - 1].hasText() && !(stext[i].isSelectable() && stext[i - 1].isSelectable())) {
				stext[i].y = stext[i - 1].y + LargeTextHeight;
			} else {
				stext[i].y = i * LargeLineHeight;
			}
		}
	} else {
		for (int i = 1; i < STORE_LINES; ++i) {
			stext[i].y = i * SmallLineHeight;
		}
	}
}

void DrawSTextBack(const Surface &out)
{
	const Point uiPosition = GetUIRectangle().position;
	ClxDraw(out, { uiPosition.x + 320 + 24, 327 + uiPosition.y }, (*pSTextBoxCels)[0]);
	DrawHalfTransparentRectTo(out, uiPosition.x + 347, uiPosition.y + 28, 265, 297);
}

void DrawSSlider(const Surface &out, int y1, int y2)
{
	const Point uiPosition = GetUIRectangle().position;
	int yd1 = y1 * 12 + 44 + uiPosition.y;
	int yd2 = y2 * 12 + 44 + uiPosition.y;
	if (stextscrlubtn != -1)
		ClxDraw(out, { uiPosition.x + 601, yd1 }, (*pSTextSlidCels)[11]);
	else
		ClxDraw(out, { uiPosition.x + 601, yd1 }, (*pSTextSlidCels)[9]);
	if (stextscrldbtn != -1)
		ClxDraw(out, { uiPosition.x + 601, yd2 }, (*pSTextSlidCels)[10]);
	else
		ClxDraw(out, { uiPosition.x + 601, yd2 }, (*pSTextSlidCels)[8]);
	yd1 += 12;
	int yd3 = yd1;
	for (; yd3 < yd2; yd3 += 12) {
		ClxDraw(out, { uiPosition.x + 601, yd3 }, (*pSTextSlidCels)[13]);
	}
	if (stextsel == BackButtonLine())
		yd3 = stextlhold;
	else
		yd3 = stextsel;
	if (storenumh > 1)
		yd3 = 1000 * (stextsval + ((yd3 - stextup) / 4)) / (storenumh - 1) * (y2 * 12 - y1 * 12 - 24) / 1000;
	else
		yd3 = 0;
	ClxDraw(out, { uiPosition.x + 601, (y1 + 1) * 12 + 44 + uiPosition.y + yd3 }, (*pSTextSlidCels)[12]);
}

void AddSLine(size_t y)
{
	stext[y]._sx = 0;
	stext[y]._syoff = 0;
	stext[y].text.clear();
	stext[y].text.shrink_to_fit();
	stext[y].type = STextStruct::Divider;
	stext[y].cursId = -1;
	stext[y].cursIndent = false;
}

void AddSTextVal(size_t y, int val)
{
	stext[y]._sval = val;
}

void AddSText(uint8_t x, size_t y, string_view text, UiFlags flags, bool sel, int cursId = -1, bool cursIndent = false)
{
	stext[y]._sx = x;
	stext[y]._syoff = 0;
	stext[y].text.clear();
	AppendStrView(stext[y].text, text);
	stext[y].flags = flags;
	stext[y].type = sel ? STextStruct::Selectable : STextStruct::Label;
	stext[y].cursId = cursId;
	stext[y].cursIndent = cursIndent;
}

void AddOptionsBackButton()
{
	const int line = BackButtonLine();
	AddSText(0, line, _("Back"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	stext[line]._syoff = IsSmallFontTall() ? 0 : 6;
}

void AddItemListBackButton(bool selectable = false)
{
	const int line = BackButtonLine();
	string_view text = _("Back");
	if (!selectable && IsSmallFontTall()) {
		AddSText(0, line, text, UiFlags::ColorWhite | UiFlags::AlignRight, selectable);
	} else {
		AddSLine(line - 1);
		AddSText(0, line, text, UiFlags::ColorWhite | UiFlags::AlignCenter, selectable);
		stext[line]._syoff = 6;
	}
}

void PrintStoreItem(const Item &item, int l, UiFlags flags, bool cursIndent = false)
{
	std::string productLine;

	if (item._iIdentified) {
		if (item._iOracoolTier == OracoolItemTier::Set) {
			// A named set piece has NO rolled affixes - MakeSetItem writes its declared powers
			// straight into the _iPL* fields, so the affix list every other item uses is empty.
			// Audit finding, 2026-08-26: this branch did not exist, so every tiered item went
			// through that list and a set piece in a store list showed no powers at all -
			// the one item family whose powers are its entire identity.
			//
			// Read from the definition, exactly as the description panel does (see the Set branch
			// in items.cpp). PrintItemPower is right here for the same reason it is right there: a
			// set piece has one source per stat, so there is no accumulation to disentangle.
			if (const oracool::SetItemDefinition *def = oracool::FindSetItemByCursor(item._iCurs);
			    def != nullptr) {
				for (const ItemPower &power : def->powers) {
					if (power.type == IPL_INVALID)
						break;
					if (!productLine.empty())
						AppendStrView(productLine, _(",  "));
					// Its own value, as the set piece's tooltip prints it - not the accumulated field (round 8 audit).
					AppendStrView(productLine, PrintOracoolAffixPower(OracoolAffix { power.type, power.param1, 0 }, item));
				}
			}
		} else {
			// Every other item's affixes are its ONE list - Rare, Buffed Unique, Primal, magic, crafted - joined
			// with commas (user, 2026-09-25: "all afixes are now one pool"). A magic item used to be read from a
			// vanilla prefix/suffix pair of fields plus this list; the pair is gone and the list is the whole of
			// it. A vanilla unique has no list entries, so nothing is added for one.
			for (int i = 0; i < item._iOracoolAffixCount; i++) {
				if (!productLine.empty())
					AppendStrView(productLine, _(",  "));
				AppendStrView(productLine, PrintOracoolAffixPower(item._iOracoolAffixes[i], item));
			}
		}
	}
	if (item._iMiscId == IMISC_STAFF && item._iMaxCharges != 0) {
		if (!productLine.empty())
			AppendStrView(productLine, _(",  "));
		productLine.append(fmt::format(fmt::runtime(_("Charges: {:d}/{:d}")), item._iCharges, item._iMaxCharges));
	}
	if (!productLine.empty()) {
		AddSText(40, l, productLine, flags, false, -1, cursIndent);
		l++;
		productLine.clear();
	}

	if (item._itype != ItemType::Misc) {
		if (item._iClass == ICLASS_WEAPON)
			productLine = fmt::format(fmt::runtime(_("Damage: {:d}-{:d}  ")), item._iMinDam, item._iMaxDam);
		else if (item._iClass == ICLASS_ARMOR)
			productLine = fmt::format(fmt::runtime(_("Armor: {:d}  ")), item._iAC);
		// Zod's stamp is on the durability, not the maximum: "Dur: 255/60" (round 27 audit).
		const bool indestructible = item._iMaxDur == DUR_INDESTRUCTIBLE || item._iDurability == DUR_INDESTRUCTIBLE;
		if (!indestructible && item._iMaxDur != 0)
			productLine += fmt::format(fmt::runtime(_("Dur: {:d}/{:d},  ")), item._iDurability, item._iMaxDur);
		else
			AppendStrView(productLine, _("Indestructible,  "));
	}

	// What CanUseItem asks: Hel and Ease lower the stats, and the level and class rules follow (round 27 audit).
	const uint8_t str = static_cast<uint8_t>(oracool::EffectiveRequirement(item, item._iMinStr));
	const uint8_t mag = static_cast<uint8_t>(oracool::EffectiveRequirement(item, item._iMinMag));
	const uint8_t dex = static_cast<uint8_t>(oracool::EffectiveRequirement(item, item._iMinDex));

	if (str == 0 && mag == 0 && dex == 0) {
		AppendStrView(productLine, _("No required attributes"));
	} else {
		AppendStrView(productLine, _("Required:"));
		if (str != 0)
			productLine.append(fmt::format(fmt::runtime(_(" {:d} Str")), str));
		if (mag != 0)
			productLine.append(fmt::format(fmt::runtime(_(" {:d} Mag")), mag));
		if (dex != 0)
			productLine.append(fmt::format(fmt::runtime(_(" {:d} Dex")), dex));
	}
	// Short, so a long weapon line still fits the store's width (round 28 audit).
	if (const int level = oracool::RequiredLevel(item); level > 1)
		productLine.append(fmt::format(fmt::runtime(_(", Lvl {:d}")), level));
	if (!oracool::ClassMayUseItem(*MyPlayer, item))
		AppendStrView(productLine, _(", wrong class"));
	AddSText(40, l++, productLine, flags, false, -1, cursIndent);
}

bool StoreAutoPlace(Item &item, bool persistItem)
{
	Player &player = *MyPlayer;
	// AutoPlaceItemInInventory already falls back to the extra Tabbed Inventory tabs once tab 1
	// has no room, so no separate call is needed here.
	const bool placed = (AutoEquipEnabled(player, item) && AutoEquip(player, item, persistItem))
	    || (item.isPotion() && AutoPlaceItemInBelt(player, item, persistItem)) // only potions go to the belt (2026-09-14)
	    || AutoPlaceItemInInventory(player, item, persistItem);
	if (placed && persistItem)
		oracool::ScheduleAutoSaveForStorePurchase();
	return placed;
}

void StartSmith()
{
	if (!gbIsMultiplayer) {
		Player &myPlayer = *MyPlayer;
		if (*sgOptions.Oracool.griswoldRestoreHealth) {
			myPlayer._pHitPoints = myPlayer._pMaxHP;
			myPlayer._pHPBase = myPlayer._pMaxHPBase;
			RedrawComponent(PanelDrawComponent::Health);
		}
		if (*sgOptions.Oracool.griswoldRestoreMana) {
			myPlayer._pMana = myPlayer._pMaxMana;
			myPlayer._pManaBase = myPlayer._pMaxManaBase;
			RedrawComponent(PanelDrawComponent::Mana);
		}
	}

	stextsize = false;
	stextscrl = false;
	AddSText(0, TownerTitleLine, _("Blacksmith's shop"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	const std::vector<TalkID> entries = SmithMenuEntries();
	const int firstLine = SmithMenuFirstLine(entries.size());
	AddSText(0, TownerPromptLine, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	for (size_t i = 0; i < entries.size(); ++i) {
		const int line = entries[i] == TalkID::None ? TownerLeaveLine : firstLine + static_cast<int>(i) * 2;
		switch (entries[i]) {
		case TalkID::Gossip:
			AddSText(0, line, _("Talk to Griswold"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
			break;
		case TalkID::SmithBuy:
			// "Enter Shop", not "Buy basic items" - it opens the shop, and buying basic items is
			// only the tab it happens to land on.
			AddSText(0, line, _("Enter Shop"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			break;
		case TalkID::SmithPremiumBuy:
			AddSText(0, line, _("Buy premium items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			break;
		case TalkID::SmithUniqueBuy:
		case TalkID::SmithRareBuy:
		case TalkID::SmithSetBuy:
			AddSText(0, line, _(CuratedShelfMenuLabel(RequireCuratedShelf(entries[i]))),
			    UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			break;
		case TalkID::SmithSell:
			AddSText(0, line, _("Sell items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			break;
		case TalkID::SmithRepair:
			AddSText(0, line, _("Repair items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			break;
		case TalkID::SmithConsumables:
			AddSText(0, line, _("Buy consumables"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			break;
		case TalkID::SmithRecharge:
			AddSText(0, line, _("Recharge staves"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			break;
		case TalkID::None:
			AddSText(0, line, _("Leave Griswold"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			break;
		default:
			break;
		}
	}
	AddSLine(5);
	storenumh = TownerLeaveLine;
}

void ScrollSmithBuy(int idx)
{
	ClearSText(5, 21);
	stextup = 5;

	// Bounded, and idx advances unconditionally - the same two corrections ScrollWitchBuy needed,
	// because these functions are copies of one another. An index that moved only when a row was
	// DRAWN stalled on the first empty slot and made everything past it unreachable; an unbounded
	// smithitem[idx] read whatever follows the array once it did get past the end.
	for (int l = 5; l < 20 && idx < SMITH_ITEMS; l += 4, idx++) {
		if (!smithitem[idx].isEmpty()) {
			UiFlags itemColor = smithitem[idx].getTextColorWithStatCheck();
			AddSText(20, l, smithitem[idx].getName(), itemColor, true, smithitem[idx]._iCurs, true);
			AddSTextVal(l, smithitem[idx]._iIvalue);
			PrintStoreItem(smithitem[idx], l + 1, itemColor, true);
			stextdown = l;
		}
	}

	if (stextsel != -1 && !stext[stextsel].isSelectable() && stextsel != BackButtonLine())
		stextsel = stextdown;
}

// TotalPlayerGold's definition moved OUT of this anonymous namespace - see below the namespace's
// close. It is declared in stores.h now so the character sheet and the inventory's gold readout can
// share it; leaving the definition here as well made the name ambiguous between the internal one
// and the exported one. Calls from inside this namespace resolve to the exported one via stores.h.

// TODO: Change `_iIvalue` to be unsigned instead of passing `int` here.
bool PlayerCanAfford(int price)
{
	return TotalPlayerGold() >= static_cast<uint32_t>(price);
}

void StartSmithBuy()
{
	stextsize = true;
	stextscrl = true;
	stextsval = 0;

	RenderGold = true;
	AddSText(20, 1, _("I have these items for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	ScrollSmithBuy(stextsval);
	AddItemListBackButton();

	storenumh = 0;
	for (Item &item : smithitem) {
		if (item.isEmpty())
			continue;

		item._iStatFlag = MyPlayer->CanUseItem(item);
		storenumh++;
	}

	stextsmax = std::max(storenumh - 4, 0);
	if (stextflag == TalkID::SmithSell && !gbIsMultiplayer)
		AddSText(0, SmithSellAllLine(), _("Sell all"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

void ScrollSmithPremiumBuy(int boughtitems)
{
	ClearSText(5, 21);
	stextup = 5;

	int idx = 0;
	for (; boughtitems != 0; idx++) {
		if (!premiumitems[idx].isEmpty())
			boughtitems--;
	}

	for (int l = 5; l < 20 && idx < SMITH_PREMIUM_ITEMS; l += 4) {
		if (!premiumitems[idx].isEmpty()) {
			UiFlags itemColor = premiumitems[idx].getTextColorWithStatCheck();
			AddSText(20, l, premiumitems[idx].getName(), itemColor, true, premiumitems[idx]._iCurs, true);
			AddSTextVal(l, premiumitems[idx]._iIvalue);
			PrintStoreItem(premiumitems[idx], l + 1, itemColor, true);
			stextdown = l;
		} else {
			l -= 4;
		}
		idx++;
	}
	if (stextsel != -1 && !stext[stextsel].isSelectable() && stextsel != BackButtonLine())
		stextsel = stextdown;
	// Oracool: user request - Refresh and Refresh Until visually share the Back button's row,
	// flush to the right/left golden border respectively, instead of sitting on their own
	// centered rows above it. They keep their own line indices (PremiumRefreshLine()/
	// PremiumRefreshUntilLine()) for StoreEnter()'s dispatch and stextsel bookkeeping - only their
	// rendered Y position (via _syoff) actually moves. CheckStoreBtn() below has the matching
	// redirect that routes a click on Back's row to whichever of the three was actually clicked.
	// Oracool bug fix: user report - Refresh/Refresh Until sat a few pixels higher than Back.
	// The actual rendered Y of any line is stext[line].y + stext[line]._syoff (see the sy
	// computation in PrintStoreItem/PrintSString) - AddItemListBackButton() (called once, before
	// this function, in StartSmithPremiumBuy) already gives Back its own _syoff of 6 for exactly
	// this screen's layout, which this calculation was not accounting for.
	if (*sgOptions.Oracool.griswoldPremiumRefresh && !gbIsMultiplayer) {
		const int line = PremiumRefreshLine();
		AddSText(5, line, _("Refresh"), UiFlags::ColorWhite | UiFlags::AlignRight, true);
		stext[line]._syoff = static_cast<uint8_t>(stext[BackButtonLine()].y + stext[BackButtonLine()]._syoff - stext[line].y);
	}
	if (*sgOptions.Oracool.refreshUntilButton && !gbIsMultiplayer) {
		const int line = PremiumRefreshUntilLine();
		AddSText(5, line, _("Refresh until"), UiFlags::ColorWhite, true);
		stext[line]._syoff = static_cast<uint8_t>(stext[BackButtonLine()].y + stext[BackButtonLine()]._syoff - stext[line].y);
	}
}

bool StartSmithPremiumBuy()
{
	storenumh = 0;
	for (Item &item : premiumitems) {
		if (item.isEmpty())
			continue;

		item._iStatFlag = MyPlayer->CanUseItem(item);
		storenumh++;
	}
	if (storenumh == 0) {
		// Emptied by a purchase on the grid: stay in the shop on the Basic tab. The text menu shut the grid and the
		// inventory around the last buy (round 3 audit, v1.12.229).
		if (oracool::IsShopGridScreen(TalkID::SmithPremiumBuy)) {
			StartStore(TalkID::SmithBuy);
			return false;
		}
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithPremiumBuy);
		return false;
	}

	stextsize = true;
	stextscrl = true;
	stextsval = 0;

	RenderGold = true;
	AddSText(20, 1, _("I have these premium items for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	AddItemListBackButton();

	stextsmax = std::max(storenumh - 4, 0);

	ScrollSmithPremiumBuy(stextsval);

	return true;
}

/** @brief The heading over @p shelf's list, and the wording on Griswold's menu row. */
const char *CuratedShelfHeading(CuratedShelf shelf)
{
	switch (shelf) {
	case CuratedShelf::Unique:
		return N_("I have these unique items for sale:");
	case CuratedShelf::Rare:
		return N_("I have these rare items for sale:");
	case CuratedShelf::Set:
		return N_("I have these set items for sale:");
	case CuratedShelf::Count:
		break;
	}
	return "";
}

void ScrollCuratedShelfBuy(CuratedShelf shelf, int idx)
{
	const Item *items = ShelfItems(shelf);
	ClearSText(5, 21);
	stextup = 5;
	for (int l = 5; l < 20 && idx < CuratedShelfCapacity; l += 4, ++idx) {
		if (items[idx].isEmpty()) {
			l -= 4;
			continue;
		}
		const UiFlags itemColor = items[idx].getTextColorWithStatCheck();
		AddSText(20, l, items[idx].getName(), itemColor, true, items[idx]._iCurs, true);
		AddSTextVal(l, items[idx]._iIvalue);
		PrintStoreItem(items[idx], l + 1, itemColor, true);
		stextdown = l;
	}
	if (stextsel != -1 && !stext[stextsel].isSelectable() && stextsel != BackButtonLine())
		stextsel = stextdown;
}

bool StartCuratedShelfBuy(CuratedShelf shelf)
{
	Item *items = ShelfItems(shelf);
	storenumh = 0;
	for (int i = 0; i < CuratedShelfCapacity; i++) {
		if (items[i].isEmpty())
			continue;
		items[i]._iStatFlag = MyPlayer->CanUseItem(items[i]);
		++storenumh;
	}
	if (storenumh == 0) {
		// As the premium shelf: the last purchase on a grid tab stays in the shop, on the Basic tab.
		if (oracool::IsShopGridScreen(TalkIdForCuratedShelf(shelf))) {
			StartStore(TalkID::SmithBuy);
			return false;
		}
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkIdForCuratedShelf(shelf));
		return false;
	}

	stextsize = true;
	stextscrl = true;
	stextsval = 0;
	RenderGold = true;
	AddSText(20, 1, _(CuratedShelfHeading(shelf)), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	AddItemListBackButton();
	stextsmax = std::max(storenumh - 4, 0);
	ScrollCuratedShelfBuy(shelf, 0);
	return true;
}

bool SmithSellOk(const Item &item)
{
	if (item.isEmpty())
		return false;

	if (!gbIsMultiplayer) {
		// Unique Shop items (CreateUniqueVendorItem, items.cpp) are ordinary merchandise even
		// when their underlying base item uses an ID in the quest-item range - identified by
		// the CF_SMITH marker every such item carries.
		const bool isUniqueShopItem = item._iMagical == ITEM_QUALITY_UNIQUE && (item._iCreateInfo & CF_SMITH) != 0;
		if (isUniqueShopItem)
			return item._iIdentified && item._iIvalue > 0;

		if (item._itype == ItemType::Gold)
			return false;
		if (item._iClass == ICLASS_QUEST)
			return false;
		// Oracool bug fix: user report - "i cant sell unique items i was awarded from NPCs to
		// griswold. They don't appear in the SELL ITEMS list." The quest-item ID range
		// (IDI_FIRSTQUEST..IDI_LASTQUEST) isn't exclusively real quest deliverables - several
		// genuine, lootable/sellable Unique items (Skeleton King's Crown, Ring of Truth, Optic
		// Amulet, Harlequin Crest, Steel Veil, ...) happen to use base-item slots that fall inside
		// it too, purely as an artifact of vanilla's item-table ordering, the same way The
		// Butcher's Cleaver (IDI_CLEAVER == IDI_FIRSTQUEST) already needed its own exemption. A
		// real quest item is never ITEM_QUALITY_UNIQUE, so checking that instead of special-casing
		// one IDidx at a time exempts every current and future case at once.
		if (item._iMagical != ITEM_QUALITY_UNIQUE && item.IDidx >= IDI_FIRSTQUEST && item.IDidx <= IDI_LASTQUEST)
			return false;
		if (item.IDidx == IDI_LAZSTAFF)
			return false;

		const int saleBaseValue = item._iMagical != ITEM_QUALITY_NORMAL && item._iIdentified ? item._iIvalue : item._ivalue;
		return saleBaseValue > 0;
	}

	if (item._iMiscId > IMISC_OILFIRST && item._iMiscId < IMISC_OILLAST)
		return true;

	if (item._itype == ItemType::Misc)
		return false;
	if (item._itype == ItemType::Gold)
		return false;
	if (item._itype == ItemType::Staff && (!gbIsHellfire || IsValidSpell(item._iSpell)))
		return false;
	if (item._iClass == ICLASS_QUEST)
		return false;
	if (item.IDidx == IDI_LAZSTAFF)
		return false;

	return true;
}

/**
 * @brief Scans InvList, the belt, and (if enabled) every Tabbed Inventory extra tab for items
 * the given predicate approves of, prices each one, and sorts the result by price if that
 * option is on - the shared logic behind StartSmithSell and StartWitchSell, which previously
 * duplicated this same ~75-line scan/price/sort sequence with only the predicate differing.
 * @return true if anything sellable was found (storenumh/storehold/storehidx/storehTabIdx are
 * populated either way, just empty when this returns false).
 */
bool PopulateSellList(bool (*sellOk)(const Item &))
{
	storenumh = 0;
	for (auto &item : storehold)
		item.clear();

	bool foundAny = false;
	const Player &myPlayer = *MyPlayer;

	auto addIfSellable = [&](const Item &item, int8_t idx, int8_t tabIdx) {
		if (storenumh >= StoreHoldCapacity || !sellOk(item))
			return;
		foundAny = true;
		storehold[storenumh] = item;

		storehold[storenumh]._ivalue = GetItemSellValue(item);
		storehold[storenumh]._iIvalue = storehold[storenumh]._ivalue;
		storehidx[storenumh] = idx;
		storehTabIdx[storenumh] = tabIdx;
		storenumh++;
	};

	for (int8_t i = 0; i < myPlayer._pNumInv; i++)
		addIfSellable(myPlayer.InvList[i], i, -1);

	// Oracool: user request - belt items are excluded from the sell list entirely when this
	// option is on, so Griswold/Adria only ever offer what's in the backpack.
	if (!*sgOptions.Oracool.griswoldSellIgnoresBelt) {
		for (int i = 0; i < MaxBeltItems; i++)
			addIfSellable(myPlayer.SpdList[i], static_cast<int8_t>(-(i + 1)), -1);
	}

	// Oracool Tabbed Inventory: items stored in an extra tab are just as sellable as anything
	// in the original backpack or belt.
	if (TabbedInventoryEnabled()) {
		for (int t = 0; t < Player::NumExtraInventoryTabs; t++) {
			for (int i = 0; i < myPlayer._pNumInvTab[t]; i++)
				addIfSellable(myPlayer.InvTabList[t][i], static_cast<int8_t>(i), static_cast<int8_t>(t));
		}
	}

	if (!gbIsMultiplayer) {
		// Stable insertion sort keeps equal-price items in their original inventory/belt order,
		// while moving the source index together with the displayed item.
		for (int i = 1; i < storenumh; ++i) {
			int j = i;
			while (j > 0 && storehold[j - 1]._iIvalue < storehold[j]._iIvalue) {
				std::swap(storehold[j - 1], storehold[j]);
				std::swap(storehidx[j - 1], storehidx[j]);
				std::swap(storehTabIdx[j - 1], storehTabIdx[j]);
				--j;
			}
		}
	}

	return foundAny;
}

void ScrollSmithSell(int idx)
{
	ClearSText(5, 21);
	stextup = 5;

	for (int l = 5; l < 20; l += 4) {
		if (idx >= storenumh)
			break;
		if (!storehold[idx].isEmpty()) {
			UiFlags itemColor = storehold[idx].getTextColorWithStatCheck();

			if (storehold[idx]._iMagical != ITEM_QUALITY_NORMAL && storehold[idx]._iIdentified) {
				AddSText(20, l, storehold[idx].getName(), itemColor, true, storehold[idx]._iCurs, true);
				AddSTextVal(l, storehold[idx]._iIvalue);
			} else {
				AddSText(20, l, storehold[idx].getName(), itemColor, true, storehold[idx]._iCurs, true);
				AddSTextVal(l, storehold[idx]._ivalue);
			}

			PrintStoreItem(storehold[idx], l + 1, itemColor, true);
			stextdown = l;
		}
		idx++;
	}

	stextsmax = std::max(storenumh - 4, 0);

	// Oracool bug fix: user report - "Repair all" was invisible. Root cause: DrawSText re-runs
	// this function every single frame the Sell/Repair/Recharge/Identify screen is open (see its
	// per-frame ScrollSmithSell(stextsval) dispatch), and ClearSText above wipes lines 5-20 on
	// every call - including whichever line "Sell all"/"Repair all" occupies. Adding them only
	// once, in StartSmithSell/StartSmithRepair, meant they got wiped again on the very next frame
	// and never came back, since nothing else ever re-added them. Must live here instead, so they
	// get put back every time this function clears and repopulates the list - the same reason
	// Griswold Premium's "Refresh" button lives inside the equally per-frame ScrollSmithPremiumBuy
	// rather than the one-time StartSmithPremiumBuy.
	//
	// Oracool: user request - "Sell all" now shares Back's row too, flush against the right
	// golden border, matching the position "Repair all"/Refresh already settled on as the
	// standard spot for this class of button, instead of its own centered row above Back.
	if (!gbIsMultiplayer && storenumh > 0) {
		if (stextflag == TalkID::SmithSell) {
			const int line = SmithSellAllLine();
			AddSText(5, line, _("Sell all"), UiFlags::ColorWhite | UiFlags::AlignRight, true);
			stext[line]._syoff = static_cast<uint8_t>(stext[BackButtonLine()].y + stext[BackButtonLine()]._syoff - stext[line].y);
		} else if (stextflag == TalkID::SmithRepair) {
			const int line = SmithRepairAllLine();
			AddSText(5, line, _("Repair all"), UiFlags::ColorWhite | UiFlags::AlignRight, true);
			stext[line]._syoff = static_cast<uint8_t>(stext[BackButtonLine()].y + stext[BackButtonLine()]._syoff - stext[line].y);
		}
	}
}

void StartSmithSell()
{
	stextsize = true;

	if (!PopulateSellList(SmithSellOk)) {
		stextscrl = false;

		RenderGold = true;
		AddSText(20, 1, _("You have nothing I want."), UiFlags::ColorWhitegold, false);
		AddSLine(3);
		AddItemListBackButton(/*selectable=*/true);
		return;
	}

	const Player &myPlayer = *MyPlayer;
	stextscrl = true;
	stextsval = 0;
	stextsmax = myPlayer._pNumInv;

	RenderGold = true;
	AddSText(20, 1, _("Which item is for sale?"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	ScrollSmithSell(stextsval);
	AddItemListBackButton();
}

bool SmithRepairOk(int i)
{
	const Player &myPlayer = *MyPlayer;

	if (myPlayer.InvList[i].isEmpty())
		return false;
	if (myPlayer.InvList[i]._itype == ItemType::Misc)
		return false;
	if (myPlayer.InvList[i]._itype == ItemType::Gold)
		return false;
	// Phase 1 ethereal: no smith can touch a ghost - that refusal IS the item's price.
	if (myPlayer.InvList[i]._iOracoolEthereal)
		return false;
	if (myPlayer.InvList[i]._iDurability == myPlayer.InvList[i]._iMaxDur)
		return false;

	return true;
}

// Every body slot whose item carries durability, in the order the repair list walks them. The
// storehidx encoding for a body slot is -(its index here + 1), which keeps the historical -1..-4
// for the vanilla four and extends the same scheme over the Oracool worn slots - one table read
// by both StartSmithRepair and SmithRepairItem, so the encoding cannot drift between them.
// Rings and amulets are absent because jewelry has no durability to lose.
constexpr inv_body_loc RepairableBodySlots[] = {
	INVLOC_HEAD, INVLOC_CHEST, INVLOC_HAND_LEFT, INVLOC_HAND_RIGHT,
	INVLOC_SHOULDERS, INVLOC_BRACERS, INVLOC_GLOVES, INVLOC_WAIST, INVLOC_LEGS, INVLOC_BOOTS
};
constexpr int NumRepairableBodySlots = sizeof(RepairableBodySlots) / sizeof(RepairableBodySlots[0]);

// Every worn slot Cain can identify, encoded in storehidx as -(index + 1) like the repair list: the first seven keep
// vanilla's -1..-7, and the six the fork added follow (audit, 2026-09-27 - an unidentified piece on the shoulders, wrists,
// hands, waist, legs or feet never appeared on his list).
constexpr inv_body_loc IdentifiableBodySlots[] = {
	INVLOC_HEAD, INVLOC_CHEST, INVLOC_HAND_LEFT, INVLOC_HAND_RIGHT, INVLOC_RING_LEFT, INVLOC_RING_RIGHT, INVLOC_AMULET,
	INVLOC_SHOULDERS, INVLOC_BRACERS, INVLOC_GLOVES, INVLOC_WAIST, INVLOC_LEGS, INVLOC_BOOTS
};
constexpr int NumIdentifiableBodySlots = sizeof(IdentifiableBodySlots) / sizeof(IdentifiableBodySlots[0]);

void StartSmithRepair()
{
	stextsize = true;
	storenumh = 0;

	for (auto &item : storehold) {
		item.clear();
	}

	Player &myPlayer = *MyPlayer;

	// All ten durability-bearing body slots, not just the vanilla four - the Oracool worn slots
	// take damage now (DamageArmor spreads its wear across every worn piece), so they must also
	// be repairable, or their gear would only ever decay. The ethereal refusal applies here too
	// (external audit, 2026-08-17): SmithRepairOk turned ghosts away from the inventory walk while
	// EQUIPPED ethereals slipped onto the list through these body-slot adds, contradicting both
	// the item's own "cannot be repaired" line and the Repair spell's decline.
	for (int k = 0; k < NumRepairableBodySlots; k++) {
		Item &worn = myPlayer.InvBody[RepairableBodySlots[k]];
		if (worn.isEmpty() || worn._iDurability == worn._iMaxDur)
			continue;
		if (worn._iOracoolEthereal)
			continue;
		AddStoreHoldRepair(&worn, static_cast<int8_t>(-(k + 1)));
	}

	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (storenumh >= StoreHoldCapacity)
			break;
		if (SmithRepairOk(i)) {
			AddStoreHoldRepair(&myPlayer.InvList[i], i);
		}
	}

	if (!gbIsMultiplayer) {
		// AddStoreHoldRepair already overwrites _iIvalue with the computed repair cost, so
		// sorting on it here ranks by "what you'd pay to fix this," not the item's own value -
		// matching the Sell list's stable insertion sort (PopulateSellList) so equal-cost
		// items keep their original relative order.
		for (int i = 1; i < storenumh; ++i) {
			int j = i;
			while (j > 0 && storehold[j - 1]._iIvalue < storehold[j]._iIvalue) {
				std::swap(storehold[j - 1], storehold[j]);
				std::swap(storehidx[j - 1], storehidx[j]);
				std::swap(storehTabIdx[j - 1], storehTabIdx[j]);
				--j;
			}
		}
	}

	if (storenumh == 0) {
		stextscrl = false;

		RenderGold = true;
		AddSText(20, 1, _("You have nothing to repair."), UiFlags::ColorWhitegold, false);
		AddSLine(3);
		AddItemListBackButton(/*selectable=*/true);
		return;
	}

	stextscrl = true;
	stextsval = 0;
	stextsmax = myPlayer._pNumInv;

	RenderGold = true;
	AddSText(20, 1, _("Repair which item?"), UiFlags::ColorWhitegold, false);
	AddSLine(3);

	ScrollSmithSell(stextsval);
	AddItemListBackButton();
}

void FillManaPlayer()
{
	if (!*sgOptions.Gameplay.adriaRefillsMana)
		return;

	Player &myPlayer = *MyPlayer;

	if (myPlayer._pMana != myPlayer._pMaxMana) {
		PlaySFX(IS_CAST8);
	}
	myPlayer._pMana = myPlayer._pMaxMana;
	myPlayer._pManaBase = myPlayer._pMaxManaBase;
	RedrawComponent(PanelDrawComponent::Mana);
}

/**
 * @brief Adria's one shop entry.
 *
 * A named constant because four places address it: the line that draws it, WitchEnter's dispatch,
 * and the back paths out of all three shop screens. It was three separate numbers before the tabs
 * collapsed them into one door, and three of those four sites would still compile after a miss.
 */
constexpr int WitchShopDoorLine = TownerDoorLine;

void StartWitch()
{
	FillManaPlayer();
	stextsize = false;
	stextscrl = false;
	AddSText(0, TownerTitleLine, _("Witch's shack"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerPromptLine, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerTalkLine, _("Talk to Adria"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	// One door, like Griswold's - buy, sell and recharge are tabs inside it now
	// (oracool/shop_tabs.h). The respec takes the shared second line and the leave line the shared
	// leave line (2026-09-24); both were 20 and 22, the only dialog that ran past 18.
	AddSText(0, WitchShopDoorLine, _("Enter Shop"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	// Oracool Phase 2.3: the respec, at Adria (megaplan). Selectable only when there are points to
	// reclaim; the price is on the line so the decision is made before the click.
	if (oracool::TotalInvestedSkillPoints(*MyPlayer) > 0) {
		AddSText(0, TownerSecondLine,
		    fmt::format(fmt::runtime(_("Reset skill points ({:d} gold)")), oracool::RespecCost(*MyPlayer)),
		    UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	} else {
		AddSText(0, TownerSecondLine, _("Reset skill points"), UiFlags::ColorUiSilverDark | UiFlags::AlignCenter, false);
	}
	AddSText(0, TownerLeaveLine, _("Leave Adria"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	storenumh = 20;
}

void ScrollWitchBuy(int idx, bool includePepinPotions)
{
	ClearSText(5, 21);
	stextup = 5;

	// CRASH FIX (user report, 2026-08-20: "i was purchasing consumables from griswold and scrolling
	// his consumables store when the game suddenly crashed").
	//
	// This function runs EVERY FRAME from the store's scroll dispatch, but storenumh and stextsmax
	// - the numbers that bound stextsval - are only recomputed in StartWitchBuy, when the screen is
	// opened. Buying a consumable removes it from witchitem immediately (RemoveWitchStockItem), so
	// the stock shrinks under a scroll offset that nobody re-clamped, and the next frame indexed
	// past the end.
	//
	// On the WITCH's own path that was survivable by accident: WitchStockItem indexes the fixed
	// 25-entry witchitem array, so a stale index read an empty slot rather than off the end. The
	// SMITH's path builds a std::vector of only the NON-EMPTY entries, which is both shorter and
	// has no empty sentinel to stop the walk - so the same stale index is unchecked
	// vector::operator[] on freed-adjacent memory. The two paths sharing one function is what let
	// the safe one vouch for the unsafe one.
	//
	// Both ends are fixed here rather than at the call site: the loop cannot read past the stock,
	// and stextsval is pulled back into range so scrolling recovers instead of staying stuck at an
	// index that renders nothing.
	// TWO different bounds, and conflating them was a regression in v1.8.92 (self-audit,
	// 2026-08-21). They answer different questions:
	//
	//   readLimit  - how far the loop may INDEX. For the witch that is the fixed array's size, so a
	//                hole in the middle cannot truncate the list early.
	//   liveCount  - how many items actually EXIST. This is what bounds scrolling, because scrolling
	//                past the last real item just shows blank rows.
	//
	// v1.8.92 used WITCH_ITEMS for both and assigned it to stextsmax, which quietly overwrote the
	// correct value StartWitchBuy had computed from storenumh. Adria's list could then scroll to
	// offset 21 no matter how few potions she had, showing blanks the whole way down. The smith path
	// was unaffected - its vector holds only non-empty entries, so there the two numbers coincide,
	// which is exactly why the mistake was invisible on the screen that had prompted the fix.
	const int readLimit = includePepinPotions
	    ? static_cast<int>(SmithConsumablesStock().size())
	    : WITCH_ITEMS;
	int liveCount = readLimit;
	if (!includePepinPotions) {
		liveCount = 0;
		for (int i = 0; i < WITCH_ITEMS; i++) {
			if (!witchitem[i].isEmpty())
				liveCount++;
		}
	}
	// Recomputed HERE as well as in StartWitchBuy, because this is the function that runs every
	// frame. StartWitchBuy set it once, when the screen opened; every purchase since has shrunk the
	// stock without anyone revising the bound StoreDown scrolls against, so the offset was free to
	// walk off the end of a list that had got shorter underneath it.
	stextsmax = std::max(liveCount - 4, 0);
	idx = std::clamp(idx, 0, stextsmax);
	stextsval = idx;

	for (int l = 5; l < 20; l += 4) {
		if (idx >= readLimit)
			break;
		Item &item = WitchStockItem(idx, includePepinPotions);
		// idx advances WHATEVER this entry turns out to be. It used to advance only when the entry
		// was drawn, so a single empty slot stalled the walk: every remaining row re-read the same
		// empty entry, drew nothing, and the rest of the list became unreachable however far you
		// scrolled (user, 2026-08-20 - blank rows while "scrolling his consumables store").
		//
		// The witch path is where this bites, because it walks the RAW witchitem array rather than
		// the filtered vector, and that array grows holes as soon as anything is bought from it.
		idx++;
		if (!item.isEmpty()) {
			UiFlags itemColor = item.getTextColorWithStatCheck();
			AddSText(20, l, item.getName(), itemColor, true, item._iCurs, true);
			AddSTextVal(l, item._iIvalue);
			PrintStoreItem(item, l + 1, itemColor, true);
			stextdown = l;
		}
	}

	if (stextsel != -1 && !stext[stextsel].isSelectable() && stextsel != BackButtonLine())
		stextsel = stextdown;
}

void WitchBookLevel(Item &bookItem)
{
	if (bookItem._iMiscId != IMISC_BOOK)
		return;
	bookItem._iMinMag = GetSpellData(bookItem._iSpell).minInt;
	uint8_t spellLevel = MyPlayer->_pSplLvl[static_cast<int16_t>(bookItem._iSpell)];
	while (spellLevel > 0) {
		bookItem._iMinMag += 20 * bookItem._iMinMag / 100;
		spellLevel--;
		if (bookItem._iMinMag + 20 * bookItem._iMinMag / 100 > 255) {
			bookItem._iMinMag = 255;
			spellLevel = 0;
		}
	}
}

void StartWitchBuy(bool includePepinPotions)
{
	stextsize = true;
	stextscrl = true;
	stextsval = 0;
	stextsmax = 20;

	RenderGold = true;
	AddSText(20, 1, _("I have these items for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	ScrollWitchBuy(stextsval, includePepinPotions);
	AddItemListBackButton();

	storenumh = 0;
	const std::vector<ConsumablesStockEntry> smithStock = includePepinPotions ? SmithConsumablesStock() : std::vector<ConsumablesStockEntry> {};
	auto updateItem = [&](Item &item) {
		if (item.isEmpty())
			return;

		WitchBookLevel(item);
		item._iStatFlag = MyPlayer->CanUseItem(item);
		storenumh++;
	};
	if (includePepinPotions) {
		for (const ConsumablesStockEntry &entry : smithStock)
			updateItem(*entry.item);
	} else {
		for (Item &item : witchitem)
			updateItem(item);
	}
	stextsmax = std::max(storenumh - 4, 0);
}

bool WitchSellOk(const Item &item)
{
	if (item.isEmpty())
		return false;

	bool rv = false;

	if (item._itype == ItemType::Misc)
		rv = true;
	if (item._iMiscId > 29 && item._iMiscId < 41)
		rv = false;
	if (item._iClass == ICLASS_QUEST)
		rv = false;
	if (item._itype == ItemType::Staff && (!gbIsHellfire || IsValidSpell(item._iSpell)))
		rv = true;
	// Oracool bug fix: same root cause as SmithSellOk's quest-ID-range exemption above - a
	// genuine Unique item (e.g. a Unique staff) can share a base-item slot with the quest-item
	// range purely by vanilla's table ordering, and a real quest item is never ITEM_QUALITY_UNIQUE.
	if (item._iMagical != ITEM_QUALITY_UNIQUE && item.IDidx >= IDI_FIRSTQUEST && item.IDidx <= IDI_LASTQUEST)
		rv = false;
	if (item.IDidx == IDI_LAZSTAFF)
		rv = false;
	// Nor a Guardian Keystone, worth nothing, as Griswold refuses it: Adria bought it for 1 gold, and one already turned
	// at the gate left the rift barred until it was bought back (round 19 audit, v1.12.244).
	if (item._iMiscId == IMISC_ORACOOL_KEYSTONE)
		rv = false;
	return rv;
}

void StartWitchSell()
{
	stextsize = true;

	if (!PopulateSellList(WitchSellOk)) {
		stextscrl = false;

		RenderGold = true;
		AddSText(20, 1, _("You have nothing I want."), UiFlags::ColorWhitegold, false);
		AddSLine(3);
		AddItemListBackButton(/*selectable=*/true);
		return;
	}

	const Player &myPlayer = *MyPlayer;
	stextscrl = true;
	stextsval = 0;
	stextsmax = myPlayer._pNumInv;

	RenderGold = true;
	AddSText(20, 1, _("Which item is for sale?"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	ScrollSmithSell(stextsval);
	AddItemListBackButton();
}

bool WitchRechargeOk(int i)
{
	const auto &item = MyPlayer->InvList[i];

	if (item._itype == ItemType::Staff && item._iCharges != item._iMaxCharges) {
		return true;
	}

	if ((item._iMiscId == IMISC_UNIQUE || item._iMiscId == IMISC_STAFF) && item._iCharges < item._iMaxCharges) {
		return true;
	}

	return false;
}

/**
 * @brief What the player has sold this session, newest first, at the price they were paid.
 *
 * This is the Sold tab (user request, 2026-08-23: "keep items i sold there so i can buy back at
 * sold price if i change my mind"). Every sale goes through RecordSale, whichever door it came in
 * by - a click on the old list, Sell all, or an item dragged onto the shop panel.
 *
 * Not saved. A buyback list that survived a reload would have to survive the shop restocking too,
 * and "the thing you just sold is still there" only needs to hold for as long as changing your mind
 * is plausible. It is cleared with the rest of the stores in InitStores.
 */
struct SoldItem {
	/**
	 * @brief The item EXACTLY as the player owned it. Its own values are never overwritten.
	 *
	 * They used to be. Selling wrote the quarter-price it fetched into `_ivalue` and `_iIvalue`
	 * before recording, so buying back returned an item worth a quarter of the one sold - and
	 * selling that fetched a quarter again. Every round trip divided the item by four, permanently
	 * and invisibly (user, 2026-08-27: "it is like it is constantly changing and reducing").
	 */
	Item item;
	/** @brief What the player was paid, and therefore what buying it back costs. */
	int price;
	/** @brief Which vendor bought it. Adria and Griswold do not share a shelf. */
	bool witch;
};
std::vector<SoldItem> BuybackStock;
constexpr size_t MaxBuybackItems = 40;

/** @brief Whether @p id is one of Adria's screens - she and Griswold take different things. */
bool IsWitchShopScreen(TalkID id)
{
	return IsAnyOf(id, TalkID::WitchBuy, TalkID::WitchSell, TalkID::WitchRecharge);
}

/**
 * @brief Positions in BuybackStock that belong on @p id's Sold tab, newest first.
 *
 * One function so the tab's CONTENTS and the buyback's TARGET cannot disagree. They are two
 * different pieces of code reading the same list through the same filter, and a filter written twice
 * is a filter that eventually sells the wrong item back.
 */
std::vector<size_t> BuybackIndicesFor(TalkID id)
{
	const bool witch = IsWitchShopScreen(id);
	std::vector<size_t> indices;
	for (size_t i = 0; i < BuybackStock.size(); i++) {
		if (BuybackStock[i].witch == witch)
			indices.push_back(i);
	}
	return indices;
}

void RecordSale(const Item &item, int price)
{
	if (item.isEmpty())
		return;
	// Newest first, and the oldest falls off the end.
	//
	//  price is passed in rather than read out of the item, since 2026-08-27. It used to be taken
	// from `_iIvalue`, which meant every caller had to overwrite the item with its own sale price
	// first - and that overwritten copy was what the player got back. See SoldItem::item.
	if (BuybackStock.size() >= MaxBuybackItems)
		BuybackStock.pop_back();
	BuybackStock.insert(BuybackStock.begin(), { item, price, IsWitchShopScreen(stextflag) });
}

/**
 * @brief Pays @p cost into the player's purse, wherever that is. Returns what would NOT go in.
 *
 * Returns rather than logs (external audit of v1.9.92, finding 4): "a function that can lose money
 * must not have a void interface that callers cannot check". It could already tell that some of the
 * proceeds had nowhere to go, and its only recourse was a red line in the event log after the item
 * was already gone.
 */
int CreditSaleProceeds(int cost)
{
	// Oracool: sale proceeds go to the shared Stash pool, matching where a purchase's change and a
	// ground pickup's gold already land (see GoldAutoPlace, inv.cpp).
	Player &myPlayer = *MyPlayer;
	// Not into a stash this game could not read: it is never written back, and the sale's gold was lost (round 16 audit).
	if (oracool::IsSinglePlayer() && !StashFileRefused) {
		// As much as the pool will take, then the rest to the backpack (external audit of v1.9.88,
		// finding 7). It used to be all-or-nothing: the whole sale went to the Stash only if the
		// whole sale fitted, and otherwise the whole sale went to the backpack - so a stash with
		// room for 100 gold contributed NOTHING to a 1,000 gold sale, and the backpack was asked to
		// absorb an amount it may not have had cells for.
		//
		// Headroom in int64 because `INT_MAX - Stash.gold` is the one subtraction here that is safe
		// only as long as Stash.gold cannot exceed INT_MAX - and the deposit guard that keeps it
		// under the cap is exactly what this branch exists to work around.
		const int64_t headroom = static_cast<int64_t>(std::numeric_limits<int>::max()) - Stash.gold;
		const int toStash = static_cast<int>(std::min<int64_t>(cost, std::max<int64_t>(headroom, 0)));
		if (toStash > 0) {
			Stash.gold += toStash;
			Stash.dirty = true;
			cost -= toStash;
		}
		if (cost == 0)
			return 0;
	}

	// AddGoldToInventory returns what it could NOT place, and that return was discarded while
	// `_pGold += cost` credited the full amount regardless - so the purse claimed gold that was in
	// no stack, and the next CalculateGold silently corrected it back down. The item was already
	// gone by then (external audit, 2026-08-25).
	//
	// _pGold is recomputed from the stacks rather than added to, which is what every other caller
	// that changes gold does: a total derived from what actually exists cannot claim what does not.
	const int unplaced = AddGoldToInventory(myPlayer, cost);
	myPlayer._pGold = CalculateGold(myPlayer);
	if (unplaced > 0) {
		// Reaching here means the caller's fit gate said the sale would fit and it did not. Every
		// sale path checks StoreGoldFit before parting the player from the item now, so this should
		// be unreachable - it is kept because "should be unreachable" and "is" are different, and a
		// named remainder beats gold vanishing in silence.
		oracool::LogEvent(StrCat("Sale proceeds could not be placed: ", unplaced, " gold lost"),
		    UiFlags::ColorRed);
	}
	return unplaced;
}

/**
 * @brief Griswold's fee to repair @p item, or 0 when there is nothing to charge for.
 *
 * Extracted from AddStoreHoldRepair for the same reason as RechargePriceFor below: the shop's
 * Repair button prices a HELD item, which is in no list.
 *
 * The ethereal refusal is here rather than only at the call sites. Ghosts cannot be repaired at any
 * price (they have their own recipe), and the last audit found equipped ones slipping onto the
 * repair list through a path that had not been given the check - putting it in the price makes the
 * refusal a property of the item rather than of whoever remembered to ask.
 */
int RepairPriceFor(const Item &item)
{
	// No isEmpty() test: `_iMaxDur <= 0` already covers an empty item, and adding one broke
	// AddStoreHoldRepair_magic, which pins this formula by handing it a bare struct with durability
	// fields and nothing else set. An extra refusal that only fires on inputs the formula could
	// already price at zero is not worth losing that.
	if (item._iOracoolEthereal || item._iMaxDur <= 0 || item._iDurability >= item._iMaxDur)
		return 0;
	const int64_t due = item._iMaxDur - item._iDurability;
	// In 64 bits and capped (audit, 2026-09-27): a Torment-tier magic item is valued in the millions, and 30 x value x
	// wear passed INT_MAX within a few points of wear - a negative price the hero could never afford, which also
	// stopped Repair All, or one wrapped to nearly free.
	const int64_t price = item._iMagical != ITEM_QUALITY_NORMAL && item._iIdentified
	    ? 30 * static_cast<int64_t>(item._iIvalue) * due / (static_cast<int64_t>(item._iMaxDur) * 100 * 2)
	    : std::max<int64_t>(static_cast<int64_t>(item._ivalue) * due / (static_cast<int64_t>(item._iMaxDur) * 2), 1);
	return static_cast<int>(std::min<int64_t>(price, std::numeric_limits<int>::max()));
}

/**
 * @brief Adria's fee to recharge @p item, or 0 when there is nothing to charge for.
 *
 * Extracted from AddStoreHoldRecharge (which now calls it) so the shop panel's Recharge button can
 * price a HELD item - one that is in the player's hand and therefore in no list this file builds.
 * One copy of the formula, because two would be two things to keep in step.
 */
int RechargePriceFor(const Item &item)
{
	const bool takesCharges = item._itype == ItemType::Staff
	    || item._iMiscId == IMISC_UNIQUE || item._iMiscId == IMISC_STAFF;
	if (item.isEmpty() || !takesCharges || item._iMaxCharges <= 0 || item._iCharges >= item._iMaxCharges)
		return 0;
	const int base = item._ivalue + GetSpellData(item._iSpell).staffCost();
	return base * (item._iMaxCharges - item._iCharges) / (item._iMaxCharges * 2);
}

void AddStoreHoldRecharge(Item itm, int8_t i)
{
	// The bound belongs HERE, not only in the loop that calls this. Every caller had to remember it
	// and one of them - the equipped weapon, added before the loop starts - never checked at all.
	// It happens to be safe because storenumh is zero at that point, which is a fact about the
	// caller rather than a property of this function.
	if (storenumh >= StoreHoldCapacity)
		return;
	const int price = RechargePriceFor(itm);
	if (price == 0)
		return;
	storehold[storenumh] = itm;
	storehold[storenumh]._ivalue = price;
	storehold[storenumh]._iIvalue = price;
	storehidx[storenumh] = i;
	storehTabIdx[storenumh] = -1; // recharge never sources from an extra tab; keep the array in sync regardless
	storenumh++;
}

void StartWitchRecharge()
{
	stextsize = true;
	bool rechargeok = false;
	storenumh = 0;

	for (auto &item : storehold) {
		item.clear();
	}

	const Player &myPlayer = *MyPlayer;
	const auto &leftHand = myPlayer.InvBody[INVLOC_HAND_LEFT];

	if ((leftHand._itype == ItemType::Staff || leftHand._iMiscId == IMISC_UNIQUE) && leftHand._iCharges != leftHand._iMaxCharges) {
		rechargeok = true;
		AddStoreHoldRecharge(leftHand, -1);
	}

	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (storenumh >= StoreHoldCapacity)
			break;
		if (WitchRechargeOk(i)) {
			rechargeok = true;
			AddStoreHoldRecharge(myPlayer.InvList[i], i);
		}
	}

	if (!rechargeok) {
		stextscrl = false;

		RenderGold = true;
		AddSText(20, 1, _("You have nothing to recharge."), UiFlags::ColorWhitegold, false);
		AddSLine(3);
		AddItemListBackButton(/*selectable=*/true);
		return;
	}

	stextscrl = true;
	stextsval = 0;
	stextsmax = myPlayer._pNumInv;

	RenderGold = true;
	AddSText(20, 1, _("Recharge which item?"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	ScrollSmithSell(stextsval);
	AddItemListBackButton();
}

void StoreNoMoney()
{
	StartStore(stextshold);
	stextscrl = false;
	stextsize = true;
	RenderGold = true;
	ClearSText(5, 23);
	AddSText(0, 14, _("You do not have enough gold"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

void StoreNoRoom()
{
	StartStore(stextshold);
	stextscrl = false;
	ClearSText(5, 23);
	AddSText(0, 14, _("You do not have enough room in inventory"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

void StoreConfirm(Item &item)
{
	StartStore(stextshold);
	stextscrl = false;
	ClearSText(5, 23);

	UiFlags itemColor = item.getTextColorWithStatCheck();
	AddSText(20, 8, item.getName(), itemColor, false);
	AddSTextVal(8, item._iIvalue);
	PrintStoreItem(item, 9, itemColor);

	string_view prompt;

	switch (stextshold) {
	case TalkID::BoyBuy:
		prompt = _("Do we have a deal?");
		break;
	case TalkID::BoyGamble:
		prompt = _("Gamble on it?");
		break;
	case TalkID::StorytellerIdentify:
		prompt = _("Are you sure you want to identify this item?");
		break;
	case TalkID::HealerBuy:
	case TalkID::SmithPremiumBuy:
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithSetBuy:
	case TalkID::WitchBuy:
	case TalkID::SmithConsumables:
	case TalkID::SmithBuy:
		prompt = _("Are you sure you want to buy this item?");
		break;
	case TalkID::WitchRecharge:
	case TalkID::SmithRecharge:
		prompt = _("Are you sure you want to recharge this item?");
		break;
	case TalkID::SmithSell:
	case TalkID::WitchSell:
		prompt = _("Are you sure you want to sell this item?");
		break;
	case TalkID::SmithRepair:
		prompt = _("Are you sure you want to repair this item?");
		break;
	default:
		app_fatal(StrCat("Unknown store dialog ", static_cast<int>(stextshold)));
	}
	AddSText(0, 15, prompt, UiFlags::ColorWhite | UiFlags::AlignCenter, false);
	AddSText(0, 18, _("Yes"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 20, _("No"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

void StartBoy()
{
	stextsize = false;
	stextscrl = false;
	AddSText(0, TownerTitleLine, _("Wirt"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSLine(5);
	AddSText(0, TownerPromptLine, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	// The 50-gold peek is gone (2026-09-20): Wirt keeps a shop now, two tabs - what he has to sell,
	// and the gamble - like the other vendors' grids.
	AddSText(0, TownerTalkLine, _("Talk to Wirt"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, TownerDoorLine, _("Enter Shop"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	// "Leave Wirt" (user, 2026-09-23 dev note) - the menu was built from Gillian's and kept her line.
	AddSText(0, TownerLeaveLine, _("Leave Wirt"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

/** @brief The grid shop's own screen state for Wirt's two tabs; the grid draws the rest. */
void StartBoyShop()
{
	stextsize = true;
	stextscrl = true;
	stextsval = 0;
	RenderGold = true;
	AddSText(20, 1, _("I have these items for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	AddItemListBackButton();
	for (Item &item : boyitems) {
		if (!item.isEmpty())
			item._iStatFlag = MyPlayer->CanUseItem(item);
	}
	for (Item &item : gambleitems) {
		if (!item.isEmpty())
			item._iStatFlag = MyPlayer->CanUseItem(item);
	}
	stextsmax = 0;
}

void SStartBoyBuy()
{
	stextsize = true;
	stextscrl = false;

	RenderGold = true;
	AddSText(20, 1, _("I have this item for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);

	boyitem._iStatFlag = MyPlayer->CanUseItem(boyitem);
	UiFlags itemColor = boyitem.getTextColorWithStatCheck();
	AddSText(20, 10, boyitem.getName(), itemColor, true, boyitem._iCurs, true);
	if (gbIsHellfire)
		AddSTextVal(10, boyitem._iIvalue - (boyitem._iIvalue / 4));
	else
		AddSTextVal(10, boyitem._iIvalue + (boyitem._iIvalue / 2));
	PrintStoreItem(boyitem, 11, itemColor, true);

	{
		// Add a Leave button. Unlike the other item list back buttons,
		// this one has different text and different layout in LargerSmallFont locales.
		const int line = BackButtonLine();
		AddSLine(line - 1);
		AddSText(0, line, _("Leave"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
		stext[line]._syoff = 6;
	}
}

void HealPlayer()
{
	Player &myPlayer = *MyPlayer;

	if (myPlayer._pHitPoints != myPlayer._pMaxHP) {
		PlaySFX(IS_CAST8);
	}
	myPlayer._pHitPoints = myPlayer._pMaxHP;
	myPlayer._pHPBase = myPlayer._pMaxHPBase;
	RedrawComponent(PanelDrawComponent::Health);
}

void StartHealer()
{
	HealPlayer();
	stextsize = false;
	stextscrl = false;
	AddSText(0, TownerTitleLine, _("Healer's home"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerPromptLine, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerTalkLine, _("Talk to Pepin"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, TownerDoorLine, _("Enter Shop"), UiFlags::ColorWhite | UiFlags::AlignCenter, true); // a grid shop, like the others' doors
	AddSText(0, TownerLeaveLine, _("Leave Pepin"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	storenumh = 20;
}

void ScrollHealerBuy(int idx)
{
	ClearSText(5, 21);
	stextup = 5;
	// Bounded, and idx advances unconditionally - see the note in ScrollSmithBuy. Pepin's list is
	// the third copy of the same function and carried the same two defects.
	for (int l = 5; l < 20 && idx < static_cast<int>(std::size(healitem)); l += 4, idx++) {
		if (!healitem[idx].isEmpty()) {
			UiFlags itemColor = healitem[idx].getTextColorWithStatCheck();
			AddSText(20, l, healitem[idx].getName(), itemColor, true, healitem[idx]._iCurs, true);
			AddSTextVal(l, healitem[idx]._iIvalue);
			PrintStoreItem(healitem[idx], l + 1, itemColor, true);
			stextdown = l;
		}
	}

	if (stextsel != -1 && !stext[stextsel].isSelectable() && stextsel != BackButtonLine())
		stextsel = stextdown;
}

void StartHealerBuy()
{
	stextsize = true;
	stextscrl = true;
	stextsval = 0;

	RenderGold = true;
	AddSText(20, 1, _("I have these items for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);

	ScrollHealerBuy(stextsval);
	AddItemListBackButton();

	storenumh = 0;
	for (Item &item : healitem) {
		if (item.isEmpty())
			continue;

		item._iStatFlag = MyPlayer->CanUseItem(item);
		storenumh++;
	}

	stextsmax = std::max(storenumh - 4, 0);
}

void StartStoryteller()
{
	stextsize = false;
	stextscrl = false;
	AddSText(0, TownerTitleLine, _("The Town Elder"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerPromptLine, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerTalkLine, _("Talk to Cain"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, TownerDoorLine, _("Identify an item"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, TownerLeaveLine, _("Leave Cain"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
}

bool IdItemOk(Item *i)
{
	if (i->isEmpty()) {
		return false;
	}
	if (i->_iMagical == ITEM_QUALITY_NORMAL) {
		return false;
	}
	return !i->_iIdentified;
}

void AddStoreHoldId(Item itm, int8_t i, int8_t tabIdx = -1)
{
	storehold[storenumh] = itm;
	storehold[storenumh]._ivalue = 100;
	storehold[storenumh]._iIvalue = 100;
	storehidx[storenumh] = i;
	storehTabIdx[storenumh] = tabIdx;
	storenumh++;
}

void StartStorytellerIdentify()
{
	bool idok = false;
	stextsize = true;
	storenumh = 0;

	for (auto &item : storehold) {
		item.clear();
	}

	Player &myPlayer = *MyPlayer;

	for (int k = 0; k < NumIdentifiableBodySlots; k++) {
		Item &worn = myPlayer.InvBody[IdentifiableBodySlots[k]];
		if (IdItemOk(&worn)) {
			idok = true;
			AddStoreHoldId(worn, static_cast<int8_t>(-(k + 1)));
		}
	}

	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (storenumh >= StoreHoldCapacity)
			break;
		auto &item = myPlayer.InvList[i];
		if (IdItemOk(&item)) {
			idok = true;
			AddStoreHoldId(item, i);
		}
	}

	// Oracool Tabbed Inventory: an unidentified item stored in an extra tab is just as
	// identifiable by Cain as anything in the original backpack.
	if (TabbedInventoryEnabled()) {
		for (int t = 0; t < Player::NumExtraInventoryTabs && storenumh < StoreHoldCapacity; t++) {
			for (int i = 0; i < myPlayer._pNumInvTab[t] && storenumh < StoreHoldCapacity; i++) {
				auto &item = myPlayer.InvTabList[t][i];
				if (IdItemOk(&item)) {
					idok = true;
					AddStoreHoldId(item, static_cast<int8_t>(i), static_cast<int8_t>(t));
				}
			}
		}
	}

	if (!idok) {
		stextscrl = false;

		RenderGold = true;
		AddSText(20, 1, _("You have nothing to identify."), UiFlags::ColorWhitegold, false);
		AddSLine(3);
		AddItemListBackButton(/*selectable=*/true);
		return;
	}

	stextscrl = true;
	stextsval = 0;
	// Oracool: matches the scroll-bound formula every other tab-aware sell/repair list already
	// uses (e.g. PopulateSellList's callers) - myPlayer._pNumInv alone would under-count once
	// extra-tab items are appended to storehold past the backpack's own count.
	stextsmax = std::max(storenumh - 4, 0);

	RenderGold = true;
	AddSText(20, 1, _("Identify which item?"), UiFlags::ColorWhitegold, false);
	AddSLine(3);

	ScrollSmithSell(stextsval);
	AddItemListBackButton();
}

void StartStorytellerIdentifyShow(Item &item)
{
	StartStore(stextshold);
	stextscrl = false;
	ClearSText(5, 23);

	UiFlags itemColor = item.getTextColorWithStatCheck();

	AddSText(0, 7, _("This item is:"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
	AddSText(20, 11, item.getName(), itemColor, false);
	PrintStoreItem(item, 12, itemColor);
	AddSText(0, 18, _("Done"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

void StartTalk()
{
	int la;

	stextsize = false;
	stextscrl = false;
	AddSText(0, 2, fmt::format(fmt::runtime(_("Talk to {:s}")), _(TownerNames[talker])), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSLine(5);
	if (gbIsSpawn) {
		AddSText(0, 10, fmt::format(fmt::runtime(_("Talking to {:s}")), _(TownerNames[talker])), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		AddSText(0, 12, _("is not available"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		AddSText(0, 14, _("in the shareware"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		AddSText(0, 16, _("version"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		AddOptionsBackButton();
		return;
	}

	int sn = 0;
	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_ACTIVE && QuestDialogTable[talker][quest._qidx] != TEXT_NONE && quest._qlog)
			sn++;
	}

	if (sn > 6) {
		sn = 14 - (sn / 2);
		la = 1;
	} else {
		sn = 15 - sn;
		la = 2;
	}

	int sn2 = sn - 2;

	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_ACTIVE && QuestDialogTable[talker][quest._qidx] != TEXT_NONE && quest._qlog) {
			AddSText(0, sn, _(QuestsData[quest._qidx]._qlstr), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			sn += la;
		}
	}
	AddSText(0, sn2, _("Gossip"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddOptionsBackButton();
}

void StartTavern()
{
	stextsize = false;
	stextscrl = false;
	AddSText(0, TownerTitleLine, _("Rising Sun"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerPromptLine, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerTalkLine, _("Talk to Ogden"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	// Ogden's Table (Levski's Cube plan, decision D8, 2026-09-20): the stones and sockets - refine
	// gems, ascend runes, temper jewels, recolour gems, free and punch sockets.
	AddSText(0, TownerDoorLine, _("Enter Shop"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, TownerLeaveLine, _("Leave Ogden"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	storenumh = 20;
}

void StartBarmaid()
{
	stextsize = false;
	stextscrl = false;
	AddSText(0, TownerTitleLine, _("Gillian"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerPromptLine, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerTalkLine, _("Talk to Gillian"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	// Gillian's Hearth (Levski's Cube plan, decision D8, 2026-09-20): charms reworked, set pieces
	// recast, magic enriched, shards cleansed.
	AddSText(0, TownerDoorLine, _("Enter Shop"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	// Oracool: user request - the physical Stash Chest in town (see OperateStashChest in
	// objects.cpp) replaces Gillian as the way to access and sort the Stash.
	AddSText(0, TownerLeaveLine, _("Leave Gillian"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	storenumh = 20;
}

void StartDrunk()
{
	stextsize = false;
	stextscrl = false;
	AddSText(0, TownerTitleLine, _("Farnham"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerPromptLine, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, TownerTalkLine, _("Talk to Farnham"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, TownerLeaveLine, _("Leave Farnham"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	storenumh = 20;
}

void SmithEnter()
{
	const std::vector<TalkID> entries = SmithMenuEntries();
	TalkID selected = TalkID::None;
	if (stextsel != TownerLeaveLine) {
		const int offset = stextsel - SmithMenuFirstLine(entries.size());
		if (offset < 0 || offset % 2 != 0 || static_cast<size_t>(offset / 2) >= entries.size())
			return;
		selected = entries[offset / 2];
		if (selected == TalkID::None)
			return; // the leave entry is on TownerLeaveLine, not in the list's run
	}
	switch (selected) {
	case TalkID::Gossip:
		talker = TOWN_SMITH;
		stextlhold = stextsel;
		stextshold = TalkID::Smith;
		StartStore(TalkID::Gossip);
		break;
	case TalkID::SmithBuy:
		StartStore(TalkID::SmithBuy);
		break;
	case TalkID::SmithPremiumBuy:
		StartStore(TalkID::SmithPremiumBuy);
		break;
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithSetBuy:
		StartStore(selected);
		break;
	case TalkID::SmithSell:
		StartStore(TalkID::SmithSell);
		break;
	case TalkID::SmithRepair:
		StartStore(TalkID::SmithRepair);
		break;
	case TalkID::SmithConsumables:
		StartStore(TalkID::SmithConsumables);
		break;
	case TalkID::SmithRecharge:
		StartStore(TalkID::SmithRecharge);
		break;
	case TalkID::SmithTransmute:
		// A real store SCREEN that happens to draw a painted page (2026-09-21). stextflag stays on the
		// tab, which is what hands it the walk-away, the talk-to-towner close, ESC and the overlap rule
		// that every other tab already had; UpdateStoreState keeps the page and the flag in step.
		stextflag = TalkID::SmithTransmute;
		oracool::OpenLevskiWindowFor(oracool::TransmuteHost::Smith);
		break;
	case TalkID::None:
		stextflag = TalkID::None;
		break;
	default:
		break;
	}
}

/**
 * @brief Purchases an item from the smith.
 */
/**
 * @brief Buys @p item and removes stock entry @p idx, compacting the array.
 *
 * The index is a PARAMETER now. Every transaction in this file used to derive it from
 * `stextvhold + ((stextlhold - stextup) / 4)` - which is not "which item" but "where the text list
 * happened to be scrolled", and that coupling is the direct cause of the store crash fixed at
 * v1.8.90 and the three stalled walks found at v1.8.94.
 *
 * A grid shop has no text lines to arithmetic on, so it needs to say which item plainly. Splitting
 * the two apart is worth doing for its own sake: the transaction should not be able to disagree
 * with the display about what is being sold.
 */
/**
 * @brief Removes entry @p idx from a fixed vendor array and closes the gap behind it.
 *
 * The three vendors each had their own copy of this, and all three were the same bug (external
 * audit, 2026-08-27). The shape was:
 *
 *     for (; !stock[idx + 1].isEmpty(); idx++) stock[idx] = std::move(stock[idx + 1]);
 *
 * which walks until it finds an EMPTY SLOT, and so reads `stock[capacity]` the moment the array has
 * none - an out-of-bounds read, followed by a write of whatever it found into the last real slot.
 * Each vendor had a special case for buying the very last entry, which is the one full-array
 * purchase that happened not to trigger it.
 *
 * A full array was impossible when those loops were written and is ORDINARY now: the reserved blocks
 * added on 2026-08-27 are sized to fill the shelf exactly. Adria is 48 rolled + 7 salvage + 12
 * socketables + 10 books + 8 staves + 5 rare staves = 90 = WITCH_ITEMS. The empty sentinel those
 * loops relied on is gone. SortVendor's own capacity bound was already evidence that a full array is
 * an expected state rather than an impossible one - it was given that bound for the same reason, and
 * these three were not looked at.
 *
 * Bounded by CAPACITY instead. The gap-closing is unchanged; what changed is that it can no longer
 * run off the end when there is no hole to stop at.
 */
// Defined below the anonymous namespace, since stores.h exports it for the full-array regression
// test. The declaration in the header is what lets the three purchase paths above reach it.

void SmithBuyItemAt(Item &item, int idx)
{
	// The index is validated BEFORE the money moves. A stale or out-of-range row must cost nothing
	// rather than charge for a purchase the removal below then declines to make - the same ordering
	// SmithBuyPItemAt was given after the 2026-08-15 self-audit.
	if (idx < 0 || idx >= SMITH_ITEMS)
		return;
	TakePlrsMoney(item._iIvalue);
	// Potions only, vanilla's reason (they stack with found ones): a bought charm or tiered base lost its effect line
	// and its Tier and Item Level for good - Cain refuses a plain item (round 12 audit, v1.12.237).
	if (item._iMagical == ITEM_QUALITY_NORMAL && item.isPotion())
		item._iIdentified = false;
	StoreAutoPlace(item, true);
	RemoveFromVendorStock(smithitem, SMITH_ITEMS, idx);
	CalcPlrInv(*MyPlayer, true);
}

/** @brief The text-store's caller: it still knows the index only as a scroll position. */
void SmithBuyItem(Item &item)
{
	SmithBuyItemAt(item, stextvhold + ((stextlhold - stextup) / 4));
}

void SmithBuyEnter()
{
	if (stextsel == BackButtonLine()) {
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithBuy);
		return;
	}

	stextlhold = stextsel;
	stextvhold = stextsval;
	stextshold = TalkID::SmithBuy;

	int idx = stextsval + ((stextsel - stextup) / 4);

	// Oracool bug fix: user report - buying out Griswold's entire basic stock let you keep
	// "buying", which put an indestructible phantom item in the inventory (a zeroed Item, drawn
	// with the low-index potion sprite, that vanishes when clicked).
	//
	// Vanilla bounced out of this screen when the list was empty, and an earlier Oracool change
	// removed that on the grounds that the empty screen renders fine - which it does. What it does
	// not do is stop this line mapping a still-selected row onto an empty slot, which then gets
	// confirmed and auto-placed. Guarding the index keeps the nicer empty-list screen and closes
	// the hole; the same applies to any row that scrolled past the end of the list.
	if (idx < 0 || idx >= SMITH_ITEMS || smithitem[idx].isEmpty())
		return;

	if (!PlayerCanAfford(smithitem[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	if (!StoreAutoPlace(smithitem[idx], false)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	StoreItem = smithitem[idx];
	StartStore(TalkID::Confirm);
}

/**
 * @brief Purchases a premium item from the smith.
 */
void SmithBuyPItemAt(Item &item, int idx)
{
	// The slot scan runs FIRST (self-audit, 2026-08-15), for two reasons. It is bounded now - the
	// old loop's only condition was the skip count, so a stale selected row (the premium list
	// shrinks on every purchase, and ConfirmEnter restores the old selection over the rebuilt
	// screen) walked isEmpty() past the end of the array; and this is the copy of the scan that
	// CLEARS a slot, so overrunning would zero whatever lives after it. And it runs before the
	// money: bailing on a stale row after TakePlrsMoney would be a purchase with no goods.
	int xx = -1;
	for (int i = 0; i < SMITH_PREMIUM_ITEMS && idx >= 0; i++) {
		if (!premiumitems[i].isEmpty()) {
			idx--;
			xx = i;
		}
	}
	if (xx < 0 || idx >= 0)
		return; // stale row: nothing charged, nothing placed, nothing cleared

	TakePlrsMoney(item._iIvalue);
	// Potions only, vanilla's reason (they stack with found ones): a bought charm or tiered base lost its effect line
	// and its Tier and Item Level for good - Cain refuses a plain item (round 12 audit, v1.12.237).
	if (item._iMagical == ITEM_QUALITY_NORMAL && item.isPotion())
		item._iIdentified = false;
	StoreAutoPlace(item, true);

	premiumitems[xx].clear();
	// EXACTLY the sold slot, and only if the replacement still fits. SpawnPremium used to be called
	// here, which refills every empty vanilla slot at once and so resurrected every hole the
	// one-page trim had made (external audit of v1.9.97, finding 1).
	//
	// The fit test is asked before the shelf is allowed to keep the replacement, rather than left to
	// the trim afterwards: premium items are not all the same size, so a larger replacement can push
	// a DIFFERENT item off the page, and a trim run at that point would delete that item to pay for
	// this one. Declining the replacement costs the shelf one slot until the next Refresh; the trim
	// would have cost the player an item they had chosen not to buy yet.
	RestockOnePremiumSlot(xx, *MyPlayer);
	if (!oracool::ShopStockFitsOnePage(TalkID::SmithPremiumBuy)) {
		premiumitems[xx].clear();
		RecountPremiumStock();
	}
}

/** @brief The text-store's caller: it still knows the index only as a scroll position. */
void SmithBuyPItem(Item &item)
{
	SmithBuyPItemAt(item, stextvhold + ((stextlhold - stextup) / 4));
}

void BuyCuratedShelfItemAt(CuratedShelf shelf, Item &item, int idx)
{
	// Clamped before the shift loop below uses it as a write index (self-audit, 2026-08-15): a
	// negative stale index would write before the array. Checked before the money, like
	// SmithBuyPItem, so a stale row costs nothing rather than costing gold for no goods.
	if (idx < 0 || idx >= CuratedShelfCapacity)
		return;
	Item *items = ShelfItems(shelf);
	TakePlrsMoney(item._iIvalue);
	// The Unique shelf's multiplier is its PRICE, written onto the shelf copy: the bought item takes its own value back,
	// or it sold for five times a found copy's and repaired at twenty times (round 9 audit, v1.12.234).
	if (shelf == CuratedShelf::Unique && item._iMagical == ITEM_QUALITY_UNIQUE && item._iUid >= 0
	    && static_cast<size_t>(item._iUid) < UniqueItemCount)
		item._iIvalue = oracool::ScaleValueForBaseTier(UniqueItems[item._iUid].UIValue, item._iOracoolBaseTier);
	// The Set shelf's salvage floor is its PRICE too: the bought piece takes back its base's tiered value (_ivalue, which
	// ApplyBaseTier scales alongside), or it sold for 675 and repaired at 60 times a found copy's (round 18 audit).
	if (shelf == CuratedShelf::Set)
		item._iIvalue = std::min(item._iIvalue, std::max(item._ivalue, 0));
	StoreAutoPlace(item, true);
	// The bought item is REMOVED and not replaced - that is what makes a shelf curated. The stock
	// closes up behind it so the list stays dense, which is what the scroll arithmetic assumes.
	for (; idx < CuratedShelfCapacity - 1; ++idx)
		items[idx] = std::move(items[idx + 1]);
	items[CuratedShelfCapacity - 1].clear();
	CalcPlrInv(*MyPlayer, true);
}

/** @brief The text-store's caller: it still knows the index only as a scroll position. */
void BuyCuratedShelfItem(CuratedShelf shelf, Item &item)
{
	BuyCuratedShelfItemAt(shelf, item, stextvhold + ((stextlhold - stextup) / 4));
}

void CuratedShelfBuyEnter(CuratedShelf shelf)
{
	const TalkID id = TalkIdForCuratedShelf(shelf);
	if (stextsel == BackButtonLine()) {
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(id);
		return;
	}

	Item *items = ShelfItems(shelf);
	stextshold = id;
	stextlhold = stextsel;
	stextvhold = stextsval;
	const int idx = stextsval + ((stextsel - stextup) / 4);
	// Stale-row guard - see SmithSellEnter (self-audit, 2026-08-15). A curated shelf shrinks on
	// every purchase, and the completion's shift loop would start from this index raw.
	if (idx < 0 || idx >= CuratedShelfCapacity || items[idx].isEmpty())
		return;
	if (!PlayerCanAfford(items[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}
	if (!StoreAutoPlace(items[idx], false)) {
		StartStore(TalkID::NoRoom);
		return;
	}
	StoreItem = items[idx];
	StartStore(TalkID::Confirm);
}

std::vector<std::string> GetPremiumRefreshTargets()
{
	std::vector<std::string> targets;
	const std::string configured = sgOptions.Oracool.refreshUntilItemNames;
	size_t begin = 0;
	while (begin <= configured.size()) {
		const size_t separator = configured.find(';', begin);
		const size_t end = separator == std::string::npos ? configured.size() : separator;
		size_t first = begin;
		while (first < end && std::isspace(static_cast<unsigned char>(configured[first])) != 0)
			++first;
		size_t last = end;
		while (last > first && std::isspace(static_cast<unsigned char>(configured[last - 1])) != 0)
			--last;
		if (first != last)
			targets.push_back(AsciiStrToLower(string_view { configured.data() + first, last - first }));
		if (separator == std::string::npos)
			break;
		begin = separator + 1;
	}
	return targets;
}

std::string RefreshPremiumUntilTarget()
{
	const std::vector<std::string> targets = GetPremiumRefreshTargets();
	if (targets.empty())
		return std::string(_("Refresh Until has no item names configured."));

	constexpr int MaximumAttempts = 100000;
	const int timeoutSeconds = *sgOptions.Oracool.refreshUntilTimeoutSeconds;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
	for (int attempt = 1; attempt <= MaximumAttempts; ++attempt) {
		for (Item &item : premiumitems)
			item.clear();
		numpremium = 0;
		SpawnPremium(*MyPlayer);
		// Trimmed BEFORE the scan, and therefore on every exit this loop has - found, timed out, or
		// the safety limit. Without it the search answered about the backing array rather than about
		// the shelf, so it could report "Found X" for an item PlaceStock cannot fit and the player
		// would open the tab to no such item; and whichever generation happened to be current when
		// the loop gave up was left untrimmed, which is the hidden reserve the one-page rule exists
		// to prevent (external audit of v1.9.97, finding 1).
		oracool::TrimShopStockToOnePage(TalkID::SmithPremiumBuy);
		RecountPremiumStock();

		for (const Item &item : premiumitems) {
			if (item.isEmpty())
				continue;
			const std::string itemName = AsciiStrToLower(item.getName());
			if (std::find(targets.begin(), targets.end(), itemName) != targets.end())
				return fmt::format(fmt::runtime(_("Found {:s} after {:d} refreshes.")), std::string(item.getName()), attempt);
		}

		if (timeoutSeconds > 0 && std::chrono::steady_clock::now() >= deadline)
			return fmt::format(fmt::runtime(_("Refresh Until timed out after {:d} refreshes.")), attempt);
	}

	return std::string(_("Refresh Until stopped at the 100,000-refresh safety limit."));
}

/**
 * @brief Oracool: user request - opens a text-entry overlay (reusing the same TextInputState
 * machinery the gold split/withdraw dialogs already use, just without NumberInputState's
 * digits-only filter) so the Refresh Until target can be typed in-game instead of hand-edited
 * into diablo.ini. Types directly into sgOptions.Oracool.refreshUntilItemNames itself (the exact
 * field GetPremiumRefreshTargets/RefreshPremiumUntilTarget already read), so no new option or
 * save-format change is needed - what's typed here both drives this search immediately and
 * persists to the INI on the next options save, same as any other option.
 */
void StartRefreshUntilPrompt()
{
	const Point uiPosition = GetUIRectangle().position;
	const Point start { uiPosition.x + 190, uiPosition.y + 210 };
	SDL_Rect rect = MakeSdlRect(start.x, start.y, 260, 20);
	SDL_SetTextInputRect(&rect);

	IsRefreshUntilPromptOpen = true;
	RefreshUntilNamesBeforeEdit = sgOptions.Oracool.refreshUntilItemNames;
	RefreshUntilPromptInputState.emplace(TextInputState::Options {
	    /*value=*/sgOptions.Oracool.refreshUntilItemNames,
	    /*cursor=*/&RefreshUntilPromptCursor,
	    /*maxLength=*/sizeof(sgOptions.Oracool.refreshUntilItemNames) - 1,
	});
	SDL_StartTextInput();
}

void SmithPremiumBuyEnter()
{
	if (stextsel == BackButtonLine()) {
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithPremiumBuy);
		return;
	}
	if (*sgOptions.Oracool.griswoldPremiumRefresh && !gbIsMultiplayer && stextsel == PremiumRefreshLine()) {
		for (Item &item : premiumitems)
			item.clear();
		numpremium = 0;
		SpawnPremium(*MyPlayer);
		// Same one-page trim a fresh town gives it. The post-purchase path needs none: it restocks
		// exactly the sold slot and puts the replacement back if it does not fit (see
		// RestockOnePremiumSlot and SmithBuyPItemAt).
		oracool::TrimShopStockToOnePage(TalkID::SmithPremiumBuy);
		RecountPremiumStock();
		StartStore(TalkID::SmithPremiumBuy);
		stextsel = PremiumRefreshLine();
		return;
	}
	if (*sgOptions.Oracool.refreshUntilButton && !gbIsMultiplayer && stextsel == PremiumRefreshUntilLine()) {
		// Oracool: user request - prompt for the target name in-game (see StartRefreshUntilPrompt)
		// instead of immediately running the search against whatever's already saved in
		// diablo.ini. RefreshUntilPromptKeyPress runs the actual search once the player confirms.
		StartRefreshUntilPrompt();
		return;
	}

	stextshold = TalkID::SmithPremiumBuy;
	stextlhold = stextsel;
	stextvhold = stextsval;

	int xx = stextsval + ((stextsel - stextup) / 4);
	// Self-audit (2026-08-15): the scan is bounded now. It walks the sparse premiumitems array
	// counting non-empty slots until it has skipped `xx` of them - and its loop condition was `xx >=
	// 0` alone, so a stale selected row (the list SHRINKS when a premium item is bought, and
	// ConfirmEnter restores the old selection over the rebuilt screen) asked it to find more items
	// than exist, and it kept reading isEmpty() past the end of the array until the bytes beyond it
	// happened to satisfy the count. Same stale-row family as the SmithBuyEnter phantom item, with
	// an out-of-bounds read instead of a phantom.
	int idx = -1;
	for (int i = 0; i < SMITH_PREMIUM_ITEMS && xx >= 0; i++) {
		if (!premiumitems[i].isEmpty()) {
			xx--;
			idx = i;
		}
	}
	if (idx < 0 || xx >= 0)
		return; // stale row: fewer premium items exist than the selection asks to skip

	if (!PlayerCanAfford(premiumitems[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	if (!StoreAutoPlace(premiumitems[idx], false)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	StoreItem = premiumitems[idx];
	StartStore(TalkID::Confirm);
}

/**
 * @brief The sale price of a storehold DISPLAY copy.
 *
 * NOT GetItemSellValue, which would quarter it a second time: the sell-list builder already
 * overwrote this copy's `_ivalue`/`_iIvalue` with the price so the row could show one (see
 * StartSmithSell). Named so that the three list-path gates read as "the price" rather than as a
 * raw field access that happens to hold it.
 */
int StoreHoldSalePrice(const Item &displayCopy)
{
	return displayCopy._iIvalue;
}

/**
 * @brief Whether every coin of a sale at @p price has somewhere to go.
 *
 * @param price the EXACT amount that will be credited - the same number the caller passes to
 *        CreditSaleProceeds. It is a parameter rather than something read back off the item
 *        because the two disagreed (external audit of v1.9.97, finding 3): this used to take
 *        `item._iIvalue`, which is the sale price only on a storehold DISPLAY copy (the list
 *        builder overwrites it, see StartSmithSell) and is the item's full value everywhere
 *        else. The two direct gestures pass a pristine item, so they gated on four times the
 *        money for an ordinary item - harmlessly strict - and on a fraction of it for a stack,
 *        because GetItemSellValue multiplies a stackable consumable by its count. A 99-potion
 *        stack was approved against one potion's value and then paid at ninety-nine quarters of
 *        it, and at a saturated Stash the difference is gold that does not exist anywhere.
 *
 * @param itemFreeingCells the item whose backpack cells the sale VACATES, which can then hold
 *        gold, or nullptr when the sale frees nothing. An item sold from the cursor was never
 *        occupying a cell, and claiming its cells is exactly the over-count that would let a
 *        held-item sale be approved and then lose the remainder (external audit of v1.9.92,
 *        finding 4).
 */
bool StoreGoldFit(int price, const Item *itemFreeingCells)
{
	int cost = price;

	Size itemSize = itemFreeingCells != nullptr ? GetInventorySize(*itemFreeingCells) : Size { 0, 0 };
	// 64-bit throughout. The cell product alone reaches 10 * 100,000,000 for the largest items, and
	// adding RoomForGold's answer (up to 7,000,000,000 on an empty backpack) overflowed an int -
	// so the gate that decides whether a sale FITS was itself computing undefined behaviour, on the
	// commonest inventory state there is (external audit, 2026-08-25).
	const int64_t itemRoomForGold = static_cast<int64_t>(itemSize.width) * itemSize.height * MaxGold;

	if (cost <= itemRoomForGold) {
		return true;
	}

	// The STASH counts too (external audit of v1.9.88, finding 7). This gate models the backpack
	// alone, and in single-player the proceeds go to the Stash pool FIRST - so it was answering a
	// question about somewhere the money mostly does not land. Two consequences, opposite in sign:
	// a sale worth more than the backpack could hold was refused even with a near-empty pool waiting
	// for it, and at the pool's INT_MAX cap the gate had nothing to say about the one case where the
	// money really can be lost.
	int64_t room = itemRoomForGold + RoomForGold();
	if (oracool::IsSinglePlayer() && !StashFileRefused)
		room += static_cast<int64_t>(std::numeric_limits<int>::max()) - Stash.gold;
	return cost <= room;
}

/**
 * @brief Sells an item from the player's inventory or belt.
 */
void StoreSellItemAt(int idx)
{
	Player &myPlayer = *MyPlayer;

	// Taken BEFORE the removal, because the removal is what destroys it. storehold's copy is NOT
	// this item: the list builder overwrote its `_ivalue` and `_iIvalue` with the sale price so the
	// row could display one, and recording that copy is what sent a devalued item to the buyback
	// shelf (user, 2026-08-27). The price is read from the display copy, where it belongs; the ITEM
	// is read from the player, where it is still whole.
	const Item pristine = [&myPlayer, idx]() -> Item {
		if (storehTabIdx[idx] >= 0)
			return myPlayer.InvTabList[storehTabIdx[idx]][storehidx[idx]];
		if (storehidx[idx] >= 0)
			return myPlayer.InvList[storehidx[idx]];
		return myPlayer.SpdList[-(storehidx[idx] + 1)];
	}();

	if (storehTabIdx[idx] >= 0)
		RemoveExtraTabItem(myPlayer, storehTabIdx[idx], storehidx[idx]);
	else if (storehidx[idx] >= 0)
		myPlayer.RemoveInvItem(storehidx[idx]);
	else
		myPlayer.RemoveSpdBarItem(-(storehidx[idx] + 1));

	// Copied BEFORE the compaction below, which overwrites storehold[idx] with its successor.
	const Item sold = storehold[idx];
	int cost = StoreHoldSalePrice(sold);
	storenumh--;
	if (idx != storenumh) {
		while (idx < storenumh) {
			storehold[idx] = storehold[idx + 1];
			storehidx[idx] = storehidx[idx + 1];
			storehTabIdx[idx] = storehTabIdx[idx + 1];
			idx++;
		}
	}

	RecordSale(pristine, cost);
	CreditSaleProceeds(cost);
	oracool::ScheduleAutoSaveForStoreTransaction();
}

/** @brief The text-store's caller: it still knows the index only as a scroll position. */
void StoreSellItem()
{
	StoreSellItemAt(stextvhold + ((stextlhold - stextup) / 4));
}

/**
 * @brief Sells Griswold everything he will buy, then returns to @p returnTo.
 *
 * @p returnTo is the Sold tab for its own Sell all, and whichever buy tab the button was pressed on
 * since 2026-09-13 (user: "i want to be able to sell all items from any tab of griswold shop") - the
 * player asked to empty their pack, not to be moved to another shelf.
 */
void SmithSellAllItems(TalkID returnTo = TalkID::SmithSell)
{
	// The gold drop, once at the end rather than once per item (user, 2026-09-21: "When i click Sell
	// All it doesnt play the proper gold sound. Substitute the current sound with gold drop sound").
	// It sold in silence before - the only sound was the button's own click - and per item would be a
	// rattle of twenty overlapping coins, so it sounds once, and only if something was actually sold.
	bool soldAnything = false;
	while (true) {
		StartSmithSell();
		// The backpack and the belt, never pages 2-10 (user, 2026-09-27: "fix the decisions for me too"). The button says
		// "sells everything in your backpack", and it took the extra pages too - where crafting stock is kept, gems,
		// runes and shards Griswold buys - in one click. One item from a page still sells from the list.
		// The CHEAPEST first (the list is sorted dearest first): the buyback holds the last 40 sales, and selling dearest
		// first pushed the valuable ones off it for good on a full backpack (round 23 audit, v1.12.248).
		int next = -1;
		for (int i = storenumh - 1; i >= 0 && next < 0; i--) {
			// Never a socketed item with stones in it: the price is the base's alone, and an Enigma went for a white body's
			// quarter with its runes (round 24 audit, v1.12.249). One at a time from the list still sells it.
			if (storehTabIdx[i] < 0 && storehold[i].socketedCount() == 0)
				next = i;
		}
		if (next < 0)
			break;
		if (!StoreGoldFit(StoreHoldSalePrice(storehold[next]), &storehold[next])) {
			// The No Room screen returns to stextshold; a buy tab has no text line to restore.
			stextshold = returnTo;
			stextlhold = returnTo == TalkID::SmithSell ? SmithSellAllLine() : 0;
			stextvhold = 0;
			// Sounds before the refusal screen too: the items sold up to that point really were sold,
			// and leaving in silence would read as nothing having happened.
			if (soldAnything)
				PlaySFX(IS_GOLD);
			StartStore(TalkID::NoRoom);
			return;
		}

		// Rebuilding the list after every removal is intentional: inventory removal compacts
		// InvList, so every later source index must be recalculated before it is used.
		StoreSellItemAt(next);
		soldAnything = true;
	}

	if (soldAnything) {
		PlaySFX(IS_GOLD);
		// Once, as every other sale path does after its removal: a sold charm kept its life on the hero (round 29 audit).
		CalcPlrInvKeepingLife(*MyPlayer);
	}
	StartStore(returnTo);
}

void SmithSellEnter()
{
	if (!gbIsMultiplayer && stextsel == SmithSellAllLine()) {
		SmithSellAllItems();
		return;
	}
	if (stextsel == BackButtonLine()) {
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithSell);
		return;
	}

	stextlhold = stextsel;
	stextshold = TalkID::SmithSell;
	stextvhold = stextsval;

	int idx = stextsval + ((stextsel - stextup) / 4);

	// Self-audit (2026-08-15): the same stale-row guard SmithBuyEnter got when the user reported
	// the phantom-item purchase, applied to every storehold-based Enter. ConfirmEnter restores the
	// old selected row after the list rebuilds, so selling the last item leaves the selection on a
	// row past the new count - and here a stale row is worse than a phantom: storehold entries past
	// storenumh hold whatever an earlier screen left in them, and their storehidx feeds fixed-size
	// arrays (InvList is 40 items; the index can reach 47).
	if (idx < 0 || idx >= storenumh)
		return;

	if (!StoreGoldFit(StoreHoldSalePrice(storehold[idx]), &storehold[idx])) {
		StartStore(TalkID::NoRoom);
		return;
	}

	StoreItem = storehold[idx];
	StartStore(TalkID::Confirm);
}

/**
 * @brief Repairs an item in the player's inventory or body in the smith.
 */
void SmithRepairItemAt(int price, int idx)
{
	storehold[idx]._iDurability = storehold[idx]._iMaxDur;

	int8_t i = storehidx[idx];

	Player &myPlayer = *MyPlayer;

	if (i < 0) {
		// Reactivates a broken (0-durability, left equipped rather than destroyed) item -
		// see BreakOrRemoveEquipment/CalcSelfItems. Harmless to clear unconditionally even
		// if the item was never broken in the first place. The slot comes back out of the
		// same RepairableBodySlots table StartSmithRepair encoded it from.
		const int k = -i - 1;
		if (k < NumRepairableBodySlots) {
			Item &worn = myPlayer.InvBody[RepairableBodySlots[k]];
			worn._iDurability = worn._iMaxDur;
			worn._iOracoolBroken = false;
		}
		TakePlrsMoney(price);
		CalcPlrInv(myPlayer, true);
		oracool::ScheduleAutoSaveForStoreTransaction();
		return;
	}

	myPlayer.InvList[i]._iDurability = myPlayer.InvList[i]._iMaxDur;
	// ...and whole. An item that broke while worn and was then taken off still carries the flag, and
	// this path mended the durability alone (user, 2026-09-11: a repaired shield "has X on it and
	// doesnt appear as shield when i equip it").
	myPlayer.InvList[i]._iOracoolBroken = false;
	TakePlrsMoney(price);
	oracool::ScheduleAutoSaveForStoreTransaction();
}

/** @brief The text-store's caller: it still knows the index only as a scroll position. */
void SmithRepairItem(int price)
{
	SmithRepairItemAt(price, stextvhold + ((stextlhold - stextup) / 4));
}

/**
 * @brief Oracool: user request - "Repair all" button, same list-rebuild-and-repeat pattern as the
 * existing SmithSellAllItems. Repairs storehold[0] over and over (StartSmithRepair already sorts
 * the list by descending repair cost, so this repairs the most expensive items first) until
 * either nothing's left to repair or the player can't afford the next one, at which point it stops
 * rather than skipping ahead to a cheaper item - matching how Sell All stops instead of skipping
 * past an item that won't fit.
 */
void SmithRepairAllItems()
{
	while (true) {
		StartSmithRepair();
		if (storenumh == 0)
			break;
		if (!PlayerCanAfford(storehold[0]._iIvalue)) {
			stextshold = TalkID::SmithRepair;
			stextlhold = SmithRepairAllLine();
			stextvhold = 0;
			StartStore(TalkID::NoMoney);
			return;
		}

		// Repair the head of the list by naming it. This used to fake up a scroll position
		// (stextvhold = 0, stextlhold = stextup) purely so the old SmithRepairItem would derive
		// index 0 back out of it.
		SmithRepairItemAt(storehold[0]._iIvalue, 0);
	}

	StartStore(TalkID::SmithRepair);
}

void SmithRepairEnter()
{
	if (!gbIsMultiplayer && stextsel == SmithRepairAllLine()) {
		SmithRepairAllItems();
		return;
	}
	if (stextsel == BackButtonLine()) {
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithRepair);
		return;
	}

	stextshold = TalkID::SmithRepair;
	stextlhold = stextsel;
	stextvhold = stextsval;

	int idx = stextsval + ((stextsel - stextup) / 4);

	// Stale-row guard - see SmithSellEnter (self-audit, 2026-08-15). For repair the stale storehidx
	// would write _iDurability through InvList[i] with i from a dead screen.
	if (idx < 0 || idx >= storenumh)
		return;

	if (!PlayerCanAfford(storehold[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	StoreItem = storehold[idx];
	StartStore(TalkID::Confirm);
}

void WitchEnter()
{
	switch (stextsel) {
	case 12:
		stextlhold = 12;
		talker = TOWN_WITCH;
		stextshold = TalkID::Witch;
		StartStore(TalkID::Gossip);
		break;
	case 14:
		// The shop's door. WitchSell and WitchRecharge are still reachable - as tabs, and as the
		// screens their own back paths return to, which is why those cases are gone rather than
		// redirected.
		StartStore(TalkID::WitchBuy);
		break;
	case TownerSecondLine: {
		// Phase 2.3: the respec. The line is unselectable with nothing invested, so reaching here
		// means there is something to refund; the price gate still runs.
		const int cost = oracool::RespecCost(*MyPlayer);
		if (!PlayerCanAfford(cost)) {
			stextshold = TalkID::Witch;
			stextlhold = TownerSecondLine;
			StartStore(TalkID::NoMoney);
			break;
		}
		TakePlrsMoney(cost);
		const int refunded = oracool::TotalInvestedSkillPoints(*MyPlayer);
		oracool::RefundAllSkillPoints(*MyPlayer);
		// The refunded ranks fed CalcPlrItemVals through the provider chain (spell levels, passive
		// bonuses, a doused aura) - rebuild before the store screen returns, not on the next
		// incidental recalc.
		CalcPlrInv(*MyPlayer, true);
		oracool::LogEvent(fmt::format("Adria reclaimed {:d} skill point(s) for {:d} gold", refunded, cost),
		    UiFlags::ColorWhitegold);
		// Rebuilt rather than left as-is so the line greys out immediately.
		StartStore(TalkID::Witch);
		break;
	}
	case TownerLeaveLine:
		stextflag = TalkID::None;
		break;
	}
}

/**
 * @brief Removes a purchased non-replenishing item from the witch's stock.
 */
void RemoveWitchStockItem(int idx)
{
	// The first three are Adria's pinned potions and portal scroll - they restock rather than sell
	// out, so they are never removed.
	if (idx < 3)
		return;
	RemoveFromVendorStock(witchitem, WITCH_ITEMS, idx);
}

/**
 * @brief Purchases an item from the witch.
 */
void WitchBuyItemAt(Item &item, int idx)
{
	if (idx < 3)
		item._iSeed = AdvanceRndSeed();

	TakePlrsMoney(item._iIvalue);
	StoreAutoPlace(item, true);
	RemoveWitchStockItem(idx);
	CalcPlrInv(*MyPlayer, true);
}

void WitchBuyItem(Item &item)
{
	WitchBuyItemAt(item, stextvhold + ((stextlhold - stextup) / 4));
}

void WitchBuyEnter()
{
	if (stextsel == BackButtonLine()) {
		const bool fromSmith = stextflag == TalkID::SmithConsumables;
		StartStore(fromSmith ? TalkID::Smith : TalkID::Witch);
		stextsel = fromSmith ? SmithMenuLine(TalkID::SmithConsumables) : 14;
		return;
	}

	stextlhold = stextsel;
	stextvhold = stextsval;
	stextshold = stextflag;

	int idx = stextsval + ((stextsel - stextup) / 4);

	// Self-audit (2026-08-15): the bounds test must come BEFORE WitchStockItem, not ride on the
	// isEmpty guard below. For the plain witch that indirection reads a fixed 20-slot array, where a
	// stale row lands on an empty Item and the guard catches it - but for the SmithConsumables
	// screen it indexes a std::vector built fresh from Pepin's potions plus only the LIVE witch
	// items, which is smaller than the fixed array. A stale selected row indexed past the vector's
	// end one line before the guard that existed to catch exactly this class of row.
	const bool fromSmithConsumables = stextflag == TalkID::SmithConsumables;
	const int stockSize = fromSmithConsumables ? static_cast<int>(SmithConsumablesStock().size()) : WITCH_ITEMS;
	if (idx < 0 || idx >= stockSize)
		return;
	Item &selectedItem = WitchStockItem(idx, fromSmithConsumables);

	// Same guard as SmithBuyEnter - see the comment there. Defensive here rather than a reported
	// fault: this list is not known to empty out in practice, but the index is derived the same
	// way from a possibly-stale selected row, so the same phantom-item outcome is reachable.
	if (selectedItem.isEmpty())
		return;

	if (!PlayerCanAfford(selectedItem._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	if (!StoreAutoPlace(selectedItem, false)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	StoreItem = selectedItem;
	StartStore(TalkID::Confirm);
}

void WitchSellEnter()
{
	if (stextsel == BackButtonLine()) {
		StartStore(TalkID::Witch);
		stextsel = WitchShopDoorLine;
		return;
	}

	stextlhold = stextsel;
	stextshold = TalkID::WitchSell;
	stextvhold = stextsval;

	int idx = stextsval + ((stextsel - stextup) / 4);

	// Stale-row guard - see SmithSellEnter (self-audit, 2026-08-15).
	if (idx < 0 || idx >= storenumh)
		return;

	if (!StoreGoldFit(StoreHoldSalePrice(storehold[idx]), &storehold[idx])) {
		StartStore(TalkID::NoRoom);
		return;
	}

	StoreItem = storehold[idx];
	StartStore(TalkID::Confirm);
}

/**
 * @brief Recharges an item in the player's inventory or body in the witch.
 */
void WitchRechargeItemAt(int price, int idx)
{
	storehold[idx]._iCharges = storehold[idx]._iMaxCharges;

	Player &myPlayer = *MyPlayer;

	int8_t i = storehidx[idx];
	if (i < 0) {
		myPlayer.InvBody[INVLOC_HAND_LEFT]._iCharges = myPlayer.InvBody[INVLOC_HAND_LEFT]._iMaxCharges;
		NetSendCmdChItem(true, INVLOC_HAND_LEFT);
	} else {
		myPlayer.InvList[i]._iCharges = myPlayer.InvList[i]._iMaxCharges;
		NetSyncInvItem(myPlayer, i);
	}

	TakePlrsMoney(price);
	CalcPlrInv(myPlayer, true);
	oracool::ScheduleAutoSaveForStoreTransaction();
}

/** @brief The text-store's caller: it still knows the index only as a scroll position. */
void WitchRechargeItem(int price)
{
	WitchRechargeItemAt(price, stextvhold + ((stextlhold - stextup) / 4));
}

void WitchRechargeEnter()
{
	if (stextsel == BackButtonLine()) {
		const bool fromSmith = stextflag == TalkID::SmithRecharge;
		StartStore(fromSmith ? TalkID::Smith : TalkID::Witch);
		stextsel = fromSmith ? SmithMenuLine(TalkID::SmithRecharge) : WitchShopDoorLine;
		return;
	}

	stextshold = stextflag;
	stextlhold = stextsel;
	stextvhold = stextsval;

	int idx = stextsval + ((stextsel - stextup) / 4);

	// Stale-row guard - see SmithSellEnter (self-audit, 2026-08-15). For recharge the stale
	// storehidx would write _iCharges through InvList[i] with i from a dead screen.
	if (idx < 0 || idx >= storenumh)
		return;

	if (!PlayerCanAfford(storehold[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	StoreItem = storehold[idx];
	StartStore(TalkID::Confirm);
}

void BoyEnter()
{
	if (stextsel == TownerDoorLine) {
		StartStore(TalkID::BoyBuy);
		return;
	}
	if (stextsel != TownerTalkLine) {
		stextflag = TalkID::None;
		return;
	}
	talker = TOWN_PEGBOY;
	stextshold = TalkID::Boy;
	stextlhold = stextsel;
	StartStore(TalkID::Gossip);
}

/** @brief Wirt's Shop tab: the grid's click, through ShopSelectIndex, lands here with the slot in stextsval. */
void BoyShopBuyEnter()
{
	if (stextsel == BackButtonLine()) {
		StartStore(TalkID::Boy);
		stextsel = TownerDoorLine;
		return;
	}
	stextlhold = stextsel;
	stextvhold = stextsval;
	stextshold = TalkID::BoyBuy;
	const int idx = stextsval + ((stextsel - stextup) / 4);
	if (idx < 0 || idx >= BOY_ITEMS || boyitems[idx].isEmpty())
		return;
	if (!PlayerCanAfford(boyitems[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}
	if (!StoreAutoPlace(boyitems[idx], false)) {
		StartStore(TalkID::NoRoom);
		return;
	}
	StoreItem = boyitems[idx];
	StartStore(TalkID::Confirm);
}

} // namespace - RefreshBoyStock is exported (stores.h), its neighbours are file-local

void RefreshBoyStock(TalkID tab)
{
	// Free, at the player's word (user, 2026-09-20: "introduce Refresh buttons to Wirts two shops"): the Shop tab
	// is rolled again as SpawnBoy rolls it, the Gamble tab restocked with fresh bases, each cut to its page.
	const int lvl = MyPlayer->_pLevel;
	if (tab == TalkID::BoyBuy) {
		for (int i = 0; i < BOY_ITEMS; i++) {
			RollBoyShopSlot(boyitems[i], i, lvl);
			boyitems[i]._iStatFlag = MyPlayer->CanUseItem(boyitems[i]);
		}
		oracool::TrimShopStockToOnePage(TalkID::BoyBuy);
	} else if (tab == TalkID::BoyGamble) {
		SpawnGambleStock(lvl);
		for (Item &item : gambleitems) {
			if (!item.isEmpty())
				item._iStatFlag = MyPlayer->CanUseItem(item);
		}
		oracool::TrimShopStockToOnePage(TalkID::BoyGamble);
	} else {
		return;
	}
	oracool::ResetShopGridSelection();
}

namespace {

void BoyBuyItemAt(int idx)
{
	if (idx < 0 || idx >= BOY_ITEMS || boyitems[idx].isEmpty())
		return;
	Item &item = boyitems[idx];
	TakePlrsMoney(item._iIvalue);
	StoreAutoPlace(item, true);
	// The slot restocks in place, so the shop never empties and the grid's indices hold still.
	RollBoyShopSlot(item, idx, MyPlayer->_pLevel); // an Oracool slot restocks as one (2026-09-24)
	// And the new item must fit beside the rest, as the Magic tab's restock checks: a bigger one repacked the grid and
	// pushed items the player had seen into a hidden reserve the next visit trimmed (round 12 audit, v1.12.237).
	for (int attempt = 0; attempt < 8 && !oracool::ShopStockFitsOnePage(TalkID::BoyBuy); attempt++)
		RollBoyShopSlot(item, idx, MyPlayer->_pLevel);
	if (!oracool::ShopStockFitsOnePage(TalkID::BoyBuy))
		item.clear();
	item._iStatFlag = MyPlayer->CanUseItem(item);
	CalcPlrInv(*MyPlayer, true);
	oracool::ScheduleAutoSaveForStoreTransaction();
}

/** @brief The Gamble tab's click: the base is known, the price is the gamble's; the roll waits for the gold. */
void BoyGambleEnter()
{
	if (stextsel == BackButtonLine()) {
		StartStore(TalkID::Boy);
		stextsel = TownerDoorLine;
		return;
	}
	stextlhold = stextsel;
	stextvhold = stextsval;
	stextshold = TalkID::BoyGamble;
	const int idx = stextsval + ((stextsel - stextup) / 4);
	if (idx < 0 || idx >= GAMBLE_ITEMS || gambleitems[idx].isEmpty())
		return;
	if (!PlayerCanAfford(gambleitems[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}
	// Room for the BASE: the roll keeps the base, so its footprint is the result's.
	if (!StoreAutoPlace(gambleitems[idx], false)) {
		StartStore(TalkID::NoRoom);
		return;
	}
	StoreItem = gambleitems[idx];
	StartStore(TalkID::Confirm);
}

void GambleBuyItemAt(int idx)
{
	if (idx < 0 || idx >= GAMBLE_ITEMS || gambleitems[idx].isEmpty())
		return;
	Item &slot = gambleitems[idx];
	const int price = slot._iIvalue;
	const _item_indexes base = slot.IDidx;
	TakePlrsMoney(price);
	// The gamble: the roll happens NOW, after the gold, and the result goes to the pack identified.
	Item result;
	RollGambleResult(result, base, MyPlayer->_pLevel);
	result._iStatFlag = MyPlayer->CanUseItem(result);
	// The pack, else the stash, else really at his feet (audit, 2026-09-27): the room check was for the BASE, which an empty
	// worn slot can pass - and a roll the hero cannot wear is refused there. The log said it fell at his feet, and it was
	// simply gone, gold paid.
	if (!StoreAutoPlace(result, true) && !AutoPlaceItemInStash(*MyPlayer, result, true)) {
		DropItemBesidePlayer(*MyPlayer, result);
		oracool::LogEvent("Wirt's gamble had nowhere to go - it fell at your feet.", UiFlags::ColorRed);
	}
	oracool::LogEvent(StrCat("Wirt's gamble: ", std::string(result.getName())), result.getTextColor());
	// The slot restocks with a fresh unidentified base of the same slot at the same price.
	const int lvl = MyPlayer->_pLevel;
	slot = {};
	slot._iSeed = AdvanceRndSeed();
	SetRndSeed(slot._iSeed);
	GetItemAttrs(slot, base, lvl);
	slot._iIdentified = false;
	slot._iIvalue = price;
	slot._iCreateInfo = std::min(lvl, static_cast<int>(CF_LEVEL)) | CF_BOY;
	slot._iStatFlag = MyPlayer->CanUseItem(slot);
	CalcPlrInv(*MyPlayer, true);
	oracool::ScheduleAutoSaveForStoreTransaction();
}

/**
 * @brief Purchases an item from the healer.
 */
void HealerBuyItemAt(Item &item, int idx)
{
	if (!gbIsMultiplayer) {
		if (idx < 2)
			item._iSeed = AdvanceRndSeed();
	} else {
		if (idx < 3)
			item._iSeed = AdvanceRndSeed();
	}

	TakePlrsMoney(item._iIvalue);
	// Potions only, vanilla's reason (they stack with found ones): a bought charm or tiered base lost its effect line
	// and its Tier and Item Level for good - Cain refuses a plain item (round 12 audit, v1.12.237).
	if (item._iMagical == ITEM_QUALITY_NORMAL && item.isPotion())
		item._iIdentified = false;
	StoreAutoPlace(item, true);

	if (!gbIsMultiplayer) {
		if (idx < 2)
			return;
	} else {
		if (idx < 3)
			return;
	}
	RemoveFromVendorStock(healitem, static_cast<int>(std::size(healitem)), idx);
	CalcPlrInv(*MyPlayer, true);
}

void HealerBuyItem(Item &item)
{
	HealerBuyItemAt(item, stextvhold + ((stextlhold - stextup) / 4));
}

void UpdateSmithConsumablesStockAfterPurchase(const ConsumablesStockEntry &entry)
{
	if (!entry.isReplenishing())
		RemoveWitchStockItem(entry.vendorIndex);
}

void SmithConsumablesBuyItem(Item &item)
{
	const int combinedIndex = stextvhold + ((stextlhold - stextup) / 4);
	// Bounded for the same reason as WitchBuyEnter's guard (self-audit, 2026-08-15): this indexes
	// the same freshly built vector with an index captured at Enter. Enter now validates it, and
	// the stock cannot change between Enter and Confirm - but this is the line that would corrupt
	// memory if either of those facts ever stopped holding, so it carries its own bound.
	const std::vector<ConsumablesStockEntry> stock = SmithConsumablesStock();
	if (combinedIndex < 0 || static_cast<size_t>(combinedIndex) >= stock.size())
		return;
	const ConsumablesStockEntry entry = stock[combinedIndex];
	if (entry.isReplenishing())
		item._iSeed = AdvanceRndSeed();
	if (entry.vendor == ConsumablesVendor::Pepin)
		item._iCreateInfo = 0;
	TakePlrsMoney(item._iIvalue);
	if (entry.vendor == ConsumablesVendor::Pepin && item._iMagical == ITEM_QUALITY_NORMAL && item.isPotion()) // potions only (round 12)
		item._iIdentified = false;
	StoreAutoPlace(item, true);
	UpdateSmithConsumablesStockAfterPurchase(entry);
	CalcPlrInv(*MyPlayer, true);
}

void BoyBuyEnter()
{
	if (stextsel != 10) {
		stextflag = TalkID::None;
		return;
	}

	stextshold = TalkID::BoyBuy;
	stextvhold = stextsval;
	stextlhold = 10;
	int price = boyitem._iIvalue;
	if (gbIsHellfire)
		price -= boyitem._iIvalue / 4;
	else
		price += boyitem._iIvalue / 2;

	if (!PlayerCanAfford(price)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	if (!StoreAutoPlace(boyitem, false)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	StoreItem = boyitem;
	StoreItem._iIvalue = price;
	StartStore(TalkID::Confirm);
}

void StorytellerIdentifyItem(Item &item)
{
	Player &myPlayer = *MyPlayer;

	int listIdx = ((stextlhold - stextup) / 4) + stextvhold;
	int8_t idx = storehidx[listIdx];
	int8_t tabIdx = storehTabIdx[listIdx];
	if (tabIdx >= 0) {
		// Oracool Tabbed Inventory: this entry came from an extra tab, not InvBody/InvList.
		myPlayer.InvTabList[tabIdx][idx]._iIdentified = true;
	} else if (idx < 0) {
		if (-idx <= NumIdentifiableBodySlots)
			myPlayer.InvBody[IdentifiableBodySlots[-idx - 1]]._iIdentified = true;
	} else {
		myPlayer.InvList[idx]._iIdentified = true;
	}
	item._iIdentified = true;
	TakePlrsMoney(item._iIvalue);
	CalcPlrInv(myPlayer, true);
	oracool::ScheduleAutoSaveForStoreTransaction();
}

void ConfirmEnter(Item &item)
{
	if (stextsel == 18) {
		switch (stextshold) {
		case TalkID::SmithBuy:
			SmithBuyItem(item);
			break;
		case TalkID::SmithSell:
		case TalkID::WitchSell:
			StoreSellItem();
			break;
		case TalkID::SmithRepair:
			SmithRepairItem(item._iIvalue);
			break;
		case TalkID::WitchBuy:
			WitchBuyItem(item);
			break;
		case TalkID::SmithConsumables:
			SmithConsumablesBuyItem(item);
			break;
		case TalkID::WitchRecharge:
		case TalkID::SmithRecharge:
			WitchRechargeItem(item._iIvalue);
			break;
		case TalkID::BoyBuy:
			BoyBuyItemAt(stextvhold + ((stextlhold - stextup) / 4));
			break;
		case TalkID::BoyGamble:
			GambleBuyItemAt(stextvhold + ((stextlhold - stextup) / 4));
			break;
		case TalkID::HealerBuy:
			HealerBuyItem(item);
			break;
		case TalkID::StorytellerIdentify:
			StorytellerIdentifyItem(item);
			StartStore(TalkID::StorytellerIdentifyShow);
			return;
		case TalkID::SmithPremiumBuy:
			SmithBuyPItem(item);
			break;
		case TalkID::SmithUniqueBuy:
		case TalkID::SmithRareBuy:
		case TalkID::SmithSetBuy:
			BuyCuratedShelfItem(RequireCuratedShelf(stextshold), item);
			break;
		default:
			break;
		}

		// The coins changing hands (user, 2026-08-26: "i need you to play the gold drop sign when i
		// buy of sell item"). Every branch above moves gold in one direction or the other, so it is
		// played once HERE rather than in each of the eleven - a new vendor action gets the sound by
		// existing, which is the only way this stays true.
		//
		// The Storyteller's identify returns before this, and rightly: it is the one branch that
		// charges nothing.
		PlaySFX(IS_GOLD);
	}

	StartStore(stextshold);

	if (stextsel == BackButtonLine())
		return;

	stextsel = stextlhold;
	stextsval = std::min(stextvhold, stextsmax);

	while (stextsel != -1 && !stext[stextsel].isSelectable()) {
		stextsel--;
	}
}

void HealerEnter()
{
	switch (stextsel) {
	case 12:
		stextlhold = 12;
		talker = TOWN_HEALER;
		stextshold = TalkID::Healer;
		StartStore(TalkID::Gossip);
		break;
	case 14:
		StartStore(TalkID::HealerBuy);
		break;
	case 18:
		stextflag = TalkID::None;
		break;
	}
}

void HealerBuyEnter()
{
	if (stextsel == BackButtonLine()) {
		StartStore(TalkID::Healer);
		stextsel = 14;
		return;
	}

	stextlhold = stextsel;
	stextvhold = stextsval;
	stextshold = TalkID::HealerBuy;

	int idx = stextsval + ((stextsel - stextup) / 4);

	// Same guard as SmithBuyEnter - see the comment there.
	if (idx < 0 || static_cast<size_t>(idx) >= std::size(healitem) || healitem[idx].isEmpty())
		return;

	if (!PlayerCanAfford(healitem[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	if (!StoreAutoPlace(healitem[idx], false)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	StoreItem = healitem[idx];
	StartStore(TalkID::Confirm);
}

void StorytellerEnter()
{
	switch (stextsel) {
	case 12:
		stextlhold = 12;
		talker = TOWN_STORY;
		stextshold = TalkID::Storyteller;
		StartStore(TalkID::Gossip);
		break;
	case 14:
		StartStore(TalkID::StorytellerIdentify);
		break;
	case 18:
		stextflag = TalkID::None;
		break;
	}
}

void StorytellerIdentifyEnter()
{
	if (stextsel == BackButtonLine()) {
		StartStore(TalkID::Storyteller);
		stextsel = 14;
		return;
	}

	stextshold = TalkID::StorytellerIdentify;
	stextlhold = stextsel;
	stextvhold = stextsval;

	int idx = stextsval + ((stextsel - stextup) / 4);

	// Stale-row guard - see SmithSellEnter (self-audit, 2026-08-15).
	if (idx < 0 || idx >= storenumh)
		return;

	if (!PlayerCanAfford(storehold[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	StoreItem = storehold[idx];
	StartStore(TalkID::Confirm);
}

void TalkEnter()
{
	if (stextsel == BackButtonLine()) {
		StartStore(stextshold);
		stextsel = stextlhold;
		return;
	}

	int sn = 0;
	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_ACTIVE && QuestDialogTable[talker][quest._qidx] != TEXT_NONE && quest._qlog)
			sn++;
	}
	int la = 2;
	if (sn > 6) {
		sn = 14 - (sn / 2);
		la = 1;
	} else {
		sn = 15 - sn;
	}

	if (stextsel == sn - 2) {
		Towner *target = GetTowner(talker);
		assert(target != nullptr);
		InitQTextMsg(target->gossip);
		return;
	}

	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_ACTIVE && QuestDialogTable[talker][quest._qidx] != TEXT_NONE && quest._qlog) {
			if (sn == stextsel) {
				InitQTextMsg(QuestDialogTable[talker][quest._qidx]);
			}
			sn += la;
		}
	}
}

void TavernEnter()
{
	switch (stextsel) {
	case 12: {
		stextlhold = 12;
		talker = TOWN_TAVERN;
		stextshold = TalkID::Tavern;
		// Ogden's quest speech waits here rather than blocking the menu on the click (towners.cpp,
		// 2026-09-20): "Talk to Ogden" plays it once, then his ordinary gossip.
		const _speech_id quest = TakeOgdenQuestText();
		if (quest != TEXT_NONE) {
			stextflag = TalkID::None;
			InitQTextMsg(quest);
			break;
		}
		StartStore(TalkID::Gossip);
		break;
	}
	case TownerDoorLine:
		stextflag = TalkID::None;
		// Ogden's shop is his workshop now (user, 2026-09-21): the gem and rune tables, his recipes a tab away.
		oracool::OpenWorkshop(oracool::WorkshopHost::Jeweller);
		break;
	case 18:
		stextflag = TalkID::None;
		break;
	}
}

void BarmaidEnter()
{
	switch (stextsel) {
	case 12:
		stextlhold = 12;
		talker = TOWN_BMAID;
		stextshold = TalkID::Barmaid;
		StartStore(TalkID::Gossip);
		break;
	case TownerDoorLine:
		stextflag = TalkID::None;
		// Gillian's shop IS the Mystic Workshop (user, 2026-09-21); her recipe book is reached from the Cube's
		// book like every other host's.
		oracool::OpenWorkshop(oracool::WorkshopHost::Mystic);
		break;
	case 18:
		stextflag = TalkID::None;
		break;
	}
}

void DrunkEnter()
{
	switch (stextsel) {
	case 12:
		stextlhold = 12;
		talker = TOWN_DRUNK;
		stextshold = TalkID::Drunk;
		StartStore(TalkID::Gossip);
		break;
	case 18:
		stextflag = TalkID::None;
		break;
	}
}

int TakeGold(Player &player, int cost, bool skipMaxPiles)
{
	for (int i = 0; i < player._pNumInv; i++) {
		auto &item = player.InvList[i];
		if (item._itype != ItemType::Gold || (skipMaxPiles && item._ivalue == MaxGold))
			continue;

		if (cost < item._ivalue) {
			item._ivalue -= cost;
			SetPlrHandGoldCurs(player.InvList[i]);
			return 0;
		}

		cost -= item._ivalue;
		player.RemoveInvItem(i);
		i = -1;
	}

	return cost;
}

void DrawSelector(const Surface &out, const Rectangle &rect, string_view text, UiFlags flags)
{
	int lineWidth = GetLineWidth(text);

	int x1 = rect.position.x - 20;
	if (HasAnyOf(flags, UiFlags::AlignCenter))
		x1 += (rect.size.width - lineWidth) / 2;

	ClxDraw(out, { x1, rect.position.y + 13 }, (*pSPentSpn2Cels)[PentSpn2Spin()]);

	int x2 = rect.position.x + rect.size.width + 5;
	if (HasAnyOf(flags, UiFlags::AlignCenter))
		x2 = rect.position.x + (rect.size.width - lineWidth) / 2 + lineWidth + 5;

	ClxDraw(out, { x2, rect.position.y + 13 }, (*pSPentSpn2Cels)[PentSpn2Spin()]);
}

} // namespace

void RemoveFromVendorStock(Item *stock, int capacity, int idx)
{
	if (idx < 0 || idx >= capacity)
		return;
	std::move(stock + idx + 1, stock + capacity, stock + idx);
	stock[capacity - 1].clear();
}

bool HasCuratedShelf(CuratedShelf shelf)
{
	// Outside the anonymous namespace because the shop tab strip needs it. One rule, one place - the
	// strip decides whether to OFFER a tab and StartCuratedShelfBuy decides whether there is
	// anything behind it, and those two must not be able to disagree.
	if (gbIsMultiplayer)
		return false;
	switch (shelf) {
	case CuratedShelf::Unique:
		return *sgOptions.Oracool.griswoldSellUniqueItems;
	case CuratedShelf::Rare:
		return *sgOptions.Oracool.griswoldSellRareItems;
	case CuratedShelf::Set:
		return *sgOptions.Oracool.griswoldSellSetItems;
	case CuratedShelf::Count:
		break;
	}
	return false;
}

bool CuratedShelfHasStock(CuratedShelf shelf)
{
	if (!HasCuratedShelf(shelf))
		return false;
	const Item *items = ShelfItems(shelf);
	for (int i = 0; i < CuratedShelfCapacity; i++) {
		if (!items[i].isEmpty())
			return true;
	}
	return false;
}

std::optional<CuratedShelf> CuratedShelfFor(TalkID id)
{
	switch (id) {
	case TalkID::SmithUniqueBuy:
		return CuratedShelf::Unique;
	case TalkID::SmithRareBuy:
		return CuratedShelf::Rare;
	case TalkID::SmithSetBuy:
		return CuratedShelf::Set;
	default:
		return std::nullopt;
	}
}

TalkID TalkIdForCuratedShelf(CuratedShelf shelf)
{
	switch (shelf) {
	case CuratedShelf::Unique:
		return TalkID::SmithUniqueBuy;
	case CuratedShelf::Rare:
		return TalkID::SmithRareBuy;
	case CuratedShelf::Set:
		return TalkID::SmithSetBuy;
	case CuratedShelf::Count:
		break;
	}
	return TalkID::None;
}

bool HasSmithUniqueShop()
{
	return HasCuratedShelf(CuratedShelf::Unique);
}

/**
 * @brief Whose counter @p id is - the towner the player must be standing at to be using it.
 *
 * Derived from the SCREEN rather than read from `talker`, deliberately. `talker` is only written by
 * the gossip paths, so it is stale for most of these screens and would name whoever was spoken to
 * last - which is exactly the wrong thing to measure a distance against.
 */
_talker_id TownerForStoreDirect(TalkID id)
{
	switch (id) {
	case TalkID::Smith:
	case TalkID::SmithBuy:
	case TalkID::SmithSell:
	case TalkID::SmithRepair:
	case TalkID::SmithPremiumBuy:
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithSetBuy:
	case TalkID::SmithConsumables:
	case TalkID::SmithRecharge:
	case TalkID::SmithTransmute:
		return TOWN_SMITH;
	case TalkID::Witch:
	case TalkID::WitchBuy:
	case TalkID::WitchSell:
	case TalkID::WitchRecharge:
		return TOWN_WITCH;
	case TalkID::Healer:
	case TalkID::HealerBuy:
		return TOWN_HEALER;
	case TalkID::Boy:
	case TalkID::BoyBuy:
	case TalkID::BoyGamble:
		return TOWN_PEGBOY;
	case TalkID::Storyteller:
	case TalkID::StorytellerIdentify:
	case TalkID::StorytellerIdentifyShow:
		return TOWN_STORY;
	case TalkID::Tavern:
		return TOWN_TAVERN;
	case TalkID::Drunk:
		return TOWN_DRUNK;
	case TalkID::Barmaid:
		return TOWN_BMAID;
	default:
		return NUM_TOWNER_TYPES;
	}
}

/**
 * @brief Whose counter @p id is, following a sub-screen back to the shop that raised it.
 *
 * Confirm, No money, No room and Gossip belong to whatever put them up, which `stextshold` still
 * remembers - asking about the sub-screen alone would answer "no towner" and leave a confirmation
 * dialog floating after its shop had closed.
 *
 * ONE step back, not a walk: a sub-screen's parent is always a real screen, and following the chain
 * recursively would hang the game outright if the two ever pointed at each other.
 */
_talker_id TownerForStore(TalkID id)
{
	const _talker_id direct = TownerForStoreDirect(id);
	if (direct != NUM_TOWNER_TYPES)
		return direct;
	return TownerForStoreDirect(stextshold);
}

_talker_id TownerForOpenVendorPage()
{
	// The Cube is a town OBJECT, not a counter, so a window hosted by it belongs to nobody and is
	// left alone - walking away from a box you are standing at is not the same gesture as walking
	// away from a shopkeeper.
	if (oracool::IsLevskiRoarOpen()) {
		switch (oracool::CurrentTransmuteHost()) {
		case oracool::TransmuteHost::Smith:
			// NOT claimed here since 2026-09-21: Griswold's Salvage page is a store SCREEN now, so the
			// store machinery owns its walk-away and its close. Answering TOWN_SMITH would have two
			// mechanisms closing one page, and the store's is the one that also clears stextflag.
			return NUM_TOWNER_TYPES;
		case oracool::TransmuteHost::Tavern:
			return TOWN_TAVERN;
		case oracool::TransmuteHost::Barmaid:
			return TOWN_BMAID;
		case oracool::TransmuteHost::Cube:
			return NUM_TOWNER_TYPES;
		}
	}
	if (oracool::IsWorkshopOpen()) {
		return oracool::CurrentWorkshopHost() == oracool::WorkshopHost::Mystic ? TOWN_BMAID : TOWN_TAVERN;
	}
	return NUM_TOWNER_TYPES;
}

bool CloseVendorPageForTowner(_talker_id owner)
{
	if (owner == NUM_TOWNER_TYPES || TownerForOpenVendorPage() != owner)
		return false;
	if (oracool::IsWorkshopOpen()) {
		oracool::CloseWorkshop();
		return !oracool::IsWorkshopOpen();
	}
	oracool::CloseLevskiRoar();
	// CloseLevskiRoar REFUSES while the grid still holds items it cannot give back, and says so in
	// red. That refusal is deliberate and stands here too: a page that will not close because the
	// player's pack is full must not be closed out from under the items it is holding.
	return !oracool::IsLevskiRoarOpen();
}

void ForceCloseStore()
{
	if (stextflag == TalkID::None)
		return;
	DisarmShopServiceCursor();
	// The prompt's own teardown, not just its flag (external audit of v1.9.88, finding 6). Clearing
	// `IsRefreshUntilPromptOpen` directly makes CloseRefreshUntilPrompt a no-op ever after - it
	// returns early on exactly that flag - so SDL_StopTextInput() never runs and the IME stays open
	// with nothing on screen asking for text. The flag is the LAST thing that function clears, and
	// setting it by hand is a way of skipping the other two.
	CloseRefreshUntilPrompt();
	stextflag = TalkID::None;
	// NOT via StoreESC: that walks a nested screen back to its parent and re-opens it, which is
	// right for the Escape key and wrong for "this shop is over". The walk-away has always shut a
	// store this way; since 2026-09-22 so does opening any other shop surface, which is why the
	// three lines are a function rather than a comment telling the next caller what to copy.
}

void UpdateStoreState()
{
	// ---- 0. The Salvage tab's PAGE and its store flag are one thing ----
	//
	// The tab is a real store screen that draws a painted page (2026-09-21), which is what gives it
	// the walk-away, the talk-to-towner close, ESC and the overlap rule the other tabs always had.
	// Two pieces of state have to agree for that to hold: `stextflag == SmithTransmute` and the
	// window being open.
	//
	// RECONCILED once a tick rather than maintained at every exit, which is the argument this file
	// already makes for the service cursor below: `stextflag = TalkID::None` appears a dozen times
	// here, and a list of places to also close the page would have to stay complete forever.
	{
		const bool pageOpen = oracool::IsLevskiRoarOpen()
		    && oracool::CurrentTransmuteHost() == oracool::TransmuteHost::Smith;
		if (pageOpen && stextflag != TalkID::SmithTransmute) {
			oracool::CloseLevskiRoar();
			// A close can be REFUSED - a page holding items with no room to give them back says so in
			// red - and then the flag goes back rather than the two drifting apart.
			if (oracool::IsLevskiRoarOpen())
				stextflag = TalkID::SmithTransmute;
		} else if (!pageOpen && stextflag == TalkID::SmithTransmute) {
			stextflag = TalkID::None; // the page's own X closed it; the screen follows it out
		}
	}

	// ---- 0b. The artisans' pages are NOT tabs, and still need this ----
	//
	// Ogden's and Gillian's workshops and recipe books are reached from a dialog row, not from a tab
	// column, so there is no store screen for them to be. They keep the window handling: the same
	// three tiles from the same towner, so a counter is a counter wherever you meet one.
	if (const _talker_id pageOwner = TownerForOpenVendorPage(); pageOwner != NUM_TOWNER_TYPES
	    && leveltype == DTYPE_TOWN && MyPlayer != nullptr) {
		if (const Towner *towner = GetTowner(pageOwner);
		    towner != nullptr && MyPlayer->position.tile.WalkingDistance(towner->position) > 3) {
			CloseVendorPageForTowner(pageOwner);
		}
	}

	// ---- 1. A service cursor cannot outlive the shop that armed it ----
	//
	// The audit's remedy for finding 2 was "call a cancel function from every shop exit: X, ESC,
	// walkaway, overlap closure, initialization, vendor transitions, and exceptional exits". That is
	// the right behaviour and the wrong mechanism: `stextflag = TalkID::None` appears in this file a
	// dozen times, the list has to stay complete forever, and the failure when it does not is a
	// player permanently losing maximum durability on an item they meant to pay to repair.
	//
	// So the invariant is RECONCILED rather than maintained: once a tick, if a service cursor is
	// armed and no shop screen is open, it is cancelled. A new exit path cannot forget to be added
	// to this, because it is not a list of exits - it is the condition itself.
	if (IsAnyShopServiceCursorArmed() && !oracool::IsShopGridScreen(stextflag))
		DisarmShopServiceCursor();

	// ---- 2. A shop does not follow the player away from its counter ----
	if (stextflag == TalkID::None || leveltype != DTYPE_TOWN || MyPlayer == nullptr)
		return;
	// A screen with no towner behind it - nothing to measure a distance against, so it is left alone
	// rather than guessed at.
	const _talker_id owner = TownerForStore(stextflag);
	if (owner == NUM_TOWNER_TYPES)
		return;
	const Towner *towner = GetTowner(owner);
	if (towner == nullptr)
		return;

	// FIVE tiles, against the two TalkToTowner needs to open a shop. The gap is deliberate: the shop
	// is a panel rather than a modal screen in this fork (so items can be dragged out of the
	// inventory to sell), which means the player can walk while it is open, and a threshold equal to
	// the opening one would slam the shop shut on a single step taken by accident. Five is far
	// enough to be a decision.
	// THREE at every counter (user, 2026-09-21: "same walk away (3 tiles) logic"), where it used to be
	// five everywhere but Wirt's. The paragraph above argued for five, and the user has overruled it:
	// one threshold for every vendor is what makes them all read as one kind of place. It is close to
	// the two tiles TalkToTowner needs to OPEN a shop, so a couple of steps now closes one.
	constexpr int walkAwayTiles = 3;
	if (MyPlayer->position.tile.WalkingDistance(towner->position) <= walkAwayTiles)
		return;

	// Straight to closed, not back to the vendor's dialog: the player has left the counter, and a
	// dialog they did not ask for is no better than the shop they did not ask to keep.
	//
	// NOT via StoreESC. That walks a screen back to its parent and re-opens it, which is the wrong
	// shape here and would need the result overriding anyway. The one piece of state it would have
	// cleaned up is the service cursor, so that is cleaned up explicitly - a hammer left armed by a
	// shop the player has walked away from would repair the next thing they clicked and charge them.
	ForceCloseStore();
}

/**
 * @brief The player's whole spendable gold: carried plus the shared Stash pool.
 *
 * Outside the anonymous namespace, and declared in stores.h, so the character sheet and the
 * inventory's gold readout share it. It was internal here, which is how the character sheet ended
 * up re-deriving the same sum inline and the inventory's readout was written twice against
 * player-side fields that read 0 - all the gold is in the Stash.
 */
uint32_t TotalPlayerGold()
{
	// Self-audit (2026-08-15): 64-bit sum, saturated. Both operands are int, and the stash's own
	// deposit guard deliberately allows Stash.gold to grow to INT_MAX - so the plain int addition
	// that stood here overflows exactly when the player has been rich for long enough, and signed
	// overflow is UB besides. The wraparound would not have been cosmetic: a negative sum converts
	// to a huge uint32_t, PlayerCanAfford starts approving EVERYTHING, and TakePlrsMoney - whose
	// callers all trust that check - drives Stash.gold negative, which keeps the wraparound alive.
	// Saturating at UINT32_MAX keeps every comparison against a real price correct.
	const uint64_t total = static_cast<uint64_t>(std::max(MyPlayer->_pGold, 0))
	    + static_cast<uint64_t>(std::max(Stash.gold, 0));
	return static_cast<uint32_t>(std::min<uint64_t>(total, std::numeric_limits<uint32_t>::max()));
}

// Oracool: defined outside the anonymous namespace (same rationale as
// SimulateStorytellerIdentifyForTest below) so diablo.cpp/scrollrt.cpp can call these - internal
// linkage symbols like StartRefreshUntilPrompt/RefreshPremiumUntilTarget/PremiumRefreshUntilLine
// stay callable from here regardless, since anonymous-namespace visibility spans the whole
// translation unit.
void CloseRefreshUntilPrompt()
{
	if (!IsRefreshUntilPromptOpen)
		return;
	SDL_StopTextInput();
	IsRefreshUntilPromptOpen = false;
	RefreshUntilPromptInputState = std::nullopt;
}

void RefreshUntilPromptKeyPress(SDL_Keycode vkey)
{
	switch (vkey) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER: {
		const std::string result = RefreshPremiumUntilTarget();
		CloseRefreshUntilPrompt();
		StartStore(TalkID::SmithPremiumBuy);
		stextsel = PremiumRefreshUntilLine();
		InitDiabloMsg(result);
		break;
	}
	case SDLK_ESCAPE:
		CloseRefreshUntilPrompt();
		// Escape cancels: the edit was made in place, so what was typed stayed and reached the ini at the next save.
		CopyUtf8(sgOptions.Oracool.refreshUntilItemNames, RefreshUntilNamesBeforeEdit, sizeof(sgOptions.Oracool.refreshUntilItemNames));
		break;
	default:
		break;
	}
}

bool HandleRefreshUntilPromptTextInputEvent(const SDL_Event &event)
{
	return HandleTextInputEvent(event, *RefreshUntilPromptInputState);
}

/** @brief The Refresh Until prompt's plate on screen: drawn by its bottom-left at (190, 178) of the UI rect. */
Rectangle RefreshUntilPromptRect()
{
	const ClxSprite plate = (*pGBoxBuff)[0];
	const Point uiPosition = GetUIRectangle().position;
	return { { uiPosition.x + 190, uiPosition.y + 178 - plate.height() + 1 }, { plate.width(), plate.height() } };
}

bool CheckRefreshUntilPromptPress(Point mousePosition)
{
	// A red X, as the withdraw box has: the prompt swallowed every click, so a mouse user could not leave it (round 13
	// audit, v1.12.238). It cancels the way Escape does; Enter still confirms.
	if (!IsRefreshUntilPromptOpen || !pGBoxBuff)
		return false;
	if (oracool::CheckWindowCloseButtonClick(RefreshUntilPromptRect(), mousePosition)) {
		RefreshUntilPromptKeyPress(SDLK_ESCAPE);
		return true;
	}
	return false;
}

void DrawRefreshUntilPrompt(const Surface &out)
{
	if (!IsRefreshUntilPromptOpen)
		return;

	const string_view targetText = sgOptions.Oracool.refreshUntilItemNames;
	const TextInputCursorState &cursor = RefreshUntilPromptCursor;

	const Point uiPosition = GetUIRectangle().position;
	const int dialogX = uiPosition.x + 190;

	ClxDraw(out, { dialogX, uiPosition.y + 178 }, (*pGBoxBuff)[0]);

	const std::string wrapped = WordWrapString(_("What item are you looking for? Refreshes until a matching item appears in Griswold's premium stock, or the configured timeout is reached."), 200);

	DrawString(out, wrapped, { { dialogX + 31, uiPosition.y + 75 }, { 200, 50 } },
	    { UiFlags::ColorWhitegold | UiFlags::AlignCenter, 1, 17 });

	oracool::DrawWindowCloseButton(out, RefreshUntilPromptRect());

	DrawString(out, targetText, { dialogX + 37, uiPosition.y + 128 },
	    TextRenderOptions {
	        /*flags=*/UiFlags::ColorWhite | UiFlags::PentaCursor,
	        /*spacing=*/1,
	        /*lineHeight=*/-1,
	        /*cursorPosition=*/static_cast<int>(cursor.position),
	        /*highlightRange=*/ { static_cast<int>(cursor.selection.begin), static_cast<int>(cursor.selection.end) },
	    });
}

/**
 * @brief Oracool: user request - hovering Refresh Until (without clicking) shows a 4-line
 * explainer in the main HUD's bottom info box (the same panel DrawInfoBox uses for item/monster
 * hover text - still empty during a store screen, since pcursitem/pcursmonst are never set then).
 * Reuses the exact row/column geometry CheckStoreBtn's click-redirect already established for
 * this button, so "where hovering shows the tooltip" and "where clicking activates the button"
 * never drift apart.
 */
std::string ShopRefreshUntilLookingFor()
{
	if (GetPremiumRefreshTargets().empty())
		return {};
	return std::string(sgOptions.Oracool.refreshUntilItemNames);
}

void DrawRefreshUntilHoverTooltip(const Surface &out)
{
	if (stextflag != TalkID::SmithPremiumBuy)
		return;
	// The Magic tab is a shop grid now, whose Refresh Until button says all of this on its own card (dev note,
	// 2026-09-27: every vendor button's hover is the card). The old text rows are not drawn there, so this box
	// would be explaining a row that is not on screen.
	if (oracool::IsShopGridScreen(stextflag))
		return;
	if (!*sgOptions.Oracool.refreshUntilButton || gbIsMultiplayer)
		return;
	if (!stext[PremiumRefreshUntilLine()].hasText())
		return;

	const Point uiPosition = GetUIRectangle().position;
	if (MousePosition.y < PaddingTop + uiPosition.y || MousePosition.y > 320 + uiPosition.y)
		return;
	if (MousePosition.x < 24 + uiPosition.x || MousePosition.x > 616 + uiPosition.x)
		return;

	const int relativeY = MousePosition.y - (uiPosition.y + PaddingTop);
	if (relativeY / LineHeight() != BackButtonLine())
		return;

	constexpr int RedirectZoneWidth = 100;
	const int leftBorder = uiPosition.x + 24;
	if (MousePosition.x >= leftBorder + RedirectZoneWidth)
		return;

	const Rectangle infoArea { GetMainPanel().position + InfoBoxTopLeft, InfoBoxSize };
	constexpr int TooltipLineHeight = 15;
	int lineY = infoArea.position.y + 2;
	const auto drawLine = [&](string_view text, UiFlags color) {
		DrawString(out, text, Rectangle { { infoArea.position.x, lineY }, { infoArea.size.width, TooltipLineHeight } }, { color | UiFlags::AlignCenter });
		lineY += TooltipLineHeight;
	};

	drawLine(_("Item to look for:"), UiFlags::ColorGold);
	const string_view configured = sgOptions.Oracool.refreshUntilItemNames;
	if (GetPremiumRefreshTargets().empty())
		drawLine(_("Please click and input"), UiFlags::ColorRed);
	else
		drawLine(configured, UiFlags::ColorWhite);
	drawLine(_("Click to begin search. Good luck!"), UiFlags::ColorGold);
	drawLine(_("Availability based on player level!"), UiFlags::ColorRed);
}

// Oracool: mirrors SimulateSmithConsumablesPurchaseForTest's approach - sets up the same globals
// StorytellerIdentifyItem reads to resolve its target from storehold[index], then calls it exactly
// as the real "identify which item?" confirm click would, without needing StartStore()'s
// screen/sprite setup (unavailable in a headless test). Defined here, outside the anonymous
// namespace StorytellerIdentifyItem itself lives in, so this test-only entry point actually gets
// the external linkage its stores.h declaration promises - internal-linkage symbols stay callable
// from here regardless, since anonymous-namespace visibility spans the whole translation unit.
void SimulateStorytellerIdentifyForTest(size_t index)
{
	stextup = 0;
	stextvhold = 0;
	stextlhold = static_cast<int>(index) * 4;
	StorytellerIdentifyItem(storehold[index]);
}

// Oracool: defined here, OUTSIDE the anonymous namespace (same pattern as CloseRefreshUntilPrompt
// above), so the stores.h declaration gets a real external-linkage definition - the internal
// helpers it calls stay reachable regardless, since anonymous-namespace visibility spans the TU.
int ResolveBackRowClickLine(int mouseX, int uiLeft)
{
	// Back's row is shared real estate. "Refresh Until" sits flush against the left golden
	// border, "Refresh"/"Repair all"/"Sell all" flush against the right one, and Back's own text
	// in the centre. A click on this row goes to whichever button is within a generous fixed
	// width of its border - comfortably wider than any of the short strings ever renders - and
	// falls through to Back otherwise, including dead centre, where Back itself renders.
	//
	// Every redirect is gated on ITS OWN screen's stextflag. The Premium pair used to check only
	// "does that line index have text", on the recorded assumption that only the Premium screen
	// ever populates those lines. Oracool bug fix: user report - "again Sell All is not working" -
	// proved that assumption wrong: on a Sell page with exactly four items, the last item's second
	// attribute line lands on the exact line PremiumRefreshLine() names, so the Premium branch
	// hijacked every right-zone click before the Sell branch was consulted, and CheckStoreBtn's
	// walk-back turned it into the last item's single-item confirmation - the user clicked
	// "Sell all" and got "sell Sapphire Buckler?". Intermittent by list shape, which is why it
	// kept coming back: with one to three items on the page, that line is empty and everything
	// works. The left-zone twin was worse: on the Sell screen PremiumRefreshUntilLine() coincides
	// with SmithSellAllLine() itself, so a click near Back's LEFT edge would have sold everything
	// with no confirmation.
	//
	// Separate function (and exported) so the routing itself is testable - the first regression
	// test written for this pinned the StoreEnter dispatch instead and passed with the bug intact.
	constexpr int RedirectZoneWidth = 100;
	const int leftBorder = uiLeft + 24;
	const int rightBorder = uiLeft + 616;

	if (stextflag == TalkID::SmithPremiumBuy && mouseX < leftBorder + RedirectZoneWidth && stext[PremiumRefreshUntilLine()].hasText())
		return PremiumRefreshUntilLine();
	if (stextflag == TalkID::SmithPremiumBuy && mouseX >= rightBorder - RedirectZoneWidth && stext[PremiumRefreshLine()].hasText())
		return PremiumRefreshLine();
	if (stextflag == TalkID::SmithRepair && mouseX >= rightBorder - RedirectZoneWidth && stext[SmithRepairAllLine()].hasText())
		return SmithRepairAllLine();
	if (stextflag == TalkID::SmithSell && mouseX >= rightBorder - RedirectZoneWidth && stext[SmithSellAllLine()].hasText())
		return SmithSellAllLine();
	return BackButtonLine();
}

// Oracool: test surface for the Sell All click-routing regression (see stores_test.cpp's
// SmithSell_FourItemPage_SellAllRowNotHijackedByPremiumRedirect). stext/stextsel/the line-index
// helpers are all internal to this translation unit, and exporting the whole STextStruct array
// for one test would be a far bigger interface than the test deserves.
int GetSellAllLineForTest()
{
	return SmithSellAllLine();
}

int GetPremiumRefreshLineForTest()
{
	return PremiumRefreshLine();
}

bool StoreLineHasTextForTest(int line)
{
	return stext[line].hasText();
}

void SetStoreSelectionForTest(int line)
{
	stextsel = line;
}

void RescrollStoreForTest()
{
	// Mirrors DrawSText's per-frame ScrollSmithSell(stextsval) dispatch. StartStore only sets
	// stextflag AFTER the Start* function has populated the screen, so anything ScrollSmithSell
	// gates on stextflag - the Sell all/Repair all buttons - is absent until the first frame's
	// redraw re-runs it. The game always gets that frame; a headless test has to ask for it.
	ScrollSmithSell(stextsval);
}

size_t GetSmithConsumablesStockCountForTest()
{
	return SmithConsumablesStock().size();
}

item_misc_id GetSmithConsumablesStockMiscIdForTest(size_t index)
{
	return SmithConsumablesStock()[index].item->_iMiscId;
}

bool IsSmithConsumablesStockFromPepinForTest(size_t index)
{
	return SmithConsumablesStock()[index].vendor == ConsumablesVendor::Pepin;
}

void UpdateSmithConsumablesStockAfterPurchaseForTest(size_t index)
{
	const ConsumablesStockEntry entry = SmithConsumablesStock()[index];
	UpdateSmithConsumablesStockAfterPurchase(entry);
}

bool StoreGoldFitForTest(int price, const Item *itemFreeingCells)
{
	return StoreGoldFit(price, itemFreeingCells);
}

void SimulateSmithPremiumBuyForTest(int selectedIndex, Item &item)
{
	// The held-selection encoding SmithBuyPItem re-derives its index from: with lhold == up the
	// row term is zero and vhold carries the whole visible index, which is how ConfirmEnter's
	// restore presents it too.
	stextvhold = selectedIndex;
	stextlhold = 0;
	stextup = 0;
	SmithBuyPItem(item);
}

bool SimulateSmithConsumablesPurchaseForTest(size_t combinedIndex)
{
	// Mirrors WitchBuyEnter() + ConfirmEnter()'s SmithConsumables case exactly (probe,
	// then real placement via the same StoreAutoPlace/SmithConsumablesBuyItem calls a
	// real purchase uses), without going through StartStore()'s screen/sprite setup,
	// which needs rendering resources unavailable in a headless test.
	const ConsumablesStockEntry entry = SmithConsumablesStock()[combinedIndex];
	Item &selectedItem = *entry.item;

	if (!PlayerCanAfford(selectedItem._iIvalue))
		return false;
	if (!StoreAutoPlace(selectedItem, false))
		return false;

	// SmithConsumablesBuyItem() re-derives the stock index from these rather than
	// taking it as a parameter, so they must be set to match combinedIndex.
	stextvhold = 0;
	stextlhold = stextup + static_cast<int>(combinedIndex) * 4;
	StoreItem = selectedItem;

	SmithConsumablesBuyItem(StoreItem);
	return true;
}

void AddStoreHoldRepair(Item *itm, int8_t i)
{
	// Same reasoning as AddStoreHoldRecharge: the bound is this function's business, not its
	// callers'.
	if (storenumh >= StoreHoldCapacity)
		return;
	const int v = RepairPriceFor(*itm);
	// Zero means "nothing to charge for", and this list is a list of things to pay for. The old
	// shape wrote the item into storehold BEFORE this test and then returned without counting it,
	// leaving a stale entry one past the end for anything that later read past storenumh.
	if (v == 0)
		return;
	storehold[storenumh] = *itm;
	storehold[storenumh]._iIvalue = v;
	storehold[storenumh]._ivalue = v;
	storehidx[storenumh] = i;
	storehTabIdx[storenumh] = -1; // repair never sources from an extra tab; keep the array in sync regardless
	storenumh++;
}

void InitStores()
{
	ClearSText(0, STORE_LINES);
	stextflag = TalkID::None;
	stextsize = false;
	stextscrl = false;
	numpremium = 0;
	premiumlevel = 1;

	BuybackStock.clear();
	// Ogden's queued quest speech is per game too: it lives in a file static (round 3 audit, v1.12.228).
	ClearOgdenQuestText();
	// A hammer (or a recharge cursor) left armed by a shop the player has since left would act on
	// the next thing they clicked and charge them for it.
	DisarmShopServiceCursor();

	for (auto &premiumitem : premiumitems)
		premiumitem.clear();
	for (CuratedShelfState &shelf : CuratedShelves) {
		for (Item &item : shelf.items)
			item.clear();
		shelf.initialized = false;
	}
	InitializeSmithPepinPotions();

	boyitem.clear();
	boylevel = 0;
	// Wirt's two grids as well (audit, 2026-09-27): SpawnBoy restocks only when its first slot is empty or the tier
	// rises, and a bought slot restocks in place - so a new level-1 hero met the last hero's level-30 stock until level 2.
	for (Item &item : boyitems)
		item.clear();
	for (Item &item : gambleitems)
		item.clear();
}

/** @brief Fills @p shelf's array from empty. The one place a shelf's identity actually differs. */
void GenerateCuratedShelf(CuratedShelf shelf, const Player &player, int vendorLevel)
{
	Item *items = ShelfItems(shelf);
	for (int i = 0; i < CuratedShelfCapacity; i++)
		items[i].clear();

	int generatedCount = 0;
	switch (shelf) {
	case CuratedShelf::Unique: {
		std::vector<_unique_items> candidates;
		for (int i = 0; UniqueItems[i].UIItemId != UITYPE_INVALID; ++i) {
			if (IsUniqueAvailable(i) && UniqueItems[i].UIMinLvl <= player._pLevel)
				candidates.push_back(static_cast<_unique_items>(i));
		}
		// No count option any more - the shelf is as long as a page (user, 2026-08-27). The INI
		// keeps only the on/off switch.
		const int priceMultiplier = std::max(*sgOptions.Oracool.griswoldUniqueItemPriceMultiplier, 1);
		while (generatedCount < CuratedShelfCapacity && !candidates.empty()) {
			const size_t candidateIndex = static_cast<size_t>(GenerateRnd(candidates.size()));
			const _unique_items uid = candidates[candidateIndex];
			// Drawn WITHOUT replacement, which is what keeps the shelf free of duplicates.
			candidates.erase(candidates.begin() + candidateIndex);
			Item item;
			if (!CreateUniqueVendorItem(player, item, uid))
				continue;
			const int64_t price = static_cast<int64_t>(item._iIvalue) * priceMultiplier;
			item._iIvalue = static_cast<int>(std::min<int64_t>(price, std::numeric_limits<int>::max()));
			items[generatedCount++] = std::move(item);
		}
		break;
	}
	case CuratedShelf::Rare: {
		// Bounded by ATTEMPTS, not by successes. A rare roll can miss - a base that cannot carry
		// tiered affixes is a legitimate miss, not an error - and a `while (generated < 40)` loop
		// over a pool that happens to be all misses would not terminate. The unique and set shelves
		// cannot hang the same way because both draw from a shrinking candidate list; this one
		// draws with replacement, so it needs its own bound.
		constexpr int MaxAttempts = CuratedShelfCapacity * 8;
		for (int attempt = 0; attempt < MaxAttempts && generatedCount < CuratedShelfCapacity; attempt++) {
			Item item;
			if (!CreateRareVendorItem(player, item, vendorLevel))
				continue;
			items[generatedCount++] = std::move(item);
		}
		break;
	}
	case CuratedShelf::Set: {
		// "Already on this shelf" is answered by DEFINITION IDENTITY - the address of the row in
		// ItemSetItems, which is a static table.
		//
		// The first version compared the item's rendered name against the translated `def.name`, and
		// that was wrong in three ways at once: it depended on the display string, which is
		// translated and could collide or be reworded; on MakeSetItem continuing to write exactly
		// that string into _iIName; and it re-derived an identity that the caller already had in its
		// hand. Comparing what a thing IS beats comparing what it is called.
		std::vector<const oracool::SetItemDefinition *> taken;
		taken.reserve(CuratedShelfCapacity);
		while (generatedCount < CuratedShelfCapacity) {
			Item item;
			const oracool::SetItemDefinition *chosen = nullptr;
			const bool made = CreateSetVendorItem(player, item, vendorLevel,
			    [&taken](const oracool::SetItemDefinition &def) {
				    return std::find(taken.begin(), taken.end(), &def) != taken.end();
			    },
			    &chosen);
			// False means the candidate list is exhausted - every piece this level has earned is
			// already on the shelf - so the shelf is as long as it is going to get.
			if (!made)
				break;
			taken.push_back(chosen);
			items[generatedCount++] = std::move(item);
		}
		break;
	}
	case CuratedShelf::Count:
		break;
	}
}

void SpawnCuratedShelves(const Player &player, int vendorLevel)
{
	for (int i = 0; i < static_cast<int>(CuratedShelf::Count); i++) {
		const auto shelf = static_cast<CuratedShelf>(i);
		CuratedShelfState &state = CuratedShelves[i];
		// Built ONCE per game, not once per town visit - that is the difference between a curated
		// shelf and a restocking one, and it is why buying from it does not refill it.
		if (state.initialized || !HasCuratedShelf(shelf))
			continue;
		GenerateCuratedShelf(shelf, player, vendorLevel);
		// "Initialized" means SOMETHING WAS BUILT, not "we tried once". Every shelf is gated on the
		// character's level - the set pieces do not start until level 18 - so a shelf generated by a
		// character below its floor comes out empty, and marking that attempt as done would mean
		// the shelf never appeared for the rest of the game however far the character got. Left
		// unmarked, the next town visit tries again.
		state.initialized = CuratedShelfHasStock(shelf);
	}
}


/**
 * @brief Trims Adria's array against the tighter of the two pages it appears on.
 *
 * `witchitem` is shown by two tabs: Adria's own Buy tab, where it has the page to itself, and
 * Griswold's Supplies tab, where Pepin's four potions come first. Trimming it against its own tab
 * left it fitting there and overflowing on Supplies, and the overflow stayed alive in the array -
 * a hidden reserve that surfaced as soon as a visible Supplies item was bought (external audit of
 * v1.9.97, finding 2).
 *
 * Trimming against Supplies instead settles both, because Supplies is strictly the smaller page:
 * anything that fits beside the four potions fits without them. It costs Adria's own tab the few
 * items that could not have been shown on Supplies anyway, which is the price of the two views
 * agreeing about what is in stock.
 */
void TrimWitchStockToOnePage()
{
	oracool::TrimShopStockToOnePage(TalkID::SmithConsumables);
}

void TrimWitchStockToOnePageForTest()
{
	TrimWitchStockToOnePage();
}

void SetupTownStores()
{
	Player &myPlayer = *MyPlayer;

	if (gbIsMultiplayer)
		SetRndSeed(glSeedTbl[currlevel] * SDL_GetTicks());

	const int l = VendorStockLevel();
	SpawnSmith(l);
	SpawnWitch(l);
	SpawnHealer(l);
	SpawnBoy(myPlayer._pLevel);
	SpawnPremium(myPlayer);
	SpawnCuratedShelves(myPlayer, l);
	// Wirt's two grids are over-supplied (BOY_ITEMS, GAMBLE_ITEMS) and cut to the page here, so both come out
	// full with no hidden reserve (user, 2026-09-20: "Fill entire grid with stock in Wirt's shop/gamble grids").
	oracool::TrimShopStockToOnePage(TalkID::BoyBuy);
	oracool::TrimShopStockToOnePage(TalkID::BoyGamble);

	// The shelf is decided HERE, once, rather than recomputed from an oversized array after every
	// purchase (external audit of v1.9.92, finding 5). Each generator deliberately over-supplies so
	// the page comes out full; without this the surplus stayed in the array as a hidden reserve that
	// surfaced whenever a visible item was bought and freed its cells.
	//
	oracool::TrimShopStockToOnePage(TalkID::SmithBuy);
	TrimWitchStockToOnePage();
	oracool::TrimShopStockToOnePage(TalkID::HealerBuy);
	oracool::TrimShopStockToOnePage(TalkID::SmithPremiumBuy);
	RecountPremiumStock();
	for (int i = 0; i < static_cast<int>(CuratedShelf::Count); i++)
		oracool::TrimShopStockToOnePage(TalkIdForCuratedShelf(static_cast<CuratedShelf>(i)));
}

void FreeStoreMem()
{
	if (*sgOptions.Gameplay.showItemGraphicsInStores) {
		FreeHalfSizeItemSprites();
	}
	stextflag = TalkID::None;
	for (STextStruct &entry : stext) {
		entry.text.clear();
		entry.text.shrink_to_fit();
	}
}

void PrintSString(const Surface &out, int margin, int line, string_view text, UiFlags flags, int price, int cursId, bool cursIndent)
{
	const Point uiPosition = GetUIRectangle().position;
	int sx = uiPosition.x + 32 + margin;
	if (!stextsize) {
		sx += 320;
	}

	const int sy = uiPosition.y + PaddingTop + stext[line].y + stext[line]._syoff;

	int width = stextsize ? 575 : 255;
	if (stextscrl && line >= 4 && line <= 20) {
		width -= 9; // Space for the selector
	}
	width -= margin * 2;

	const Rectangle rect { { sx, sy }, { width, 0 } };

	// Space reserved for item graphic is based on the size of 2x3 cursor sprites
	constexpr int CursWidth = INV_SLOT_SIZE_PX * 2;
	constexpr int HalfCursWidth = CursWidth / 2;

	if (*sgOptions.Gameplay.showItemGraphicsInStores && cursId >= 0) {
		const Size size = GetInvItemSize(static_cast<int>(CURSOR_FIRSTITEM) + cursId);
		const bool useHalfSize = size.width > INV_SLOT_SIZE_PX || size.height > INV_SLOT_SIZE_PX;
		const bool useRed = HasColor(flags, UiFlags::ColorRed); // a colour is a field, not a bit (2026-09-07)
		const ClxSprite sprite = useHalfSize
		    ? (useRed ? GetHalfSizeItemSpriteRed(cursId) : GetHalfSizeItemSprite(cursId))
		    : GetInvItemSprite(static_cast<int>(CURSOR_FIRSTITEM) + cursId);
		const Point position {
			rect.position.x + (HalfCursWidth - sprite.width()) / 2,
			rect.position.y + (TextHeight() * 3 + sprite.height()) / 2
		};
		if (useHalfSize || !useRed) {
			ClxDraw(out, position, sprite);
		} else {
			ClxDrawTRN(out, position, sprite, GetInfravisionTRN());
		}
	}

	if (*sgOptions.Gameplay.showItemGraphicsInStores && cursIndent) {
		const Rectangle textRect { { rect.position.x + HalfCursWidth + 8, rect.position.y }, { rect.size.width - HalfCursWidth + 8, rect.size.height } };
		DrawString(out, text, textRect, { flags });
	} else {
		DrawString(out, text, rect, { flags });
	}

	if (price > 0)
		DrawString(out, FormatInteger(price), rect, { flags | UiFlags::AlignRight });

	if (stextsel == line) {
		DrawSelector(out, rect, text, flags);
	}
}

void DrawSLine(const Surface &out, int sy)
{
	const Point uiPosition = GetUIRectangle().position;
	int sx = 26;
	int width = 587;

	if (!stextsize) {
		sx += SidePanelSize.width;
		width -= SidePanelSize.width;
	}

	uint8_t *src = out.at(uiPosition.x + sx, uiPosition.y + 25);
	uint8_t *dst = out.at(uiPosition.x + sx, sy);

	for (int i = 0; i < 3; i++, src += out.pitch(), dst += out.pitch())
		memcpy(dst, src, static_cast<size_t>(width) * out.bytesPerPixel()); // bytes, not pixels (v1.11)
}

void DrawSTextHelp()
{
	stextsel = -1;
	stextsize = true;
}

void ClearSText(int s, int e)
{
	for (int i = s; i < e; i++) {
		stext[i]._sx = 0;
		stext[i]._syoff = 0;
		stext[i].text.clear();
		stext[i].text.shrink_to_fit();
		stext[i].flags = UiFlags::None;
		stext[i].type = STextStruct::Label;
		stext[i]._sval = 0;
	}
}

void StartStore(TalkID s)
{
	// "You do not have enough gold" is a BANNER on the grid shop, not a screen (user, 2026-08-31:
	// "try a pop up message which doesnt require confirmation from my side"). Vanilla replaces the
	// whole store with one sentence the player then has to dismiss - which, on a screen where the
	// goods, the gold and the tabs are all visible at once, answers "can I afford this" by hiding
	// everything they were comparing.
	//
	// Intercepted here rather than at the twenty-odd StartStore(TalkID::NoMoney) call sites: they
	// all mean the same thing, and every one of them already returns straight after.
	//
	// Gated on the CURRENT screen being a grid shop, not on stextshold. That is what makes the early
	// return safe: the screen we decline to leave is a real shop screen, so nothing is left sitting
	// on a confirmation dialog with its own exit skipped. The classic stores keep vanilla's screen.
	// The Forge tab is not a screen: it leaves the store and opens the transmute window on Griswold's
	// book (Levski's Cube, D8). The tab column reaches here, not SmithEnter (audit, 2026-09-20: the
	// tab drew an empty stock grid).
	if (s == TalkID::SmithTransmute) {
		stextflag = TalkID::SmithTransmute; // a store screen that draws a page - see the case above
		oracool::OpenLevskiWindowFor(oracool::TransmuteHost::Smith);
		return;
	}
	if (s == TalkID::NoMoney && oracool::IsShopGridScreen(stextflag)) {
		oracool::ShowShopToast(std::string(_("You do not have enough gold")));
		return;
	}

	// Only on the way IN to a shop. StartStore is also how a shop screen rebuilds itself after every
	// purchase, and resetting there would throw the cursor back to the first item each time.
	if (oracool::IsShopGridScreen(s) && !oracool::IsShopGridScreen(stextflag))
		oracool::ResetShopGridSelection();

	if (*sgOptions.Gameplay.showItemGraphicsInStores) {
		CreateHalfSizeItemSprites();
	}
	sbookflag = false;
	CloseInventory();
	CloseCharPanel();
	// The waypoint list and the crafting window share the top-left slot with the character sheet,
	// the quest log and the stash - all of which this already closes - and since v1.9.26 the shop
	// panel is in that slot too. They were missed because before the shop moved there, a store was
	// a box in the middle of the screen and nothing it opened could collide.
	oracool::CloseWaypointMenu();
	oracool::CloseCraftingMenu();
	// ...and then straight back open for a shop, because that is where the goods you are selling
	// live (user request, 2026-08-23). CloseInventory runs first rather than being skipped: it also
	// shuts the stash and the gold-withdraw prompt, which have no business being open over a shop,
	// and the shop panel occupies the same left-hand slot the stash does.
	//
	// The three refusal/confirmation screens count too: they are not shop tabs, but they are screens
	// a shop tab put you on and will put you back from, and without them the inventory shut and
	// reopened around every single purchase. Named explicitly rather than testing stextshold alone,
	// because stextshold is stale as often as not - it is only written by the Enter handlers, so
	// after backing out of a shop it still names the tab you left, and a bare test would have
	// reopened the inventory over the towner's dialog.
	const bool returningToShop = IsAnyOf(s, TalkID::Confirm, TalkID::NoMoney, TalkID::NoRoom)
	    && oracool::IsShopGridScreen(stextshold);
	if (oracool::IsShopGridScreen(s) || returningToShop)
		invflag = true;
	RenderGold = false;
	QuestLogIsOpen = false;
	CloseGoldDrop();
	ClearSText(0, STORE_LINES);
	ReleaseStoreBtn();
	switch (s) {
	case TalkID::Smith:
		StartSmith();
		break;
	case TalkID::SmithBuy:
		// Griswold's basic-items stock can run out entirely (e.g. after buying everything he
		// has). StartSmithBuy already renders correctly with zero items - "I have these items
		// for sale:" and just a Back button - so there's no need to bounce the player back out
		// to the store menu the way vanilla did here.
		StartSmithBuy();
		break;
	case TalkID::SmithSell:
		StartSmithSell();
		break;
	case TalkID::SmithRepair:
		StartSmithRepair();
		break;
	case TalkID::SmithConsumables:
		StartWitchBuy(true);
		break;
	case TalkID::SmithRecharge:
		StartWitchRecharge();
		break;
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithSetBuy:
		if (!StartCuratedShelfBuy(RequireCuratedShelf(s)))
			return;
		break;
	case TalkID::Witch:
		StartWitch();
		break;
	case TalkID::WitchBuy:
		if (storenumh > 0)
			StartWitchBuy(false);
		break;
	case TalkID::WitchSell:
		StartWitchSell();
		break;
	case TalkID::WitchRecharge:
		StartWitchRecharge();
		break;
	case TalkID::NoMoney:
		StoreNoMoney();
		break;
	case TalkID::NoRoom:
		StoreNoRoom();
		break;
	case TalkID::Confirm:
		StoreConfirm(StoreItem);
		break;
	case TalkID::Boy:
		StartBoy();
		break;
	case TalkID::BoyBuy:
	case TalkID::BoyGamble:
		StartBoyShop(); // the grid shop with the Shop and Gamble tabs (2026-09-20)
		break;
	case TalkID::Healer:
		StartHealer();
		break;
	case TalkID::Storyteller:
		StartStoryteller();
		break;
	case TalkID::HealerBuy:
		if (storenumh > 0)
			StartHealerBuy();
		break;
	case TalkID::StorytellerIdentify:
		StartStorytellerIdentify();
		break;
	case TalkID::SmithPremiumBuy:
		if (!StartSmithPremiumBuy())
			return;
		break;
	case TalkID::Gossip:
		StartTalk();
		break;
	case TalkID::StorytellerIdentifyShow:
		StartStorytellerIdentifyShow(StoreItem);
		break;
	case TalkID::Tavern:
		StartTavern();
		break;
	case TalkID::Drunk:
		StartDrunk();
		break;
	case TalkID::Barmaid:
		StartBarmaid();
		break;
	case TalkID::None:
		break;
	}

	stextsel = -1;
	for (int i = 0; i < STORE_LINES; i++) {
		if (stext[i].isSelectable()) {
			stextsel = i;
			break;
		}
	}

	stextflag = s;
}

namespace {

/** @brief The price the text list would print beside @p item on a sell-side screen. */
int SellSidePrice(const Item &item)
{
	return (item._iMagical != ITEM_QUALITY_NORMAL && item._iIdentified) ? item._iIvalue : item._ivalue;
}

} // namespace

std::vector<oracool::ShopSlot> GetShopStock(TalkID id)
{
	std::vector<oracool::ShopSlot> stock;
	switch (id) {
	case TalkID::SmithBuy:
		for (int i = 0; i < SMITH_ITEMS; i++) {
			if (!smithitem[i].isEmpty())
				stock.push_back({ &smithitem[i], i, smithitem[i]._iIvalue });
		}
		break;
	case TalkID::SmithPremiumBuy: {
		// Visible position, not array slot - SmithBuyPItemAt counts non-empty entries to find its
		// item, because a purchase leaves a hole behind rather than compacting the array.
		int visible = 0;
		for (int i = 0; i < SMITH_PREMIUM_ITEMS; i++) {
			if (!premiumitems[i].isEmpty()) {
				stock.push_back({ &premiumitems[i], visible, premiumitems[i]._iIvalue });
				visible++;
			}
		}
		break;
	}
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithSetBuy: {
		Item *items = ShelfItems(RequireCuratedShelf(id));
		for (int i = 0; i < CuratedShelfCapacity; i++) {
			if (!items[i].isEmpty())
				stock.push_back({ &items[i], i, items[i]._iIvalue });
		}
		break;
	}
	case TalkID::SmithConsumables: {
		const std::vector<ConsumablesStockEntry> entries = SmithConsumablesStock();
		for (size_t i = 0; i < entries.size(); i++) {
			// Pepin's potions are marked protected: they are shown and placed like anything else,
			// but the one-page trim must not clear them - see ShopSlot::neverTrim.
			stock.push_back({ entries[i].item, static_cast<int>(i), entries[i].item->_iIvalue,
			    /*neverTrim=*/entries[i].vendor == ConsumablesVendor::Pepin });
		}
		break;
	}
	case TalkID::WitchBuy:
		for (int i = 0; i < WITCH_ITEMS; i++) {
			if (!witchitem[i].isEmpty())
				stock.push_back({ &witchitem[i], i, witchitem[i]._iIvalue });
		}
		break;
	case TalkID::HealerBuy:
		// std::size, not a literal 20. Pepin's array is the one vendor array this pass did not
		// resize, and a hardcoded length beside three that just changed is a trap.
		for (int i = 0; i < static_cast<int>(std::size(healitem)); i++) {
			if (!healitem[i].isEmpty())
				stock.push_back({ &healitem[i], i, healitem[i]._iIvalue });
		}
		break;
	case TalkID::BoyBuy:
		// Wirt's Shop tab (2026-09-20): a purchase restocks its slot in place, so the index is the slot.
		for (int i = 0; i < BOY_ITEMS; i++) {
			if (!boyitems[i].isEmpty())
				stock.push_back({ &boyitems[i], i, boyitems[i]._iIvalue });
		}
		break;
	case TalkID::BoyGamble:
		// The Gamble tab: one unidentified base per slot; the price is the gamble's, stamped on the item.
		for (int i = 0; i < GAMBLE_ITEMS; i++) {
			if (!gambleitems[i].isEmpty())
				stock.push_back({ &gambleitems[i], i, gambleitems[i]._iIvalue }); // trimmed to the page like the Shop tab since 2026-09-20 (the slot restocks in place, so the page stays full)
		}
		break;
	case TalkID::SmithSell:
	case TalkID::WitchSell:
		// The Sold tab. Not the player's sellable inventory any more - selling is a drag onto the
		// panel now, and what this shows is what THIS vendor has already bought. The index is the
		// position in the filtered list, which is what ShopBuyBack takes.
		{
			const std::vector<size_t> indices = BuybackIndicesFor(id);
			for (size_t i = 0; i < indices.size(); i++) {
				SoldItem &sold = BuybackStock[indices[i]];
				// The stored price, not the item's own value: the item is pristine now, so its value is
				// what it is WORTH, and what this row must show is what it costs to take back.
				stock.push_back({ &sold.item, static_cast<int>(i), sold.price });
			}
		}
		break;
	case TalkID::SmithRepair:
	case TalkID::SmithRecharge:
	case TalkID::WitchRecharge:
	case TalkID::StorytellerIdentify:
		// These three screens overwrite _iIvalue with the SERVICE cost when they build storehold -
		// see StartSmithRepair. The item's own worth is not what the player is being charged.
		for (int i = 0; i < storenumh; i++) {
			if (!storehold[i].isEmpty())
				stock.push_back({ &storehold[i], i, storehold[i]._iIvalue });
		}
		break;
	default:
		break;
	}
	return stock;
}

namespace {

/** @brief Whether entry @p index of tab @p id restocks rather than sells out - each vendor's own rule, as its buy handler applies it. */
bool IsReplenishingShopEntry(TalkID id, int index)
{
	if (index < 0)
		return false;
	switch (id) {
	case TalkID::HealerBuy:
		return index < (gbIsMultiplayer ? 3 : 2); // HealerBuyItemAt keeps these
	case TalkID::WitchBuy:
		return index < 3; // RemoveWitchStockItem keeps these
	case TalkID::SmithConsumables: {
		const std::vector<ConsumablesStockEntry> entries = SmithConsumablesStock();
		return static_cast<size_t>(index) < entries.size() && entries[static_cast<size_t>(index)].isReplenishing();
	}
	default:
		return false;
	}
}

bool IsStackBuyPotion(const Item &item)
{
	return IsAnyOf(item._iMiscId, IMISC_HEAL, IMISC_FULLHEAL, IMISC_MANA, IMISC_FULLMANA, IMISC_REJUV, IMISC_FULLREJUV);
}

} // namespace

int ShopBuyPotionStack(TalkID id, int index)
{
	// Ctrl+right click (user, 2026-09-13: "make ctrl+right click on a consumable potion in the vendors
	// to purchase a stack of up to 99 of these, limited by amount of available money, or free slots in
	// the belt/inv grid"). Only a potion the vendor RESTOCKS: a one-off potion is one item, and buying
	// "a stack" of it would conjure copies the shop never had.
	if (!oracool::IsSinglePlayer() || !IsReplenishingShopEntry(id, index))
		return -1;
	const std::vector<oracool::ShopSlot> stock = GetShopStock(id);
	const oracool::ShopSlot *slot = nullptr;
	for (const oracool::ShopSlot &candidate : stock) {
		if (candidate.index == index) {
			slot = &candidate;
			break;
		}
	}
	if (slot == nullptr || slot->item == nullptr || slot->item->isEmpty() || !IsStackBuyPotion(*slot->item) || slot->price <= 0)
		return -1;

	const int affordable = static_cast<int>(std::min<uint32_t>(TotalPlayerGold() / static_cast<uint32_t>(slot->price), Item::MaxStackCount));
	if (affordable <= 0)
		return -1; // the ordinary purchase path says why: its NoMoney screen

	// Prepared the way each vendor's single purchase prepares one - a fresh seed for a restocking
	// entry, and Pepin's potions sold unidentified and without their create info.
	Item stack = *slot->item;
	stack._iSeed = AdvanceRndSeed();
	const bool pepin = id == TalkID::HealerBuy
	    || (id == TalkID::SmithConsumables && SmithConsumablesStock()[static_cast<size_t>(index)].vendor == ConsumablesVendor::Pepin);
	if (id == TalkID::SmithConsumables && pepin)
		stack._iCreateInfo = 0;
	if (pepin && stack._iMagical == ITEM_QUALITY_NORMAL && stack.isPotion()) // potions only (round 12)
		stack._iIdentified = false;

	// The largest stack that fits. Placement is all-or-nothing and a stack goes whole to the belt or
	// whole to the backpack (INV-01), so the probe's answer for N is exactly what the commit will do.
	int count = affordable;
	for (; count > 0; count--) {
		Item probe = stack;
		probe.setStackCount(count);
		if (StoreAutoPlace(probe, /*persistItem=*/false))
			break;
	}
	if (count <= 0)
		return -1; // the ordinary purchase path says why: its NoRoom screen

	stack.setStackCount(count);
	TakePlrsMoney(slot->price * count);
	StoreAutoPlace(stack, /*persistItem=*/true);
	CalcPlrInv(*MyPlayer, true);
	PlaySFX(IS_GOLD); // once for the stack - the coins changing hands, as ConfirmEnter plays for one
	return count;
}

void ShopSelectIndex(TalkID id, int index)
{
	if (index < 0)
		return;

	// The tab's Enter handler reads `stextsval + ((stextsel - stextup) / 4)`. Putting the index in
	// stextsval and the selection on the first row makes that expression evaluate to `index`, so
	// the handler runs on the item the grid was clicked on and nothing else about it changes. Both
	// values are scroll state the grid screens do not render, and ConfirmEnter re-clamps stextsval
	// against stextsmax on the way back out.
	stextsel = stextup;
	stextsval = index;

	switch (id) {
	case TalkID::SmithBuy:
		SmithBuyEnter();
		break;
	case TalkID::SmithPremiumBuy:
		SmithPremiumBuyEnter();
		break;
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithSetBuy:
		CuratedShelfBuyEnter(RequireCuratedShelf(id));
		break;
	case TalkID::SmithConsumables:
	case TalkID::WitchBuy:
		WitchBuyEnter();
		break;
	case TalkID::HealerBuy:
		HealerBuyEnter();
		break;
	case TalkID::BoyBuy:
		BoyShopBuyEnter();
		break;
	case TalkID::BoyGamble:
		BoyGambleEnter();
		break;
	case TalkID::SmithSell:
	case TalkID::WitchSell:
		ShopBuyBack(index);
		break;
	default:
		break;
	}

	// NO CONFIRMATION STEP (user, 2026-08-26: "you need to remove the purchase confirmation windows.
	// we don't need it with the new interface").
	//
	// The handlers above end by opening TalkID::Confirm - a full-screen "Are you sure?" inherited
	// from the text-list stores, where the click that reached an item was a cursor landing on a row
	// and could plausibly be a mistake. The shop grid is not that interface: the item is under the
	// pointer, priced, with its stats beside it, and the purchase now takes a deliberate RIGHT-click.
	// Asking again afterwards adds a keystroke and answers a question the gesture already answered.
	//
	// Answered here rather than by deleting the Confirm screen, because that screen is the one place
	// every vendor action converges - the buy, the sell, the repair, the recharge, the buy-back. The
	// handlers still run their own afford and room checks first, and those open NoMoney or NoRoom
	// INSTEAD of Confirm, so this only ever auto-answers a transaction that was already going to be
	// allowed.
	if (stextflag == TalkID::Confirm) {
		stextsel = 18; // the "Yes" line ConfirmEnter tests for
		ConfirmEnter(StoreItem);
	}
}

bool ShopSellHeldItem()
{
	Player &myPlayer = *MyPlayer;
	if (myPlayer.HoldItem.isEmpty())
		return false;

	// Pepin buys nothing. He never had a Sell screen, so there is no sellOk function that speaks for
	// him - and without this test the fall-through below would have had him buying whatever Griswold
	// buys, at Griswold's prices, the moment an item was dropped on his panel.
	const bool witch = IsWitchShopScreen(stextflag);
	if (!witch && !IsAnyOf(stextflag, TalkID::SmithBuy, TalkID::SmithPremiumBuy, TalkID::SmithUniqueBuy,
	        TalkID::SmithRareBuy, TalkID::SmithSetBuy,
	        TalkID::SmithConsumables, TalkID::SmithSell))
		return false;

	// The vendor's own list of what they will take, not a new one. Adria does not buy armour and
	// Griswold does not buy potions, and that judgement already exists in two functions the sell
	// screens have always used.
	const bool accepted = witch ? WitchSellOk(myPlayer.HoldItem) : SmithSellOk(myPlayer.HoldItem);
	if (!accepted)
		return false;

	// Recorded UNCHANGED, with the price alongside - see SoldItem::item. Overwriting the item with
	// its own sale price is what made a sold-and-rebought item lose three quarters of its value.
	const Item sold = myPlayer.HoldItem;
	const int price = GetItemSellValue(sold);

	// The room check the list path has always made, which this used to skip (external audit of
	// v1.9.92, finding 4). The old comment said the Stash "has no grid to fill", which is true and
	// incomplete: Stash.gold is capped at INT_MAX, and once the pool has no headroom the remainder
	// has to fit in finite backpack stacks. Without this the item was cleared unconditionally and
	// whatever would not fit was logged as lost - and buyback is no remedy, since recovering it
	// means paying again in the same saturated state.
	//
	// freesItemCells is FALSE: the item is on the cursor, not in the backpack, so selling it vacates
	// nothing that could hold gold.
	if (!StoreGoldFit(price, /*itemFreeingCells=*/nullptr)) {
		stextshold = stextflag;
		stextlhold = stextup;
		StartStore(TalkID::NoRoom);
		return false;
	}

	RecordSale(sold, price);
	CreditSaleProceeds(price);

	myPlayer.HoldItem.clear();
	NewCursor(CURSOR_HAND);
	PlaySFX(IS_GOLD);
	oracool::ScheduleAutoSaveForStoreTransaction();
	return true;
}

bool ShopSellInventoryItem(int cii)
{
	// Selling by RIGHT-CLICKING the item where it lies (user, 2026-08-26: "selling should also be
	// done by right clicking an item in my inv grid. only in the inv grid, not in the item slots on
	// my hero").
	//
	// The backpack only. A worn item's cii is below INVITEM_INV_FIRST and is refused here rather
	// than handled: selling the armour off your back with one click, in a window whose whole purpose
	// is the item under the cursor, is a mis-click that costs a character its gear.
	if (cii < INVITEM_INV_FIRST || cii > INVITEM_INV_LAST)
		return false;

	Player &myPlayer = *MyPlayer;
	if (!myPlayer.HoldItem.isEmpty())
		return false;

	// A LIST index, not a grid cell. Reported from play, 2026-08-27: "i right click on items to buy
	// them back and then to resell then over and over and something weird happend. random itrem get
	// sold back."
	//
	// It did. The first version of this read `InvGrid[cii - INVITEM_INV_FIRST]`, treating the value
	// as a cell coordinate and looking up whatever item occupied that cell - but `pcursinvitem` is
	// built from GetActiveInvListItem (see CheckInvHLight), so the offset is already an index into
	// the backpack list. The two numbering schemes agree only by coincidence, and the coincidence
	// breaks the moment the list is compacted by a sale - which is why it took repeated selling to
	// show itself.
	//
	// Tab-aware for the same reason UseInvItem is: the index belongs to whichever backpack page is
	// displayed, and reading InvList directly silently used the wrong item on tabs 2-10.
	const int index = cii - INVITEM_INV_FIRST;
	if (index < 0 || index >= GetActiveNumInv(myPlayer))
		return false;

	// The vendor's own judgement of what they will take, exactly as the held-item path asks it -
	// Adria does not buy armour and Griswold does not buy potions, and neither should start doing so
	// because the item arrived by a different gesture.
	const bool witch = IsWitchShopScreen(stextflag);
	if (!witch && !IsAnyOf(stextflag, TalkID::SmithBuy, TalkID::SmithPremiumBuy, TalkID::SmithUniqueBuy,
	        TalkID::SmithRareBuy, TalkID::SmithSetBuy,
	        TalkID::SmithConsumables, TalkID::SmithSell))
		return false;

	const Item &sold = GetActiveInvListItem(myPlayer, index);
	if (sold.isEmpty())
		return false;
	if (!(witch ? WitchSellOk(sold) : SmithSellOk(sold)))
		return false;

	// The item is recorded UNCHANGED and the price travels beside it. Reported from play,
	// 2026-08-27: "something weird is happening with the sell/back back/resell, rebuyback price of
	// items. it is like it is constantly changing and reducing."
	//
	// It was. Selling used to overwrite the item's own `_ivalue`/`_iIvalue` with the quarter-price
	// it fetched, and that mutated copy was what went into the buyback list - so buying it back
	// handed the player an item worth a quarter of what they had sold. Sell it again and it fetched
	// a quarter of THAT. Every round trip through the vendor divided the item by four, permanently.
	const int price = GetItemSellValue(sold);
	const Item pristine = sold;

	// Checked BEFORE the item is removed - the same gate the list path uses, which this gesture also
	// skipped (external audit of v1.9.92, finding 4). freesItemCells is true here: this one IS in the
	// backpack, so its cells become available to hold the gold it fetches.
	if (!StoreGoldFit(price, &pristine)) {
		stextshold = stextflag;
		stextlhold = stextup;
		StartStore(TalkID::NoRoom);
		return false;
	}

	RemoveActiveInvItem(myPlayer, index);
	RecordSale(pristine, price);
	CreditSaleProceeds(price);
	PlaySFX(IS_GOLD);
	CalcPlrInv(myPlayer, true);
	oracool::ScheduleAutoSaveForStoreTransaction();
	return true;
}

/**
 * @brief Whether the hammer cursor is armed by the shop rather than by the Repair skill.
 *
 * The two share `CURSOR_REPAIR` and everything that hangs off it - the hammer graphic, the
 * click-an-item targeting, the inventory/tab/stash routing in TryIconCurs - which is exactly what was
 * asked for (user, 2026-08-27: "borrow the entire mechanic behind the vanilla Repair Item skill but
 * produce 100% durability recovery"). What differs is what happens on the click: the skill repairs
 * partially and free, the shop repairs fully and charges. This flag is the only thing that tells
 * them apart.
 */
ShopServiceCursor ShopArmedServiceCursor = ShopServiceCursor::None;

bool ShopRepairHeldItem()
{
	Player &myPlayer = *MyPlayer;
	const int price = RepairPriceFor(myPlayer.HoldItem);
	if (price == 0)
		return false;
	if (!PlayerCanAfford(price)) {
		stextshold = stextflag;
		stextlhold = stextup;
		StartStore(TalkID::NoMoney);
		return false;
	}
	TakePlrsMoney(price);
	myPlayer.HoldItem._iDurability = myPlayer.HoldItem._iMaxDur;
	// A broken item left equipped is flagged as well as emptied - see SmithRepairItemAt, which
	// clears the same flag for the same reason.
	myPlayer.HoldItem._iOracoolBroken = false;
	PlaySFX(IS_GOLD); // the hammer's sound (ShopRepairItemAt), for the same paid repair
	oracool::ScheduleAutoSaveForStoreTransaction();
	return true;
}

void ArmShopRepairCursor()
{
	ShopArmedServiceCursor = ShopServiceCursor::Repair;
	NewCursor(CURSOR_REPAIR);
}

void ArmShopSellCursor()
{
	// The HAMMER, not a cursor of its own (user, 2026-09-21: "when clicked use the Repair Item hammer
	// cursor"). Sharing CURSOR_REPAIR means sharing its targeting and TryIconCurs' inventory/tab/stash
	// routing; ShopArmedServiceCursor is what tells the click apart from a paid repair, and it is
	// asked before either of them.
	ShopArmedServiceCursor = ShopServiceCursor::Sell;
	NewCursor(CURSOR_REPAIR);
}

bool ShopSellItemAt(Player &player, int tab, int index)
{
	if (index < 0)
		return false;
	Item *item = nullptr;
	if (tab < 0) {
		if (index >= player._pNumInv)
			return false;
		item = &player.InvList[index];
	} else {
		if (tab >= Player::NumExtraInventoryTabs || index >= player._pNumInvTab[tab])
			return false;
		item = &player.InvTabList[tab][index];
	}
	if (item->isEmpty())
		return false;

	// The vendor's OWN list of what they take, the same two functions every sell screen has always
	// used - Adria does not buy armour and Griswold does not buy potions, and that judgement is not
	// re-decided here.
	const bool witch = IsWitchShopScreen(stextflag);
	// Griswold's own screens only, as the held-item and right-click sales ask: the grid test let Pepin's and Wirt's
	// screens through at Griswold's prices (round 3 audit, v1.12.229).
	if (!witch && !IsAnyOf(stextflag, TalkID::SmithBuy, TalkID::SmithPremiumBuy, TalkID::SmithUniqueBuy,
	        TalkID::SmithRareBuy, TalkID::SmithSetBuy,
	        TalkID::SmithConsumables, TalkID::SmithSell))
		return false;
	if (!(witch ? WitchSellOk(*item) : SmithSellOk(*item)))
		return false;

	// Recorded UNCHANGED, with the price alongside - see SoldItem::item. Overwriting the item with
	// its own sale price is what once made a sold-and-rebought item lose three quarters of its value.
	const Item sold = *item;
	const int price = GetItemSellValue(sold);
	// The item's own cells DO free up here, unlike the held-item sale - it is in the pack, so selling
	// it makes room the gold may need.
	if (!StoreGoldFit(price, item)) {
		stextshold = stextflag;
		stextlhold = stextup;
		StartStore(TalkID::NoRoom);
		return false;
	}

	RecordSale(sold, price);
	if (tab < 0)
		player.RemoveInvItem(index, false);
	else
		RemoveExtraTabItem(player, tab, index);
	CreditSaleProceeds(price);
	PlaySFX(IS_GOLD); // the gold drop, as the user asked
	CalcPlrInv(player, true);
	oracool::ScheduleAutoSaveForStoreTransaction();
	return true;
}

void ArmShopRechargeCursor()
{
	// Adria's twin of the above, and deliberately the same shape (user, 2026-08-27: "make recharge
	// button work as repair button. use cursor from vanilla recharge skill"). CURSOR_RECHARGE is the
	// Recharge skill's own cursor, so the graphic and TryIconCurs' inventory/tab/stash routing come
	// with it; only the click differs - full charges, and paid for.
	ShopArmedServiceCursor = ShopServiceCursor::Recharge;
	NewCursor(CURSOR_RECHARGE);
}

/**
 * @brief Whether a shop service cursor is armed AND there is still a shop to charge for it.
 *
 * The screen test is not belt-and-braces (external audit of v1.9.88, finding 2). The flag alone was
 * the whole authority to take gold, so any exit that cleared the screen but not the flag left a
 * cursor that could still run a PAID transaction with no shop open. Requiring both means the
 * authority expires with the thing that granted it.
 */
bool ShopServiceCursorLive(ShopServiceCursor kind)
{
	return ShopArmedServiceCursor == kind && oracool::IsShopGridScreen(stextflag);
}

bool ConsumeStaleShopServiceCursor()
{
	// Called at the POINT OF USE, before either vanilla fallback in TryIconCurs.
	//
	// v1.9.92 fixed this the wrong way round (external audit of v1.9.92, finding 1). It made the paid
	// predicate require an open shop - correct, and it does stop gold being taken outside one - and
	// then reconciled the leftover cursor once a game-logic tick. I argued that reconciling beat a
	// list of exit sites because a new exit cannot forget to join a condition. That is true of the
	// BACKSTOP and was wrong as the whole answer: SDL drains an entire queued event batch before game
	// logic runs, so a close and a click land in the same batch and the tick has not happened yet.
	//
	// In that window the paid predicate is already false while `pcurs` is still CURSOR_REPAIR, so the
	// click fell through to the VANILLA Repair skill - which permanently reduces maximum durability.
	// The exact destruction the whole fix existed to prevent, moved from "always" to "if you click
	// fast enough". Two queued events are sufficient; no race is needed.
	//
	// So the question is asked where it cannot be raced: if any raw shop-service state exists, this
	// cursor belongs to a shop, full stop. It is never reinterpreted as the class skill - it is
	// cleared and the click is spent. A player who armed a paid repair and clicked after the shop
	// closed gets nothing, which is the correct nothing.
	if (!IsAnyShopServiceCursorArmed())
		return false;
	DisarmShopServiceCursor();
	return true;
}

bool IsAnyShopServiceCursorArmed()
{
	// The FLAG alone, with no screen test - the opposite question to the two above. They ask "may
	// this charge gold", which must be false once the shop is gone; this asks "is there state left
	// to clean up", which must stay true precisely then.
	return ShopArmedServiceCursor != ShopServiceCursor::None;
}

bool IsShopRepairCursorArmed()
{
	return ShopServiceCursorLive(ShopServiceCursor::Repair);
}

bool IsShopRechargeCursorArmed()
{
	return ShopServiceCursorLive(ShopServiceCursor::Recharge);
}

bool IsShopSellCursorArmed()
{
	return ShopServiceCursorLive(ShopServiceCursor::Sell);
}

void DisarmShopServiceCursor()
{
	// Clears the FLAG and the CURSOR, and the second half is the fix (external audit of v1.9.88,
	// finding 2). It used to clear the flag alone, and that is the more dangerous half to leave
	// behind: `pcurs` stays CURSOR_REPAIR, TryIconCurs sees the paid flag gone, and falls through to
	// the VANILLA Repair skill - which reduces _iMaxDur permanently. So a player who armed a paid
	// repair and then walked away from the counter would, on their next click, silently take
	// permanent maximum-durability damage off the item they meant to pay to fix. Recharge is the
	// same shape against _iMaxCharges.
	//
	// One clear for both kinds, because they are one piece of state. Two independent flags would let
	// a stale Recharge survive a Repair click and charge for the next thing the player touched.
	ShopArmedServiceCursor = ShopServiceCursor::None;
	if (pcurs == CURSOR_REPAIR || pcurs == CURSOR_RECHARGE)
		NewCursor(CURSOR_HAND);
}

bool ShopRepairItemAt(Item &item)
{
	// Full durability, and charged for (user, 2026-08-27: "produce 100% durability recovery").
	//
	// RepairPriceFor returns 0 for anything with nothing to repair, which doubles as the "not a
	// valid target" test: clicking an undamaged item costs nothing and does nothing, rather than
	// taking gold for no work.
	const int price = RepairPriceFor(item);
	if (price == 0)
		return false;
	if (!PlayerCanAfford(price)) {
		stextshold = stextflag;
		stextlhold = stextup;
		StartStore(TalkID::NoMoney);
		return false;
	}
	// The work first, the fee after, as SmithRepairItemAt does: a gold pile the fee used up leaves InvList by moving the
	// last entry into its slot, and when the item was that last entry the repair landed on the emptied slot - gold taken,
	// item still worn (round 23 audit, v1.12.248).
	item._iDurability = item._iMaxDur;
	// A broken item left equipped is flagged as well as emptied - see SmithRepairItemAt, which
	// clears the same flag for the same reason.
	item._iOracoolBroken = false;
	TakePlrsMoney(price);
	PlaySFX(IS_GOLD);
	oracool::ScheduleAutoSaveForStoreTransaction();
	return true;
}

bool ShopRechargeItemAt(Item &item)
{
	// RechargePriceFor returns 0 for anything with nothing to recharge - not a staff, no charge
	// slots, already full - so it doubles as the "not a valid target" test, exactly as
	// RepairPriceFor does for the hammer.
	const int price = RechargePriceFor(item);
	if (price == 0)
		return false;
	if (!PlayerCanAfford(price)) {
		stextshold = stextflag;
		stextlhold = stextup;
		StartStore(TalkID::NoMoney);
		return false;
	}
	item._iCharges = item._iMaxCharges; // before the fee, as the repair above (round 23 audit)
	TakePlrsMoney(price);
	PlaySFX(IS_GOLD);
	oracool::ScheduleAutoSaveForStoreTransaction();
	return true;
}

bool ShopRechargeHeldItem()
{
	Player &myPlayer = *MyPlayer;
	const int price = RechargePriceFor(myPlayer.HoldItem);
	if (price == 0)
		return false;
	if (!PlayerCanAfford(price)) {
		stextshold = stextflag;
		stextlhold = stextup;
		StartStore(TalkID::NoMoney);
		return false;
	}
	TakePlrsMoney(price);
	myPlayer.HoldItem._iCharges = myPlayer.HoldItem._iMaxCharges;
	PlaySFX(IS_GOLD); // the recharge cursor's sound (ShopRechargeItemAt), for the same paid work
	oracool::ScheduleAutoSaveForStoreTransaction();
	return true;
}

int ShopSellOfferFor(const Item &item)
{
	// What the OPEN vendor would pay, or 0 if there is no vendor or they will not take it.
	//
	// The same two judgements the sell paths make - IsWitchShopScreen decides whose rules apply and
	// their own SellOk decides whether the item qualifies - so the price shown here and the gold
	// actually paid cannot disagree. That mattering is not hypothetical: a quoted price the shop
	// then refuses to honour is worse than quoting nothing.
	if (!oracool::IsShopGridScreen(stextflag))
		return 0;
	if (item.isEmpty() || item._itype == ItemType::Gold)
		return 0;
	const bool witch = IsWitchShopScreen(stextflag);
	if (!witch && !IsAnyOf(stextflag, TalkID::SmithBuy, TalkID::SmithPremiumBuy, TalkID::SmithUniqueBuy,
	        TalkID::SmithRareBuy, TalkID::SmithSetBuy,
	        TalkID::SmithConsumables, TalkID::SmithSell))
		return 0;
	if (!(witch ? WitchSellOk(item) : SmithSellOk(item)))
		return 0;
	return GetItemSellValue(item);
}

int ShopRepairPriceFor(const Item &item)
{
	return IsShopRepairCursorArmed() ? RepairPriceFor(item) : 0;
}

int ShopRechargePriceFor(const Item &item)
{
	return IsShopRechargeCursorArmed() ? RechargePriceFor(item) : 0;
}

int ShopRepairAllPrice()
{
	// What Repair All would cost, asked WITHOUT running it (user, 2026-08-27: "Repair All to show
	// necesary amount of gold when hovered over").
	//
	// Walks the same containers StartSmithRepair does, with the same refusals, rather than calling
	// it: StartSmithRepair rebuilds `storehold` and `storenumh`, which are the shop's live state, and
	// a drawing path must not rewrite the thing it is drawing. The duplication is the price of that,
	// and it is why this sits directly beside ShopRepairAll - if one grows a rule the other needs it.
	const Player &myPlayer = *MyPlayer;
	int64_t total = 0; // a sum of prices that can each be near INT_MAX

	for (int k = 0; k < NumRepairableBodySlots; k++) {
		const Item &worn = myPlayer.InvBody[RepairableBodySlots[k]];
		if (worn.isEmpty() || worn._iDurability == worn._iMaxDur)
			continue;
		if (worn._iOracoolEthereal)
			continue;
		total += RepairPriceFor(worn);
	}
	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (SmithRepairOk(i))
			total += RepairPriceFor(myPlayer.InvList[i]);
	}
	return static_cast<int>(std::min<int64_t>(total, std::numeric_limits<int>::max()));
}

void ShopRepairAll()
{
	// Rebuild-and-repeat, exactly as SmithRepairAllItems does, but returning to the tab the button
	// was pressed on instead of to the repair screen - the repair screen is a button now, not a
	// place you can be.
	const TalkID resume = stextflag;
	// Bounded, and the bound is not paranoia. The loop's exit depends on each pass actually
	// repairing something, so anything that charges the player without clearing the damage - a
	// storehidx encoding this function cannot decode, say - would spin here taking gold until the
	// player could no longer afford the next one. StoreHoldCapacity is storehold's capacity, so a run that
	// repairs something every pass can never reach it (a bare 48 until 2026-09-27, when the capacity grew).
	int repaired = 0;
	bool shortOfGold = false;
	for (int guard = 0; guard < StoreHoldCapacity; guard++) {
		StartSmithRepair();
		if (storenumh == 0)
			break;
		if (!PlayerCanAfford(storehold[0]._iIvalue)) {
			shortOfGold = true;
			break;
		}
		SmithRepairItemAt(storehold[0]._iIvalue, 0);
		repaired++;
	}
	// One sound for the whole batch, and only if something was mended. Here rather than in
	// SmithRepairItemAt, which the text store's single repair shares.
	if (repaired > 0)
		PlaySFX(IS_REPAIR);
	StartStore(resume);
	// Said when it stops short, as every other refused purchase is: it had stopped in silence (round 23 audit).
	if (shortOfGold)
		oracool::ShowShopToast(std::string(_("You do not have enough gold")));
}

void ShopBuyBack(int index)
{
	// Through the same filter GetShopStock used to build the tab, so the index means the same thing
	// on both sides.
	const std::vector<size_t> indices = BuybackIndicesFor(stextflag);
	if (index < 0 || index >= static_cast<int>(indices.size()))
		return;
	const size_t slot = indices[index];
	// The item exactly as it was sold, and the price it fetched - stored side by side rather than
	// the price being smuggled inside the item. See SoldItem.
	Item item = BuybackStock[slot].item;
	const int price = BuybackStock[slot].price;

	// Both refusal screens return to the tab through stextshold, so they have to be told which one
	// that is before either can fire.
	stextshold = stextflag;
	stextlhold = stextup;
	if (!PlayerCanAfford(price)) {
		StartStore(TalkID::NoMoney);
		return;
	}
	if (!StoreAutoPlace(item, false)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	TakePlrsMoney(price);
	StoreAutoPlace(item, true);
	BuybackStock.erase(BuybackStock.begin() + static_cast<ptrdiff_t>(slot));
	CalcPlrInv(*MyPlayer, true); // its usable flag against this hero, as every other purchase (round 23 audit)
	// Coins changing hands, same as every other vendor transaction (user, 2026-08-27: "when i buy
	// back an item - play gold sound"). This path completes itself instead of going through
	// ConfirmEnter, so it does not inherit the sound played there.
	PlaySFX(IS_GOLD);
	oracool::ScheduleAutoSaveForStoreTransaction();
}

/**
 * @brief The buy tabs' Sell all, as an ACTION LINE - a value no store text line can hold.
 *
 * The Sold tab's own Sell all dispatches on SmithSellAllLine(), and that number is shared: at the normal
 * font size PremiumRefreshUntilLine() is the same line. The Magic tab dispatches its actions by line, so
 * reusing it there would have fired Refresh until. The store has 24 text lines; this sits far past them
 * and is caught in ShopActivateAction before any line-based handler sees it.
 */
constexpr int GriswoldTabSellAllLine = 1000;

std::vector<oracool::ShopAction> GetShopActions(TalkID id)
{
	// The gating conditions are copied from the places that used to ADD these rows to the text list
	// - ScrollSmithSell for the two "all" buttons, ScrollSmithPremiumBuy for the two refreshes. They
	// have to match, because the Enter handlers those rows dispatch to re-test the same conditions
	// and silently do nothing when they disagree.
	std::vector<oracool::ShopAction> actions;
	if (gbIsMultiplayer)
		return actions;
	switch (id) {
	case TalkID::SmithSell:
		if (storenumh > 0)
			actions.push_back({ N_("Sell all"), SmithSellAllLine(), oracool::ShopActionKind::SellAll });
		break;
	case TalkID::SmithRepair:
		if (storenumh > 0)
			actions.push_back({ N_("Repair all"), SmithRepairAllLine(), oracool::ShopActionKind::RepairAll });
		break;
	case TalkID::SmithPremiumBuy:
		if (*sgOptions.Oracool.griswoldPremiumRefresh)
			actions.push_back({ N_("Refresh"), PremiumRefreshLine(), oracool::ShopActionKind::Refresh });
		if (*sgOptions.Oracool.refreshUntilButton)
			actions.push_back({ N_("Refresh until"), PremiumRefreshUntilLine(), oracool::ShopActionKind::RefreshUntil });
		// Sell all beside the refreshes (user, 2026-09-13: "any screen that has refresh button reduce its
		// width in half and add a second button next to it SELL ALL"). The action row divides its width
		// between whatever is on it, so this is what halves Refresh.
		actions.push_back({ N_("Sell all"), GriswoldTabSellAllLine, oracool::ShopActionKind::SellAll });
		break;
	// The other three shelves that REGENERATE (user, 2026-08-27: "Refresh on BASIC, RARE, SUPPLIES
	// tabs"). Premium is not in this list because it has its own, older switch above.
	//
	// Unique and Set are deliberately absent, and that is a rule rather than an oversight: both are
	// drawn WITHOUT replacement from a finite pool, so their shelf is already every item the pool
	// can offer. A Refresh on either would reshuffle the same contents and read as broken.
	case TalkID::SmithBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithConsumables:
		if (*sgOptions.Oracool.shopStockRefresh)
			actions.push_back({ N_("Refresh"), PremiumRefreshLine(), oracool::ShopActionKind::Refresh });
		actions.push_back({ N_("Sell all"), GriswoldTabSellAllLine, oracool::ShopActionKind::SellAll });
		break;
	// No Refresh on these two (see above), but Sell all all the same: "sell all items from any tab of
	// griswold shop". Alone on the row, it takes the whole width.
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithSetBuy:
		actions.push_back({ N_("Sell all"), GriswoldTabSellAllLine, oracool::ShopActionKind::SellAll });
		break;
	default:
		break;
	}
	return actions;
}

namespace {

/**
 * @brief The line of @p id's row of @p kind, or -1.
 *
 * Matched on KIND, never on the line number. Line indices are per-screen and collide: in English three
 * of them land on line 20 (Sell all, Repair all and Refresh until), so a by-line lookup had the Sold
 * tab's Sell-all row answering yes to "do you offer Refresh until" - and that button appeared there
 * whenever the backpack held anything (user report, 2026-09-21). See ShopActionKind in stores.h.
 */
int ActionLineOn(TalkID id, oracool::ShopActionKind kind)
{
	for (const oracool::ShopAction &action : GetShopActions(id)) {
		if (action.kind == kind)
			return action.line;
	}
	return -1;
}

} // namespace

bool ShopTabHasSellAll(TalkID id)
{
	return ActionLineOn(id, oracool::ShopActionKind::SellAll) != -1;
}

bool ShopTabHasRefresh(TalkID id)
{
	return ActionLineOn(id, oracool::ShopActionKind::Refresh) != -1;
}

void ShopRunSellAll(TalkID id)
{
	const int line = ActionLineOn(id, oracool::ShopActionKind::SellAll);
	if (line != -1)
		ShopActivateAction(id, line);
}

void ShopRunRefresh(TalkID id)
{
	const int line = ActionLineOn(id, oracool::ShopActionKind::Refresh);
	if (line != -1)
		ShopActivateAction(id, line);
}

bool ShopTabHasRefreshUntil(TalkID id)
{
	return ActionLineOn(id, oracool::ShopActionKind::RefreshUntil) != -1;
}

void ShopRunRefreshUntil(TalkID id)
{
	const int line = ActionLineOn(id, oracool::ShopActionKind::RefreshUntil);
	if (line != -1)
		ShopActivateAction(id, line);
}

/**
 * @brief Rerolls the stock behind @p id, at the depth it was first generated at.
 *
 * The three shelves reroll differently because they are stocked differently, and each says so
 * rather than pretending to a common shape it does not have.
 */
void RefreshShopStock(TalkID id)
{
	if (!*sgOptions.Oracool.shopStockRefresh || gbIsMultiplayer)
		return;
	const int lvl = VendorStockLevel();
	switch (id) {
	case TalkID::SmithBuy:
		SpawnSmith(lvl);
		break;
	case TalkID::SmithRareBuy:
		// Straight back through the generator, initialized flag untouched: the flag is about "has
		// this game built the shelf yet", and a deliberate reroll is not the same question.
		GenerateCuratedShelf(CuratedShelf::Rare, *MyPlayer, lvl);
		break;
	case TalkID::SmithConsumables:
		// Supplies is Pepin's four fixed potions PLUS Adria's stock, and only the second half is
		// generated - so this rerolls Adria and the potions stay exactly where they are. Worth
		// knowing before pressing it: a Refresh here changes what is on Adria's own Buy tab too,
		// because it is the same array.
		SpawnWitch(lvl);
		break;
	default:
		return;
	}
	// A Refresh regenerates, so the new stock needs the same one-page trim SetupTownStores applies -
	// otherwise a refreshed shelf goes back to being a view over an oversized array, which is the
	// hidden-reserve behaviour the v1.9.92 audit's finding 5 is about.
	//
	// Supplies goes through TrimWitchStockToOnePage for the same reason SetupTownStores does: it is
	// the tighter of the two pages Adria's array appears on, and the trim has to be run against
	// that one for both views to agree.
	if (id == TalkID::SmithConsumables)
		TrimWitchStockToOnePage();
	else
		oracool::TrimShopStockToOnePage(id);
	StartStore(id);
}

void ShopActivateAction(TalkID id, int line)
{
	// A buy tab's Sell all (2026-09-13): caught before the selection is touched, because its value is not
	// a text line - see GriswoldTabSellAllLine. It empties the pack to Griswold and stays on this tab.
	if (line == GriswoldTabSellAllLine) {
		oracool::PlayUiSelectSound(); // this bridge skips StoreEnter, as for Refresh below
		SmithSellAllItems(id);
		return;
	}
	// Same bridge as ShopSelectIndex: put the selection where the handler expects to find it, then
	// let the handler do its own work.
	stextsel = line;
	switch (id) {
	case TalkID::SmithSell:
		SmithSellEnter();
		break;
	case TalkID::SmithRepair:
		SmithRepairEnter();
		break;
	case TalkID::SmithPremiumBuy:
		// Refresh and Refresh until, the only two actions on this tab. The click is played HERE
		// because this bridge skips StoreEnter, which is where the text store sounds every row -
		// putting it in SmithPremiumBuyEnter would ring twice on that screen.
		oracool::PlayUiSelectSound();
		SmithPremiumBuyEnter();
		break;
	case TalkID::SmithBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithConsumables:
		RefreshShopStock(id);
		oracool::PlayUiSelectSound(); // same reason as Premium above
		break;
	default:
		break;
	}
}

void DrawSText(const Surface &out)
{
	// A shop tab is its own panel and draws none of the text box below - see oracool/shop_grid.h.
	// The vanilla box is still what Confirm, No money, No room and every towner dialog use, so this
	// is a branch rather than a replacement.
	// The Salvage tab is a shop TAB that draws a painted page rather than a grid. It is a real store
	// screen (2026-09-21), so it takes the overlap rule below with every other tab - but it draws
	// itself from scrollrt, and the vanilla text box must not be painted over it.
	if (oracool::IsShopTab(stextflag) && !oracool::IsShopGridScreen(stextflag)) {
		if (GetLeftPanelContent() != LeftPanelContent::None || oracool::IsRunewordBookOpen())
			stextflag = TalkID::None; // UpdateStoreState's reconciliation closes the page behind it
		return;
	}

	if (oracool::IsShopGridScreen(stextflag)) {
		// The shop cannot share the screen with the windows that overlap it, so the newer one wins
		// and the shop closes. Two things make this necessary rather than tidy: the shop is drawn
		// BEFORE the left-panel content, so anything opened over it is visible while the shop
		// underneath still swallows every click in its rect; and since the click router stopped
		// treating a shop as modal (so items could be dragged from the inventory), the burger menu
		// is reachable while a shop is open, which is one click from doing exactly that.
		//
		// The test lives here because this is the one place that runs every frame a shop is up.
		// Putting it at each opener would mean finding all of them, and then finding the next one.
		if (GetLeftPanelContent() != LeftPanelContent::None || oracool::IsRunewordBookOpen()) {
			stextflag = TalkID::None;
			return;
		}
		oracool::DrawShopGrid(out);
		// After the panel, so the banner reads on top of it rather than under the grid bezel. It
		// draws nothing when no message is up, and clears itself the first frame after one lapses.
		oracool::DrawShopToast(out);
		return;
	}

	if (!stextsize)
		DrawSTextBack(out);
	else
		DrawQTextBack(out);

	if (stextscrl) {
		switch (stextflag) {
		case TalkID::SmithBuy:
			ScrollSmithBuy(stextsval);
			break;
		case TalkID::SmithSell:
		case TalkID::SmithRepair:
		case TalkID::WitchSell:
		case TalkID::WitchRecharge:
		case TalkID::SmithRecharge:
		case TalkID::StorytellerIdentify:
			ScrollSmithSell(stextsval);
			break;
		case TalkID::WitchBuy:
			ScrollWitchBuy(stextsval, false);
			break;
		case TalkID::SmithConsumables:
			ScrollWitchBuy(stextsval, true);
			break;
		case TalkID::HealerBuy:
			ScrollHealerBuy(stextsval);
			break;
		case TalkID::SmithPremiumBuy:
			ScrollSmithPremiumBuy(stextsval);
			break;
		case TalkID::SmithUniqueBuy:
		case TalkID::SmithRareBuy:
		case TalkID::SmithSetBuy:
			ScrollCuratedShelfBuy(RequireCuratedShelf(stextflag), stextsval);
			break;
		default:
			break;
		}
	}

	CalculateLineHeights();
	const Point uiPosition = GetUIRectangle().position;
	for (int i = 0; i < STORE_LINES; i++) {
		if (stext[i].isDivider())
			DrawSLine(out, uiPosition.y + PaddingTop + stext[i].y + TextHeight() / 2);
		else if (stext[i].hasText())
			PrintSString(out, stext[i]._sx, i, stext[i].text, stext[i].flags, stext[i]._sval, stext[i].cursId, stext[i].cursIndent);
	}

	if (RenderGold) {
		PrintSString(out, 28, 1, fmt::format(fmt::runtime(_("Your gold: {:s}")), FormatInteger(TotalPlayerGold())).c_str(), UiFlags::ColorWhitegold | UiFlags::AlignRight);
	}

	if (stextscrl)
		DrawSSlider(out, 4, 20);
}

void StoreESC()
{
	if (qtextflag) {
		qtextflag = false;
		if (leveltype == DTYPE_TOWN)
			stream_stop();
		return;
	}

	switch (stextflag) {
	case TalkID::Smith:
	case TalkID::Witch:
	case TalkID::Boy:
	case TalkID::BoyBuy:
	// The Gamble tab closes the way his Shop tab does (user, 2026-09-24 dev note: "space doesnt close
	// it. esc doesnt close it"). It had no case here at all, so Escape - and Space, which comes
	// through this same function - fell to the end of the switch and did nothing.
	case TalkID::BoyGamble:
	case TalkID::Healer:
	case TalkID::Storyteller:
	case TalkID::Tavern:
	case TalkID::Drunk:
	case TalkID::Barmaid:
		stextflag = TalkID::None;
		break;
	case TalkID::Gossip:
		StartStore(stextshold);
		stextsel = stextlhold;
		break;
	case TalkID::SmithBuy:
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithBuy);
		break;
	case TalkID::SmithPremiumBuy:
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithPremiumBuy);
		break;
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithSetBuy: {
		// Captured BEFORE StartStore, which assigns stextflag - reading it afterwards would ask
		// which menu row to select for the screen we just left for, not the one we came from.
		const TalkID from = stextflag;
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(from);
		break;
	}
	case TalkID::SmithSell:
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithSell);
		break;
	case TalkID::SmithRepair:
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithRepair);
		break;
	case TalkID::SmithConsumables:
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithConsumables);
		break;
	case TalkID::SmithRecharge:
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithRecharge);
		break;
	case TalkID::WitchBuy:
	case TalkID::WitchSell:
	case TalkID::WitchRecharge:
		// All three land back on the one door they now share.
		StartStore(TalkID::Witch);
		stextsel = WitchShopDoorLine;
		break;
	case TalkID::HealerBuy:
		StartStore(TalkID::Healer);
		stextsel = 14;
		break;
	case TalkID::StorytellerIdentify:
		StartStore(TalkID::Storyteller);
		stextsel = 14;
		break;
	case TalkID::StorytellerIdentifyShow:
		StartStore(TalkID::StorytellerIdentify);
		break;
	case TalkID::NoMoney:
	case TalkID::NoRoom:
	case TalkID::Confirm:
		StartStore(stextshold);
		stextsel = stextlhold;
		stextsval = stextvhold;
		break;
	case TalkID::SmithTransmute:
		// Straight out, not back to his dialog: the Salvage page is a shelf, and Escape on a shelf
		// closes the shop. UpdateStoreState closes the page behind the flag.
		stextflag = TalkID::None;
		break;
	case TalkID::None:
		break;
	}
}

void StoreUp()
{
	PlaySFX(IS_TITLEMOV);
	// On a grid screen the cursor is a position in the stock, not a text line - see
	// oracool/shop_grid.h. Up and down move a whole grid row, so a stock of single-cell items walks
	// the way it looks like it should.
	if (oracool::IsShopGridScreen(stextflag)) {
		oracool::MoveShopGridSelection(0, -1);
		return;
	}
	if (stextsel == -1) {
		return;
	}

	if (stextscrl) {
		if (stextsel == stextup) {
			if (stextsval != 0)
				stextsval--;
			return;
		}

		stextsel--;
		while (!stext[stextsel].isSelectable()) {
			if (stextsel == 0)
				stextsel = STORE_LINES - 1;
			else
				stextsel--;
		}
		return;
	}

	if (stextsel == 0)
		stextsel = STORE_LINES - 1;
	else
		stextsel--;

	while (!stext[stextsel].isSelectable()) {
		if (stextsel == 0)
			stextsel = STORE_LINES - 1;
		else
			stextsel--;
	}
}

void StoreDown()
{
	PlaySFX(IS_TITLEMOV);
	if (oracool::IsShopGridScreen(stextflag)) {
		oracool::MoveShopGridSelection(0, 1);
		return;
	}
	if (stextsel == -1) {
		return;
	}

	if (stextscrl) {
		if (stextsel == stextdown) {
			if (stextsval < stextsmax)
				stextsval++;
			return;
		}

		stextsel++;
		while (!stext[stextsel].isSelectable()) {
			if (stextsel == STORE_LINES - 1)
				stextsel = 0;
			else
				stextsel++;
		}
		return;
	}

	if (stextsel == STORE_LINES - 1)
		stextsel = 0;
	else
		stextsel++;

	while (!stext[stextsel].isSelectable()) {
		if (stextsel == STORE_LINES - 1)
			stextsel = 0;
		else
			stextsel++;
	}
}

void StorePrior()
{
	PlaySFX(IS_TITLEMOV);
	if (stextsel != -1 && stextscrl) {
		if (stextsel == stextup) {
			stextsval = std::max(stextsval - 4, 0);
		} else {
			stextsel = stextup;
		}
	}
}

void StoreNext()
{
	PlaySFX(IS_TITLEMOV);
	if (stextsel != -1 && stextscrl) {
		if (stextsel == stextdown) {
			if (stextsval < stextsmax)
				stextsval += 4;
			if (stextsval > stextsmax)
				stextsval = stextsmax;
		} else {
			stextsel = stextdown;
		}
	}
}

void TakePlrsMoney(int cost)
{
	Player &myPlayer = *MyPlayer;

	myPlayer._pGold -= std::min(cost, myPlayer._pGold);

	cost = TakeGold(myPlayer, cost, true);
	if (cost != 0) {
		cost = TakeGold(myPlayer, cost, false);
	}

	Stash.gold -= cost;
	Stash.dirty = true;
}

void StoreEnter()
{
	if (qtextflag) {
		qtextflag = false;
		if (leveltype == DTYPE_TOWN)
			stream_stop();

		return;
	}

	PlaySFX(IS_TITLSLCT);

	// A grid screen has no selected TEXT LINE for the switch below to dispatch on, so it is handled
	// here rather than as another case. Escape is still the way back out, via StoreESC.
	if (oracool::IsShopGridScreen(stextflag)) {
		oracool::ActivateShopGridSelection();
		return;
	}

	switch (stextflag) {
	case TalkID::Smith:
		SmithEnter();
		break;
	case TalkID::SmithPremiumBuy:
		SmithPremiumBuyEnter();
		break;
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithSetBuy:
		CuratedShelfBuyEnter(RequireCuratedShelf(stextflag));
		break;
	case TalkID::SmithBuy:
		SmithBuyEnter();
		break;
	case TalkID::SmithSell:
		SmithSellEnter();
		break;
	case TalkID::SmithRepair:
		SmithRepairEnter();
		break;
	case TalkID::SmithConsumables:
		WitchBuyEnter();
		break;
	case TalkID::SmithRecharge:
		WitchRechargeEnter();
		break;
	case TalkID::Witch:
		WitchEnter();
		break;
	case TalkID::WitchBuy:
		WitchBuyEnter();
		break;
	case TalkID::WitchSell:
		WitchSellEnter();
		break;
	case TalkID::WitchRecharge:
		WitchRechargeEnter();
		break;
	case TalkID::NoMoney:
	case TalkID::NoRoom:
		StartStore(stextshold);
		stextsel = stextlhold;
		stextsval = stextvhold;
		break;
	case TalkID::Confirm:
		ConfirmEnter(StoreItem);
		break;
	case TalkID::Boy:
		BoyEnter();
		break;
	case TalkID::BoyBuy:
		BoyBuyEnter();
		break;
	case TalkID::Healer:
		HealerEnter();
		break;
	case TalkID::Storyteller:
		StorytellerEnter();
		break;
	case TalkID::HealerBuy:
		HealerBuyEnter();
		break;
	case TalkID::StorytellerIdentify:
		StorytellerIdentifyEnter();
		break;
	case TalkID::Gossip:
		TalkEnter();
		break;
	case TalkID::StorytellerIdentifyShow:
		StartStore(TalkID::StorytellerIdentify);
		break;
	case TalkID::Drunk:
		DrunkEnter();
		break;
	case TalkID::Tavern:
		TavernEnter();
		break;
	case TalkID::Barmaid:
		BarmaidEnter();
		break;
	case TalkID::None:
		break;
	}
}

void CheckStoreBtn()
{
	const Point uiPosition = GetUIRectangle().position;
	if (qtextflag) {
		qtextflag = false;
		if (leveltype == DTYPE_TOWN)
			stream_stop();
		return;
	}

	// The shop panel FIRST, and it absorbs every click inside itself. It is a different rect from
	// the text box this function hit-tests, and it covers the world - so a click on its background
	// that fell through would walk the player somewhere behind the panel.
	if (oracool::CheckShopGridClick(MousePosition))
		return;

	if (stextsel != -1 && MousePosition.y >= (PaddingTop + uiPosition.y) && MousePosition.y <= (320 + uiPosition.y)) {
		if (!stextsize) {
			if (MousePosition.x < 344 + uiPosition.x || MousePosition.x > 616 + uiPosition.x)
				return;
		} else {
			if (MousePosition.x < 24 + uiPosition.x || MousePosition.x > 616 + uiPosition.x)
				return;
		}

		const int relativeY = MousePosition.y - (uiPosition.y + PaddingTop);

		// Oracool bug fix: user report - "Sell all" occasionally did nothing (or something else
		// entirely) instead of selling everything, requiring several extra clicks before it
		// finally worked. Root cause: "Sell all"/"Repair all"/Griswold Premium's "Refresh"/
		// "Refresh until" all share Back's row and are clickable across a 100px-wide band from
		// the right golden border (x >= uiPosition.x + 616 - 100 = uiPosition.x + 516 - see the
		// matching redirect logic below), but this scrollbar hit-test unconditionally claims and
		// returns on *any* click with x > uiPosition.x + 600, regardless of row. That's a 16px
		// sliver (x in (600, 616]) where those buttons' own clickable zone overlaps the
		// scrollbar's, and a click landing there was swallowed here - doing nothing at all if it
		// didn't also happen to match one of the scrollbar's own arrow rows - instead of ever
		// reaching the button it visually landed on.
		const bool clickIsOnBackRowButtonZone = stext[BackButtonLine()].hasText()
		    && relativeY / LineHeight() == BackButtonLine()
		    && MousePosition.x >= uiPosition.x + 616 - 100;
		if (stextscrl && MousePosition.x > 600 + uiPosition.x && !clickIsOnBackRowButtonZone) {
			// Scroll bar is always measured in terms of the small line height.
			int y = relativeY / SmallLineHeight;
			if (y == 4) {
				if (stextscrlubtn <= 0) {
					StoreUp();
					stextscrlubtn = 10;
				} else {
					stextscrlubtn--;
				}
			}
			if (y == 20) {
				if (stextscrldbtn <= 0) {
					StoreDown();
					stextscrldbtn = 10;
				} else {
					stextscrldbtn--;
				}
			}
			return;
		}

		int y = relativeY / LineHeight();

		// Large small fonts draw beyond LineHeight. Check if the click was on the overflow text.
		if (IsSmallFontTall() && y > 0 && y < STORE_LINES
		    && stext[y - 1].hasText() && !stext[y].hasText()
		    && relativeY < stext[y - 1].y + LargeTextHeight) {
			--y;
		}

		if (y >= 5) {
			if (y >= BackButtonLine() + 1)
				y = BackButtonLine();
			// Oracool: user request - Griswold Premium's Refresh/Refresh Until buttons visually
			// overlay Back's row (see ScrollSmithPremiumBuy) instead of sitting on their own rows,
			// so a click that lands on Back's row needs to be routed to whichever of the three is
			// actually under the cursor. stext[...].hasText() is only ever true here when that
			// specific button is currently live and overlaid on this exact row, so this can't
			// misfire on any other store screen.
			if (y == BackButtonLine()) {
				y = ResolveBackRowClickLine(MousePosition.x, uiPosition.x);
			}
			// Oracool bug fix: user report - clicking in the visually-blank gap between the item
			// list and "Sell all"/"Repair all"/Back (e.g. row 19, between SmithSellAllLine's row 20
			// and the last item's rows 17-18) triggered that *last item's* individual sell
			// confirmation instead of doing nothing. Root cause: this walk-back step assumed any
			// unselectable row within 2 of a selectable one must be a continuation of that row's
			// content (e.g. a price column rendered on its own unselectable row) - true when the
			// row actually has text, but row 19 here is genuinely blank (nothing ever populates
			// it), not a continuation of anything. Blindly walking back turned "click on dead
			// space" into "click on the last item's own row", which the generic idx math below then
			// resolves to a real, sellable item by coincidence. Requiring the row to actually have
			// text before walking back leaves genuinely blank rows alone - they now fall through to
			// the final check below and correctly do nothing.
			if (stextscrl && y <= 20 && stext[y].hasText() && !stext[y].isSelectable()) {
				if (stext[y - 2].isSelectable()) {
					y -= 2;
				} else if (stext[y - 1].isSelectable()) {
					y--;
				}
			}
			if (stext[y].isSelectable() || (stextscrl && y == BackButtonLine())) {
				stextsel = y;
				StoreEnter();
			}
		}
	}
}

void ReleaseStoreBtn()
{
	stextscrlubtn = -1;
	stextscrldbtn = -1;
}

} // namespace devilution
