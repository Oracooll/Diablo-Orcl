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
/** @brief Size of the Reset Stats button - shared by charpanel.cpp's drawing code and control.cpp's
 * press/release hit-testing so the visual and clickable area can never drift apart again (they
 * briefly did, after OE-022 repositioned the button but missed updating these two hardcoded
 * hit-test rectangles). Its POSITION now depends on measured text, so it comes from
 * GetResetStatsButtonPosition() in panels/charpanel.hpp rather than a constant here. */
constexpr Size ResetStatsButtonSize { 44, 24 };
extern UiFlags InfoColor;
extern bool talkflag;
extern DVL_API_FOR_TEST bool sbookflag;
extern bool chrflag;
/**
 * @brief The hover panel's text, with a plain assignment made SAFE.
 *
 * Oracool: crash fix (2026-09-03, user screenshot - the assert in cursor_tooltip.cpp fired while
 * moving an unsocketed ring back to the inventory). This was a bare StringOrView and the third
 * outing of one bug: assigning to it replaces the text while InfoStringLineColors keeps the PREVIOUS
 * hover's per-line colours, so a short string inherits a long string's colour list and the tooltip's
 * size check - which exists to catch exactly this - fires. It was fixed at the shrine call site in
 * August, at the gold one in August, and it came back here from a third.
 *
 * So the type carries the rule now instead of every caller remembering it: assignment clears the
 * colour list, which puts the text on the single-colour path with the live InfoColor - precisely
 * what a bare assignment has always meant. AddPanelString and SetPanelString keep the arrays in step
 * themselves and assign through AssignKeepingLineColors.
 *
 * Deliberately NOT recording InfoColor at assignment time: several callers set InfoColor after the
 * text (the player hover sets it before, the trigger strings never touch it), and a colour captured
 * here would be the one from before their change. An empty list defers that question to the draw,
 * which is where it was always answered.
 */
struct PanelInfoText {
	StringOrView text;

	PanelInfoText &operator=(StringOrView str);

	/** @brief Assigns without touching the colour arrays - for the two functions that own them. */
	void AssignKeepingLineColors(StringOrView str) { text = std::move(str); }

	[[nodiscard]] string_view str() const { return text.str(); }
	[[nodiscard]] bool empty() const { return text.empty(); }
};

extern DVL_API_FOR_TEST PanelInfoText InfoString;
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
	/** Phase 1: the crafting recipe window (oracool/crafting_menu.h). */
	Crafting,
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

/**
 * @brief Screen rect of whatever the left panel is currently showing, or an empty rect if nothing.
 *
 * NOT the same as GetLeftPanel(). That is the vanilla 320x352 slot; the character sheet, quest log
 * and waypoint list have each grown into their own 340x720 window. Anything routing or absorbing a
 * click over the left panel must use this, so that no part of an open window lets a click reach the
 * ground beneath it.
 */
Rectangle GetLeftPanelContentRect();

/** @brief Whether @p position is over the open left-panel window, per GetLeftPanelContentRect(). */
bool IsOverLeftPanel(Point position);

/**
 * @brief Whether @p position is over ANY interface surface: HUD chrome, either panel, or a
 * floating window.
 *
 * The single authority for "this click belongs to the interface, not to the world", and the answer
 * every input path should be asking.
 *
 * Audit finding, 2026-08-26. LeftMouseDown routes carefully through the pieces this composes;
 * RightMouseDown had a special case for the inventory and nothing else, so a right-click over the
 * character sheet, the quest log, the waypoint list, Levski's Roar, the runeword book or the lower
 * part of any 340x720 window fell straight through to CheckPlrSpell - casting Teleport, laying a
 * Fire Wall or walking the player, through a window that was plainly on top.
 *
 * Composed rather than rewritten: IsPointOverHudChrome, IsPointOverFloatingWindow, IsOverLeftPanel
 * and IsOverRightPanel each already existed and are each already the authority for their own
 * surface. What was missing was somewhere that asked all four.
 */
DVL_API_FOR_TEST bool IsOverAnyInterface(Point position);

/**
 * @brief Whether a numeric prompt owns input right now: drop gold, withdraw gold, Refresh Until.
 *
 * These three are MODAL - they take every keystroke, and ReleaseKey treats them that way - but no
 * mouse path knew about them (audit, 2026-08-26). Clicking anywhere outside their small panel
 * therefore reached the world: the character walked, attacked, cast, or opened a door behind a
 * prompt that was still waiting for a number.
 *
 * A position-free question on purpose. These are not surfaces to be over, they are owners of input,
 * so the answer must not depend on where the cursor happens to be.
 */
bool IsModalPromptOpen();

/**
 * @brief Closes whichever left-panel window is open, whatever it is.
 *
 * Oracool: the close-button rule (user, 2026-08-19) needs ONE place that knows how to shut the left
 * slot, for the same reason GetLeftPanelContentRect() exists - five windows share the slot, and
 * five hand-written closers would drift out of step with the switch above the way the draw and
 * click chains once did. A window added to that switch must be added here too.
 */
void CloseLeftPanelContent();

/**
 * @brief Closes every left-panel window except @p content, so opening it actually shows it.
 *
 * Call from an opener BEFORE it raises its own flag. Five windows share the slot and
 * GetLeftPanelContent picks one by a fixed precedence, so setting a flag is not the same as becoming
 * visible: with the character sheet up, opening the stash used to leave the stash "open" and
 * invisible, its clicks routed to the sheet, and it appeared only when the sheet was closed (user
 * report, 2026-08-31).
 *
 * Passing the content being opened rather than "close everything" keeps a re-open from tearing down
 * the window that is already there - CloseStash in particular returns a held item.
 */
