/**
 * @file track.cpp
 *
 * Implementation of functionality tracking what the mouse cursor is pointing at.
 */
#include "track.h"

#include <SDL.h>

#include "controls/game_controls.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "engine/point.hpp"
#include "objects.h"
#include "oracool/levski_roar.h"
#include "oracool/stonegate.h" // IsStonegateObject - the Monument opens a menu, not a one-shot
#include "player.h"
#include "control.h" // IsOverLeftPanel, IsOverRightPanel
#include "oracool/hud_layout.h" // IsPointOverFloatingWindow
#include "help.h" // HelpFlag
#include "qol/chatlog.h" // ChatLogFlag
#include "stores.h"

namespace devilution {

namespace {

void RepeatWalk(Player &player)
{
	if (!InDungeonBounds(cursPosition))
		return;

	if (player._pmode != PM_STAND && !(player.isWalking() && player.AnimInfo.getFrameToUseForRendering() > 6))
		return;

	const Point target = player.GetTargetPosition();
	if (cursPosition == target)
		return;

	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
}

} // namespace

void InvalidateTargets()
{
	if (pcursmonst != -1) {
		const Monster &monster = Monsters[pcursmonst];
		if (monster.isInvalid || monster.hitPoints >> 6 <= 0
		    || (monster.flags & MFLAG_HIDDEN) != 0
		    || !IsTileLit(monster.position.tile)) {
			pcursmonst = -1;
		}
	}

	if (ObjectUnderCursor != nullptr && ObjectUnderCursor->_oSelFlag < 1)
		ObjectUnderCursor = nullptr;

	if (pcursplr != -1) {
		Player &targetPlayer = Players[pcursplr];
		if (targetPlayer._pmode == PM_DEATH || targetPlayer._pmode == PM_QUIT || !targetPlayer.plractive
		    || !targetPlayer.isOnActiveLevel() || targetPlayer._pHitPoints >> 6 <= 0
		    || !IsTileLit(targetPlayer.position.tile))
			pcursplr = -1;
	}
}

void RepeatMouseAction()
{
	if (pcurs != CURSOR_HAND)
		return;

	if (sgbMouseDown == CLICK_NONE && ControllerActionHeld == GameActionType_NONE)
		return;

	if (stextflag != TalkID::None)
		return;

	// Nor under a window (round 36 audit: a held walk or cast went on behind the inventory, the sheet or a book a hotkey
	// opened while the button was down). The windows only, and only with the mouse (round 37 audit: the HUD's chrome
	// stopped a held swing or walk the moment the cursor touched the plate, and a pad's cursor sits wherever it was left).
	if (ControlMode == ControlTypes::KeyboardAndMouse
	    && (IsOverLeftPanel(MousePosition) || IsOverRightPanel(MousePosition) || oracool::IsPointOverFloatingWindow(MousePosition)))
		return;
	if (ChatLogFlag || HelpFlag)
		return; // the modal screens, wherever the cursor is (round 38 audit)

	if (LastMouseButtonAction == MouseActionType::None)
		return;

	Player &myPlayer = *MyPlayer;
	if (myPlayer.destAction != ACTION_NONE)
		return;
	if (myPlayer._pInvincible)
		return;
	if (!myPlayer.CanChangeAction())
		return;

	// Nothing is attacked in town, and holding the button must not keep asking (user, 2026-09-02).
	// StartAttack refuses the swing itself - see the town guard there for why the hero blinks
	// without it - but a repeat that keeps sending the command would keep reaching that refusal,
	// once per frame, for as long as the button is down. Refused before the send, so the answer is
	// given once.
	const bool inTown = leveltype == DTYPE_TOWN;

	bool rangedAttack = myPlayer.UsesRangedWeapon();
	switch (LastMouseButtonAction) {
	case MouseActionType::Attack:
		if (!inTown && InDungeonBounds(cursPosition))
			NetSendCmdLoc(MyPlayerId, true, rangedAttack ? CMD_RATTACKXY : CMD_SATTACKXY, cursPosition);
		break;
	case MouseActionType::AttackMonsterTarget:
		if (!inTown && pcursmonst != -1)
			NetSendCmdParam1(true, rangedAttack ? CMD_RATTACKID : CMD_ATTACKID, pcursmonst);
		break;
	case MouseActionType::AttackPlayerTarget:
		if (!inTown && pcursplr != -1 && !myPlayer.friendlyMode)
			NetSendCmdParam1(true, rangedAttack ? CMD_RATTACKPID : CMD_ATTACKPID, pcursplr);
		break;
	// All three repeat the spell the last click ACTUALLY cast, not CheckPlrSpell's default
	// arguments. Those defaults are MyPlayer->_pRSpell / _pRSplType - right for a game where only
	// the right button holds a spell, wrong the moment this fork let the left button hold one too.
	// Holding the left button after casting a left-button skill repeated the RIGHT button's spell:
	// "when i cast fist of the heavens game also casts teleport" (user, 2026-08-20).
	//
	// The guard is new as well as the argument. LastMouseButtonSpell is Invalid until something
	// casts, so a repeat can no longer conjure a spell out of the readied slot when the last action
	// was not a cast at all.
	case MouseActionType::Spell:
		if (!IsValidSpell(LastMouseButtonSpell))
			break;
		if (ControlMode != ControlTypes::KeyboardAndMouse) {
			UpdateSpellTarget(LastMouseButtonSpell);
		}
		CheckPlrSpell(ControlMode == ControlTypes::KeyboardAndMouse, LastMouseButtonSpell, LastMouseButtonSpellType);
		break;
	case MouseActionType::SpellMonsterTarget:
		if (pcursmonst != -1 && IsValidSpell(LastMouseButtonSpell))
			CheckPlrSpell(false, LastMouseButtonSpell, LastMouseButtonSpellType);
		break;
	case MouseActionType::SpellPlayerTarget:
		if (pcursplr != -1 && !myPlayer.friendlyMode && IsValidSpell(LastMouseButtonSpell))
			CheckPlrSpell(false, LastMouseButtonSpell, LastMouseButtonSpellType);
		break;
	case MouseActionType::OperateObject:
		// Repeats the operate every frame the button is held. That is safe for the objects vanilla
		// has here, because operating one CONSUMES it - a chest opens, a shrine spends itself, and
		// the repeats after the first are no-ops. It is not safe for anything that TOGGLES, which is
		// why doors were already excluded.
		//
		// Levski's Roar is a toggle too, and it was not excluded (user, 2026-08-27: "fix some
		// flashing which occurs when i click on levski. if i am next to him, i need to click a few
		// times until the window remains open, instead of blinking and closing"). Holding the button
		// for a fifth of a second flipped the window open and shut several times over, so whether it
		// ended up open came down to the parity of how long the click lasted. It only bit when the
		// player was already ADJACENT: from further away the walk eats the hold, and the button is
		// released before the operate ever fires.
		// Nor the Rift Monument or a waypoint sigil, which OPEN a menu: held, each frame re-opened it, replaying its select
		// click and, for a sigil, scheduling a save (round 23 audit, v1.12.248).
		if (ObjectUnderCursor != nullptr && !ObjectUnderCursor->isDoor()
		    && !oracool::IsLevskiRoarObject(*ObjectUnderCursor) && !oracool::IsStonegateObject(*ObjectUnderCursor)
		    && ObjectUnderCursor->_otype != OBJ_WAYPOINT
		    && !(leveltype == DTYPE_TOWN && IsStashChestObject(*ObjectUnderCursor))) { // it opens the stash (round 36 audit)
			NetSendCmdLoc(MyPlayerId, true, CMD_OPOBJXY, cursPosition);
		}
		break;
	case MouseActionType::Walk:
		RepeatWalk(myPlayer);
		break;
	case MouseActionType::None:
		break;
	}
}

bool track_isscrolling()
{
	return LastMouseButtonAction == MouseActionType::Walk;
}

} // namespace devilution
