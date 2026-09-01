/**
 * @file diablo.cpp
 *
 * Implementation of the main game initialization functions.
 */
#include <array>
#include <cstdint>

#include <fmt/format.h>

#include <config.h>

#include "DiabloUI/selstart.h"
#include "automap.h"
#include "capture.h"
#include "cursor.h"
#include "dead.h"
#ifdef _DEBUG
#include "debug.h"
#endif
#include "DiabloUI/diabloui.h"
#include "controls/devices/kbcontroller.h"
#include "controls/plrctrls.h"
#include "controls/remap_keyboard.h"
#include "diablo.h"
#include "discord/discord.h"
#include "doom.h"
#include "encrypt.h"
#include "engine/backbuffer_state.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/demomode.h"
#include "engine/dx.h"
#include "engine/events.hpp"
#include "engine/load_cel.hpp"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/sound.h"
#include "error.h"
#include "gamemenu.h"
#include "gmenu.h"
#include "help.h"
#include "hwcursor.hpp"
#include "init.h"
#include "levels/drlg_l1.h"
#include "levels/drlg_l2.h"
#include "levels/drlg_l3.h"
#include "levels/drlg_l4.h"
#include "levels/gendung.h"
#include "levels/setmaps.h"
#include "levels/themes.h"
#include "levels/town.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "loadsave.h"
#include "menu.h"
#include "minitext.h"
#include "missiles.h"
#include "movie.h"
#include "multi.h"
#include "nthread.h"
#include "objects.h"
#include "oracool/attack_skills.h"
#include "oracool/shutdown_watchdog.h"
#include "oracool/skill_picker.h"
#include "oracool/auto_save.h"
#include "oracool/gradual_healing.h"
#include "oracool/furious_charge.h"
#include "oracool/game_speed.h"
#include "oracool/event_log.h"
#include "oracool/skill_sounds.h"
#include "oracool/hud_layout.h"
#include "oracool/crafting_menu.h"
#include "oracool/hud_menu.h"
#include "oracool/levski_roar.h"
#include "oracool/runeword_book.h"
#include "oracool/run_toggle.h"
#include "oracool/shop_grid.h"
#include "oracool/paladin_melee.h"
#include "oracool/inventory_layout.h"
#include "oracool/oracool.h"
#include "oracool/waypoint_menu.h"
#include "oracool/xp_counter.h"
#include "options.h"
#include "panels/charpanel.hpp" // ScrollCharacterSheet
#include "panels/info_box.hpp"
#include "panels/spell_book.hpp"
#include "oracool/window_close.h"
#include "panels/spell_list.hpp"
#include "pfile.h"
#include "plrmsg.h"
#include "qol/chatlog.h"
#include "qol/floatingnumbers.h"
#include "qol/itemlabels.h"
#include "qol/monhealthbar.h"
#include "qol/stash.h"
#include "qol/xpbar.h"
#include "restrict.h"
#include "stores.h"
#include "storm/storm_net.hpp"
#include "storm/storm_svid.h"
#include "towners.h"
#include "track.h"
#include "utils/console.h"
#include "utils/display.h"
#include "utils/language.h"
#include "utils/paths.h"
#include "utils/stdcompat/string_view.hpp"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

#ifndef USE_SDL1
#include "controls/touch/gamepad.h"
#include "controls/touch/renderers.h"
#endif

#ifdef __vita__
#include "platform/vita/touch.h"
#endif

#ifdef GPERF_HEAP_FIRST_GAME_ITERATION
#include <gperftools/heap-profiler.h>
#endif

