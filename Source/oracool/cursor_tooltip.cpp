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
    const std::vector<UiFlags> &colors, const std::vector<uint16_t> &tails, UiFlags singleColor, bool asPanel)
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
		if (tailStart > 0 && tailStart < line.size()) {
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

	ClearPanelStrings();
	SetPanelString(item.getName(), item.getTextColor());
	if (item._iIdentified)
		PrintItemDetails(item);
	else
		PrintItemDur(item);

	TooltipBlock block { std::string(InfoString.str()), InfoStringLineColors, InfoStringLineTailStart };

	InfoString.AssignKeepingLineColors(std::string(savedText));
	InfoStringLineColors = savedColors;
	InfoStringLineTailStart = savedTails;
	return block;
}

} // namespace

void DrawCursorTooltip(const Surface &out)
{
	PrevTooltipRect = {};

	if (talkflag || InfoString.empty())
		return;

	const bool asPanel = IsHoveringItem();
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
	DrawBlock(out, box, m, InfoString.str(), InfoStringLineColors, InfoStringLineTailStart, InfoColor, asPanel);

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
	constexpr int SideGap = 6;
	int nextRight = box.position.x + box.size.width + SideGap;
	int nextLeft = box.position.x - SideGap;
	for (const inv_body_loc loc : EquippedCounterparts(player, *hovered)) {
		if (&player.InvBody[loc] == hovered)
			continue; // a worn piece on the Repair or Recharge tab: nothing to compare it with but itself
		TooltipBlock block = CaptureItemBlock(player.InvBody[loc]);
		block.text = std::string(_("EQUIPPED ITEM")) + "\n" + block.text;
		block.colors.insert(block.colors.begin(), UiFlags::ColorOracoolGreen);
		block.tails.insert(block.tails.begin(), 0);
		const BlockMetrics cm = MeasureBlock(block.text, /*asPanel=*/true);
		int cx;
		if (nextRight + cm.boxSize.width <= static_cast<int>(gnScreenWidth)) {
			cx = nextRight;
			nextRight += cm.boxSize.width + SideGap;
		} else {
			cx = std::max(0, nextLeft - cm.boxSize.width);
			nextLeft = cx - SideGap;
		}
		const int cy = std::clamp(box.position.y, 0, std::max(0, static_cast<int>(gnScreenHeight) - cm.boxSize.height));
		const Rectangle cbox { { cx, cy }, cm.boxSize };
		DrawBlock(out, cbox, cm, block.text, block.colors, block.tails, UiFlags::ColorWhite, /*asPanel=*/true);
		// One dirty rect for the lot, so the erase pass covers every panel drawn this frame.
		const int right = std::max(PrevTooltipRect.position.x + PrevTooltipRect.size.width, cbox.position.x + cbox.size.width);
		const int bottom = std::max(PrevTooltipRect.position.y + PrevTooltipRect.size.height, cbox.position.y + cbox.size.height);
		PrevTooltipRect.position.x = std::min(PrevTooltipRect.position.x, cbox.position.x);
		PrevTooltipRect.position.y = std::min(PrevTooltipRect.position.y, cbox.position.y);
		PrevTooltipRect.size = { right - PrevTooltipRect.position.x, bottom - PrevTooltipRect.position.y };
	}
}

} // namespace devilution::oracool
