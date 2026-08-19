#include "oracool/runeword_book.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "control.h"
#include "cursor.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "itemdat.h"
#include "oracool/hud_layout.h"
#include "oracool/ornate_border.h"
#include "oracool/runewords.h"
#include "oracool/window_close.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

bool BookOpen = false;

constexpr Size WindowSize { 944, 616 };
constexpr int Padding = 10;
constexpr int TitleHeight = 22;

/** @brief The slot filter keys. One per RunewordHost, in the enum's own order. */
constexpr int SlotFilterCount = 10;
constexpr const char *SlotFilterNames[SlotFilterCount] = {
	"Weapon", "Shield", "Armor", "Helm", "Shoulders", "Bracers", "Gloves", "Belt", "Pants", "Boots",
};
constexpr int SlotKeyHeight = 20;
constexpr int SlotKeyGap = 4;

/** @brief The rune filter row: every rune, evenly spread across the window's width. */
constexpr int RuneRowHeight = 30;

/** @brief One list entry. Four columns of these scroll together. */
constexpr int ColumnCount = 4;
constexpr int EntryHeight = 74;
constexpr int EntryGap = 4;
constexpr int LineHeight = 12;

constexpr uint8_t KeyLitColor = PAL16_YELLOW + 4;
constexpr uint8_t KeyDimColor = PAL16_YELLOW + 12;

int ScrollOffsetRows = 0;

/**
 * @brief Which filter keys are lit. Two independent sets - see the header on how they combine.
 *
 * Plain arrays rather than a bitmask: 33 runes needs a 64-bit mask and the debugging value of being
 * able to print one of these is worth more than the eight bytes.
 */
std::array<bool, SlotFilterCount> SlotSelected {};
std::vector<bool> RuneSelected;

/** @brief The rune item indices, in ladder order, built once. */
std::vector<uint16_t> RuneIndices;

void EnsureRuneList()
{
	if (!RuneIndices.empty())
		return;
	for (int i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (IsOracoolRuneIdx(i))
			RuneIndices.push_back(static_cast<uint16_t>(i));
	}
	RuneSelected.assign(RuneIndices.size(), false);
}

/** @brief Whether @p word passes both filter rows. See the header: OR within a row, AND across. */
bool PassesFilters(const RunewordDefinition &word)
{
	const bool anySlot = std::any_of(SlotSelected.begin(), SlotSelected.end(), [](bool b) { return b; });
	if (anySlot) {
		if (word.host >= SlotFilterCount || !SlotSelected[word.host])
			return false;
	}

	const bool anyRune = std::any_of(RuneSelected.begin(), RuneSelected.end(), [](bool b) { return b; });
	if (anyRune) {
		bool matched = false;
		for (size_t r = 0; r < RuneIndices.size() && !matched; r++) {
			if (!RuneSelected[r])
				continue;
			for (int k = 0; k < word.runeCount; k++) {
				if (word.runes[k] == RuneIndices[r]) {
					matched = true;
					break;
				}
			}
		}
		if (!matched)
			return false;
	}
	return true;
}

/**
 * @brief The words to show, grouped by host.
 *
 * Rebuilt every frame rather than cached on filter change. 370 rows of pointer-copying is nothing
 * next to the drawing that follows it, and a cache would be a second source of truth that has to be
 * invalidated from four places - the two filter rows, the open, and any future data reload.
 */
std::vector<const RunewordDefinition *> VisibleWords()
{
	std::vector<const RunewordDefinition *> out;
	// Grouped by iterating hosts in enum order, which is also the order the filter keys are drawn
	// in - so the list's grouping and the key row read as the same ordering.
	for (int host = 0; host < SlotFilterCount; host++) {
		for (size_t i = 0; i < RunewordCount(); i++) {
			const RunewordDefinition *word = RunewordAt(i);
			if (word == nullptr || word->host != host)
				continue;
			if (PassesFilters(*word))
				out.push_back(word);
		}
	}
	return out;
}

Rectangle ContentRect()
{
	const Rectangle window = GetRunewordBookRect();
	const int top = window.position.y + Padding + TitleHeight + SlotKeyHeight + SlotKeyGap + RuneRowHeight + SlotKeyGap;
	return { { window.position.x + Padding, top },
		{ window.size.width - Padding * 2, window.position.y + window.size.height - Padding - top } };
}

