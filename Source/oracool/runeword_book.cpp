#include "oracool/runeword_book.h"
#include "oracool/levski_roar.h" // IsLevskiRoarOpen - not over a refused close
#include "oracool/workshop.h"

#include "oracool/book_frame.h" // the painted wide frame

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include "automap.h"
#include "diablo.h" // CloseAllWindows
#include "control.h"
#include "cursor.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "itemdat.h"
#include "oracool/hud_art.h" // the slot keys' plate
#include "oracool/hud_layout.h"
#include "oracool/hud_menu.h"
#include "oracool/ornate_border.h"
#include "oracool/runewords.h"
#include "oracool/shop_grid.h" // DrawVendorButtonBacking - the filters wear the vendors' tab face
#include "oracool/ui_sound.h"
#include "oracool/window_close.h"
#include "player.h"
#include "qol/stash.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "stores.h" // ForceCloseStore

namespace devilution::oracool {

namespace {

bool BookOpen = false;

constexpr Size WindowSize { 944, 616 };
constexpr int Padding = 30; // the painted frame's bezel is 21-24 deep on every side (book_frame.cpp); was 10 inside the drawn border
constexpr int TitleHeight = 22;

/**
 * @brief Air between the title and the slot filter row (user, 2026-08-20: "move first row filter
 * 8 px down to leave a gap between it and title").
 */
constexpr int TitleToFilterGap = 8;

constexpr int SlotFilterCount = 10;
constexpr const char *SlotFilterNames[SlotFilterCount] = {
	"Weapon", "Shield", "Armor", "Helm", "Shoulders", "Bracers", "Gloves", "Belt", "Pants", "Boots",
};
constexpr int SlotKeyHeight = 20;
constexpr int SlotKeyGap = 4;

/*
 * Oracool: the slot keys' own art (batch 11, 2026-09-11). One BLANK plate in three 84x20 rows - idle,
 * hover, selected - with the label still the game's, drawn on top. SlotKeyRect's width is computed,
 * so the assert holds it to the plate: a book that changes width fails the build, not the look.
 */
constexpr const char *RunewordKeyArt = "ui\\runeword_key.png";
constexpr Size RunewordKeyCell { 84, 20 };
static_assert(RunewordKeyCell.height == SlotKeyHeight
        && RunewordKeyCell.width == (WindowSize.width - Padding * 2 - SlotKeyGap * (SlotFilterCount - 1)) / SlotFilterCount,
    "runeword_key.png's rows no longer match SlotKeyRect");

/**
 * @brief The rune row's height, and the gap that keeps it clear of the slot row above it.
 *
 * A rune's inventory sprite is 28px tall - one grid cell - and item sprites in this engine are
 * drawn from their BOTTOM-left. The first build passed the row's TOP as if it were a centre, so
 * every icon extended thirteen pixels upward into the slot keys. The row is sized to the sprite and
 * the icons are bottom-anchored inside it now, which is what makes "no overlapping" structural
 * rather than a tuned constant.
 */
constexpr int RuneIconSize = 28;
constexpr int RuneRowHeight = RuneIconSize + 4;
constexpr int RuneRowGap = 6;

// Five columns (user, 2026-08-20: "there is enough space"). 944 less 20 of padding is 924, so a
// column is 184 wide and an entry 178 - which is exactly six rune icons at 28 plus their gaps, the
// longest recipe the data holds. Any narrower and the six-rune words would wrap.
constexpr int ColumnCount = 5;
constexpr int ColumnGap = 6;
constexpr int LineHeight = 12;
/** @brief Title line, host line, then the recipe icons. Stat lines are added to this per word. */
constexpr int EntryHeaderHeight = LineHeight * 2 + RuneIconSize + 4;
constexpr int EntryGap = 8;

constexpr uint8_t KeyLitColor = PAL16_YELLOW + 4;
/** @brief The Possible-Runewords toggle. Yellow, per the user, and bright enough to read as a control. */
constexpr uint8_t PossibleFilterColor = PAL16_YELLOW + 3;
constexpr int PossibleFilterSize = 18;

int ScrollOffsetPx = 0;

/**
 * @brief The title row's buttons (user, 2026-09-25 dev note: "add more filter buttons - chest icon and buttons
 * 2,3,4,5,6. add them on the title row. 6px apart from each other. the left most one aligned flush with left end
 * of weapon filter button. chest icon replaces gold X, number icons filter words according to how many runes
 * they required"). Index 0 is the chest - the Possible-Runewords toggle the yellow X used to be - and 1..5 are
 * the rune counts 2..6.
 */
constexpr int TitleButtonGap = 6;
constexpr Size TitleButtonSize { 28, TitleHeight };
constexpr int RuneCountFilterFirst = 2;
constexpr int RuneCountFilterCount = 5; // 2, 3, 4, 5, 6 runes
std::array<bool, RuneCountFilterCount> RuneCountSelected {};

std::array<bool, SlotFilterCount> SlotSelected {};
std::vector<bool> RuneSelected;
/**
 * Which of the two rune-filter meanings is in force (user, 2026-09-05). POSSIBLE, lit by the yellow
 * toggle, means "buildable from the runes I hold": every rune of the word must be held. A manual
 * rune selection means "requires these": the word must contain every SELECTED rune, and may need
 * more - "it is not to be mistaken as an exact match". The two are different questions, so the
 * toggle records which one the selection answers.
 */
bool PossibleMode = false;
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

bool AnyRuneSelected()
{
	return std::any_of(RuneSelected.begin(), RuneSelected.end(), [](bool b) { return b; });
}

bool PassesFilters(const RunewordDefinition &word)
{
	const bool anySlot = std::any_of(SlotSelected.begin(), SlotSelected.end(), [](bool b) { return b; });
	if (anySlot && (word.host >= SlotFilterCount || !SlotSelected[word.host]))
		return false;
	// The rune-count buttons: any of the lit counts, like the slot keys - several lit is an OR.
	const bool anyCount = std::any_of(RuneCountSelected.begin(), RuneCountSelected.end(), [](bool b) { return b; });
	if (anyCount) {
		const int slot = static_cast<int>(word.runeCount) - RuneCountFilterFirst;
		if (slot < 0 || slot >= RuneCountFilterCount || !RuneCountSelected[slot])
			return false;
	}

	if (AnyRuneSelected()) {
		if (PossibleMode) {
			// Buildable: every rune the word needs is among the selected (held) ones.
			for (int k = 0; k < word.runeCount; k++) {
				bool held = false;
				for (size_t r = 0; r < RuneIndices.size() && !held; r++) {
					if (RuneSelected[r] && RuneIndices[r] == word.runes[k])
						held = true;
				}
				if (!held)
					return false;
			}
		} else {
			// Requires: every SELECTED rune is among the word's - the word may need others too.
			for (size_t r = 0; r < RuneIndices.size(); r++) {
				if (!RuneSelected[r])
					continue;
				bool inWord = false;
				for (int k = 0; k < word.runeCount && !inWord; k++) {
					if (word.runes[k] == RuneIndices[r])
						inWord = true;
				}
				if (!inWord)
					return false;
			}
		}
	}
	return true;
}

/** @brief The word's stat lines - every non-zero field the definition carries. */
std::vector<std::string> StatLines(const RunewordDefinition &word)
{
	// The word's own bonuses, from the one describer the item panel uses (2026-09-05).
	return RunewordBonusLines(word);
}

/**
 * @brief What each of the word's runes does when set in the word's host, one line per rune
 * (user, 2026-09-05: "show each rune's socket effect under the word in the book") - the other half
 * of a finished word, which the book never showed.
 */
std::vector<std::string> RuneLines(const RunewordDefinition &word)
{
	std::vector<std::string> lines;
	const SocketHost host = RunewordSocketHost(static_cast<RunewordHost>(word.host));
	for (int i = 0; i < word.runeCount; i++) {
		std::string line = GemSocketLine(word.runes[i], host);
		if (!line.empty())
			lines.push_back(std::move(line));
	}
	return lines;
}

int EntryHeight(const RunewordDefinition &word)
{
	// Cached per word (round 35 audit): the layout ran every frame and formatted every word's lines to count them.
	static std::unordered_map<const RunewordDefinition *, int> Heights;
	if (const auto it = Heights.find(&word); it != Heights.end())
		return it->second;
	const int height = EntryHeaderHeight + static_cast<int>(StatLines(word).size() + RuneLines(word).size()) * LineHeight;
	Heights.emplace(&word, height);
	return height;
}

std::vector<const RunewordDefinition *> VisibleWords()
{
	std::vector<const RunewordDefinition *> out;
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

Rectangle SlotKeyRect(int index)
{
	const Rectangle window = GetRunewordBookRect();
	const int usable = window.size.width - Padding * 2;
	const int keyWidth = (usable - SlotKeyGap * (SlotFilterCount - 1)) / SlotFilterCount;
	return { { window.position.x + Padding + index * (keyWidth + SlotKeyGap),
		         window.position.y + Padding + TitleHeight + TitleToFilterGap },
		{ keyWidth, SlotKeyHeight } };
}

int RuneRowTop()
{
	const Rectangle window = GetRunewordBookRect();
	return window.position.y + Padding + TitleHeight + TitleToFilterGap + SlotKeyHeight + RuneRowGap;
}

Rectangle RuneKeyRect(size_t index)
{
	const Rectangle window = GetRunewordBookRect();
	const int usable = window.size.width - Padding * 2;
	const int count = std::max<int>(1, static_cast<int>(RuneIndices.size()));
	const int step = usable / count;
	return { { window.position.x + Padding + static_cast<int>(index) * step, RuneRowTop() },
		{ step, RuneRowHeight } };
}

/** @brief Title-row button @p index: 0 the chest, 1..5 the rune counts. Flush left with the Weapon key, 6px apart. */
Rectangle TitleButtonRect(int index)
{
	const Rectangle window = GetRunewordBookRect();
	return { { SlotKeyRect(0).position.x + index * (TitleButtonSize.width + TitleButtonGap), window.position.y + Padding },
		TitleButtonSize };
}

/** @brief The Possible-Runewords toggle: the chest, first on the title row since 2026-09-25 (it was a yellow X in the corner). */
Rectangle PossibleFilterRect()
{
	return TitleButtonRect(0);
}

Rectangle ContentRect()
{
	const Rectangle window = GetRunewordBookRect();
	const int top = RuneRowTop() + RuneRowHeight + RuneRowGap;
	return { { window.position.x + Padding, top },
		{ window.size.width - Padding * 2, window.position.y + window.size.height - Padding - top } };
}

/**
 * @brief Bottom-anchors a rune's inventory sprite inside @p box, horizontally centred.
 *
 * Item sprites are drawn from their bottom-left in this engine. Taking a rect rather than a point is
 * the whole fix for the overlap: a caller cannot get the anchor convention wrong from here.
 */
/**
 * @brief A palette table that lifts every colour a few steps up its ramp - the "brighter" a rune
 * key shows under the cursor (user, 2026-09-05: "when i hover over rune filter make runes brighter").
 *
 * The palette is sixteen-entry ramps, light to dark, except 128..159, which are four eight-entry
 * mini-ramps; each index moves toward its ramp's light end without leaving the ramp. Index 0 is
 * the sprites' transparent key and stays 0.
 */
const uint8_t *BrightenTRN()
{
	static std::array<uint8_t, 256> table = [] {
		std::array<uint8_t, 256> t {};
		for (int i = 1; i < 256; i++) {
			const bool mini = i >= 128 && i < 160;
			const int rampLength = mini ? 8 : 16;
			const int base = mini ? 128 + ((i - 128) / 8) * 8 : (i / 16) * 16;
			t[static_cast<size_t>(i)] = static_cast<uint8_t>(std::max(base, i - rampLength / 5));
		}
		return t;
	}();
	return table.data();
}

void DrawRuneIcon(const Surface &out, uint16_t runeIdx, Rectangle box, const uint8_t *trn = nullptr)
{
	const int cursId = AllItemsList[runeIdx].iCurs + CURSOR_FIRSTITEM;
	const ClxSprite sprite = GetInvItemSprite(cursId);
	const Point position { box.position.x + (box.size.width - static_cast<int>(sprite.width())) / 2,
		box.position.y + box.size.height };
	if (trn != nullptr)
		ClxDrawTRN(out, position, sprite, trn);
	else
		ClxDraw(out, position, sprite);
}

/** @brief Every rune index the player is carrying, in the backpack, on the belt, or in the stash. */
std::vector<uint16_t> HeldRunes()
{
	std::vector<uint16_t> held;
	const auto note = [&held](const Item &item) {
		if (item.isEmpty())
			return;
		const int idx = static_cast<int>(item.IDidx);
		if (IsOracoolRuneIdx(idx) && std::find(held.begin(), held.end(), idx) == held.end())
			held.push_back(static_cast<uint16_t>(idx));
	};

	if (MyPlayer != nullptr) {
		for (int i = 0; i < MyPlayer->_pNumInv; i++)
			note(MyPlayer->InvList[i]);
		// The extra backpack pages too, where crafting stock is kept (round 11 audit, v1.12.236).
		for (int tab = 0; tab < Player::NumExtraInventoryTabs; tab++) {
			for (int i = 0; i < MyPlayer->_pNumInvTab[tab]; i++)
				note(MyPlayer->InvTabList[tab][i]);
		}
		for (const Item &item : MyPlayer->SpdList)
			note(item);
	}
	// The stash is where runes actually live in this fork - gold and loot both go there - so a
	// "what can I build" button that ignored it would answer the wrong question almost always.
	for (const Item &item : Stash.stashList)
		note(item);
	return held;
}

void ApplyPossibleFilter()
{
	const std::vector<uint16_t> held = HeldRunes();
	for (size_t i = 0; i < RuneIndices.size(); i++)
		RuneSelected[i] = std::find(held.begin(), held.end(), RuneIndices[i]) != held.end();
}

void DrawEntry(const Surface &out, const RunewordDefinition &word, Rectangle rect)
{
	DrawString(out, _(word.name), Rectangle { rect.position, { rect.size.width, LineHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 });

	const int host = word.host < SlotFilterCount ? word.host : 0;
	DrawString(out, _(SlotFilterNames[host]),
	    Rectangle { rect.position + Displacement { 0, LineHeight }, { rect.size.width, LineHeight } },
	    { UiFlags::ColorBlue | UiFlags::FontSize12 });

	const int iconStep = RuneIconSize + 2;
	for (int i = 0; i < word.runeCount; i++) {
		DrawRuneIcon(out, word.runes[i],
		    Rectangle { { rect.position.x + i * iconStep, rect.position.y + LineHeight * 2 },
		        { RuneIconSize, RuneIconSize } });
	}

	// Every line, always. The box is sized from the count, so nothing is clipped and nothing needs
	// to be - which is what "we have unlimited scrolling space, use it" buys.
	int y = rect.position.y + EntryHeaderHeight;
	for (const std::string &line : StatLines(word)) {
		DrawString(out, line, Rectangle { { rect.position.x, y }, { rect.size.width, LineHeight } },
		    { UiFlags::ColorWhite | UiFlags::FontSize12 });
		y += LineHeight;
	}
	// Then the runes' own socket effects, in the tier colour the runes wear on their plates, so the
	// two halves read as two halves.
	for (const std::string &line : RuneLines(word)) {
		DrawString(out, line, Rectangle { { rect.position.x, y }, { rect.size.width, LineHeight } },
		    { UiFlags::ColorOrange | UiFlags::FontSize12 });
		y += LineHeight;
	}
}

/** @brief Row layout: each row is as tall as its tallest entry, so no two rows can overlap. */
struct RowLayout {
	int top;
	int height;
	size_t firstIndex;
};

std::vector<RowLayout> LayOutRows(const std::vector<const RunewordDefinition *> &words)
{
	std::vector<RowLayout> rows;
	int y = 0;
	for (size_t i = 0; i < words.size(); i += ColumnCount) {
		int tallest = 0;
		for (size_t c = 0; c < ColumnCount && i + c < words.size(); c++)
			tallest = std::max(tallest, EntryHeight(*words[i + c]));
		rows.push_back(RowLayout { y, tallest, i });
		y += tallest + EntryGap;
	}
	return rows;
}

} // namespace

bool IsRunewordBookOpen()
{
	return BookOpen;
}

Rectangle GetRunewordBookRect()
{
	// Flush with the mini-map's top border (user, 2026-08-20). The book is 944 wide against a 960
	// screen, so it necessarily runs under the mini-map horizontally; lining their top edges up is
	// what stops that reading as an accident.
	// Left where it was. It was moved to the bottom on 2026-08-27 and moved straight back: the
	// bottom-docking rule was about the SIDE PANELS, and this window was never in scope (user:
	// "runeword book was fine as it was. i didnt ask for it to be moved").
	const int top = GetMiniMapScreenRect().position.y;
	return { { (gnScreenWidth - WindowSize.width) / 2, top }, WindowSize };
}

void OpenRunewordBook()
{
	EnsureRuneList();
	// User, 2026-08-20: "when rwbook opens, close all other windows incl log and minimap." The book
	// is 944 wide on a 960 screen, so it is not a window that shares the screen with anything - it
	// IS the screen while it is up.
	//
	// CloseAllWindows is the space-bar master closer, which is the right one: it is documented as
	// the list every new window must be added to, so this cannot fall behind as windows are added.
	// It is called BEFORE BookOpen goes true, because it closes the book too - the mini-map and the
	// corner widgets have no open state and are suppressed in scrollrt instead.
	devilution::CloseAllWindows();
	// And a shop outright (audit, 2026-09-27): CloseAllWindows only steps a shop tab back to its vendor's dialog, which is
	// modal - it drew under this window and took every click meant for it.
	devilution::ForceCloseStore();
	// Not over a bench or the Cube that refused to close (round 16 audit, v1.12.241).
	if (IsWorkshopOpen() || IsLevskiRoarOpen())
		return;
	// The burger row is NOT in CloseAllWindows - space deliberately leaves it up, and the row's own
	// click handler keeps it open so several panels can be toggled in one go. Neither argument
	// survives contact with this window: the row sits just above the HUD plate, the book reaches
	// within a few pixels of it, and diablo.cpp routes IsPointOverHudMenu BEFORE
	// HandleRunewordBookClick - so an overlapping row would draw on top of the book AND eat the
	// clicks in that strip.
	//
	// Closed here rather than in the menu entry, so it holds for the W key too.
	CloseHudMenu();
	BookOpen = true;
	ScrollOffsetPx = 0;
}

void CloseRunewordBook()
{
	BookOpen = false;
}

void ResetRunewordBookForNewGame()
{
	// The filters are kept between opens in a game, not between heroes: the next one's first open showed the last one's
	// rune, slot and count filters, and "Possible" judged against the new hero's runes (round 22 audit, v1.12.247).
	CloseRunewordBook();
	RuneCountSelected.fill(false);
	SlotSelected.fill(false);
	std::fill(RuneSelected.begin(), RuneSelected.end(), false);
	PossibleMode = false;
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
	// The painted wide frame (user, 2026-09-05): dark backing in its core, the bezel over it, the
	// red X at the frame's top-right as on every window.
	DrawBookFrame(out, BookFrame::Wide, window);
	DrawWindowCloseButton(out, window);

	// The title in the game's own font, like the other books (user, 2026-09-05: "remove the runeword
	// book title png from chatgpt and use same font as the rest"); the engraved plate lasted one build.
	DrawString(out, _("Runeword Book"),
	    Rectangle { window.position + Displacement { Padding, Padding }, { window.size.width - Padding * 2, TitleHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize24 | UiFlags::AlignCenter });

	// THE TITLE ROW'S BUTTONS (2026-09-25): the chest - the Possible-Runewords toggle, open while it is what
	// selected the rune row - then the rune counts 2 to 6. All wear the vendors' tab face, gold when lit.
	const Rectangle possible = PossibleFilterRect();
	const bool possibleHovered = possible.contains(MousePosition);
	if (!DrawVendorButtonBacking(out, possible, PossibleMode, possibleHovered))
		DrawOrnateBorder(out, possible);
	DrawTabGlyph(out, possible, /*open=*/PossibleMode, /*gold=*/possibleHovered);
	for (int c = 0; c < RuneCountFilterCount; c++) {
		const Rectangle button = TitleButtonRect(1 + c);
		const bool hovered = button.contains(MousePosition);
		if (!DrawVendorButtonBacking(out, button, RuneCountSelected[c], hovered))
			DrawOrnateBorder(out, button);
		DrawString(out, StrCat(RuneCountFilterFirst + c), button,
		    { (RuneCountSelected[c] || hovered ? UiFlags::ColorWhite : UiFlags::ColorWhitegold) | UiFlags::FontSize12
		        | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}
	// The hover labels, just under the row so they cannot cover the title - and drawn LAST, over everything
	// else in the book (user, 2026-09-26 dev note: "make tooltips on runeword book filter buttons render on top
	// of other content"). They were drawn here, before the slot filters and the rune keys, which then painted
	// over them. Sized to their text now, on a darker plate.
	const auto tipUnder = [&out](Rectangle button, string_view text) {
		const Rectangle tip { { button.position.x, button.position.y + button.size.height + 2 }, { GetLineWidth(text) + 8, LineHeight } };
		DrawHalfTransparentRectTo(out, tip.position.x, tip.position.y, tip.size.width, tip.size.height);
		DrawHalfTransparentRectTo(out, tip.position.x, tip.position.y, tip.size.width, tip.size.height);
		DrawString(out, text, Rectangle { tip.position + Displacement { 4, 0 }, { tip.size.width - 4, tip.size.height } },
		    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::Shadowed });
	};
	const auto drawHoverTips = [&]() {
		if (possibleHovered)
			tipUnder(possible, _("Possible RW"));
		for (int c = 0; c < RuneCountFilterCount; c++) {
			if (TitleButtonRect(1 + c).contains(MousePosition))
				tipUnder(TitleButtonRect(1 + c), fmt::format(fmt::runtime(_("{:d} runes")), RuneCountFilterFirst + c));
		}
	};

	// The plate when it shipped, the ornate border when it did not - the fallback shop_grid.cpp makes.
	const bool keyArt = GetLoosePngSize(RunewordKeyArt).width != 0;
	for (int i = 0; i < SlotFilterCount; i++) {
		const Rectangle key = SlotKeyRect(i);
		const bool hovered = key.contains(MousePosition);
		// The vendors' tab face (user, 2026-09-25 dev note: "replace the filter buttons backing in runeword book
		// with the backing we use for vendor tabs" ... "selected filters to have their backing gold, rest - grey.
		// texts on buttons to have text shadow 2px"). The painted key and the ornate edge stay as fallbacks.
		if (!DrawVendorButtonBacking(out, key, SlotSelected[i], hovered)) {
			if (keyArt) {
				const int state = SlotSelected[i] ? 2 : (hovered ? 1 : 0);
				DrawLoosePngPart(out, RunewordKeyArt, Rectangle { { 0, state * RunewordKeyCell.height }, RunewordKeyCell }, key.position);
			} else {
				DrawOrnateBorder(out, key);
			}
		}
		DrawString(out, _(SlotFilterNames[i]), key,
		    { (hovered || SlotSelected[i] ? UiFlags::ColorWhite : UiFlags::ColorWhitegold) // white under the cursor (user, 2026-09-05)
		        | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}

	for (size_t i = 0; i < RuneIndices.size(); i++) {
		const Rectangle key = RuneKeyRect(i);
		if (RuneSelected[i])
			UnsafeDrawBorder2px(out, key, KeyLitColor);
		DrawRuneIcon(out, RuneIndices[i],
		    Rectangle { { key.position.x, key.position.y + 2 }, { key.size.width, RuneIconSize } },
		    key.contains(MousePosition) ? BrightenTRN() : nullptr); // brighter under the cursor (user, 2026-09-05)
	}

	const std::vector<const RunewordDefinition *> words = VisibleWords();
	const Rectangle content = ContentRect();

	if (words.empty()) {
		DrawString(out, _("No runewords match these filters."), content,
		    { UiFlags::ColorGold | UiFlags::FontSize12 | UiFlags::AlignCenter });
		drawHoverTips();
		return;
	}

	const std::vector<RowLayout> rows = LayOutRows(words);
	const int totalHeight = rows.empty() ? 0 : rows.back().top + rows.back().height;
	ScrollOffsetPx = std::clamp(ScrollOffsetPx, 0, std::max(0, totalHeight - content.size.height));

	// Clipped to the content area, so a row straddling the bottom edge is cut there rather than
	// spilling over the border - the alternative to clipping is drawing partial rows by hand, and
	// that is how entries end up overlapping the frame.
	const Surface view = out.subregion(content.position.x, content.position.y,
	    content.size.width, content.size.height);
	const int columnWidth = content.size.width / ColumnCount;

	for (const RowLayout &row : rows) {
		const int y = row.top - ScrollOffsetPx;
		if (y + row.height < 0 || y > content.size.height)
			continue;
		for (int c = 0; c < ColumnCount; c++) {
			const size_t index = row.firstIndex + c;
			if (index >= words.size())
				break;
			DrawEntry(view, *words[index],
			    Rectangle { { c * columnWidth, y }, { columnWidth - ColumnGap, row.height } });
		}
	}
	drawHoverTips();
}

namespace {

/**
 * @brief The filter button held down (2026-09-27): it toggles on the RELEASE inside it, the press/release rule every
 * button follows (user: "fix the decisions for me too"). It toggled on the press.
 */
enum class FilterKind : int8_t {
	None,
	Possible,
	RuneCount,
	Slot,
	Rune,
};
FilterKind PressedFilter = FilterKind::None;
size_t PressedFilterIndex = 0;

Rectangle PressedFilterRect()
{
	switch (PressedFilter) {
	case FilterKind::Possible:
		return PossibleFilterRect();
	case FilterKind::RuneCount:
		return TitleButtonRect(1 + static_cast<int>(PressedFilterIndex));
	case FilterKind::Slot:
		return SlotKeyRect(static_cast<int>(PressedFilterIndex));
	case FilterKind::Rune:
		return RuneKeyRect(PressedFilterIndex);
	case FilterKind::None:
		break;
	}
	return { { 0, 0 }, { 0, 0 } };
}

bool PressFilter(FilterKind kind, size_t index)
{
	PressedFilter = kind;
	PressedFilterIndex = index;
	PlayUiMoveSound();
	return true;
}

} // namespace

void ReleaseRunewordBookButton()
{
	const FilterKind kind = PressedFilter;
	const size_t i = PressedFilterIndex;
	const Rectangle rect = PressedFilterRect();
	PressedFilter = FilterKind::None;
	if (!BookOpen || kind == FilterKind::None || !rect.contains(MousePosition))
		return;
	switch (kind) {
	case FilterKind::Possible:
		if (PossibleMode) {
			RuneSelected.assign(RuneIndices.size(), false);
			PossibleMode = false;
		} else {
			ApplyPossibleFilter();
			PossibleMode = true;
		}
		break;
	case FilterKind::RuneCount:
		RuneCountSelected[i] = !RuneCountSelected[i];
		break;
	case FilterKind::Slot:
		SlotSelected[i] = !SlotSelected[i];
		break;
	case FilterKind::Rune:
		if (i < RuneSelected.size())
			RuneSelected[i] = !RuneSelected[i];
		PossibleMode = false; // a hand-picked rune asks "requires", whatever the toggle had selected
		break;
	case FilterKind::None:
		break;
	}
	ScrollOffsetPx = 0;
}

bool HandleRunewordBookClick(Point position)
{
	if (!BookOpen)
		return false;
	const Rectangle window = GetRunewordBookRect();
	if (!window.contains(position))
		return false;

	// The X closes on the release inside it, as the filters below toggle (round 75 audit).
	if (CheckWindowCloseButtonClick(window, position, [] { CloseRunewordBook(); }))
		return true;

	// Each filter is pressed here and toggles on the release inside it (ReleaseRunewordBookButton).
	if (PossibleFilterRect().contains(position))
		return PressFilter(FilterKind::Possible, 0);

	for (int c = 0; c < RuneCountFilterCount; c++) {
		if (TitleButtonRect(1 + c).contains(position))
			return PressFilter(FilterKind::RuneCount, static_cast<size_t>(c));
	}

	for (int i = 0; i < SlotFilterCount; i++) {
		if (SlotKeyRect(i).contains(position))
			return PressFilter(FilterKind::Slot, static_cast<size_t>(i));
	}

	for (size_t i = 0; i < RuneIndices.size(); i++) {
		if (RuneKeyRect(i).contains(position))
			return PressFilter(FilterKind::Rune, i);
	}

	return true;
}

bool HandleRunewordBookScroll(int delta)
{
	if (!BookOpen)
		return false;
	if (!GetRunewordBookRect().contains(MousePosition))
		return false;
	// A fixed pixel step rather than one row: rows are variable height now, so "one row" would
	// scroll a different distance depending on where you happened to be.
	ScrollOffsetPx = std::max(0, ScrollOffsetPx - delta * (LineHeight * 3));
	return true;
}

} // namespace devilution::oracool
