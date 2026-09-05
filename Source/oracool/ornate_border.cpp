#include "utils/language.h"
#include "oracool/ornate_border.h"

#include <vector>
#include <algorithm>
#include <string>

#include "engine/render/primitive_render.hpp"
#include "options.h"        // panelDocking - bottom or middle is the player's call
#include "utils/ui_fwd.h"  // gnScreenHeight - the line the panels dock against
#include "engine/render/text_render.hpp"

namespace devilution::oracool {

namespace {

// Sampled from 99-original-game-art/png/textbox/textbox_frame00.png at the mid-point of each edge,
// then matched back to palette indices. The frame is a bevel lit from the bottom-right: all four
// outer edges share a mid-gold, and the SECOND ring is what carries the lighting - dark on the top
// and left, bright on the bottom and right. The innermost ring is near-black all round, which is
// what separates the frame from whatever it surrounds.
//
// The original's edges vary by a shade or two along their length (201/202/203 on the outer ring,
// 197/198/200 on the lit middle one) - hand-painted texture rather than a rule. These are the
// modal values; reproducing the per-pixel noise would need the art itself, at which point it could
// not be resized.
constexpr uint8_t OuterColor = 202;      // (91, 81, 52)
constexpr uint8_t MidShadowColor = 204;  // (57, 49, 29) - top and left
constexpr uint8_t MidHighlightColor = 198; // (152, 139, 93) - bottom and right
constexpr uint8_t InnerColor = 223;      // (15, 5, 0)

void DrawRing(const Surface &out, Rectangle rect, uint8_t topLeftColor, uint8_t bottomRightColor)
{
	if (rect.size.width <= 0 || rect.size.height <= 0)
		return;

	const int x = rect.position.x;
	const int y = rect.position.y;
	const int width = rect.size.width;
	const int height = rect.size.height;

	DrawHorizontalLine(out, { x, y }, width, topLeftColor);
	DrawVerticalLine(out, { x, y }, height, topLeftColor);
	DrawHorizontalLine(out, { x, y + height - 1 }, width, bottomRightColor);
	DrawVerticalLine(out, { x + width - 1, y }, height, bottomRightColor);
}

Rectangle Inset(Rectangle rect, int by)
{
	return Rectangle { { rect.position.x + by, rect.position.y + by },
		{ rect.size.width - 2 * by, rect.size.height - 2 * by } };
}

} // namespace

void DrawOrnateBorderOutside(const Surface &out, Rectangle rect)
{
	// The bevel's three rings are drawn INSIDE whatever rect they are given, so framing content
	// without eating into it means handing them a rect grown by their own width.
	DrawOrnateBorder(out,
	    Rectangle { { rect.position.x - OrnateBorderWidth, rect.position.y - OrnateBorderWidth },
	        { rect.size.width + 2 * OrnateBorderWidth, rect.size.height + 2 * OrnateBorderWidth } });
}

void DrawOrnateBorder(const Surface &out, Rectangle rect)
{
	DrawRing(out, rect, OuterColor, OuterColor);
	DrawRing(out, Inset(rect, 1), MidShadowColor, MidHighlightColor);
	DrawRing(out, Inset(rect, 2), InnerColor, InnerColor);
}

void DrawLegacyTextBox(const Surface &out, Rectangle rect, uint8_t fill)
{
	if (rect.size.width <= 2 * LegacyTextBoxBevel || rect.size.height <= 2 * LegacyTextBoxBevel)
		return;
	// See the header for where these four come from. The field first, then the three rings over
	// its edge, so a fill that is not the default still sits inside the stripe.
	const Rectangle field = Inset(rect, LegacyTextBoxBevel);
	FillRect(out, field.position.x, field.position.y, field.size.width, field.size.height, fill);
	constexpr uint8_t Rail = 204;
	constexpr uint8_t StripeTopLeft = 194;
	constexpr uint8_t StripeBottomRight = 195;
	DrawRing(out, rect, Rail, Rail);
	DrawRing(out, Inset(rect, 1), StripeTopLeft, StripeBottomRight);
	DrawRing(out, Inset(rect, 2), Rail, Rail);
}

void DrawOrnateSeparator(const Surface &out, Point from, int width)
{
	// The bevel's own vertical cross-section, top to bottom: the shadow it puts on a top edge, the
	// mid-gold body, then the highlight it puts on a bottom edge. Reusing those three indices is
	// what makes a rule drawn here read as part of the same frame rather than a line laid over it.
	DrawHorizontalLine(out, from, width, MidShadowColor);
	DrawHorizontalLine(out, { from.x, from.y + 1 }, width, OuterColor);
	DrawHorizontalLine(out, { from.x, from.y + 2 }, width, MidHighlightColor);
}

void DrawOrnateSeparatorVertical(const Surface &out, Point from, int height)
{
	// Left-to-right this time, matching how the bevel lights a left edge dark and a right edge
	// bright - so a vertical rule reads consistently with the frame's own sides.
	DrawVerticalLine(out, from, height, MidShadowColor);
	DrawVerticalLine(out, { from.x + 1, from.y }, height, OuterColor);
	DrawVerticalLine(out, { from.x + 2, from.y }, height, MidHighlightColor);
}

void DrawThemedFill(const Surface &out, Rectangle rect, int passes)
{
	if (rect.size.width <= 0 || rect.size.height <= 0)
		return;
	for (int i = 0; i < std::max(1, passes); i++)
		DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
}

void DrawColoredOutline(const Surface &out, Rectangle rect, uint8_t color)
{
	if (rect.size.width <= 1 || rect.size.height <= 1)
		return;
	const int x = rect.position.x;
	const int y = rect.position.y;
	const int w = rect.size.width;
	const int h = rect.size.height;
	DrawHorizontalLine(out, { x, y }, w, color);
	DrawHorizontalLine(out, { x, y + h - 1 }, w, color);
	DrawVerticalLine(out, { x, y }, h, color);
	DrawVerticalLine(out, { x + w - 1, y }, h, color);
}

void DrawSplitOutline(const Surface &out, Rectangle rect, uint8_t leftTopColor, uint8_t rightBottomColor, int weight)
{
	if (weight <= 0 || rect.size.width <= 2 * weight || rect.size.height <= 2 * weight)
		return;
	const int x = rect.position.x;
	const int y = rect.position.y;
	const int w = rect.size.width;
	const int h = rect.size.height;

	// The VERTICALS run the full height and the horizontals stop short of them, so each corner
	// belongs to the side it is on: both left corners to the left edge's colour, both right corners
	// to the right edge's. Drawing all four full length instead would hand both mixed corners to
	// whichever colour was painted second, which reads as one colour bleeding into the other rather
	// than as a square split down the middle.
	const int innerX = x + weight;
	const int innerWidth = w - 2 * weight;
	for (int i = 0; i < weight; i++) {
		DrawHorizontalLine(out, { innerX, y + i }, innerWidth, leftTopColor);
		DrawHorizontalLine(out, { innerX, y + h - 1 - i }, innerWidth, rightBottomColor);
		DrawVerticalLine(out, { x + i, y }, h, leftTopColor);
		DrawVerticalLine(out, { x + w - 1 - i, y }, h, rightBottomColor);
	}
}

void DrawHoverOutline(const Surface &out, Rectangle rect)
{
	// MidHighlightColor, the frame's LIT gold, rather than OuterColor's dimmer one. Both are "gold"
	// and the dim one is the more literal reading of "subtle", but these rows sit on a
	// half-transparent panel over the dungeon, where 202 barely separates from the fill. 198 reads as
	// a highlight at a glance and still belongs to the same frame the window is built from.
	DrawColoredOutline(out, rect, MidHighlightColor);
}

void DrawHoverOutlineHeavy(const Surface &out, Rectangle rect, int clearanceX, int clearanceY)
{
	constexpr int Weight = 3;
	// Twelve outside a slot's rect left and right (user, 2026-09-05: "the rectangle left border to
	// be 3px to the left from the cast shadow from the spell slots"): the carved bezel is six
	// outside the rect, its shadow three beyond that, and three of air after the shadow. Nine above
	// and below, so the ring and its shadow fit the six of air between neighbouring frames - see the
	// header. A caller whose rect has neither bezel nor shadow passes 0 for both.
	const Rectangle outer { rect.position - Displacement { clearanceX, clearanceY },
		{ rect.size.width + 2 * clearanceX, rect.size.height + 2 * clearanceY } };

	// The shadow: the same ring two left and two down, as a half-transparent darkening - "black/
	// semi-transparent, like the skill slots" - not a solid colour. Four bands that do not overlap,
	// so no corner is darkened twice.
	constexpr Displacement ShadowOffset { -2, 2 };
	const Rectangle s { outer.position + ShadowOffset, outer.size };
	DrawHalfTransparentRectTo(out, s.position.x, s.position.y, s.size.width, Weight);
	DrawHalfTransparentRectTo(out, s.position.x, s.position.y + s.size.height - Weight, s.size.width, Weight);
	DrawHalfTransparentRectTo(out, s.position.x, s.position.y + Weight, Weight, s.size.height - 2 * Weight);
	DrawHalfTransparentRectTo(out, s.position.x + s.size.width - Weight, s.position.y + Weight, Weight, s.size.height - 2 * Weight);

	DrawSplitOutline(out, outer, MidHighlightColor, MidHighlightColor, Weight);
}

void DrawDropShadow(const Surface &out, Rectangle rect, int bezelWidth)
{
	// The character sheet's text shadow ANGLE - left and down - at three pixels (user, 2026-09-05:
	// 2 was under the bezel, 6 was "increase px count to 6", then "i really meant making it 3px").
	constexpr Displacement ShadowOffset { -3, 3 };
	const Rectangle footprint { rect.position - Displacement { bezelWidth, bezelWidth },
		{ rect.size.width + 2 * bezelWidth, rect.size.height + 2 * bezelWidth } };
	const Rectangle shadow { footprint.position + ShadowOffset, footprint.size };
	// One pass, not two (user, 2026-09-05: "reduce the shadow by half") - half the darkening.
	DrawHalfTransparentRectTo(out, shadow.position.x, shadow.position.y, shadow.size.width, shadow.size.height);
}

bool IsHoverHeadingLine(string_view line)
{
	// The headings the two block builders emit (ClassTreeEffectLine, BuildSpellStatBlock), matched
	// on their translated prefix so the panel and the builders cannot disagree about a language.
	const string_view headings[] = {
		_("Current Skill Level"), _("Current Spell Level"), _("Next Level"), _("First Level")
	};
	for (const string_view heading : headings) {
		if (line.size() >= heading.size() && line.substr(0, heading.size()) == heading)
			return true;
	}
	return false;
}

void DrawHoverPanel(const Surface &out, string_view title, string_view text, Rectangle anchor)
{
	DrawHoverPanel(out, title, text, anchor, Rectangle { { 0, 0 }, { 0, 0 } });
}

void DrawHoverPanel(const Surface &out, string_view title, string_view text, Rectangle anchor, Rectangle avoid)
{
	if (title.empty() && text.empty())
		return;

	// Wide enough for a sentence without becoming a paragraph, and narrow enough to sit beside the
	// 340px window rather than across the view.
	constexpr int MaxTextWidth = 240;
	constexpr int Padding = 10;
	constexpr int Gap = 8; // between the row (or the window it is in) and the panel

	const std::string wrapped = WordWrapString(text, MaxTextWidth, GameFont12, 1);
	const int lineHeight = 18;

	// The lines, kept as lines: each is drawn on its own so a heading can wear its own colour
	// (user, 2026-09-05: "Name - in Gold. Current Level: X - in gold. Next Level - in gold").
	std::vector<string_view> lines;
	{
		size_t start = 0;
		const string_view all = wrapped;
		while (start <= all.size()) {
			const size_t end = all.find('\n', start);
			lines.push_back(all.substr(start, end == std::string::npos ? std::string::npos : end - start));
			if (end == std::string::npos)
				break;
			start = end + 1;
		}
	}

	// Measured from the WRAPPED text, so a short description gets a short panel instead of always
	// reserving the full MaxTextWidth.
	int textWidth = 0;
	for (const string_view line : lines)
		textWidth = std::max(textWidth, GetLineWidth(line, GameFont12, 1));

	// Oracool: user request (2026-08-15) - "Put Skill name with Gold letters in the pop-up window."
	// The name moved here because the rows themselves lost their text, so this is now the ONLY place
	// a skill says what it is called. Measured into the panel's width so a long name widens the box
	// rather than wrapping under the description.
	const int titleWidth = title.empty() ? 0 : GetLineWidth(title, GameFont12, 1);
	const int titleHeight = title.empty() ? 0 : lineHeight;

	const int panelWidth = std::max(textWidth, titleWidth) + 2 * Padding;
	const int panelHeight = titleHeight + static_cast<int>(lines.size()) * lineHeight + 2 * Padding;

	// BESIDE THE WINDOW, never on it (user, 2026-09-05: "to not overlap abilities window. to be
	// adjacent to it - 8px apart"). When a rect to avoid is given the panel hangs off ITS edge -
	// left of it by preference, right of it when the left has no room - rather than off the row's,
	// whose left edge is the window's interior and left the panel across the frame. Without one,
	// the old rule: right of the row, flipped left when there is no room.
	int px;
	if (avoid.size.width > 0) {
		px = avoid.position.x - Gap - panelWidth;
		if (px < 0)
			px = avoid.position.x + avoid.size.width + Gap;
	} else {
		px = anchor.position.x + anchor.size.width + Gap;
		if (px + panelWidth > out.w())
			px = anchor.position.x - Gap - panelWidth;
	}
	px = std::clamp(px, 0, std::max(0, out.w() - panelWidth));
	// Vertically centred on the row, then pulled back inside the screen.
	int py = anchor.position.y + (anchor.size.height - panelHeight) / 2;
	py = std::clamp(py, 0, std::max(0, out.h() - panelHeight));

	const Rectangle panel { { px, py }, { panelWidth, panelHeight } };
	// Two passes: one is too sheer to read text over when the dungeon behind it is bright.
	DrawThemedFill(out, panel, 2);
	// A GOLD frame (user, 2026-09-05: "Tooltip window to have golden border"): two rings off the
	// yellow ramp, the darker outside, over the theme's brown tracery which stays as the outer line.
	DrawOrnateBorder(out, panel);
	DrawRing(out, Inset(panel, 1), PAL16_YELLOW + 9, PAL16_YELLOW + 9);
	DrawRing(out, Inset(panel, 2), PAL16_YELLOW + 4, PAL16_YELLOW + 4);
	const int innerWidth = panelWidth - 2 * Padding;
	// Centred, as D2 sets its skill tooltips (user, 2026-09-05: "look at diablo 2 description
	// theme"). Shadowed, title and text (user, 2026-09-05: "apply the same text shadow to abilities
	// tree skill names") - this panel is the Abilities window's hover, and the skill's name is its
	// title.
	if (!title.empty()) {
		DrawString(out, title, { { px + Padding, py + Padding }, { innerWidth, lineHeight } },
		    { UiFlags::ColorGold | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}
	int y = py + Padding + titleHeight;
	for (const string_view line : lines) {
		if (!line.empty()) {
			const UiFlags color = IsHoverHeadingLine(line) ? UiFlags::ColorGold : UiFlags::ColorWhite;
			DrawString(out, line, { { px + Padding, y }, { innerWidth, lineHeight } },
			    { color | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
		}
		y += lineHeight;
	}
}

void DrawOutlinedString(const Surface &out, string_view text, Rectangle area, UiFlags style)
{
	constexpr Displacement Offsets[] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
	// Strip the caller's colour from the outline pass, keeping its size and alignment, so the
	// black sits exactly under the glyphs rather than at a different size or offset.
	constexpr UiFlags ColorMask = UiFlags::ColorUiGold | UiFlags::ColorUiSilver | UiFlags::ColorUiGoldDark
	    | UiFlags::ColorUiSilverDark | UiFlags::ColorDialogWhite | UiFlags::ColorDialogYellow
	    | UiFlags::ColorDialogRed | UiFlags::ColorYellow | UiFlags::ColorGold | UiFlags::ColorBlack
	    | UiFlags::ColorWhite | UiFlags::ColorWhitegold | UiFlags::ColorRed | UiFlags::ColorBlue
	    | UiFlags::ColorOrange | UiFlags::ColorButtonface | UiFlags::ColorButtonpushed;
	const UiFlags layout = style & ~ColorMask;
	for (const Displacement &d : Offsets) {
		Rectangle shifted = area;
		shifted.position += d;
		DrawString(out, text, shifted, { layout | UiFlags::ColorBlack });
	}
	DrawString(out, text, area, { style });
}

int BottomDockedTop(int windowHeight)
{
	const int slack = static_cast<int>(gnScreenHeight) - windowHeight;
	if (slack <= 0)
		return 0;
	return *sgOptions.Oracool.panelDocking == PanelDocking::Middle ? slack / 2 : slack;
}

} // namespace devilution::oracool
