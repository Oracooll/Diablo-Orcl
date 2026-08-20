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
#include "player.h"
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

	if (LastMouseButtonAction == MouseActionType::None)
		return;

	Player &myPlayer = *MyPlayer;
	if (myPlayer.destAction != ACTION_NONE)
		return;
	if (myPlayer._pInvincible)
		return;
	if (!myPlayer.CanChangeAction())
		return;

	bool rangedAttack = myPlayer.UsesRangedWeapon();
	switch (LastMouseButtonAction) {
	case MouseActionType::Attack:
		if (InDungeonBounds(cursPosition))
			NetSendCmdLoc(MyPlayerId, true, rangedAttack ? CMD_RATTACKXY : CMD_SATTACKXY, cursPosition);
		break;
	case MouseActionType::AttackMonsterTarget:
		if (pcursmonst != -1)
			NetSendCmdParam1(true, rangedAttack ? CMD_RATTACKID : CMD_ATTACKID, pcursmonst);
		break;
	case MouseActionType::AttackPlayerTarget:
		if (pcursplr != -1 && !myPlayer.friendlyMode)
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
		if (ObjectUnderCursor != nullptr && !ObjectUnderCursor->isDoor()) {
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
