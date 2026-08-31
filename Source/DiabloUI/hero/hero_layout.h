/**
 * @file hero_layout.h
 *
 * Oracool: the chrome the character screens share - the logo's height, the title band, the action row
 * at the bottom, and the band of screen between them.
 *
 * Extracted from selhero.cpp when the delete-character confirmation became the fourth screen wanting
 * the same row. Three screens could live with the geometry sitting in the file that owned most of
 * them; a fourth in a different file could not, and copying it would have made "the buttons are in the
 * same place" a thing that is true today rather than a thing that stays true.
 *
 * Header-only on purpose: it is a dozen constants and six one-line functions, and a .cpp for it would
 * be more build wiring than code.
 */
#pragma once

#include <algorithm>

#include <SDL.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/size.hpp"
#include "oracool/ui_backgrounds.h" // MapBackgroundPointToScreen - the dais mark
#include "utils/display.h"
#include "utils/sdl_geometry.h"

namespace devilution {

/**
 * Oracool: user request - the logo 120px higher than UiAddLogo's default (which is the UI rect's own
 * top, i.e. inset from the screen). Clamped at zero: the curated resolutions are all at least 720 tall
 * so the inset is always >= 120, but a negative y would clip the flames off the top.
 */
inline int HeroLogoTop()
{
	return std::max(0, GetUIRectangle().position.y - 120);
}

/**
 * @brief The screen title's band, UI-rect-relative. Shared so the content can start below it.
 *
 * 117, not the 61 that stood here while the masthead was ui_art\smlogo. The logo is ui_art\logo now
 * (user request) and that frame is 216px tall against smlogo's 154 - measured, and both are
 * full-bleed, with ink running to within 2px of the frame's bottom edge, so there is no transparent
 * padding to absorb the difference.
 *
 * At 720p the logo sits at screen y=0 (HeroLogoTop) and ends at 216; the title starts at 120+117=237,
 * leaving 21px of air under the flames. The full 62px shift would have given the old 27px gap but
 * left the content band 5px short of six class rows - see below.
 *
 * THE BAND HOLDS EXACTLY SIX CLASS ROWS AT 720p. HeroContentTop..HeroContentBottom works out at
 * 313px and the class list is 6 x HeroListItemHeight (52) = 312. A seventh class, a taller row or a
 * lower title all need the class list to start scrolling rather than just being nudged.
 */
constexpr int HeroTitleTop = 117;
constexpr int HeroTitleHeight = 42; // the FontSize42 line height - see HeroTitleRect
constexpr UiFlags HeroTitleFontSize = UiFlags::FontSize42;

/**
 * Oracool: user request - the title 100px higher and a size larger (30 -> 42). The rect grew from 35
 * to 42 tall with it, because DrawString clips at rect.y + rect.height while anchoring the glyph
 * bottom at rect.y + the font's line height; at FontSize42 a 35px rect would have taken seven pixels
 * off the bottom of every letter.
 */
inline SDL_Rect HeroTitleRect()
{
	const Point uiPosition = GetUIRectangle().position;
	return MakeSdlRect(static_cast<Sint16>(uiPosition.x + 24), static_cast<Sint16>(uiPosition.y + HeroTitleTop),
	    590, HeroTitleHeight);
}

/**
 * Oracool: user rule - "divide the 960px screen into 4 equal width vertical zones. The bottom of each
 * zone, in the middle, to be considered a button designation."
 *
 * So the row is the WHOLE window, quartered - not the 800px line centred on the screen that stood
 * here before, which left 80px of dead margin at each end and made a "zone" something you had to
 * measure rather than something you could see. A zone is `gnScreenWidth / 4`: 240px at 960, and it
 * scales, so the rule holds at every resolution rather than only at the one it was written for.
 *
 * Each button fills its zone and centres its label in it, so the row stays evenly spaced whatever the
 * labels translate to, and a wider zone (240 against the old 200) gives the longest label - New Hero -
 * more room than it had.
 *
 * Height is the FontSize42 line height rather than the 35 the old buttons used: DrawString anchors a
 * glyph's bottom at rect.y + lineHeight and clips at rect.y + rect.height, so 35 was quietly shaving
 * pixels off every label. Nothing showed because none of these words has a descender.
 */
constexpr int HeroButtonRowHeight = 42;
constexpr int HeroButtonRowBottomMargin = 50;
/** @brief Buttons and hero names, both a size up on the user's call. */
constexpr UiFlags HeroButtonFontSize = UiFlags::FontSize42;
constexpr UiFlags HeroListFontSize = UiFlags::FontSize30;

/**
 * @brief The four zones, and which button owns each.
 *
 * Oracool: user rule, verbatim - "buttons OK, Cancel live in section 2 and 3 on every front end
 * screen"; "should more buttons be required by a screen they will occupy bottoms of zone 1 and 4.
 * New Hero takes 1, Delete takes 4."
 *
 * The user counts zones from one, so the indices below are their numbers minus one. Reading them in
 * order gives the row as it appears: New Hero, OK, Cancel, Delete.
 *
 * The point of fixing them by zone rather than by "next free cell" is that no button ever moves
 * between screens. OK is in the same place on the character list, the class list, the name box, the
 * difficulty picker and the delete prompt; a screen with only OK and Cancel simply leaves zones 1 and
 * 4 empty rather than sliding its two controls inward to close the gap. A control that stays put is
 * one you stop having to look for.
 *
 * Yes/No on the delete prompt are the OK and Cancel of that screen and take those two zones (see
 * selyesno.cpp) - the rule is about the positions, not about the words printed in them.
 */
constexpr int HeroButtonCount = 4;
constexpr int NewHeroButtonIndex = 0;
constexpr int OkButtonIndex = 1;
constexpr int CancelButtonIndex = 2;
constexpr int DeleteButtonIndex = 3;

/** @brief Every action-row button's look. Shared so a screen cannot drift from the others. */
constexpr UiFlags HeroButtonFlags = UiFlags::AlignCenter | HeroButtonFontSize | UiFlags::ColorUiGold;

/** @brief Clearance between the content band and the action row. */
constexpr int ListToButtonsGap = 12;

inline int HeroButtonRowTop()
{
	return gnScreenHeight - HeroButtonRowBottomMargin - HeroButtonRowHeight;
}

/**
 * @brief The width the zones are cut from - the window, but never more than 960.
 *
 * Oracool: user report - "are you sure the four main buttons at the bottom fit within 960px width
 * with change of aspect ratio, to me it seems as if they drift away with increased ratio."
 *
 * They fit, and they drifted. Nothing ever left the window or clipped: measured at 960 the widest
 * label is New Hero at 180px inside a 240px zone, and the labels do not grow with the window. But
 * the ZONES did - cutting gnScreenWidth into quarters put the outer two centres at 120 and 840 at
 * 960 wide and at 160 and 1120 at 1280, so the row spread by 200px at each end while the words in it
 * stayed the same size and everything else on the screen stayed put.
 *
 * Capped, so the row has one shape at every width. The user's rule named "the 960px screen" when it
 * set these zones out, which is what this restores: at 960 the arithmetic is bit-for-bit what it was,
 * and wider windows centre the same 960px row rather than stretching it.
 */
constexpr int HeroButtonRowMaxWidth = 960;

/**
 * @brief Zone @p index of @p count equal zones, at the button line.
 *
 * @p count is a parameter only so a screen can say what it means; every caller now passes the default.
 * The one that did not was the lone-OK message box, which quartered the row into thirds to get its
 * single button dead centre - and under the zone rule OK belongs in zone 2 like everywhere else.
 */
inline SDL_Rect HeroButtonRect(int index, int count = HeroButtonCount)
{
	// Cast, because gnScreenWidth is not an int and std::min will not deduce across the two.
	const int rowWidth = std::min(static_cast<int>(gnScreenWidth), HeroButtonRowMaxWidth);
	const int left = (static_cast<int>(gnScreenWidth) - rowWidth) / 2;
	const int cell = rowWidth / count;
	return MakeSdlRect(static_cast<Sint16>(left + index * cell), static_cast<Sint16>(HeroButtonRowTop()),
	    static_cast<Uint16>(cell), static_cast<Uint16>(HeroButtonRowHeight));
}

/** @brief The vertical band between the title and the action row - everything else lives in it. */
inline int HeroContentTop()
{
	return GetUIRectangle().position.y + HeroTitleTop + HeroTitleHeight + 24;
}

inline int HeroContentBottom()
{
	return HeroButtonRowTop() - ListToButtonsGap;
}

/**
 * Oracool: the class list, the name box, the multiplayer Continue prompt and the delete confirmation
 * used to hang off the 640x480 dialog art - `uiPosition.x + 264`, `uiPosition.y + 246` and so on.
 * Those numbers composed against a painting that is no longer there, and they are not even centred:
 * the old caption rect (`x + 242`, 365 wide) sits 104px right of the screen's middle at 960.
 *
 * So they share one centred column in the same band the character list uses, at the same font. Four
 * screens that were four layouts are one layout with different contents in it.
 */
/**
 * @brief The character list's column - zone FOUR, list plus gap plus scrollbar.
 *
 * Moved here from selhero.cpp on 2026-08-31, when the stats column opposite needed the same width
 * and a test needed to prove the two cannot collide. It was file-local because nothing outside that
 * screen asked; two things do now, and this header is where the screen's geometry lives.
 *
 * The reasoning behind the zone anchor is unchanged and worth keeping: pinned to the WINDOW's right
 * edge the column drifts away from the button row at any width past 960, because the row is capped
 * to a centred 960 band. Tied to zone four's own rect it goes where the row goes.
 */
constexpr int HeroScrollbarWidth = 12;
constexpr int HeroScrollbarGap = 4;

inline int HeroListWidth()
{
	const SDL_Rect zone = HeroButtonRect(DeleteButtonIndex);
	return std::max(0, zone.w - HeroScrollbarGap - HeroScrollbarWidth);
}

inline int HeroListX()
{
	return HeroButtonRect(DeleteButtonIndex).x;
}

/**
 * @brief The stats column - zone ONE, mirroring the character list in zone four.
 *
 * Oracool: user request (2026-08-31), "a hero stats somewhere befitting". The list sits in the
 * button row's fourth zone and the figure stands in the middle, which left the whole left band of
 * this screen empty; putting the stats opposite the list makes the screen a pair of columns with
 * the character between them instead of a right-heavy one.
 *
 * NOT where the old class portrait and its five stat rows were - that space is the animated figure
 * now, by the same user's earlier call, and this is meant to sit beside it rather than displace it
 * again.
 *
 * Tied to zone one's own rect for the reason the list is tied to zone four's (see HeroListWidth's
 * note): anything anchored to the window edge instead drifts away from the button row the moment
 * the row is capped to a centred 960 band. Width matches the list's exactly so the two columns are
 * a matched pair, and the vertical band is the same HeroContentTop..HeroContentBottom.
 */
inline SDL_Rect HeroStatsColumnRect()
{
	const SDL_Rect zone = HeroButtonRect(NewHeroButtonIndex);
	const int top = HeroContentTop();
	return MakeSdlRect(static_cast<Sint16>(zone.x), static_cast<Sint16>(top),
	    static_cast<Uint16>(HeroListWidth()), static_cast<Uint16>(std::max(0, HeroContentBottom() - top)));
}

constexpr int HeroFormWidth = 400;
constexpr int HeroFormCaptionHeight = 42;
constexpr int HeroFormCaptionGap = 16;

inline int HeroFormX()
{
	return (gnScreenWidth - HeroFormWidth) / 2;
}

/** @brief The top of whatever sits under a caption. */
inline int HeroFormBodyTop()
{
	return HeroContentTop() + HeroFormCaptionHeight + HeroFormCaptionGap;
}

/** @brief @p height centred in the band between the caption and the action row. */
inline int HeroFormBodyTopFor(int height)
{
	const int top = HeroFormBodyTop();
	return top + std::max(0, (HeroContentBottom() - top - height) / 2);
}

/**
 * @brief @p height centred in the WHOLE content band, for a screen with no caption over it.
 *
 * The difference is HeroFormCaptionHeight + HeroFormCaptionGap - 58px that a captioned screen spends
 * before its body starts. That is not a rounding matter: the class list is six rows of
 * HeroListItemHeight (52) = 312px, and the band is 313, so a caption above it puts the last class
 * straight through the action row. Losing the caption is what makes six classes fit at all.
 */
inline int HeroContentTopFor(int height)
{
	const int top = HeroContentTop();
	return top + std::max(0, (HeroContentBottom() - top - height) / 2);
}

/** @brief A caption sitting under the title, over a column of the given width. */
inline SDL_Rect HeroCaptionRect(int x, int width)
{
	return MakeSdlRect(static_cast<Sint16>(x), static_cast<Sint16>(HeroContentTop()),
	    static_cast<Uint16>(width), HeroFormCaptionHeight);
}

/**
 * @brief The character screens' painting, and the spot on it a character is meant to stand.
 *
 * Oracool: user request - "one is to be used as background for hero select. the other one has a
 * green circle on it - i want you to make it so that the preview sprites of heros land on that
 * spot." Two copies of the same 1916x821 image were supplied, one clean and one with the spot marked
 * in pure green.
 *
 * MEASURED off the marked copy rather than eyeballed: every pixel with G > 90 and G more than 50
 * above both R and B, 4324 of them, forming an ellipse with its bounding box at x 898..1015,
 * y 623..668 and its centroid at (957, 646). An ellipse and not a circle because it is a mark on the
 * FLOOR, drawn in the painting's perspective - the middle of the stone dais in the foreground.
 *
 * Only the centroid is kept. The ellipse's size says how big the dais is, not how big the character
 * should be, and reading a scale out of it would be inventing a requirement.
 *
 * Here rather than in selhero.cpp because the DELETE prompt stands its character on the same dais
 * (user request). One measurement, two screens - the alternative was a second copy of these numbers
 * that agreed until one of them was edited.
 */
constexpr Size HeroSelectArtSize { 1916, 821 };
constexpr Point HeroSelectGroundInArt { 957, 646 };

/**
 * @brief How far below the measured mark the figure actually stands.
 *
 * Oracool: user request - "bring the previews about 20-30px lower". Kept as an offset rather than
 * folded into HeroSelectGroundInArt above, so the measurement stays a measurement: that centroid is
 * what the green ellipse says, and this is the adjustment made after looking at a character standing
 * on it. Editing the marked point would have left a number that agreed with nothing.
 *
 * SCREEN pixels, not art pixels, because that is the space the request was made in - and it holds at
 * both target resolutions, which are 720 tall.
 */
constexpr int HeroPreviewGroundNudgeY = 25;

/** @brief That spot, in screen pixels at the current resolution. */
inline Point HeroPreviewGroundPoint()
{
	const Point mark = oracool::MapBackgroundPointToScreen(HeroSelectArtSize, HeroSelectGroundInArt);
	return { mark.x, mark.y + HeroPreviewGroundNudgeY };
}

/**
 * @brief The figure's own top - closer under the title than HeroContentTop's, by 16px.
 *
 * That 24px gap is the LIST's breathing room: rows of text immediately under a heading of the same
 * colour need the separation. The figure does not - it reads as a picture, not as another line - and
 * every pixel here is a pixel of scale, since the preview is height-bound (see PreviewScaleFor).
 */
inline int HeroPreviewTop()
{
	return GetUIRectangle().position.y + HeroTitleTop + HeroTitleHeight + 8;
}

/**
 * @brief Where the animated character stands - on the dais the painting draws for it.
 *
 * The rect is built so its BOTTOM EDGE is the mark and its middle is over it, because that is where
 * DrawHeroPreview puts the figure: centred across the area, standing on its bottom. Aiming the area
 * rather than the sprite means nothing in the preview code has to learn about backgrounds.
 *
 * On the character list this used to be "everything left of the list and its scrollbar", which was
 * the right answer for a screen with no painting behind it - the figure went in the empty half. The
 * painting has a place for it now, and that place is the middle of the screen.
 */
inline Rectangle HeroPreviewRect()
{
	const Point ground = HeroPreviewGroundPoint();
	const int top = HeroPreviewTop();
	// Wider than any figure at any scale, so the width is never what limits PreviewScaleFor and the
	// only thing deciding the size is the height between the title and the dais.
	constexpr int Width = 440;
	return { { ground.x - Width / 2, top }, { Width, std::max(0, ground.y - top) } };
}

} // namespace devilution
