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
#include "quests.h" // QuestLogIsOpen - the history shares the panels' corner
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

	// The history lives in the minimap's column now, and every 340-wide side panel is drawn in that
	// same corner - the inventory is flush top-right at 340x720, with the minimap inside its
	// footprint. DrawPlrMsg runs after DrawInv, so without this the messages paint straight over an
	// open panel (audit, 2026-08-18).
	//
	// Suppressed rather than moved: the old bottom-left placement dodged panels by shrinking, which
	// is what a full-width strip could do and a fixed column cannot. While chatting the panels are
	// closed anyway, so this costs nothing at the moment the history matters most.
	if (invflag || sbookflag || chrflag || QuestLogIsOpen || IsStashOpen)
		return;

	// ONE window, the event log's own rect, with ONE frame around it - not a frame per message
	// (user, 2026-08-18: "make the Message History window the size of Log, and don't put golden
	// border around every message. Put it around just the window itself").
	//
	// The geometry mirrors event_log.cpp's WindowTopLeftBelowMiniMap/WindowWidth/WindowHeight rather
	// than inventing its own, so the history and the log occupy exactly the same column - they are
	// alternatives for that space, and the log steps aside while chat is open.
	//
	// All of it derived from GetMiniMapScreenRect(): the minimap's frame is computed per zoom level,
	// so its nominal 306x175 is not a constant anyone should restate.
	const Rectangle miniMap = GetMiniMapScreenRect();
	const int windowTop = miniMap.position.y + miniMap.size.height + 1;
	const Rectangle window { { miniMap.position.x, windowTop },
		{ miniMap.size.width, gnScreenHeight - miniMap.position.y - windowTop } };
	if (window.size.height <= 0)
		return;

	// The frame sits inside the window's own bounds, so the text column is inset by it plus a little
	// air on each side.
	constexpr int FramePad = oracool::OrnateBorderWidth;
	constexpr int TextPad = FramePad + 4;
	const int textX = window.position.x + TextPad;
	const int textWidth = window.size.width - 2 * TextPad;
	if (textWidth < 100)
		return;

	// Nothing to say means no window at all - an empty framed box sitting under the minimap would
	// read as a broken panel rather than as a quiet one.
	const bool anyVisible = !Messages[0].text.empty()
	    && (talkflag || SDL_GetTicks() - Messages[0].time < 10000);
	if (!anyVisible)
		return;

	DrawHalfTransparentRectTo(out, window.position.x, window.position.y, window.size.width, window.size.height);
	oracool::DrawOrnateBorder(out, window);

	// Newest at the TOP, filling downward (user, 2026-08-18: "make Message History window populate
	// top to bottom, not bottom to top"). Messages already arrive newest-first in the array, so this
	// is simply reading it in order - and it puts the line you just triggered where the eye starts
	// rather than at the far end of a growing column.
	int y = window.position.y + TextPad;
	for (PlayerMessage &message : Messages) {
		if (message.text.empty())
			break;
		if (!talkflag && SDL_GetTicks() - message.time >= 10000)
			break;

		std::string text = WordWrapString(message.text, textWidth);
		int chatlines = CountLinesOfText(text);
		const int blockHeight = message.lineHeight * chatlines;
		if (y + blockHeight > window.position.y + window.size.height - TextPad)
			break; // the window is full; older lines simply do not fit

		DrawString(out, text, { { textX, y }, { textWidth, 0 } }, { message.style, 1, message.lineHeight });
		DrawString(out, message.from, { { textX, y }, { textWidth, 0 } }, { UiFlags::ColorWhitegold, 1, message.lineHeight });
		y += blockHeight;
	}
}

} // namespace devilution