Rectangle SlotKeyRect(int index)
{
	const Rectangle window = GetRunewordBookRect();
	const int usable = window.size.width - Padding * 2;
	const int keyWidth = (usable - SlotKeyGap * (SlotFilterCount - 1)) / SlotFilterCount;
	return { { window.position.x + Padding + index * (keyWidth + SlotKeyGap),
		         window.position.y + Padding + TitleHeight },
		{ keyWidth, SlotKeyHeight } };
}

Rectangle RuneKeyRect(size_t index)
{
	const Rectangle window = GetRunewordBookRect();
	const int usable = window.size.width - Padding * 2;
	const int count = static_cast<int>(RuneIndices.size());
	// Evenly distributed across the full width, as asked. Integer division leaves a remainder of at
	// most count-1 pixels at the right; spreading it would cost a per-key offset table for a gap
	// nobody can see at this size.
	const int step = count > 0 ? usable / count : usable;
	return { { window.position.x + Padding + static_cast<int>(index) * step,
		         window.position.y + Padding + TitleHeight + SlotKeyHeight + SlotKeyGap },
		{ step, RuneRowHeight } };
}

int RowsPerScreen()
{
	return std::max(1, ContentRect().size.height / (EntryHeight + EntryGap));
}

int TotalRows(size_t wordCount)
{
	return static_cast<int>((wordCount + ColumnCount - 1) / ColumnCount);
}

Rectangle EntryRect(int row, int column)
{
	const Rectangle content = ContentRect();
	const int columnWidth = content.size.width / ColumnCount;
	return { { content.position.x + column * columnWidth,
		         content.position.y + (row - ScrollOffsetRows) * (EntryHeight + EntryGap) },
		{ columnWidth - EntryGap, EntryHeight } };
}

/** @brief The word's stat lines, only the ones it actually carries. */
std::vector<std::string> StatLines(const RunewordDefinition &word)
{
	std::vector<std::string> lines;
	const auto add = [&lines](const char *label, int value, const char *suffix = "") {
		if (value != 0)
			lines.push_back(StrCat(label, " ", value > 0 ? "+" : "", value, suffix));
	};
	add("Dam", word.bonusDamagePercent, "%");
	add("Dam", word.damageMod);
	add("ToHit", word.toHit);
	add("Res", word.allResists, "%");
	add("AC", word.bonusAc);
	add("Spells", word.spellLevels);
	add("Mana", word.mana);
	add("Life", word.hitPoints);
	return lines;
}

void DrawRuneIcon(const Surface &out, uint16_t runeIdx, Point centre, int boxSize)
{
	const int cursId = AllItemsList[runeIdx].iCurs + CURSOR_FIRSTITEM;
	const ClxSprite sprite = GetInvItemSprite(cursId);
	// Item sprites are drawn from their BOTTOM-left in this engine, which is why the y here adds
	// half the box rather than subtracting it - the same convention DrawInv uses.
	const Point position { centre.x - sprite.width() / 2, centre.y + boxSize / 2 };
	ClxDraw(out, position, sprite);
}

void DrawEntry(const Surface &out, const RunewordDefinition &word, Rectangle rect)
{
	DrawString(out, _(word.name), Rectangle { rect.position, { rect.size.width, LineHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 });

	const int host = word.host < SlotFilterCount ? word.host : 0;
	DrawString(out, _(SlotFilterNames[host]),
	    Rectangle { rect.position + Displacement { 0, LineHeight }, { rect.size.width, LineHeight } },
	    { UiFlags::ColorBlue | UiFlags::FontSize12 });

	// The recipe, as the runes' own icons in order - the picture the user asked for, and more use
	// than their names: a rune is recognised in the stash by its icon, not by reading it.
	const int iconBox = 22;
	for (int i = 0; i < word.runeCount; i++) {
		const Point centre { rect.position.x + iconBox / 2 + i * (iconBox + 2),
			rect.position.y + LineHeight * 2 + iconBox / 2 };
		DrawRuneIcon(out, word.runes[i], centre, iconBox);
	}

	int y = rect.position.y + LineHeight * 2 + iconBox + 2;
	for (const std::string &line : StatLines(word)) {
		if (y + LineHeight > rect.position.y + rect.size.height)
			break; // the entry box is fixed; a word with more lines than fit shows what it can
		DrawString(out, line, Rectangle { { rect.position.x, y }, { rect.size.width, LineHeight } },
		    { UiFlags::ColorWhite | UiFlags::FontSize12 });
		y += LineHeight;
	}
}

} // namespace

bool IsRunewordBookOpen()
{
	return BookOpen;
}

