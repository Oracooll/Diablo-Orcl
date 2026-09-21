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
#include "oracool/ui_sound.h"
#include "oracool/window_close.h"
#include "player.h"
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
constexpr const char *JewellerCanvasAsset = "ui\\artisan_workshop.png";
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

/** The tab column, at the shop's own geometry so the two read as one family. */
constexpr int TabTop = 96;
constexpr Size TabSize { 27, 80 };
constexpr int TabGap = 3;

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
	Recipes,
};
constexpr int MaxTabs = 4;

/** Every control the page can hold. The press sinks one of these; the release runs it. */
enum class Control : uint8_t {
	None,
	Tab0,
	Tab1,
	Tab2,
	Tab3,
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
	Close,
};
constexpr int OptionCount = 4; // the affix as it stands, and three alternatives

bool WindowOpen = false;
WorkshopHost Host = WorkshopHost::Mystic;
Tab OpenTab = Tab::Reroll;
/** The one item on the bench. Returned to the pack when the window closes. */
Item Bench;
int SelectedRow = -1;
int StockScroll = 0;
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

Rectangle TabRect(int index)
{
	const Rectangle page = PageRect();
	return Rectangle { { page.position.x + page.size.width, page.position.y + TabTop + index * (TabSize.height + TabGap) }, TabSize };
}