namespace devilution {

uint32_t glSeedTbl[NUMLEVELS];
Point MousePosition;
bool gbRunGame;
bool gbRunGameResult;
bool ReturnToMainMenu;
/** Enable updating of player character, set to false once Diablo dies */
bool gbProcessPlayers;
bool gbLoadGame;
bool cineflag;
int PauseMode;
bool gbBard;
bool gbBarbarian;
bool HeadlessMode = false;
clicktype sgbMouseDown;
uint16_t gnTickDelay = 50;
char gszProductName[128] = "DevilutionX vUnknown";
char gszMainMenuVersionText[192] = "";

#ifdef _DEBUG
bool DebugDisableNetworkTimeout = false;
std::vector<std::string> DebugCmdsFromCommandLine;
#endif
GameLogicStep gGameLogicStep = GameLogicStep::None;
QuickMessage QuickMessages[QUICK_MESSAGE_OPTIONS] = {
	{ "QuickMessage1", N_("I need help! Come Here!") },
	{ "QuickMessage2", N_("Follow me.") },
	{ "QuickMessage3", N_("Here's something for you.") },
	{ "QuickMessage4", N_("Now you DIE!") }
};

/** This and the following mouse variables are for handling in-game click-and-hold actions */
MouseActionType LastMouseButtonAction = MouseActionType::None;
SpellID LastMouseButtonSpell = SpellID::Invalid;
SpellType LastMouseButtonSpellType = SpellType::Invalid;

// Controller support: Actions to run after updating the cursor state.
// Defined in SourceX/controls/plctrls.cpp.
extern void plrctrls_after_check_curs_move();
extern void plrctrls_every_frame();
extern void plrctrls_after_game_logic();

namespace {

char gszVersionNumber[64] = "internal version unknown";

bool gbGameLoopStartup;
bool forceSpawn;
bool forceDiablo;
int sgnTimeoutCurs;
bool gbShowIntro = true;
/** To know if these things have been done when we get to the diablo_deinit() function */
bool was_archives_init = false;
/** To know if surfaces have been initialized or not */
bool was_window_init = false;
bool was_ui_init = false;

void StartGame(interface_mode uMsg)
{
	if (uMsg == WM_DIABNEWGAME || uMsg == WM_DIABLOADGAME) {
		oracool::ResetAutoSave();
		// Same reason as the autosave clock: session state must not leak from the previous hero.
		oracool::ResetGradualHealing();
	}
	CalcViewportGeometry();
	cineflag = false;
	InitCursor();
#ifdef _DEBUG
	LoadDebugGFX();
#endif
	assert(HeadlessMode || ghMainWnd);
	music_stop();
	InitMonsterHealthBar();
	InitXPBar();
	ShowProgress(uMsg);
	gmenu_init_menu();
	InitLevelCursor();
	sgnTimeoutCurs = CURSOR_NONE;
	sgbMouseDown = CLICK_NONE;
	LastMouseButtonAction = MouseActionType::None;
	LastMouseButtonSpell = SpellID::Invalid;
	LastMouseButtonSpellType = SpellType::Invalid;
}

void FreeGame()
{
	// Before anything else tears down: a looping aura owns a sound handle, and leaving the game is
	// one of the transitions the sound package's contract requires it released on. Silenced rather
	// than stopped, because the stop CUE would be a sound the player has no cause for - they left,
	// they did not switch the aura off.
	oracool::SilenceAuraLoopForTransition();
	// And the set-completion baseline describes a character who is no longer here.
	oracool::ResetSetCompletionBaseline();
	// So does anything left in the monument's grid. Both it and the window's open flag are statics
	// that outlive the GAME rather than the process, and nothing on the way out of a game closes
	// windows - so without this the next character started in the same session opened onto this
	// window, already up, holding the previous character's items (audit, 2026-08-30).
	oracool::ResetLevskiRoarForNewGame();
	// The log is the same shape of problem without the item duplication: its entries are a
	// file-local deque, so the next character opened it onto the previous one's kills and crafts.
	oracool::ClearEventLogForNewGame();
	// And the rest of the fork's windows, swept for the same fault (audit, 2026-08-31). Their open
	// flags are file-local statics too, and nothing on the way out of a game closes them - so the
	// next character in the same session started with whatever the last one left up. Cosmetic for
	// three of them; the waypoint menu also carries an unconsumed spawn request, which would move
	// the next character onto a waypoint on their first level load.
	oracool::ResetWaypointMenuForNewGame();
	// Furious Charge's dash and cooldown are the same shape, keyed to SDL_GetTicks: quit inside the
	// three seconds after a charge and the next character's icon starts half-filled and refuses the
	// skill. Bounded by its own timer rather than permanent, so this is hardening (audit, 2026-08-31).
	oracool::ResetFuriousChargeForNewGame();
	oracool::CloseCraftingMenu();
	oracool::CloseHudMenu();
	oracool::CloseSkillPicker();
	// And Zeal's burst holds raw Monster POINTERS into the monster array this teardown is about to
	// invalidate. StartStand already resets the chain on every interruption a game can produce, and
	// a new game reaches it long before the player can swing - so this is hardening rather than a
	// reported fault. It costs one call to make a dangling-pointer class impossible by construction
	// instead of merely unreachable.
	oracool::ResetZealChain();

	FreeMonsterHealthBar();
	FreeXPBar();
	FreeControlPan();
	FreeInvGFX();
	FreeGMenu();
	FreeQuestText();
	FreeInfoBoxGfx();
	FreeStoreMem();

	for (Player &player : Players)
		ResetPlayerGFX(player);

	FreeCursor();
#ifdef _DEBUG
	FreeDebugGFX();
#endif
	FreeGameMem();
	stream_stop();
	music_stop();
}

bool ProcessInput()
{
	if (PauseMode == 2) {
		return false;
	}

	plrctrls_every_frame();

	if (!gbIsMultiplayer && gmenu_is_active()) {
		RedrawViewport();
		return false;
	}

	if (!gmenu_is_active() && sgnTimeoutCurs == CURSOR_NONE) {
#ifdef __vita__
		FinishSimulatedMouseClicks(MousePosition);
#endif
		CheckCursMove();
		plrctrls_after_check_curs_move();
		RepeatMouseAction();
	}

	return true;
}

/**
 * @brief Whether the cursor is over something the world lets you ACT on, rather than fight.
 *
 * Oracool: user report (2026-08-15) - a skill readied on the left button made items, doors, shrines
 * and bookstands unclickable, because the skill dispatch skipped LeftMouseCmd and LeftMouseCmd is
 * where all of them are handled. This is the test that lets interaction win.
 *
 * Monsters are deliberately absent: attacking one IS the interaction there, and that is what the
 * readied skill is for. Townspeople are present - pcursmonst covers them too, so the town check is
 * what separates "talk to Griswold" from "hit the thing in front of me".
 */
/**
 * @brief Whether the cursor is over something the left button should ATTACK.
 *
 * The left button's readied spell fires on this and nothing else (user report, 2026-08-20: "only
 * fire when over enemy. now they also fire when over ground. makes me not able to move"). The old
 * rule was the inverse - cast unless something INTERACTABLE was under the cursor - and bare ground
 * is not interactable, so every click on the floor was a cast and walking became impossible with a
 * spell readied.
 *
 * Townspeople are excluded: in town every "monster" is a conversation, not a target.
 */
bool IsEnemyUnderCursor()
{
	return leveltype != DTYPE_TOWN && pcursmonst != -1;
}

void LeftMouseCmd(bool bShift)
{
	bool bNear;

	// Oracool: this is the plain-attack path - reached when no skill is readied on the left button,
	// or when shift forces the swing - so whatever skill an earlier click armed does not apply to
	// what happens next. Cleared here rather than at every attack site because this is the one that
	// means "no skill"; the controller and hold-to-attack repeat paths deliberately leave the latch
	// alone, since you are still holding the same button. See oracool/paladin_melee.h.
	oracool::ArmMeleeSkill(std::nullopt);

	// Oracool: bug postmortem (2026-08-11) - this used to assert that the click was outside
	// GetMainPanel(), which held while the old 640x128 panel swallowed every click inside its rect.
	// The new HUD is a small centre-bottom plate, and clicks in the empty space around it are meant
	// to reach the world (user request) - so that invariant is gone, and in a Debug build the stale
	// assert aborted the game the moment the player clicked anywhere in that band. Removed rather
	// than relaxed: the caller (LeftMouseDown) already decides what counts as HUD, and CheckCursMove
	// computes cursPosition for panel-area coordinates too, so the target tile here is valid.

	if (leveltype == DTYPE_TOWN) {
		CloseGoldWithdraw();
		CloseStash();
		if (pcursitem != -1 && pcurs == CURSOR_HAND)
			NetSendCmdLocParam1(true, invflag ? CMD_GOTOGETITEM : CMD_GOTOAGETITEM, cursPosition, pcursitem);
		if (pcursmonst != -1)
			NetSendCmdLocParam1(true, CMD_TALKXY, cursPosition, pcursmonst);
		// Oracool: user request - Stash Chest. Vanilla town never needed to click a generic Object
		// to operate it (NPCs go through pcursmonst/CMD_TALKXY just above; nothing else in vanilla
		// town used the Object system's click-to-operate path), so this branch never checked
		// ObjectUnderCursor at all - a click on the chest fell straight through to the walk-there
		// branch below and silently never opened it. Mirrors the equivalent check in the dungeon
		// branch further down this function.
		if (pcursitem == -1 && pcursmonst == -1 && ObjectUnderCursor != nullptr && !ObjectUnderCursor->IsDisabled()) {
			LastMouseButtonAction = MouseActionType::OperateObject;
			NetSendCmdLoc(MyPlayerId, true, CMD_OPOBJXY, cursPosition);
			return;
		}
		if (pcursitem == -1 && pcursmonst == -1 && pcursplr == -1) {
			LastMouseButtonAction = MouseActionType::Walk;
			NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
		}
		return;
	}

	Player &myPlayer = *MyPlayer;
	bNear = myPlayer.position.tile.WalkingDistance(cursPosition) < 2;
	if (pcursitem != -1 && pcurs == CURSOR_HAND && !bShift) {
		NetSendCmdLocParam1(true, invflag ? CMD_GOTOGETITEM : CMD_GOTOAGETITEM, cursPosition, pcursitem);
	} else if (ObjectUnderCursor != nullptr && !ObjectUnderCursor->IsDisabled() && (!bShift || (bNear && ObjectUnderCursor->_oBreak == 1))) {
		LastMouseButtonAction = MouseActionType::OperateObject;
		NetSendCmdLoc(MyPlayerId, true, pcurs == CURSOR_DISARM ? CMD_DISARMXY : CMD_OPOBJXY, cursPosition);
	} else if (myPlayer.UsesRangedWeapon()) {
		if (bShift) {
			LastMouseButtonAction = MouseActionType::Attack;
			NetSendCmdLoc(MyPlayerId, true, CMD_RATTACKXY, cursPosition);
		} else if (pcursmonst != -1) {
			if (CanTalkToMonst(Monsters[pcursmonst])) {
				NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
			} else {
				LastMouseButtonAction = MouseActionType::AttackMonsterTarget;
				NetSendCmdParam1(true, CMD_RATTACKID, pcursmonst);
			}
		} else if (pcursplr != -1 && !myPlayer.friendlyMode) {
			LastMouseButtonAction = MouseActionType::AttackPlayerTarget;
			NetSendCmdParam1(true, CMD_RATTACKPID, pcursplr);
		}
	} else {
		if (bShift) {
			if (pcursmonst != -1) {
				if (CanTalkToMonst(Monsters[pcursmonst])) {
					NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
				} else {
					LastMouseButtonAction = MouseActionType::Attack;
					NetSendCmdLoc(MyPlayerId, true, CMD_SATTACKXY, cursPosition);
				}
			} else {
				LastMouseButtonAction = MouseActionType::Attack;
				NetSendCmdLoc(MyPlayerId, true, CMD_SATTACKXY, cursPosition);
			}
		} else if (pcursmonst != -1) {
			LastMouseButtonAction = MouseActionType::AttackMonsterTarget;
			NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
		} else if (pcursplr != -1 && !myPlayer.friendlyMode) {
			LastMouseButtonAction = MouseActionType::AttackPlayerTarget;
			NetSendCmdParam1(true, CMD_ATTACKPID, pcursplr);
		}
	}
	if (!bShift && pcursitem == -1 && ObjectUnderCursor == nullptr && pcursmonst == -1 && pcursplr == -1) {
		LastMouseButtonAction = MouseActionType::Walk;
		NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
	}
}

bool TryOpenDungeonWithMouse()
{
	if (leveltype != DTYPE_TOWN)
		return false;

	Item &holdItem = MyPlayer->HoldItem;
	if (holdItem.IDidx == IDI_RUNEBOMB && OpensHive(cursPosition))
		OpenHive();
	else if (holdItem.IDidx == IDI_MAPOFDOOM && OpensGrave(cursPosition))
		OpenGrave();
	else
		return false;

	NewCursor(CURSOR_HAND);
	return true;
}

void LeftMouseDown(uint16_t modState)
{
	LastMouseButtonAction = MouseActionType::None;
	LastMouseButtonSpell = SpellID::Invalid;
	LastMouseButtonSpellType = SpellType::Invalid;

	// A numeric prompt owns input while it is open - drop gold, withdraw gold, Refresh Until.
	// ReleaseKey has always treated them as modal; no mouse path did, so a click outside their
	// panel walked the player or swung at something behind the prompt (audit, 2026-08-26).
	//
	// Swallowed rather than routed: these three have no mouse handling of their own, so there
	// is nothing to forward a click TO. Escape and Enter are how they close.
	if (IsModalPromptOpen())
		return;

	if (gmenu_left_mouse(true))
		return;

	// The basic-attack quick list eats the click while it is showing - on an entry it readies the
	// attack, anywhere else it just dismisses. Ahead of everything else for the same reason every
	// popup is: a strip floating over the world must not let clicks through to the world.
	if (oracool::CheckSkillPickerClick(MousePosition))
		return;

	if (control_check_talk_btn())
		return;

	if (sgnTimeoutCurs != CURSOR_NONE)
		return;

	if (MyPlayerIsDead) {
		// Oracool: HUD overhaul - the old dead-mode panel buttons (Game Menu, Chat) are gone; the
		// belt's Menu popup covers both, so it stays clickable while dead.
		if (oracool::IsHudMenuOpen())
			oracool::CheckHudMenuClick(MousePosition);
		else
			oracool::CheckHudMenuSlotClick(MousePosition);
		return;
	}

	if (PauseMode == 2) {
		return;
	}
	if (DoomFlag) {
		doom_close();
		return;
	}

	if (spselflag) {
		SetSpell();
		return;
	}

	if (stextflag != TalkID::None) {
		// A shop grid is a PANEL, not a modal screen. The inventory is open beside it so items can
		// be dragged across to sell, repair and recharge, so only clicks that land on the shop
		// itself belong to the store - the rest fall through to the inventory routing below, which
		// is what lets the player pick an item up in the first place. Every other store screen
		// (the towner dialogs, Confirm, No money) still swallows the whole screen, as it always did.
		if (!oracool::IsShopGridScreen(stextflag) || oracool::IsPointOverShop(MousePosition)) {
			CheckStoreBtn();
			return;
		}
	}

	// Oracool: the XP Counter is always-visible during normal gameplay (like the mini-map),
	// independent of which panel is open, so it's checked here rather than inside the
	// panel-state-gated branches below.
	if (oracool::CheckXpCounterButtonClick(MousePosition))
		return;

	// Oracool: bug postmortem (2026-08-11) - the burger menu's icon row has to be tested here,
	// ahead of the HUD/world split below. The row counts as HUD, so the world branch (which owns
	// the "click away to close" case) never sees it; but it sits above the plate rather than on
	// it, so the HUD branch did not see it either. The icons rendered and highlighted correctly
	// and were simply inert, because their click handler lived in the branch they never reached.
	if (oracool::IsPointOverHudMenu(MousePosition)) {
		oracool::CheckHudMenuClick(MousePosition);
		return;
	}

	// The skill-point pool, tested here for exactly the reason the burger menu above is: it is
	// drawn above the plate rather than on it, so IsPointOverHudChrome says no and the world branch
	// would walk the character instead (user, 2026-08-30: "clicking skill points button to open
	// abilities screen").
	if (CheckUnspentPointsFrameClick(MousePosition))
		return;

	const bool isShiftHeld = (modState & KMOD_SHIFT) != 0;
	const bool isCtrlHeld = (modState & KMOD_CTRL) != 0;

	// Oracool: user request (2026-08-11) - the old 640x128 main panel used to swallow every click
	// inside its rect, but the new HUD occupies only a small plate at the centre-bottom. Everything
	// else in that rect - the gaps either side of the plate, the strip above the XP counter - is
	// now empty screen and must behave like it: clicking there walks the player, exactly as
	// clicking anywhere else in the world does. Only the plate itself, the XP counter's strip, and
	// (while open) the chat panel still absorb clicks as UI.
	//
	// The test moved into hud_layout.cpp on 2026-08-15 because it turned out not to be the only one:
	// CheckPlrSpell kept a second, stale idea of where the UI is, and the disagreement made every
	// skill click in the lower band do nothing. See IsPointOverHudChrome.
	const bool isOverHud = oracool::IsPointOverHudChrome(MousePosition);

	// Levski's Roar is a free-floating window rather than a left-panel slot, so it is routed
	// before the panel switch and claims the click itself. Returning early is what stops a click
	// over the monument's grid from also walking the player toward it.
	// The runeword book is routed FIRST of the free-floating windows: it is the widest thing on
	// screen and it consumes every click inside its own rect, so anything drawn under it must not
	// see the click either.
	if (oracool::HandleRunewordBookClick(MousePosition))
		return;

	if (oracool::CheckLevskiRoarClick(MousePosition, isCtrlHeld))
		return;


	if (!isOverHud) {
		if (!gmenu_is_active() && !TryIconCurs()) {
			// Oracool bug fix: user report - one click, routed by whichever left-hand panel is
			// actually on screen. This used to be four separate `else if`s in an order of their
			// own, which disagreed with the renderer's: with both the waypoint list and the
			// Character panel open, the Character panel was drawn but the click went to the
			// waypoint list and teleported the player. GetLeftPanelContent is now the single
			// authority, shared with scrollrt.cpp's draw chain.
			//
			// The rect comes from GetLeftPanelContentRect(), not GetLeftPanel(): the sheet, log
			// and waypoint list are 340x720 windows in a 320x352 slot, and routing on the slot let
			// clicks over the rest of the window walk the player.
			// The close-button rule (user, 2026-08-19). Tested ahead of every window's own click
			// handling, because the X is the one control that must never be shadowed by whatever
			// happens to sit under it - a scroll arrow, a tab, a grid cell.
			if (IsLeftPanelOpen() && oracool::CheckWindowCloseButtonClick(GetLeftPanelContentRect(), MousePosition)) {
				CloseLeftPanelContent();
			} else if (invflag && oracool::CheckWindowCloseButtonClick(oracool::GetInventoryPanelRect(), MousePosition)) {
				CloseInventory();
			} else if (sbookflag && oracool::CheckWindowCloseButtonClick(GetSpellBookPanelRect(), MousePosition)) {
				sbookflag = false;
			} else if (oracool::IsEventLogOpen()
			    && oracool::CheckWindowCloseButtonClick(oracool::GetEventLogWindowRect(), MousePosition)) {
				// The log got its X in v1.9.147 (audit). It is a floating window the player opens, so
				// the close-button rule covers it - it had been closable only by pressing its key
				// again. Routed here with the other three rather than in a handler of its own,
				// because this block is where the X is deliberately tested ahead of everything else.
				oracool::ToggleEventLog();
			} else if (IsOverLeftPanel(MousePosition)) {
				switch (GetLeftPanelContent()) {
				case LeftPanelContent::Character:
					CheckChrBtns();
					break;
				case LeftPanelContent::QuestLog:
					QuestlogESC();
					break;
				case LeftPanelContent::Stash:
					if (!IsWithdrawGoldOpen)
						CheckStashItem(MousePosition, isShiftHeld, isCtrlHeld);
					CheckStashButtonPress(MousePosition);
					break;
				case LeftPanelContent::WaypointMenu:
					oracool::CheckWaypointMenuClick(MousePosition);
					break;
				case LeftPanelContent::Crafting:
					oracool::CheckCraftingMenuClick(MousePosition);
					break;
				case LeftPanelContent::None:
					break;
				}
			} else if (oracool::IsHudMenuOpen()) {
				oracool::CheckHudMenuClick(MousePosition);
			} else if (qtextflag) {
				qtextflag = false;
				stream_stop();
			} else if (invflag && oracool::GetInventoryPanelRect().contains(MousePosition)) {
				if (!DropGoldFlag)
					CheckInvItem(isShiftHeld, isCtrlHeld);
			} else if (sbookflag && GetSpellBookPanelRect().contains(MousePosition)) {
				CheckSBook(/*assignToRightButton=*/false);
			} else if (!MyPlayer->HoldItem.isEmpty()) {
				if (!TryOpenDungeonWithMouse()) {
					Point currentPosition = MyPlayer->position.tile;
					std::optional<Point> itemTile = FindAdjacentPositionForItem(currentPosition, GetDirection(currentPosition, cursPosition));
					if (itemTile) {
						NetSendCmdPItem(true, CMD_PUTITEM, *itemTile, MyPlayer->HoldItem);
						NewCursor(CURSOR_HAND);
					}
				}
			} else {
				CheckLvlBtn();
				if (!lvlbtndown) {
					// Oracool: user request (2026-08-15) - a spell assigned to the LEFT button casts
					// instead of swinging. Routed through the same CheckPlrSpell the right button
					// uses, just with the other pair of fields, so casting rules, mana, targeting
					// and the Charge intercept all behave identically on both buttons.
					//
					// INTERACTION WINS (user report, 2026-08-15: "Left Click to always interact with
					// interact-able objects [...] Currently i cant interact with anything if i have a
					// skill on LMB"). The world's own affordances - an item to pick up, a door, a
					// shrine, a bookstand, a townsperson to talk to - are not things a readied skill
					// should be able to switch off, and this dispatch used to switch all of them off
					// at once by skipping LeftMouseCmd, which is where every one of them is handled.
					// A MONSTER under the cursor is deliberately not in that set: attacking it IS the
					// interaction, and that is what the skill is for.
					// isShiftHeld, not a hardcoded false. It was false here, which is why the user's
					// report that "shift left click still moved my hero" was correct: the left
					// button could never see the modifier at all, so CheckPlrSpell's shift handling
					// - the whole point of the previous change - was unreachable from this side.
					//
					// And with shift held, interaction does NOT win (self-audit, 2026-08-15). The
					// interaction-wins rule exists so a readied skill cannot switch off the world's
					// affordances on a PLAIN click - but shift's meaning is "cast, no matter what",
					// and vanilla's own shift-click already ignores items and objects to swing in
					// place. Routing shift+click over an item to LeftMouseCmd made it the one spot
					// on the screen where shift quietly stopped casting.
					// AN ENEMY, OR SHIFT. Nothing else casts from the left button.
					//
					// This gate used to be the inverse - cast unless something interactable was
					// under the cursor - which meant bare ground cast, because ground is not
					// interactable. With a spell readied you could not walk (user report,
					// 2026-08-20: "makes me not able to move"). Movement is the left button's
					// baseline job and a readied spell must not be able to take it away.
					//
					// The interaction-wins rule from 2026-08-15 survives as a consequence rather
					// than as a clause: an item, a door, a shrine or a townsperson is not an enemy,
					// so a plain click on one falls through to LeftMouseCmd exactly as it did.
					//
					// Shift still means "cast, no matter what", and it is now the ONLY way to aim a
					// ground-targeted spell - Fire Wall, Teleport, Blessed Hammer - from this
					// button. That is the trade the report asks for: "they only need to fire if i
					// hold shift".
					Player &lmbPlayer = *MyPlayer;
					if (IsValidSpell(lmbPlayer._pLRSpell) && (isShiftHeld || IsEnemyUnderCursor()))
						CheckPlrSpell(isShiftHeld, lmbPlayer._pLRSpell, lmbPlayer._pLRSplType);
					else
						LeftMouseCmd(isShiftHeld);
				}
			}
		}
	} else {
		if (oracool::CheckHudMenuSlotClick(MousePosition) || oracool::CheckTownPortalBeltSlotClick(MousePosition))
			return;
		// Oracool: user request - either skill button opens the Abilities window, which is where
		// spells, skills and auras are now chosen. The LMB well was purely decorative before this.
		//
		// Only the LMB well is claimed here. The RMB well is left to DoPanBtn below, which already
		// opens the chooser AND handles shift-click-to-clear the readied spell; intercepting it
		// here would run before DoPanBtn and silently kill that shortcut.
		if (oracool::GetLmbSkillButtonRect().contains(MousePosition)) {
			// Shift-click clears the left button, exactly as it does on the RMB well (DoPanBtn).
			//
			// Added 2026-08-16 with the Skills sheet's removal. That sheet's two basic-attack rows
			// were the ONLY way back to a plain left click once a skill had been put there - clicking
			// either one cleared _pLRSpell - so deleting the sheet without this would have made a
			// left-button assignment permanent. The right button never had that problem, because it
			// has had this shortcut since the HUD overhaul.
			if (isShiftHeld) {
				MyPlayer->_pLRSpell = SpellID::Invalid;
				MyPlayer->_pLRSplType = SpellType::Invalid;
				oracool::ScheduleAutoSaveForSkillChange();
				RedrawEverything();
				return;
			}
			// The quick list, matching the RMB well - see DoPanBtn.
			oracool::OpenSkillPicker(/*forLeftButton=*/true);
			return;
		}
		if (!talkflag && !DropGoldFlag && !IsWithdrawGoldOpen && !gmenu_is_active())
			CheckInvScrn(isShiftHeld, isCtrlHeld);
		DoPanBtn();
		CheckStashButtonPress(MousePosition);
		if (pcurs > CURSOR_HAND && pcurs < CURSOR_FIRSTITEM)
			NewCursor(CURSOR_HAND);
	}
}

void LeftMouseUp(uint16_t modState)
{
	gmenu_left_mouse(false);
	control_release_talk_btn();
	CheckStashButtonRelease(MousePosition);
	if (chrbtnactive) {
		const bool isShiftHeld = (modState & KMOD_SHIFT) != 0;
		ReleaseChrBtns(isShiftHeld);
	}
	if (lvlbtndown)
		ReleaseLvlBtn();
	if (stextflag != TalkID::None)
		ReleaseStoreBtn();
	inventorySortButtonDown = false;
	oracool::ReleaseXpCounterButton();
	ReleaseSpellBookButtons();
}

// Oracool bug fix (2026-08-16): user report - "i cant hit with rmb with regular attack."
// Selecting Regular Attack on the right button clears _pRSpell, because the basic attack IS the
// engine's no-spell-readied state (see oracool/attack_skills.h) - but CheckPlrSpell's first act on
// an invalid spell is to say "I don't have a spell ready" and return. So the one ability the RMB
// well always offers was the one ability the button could never throw.
//
// This is LeftMouseCmd's attack dispatch without its item-pickup and object-operate branches: the
// right button is the SKILL button, and Regular Attack on it means swing, not interact - picking
// things up and opening doors stay the left button's job. Talking still wins over stabbing for
// towners and quest monsters, exactly as it does on the left.
void RightMouseBasicAttack(bool isShiftHeld)
{
	// The plain swing, so no earlier click's skill may ride it - the same rule LeftMouseCmd applies.
	oracool::ArmMeleeSkill(std::nullopt);

	Player &myPlayer = *MyPlayer;
	if (leveltype == DTYPE_TOWN) {
		if (pcursmonst != -1) {
			NetSendCmdLocParam1(true, CMD_TALKXY, cursPosition, pcursmonst);
		} else {
			LastMouseButtonAction = MouseActionType::Walk;
			NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
		}
		return;
	}

	const bool ranged = myPlayer.UsesRangedWeapon();
	if (isShiftHeld) {
		LastMouseButtonAction = MouseActionType::Attack;
		NetSendCmdLoc(MyPlayerId, true, ranged ? CMD_RATTACKXY : CMD_SATTACKXY, cursPosition);
		return;
	}
	if (pcursmonst != -1) {
		if (CanTalkToMonst(Monsters[pcursmonst])) {
			NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
		} else {
			LastMouseButtonAction = MouseActionType::AttackMonsterTarget;
			NetSendCmdParam1(true, ranged ? CMD_RATTACKID : CMD_ATTACKID, pcursmonst);
		}
		return;
	}
	if (pcursplr != -1 && !myPlayer.friendlyMode) {
		LastMouseButtonAction = MouseActionType::AttackPlayerTarget;
		NetSendCmdParam1(true, ranged ? CMD_RATTACKPID : CMD_ATTACKPID, pcursplr);
		return;
	}
	// No target at all: move there. The same "never does nothing" rule every skill follows.
	LastMouseButtonAction = MouseActionType::Walk;
	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
}

void RightMouseDown(bool isShiftHeld)
{
	LastMouseButtonAction = MouseActionType::None;
	LastMouseButtonSpell = SpellID::Invalid;
	LastMouseButtonSpellType = SpellType::Invalid;

	// The same modal owners the left button now respects. See LeftMouseDown.
	if (IsModalPromptOpen())
		return;

	if (gmenu_is_active() || sgnTimeoutCurs != CURSOR_NONE || PauseMode == 2 || MyPlayer->_pInvincible) {
		return;
	}

	if (qtextflag) {
		qtextflag = false;
		stream_stop();
		return;
	}

	if (DoomFlag) {
		doom_close();
		return;
	}
	if (stextflag != TalkID::None) {
		// The right button is what BUYS and SELLS now (user, 2026-08-26). It used to be a bare
		// return, so nothing in a shop answered it at all.
		//
		// On the shop panel: a right click on an item is the purchase, complete, with no
		// confirmation after it - see CheckShopGridClick.
		if (oracool::CheckShopGridClick(MousePosition, /*rightClick=*/true))
			return;

		// In the backpack: a right click on an item sells it to whichever vendor is open. Only the
		// backpack grid - ShopSellInventoryItem refuses a worn item, because selling the armour off
		// your back to a mis-click is not a trade, it is an accident.
		if (invflag && pcursinvitem != -1 && ShopSellInventoryItem(pcursinvitem))
			return;
		// TABS 2-10 reach the same sale by a different hover variable (user, 2026-08-28: "rightclick
		// doesnt sell items in inv tabs 2-9").
		//
		// pcursinvitem is deliberately -1 for an extra tab - its encoding is a tab-1 list index that
		// the legacy drag/drop code assumes, so CheckInvHLight refuses to hand out one for an item
		// that is not in InvList, and publishes pcursinvtabidx/pcursinvtabitem instead. Every
		// single-shot cursor action already goes through that pair; the sale did not, so it silently
		// did nothing on nine tenths of the backpack.
		//
		// ShopSellInventoryItem itself needed no change: it resolves through GetActiveInvListItem,
		// which reads whichever tab is displayed - and the hovered tab is always the displayed one.
		if (invflag && ActiveTabItemHovered && pcursinvtabitem >= 0
		    && ShopSellInventoryItem(pcursinvtabitem + INVITEM_INV_FIRST))
			return;
		return;
	}
	if (spselflag) {
		SetSpell();
		return;
	}
	if (sbookflag && GetSpellBookPanelRect().contains(MousePosition)) {
		// Oracool: user request (2026-08-15) - a row is readied on the button that clicked it, so the
		// right button has to reach the window too. This used to be a bare return, which is why the
		// previous build needed a shift-click to assign the left button.
		CheckSBook(/*assignToRightButton=*/true);
		return;
	}
	if (TryIconCurs())
		return;
	if (isShiftHeld && pcursinvitem != -1 && TryStartStackSplit(pcursinvitem))
		return;
	if (pcursinvitem != -1 && UseInvItem(pcursinvitem))
		return;
	// Oracool: an item hovered in an active Tabbed Inventory extra tab (2-10) never gets a
	// pcursinvitem encoding (see CheckInvHLight) - only pcursinvtabitem/pcursinvtabidx, which
	// TryIconCurs already uses for Identify/Repair/Recharge/Oil above. Without this, right-click
	// use (reading a book, drinking a potion, etc.) silently did nothing for any item stored in
	// an extra tab. ActiveInventoryTab is still set to the tab being viewed, so UseInvItem's own
	// GetActiveInvListItem lookup resolves the right item once given the equivalent cii encoding.
	if (pcursinvtabitem != -1 && UseInvItem(pcursinvtabitem + INVITEM_INV_FIRST))
		return;
	if (pcursstashitem != StashStruct::EmptyCell && UseStashItem(pcursstashitem))
		return;
	// Oracool: user request - inside the inventory window a right-click equips or un-equips
	// instead of falling through to the readied spell ("right click on the inv window casts the
	// RMB skill which is meaningless in this case"). Order matters: the UseInvItem attempts above
	// have already run, so potions still drink and scrolls still read - only what they refused
	// (equipment is not usable; a worn item's cii is below INVITEM_INV_FIRST) reaches this.
	// CheckInvCut's automaticMove path is the exact machinery shift-click uses: backpack
	// equipment auto-equips, a worn item auto-stashes into the backpack.
	//
	// The unconditional return is the other half of the request: a right-click anywhere in the
	// window - empty cells, panel chrome - now does nothing rather than casting.
	if (invflag && oracool::GetInventoryPanelRect().contains(MousePosition)) {
		if (MyPlayer->HoldItem.isEmpty() && pcurs == CURSOR_HAND)
			CheckInvCut(*MyPlayer, MousePosition, /*automaticMove=*/true, /*dropItem=*/false);
		return;
	}
	// Audit finding, 2026-08-26: the authoritative rejection this function never had.
	//
	// Everything above handles a specific surface - the spell book, the inventory, an item under
	// the cursor. What was missing was the general case, so a right-click over the character sheet,
	// the quest log, the waypoint list, Levski's Roar, the runeword book or the lower half of any
	// 340x720 window reached CheckPlrSpell and cast through it. Teleport was the loud one; walking
	// the player toward whatever was behind the window was the common one.
	//
	// Placed HERE rather than at the top, because the handlers above are the ones that legitimately
	// act on interface: readying a skill in the Abilities window, using an item, equipping from the
	// backpack. This guards only the fall-through to the WORLD, which is the only part that was
	// ever wrong.
	if (IsOverAnyInterface(MousePosition))
		return;

	if (pcurs == CURSOR_HAND) {
		// An empty right button swings instead of apologizing - see RightMouseBasicAttack above.
		if (!IsValidSpell(MyPlayer->_pRSpell))
			RightMouseBasicAttack(isShiftHeld);
		else
			CheckPlrSpell(isShiftHeld);
	} else if (pcurs > CURSOR_HAND && pcurs < CURSOR_FIRSTITEM) {
		NewCursor(CURSOR_HAND);
	}
}

void ReleaseKey(SDL_Keycode vkey)
{
	remap_keyboard_key(&vkey);
	if (sgnTimeoutCurs != CURSOR_NONE)
		return;
	// The reserved keys are swallowed on the way UP as well as on the way down (user, 2026-08-19:
	// "F12 double screenshot is not fixed - game takes one SS on click and one SS on release").
	//
	// The keymapper's Screenshot action is registered with a null actionPressed and CaptureScreen as
	// its actionRELEASED (see the AddAction call), so an ini that still binds it to F12 - and a
	// settled install does - fired on the release even though PressKey had already intercepted the
	// press. Filtering auto-repeat last version fixed a different double, not this one.
	//
	// F9-F12 are reserved outright, so nothing else is entitled to either edge of them.
	if ((vkey >= SDLK_F1 && vkey <= SDLK_F12) || vkey == SDLK_SPACE)
		return;
	sgOptions.Keymapper.KeyReleased(vkey);
}

void ClosePanels()
{
	if (CanPanelsCoverView()) {
		if (!IsLeftPanelOpen() && IsRightPanelOpen() && MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
			SetCursorPos(MousePosition + Displacement { 160, 0 });
		} else if (!IsRightPanelOpen() && IsLeftPanelOpen() && MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
			SetCursorPos(MousePosition - Displacement { 160, 0 });
		}
	}
	CloseInventory();
	CloseCharPanel();
	sbookflag = false;
	QuestLogIsOpen = false;
	oracool::CloseWaypointMenu();
	oracool::CloseCraftingMenu();
	oracool::CloseHudMenu();
	oracool::CloseSkillPicker();
}


bool CanPlayerTakeAction(); // defined below, past the keymap tables that also use it

void PressKey(SDL_Keycode vkey, uint16_t modState)
{
	remap_keyboard_key(&vkey);

	if (vkey == SDLK_UNKNOWN)
		return;

	if (gmenu_presskeys(vkey) || control_presskeys(vkey)) {
		return;
	}

	if (MyPlayerIsDead) {
		if (sgnTimeoutCurs != CURSOR_NONE) {
			return;
		}
		sgOptions.Keymapper.KeyPressed(vkey);
		if (vkey == SDLK_RETURN || vkey == SDLK_KP_ENTER) {
#if HAS_KBCTRL == 0
			if ((modState & KMOD_ALT) != 0) {
				sgOptions.Graphics.fullscreen.SetValue(!IsFullScreen());
				SaveOptions();
			} else
#endif
			{
				control_type_message();
			}
		}
		if (vkey != SDLK_ESCAPE) {
			return;
		}
	}
	if (vkey == SDLK_ESCAPE) {
		if (!PressEscKey()) {
			LastMouseButtonAction = MouseActionType::None;
			gamemenu_on();
		}
		return;
	}

	if (DropGoldFlag) {
		control_drop_gold(vkey);
		return;
	}
	if (IsWithdrawGoldOpen) {
		WithdrawGoldKeyPress(vkey);
		return;
	}
	if (IsRefreshUntilPromptOpen) {
		RefreshUntilPromptKeyPress(vkey);
		return;
	}

	if (sgnTimeoutCurs != CURSOR_NONE) {
		return;
	}

	// Oracool: F1-F8 are the ability hotkeys, reserved outright (user, 2026-08-17: "F1-F6 to be
	// available for hotkeying, ergo not be used in any other way in the game"; widened to F8 on
	// 2026-08-18). Intercepted BEFORE the keymapper, which is also what makes them UNREMAPPABLE as
	// requested: the keymapper never sees the press, so no ini row - not the old Help=F1 or
	// QuickSpell=F5..F8 defaults a settled install still carries, and not one the user could add from
	// the Keymapping screen - can double-book them. Help and the quick-spell actions remain listed
	// there, default-unbound, for anyone who wants them on other keys.
	//
	// SHIFT selects the LEFT button's binding; the bare key is the right's. See HandleAbilityFKey.
	// Oracool: SPACE is the master window closer, reserved on the same terms as the F-keys (user,
	// 2026-08-19: "space bar should supersede everything by design"). Intercepted ahead of the
	// keymapper so no ini row can take it - a settled install still carries Hide Info Screens on
	// SPACE, and that action is a strict subset of this one.
	//
	// Everything that legitimately wants a literal space has already returned above: chat typing is
	// consumed by control_presskeys, and the gold-amount and refresh prompts by their own handlers.
	if (vkey == SDLK_SPACE) {
		CloseAllWindows();
		return;
	}

	if (vkey >= SDLK_F1 && vkey <= SDLK_F8 && CanPlayerTakeAction()) {
		HandleAbilityFKey(static_cast<size_t>(vkey - SDLK_F1), (modState & KMOD_SHIFT) != 0);
		return;
	}

	// F9-F12, reserved on the same terms and for the same reason - intercepted ahead of the
	// keymapper, so nothing in the ini can take them (user request, 2026-08-18).
	//
	// Deliberately NOT behind CanPlayerTakeAction(), unlike F1-F8 above. Changing the speed, opening
	// the log and taking a screenshot are all observation or presentation rather than acts in the
	// world; the loading-screen handler already makes exactly that argument for the screenshot key
	// in its own comment (see DisableInputEventHandler).
	switch (vkey) {
	case SDLK_F9:
		oracool::AdjustGameSpeed(-1);
		return;
	case SDLK_F10:
		oracool::AdjustGameSpeed(1);
		return;
	case SDLK_F11:
		oracool::ToggleEventLog();
		return;
	case SDLK_F12:
		// In play. The engine already caught F12 during LOADING screens, but the in-game binding was
		// PrintScreen alone - so the one key the user expects did nothing for the whole game.
		CaptureScreen();
		return;
	default:
		break;
	}

	sgOptions.Keymapper.KeyPressed(vkey);

	if (PauseMode == 2) {
#if HAS_KBCTRL == 0
		if ((vkey == SDLK_RETURN || vkey == SDLK_KP_ENTER) && (modState & KMOD_ALT) != 0) {
			sgOptions.Graphics.fullscreen.SetValue(!IsFullScreen());
			SaveOptions();
		}
#endif
		return;
	}

	if (DoomFlag) {
		doom_close();
		return;
	}

	switch (vkey) {
	case SDLK_PLUS:
	case SDLK_KP_PLUS:
	case SDLK_EQUALS:
	case SDLK_KP_EQUALS:
		if (AutomapActive) {
			AutomapZoomIn();
		}
		return;
	case SDLK_MINUS:
	case SDLK_KP_MINUS:
	case SDLK_UNDERSCORE:
		if (AutomapActive) {
			AutomapZoomOut();
		}
		return;
#ifdef _DEBUG
	case SDLK_m:
		if ((modState & KMOD_SHIFT) != 0)
			NextDebugMonster();
		else
			GetDebugMonster();
		return;
#endif
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if ((modState & KMOD_ALT) != 0) {
#if HAS_KBCTRL == 0
			sgOptions.Graphics.fullscreen.SetValue(!IsFullScreen());
			SaveOptions();
#endif
		} else if (stextflag != TalkID::None) {
			StoreEnter();
		} else if (QuestLogIsOpen) {
			QuestlogEnter();
		} else {
			control_type_message();
		}
		return;
	case SDLK_UP:
		if ((modState & KMOD_ALT) != 0) {
			MiniMapUp();
		} else if (stextflag != TalkID::None) {
			StoreUp();
		} else if (QuestLogIsOpen) {
			QuestlogUp();
		} else if (HelpFlag) {
			HelpScrollUp();
		} else if (ChatLogFlag) {
			ChatLogScrollUp();
		} else if (AutomapActive) {
			AutomapUp();
		} else if (IsStashOpen) {
			Stash.PreviousPage();
		}
		return;
	case SDLK_DOWN:
		if ((modState & KMOD_ALT) != 0) {
			MiniMapDown();
		} else if (stextflag != TalkID::None) {
			StoreDown();
		} else if (QuestLogIsOpen) {
			QuestlogDown();
		} else if (HelpFlag) {
			HelpScrollDown();
		} else if (ChatLogFlag) {
			ChatLogScrollDown();
		} else if (AutomapActive) {
			AutomapDown();
		} else if (IsStashOpen) {
			Stash.NextPage();
		}
		return;
	case SDLK_PAGEUP:
		if (stextflag != TalkID::None) {
			StorePrior();
		} else if (ChatLogFlag) {
			ChatLogScrollTop();
		}
		return;
	case SDLK_PAGEDOWN:
		if (stextflag != TalkID::None) {
			StoreNext();
		} else if (ChatLogFlag) {
			ChatLogScrollBottom();
		}
		return;
	case SDLK_LEFT:
		if ((modState & KMOD_ALT) != 0) {
			MiniMapLeft();
		} else if (oracool::IsShopGridScreen(stextflag)) {
			// Left and right only mean something on a store screen that is a GRID. The text list
			// has one item per row, so the two keys had nothing to move along and were left to the
			// automap - which is why this is a shop test rather than a `stextflag != None` one.
			oracool::MoveShopGridSelection(-1, 0);
		} else if (AutomapActive && !talkflag) {
			AutomapLeft();
		}
		return;
	case SDLK_RIGHT:
		if ((modState & KMOD_ALT) != 0) {
			MiniMapRight();
		} else if (oracool::IsShopGridScreen(stextflag)) {
			oracool::MoveShopGridSelection(1, 0);
		} else if (AutomapActive && !talkflag) {
			AutomapRight();
		}
		return;
	case SDLK_BACKQUOTE:
		// Oracool: ALT+` (the key next to 1) recenters the mini-map's pan on the character.
		if ((modState & KMOD_ALT) != 0) {
			RecenterMiniMap();
		}
		return;
	// SDLK_SPACE used to close the event log here. It is now reserved outright and handled far
	// above, ahead of the keymapper, as the master window closer - the event log is one of the
	// windows CloseAllWindows() shuts, so nothing was lost.
	default:
		break;
	}
}

void HandleMouseButtonDown(Uint8 button, uint16_t modState)
{
	if (stextflag != TalkID::None && (button == SDL_BUTTON_X1
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	        || button == 8
#endif
	        )) {
		StoreESC();
		return;
	}

	if (sgbMouseDown == CLICK_NONE) {
		switch (button) {
		case SDL_BUTTON_LEFT:
			sgbMouseDown = CLICK_LEFT;
			LeftMouseDown(modState);
			break;
		case SDL_BUTTON_RIGHT:
			sgbMouseDown = CLICK_RIGHT;
			RightMouseDown((modState & KMOD_SHIFT) != 0);
			break;
		case SDL_BUTTON_MIDDLE:
			// Oracool: modifier-qualified middle-click actions, mirroring the wheel's own
			// Ctrl/Alt branches - hardcoded here rather than through the (unqualified) Keymapper,
			// matching how other modifier combos like Ctrl+Wheel and Alt+Enter are handled.
			if ((modState & KMOD_CTRL) != 0) {
				if (AutomapActive) {
					ToggleAutomapZoom();
				}
			} else if ((modState & KMOD_ALT) != 0) {
				ToggleMiniMapZoom();
			} else {
				ToggleDungeonZoom();
			}
			break;
		default:
			sgOptions.Keymapper.KeyPressed(button | KeymapperMouseButtonMask);
			break;
		}
	}
}

void HandleMouseButtonUp(Uint8 button, uint16_t modState)
{
	if (sgbMouseDown == CLICK_LEFT && button == SDL_BUTTON_LEFT) {
		LastMouseButtonAction = MouseActionType::None;
		sgbMouseDown = CLICK_NONE;
		LeftMouseUp(modState);
	} else if (sgbMouseDown == CLICK_RIGHT && button == SDL_BUTTON_RIGHT) {
		LastMouseButtonAction = MouseActionType::None;
		sgbMouseDown = CLICK_NONE;
	} else {
		sgOptions.Keymapper.KeyReleased(static_cast<SDL_Keycode>(button | KeymapperMouseButtonMask));
	}
}

[[maybe_unused]] void LogUnhandledEvent(const char *name, int value)
{
	LogVerbose("Unhandled SDL event: {} {}", name, value);
}

void GameEventHandler(const SDL_Event &event, uint16_t modState)
{
	StaticVector<ControllerButtonEvent, 4> ctrlEvents = ToControllerButtonEvents(event);
	for (ControllerButtonEvent ctrlEvent : ctrlEvents) {
		GameAction action;
		if (HandleControllerButtonEvent(event, ctrlEvent, action) && action.type == GameActionType_SEND_KEY) {
			if ((action.send_key.vk_code & KeymapperMouseButtonMask) != 0) {
				const unsigned button = action.send_key.vk_code & ~KeymapperMouseButtonMask;
				if (!action.send_key.up)
					HandleMouseButtonDown(static_cast<Uint8>(button), modState);
				else
					HandleMouseButtonUp(static_cast<Uint8>(button), modState);
			} else {
				if (!action.send_key.up)
					PressKey(static_cast<SDL_Keycode>(action.send_key.vk_code), modState);
				else
					ReleaseKey(static_cast<SDL_Keycode>(action.send_key.vk_code));
			}
		}
	}
	if (ctrlEvents.size() > 0 && ctrlEvents[0].button != ControllerButton_NONE) {
		return;
	}

	if (IsTalkActive() && HandleTalkTextInputEvent(event)) {
		return;
	}
	if (DropGoldFlag && HandleGoldDropTextInputEvent(event)) {
		return;
	}
	if (IsWithdrawGoldOpen && HandleGoldWithdrawTextInputEvent(event)) {
		return;
	}
	if (IsRefreshUntilPromptOpen && HandleRefreshUntilPromptTextInputEvent(event)) {
		return;
	}

	switch (event.type) {
	case SDL_KEYDOWN: {
		// Auto-repeat is dropped for the four reserved keys (user, 2026-08-19: "when i press F12 game
		// takes two screenshots"). CaptureScreen blocks for 300ms inside SDL_Delay while it restores
		// the palette, so an ordinary press is still held when the handler returns - long enough for
		// the OS to have queued a repeat, which then captured a second file. The same reasoning
		// covers F9/F10, where a repeat would silently step the speed twice per press.
		//
		// Only these four: a held movement or hotkey key SHOULD repeat, which is why this is not a
		// blanket filter on event.key.repeat.
		if (event.key.repeat != 0 && event.key.keysym.sym >= SDLK_F9 && event.key.keysym.sym <= SDLK_F12)
			return;
		PressKey(event.key.keysym.sym, modState);
		return;
	}
	case SDL_KEYUP:
		ReleaseKey(event.key.keysym.sym);
		return;
	case SDL_MOUSEMOTION:
		if (ControlMode == ControlTypes::KeyboardAndMouse && invflag)
			InvalidateInventorySlot();
		MousePosition = { event.motion.x, event.motion.y };
		gmenu_on_mouse_move();
		return;
	case SDL_MOUSEBUTTONDOWN:
		MousePosition = { event.button.x, event.button.y };
		HandleMouseButtonDown(event.button.button, modState);
		return;
	case SDL_MOUSEBUTTONUP:
		MousePosition = { event.button.x, event.button.y };
		HandleMouseButtonUp(event.button.button, modState);
		return;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	case SDL_MOUSEWHEEL:
		if (event.wheel.y > 0) { // Up
			if (oracool::IsSkillPickerOpen()) {
				// The picker only ever overflows when a character knows more than fits above the
				// plate; ungated by cursor position because the window is the only thing on screen
				// worth scrolling while it is open.
				oracool::ScrollSkillPicker(1);
			} else if (oracool::HandleLevskiRecipeBookScroll(1)) {
				// consumed - the recipe book is capped to the screen and scrolls inside the cap
			} else if (oracool::HandleRunewordBookScroll(1)) {
				// consumed
			} else if (oracool::HandleCraftingMenuScroll(1)) {
				// consumed
			} else if (stextflag != TalkID::None) {
				StoreUp();
			} else if (QuestLogIsOpen) {
				QuestlogUp();
			} else if (HelpFlag) {
				HelpScrollUp();
			} else if (ChatLogFlag) {
				ChatLogScrollUp();
			} else if (oracool::GetEventLogWindowRect().contains(MousePosition)) {
				// Gated on the cursor being over the log (audit, 2026-08-30), like the waypoint
				// list, the character sheet and the spell book below. It used to be ungated, and
				// sat above all three - so with the log open, scrolling the character sheet
				// scrolled the LOG, and the dungeon zoom was dead everywhere on screen. The log is
				// the one window a player leaves open for a whole session, which is what made an
				// ungated branch cost the most here.
				oracool::ScrollEventLogUp();
			} else if (oracool::IsWaypointMenuOpen() && oracool::GetWaypointMenuRect().contains(MousePosition)) {
				// Oracool V1: the travel list is 25 rows against a 595px viewport once Hellfire's
				// Nest and Crypt are in it. Gated on the cursor actually being over the panel, like
				// the character sheet and spell book below, so the wheel still zooms the dungeon
				// everywhere else while it is open.
				oracool::ScrollWaypointMenuUp();
			} else if (IsStashOpen) {
				Stash.PreviousPage();
			} else if (chrflag && IsOverLeftPanel(MousePosition)) {
				// Oracool V1: the character sheet scrolls - the hidden stats below Mana make it
				// about twice its window's height. Gated on the cursor actually being over the
				// sheet so the wheel still zooms the dungeon everywhere else while it is open.
				ScrollCharacterSheet(-1);
			} else if (sbookflag && GetSpellBookPanelRect().contains(MousePosition)) {
				// Oracool V1: the book is one scrolling list of every spell, not six tabbed pages.
				ScrollSpellBook(-1);
			} else if (SDL_GetModState() & KMOD_CTRL) {
				if (AutomapActive) {
					AutomapZoomIn();
				}
			} else if (SDL_GetModState() & KMOD_ALT) {
				// Oracool: Alt+Wheel zooms the mini-map's own content, independent of the
				// dungeon-view zoom on the plain wheel below.
				MiniMapZoomIn();
			} else if (SDL_GetModState() & KMOD_SHIFT) {
				// Oracool: belt/hotbar cycling, displaced here from the plain wheel below to make
				// room for dungeon-view zoom.
				sgOptions.Keymapper.KeyPressed(MouseScrollUpButton);
			} else {
				// Oracool: plain wheel now zooms the dungeon view in/out, one 0.1x step per notch.
				AdjustDungeonZoom(1);
			}
		} else if (event.wheel.y < 0) { // down
			if (oracool::IsSkillPickerOpen()) {
				oracool::ScrollSkillPicker(-1); // see the wheel-up branch above
			} else if (oracool::HandleLevskiRecipeBookScroll(-1)) {
				// consumed - see the wheel-up branch above
			} else if (oracool::HandleRunewordBookScroll(-1)) {
				// consumed
			} else if (oracool::HandleCraftingMenuScroll(-1)) {
				// consumed
			} else if (stextflag != TalkID::None) {
				StoreDown();
			} else if (QuestLogIsOpen) {
				QuestlogDown();
			} else if (HelpFlag) {
				HelpScrollDown();
			} else if (ChatLogFlag) {
				ChatLogScrollDown();
			} else if (oracool::GetEventLogWindowRect().contains(MousePosition)) {
				// Gated on the cursor - see the wheel-up branch above.
				oracool::ScrollEventLogDown();
			} else if (oracool::IsWaypointMenuOpen() && oracool::GetWaypointMenuRect().contains(MousePosition)) {
				// Oracool V1: travel list scrolling - see the wheel-up branch above.
				oracool::ScrollWaypointMenuDown();
			} else if (IsStashOpen) {
				Stash.NextPage();
			} else if (chrflag && IsOverLeftPanel(MousePosition)) {
				// Oracool V1: character sheet scrolling - see the wheel-up branch above.
				ScrollCharacterSheet(1);
			} else if (sbookflag && GetSpellBookPanelRect().contains(MousePosition)) {
				// Oracool V1: spell book scrolling - see the wheel-up branch above.
				ScrollSpellBook(1);
			} else if (SDL_GetModState() & KMOD_CTRL) {
				if (AutomapActive) {
					AutomapZoomOut();
				}
			} else if (SDL_GetModState() & KMOD_ALT) {
				// Oracool: Alt+Wheel zooms the mini-map's own content - see the wheel-up branch above.
				MiniMapZoomOut();
			} else if (SDL_GetModState() & KMOD_SHIFT) {
				// Oracool: belt/hotbar cycling, displaced here - see the wheel-up branch above.
				sgOptions.Keymapper.KeyPressed(MouseScrollDownButton);
			} else {
				// Oracool: plain wheel now zooms the dungeon view in/out, one 0.1x step per notch.
				AdjustDungeonZoom(-1);
			}
		} else if (event.wheel.x > 0) { // left
			sgOptions.Keymapper.KeyPressed(MouseScrollLeftButton);
		} else if (event.wheel.x < 0) { // right
			sgOptions.Keymapper.KeyPressed(MouseScrollRightButton);
		}
		break;
#endif
	default:
		if (IsCustomEvent(event.type)) {
			if (gbIsMultiplayer)
				pfile_write_hero();
			nthread_ignore_mutex(true);
			PaletteFadeOut(8);
			sound_stop();
			ShowProgress(GetCustomEvent(event.type));

			RedrawEverything();
			if (!HeadlessMode) {
				while (IsRedrawEverything()) {
					// In direct rendering mode with double/triple buffering, we need
					// to prepare all buffers before fading in.
					DrawAndBlit();
				}
			}

			LoadPWaterPalette();
			if (gbRunGame)
				PaletteFadeIn(8);
			nthread_ignore_mutex(false);
			gbGameLoopStartup = true;
			return;
		}
		MainWndProc(event);
		break;
	}
}

void RunGameLoop(interface_mode uMsg)
{
	demo::NotifyGameLoopStart();

	nthread_ignore_mutex(true);
	StartGame(uMsg);
	assert(HeadlessMode || ghMainWnd);
	EventHandler previousHandler = SetEventHandler(GameEventHandler);
	run_delta_info();
	gbRunGame = true;
	gbProcessPlayers = IsDiabloAlive(true);
	gbRunGameResult = true;

	RedrawEverything();
	if (!HeadlessMode) {
		while (IsRedrawEverything()) {
			// In direct rendering mode with double/triple buffering, we need
			// to prepare all buffers before fading in.
			DrawAndBlit();
		}
	}

	LoadPWaterPalette();
	PaletteFadeIn(8);
	InitBackbufferState();
	RedrawEverything();
	gbGameLoopStartup = true;
	nthread_ignore_mutex(false);

	discord_manager::StartGame();
#ifdef GPERF_HEAP_FIRST_GAME_ITERATION
	unsigned run_game_iteration = 0;
#endif

	while (gbRunGame) {

#ifdef _DEBUG
		if (!gbGameLoopStartup && !DebugCmdsFromCommandLine.empty()) {
			for (auto &cmd : DebugCmdsFromCommandLine) {
				CheckDebugTextCommand(cmd);
			}
			DebugCmdsFromCommandLine.clear();
		}
#endif

		SDL_Event event;
		uint16_t modState;
		while (FetchMessage(&event, &modState)) {
			if (event.type == SDL_QUIT) {
				gbRunGameResult = false;
				gbRunGame = false;
				break;
			}
			HandleMessage(event, modState);
		}
		if (!gbRunGame)
			break;

		bool drawGame = true;
		bool processInput = true;
		bool runGameLoop = demo::IsRunning() ? demo::GetRunGameLoop(drawGame, processInput) : nthread_has_500ms_passed(&drawGame);
		if (demo::IsRecording())
			demo::RecordGameLoopResult(runGameLoop);

		discord_manager::UpdateGame();

		if (!runGameLoop) {
			if (processInput)
				ProcessInput();
			if (!drawGame)
				continue;
			RedrawViewport();
			DrawAndBlit();
			continue;
		}

		multi_process_network_packets();
		if (game_loop(gbGameLoopStartup))
			diablo_color_cyc_logic();
		oracool::ProcessAutoSave();
		gbGameLoopStartup = false;
		if (drawGame)
			DrawAndBlit();
#ifdef GPERF_HEAP_FIRST_GAME_ITERATION
		if (run_game_iteration++ == 0)
			HeapProfilerDump("first_game_iteration");
#endif
	}

	demo::NotifyGameLoopEnd();

	if (gbIsMultiplayer) {
		SaveHeroAndStash(/*writeGameData=*/false);
	} else {
		// Oracool (audit, 2026-08-26): the single-player half of the same thought. Alt+F4 raises
		// SDL_QUIT, which clears gbRunGame and breaks the loop above rather than going through the
		// menu - so it arrived HERE, where the only save was multiplayer-only, and wrote nothing.
		//
		// SaveOnExit is idempotent enough for this to be safe when the menu route already ran: it
		// simply saves the same character again.
		oracool::SaveOnExit();
	}

	PaletteFadeOut(8);
	NewCursor(CURSOR_NONE);
	ClearScreenBuffer();
	RedrawEverything();
	scrollrt_draw_game_screen();
	previousHandler = SetEventHandler(previousHandler);
	assert(HeadlessMode || previousHandler == GameEventHandler);
	FreeGame();

	if (cineflag) {
		cineflag = false;
		DoEnding();
	}
}

void PrintWithRightPadding(string_view str, size_t width)
{
	printInConsole(str);
	if (str.size() >= width)
		return;
	printInConsole(std::string(width - str.size(), ' '));
}

void PrintHelpOption(string_view flags, string_view description)
{
	printInConsole("    ");
	PrintWithRightPadding(flags, 20);
	printInConsole(" ");
	PrintWithRightPadding(description, 30);
	printNewlineInConsole();
}

[[noreturn]] void PrintHelpAndExit()
{
	printInConsole((/* TRANSLATORS: Commandline Option */ "Options:"));
	printNewlineInConsole();
	PrintHelpOption("-h, --help", _(/* TRANSLATORS: Commandline Option */ "Print this message and exit"));
	PrintHelpOption("--version", _(/* TRANSLATORS: Commandline Option */ "Print the version and exit"));
	PrintHelpOption("--data-dir", _(/* TRANSLATORS: Commandline Option */ "Specify the folder of diabdat.mpq"));
	PrintHelpOption("--save-dir", _(/* TRANSLATORS: Commandline Option */ "Specify the folder of save files"));
	PrintHelpOption("--config-dir", _(/* TRANSLATORS: Commandline Option */ "Specify the location of diablo.ini"));
	PrintHelpOption("--lang", _(/* TRANSLATORS: Commandline Option */ "Specify the language code (e.g. en or pt_BR)"));
	PrintHelpOption("-n", _(/* TRANSLATORS: Commandline Option */ "Skip startup videos"));
	PrintHelpOption("-f", _(/* TRANSLATORS: Commandline Option */ "Display frames per second"));
	PrintHelpOption("--verbose", _(/* TRANSLATORS: Commandline Option */ "Enable verbose logging"));
#ifndef DISABLE_DEMOMODE
	PrintHelpOption("--record <#>", _(/* TRANSLATORS: Commandline Option */ "Record a demo file"));
	PrintHelpOption("--demo <#>", _(/* TRANSLATORS: Commandline Option */ "Play a demo file"));
	PrintHelpOption("--timedemo", _(/* TRANSLATORS: Commandline Option */ "Disable all frame limiting during demo playback"));
#endif
	printNewlineInConsole();
	printInConsole(_(/* TRANSLATORS: Commandline Option */ "Game selection:"));
	printNewlineInConsole();
	PrintHelpOption("--spawn", _(/* TRANSLATORS: Commandline Option */ "Force Shareware mode"));
	PrintHelpOption("--diablo", _(/* TRANSLATORS: Commandline Option */ "Force Diablo mode"));
	PrintHelpOption("--hellfire", _(/* TRANSLATORS: Commandline Option */ "Force Hellfire mode"));
	printInConsole(_(/* TRANSLATORS: Commandline Option */ "Hellfire options:"));
	printNewlineInConsole();
#ifdef _DEBUG
	printNewlineInConsole();
	printInConsole("Debug options:");
	printNewlineInConsole();
	PrintHelpOption("-i", "Ignore network timeout");
	PrintHelpOption("+<internal command>", "Pass commands to the engine");
#endif
	printNewlineInConsole();
	printInConsole(_("Report bugs at https://github.com/diasurgical/devilutionX/"));
	printNewlineInConsole();
	diablo_quit(0);
}

void PrintFlagsRequiresArgument(string_view flag)
{
	printInConsole(flag);
	printInConsole(" requires an argument");
	printNewlineInConsole();
}

void DiabloParseFlags(int argc, char **argv)
{
#ifdef _DEBUG
	int argumentIndexOfLastCommandPart = -1;
	std::string currentCommand;
#endif
#ifndef DISABLE_DEMOMODE
	bool timedemo = false;
	int demoNumber = -1;
	int recordNumber = -1;
	bool createDemoReference = false;
#endif
	for (int i = 1; i < argc; i++) {
		const string_view arg = argv[i];
		if (arg == "-h" || arg == "--help") {
			PrintHelpAndExit();
		} else if (arg == "--version") {
			// ORACOOL_VERSION first, and on a line automation can parse exactly (external audit of
			// v1.9.92, finding 10). This printed PROJECT_VERSION alone - the DevilutionX engine base,
			// which does not move when this fork releases - so there was no authoritative way to ask
			// a binary what it is. Packaging was reduced to scanning the exe's ASCII strings for the
			// expected number, which proves only that those bytes occur SOMEWHERE: a changelog
			// string or a dead resource path would satisfy it just as well as the real stamp.
			//
			// The engine base stays on its own line, because it is genuinely useful and dropping it
			// would lose information the old output carried.
			printInConsole(PROJECT_NAME);
			printInConsole(" ");
			printInConsole(ORACOOL_VERSION);
			printNewlineInConsole();
			printInConsole("engine ");
			printInConsole(PROJECT_VERSION);
			printNewlineInConsole();
			diablo_quit(0);
		} else if (arg == "--data-dir") {
			if (i + 1 == argc) {
				PrintFlagsRequiresArgument("--data-dir");
				diablo_quit(64);
			}
			paths::SetBasePath(argv[++i]);
		} else if (arg == "--save-dir") {
			if (i + 1 == argc) {
				PrintFlagsRequiresArgument("--save-dir");
				diablo_quit(64);
			}
			paths::SetPrefPath(argv[++i]);
		} else if (arg == "--config-dir") {
			if (i + 1 == argc) {
				PrintFlagsRequiresArgument("--config-dir");
				diablo_quit(64);
			}
			paths::SetConfigPath(argv[++i]);
		} else if (arg == "--lang") {
			if (i + 1 == argc) {
				PrintFlagsRequiresArgument("--lang");
				diablo_quit(64);
			}
			forceLocale = argv[++i];
#ifndef DISABLE_DEMOMODE
		} else if (arg == "--demo") {
			if (i + 1 == argc) {
				PrintFlagsRequiresArgument("--demo");
				diablo_quit(64);
			}
			demoNumber = SDL_atoi(argv[++i]);
			gbShowIntro = false;
		} else if (arg == "--timedemo") {
			timedemo = true;
		} else if (arg == "--record") {
			if (i + 1 == argc) {
				PrintFlagsRequiresArgument("--record");
				diablo_quit(64);
			}
			recordNumber = SDL_atoi(argv[++i]);
		} else if (arg == "--create-reference") {
			createDemoReference = true;
#else
		} else if (arg == "--demo" || arg == "--timedemo" || arg == "--record" || arg == "--create-reference") {
			printInConsole("Binary compiled without demo mode support.");
			printNewlineInConsole();
			diablo_quit(1);
#endif
		} else if (arg == "-n") {
			gbShowIntro = false;
		} else if (arg == "-f") {
			EnableFrameCount();
		} else if (arg == "--spawn") {
			forceSpawn = true;
		} else if (arg == "--diablo") {
			forceDiablo = true;
		} else if (arg == "--hellfire") {
			forceHellfire = true;
		} else if (arg == "--vanilla") {
			gbVanilla = true;
		} else if (arg == "--verbose") {
			SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
#ifdef _DEBUG
		} else if (arg == "-i") {
			DebugDisableNetworkTimeout = true;
		} else if (arg[0] == '+') {
			if (!currentCommand.empty())
				DebugCmdsFromCommandLine.push_back(currentCommand);
			argumentIndexOfLastCommandPart = i;
			currentCommand = arg.substr(1);
		} else if (arg[0] != '-' && (argumentIndexOfLastCommandPart + 1) == i) {
			currentCommand.append(" ");
			currentCommand.append(arg);
			argumentIndexOfLastCommandPart = i;
#endif
		} else {
			printInConsole("unrecognized option '");
			printInConsole(argv[i]);
			printInConsole("'");
			printNewlineInConsole();
			PrintHelpAndExit();
		}
	}

#ifdef _DEBUG
	if (!currentCommand.empty())
		DebugCmdsFromCommandLine.push_back(currentCommand);
#endif

	// Oracool is a Hellfire-based game, full stop (external audit, 2026-08-17). Everything the fork
	// adds - the class trees' spell ids past the Diablo cutoff, the expansion uniques past index 89,
	// the 25 waypoints, Nest and Crypt - sits behind gbIsHellfire gates, so a Diablo-mode boot is a
	// half-broken game that was only ever reachable by accident (--diablo, or a stale ini value).
	// Forcing it HERE, before the archives load, buys init.cpp's existing missing-hellfire dialog
	// for free: an install without the Hellfire MPQs is told what to add instead of limping.
	if (forceDiablo)
		Log("Diablo Orcl is a Hellfire-based game; --diablo is ignored.");
	forceDiablo = false;
	forceHellfire = true;

#ifndef DISABLE_DEMOMODE
	if (demoNumber != -1)
		demo::InitPlayBack(demoNumber, timedemo);
	if (recordNumber != -1)
		demo::InitRecording(recordNumber, createDemoReference);
#endif
}

void DiabloInitScreen()
{
	MousePosition = { gnScreenWidth / 2, gnScreenHeight / 2 };
	if (ControlMode == ControlTypes::KeyboardAndMouse)
		SetCursorPos(MousePosition);

	ClrDiabloMsg();
}

void SetApplicationVersions()
{
	// The visible Oracool release version is intentionally independent from
	// PROJECT_VERSION, which tracks the DevilutionX engine base. (The multiplayer
	// version-compatibility check rode PROJECT_VERSION too until the external audit
	// of 2026-08-17; it now carries the Oracool version - see InitGameInfo in
	// multi.cpp - because the wire structs no longer match the base engine's.)
	*BufCopy(gszProductName, PROJECT_NAME, " v", ORACOOL_VERSION, " - Based on DevilutionX ", PROJECT_VERSION) = '\0';
	*BufCopy(gszVersionNumber, "version ", PROJECT_VERSION) = '\0';
	*BufCopy(gszMainMenuVersionText, "DevilutionX ", PROJECT_VERSION, "\nOracool Edition v", ORACOOL_VERSION) = '\0';
}

void CheckArchivesUpToDate()
{
#ifdef UNPACKED_MPQS
	const bool devilutionxMpqOutOfDate = false;
#else
	const bool devilutionxMpqOutOfDate = devilutionx_mpq && (!devilutionx_mpq->HasFile("data\\charbg.clx") || devilutionx_mpq->HasFile("fonts\\12-00.bin"));
#endif
	const bool fontsMpqOutOfDate = AreExtraFontsOutOfDate();

	if (devilutionxMpqOutOfDate && fontsMpqOutOfDate) {
		app_fatal(_("Please update devilutionx.mpq and fonts.mpq to the latest version"));
	} else if (devilutionxMpqOutOfDate) {
		app_fatal(_("Failed to load UI resources.\n"
		            "\n"
		            "Make sure devilutionx.mpq is in the game folder and that it is up to date."));
	} else if (fontsMpqOutOfDate) {
		app_fatal(_("Please update fonts.mpq to the latest version"));
	}
}

void ApplicationInit()
{
	if (*sgOptions.Graphics.showFPS)
		EnableFrameCount();

	init_create_window();
	was_window_init = true;

	LanguageInitialize();

	SetApplicationVersions();

	ReadOnlyTest();
}

void DiabloInit()
{
	if (forceSpawn || *sgOptions.GameMode.shareware)
		gbIsSpawn = true;

	// Oracool: user request - "I don't want Diablo Orcl to ask Diablo or Hellfire on boot or after
	// initial installation. Hellfire is Default."
	//
	// Migrated rather than only defaulted. The option's default is now Hellfire, but that alone
	// fixes nothing for anyone who has already run the game: vanilla's startup dialog leaves
	// `Ask` written in diablo.ini, and a stored value beats a default forever. Ask is also hidden
	// from the settings menu (see GameModeOptions), so a character stuck on it has no way to pick
	// anything else from inside the game.
	//
	// Guarded on gbIsHellfire, which init.cpp sets from hellfire.mpq's presence: without that
	// archive this would promise a Hellfire game the install cannot deliver.
	// Diablo joins Ask in the migration (external audit, 2026-08-17): DiabloParseFlags now forces
	// Hellfire mode unconditionally, so a stored Diablo value would be a setting the game ignores -
	// worse than one it honors badly. gbIsHellfire is guaranteed true here by that force (init.cpp
	// raised the missing-MPQ dialog otherwise), so the guard below keeps its meaning.
	if (gbIsHellfire && IsAnyOf(*sgOptions.GameMode.gameMode, StartUpGameMode::Ask, StartUpGameMode::Diablo))
		sgOptions.GameMode.gameMode.SetValue(StartUpGameMode::Hellfire);

	if (forceDiablo || *sgOptions.GameMode.gameMode == StartUpGameMode::Diablo)
		gbIsHellfire = false;
	if (forceHellfire)
		gbIsHellfire = true;

	gbIsHellfireSaveGame = gbIsHellfire;

	for (size_t i = 0; i < QUICK_MESSAGE_OPTIONS; i++) {
		auto &messages = sgOptions.Chat.szHotKeyMsgs[i];
		if (messages.empty()) {
			messages.emplace_back(_(QuickMessages[i].message));
		}
	}

#ifndef USE_SDL1
	InitializeVirtualGamepad();
#endif

	UiInitialize();
	was_ui_init = true;

	// Oracool: the "Play Diablo or Hellfire?" startup dialog used to run here. It is gone on the
	// user's instruction - see the migration in DiabloInit above, which is what guarantees the
	// setting can never sit on Ask and reach this point. UiSelStartUpGameOption() itself is left in
	// the tree, unreferenced, the same way the multiplayer screens are: unreachable, not deleted.
	// The Game Mode setting still switches to Diablo for anyone who wants it.

	DiabloInitScreen();

	snd_init();

	ui_sound_init();

	// Item graphics are loaded early, they already get touched during hero selection.
	InitItemGFX();

	// Always available.
	LoadSmallSelectionSpinner();

	CheckArchivesUpToDate();
}

void DiabloSplash()
{
	if (!gbShowIntro)
		return;

	if (*sgOptions.StartUp.splash == StartUpSplash::LogoAndTitleDialog)
		play_movie("gendata\\logo.smk", true);

	auto &intro = gbIsHellfire ? sgOptions.StartUp.hellfireIntro : sgOptions.StartUp.diabloIntro;

	if (*intro != StartUpIntro::Off) {
		if (gbIsHellfire)
			play_movie("gendata\\Hellfire.smk", true);
		else
			play_movie("gendata\\diablo1.smk", true);
		if (*intro == StartUpIntro::Once) {
			intro.SetValue(StartUpIntro::Off);
			SaveOptions();
		}
	}

	if (IsAnyOf(*sgOptions.StartUp.splash, StartUpSplash::TitleDialog, StartUpSplash::LogoAndTitleDialog))
		UiTitleDialog();
}

void DiabloDeinit()
{
	// Armed HERE rather than at a call site, because there are two of them and the first attempt
	// (v1.9.137) armed the wrong one. It went into DiabloMain's game-quit path; the ordinary way out
	// is mainmenu_loop() returning and main() calling this directly, which is the path a player
	// actually takes - quit to the menu, then exit. So a v1.9.138 build, watchdog and all, still
	// left a windowless process behind at 08:26 on 2026-08-31.
	//
	// One arming inside the function every exit funnels through cannot be bypassed by adding a
	// third. See oracool/shutdown_watchdog.h for why bounding this is the honest fix rather than
	// guessing at the deadlock.
	oracool::ArmShutdownWatchdog();

	FreeItemGFX();

	if (gbSndInited)
		effects_cleanup_sfx();
	snd_deinit();
	if (was_ui_init)
		UiDestroy();
	if (was_archives_init)
		init_cleanup();
	if (was_window_init)
		dx_cleanup(); // Cleanup SDL surfaces stuff, so we have to do it before SDL_Quit().
	UnloadFonts();
	if (SDL_WasInit(SDL_INIT_EVERYTHING & ~SDL_INIT_HAPTIC) != 0)
		SDL_Quit();
}

void LoadLvlGFX()
{
	assert(pDungeonCels == nullptr);
	constexpr int SpecialCelWidth = 64;

	switch (leveltype) {
	case DTYPE_TOWN:
		if (gbIsHellfire) {
			pDungeonCels = LoadFileInMem("nlevels\\towndata\\town.cel");
			pMegaTiles = LoadFileInMem<MegaTile>("nlevels\\towndata\\town.til");
		} else {
			pDungeonCels = LoadFileInMem("levels\\towndata\\town.cel");
			pMegaTiles = LoadFileInMem<MegaTile>("levels\\towndata\\town.til");
		}
		pSpecialCels = LoadCel("levels\\towndata\\towns", SpecialCelWidth);
		break;
	case DTYPE_CATHEDRAL:
		pDungeonCels = LoadFileInMem("levels\\l1data\\l1.cel");
		pMegaTiles = LoadFileInMem<MegaTile>("levels\\l1data\\l1.til");
		pSpecialCels = LoadCel("levels\\l1data\\l1s", SpecialCelWidth);
		break;
	case DTYPE_CATACOMBS:
		pDungeonCels = LoadFileInMem("levels\\l2data\\l2.cel");
		pMegaTiles = LoadFileInMem<MegaTile>("levels\\l2data\\l2.til");
		pSpecialCels = LoadCel("levels\\l2data\\l2s", SpecialCelWidth);
		break;
	case DTYPE_CAVES:
		pDungeonCels = LoadFileInMem("levels\\l3data\\l3.cel");
		pMegaTiles = LoadFileInMem<MegaTile>("levels\\l3data\\l3.til");
		pSpecialCels = LoadCel("levels\\l1data\\l1s", SpecialCelWidth);
		break;
	case DTYPE_HELL:
		pDungeonCels = LoadFileInMem("levels\\l4data\\l4.cel");
		pMegaTiles = LoadFileInMem<MegaTile>("levels\\l4data\\l4.til");
		pSpecialCels = LoadCel("levels\\l2data\\l2s", SpecialCelWidth);
		break;
	case DTYPE_NEST:
		pDungeonCels = LoadFileInMem("nlevels\\l6data\\l6.cel");
		pMegaTiles = LoadFileInMem<MegaTile>("nlevels\\l6data\\l6.til");
		pSpecialCels = LoadCel("levels\\l1data\\l1s", SpecialCelWidth);
		break;
	case DTYPE_CRYPT:
		pDungeonCels = LoadFileInMem("nlevels\\l5data\\l5.cel");
		pMegaTiles = LoadFileInMem<MegaTile>("nlevels\\l5data\\l5.til");
		pSpecialCels = LoadCel("nlevels\\l5data\\l5s", SpecialCelWidth);
		break;
	default:
		app_fatal("LoadLvlGFX");
	}
}

void LoadAllGFX()
{
	IncProgress();
#if !defined(USE_SDL1) && !defined(__vita__)
	InitVirtualGamepadGFX(renderer);
#endif
	IncProgress();
	InitObjectGFX();
	IncProgress();
	InitMissileGFX(gbIsHellfire);
	IncProgress();
}

/**
 * @param entry Where is the player entering from
 */
void CreateLevel(lvl_entry entry)
{
	CreateDungeon(glSeedTbl[currlevel], entry);

	switch (leveltype) {
	case DTYPE_TOWN:
		InitTownTriggers();
		break;
	case DTYPE_CATHEDRAL:
		InitL1Triggers();
		break;
	case DTYPE_CATACOMBS:
		InitL2Triggers();
		break;
	case DTYPE_CAVES:
		InitL3Triggers();
		break;
	case DTYPE_HELL:
		InitL4Triggers();
		break;
	case DTYPE_NEST:
		InitHiveTriggers();
		break;
	case DTYPE_CRYPT:
		InitCryptTriggers();
		break;
	default:
		app_fatal("CreateLevel");
	}

	if (leveltype != DTYPE_TOWN) {
		Freeupstairs();
	}
	LoadRndLvlPal(leveltype);
}

void UnstuckChargers()
{
	if (gbIsMultiplayer) {
		for (Player &player : Players) {
			if (!player.plractive)
				continue;
			if (player._pLvlChanging)
				continue;
			if (!player.isOnActiveLevel())
				continue;
			if (&player == MyPlayer)
				continue;
			return;
		}
	}
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		auto &monster = Monsters[ActiveMonsters[i]];
		if (monster.mode == MonsterMode::Charge)
			monster.mode = MonsterMode::Stand;
	}
}

void UpdateMonsterLights()
{
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		auto &monster = Monsters[ActiveMonsters[i]];

		if ((monster.flags & MFLAG_BERSERK) != 0) {
			int lightRadius = leveltype == DTYPE_NEST ? 9 : 3;
			monster.lightId = AddLight(monster.position.tile, lightRadius);
		}

		if (monster.lightId != NO_LIGHT) {
			if (monster.lightId == MyPlayer->lightId) { // Fix old saves where some monsters had 0 instead of NO_LIGHT
				monster.lightId = NO_LIGHT;
				continue;
			}

			Light &light = Lights[monster.lightId];
			if (monster.position.tile != light.position.tile) {
				ChangeLightXY(monster.lightId, monster.position.tile);
			}
		}
	}
}

