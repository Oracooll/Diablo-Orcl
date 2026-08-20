/**
 * @file stores.cpp
 *
 * Implementation of functionality for stores and towner dialogs.
 */
#include "stores.h"

#include <algorithm>
#include <iterator>
#include <array>
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
#include "oracool/oracool.h"
#include "oracool/skill_points.h"
#include "panels/info_box.hpp"
#include "qol/stash.h"
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
int8_t storehidx[48];
/**
 * @brief Oracool Tabbed Inventory: -1 means storehidx[i] keeps its existing InvList/belt meaning;
 * 0-8 means the item at storehold[i] came from extra tab storehTabIdx[i] (displayed as tab
 * storehTabIdx[i]+2), at InvTabList position storehidx[i] within that tab.
 */
int8_t storehTabIdx[48];
Item storehold[48];

Item smithitem[SMITH_ITEMS];
int numpremium;
int premiumlevel;
Item premiumitems[SMITH_PREMIUM_ITEMS];

constexpr int SmithUniqueItemsMaximum = 8;
Item smithUniqueItems[SmithUniqueItemsMaximum];
bool smithUniqueItemsInitialized;

Item healitem[20];

Item witchitem[WITCH_ITEMS];

int boylevel;
Item boyitem;

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

bool HasSmithUniqueShop()
{
	return !gbIsMultiplayer && *sgOptions.Oracool.griswoldSellUniqueItems;
}

std::vector<TalkID> SmithMenuEntries()
{
	std::vector<TalkID> entries { TalkID::Gossip, TalkID::SmithBuy, TalkID::SmithPremiumBuy };
	if (HasSmithUniqueShop())
		entries.push_back(TalkID::SmithUniqueBuy);
	if (!gbIsMultiplayer)
		entries.push_back(TalkID::SmithConsumables);
	entries.push_back(TalkID::SmithSell);
	entries.push_back(TalkID::SmithRepair);
	if (!gbIsMultiplayer)
		entries.push_back(TalkID::SmithRecharge);
	entries.push_back(TalkID::None);
	return entries;
}

int SmithMenuFirstLine(size_t entryCount)
{
	return entryCount >= 9 ? 6 : entryCount >= 7 ? 8
	                                             : 10;
}