void TakeLeftPanelSlot(LeftPanelContent content);
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

/**
 * @brief Appends a line to InfoString and records the colour it should be drawn in.
 *
 * Oracool V1: the hover panel used to draw the whole block in one colour (InfoColor), which is
 * fine for a one-line label and wrong for an item, where the name, its base stats, its affixes and
 * its requirements are different KINDS of information. This carries a colour per line alongside the
 * text, in InfoStringLineColors, so oracool::DrawCursorTooltip can render each line in its own.
 *
 * The uncoloured overloads above keep working and record white, so nothing outside the item path
 * needs touching.
 */
void AddPanelString(string_view str, UiFlags color);
void AddPanelString(std::string &&str, UiFlags color);

/** @brief Replaces InfoString with a single coloured line, resetting the per-line colours. */
void SetPanelString(StringOrView str, UiFlags color);

/** @brief Clears InfoString and its per-line colours together - they must not drift apart. */
void ClearPanelStrings();

/**
 * @brief One colour per LINE of InfoString, or empty when the hover did not supply any.
 *
 * Consumers MUST check that its size matches the line count before using it and fall back to
 * InfoColor otherwise: not every producer of InfoString goes through AddPanelString, so the two can
 * legitimately be out of step.
 */
extern DVL_API_FOR_TEST std::vector<UiFlags> InfoStringLineColors;

/**
 * @brief Where a line's WHITE tail starts, per line, or 0 for "the whole line is one colour".
 *
 * The tooltip draws one colour per line, which is all an item needed until the set panel: the user
 * asked (2026-08-16) for a set's item list to show each piece's name in green or red with its slot
 * "in brackets with white text", which is two colours on one row.
 *
 * A byte offset rather than a general run-list, because that is the whole requirement: one head, one
 * white tail. A line whose entry is 0 draws exactly as before, so every other producer is untouched.
 * Parallel to InfoStringLineColors and subject to the same size check.
 */
extern DVL_API_FOR_TEST std::vector<uint16_t> InfoStringLineTailStart;

/**
 * @brief A colour change INSIDE a line: from byte @p start on, the line is drawn in @p color.
 *
 * The general form of the tail above, for the one line that needs more than a head and a tail: an
 * item's "Required: 60 Str 20 Mag 25 Dex", where the user asked (2026-09-06) for each requirement
 * the character does not meet to be RED, "so it is easier to spot it" - and the unmet one can be
 * any of the three, or two of them, with met ones in between.
 */
struct PanelLineRun {
	uint16_t start;
	UiFlags color;
};

/**
 * @brief Per line, the colour runs after its opening colour, or an empty list for "no runs".
 *
 * Parallel to InfoStringLineColors and InfoStringLineTailStart, kept in step by the same functions,
 * subject to the same size check. A line with runs ignores its tail entry. Runs are in ascending
 * order of `start`; the text before the first run is drawn in the line's own colour.
 */
extern DVL_API_FOR_TEST std::vector<std::vector<PanelLineRun>> InfoStringLineRuns;

/**
 * @brief Appends a line drawn in two colours: @p str up to @p tailStart in @p color, the rest white.
 *
 * @p tailStart is a byte offset into @p str. Passing 0 is the same as the plain overload.
 */
void AddPanelStringSplit(std::string &&str, UiFlags color, size_t tailStart);

/**
 * @brief Appends ONE line drawn in @p color up to the first run, then in each run's colour from
 * its start. Runs must be in ascending order of start; an empty list is the plain overload.
 */
void AddPanelStringRuns(std::string &&str, UiFlags color, std::vector<PanelLineRun> runs);
void DrawPanelBox(const Surface &out, SDL_Rect srcRect, Point targetPosition);
Point GetPanelPosition(UiPanels panel, Point offset = { 0, 0 });

/**
 * Controls drawing of current / max values (health, mana). Oracool: HUD art pass - the flask
 * rendering this used to accompany is gone (replaced by oracool/hud_art.cpp's orb compositions);
 * this text readout survives, drawn centered on each orb's sphere (see scrollrt.cpp).
 */
void DrawFlaskValues(const Surface &out, Point pos, int currValue, int maxValue);
/** @brief The same in one fixed colour - the second pool of a split orb, which is never "wounded" red or "full" gold. */
void DrawFlaskValuesInColor(const Surface &out, Point pos, int currValue, int maxValue, UiFlags color);

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
/** @brief Oracool: the unspent skill pool in its LevelUpIconSize frame (ui\skill_points.png) above the RMB well. Hidden at zero. */
void DrawUnspentPointsFrame(const Surface &out);

/** @brief Whether the unspent skill-point frame is being drawn at all (nonzero pool, own player). */
bool IsUnspentPointsFrameVisible();

/** @brief Screen rect of that frame - shared by the draw and the click so they cannot drift apart. */
Rectangle GetUnspentPointsFrameRect();

/**
 * @brief Routes a click on the skill-point pool: opens the Abilities window. True when consumed.
 *
 * The frame sits ABOVE the HUD plate rather than on it, so - like the burger menu's icon row - it
 * is in neither the HUD branch nor the world branch of LeftMouseDown and has to be tested ahead of
 * both, or it renders, highlights and does nothing.
 */
bool CheckUnspentPointsFrameClick(Point position);
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
/** @brief The same amount prompt, for a stack that lives in the stash rather than the backpack. */
void OpenStashStackSplit(uint16_t stashIndex, int max);
void CloseGoldDrop();
bool HandleGoldDropTextInputEvent(const SDL_Event &event);
extern Rectangle ChrBtnsRect[4];

} // namespace devilution
