#include "oracool/shop_toast.h"

#include <algorithm>
#include <utility>

#include <SDL.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine/render/text_render.hpp"
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
	DrawThemedFill(out, toast, 2);
	DrawOrnateBorder(out, toast);
	DrawString(out, Message, toast,
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
}

void ResetShopToastForNewGame()
{
	Message.clear();
	ExpiresAtMs = 0;
}

} // namespace devilution::oracool