void GameLogic()
{
	if (!ProcessInput()) {
		return;
	}
	if (gbProcessPlayers) {
		gGameLogicStep = GameLogicStep::ProcessPlayers;
		ProcessPlayers();
	}
	if (leveltype != DTYPE_TOWN) {
		gGameLogicStep = GameLogicStep::ProcessMonsters;
		ProcessMonsters();
		gGameLogicStep = GameLogicStep::ProcessObjects;
		ProcessObjects();
		gGameLogicStep = GameLogicStep::ProcessMissiles;
		ProcessMissiles();
		gGameLogicStep = GameLogicStep::ProcessItems;
		ProcessItems();
		ProcessLightList();
		ProcessVisionList();
	} else {
		gGameLogicStep = GameLogicStep::ProcessTowners;
		ProcessTowners();
		gGameLogicStep = GameLogicStep::ProcessItemsTown;
		ProcessItems();
		gGameLogicStep = GameLogicStep::ProcessMissilesTown;
		ProcessMissiles();
	}
	gGameLogicStep = GameLogicStep::None;

	// After the player has moved, so the distance it measures is this tick's, and OUTSIDE the town
	// branch: the walkaway half returns immediately anywhere else, but the service-cursor half must
	// run everywhere. A cursor armed at Griswold's and carried down a portal is exactly the case a
	// town-only check would miss.
	UpdateStoreState();

#ifdef _DEBUG
	if (DebugScrollViewEnabled && (SDL_GetModState() & KMOD_SHIFT) != 0) {
		ScrollView();
	}
#endif

	sound_update();
	CheckTriggers();
	CheckQuests();
	RedrawViewport();
	pfile_update(false);

	plrctrls_after_game_logic();
}