Rectangle GetRunewordBookRect()
{
	// Centred horizontally, and centred in the band between the top of the screen and the top of the
	// HUD plate - "the area above the hud", which is what the user asked for. Clamped to y >= 0 so a
	// short screen puts it at the top rather than off it.
	const Rectangle plate = GetMiddleHudRect();
	const int available = plate.position.y;
	const int y = std::max(0, (available - WindowSize.height) / 2);
	return { { (gnScreenWidth - WindowSize.width) / 2, y }, WindowSize };
}

void OpenRunewordBook()
{
	EnsureRuneList();
	BookOpen = true;
	ScrollOffsetRows = 0;
}

void CloseRunewordBook()
{
	BookOpen = false;
}

void ToggleRunewordBook()
{
	if (BookOpen)
		CloseRunewordBook();
	else
		OpenRunewordBook();
}

void DrawRunewordBook(const Surface &out)
{
	if (!BookOpen)
		return;
	EnsureRuneList();

	const Rectangle window = GetRunewordBookRect();
	DrawHalfTransparentRectTo(out, window.position.x, window.position.y, window.size.width, window.size.height);
	DrawOrnateBorder(out, window);
	DrawWindowCloseButton(out, window);

	DrawString(out, _("Runeword Book"),
	    Rectangle { window.position + Displacement { Padding, Padding }, { window.size.width - Padding * 2, TitleHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize24 | UiFlags::AlignCenter });

	for (int i = 0; i < SlotFilterCount; i++) {
		const Rectangle key = SlotKeyRect(i);
		DrawOrnateBorder(out, key);
		DrawString(out, _(SlotFilterNames[i]), key,
		    { (SlotSelected[i] ? UiFlags::ColorWhitegold : UiFlags::ColorBlue)
		        | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}

	for (size_t i = 0; i < RuneIndices.size(); i++) {
		const Rectangle key = RuneKeyRect(i);
		// A lit rune gets a frame; an unlit one is just its icon. Cheaper to read at 33 across than
		// two shades of the same picture would be.
		if (RuneSelected[i])
			UnsafeDrawBorder2px(out, key, KeyLitColor);
		DrawRuneIcon(out, RuneIndices[i],
		    { key.position.x + key.size.width / 2, key.position.y + 2 }, RuneRowHeight - 4);
	}

	const std::vector<const RunewordDefinition *> words = VisibleWords();
	const int rows = TotalRows(words.size());
	const int perScreen = RowsPerScreen();
	ScrollOffsetRows = std::clamp(ScrollOffsetRows, 0, std::max(0, rows - perScreen));

	if (words.empty()) {
		DrawString(out, _("No runewords match these filters."), ContentRect(),
		    { UiFlags::ColorGold | UiFlags::FontSize12 | UiFlags::AlignCenter });
		return;
	}

	for (int row = ScrollOffsetRows; row < std::min(rows, ScrollOffsetRows + perScreen); row++) {
		for (int column = 0; column < ColumnCount; column++) {
			const size_t index = static_cast<size_t>(row) * ColumnCount + column;
			if (index >= words.size())
				break;
			DrawEntry(out, *words[index], EntryRect(row, column));
		}
	}
}

bool HandleRunewordBookClick(Point position)
{
	if (!BookOpen)
		return false;
	const Rectangle window = GetRunewordBookRect();
	if (!window.contains(position))
		return false;

	if (GetWindowCloseButtonRect(window).contains(position)) {
		CloseRunewordBook();
		return true;
	}

	for (int i = 0; i < SlotFilterCount; i++) {
		if (SlotKeyRect(i).contains(position)) {
			// Toggle, not select: clicking a lit key clears it, which is what makes a multi-select
			// row usable without a separate reset control.
			SlotSelected[i] = !SlotSelected[i];
			ScrollOffsetRows = 0;
			return true;
		}
	}

	for (size_t i = 0; i < RuneIndices.size(); i++) {
		if (RuneKeyRect(i).contains(position)) {
			RuneSelected[i] = !RuneSelected[i];
			ScrollOffsetRows = 0;
			return true;
		}
	}

	// Anywhere else inside the window: consumed and ignored. Returning false here would let the
	// click through to the world, which is the standing rule this window must not break.
	return true;
}

bool HandleRunewordBookScroll(int delta)
{
	if (!BookOpen)
		return false;
	if (!GetRunewordBookRect().contains(MousePosition))
		return false;
	ScrollOffsetRows = std::max(0, ScrollOffsetRows - delta);
	return true;
}

} // namespace devilution::oracool
