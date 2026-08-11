/**
 * @file control.h
 *
 * Interface of the character and main control panels
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

#include "DiabloUI/ui_flags.hpp"
#include "engine.h"
#include "engine/displacement.hpp"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "engine/size.hpp"
#include "panels/ui_panels.hpp"
#include "spelldat.h"
#include "spells.h"
#include "utils/attributes.h"
#include "utils/stdcompat/optional.hpp"
#include "utils/stdcompat/string_view.hpp"
#include "utils/string_or_view.hpp"
#include "utils/ui_fwd.h"

namespace devilution {

constexpr Size SidePanelSize { 320, 352 };

// Info box displacement of the top-left corner relative to GetMainPanel().position.
constexpr Displacement InfoBoxTopLeft { 177, 46 };
constexpr Size InfoBoxSize { 288, 64 };

extern bool DropGoldFlag;
extern bool chrbtn[4];
extern bool lvlbtndown;
extern bool chrbtnactive;
extern bool resetStatsButtonDown;
/** @brief Panel-relative position and size of the Reset Stats button - shared by charpanel.cpp's
 * drawing code and control.cpp's press/release hit-testing so the visual and clickable area can
 * never drift apart again (they briefly did, after OE-022 repositioned the button but missed
 * updating these two hardcoded hit-test rectangles). */
constexpr Point ResetStatsButtonPosition { 141, 246 };
constexpr Size ResetStatsButtonSize { 44, 24 };
extern UiFlags InfoColor;
extern int sbooktab;
extern bool talkflag;
extern bool sbookflag;
extern bool chrflag;
extern DVL_API_FOR_TEST StringOrView InfoString;
extern bool panelflag;
extern bool spselflag;
const Rectangle &GetMainPanel();
const Rectangle &GetLeftPanel();
const Rectangle &GetRightPanel();
bool IsLeftPanelOpen();
bool IsRightPanelOpen();

/**
 * @brief Which of the mutually exclusive left-hand panels is currently on screen.
 *
 * Oracool bug fix: user report - the waypoint list and the Character panel could both be "open",
 * and the renderer and the click handler each decided independently which one that meant. The
 * renderer showed the Character panel; the click handler routed the click to the waypoint list and
 * teleported the player. Every place that needs to know which panel is showing - the draw chain in
 * scrollrt.cpp's DrawAndBlit and the left-click chain in diablo.cpp's LeftMouseDown - must read it
 * from here rather than re-deriving it, or the two can disagree again.
 */
enum class LeftPanelContent : uint8_t {
	None,
	Character,
	QuestLog,
	Stash,
	WaypointMenu,
};
LeftPanelContent GetLeftPanelContent();

/**
 * @brief Whether @p position is over whichever right-hand panel is currently open.
 *
 * Oracool V1: the inventory has its own 320x660 top-right rect and no longer lives inside
 * RightPanel, so "is the cursor over the right-hand UI" stopped being a single rect test. Every
 * site that gates interaction on the right panel must go through this rather than
 * GetRightPanel().contains() - otherwise the inventory silently loses every click outside the old
 * 320x352 band, and those clicks fall through to the world (the character walks off, held items
 * drop on the ground).
 */
bool IsOverRightPanel(Point position);
extern std::optional<OwnedSurface> pBtmBuff;
extern OptionalOwnedClxSpriteList pGBoxBuff;

void CalculatePanelAreas();
bool IsChatAvailable();

/**
 * @brief Moves the mouse to the first attribute "+" button.
 */
void FocusOnCharInfo();
void OpenCharPanel();
void CloseCharPanel();
void ToggleCharPanel();

/**
 * @brief Check if the UI can cover the game area entierly
 */
inline bool CanPanelsCoverView()
{
	const Rectangle &mainPanel = GetMainPanel();
	return GetScreenWidth() <= mainPanel.size.width && GetScreenHeight() <= SidePanelSize.height + mainPanel.size.height;
}
void DrawSpellList(const Surface &out);
void SetSpell();
void SetSpeedSpell(size_t slot);
void ToggleSpell(size_t slot);

void AddPanelString(string_view str);
void AddPanelString(std::string &&str);
void DrawPanelBox(const Surface &out, SDL_Rect srcRect, Point targetPosition);
Point GetPanelPosition(UiPanels panel, Point offset = { 0, 0 });

/**
 * Controls drawing of current / max values (health, mana). Oracool: HUD art pass - the flask
 * rendering this used to accompany is gone (replaced by oracool/hud_art.cpp's orb compositions);
 * this text readout survives, drawn centered on each orb's sphere (see scrollrt.cpp).
 */
void DrawFlaskValues(const Surface &out, Point pos, int currValue, int maxValue);

/**
 * @brief calls on the active player object to update HP/Mana percentage variables
 *
 * This is used to ensure that DrawFlask routines display an accurate representation of the players health/mana
 *
 * @see Player::UpdateHitPointPercentage() and Player::UpdateManaPercentage()
 */
void control_update_life_mana();

/**
 * @brief draws the current right mouse button spell.
 * @param out screen buffer representing the main UI panel
 */
void DrawSpell(const Surface &out);

void InitControlPan();

/**
 * @brief Oracool: HUD overhaul - the 8 old panel buttons are gone (their actions moved to the
 * belt's Menu popup, oracool/hud_menu.h). This now only handles the RMB skill button
 * (readied-spell/speedbook slot) click - the one non-belt click target left on the main panel.
 */
void DoPanBtn();

void DoAutoMap();

/**
 * Checks the mouse cursor position within the control panel and sets information
 * strings if needed.
 */
void CheckPanelInfo();

void FreeControlPan();

/**
 * @brief Populates InfoString/InfoColor from whatever's currently hovered/held (item, monster,
 * NPC, player, gold). Every hover system already relies on this running each frame - see its
 * definition in control.cpp. Oracool: HUD overhaul - this used to also draw a fixed-position info
 * box; that half moved to oracool::DrawCursorTooltip (oracool/cursor_tooltip.h), which reads the
 * same InfoString/InfoColor this populates.
 */
void UpdateInfoString();
void CheckLvlBtn();
void ReleaseLvlBtn();
void DrawLevelUpIcon(const Surface &out);
void CheckChrBtns();
void ReleaseChrBtns(bool addAllStatPoints);
void DrawDurIcon(const Surface &out);
void RedBack(const Surface &out);
void DrawSpellBook(const Surface &out);
void DrawGoldSplit(const Surface &out);
void control_drop_gold(SDL_Keycode vkey);
void DrawTalkPan(const Surface &out);
bool control_check_talk_btn();
void control_release_talk_btn();
void control_type_message();
void control_reset_talk();
bool IsTalkActive();
bool HandleTalkTextInputEvent(const SDL_Event &event);
bool control_presskeys(SDL_Keycode vkey);
void DiabloHotkeyMsg(uint32_t dwMsg);
void OpenGoldDrop(int8_t invIndex, int max);
void CloseGoldDrop();
bool HandleGoldDropTextInputEvent(const SDL_Event &event);
extern Rectangle ChrBtnsRect[4];

} // namespace devilution