void TimeoutCursor(bool bTimeout)
{
	if (bTimeout) {
		if (sgnTimeoutCurs == CURSOR_NONE && sgbMouseDown == CLICK_NONE) {
			sgnTimeoutCurs = pcurs;
			multi_net_ping();
			ClearPanelStrings();
			AddPanelString(_("-- Network timeout --"));
			AddPanelString(_("-- Waiting for players --"));
			NewCursor(CURSOR_HOURGLASS);
			RedrawEverything();
		}
		scrollrt_draw_game_screen();
	} else if (sgnTimeoutCurs != CURSOR_NONE) {
		// Timeout is gone, we should restore the previous cursor.
		// But the timeout cursor could already be changed by the now processed messages (for example item cursor from CMD_GETITEM).
		// Changing the item cursor back to the previous (hand) cursor could result in deleted items, cause this resets Player.HoldItem (see NewCursor).
		if (pcurs == CURSOR_HOURGLASS)
			NewCursor(sgnTimeoutCurs);
		sgnTimeoutCurs = CURSOR_NONE;
		ClearPanelStrings();
		RedrawEverything();
	}
}

void HelpKeyPressed()
{
	if (HelpFlag) {
		HelpFlag = false;
	} else if (stextflag != TalkID::None) {
		ClearPanelStrings();
		AddPanelString(_("No help available")); /// BUGFIX: message isn't displayed
		AddPanelString(_("while in stores"));
		LastMouseButtonAction = MouseActionType::None;
	} else {
		CloseInventory();
		CloseCharPanel();
		sbookflag = false;
		spselflag = false;
		if (qtextflag && leveltype == DTYPE_TOWN) {
			qtextflag = false;
			stream_stop();
		}
		QuestLogIsOpen = false;
		CancelCurrentDiabloMsg();
		gamemenu_off();
		DisplayHelp();
		doom_close();
	}
}

