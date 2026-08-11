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

	if (IsChatAvailable()) {
		OptionalOwnedClxSpriteList talkButton = LoadClx("data\\talkbutton.clx");
		const int talkButtonWidth = (*talkButton)[0].width();

		constexpr size_t NumOtherPlayers = 3;
		// Render the unpressed voice buttons to pBtmBuff.
		string_view text = _("voice");
		const int textWidth = GetLineWidth(text, GameFont12, 1);
		for (size_t i = 0; i < NumOtherPlayers; ++i) {
			Point position { 176, static_cast<int>(GetMainPanel().size.height + 101 + 18 * i) };
			RenderClxSprite(*pBtmBuff, (*talkButton)[0], position);
			int width = std::min<int>(textWidth, (*PanelButton)[0].width());
			RenderClxSprite(pBtmBuff->subregion(position.x + (talkButtonWidth - width) / 2, position.y + 6, width, 9), (*PanelButtonGrime)[1], { 0, 0 });
			DrawButtonText(*pBtmBuff, text, { position, { talkButtonWidth, 0 } }, UiFlags::ColorButtonface);
		}

		const int talkButtonHeight = (*talkButton)[0].height();
		constexpr uint16_t NumTalkButtonSprites = 3;
		OwnedSurface talkSurface(talkButtonWidth, talkButtonHeight * NumTalkButtonSprites);

		// Prerender translated versions of the other button states for voice buttons
		RenderClxSprite(talkSurface, (*talkButton)[0], { 0, 0 });                    // background for unpressed mute button
		RenderClxSprite(talkSurface, (*talkButton)[1], { 0, talkButtonHeight });     // background for pressed mute button
		RenderClxSprite(talkSurface, (*talkButton)[1], { 0, talkButtonHeight * 2 }); // background for pressed voice button

		talkButton = std::nullopt;

		int muteWidth = GetLineWidth(_("mute"), GameFont12, 2);
		RenderClxSprite(talkSurface.subregion((talkButtonWidth - muteWidth) / 2, 6, muteWidth, 9), (*PanelButtonGrime)[1], { 0, 0 });
		DrawButtonText(talkSurface, _("mute"), { { 0, 0 }, { talkButtonWidth, 0 } }, UiFlags::ColorButtonface);
		RenderClxSprite(talkSurface.subregion((talkButtonWidth - muteWidth) / 2, 23, muteWidth, 9), (*PanelButtonGrime)[1], { 0, 0 });
		DrawButtonText(talkSurface, _("mute"), { { 0, 17 }, { talkButtonWidth, 0 } }, UiFlags::ColorButtonpushed);
		int voiceWidth = GetLineWidth(_("voice"), GameFont12, 2);
		RenderClxSprite(talkSurface.subregion((talkButtonWidth - voiceWidth) / 2, 39, voiceWidth, 9), (*PanelButtonGrime)[1], { 0, 0 });
		DrawButtonText(talkSurface, _("voice"), { { 0, 33 }, { talkButtonWidth, 0 } }, UiFlags::ColorButtonpushed);
		TalkButton = SurfaceToClx(talkSurface, NumTalkButtonSprites);
	}

	PanelButtonGrime = std::nullopt;
	PanelButton = std::nullopt;
}

void FreeMainPanel()
{
	TalkButton = std::nullopt;
}

} // namespace devilution
