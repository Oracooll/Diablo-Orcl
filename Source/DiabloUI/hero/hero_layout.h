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

/** @brief The screen title's band, UI-rect-relative. Shared so the content can start below it. */
constexpr int HeroTitleTop = 61;
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
 * Oracool: user request - the character screens' action row.
 *
 * Four buttons spread evenly along an 800px line centred on the SCREEN (not on the 640-wide UI rect
 * the rest of these dialogs hang off), sitting 50px above the bottom edge. Each button owns a quarter
 * of the line and centres its label inside it, so the row stays evenly spaced whatever the labels
 * translate to.
 *
 * Height is the FontSize42 line height rather than the 35 the old buttons used: DrawString anchors a
 * glyph's bottom at rect.y + lineHeight and clips at rect.y + rect.height, so 35 was quietly shaving
 * pixels off every label. Nothing showed because none of these words has a descender.
 */
constexpr int HeroButtonRowWidth = 800;
constexpr int HeroButtonRowHeight = 42;
constexpr int HeroButtonRowBottomMargin = 50;
/** @brief Buttons and hero names, both a size up on the user's call. */
constexpr UiFlags HeroButtonFontSize = UiFlags::FontSize42;
constexpr UiFlags HeroListFontSize = UiFlags::FontSize30;

/**
 * @brief The action row: four cells, and which one each button occupies.
 *
 * OK and Cancel keep their cells on every screen that has them - the character list, the class list,
 * the name box, the multiplayer Continue prompt - so neither button moves as you step between them.
 * That is worth more than packing a two-button row closer together: a control that stays put is one
 * you stop having to look for.
 *
 * The user calls these the button columns, counting from one. The delete confirmation puts its two
 * answers in the second and third.
 */
constexpr int HeroButtonCount = 4;
constexpr int OkButtonIndex = 0;
constexpr int DeleteButtonIndex = 1;
/**
 * New Hero and Cancel swapped on the user's call, which puts **Cancel in the row's last cell** - the
 * one the list column sits over (see HeroListX). Both list screens therefore have the same button
 * under their list, which is the point of the swap.
 */
constexpr int NewHeroButtonIndex = 2;
constexpr int CancelButtonIndex = 3;

/** @brief Every action-row button's look. Shared so a screen cannot drift from the others. */
constexpr UiFlags HeroButtonFlags = UiFlags::AlignCenter | HeroButtonFontSize | UiFlags::ColorUiGold;

/** @brief Clearance between the content band and the action row. */
constexpr int ListToButtonsGap = 12;

inline int HeroButtonRowTop()
{
	return gnScreenHeight - HeroButtonRowBottomMargin - HeroButtonRowHeight;
}

/** @brief Cell @p index of a @p count-cell action row. */
inline SDL_Rect HeroButtonRect(int index, int count = HeroButtonCount)
{
	const int cell = HeroButtonRowWidth / count;
	const int left = (gnScreenWidth - HeroButtonRowWidth) / 2;
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

/** @brief A caption sitting under the title, over a column of the given width. */
inline SDL_Rect HeroCaptionRect(int x, int width)
{
	return MakeSdlRect(static_cast<Sint16>(x), static_cast<Sint16>(HeroContentTop()),
	    static_cast<Uint16>(width), HeroFormCaptionHeight);
}

} // namespace devilution