void InventoryKeyPressed()
{
	if (stextflag != TalkID::None)
		return;
	invflag = !invflag;
	if (!IsLeftPanelOpen() && CanPanelsCoverView()) {
		if (!invflag) { // We closed the invetory
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		} else if (!sbookflag) { // We opened the invetory
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		}
	}
	sbookflag = false;
	CloseGoldWithdraw();
	CloseStash();
}

void CharacterSheetKeyPressed()
{
	if (stextflag != TalkID::None)
		return;
	if (!IsRightPanelOpen() && CanPanelsCoverView()) {
		if (chrflag) { // We are closing the character sheet
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		} else if (!QuestLogIsOpen) { // We opened the character sheet
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		}
	}
	ToggleCharPanel();
}

void QuestLogKeyPressed()
{
	if (stextflag != TalkID::None)
		return;
	if (!QuestLogIsOpen) {
		StartQuestlog();
	} else {
		QuestLogIsOpen = false;
	}
	if (!IsRightPanelOpen() && CanPanelsCoverView()) {
		if (!QuestLogIsOpen) { // We closed the quest log
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		} else if (!chrflag) { // We opened the character quest log
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		}
	}
	CloseCharPanel();
	CloseGoldWithdraw();
	CloseStash();
}

void DisplaySpellsKeyPressed()
{
	if (stextflag != TalkID::None)
		return;
	CloseCharPanel();
	QuestLogIsOpen = false;
	// Oracool: user request - S opens the Abilities window; the speedbook ring it used to raise is
	// retired. ToggleAbilitiesWindow closes the inventory itself, so the explicit CloseInventory
	// and the `sbookflag = false` that used to precede this are gone - the latter actively broke
	// the toggle, forcing the window closed a line before flipping it back open, so S could only
	// ever open and never close.
	ToggleAbilitiesWindow();
	LastMouseButtonAction = MouseActionType::None;
	LastMouseButtonSpell = SpellID::Invalid;
	LastMouseButtonSpellType = SpellType::Invalid;
}

void SpellBookKeyPressed()
{
	if (stextflag != TalkID::None)
		return;
	sbookflag = !sbookflag;
	// Oracool V1: the book is a scrolling list twice its window's height, so opening it should
	// always show the top. Unconditional because resetting a closed book costs nothing.
	ResetSpellBookScroll();
	if (!IsLeftPanelOpen() && CanPanelsCoverView()) {
		if (!sbookflag) { // We closed the invetory
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		} else if (!invflag) { // We opened the invetory
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		}
	}
	CloseInventory();
}

bool IsPlayerDead()
{
	return MyPlayer->_pmode == PM_DEATH || MyPlayerIsDead;
}

bool IsGameRunning()
{
	return PauseMode != 2;
}

bool CanPlayerTakeAction()
{
	// Audit finding, 2026-08-26. A numeric prompt - drop gold, withdraw gold, "Refresh Until" -
	// owns the keyboard while it is open, and the two mouse paths were taught to respect that. The
	// controller was not: pad actions are dispatched through this predicate, so gamepad attack,
	// spellcast and potion quaffing went straight past an open prompt into the world.
	//
	// It belongs HERE rather than at each of the forty-odd call sites, because every one of them is
	// a keymapper or padmapper gameplay action and the answer is the same for all of them. The
	// prompt's own confirm and cancel do not come through this predicate - they are handled in the
	// text-input path, which checks IsModalPromptOpen first - so the prompt stays usable.
	return !IsPlayerDead() && IsGameRunning() && !IsModalPromptOpen();
}
} // namespace

// Oracool: moved OUT of the anonymous namespace above on 2026-08-20. It was file-local, so
// declaring it in diablo.h produced an unresolved external rather than a working export - the
// runeword book is its second caller and the first from another translation unit.
void CloseAllWindows()
{
	// Oracool: the space bar's master close (user, 2026-08-19) - "space bar to close any and all
	// windows regardless of when they were introduced in the game. space bar should supersede
	// everything by design and act as master windows closer."
	//
	// Deliberately a superset of ClosePanels() rather than a rename of it. ClosePanels() is called
	// from a dozen places that mean "put the side panels away" - opening a store, taking a
	// waypoint, starting a cutscene - and none of them should also be dismissing the help screen or
	// the automap. This one means what the player means when they hit space: everything, gone.
	//
	// A window added to the game must be added HERE, not only to ClosePanels(). That is the whole
	// point of the rule: "regardless of when they were introduced".
	ClosePanels();
	CloseStash();
	CloseGoldWithdraw();
	DropGoldFlag = false;
	// Levski's Roar can refuse, and is allowed to: its grid is not save state, so closing on a full
	// backpack would destroy what is in it. It says so in the log and stays open - the one window
	// space cannot force, by design rather than by omission.
	oracool::CloseLevskiRoar();
	oracool::CloseRunewordBook();
	if (oracool::IsEventLogOpen())
		oracool::ToggleEventLog();
	// Stores go through StoreESC(), the same path Escape uses - so a store closes the way it always
	// has, one level at a time out of a nested menu, rather than being torn down from outside.
	if (stextflag != TalkID::None)
		StoreESC();
	HelpFlag = false;
	ChatLogFlag = false;
	spselflag = false;
	if (qtextflag) {
		qtextflag = false;
		stream_stop();
	}
	if (talkflag)
		control_reset_talk();
	AutomapActive = false;
	CancelCurrentDiabloMsg();
	doom_close();
}