/** @brief The tabs @p host shows, in column order. */
std::vector<Tab> TabsFor(WorkshopHost host)
{
	if (host == WorkshopHost::Mystic)
		return { Tab::Reroll, Tab::Imbue, Tab::Recipes };
	// Ogden's tables (user, 2026-09-21): "a list of all Gem types with the number the user curently owns of each
	// and clicking on certain type provides Upgrade/Downgrade options", the same for runes, and his jewels beside
	// them; his socket recipes are a tab away in his book.
	return { Tab::Gems, Tab::Runes, Tab::Jewels, Tab::Recipes };
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

/** @brief How many of @p tab's kinds the pack holds, one row per kind, in item order. */
struct StockRow {
	int idx = 0;
	int count = 0;
};

std::vector<StockRow> StockFor(const Player &player, Tab tab)
{
	std::vector<StockRow> rows;
	const auto add = [&](const Item *list, int count) {
		for (int i = 0; i < count; i++) {
			if (list[i].isEmpty() || !StockMatches(tab, list[i].IDidx))
				continue;
			const int units = std::max(1, list[i].stackCount());
			bool found = false;
			for (StockRow &row : rows) {
				if (row.idx == list[i].IDidx) {
					row.count += units;
					found = true;
					break;
				}
			}
			if (!found)
				rows.push_back(StockRow { static_cast<int>(list[i].IDidx), units });
		}
	};
	add(player.InvList, player._pNumInv);
	for (int tabIndex = 0; tabIndex < Player::NumExtraInventoryTabs; tabIndex++)
		add(player.InvTabList[tabIndex].data(), player._pNumInvTab[tabIndex]);
	std::sort(rows.begin(), rows.end(), [](const StockRow &a, const StockRow &b) { return a.idx < b.idx; });
	return rows;
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
	case Control::Tab3: {
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
	case Control::Upgrade:
		return IsStockTab(OpenTab) ? ButtonRect(0, 0, 2) : Rectangle { { 0, 0 }, { 0, 0 } };
	case Control::Downgrade:
		return IsStockTab(OpenTab) ? ButtonRect(0, 1, 2) : Rectangle { { 0, 0 }, { 0, 0 } };
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
		const Rectangle rect = TabRect(i);
		const bool active = tabs[i] == OpenTab;
		FillRect(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, active ? PlateEdge : PlateFill);
		OutlineRect(out, rect, active ? FrameGold : PlateEdge);
		// No rotated text in this engine: a vertical label is a stack of capitals.
		const std::string label = std::string(_(TabName(tabs[i])));
		const int lineHeight = GetLineHeight("A", GameFont12);
		int y = rect.position.y + (rect.size.height - static_cast<int>(label.size()) * lineHeight) / 2;
		for (const char ch : label) {
			const std::string one(1, static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
			DrawString(out, one, Rectangle { { rect.position.x, y }, { rect.size.width, lineHeight } },
			    { (active ? UiFlags::ColorGold : UiFlags::ColorWhitegold) | UiFlags::FontSize12 | UiFlags::AlignCenter });
			y += lineHeight;
		}
		if (rect.contains(MousePosition))
			BrightenRectRgb(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, HoverBrightenPercent);
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
void DrawStockList(const Surface &out)
{
	const std::vector<StockRow> rows = StockFor(*MyPlayer, OpenTab);
	if (rows.empty()) {
		DrawString(out, _("you carry none of these"), ListRowRect(0), { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter });
		return;
	}
	for (int row = StockScroll; row < static_cast<int>(rows.size()) && row - StockScroll < ListLines; row++) {
		const Rectangle rect = ListRowRect(row - StockScroll);
		const bool selected = SelectedRow == row;
		if (selected)
			FillRect(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, PlateFill);
		DrawString(out, _(AllItemsList[rows[row].idx].iName),
		    Rectangle { { rect.position.x + 4, rect.position.y }, { rect.size.width - 44, rect.size.height } },
		    { (selected ? UiFlags::ColorGold : UiFlags::ColorWhite) | UiFlags::FontSize12 | UiFlags::VerticalCenter });
		DrawString(out, StrCat(rows[row].count),
		    Rectangle { { rect.position.x + rect.size.width - 40, rect.position.y }, { 36, rect.size.height } },
		    { (selected ? UiFlags::ColorGold : UiFlags::ColorWhitegold) | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
		if (!selected && rect.contains(MousePosition))
			OutlineRect(out, rect, PlateEdge);
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
	StockScroll = 0;
	SelectedRow = -1;
	OfferOpen = false;
	Pressed = Control::None;
	Board.clear();
	PlayUiSelectSound();
}

void CloseWorkshop()
{
	if (!WindowOpen)
		return;
	if (!ReturnBench()) {
		LogEvent(std::string(_("Your pack is full - the bench keeps what it holds.")), UiFlags::ColorRed);
		return;
	}
	WindowOpen = false;
	OfferOpen = false;
	Pressed = Control::None;
	SelectedRow = -1;
}

bool IsWorkshopOpen()
{
	return WindowOpen;
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
	Bench.clear();
	Counters.clear();
	Board.clear();
}

void DrawWorkshop(const Surface &out)
{
	if (!WindowOpen)
		return;
	const Rectangle page = PageRect();
	// The user's painted canvas, or the shared 340x720 side panel until it lands.
	const char *canvas = Host == WorkshopHost::Mystic ? MysticCanvasAsset : JewellerCanvasAsset;
	if (GetLoosePngSize(canvas).width > 0) {
		DrawLoosePng(out, canvas, page.position);
	} else if (HasSidePanelArt()) {
		DrawSidePanelArt(out, page.position);
	} else {
		DrawThemedFill(out, page);
		DrawOrnateBorder(out, page);
	}
	DrawString(out, Host == WorkshopHost::Mystic ? _("Mystic Workshop") : _("Jeweller's Tables"), Panel(TitleRect),
	    { UiFlags::ColorGold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	DrawTabColumn(out);
	if (!IsStockTab(OpenTab))
		DrawBench(out);

	if (IsStockTab(OpenTab)) {
		DrawStockList(out);
		const std::vector<StockRow> rows = StockFor(*MyPlayer, OpenTab);
		const bool picked = SelectedRow >= 0 && SelectedRow < static_cast<int>(rows.size());
		const int up = picked ? StepUp(OpenTab, rows[SelectedRow].idx) : 0;
		const int down = picked ? StepDown(OpenTab, rows[SelectedRow].idx) : 0;
		DrawPlateButton(out, Control::Upgrade, StrCat(_("UPGRADE"), "  ", StepUpCost(OpenTab), _(" -> 1")), picked && up != 0 && rows[SelectedRow].count >= StepUpCost(OpenTab));
		DrawPlateButton(out, Control::Downgrade, StrCat(_("DOWNGRADE"), _("  1 -> 2")), picked && down != 0 && rows[SelectedRow].count >= 1);
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

	DrawString(out, StrCat(_("Gold"), ": ", FormatInteger(static_cast<int>(TotalPlayerGold()))), Panel(GoldRect),
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	DrawBoard(out);
	DrawWindowCloseButtonAt(out, Panel(CloseRect));

	// The hover sound, once as the cursor arrives on a control.
	Control hoveredNow = Control::None;
	for (const Control control : { Control::Tab0, Control::Tab1, Control::Tab2, Control::Tab3, Control::Reroll,
	         Control::Imbue, Control::Remove, Control::Cleanse, Control::Upgrade, Control::Downgrade,
	         Control::Option0, Control::Option1, Control::Option2, Control::Option3 }) {
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
void RunControl(Control control)
{
	switch (control) {
	case Control::Tab0:
	case Control::Tab1:
	case Control::Tab2:
	case Control::Tab3: {
		const std::vector<Tab> tabs = TabsFor(Host);
		const int slot = static_cast<int>(control) - static_cast<int>(Control::Tab0);
		if (slot >= static_cast<int>(tabs.size()) || tabs[slot] == OpenTab)
			break;
		if (tabs[slot] == Tab::Recipes) {
			// The artisan's own recipe book, on its docked page - the workshop stands down while it is up.
			const WorkshopHost host = Host;
			CloseWorkshop();
			if (!IsWorkshopOpen())
				OpenLevskiWindowFor(host == WorkshopHost::Mystic ? TransmuteHost::Barmaid : TransmuteHost::Tavern);
			break;
		}
		OpenTab = tabs[slot];
		SelectedRow = -1;
		StockScroll = 0;
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
		Player &player = *MyPlayer;
		const std::vector<StockRow> rows = StockFor(player, OpenTab);
		if (SelectedRow < 0 || SelectedRow >= static_cast<int>(rows.size())) {
			SetBoard(std::string(_("Choose a kind from the list first.")));
			break;
		}
		const int idx = rows[SelectedRow].idx;
		const bool up = control == Control::Upgrade;
		const int made = up ? StepUp(OpenTab, idx) : StepDown(OpenTab, idx);
		if (made == 0) {
			SetBoard(std::string(up ? _("Nothing stands above this one.") : _("Nothing stands below this one.")));
			break;
		}
		const int cost = up ? StepUpCost(OpenTab) : 1;
		if (CountInPack(player, idx) < cost) {
			SetBoard(fmt::format(fmt::runtime(_("You need {:d} of those.")), cost));
			break;
		}
		TakeFromPack(player, idx, cost);
		const int placed = GiveToPack(player, made, up ? 1 : 2);
		CalcPlrInv(player, true);
		SelectedRow = -1;
		if (placed == 0) {
			SetBoard(std::string(_("Your pack had no room - the work was undone.")));
			GiveToPack(player, idx, cost); // put them back rather than swallow them
			break;
		}
		SetBoard(StrCat(_("Made"), " ", placed, " ", _(AllItemsList[made].iName)));
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
	for (const Control control : { Control::Tab0, Control::Tab1, Control::Tab2, Control::Tab3, Control::Close,
	         Control::Reroll, Control::Imbue, Control::Remove, Control::Cleanse, Control::Upgrade, Control::Downgrade,
	         Control::Option0, Control::Option1, Control::Option2, Control::Option3 }) {
		const Rectangle rect = ControlRect(control);
		if (rect.size.width == 0 || !rect.contains(position))
			continue;
		Pressed = control;
		PlayUiMoveSound();
		return true;
	}

	// The bench takes an item from the cursor and gives it back to an empty hand.
	Player &player = *MyPlayer;
	if (Panel(SlotRect).contains(position)) {
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
		const std::vector<StockRow> rows = StockFor(player, OpenTab);
		for (int row = StockScroll; row < static_cast<int>(rows.size()) && row - StockScroll < ListLines; row++) {
			if (!ListRowRect(row - StockScroll).contains(position))
				continue;
			SelectedRow = SelectedRow == row ? -1 : row;
			PlayUiSelectSound();
			return true;
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
