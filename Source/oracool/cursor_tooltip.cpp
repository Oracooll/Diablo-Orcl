#include "oracool/cursor_tooltip.h"

#include <algorithm>
#include <cassert>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "cursor.h"
#include "engine/point.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h"
#include "oracool/ornate_border.h" // ThemeEdgeColor
#include "oracool/levski_roar.h" // HoveredLevskiGridItem
#include "oracool/shop_grid.h"
#include "qol/stash.h"
#include "items.h"
#include "player.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "options.h"
#include <cstdlib>
#include <string>
#include <vector>
#include "utils/ui_fwd.h"

namespace devilution::oracool {

namespace {

// Oracool: user request (2026-08-11) - the tooltip lost its dark backing plate and now sits
// centred just above the cursor rather than trailing below-right of it. Without a backdrop the
// text is outlined instead, which is what keeps it legible over bright dungeon floors.
constexpr int GapAboveCursor = 6;

// Oracool: user request (2026-08-12) - items are the exception to the above. An item's block runs
// to a dozen lines of stats, which as bare outlined text over a dungeon floor is unreadable, so
// item hovers get a real panel: padded, darkened, bordered. This replaces BOTH the outlined
// tooltip and the fixed "item stats" box that used to sit beside the inventory - one panel, at the
// cursor, sized to its contents.
constexpr int PanelPaddingX = 10;
constexpr int PanelPaddingY = 7;
/** Extra air between the panel's rows, on top of the font's own line height. */
constexpr int PanelLineGap = 4;
constexpr int PanelBorderWidth = 2;
/** Deep gold, shared with the class silhouette's outline so the two edges cannot drift apart -
 * see oracool::ThemeEdgeColor for why that is one constant rather than two literals. */
constexpr uint8_t PanelBorderColor = ThemeEdgeColor;

Rectangle PrevTooltipRect;

/** @brief Widest line and line count of a possibly multi-line string. */
void MeasureText(string_view text, int &maxWidth, int &lineCount)
{
	maxWidth = 0;
	lineCount = 0;
	size_t start = 0;
	while (true) {
		const size_t newline = text.find('\n', start);
		const string_view line = (newline == string_view::npos)
		    ? text.substr(start)
		    : text.substr(start, newline - start);
		maxWidth = std::max(maxWidth, GetLineWidth(line, GameFont12, 1));
		lineCount++;
		if (newline == string_view::npos)
			break;
		start = newline + 1;
	}
}

/**
 * @brief Whether what is under the cursor right now is an item, and so wants the panel treatment.
 *
 * Derived from the hover globals rather than from a flag the hover code sets, deliberately: every
 * one of these is already cleared and repopulated once per frame by the cursor/hover pass, so
 * there is no way for this to go stale, and no new plumbing threaded through inv.cpp, stash.cpp
 * and control.cpp for something all three already record.
 *
 * `pcursinvitem` covers the inventory grid, the equipment slots and the belt; `ActiveTabItemHovered`
 * covers the extra inventory tabs, whose items have no pcursinvitem encoding (see CheckInvHLight).
 * A held item is not included - it is a single line, and a plate following a dragged item around
 * would be in the way.
 */
bool IsHoveringItem()
{
	return pcursitem != -1
	    || pcursinvitem != -1
	    || pcursstashitem != StashStruct::EmptyCell
	    || ActiveTabItemHovered
	    // The shop grid has no pcurs global of its own - its items are the vendor's, not the
	    // player's, so none of the four above ever describe one. It records its hover instead, on
	    // the same once-per-frame pass that clears and repopulates these.
	    || IsShopItemHovered()
	    // And Levski's grid (user, 2026-09-05: its items' popup drew as bare outlined text).
	    || HoveredLevskiGridItem() != nullptr;
}

} // namespace

Rectangle GetPrevCursorTooltipRect()
{
	return PrevTooltipRect;
}

namespace {

/** @brief One tooltip block as text plus its per-line colours and two-run tail starts. */
struct TooltipBlock {
	std::string text;
	std::vector<UiFlags> colors;
	std::vector<uint16_t> tails;
	std::vector<std::vector<PanelLineRun>> runs;
};

/** @brief Widest line, line count and line height of a block, and the box it needs. */
struct BlockMetrics {
	int maxWidth = 0;
	int lineCount = 0;
	int lineHeight = 0;
	int lineStride = 0;
	Size textSize;
	Size boxSize;
	int padX = 0;
	int padY = 0;
};

BlockMetrics MeasureBlock(string_view text, bool asPanel)
{
	BlockMetrics m;
	MeasureText(text, m.maxWidth, m.lineCount);
	m.lineHeight = GetLineHeight(text, GameFont12);
	// Oracool: user request - an item's stat block reads cramped at the font's own line height, so
	// the panel opens the rows up. Only the panel: a one-line hover has no rows to space out.
	//
	// The gap goes BETWEEN rows, not below each of them - the height is n line boxes plus n-1 gaps.
	// Adding it to every row instead would leave a trailing gap under the last line, making the
	// panel's bottom padding visibly deeper than its top.
	const int lineGap = asPanel ? PanelLineGap : 0;
	m.lineStride = m.lineHeight + lineGap;
	m.textSize = { m.maxWidth, m.lineCount * m.lineHeight + (m.lineCount - 1) * lineGap };
	m.padX = asPanel ? PanelPaddingX + PanelBorderWidth : 0;
	m.padY = asPanel ? PanelPaddingY + PanelBorderWidth : 0;
	m.boxSize = { m.textSize.width + 2 * m.padX, m.textSize.height + 2 * m.padY };
	return m;
}

/**
 * @brief Draws one block into @p box: the darkened plate and gold border when @p asPanel, then the
 * lines - per line in their own colours when a colour list of the right length is given, else the
 * whole text in @p singleColor.
 */
void DrawBlock(const Surface &out, const Rectangle &box, const BlockMetrics &m, string_view text,
    const std::vector<UiFlags> &colors, const std::vector<uint16_t> &tails,
    const std::vector<std::vector<PanelLineRun>> &runs, UiFlags singleColor, bool asPanel)
{
	const bool boxFitsOnScreen = box.size.width <= static_cast<int>(gnScreenWidth) && box.size.height <= static_cast<int>(gnScreenHeight);
	if (asPanel && boxFitsOnScreen) {
		// Twice, for ~75% darkening: one pass leaves the floor tiles reading straight through the
		// stat lines, which is the readability problem this panel exists to solve.
		DrawHalfTransparentRectTo(out, box.position.x, box.position.y, box.size.width, box.size.height);
		DrawHalfTransparentRectTo(out, box.position.x, box.position.y, box.size.width, box.size.height);
		UnsafeDrawBorder2px(out, box, PanelBorderColor);
	}

	const Rectangle textArea { box.position + Displacement { m.padX, m.padY }, m.textSize };
	const UiFlags sharedFlags = UiFlags::AlignCenter | UiFlags::KerningFitSpacing
	    | (asPanel ? UiFlags::None : UiFlags::Outlined);
	// The same flags WITHOUT centring, for the lines drawn in two runs. Those are positioned by hand
	// below, and DrawString centring each run inside its own rectangle is precisely what broke them.
	const UiFlags runFlags = UiFlags::KerningFitSpacing
	    | (asPanel ? UiFlags::None : UiFlags::Outlined);

	// Oracool: an item's block is several KINDS of information - its name, what it is, what was
	// rolled onto it, what it demands of you - and each line carries its own colour (see
	// control.h's InfoStringLineColors). Drawing per line is the only way to honour that, since
	// DrawString takes one colour for the whole string.
	//
	// An empty colour list is normal: hovers that are a single line - a monster, an NPC, a shrine -
	// assign InfoString directly and keep the single-colour path exactly as before.
	//
	// A list that is non-empty but the WRONG size is not normal, it is the signature of a bug:
	// somebody appended coloured lines onto text that was assigned without registering its own
	// colour. That is precisely how the item panel shipped in one colour - the inventory, stash and
	// held-item hovers each set the name with a bare assignment, leaving the list one short of the
	// block PrintItemDetails then built, and this check quietly refused all of it. Silence is what
	// made it hard to see, so it asserts now.
	const bool perLineColors = colors.size() == static_cast<size_t>(m.lineCount);
	assert((colors.empty() || perLineColors)
	    && "InfoString gained lines whose colours were never recorded - use SetPanelString/AddPanelString");
	if (!perLineColors) {
		// lineStride, not lineHeight: DrawString's lineHeight option IS the row-to-row step, so
		// this is where the gap gets applied on the single-colour path.
		DrawString(out, text, textArea, { singleColor | sharedFlags, 1, m.lineStride });
		return;
	}
	size_t start = 0;
	for (int i = 0; i < m.lineCount; i++) {
		const size_t newline = text.find('\n', start);
		const string_view line = (newline == string_view::npos)
		    ? text.substr(start)
		    : text.substr(start, newline - start);
		// Stepped by the stride, but each row's box is one line tall - the gap is the space
		// between boxes, not part of them.
		const Rectangle lineArea { textArea.position + Displacement { 0, i * m.lineStride },
			{ textArea.size.width, m.lineHeight } };
		// A line may be drawn in TWO runs: the head in its own colour and a white tail from
		// InfoStringLineTailStart. The set panel's item list needs it - each piece's name is
		// green or red while its slot "(helm)" is white (user request, 2026-08-16) - and one
		// colour per line cannot say that. Zero, the value every other producer records, takes
		// the single-run path below unchanged.
		const size_t tailStart = i < static_cast<int>(tails.size()) ? tails[i] : 0;
		// Or in SEVERAL runs (control.h's InfoStringLineRuns): the requirement line paints each
		// unmet stat red (user, 2026-09-06). Centred as one line, then laid out left to right, the
		// same way as the two-run case below.
		const bool hasRuns = i < static_cast<int>(runs.size()) && !runs[i].empty();
		if (hasRuns) {
			int x = lineArea.position.x + std::max(0, (lineArea.size.width - GetLineWidth(line)) / 2);
			size_t segStart = 0;
			UiFlags segColor = colors[i];
			auto drawSegment = [&](size_t end) {
				if (end <= segStart)
					return;
				const string_view seg = line.substr(segStart, end - segStart);
				const int w = GetLineWidth(seg);
				DrawString(out, seg, Rectangle { { x, lineArea.position.y }, { w, m.lineHeight } }, { segColor | runFlags, 1, m.lineHeight });
				x += w;
			};
			for (const PanelLineRun &run : runs[i]) {
				const size_t start = std::min<size_t>(run.start, line.size());
				drawSegment(start);
				segStart = start;
				segColor = run.color;
			}
			drawSegment(line.size());
		} else if (tailStart > 0 && tailStart < line.size()) {
			const string_view head = line.substr(0, tailStart);
			const string_view tail = line.substr(tailStart);
			// Bug (fixed 2026-08-17, user: "white letters overlap the green one, when there is
			// obviously enough space to avoid it"). Both runs were drawn with the shared
			// AlignCenter flag: the head centred inside the FULL line box, the tail centred
			// inside whatever was left of it to the right. Two independent centrings, so the
			// white slot label landed on top of the green item name every time.
			//
			// The pair is centred as ONE line now, then laid out left to right from that origin.
			const int left = lineArea.position.x
			    + std::max(0, (lineArea.size.width - GetLineWidth(line)) / 2);
			const int headWidth = GetLineWidth(head);
			const Rectangle headArea { { left, lineArea.position.y }, { headWidth, m.lineHeight } };
			const Rectangle tailArea { { left + headWidth, lineArea.position.y },
				{ GetLineWidth(tail), m.lineHeight } };
			DrawString(out, head, headArea, { colors[i] | runFlags, 1, m.lineHeight });
			DrawString(out, tail, tailArea, { UiFlags::ColorWhite | runFlags, 1, m.lineHeight });
		} else {
			DrawString(out, line, lineArea, { colors[i] | sharedFlags, 1, m.lineHeight });
		}
		if (newline == string_view::npos)
			break;
		start = newline + 1;
	}
}

/**
 * @brief The item under the cursor in a CONTAINER - the backpack grid (any tab) or the stash - or
 * nullptr. Equipped slots and the belt are deliberately not containers here: comparing a worn
 * helm to itself says nothing, and a potion has no slot to compare against.
 */
const Item *HoveredContainerItem()
{
	// A SHOP's wares too (user, 2026-09-05: "add comparison tooltip for shop items too"). On the
	// Repair and Recharge tabs the "ware" is the player's own worn piece; the caller skips the
	// counterpart that IS the hovered item, so a helm is never compared with itself.
	if (const Item *ware = HoveredShopItem(); ware != nullptr)
		return ware;
	if (const Item *cube = HoveredLevskiGridItem(); cube != nullptr)
		return cube; // Levski's grid is a container like the stash: its items compare with what is worn
	if (pcursstashitem != StashStruct::EmptyCell)
		return &Stash.stashList[pcursstashitem];
	Player &player = *InspectPlayer;
	if (ActiveTabItemHovered && pcursinvtabitem >= 0)
		return &GetActiveInvListItem(player, pcursinvtabitem);
	if (pcursinvitem >= INVITEM_INV_FIRST && pcursinvitem <= INVITEM_INV_LAST)
		return &player.InvList[pcursinvitem - INVITEM_INV_FIRST];
	return nullptr;
}

/**
 * @brief The worn slots @p item would take, whose occupants are worth comparing it with.
 *
 * Both ring fingers for a ring; for a one-hander the weapon hand, or the shield hand for a shield;
 * for a two-hander both hands, since equipping it displaces both. The six Oracool slots map one to
 * one. Only slots that actually hold something come back.
 */
std::vector<inv_body_loc> EquippedCounterparts(const Player &player, const Item &item)
{
	std::vector<inv_body_loc> slots;
	const auto add = [&](inv_body_loc loc) {
		if (!player.InvBody[loc].isEmpty())
			slots.push_back(loc);
	};
	switch (player.GetItemLocation(item)) {
	case ILOC_HELM:
		add(INVLOC_HEAD);
		break;
	case ILOC_ARMOR:
		add(INVLOC_CHEST);
		break;
	case ILOC_AMULET:
		add(INVLOC_AMULET);
		break;
	case ILOC_RING:
		add(INVLOC_RING_LEFT);
		add(INVLOC_RING_RIGHT);
		break;
	case ILOC_ONEHAND:
		if (item._itype == ItemType::Shield) {
			// A shield goes in the off hand - unless a two-hander fills both, which it displaces.
			const Item &left = player.InvBody[INVLOC_HAND_LEFT];
			if (!left.isEmpty() && player.GetItemLocation(left) == ILOC_TWOHAND)
				add(INVLOC_HAND_LEFT);
			else
				add(INVLOC_HAND_RIGHT);
		} else {
			add(INVLOC_HAND_LEFT);
		}
		break;
	case ILOC_TWOHAND:
		add(INVLOC_HAND_LEFT);
		add(INVLOC_HAND_RIGHT);
		break;
	case ILOC_SHOULDERS:
		add(INVLOC_SHOULDERS);
		break;
	case ILOC_BRACERS:
		add(INVLOC_BRACERS);
		break;
	case ILOC_GLOVES:
		add(INVLOC_GLOVES);
		break;
	case ILOC_WAIST:
		add(INVLOC_WAIST);
		break;
	case ILOC_LEGS:
		add(INVLOC_LEGS);
		break;
	case ILOC_BOOTS:
		add(INVLOC_BOOTS);
		break;
	default:
		break;
	}
	return slots;
}

/**
 * @brief @p item's tooltip block, exactly as hovering it would print it - the same three lines the
 * stash and the backpack use (name in its tier colour, then PrintItemDetails or PrintItemDur) -
 * captured out of the panel-string store and the store put back as it was.
 *
 * Through the store rather than a second describer, so the comparison can never disagree with the
 * hover it sits beside: there is one item printer, and this borrows it.
 */
TooltipBlock CaptureItemBlock(const Item &item)
{
	const std::string savedText { InfoString.str() };
	const std::vector<UiFlags> savedColors = InfoStringLineColors;
	const std::vector<uint16_t> savedTails = InfoStringLineTailStart;
	const std::vector<std::vector<PanelLineRun>> savedRuns = InfoStringLineRuns;

	ClearPanelStrings();
	SetPanelString(item.getName(), item.getTextColor());
	if (item._iIdentified)
		PrintItemDetails(item);
	else
		PrintItemDur(item);

	TooltipBlock block { std::string(InfoString.str()), InfoStringLineColors, InfoStringLineTailStart, InfoStringLineRuns };

	InfoString.AssignKeepingLineColors(std::string(savedText));
	InfoStringLineColors = savedColors;
	InfoStringLineTailStart = savedTails;
	InfoStringLineRuns = savedRuns;
	return block;
}

// =================================================================================================
// THE CARD (user, 2026-09-26, after a Diablo IV tooltip: "build the backings and the tooltips but keep
// them easily rollback-able"). The item printer is untouched: the card lays out the SAME lines the panel
// above draws, sorted by what each one is - the name large, the type and tier under it, the armour or
// damage as one big number, the stats left-aligned, the requirements in a band at the foot, the item's
// own picture top right. The Item Tooltip Card option switches back to the panel.
// =================================================================================================

/** @brief One printed line and its colours, as the panel store holds it. */
struct CardLine {
	std::string text;
	UiFlags color = UiFlags::ColorWhite;
	uint16_t tail = 0;
	std::vector<PanelLineRun> runs;
};

/** @brief A block sorted into the card's places. */
struct Card {
	std::string banner; // "EQUIPPED ITEM" on a comparison card
	CardLine title;
	std::string subtitle; // the parts below, joined - for measuring
	/** @brief The subtitle's parts in their colours: the type in grey, the TIER in the colour its own line had
	 * (user, 2026-09-26 dev note: "the item tier - norm, nightmare, hell, torment to keep the colors we have
	 * apointed to them prior to the redesign"), the item level in grey. */
	std::vector<std::pair<std::string, UiFlags>> subtitleParts;
	/** @brief The item's quality colour - the plate, the frame, the rule and the bullets wear it. */
	uint32_t hue = 0xB08A48;
	std::string headValue; // "81" / "3-9"
	std::string headLabel; // "ARMOR" / "DAMAGE"
	std::string headNote;  // "Dur: 119/119"
	std::vector<CardLine> body;
	std::vector<bool> bullet; // a socketed stone's line, per body line
	std::vector<CardLine> footer;
	const Item *item = nullptr;
};

constexpr int CardPadX = 12;
constexpr int CardPadTop = 10;
constexpr int CardPadBottom = 8;
constexpr int CardLineGap = 4;
constexpr int CardSpriteGap = 10;
constexpr int CardMinInnerWidth = 180;
/** @brief The widest a name may be and still be drawn at 24: past it, the name drops to 12 rather than the card growing. */
constexpr int CardMaxTitleWidth24 = 250;
constexpr int CardBulletIndent = 10;

bool CardStartsWith(string_view text, string_view prefix)
{
	return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

/** @brief "rare armor" -> "Rare Armor". */
std::string CardTitleCase(string_view text)
{
	std::string out(text);
	bool start = true;
	for (char &c : out) {
		if (start && c >= 'a' && c <= 'z')
			c = static_cast<char>(c - 'a' + 'A');
		start = c == ' ';
	}
	return out;
}

std::string CardUpper(string_view text)
{
	std::string out(text);
	for (char &c : out) {
		if (c >= 'a' && c <= 'z')
			c = static_cast<char>(c - 'a' + 'A');
	}
	return out;
}

Card BuildCard(const TooltipBlock &block, const Item *item)
{
	std::vector<CardLine> lines;
	size_t start = 0;
	for (size_t i = 0;; i++) {
		const size_t newline = block.text.find('\n', start);
		CardLine line;
		line.text = block.text.substr(start, newline == std::string::npos ? std::string::npos : newline - start);
		if (i < block.colors.size())
			line.color = block.colors[i];
		if (i < block.tails.size())
			line.tail = block.tails[i];
		if (i < block.runs.size())
			line.runs = block.runs[i];
		lines.push_back(std::move(line));
		if (newline == std::string::npos)
			break;
		start = newline + 1;
	}

	Card card;
	card.item = item;
	size_t i = 0;
	if (i < lines.size() && lines[i].text == _("EQUIPPED ITEM")) {
		card.banner = lines[i].text;
		i++;
	}
	if (i >= lines.size())
		return card;
	card.title = lines[i++];

	std::string type;
	std::string tier;
	UiFlags tierColor = UiFlags::ColorGray5;
	std::string level;
	int socketLinesLeft = 0;
	for (size_t first = i; i < lines.size(); i++) {
		const CardLine &line = lines[i];
		const string_view text = line.text;
		// The line under the name in the name's own colour is what the item IS ("rare armor").
		if (i == first && line.color == card.title.color && line.runs.empty() && !CardStartsWith(text, "Required")) {
			type = line.text;
			continue;
		}
		if (tier.empty() && CardStartsWith(text, "Tier: ")) {
			tier = std::string(text.substr(6));
			tierColor = line.color;
			continue;
		}
		if (level.empty() && CardStartsWith(text, "Item Level: ")) {
			level = std::string(text.substr(12));
			continue;
		}
		if (card.headValue.empty() && (CardStartsWith(text, "armor: ") || CardStartsWith(text, "damage: "))) {
			const bool armor = CardStartsWith(text, "armor: ");
			const string_view rest = text.substr(armor ? 7 : 8);
			const size_t split = rest.find("  ");
			card.headValue = std::string(rest.substr(0, split));
			card.headLabel = armor ? CardUpper(_("armor")) : CardUpper(_("damage"));
			if (split != string_view::npos)
				card.headNote = std::string(rest.substr(split + 2));
			continue;
		}
		if (CardStartsWith(text, "Required")) {
			card.footer.push_back(line);
			continue;
		}
		// The stones in the sockets: as many lines as "Sockets: 2/4" says are filled.
		bool isStone = false;
		if (CardStartsWith(text, "Sockets: ")) {
			socketLinesLeft = std::max(0, std::atoi(std::string(text.substr(9)).c_str()));
		} else if (socketLinesLeft > 0 && !CardStartsWith(text, " ")) {
			isStone = true;
			socketLinesLeft--;
		}
		card.body.push_back(line);
		card.bullet.push_back(isStone);
	}

	const auto append = [&card](const std::string &part, UiFlags color) {
		if (part.empty())
			return;
		if (!card.subtitleParts.empty())
			card.subtitleParts.emplace_back("  \xE2\x80\xA2  ", UiFlags::ColorGray5); // a bullet, U+2022
		card.subtitleParts.emplace_back(part, color);
	};
	append(CardTitleCase(type), UiFlags::ColorGray5);
	if (!tier.empty())
		append(StrCat(_("Tier"), " ", tier), tierColor);
	if (!level.empty())
		append(StrCat(_("ilvl"), " ", level), UiFlags::ColorGray5);
	for (const auto &[part, color] : card.subtitleParts)
		card.subtitle += part;
	if (item != nullptr && !item->isEmpty()) {
		if (const uint32_t hue = ItemQualityRimColor(*item); hue != 0)
			card.hue = hue;
	}
	return card;
}

/** @brief Draws @p line from @p x, left to right, in its colours - the head/tail and run cases as DrawBlock does. */
void DrawCardLine(const Surface &out, const CardLine &line, int x, int y, int lineHeight)
{
	const UiFlags flags = UiFlags::KerningFitSpacing;
	const string_view text = line.text;
	const auto run = [&](string_view seg, UiFlags color) {
		const int w = GetLineWidth(seg);
		DrawString(out, seg, Rectangle { { x, y }, { w + 2, lineHeight } }, { color | flags, 1, lineHeight });
		x += w;
	};
	if (!line.runs.empty()) {
		size_t segStart = 0;
		UiFlags segColor = line.color;
		for (const PanelLineRun &r : line.runs) {
			const size_t s = std::min<size_t>(r.start, text.size());
			if (s > segStart)
				run(text.substr(segStart, s - segStart), segColor);
			segStart = s;
			segColor = r.color;
		}
		if (text.size() > segStart)
			run(text.substr(segStart), segColor);
	} else if (line.tail > 0 && line.tail < text.size()) {
		run(text.substr(0, line.tail), line.color);
		run(text.substr(line.tail), UiFlags::ColorWhite);
	} else {
		run(text, line.color);
	}
}

/** @brief Every size the card is laid out from, measured once. */
struct CardLayout {
	GameFontTables titleFont = GameFont12;
	int line = 0; // font 12's line height
	int stride = 0;
	int titleHeight = 0;
	int bigHeight = 0;
	Size sprite;
	int headerHeight = 0;
	int bodyTop = 0;
	int footerTop = 0;
	Size size;
};

ClxSprite CardSprite(const Item &item)
{
	return GetInvItemSprite(item._iCurs + CURSOR_FIRSTITEM);
}

CardLayout MeasureCard(const Card &card)
{
	CardLayout l;
	l.line = GetLineHeight("A", GameFont12);
	l.stride = l.line + CardLineGap;
	l.bigHeight = GetLineHeight("0", GameFont24);
	const int title24 = GetLineWidth(card.title.text, GameFont24, 1);
	l.titleFont = title24 <= CardMaxTitleWidth24 ? GameFont24 : GameFont12;
	l.titleHeight = l.titleFont == GameFont24 ? l.bigHeight : l.line;
	if (card.item != nullptr && !card.item->isEmpty()) {
		const ClxSprite sprite = CardSprite(*card.item);
		l.sprite = { static_cast<int>(sprite.width()), static_cast<int>(sprite.height()) };
	}

	int headerText = l.titleFont == GameFont24 ? title24 : GetLineWidth(card.title.text);
	headerText = std::max(headerText, GetLineWidth(card.subtitle));
	if (!card.headValue.empty()) {
		headerText = std::max(headerText, GetLineWidth(card.headValue, GameFont24, 1) + 6 + GetLineWidth(card.headLabel)
		        + (card.headNote.empty() ? 0 : 12 + GetLineWidth(card.headNote)));
	}
	int inner = std::max(CardMinInnerWidth, headerText + (l.sprite.width > 0 ? l.sprite.width + CardSpriteGap : 0));
	for (size_t i = 0; i < card.body.size(); i++)
		inner = std::max(inner, GetLineWidth(card.body[i].text) + (card.bullet[i] ? CardBulletIndent : 0));
	for (const CardLine &line : card.footer)
		inner = std::max(inner, GetLineWidth(line.text));
	inner = std::min(inner, static_cast<int>(gnScreenWidth) - 2 * CardPadX);

	int header = l.titleHeight;
	if (!card.subtitle.empty())
		header += 2 + l.line;
	if (!card.headValue.empty())
		header += 4 + l.bigHeight;
	l.headerHeight = std::max(header, l.sprite.height);

	int y = CardPadTop + (card.banner.empty() ? 0 : l.stride);
	y += l.headerHeight + 12; // the divider sits in these twelve
	l.bodyTop = y;
	y += card.body.empty() ? 0 : static_cast<int>(card.body.size()) * l.stride - CardLineGap;
	if (!card.footer.empty()) {
		y += 10;
		l.footerTop = y;
		y += 6 + static_cast<int>(card.footer.size()) * l.stride - CardLineGap + 6;
	} else {
		y += CardPadBottom;
	}
	l.size = { inner + 2 * CardPadX, y };
	return l;
}

uint32_t CardMixRgb(uint32_t a, uint32_t b, int t256)
{
	const auto ch = [&](int shift) {
		const int ca = static_cast<int>((a >> shift) & 0xFF);
		const int cb = static_cast<int>((b >> shift) & 0xFF);
		return static_cast<uint32_t>(std::clamp(ca + (cb - ca) * t256 / 256, 0, 255)) << shift;
	};
	return ch(16) | ch(8) | ch(0);
}

/** @brief The card's plate: the world behind it darkened to a fifth under a warm gradient, a darker foot band, a gold frame. */
void DrawCardPlate(const Surface &out, const Rectangle &box, int footerTop, uint32_t hue)
{
	// In the ITEM'S colour (user, 2026-09-26 dev note: "make the tooltip backing and frame match the items
	// color, not use the same frame and color for all items"): the gradient leans a seventh of the way to the
	// hue at the top and a sixteenth at the foot, dark enough for every text colour to read on it.
	const uint32_t groundTop = CardMixRgb(0x181512u, hue, 36);
	const uint32_t groundBottom = CardMixRgb(0x0C0B0Au, hue, 16);
	const int x0 = std::max(box.position.x, 0), y0 = std::max(box.position.y, 0);
	const int x1 = std::min(box.position.x + box.size.width, out.w());
	const int y1 = std::min(box.position.y + box.size.height, out.h());
	for (int y = y0; y < y1; y++) {
		const int row = y - box.position.y;
		const bool foot = footerTop > 0 && row >= footerTop;
		const int t = box.size.height > 1 ? row * 256 / (box.size.height - 1) : 0;
		const uint32_t ground = foot ? CardMixRgb(0x080706u, hue, 10) : CardMixRgb(groundTop, groundBottom, t);
		uint32_t *dst = out.at<uint32_t>(x0, y);
		for (int x = x0; x < x1; x++, dst++) {
			const uint32_t c = *dst;
			const auto part = [&](int shift) {
				const int keep = static_cast<int>((c >> shift) & 0xFF) * (foot ? 26 : 46) / 256;
				return static_cast<uint32_t>(std::min(255, keep + static_cast<int>((ground >> shift) & 0xFF))) << shift;
			};
			*dst = (c & 0xFF000000u) | part(16) | part(8) | part(0);
		}
	}
	const auto hline = [&](int y, int from, int to, uint32_t rgb) {
		if (y < 0 || y >= out.h())
			return;
		for (int x = std::max(from, 0); x < std::min(to, out.w()); x++)
			*out.at<uint32_t>(x, y) = rgb;
	};
	const auto vline = [&](int x, int from, int to, uint32_t rgb) {
		if (x < 0 || x >= out.w())
			return;
		for (int y = std::max(from, 0); y < std::min(to, out.h()); y++)
			*out.at<uint32_t>(x, y) = rgb;
	};
	const int left = box.position.x, top = box.position.y;
	const int right = left + box.size.width - 1, bottom = top + box.size.height - 1;
	if (footerTop > 0)
		hline(top + footerTop, left + 2, right - 1, CardMixRgb(0x1A1612u, hue, 90));
	// Outside in: a black edge, the gold line (brighter along the top, as if lit from above), a dark inner line.
	hline(top, left, right + 1, 0x050403u);
	hline(bottom, left, right + 1, 0x050403u);
	vline(left, top, bottom + 1, 0x050403u);
	vline(right, top, bottom + 1, 0x050403u);
	const uint32_t lit = CardMixRgb(hue, 0xFFFFFFu, 40);
	const uint32_t side = CardMixRgb(0x000000u, hue, 205);
	const uint32_t shade = CardMixRgb(0x000000u, hue, 140);
	const uint32_t inner = CardMixRgb(0x0E0C0Au, hue, 56);
	hline(top + 1, left + 1, right, lit);
	hline(bottom - 1, left + 1, right, shade);
	vline(left + 1, top + 1, bottom, side);
	vline(right - 1, top + 1, bottom, side);
	hline(top + 2, left + 2, right - 1, inner);
	vline(left + 2, top + 2, bottom - 1, inner);
	vline(right - 2, top + 2, bottom - 1, inner);
}

/** @brief A gold rule that fades out towards both ends. */
void DrawCardDivider(const Surface &out, int x, int y, int width, uint32_t hue)
{
	if (y < 0 || y >= out.h())
		return;
	constexpr int Fade = 48;
	for (int i = 0; i < width; i++) {
		const int px = x + i;
		if (px < 0 || px >= out.w())
			continue;
		const int edge = std::min(i, width - 1 - i);
		const int t = std::min(256, edge * 256 / Fade) * 200 / 256;
		uint32_t *dst = out.at<uint32_t>(px, y);
		*dst = (*dst & 0xFF000000u) | CardMixRgb(*dst, hue, t);
	}
}

/** @brief A small diamond before a socketed stone's line, in the line's own colour. */
void DrawCardBullet(const Surface &out, Point centre, uint32_t rgb)
{
	for (int dy = -2; dy <= 2; dy++) {
		const int half = 2 - std::abs(dy);
		for (int dx = -half; dx <= half; dx++) {
			const int x = centre.x + dx, y = centre.y + dy;
			if (x >= 0 && y >= 0 && x < out.w() && y < out.h())
				*out.at<uint32_t>(x, y) = rgb;
		}
	}
}

/**
 * @brief DrawString, then every pixel it painted made @p liftPercent brighter - hue kept, clamped at full. For the
 * name (user, 2026-09-26 dev note: "make item name fonts more readable. may increase their brightnes a noth.
 * especially the yellow rare titles"): the fonts' colour ramps are shaded for the old black panel and read dim
 * on the card's warmer plate. Drawn and then lifted, so the name keeps its own colour, not a new one.
 */
void DrawStringLifted(const Surface &out, string_view text, const Rectangle &rect, const TextRenderOptions &options, int liftPercent)
{
	const int x0 = std::max(rect.position.x, 0), y0 = std::max(rect.position.y, 0);
	const int x1 = std::min(rect.position.x + rect.size.width, out.w());
	const int y1 = std::min(rect.position.y + rect.size.height, out.h());
	if (x1 <= x0 || y1 <= y0) {
		DrawString(out, text, rect, options);
		return;
	}
	std::vector<uint32_t> before;
	before.reserve(static_cast<size_t>((x1 - x0) * (y1 - y0)));
	for (int y = y0; y < y1; y++)
		before.insert(before.end(), out.at<uint32_t>(x0, y), out.at<uint32_t>(x0, y) + (x1 - x0));
	DrawString(out, text, rect, options);
	size_t i = 0;
	for (int y = y0; y < y1; y++) {
		uint32_t *dst = out.at<uint32_t>(x0, y);
		for (int x = x0; x < x1; x++, dst++, i++) {
			if (*dst == before[i])
				continue;
			const auto ch = [&](int shift) {
				const int v = static_cast<int>((*dst >> shift) & 0xFF);
				return static_cast<uint32_t>(std::min(255, v * (100 + liftPercent) / 100)) << shift;
			};
			*dst = (*dst & 0xFF000000u) | ch(16) | ch(8) | ch(0);
		}
	}
}

void DrawCard(const Surface &out, const Card &card, const CardLayout &l, Point origin)
{
	const Rectangle box { origin, l.size };
	DrawCardPlate(out, box, l.footerTop, card.hue);
	const int left = origin.x + CardPadX;
	const int innerWidth = l.size.width - 2 * CardPadX;
	int y = origin.y + CardPadTop;
	if (!card.banner.empty()) {
		DrawString(out, card.banner, Rectangle { { left, y }, { innerWidth, l.line } },
		    { UiFlags::ColorOracoolGreen | UiFlags::KerningFitSpacing, 1, l.line });
		y += l.stride;
	}
	const int headerTop = y;
	if (l.sprite.width > 0 && card.item != nullptr) {
		// Bottom-left, as every sprite is placed; unusable items keep the red cast DrawItem gives them.
		const Point spriteAt { origin.x + l.size.width - CardPadX - l.sprite.width, headerTop + l.sprite.height };
		DrawItem(*card.item, out, spriteAt, CardSprite(*card.item));
	}
	const UiFlags titleSize = l.titleFont == GameFont24 ? UiFlags::FontSize24 : UiFlags::None;
	// A notch brighter for every name, two for the rare's yellow, which is the darkest of the ramps.
	const int lift = card.title.color == UiFlags::ColorYellow3 ? 55 : 30;
	DrawStringLifted(out, card.title.text, Rectangle { { left, y }, { GetLineWidth(card.title.text, l.titleFont, 1) + 4, l.titleHeight } },
	    { card.title.color | titleSize | UiFlags::KerningFitSpacing, 1, l.titleHeight }, lift);
	y += l.titleHeight;
	if (!card.subtitle.empty()) {
		y += 2;
		int x = left;
		for (const auto &[part, color] : card.subtitleParts) {
			const int w = GetLineWidth(part);
			DrawString(out, part, Rectangle { { x, y }, { w + 2, l.line } }, { color | UiFlags::KerningFitSpacing, 1, l.line });
			x += w;
		}
		y += l.line;
	}
	if (!card.headValue.empty()) {
		y += 4;
		const int valueWidth = GetLineWidth(card.headValue, GameFont24, 1);
		DrawString(out, card.headValue, Rectangle { { left, y }, { valueWidth + 4, l.bigHeight } },
		    { UiFlags::ColorWhite | UiFlags::FontSize24 | UiFlags::KerningFitSpacing, 1, l.bigHeight });
		// The label on the number's baseline: font 12 set at the foot of the 24's line.
		const int labelY = y + l.bigHeight - l.line - 2;
		int x = left + valueWidth + 6;
		DrawString(out, card.headLabel, Rectangle { { x, labelY }, { GetLineWidth(card.headLabel) + 4, l.line } },
		    { UiFlags::ColorGray5 | UiFlags::KerningFitSpacing, 1, l.line });
		if (!card.headNote.empty()) {
			x += GetLineWidth(card.headLabel) + 12;
			DrawString(out, card.headNote, Rectangle { { x, labelY }, { GetLineWidth(card.headNote) + 4, l.line } },
			    { UiFlags::ColorGray5 | UiFlags::KerningFitSpacing, 1, l.line });
		}
	}
	DrawCardDivider(out, left, headerTop + l.headerHeight + 6, innerWidth, card.hue);

	y = origin.y + l.bodyTop;
	for (size_t i = 0; i < card.body.size(); i++) {
		const CardLine &line = card.body[i];
		int x = left;
		if (card.bullet[i]) {
			DrawCardBullet(out, { x + 3, y + l.line / 2 }, card.hue);
			x += CardBulletIndent;
		}
		DrawCardLine(out, line, x, y, l.line);
		y += l.stride;
	}
	if (l.footerTop > 0) {
		y = origin.y + l.footerTop + 6;
		for (const CardLine &line : card.footer) {
			const int w = GetLineWidth(line.text);
			DrawCardLine(out, line, origin.x + (l.size.width - w) / 2, y, l.line);
			y += l.stride;
		}
	}
}

/** @brief The item the panel is describing: a container's, a worn or belt one, or one on the ground. */
const Item *HoveredCardItem()
{
	if (const Item *item = HoveredContainerItem(); item != nullptr)
		return item;
	if (InspectPlayer != nullptr && pcursinvitem >= 0 && pcursinvitem < INVITEM_INV_FIRST)
		return &InspectPlayer->InvBody[pcursinvitem];
	if (InspectPlayer != nullptr && pcursinvitem >= INVITEM_BELT_FIRST && pcursinvitem <= INVITEM_BELT_LAST)
		return &InspectPlayer->SpdList[pcursinvitem - INVITEM_BELT_FIRST];
	if (pcursitem >= 0)
		return &Items[pcursitem];
	return nullptr;
}

void GrowTooltipRect(const Rectangle &box)
{
	if (PrevTooltipRect.size.width == 0) {
		PrevTooltipRect = box;
		return;
	}
	const int right = std::max(PrevTooltipRect.position.x + PrevTooltipRect.size.width, box.position.x + box.size.width);
	const int bottom = std::max(PrevTooltipRect.position.y + PrevTooltipRect.size.height, box.position.y + box.size.height);
	PrevTooltipRect.position.x = std::min(PrevTooltipRect.position.x, box.position.x);
	PrevTooltipRect.position.y = std::min(PrevTooltipRect.position.y, box.position.y);
	PrevTooltipRect.size = { right - PrevTooltipRect.position.x, bottom - PrevTooltipRect.position.y };
}

int OverlapArea(const Rectangle &a, const Rectangle &b)
{
	const int w = std::min(a.position.x + a.size.width, b.position.x + b.size.width) - std::max(a.position.x, b.position.x);
	const int h = std::min(a.position.y + a.size.height, b.position.y + b.size.height) - std::max(a.position.y, b.position.y);
	return w > 0 && h > 0 ? w * h : 0;
}

/**
 * @brief Where an EQUIPPED ITEM panel of @p size goes, given the hovered item's panel @p hovered and the comparison
 * panels already @p placed.
 *
 * Tried in order: beside the hovered panel on the right, then on the left, then stacked under each panel already
 * placed, then over it - and the first that overlaps nothing wins, else the one that overlaps least (user,
 * 2026-09-26 dev note: "sometime comparing an item to two equipped items results in the two equipped items tooltips
 * overlapping. if that happens try to stack the tooltips one on top of the other to avoid overlapping. i[f]
 * tooltips overlap in both scenarios pick the one with less overlapping"). The old rule went right, then left, and
 * with no room on the left it pinned the second ring's panel to the screen edge, over the others.
 */
Point PlaceComparison(const Rectangle &hovered, const std::vector<Rectangle> &placed, Size size)
{
	constexpr int Gap = 6;
	std::vector<Point> candidates {
		{ hovered.position.x + hovered.size.width + Gap, hovered.position.y },
		{ hovered.position.x - Gap - size.width, hovered.position.y },
	};
	for (const Rectangle &r : placed) {
		candidates.push_back({ r.position.x, r.position.y + r.size.height + Gap });
		candidates.push_back({ r.position.x, r.position.y - Gap - size.height });
	}
	const int maxX = std::max(0, static_cast<int>(gnScreenWidth) - size.width);
	const int maxY = std::max(0, static_cast<int>(gnScreenHeight) - size.height);
	Point best {};
	int bestOverlap = -1;
	for (Point c : candidates) {
		c.x = std::clamp(c.x, 0, maxX);
		c.y = std::clamp(c.y, 0, maxY);
		const Rectangle rect { c, size };
		int overlap = OverlapArea(rect, hovered);
		for (const Rectangle &r : placed)
			overlap += OverlapArea(rect, r);
		if (bestOverlap < 0 || overlap < bestOverlap) {
			best = c;
			bestOverlap = overlap;
			if (overlap == 0)
				break;
		}
	}
	return best;
}

/** @brief The hovered item as a card, with its worn counterparts beside it as the panel path does. */
void DrawCardTooltip(const Surface &out)
{
	const TooltipBlock hoveredBlock { std::string(InfoString.str()), InfoStringLineColors, InfoStringLineTailStart, InfoStringLineRuns };
	const Item *hovered = HoveredCardItem();
	const Card card = BuildCard(hoveredBlock, hovered);
	const CardLayout l = MeasureCard(card);

	const int maxX = std::max(0, static_cast<int>(gnScreenWidth) - l.size.width);
	const int maxY = std::max(0, static_cast<int>(gnScreenHeight) - l.size.height);
	Point origin { MousePosition.x - l.size.width / 2, MousePosition.y - l.size.height - GapAboveCursor };
	origin.x = std::clamp(origin.x, 0, maxX);
	if (origin.y < 0)
		origin.y = std::min(MousePosition.y + GapAboveCursor, maxY);
	origin.y = std::clamp(origin.y, 0, maxY);
	DrawCard(out, card, l, origin);
	GrowTooltipRect({ origin, l.size });

	const Item *container = HoveredContainerItem();
	if (container == nullptr || container->isEmpty())
		return;
	const Player &player = *InspectPlayer;
	std::vector<Rectangle> placed;
	for (const inv_body_loc loc : EquippedCounterparts(player, *container)) {
		if (&player.InvBody[loc] == container)
			continue;
		TooltipBlock block = CaptureItemBlock(player.InvBody[loc]);
		block.text = std::string(_("EQUIPPED ITEM")) + "\n" + block.text;
		block.colors.insert(block.colors.begin(), UiFlags::ColorOracoolGreen);
		block.tails.insert(block.tails.begin(), 0);
		block.runs.insert(block.runs.begin(), {});
		const Card worn = BuildCard(block, &player.InvBody[loc]);
		const CardLayout wl = MeasureCard(worn);
		const Point at = PlaceComparison({ origin, l.size }, placed, wl.size);
		DrawCard(out, worn, wl, at);
		placed.push_back({ at, wl.size });
		GrowTooltipRect({ at, wl.size });
	}
}

} // namespace

void DrawCursorTooltip(const Surface &out)
{
	PrevTooltipRect = {};

	if (talkflag || InfoString.empty())
		return;

	const bool asPanel = IsHoveringItem();
	// The card (2026-09-26), behind its option; the panel below is the look it replaced, untouched, for
	// the option's OFF - and for an indexed surface, which the card's plate cannot shade.
	if (asPanel && *sgOptions.Oracool.itemTooltipCard && !out.isIndexed()) {
		DrawCardTooltip(out);
		return;
	}
	const BlockMetrics m = MeasureBlock(InfoString.str(), asPanel);
	const Size boxSize = m.boxSize;

	// The upper bounds are floored at 0 rather than used raw: std::clamp is undefined when hi < lo,
	// which is what a box wider or taller than the screen would produce. Unlikely with a 12pt font
	// on a 960-wide canvas, but the item panel made it reachable in a way the one-line tooltip
	// never was, and UnsafeDrawBorder2px below does no clipping of its own.
	const int maxX = std::max(0, static_cast<int>(gnScreenWidth) - boxSize.width);
	const int maxY = std::max(0, static_cast<int>(gnScreenHeight) - boxSize.height);

	Point origin { MousePosition.x - boxSize.width / 2, MousePosition.y - boxSize.height - GapAboveCursor };
	origin.x = std::clamp(origin.x, 0, maxX);
	// Near the top of the screen there is no room above the cursor, so fall below it instead of
	// letting the text sit on top of what is being hovered. A tall item panel hits this often.
	if (origin.y < 0)
		origin.y = std::min(MousePosition.y + GapAboveCursor, maxY);
	origin.y = std::clamp(origin.y, 0, maxY);

	const Rectangle box { origin, boxSize };
	DrawBlock(out, box, m, InfoString.str(), InfoStringLineColors, InfoStringLineTailStart, InfoStringLineRuns, InfoColor, asPanel);

	// The outline bleeds a pixel past the glyphs, so the region the dirty-rect path has to erase
	// is slightly larger than the text box itself. The panel's border is already inside `box`.
	const int bleed = asPanel ? 0 : 2;
	PrevTooltipRect = { { origin.x - bleed, origin.y - bleed },
		{ boxSize.width + bleed * 2, boxSize.height + bleed * 2 } };

	// THE COMPARISON (user, 2026-09-05: "add comparison tool tip (showing stats of equipped item)
	// when hovering over items in stash/inv grid for easy comparison to equipped item of same item
	// slot. equipped item tooltip to show title EQUIPPED ITEM somewhere (bottom or top of
	// description in GREEN font)"). Beside the hovered item's panel - right of it, left when the
	// right has no room - one panel per worn counterpart (two for a ring or a two-hander), each
	// headed EQUIPPED ITEM in green and otherwise printed by the same item printer as the hover.
	if (!asPanel)
		return;
	const Item *hovered = HoveredContainerItem();
	if (hovered == nullptr || hovered->isEmpty())
		return;
	const Player &player = *InspectPlayer;
	std::vector<Rectangle> placed;
	for (const inv_body_loc loc : EquippedCounterparts(player, *hovered)) {
		if (&player.InvBody[loc] == hovered)
			continue; // a worn piece on the Repair or Recharge tab: nothing to compare it with but itself
		TooltipBlock block = CaptureItemBlock(player.InvBody[loc]);
		block.text = std::string(_("EQUIPPED ITEM")) + "\n" + block.text;
		block.colors.insert(block.colors.begin(), UiFlags::ColorOracoolGreen);
		block.tails.insert(block.tails.begin(), 0);
		block.runs.insert(block.runs.begin(), {});
		const BlockMetrics cm = MeasureBlock(block.text, /*asPanel=*/true);
		const Rectangle cbox { PlaceComparison(box, placed, cm.boxSize), cm.boxSize };
		placed.push_back(cbox);
		DrawBlock(out, cbox, cm, block.text, block.colors, block.tails, block.runs, UiFlags::ColorWhite, /*asPanel=*/true);
		// One dirty rect for the lot, so the erase pass covers every panel drawn this frame.
		const int right = std::max(PrevTooltipRect.position.x + PrevTooltipRect.size.width, cbox.position.x + cbox.size.width);
		const int bottom = std::max(PrevTooltipRect.position.y + PrevTooltipRect.size.height, cbox.position.y + cbox.size.height);
		PrevTooltipRect.position.x = std::min(PrevTooltipRect.position.x, cbox.position.x);
		PrevTooltipRect.position.y = std::min(PrevTooltipRect.position.y, cbox.position.y);
		PrevTooltipRect.size = { right - PrevTooltipRect.position.x, bottom - PrevTooltipRect.position.y };
	}
}

} // namespace devilution::oracool