void InitKeymapActions()
{
	// Oracool: HUD overhaul - only belt slots 1-4 are real item slots now (0 is the Menu button, 5
	// is the permanent Town Portal button, 6/7 are hidden - see hud_layout.h's IsRealBeltItemSlot),
	// so keys '1'-'4' map straight to SpdList[1..4].
	for (int i = 1; i <= 4; ++i) {
		sgOptions.Keymapper.AddAction(
		    "BeltItem{}",
		    N_("Belt item {}"),
		    N_("Use Belt item."),
		    '0' + i,
		    [i] {
			    Player &myPlayer = *MyPlayer;
			    if (!myPlayer.SpdList[i].isEmpty() && myPlayer.SpdList[i]._itype != ItemType::Gold) {
				    UseInvItem(INVITEM_BELT_FIRST + i);
			    }
		    },
		    nullptr,
		    CanPlayerTakeAction,
		    i);
	}
	for (size_t i = 0; i < NumHotkeys; ++i) {
		sgOptions.Keymapper.AddAction(
		    "QuickSpell{}",
		    N_("Quick spell {}"),
		    N_("Hotkey for skill or spell."),
		    // Default-unbound since 2026-08-17: F1-F6 are the reserved ability hotkeys, handled
		    // ahead of the keymapper (see PressKey), and the old F5-F8 defaults would have aliased
		    // slots 0-3 onto two keys each.
		    static_cast<uint32_t>(SDLK_UNKNOWN),
		    [i]() {
			    if (spselflag) {
				    SetSpeedSpell(i);
				    return;
			    }
			    if (!*sgOptions.Gameplay.quickCast)
				    ToggleSpell(i);
			    else
				    QuickCast(i);
		    },
		    nullptr,
		    CanPlayerTakeAction,
		    i + 1);
	}
	sgOptions.Keymapper.AddAction(
	    "UseHealthPotion",
	    N_("Use health potion"),
	    N_("Use health potions from belt."),
	    SDLK_UNKNOWN,
	    [] { UseBeltItem(BLT_HEALING); },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Keymapper.AddAction(
	    "UseManaPotion",
	    N_("Use mana potion"),
	    N_("Use mana potions from belt."),
	    SDLK_UNKNOWN,
	    [] { UseBeltItem(BLT_MANA); },
	    nullptr,
	    CanPlayerTakeAction);
	// Oracool: user request (2026-08-16) - "i want the town portal to have a hotkey T just like
	// Diablo 3. To be settable in the keymapping settings." Same cast the belt's Portal button
	// throws (free, always available, single-player only - see CastTownPortalAtFeet), so the key
	// and the button can never disagree about what a portal costs or where it opens.
	sgOptions.Keymapper.AddAction(
	    "TownPortal",
	    N_("Town portal"),
	    N_("Open a Town Portal at your feet."),
	    'T',
	    [] { oracool::CastTownPortalAtFeet(); },
	    nullptr,
	    CanPlayerTakeAction);
	// Oracool Phase 2.5: the run toggle - Run In Town's double-speed frame skip, everywhere,
	// flipped by one key. See oracool/run_toggle.h.
	sgOptions.Keymapper.AddAction(
	    "ToggleRun",
	    N_("Toggle run"),
	    N_("Switch between walking and running."),
	    'R',
	    [] { oracool::ToggleRun(); },
	    nullptr,
	    CanPlayerTakeAction);
	// Oracool: user request (2026-08-20) - the runeword book. A KEY rather than a burger-menu entry
	// because MenuEntries is locked to the row order of menu_icons.png, so adding an entry there
	// needs the sheet recut first - the same blocker directive point 8 is waiting on.
	//
	// W for runeWord. It shipped on B first, which SpellBook already owns further down this same
	// function - the keymapper took both without complaint and the later registration won, so the
	// key simply opened the spellbook and the new action was unreachable. Bound keys as of now:
	// B C F G I L P Q R S T V X Z.
	sgOptions.Keymapper.AddAction(
	    "RunewordBook",
	    N_("Runeword book"),
	    N_("Open the runeword reference."),
	    'W',
	    [] { oracool::ToggleRunewordBook(); },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Keymapper.AddAction(
	    "DisplaySpells",
	    N_("Abilities"),
	    N_("Open the Abilities window."),
	    'S',
	    DisplaySpellsKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
#ifndef NOEXIT
	sgOptions.Keymapper.AddAction(
	    "QuitGame",
	    N_("Quit game"),
	    N_("Closes the game."),
	    SDLK_UNKNOWN,
	    [] { gamemenu_quit_game(false); });
#endif
	sgOptions.Keymapper.AddAction(
	    "StopHero",
	    N_("Stop hero"),
	    N_("Stops walking and cancel pending actions."),
	    SDLK_UNKNOWN,
	    [] { MyPlayer->Stop(); },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Keymapper.AddAction(
	    "Item Highlighting",
	    N_("Item highlighting"),
	    N_("Show/hide items on ground."),
	    SDLK_LALT,
	    [] { HighlightKeyPressed(true); },
	    [] { HighlightKeyPressed(false); });
	sgOptions.Keymapper.AddAction(
	    "Toggle Item Highlighting",
	    N_("Toggle item highlighting"),
	    N_("Permanent show/hide items on ground."),
	    SDLK_RCTRL,
	    nullptr,
	    [] { ToggleItemLabelHighlight(); });
	sgOptions.Keymapper.AddAction(
	    "Toggle Automap",
	    N_("Toggle automap"),
	    N_("Toggles if automap is displayed."),
	    SDLK_TAB,
	    DoAutoMap,
	    nullptr,
	    IsGameRunning);

	sgOptions.Keymapper.AddAction(
	    "Inventory",
	    N_("Inventory"),
	    N_("Open Inventory screen."),
	    'I',
	    InventoryKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Keymapper.AddAction(
	    "Character",
	    N_("Character"),
	    N_("Open Character screen."),
	    'C',
	    CharacterSheetKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Keymapper.AddAction(
	    "QuestLog",
	    N_("Quest log"),
	    N_("Open Quest log."),
	    'Q',
	    QuestLogKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Keymapper.AddAction(
	    "SpellBook",
	    N_("Spellbook"),
	    N_("Open Spellbook."),
	    'B',
	    SpellBookKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	for (int i = 0; i < 4; ++i) {
		sgOptions.Keymapper.AddAction(
		    "QuickMessage{}",
		    N_("Quick Message {}"),
		    N_("Use Quick Message in chat."),
		    // Default-unbound since 2026-08-18: F9-F12 are reserved (game speed, log, screenshot) and
		    // handled ahead of the keymapper, so the old F9..F12 defaults would have been dead rows
		    // that looked bound. The action stays listed for anyone who wants it on other keys.
		    static_cast<uint32_t>(SDLK_UNKNOWN),
		    [i]() { DiabloHotkeyMsg(i); },
		    nullptr,
		    nullptr,
		    i + 1);
	}
	sgOptions.Keymapper.AddAction(
	    "Hide Info Screens",
	    N_("Hide Info Screens"),
	    N_("Hide all info screens."),
	    SDLK_SPACE,
	    [] {
		    ClosePanels();
		    HelpFlag = false;
		    ChatLogFlag = false;
		    spselflag = false;
		    if (qtextflag && leveltype == DTYPE_TOWN) {
			    qtextflag = false;
			    stream_stop();
		    }
		    AutomapActive = false;
		    // Oracool: MiniMapActive is no longer a toggle state (see DrawMiniMap, automap.cpp) -
		    // nothing to force off here, the mini-map option controls it independently of Hide
		    // Info Screens, matching the "unturn-offable except via the INI option" design.
		    CancelCurrentDiabloMsg();
		    gamemenu_off();
		    doom_close();
	    },
	    nullptr,
	    IsGameRunning);
	sgOptions.Keymapper.AddAction(
	    "Zoom",
	    N_("Zoom"),
	    N_("Zoom Game Screen."),
	    'Z',
	    // Oracool: retired the old on/off Graphics.zoom in favor of a continuous dungeon-view zoom
	    // (mouse wheel / middle-click) - this hotkey now jumps to whichever limit it isn't at.
	    ToggleDungeonZoom,
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Keymapper.AddAction(
	    "Pause Game",
	    N_("Pause Game"),
	    N_("Pauses the game."),
	    'P',
	    diablo_pause_game);
	sgOptions.Keymapper.AddAction(
	    "Pause Game (Alternate)",
	    N_("Pause Game (Alternate)"),
	    N_("Pauses the game."),
	    SDLK_PAUSE,
	    diablo_pause_game);
	sgOptions.Keymapper.AddAction(
	    "DecreaseGamma",
	    N_("Decrease Gamma"),
	    N_("Reduce screen brightness."),
	    'G',
	    DecreaseGamma,
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Keymapper.AddAction(
	    "IncreaseGamma",
	    N_("Increase Gamma"),
	    N_("Increase screen brightness."),
	    'F',
	    IncreaseGamma,
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Keymapper.AddAction(
	    "Help",
	    N_("Help"),
	    N_("Open Help Screen."),
	    // Default-unbound since 2026-08-17: F1 belongs to the reserved ability hotkeys. The action
	    // stays keymappable for anyone who wants the help screen on another key.
	    SDLK_UNKNOWN,
	    HelpKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Keymapper.AddAction(
	    "Screenshot",
	    N_("Screenshot"),
	    N_("Takes a screenshot."),
	    SDLK_PRINTSCREEN,
	    nullptr,
	    CaptureScreen);
	sgOptions.Keymapper.AddAction(
	    "GameInfo",
	    N_("Game info"),
	    N_("Displays game infos."),
	    'V',
	    [] {
		    // Oracool: this used to print PROJECT_NAME + PROJECT_VERSION, which is only the
		    // DevilutionX engine version (e.g. "1.5.5") - never the Oracool release number.
		    // gszProductName is already built by SetApplicationVersions() from ORACOOL_VERSION,
		    // so reusing it here keeps this message correct across every version bump for free.
		    EventPlrMsg(gszProductName, UiFlags::ColorWhite);
	    },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Keymapper.AddAction(
	    "ChatLog",
	    N_("Chat Log"),
	    N_("Displays chat log."),
	    'L',
	    [] {
		    ToggleChatLog();
	    });
#ifdef _DEBUG
	sgOptions.Keymapper.AddAction(
	    "DebugToggle",
	    "Debug toggle",
	    "Programming is like magic.",
	    'X',
	    [] {
		    DebugToggle = !DebugToggle;
	    });
#endif
	sgOptions.Keymapper.CommitActions();
}

void InitPadmapActions()
{
	// Oracool: HUD overhaul - see InitKeymapActions' matching comment above.
	for (int i = 1; i <= 4; ++i) {
		sgOptions.Padmapper.AddAction(
		    "BeltItem{}",
		    N_("Belt item {}"),
		    N_("Use Belt item."),
		    ControllerButton_NONE,
		    [i] {
			    Player &myPlayer = *MyPlayer;
			    if (!myPlayer.SpdList[i].isEmpty() && myPlayer.SpdList[i]._itype != ItemType::Gold) {
				    UseInvItem(INVITEM_BELT_FIRST + i);
			    }
		    },
		    nullptr,
		    CanPlayerTakeAction,
		    i);
	}
	for (size_t i = 0; i < NumHotkeys; ++i) {
		sgOptions.Padmapper.AddAction(
		    "QuickSpell{}",
		    N_("Quick spell {}"),
		    N_("Hotkey for skill or spell."),
		    ControllerButton_NONE,
		    [i]() {
			    if (spselflag) {
				    SetSpeedSpell(i);
				    return;
			    }
			    if (!*sgOptions.Gameplay.quickCast)
				    ToggleSpell(i);
			    else
				    QuickCast(i);
		    },
		    nullptr,
		    CanPlayerTakeAction,
		    i + 1);
	}
	sgOptions.Padmapper.AddAction(
	    "PrimaryAction",
	    N_("Primary action"),
	    N_("Attack monsters, talk to towners, lift and place inventory items."),
	    ControllerButton_BUTTON_B,
	    [] {
		    ControllerActionHeld = GameActionType_PRIMARY_ACTION;
		    LastMouseButtonAction = MouseActionType::None;
		    PerformPrimaryAction();
	    },
	    [] {
		    ControllerActionHeld = GameActionType_NONE;
		    LastMouseButtonAction = MouseActionType::None;
	    },
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "SecondaryAction",
	    N_("Secondary action"),
	    N_("Open chests, interact with doors, pick up items."),
	    ControllerButton_BUTTON_Y,
	    [] {
		    ControllerActionHeld = GameActionType_SECONDARY_ACTION;
		    LastMouseButtonAction = MouseActionType::None;
		    PerformSecondaryAction();
	    },
	    [] {
		    ControllerActionHeld = GameActionType_NONE;
		    LastMouseButtonAction = MouseActionType::None;
	    },
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "SpellAction",
	    N_("Spell action"),
	    N_("Cast the active spell."),
	    ControllerButton_BUTTON_X,
	    [] {
		    ControllerActionHeld = GameActionType_CAST_SPELL;
		    LastMouseButtonAction = MouseActionType::None;
		    PerformSpellAction();
	    },
	    [] {
		    ControllerActionHeld = GameActionType_NONE;
		    LastMouseButtonAction = MouseActionType::None;
	    },
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "CancelAction",
	    N_("Cancel action"),
	    N_("Close menus."),
	    ControllerButton_BUTTON_A,
	    [] {
		    if (DoomFlag) {
			    doom_close();
			    return;
		    }

		    GameAction action;
		    if (spselflag)
			    action = GameAction(GameActionType_TOGGLE_QUICK_SPELL_MENU);
		    else if (invflag)
			    action = GameAction(GameActionType_TOGGLE_INVENTORY);
		    else if (sbookflag)
			    action = GameAction(GameActionType_TOGGLE_SPELL_BOOK);
		    else if (QuestLogIsOpen)
			    action = GameAction(GameActionType_TOGGLE_QUEST_LOG);
		    else if (chrflag)
			    action = GameAction(GameActionType_TOGGLE_CHARACTER_INFO);
		    ProcessGameAction(action);
	    },
	    nullptr,
	    [] { return DoomFlag || spselflag || invflag || sbookflag || QuestLogIsOpen || chrflag; });
	sgOptions.Padmapper.AddAction(
	    "MoveUp",
	    N_("Move up"),
	    N_("Moves the player character up."),
	    ControllerButton_BUTTON_DPAD_UP,
	    [] {});
	sgOptions.Padmapper.AddAction(
	    "MoveDown",
	    N_("Move down"),
	    N_("Moves the player character down."),
	    ControllerButton_BUTTON_DPAD_DOWN,
	    [] {});
	sgOptions.Padmapper.AddAction(
	    "MoveLeft",
	    N_("Move left"),
	    N_("Moves the player character left."),
	    ControllerButton_BUTTON_DPAD_LEFT,
	    [] {});
	sgOptions.Padmapper.AddAction(
	    "MoveRight",
	    N_("Move right"),
	    N_("Moves the player character right."),
	    ControllerButton_BUTTON_DPAD_RIGHT,
	    [] {});
	sgOptions.Padmapper.AddAction(
	    "StandGround",
	    N_("Stand ground"),
	    N_("Hold to prevent the player from moving."),
	    ControllerButton_NONE,
	    [] {});
	sgOptions.Padmapper.AddAction(
	    "ToggleStandGround",
	    N_("Toggle stand ground"),
	    N_("Toggle whether the player moves."),
	    ControllerButton_NONE,
	    [] { StandToggle = !StandToggle; },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "UseHealthPotion",
	    N_("Use health potion"),
	    N_("Use health potions from belt."),
	    ControllerButton_BUTTON_LEFTSHOULDER,
	    [] { UseBeltItem(BLT_HEALING); },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "UseManaPotion",
	    N_("Use mana potion"),
	    N_("Use mana potions from belt."),
	    ControllerButton_BUTTON_RIGHTSHOULDER,
	    [] { UseBeltItem(BLT_MANA); },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "Character",
	    N_("Character"),
	    N_("Open Character screen."),
	    ControllerButton_AXIS_TRIGGERLEFT,
	    [] {
		    ProcessGameAction(GameAction { GameActionType_TOGGLE_CHARACTER_INFO });
	    });
	sgOptions.Padmapper.AddAction(
	    "Inventory",
	    N_("Inventory"),
	    N_("Open Inventory screen."),
	    ControllerButton_AXIS_TRIGGERRIGHT,
	    [] {
		    ProcessGameAction(GameAction { GameActionType_TOGGLE_INVENTORY });
	    },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "QuestLog",
	    N_("Quest log"),
	    N_("Open Quest log."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_AXIS_TRIGGERLEFT },
	    [] {
		    ProcessGameAction(GameAction { GameActionType_TOGGLE_QUEST_LOG });
	    },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "SpellBook",
	    N_("Spellbook"),
	    N_("Open Spellbook."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_AXIS_TRIGGERRIGHT },
	    [] {
		    ProcessGameAction(GameAction { GameActionType_TOGGLE_SPELL_BOOK });
	    },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "DisplaySpells",
	    N_("Abilities"),
	    N_("Open the Abilities window."),
	    ControllerButton_BUTTON_A,
	    [] {
		    ProcessGameAction(GameAction { GameActionType_TOGGLE_QUICK_SPELL_MENU });
	    },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "Toggle Automap",
	    N_("Toggle automap"),
	    N_("Toggles if automap is displayed."),
	    ControllerButton_BUTTON_LEFTSTICK,
	    DoAutoMap);
	sgOptions.Padmapper.AddAction(
	    "MouseUp",
	    N_("Move mouse up"),
	    N_("Simulates upward mouse movement."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_DPAD_UP },
	    [] {});
	sgOptions.Padmapper.AddAction(
	    "MouseDown",
	    N_("Move mouse down"),
	    N_("Simulates downward mouse movement."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_DPAD_DOWN },
	    [] {});
	sgOptions.Padmapper.AddAction(
	    "MouseLeft",
	    N_("Move mouse left"),
	    N_("Simulates leftward mouse movement."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_DPAD_LEFT },
	    [] {});
	sgOptions.Padmapper.AddAction(
	    "MouseRight",
	    N_("Move mouse right"),
	    N_("Simulates rightward mouse movement."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_DPAD_RIGHT },
	    [] {});
	auto leftMouseDown = [] {
		ControllerButtonCombo standGroundCombo = sgOptions.Padmapper.ButtonComboForAction("StandGround");
		bool standGround = StandToggle || IsControllerButtonComboPressed(standGroundCombo);
		sgbMouseDown = CLICK_LEFT;
		LeftMouseDown(standGround ? KMOD_SHIFT : KMOD_NONE);
	};
	auto leftMouseUp = [] {
		ControllerButtonCombo standGroundCombo = sgOptions.Padmapper.ButtonComboForAction("StandGround");
		bool standGround = StandToggle || IsControllerButtonComboPressed(standGroundCombo);
		LastMouseButtonAction = MouseActionType::None;
		sgbMouseDown = CLICK_NONE;
		LeftMouseUp(standGround ? KMOD_SHIFT : KMOD_NONE);
	};
	sgOptions.Padmapper.AddAction(
	    "LeftMouseClick1",
	    N_("Left mouse click"),
	    N_("Simulates the left mouse button."),
	    ControllerButton_BUTTON_RIGHTSTICK,
	    leftMouseDown,
	    leftMouseUp);
	sgOptions.Padmapper.AddAction(
	    "LeftMouseClick2",
	    N_("Left mouse click"),
	    N_("Simulates the left mouse button."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_LEFTSHOULDER },
	    leftMouseDown,
	    leftMouseUp);
	auto rightMouseDown = [] {
		ControllerButtonCombo standGroundCombo = sgOptions.Padmapper.ButtonComboForAction("StandGround");
		bool standGround = StandToggle || IsControllerButtonComboPressed(standGroundCombo);
		LastMouseButtonAction = MouseActionType::None;
		sgbMouseDown = CLICK_RIGHT;
		RightMouseDown(standGround);
	};
	auto rightMouseUp = [] {
		LastMouseButtonAction = MouseActionType::None;
		sgbMouseDown = CLICK_NONE;
	};
	sgOptions.Padmapper.AddAction(
	    "RightMouseClick1",
	    N_("Right mouse click"),
	    N_("Simulates the right mouse button."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_RIGHTSTICK },
	    rightMouseDown,
	    rightMouseUp);
	sgOptions.Padmapper.AddAction(
	    "RightMouseClick2",
	    N_("Right mouse click"),
	    N_("Simulates the right mouse button."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_RIGHTSHOULDER },
	    rightMouseDown,
	    rightMouseUp);
	sgOptions.Padmapper.AddAction(
	    "PadHotspellMenu",
	    N_("Gamepad hotspell menu"),
	    N_("Hold to set or use spell hotkeys."),
	    ControllerButton_BUTTON_BACK,
	    [] { PadHotspellMenuActive = true; },
	    [] { PadHotspellMenuActive = false; });
	sgOptions.Padmapper.AddAction(
	    "PadMenuNavigator",
	    N_("Gamepad menu navigator"),
	    N_("Hold to access gamepad menu navigation."),
	    ControllerButton_BUTTON_START,
	    [] { PadMenuNavigatorActive = true; },
	    [] { PadMenuNavigatorActive = false; });
	auto toggleGameMenu = [] {
		bool inMenu = gmenu_is_active();
		PressEscKey();
		LastMouseButtonAction = MouseActionType::None;
		PadHotspellMenuActive = false;
		PadMenuNavigatorActive = false;
		if (!inMenu)
			gamemenu_on();
	};
	sgOptions.Padmapper.AddAction(
	    "ToggleGameMenu1",
	    N_("Toggle game menu"),
	    N_("Opens the game menu."),
	    {
	        ControllerButton_BUTTON_BACK,
	        ControllerButton_BUTTON_START,
	    },
	    toggleGameMenu);
	sgOptions.Padmapper.AddAction(
	    "ToggleGameMenu2",
	    N_("Toggle game menu"),
	    N_("Opens the game menu."),
	    {
	        ControllerButton_BUTTON_START,
	        ControllerButton_BUTTON_BACK,
	    },
	    toggleGameMenu);
	sgOptions.Padmapper.AddAction(
	    "Item Highlighting",
	    N_("Item highlighting"),
	    N_("Show/hide items on ground."),
	    ControllerButton_NONE,
	    [] { HighlightKeyPressed(true); },
	    [] { HighlightKeyPressed(false); });
	sgOptions.Padmapper.AddAction(
	    "Toggle Item Highlighting",
	    N_("Toggle item highlighting"),
	    N_("Permanent show/hide items on ground."),
	    ControllerButton_NONE,
	    nullptr,
	    [] { ToggleItemLabelHighlight(); });
	sgOptions.Padmapper.AddAction(
	    "Hide Info Screens",
	    N_("Hide Info Screens"),
	    N_("Hide all info screens."),
	    ControllerButton_NONE,
	    [] {
		    ClosePanels();
		    HelpFlag = false;
		    ChatLogFlag = false;
		    spselflag = false;
		    if (qtextflag && leveltype == DTYPE_TOWN) {
			    qtextflag = false;
			    stream_stop();
		    }
		    AutomapActive = false;
		    // Oracool: MiniMapActive is no longer a toggle state (see DrawMiniMap, automap.cpp) -
		    // nothing to force off here, the mini-map option controls it independently of Hide
		    // Info Screens, matching the "unturn-offable except via the INI option" design.
		    CancelCurrentDiabloMsg();
		    gamemenu_off();
		    doom_close();
	    },
	    nullptr,
	    IsGameRunning);
	sgOptions.Padmapper.AddAction(
	    "Zoom",
	    N_("Zoom"),
	    N_("Zoom Game Screen."),
	    ControllerButton_NONE,
	    // Oracool: retired the old on/off Graphics.zoom in favor of a continuous dungeon-view zoom
	    // (mouse wheel / middle-click) - this action now jumps to whichever limit it isn't at.
	    ToggleDungeonZoom,
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "Pause Game",
	    N_("Pause Game"),
	    N_("Pauses the game."),
	    ControllerButton_NONE,
	    diablo_pause_game);
	sgOptions.Padmapper.AddAction(
	    "DecreaseGamma",
	    N_("Decrease Gamma"),
	    N_("Reduce screen brightness."),
	    ControllerButton_NONE,
	    DecreaseGamma,
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "IncreaseGamma",
	    N_("Increase Gamma"),
	    N_("Increase screen brightness."),
	    ControllerButton_NONE,
	    IncreaseGamma,
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "Help",
	    N_("Help"),
	    N_("Open Help Screen."),
	    ControllerButton_NONE,
	    HelpKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "Screenshot",
	    N_("Screenshot"),
	    N_("Takes a screenshot."),
	    ControllerButton_NONE,
	    nullptr,
	    CaptureScreen);
	sgOptions.Padmapper.AddAction(
	    "GameInfo",
	    N_("Game info"),
	    N_("Displays game infos."),
	    ControllerButton_NONE,
	    [] {
		    // Oracool: this used to print PROJECT_NAME + PROJECT_VERSION, which is only the
		    // DevilutionX engine version (e.g. "1.5.5") - never the Oracool release number.
		    // gszProductName is already built by SetApplicationVersions() from ORACOOL_VERSION,
		    // so reusing it here keeps this message correct across every version bump for free.
		    EventPlrMsg(gszProductName, UiFlags::ColorWhite);
	    },
	    nullptr,
	    CanPlayerTakeAction);
	sgOptions.Padmapper.AddAction(
	    "ChatLog",
	    N_("Chat Log"),
	    N_("Displays chat log."),
	    ControllerButton_NONE,
	    [] {
		    ToggleChatLog();
	    });
	sgOptions.Padmapper.CommitActions();
}

void SetCursorPos(Point position)
{
	if (ControlDevice != ControlTypes::KeyboardAndMouse) {
		MousePosition = position;
		return;
	}

	LogicalToOutput(&position.x, &position.y);
	if (!demo::IsRunning())
		SDL_WarpMouseInWindow(ghMainWnd, position.x, position.y);
}

void FreeGameMem()
{
	pDungeonCels = nullptr;
	pMegaTiles = nullptr;
	pSpecialCels = std::nullopt;

	FreeMonsters();
	FreeMissileGFX();
	FreeObjectGFX();
	FreeTownerGFX();
	FreeStashGFX();
#ifndef USE_SDL1
	DeactivateVirtualGamepad();
	FreeVirtualGamepadGFX();
#endif
}

bool StartGame(bool bNewGame, bool bSinglePlayer)
{
	gbSelectProvider = true;
	ReturnToMainMenu = false;

	do {
		gbLoadGame = false;

		if (!NetInit(bSinglePlayer)) {
			gbRunGameResult = true;
			break;
		}

		// Save 2.8 MiB of RAM by freeing all main menu resources
		// before starting the game.
		UiDestroy();

		gbSelectProvider = false;

		if (bNewGame || !gbValidSaveFile) {
			InitLevels();
			InitQuests();
			InitPortals();
			InitDungMsgs(*MyPlayer);
			DeltaSyncJunk();
		}
		giNumberOfLevels = gbIsHellfire ? 25 : 17;
		interface_mode uMsg = WM_DIABNEWGAME;
		if (gbValidSaveFile && gbLoadGame) {
			uMsg = WM_DIABLOADGAME;
		}
		RunGameLoop(uMsg);
		NetClose();
		UnloadFonts();

		// If the player left the game into the main menu,
		// initialize main menu resources.
		if (gbRunGameResult)
			UiInitialize();
		if (ReturnToMainMenu)
			return true;
	} while (gbRunGameResult);

	SNetDestroy();
	return gbRunGameResult;
}

void diablo_quit(int exitStatus)
{
	// Oracool (audit, 2026-08-26): closing the window is a way of leaving the game, and it used to
	// be the one way that saved nothing. SDL_WINDOWEVENT_CLOSE lands here and this function went
	// straight to exit(), so a single-player character who clicked the X lost everything since the
	// last periodic autosave.
	//
	// ONLY on a clean exit. appfat() also arrives here, with status 1, after the game has decided
	// its own state is broken - writing a character out of a state we have just declared invalid is
	// how a crash becomes a corrupt save. A crash costing the last few minutes is the correct
	// trade; SaveOnExit's own guards (gbRunGame, MyPlayer, demo mode) handle the rest.
	if (exitStatus == 0)
		oracool::SaveOnExit();

	FreeGameMem();
	music_stop();

	// The watchdog is armed inside DiabloDeinit itself, not here - see the note there. Arming it at
	// this call site only was the v1.9.137 mistake: it is the path a player does NOT take.
	DiabloDeinit();
	exit(exitStatus);
}

#ifdef __UWP__
void (*onInitialized)() = NULL;

void setOnInitialized(void (*callback)())
{
	onInitialized = callback;
}
#endif

int DiabloMain(int argc, char **argv)
{
#ifdef _DEBUG
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#endif

	DiabloParseFlags(argc, argv);
	InitKeymapActions();
	InitPadmapActions();

	// Need to ensure devilutionx.mpq (and fonts.mpq if available) are loaded before attempting to read translation settings
	LoadCoreArchives();
	was_archives_init = true;

	// Read settings including translation next. This will use the presence of fonts.mpq and look for assets in devilutionx.mpq
	LoadOptions();
	// Then look for a voice pack file based on the selected translation
	LoadLanguageArchive();

	ApplicationInit();
	SaveOptions();

	// Finally load game data
	LoadGameArchives();

	DiabloInit();
#ifdef __UWP__
	onInitialized();
#endif
	SaveOptions();

	DiabloSplash();
	mainmenu_loop();
	DiabloDeinit();

	return 0;
}

bool TryIconCurs()
{
	if (pcurs == CURSOR_RESURRECT) {
		if (pcursplr != -1) {
			NetSendCmdParam1(true, CMD_RESURRECT, pcursplr);
			NewCursor(CURSOR_HAND);
			return true;
		}

		return false;
	}

	if (pcurs == CURSOR_HEALOTHER) {
		if (pcursplr != -1) {
			NetSendCmdParam1(true, CMD_HEALOTHER, pcursplr);
			NewCursor(CURSOR_HAND);
			return true;
		}

		return false;
	}

	if (pcurs == CURSOR_TELEKINESIS) {
		DoTelekinesis();
		return true;
	}

	Player &myPlayer = *MyPlayer;

	if (pcurs == CURSOR_IDENTIFY) {
		if (pcursinvitem != -1 && !IsInspectingPlayer())
			CheckIdentify(myPlayer, pcursinvitem);
		else if (pcursinvtabitem != -1 && !IsInspectingPlayer())
			CheckIdentify(myPlayer, pcursinvtabitem, pcursinvtabidx);
		else if (pcursstashitem != StashStruct::EmptyCell) {
			Item &item = Stash.stashList[pcursstashitem];
			item._iIdentified = true;
		}
		NewCursor(CURSOR_HAND);
		return true;
	}

	if (pcurs == CURSOR_REPAIR) {
		// The SHOP's hammer borrows this whole mechanic - the cursor, the targeting, the routing
		// below - and differs only in what the click does: full durability, and paid for. See
		// ArmShopRepairCursor.
		if (IsShopRepairCursorArmed()) {
			if (pcursinvitem != -1 && !IsInspectingPlayer()) {
				ShopRepairItemAt(GetActiveInvListItem(myPlayer, pcursinvitem - INVITEM_INV_FIRST));
			} else if (pcursinvtabitem != -1 && !IsInspectingPlayer()) {
				ShopRepairItemAt(myPlayer.InvTabList[pcursinvtabidx][pcursinvtabitem]);
			} else if (pcursstashitem != StashStruct::EmptyCell) {
				ShopRepairItemAt(Stash.stashList[pcursstashitem]);
			}
			DisarmShopServiceCursor();
			CalcPlrInv(myPlayer, true);
			NewCursor(CURSOR_HAND);
			return true;
		}
		// A hammer left over from a shop that has already closed - see
		// ConsumeStaleShopServiceCursor. It must never fall through to the vanilla skill below,
		// which repairs by permanently reducing maximum durability.
		if (ConsumeStaleShopServiceCursor()) {
			NewCursor(CURSOR_HAND);
			return true;
		}
		if (pcursinvitem != -1 && !IsInspectingPlayer())
			DoRepair(myPlayer, pcursinvitem);
		else if (pcursinvtabitem != -1 && !IsInspectingPlayer())
			DoRepair(myPlayer, pcursinvtabitem, pcursinvtabidx);
		else if (pcursstashitem != StashStruct::EmptyCell) {
			Item &item = Stash.stashList[pcursstashitem];
			RepairItem(item, myPlayer._pLevel);
		}
		NewCursor(CURSOR_HAND);
		return true;
	}

	if (pcurs == CURSOR_RECHARGE) {
		// Adria's Recharge button borrows this mechanic the same way the shop's hammer borrows
		// Repair's - same cursor, same targeting, same routing; the click is what differs. See
		// ArmShopRechargeCursor.
		if (IsShopRechargeCursorArmed()) {
			if (pcursinvitem != -1 && !IsInspectingPlayer()) {
				ShopRechargeItemAt(GetActiveInvListItem(myPlayer, pcursinvitem - INVITEM_INV_FIRST));
			} else if (pcursinvtabitem != -1 && !IsInspectingPlayer()) {
				ShopRechargeItemAt(myPlayer.InvTabList[pcursinvtabidx][pcursinvtabitem]);
			} else if (pcursstashitem != StashStruct::EmptyCell) {
				ShopRechargeItemAt(Stash.stashList[pcursstashitem]);
			}
			DisarmShopServiceCursor();
			CalcPlrInv(myPlayer, true);
			NewCursor(CURSOR_HAND);
			return true;
		}
		// The Recharge twin of the hammer's guard above: the vanilla skill below recharges by
		// permanently reducing maximum charges.
		if (ConsumeStaleShopServiceCursor()) {
			NewCursor(CURSOR_HAND);
			return true;
		}
		if (pcursinvitem != -1 && !IsInspectingPlayer())
			DoRecharge(myPlayer, pcursinvitem);
		else if (pcursinvtabitem != -1 && !IsInspectingPlayer())
			DoRecharge(myPlayer, pcursinvtabitem, pcursinvtabidx);
		else if (pcursstashitem != StashStruct::EmptyCell) {
			Item &item = Stash.stashList[pcursstashitem];
			RechargeItem(item, myPlayer);
		}
		NewCursor(CURSOR_HAND);
		return true;
	}

	if (pcurs == CURSOR_OIL) {
		bool changeCursor = true;
		if (pcursinvitem != -1 && !IsInspectingPlayer())
			changeCursor = DoOil(myPlayer, pcursinvitem);
		else if (pcursinvtabitem != -1 && !IsInspectingPlayer())
			changeCursor = DoOil(myPlayer, pcursinvtabitem, pcursinvtabidx);
		else if (pcursstashitem != StashStruct::EmptyCell) {
			Item &item = Stash.stashList[pcursstashitem];
			changeCursor = ApplyOilToItem(item, myPlayer);
		}
		if (changeCursor)
			NewCursor(CURSOR_HAND);
		return true;
	}

	if (pcurs == CURSOR_TELEPORT) {
		const SpellID spellID = myPlayer.inventorySpell;
		const SpellType spellType = SpellType::Scroll;
		const int spellFrom = myPlayer.spellFrom;
		if (IsWallSpell(spellID)) {
			Direction sd = GetDirection(myPlayer.position.tile, cursPosition);
			NetSendCmdLocParam4(true, CMD_SPELLXYD, cursPosition, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), static_cast<uint16_t>(sd), spellFrom);
		} else if (pcursmonst != -1) {
			NetSendCmdParam4(true, CMD_SPELLID, pcursmonst, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
		} else if (pcursplr != -1 && !myPlayer.friendlyMode) {
			NetSendCmdParam4(true, CMD_SPELLPID, pcursplr, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
		} else {
			NetSendCmdLocParam3(true, CMD_SPELLXY, cursPosition, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
		}
		NewCursor(CURSOR_HAND);
		return true;
	}

	if (pcurs == CURSOR_DISARM && ObjectUnderCursor == nullptr) {
		NewCursor(CURSOR_HAND);
		return true;
	}

	return false;
}

void diablo_pause_game()
{
	if (!gbIsMultiplayer) {
		if (PauseMode != 0) {
			PauseMode = 0;
		} else {
			PauseMode = 2;
			sound_stop();
			qtextflag = false;
			LastMouseButtonAction = MouseActionType::None;
		}

		RedrawEverything();
	}
}

bool GameWasAlreadyPaused = false;
bool MinimizePaused = false;

bool diablo_is_focused()
{
#ifndef USE_SDL1
	return SDL_GetKeyboardFocus() == ghMainWnd;
#else
	Uint8 appState = SDL_GetAppState();
	return (appState & SDL_APPINPUTFOCUS) != 0;
#endif
}

void diablo_focus_pause()
{
	if (!movie_playing && (gbIsMultiplayer || MinimizePaused)) {
		return;
	}

	GameWasAlreadyPaused = PauseMode != 0;

	if (!GameWasAlreadyPaused) {
		PauseMode = 2;
		sound_stop();
		LastMouseButtonAction = MouseActionType::None;
	}

	SVidMute();
	music_mute();

	MinimizePaused = true;
}

void diablo_focus_unpause()
{
	if (!GameWasAlreadyPaused) {
		PauseMode = 0;
	}

	SVidUnmute();
	music_unmute();

	MinimizePaused = false;
}

bool PressEscKey()
{
	bool rv = false;

	// Oracool: checked first, with an immediate return - the Refresh Until prompt stays open
	// while its underlying store screen is still technically open (stextflag != TalkID::None),
	// unlike every other dialog checked below, so this needs to close *only* the prompt itself
	// rather than also falling through into the stextflag check and closing the whole store.
	if (IsRefreshUntilPromptOpen) {
		RefreshUntilPromptKeyPress(SDLK_ESCAPE);
		return true;
	}

	if (DoomFlag) {
		doom_close();
		rv = true;
	}

	if (oracool::IsHudMenuOpen()) {
		oracool::CloseHudMenu();
		rv = true;
	}

	// Oracool: the monument's window is centred over the world rather than docked to a panel, so
	// none of the panel closers below reach it. Escape is the keyboard half of the close rule the
	// red X covers for the mouse.
	if (oracool::IsLevskiRoarOpen()) {
		oracool::CloseLevskiRoar();
		rv = true;
	}

	// Same reasoning as the monument above: a free-floating window that no panel closer reaches.
	if (oracool::IsRunewordBookOpen()) {
		oracool::CloseRunewordBook();
		rv = true;
	}

	// The skill picker, for the same reason: it floats over the world and no panel closer reaches it.
	if (oracool::IsSkillPickerOpen()) {
		oracool::CloseSkillPicker();
		rv = true;
	}

	if (HelpFlag) {
		HelpFlag = false;
		rv = true;
	}

	if (ChatLogFlag) {
		ChatLogFlag = false;
		rv = true;
	}

	if (qtextflag) {
		qtextflag = false;
		stream_stop();
		rv = true;
	}

	if (stextflag != TalkID::None) {
		StoreESC();
		rv = true;
	}

	if (IsDiabloMsgAvailable()) {
		CancelCurrentDiabloMsg();
		rv = true;
	}

	if (talkflag) {
		control_reset_talk();
		rv = true;
	}

	if (DropGoldFlag) {
		control_drop_gold(SDLK_ESCAPE);
		rv = true;
	}

	if (IsWithdrawGoldOpen) {
		WithdrawGoldKeyPress(SDLK_ESCAPE);
		rv = true;
	}

	if (spselflag) {
		spselflag = false;
		rv = true;
	}

	if (IsLeftPanelOpen() || IsRightPanelOpen()) {
		ClosePanels();
		rv = true;
	}

	return rv;
}

void DisableInputEventHandler(const SDL_Event &event, uint16_t modState)
{
	switch (event.type) {
	// Oracool: user request (2026-08-15) - "enable screenshot taking during loading screens".
	// This handler is what ShowProgress swaps in for the whole load, and it swallowed every key -
	// which made a loading-screen rendering problem impossible to capture with the game's own
	// screenshot key. The same two keys the front end accepts (DiabloUI/diabloui.cpp), for the
	// same reason it accepts them: a screenshot is observation, not input, and the loading screen
	// has nothing an observer could disturb.
	case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_PRINTSCREEN || event.key.keysym.sym == SDLK_F12)
			CaptureScreen();
		return;
	case SDL_MOUSEMOTION:
		MousePosition = { event.motion.x, event.motion.y };
		return;
	case SDL_MOUSEBUTTONDOWN:
		if (sgbMouseDown != CLICK_NONE)
			return;
		switch (event.button.button) {
		case SDL_BUTTON_LEFT:
			sgbMouseDown = CLICK_LEFT;
			return;
		case SDL_BUTTON_RIGHT:
			sgbMouseDown = CLICK_RIGHT;
			return;
		default:
			return;
		}
	case SDL_MOUSEBUTTONUP:
		sgbMouseDown = CLICK_NONE;
		return;
	}

	MainWndProc(event);
}

void LoadGameLevel(bool firstflag, lvl_entry lvldir)
{
	_music_id neededTrack = GetLevelMusic(leveltype);
	ClearFloatingNumbers();

	// A level transition releases the aura's loop handle (the sound package's contract lists it
	// alongside death and disconnect) without its stop cue - the aura itself is still on, and will
	// be re-lit below once the level is up.
	oracool::SilenceAuraLoopForTransition();
	// And re-baseline which sets are complete, silently. Equipment is recomputed all through a level
	// load, and without this the first CalcPlrInv would read every already-worn set as newly
	// finished and ring the stinger for it.
	if (MyPlayer != nullptr)
		oracool::ArmSetCompletionBaseline(*MyPlayer);

	if (neededTrack != sgnMusicTrack)
		music_stop();
	if (pcurs > CURSOR_HAND && pcurs < CURSOR_FIRSTITEM) {
		NewCursor(CURSOR_HAND);
	}
	SetRndSeed(glSeedTbl[currlevel]);
	IncProgress();
	MakeLightTable();
	SetDungeonMicros();
	ClearClxDrawCache();
	LoadLvlGFX();
	IncProgress();

	if (firstflag) {
		CloseInventory();
		qtextflag = false;
		if (!HeadlessMode) {
			InitInv();
			ClearUniqueItemFlags();
			InitQuestText();
			InitInfoBoxGfx();
			InitHelp();
		}
		InitStores();
		InitAutomapOnce();
	}
	if (!setlevel) {
		SetRndSeed(glSeedTbl[currlevel]);
	} else {
		// Maps are not randomly generated, but the monsters max hitpoints are.
		// So we need to ensure that we have a stable seed when generating quest/set-maps.
		// For this purpose we reuse the normal dungeon seeds.
		SetRndSeed(glSeedTbl[static_cast<size_t>(setlvlnum)]);
	}

	if (leveltype == DTYPE_TOWN) {
		SetupTownStores();
	} else {
		FreeStoreMem();
	}

	if (firstflag || lvldir == ENTRY_LOAD) {
		bool isHellfireSaveGame = gbIsHellfireSaveGame;
		gbIsHellfireSaveGame = gbIsHellfire;
		LoadStash();
		gbIsHellfireSaveGame = isHellfireSaveGame;
	}

	IncProgress();
	InitAutomap();

	if (leveltype != DTYPE_TOWN && lvldir != ENTRY_LOAD) {
		InitLighting();
	}

	InitLevelMonsters();
	IncProgress();

	Player &myPlayer = *MyPlayer;

	if (!setlevel) {
		CreateLevel(lvldir);
		IncProgress();
		LoadLevelSOLData();
		SetRndSeed(glSeedTbl[currlevel]);

		if (leveltype != DTYPE_TOWN) {
			GetLevelMTypes();
			InitThemes();
			if (!HeadlessMode)
				LoadAllGFX();
			// Oracool: bug postmortem (2026-08-10) - a revisited dungeon level's waypoint sigil
			// (if any) crashed the game the moment it came into view. Root cause: its graphic
			// (OFILE_ORCLWAYP) is only ever registered by AddWaypointSigilObject(), which - like
			// every level-content placement call - only runs on a fresh level generation, not a
			// revisit (LoadLevel() restores the object itself, but that's a different system from
			// the per-level graphics registry). LoadLevel()'s own SyncObjectAnim() call then can't
			// find the graphic in ObjFileList, logs "Unable to find object_graphic_id" and leaves
			// the object's _oAnimData unset - DrawObject() dereferences that the instant the
			// object is close enough to render. Registering the graphic here runs unconditionally
			// on every entry to a level that could have a waypoint, fresh or revisit, before
			// LoadLevel()'s sync runs. Town's own is covered separately - AddWaypointSigilObject
			// already runs unconditionally there.
			//
			// The bound is Player::MaxWaypointSlots, character for character the same test
			// AddWaypointSigilObject uses to decide whether to PLACE a sigil. It was a literal 16
			// while placement said 16 too; when placement widened to cover Hellfire's Nest (17-20)
			// and Crypt (21-24) this did not follow, so a sigil on those eight levels was placed on
			// a fresh visit and then, on a REVISIT, restored by LoadLevel with no graphic ever
			// registered - which is precisely the crash the paragraph above describes, re-opened on
			// the levels nobody had walked back into yet. Two copies of one bound is what let them
			// drift; there is one now.
			if (currlevel >= 1 && currlevel < static_cast<int>(Player::MaxWaypointSlots))
				oracool::EnsureWaypointGraphicsLoaded();
		} else if (!HeadlessMode) {
			IncProgress();
#if !defined(USE_SDL1) && !defined(__vita__)
			InitVirtualGamepadGFX(renderer);
#endif
			IncProgress();
			InitMissileGFX(gbIsHellfire);
			IncProgress();
			IncProgress();
		}

		IncProgress();

		if (lvldir == ENTRY_RTNLVL) {
			ViewPosition = GetMapReturnPosition();
			if (Quests[Q_BETRAYER]._qactive == QUEST_DONE)
				Quests[Q_BETRAYER]._qvar2 = 2;
		}
		if (lvldir == ENTRY_WARPLVL)
			GetPortalLvlPos();

		IncProgress();

		for (Player &player : Players) {
			if (player.plractive && player.isOnActiveLevel()) {
				InitPlayerGFX(player);
				if (lvldir != ENTRY_LOAD)
					InitPlayer(player, firstflag);
			}
		}

		PlayDungMsgs();
		InitMultiView();
		IncProgress();

		bool visited = false;
		for (const Player &player : Players) {
			if (player.plractive)
				visited = visited || player._pLvlVisited[currlevel];
		}

		SetRndSeed(glSeedTbl[currlevel]);

		if (leveltype != DTYPE_TOWN) {
			if (firstflag || lvldir == ENTRY_LOAD || !myPlayer._pLvlVisited[currlevel] || gbIsMultiplayer) {
				HoldThemeRooms();
				[[maybe_unused]] uint32_t mid1Seed = GetLCGEngineState();
				InitGolems();
				InitObjects();
				[[maybe_unused]] uint32_t mid2Seed = GetLCGEngineState();
				IncProgress();
				InitMonsters();
				InitItems();
				CreateThemeRooms();
				// Oracool: user report - the waypoint is placed back in InitObjects, before this
				// level had any monsters or items to avoid, using the engine's generic
				// "2x2 of floor will do" search. Now that everything is placed, re-pick its spot
				// properly. Fresh generation only: the revisit branch below restores the saved
				// position, which must not move under the player.
				oracool::ImproveWaypointSpawnPosition();
				IncProgress();
				[[maybe_unused]] uint32_t mid3Seed = GetLCGEngineState();
				InitMissiles();
				InitCorpses();
#ifdef _DEBUG
				SetDebugLevelSeedInfos(mid1Seed, mid2Seed, mid3Seed, GetLCGEngineState());
#endif
				SavePreLighting();
				IncProgress();

				if (gbIsMultiplayer)
					DeltaLoadLevel();
			} else {
				HoldThemeRooms();
				InitGolems();
				InitMonsters();
				InitMissiles();
				InitCorpses();
				IncProgress();
				LoadLevel();
				IncProgress();
			}
		} else {
			for (int i = 0; i < MAXDUNX; i++) { // NOLINT(modernize-loop-convert)
				for (int j = 0; j < MAXDUNY; j++) {
					dFlags[i][j] |= DungeonFlag::Lit;
				}
			}

			InitTowners();
			// Oracool: bug postmortem (2026-08-10) - user report: after warping to a dungeon level
			// and back via a waypoint, the Stash Chest and waypoint sigil were both gone. Root
			// cause: this used to be guarded the same way as the dungeon branch's own
			// fresh-vs-reload check just below, on the theory that a return-to-town reload via
			// LoadLevel() would otherwise duplicate these objects. That premise is false for town -
			// SaveLevel()/LoadLevel() (loadsave.cpp) explicitly skip saving/loading Objects[],
			// ActiveObjects, and dObject for DTYPE_TOWN (vanilla town has none to save), so
			// ActiveObjectCount gets restored from the save file but the object data behind it
			// never does. A return visit therefore always needs these two re-placed from scratch,
			// never just once on the town's first generation - unlike a dungeon level, whose own
			// LoadLevel() call genuinely does restore its objects correctly.
			// InitTownObjectPool() must run first - see its doc comment in oracool/oracool.h - town
			// never otherwise initializes the Objects[] pool the way every dungeon level's own
			// InitObjects() does, so without this every object placed here would silently collide
			// on the same internal slot.
			oracool::InitTownObjectPool();
			oracool::AddStashChestObject();
			oracool::AddLevskiRoarObject();
			oracool::AddWaypointSigilObject();
			InitStash();
			InitItems();
			InitMissiles();
			IncProgress();

			if (!firstflag && lvldir != ENTRY_LOAD && myPlayer._pLvlVisited[currlevel] && !gbIsMultiplayer)
				LoadLevel();
			if (gbIsMultiplayer)
				DeltaLoadLevel();

			IncProgress();
			for (int x = 0; x < DMAXX; x++)
				for (int y = 0; y < DMAXY; y++)
					UpdateAutomapExplorer({ x, y }, MAP_EXP_SELF);
		}
		if (UseMultiplayerQuests())
			ResyncMPQuests();
		else
			ResyncQuests();
	} else {
		LoadSetMap();
		IncProgress();
		GetLevelMTypes();
		IncProgress();
		InitGolems();
		InitMonsters();
		IncProgress();
		if (!HeadlessMode) {
#if !defined(USE_SDL1) && !defined(__vita__)
			InitVirtualGamepadGFX(renderer);
#endif
			InitMissileGFX(gbIsHellfire);
			IncProgress();
		}
		InitCorpses();
		IncProgress();
		LoadLevelSOLData();
		IncProgress();

		if (lvldir == ENTRY_WARPLVL)
			GetPortalLvlPos();
		IncProgress();

		for (Player &player : Players) {
			if (player.plractive && player.isOnActiveLevel()) {
				InitPlayerGFX(player);
				if (lvldir != ENTRY_LOAD)
					InitPlayer(player, firstflag);
			}
		}
		IncProgress();
		PlayDungMsgs();
		InitMultiView();
		IncProgress();

		if (firstflag || lvldir == ENTRY_LOAD || !myPlayer._pSLvlVisited[setlvlnum] || gbIsMultiplayer) {
			InitItems();
			SavePreLighting();
		} else {
			LoadLevel();
		}
		if (gbIsMultiplayer) {
			DeltaLoadLevel();
			if (!UseMultiplayerQuests())
				ResyncQuests();
		}

		InitMissiles();
		IncProgress();
	}

	SyncPortals();

	for (Player &player : Players) {
		if (player.plractive && player.isOnActiveLevel() && (!player._pLvlChanging || &player == MyPlayer)) {
			if (player._pHitPoints > 0) {
				if (lvldir != ENTRY_LOAD)
					SyncInitPlrPos(player);
			} else {
				dFlags[player.position.tile.x][player.position.tile.y] |= DungeonFlag::DeadPlayer;
			}
		}
	}

	IncProgress();
	IncProgress();

	if (firstflag) {
		InitControlPan();
	}
	IncProgress();
	UpdateMonsterLights();
	UnstuckChargers();
	if (leveltype != DTYPE_TOWN) {
		memcpy(dLight, dPreLight, sizeof(dLight));                                     // resets the light on entering a level to get rid of incorrect light
		ChangeLightXY(Players[MyPlayerId].lightId, Players[MyPlayerId].position.tile); // forces player light refresh
		ProcessLightList();
		ProcessVisionList();
	}

	if (leveltype == DTYPE_CRYPT) {
		if (CornerStone.isAvailable()) {
			CornerstoneLoad(CornerStone.position);
		}
		if (Quests[Q_NAKRUL]._qactive == QUEST_DONE && currlevel == 24) {
			SyncNakrulRoom();
		}
	}

#ifndef USE_SDL1
	ActivateVirtualGamepad();
#endif

	if (sgnMusicTrack != neededTrack)
		music_start(neededTrack);

	if (MinimizePaused) {
		music_mute();
	}

	CompleteProgress();

	// Recalculate mouse selection of entities after level change/load
	LastMouseButtonAction = MouseActionType::None;
	LastMouseButtonSpell = SpellID::Invalid;
	LastMouseButtonSpellType = SpellType::Invalid;
	sgbMouseDown = CLICK_NONE;
	ResetItemlabelHighlighted(); // level changed => item changed
	pcursmonst = -1;             // ensure pcurstemp is set to a valid value
	CheckCursMove();

	// Re-attach the aura loop released at the top of this function, now that the level is up and the
	// player's state is settled. Resume rather than Start: the aura never went out, so its start cue
	// would announce something that did not happen.
	if (MyPlayer != nullptr) {
		if (const oracool::ClassTreeSkill aura = oracool::GetActiveClassAura(*MyPlayer);
		    aura != oracool::ClassTreeSkill::None) {
			oracool::ResumeAuraLoopAfterTransition(aura);
		}
	}
	// And re-baseline once more: the equipment recalculations during the load are finished, so this
	// is the state the next real transaction will be compared against.
	if (MyPlayer != nullptr)
		oracool::ArmSetCompletionBaseline(*MyPlayer);
}

bool game_loop(bool bStartup)
{
	uint16_t wait = bStartup ? sgGameInitInfo.nTickRate * 3 : 3;

	for (unsigned i = 0; i < wait; i++) {
		if (!multi_handle_delta()) {
			TimeoutCursor(true);
			return false;
		}
		TimeoutCursor(false);
		GameLogic();
		ClearLastSentPlayerCmd();

		if (!gbRunGame || !gbIsMultiplayer || demo::IsRunning() || demo::IsRecording() || !nthread_has_500ms_passed())
			break;
	}
	return true;
}

void diablo_color_cyc_logic()
{
	if (!*sgOptions.Graphics.colorCycling)
		return;

	if (PauseMode != 0)
		return;

	if (leveltype == DTYPE_CAVES) {
		if (setlevel && setlvlnum == Quests[Q_PWATER]._qslvl) {
			UpdatePWaterPalette();
		} else {
			palette_update_caves();
		}
	} else if (leveltype == DTYPE_HELL) {
		lighting_color_cycling();
	} else if (leveltype == DTYPE_NEST) {
		palette_update_hive();
	} else if (leveltype == DTYPE_CRYPT) {
		palette_update_crypt();
	}
}

bool IsDiabloAlive(bool playSFX)
{
	if (Quests[Q_DIABLO]._qactive == QUEST_DONE && !gbIsMultiplayer) {
		if (playSFX)
			PlaySFX(USFX_DIABLOD);
		return false;
	}

	return true;
}

void PrintScreen(SDL_Keycode vkey)
{
	ReleaseKey(vkey);
}

} // namespace devilution
