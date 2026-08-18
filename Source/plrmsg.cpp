/**
 * @file plrmsg.cpp
 *
 * Implementation of functionality for printing the ingame chat messages.
 */
#include "plrmsg.h"

#include <algorithm>
#include <cstdint>

#include <fmt/format.h>

#include "automap.h" // GetMiniMapScreenRect - the history shares the minimap column
#include "control.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h"
#include "oracool/ornate_border.h"
#include "qol/chatlog.h"
#include "qol/stash.h"
#include "utils/language.h"
#include "utils/utf8.hpp"

namespace devilution {

namespace {

struct PlayerMessage {
	/** Time message was recived */
	Uint32 time;
	/** The default text color */
	UiFlags style;
	/** The text message to display on screen */
	std::string text;
	/** First portion of text that should be rendered in gold */
	string_view from;
	/** The line height of the text */
	int lineHeight;
};

std::array<PlayerMessage, 8> Messages;

int CountLinesOfText(string_view text)
{
	return 1 + std::count(text.begin(), text.end(), '\n');
}

PlayerMessage &GetNextMessage()
{
	std::move_backward(Messages.begin(), Messages.end() - 1, Messages.end()); // Push back older messages

	return Messages.front();
}

} // namespace

void plrmsg_delay(bool delay)
{
	static uint32_t plrmsgTicks;

	if (delay) {
		plrmsgTicks = -SDL_GetTicks();
		return;
	}

	plrmsgTicks += SDL_GetTicks();
	for (PlayerMessage &message : Messages)
		message.time += plrmsgTicks;
}

void EventPlrMsg(string_view text, UiFlags style)
{
	PlayerMessage &message = GetNextMessage();

	message.style = style;
	message.time = SDL_GetTicks();
	message.text = std::string(text);
	message.from = string_view(message.text.data(), 0);
	message.lineHeight = GetLineHeight(message.text, GameFont12) + 3;
	AddMessageToChatLog(text);
}

void SendPlrMsg(Player &player, string_view text)
{
	PlayerMessage &message = GetNextMessage();

	std::string from = fmt::format(fmt::runtime(_("{:s} (lvl {:d}): ")), player._pName, player._pLevel);

	message.style = UiFlags::ColorWhite;
	message.time = SDL_GetTicks();
	message.text = from + std::string(text);
	message.from = string_view(message.text.data(), from.size());
	message.lineHeight = GetLineHeight(message.text, GameFont12) + 3;
	AddMessageToChatLog(text, &player);
}

void InitPlrMsg()
{
	Messages = {};
}

void DrawPlrMsg(const Surface &out)
{
	if (ChatLogFlag)
		return;

	// Oracool (2026-08-18): the history occupies the MINIMAP'S COLUMN - flush to its right edge and
	// rising to sit flush under its bottom border, with the same ornate frame the minimap and the
	// event log wear (user: "span the Messages History window all the way to the right until it hits
	// flush the right edge of the minimap window. Increase its vertical size all the way up until it
	// hits flush the minimap window bottom border. Put around it a golden border").
	//
	// It used to be bottom-left anchored, capped at 540px, borderless, and it dodged whichever side
	// panels were open. Derived from GetMiniMapScreenRect() rather than from the minimap's nominal
	// 306x175, exactly as event_log.cpp does - the frame size is computed per zoom level and is not
	// a constant anyone should be restating.
	const Rectangle miniMap = GetMiniMapScreenRect();
	const int ceiling = miniMap.position.y + miniMap.size.height;

	const int x = miniMap.position.x;
	const int width = miniMap.size.width;
	int y = GetMainPanel().position.y - 13;

	// The frame sits outside the text, so the text column is inset by it on both sides.
	constexpr int FramePad = oracool::OrnateBorderWidth;
	const int textX = x + FramePad;
	const int textWidth = width - 2 * FramePad;
	if (textWidth < 100)
		return;

	for (PlayerMessage &message : Messages) {
		if (message.text.empty())
			break;
		if (!talkflag && SDL_GetTicks() - message.time >= 10000)
			break;

		std::string text = WordWrapString(message.text, textWidth);
		int chatlines = CountLinesOfText(text);
		const int blockHeight = message.lineHeight * chatlines;
		// Stop at the minimap rather than running under it. The list grows upward from the main
		// panel, so the oldest visible message is the one that would cross the line.
		if (y - blockHeight < ceiling)
			break;
		y -= blockHeight;

		const Rectangle block { { x, y }, { width, blockHeight } };
		DrawHalfTransparentRectTo(out, block.position.x, block.position.y, block.size.width, block.size.height);
		oracool::DrawOrnateBorder(out, block);
		DrawString(out, text, { { textX, y }, { textWidth, 0 } }, { message.style, 1, message.lineHeight });
		DrawString(out, message.from, { { textX, y }, { textWidth, 0 } }, { UiFlags::ColorWhitegold, 1, message.lineHeight });
	}
}

} // namespace devilution