int SmithMenuLine(TalkID service)
{
	const std::vector<TalkID> entries = SmithMenuEntries();
	const auto position = std::find(entries.begin(), entries.end(), service);
	if (position == entries.end())
		return SmithMenuFirstLine(entries.size());
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
	for (size_t i = 0; i < SmithPepinPotionCount; ++i)
		stock.push_back({ &smithPepinPotions[i], ConsumablesVendor::Pepin, static_cast<int>(i) });
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
		if (item.hasOracoolTier()) {
			// Oracool-tiered items (up to 3 prefixes + 3 suffixes) don't populate the
			// vanilla single-prefix/single-suffix _iPrePower/_iSufPower fields, so they
			// need their own comma-joined line built from the stored affix list instead.
			for (int i = 0; i < item._iOracoolPrefixCount; i++) {
				if (!productLine.empty())
					AppendStrView(productLine, _(",  "));
				AppendStrView(productLine, PrintOracoolAffixPower(item._iOracoolPrefixes[i], item));
			}
			for (int i = 0; i < item._iOracoolSuffixCount; i++) {
				if (!productLine.empty())
					AppendStrView(productLine, _(",  "));
				AppendStrView(productLine, PrintOracoolAffixPower(item._iOracoolSuffixes[i], item));
			}
		} else {
			if (item._iMagical != ITEM_QUALITY_UNIQUE) {
				if (item._iPrePower != -1) {
					AppendStrView(productLine, PrintItemPower(item._iPrePower, item));
				}
			}
			if (item._iSufPower != -1) {
				if (!productLine.empty())
					AppendStrView(productLine, _(",  "));
				AppendStrView(productLine, PrintItemPower(item._iSufPower, item));
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
		if (item._iMaxDur != DUR_INDESTRUCTIBLE && item._iMaxDur != 0)
			productLine += fmt::format(fmt::runtime(_("Dur: {:d}/{:d},  ")), item._iDurability, item._iMaxDur);
		else
			AppendStrView(productLine, _("Indestructible,  "));
	}

	int8_t str = item._iMinStr;
	uint8_t mag = item._iMinMag;
	int8_t dex = item._iMinDex;

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
	AddSText(40, l++, productLine, flags, false, -1, cursIndent);
}

bool StoreAutoPlace(Item &item, bool persistItem)
{
	Player &player = *MyPlayer;
	// AutoPlaceItemInInventory already falls back to the extra Tabbed Inventory tabs once tab 1
	// has no room, so no separate call is needed here.
	const bool placed = (AutoEquipEnabled(player, item) && AutoEquip(player, item, persistItem))
	    || AutoPlaceItemInBelt(player, item, persistItem)
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
	AddSText(0, 1, _("Welcome to the"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 3, _("Blacksmith's shop"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	const std::vector<TalkID> entries = SmithMenuEntries();
	const int firstLine = SmithMenuFirstLine(entries.size());
	AddSText(0, firstLine - 2, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	for (size_t i = 0; i < entries.size(); ++i) {
		const int line = firstLine + static_cast<int>(i) * 2;
		switch (entries[i]) {
		case TalkID::Gossip:
			AddSText(0, line, _("Talk to Griswold"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
			break;
		case TalkID::SmithBuy:
			AddSText(0, line, _("Buy basic items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			break;
		case TalkID::SmithPremiumBuy:
			AddSText(0, line, _("Buy premium items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			break;
		case TalkID::SmithUniqueBuy:
			AddSText(0, line, _("Buy unique items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
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
			AddSText(0, line, _("Leave the shop"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			break;
		default:
			break;
		}
	}
	AddSLine(5);
	storenumh = firstLine + static_cast<int>(entries.size() - 1) * 2;
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

void ScrollSmithUniqueBuy(int idx)
{
	ClearSText(5, 21);
	stextup = 5;
	for (int l = 5; l < 20 && idx < SmithUniqueItemsMaximum; l += 4, ++idx) {
		if (smithUniqueItems[idx].isEmpty()) {
			l -= 4;
			continue;
		}
		const UiFlags itemColor = smithUniqueItems[idx].getTextColorWithStatCheck();
		AddSText(20, l, smithUniqueItems[idx].getName(), itemColor, true, smithUniqueItems[idx]._iCurs, true);
		AddSTextVal(l, smithUniqueItems[idx]._iIvalue);
		PrintStoreItem(smithUniqueItems[idx], l + 1, itemColor, true);
		stextdown = l;
	}
	if (stextsel != -1 && !stext[stextsel].isSelectable() && stextsel != BackButtonLine())
		stextsel = stextdown;
}

bool StartSmithUniqueBuy()
{
	storenumh = 0;
	for (Item &item : smithUniqueItems) {
		if (item.isEmpty())
			continue;
		item._iStatFlag = MyPlayer->CanUseItem(item);
		++storenumh;
	}
	if (storenumh == 0) {
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithUniqueBuy);
		return false;
	}

	stextsize = true;
	stextscrl = true;
	stextsval = 0;
	RenderGold = true;
	AddSText(20, 1, _("I have these unique items for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	AddItemListBackButton();
	stextsmax = std::max(storenumh - 4, 0);
	ScrollSmithUniqueBuy(0);
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
		if (storenumh >= 48 || !sellOk(item))
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
		if (storenumh >= 48)
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

void StartWitch()
{
	FillManaPlayer();
	stextsize = false;
	stextscrl = false;
	AddSText(0, 2, _("Witch's shack"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Adria"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 14, _("Buy items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 16, _("Sell items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Recharge staves"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	// Oracool Phase 2.3: the respec, at Adria (megaplan). Selectable only when there are points to
	// reclaim; the price is on the line so the decision is made before the click.
	if (oracool::TotalInvestedSkillPoints(*MyPlayer) > 0) {
		AddSText(0, 20,
		    fmt::format(fmt::runtime(_("Reset skill points ({:d} gold)")), oracool::RespecCost(*MyPlayer)),
		    UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	} else {
		AddSText(0, 20, _("Reset skill points"), UiFlags::ColorUiSilverDark | UiFlags::AlignCenter, false);
	}
	AddSText(0, 22, _("Leave the shack"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
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
	const int stockSize = includePepinPotions
	    ? static_cast<int>(SmithConsumablesStock().size())
	    : WITCH_ITEMS;
	// stextsmax is recomputed HERE as well as in StartWitchBuy, because this function is the one
	// that runs every frame. StartWitchBuy set it once, when the screen opened; every purchase since
	// has shrunk the stock without anyone revising the bound StoreDown scrolls against, so the
	// offset was free to walk off the end of a list that had got shorter underneath it.
	stextsmax = std::max(stockSize - 4, 0);
	idx = std::clamp(idx, 0, stextsmax);
	stextsval = idx;

	for (int l = 5; l < 20; l += 4) {
		if (idx >= stockSize)
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
	uint8_t spellLevel = MyPlayer->_pSplLvl[static_cast<int8_t>(bookItem._iSpell)];
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

void AddStoreHoldRecharge(Item itm, int8_t i)
{
	storehold[storenumh] = itm;
	storehold[storenumh]._ivalue += GetSpellData(itm._iSpell).staffCost();
	storehold[storenumh]._ivalue = storehold[storenumh]._ivalue * (storehold[storenumh]._iMaxCharges - storehold[storenumh]._iCharges) / (storehold[storenumh]._iMaxCharges * 2);
	storehold[storenumh]._iIvalue = storehold[storenumh]._ivalue;
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
		if (storenumh >= 48)
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
	case TalkID::StorytellerIdentify:
		prompt = _("Are you sure you want to identify this item?");
		break;
	case TalkID::HealerBuy:
	case TalkID::SmithPremiumBuy:
	case TalkID::SmithUniqueBuy:
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
	AddSText(0, 2, _("Wirt the Peg-legged boy"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSLine(5);
	if (!boyitem.isEmpty()) {
		AddSText(0, 8, _("Talk to Wirt"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
		AddSText(0, 12, _("I have something for sale,"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
		AddSText(0, 14, _("but it will cost 50 gold"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
		AddSText(0, 16, _("just to take a look. "), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
		AddSText(0, 18, _("What have you got?"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
		AddSText(0, 20, _("Say goodbye"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	} else {
		AddSText(0, 12, _("Talk to Wirt"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
		AddSText(0, 18, _("Say goodbye"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	}
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
	AddSText(0, 1, _("Welcome to the"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 3, _("Healer's home"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Pepin"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 14, _("Buy items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Leave Healer's home"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
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
	AddSText(0, 2, _("The Town Elder"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Cain"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 14, _("Identify an item"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Say goodbye"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
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

	auto &helmet = myPlayer.InvBody[INVLOC_HEAD];
	if (IdItemOk(&helmet)) {
		idok = true;
		AddStoreHoldId(helmet, -1);
	}

	auto &armor = myPlayer.InvBody[INVLOC_CHEST];
	if (IdItemOk(&armor)) {
		idok = true;
		AddStoreHoldId(armor, -2);
	}

	auto &leftHand = myPlayer.InvBody[INVLOC_HAND_LEFT];
	if (IdItemOk(&leftHand)) {
		idok = true;
		AddStoreHoldId(leftHand, -3);
	}

	auto &rightHand = myPlayer.InvBody[INVLOC_HAND_RIGHT];
	if (IdItemOk(&rightHand)) {
		idok = true;
		AddStoreHoldId(rightHand, -4);
	}

	auto &leftRing = myPlayer.InvBody[INVLOC_RING_LEFT];
	if (IdItemOk(&leftRing)) {
		idok = true;
		AddStoreHoldId(leftRing, -5);
	}

	auto &rightRing = myPlayer.InvBody[INVLOC_RING_RIGHT];
	if (IdItemOk(&rightRing)) {
		idok = true;
		AddStoreHoldId(rightRing, -6);
	}

	auto &amulet = myPlayer.InvBody[INVLOC_AMULET];
	if (IdItemOk(&amulet)) {
		idok = true;
		AddStoreHoldId(amulet, -7);
	}

	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (storenumh >= 48)
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
		for (int t = 0; t < Player::NumExtraInventoryTabs && storenumh < 48; t++) {
			for (int i = 0; i < myPlayer._pNumInvTab[t] && storenumh < 48; i++) {
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
	AddSText(0, 1, _("Welcome to the"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 3, _("Rising Sun"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Ogden"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Leave the tavern"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	storenumh = 20;
}

void StartBarmaid()
{
	stextsize = false;
	stextscrl = false;
	AddSText(0, 2, _("Gillian"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Gillian"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	// Oracool: user request - the physical Stash Chest in town (see OperateStashChest in
	// objects.cpp) replaces Gillian as the way to access and sort the Stash.
	AddSText(0, 18, _("Say goodbye"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	storenumh = 20;
}

void StartDrunk()
{
	stextsize = false;
	stextscrl = false;
	AddSText(0, 2, _("Farnham the Drunk"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Farnham"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Say Goodbye"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	storenumh = 20;
}

void SmithEnter()
{
	const std::vector<TalkID> entries = SmithMenuEntries();
	const int offset = stextsel - SmithMenuFirstLine(entries.size());
	if (offset < 0 || offset % 2 != 0 || static_cast<size_t>(offset / 2) >= entries.size())
		return;
	const TalkID selected = entries[offset / 2];
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
		StartStore(TalkID::SmithUniqueBuy);
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
void SmithBuyItem(Item &item)
{
	TakePlrsMoney(item._iIvalue);
	if (item._iMagical == ITEM_QUALITY_NORMAL)
		item._iIdentified = false;
	StoreAutoPlace(item, true);
	int idx = stextvhold + ((stextlhold - stextup) / 4);
	if (idx == SMITH_ITEMS - 1) {
		smithitem[SMITH_ITEMS - 1].clear();
	} else {
		for (; !smithitem[idx + 1].isEmpty(); idx++) {
			smithitem[idx] = std::move(smithitem[idx + 1]);
		}
		smithitem[idx].clear();
	}
	CalcPlrInv(*MyPlayer, true);
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
void SmithBuyPItem(Item &item)
{
	// The slot scan runs FIRST (self-audit, 2026-08-15), for two reasons. It is bounded now - the
	// old loop's only condition was the skip count, so a stale selected row (the premium list
	// shrinks on every purchase, and ConfirmEnter restores the old selection over the rebuilt
	// screen) walked isEmpty() past the end of the array; and this is the copy of the scan that
	// CLEARS a slot, so overrunning would zero whatever lives after it. And it runs before the
	// money: bailing on a stale row after TakePlrsMoney would be a purchase with no goods.
	int idx = stextvhold + ((stextlhold - stextup) / 4);
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
	if (item._iMagical == ITEM_QUALITY_NORMAL)
		item._iIdentified = false;
	StoreAutoPlace(item, true);

	premiumitems[xx].clear();
	numpremium--;
	SpawnPremium(*MyPlayer);
}

void SmithBuyUniqueItem(Item &item)
{
	int idx = stextvhold + ((stextlhold - stextup) / 4);
	// Clamped before the shift loop below uses it as a write index (self-audit, 2026-08-15): a
	// negative stale index would write before the array. Checked before the money, like
	// SmithBuyPItem, so a stale row costs nothing rather than costing gold for no goods.
	if (idx < 0 || idx >= SmithUniqueItemsMaximum)
		return;
	TakePlrsMoney(item._iIvalue);
	StoreAutoPlace(item, true);
	for (; idx < SmithUniqueItemsMaximum - 1; ++idx)
		smithUniqueItems[idx] = std::move(smithUniqueItems[idx + 1]);
	smithUniqueItems[SmithUniqueItemsMaximum - 1].clear();
	CalcPlrInv(*MyPlayer, true);
}

void SmithUniqueBuyEnter()
{
	if (stextsel == BackButtonLine()) {
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithUniqueBuy);
		return;
	}

	stextshold = TalkID::SmithUniqueBuy;
	stextlhold = stextsel;
	stextvhold = stextsval;
	const int idx = stextsval + ((stextsel - stextup) / 4);
	// Stale-row guard - see SmithSellEnter (self-audit, 2026-08-15). The unique stock shrinks on
	// every purchase, and the completion's shift loop would start from this index raw.
	if (idx < 0 || idx >= SmithUniqueItemsMaximum || smithUniqueItems[idx].isEmpty())
		return;
	if (!PlayerCanAfford(smithUniqueItems[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}
	if (!StoreAutoPlace(smithUniqueItems[idx], false)) {
		StartStore(TalkID::NoRoom);
		return;
	}
	StoreItem = smithUniqueItems[idx];
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

bool StoreGoldFit(Item &item)
{
	int cost = item._iIvalue;

	Size itemSize = GetInventorySize(item);
	int itemRoomForGold = itemSize.width * itemSize.height * MaxGold;

	if (cost <= itemRoomForGold) {
		return true;
	}

	return cost <= itemRoomForGold + RoomForGold();
}

/**
 * @brief Sells an item from the player's inventory or belt.
 */
void StoreSellItem()
{
	Player &myPlayer = *MyPlayer;

	int idx = stextvhold + ((stextlhold - stextup) / 4);
	if (storehTabIdx[idx] >= 0)
		RemoveExtraTabItem(myPlayer, storehTabIdx[idx], storehidx[idx]);
	else if (storehidx[idx] >= 0)
		myPlayer.RemoveInvItem(storehidx[idx]);
	else
		myPlayer.RemoveSpdBarItem(-(storehidx[idx] + 1));

	int cost = storehold[idx]._iIvalue;
	storenumh--;
	if (idx != storenumh) {
		while (idx < storenumh) {
			storehold[idx] = storehold[idx + 1];
			storehidx[idx] = storehidx[idx + 1];
			storehTabIdx[idx] = storehTabIdx[idx + 1];
			idx++;
		}
	}

	// Oracool: sale proceeds go to the shared Stash pool, matching where a purchase's change and a
	// ground pickup's gold already land (see GoldAutoPlace, inv.cpp).
	if (oracool::IsSinglePlayer() && Stash.gold <= std::numeric_limits<int>::max() - cost) {
		Stash.gold += cost;
		Stash.dirty = true;
	} else {
		AddGoldToInventory(myPlayer, cost);
		myPlayer._pGold += cost;
	}
	oracool::ScheduleAutoSaveForStoreTransaction();
}

void SmithSellAllItems()
{
	while (true) {
		StartSmithSell();
		if (storenumh == 0)
			break;
		if (!StoreGoldFit(storehold[0])) {
			stextshold = TalkID::SmithSell;
			stextlhold = SmithSellAllLine();
			stextvhold = 0;
			StartStore(TalkID::NoRoom);
			return;
		}

		// Rebuilding the list after every removal is intentional: inventory removal compacts
		// InvList, so every later source index must be recalculated before it is used.
		stextvhold = 0;
		stextlhold = stextup;
		StoreSellItem();
	}

	StartStore(TalkID::SmithSell);
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

	if (!StoreGoldFit(storehold[idx])) {
		StartStore(TalkID::NoRoom);
		return;
	}

	StoreItem = storehold[idx];
	StartStore(TalkID::Confirm);
}

/**
 * @brief Repairs an item in the player's inventory or body in the smith.
 */
void SmithRepairItem(int price)
{
	int idx = stextvhold + ((stextlhold - stextup) / 4);
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
	TakePlrsMoney(price);
	oracool::ScheduleAutoSaveForStoreTransaction();
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

		stextvhold = 0;
		stextlhold = stextup;
		SmithRepairItem(storehold[0]._iIvalue);
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
		StartStore(TalkID::WitchBuy);
		break;
	case 16:
		StartStore(TalkID::WitchSell);
		break;
	case 18:
		StartStore(TalkID::WitchRecharge);
		break;
	case 20: {
		// Phase 2.3: the respec. The line is unselectable with nothing invested, so reaching here
		// means there is something to refund; the price gate still runs.
		const int cost = oracool::RespecCost(*MyPlayer);
		if (!PlayerCanAfford(cost)) {
			stextshold = TalkID::Witch;
			stextlhold = 20;
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
	case 22:
		stextflag = TalkID::None;
		break;
	}
}

/**
 * @brief Removes a purchased non-replenishing item from the witch's stock.
 */
void RemoveWitchStockItem(int idx)
{
	if (idx < 3)
		return;
	if (idx == WITCH_ITEMS - 1) {
		witchitem[WITCH_ITEMS - 1].clear();
	} else {
		for (; !witchitem[idx + 1].isEmpty(); idx++) {
			witchitem[idx] = std::move(witchitem[idx + 1]);
		}
		witchitem[idx].clear();
	}
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
		stextsel = 16;
		return;
	}

	stextlhold = stextsel;
	stextshold = TalkID::WitchSell;
	stextvhold = stextsval;

	int idx = stextsval + ((stextsel - stextup) / 4);

	// Stale-row guard - see SmithSellEnter (self-audit, 2026-08-15).
	if (idx < 0 || idx >= storenumh)
		return;

	if (!StoreGoldFit(storehold[idx])) {
		StartStore(TalkID::NoRoom);
		return;
	}

	StoreItem = storehold[idx];
	StartStore(TalkID::Confirm);
}

/**
 * @brief Recharges an item in the player's inventory or body in the witch.
 */
void WitchRechargeItem(int price)
{
	int idx = stextvhold + ((stextlhold - stextup) / 4);
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

void WitchRechargeEnter()
{
	if (stextsel == BackButtonLine()) {
		const bool fromSmith = stextflag == TalkID::SmithRecharge;
		StartStore(fromSmith ? TalkID::Smith : TalkID::Witch);
		stextsel = fromSmith ? SmithMenuLine(TalkID::SmithRecharge) : 18;
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
	if (!boyitem.isEmpty() && stextsel == 18) {
		if (!PlayerCanAfford(50)) {
			stextshold = TalkID::Boy;
			stextlhold = 18;
			stextvhold = stextsval;
			StartStore(TalkID::NoMoney);
		} else {
			TakePlrsMoney(50);
			StartStore(TalkID::BoyBuy);
		}
		return;
	}

	if ((stextsel != 8 && !boyitem.isEmpty()) || (stextsel != 12 && boyitem.isEmpty())) {
		stextflag = TalkID::None;
		return;
	}

	talker = TOWN_PEGBOY;
	stextshold = TalkID::Boy;
	stextlhold = stextsel;
	StartStore(TalkID::Gossip);
}

void BoyBuyItem(Item &item, int itemPrice)
{
	TakePlrsMoney(itemPrice);
	// Phase 1 gambling: the purchase IS the reveal - the roll was made at stock time, the
	// knowledge is what the gold bought.
	item._iIdentified = true;
	StoreAutoPlace(item, true);
	item.clear();
	// And the table never goes empty: a fresh unidentified roll replaces the one just sold, so
	// gambling is a LOOP rather than a once-per-dungeon-tier event. boylevel is the once-per-tier
	// gate; zeroing it is what lets SpawnBoy actually restock.
	boylevel = 0;
	SpawnBoy(MyPlayer->_pLevel);
	stextshold = TalkID::Boy;
	CalcPlrInv(*MyPlayer, true);
	stextlhold = 12;
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
	if (item._iMagical == ITEM_QUALITY_NORMAL)
		item._iIdentified = false;
	StoreAutoPlace(item, true);

	if (!gbIsMultiplayer) {
		if (idx < 2)
			return;
	} else {
		if (idx < 3)
			return;
	}
	if (idx == 19) {
		healitem[19].clear();
	} else {
		for (; !healitem[idx + 1].isEmpty(); idx++) {
			healitem[idx] = std::move(healitem[idx + 1]);
		}
		healitem[idx].clear();
	}
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
	if (entry.vendor == ConsumablesVendor::Pepin && item._iMagical == ITEM_QUALITY_NORMAL)
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
		if (idx == -1)
			myPlayer.InvBody[INVLOC_HEAD]._iIdentified = true;
		if (idx == -2)
			myPlayer.InvBody[INVLOC_CHEST]._iIdentified = true;
		if (idx == -3)
			myPlayer.InvBody[INVLOC_HAND_LEFT]._iIdentified = true;
		if (idx == -4)
			myPlayer.InvBody[INVLOC_HAND_RIGHT]._iIdentified = true;
		if (idx == -5)
			myPlayer.InvBody[INVLOC_RING_LEFT]._iIdentified = true;
		if (idx == -6)
			myPlayer.InvBody[INVLOC_RING_RIGHT]._iIdentified = true;
		if (idx == -7)
			myPlayer.InvBody[INVLOC_AMULET]._iIdentified = true;
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
			BoyBuyItem(boyitem, item._iIvalue);
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
			SmithBuyUniqueItem(item);
			break;
		default:
			break;
		}
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
	case 12:
		stextlhold = 12;
		talker = TOWN_TAVERN;
		stextshold = TalkID::Tavern;
		StartStore(TalkID::Gossip);
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
		break;
	default:
		break;
	}
}

bool HandleRefreshUntilPromptTextInputEvent(const SDL_Event &event)
{
	return HandleTextInputEvent(event, *RefreshUntilPromptInputState);
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
void DrawRefreshUntilHoverTooltip(const Surface &out)
{
	if (stextflag != TalkID::SmithPremiumBuy)
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
	Item *item;
	int v;

	item = &storehold[storenumh];
	storehold[storenumh] = *itm;

	int due = item->_iMaxDur - item->_iDurability;
	if (item->_iMagical != ITEM_QUALITY_NORMAL && item->_iIdentified) {
		v = 30 * item->_iIvalue * due / (item->_iMaxDur * 100 * 2);
		if (v == 0)
			return;
	} else {
		v = item->_ivalue * due / (item->_iMaxDur * 2);
		v = std::max(v, 1);
	}
	item->_iIvalue = v;
	item->_ivalue = v;
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

	for (auto &premiumitem : premiumitems)
		premiumitem.clear();
	for (Item &item : smithUniqueItems)
		item.clear();
	smithUniqueItemsInitialized = false;
	InitializeSmithPepinPotions();

	boyitem.clear();
	boylevel = 0;
}

void SpawnSmithUniqueItems(const Player &player)
{
	if (smithUniqueItemsInitialized || !HasSmithUniqueShop())
		return;
	smithUniqueItemsInitialized = true;

	std::vector<_unique_items> candidates;
	for (int i = 0; UniqueItems[i].UIItemId != UITYPE_INVALID; ++i) {
		if (IsUniqueAvailable(i) && UniqueItems[i].UIMinLvl <= player._pLevel)
			candidates.push_back(static_cast<_unique_items>(i));
	}

	const int requestedCount = std::clamp(*sgOptions.Oracool.griswoldUniqueShopItems, 1, SmithUniqueItemsMaximum);
	const int priceMultiplier = std::max(*sgOptions.Oracool.griswoldUniqueItemPriceMultiplier, 1);
	int generatedCount = 0;
	while (generatedCount < requestedCount && !candidates.empty()) {
		const size_t candidateIndex = static_cast<size_t>(GenerateRnd(candidates.size()));
		const _unique_items uid = candidates[candidateIndex];
		candidates.erase(candidates.begin() + candidateIndex);
		Item item;
		if (!CreateUniqueVendorItem(player, item, uid))
			continue;
		const int64_t price = static_cast<int64_t>(item._iIvalue) * priceMultiplier;
		item._iIvalue = static_cast<int>(std::min<int64_t>(price, std::numeric_limits<int>::max()));
		smithUniqueItems[generatedCount++] = std::move(item);
	}
}

void SetupTownStores()
{
	Player &myPlayer = *MyPlayer;

	int l = myPlayer._pLevel / 2;
	if (!gbIsMultiplayer) {
		l = 0;
		for (int i = 0; i < NUMLEVELS; i++) {
			if (myPlayer._pLvlVisited[i])
				l = i;
		}
	} else {
		SetRndSeed(glSeedTbl[currlevel] * SDL_GetTicks());
	}

	l = clamp(l + 2, 6, 16);
	SpawnSmith(l);
	SpawnWitch(l);
	SpawnHealer(l);
	SpawnBoy(myPlayer._pLevel);
	SpawnPremium(myPlayer);
	SpawnSmithUniqueItems(myPlayer);
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
		const bool useRed = HasAnyOf(flags, UiFlags::ColorRed);
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
		memcpy(dst, src, width);
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
	if (*sgOptions.Gameplay.showItemGraphicsInStores) {
		CreateHalfSizeItemSprites();
	}
	sbookflag = false;
	CloseInventory();
	CloseCharPanel();
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
		if (!StartSmithUniqueBuy())
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
		SStartBoyBuy();
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

void DrawSText(const Surface &out)
{
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
			ScrollSmithUniqueBuy(stextsval);
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
		StartStore(TalkID::Smith);
		stextsel = SmithMenuLine(TalkID::SmithUniqueBuy);
		break;
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
		StartStore(TalkID::Witch);
		stextsel = 14;
		break;
	case TalkID::WitchSell:
		StartStore(TalkID::Witch);
		stextsel = 16;
		break;
	case TalkID::WitchRecharge:
		StartStore(TalkID::Witch);
		stextsel = 18;
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
	case TalkID::None:
		break;
	}
}

void StoreUp()
{
	PlaySFX(IS_TITLEMOV);
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
	switch (stextflag) {
	case TalkID::Smith:
		SmithEnter();
		break;
	case TalkID::SmithPremiumBuy:
		SmithPremiumBuyEnter();
		break;
	case TalkID::SmithUniqueBuy:
		SmithUniqueBuyEnter();
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
	} else if (stextsel != -1 && MousePosition.y >= (PaddingTop + uiPosition.y) && MousePosition.y <= (320 + uiPosition.y)) {
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
