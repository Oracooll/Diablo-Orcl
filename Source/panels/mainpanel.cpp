#include "panels/mainpanel.hpp"

#include <cstdint>

#include "control.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "utils/display.h"
#include "utils/language.h"
#include "utils/sdl_compat.h"
#include "utils/sdl_geometry.h"
#include "utils/stdcompat/optional.hpp"
#include "utils/surface_to_clx.hpp"

namespace devilution {

OptionalOwnedClxSpriteList TalkButton;

namespace {

OptionalOwnedClxSpriteList PanelButton;
OptionalOwnedClxSpriteList PanelButtonGrime;

void DrawButtonText(const Surface &out, string_view text, Rectangle placement, UiFlags style, int spacing = 1)
{
	DrawString(out, text, { placement.position + Displacement { 0, 1 }, placement.size }, { UiFlags::AlignCenter | UiFlags::KerningFitSpacing | UiFlags::ColorBlack, spacing });
	DrawString(out, text, placement, { UiFlags::AlignCenter | UiFlags::KerningFitSpacing | style, spacing });
}

} // namespace

// Oracool: HUD overhaul - the 6 labeled panel buttons this used to bake into pBtmBuff (char/
// quests/map/menu/inv/spells, plus their pressed-state sprite sheet PanelButtonDown) are gone;
// their actions live in the belt's Menu popup (oracool/hud_menu.h). Only the multiplayer chat
// panel's voice/mute buttons are still assembled here.
void LoadMainPanel()
{
	PanelButton = LoadOptionalClx("data\\panel8buc.clx");
	PanelButtonGrime = LoadOptionalClx("data\\dirtybuc.clx");

	// Oracool (2026-08-18): the VOICE/MUTE button bake is gone.
	//
	// This rendered three whisper toggles into pBtmBuff and pre-rendered their translated pressed
	// states into TalkButton, for the chat panel's lower plate. That plate is no longer drawn - the
	// Enter box is now a plain bordered rectangle of our own - so the bake was painting buttons onto
	// a region nothing blits, and loading data\talkbutton.clx to do it.
	//
	// TalkButton itself stays declared and freed above/below, at std::nullopt, so anything that
	// still tests it sees "no art" rather than a dangling reference.
	PanelButtonGrime = std::nullopt;
	PanelButton = std::nullopt;
}

void FreeMainPanel()
{
	TalkButton = std::nullopt;
}

} // namespace devilution
