#include "oracool/shop_toast.h"

#include <algorithm>
#include <utility>

#include <SDL.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine/render/text_render.hpp"
#include "oracool/hud_art.h" // the toast's plate
#include "oracool/ornate_border.h"
#include "oracool/shop_grid.h"

namespace devilution::oracool {

namespace {

std::string Message;
uint32_t ExpiresAtMs = 0;

constexpr int ToastHeight = 34;
/** @brief Inset from the shop panel's own left and right edges, so the banner reads as belonging to
 * the panel rather than as a separate window that happens to be the same width. */
constexpr int ToastSideInset = 14;

/*
 * Oracool: the toast's own art (batch 11, 2026-09-11) - one BLANK 312x34 plate, the message still the
 * game's text on top. 312 is the 340 shop panel less two insets; the panel's size is shop_grid.cpp's
 * own, so the width is checked at draw against the rect and only the height can be held here.
 */
constexpr const char *ShopToastArt = "ui\\shop_toast.png";
constexpr Size ShopToastPlateSize { 312, 34 };
static_assert(ShopToastPlateSize.height == ToastHeight, "shop_toast.png no longer matches ToastHeight");
static_assert(ShopToastPlateSize.width == 340 - ToastSideInset * 2, "shop_toast.png no longer matches the toast's width");

/**
 * @brief Where the banner sits: across the shop panel, just above its grid.
 *
 * Above the grid rather than over it. The grid is what the player is looking at when the refusal
 * arrives - it holds the thing they just tried to buy - and covering it to explain why they cannot
 * buy it would repeat, in miniature, the mistake this replaces.
 */
Rectangle ToastRectImpl()
{
	const Rectangle panel = GetShopPanelRect();
	const Rectangle grid = GetShopGridRect();
	const int width = panel.size.width - ToastSideInset * 2;
	// Centred in the gap between the grid's top and the panel's, so it never overlaps either. On a
	// layout where that gap is too small the toast is clamped to sit directly above the grid, which
	// is the last position that is still legible rather than a position that hides goods.
	const int gapTop = panel.position.y;
	const int gapBottom = grid.position.y;
	const int y = std::max(gapTop, gapBottom - ToastHeight);
	return Rectangle { { panel.position.x + ToastSideInset, y }, { width, ToastHeight } };
}

} // namespace

Rectangle GetShopToastRect()
{
	return ToastRectImpl();
}

void ShowShopToast(std::string message)
{
	Message = std::move(message);
	// Replaces rather than queues. Two refusals in a row are nearly always the same refusal twice -
	// a player clicking an item they cannot afford again - and a queue would make the second one
	// wait behind the first for no reason.
	ExpiresAtMs = SDL_GetTicks() + ShopToastDurationMs;
}

bool IsShopToastVisible()
{
	if (Message.empty())
		return false;
	// Signed comparison against the wrap point, so this stays correct across SDL_GetTicks' 32-bit
	// rollover instead of going true for 49 days.
	return static_cast<int32_t>(ExpiresAtMs - SDL_GetTicks()) > 0;
}

void DrawShopToast(const Surface &out)
{
	if (!IsShopToastVisible()) {
		// Cleared on the first draw after it lapses rather than left to linger as dead state, so
		// anything that later asks "is there a message" gets the same answer the screen gives.
		Message.clear();
		return;
	}

	const Rectangle toast = GetShopToastRect();
	// The shop's own surface treatment - the themed fill and the ornate border every panel in this
	// build wears - so the banner reads as part of the shop rather than as a system message that
	// happens to be on top of it. That is the whole of "in line with the new shops design".
	// The plate when it shipped and still fits the rect; otherwise that treatment drawn in code.
	const Size plate = GetLoosePngSize(ShopToastArt);
	if (plate.width != 0 && plate == toast.size) {
		DrawLoosePng(out, ShopToastArt, toast.position);
	} else {
		DrawThemedFill(out, toast, 2);
		DrawOrnateBorder(out, toast);
	}
	DrawString(out, Message, toast,
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
}

void ResetShopToastForNewGame()
{
	Message.clear();
	ExpiresAtMs = 0;
}

} // namespace devilution::oracool
