/**
 * @file player.cpp
 *
 * Implementation of player functionality, leveling, actions, creation, loading, etc.
 */
#include <algorithm>
#include <cstdint>
#include <limits>

#include <fmt/core.h>

#include "control.h"
#include "oracool/weapon_throw.h"
#include "oracool/combat_odds.h"
#include "oracool/gems.h"
#include "oracool/level_requirement.h"
#include "oracool/hero_look.h"
#include "oracool/sprite_colours.h"
#include "oracool/sprite_mix.h"
#include "oracool/inventory_layout.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "dead.h"
#ifdef _DEBUG
#include "debug.h"
#include "effects.h"
#endif
#include "engine/backbuffer_state.hpp"
#include "engine/assets.hpp" // FindAsset - does the archive carry this block sheet at all
#include "engine/load_cl2.hpp"
#include "engine/load_file.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/random.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/trn.hpp"
#include "engine/world_tile.hpp"
#include "gamemenu.h"
#include "help.h"
#include "init.h"
#include "inv.h" // CalculateGold
#include "inv_iterators.hpp"
#include "levels/trigs.h"
#include "lighting.h"
#include "loadsave.h"
#include "minitext.h"
#include "missiles.h"
#include "nthread.h"
#include "objects.h"
#include "options.h"
#include "oracool/aura_field.h"
#include "oracool/signets.h"
#include "oracool/auto_save.h"
#include "oracool/event_log.h"
#include "oracool/class_skills.h"
#include "oracool/cold.h"
#include "oracool/venom.h"
#include "oracool/minions.h" // DismissMinions - the army dies with its master
#include "oracool/melee_skills.h"
#include "oracool/passives.h"
#include "oracool/rfa12_effects.h"
#include "oracool/curses.h"
#include "oracool/essence.h"
#include "oracool/necro_items.h"
#include "oracool/rage.h"
#include "oracool/rfa12_actives.h"
#include "oracool/warcries.h"
#include "oracool/rogue_arrows.h"
#include "oracool/furious_charge.h"
#include "oracool/hud_layout.h"
#include "oracool/paladin_skills.h"
#include "oracool/class_tree.h"
#include "oracool/companion.h"
#include "oracool/readied_spells.h"
#include "oracool/run_toggle.h"
#include "oracool/skill_points.h"
#include "oracool/skill_sounds.h"
#include "oracool/gradual_healing.h"
#include "oracool/oracool.h"
#include "oracool/sprite_import.h"
#include "oracool/paladin_melee.h"
#include "oracool/paladin_ranged.h"
#include "oracool/xp_gain_indicator.h"
#include "oracool/whirlwind.h"
#include "player.h"
#include "oracool/sat_math.h"
#include "playerdat.hpp"
#include "qol/autopickup.h"
#include "qol/floatingnumbers.h"
#include "qol/stash.h"
#include "spells.h"
#include "stores.h"
#include "towners.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

size_t MyPlayerId;
Player *MyPlayer;
std::vector<Player> Players;
Player *InspectPlayer;
bool MyPlayerIsDead;

namespace {

struct DirectionSettings {
	Direction dir;
	DisplacementOf<int8_t> tileAdd;
	DisplacementOf<int8_t> map;
	PLR_MODE walkMode;
	void (*walkModeHandler)(Player &, const DirectionSettings &);
};

void UpdatePlayerLightOffset(Player &player)
{
	if (player.lightId == NO_LIGHT)
		return;

	const WorldTileDisplacement offset = player.position.CalculateWalkingOffset(player._pdir, player.AnimInfo);
	ChangeLightOffset(player.lightId, offset.screenToLight());
}

void WalkNorthwards(Player &player, const DirectionSettings &walkParams)
{
	dPlayer[player.position.future.x][player.position.future.y] = -(player.getId() + 1);
	player.position.temp = player.position.tile + walkParams.tileAdd;
}

void WalkSouthwards(Player &player, const DirectionSettings & /*walkParams*/)
{
	const size_t playerId = player.getId();
	dPlayer[player.position.tile.x][player.position.tile.y] = -(playerId + 1);
	player.position.temp = player.position.tile;
	player.position.tile = player.position.future; // Move player to the next tile to maintain correct render order
	dPlayer[player.position.tile.x][player.position.tile.y] = playerId + 1;
	// BUGFIX: missing `if (leveltype != DTYPE_TOWN) {` for call to ChangeLightXY and PM_ChangeLightOff.
	ChangeLightXY(player.lightId, player.position.tile);
	UpdatePlayerLightOffset(player);
}

void WalkSideways(Player &player, const DirectionSettings &walkParams)
{
	Point const nextPosition = player.position.tile + walkParams.map;

	const size_t playerId = player.getId();
	dPlayer[player.position.tile.x][player.position.tile.y] = -(playerId + 1);
	dPlayer[player.position.future.x][player.position.future.y] = playerId + 1;

	if (leveltype != DTYPE_TOWN) {
		ChangeLightXY(player.lightId, nextPosition);
		UpdatePlayerLightOffset(player);
	}

	player.position.temp = player.position.future;
}

constexpr std::array<const DirectionSettings, 8> WalkSettings { {
	// clang-format off
	{ Direction::South,     {  1,  1 }, { 0, 0 }, PM_WALK_SOUTHWARDS, WalkSouthwards },
	{ Direction::SouthWest, {  0,  1 }, { 0, 0 }, PM_WALK_SOUTHWARDS, WalkSouthwards },
	{ Direction::West,      { -1,  1 }, { 0, 1 }, PM_WALK_SIDEWAYS,   WalkSideways   },
	{ Direction::NorthWest, { -1,  0 }, { 0, 0 }, PM_WALK_NORTHWARDS, WalkNorthwards },
	{ Direction::North,     { -1, -1 }, { 0, 0 }, PM_WALK_NORTHWARDS, WalkNorthwards },
	{ Direction::NorthEast, {  0, -1 }, { 0, 0 }, PM_WALK_NORTHWARDS, WalkNorthwards },
	{ Direction::East,      {  1, -1 }, { 1, 0 }, PM_WALK_SIDEWAYS,   WalkSideways   },
	{ Direction::SouthEast, {  1,  0 }, { 0, 0 }, PM_WALK_SOUTHWARDS, WalkSouthwards }
	// clang-format on
} };

bool PlrDirOK(const Player &player, Direction dir)
{
	Point position = player.position.tile;
	Point futurePosition = position + dir;
	if (futurePosition.x < 0 || !PosOkPlayer(player, futurePosition)) {
		return false;
	}

	if (dir == Direction::East) {
		return !IsTileSolid(position + Direction::SouthEast);
	}

	if (dir == Direction::West) {
		return !IsTileSolid(position + Direction::SouthWest);
	}

	return true;
}

void HandleWalkMode(Player &player, Direction dir)
{
	const auto &dirModeParams = WalkSettings[static_cast<size_t>(dir)];
	SetPlayerOld(player);
	if (!PlrDirOK(player, dir)) {
		return;
	}
	oracool::CompanionsMakeWay(player, player.position.tile + dir);

	player._pdir = dir;

	// The player's tile position after finishing this movement action
	player.position.future = player.position.tile + dirModeParams.tileAdd;

	dirModeParams.walkModeHandler(player, dirModeParams);

	player.tempDirection = dirModeParams.dir;
	player._pmode = dirModeParams.walkMode;
}

void StartWalkAnimation(Player &player, Direction dir, bool pmWillBeCalled)
{
	int8_t skippedFrames = -2;
	// Oracool: Furious Charge reuses the same double-speed frame-skip Run In Town already uses,
	// rather than inventing a separate speed mechanic - see oracool/furious_charge.h. The Phase 2.5
	// run toggle (R) is the third consumer, extending that skip to every level type.
	// Vigor is the fourth consumer: Diablo II's movement-speed aura, in an engine with no
	// walk-speed modifier, is exactly this frame skip held on for as long as the aura burns.
	// A slow reaches the feet (audit, 2026-09-27): the walk took max(-2, skip), so it could never be slower than a plain
	// walk, and the run ignored slows outright - a chill or a lead affix showed "Move speed 50%" on the sheet and changed
	// nothing. The walk takes the skip as it is now; the run keeps its +4 frames over whatever the slowed walk is.
	const int8_t walkSkip = oracool::WalkFrameSkipFor(player);
	// The dash belongs to the charge's approach: a walk order that replaced it (a click on the floor) kept the sprint for
	// up to 2 s, and with no swing no cooldown started (round 11 audit, v1.12.236).
	if (&player == MyPlayer && player.destAction != ACTION_ATTACKMON)
		oracool::StopFuriousChargeDash();
	if (oracool::IsFuriousChargeDashing() && &player == MyPlayer) {
		// Charge is a sprint at the monster (dev note, 2026-09-27: "time per tile 0,1s"): 2 ticks a tile (see
		// ChargeDashSkipFrames for the arithmetic). Slows do not reach it - the dash is the skill, and it lasts one approach.
		skippedFrames = static_cast<int8_t>(std::max(0, std::min(oracool::ChargeDashSkipFrames, player._pWFrames - 2)));
	} else if ((leveltype == DTYPE_TOWN && sgGameInitInfo.bRunInTown != 0)
	    || oracool::IsRunEnabled() || oracool::IsClassTreeRunActive(player)
	    || oracool::IsWhirlwinding(player)) // the spin glides at run speed (2026-09-29)
		// The run, slowed: four frames over the slowed walk, but never past the run the same walk gives unslowed - a chilled
		// hero at +60% ran faster than an unchilled one, and past +90% crossed the whole path in one tick (round 20 audit).
		skippedFrames = oracool::PlayerSlowPercent(player) > 0 ? std::min<int8_t>(static_cast<int8_t>(walkSkip + 4), std::max<int8_t>(2, walkSkip))
		                                                     : std::max<int8_t>(2, walkSkip); // the run, or Movement Speed past it (2026-09-12)
	else
		skippedFrames = walkSkip; // Movement Speed %: items and Vigor, every percent - and slows
	if (pmWillBeCalled)
		skippedFrames += 1;
	NewPlrAnim(player, player_graphic::Walk, dir, AnimationDistributionFlags::ProcessAnimationPending, skippedFrames);
}

/**
 * @brief Start moving a player to a new tile
 */
void StartWalk(Player &player, Direction dir, bool pmWillBeCalled)
{
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	StartWalkAnimation(player, dir, pmWillBeCalled);
	HandleWalkMode(player, dir);
}

void ClearStateVariables(Player &player)
{
	player.position.temp = { 0, 0 };
	player.tempDirection = Direction::South;
	player.queuedSpell.spellLevel = 0;
}

void StartAttack(Player &player, Direction d, bool includesFirstFrame)
{
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	// TOWN HAS NO SWING, and says so out loud (user, 2026-09-02: "either allow shift+click attacks
	// in town or when i try it play the hero sound saying he cant do it and dont attempt any hero
	// animations. right now when i shiftclick hero blinks").
	//
	// The blink is not a graphical glitch, it is this function running where its animation does not
	// exist. LoadPlrGFX RETURNS for player_graphic::Attack in town - the town sheets are idle and
	// walk only, there is no "at" file to load - so NewPlrAnim binds AnimInfo to an EMPTY sprite
	// list, and for as long as PM_ATTACK lasts the hero has nothing to draw. He vanishes and comes
	// back. Allowing the attack instead would mean shipping town attack sheets for every class,
	// armour and weapon combination, which is why Blizzard did not allow it either.
	//
	// Refused HERE rather than at each click, because there are five ways to reach this function -
	// two shift branches, the monster-target path, the barrel-break path, and the hold-to-repeat -
	// and a rule enforced at four of them is a rule that a sixth caller will break. Same argument as
	// the LoadPlrGFX death-sheet fix above: ask the single authority, do not make every caller
	// remember.
	//
	// SaySpecific, not Say: it declines while that line is already playing, so holding the button
	// down refuses once rather than sixty times a second.
	if (leveltype == DTYPE_TOWN) {
		player.destAction = ACTION_NONE;
		if (&player == MyPlayer) {
			LastMouseButtonAction = MouseActionType::None;
			player.SaySpecific(HeroSpeech::ICantDoThat);
		}
		return;
	}

	// Oracool: a furious-charge dash resolves into this swing - end the dash and start the
	// cooldown here so it fires regardless of whether the target was already adjacent (no walk
	// needed) or reached after several walk steps.
	// ...and whether this swing is the dash's arriving blow, which alone carries Charge's +20% a level
	// (2026-09-12) - a Charge clicked on cooldown walks up and swings plainly.
	oracool::SetChargeBlowArmed(oracool::IsFuriousChargeDashing());
	if (oracool::IsFuriousChargeDashing()) {
		oracool::StopFuriousChargeDash();
		oracool::StartFuriousChargeCooldown();
	}

	int8_t skippedAnimationFrames = 0;
	if (includesFirstFrame) {
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastestAttack) && HasAnyOf(player._pIFlags, ItemSpecialEffect::QuickAttack | ItemSpecialEffect::FastAttack)) {
			// Combining Fastest Attack with any other attack speed modifier skips over the fourth frame, reducing the effectiveness of Fastest Attack.
			// Faster Attack makes up for this by also skipping the sixth frame so this case only applies when using Quick or Fast Attack modifiers.
			skippedAnimationFrames = 3;
		} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastestAttack)) {
			skippedAnimationFrames = 4;
		} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FasterAttack)) {
			skippedAnimationFrames = 3;
		} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastAttack)) {
			skippedAnimationFrames = 2;
		} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::QuickAttack)) {
			skippedAnimationFrames = 1;
		}
	} else {
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FasterAttack)) {
			// The combination of Faster and Fast Attack doesn't result in more skipped frames, because the second frame skip of Faster Attack is not triggered.
			skippedAnimationFrames = 2;
		} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastAttack)) {
			skippedAnimationFrames = 1;
		} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastestAttack)) {
			// Fastest Attack is skipped if Fast or Faster Attack is also specified, because both skip the frame that triggers Fastest Attack skipping.
			skippedAnimationFrames = 2;
		}
	}

	auto animationFlags = AnimationDistributionFlags::ProcessAnimationPending;
	if (player._pmode == PM_ATTACK)
		animationFlags = static_cast<AnimationDistributionFlags>(animationFlags | AnimationDistributionFlags::RepeatedAction);
	// Oracool: user request (2026-08-15) - Shield Bash "always play Shield Hit animation, regardless
	// of equipped weapon". The character shoves with the shield, so it borrows the BLOCK graphic
	// rather than the weapon's swing; every other skill and a plain attack are unaffected.
	//
	// The hit frame has to come with it. player_graphic::Block runs _pBFrames frames where the swing
	// runs to _pAFNum, and DoAttack lands the blow on a specific frame - so on a block animation
	// shorter than _pAFNum the blow would simply never arrive. oracool::MeleeHitFrame clamps it, and
	// DoAttack asks the same function, so the two cannot disagree about when the hit is.
	const bool bashesWithShield = oracool::IsShieldBashSwing(player);
	// Oracool: a Zeal-armed swing is compressed from its FIRST swing (user spec, 2026-08-15) - the
	// 150% budget divides across every strike of the burst, so a 2-strike Zeal shows two ~15-frame
	// swings, not one full swing and one stub. Combined with the item speed skips above and clamped
	// so the hit frame always survives.
	// Heavenly Strength's -20% attack speed (2026-09-27) is a negative skip: extra ticks before the first frame.
	// Double Swing and Frenzy compress the same way, two swings to one attack's time (2026-09-29).
	if (&player == MyPlayer)
		oracool::BeginClassMeleeSwing();
	skippedAnimationFrames = static_cast<int8_t>(std::min<int>(
	    skippedAnimationFrames + oracool::ZealSwingSkipFrames(player) + oracool::ClassMeleeSwingSkipFrames(player)
	        - oracool::HeavenlyStrengthSwingDelayFrames(player),
	    std::max(0, oracool::MeleeHitFrame(player) - 2)));
	const player_graphic swing = oracool::SwingsShieldAttackSheet(player) ? player_graphic::ShieldAttack
	    : bashesWithShield                                             ? player_graphic::Block
	                                                                   : player_graphic::Attack;
	NewPlrAnim(player, swing, d,
	    animationFlags, skippedAnimationFrames, oracool::MeleeHitFrame(player));
	player._pmode = PM_ATTACK;
	FixPlayerLocation(player, d);
	SetPlayerOld(player);
}

void StartRangeAttack(Player &player, Direction d, WorldTileCoord cx, WorldTileCoord cy, bool includesFirstFrame)
{
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}
	// As StartAttack's town guard: the attack sheet is not loaded in town, so a bow skill made the hero blink - and the
	// skill's mana went for an arrow that could hit nothing (round 6 audit, v1.12.231). The bow latch goes with it.
	if (leveltype == DTYPE_TOWN) {
		player.destAction = ACTION_NONE;
		if (&player == MyPlayer) {
			oracool::ArmArrowSkill(std::nullopt);
			LastMouseButtonAction = MouseActionType::None;
			player.SaySpecific(HeroSpeech::ICantDoThat);
		}
		return;
	}

	int8_t skippedAnimationFrames = 0;
	if (!gbIsHellfire) {
		if (includesFirstFrame && HasAnyOf(player._pIFlags, ItemSpecialEffect::QuickAttack | ItemSpecialEffect::FastAttack)) {
			skippedAnimationFrames += 1;
		}
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastAttack)) {
			skippedAnimationFrames += 1;
		}
	}

	auto animationFlags = AnimationDistributionFlags::ProcessAnimationPending;
	if (player._pmode == PM_RATTACK)
		animationFlags = static_cast<AnimationDistributionFlags>(animationFlags | AnimationDistributionFlags::RepeatedAction);
	NewPlrAnim(player, player_graphic::Attack, d, animationFlags, skippedAnimationFrames, player._pAFNum);

	player._pmode = PM_RATTACK;
	FixPlayerLocation(player, d);
	SetPlayerOld(player);
	player.position.temp = WorldTilePosition { cx, cy };
}

player_graphic GetPlayerGraphicForSpell(SpellID spellId)
{
	// Oracool: the Paladin's three cast skills take the animation the user named for each (2026-09-11:
	// Blessed Shield "magic", Fist of the Heavens "lightning", and Blessed Hammer magic once its damage
	// became magic the same day) rather than their element's - the element stays what the rows say.
	switch (oracool::PaladinCastAnimation(spellId).value_or(GetSpellData(spellId).type())) {
	case MagicType::Fire:
		return player_graphic::Fire;
	case MagicType::Lightning:
		return player_graphic::Lightning;
	default:
		return player_graphic::Magic;
	}
}

void StartSpell(Player &player, Direction d, WorldTileCoord cx, WorldTileCoord cy)
{
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	// Checks conditions for spell again, cause initial check was done when spell was queued and the parameters could be changed meanwhile
	bool isValid = true;
	switch (player.queuedSpell.spellType) {
	case SpellType::Skill:
	case SpellType::Spell:
		isValid = CheckSpell(player, player.queuedSpell.spellId, player.queuedSpell.spellType, true) == SpellCheckResult::Success;
		// The Paladin's three cast skills pay outside CheckSpell, which therefore always passed them: a second click on
		// the last mana played a whole cast that ended in nothing (round 6 audit, v1.12.231). Their own check, again.
		if (isValid && player.queuedSpell.spellType == SpellType::Skill) {
			if (const auto skill = oracool::PaladinSkillForSpell(player.queuedSpell.spellId); skill && oracool::IsCastPaladinSkill(*skill))
				isValid = oracool::CanStartRangedPaladinSkill(player, *skill);
		}
		break;
	case SpellType::Scroll:
		isValid = CanUseScroll(player, player.queuedSpell.spellId);
		break;
	case SpellType::Charges:
		isValid = CanUseStaff(player, player.queuedSpell.spellId);
		break;
	case SpellType::Invalid:
		isValid = false;
		break;
	}
	if (!isValid)
		return;

	auto animationFlags = AnimationDistributionFlags::ProcessAnimationPending;
	if (player._pmode == PM_SPELL)
		animationFlags = static_cast<AnimationDistributionFlags>(animationFlags | AnimationDistributionFlags::RepeatedAction);
	// Faster Cast Rate (2026-09-11): the frames it earns come off the start of the cast, spread over the
	// frames before the cast frame so the animation still reads whole. Never the cast frame itself -
	// see oracool::CastFrameSkip.
	NewPlrAnim(player, GetPlayerGraphicForSpell(player.queuedSpell.spellId), d, animationFlags,
	    static_cast<int8_t>(oracool::CastFrameSkip(player._pSFNum, player._pIFastCast)), player._pSFNum);

	// The spell's OWN sound, and only one (user, 2026-09-03: "there is some unnecessary chatgpt
	// sound played every time i cast same spells. remove it. spells have their own sounds").
	//
	// The class-tree sound package used to layer a cast cue on top of this for 92 of the 163 rows,
	// so a spell with a perfectly good vanilla noise made two. The package's aura loops stay - an
	// aura has no engine sound at all, so those are its only voice, not a second one - and so does
	// the Learn click in the Abilities window, which is UI feedback rather than a spell.
	//
	// One exception, REPLACING rather than stacking (user, 2026-09-26: the war cries' delivered shouts and
	// the other delivered cast cues): a spell whose own sound is only the generic IS_CAST2 plays its tree
	// row's Cast cue instead, when the row has one. The cold spells are left out - their cue already
	// replaces the missile's launch sound (oracool/cold.h), and here it would ring twice.
	{
		const SpellID castSpell = player.queuedSpell.spellId;
		const _sfx_id sound = GetSpellData(castSpell).sSFX;
		bool played = false;
		if (sound == IS_CAST2 && &player == MyPlayer && !oracool::IsColdSpell(castSpell)) {
			const oracool::ClassTreeSkill row = oracool::ClassTreeSkillForSpell(player._pClass, castSpell);
			played = row != oracool::ClassTreeSkill::None && oracool::PlaySkillSound(row, oracool::SkillSoundEvent::Cast);
		}
		if (!played)
			PlaySfxLoc(sound, player.position.tile);
	}

	player._pmode = PM_SPELL;

	FixPlayerLocation(player, d);
	SetPlayerOld(player);

	player.position.temp = WorldTilePosition { cx, cy };
	player.queuedSpell.spellLevel = player.GetSpellLevel(player.queuedSpell.spellId);
	player.executedSpell = player.queuedSpell;
}

void RespawnDeadItem(Item &&itm, Point target)
{
	if (ActiveItemCount >= MAXITEMS)
		return;

	int ii = AllocateItem();

	dItem[target.x][target.y] = ii + 1;

	Items[ii] = itm;
	Items[ii].position = target;
	RespawnItem(Items[ii], true);
	NetSendCmdPItem(false, CMD_SPAWNITEM, target, Items[ii]);
}

void DeadItem(Player &player, Item &&itm, Displacement direction)
{
	if (itm.isEmpty())
		return;

	Point target = player.position.tile + direction;
	if (direction != Displacement { 0, 0 } && ItemSpaceOk(target)) {
		RespawnDeadItem(std::move(itm), target);
		return;
	}

	for (int k = 1; k < 50; k++) {
		for (int j = -k; j <= k; j++) {
			for (int i = -k; i <= k; i++) {
				Point next = player.position.tile + Displacement { i, j };
				if (ItemSpaceOk(next)) {
					RespawnDeadItem(std::move(itm), next);
					return;
				}
			}
		}
	}
}

int DropGold(Player &player, int amount, bool skipFullStacks)
{
	for (int i = 0; i < player._pNumInv && amount > 0; i++) {
		auto &item = player.InvList[i];

		if (item._itype != ItemType::Gold || (skipFullStacks && item._ivalue == MaxGold))
			continue;

		if (amount < item._ivalue) {
			Item goldItem;
			MakeGoldStack(goldItem, amount);
			DeadItem(player, std::move(goldItem), { 0, 0 });

			item._ivalue -= amount;

			return 0;
		}

		amount -= item._ivalue;
		DeadItem(player, std::move(item), { 0, 0 });
		player.RemoveInvItem(i);
		i = -1;
	}

	return amount;
}

void DropHalfPlayersGold(Player &player)
{
	int remainingGold = DropGold(player, player._pGold / 2, true);
	if (remainingGold > 0) {
		DropGold(player, remainingGold, false);
	}

	player._pGold /= 2;
}

void InitLevelChange(Player &player)
{
	Player &myPlayer = *MyPlayer;

	RemovePlrMissiles(player);
	player.pManaShield = false;
	player.wReflections = 0;
	if (&player != MyPlayer) {
		// share info about your manashield when another player joins the level
		if (myPlayer.pManaShield)
			NetSendCmd(true, CMD_SETSHIELD);
		// share info about your reflect charges when another player joins the level
		NetSendCmdParam1(true, CMD_SETREFLECT, myPlayer.wReflections);
	} else if (qtextflag) {
		qtextflag = false;
		stream_stop();
	}

	FixPlrWalkTags(player);
	SetPlayerOld(player);
	if (&player == MyPlayer) {
		dPlayer[player.position.tile.x][player.position.tile.y] = player.getId() + 1;
	} else {
		player._pLvlVisited[player.plrlevel] = true;
	}

	ClrPlrPath(player);
	player.destAction = ACTION_NONE;
	player._pLvlChanging = true;

	if (&player == MyPlayer) {
		player.pLvlLoad = 10;
	}
}

/**
 * @brief Continue movement towards new tile
 */
bool DoWalk(Player &player, int variant)
{
	// Play walking sound effect on certain animation frames
	if (*sgOptions.Audio.walkingSound && (leveltype != DTYPE_TOWN || sgGameInitInfo.bRunInTown == 0)) {
		if (player.AnimInfo.currentFrame == 0
		    || player.AnimInfo.currentFrame == 4) {
			PlaySfxLoc(PS_WALK1, player.position.tile);
		}
	}

	if (!player.AnimInfo.isLastFrame()) {
		// We didn't reach new tile so update player's "sub-tile" position
		UpdatePlayerLightOffset(player);
		return false;
	}

	// We reached the new tile -> update the player's tile position
	switch (variant) {
	case PM_WALK_NORTHWARDS:
		dPlayer[player.position.tile.x][player.position.tile.y] = 0;
		player.position.tile = player.position.temp;
		dPlayer[player.position.tile.x][player.position.tile.y] = player.getId() + 1;
		break;
	case PM_WALK_SOUTHWARDS:
		dPlayer[player.position.temp.x][player.position.temp.y] = 0;
		break;
	case PM_WALK_SIDEWAYS:
		dPlayer[player.position.tile.x][player.position.tile.y] = 0;
		player.position.tile = player.position.temp;
		// dPlayer is set here for backwards comparability, without it the player would be invisible if loaded from a vanilla save.
		dPlayer[player.position.tile.x][player.position.tile.y] = player.getId() + 1;
		break;
	}

	// Update the coordinates for lighting and vision entries for the player
	if (leveltype != DTYPE_TOWN) {
		ChangeLightXY(player.lightId, player.position.tile);
		ChangeVisionXY(player.getId(), player.position.tile);
	}

	StartStand(player, player.tempDirection);

	ClearStateVariables(player);

	// Reset the "sub-tile" position of the player's light entry to 0
	if (leveltype != DTYPE_TOWN) {
		ChangeLightOffset(player.lightId, { 0, 0 });
	}

	AutoPickup(player);
	return true;
}

bool WeaponDecay(Player &player, int ii)
{
	if (!player.InvBody[ii].isEmpty() && player.InvBody[ii]._iClass == ICLASS_WEAPON && HasAnyOf(player.InvBody[ii]._iDamAcFlags, ItemSpecialEffectHf::Decay)) {
		player.InvBody[ii]._iPLDam -= 5;
		// The record decays with it, so a rework of another row replays the decayed value, not the first roll (round 14
		// audit, v1.12.239).
		Item &weapon = player.InvBody[ii];
		for (uint8_t a = 0; a < weapon._iOracoolAffixCount && a < weapon._iOracoolAffixes.size(); a++) {
			if (weapon._iOracoolAffixes[a].type == IPL_DECAY)
				weapon._iOracoolAffixes[a].param1 -= 5;
		}
		if (player.InvBody[ii]._iPLDam <= -100) {
			RemoveEquipment(player, static_cast<inv_body_loc>(ii), true);
			CalcPlrInv(player, true);
			return true;
		}
		CalcPlrInv(player, true);
	}
	return false;
}

bool DamageWeapon(Player &player, unsigned damageFrequency)
{
	if (&player != MyPlayer) {
		return false;
	}

	if (WeaponDecay(player, INVLOC_HAND_LEFT))
		return true;
	if (WeaponDecay(player, INVLOC_HAND_RIGHT))
		return true;

	if (!FlipCoin(damageFrequency)) {
		return false;
	}

	if (!player.InvBody[INVLOC_HAND_LEFT].isEmpty() && player.InvBody[INVLOC_HAND_LEFT]._iClass == ICLASS_WEAPON) {
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		if (WearDurabilityPoint(player, INVLOC_HAND_LEFT))
			return true;
	}

	if (!player.InvBody[INVLOC_HAND_RIGHT].isEmpty() && player.InvBody[INVLOC_HAND_RIGHT]._iClass == ICLASS_WEAPON) {
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		if (WearDurabilityPoint(player, INVLOC_HAND_RIGHT))
			return true;
	}

	if (player.InvBody[INVLOC_HAND_LEFT].isEmpty() && player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield) {
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		if (WearDurabilityPoint(player, INVLOC_HAND_RIGHT))
			return true;
	}

	if (player.InvBody[INVLOC_HAND_RIGHT].isEmpty() && player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield) {
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		if (WearDurabilityPoint(player, INVLOC_HAND_LEFT))
			return true;
	}

	return false;
}

bool PlrHitMonst(Player &player, Monster &monster, bool adjacentDamage = false, int *dealtDamage = nullptr)
{
	int hper = 0;

	if (!monster.isPossibleToHit())
		return false;
	// Not the hero's own army or golem: a swing lands on whatever stands on the tile at the hit frame, and a skeleton
	// stepping into a dying enemy's place took the blow (round 14 audit, v1.12.239).
	// Nor a monster Conversion turned: it is the hero's ally for its span, and the swing, the cleave and Zeal's chain took
	// it (round 19 audit, v1.12.244).
	if (monster.isPlayerMinion() || oracool::IsMonsterConverted(monster))
		return false;

	if (adjacentDamage) {
		if (player._pLevel > 20)
			hper -= 30;
		else
			hper -= (35 - player._pLevel) * 2;
	}

	int hit = GenerateRnd(100);
	if (monster.mode == MonsterMode::Petrified) {
		hit = 0;
	}
	// Smite always connects, as its text says (round 19 audit, v1.12.244): it rolled like any swing, and a miss did nothing.
	// Asked of the latch: IsShieldBashSwing answers which ANIMATION the swing wears, false in play wherever the shield
	// sheet is loaded, so v1.12.244's test never fired (round 20 audit).
	if (!adjacentDamage && &player == MyPlayer && oracool::ArmedMeleeSkill() == oracool::PaladinSkill::ShieldBash
	    && oracool::CanUsePaladinSkill(player, oracool::PaladinSkill::ShieldBash))
		hit = 0;

	hper += player.GetMeleePiercingToHit() - player.CalculateArmorPierce(oracool::EffectiveMonsterArmor(monster), true);
	// Zeal's own accuracy, one point per level invested (user, 2026-08-30). Added before the clamp
	// so it competes with armour on the same terms as every other to-hit source rather than being
	// applied to an already-decided number.
	hper += oracool::ZealToHitBonus(player);
	hper = clamp(hper, 5, 95);
	// The monster the hero swung at - not a cleave's neighbour - is what the sheet's To hit bar measures against.
	if (!adjacentDamage)
		oracool::NotePlayerAttackedMonster(player, monster, /*arrow=*/false, 0);

	if (monster.tryLiftGargoyle())
		return true;

	if (hit >= hper) {
#ifdef _DEBUG
		if (!DebugGodMode)
#endif
			return false;
	}

	// Not in the fork's single player (audit, 2026-09-27): a Flame shard and a Spark shard set both flags, and Hellfire's
	// rule turned the two blows the sheet lists into one spectral bolt, rolled from the fire range, on a landed hit only.
	if (gbIsHellfire && !oracool::IsSinglePlayer() && HasAllOf(player._pIFlags, ItemSpecialEffect::FireDamage | ItemSpecialEffect::LightningDamage)) {
		int midam = player._pIFMinDam + GenerateRnd(player._pIFMaxDam - player._pIFMinDam);
		AddMissile(player.position.tile, player.position.temp, player._pdir, MissileID::SpectralArrow, TARGET_MONSTERS, player.getId(), midam, 0);
	}
	int mind = player._pIMinDam;
	int maxd = player._pIMaxDam;
	int dam = GenerateRnd(maxd - mind + 1) + mind;
	dam += dam * player._pIBonusDam / 100;
	dam += player._pIBonusDamMod;
	// Oracool, Round 4: the armed melee skill's bonus, on every blow of the swing - the extra blows
	// a skill adds come back through this function, so they carry it too. Zero when nothing is
	// armed or the skill cannot be paid for, which is what makes an unaffordable skill a plain swing.
	// The Paladin's two with a per-level blow, Smite and Charge's arrival (2026-09-12), the same way.
	dam += dam * (oracool::ClassMeleeSkillDamagePercent(player) + oracool::PaladinMeleeDamagePercent(player) + oracool::Rfa12MeleeDamagePercent(player)) / 100;
	// And the passives that read the situation - Ruthless, Brawler, Steady Aim and the rest (Round 5).
	dam += dam * (oracool::PassiveDamageDealtPercent(player, monster, true) + oracool::Rfa12DamageDealtPercent(player, monster, true)) / 100;
	int dam2 = dam << 6;
	dam += player._pDamageMod;
	if (player._pClass == HeroClass::Warrior || player._pClass == HeroClass::Barbarian) {
		if (GenerateRnd(100) < player._pLevel) {
			dam *= 2;
		}
	}

	ItemType phanditype = ItemType::None;
	if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Sword || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Sword) {
		phanditype = ItemType::Sword;
	}
	if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Mace || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Mace) {
		phanditype = ItemType::Mace;
	}

	switch (monster.data().monsterClass) {
	case MonsterClass::Undead:
		if (phanditype == ItemType::Sword) {
			dam -= dam / 2;
		} else if (phanditype == ItemType::Mace) {
			dam += dam / 2;
		}
		break;
	case MonsterClass::Animal:
		if (phanditype == ItemType::Mace) {
			dam -= dam / 2;
		} else if (phanditype == ItemType::Sword) {
			dam += dam / 2;
		}
		break;
	case MonsterClass::Demon:
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::TripleDemonDamage)) {
			dam *= 3;
		}
		break;
	}

	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Devastation) && GenerateRnd(100) < 5) {
		dam *= 3;
	}

	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Doppelganger) && monster.type().type != MT_DIABLO && !monster.isUnique() && GenerateRnd(100) < 10) {
		AddDoppelganger(monster);
	}

	dam <<= 6;
	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Jesters)) {
		int r = GenerateRnd(201);
		if (r >= 100)
			r = 100 + (r - 100) * 5;
		// In 64 bits: dam is already x64 here and r reaches 600, and a crit, a triple-demon and Devastation on a strong
		// weapon passed the int limit (round 19 audit, v1.12.244).
		dam = static_cast<int>(std::min<int64_t>(static_cast<int64_t>(dam) * r / 100, std::numeric_limits<int>::max()));
	}

	if (adjacentDamage)
		dam >>= 2;

	if (&player == MyPlayer) {
		if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Peril)) {
			dam2 += player._pIGetHit << 6;
			if (dam2 >= 0) {
				ApplyPlrDamage(DamageType::Physical, player, 0, 1, dam2);
			}
			dam = oracool::PercentOfSat(dam, 200); // not past Jester's clamp (round 27 audit)
		}
#ifdef _DEBUG
		if (DebugGodMode) {
			dam = monster.hitPoints; /* ensure monster is killed with one hit */
		}
#endif
		ApplyMonsterDamage(DamageType::Physical, monster, dam);
		if (dealtDamage != nullptr)
			*dealtDamage = dam;
	}

	int skdam = 0;
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::RandomStealLife)) {
		skdam = GenerateRnd(dam / 8);
		player._pHitPoints += skdam;
		if (player._pHitPoints > player._pMaxHP) {
			player._pHitPoints = player._pMaxHP;
		}
		player._pHPBase += skdam;
		if (player._pHPBase > player._pMaxHPBase) {
			player._pHPBase = player._pMaxHPBase;
		}
		RedrawComponent(PanelDrawComponent::Health);
	}
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealMana3 | ItemSpecialEffect::StealMana5) && HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealMana3)) {
			skdam = 3 * dam / 100;
		}
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealMana5)) {
			skdam = 5 * dam / 100;
		}
		player._pMana += skdam;
		if (player._pMana > player._pMaxMana) {
			player._pMana = player._pMaxMana;
		}
		player._pManaBase += skdam;
		if (player._pManaBase > player._pMaxManaBase) {
			player._pManaBase = player._pMaxManaBase;
		}
		RedrawComponent(PanelDrawComponent::Mana);
	}
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealLife3 | ItemSpecialEffect::StealLife5)) {
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealLife3)) {
			skdam = 3 * dam / 100;
		}
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealLife5)) {
			skdam = 5 * dam / 100;
		}
		player._pHitPoints += skdam;
		if (player._pHitPoints > player._pMaxHP) {
			player._pHitPoints = player._pMaxHP;
		}
		player._pHPBase += skdam;
		if (player._pHPBase > player._pMaxHPBase) {
			player._pHPBase = player._pMaxHPBase;
		}
		RedrawComponent(PanelDrawComponent::Health);
	}
	if ((monster.hitPoints >> 6) <= 0) {
		M_StartKill(monster, player);
	} else {
		if (monster.mode != MonsterMode::Petrified && HasAnyOf(player._pIFlags, ItemSpecialEffect::Knockback))
			M_GetKnockback(monster);
		M_StartHit(monster, player, dam);
	}
	return true;
}

bool PlrHitPlr(Player &attacker, Player &target)
{
	if (target._pInvincible) {
		return false;
	}

	if (HasAnyOf(target._pSpellFlags, SpellFlag::Etherealize)) {
		return false;
	}

	int hit = GenerateRnd(100);

	int hper = attacker.GetMeleeToHit() - target.GetArmor();
	hper = clamp(hper, 5, 95);

	int blk = 100;
	if ((target._pmode == PM_STAND || target._pmode == PM_ATTACK) && target._pBlockFlag) {
		blk = GenerateRnd(100);
	}

	int blkper = target.GetBlockChance() - (attacker._pLevel * 2);
	blkper = clamp(blkper, 0, 100);

	if (hit >= hper) {
		return false;
	}

	if (blk < blkper) {
		Direction dir = GetDirection(target.position.tile, attacker.position.tile);
		StartPlrBlock(target, dir);
		return true;
	}

	int mind = attacker._pIMinDam;
	int maxd = attacker._pIMaxDam;
	int dam = GenerateRnd(maxd - mind + 1) + mind;
	dam += (dam * attacker._pIBonusDam) / 100;
	dam += attacker._pIBonusDamMod + attacker._pDamageMod;

	if (attacker._pClass == HeroClass::Warrior || attacker._pClass == HeroClass::Barbarian) {
		if (GenerateRnd(100) < attacker._pLevel) {
			dam *= 2;
		}
	}
	int skdam = dam << 6;
	if (HasAnyOf(attacker._pIFlags, ItemSpecialEffect::RandomStealLife)) {
		int tac = GenerateRnd(skdam / 8);
		attacker._pHitPoints += tac;
		if (attacker._pHitPoints > attacker._pMaxHP) {
			attacker._pHitPoints = attacker._pMaxHP;
		}
		attacker._pHPBase += tac;
		if (attacker._pHPBase > attacker._pMaxHPBase) {
			attacker._pHPBase = attacker._pMaxHPBase;
		}
		RedrawComponent(PanelDrawComponent::Health);
	}
	if (&attacker == MyPlayer) {
		NetSendCmdDamage(true, target.getId(), skdam, DamageType::Physical);
	}
	StartPlrHit(target, skdam, false);

	return true;
}

bool PlrHitObj(const Player &player, Object &targetObject)
{
	if (targetObject.IsBreakable()) {
		BreakObject(player, targetObject);
		return true;
	}

	return false;
}

bool DoAttack(Player &player)
{
	// Oracool: MeleeHitFrame, not _pAFNum - a Shield Bash borrows the shorter block animation and
	// lands its blow earlier. See oracool/paladin_melee.h.
	const int hitFrame = oracool::MeleeHitFrame(player);
	if (player.AnimInfo.currentFrame == hitFrame - 2) {
		// Oracool: a skill with its own swing sound plays it instead of the whoosh - one sound.
		if (!oracool::PlayArmedSwingCue(player))
			PlaySfxLoc(PS_SWING, player.position.tile);
	}

	bool didhit = false;

	if (player.AnimInfo.currentFrame == hitFrame - 1 && oracool::ThrowArmedWeapon(player)) {
		// Weapon Throw: this swing let go of the weapon instead of striking with it (oracool/weapon_throw.h).
	} else if (player.AnimInfo.currentFrame == hitFrame - 1) {
		Point position = player.position.tile + player._pdir;
		Monster *monster = FindMonsterAtPosition(position);
		// Where the blow's fire and lightning burst: the tile in front, or the enemy Long Reach took - the burst hits only
		// its own tile, and on the empty front tile a reached enemy never took them (round 19 audit, v1.12.244).
		Point blowTile = position;
		// Long Reach (RfA-12): with a staff, spear or pike, a swing at an empty tile reaches the enemy beyond it.
		if (monster == nullptr) {
			monster = oracool::Rfa12ReachTarget(player, position);
			if (monster != nullptr)
				blowTile = monster->position.tile;
		}

		if (monster != nullptr) {
			if (CanTalkToMonst(*monster)) {
				player.position.temp.x = 0; /** @todo Looks to be irrelevant, probably just remove it */
				return false;
			}
		}

		if (!gbIsHellfire || oracool::IsSinglePlayer() || !HasAllOf(player._pIFlags, ItemSpecialEffect::FireDamage | ItemSpecialEffect::LightningDamage)) {
			const size_t playerId = player.getId();
			// Oracool (2026-09-11): OR fire/lightning damage from anywhere, not only from a weapon
			// carrying the flag. Enchant, Vengeance and the two Masteries add to the fire and
			// lightning ranges (the sheet shows them) but set no item flag, so until now their damage
			// reached a blow only when the weapon was already a fire or lightning weapon.
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FireDamage) || player._pIFMaxDam > 0) {
				AddMissile(blowTile, { 1, 0 }, Direction::South, MissileID::WeaponExplosion, TARGET_MONSTERS, playerId, 0, 0);
			}
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::LightningDamage) || player._pILMaxDam > 0) {
				AddMissile(blowTile, { 2, 0 }, Direction::South, MissileID::WeaponExplosion, TARGET_MONSTERS, playerId, 0, 0);
			}
		}

		// Sweeping Reed or Wheel of Heaven strikes the side tiles itself when it fires, so the staff cleave below stands aside -
		// but only when it fires, which the swing itself reports (ClassMeleeSkillSwept, round 28 audit).
		oracool::ForgetClassMeleeSweep();
		if (monster != nullptr) {
			// A swing at a monster, landed or not, is combat: the Barbarian's Rage holds (2026-09-14).
			// A swing at an empty tile is not, and lets the calm clock run.
			oracool::NoteRageCombat(player);
			int hitDamage = 0;
			didhit = PlrHitMonst(player, *monster, false, &hitDamage);
			// Oracool: one hook for every skill that rides a swing - Zeal, Hammer of Faith, Shield
			// Bash. Which of them applies, if any, is decided by the button that threw this swing,
			// latched at the click because it is gone by the time the animation lands. See
			// oracool/paladin_melee.h.
			if (didhit)
				oracool::ApplyMeleeSkillOnHit(player, *monster, hitDamage);
			// Charge's arriving blow: a half-size Holy Bolt burst, Paladin gold, on the enemy it lands on (the Paladin Skill
			// Cards page, 2026-09-28).
			if (didhit && oracool::IsChargeBlowArmed())
				oracool::DrawHolyBurst(player, monster->position.tile, 50, oracool::hue::PaladinGold);
			if (didhit)
				oracool::OnPassiveHit(player, *monster, hitDamage, true);
			if (didhit)
				oracool::OnRfa12Hit(player, *monster, hitDamage, true);
			if (didhit)
				oracool::ApplyVengeanceCold(player, *monster); // Vengeance's third element (2026-09-27)
			if (didhit)
				oracool::OnCursedMonsterStruck(*monster, player, nullptr, hitDamage); // Life Tap (oracool/curses.h)
			// And the Barbarian's and Monk's (Round 4), which want the swing whether or not it
			// landed - Whirlwind spins through an empty front tile as readily as a full one.
			if (oracool::ApplyClassMeleeSkillOnSwing(player, monster, didhit, hitDamage))
				didhit = true;
			// And the RfA-12 swings, on their own latch.
			if (oracool::ApplyRfa12MeleeOnSwing(player, monster, didhit, hitDamage))
				didhit = true;
		} else if (PlayerAtPosition(position) != nullptr && !player.friendlyMode) {
			didhit = PlrHitPlr(player, *PlayerAtPosition(position));
		} else {
			Object *object = FindObjectAtPosition(position, false);
			if (object != nullptr) {
				didhit = PlrHitObj(player, *object);
			}
			if (oracool::ApplyClassMeleeSkillOnSwing(player, nullptr, false, 0))
				didhit = true;
			// The RfA-12 swings too: Cleave, Sweep, Crusade and the rest strike their sides and rear whether or not the
			// front tile holds anything, and a shift swing or a front target gone mid-swing skipped them (round 19 audit).
			if (oracool::ApplyRfa12MeleeOnSwing(player, nullptr, false, 0))
				didhit = true;
		}
		// Not under Sweeping Reed or Wheel of Heaven: they strike these same side tiles themselves, and each side enemy took
		// two blows (round 26 audit, v1.12.251) - when the swing's skill actually fired (round 28 audit).
		const bool skillSweeps = &player == MyPlayer && oracool::ClassMeleeSkillSwept();
		if (!skillSweeps && (player._pClass == HeroClass::Monk
		        && (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Staff || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Staff))
		    || (player._pClass == HeroClass::Bard
		        && player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Sword && player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Sword)
		    || (player._pClass == HeroClass::Barbarian
		        && (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Axe || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Axe
		            || (((player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Mace && player.InvBody[INVLOC_HAND_LEFT]._iLoc == ILOC_TWOHAND)
		                    || (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Mace && player.InvBody[INVLOC_HAND_RIGHT]._iLoc == ILOC_TWOHAND)
		                    || (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Sword && player.InvBody[INVLOC_HAND_LEFT]._iLoc == ILOC_TWOHAND)
		                    || (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Sword && player.InvBody[INVLOC_HAND_RIGHT]._iLoc == ILOC_TWOHAND))
		                && !(player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield))))) {
			// playing as a class/weapon with cleave
			position = player.position.tile + Right(player._pdir);
			monster = FindMonsterAtPosition(position);
			if (monster != nullptr) {
				if (!CanTalkToMonst(*monster) && monster->position.old == position) {
					if (PlrHitMonst(player, *monster, true))
						didhit = true;
				}
			}
			position = player.position.tile + Left(player._pdir);
			monster = FindMonsterAtPosition(position);
			if (monster != nullptr) {
				if (!CanTalkToMonst(*monster) && monster->position.old == position) {
					if (PlrHitMonst(player, *monster, true))
						didhit = true;
				}
			}
		}

		if (didhit && DamageWeapon(player, 30)) {
			StartStand(player, player._pdir);
			ClearStateVariables(player);
			return true;
		}
	}

	// Oracool: a chained Zeal swing hands over the moment its blow has landed, cutting the recovery
	// frames - that is what fits N swings inside 150% of ONE attack (user spec, 2026-08-15). The
	// burst's final swing finds no chain to continue and plays its recovery out through the
	// isLastFrame path below, so the flurry ends on a complete motion.
	if (player.AnimInfo.currentFrame >= oracool::MeleeHitFrame(player) && (oracool::TryContinueZealChain(player) || oracool::TryContinueClassMeleeChain(player)))
		return false;

	if (player.AnimInfo.isLastFrame()) {
		// Also asked here for the natural end: a swing whose hit frame IS its last frame (a heavily
		// compressed chain swing) must still hand over.
		if (oracool::TryContinueZealChain(player) || oracool::TryContinueClassMeleeChain(player))
			return false;
		StartStand(player, player._pdir);
		ClearStateVariables(player);
		return true;
	}

	return false;
}

bool DoRangeAttack(Player &player)
{
	int arrows = 0;
	// _pAFNum, not the melee hit frame: a bow shot never borrows the block animation, and Shield Bash
	// is a melee skill that cannot reach this path at all.
	if (player.AnimInfo.currentFrame == player._pAFNum - 1) {
		arrows = 1;
	}

	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::MultipleArrows) && player.AnimInfo.currentFrame == player._pAFNum + 1) {
		arrows = 2;
	}

	// A bow skill looses its volley on the first release frame only: the multiple-arrows flag's second frame shoots
	// nothing then, and wore the bow twice and counted a Grenadier shot for it (round 19 audit, v1.12.244).
	if (arrows == 2 && &player == MyPlayer && oracool::ArmedArrowSkill().has_value())
		arrows = 0;

	// Grenadier (Rogue, 2026-09-14) counts the shots this frame looses.
	if (arrows > 0 && &player == MyPlayer)
		oracool::OnPassiveArrowLoosed(player, player.position.temp);

	for (int arrow = 0; arrow < arrows; arrow++) {
		int xoff = 0;
		int yoff = 0;
		if (arrows != 1) {
			int angle = arrow == 0 ? -1 : 1;
			int x = player.position.temp.x - player.position.tile.x;
			if (x != 0)
				yoff = x < 0 ? angle : -angle;
			int y = player.position.temp.y - player.position.tile.y;
			if (y != 0)
				xoff = y < 0 ? -angle : angle;
		}

		// Oracool, Round 3: a bow skill on the button looses ITS arrow(s) in place of the plain one
		// (oracool/rogue_arrows.h). The latch is only ever armed for the local player, and the
		// skill decides its own count, so the multiple-arrows item flag's second shot is not fired
		// on top of it - one skill, one volley.
		if (const std::optional<oracool::RogueArrow> skill = oracool::ArmedArrowSkill();
		    skill.has_value() && &player == MyPlayer) {
			// On the FIRST release frame only: Gnat Sting's multiple-arrows flag brings a second release frame, and the
			// skill fired and was paid again on it (round 8 audit, v1.12.233).
			if (arrow == 0 && player.AnimInfo.currentFrame == player._pAFNum - 1) {
				oracool::FireArrowSkill(player, *skill, player.position.temp);
				PlaySfxLoc(PS_BFIRE, player.position.tile);
			}
		} else {
			int dmg = 4;
			MissileID mistype = MissileID::Arrow;
			// Fire and lightning damage from anywhere, as the melee blow takes it (see the WeaponExplosion above): a
			// Ruby or a Ral in a bow, a Flame shard, Enchant - the sheet printed "on every hit" and a plain arrow carried
			// none of it (round 11 audit, v1.12.236).
			const bool fireArrows = HasAnyOf(player._pIFlags, ItemSpecialEffect::FireArrows) || player._pIFMaxDam > 0;
			const bool lightningArrows = HasAnyOf(player._pIFlags, ItemSpecialEffect::LightningArrows) || player._pILMaxDam > 0;
			if (fireArrows) {
				mistype = MissileID::FireArrow;
			}
			if (lightningArrows) {
				mistype = MissileID::LightningArrow;
			}
			// Spectral only for the two Hellfire flags together, the item's own behaviour. Fire and lightning from
			// sockets, shards or Enchant ride one fire arrow instead - its hit carries the lightning too (see
			// ProcessElementalArrow). Made Spectral, the arrow dropped both elements and the bow's own damage (round 12
			// audit, v1.12.237 - a regression of v1.12.236).
			// Spectral only for the BOW's own pair (Flambeau, Blitzen): the two flags from two sources - a Stormcrow rung
			// and a fire bow - made spectral arrows too, and the missile kind was read off the hero's summed lightning
			// minimum, which any lightning source moved (round 21 audit, v1.12.246).
			const Item *spectralBow = nullptr;
			for (const Item *hand : { &player.InvBody[INVLOC_HAND_LEFT], &player.InvBody[INVLOC_HAND_RIGHT] }) {
				if (hand->_itype == ItemType::Bow && hand->_iStatFlag
				    && HasAllOf(hand->_iFlags, ItemSpecialEffect::FireArrows | ItemSpecialEffect::LightningArrows))
					spectralBow = hand;
			}
			if (fireArrows && lightningArrows && spectralBow == nullptr)
				mistype = MissileID::FireArrow;
			else if (fireArrows && lightningArrows) {
				// The hero's fire range, the bow's with a Ruby, a Flame shard or Enchant on top: the kind is the bow's, the
				// damage all of it (round 22 audit of v1.12.246).
				dmg = player._pIFMinDam + GenerateRnd(player._pIFMaxDam - player._pIFMinDam);
				mistype = MissileID::SpectralArrow;
			}

			AddMissile(
			    player.position.tile,
			    player.position.temp + Displacement { xoff, yoff },
			    player._pdir,
			    mistype,
			    TARGET_MONSTERS,
			    player.getId(),
			    dmg,
			    0);

			if (arrow == 0 && mistype != MissileID::SpectralArrow) {
				PlaySfxLoc(arrows != 1 ? IS_STING1 : PS_BFIRE, player.position.tile);
			}
		}

		if (DamageWeapon(player, 40)) {
			StartStand(player, player._pdir);
			ClearStateVariables(player);
			return true;
		}
	}

	if (player.AnimInfo.isLastFrame()) {
		StartStand(player, player._pdir);
		ClearStateVariables(player);
		return true;
	}
	return false;
}

void DamageParryItem(Player &player)
{
	if (&player != MyPlayer) {
		return;
	}

	if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield || player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Staff) {
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == DUR_INDESTRUCTIBLE) {
			return;
		}

		WearDurabilityPoint(player, INVLOC_HAND_LEFT);
	}

	if (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield) {
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability != DUR_INDESTRUCTIBLE) {
			WearDurabilityPoint(player, INVLOC_HAND_RIGHT);
		}
	}
}

bool DoBlock(Player &player)
{
	if (player.AnimInfo.isLastFrame()) {
		StartStand(player, player._pdir);
		ClearStateVariables(player);

		if (FlipCoin(10)) {
			DamageParryItem(player);
		}
		return true;
	}

	return false;
}

void DamageArmor(Player &player)
{
	if (&player != MyPlayer) {
		return;
	}

	// Every worn armor slot is in the wear pool, not just head and chest (external audit,
	// 2026-08-17): the Oracool worn slots shipped with gear that could never lose a point of
	// durability, which made the stat - and ethereal's half-lifespan bargain - meaningless there.
	// One point per trigger, one uniformly-chosen worn piece, same total wear rate as vanilla
	// spread across more gear; the smith repairs all ten slots (see stores.cpp's
	// RepairableBodySlots).
	constexpr inv_body_loc ArmorWearSlots[] = {
		INVLOC_HEAD, INVLOC_CHEST,
		INVLOC_SHOULDERS, INVLOC_BRACERS, INVLOC_GLOVES, INVLOC_WAIST, INVLOC_LEGS, INVLOC_BOOTS
	};

	inv_body_loc worn[sizeof(ArmorWearSlots) / sizeof(ArmorWearSlots[0])];
	int wornCount = 0;
	for (const inv_body_loc loc : ArmorWearSlots) {
		const Item &item = player.InvBody[loc];
		// Broken pieces are excluded, not merely spared: leaving one in the pool would let it
		// absorb wear ticks that should have landed on gear that still has durability to lose.
		if (!item.isEmpty() && !item._iOracoolBroken && item._iDurability != DUR_INDESTRUCTIBLE)
			worn[wornCount++] = loc;
	}
	if (wornCount == 0) {
		return;
	}

	const inv_body_loc target = worn[wornCount == 1 ? 0 : GenerateRnd(wornCount)];
	WearDurabilityPoint(player, target);
}

bool DoSpell(Player &player)
{
	if (player.AnimInfo.currentFrame == player._pSFNum) {
		CastSpell(
		    player.getId(),
		    player.executedSpell.spellId,
		    player.position.tile.x,
		    player.position.tile.y,
		    player.position.temp.x,
		    player.position.temp.y,
		    player.executedSpell.spellLevel);

		if (IsAnyOf(player.executedSpell.spellType, SpellType::Scroll, SpellType::Charges)) {
			EnsureValidReadiedSpell(player);
		}
	}

	if (player.AnimInfo.isLastFrame()) {
		StartStand(player, player._pdir);
		ClearStateVariables(player);
		return true;
	}

	return false;
}

bool DoGotHit(Player &player)
{
	if (player.AnimInfo.isLastFrame()) {
		StartStand(player, player._pdir);
		ClearStateVariables(player);
		if (!FlipCoin(4)) {
			DamageArmor(player);
		}

		return true;
	}

	return false;
}

bool DoDeath(Player &player)
{
	if (player.AnimInfo.isLastFrame()) {
		if (player.AnimInfo.tickCounterOfCurrentFrame == 0) {
			player.AnimInfo.ticksPerFrame = 100;
			dFlags[player.position.tile.x][player.position.tile.y] |= DungeonFlag::DeadPlayer;
		} else if (&player == MyPlayer && player.AnimInfo.tickCounterOfCurrentFrame == 30) {
			MyPlayerIsDead = true;
			if (!gbIsMultiplayer) {
				gamemenu_on();
			}
		}
	}

	return false;
}

bool IsPlayerAdjacentToObject(Player &player, Object &object)
{
	int x = abs(player.position.tile.x - object.position.x);
	int y = abs(player.position.tile.y - object.position.y);
	if (y > 1 && object.position.y >= 1 && FindObjectAtPosition(object.position + Direction::NorthEast) == &object) {
		// special case for activating a large object from the north-east side
		y = abs(player.position.tile.y - object.position.y + 1);
	}
	return x <= 1 && y <= 1;
}

void TryDisarm(const Player &player, Object &object)
{
	if (&player == MyPlayer)
		NewCursor(CURSOR_HAND);
	if (!object._oTrapFlag) {
		return;
	}
	int trapdisper = 2 * player._pDexterity - 5 * currlevel;
	if (GenerateRnd(100) > trapdisper) {
		return;
	}
	for (int j = 0; j < ActiveObjectCount; j++) {
		Object &trap = Objects[ActiveObjects[j]];
		if (trap.IsTrap() && FindObjectAtPosition({ trap._oVar1, trap._oVar2 }) == &object) {
			trap._oVar4 = 1;
			object._oTrapFlag = false;
		}
	}
	if (object.IsTrappedChest()) {
		object._oTrapFlag = false;
	}
}

void CheckNewPath(Player &player, bool pmWillBeCalled)
{
	int x = 0;
	int y = 0;

	Monster *monster;
	Player *target;
	Object *object;
	Item *item;

	int targetId = player.destParam1;

	switch (player.destAction) {
	case ACTION_ATTACKMON:
	case ACTION_RATTACKMON:
	case ACTION_SPELLMON:
		monster = &Monsters[targetId];
		if ((monster->hitPoints >> 6) <= 0) {
			// Oracool: the target died before a furious charge landed - cancel the dash without
			// starting a cooldown (StopFuriousChargeDash is a no-op when not dashing).
			oracool::StopFuriousChargeDash();
			player.Stop();
			return;
		}
		if (player.destAction == ACTION_ATTACKMON)
			MakePlrPath(player, monster->position.future, false);
		break;
	case ACTION_ATTACKPLR:
	case ACTION_RATTACKPLR:
	case ACTION_SPELLPLR:
		target = &Players[targetId];
		if ((target->_pHitPoints >> 6) <= 0) {
			player.Stop();
			return;
		}
		if (player.destAction == ACTION_ATTACKPLR)
			MakePlrPath(player, target->position.future, false);
		break;
	case ACTION_OPERATE:
	case ACTION_DISARM:
	case ACTION_OPERATETK:
		object = &Objects[targetId];
		break;
	case ACTION_PICKUPITEM:
	case ACTION_PICKUPAITEM:
		item = &Items[targetId];
		break;
	default:
		break;
	}

	Direction d;
	if (player.walkpath[0] != WALK_NONE) {
		if (player._pmode == PM_STAND) {
			if (&player == MyPlayer) {
				if (player.destAction == ACTION_ATTACKMON || player.destAction == ACTION_ATTACKPLR) {
					if (player.destAction == ACTION_ATTACKMON) {
						x = abs(player.position.future.x - monster->position.future.x);
						y = abs(player.position.future.y - monster->position.future.y);
						d = GetDirection(player.position.future, monster->position.future);
					} else {
						x = abs(player.position.future.x - target->position.future.x);
						y = abs(player.position.future.y - target->position.future.y);
						d = GetDirection(player.position.future, target->position.future);
					}

					if (x < 2 && y < 2) {
						ClrPlrPath(player);
						if (player.destAction == ACTION_ATTACKMON && monster->talkMsg != TEXT_NONE && monster->talkMsg != TEXT_VILE14) {
							TalktoMonster(player, *monster);
						} else {
							StartAttack(player, d, pmWillBeCalled);
						}
						player.destAction = ACTION_NONE;
					}
				}
			}

			switch (player.walkpath[0]) {
			case WALK_N:
				StartWalk(player, Direction::North, pmWillBeCalled);
				break;
			case WALK_NE:
				StartWalk(player, Direction::NorthEast, pmWillBeCalled);
				break;
			case WALK_E:
				StartWalk(player, Direction::East, pmWillBeCalled);
				break;
			case WALK_SE:
				StartWalk(player, Direction::SouthEast, pmWillBeCalled);
				break;
			case WALK_S:
				StartWalk(player, Direction::South, pmWillBeCalled);
				break;
			case WALK_SW:
				StartWalk(player, Direction::SouthWest, pmWillBeCalled);
				break;
			case WALK_W:
				StartWalk(player, Direction::West, pmWillBeCalled);
				break;
			case WALK_NW:
				StartWalk(player, Direction::NorthWest, pmWillBeCalled);
				break;
			}

			for (size_t j = 1; j < MaxPathLength; j++) {
				player.walkpath[j - 1] = player.walkpath[j];
			}

			player.walkpath[MaxPathLength - 1] = WALK_NONE;

			if (player._pmode == PM_STAND) {
				StartStand(player, player._pdir);
				player.destAction = ACTION_NONE;
			}
		}

		return;
	}
	if (player.destAction == ACTION_NONE) {
		return;
	}

	if (player._pmode == PM_STAND) {
		switch (player.destAction) {
		case ACTION_ATTACK:
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartAttack(player, d, pmWillBeCalled);
			break;
		case ACTION_ATTACKMON:
			x = abs(player.position.tile.x - monster->position.future.x);
			y = abs(player.position.tile.y - monster->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, monster->position.future);
				if (monster->talkMsg != TEXT_NONE && monster->talkMsg != TEXT_VILE14) {
					TalktoMonster(player, *monster);
				} else {
					StartAttack(player, d, pmWillBeCalled);
				}
			} else if (&player == MyPlayer) {
				// Stood short of a monster it could not reach: a Charge's dash ends here, paid, rather than staying armed
				// for 2 s for the next melee click to sprint and land the arriving blow free (round 20 audit, v1.12.245).
				oracool::StopFuriousChargeDash();
			}
			break;
		case ACTION_ATTACKPLR:
			x = abs(player.position.tile.x - target->position.future.x);
			y = abs(player.position.tile.y - target->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, target->position.future);
				StartAttack(player, d, pmWillBeCalled);
			}
			break;
		case ACTION_RATTACK:
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartRangeAttack(player, d, player.destParam1, player.destParam2, pmWillBeCalled);
			break;
		case ACTION_RATTACKMON:
			d = GetDirection(player.position.future, monster->position.future);
			if (monster->talkMsg != TEXT_NONE && monster->talkMsg != TEXT_VILE14) {
				TalktoMonster(player, *monster);
			} else {
				StartRangeAttack(player, d, monster->position.future.x, monster->position.future.y, pmWillBeCalled);
			}
			break;
		case ACTION_RATTACKPLR:
			d = GetDirection(player.position.future, target->position.future);
			StartRangeAttack(player, d, target->position.future.x, target->position.future.y, pmWillBeCalled);
			break;
		case ACTION_SPELL:
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartSpell(player, d, player.destParam1, player.destParam2);
			break;
		case ACTION_SPELLWALL:
			StartSpell(player, static_cast<Direction>(player.destParam3), player.destParam1, player.destParam2);
			player.tempDirection = static_cast<Direction>(player.destParam3);
			break;
		case ACTION_SPELLMON:
			d = GetDirection(player.position.tile, monster->position.future);
			StartSpell(player, d, monster->position.future.x, monster->position.future.y);
			break;
		case ACTION_SPELLPLR:
			d = GetDirection(player.position.tile, target->position.future);
			StartSpell(player, d, target->position.future.x, target->position.future.y);
			break;
		case ACTION_OPERATE:
			if (IsPlayerAdjacentToObject(player, *object)) {
				if (object->_oBreak == 1) {
					d = GetDirection(player.position.tile, object->position);
					StartAttack(player, d, pmWillBeCalled);
				} else {
					OperateObject(player, *object);
				}
			}
			break;
		case ACTION_DISARM:
			if (IsPlayerAdjacentToObject(player, *object)) {
				if (object->_oBreak == 1) {
					d = GetDirection(player.position.tile, object->position);
					StartAttack(player, d, pmWillBeCalled);
				} else {
					TryDisarm(player, *object);
					OperateObject(player, *object);
				}
			}
			break;
		case ACTION_OPERATETK:
			if (object->_oBreak != 1) {
				OperateObject(player, *object);
			}
			break;
		case ACTION_PICKUPITEM:
			if (&player == MyPlayer) {
				x = abs(player.position.tile.x - item->position.x);
				y = abs(player.position.tile.y - item->position.y);
				if (x <= 1 && y <= 1 && pcurs == CURSOR_HAND && !item->_iRequest) {
					NetSendCmdGItem(true, CMD_REQUESTGITEM, player.getId(), targetId);
					item->_iRequest = true;
				}
			}
			break;
		case ACTION_PICKUPAITEM:
			if (&player == MyPlayer) {
				x = abs(player.position.tile.x - item->position.x);
				y = abs(player.position.tile.y - item->position.y);
				if (x <= 1 && y <= 1 && pcurs == CURSOR_HAND) {
					NetSendCmdGItem(true, CMD_REQUESTAGITEM, player.getId(), targetId);
				}
			}
			break;
		case ACTION_TALK:
			if (&player == MyPlayer) {
				HelpFlag = false;
				TalkToTowner(player, player.destParam1);
			}
			break;
		default:
			break;
		}

		FixPlayerLocation(player, player._pdir);
		player.destAction = ACTION_NONE;

		return;
	}

	if (player._pmode == PM_ATTACK && player.AnimInfo.currentFrame >= player._pAFNum) {
		if (player.destAction == ACTION_ATTACK) {
			d = GetDirection(player.position.future, { player.destParam1, player.destParam2 });
			StartAttack(player, d, pmWillBeCalled);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_ATTACKMON) {
			x = abs(player.position.tile.x - monster->position.future.x);
			y = abs(player.position.tile.y - monster->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, monster->position.future);
				StartAttack(player, d, pmWillBeCalled);
			}
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_ATTACKPLR) {
			x = abs(player.position.tile.x - target->position.future.x);
			y = abs(player.position.tile.y - target->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, target->position.future);
				StartAttack(player, d, pmWillBeCalled);
			}
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_OPERATE) {
			if (IsPlayerAdjacentToObject(player, *object)) {
				if (object->_oBreak == 1) {
					d = GetDirection(player.position.tile, object->position);
					StartAttack(player, d, pmWillBeCalled);
				}
			}
		}
	}

	if (player._pmode == PM_RATTACK && player.AnimInfo.currentFrame >= player._pAFNum) {
		if (player.destAction == ACTION_RATTACK) {
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartRangeAttack(player, d, player.destParam1, player.destParam2, pmWillBeCalled);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_RATTACKMON) {
			d = GetDirection(player.position.tile, monster->position.future);
			StartRangeAttack(player, d, monster->position.future.x, monster->position.future.y, pmWillBeCalled);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_RATTACKPLR) {
			d = GetDirection(player.position.tile, target->position.future);
			StartRangeAttack(player, d, target->position.future.x, target->position.future.y, pmWillBeCalled);
			player.destAction = ACTION_NONE;
		}
	}

	if (player._pmode == PM_SPELL && player.AnimInfo.currentFrame >= player._pSFNum) {
		if (player.destAction == ACTION_SPELL) {
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartSpell(player, d, player.destParam1, player.destParam2);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_SPELLMON) {
			d = GetDirection(player.position.tile, monster->position.future);
			StartSpell(player, d, monster->position.future.x, monster->position.future.y);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_SPELLPLR) {
			d = GetDirection(player.position.tile, target->position.future);
			StartSpell(player, d, target->position.future.x, target->position.future.y);
			player.destAction = ACTION_NONE;
		}
	}
}

bool PlrDeathModeOK(Player &player)
{
	if (&player != MyPlayer) {
		return true;
	}
	if (player._pmode == PM_DEATH) {
		return true;
	}
	if (player._pmode == PM_QUIT) {
		return true;
	}
	if (player._pmode == PM_NEWLVL) {
		return true;
	}

	return false;
}

void ValidatePlayer()
{
	assert(MyPlayer != nullptr);
	Player &myPlayer = *MyPlayer;

	if (myPlayer._pLevel > MaxCharacterLevel)
		myPlayer._pLevel = MaxCharacterLevel;
	if (myPlayer._pExperience > myPlayer._pNextExper) {
		myPlayer._pExperience = myPlayer._pNextExper;
		if (*sgOptions.Gameplay.experienceBar) {
			RedrawEverything();
		}
	}

	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (myPlayer.InvList[i]._itype == ItemType::Gold && myPlayer.InvList[i]._ivalue > MaxGold)
			myPlayer.InvList[i]._ivalue = MaxGold;
	}
	// Summed the saturating way: an int accumulator wrapped negative past 2.1 billion in the backpack and overwrote the
	// value CalculateGold had just stored, every tick (round 11 audit, v1.12.236).
	myPlayer._pGold = CalculateGold(myPlayer);

	// 255 is the hard save-format-safe ceiling for base attributes.
	myPlayer._pBaseStr = std::min(myPlayer._pBaseStr, 255);
	myPlayer._pBaseMag = std::min(myPlayer._pBaseMag, 255);
	myPlayer._pBaseDex = std::min(myPlayer._pBaseDex, 255);
	myPlayer._pBaseVit = std::min(myPlayer._pBaseVit, 255);

	if (!gbIsMultiplayer) {
		const auto portal = static_cast<size_t>(SpellID::TownPortal);
		myPlayer._pMemSpells |= GetSpellBitmask(SpellID::TownPortal);
		myPlayer._pSplLvl[portal] = std::max<uint8_t>(myPlayer._pSplLvl[portal], 1);
	}

	SpellMask msk;
	for (int b = static_cast<int16_t>(SpellID::Firebolt); b < MAX_SPELLS; b++) {
		if (GetSpellBookLevel((SpellID)b) != -1) {
			msk |= GetSpellBitmask(static_cast<SpellID>(b));
			// The book-level store is 64 wide; a book spell with a higher id would have indexed
			// past it here every tick (external audit, 2026-09-06: SKL-02). None exists yet.
			if (static_cast<size_t>(b) < std::size(myPlayer._pSplLvl) && myPlayer._pSplLvl[b] > MaxSpellLevel)
				myPlayer._pSplLvl[b] = MaxSpellLevel;
		}
	}

	myPlayer._pMemSpells &= msk;
}

void CheckCheatStats(Player &player)
{
	if (player._pStrength > 750) {
		player._pStrength = 750;
	}

	if (player._pDexterity > 750) {
		player._pDexterity = 750;
	}

	if (player._pMagic > 750) {
		player._pMagic = 750;
	}

	if (player._pVitality > 750) {
		player._pVitality = 750;
	}

	// Life and mana are held at the hero's own maximum, not at vanilla's 2000 (128000 in 64ths): that ceiling was set
	// for level-50 heroes, and a fork hero past 2000 maximum could never fill the orb - potions and regeneration stopped
	// at 2000 every tick (round 11 audit, v1.12.236).
	if (player._pHitPoints > player._pMaxHP) {
		player._pHitPoints = player._pMaxHP;
	}

	if (player._pMana > player._pMaxMana) {
		player._pMana = player._pMaxMana;
	}
}

HeroClass GetPlayerSpriteClass(HeroClass cls)
{
	if (cls == HeroClass::Bard && !gbBard)
		return HeroClass::Rogue;
	if (cls == HeroClass::Barbarian && !gbBarbarian)
		return HeroClass::Warrior;
	// The Necromancer has no sheets of his own and no archive that could bring any: he is the Sorcerer's body,
	// dyed (oracool/hero_look), always.
	if (cls == HeroClass::Necromancer)
		return HeroClass::Sorcerer;
	return cls;
}

PlayerWeaponGraphic GetPlayerWeaponGraphic(player_graphic graphic, PlayerWeaponGraphic weaponGraphic)
{
	if (graphic == player_graphic::ShieldAttack)
		return PlayerWeaponGraphic::UnarmedShield; // the shield strike is the unarmed-with-shield sheet, whatever is held
	if (leveltype == DTYPE_TOWN && IsAnyOf(graphic, player_graphic::Lightning, player_graphic::Fire, player_graphic::Magic)) {
		// If the hero doesn't hold the weapon in town then we should use the unarmed animation for casting
		switch (weaponGraphic) {
		case PlayerWeaponGraphic::Mace:
		case PlayerWeaponGraphic::Sword:
			return PlayerWeaponGraphic::Unarmed;
		case PlayerWeaponGraphic::SwordShield:
		case PlayerWeaponGraphic::MaceShield:
			return PlayerWeaponGraphic::UnarmedShield;
		default:
			break;
		}
	}
	return weaponGraphic;
}

uint16_t GetPlayerSpriteWidth(HeroClass cls, player_graphic graphic, PlayerWeaponGraphic weaponGraphic)
{
	PlayerSpriteData spriteData = PlayersSpriteData[static_cast<size_t>(cls)];

	switch (graphic) {
	case player_graphic::Stand:
		return spriteData.stand;
	case player_graphic::Walk:
		return spriteData.walk;
	case player_graphic::Attack:
		if (weaponGraphic == PlayerWeaponGraphic::Bow)
			return spriteData.bow;
		return spriteData.attack;
	case player_graphic::Hit:
		return spriteData.swHit;
	case player_graphic::Block:
		return spriteData.block;
	case player_graphic::ShieldAttack:
		return spriteData.attack;
	case player_graphic::Lightning:
		return spriteData.lightning;
	case player_graphic::Fire:
		return spriteData.fire;
	case player_graphic::Magic:
		return spriteData.magic;
	case player_graphic::Death:
		return spriteData.death;
	}
	app_fatal("Invalid player_graphic");
}

} // namespace

/**
 * Vanilla could afford to write this out seven times, because breaking an item REMOVED it from the
 * slot: the next wear tick found the slot empty and there was nothing left to decrement. This fork
 * leaves a broken item equipped and inert instead (BreakOrRemoveEquipment, inv.cpp), which turned
 * all seven copies into runaways - the item sits at zero and keeps being decremented - and four of
 * the seven tested `== 0` rather than `<= 0`, so they sailed straight past the break they were
 * meant to catch and drove durability negative for the rest of the character's life.
 *
 * User, 2026-08-27: "i have magic oracool items with negative durability."
 */
bool WearDurabilityPoint(Player &player, inv_body_loc slot)
{
	Item &item = player.InvBody[slot];
	// A broken item has nothing left to spend. It is still worn, still drawn and still repairable
	// at the smith - it just no longer takes wear.
	if (item.isEmpty() || item._iOracoolBroken || item._iDurability == DUR_INDESTRUCTIBLE)
		return false;

	if (item._iDurability > 0)
		item._iDurability--;

	if (item._iDurability > 0)
		return false;

	// Clamped rather than left where it landed: a save written by a build that had the runaway can
	// hand us a deeply negative number, and the repair cost, the durability bar and the smith's
	// "Dur: x/y" line all read this field directly.
	item._iDurability = 0;
	BreakOrRemoveEquipment(player, slot, true);
	CalcPlrInv(player, true);
	return true;
}

void Player::CalcScrolls()
{
	_pScrlSpells = 0;
	for (Item &item : InventoryAndBeltPlayerItemsRange { *this }) {
		if (item.isScroll() && item._iStatFlag) {
			_pScrlSpells |= GetSpellBitmask(item._iSpell);
		}
	}
	EnsureValidReadiedSpell(*this);
}

bool Player::CanUseItem(const Item &item) const
{
	if (!IsItemValid(item))
		return false;

	// Sockets v2: Hel reduces the host's own requirements. Read here rather than written into the
	// item, so it cannot compound across the many recalculations a character sheet triggers.
	// The shrunken heads are the Necromancer's alone (oracool/necro_items.h): red in any other hand.
	if (!oracool::ClassMayUseItem(*this, item))
		return false;
	// The fourth requirement (2026-09-20): a character level, derived from the item -
	// oracool/level_requirement.h. Asked here, in CalcSelfItems and on the item panel, nowhere else.
	return _pStrength >= oracool::EffectiveRequirement(item, item._iMinStr)
	    && _pMagic >= oracool::EffectiveRequirement(item, item._iMinMag)
	    && _pDexterity >= oracool::EffectiveRequirement(item, item._iMinDex)
	    && _pLevel >= oracool::RequiredLevel(item);
}

void Player::RemoveInvItem(int iv, bool calcScrolls)
{
	if (this == MyPlayer) {
		// Locate the first grid index containing this item and notify remote clients
		for (size_t i = 0; i < InventoryGridCells; i++) {
			int8_t itemIndex = InvGrid[i];
			if (abs(itemIndex) - 1 == iv) {
				NetSendCmdParam1(false, CMD_DELINVITEMS, i);
				break;
			}
		}
	}

	// Iterate through invGrid and remove every reference to item
	for (int8_t &itemIndex : InvGrid) {
		if (abs(itemIndex) - 1 == iv) {
			itemIndex = 0;
		}
	}

	InvList[iv].clear();

	_pNumInv--;

	// If the item at the end of inventory array isn't the one we removed, we need to swap its position in the array with the removed item
	if (_pNumInv > 0 && _pNumInv != iv) {
		InvList[iv] = InvList[_pNumInv].pop();

		for (int8_t &itemIndex : InvGrid) {
			if (itemIndex == _pNumInv + 1) {
				itemIndex = iv + 1;
			}
			if (itemIndex == -(_pNumInv + 1)) {
				itemIndex = -(iv + 1);
			}
		}
	}

	if (calcScrolls) {
		CalcScrolls();
	}
}

void Player::RemoveSpdBarItem(int iv)
{
	if (this == MyPlayer) {
		NetSendCmdParam1(false, CMD_DELBELTITEMS, iv);
	}

	SpdList[iv].clear();

	CalcScrolls();
	RedrawEverything();
}

[[nodiscard]] size_t Player::getId() const
{
	return std::distance<const Player *>(&Players[0], this);
}

int Player::GetBaseAttributeValue(CharacterAttribute attribute) const
{
	switch (attribute) {
	case CharacterAttribute::Dexterity:
		return this->_pBaseDex;
	case CharacterAttribute::Magic:
		return this->_pBaseMag;
	case CharacterAttribute::Strength:
		return this->_pBaseStr;
	case CharacterAttribute::Vitality:
		return this->_pBaseVit;
	default:
		app_fatal("Unsupported attribute");
	}
}

int Player::GetCurrentAttributeValue(CharacterAttribute attribute) const
{
	switch (attribute) {
	case CharacterAttribute::Dexterity:
		return this->_pDexterity;
	case CharacterAttribute::Magic:
		return this->_pMagic;
	case CharacterAttribute::Strength:
		return this->_pStrength;
	case CharacterAttribute::Vitality:
		return this->_pVitality;
	default:
		app_fatal("Unsupported attribute");
	}
}

int Player::GetMaximumAttributeValue(CharacterAttribute attribute) const
{
	PlayerData plrData = PlayersData[static_cast<std::size_t>(_pClass)];
	switch (attribute) {
	case CharacterAttribute::Strength:
		return plrData.maxStr;
	case CharacterAttribute::Magic:
		return plrData.maxMag;
	case CharacterAttribute::Dexterity:
		return plrData.maxDex;
	case CharacterAttribute::Vitality:
		return plrData.maxVit;
	}
	app_fatal("Unsupported attribute");
}

Point Player::GetTargetPosition() const
{
	// clang-format off
	constexpr int DirectionOffsetX[8] = {  0,-1, 1, 0,-1, 1, 1,-1 };
	constexpr int DirectionOffsetY[8] = { -1, 0, 0, 1,-1,-1, 1, 1 };
	// clang-format on
	Point target = position.future;
	for (auto step : walkpath) {
		if (step == WALK_NONE)
			break;
		if (step > 0) {
			target.x += DirectionOffsetX[step - 1];
			target.y += DirectionOffsetY[step - 1];
		}
	}
	return target;
}

bool Player::IsPositionInPath(Point pos)
{
	constexpr Displacement DirectionOffset[8] = { { 0, -1 }, { -1, 0 }, { 1, 0 }, { 0, 1 }, { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
	Point target = position.future;
	for (auto step : walkpath) {
		if (target == pos) {
			return true;
		}
		if (step == WALK_NONE)
			break;
		if (step > 0) {
			target += DirectionOffset[step - 1];
		}
	}
	return false;
}

void Player::Say(HeroSpeech speechId) const
{
	_sfx_id soundEffect = herosounds[static_cast<size_t>(_pClass)][static_cast<size_t>(speechId)];

	if (soundEffect == SFX_NONE)
		return;

	PlaySfxLoc(soundEffect, position.tile);
}

void Player::SaySpecific(HeroSpeech speechId) const
{
	_sfx_id soundEffect = herosounds[static_cast<size_t>(_pClass)][static_cast<size_t>(speechId)];

	if (soundEffect == SFX_NONE || effect_is_playing(soundEffect))
		return;

	PlaySfxLoc(soundEffect, position.tile, false);
}

void Player::Say(HeroSpeech speechId, int delay) const
{
	sfxdelay = delay;
	sfxdnum = herosounds[static_cast<size_t>(_pClass)][static_cast<size_t>(speechId)];
}

void Player::Stop()
{
	ClrPlrPath(*this);
	destAction = ACTION_NONE;
}

bool Player::isWalking() const
{
	return IsAnyOf(_pmode, PM_WALK_NORTHWARDS, PM_WALK_SOUTHWARDS, PM_WALK_SIDEWAYS);
}

int ManaShieldDamageReductionAtLevel(int spellLevel)
{
	constexpr int Max = 7;
	return 24 - std::min(spellLevel, Max) * 3;
}

int Player::GetManaShieldDamageReduction()
{
	// Oracool: reads the EFFECTIVE level, not the raw memorised one. Vanilla read _pSplLvl here, so
	// neither +spell-level items nor this fork's skill investment reached the shield - the one
	// ladder that opted out of the Phase 2.1 seam. The Monk's Spirit Ward rides this spell, and a
	// row that takes five points has to buy something with all five.
	return ManaShieldDamageReductionAtLevel(GetSpellLevel(SpellID::ManaShield));
}

int Player::CalcPartialLifeRestoreAmount() const
{
	int wholeHitpoints = _pMaxHP >> 6;
	int l = ((wholeHitpoints / 8) + GenerateRnd(wholeHitpoints / 4)) << 6;
	if (IsAnyOf(_pClass, HeroClass::Warrior, HeroClass::Barbarian))
		l *= 2;
	if (IsAnyOf(_pClass, HeroClass::Rogue, HeroClass::Monk, HeroClass::Bard))
		l += l / 2;
	return l;
}

void Player::RestorePartialLife()
{
	int l = CalcPartialLifeRestoreAmount();
	_pHitPoints = std::min(_pHitPoints + l, _pMaxHP);
	_pHPBase = std::min(_pHPBase + l, _pMaxHPBase);
}

int Player::CalcPartialManaRestoreAmount() const
{
	int wholeManaPoints = _pMaxMana >> 6;
	int l = ((wholeManaPoints / 8) + GenerateRnd(wholeManaPoints / 4)) << 6;
	if (IsAnyOf(_pClass, HeroClass::Sorcerer, HeroClass::Necromancer))
		l *= 2;
	if (IsAnyOf(_pClass, HeroClass::Rogue, HeroClass::Monk, HeroClass::Bard))
		l += l / 2;
	return l;
}

void Player::RestorePartialMana()
{
	int l = CalcPartialManaRestoreAmount();
	if (HasNoneOf(_pIFlags, ItemSpecialEffect::NoMana)) {
		_pMana = std::min(_pMana + l, _pMaxMana);
		_pManaBase = std::min(_pManaBase + l, _pMaxManaBase);
	}
}

void Player::ReadySpellFromEquipment(inv_body_loc bodyLocation, bool forceSpell)
{
	auto &item = InvBody[bodyLocation];
	if (item._itype == ItemType::Staff && IsValidSpell(item._iSpell) && item._iCharges > 0) {
		// Never while an aura burns: the aura IS the right button (ClearClassAuraForRightButton), and a staff equipped or
		// recharged under one readied its spell beside it - aura bonuses plus a staff spell, the hover and the well
		// disagreeing (round 18 audit, v1.12.243). The staff's cell stays in the picker.
		if (oracool::GetActiveClassAura(*this) != oracool::ClassTreeSkill::None)
			return;
		if (forceSpell || _pRSpell == SpellID::Invalid || _pRSplType == SpellType::Invalid) {
			_pRSpell = item._iSpell;
			_pRSplType = SpellType::Charges;
			RedrawEverything();
		}
	}
}

player_graphic Player::getGraphic() const
{
	switch (_pmode) {
	case PM_STAND:
	case PM_NEWLVL:
	case PM_QUIT:
		return player_graphic::Stand;
	case PM_WALK_NORTHWARDS:
	case PM_WALK_SOUTHWARDS:
	case PM_WALK_SIDEWAYS:
		return player_graphic::Walk;
	case PM_ATTACK:
		return oracool::SwingsShieldAttackSheet(*this) ? player_graphic::ShieldAttack : player_graphic::Attack;
	case PM_RATTACK:
		return player_graphic::Attack;
	case PM_BLOCK:
		return player_graphic::Block;
	case PM_SPELL:
		return GetPlayerGraphicForSpell(executedSpell.spellId);
	case PM_GOTHIT:
		return player_graphic::Hit;
	case PM_DEATH:
		return player_graphic::Death;
	default:
		app_fatal("SyncPlrAnim");
	}
}

uint16_t Player::getSpriteWidth() const
{
	if (!HeadlessMode)
		return (*AnimInfo.sprites)[0].width();
	const player_graphic graphic = getGraphic();
	const HeroClass cls = GetPlayerSpriteClass(_pClass);
	const PlayerWeaponGraphic weaponGraphic = GetPlayerWeaponGraphic(graphic, static_cast<PlayerWeaponGraphic>(_pgfxnum & 0xF));
	return GetPlayerSpriteWidth(cls, graphic, weaponGraphic);
}

void Player::getAnimationFramesAndTicksPerFrame(player_graphic graphics, int8_t &numberOfFrames, int8_t &ticksPerFrame) const
{
	ticksPerFrame = 1;
	switch (graphics) {
	case player_graphic::Stand:
		numberOfFrames = _pNFrames;
		ticksPerFrame = 4;
		break;
	case player_graphic::Walk:
		numberOfFrames = _pWFrames;
		break;
	case player_graphic::Attack:
		numberOfFrames = _pAFrames;
		break;
	case player_graphic::ShieldAttack:
		numberOfFrames = PlayersAnimData[static_cast<size_t>(_pClass)].unarmedShieldFrames;
		break;
	case player_graphic::Hit:
		numberOfFrames = _pHFrames;
		break;
	case player_graphic::Lightning:
	case player_graphic::Fire:
	case player_graphic::Magic:
		numberOfFrames = _pSFrames;
		break;
	case player_graphic::Death:
		numberOfFrames = _pDFrames;
		ticksPerFrame = 2;
		break;
	case player_graphic::Block:
		numberOfFrames = _pBFrames;
		ticksPerFrame = 3;
		// Oracool bug fix (2026-08-16): user report - Shield Bash "makes very rapid hits, but should
		// make regular shield hits animation". The bash borrows this graphic, and the block animation
		// is 2-6 frames against an attack's 16-20 - at 3 ticks a frame each shove was over in a third
		// of a second and holding the button read as a jackhammer. Stretched so the WHOLE shove lasts
		// about one regular attack (_pAFrames ticks), which is the pace of every other swing; a plain
		// raised-shield block still plays at vanilla speed.
		if (oracool::IsShieldBashSwing(*this) && _pBFrames > 0)
			ticksPerFrame = static_cast<int8_t>(std::clamp(static_cast<int>(_pAFrames) / _pBFrames, 3, 12));
		break;
	default:
		app_fatal("Unknown player graphics");
	}
}

void Player::UpdatePreviewCelSprite(_cmd_id cmdId, Point point, uint16_t wParam1, uint16_t wParam2)
{
	// if game is not running don't show a preview
	if (!gbRunGame || PauseMode != 0 || !gbProcessPlayers)
		return;

	// we can only show a preview if our command is executed in the next game tick
	if (_pmode != PM_STAND)
		return;

	std::optional<player_graphic> graphic;
	Direction dir = Direction::South;
	int minimalWalkDistance = -1;

	switch (cmdId) {
	case _cmd_id::CMD_RATTACKID: {
		auto &monster = Monsters[wParam1];
		dir = GetDirection(position.future, monster.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_SPELLID: {
		auto &monster = Monsters[wParam1];
		dir = GetDirection(position.future, monster.position.future);
		graphic = GetPlayerGraphicForSpell(static_cast<SpellID>(wParam2));
		break;
	}
	case _cmd_id::CMD_ATTACKID: {
		auto &monster = Monsters[wParam1];
		point = monster.position.future;
		minimalWalkDistance = 2;
		if (!CanTalkToMonst(monster)) {
			dir = GetDirection(position.future, monster.position.future);
			graphic = player_graphic::Attack;
		}
		break;
	}
	case _cmd_id::CMD_RATTACKPID: {
		Player &targetPlayer = Players[wParam1];
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_SPELLPID: {
		Player &targetPlayer = Players[wParam1];
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = GetPlayerGraphicForSpell(static_cast<SpellID>(wParam2));
		break;
	}
	case _cmd_id::CMD_ATTACKPID: {
		Player &targetPlayer = Players[wParam1];
		point = targetPlayer.position.future;
		minimalWalkDistance = 2;
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_ATTACKXY:
		dir = GetDirection(position.tile, point);
		graphic = player_graphic::Attack;
		minimalWalkDistance = 2;
		break;
	case _cmd_id::CMD_RATTACKXY:
	case _cmd_id::CMD_SATTACKXY:
		dir = GetDirection(position.tile, point);
		graphic = player_graphic::Attack;
		break;
	case _cmd_id::CMD_SPELLXY:
		dir = GetDirection(position.tile, point);
		graphic = GetPlayerGraphicForSpell(static_cast<SpellID>(wParam1));
		break;
	case _cmd_id::CMD_SPELLXYD:
		dir = static_cast<Direction>(wParam2);
		graphic = GetPlayerGraphicForSpell(static_cast<SpellID>(wParam1));
		break;
	case _cmd_id::CMD_WALKXY:
		minimalWalkDistance = 1;
		break;
	case _cmd_id::CMD_TALKXY:
	case _cmd_id::CMD_DISARMXY:
	case _cmd_id::CMD_OPOBJXY:
	case _cmd_id::CMD_GOTOGETITEM:
	case _cmd_id::CMD_GOTOAGETITEM:
		minimalWalkDistance = 2;
		break;
	default:
		return;
	}

	if (minimalWalkDistance >= 0 && position.future != point) {
		int8_t testWalkPath[MaxPathLength];
		int steps = FindPath([this](Point position) { return PosOkPlayer(*this, position); }, position.future, point, testWalkPath);
		if (steps == 0) {
			// Can't walk to desired location => stand still
			return;
		}
		if (steps >= minimalWalkDistance) {
			graphic = player_graphic::Walk;
			switch (testWalkPath[0]) {
			case WALK_N:
				dir = Direction::North;
				break;
			case WALK_NE:
				dir = Direction::NorthEast;
				break;
			case WALK_E:
				dir = Direction::East;
				break;
			case WALK_SE:
				dir = Direction::SouthEast;
				break;
			case WALK_S:
				dir = Direction::South;
				break;
			case WALK_SW:
				dir = Direction::SouthWest;
				break;
			case WALK_W:
				dir = Direction::West;
				break;
			case WALK_NW:
				dir = Direction::NorthWest;
				break;
			}
			if (!PlrDirOK(*this, dir))
				return;
		}
	}

	if (!graphic || HeadlessMode)
		return;

	LoadPlrGFX(*this, *graphic);
	const OptionalClxSpriteList sprites = AnimationData[static_cast<size_t>(*graphic)].spritesForDirection(dir);
	// No sheet, no preview. LoadPlrGFX declines Attack and Hit in town and Block without the block
	// flag, and this runs on every click - it is the busiest path into an animation that might not
	// be there.
	if (!sprites)
		return;
	if (!previewCelSprite || *previewCelSprite != (*sprites)[0]) {
		previewCelSprite = (*sprites)[0];
		progressToNextGameTickWhenPreviewWasSet = ProgressToNextGameTick;
	}
}

int32_t Player::calculateBaseLife() const
{
	const PlayerData &playerData = PlayersData[static_cast<size_t>(_pClass)];
	return playerData.adjLife + (playerData.lvlLife * _pLevel) + (playerData.chrLife * _pBaseVit);
}

int32_t Player::calculateBaseMana() const
{
	const PlayerData &playerData = PlayersData[static_cast<size_t>(_pClass)];
	return playerData.adjMana + (playerData.lvlMana * _pLevel) + (playerData.chrMana * _pBaseMag);
}

Player *PlayerAtPosition(Point position)
{
	if (!InDungeonBounds(position))
		return nullptr;

	auto playerIndex = dPlayer[position.x][position.y];
	if (playerIndex == 0)
		return nullptr;

	return &Players[abs(playerIndex) - 1];
}

namespace oracool {

PlayerWeaponGraphic BlockSheetFallback(PlayerWeaponGraphic weapon)
{
	switch (weapon) {
	case PlayerWeaponGraphic::Axe:
	case PlayerWeaponGraphic::Staff:
	case PlayerWeaponGraphic::Mace:
		return PlayerWeaponGraphic::MaceShield;
	case PlayerWeaponGraphic::Sword:
		return PlayerWeaponGraphic::SwordShield;
	case PlayerWeaponGraphic::Unarmed:
	case PlayerWeaponGraphic::Bow:
		return PlayerWeaponGraphic::UnarmedShield;
	case PlayerWeaponGraphic::UnarmedShield:
	case PlayerWeaponGraphic::SwordShield:
	case PlayerWeaponGraphic::MaceShield:
		break;
	}
	return weapon;
}

} // namespace oracool

void LoadPlrGFX(Player &player, player_graphic graphic)
{
	if (HeadlessMode)
		return;

	auto &animationData = player.AnimationData[static_cast<size_t>(graphic)];
	if (animationData.sprites)
		return;

	const HeroClass cls = GetPlayerSpriteClass(player._pClass);
	PlayerWeaponGraphic animWeaponId = GetPlayerWeaponGraphic(graphic, static_cast<PlayerWeaponGraphic>(player._pgfxnum & 0xF));

	const char *path = PlayersData[static_cast<std::size_t>(cls)].classPath;

	const char *szCel;
	switch (graphic) {
	case player_graphic::Stand:
		szCel = "as";
		if (leveltype == DTYPE_TOWN)
			szCel = "st";
		break;
	case player_graphic::Walk:
		szCel = "aw";
		if (leveltype == DTYPE_TOWN)
			szCel = "wl";
		break;
	case player_graphic::Attack:
		if (leveltype == DTYPE_TOWN)
			return;
		szCel = "at";
		break;
	case player_graphic::Hit:
		if (leveltype == DTYPE_TOWN)
			return;
		szCel = "ht";
		break;
	case player_graphic::Lightning:
		szCel = "lm";
		break;
	case player_graphic::Fire:
		szCel = "fm";
		break;
	case player_graphic::Magic:
		szCel = "qm";
		break;
	case player_graphic::Death:
		// Oracool bug fix (2026-08-16, user crash report - "assertion failed ... value_.data_ !=
		// nullptr" on death): this used to RETURN when a weapon was held, loading nothing at all.
		//
		// The death animation is only authored unarmed, and the four places that start it all clear
		// the weapon nibble first - so the refusal looked harmless. It is not, because it is not the
		// only caller. CalcPlrItemVals rebuilds the current animation whenever _pgfxnum changes, and
		// on a DEAD player getGraphic() is Death while _pgfxnum has just been recomputed from the
		// weapon still in hand. LoadPlrGFX refused, AnimInfo was bound to an EMPTY sprite list, and
		// the next DrawPlayer dereferenced it.
		//
		// Fixed the way the Shield Bash block-sheet crash was fixed at v1.6.24 - at the single
		// authority, by ASKING for the sheet that exists rather than making every caller remember to.
		// Nothing else reads the weapon for this graphic: the frame count comes from _pDFrames and
		// GetPlayerSpriteWidth returns spriteData.death whatever the weapon is.
		animWeaponId = PlayerWeaponGraphic::Unarmed;
		szCel = "dt";
		break;
	case player_graphic::Block:
		if (leveltype == DTYPE_TOWN)
			return;
		if (!player._pBlockFlag)
			return;
		szCel = "bl";
		break;
	case player_graphic::ShieldAttack:
		// The Paladin alone strikes with the shield (Shield Bash, Aegis Slam); nothing to load in town, where no one swings.
		if (leveltype == DTYPE_TOWN || player._pClass != HeroClass::Warrior)
			return;
		szCel = "at";
		break;
	default:
		app_fatal("PLR:2");
	}

	if (HeadlessMode)
		return;

	char prefix[3] = { CharChar[static_cast<std::size_t>(cls)], ArmourChar[player._pgfxnum >> 4], WepChar[static_cast<std::size_t>(animWeaponId)] };
	char pszName[256];
	*fmt::format_to(pszName, R"(plrgfx\{0}\{1}\{1}{2})", path, string_view(prefix, 3), szCel) = 0;
	// Oracool (user crash, 2026-09-11: "Failed to open file: plrgfx\warrior\wma\wmabl.cl2", entering a
	// dungeon). Heavenly Strength puts an axe, a bow or a staff beside a shield, and no block sheet was ever
	// drawn for that pair - the vanilla game never allowed it - so the archive has none, and the level load
	// that asks for every graphic stopped the game. Asked of the archive rather than assumed, so a class that
	// HAS the sheet keeps it (the Monk blocks with his staff); otherwise the shield-carrying block nearest the
	// weapon. Before the width and the PNG look-up below, which follow the weapon graphic.
	if (graphic == player_graphic::Block && !FindAsset((std::string(pszName) + DEVILUTIONX_CL2_EXT).c_str()).ok()) {
		animWeaponId = oracool::BlockSheetFallback(animWeaponId);
		prefix[2] = WepChar[static_cast<std::size_t>(animWeaponId)];
		*fmt::format_to(pszName, R"(plrgfx\{0}\{1}\{1}{2})", path, string_view(prefix, 3), szCel) = 0;
	}
	const uint16_t animationWidth = GetPlayerSpriteWidth(cls, graphic, animWeaponId);

	// Oracool: a PNG sheet supplied for this animation wins over the CL2. Looked up under the
	// character's OWN class rather than `cls` - which is the class whose CL2s it borrows, "warrior"
	// for a Barbarian - because supplying art is exactly how a class stops borrowing. Absent or
	// malformed, LoadPngSpriteSheet returns nothing and the original loads as before, so a class can
	// be converted one animation at a time. See oracool/sprite_import.h.
	char pngName[256];
	*fmt::format_to(pngName, R"(plrgfx\{0}\{1}\{1}{2}.png)",
	    oracool::ClassSpriteFolder(player._pClass), string_view(prefix, 3), szCel)
	    = 0;
	//
	// In TRUE COLOUR since 2026-09-17: an imported sheet keeps its own colours and carries them as
	// animationData.colours, which DrawPlayer draws through. Its indices mean nothing to the level
	// palette, so the class TRN - an index translation - is for the CL2 alone.
	animationData.colours = nullptr;
	if (std::optional<oracool::ColouredSpriteSheet> imported = oracool::LoadPngSpriteSheetColoured(pngName, animationWidth)) {
		animationData.sprites = std::move(imported->sheet);
		animationData.colours = std::move(imported->colours);
	} else {
		// A shield or a sword from another armour tier, by what is actually held (oracool/sprite_mix.h). Never
		// built HERE: that was a hitch on the first frame of every animation (user, 2026-09-17). A sheet built
		// before comes out of the cache already at the size of the class; one never built is asked for in the
		// background, the plain sheet is worn meanwhile, and PumpPlayerSheetMixer swaps it in when it arrives.
		animationData.pendingSheetKey.clear();
		const oracool::PlayerSheetRequest request = oracool::MakePlayerSheetRequest(player, cls, animWeaponId, szCel, animationWidth);
		if (oracool::WantsMixedSheet(request)) {
			std::optional<oracool::ColouredSpriteSheet> cached;
			switch (oracool::TakeCachedPlayerSheet(request, cached)) {
			case oracool::CachedSheetState::Ready:
				animationData.sprites = std::move(cached->sheet);
				animationData.colours = std::move(cached->colours);
				return;
			case oracool::CachedSheetState::Unknown:
				oracool::RequestPlayerSheet(request);
				animationData.pendingSheetKey = request.Key();
				break;
			case oracool::CachedSheetState::Nothing:
				break;
			}
		}
		animationData.sprites = LoadCl2Sheet(pszName, animationWidth);
		std::optional<std::array<uint8_t, 256>> trn = GetClassTRN(player);
		if (trn) {
			ClxApplyTrans(*animationData.sprites, trn->data());
		}
		// The dye of the class, as colours rather than baked indices: the sheet stays the Warrior sheet and only
		// what its indices MEAN changes. Null for every class but the one that has a dye.
		animationData.colours = oracool::HeroColours(player);
	}

	// Oracool: the class's own size on the borrowed body (2026-09-16). Scaling moves indices about and
	// never changes one, so the colours above still describe the result. See oracool/hero_look.h.
	if (const int scale = oracool::SpriteScalePercent(player._pClass); scale != 100) {
		if (OptionalOwnedClxSpriteSheet scaled = oracool::ScaleSpriteSheet(*animationData.sprites, scale))
			animationData.sprites = std::move(scaled);
	}
}

void InitPlayerGFX(Player &player)
{
	if (HeadlessMode)
		return;

	ResetPlayerGFX(player);

	if (player._pHitPoints >> 6 == 0) {
		player._pgfxnum &= ~0xFU;
		LoadPlrGFX(player, player_graphic::Death);
		return;
	}

	for (size_t i = 0; i < enum_size<player_graphic>::value; i++) {
		auto graphic = static_cast<player_graphic>(i);
		if (graphic == player_graphic::Death)
			continue;
		LoadPlrGFX(player, graphic);
	}
}

void ResetPlayerGFX(Player &player)
{
	player.AnimInfo.sprites = std::nullopt;
	for (PlayerAnimationData &animData : player.AnimationData) {
		animData.sprites = std::nullopt;
		animData.colours = nullptr;
		animData.pendingSheetKey.clear();
	}
}

void PrewarmPlayerLook(Player &player)
{
	if (HeadlessMode)
		return;
	const HeroClass cls = GetPlayerSpriteClass(player._pClass);
	struct Animation {
		player_graphic graphic;
		const char *cel;
	};
	// Every animation the look can touch, asked for the moment the gear changes - so by the first swing the
	// attack sheet is already built, or nearly. The names are the ones LoadPlrGFX uses: town has its own standing
	// and walking sheets, and no attack or hit at all.
	const bool town = leveltype == DTYPE_TOWN;
	const Animation Animations[] = {
		{ player_graphic::Stand, town ? "st" : "as" }, { player_graphic::Walk, town ? "wl" : "aw" },
		{ player_graphic::Attack, town ? nullptr : "at" }, { player_graphic::Hit, town ? nullptr : "ht" },
		{ player_graphic::Lightning, "lm" }, { player_graphic::Fire, "fm" }, { player_graphic::Magic, "qm" },
		{ player_graphic::ShieldAttack, town || player._pClass != HeroClass::Warrior ? nullptr : "at" },
	};
	for (const Animation &animation : Animations) {
		if (animation.cel == nullptr)
			continue;
		const PlayerWeaponGraphic weapon = GetPlayerWeaponGraphic(animation.graphic, static_cast<PlayerWeaponGraphic>(player._pgfxnum & 0xF));
		const oracool::PlayerSheetRequest request = oracool::MakePlayerSheetRequest(player, cls, weapon, animation.cel, GetPlayerSpriteWidth(cls, animation.graphic, weapon));
		if (!oracool::WantsMixedSheet(request))
			continue;
		std::optional<oracool::ColouredSpriteSheet> unused;
		if (oracool::TakeCachedPlayerSheet(request, unused) == oracool::CachedSheetState::Unknown)
			oracool::RequestPlayerSheet(request);
	}
}

void PumpPlayerSheetMixer()
{
	if (HeadlessMode)
		return;
	oracool::PumpSpriteMixer();
	for (const oracool::FinishedPlayerSheet &finished : oracool::TakeFinishedPlayerSheets()) {
		for (Player &player : Players) {
			for (size_t g = 0; g < player.AnimationData.size(); g++) {
				PlayerAnimationData &animationData = player.AnimationData[g];
				if (animationData.pendingSheetKey != finished.key)
					continue;
				animationData.pendingSheetKey.clear();
				if (finished.nothing || !animationData.sprites)
					continue;

				// The sheet about to be freed may be the one on screen: AnimInfo and the preview sprite are VIEWS
				// into it, so whichever of them points inside it is re-bound to the new sheet, never left dangling.
				const ClxSpriteSheet old { *animationData.sprites };
				const auto inside = [&](const uint8_t *p) { return p >= old.data() && p < old.data() + old.dataSize(); };
				const bool showing = player.AnimInfo.sprites && inside(player.AnimInfo.sprites->data());
				if (player.previewCelSprite && inside(player.previewCelSprite->pixelData()))
					player.previewCelSprite = std::nullopt;

				oracool::ColouredSpriteSheet fresh = finished.make();
				animationData.sprites = std::move(fresh.sheet);
				animationData.colours = std::move(fresh.colours);
				if (showing) {
					int8_t numberOfFrames;
					int8_t ticksPerFrame;
					player.getAnimationFramesAndTicksPerFrame(static_cast<player_graphic>(g), numberOfFrames, ticksPerFrame);
					player.AnimInfo.changeAnimationData(animationData.spritesForDirection(player._pdir), numberOfFrames, ticksPerFrame);
				}
			}
		}
	}
}

void NewPlrAnim(Player &player, player_graphic graphic, Direction dir, AnimationDistributionFlags flags /*= AnimationDistributionFlags::None*/, int8_t numSkippedFrames /*= 0*/, int8_t distributeFramesBeforeFrame /*= 0*/)
{
	LoadPlrGFX(player, graphic);

	OptionalClxSpriteList sprites;
	int previewShownGameTickFragments = 0;
	if (!HeadlessMode) {
		sprites = player.AnimationData[static_cast<size_t>(graphic)].spritesForDirection(dir);
		// `sprites` first: LoadPlrGFX still declines three graphics outright - Attack and Hit in
		// town, and Block without the block flag - so an empty list here is a reachable state, not
		// an impossible one, and this deref had no guard at all.
		if (sprites && player.previewCelSprite && (*sprites)[0] == *player.previewCelSprite && !player.isWalking()) {
			previewShownGameTickFragments = clamp<int>(AnimationInfo::baseValueFraction - player.progressToNextGameTickWhenPreviewWasSet, 0, AnimationInfo::baseValueFraction);
		}
	}

	int8_t numberOfFrames;
	int8_t ticksPerFrame;
	player.getAnimationFramesAndTicksPerFrame(graphic, numberOfFrames, ticksPerFrame);
	player.AnimInfo.setNewAnimation(sprites, numberOfFrames, ticksPerFrame, flags, numSkippedFrames, distributeFramesBeforeFrame, static_cast<uint8_t>(previewShownGameTickFragments));
}

void SetPlrAnims(Player &player)
{
	HeroClass pc = player._pClass;
	PlayerAnimData plrAtkAnimData = PlayersAnimData[static_cast<uint8_t>(pc)];
	auto gn = static_cast<PlayerWeaponGraphic>(player._pgfxnum & 0xFU);

	if (leveltype == DTYPE_TOWN) {
		player._pNFrames = plrAtkAnimData.townIdleFrames;
		player._pWFrames = plrAtkAnimData.townWalkingFrames;
	} else {
		player._pNFrames = plrAtkAnimData.idleFrames;
		player._pWFrames = plrAtkAnimData.walkingFrames;
		player._pHFrames = plrAtkAnimData.recoveryFrames;
		player._pBFrames = plrAtkAnimData.blockingFrames;
		switch (gn) {
		case PlayerWeaponGraphic::Unarmed:
			player._pAFrames = plrAtkAnimData.unarmedFrames;
			player._pAFNum = plrAtkAnimData.unarmedActionFrame;
			break;
		case PlayerWeaponGraphic::UnarmedShield:
			player._pAFrames = plrAtkAnimData.unarmedShieldFrames;
			player._pAFNum = plrAtkAnimData.unarmedShieldActionFrame;
			break;
		case PlayerWeaponGraphic::Sword:
			player._pAFrames = plrAtkAnimData.swordFrames;
			player._pAFNum = plrAtkAnimData.swordActionFrame;
			break;
		case PlayerWeaponGraphic::SwordShield:
			player._pAFrames = plrAtkAnimData.swordShieldFrames;
			player._pAFNum = plrAtkAnimData.swordShieldActionFrame;
			break;
		case PlayerWeaponGraphic::Bow:
			player._pAFrames = plrAtkAnimData.bowFrames;
			player._pAFNum = plrAtkAnimData.bowActionFrame;
			break;
		case PlayerWeaponGraphic::Axe:
			player._pAFrames = plrAtkAnimData.axeFrames;
			player._pAFNum = plrAtkAnimData.axeActionFrame;
			break;
		case PlayerWeaponGraphic::Mace:
			player._pAFrames = plrAtkAnimData.maceFrames;
			player._pAFNum = plrAtkAnimData.maceActionFrame;
			break;
		case PlayerWeaponGraphic::MaceShield:
			player._pAFrames = plrAtkAnimData.maceShieldFrames;
			player._pAFNum = plrAtkAnimData.maceShieldActionFrame;
			break;
		case PlayerWeaponGraphic::Staff:
			player._pAFrames = plrAtkAnimData.staffFrames;
			player._pAFNum = plrAtkAnimData.staffActionFrame;
			break;
		}
	}

	player._pDFrames = plrAtkAnimData.deathFrames;
	player._pSFrames = plrAtkAnimData.castingFrames;
	player._pSFNum = plrAtkAnimData.castingActionFrame;
	int armorGraphicIndex = player._pgfxnum & ~0xFU;
	if (IsAnyOf(pc, HeroClass::Warrior, HeroClass::Barbarian)) {
		if (gn == PlayerWeaponGraphic::Bow && leveltype != DTYPE_TOWN)
			player._pNFrames = 8;
		if (armorGraphicIndex > 0)
			player._pDFrames = 15;
	}
}

/**
 * @param player The player reference.
 * @param c The hero class.
 */
void CreatePlayer(Player &player, HeroClass c)
{
	player = {};
	// `player = {}` clears everything ON the Player, but the milestone mask and the signet count
	// live in side tables keyed by player slot, so they survive it (audit, 2026-08-26). A new hero
	// is created in slot 0, which the character-select screen has just used to preview every
	// existing save - and pfile_ui_save_create serialises the tail immediately, so whatever the
	// last previewed character had claimed became this one's starting position.
	oracool::ResetProgressionState(player);
	SetRndSeed(SDL_GetTicks());

	const PlayerData &playerData = PlayersData[static_cast<size_t>(c)];

	player._pLevel = 1;
	player._pClass = c;

	player._pBaseStr = playerData.baseStr;
	player._pStrength = player._pBaseStr;

	player._pBaseMag = playerData.baseMag;
	player._pMagic = player._pBaseMag;

	player._pBaseDex = playerData.baseDex;
	player._pDexterity = player._pBaseDex;

	player._pBaseVit = playerData.baseVit;
	player._pVitality = player._pBaseVit;

	player._pBaseToBlk = playerData.blockBonus;

	player._pHitPoints = player.calculateBaseLife();
	player._pMaxHP = player._pHitPoints;
	player._pHPBase = player._pHitPoints;
	player._pMaxHPBase = player._pHitPoints;

	player._pMana = player.calculateBaseMana();
	player._pMaxMana = player._pMana;
	player._pManaBase = player._pMana;
	player._pMaxManaBase = player._pMana;

	player._pMaxLvl = player._pLevel;
	player._pExperience = 0;
	player._pNextExper = ExpLvlsTbl[1];
	player._pArmorClass = 0;
	player._pLightRad = 10;
	player._pInfraFlag = false;

	// Nothing readied on either button. playerData.skill - Item Repair for the Paladin, Trap Disarm
	// for the Rogue and so on - is no longer granted at all (user, 2026-08-19), so readying it here
	// would put a skill the character does not have on their right hand, which is exactly what a new
	// Paladin was spawning with. The right button starts as the basic attack, like the left.
	player._pRSplType = SpellType::Invalid;
	player._pAblSpells = oracool::InnateSpellsBitmask(player);
	player._pRSpell = SpellID::Invalid;
	// Left button starts as the plain attack, which is vanilla behaviour.
	player._pLRSpell = SpellID::Invalid;
	player._pLRSplType = SpellType::Invalid;

	// The Necromancer too, until his own first skill exists (Teeth, phase N6 of the plan): every one of his 72
	// rows is inert at N1, and a caster with nothing to cast is not a playable class.
	if (IsAnyOf(c, HeroClass::Sorcerer, HeroClass::Necromancer)) {
		player._pMemSpells = GetSpellBitmask(SpellID::Firebolt);
		player._pRSplType = SpellType::Spell;
		player._pRSpell = SpellID::Firebolt;
	} else {
		player._pMemSpells = 0;
	}

	for (uint8_t &spellLevel : player._pSplLvl) {
		spellLevel = 0;
	}

	player._pSpellFlags = SpellFlag::None;

	if (IsAnyOf(player._pClass, HeroClass::Sorcerer, HeroClass::Necromancer)) {
		player._pSplLvl[static_cast<int16_t>(SpellID::Firebolt)] = 2;
	}

	// Initializing the hotkey bindings to no selection
	std::fill(player._pSplHotKey, player._pSplHotKey + NumHotkeys, SpellID::Invalid);

	// The last character's mouse buttons, where they still make sense on this one (user,
	// 2026-08-30). AFTER the class defaults above, so a first-ever Sorcerer still starts on
	// Firebolt and a remembered byte only overrides it when this character can actually use what it
	// names - UnpackReadiedSpell leaves the slot alone otherwise, which is what saves this from
	// needing a class check of its own.
	oracool::ApplyRememberedReadiedSpells(player);

	PlayerWeaponGraphic animWeaponId = PlayerWeaponGraphic::Unarmed;
	switch (c) {
	case HeroClass::Warrior:
	case HeroClass::Bard:
	case HeroClass::Barbarian:
		animWeaponId = PlayerWeaponGraphic::SwordShield;
		break;
	case HeroClass::Rogue:
		animWeaponId = PlayerWeaponGraphic::Bow;
		break;
	case HeroClass::Sorcerer:
	case HeroClass::Monk:
	case HeroClass::Necromancer:
		animWeaponId = PlayerWeaponGraphic::Staff;
		break;
	}
	player._pgfxnum = static_cast<uint8_t>(animWeaponId);

	for (bool &levelVisited : player._pLvlVisited) {
		levelVisited = false;
	}

	for (int i = 0; i < 10; i++) {
		player._pSLvlVisited[i] = false;
	}

	player._pLvlChanging = false;
	player.pTownWarps = 0;
	player.pLvlLoad = 0;
	player.pManaShield = false;
	player.pDamAcFlags = ItemSpecialEffectHf::None;
	player.wReflections = 0;

	InitDungMsgs(player);
	CreatePlrItems(player);
	SetRndSeed(0);
}

int CalcStatDiff(Player &player)
{
	int diff = 0;
	for (auto attribute : enum_values<CharacterAttribute>()) {
		diff += 255;
		diff -= player.GetBaseAttributeValue(attribute);
	}
	return diff;
}

void NextPlrLevel(Player &player)
{
	player._pLevel++;
	player._pMaxLvl++;

	// Oracool: Charge is granted by CHARACTER level (12), and _pAblSpells was previously only built
	// at creation and on load - so without this the skill would not appear until the next reload.
	// Recomputing the whole innate mask rather than OR-ing one bit keeps this site from needing to
	// know which skills are level-gated.
	player._pAblSpells = oracool::InnateSpellsBitmask(player);

	// D2MXL-to-ORCL Phase 2: the level milestones. Checked HERE rather than per-tick, because a
	// level-up is the only moment their answer can change - and ClaimMilestone is idempotent, so
	// re-testing every threshold on every level costs a handful of bit tests.
	oracool::CheckPassiveMilestones(player);

	// Oracool: user request - the same "quest completed" jingle used when the Poisoned Water
	// Supply quest finishes (see quests.cpp's StartPWaterPurify), repurposed as a level-up cue.
	// No sound played on level-up before this.
	if (&player == MyPlayer) {
		PlaySfxLoc(IS_QUESTDN, player.position.tile);
		oracool::LogEvent(fmt::format("Reached level {:d}", player._pLevel), UiFlags::ColorWhitegold);
	}

	// Oracool Phase 2.1: one skill point per level, D2's rate.
	oracool::GrantLevelUpSkillPoint(player);

	CalcPlrInv(player, true);

	if (CalcStatDiff(player) < 5) {
		player._pStatPts = CalcStatDiff(player);
	} else {
		player._pStatPts += 5;
	}
	player._pNextExper = ExpLvlsTbl[std::min<int8_t>(player._pLevel, MaxCharacterLevel - 1)];

	int hp = PlayersData[static_cast<size_t>(player._pClass)].lvlLife;

	player._pMaxHP += hp;
	player._pHitPoints = player._pMaxHP;
	player._pMaxHPBase += hp;
	player._pHPBase = player._pMaxHPBase;

	if (&player == MyPlayer) {
		RedrawComponent(PanelDrawComponent::Health);
	}

	int mana = PlayersData[static_cast<size_t>(player._pClass)].lvlMana;

	player._pMaxMana += mana;
	player._pMaxManaBase += mana;

	if (HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		player._pMana = player._pMaxMana;
		player._pManaBase = player._pMaxManaBase;
	}

	if (&player == MyPlayer) {
		RedrawComponent(PanelDrawComponent::Mana);
	}

	if (ControlMode != ControlTypes::KeyboardAndMouse)
		FocusOnCharInfo();

	CalcPlrInv(player, true);
}

uint64_t KillExperienceFor(const Player &player, int monsterLevel, int monsterExp)
{
	// Diablo I's own level-gap factor (a tenth per level either way, ZERO ten levels down) is gone
	// with Diablo II's table: stacked under the brake below it paid nothing for most of Hell to a
	// hero past 80, and the table's last thirty levels were out of reach (audit, 2026-09-20).
	// Diablo II has no bonus for a monster above the hero either; the brakes are the whole rule.
	uint64_t clampedExp = static_cast<uint64_t>(std::max(monsterExp, 0));

	// Diablo II's two brakes (2026-09-20, decision D3 of the Level Requirements plan - they come
	// with its experience table, which the fork's Diablo I monster experience would otherwise race
	// up). One: a monster more than five levels below the hero pays its share of the hero's level
	// (mlvl / clvl). Two: above level 70 the gain itself is cut, five points a level down to a tenth
	// at 88, then a twentieth from 95 - the shape of Diablo II's post-70 table.
	const int clvl = std::max<int>(1, player._pLevel);
	if (clvl - monsterLevel > 5)
		clampedExp = clampedExp * static_cast<uint64_t>(std::max(1, monsterLevel)) / static_cast<uint64_t>(clvl);
	if (clvl > 70) {
		const int percent = clvl <= 88 ? 100 - 5 * (clvl - 70) : (clvl < 95 ? 10 : 5);
		clampedExp = clampedExp * static_cast<uint64_t>(percent) / 100;
	}

	// Prevent power leveling
	if (gbIsMultiplayer) {
		const uint32_t clampedPlayerLevel = clamp(static_cast<int>(player._pLevel), 1, MaxCharacterLevel);

		// for low level characters experience gain is capped to 1/20 of current levels xp
		// for high level characters experience gain is capped to 200 * current level - this is a smaller value than 1/20 of the exp needed for the next level after level 5.
		// UINT64_C(200), not a plain 200ULL literal - uint64_t is `unsigned long` rather than
		// `unsigned long long` on LP64 platforms (e.g. PS4), and std::min's initializer_list
		// overload requires every element to deduce to the exact same type.
		// The table has MaxCharacterLevel entries (0..98); a hero AT the cap reads the last one (audit, 2026-09-20).
		clampedExp = std::min({ clampedExp, /* level 0-5: */ ExpLvlsTbl[std::min<uint32_t>(clampedPlayerLevel, MaxCharacterLevel - 1)] / 20U, /* level 6-99: */ UINT64_C(200) * clampedPlayerLevel });
	}
	return clampedExp;
}

void AddPlrExperience(Player &player, int lvl, int exp)
{
	if (&player != MyPlayer || player._pHitPoints <= 0)
		return;

	if (player._pLevel >= MaxCharacterLevel) {
		player._pLevel = MaxCharacterLevel;
		return;
	}

	const uint64_t clampedExp = KillExperienceFor(player, lvl, exp);

	const uint64_t MaxExperience = ExpLvlsTbl[MaxCharacterLevel - 1];

	// Overflow is only possible if a kill grants more than (2^64-1 - MaxExperience) XP in one go, which doesn't happen in normal gameplay. Clamp to experience required to reach max level
	const uint64_t previousExperience = player._pExperience;
	player._pExperience = std::min(player._pExperience + clampedExp, MaxExperience);

	// Oracool: user request - a brief "+N" flash below the XP Counter showing the actual amount
	// gained (after every clamp above), not the raw, pre-clamp exp argument.
	if (*sgOptions.Oracool.xpGainIndicator && player._pExperience > previousExperience)
		oracool::TriggerXpGainIndicator(player._pExperience - previousExperience);

	// Oracool: user request - experience gain persists instantly, matching Diablo 3's
	// always-saved progress rather than waiting for the next scheduled/periodic save.
	if (player._pExperience > previousExperience)
		oracool::ScheduleAutoSaveForExperienceGain();

	if (*sgOptions.Gameplay.experienceBar) {
		RedrawEverything();
	}

	// Increase player level if applicable
	int newLvl = player._pLevel;
	while (newLvl < MaxCharacterLevel && player._pExperience >= ExpLvlsTbl[newLvl]) {
		newLvl++;
	}
	if (newLvl != player._pLevel) {
		for (int i = newLvl - player._pLevel; i > 0; i--) {
			NextPlrLevel(player);
		}
	}

	NetSendCmdParam1(false, CMD_PLRLEVEL, player._pLevel);
}

void AddPlrMonstExper(int lvl, int exp, char pmask)
{
	int totplrs = 0;
	for (size_t i = 0; i < Players.size(); i++) {
		if (((1 << i) & pmask) != 0) {
			totplrs++;
		}
	}

	if (totplrs != 0) {
		int e = exp / totplrs;
		if ((pmask & (1 << MyPlayerId)) != 0)
			AddPlrExperience(*MyPlayer, lvl, e);
	}
}

void InitPlayer(Player &player, bool firstTime)
{
	if (firstTime) {
		// The Rage pool starts empty on a NEW CHARACTER only (user, 2026-09-16: "barb should carry his
		// rage over dungeon levels"). It used to be emptied on every level entry, which meant a
		// staircase taken mid-fight cost the whole pool; the calm clock in oracool/rage.h is what
		// drains it, and a level change is not a reason to stop being angry.
		oracool::ResetRage(player);
		oracool::ResetEssence(player); // the same moment, for the same reason: a new game, not a new level
		// NORMALISED, not cleared (user, 2026-09-02: "forgetting lmb skill isnt [fixed]. fix it.").
		//
		// This reset exists for one narrow reason, recorded when the left pair was added: value-
		// initialising a Player leaves these at SpellID::Null, and only SpellID::Invalid means "this
		// button swings" everywhere else. It was written as an unconditional wipe, which does that
		// job and also destroys a pair that was deliberately set.
		//
		// And by the time this runs, one has been. LoadGameLevel calls InitPlayer(firstflag) when
		// the game starts, which is AFTER pfile_read_player_from_save has decoded both readied slots
		// out of the hero file - so the correct answer was computed, stored, and then overwritten a
		// moment later. LoadHotkeys below could only ever put back what the GAME save holds, and V1
		// always starts a new game, so for the left button there was nothing to put back at all.
		//
		// A slot holding a valid spell is therefore left exactly as it is. Only Null - the value
		// nothing ever sets on purpose - is turned into Invalid.
		if (!IsValidSpell(player._pRSpell)) {
			player._pRSplType = SpellType::Invalid;
			player._pRSpell = SpellID::Invalid;
		}
		if (!IsValidSpell(player._pLRSpell)) {
			player._pLRSplType = SpellType::Invalid;
			player._pLRSpell = SpellID::Invalid;
		}
		if (&player == MyPlayer)
			LoadHotkeys();
		// A new game starts with no armour of ice on (Oracool, Round 2). The state is a static in
		// cold.cpp and would otherwise carry from the last character to this one.
		oracool::ClearColdArmour(player);
		// ...and no Venomous bite still bleeding (2026-09-19): the same static, the same carry-over.
		oracool::ClearPlayerVenom(player);
		// ...and no cry still ringing, no passive clock still running (Rounds 5 and 6): same statics,
		// same carry-over, same cure.
		oracool::ClearWarcryBuffs(player);
		oracool::ClearPassiveClocks(player);
		oracool::ClearRfa12ActiveBuffs(player);
		player._pSBkSpell = SpellID::Invalid;
		player.queuedSpell.spellId = player._pRSpell;
		player.queuedSpell.spellType = player._pRSplType;
		player.pManaShield = false;
		player.wReflections = 0;
	}

	if (player.isOnActiveLevel()) {

		SetPlrAnims(player);

		ClearStateVariables(player);

		if (player._pHitPoints >> 6 > 0) {
			player._pmode = PM_STAND;
			NewPlrAnim(player, player_graphic::Stand, Direction::South);
			player.AnimInfo.currentFrame = GenerateRnd(player._pNFrames - 1);
			player.AnimInfo.tickCounterOfCurrentFrame = GenerateRnd(3);
		} else {
			player._pgfxnum &= ~0xFU;
			player._pmode = PM_DEATH;
			NewPlrAnim(player, player_graphic::Death, Direction::South);
			player.AnimInfo.currentFrame = player.AnimInfo.numberOfFrames - 2;
		}

		player._pdir = Direction::South;

		if (&player == MyPlayer && (!firstTime || leveltype != DTYPE_TOWN)) {
			player.position.tile = ViewPosition;
		}

		SetPlayerOld(player);
		player.walkpath[0] = WALK_NONE;
		player.destAction = ACTION_NONE;

		if (&player == MyPlayer) {
			player.lightId = AddLight(player.position.tile, player._pLightRad);
			ChangeLightXY(player.lightId, player.position.tile); // fix for a bug where old light is still visible at the entrance after reentering level
		} else {
			player.lightId = NO_LIGHT;
		}
		ActivateVision(player.position.tile, player._pLightRad, player.getId());
	}

	// Oracool: rebuilt on every load, which is what lets a level gate open without a save migration -
	// and, since 2026-08-19, what takes the six retired vanilla class skills back off characters made
	// while they were still granted.
	player._pAblSpells = oracool::InnateSpellsBitmask(player);

	// A button still holding a retired skill has to be cleared, or it would point at something the
	// character no longer owns: the well would draw it and a click would try to cast it.
	const auto clearIfLost = [&player](SpellID &spell, SpellType &type) {
		if (type == SpellType::Skill && IsValidSpell(spell)
		    && (player._pAblSpells & GetSpellBitmask(spell)) == 0) {
			spell = SpellID::Invalid;
			type = SpellType::Invalid;
		}
	};
	clearIfLost(player._pRSpell, player._pRSplType);
	clearIfLost(player._pLRSpell, player._pLRSplType);

	player._pNextExper = ExpLvlsTbl[std::min<int8_t>(player._pLevel, MaxCharacterLevel - 1)];
	player._pInvincible = false;

	if (&player == MyPlayer) {
		MyPlayerIsDead = false;
	}
}

void InitMultiView()
{
	assert(MyPlayer != nullptr);
	ViewPosition = MyPlayer->position.tile;
}

void PlrClrTrans(Point position)
{
	for (int i = position.y - 1; i <= position.y + 1; i++) {
		for (int j = position.x - 1; j <= position.x + 1; j++) {
			TransList[dTransVal[j][i]] = false;
		}
	}
}

void PlrDoTrans(Point position)
{
	if (IsNoneOf(leveltype, DTYPE_CATHEDRAL, DTYPE_CATACOMBS, DTYPE_CRYPT)) {
		TransList[1] = true;
		return;
	}

	for (int i = position.y - 1; i <= position.y + 1; i++) {
		for (int j = position.x - 1; j <= position.x + 1; j++) {
			if (IsTileNotSolid({ j, i }) && dTransVal[j][i] != 0) {
				TransList[dTransVal[j][i]] = true;
			}
		}
	}
}

void SetPlayerOld(Player &player)
{
	player.position.old = player.position.tile;
}

void FixPlayerLocation(Player &player, Direction bDir)
{
	player.position.future = player.position.tile;
	player._pdir = bDir;
	if (&player == MyPlayer) {
		ViewPosition = player.position.tile;
	}
	ChangeLightXY(player.lightId, player.position.tile);
	ChangeVisionXY(player.getId(), player.position.tile);
}

void StartStand(Player &player, Direction dir)
{
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	// Standing is the end of whatever was happening, so a Zeal burst that has not finished does not
	// get to finish later. Every interruption reaches here or StartPlrHit - a hit reaction, a
	// broken weapon, a death, a level change - which is why the reset lives at these two rather
	// than being chased around each cause. See oracool::ResetZealChain.
	if (&player == MyPlayer)
		oracool::ResetZealChain();

	NewPlrAnim(player, player_graphic::Stand, dir);
	player._pmode = PM_STAND;
	FixPlayerLocation(player, dir);
	FixPlrWalkTags(player);
	dPlayer[player.position.tile.x][player.position.tile.y] = player.getId() + 1;
	SetPlayerOld(player);
}

void StartPlrBlock(Player &player, Direction dir)
{
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	PlaySfxLoc(IS_ISWORD, player.position.tile);
	// Insurmountable, Renewal, Counterstroke (2026-09-14): a block is an event.
	oracool::OnPassiveBlock(player);

	int8_t skippedAnimationFrames = 0;
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastBlock)) {
		skippedAnimationFrames = (player._pBFrames - 2); // ISPL_FASTBLOCK means we cancel the animation if frame 2 was shown
	}

	NewPlrAnim(player, player_graphic::Block, dir, AnimationDistributionFlags::SkipsDelayOfLastFrame, skippedAnimationFrames);

	player._pmode = PM_BLOCK;
	FixPlayerLocation(player, dir);
	SetPlayerOld(player);
}

/**
 * @todo Figure out why clearing player.position.old sometimes fails
 */
void FixPlrWalkTags(const Player &player)
{
	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			if (PlayerAtPosition({ x, y }) == &player)
				dPlayer[x][y] = 0;
		}
	}
}

void StartPlrHit(Player &player, int dam, bool forcehit)
{
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	player.Say(HeroSpeech::ArghClang);

	RedrawComponent(PanelDrawComponent::Health);
	if (player._pClass == HeroClass::Barbarian) {
		if (dam >> 6 < player._pLevel + player._pLevel / 4 && !forcehit) {
			return;
		}
	} else if (dam >> 6 < player._pLevel && !forcehit) {
		return;
	}

	Direction pd = player._pdir;

	// Anthem of Valor, and Grip of Iron with one enemy beside you (RfA-12): the blow lands, the flinch does not.
	if (!forcehit && oracool::PlayerHoldsAgainstHit(player))
		return;
	// Juggernaut (Barbarian, 2026-09-14): half the staggers do not take, and one that does may heal.
	if (!forcehit && oracool::PassiveShrugsOffStagger(player))
		return;

	int8_t skippedAnimationFrames = 0;
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastestHitRecovery)) {
		skippedAnimationFrames = 3;
	} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FasterHitRecovery)) {
		skippedAnimationFrames = 2;
	} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastHitRecovery)) {
		skippedAnimationFrames = 1;
	} else {
		skippedAnimationFrames = 0;
	}

	// The hit reaction interrupts the swing, so an unfinished Zeal burst ends here rather than
	// resuming inside whatever the player does next. Placed after the two early returns above: a
	// blow too small to stagger this character does not interrupt anything, so it must not disarm
	// the chain either. See oracool::ResetZealChain.
	if (&player == MyPlayer) {
		oracool::ResetZealChain();
		// Every swing latch with it: the swing they were armed for is over. A throw armed and then interrupted threw at the
		// next skill's hit frame, on another floor or in the next game (round 5 audit, v1.12.230).
		// Except a Charge still dashing: its approach resumes after the stagger and its paid-for blow lands then - the
		// latch cleared here made it a plain swing with the mana spent and the cooldown run (round 16 audit, v1.12.241).
		if (!oracool::IsFuriousChargeDashing())
			oracool::ArmMeleeSkill(std::nullopt);
		oracool::ArmArrowSkill(std::nullopt);
		oracool::ArmClassMeleeSkill(std::nullopt);
		oracool::ArmRfa12Melee(std::nullopt);
		oracool::ArmWeaponThrow(std::nullopt);
	}

	NewPlrAnim(player, player_graphic::Hit, pd, AnimationDistributionFlags::None, skippedAnimationFrames);

	player._pmode = PM_GOTHIT;
	FixPlayerLocation(player, pd);
	FixPlrWalkTags(player);
	dPlayer[player.position.tile.x][player.position.tile.y] = player.getId() + 1;
	SetPlayerOld(player);
}

static bool ShouldDropGoldOnDeath(const Player &player)
{
	// Oracool's Respawn In Town relies on the death menu offering a revive that
	// keeps everything the character was carrying. That guarantee only holds if
	// nothing is scattered on the ground the moment HP reaches zero, well before
	// the death menu (and its Respawn In Town choice) is ever shown.
	if (oracool::IsSinglePlayer())
		return false;

	return !gbIsMultiplayer || !(player.isOnLevel(16) || player.isOnArenaLevel());
}

#if defined(__clang__) || defined(__GNUC__)
__attribute__((no_sanitize("shift-base")))
#endif
void
StartPlayerKill(Player &player, DeathReason deathReason)
{
	if (player._pHitPoints <= 0 && player._pmode == PM_DEATH) {
		return;
	}

	if (&player == MyPlayer) {
		NetSendCmdParam1(true, CMD_PLRDEAD, static_cast<uint16_t>(deathReason));
	}

	const bool dropGold = ShouldDropGoldOnDeath(player);
	const bool dropItems = dropGold && deathReason == DeathReason::MonsterOrTrap;
	const bool dropEar = dropGold && deathReason == DeathReason::Player;

	player.Say(HeroSpeech::AuughUh);

	// Are the current animations item dependend?
	if (player._pgfxnum != 0) {
		if (dropItems) {
			// Ensure death animation show the player without weapon and armor, because they drop on death
			player._pgfxnum = 0;
		} else {
			// Death animation aren't weapon specific, so always use the unarmed animations
			player._pgfxnum &= ~0xFU;
		}
		ResetPlayerGFX(player);
		SetPlrAnims(player);
	}

	NewPlrAnim(player, player_graphic::Death, player._pdir);

	player._pBlockFlag = false;
	player._pmode = PM_DEATH;
	player._pInvincible = true;
	SetPlayerHitPoints(player, 0);
	// The army dies with its master, Diablo II's rule: it re-formed whole on the next floor after a Respawn (round 14
	// audit, v1.12.239). Final Service, which spends the army to save him, has already run by now.
	oracool::DismissMinions(player);

	if (&player != MyPlayer && dropItems) {
		// Ensure that items are removed for remote players
		// The dropped items will be synced seperatly (by the remote client)
		for (auto &item : player.InvBody) {
			item.clear();
		}
		CalcPlrInv(player, false);
	}

	if (player.isOnActiveLevel()) {
		FixPlayerLocation(player, player._pdir);
		FixPlrWalkTags(player);
		dFlags[player.position.tile.x][player.position.tile.y] |= DungeonFlag::DeadPlayer;
		SetPlayerOld(player);

		// Only generate drops once (for the local player)
		// For remote players we get seperated sync messages (by the remote client)
		if (&player == MyPlayer) {
			RedrawComponent(PanelDrawComponent::Health);

			if (!player.HoldItem.isEmpty()) {
				// Single-player death keeps everything carried, the cursor's item too (audit, 2026-09-29): dropped where he
				// fell, it was lost to a Main Menu exit, which writes no world. Put away as the exit save puts it away -
				// pack, belt for a potion, then stash - and dropped only when all three are full.
				bool kept = false;
				if (!gbIsMultiplayer) {
					kept = AutoPlaceItemInInventory(player, player.HoldItem, /*persistItem=*/true)
					    || (player.HoldItem.isPotion() && AutoPlaceItemInBelt(player, player.HoldItem, /*persistItem=*/true))
					    || AutoPlaceItemInStash(player, player.HoldItem, /*persistItem=*/true);
					if (kept)
						player.HoldItem.clear();
				}
				if (!kept)
					DeadItem(player, std::move(player.HoldItem), { 0, 0 });
				NewCursor(CURSOR_HAND);
			}
			if (dropGold) {
				DropHalfPlayersGold(player);
			}
			if (dropEar) {
				Item ear;
				InitializeItem(ear, IDI_EAR);
				CopyUtf8(ear._iName, fmt::format(fmt::runtime("Ear of {:s}"), player._pName), sizeof(ear._iName));
				CopyUtf8(ear._iIName, player._pName, sizeof(ear._iIName));
				switch (player._pClass) {
				case HeroClass::Sorcerer:
				case HeroClass::Necromancer:
					ear._iCurs = ICURS_EAR_SORCERER;
					break;
				case HeroClass::Warrior:
					ear._iCurs = ICURS_EAR_WARRIOR;
					break;
				case HeroClass::Rogue:
				case HeroClass::Monk:
				case HeroClass::Bard:
				case HeroClass::Barbarian:
					ear._iCurs = ICURS_EAR_ROGUE;
					break;
				}

				ear._iCreateInfo = player._pName[0] << 8 | player._pName[1];
				ear._iSeed = player._pName[2] << 24 | player._pName[3] << 16 | player._pName[4] << 8 | player._pName[5];
				ear._ivalue = player._pLevel;

				if (FindGetItem(ear._iSeed, IDI_EAR, ear._iCreateInfo) == -1) {
					DeadItem(player, std::move(ear), { 0, 0 });
				}
			}
			if (dropItems) {
				Direction pdd = player._pdir;
				for (auto &item : player.InvBody) {
					pdd = Left(pdd);
					DeadItem(player, item.pop(), Displacement(pdd));
				}

				CalcPlrInv(player, false);
			}
		}
	}
	SetPlayerHitPoints(player, 0);
}

void StripTopGold(Player &player)
{
	for (Item &item : InventoryPlayerItemsRange { player }) {
		if (item._itype == ItemType::Gold) {
			if (item._ivalue > MaxGold) {
				Item excessGold;
				MakeGoldStack(excessGold, item._ivalue - MaxGold);
				item._ivalue = MaxGold;

				if (!GoldAutoPlace(player, excessGold)) {
					DeadItem(player, std::move(excessGold), { 0, 0 });
				}
			}
		}
	}
	player._pGold = CalculateGold(player);
}

void ApplyPlrDamage(DamageType damageType, Player &player, int dam, int minHP /*= 0*/, int frac /*= 0*/, DeathReason deathReason /*= DeathReason::MonsterOrTrap*/)
{
	// A dead hero takes nothing more. The Drain Life tick kept landing through the death animation: another
	// "Slain by" in the log and another telemetry death every tick, and a cheat-death save coming off cooldown could
	// set life on a corpse (round 4 audit, v1.12.229).
	if (player._pmode == PM_DEATH)
		return;
	int totalDamage = (dam << 6) + frac;
	// Oracool, Round 5: the passives that soften a blow - Blur, Sixth Sense, Sword and Board and the
	// rest - answer here, before the number is shown, so what floats up is what was taken.
	if (totalDamage > 0) {
		const int passive = oracool::PassiveDamageTakenPercent(player, damageType);
		// The quarter-of-every-blow floor holds for the sum: Battle Hardened stacked on a capped -75 took it to -95
		// (round 4 audit). Rathma's Shield's -100 is the one deliberate exception.
		int percent = passive + oracool::Rfa12DamageTakenPercent(player, damageType);
		if (passive > -100)
			percent = std::max(percent, -75);
		percent = std::max(percent, -100); // never below nothing: a reduction past -100 would heal (round 28 audit)
		totalDamage += totalDamage * percent / 100;
	}
	// Galvanizing Ward's clock restarts on the blow itself, before any ward or shield takes it (round 14 audit).
	if (totalDamage > 0)
		oracool::OnPassiveStruck(player);
	// Chord of Warding (RfA-12) drinks its share before anything is shown or taken.
	if (totalDamage > 0)
		totalDamage = oracool::Rfa12AbsorbDamage(player, totalDamage);
	if (&player == MyPlayer && player._pHitPoints > 0) {
		AddFloatingNumber(damageType, player, totalDamage);
	}
	if (totalDamage > 0 && player.pManaShield) {
		// Effective level, for the same reason GetManaShieldDamageReduction uses it: a Monk who
		// bought Spirit Ward from the tree has nothing in _pSplLvl and would get no reduction.
		const int manaShieldLevel = player.GetSpellLevel(SpellID::ManaShield);
		if (manaShieldLevel > 0) {
			totalDamage += totalDamage / -player.GetManaShieldDamageReduction();
		}
		if (&player == MyPlayer)
			RedrawComponent(PanelDrawComponent::Mana);
		if (player._pMana >= totalDamage) {
			player._pMana -= totalDamage;
			player._pManaBase -= totalDamage;
			totalDamage = 0;
		} else {
			totalDamage -= player._pMana;
			if (manaShieldLevel > 0) {
				totalDamage += totalDamage / (player.GetManaShieldDamageReduction() - 1);
			}
			player._pMana = 0;
			player._pManaBase = player._pMaxManaBase - player._pMaxMana;
			if (&player == MyPlayer)
				NetSendCmd(true, CMD_REMSHIELD);
		}
	}

	if (totalDamage == 0) {
		if (&player == MyPlayer)
			oracool::ClearPendingDeathSource(); // nothing reached the hero (round 12 audit)
		return;
	}

	RedrawComponent(PanelDrawComponent::Health);
	player._pHitPoints -= totalDamage;
	player._pHPBase -= totalDamage;
	// Galvanizing Ward's clock and Illusionist's burst (2026-09-14) answer a blow actually taken.
	oracool::OnPassiveDamaged(player, totalDamage);
	if (player._pHitPoints > player._pMaxHP) {
		player._pHitPoints = player._pMaxHP;
		player._pHPBase = player._pMaxHPBase;
	}
	int minHitPoints = minHP << 6;
	if (player._pHitPoints < minHitPoints) {
		SetPlayerHitPoints(player, minHitPoints);
	}
	// And the once-a-minute saves (Round 5): a killing blow that a passive stands the character back
	// up from is not a death.
	// Mercy (RfA-12) answers a blow that leaves you low but standing.
	if (player._pHitPoints >> 6 > 0)
		oracool::OnRfa12PlayerDamaged(player);
	if (player._pHitPoints >> 6 <= 0 && !oracool::PassiveCheatsDeath(player)) {
		SyncPlrKill(player, deathReason);
		return;
	}
	// A blow the hero survived names no one: its source stayed pending, and a later death to a burning cross, a Drain
	// Life tick or a Blood Star's cost was logged as the last monster's, minutes and floors away (round 12 audit).
	if (&player == MyPlayer)
		oracool::ClearPendingDeathSource();
}

void SyncPlrKill(Player &player, DeathReason deathReason)
{
	// Once: every Start* sends a corpse here, and the log line and the telemetry death ran before StartPlayerKill's own
	// once-only guard (round 28 audit; no caller reaches it today).
	if (player._pmode == PM_DEATH)
		return;
	if (player._pHitPoints <= 0 && leveltype == DTYPE_TOWN) {
		SetPlayerHitPoints(player, 64);
		return;
	}

	SetPlayerHitPoints(player, 0);
	std::string fallbackDeathReason;
	switch (deathReason) {
	case DeathReason::Player:
		fallbackDeathReason = "another player";
		break;
	case DeathReason::Unknown:
		fallbackDeathReason = "unknown causes";
		break;
	case DeathReason::MonsterOrTrap:
	default:
		fallbackDeathReason = "a monster or trap";
		break;
	}
	oracool::LogPlayerDeath(fallbackDeathReason);
	StartPlayerKill(player, deathReason);
}

void RemovePlrMissiles(const Player &player)
{
	if (leveltype != DTYPE_TOWN && &player == MyPlayer) {
		Monster &golem = Monsters[MyPlayerId];
		if (golem.position.tile.x != 1 || golem.position.tile.y != 0) {
			KillMyGolem();
			AddCorpse(golem.position.tile, golem.type().corpseId, golem.direction);
			int mx = golem.position.tile.x;
			int my = golem.position.tile.y;
			dMonster[mx][my] = 0;
			golem.isInvalid = true;
			DeleteMonsterList();
		}
	}

	for (auto &missile : Missiles) {
		if (missile._mitype == MissileID::StoneCurse && &Players[missile._misource] == &player) {
			Monsters[missile.var2].mode = static_cast<MonsterMode>(missile.var1);
		}
	}
}

#if defined(__clang__) || defined(__GNUC__)
__attribute__((no_sanitize("shift-base")))
#endif
void
StartNewLvl(Player &player, interface_mode fom, int lvl)
{
	InitLevelChange(player);

	switch (fom) {
	case WM_DIABNEXTLVL:
	case WM_DIABPREVLVL:
	case WM_DIABRTNLVL:
	case WM_DIABTOWNWARP:
		player.setLevel(lvl);
		break;
	case WM_DIABSETLVL:
		if (&player == MyPlayer)
			setlvlnum = (_setlevels)lvl;
		player.setLevel(setlvlnum);
		break;
	case WM_DIABTWARPUP:
		MyPlayer->pTownWarps |= 1 << (leveltype - 2);
		player.setLevel(lvl);
		break;
	case WM_DIABRETOWN:
		break;
	default:
		app_fatal("StartNewLvl");
	}

	if (&player == MyPlayer) {
		player._pmode = PM_NEWLVL;
		player._pInvincible = true;
		SDL_Event event;
		event.type = CustomEventToSdlEvent(fom);
		SDL_PushEvent(&event);
		if (gbIsMultiplayer) {
			NetSendCmdParam2(true, CMD_NEWLVL, fom, lvl);
		}
	}
}

void RestartTownLvl(Player &player)
{
	InitLevelChange(player);

	player.setLevel(0);
	player._pInvincible = false;

	SetPlayerHitPoints(player, 64);

	player._pMana = 0;
	player._pManaBase = player._pMana - (player._pMaxMana - player._pMaxManaBase);
	// The rest of a potion drunk just before death does not heal the hero standing in town (round 21 audit).
	if (&player == MyPlayer)
		oracool::ResetGradualHealing();
	// Nor his Rage: it froze at its value on the corpse (v1.12.246's death guard), and the respawn carried the whole pool
	// into town, where it only drains (round 22 audit of v1.12.246).
	oracool::ResetRage(player);
	// Nor his Essence, for the same reason: the refill froze on the corpse (round 28 audit).
	oracool::ResetEssence(player);
	// Nor a cold armour: it rode the respawn into town with its tint and its freeze-on-hit, where vanilla ended it
	// (round 24 audit, v1.12.249).
	oracool::ClearColdArmour(player);

	// Out of PM_DEATH before the totals: a dead hero's aura counts for nothing (GetActiveClassAura), so the lit aura's
	// life and resistances were left out - a life aura took the 1 life below zero and the hero arrived dead in town,
	// and its bonuses stayed missing until the next re-equip (round 12 audit, v1.12.237).
	player._pmode = PM_NEWLVL;
	// Nor the warcries' buffs: Battle Orders' life rode the respawn into town on a running clock (round 28 audit). Cleared
	// out of PM_DEATH, and the totals keep the 1 life: recalculated while dead, the lit aura's life was left out, the life
	// pinned, and then added back on top - a free heal (round 29 audit).
	oracool::ClearWarcryBuffs(player);
	CalcPlrInvKeepingLife(player);

	if (&player == MyPlayer) {
		player._pInvincible = true;
		SDL_Event event;
		event.type = CustomEventToSdlEvent(WM_DIABRETOWN);
		SDL_PushEvent(&event);
	}
}

void StartWarpLvl(Player &player, size_t pidx)
{
	InitLevelChange(player);

	if (gbIsMultiplayer) {
		if (!player.isOnLevel(0)) {
			player.setLevel(0);
		} else {
			if (Portals[pidx].setlvl)
				player.setLevel(static_cast<_setlevels>(Portals[pidx].level));
			else
				player.setLevel(Portals[pidx].level);
		}
	}

	if (&player == MyPlayer) {
		SetCurrentPortal(pidx);
		player._pmode = PM_NEWLVL;
		player._pInvincible = true;
		SDL_Event event;
		event.type = CustomEventToSdlEvent(WM_DIABWARPLVL);
		SDL_PushEvent(&event);
	}
}

void ProcessPlayers()
{
	assert(MyPlayer != nullptr);
	Player &myPlayer = *MyPlayer;

	if (myPlayer.pLvlLoad > 0) {
		myPlayer.pLvlLoad--;
	}

	if (sfxdelay > 0) {
		sfxdelay--;
		if (sfxdelay == 0) {
			switch (sfxdnum) {
			case USFX_DEFILER1:
				InitQTextMsg(TEXT_DEFILER1);
				break;
			case USFX_DEFILER2:
				InitQTextMsg(TEXT_DEFILER2);
				break;
			case USFX_DEFILER3:
				InitQTextMsg(TEXT_DEFILER3);
				break;
			case USFX_DEFILER4:
				InitQTextMsg(TEXT_DEFILER4);
				break;
			default:
				PlaySFX(sfxdnum);
			}
		}
	}

	ValidatePlayer();

	for (size_t pnum = 0; pnum < Players.size(); pnum++) {
		Player &player = Players[pnum];
		if (player.plractive && player.isOnActiveLevel() && (&player == MyPlayer || !player._pLvlChanging)) {
			CheckCheatStats(player);

			// The once-a-minute saves and Final Service are offered here too, as ApplyPlrDamage offers them: life reaching
			// zero by a recalculation (gear taken off at low life) killed with no save, and now takes the army with it
			// (round 15 audit, v1.12.240).
			// Not in town, where SyncPlrKill only sets life back to 1: there Final Service dismissed the army and the saves
			// spent their cooldown on a death that could not happen (round 16 audit, a regression of round 15).
			if (!PlrDeathModeOK(player) && (player._pHitPoints >> 6) <= 0
			    && (leveltype == DTYPE_TOWN || !oracool::PassiveCheatsDeath(player))) {
				SyncPlrKill(player, DeathReason::Unknown);
			}

			if (&player == MyPlayer) {
				if (HasAnyOf(player._pIFlags, ItemSpecialEffect::DrainLife) && leveltype != DTYPE_TOWN) {
					ApplyPlrDamage(DamageType::Physical, player, 0, 0, 4);
				}
				if (HasAnyOf(player._pIFlags, ItemSpecialEffect::NoMana) && player._pManaBase > 0) {
					player._pManaBase -= player._pMana;
					player._pMana = 0;
					RedrawComponent(PanelDrawComponent::Mana);
				}
				oracool::ProcessGradualHealing(player);
				// ...and the Venomous variant's bleed (2026-09-19), the same per-tick seam - not in
				// town, as the life drain two lines up is not (audit, 2026-09-19).
				// In town the bleed is washed out rather than paused: it froze mid-count and drained again on the next
				// trip down, minutes later, with nothing on screen to say why (round 11 audit, v1.12.236).
				if (leveltype != DTYPE_TOWN)
					oracool::TickPlayerVenom(player);
				else
					oracool::ClearPlayerVenom(player);
				// The Paladin's Prayer and Meditation auras regenerate here, beside the engine's
				// own per-tick life and mana effects.
				oracool::ProcessClassTreeTick(player);
			}

			// Chilled (2026-09-26, the Diablo II chill): every other tick of an attack, a cast, a block or a hit
			// recovery is the cold's. The walk is slowed by the movement slow instead - see oracool::ChillPlayer.
			if (oracool::PlayerChillTakesThisTick(player))
				continue;

			bool tplayer = false;
			do {
				switch (player._pmode) {
				case PM_STAND:
				case PM_NEWLVL:
				case PM_QUIT:
					tplayer = false;
					break;
				case PM_WALK_NORTHWARDS:
				case PM_WALK_SOUTHWARDS:
				case PM_WALK_SIDEWAYS:
					tplayer = DoWalk(player, player._pmode);
					break;
				case PM_ATTACK:
					tplayer = DoAttack(player);
					break;
				case PM_RATTACK:
					tplayer = DoRangeAttack(player);
					break;
				case PM_BLOCK:
					tplayer = DoBlock(player);
					break;
				case PM_SPELL:
					tplayer = DoSpell(player);
					break;
				case PM_GOTHIT:
					tplayer = DoGotHit(player);
					break;
				case PM_DEATH:
					tplayer = DoDeath(player);
					break;
				}
				CheckNewPath(player, tplayer);
			} while (tplayer);

			player.previewCelSprite = std::nullopt;
			if (player._pmode != PM_DEATH || player.AnimInfo.tickCounterOfCurrentFrame != 40)
				player.AnimInfo.processAnimation();
		}
	}
}

void ClrPlrPath(Player &player)
{
	memset(player.walkpath, WALK_NONE, sizeof(player.walkpath));
}

/**
 * @brief Determines if the target position is clear for the given player to stand on.
 *
 * This requires an ID instead of a Player& to compare with the dPlayer lookup table values.
 *
 * @param player The player to check.
 * @param position Dungeon tile coordinates.
 * @return False if something (other than the player themselves) is blocking the tile.
 */
bool PosOkPlayer(const Player &player, Point position)
{
	if (!InDungeonBounds(position))
		return false;
	if (!IsTileWalkable(position))
		return false;
	if (dPlayer[position.x][position.y] != 0) {
		auto &otherPlayer = Players[abs(dPlayer[position.x][position.y]) - 1];
		if (&otherPlayer != &player && otherPlayer._pHitPoints != 0) {
			return false;
		}
	}

	if (dMonster[position.x][position.y] != 0) {
		if (leveltype == DTYPE_TOWN) {
			return false;
		}
		if (dMonster[position.x][position.y] <= 0) {
			return false;
		}
		const Monster &standing = Monsters[dMonster[position.x][position.y] - 1];
		// Your own companion steps aside: you walk through it, and it takes the tile you left (oracool/companion.h).
		if (oracool::CompanionMakesWay(player, standing))
			return true;
		if ((standing.hitPoints >> 6) > 0) {
			return false;
		}
	}

	return true;
}

void MakePlrPath(Player &player, Point targetPosition, bool endspace)
{
	if (player.position.future == targetPosition) {
		return;
	}

	int path = FindPath([&player](Point position) { return PosOkPlayer(player, position); }, player.position.future, targetPosition, player.walkpath);
	if (path == 0) {
		return;
	}

	if (!endspace) {
		path--;
	}

	player.walkpath[path] = WALK_NONE;
}

void CalcPlrStaff(Player &player)
{
	player._pISpells = 0;
	if (!player.InvBody[INVLOC_HAND_LEFT].isEmpty()
	    && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag
	    && player.InvBody[INVLOC_HAND_LEFT]._iCharges > 0) {
		player._pISpells |= GetSpellBitmask(player.InvBody[INVLOC_HAND_LEFT]._iSpell);
	}
}

/** @brief What the hero says when he cannot pay for @p spell: "not enough mana" only when mana is the price (round 28 audit:
 * a Barbarian short of Rage and a Necromancer short of Essence said it too). */
HeroSpeech ShortOfPriceSpeech(const Player &player, SpellID spell)
{
	return oracool::UsesRage(player) || (oracool::UsesEssence(player) && oracool::EssenceCost(spell) > 0) ? HeroSpeech::ICantDoThat : HeroSpeech::NotEnoughMana;
}

void CheckPlrSpell(bool isShiftHeld, SpellID spellID, SpellType spellType)
{
	bool addflag = false;

	assert(MyPlayer != nullptr);
	Player &myPlayer = *MyPlayer;

	if (!IsValidSpell(spellID)) {
		myPlayer.Say(HeroSpeech::IDontHaveASpellReady);
		return;
	}

	if (ControlMode == ControlTypes::KeyboardAndMouse) {
		if (pcurs != CURSOR_HAND)
			return;

		// Oracool bug fix (2026-08-15): user report - "skills dont seem to work". This line was why.
		// It used to test GetMainPanel(), the vanilla 640x128 rect at the screen bottom, which the
		// HUD overhaul deliberately left in place because the flyout panels centre against it - but
		// which has not been solid UI since. A right-click on a monster anywhere in that band
		// returned here having done nothing: no cast, no walk, no message. With the button empty the
		// same click went to LeftMouseCmd instead and worked, so it read as "the skills are broken"
		// rather than "the bottom third of the screen eats clicks".
		//
		// oracool::IsPointOverHudChrome is the same test LeftMouseDown routes on, so the two can no
		// longer disagree about where the UI is.
		if (oracool::IsPointOverHudChrome(MousePosition))
			return;

		if (
		    (IsLeftPanelOpen() && GetLeftPanel().contains(MousePosition)) // inside left panel
		    || IsOverRightPanel(MousePosition)                           // inside right panel
		) {
			if (spellID != SpellID::Healing
			    && spellID != SpellID::Identify
			    && spellID != SpellID::ItemRepair
			    && spellID != SpellID::Infravision
			    && spellID != SpellID::StaffRecharge)
				return;
		}
	}

	// TALKING WINS OVER EVERY SKILL (user, 2026-09-13: "sweep code behind all attacking skills and make
	// sure they let me engage quest npcs into conversation mode, because now they dont"). Every readied
	// ability reaches the world through this function - both mouse buttons, the quick-cast keys, the
	// controller and the held-button repeat - and every branch below it (the bow, RfA-12 and class melee
	// latches, the Paladin skills, the leaps, ordinary spells) sends an attack, a cast, a leap or a walk.
	// None asked whether the monster under the cursor is Lachdanan, Zhar, Gharbad, Snotspill or Lazarus
	// waiting to speak, so a skill on the button made those conversations unreachable. LeftMouseCmd and
	// RightMouseBasicAttack already route a talker to CMD_ATTACKID, which walks up and opens the dialogue
	// (ACTION_ATTACKMON's talk branch); this is the same rule, once, for everything readied.
	//
	// Shift does not override it, as in vanilla's shift-click. The latches are disarmed first so the next
	// real swing does not inherit a skill this click never used.
	if (pcursmonst != -1) {
		const bool townsperson = leveltype == DTYPE_TOWN;
		if (townsperson || CanTalkToMonst(Monsters[pcursmonst])) {
			oracool::ArmMeleeSkill(std::nullopt);
			oracool::ArmArrowSkill(std::nullopt);
			oracool::ArmClassMeleeSkill(std::nullopt);
			oracool::ArmRfa12Melee(std::nullopt);
			oracool::ArmWeaponThrow(std::nullopt);
			LastMouseButtonAction = MouseActionType::None;
			if (townsperson)
				// The townsperson's own tile: a pad leaves cursPosition at (-1,-1), and the talk was dropped (round 8 audit).
				NetSendCmdLocParam1(true, CMD_TALKXY, Towners[pcursmonst].position, pcursmonst);
			else
				NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
			return;
		}
	}

	if (leveltype == DTYPE_TOWN && !GetSpellData(spellID).isAllowedInTown()) {
		myPlayer.Say(HeroSpeech::ICantCastThatHere);
		return;
	}

	SpellCheckResult spellcheck = SpellCheckResult::Success;
	switch (spellType) {
	case SpellType::Skill:
	case SpellType::Spell:
		spellcheck = CheckSpell(*MyPlayer, spellID, spellType, false);
		addflag = spellcheck == SpellCheckResult::Success;
		break;
	case SpellType::Scroll:
		addflag = pcurs == CURSOR_HAND && CanUseScroll(myPlayer, spellID);
		break;
	case SpellType::Charges:
		addflag = pcurs == CURSOR_HAND && CanUseStaff(myPlayer, spellID);
		break;
	case SpellType::Invalid:
		return;
	}

	if (!addflag) {
		// Skills too: a tree skill short of mana, Rage or Essence failed without a word (round 6 audit, v1.12.231).
		if (spellType == SpellType::Spell || spellType == SpellType::Skill) {
			switch (spellcheck) {
			case SpellCheckResult::Fail_NoMana:
				myPlayer.Say(ShortOfPriceSpeech(myPlayer, spellID));
				break;
			case SpellCheckResult::Fail_Level0:
				myPlayer.Say(HeroSpeech::ICantCastThatYet);
				break;
			default:
				myPlayer.Say(HeroSpeech::ICantDoThat);
				break;
			}
			LastMouseButtonAction = MouseActionType::None;
		}
		return;
	}

	// Oracool: with Furious Charge active, the Warrior's class-ability slot (SpellID::ItemRepair)
	// stops casting Item Repair and instead dashes at the targeted monster - this fully replaces
	// the normal spell dispatch below for that slot, never falling back to Repair's cursor-switch
	// behavior. Off cooldown, the dash gets the speed boost (see StartWalkAnimation); on cooldown,
	// it still attacks normally, just without the speed boost - the ability never "does nothing."
	// Oracool: standing rule (2026-08-15) - "melee skills only initiate when clicked on monsters
	// within range, else - move command", and the same for ranged, on both mouse buttons.
	//
	// This one block now covers every Paladin skill including Charge, which used to have its own
	// copy and its own bug: with no monster under the cursor it fell out of the `if` and RETURNED,
	// so a click on open ground did nothing at all - the ground was unclickable while a skill sat on
	// your button. Walking there is both the rule and the better reading of the promise Charge's own
	// comment made, that the ability never "does nothing".
	//
	// Reaching CastSpell would be wrong for the melee four and Charge for a second reason: none of them
	// is cast through the missile system, so it would find MissileID::Null in both slots, spawn nothing,
	// and then call ConsumeSpell - charging mana for no effect. The three CAST skills do reach it since
	// 2026-09-11, and CastSpell hands them to oracool/paladin_ranged before the missile table is read.
	// Oracool, Round 3: a BOW skill is shot, not cast. The click becomes the ordinary ranged attack
	// - bow animation, arrow sound, weapon wear - with the latch in oracool/rogue_arrows.h saying
	// which skill threw it, and DoRangeAttack looses the skill's arrow instead of a plain one. The
	// same shape as the Paladin branch below, for the same reason: by the time the animation fires,
	// which button acted is gone.
	//
	// Needs a bow in hand - a skill that promises an arrow cannot be swung - and the mana up front,
	// so the refusal is heard at the click rather than discovered at the release. Shift keeps its
	// meaning: shoot at the cursor's tile, monster or not.
	if (const std::optional<oracool::RogueArrow> arrow = oracool::RogueArrowForSpell(spellID); arrow.has_value()) {
		if (!myPlayer.UsesRangedWeapon()) {
			myPlayer.Say(HeroSpeech::ICantDoThat);
			LastMouseButtonAction = MouseActionType::None; // said once, not at tick rate under a held button (round 19)
			return;
		}
		if (CheckSpell(myPlayer, spellID, SpellType::Skill, /*manaonly=*/true) != SpellCheckResult::Success) {
			myPlayer.Say(ShortOfPriceSpeech(myPlayer, spellID));
			return;
		}
		oracool::ArmArrowSkill(*arrow);
		if (pcursmonst != -1 && !isShiftHeld) {
			LastMouseButtonAction = MouseActionType::AttackMonsterTarget;
			NetSendCmdParam1(true, CMD_RATTACKID, pcursmonst);
		} else {
			LastMouseButtonAction = MouseActionType::Attack;
			NetSendCmdLoc(MyPlayerId, true, CMD_RATTACKXY, cursPosition);
		}
		return;
	}

	// The eight RfA-12 bow skills are cast, not shot, and refused only at the cast frame: the whole animation played with
	// no bow, then "I can't do that", every cycle of a held button. Refused at the click, as the arrows above (round 18
	// audit, v1.12.243).
	if (oracool::Rfa12LacksBowFor(myPlayer, spellID)) {
		myPlayer.Say(HeroSpeech::ICantDoThat);
		LastMouseButtonAction = MouseActionType::None; // said once, not at tick rate under a held button (round 19)
		return;
	}

	// Weapon Throw (Barbarian, user note 2026-09-14): the ordinary attack, in place, toward the cursor - and at the hit
	// frame DoAttack throws the weapon instead of swinging it (oracool/weapon_throw.h).
	if (oracool::IsWeaponThrow(spellID)) {
		if (!oracool::CanThrowWeapon(myPlayer)) {
			myPlayer.Say(HeroSpeech::ICantDoThat);
			LastMouseButtonAction = MouseActionType::None; // said once, not at the hold's repeat rate (round 20 audit)
			return;
		}
		if (CheckSpell(myPlayer, spellID, SpellType::Skill, /*manaonly=*/true) != SpellCheckResult::Success) {
			myPlayer.Say(ShortOfPriceSpeech(myPlayer, spellID));
			LastMouseButtonAction = MouseActionType::None; // said once, as the throw refusal above (round 28 audit)
			return;
		}
		oracool::ArmClassMeleeSkill(std::nullopt);
		oracool::ArmRfa12Melee(std::nullopt);
		oracool::ArmMeleeSkill(std::nullopt);
		oracool::ArmArrowSkill(std::nullopt);
		oracool::ArmWeaponThrow(cursPosition);
		// The hold is a cast, so each repeat comes back here and re-arms: as an Attack the throw spent its latch and the
		// held button swung at the air in place (round 19 audit, v1.12.244).
		LastMouseButtonSpell = spellID;
		LastMouseButtonSpellType = spellType;
		LastMouseButtonAction = MouseActionType::Spell;
		NetSendCmdLoc(MyPlayerId, true, CMD_SATTACKXY, cursPosition);
		return;
	}

	// Oracool, RfA-12 (2026-09-13): the new melee skills are swung on their own latch, the Round 4 way below.
	if (oracool::IsRfa12Melee(spellID)) {
		if (CheckSpell(myPlayer, spellID, SpellType::Skill, /*manaonly=*/true) != SpellCheckResult::Success) {
			myPlayer.Say(ShortOfPriceSpeech(myPlayer, spellID));
			return;
		}
		if (!oracool::Rfa12MeleeUsable(myPlayer, spellID)) {
			myPlayer.Say(HeroSpeech::ICantDoThat);
			return;
		}
		oracool::ArmClassMeleeSkill(std::nullopt);
		// One latch at a time: the Paladin's rode an RfA-12 swing (Zeal's speed, to-hit and chain on a Judgment) and the
		// bow skill's rode the held repeat (round 19 audit, v1.12.244).
		oracool::ArmMeleeSkill(std::nullopt);
		oracool::ArmArrowSkill(std::nullopt);
		if (isShiftHeld) {
			oracool::ArmRfa12Melee(spellID);
			LastMouseButtonAction = MouseActionType::Attack;
			NetSendCmdLoc(MyPlayerId, true, CMD_SATTACKXY, cursPosition);
			return;
		}
		if (pcursmonst == -1) {
			LastMouseButtonAction = MouseActionType::Walk;
			NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
			return;
		}
		oracool::ArmRfa12Melee(spellID);
		LastMouseButtonAction = MouseActionType::AttackMonsterTarget;
		NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
		return;
	}

	// Oracool, Round 4: a Barbarian's or Monk's MELEE skill is swung, not cast - the same latch
	// shape as the Paladin branch below and the bow branch above. Adjacent target: a swing with the
	// skill armed. Distant target and a leaping skill: the leap. Distant target otherwise: walk, as
	// the Paladin's skills do. Shift: swing in place, armed.
	if (const std::optional<oracool::ClassMeleeSkill> skill = oracool::ClassMeleeSkillForSpell(spellID); skill.has_value()) {
		if (CheckSpell(myPlayer, spellID, SpellType::Skill, /*manaonly=*/true) != SpellCheckResult::Success) {
			myPlayer.Say(ShortOfPriceSpeech(myPlayer, spellID));
			return;
		}
		// A bow is not swung (round 19 audit, v1.12.244): the hero drew it and struck in melee with the skill's bonus.
		if (oracool::LacksMeleeWeaponFor(myPlayer, spellID)) {
			myPlayer.Say(HeroSpeech::ICantDoThat);
			LastMouseButtonAction = MouseActionType::None;
			return;
		}
		oracool::ArmMeleeSkill(std::nullopt);
		oracool::ArmArrowSkill(std::nullopt);
		// Whirlwind is held on the right button, not swung (2026-09-29): the spin runs from here until the button is let
		// go or the Rage runs out - oracool/whirlwind.h. Never from the left button.
		if (*skill == oracool::ClassMeleeSkill::Whirlwind) {
			oracool::ArmClassMeleeSkill(std::nullopt);
			LastMouseButtonAction = MouseActionType::None;
			if (sgbMouseDown != CLICK_RIGHT || !oracool::StartWhirlwind(myPlayer))
				myPlayer.Say(HeroSpeech::ICantDoThat);
			return;
		}
		const bool adjacent = pcursmonst != -1
		    && myPlayer.position.tile.WalkingDistance(Monsters[pcursmonst].position.tile) <= 1;

		// Plain Leap is a leap, always - before the shift and adjacency tests: armed as a swing it struck in place and took
		// 10 Rage, with the leap's cues (round 7 audit, v1.12.232).
		const bool plainLeap = *skill == oracool::ClassMeleeSkill::Leap;
		if (isShiftHeld && !plainLeap) {
			oracool::ArmClassMeleeSkill(*skill);
			LastMouseButtonAction = MouseActionType::Attack;
			NetSendCmdLoc(MyPlayerId, true, CMD_SATTACKXY, cursPosition);
			return;
		}
		if (plainLeap || (oracool::IsLeapSkill(*skill) && !adjacent)) {
			// Plain Leap always leaps; the two striking leaps leap when the target is out of reach.
			oracool::ArmClassMeleeSkill(std::nullopt);
			if (oracool::LeapToward(myPlayer, *skill, cursPosition)) {
				LastMouseButtonAction = MouseActionType::None;
				return;
			}
			myPlayer.Say(HeroSpeech::ICantDoThat);
			return;
		}
		if (pcursmonst == -1) {
			oracool::ArmClassMeleeSkill(std::nullopt);
			LastMouseButtonAction = MouseActionType::Walk;
			NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
			return;
		}
		oracool::ArmClassMeleeSkill(*skill);
		LastMouseButtonAction = MouseActionType::AttackMonsterTarget;
		NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
		return;
	}

	if (const std::optional<oracool::PaladinSkill> skill = oracool::PaladinSkillForSpell(spellID); skill.has_value()) {
		// Smite and Blessed Shield stay on the button without a shield (2026-09-27): they refuse here,
		// out loud, rather than falling through to a plain swing that reads as the skill misfiring.
		if (oracool::GetPaladinSkillData(*skill).requiresShield && !oracool::HasShieldEquipped(myPlayer)) {
			myPlayer.Say(HeroSpeech::ICantDoThat);
			return;
		}
		if (oracool::LacksMeleeWeaponFor(myPlayer, spellID)) { // a bow is not swung (round 19 audit)
			myPlayer.Say(HeroSpeech::ICantDoThat);
			LastMouseButtonAction = MouseActionType::None;
			return;
		}
		// One latch at a time: an RfA-12 or class swing latch rode the Paladin's swing, and the other way round - both
		// skills fired and both were paid (round 19 audit, v1.12.244). ArmClassMeleeSkill drops the RfA-12 and throw.
		oracool::ArmClassMeleeSkill(std::nullopt);
		oracool::ArmArrowSkill(std::nullopt);
		// Oracool: user correction (2026-08-15) - "LMB/RMB Clicks + Shift - as designed by Blizzard -
		// to always cast spell/skill, no matter what as long as we are not breaking other hard
		// disablers". Shift used to force a plain attack here and disarm the skill, on the reading
		// that it means "ignore what is readied". It means the opposite: act, now, without walking
		// anywhere first.
		//
		// The hard disablers still hold: the level gate, the mana price, and a missing shield. The
		// RANGE test is not one of them when shift is down - that is what shift overrides.
		if (isShiftHeld) {
			// Oracool bug fix (2026-08-15): user report - "shift click didnt produce blessed hamer.
			// shift left click still moved my hero." The previous version treated out-of-range-with-
			// shift as the one case that does NOTHING, reasoning that walking would disobey shift.
			// That reasoning stopped one line short: the answer is not to walk OR to give up, it is
			// to act where the cursor is - which is what shift means everywhere else in this game.
			//
			// So a ranged skill fires at the cursor's own tile, monster or not, and a melee skill
			// swings in place toward it (CMD_SATTACKXY, the same command vanilla shift-click uses)
			// with the latch armed, so Zeal, Hammer of Faith and Shield Bash still ride the swing if
			// it connects with anything.
			if (oracool::CanStartRangedPaladinSkill(myPlayer, *skill)) {
				// A real cast at the cursor's tile (2026-09-11): the spell animation, at cast speed, and
				// the skill at its cast frame - see the in-range case below.
				oracool::ArmMeleeSkill(std::nullopt);
				LastMouseButtonSpell = spellID;
				LastMouseButtonSpellType = spellType;
				LastMouseButtonAction = MouseActionType::Spell;
				NetSendCmdLocParam3(true, CMD_SPELLXY, cursPosition, static_cast<int16_t>(spellID), static_cast<uint8_t>(spellType), 0);
				return;
			}
			// No dash for Charge here, deliberately (self-audit, 2026-08-15): the dash is a
			// walk-speed boost, and CMD_SATTACKXY below is a swing IN PLACE - there is no walk for it
			// to boost. The first version started the dash and spent the 10 mana anyway, which bought
			// a stationary swing at full price. Shift means "act without moving"; for a skill whose
			// whole identity is movement, that leaves the swing, so the swing is what shift gets.
			oracool::ArmMeleeSkill(*skill);
			LastMouseButtonAction = MouseActionType::Attack;
			NetSendCmdLoc(MyPlayerId, true, CMD_SATTACKXY, cursPosition);
			return;
		}

		// A CAST skill (spell animation: Blessed Shield, Blessed Hammer, Fist of the Heavens) casts like
		// a spell (dev note, 2026-09-27: "they should not require a target mob under the cursor to cast"):
		// on open ground it goes to the cursor's tile, and on a monster past the reach it goes to the
		// monster, instead of walking there first. A refusal on open ground says why and stays put.
		if (oracool::IsCastPaladinSkill(*skill) && (pcursmonst == -1 || !oracool::IsPaladinSkillTargetInRange(myPlayer, *skill))) {
			oracool::ArmMeleeSkill(std::nullopt);
			if (oracool::CanStartRangedPaladinSkill(myPlayer, *skill)) {
				LastMouseButtonSpell = spellID;
				LastMouseButtonSpellType = spellType;
				if (pcursmonst != -1) {
					LastMouseButtonAction = MouseActionType::SpellMonsterTarget;
					NetSendCmdParam4(true, CMD_SPELLID, pcursmonst, static_cast<int16_t>(spellID), static_cast<uint8_t>(spellType), 0);
				} else {
					LastMouseButtonAction = MouseActionType::Spell;
					NetSendCmdLocParam3(true, CMD_SPELLXY, cursPosition, static_cast<int16_t>(spellID), static_cast<uint8_t>(spellType), 0);
				}
				return;
			}
			if (pcursmonst == -1) {
				myPlayer.Say(myPlayer._pMana < (oracool::GetPaladinSkillData(*skill).manaCost << 6) ? HeroSpeech::NotEnoughMana : HeroSpeech::ICantDoThat);
				return;
			}
		}

		if (!oracool::IsPaladinSkillTargetInRange(myPlayer, *skill)) {
			// REACH AND HIT (user, 2026-09-07: "i hold left click over a monster and many times my
			// hero reaches the mob but doesn't start attacking"). This branch sent a plain walk to the
			// monster's TILE and recorded the hold as a Walk, so the held button repeated a walk: the
			// hero arrived beside the monster, RepeatWalk saw the walk's target already reached, and
			// nothing ever asked the skill again. The "else - move command" rule (2026-08-15) meant
			// the hero should go there, not that the click should forget what it was for.
			//
			// A MELEE skill on a monster takes the engine's own walk-then-swing (CMD_ATTACKID, the
			// path re-laid to the monster every tick, the swing at arrival) with the skill armed -
			// exactly what the Barbarian's and Monk's melee skills already do. A RANGED skill still
			// walks, but the hold is recorded as a cast on this monster, so a held button asks again
			// every step and the skill fires the moment the range is met.
			if (pcursmonst != -1 && oracool::GetPaladinSkillData(*skill).rangeTiles == oracool::MeleeSkillRangeTiles) {
				oracool::ArmMeleeSkill(*skill);
				LastMouseButtonAction = MouseActionType::AttackMonsterTarget;
				NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
				return;
			}
			// Disarmed: nothing is being swung, and leaving the latch set would let the NEXT swing -
			// however it was thrown - inherit this skill.
			oracool::ArmMeleeSkill(std::nullopt);
			if (pcursmonst != -1) {
				LastMouseButtonSpell = spellID;
				LastMouseButtonSpellType = spellType;
				LastMouseButtonAction = MouseActionType::SpellMonsterTarget;
			} else {
				LastMouseButtonAction = MouseActionType::Walk;
			}
			NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
			return;
		}

		// The skills that strike at a distance get their chance first, and they are CAST (user,
		// 2026-09-11: "Blessed Hammer needs to act as spell in a sense that it should be cast with the
		// cast speed of paladin"). The click queues a real spell on the monster - the same CMD_SPELLID
		// every spell sends - so the hero plays the spell animation, Faster Cast Rate shortens it, and
		// CastSpell hands the skill to CastRangedPaladinSkill at the cast frame. It used to fire here, on
		// the click's own tick, with no animation at all.
		//
		// A refusal (unaffordable, no shield, a full missile pool) is asked NOW rather than at the cast
		// frame, so it still falls through to the swing below - the same "never does nothing" fallback
		// the rest use - instead of playing an animation that ends in nothing.
		if (oracool::CanStartRangedPaladinSkill(myPlayer, *skill)) {
			oracool::ArmMeleeSkill(std::nullopt);
			LastMouseButtonSpell = spellID;
			LastMouseButtonSpellType = spellType;
			LastMouseButtonAction = MouseActionType::SpellMonsterTarget;
			NetSendCmdParam4(true, CMD_SPELLID, pcursmonst, static_cast<int16_t>(spellID), static_cast<uint8_t>(spellType), 0);
			return;
		}

		// Armed for DoAttack, which resolves the swing several frames from now and cannot otherwise
		// tell which button threw it. Set for every skill, including the ones with no melee effect,
		// so the latch always describes the CURRENT swing rather than some earlier one.
		oracool::ArmMeleeSkill(*skill);

		if (*skill == oracool::PaladinSkill::Charge) {
			// Mana is spent only when the dash actually launches, and the && short-circuits so a
			// Charge refused by the cooldown costs nothing. That follows the same rule: out of
			// cooldown or out of mana, the ability still swings rather than doing nothing - it just
			// arrives at walking pace.
			// Not while already dashing: every click of a sprint paid 10 mana again and restarted the dash, the cooldown
			// starting only at the arriving swing (round 21 audit, v1.12.246).
			if (!oracool::IsFuriousChargeOnCooldown() && !oracool::IsFuriousChargeDashing()
			    && oracool::SpendPaladinSkillMana(myPlayer, oracool::PaladinSkill::Charge))
				oracool::StartFuriousChargeDash();
		}
		LastMouseButtonAction = MouseActionType::AttackMonsterTarget;
		NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
		return;
	}

	const int spellFrom = 0;
	// Remember WHICH spell this cast was, so the hold-to-repeat path can repeat this one rather
	// than falling back on CheckPlrSpell's default arguments - which are the RIGHT button's spell.
	// Set once here, above all four dispatch branches, because every one of them is a cast of
	// exactly this spellID and a per-branch copy is a fifth place to forget.
	LastMouseButtonSpell = spellID;
	LastMouseButtonSpellType = spellType;
	if (IsWallSpell(spellID)) {
		LastMouseButtonAction = MouseActionType::Spell;
		Direction sd = GetDirection(myPlayer.position.tile, cursPosition);
		NetSendCmdLocParam4(true, CMD_SPELLXYD, cursPosition, static_cast<int16_t>(spellID), static_cast<uint8_t>(spellType), static_cast<uint16_t>(sd), spellFrom);
	} else if (pcursmonst != -1 && !isShiftHeld) {
		LastMouseButtonAction = MouseActionType::SpellMonsterTarget;
		NetSendCmdParam4(true, CMD_SPELLID, pcursmonst, static_cast<int16_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
	} else if (pcursplr != -1 && !isShiftHeld && !myPlayer.friendlyMode) {
		LastMouseButtonAction = MouseActionType::SpellPlayerTarget;
		NetSendCmdParam4(true, CMD_SPELLPID, pcursplr, static_cast<int16_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
	} else {
		LastMouseButtonAction = MouseActionType::Spell;
		NetSendCmdLocParam3(true, CMD_SPELLXY, cursPosition, static_cast<int16_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
	}
}

void DropItemBesidePlayer(Player &player, Item item)
{
	DeadItem(player, std::move(item), { 0, 0 });
}

void SyncPlrAnim(Player &player)
{
	const player_graphic graphic = player.getGraphic();
	if (!HeadlessMode)
		player.AnimInfo.sprites = player.AnimationData[static_cast<size_t>(graphic)].spritesForDirection(player._pdir);
}

void SyncInitPlrPos(Player &player)
{
	if (!player.isOnActiveLevel())
		return;

	const WorldTileDisplacement offset[9] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 }, { 2, 0 }, { 0, 2 }, { 1, 2 }, { 2, 1 }, { 2, 2 } };

	const auto onTrigger = [](Point testPosition) {
		for (int i = 0; i < numtrigs; i++) {
			if (trigs[i].position == testPosition)
				return true;
		}
		return false;
	};
	Point position = [&]() {
		// Never onto an exit, as the fallback below already refuses (audit, 2026-09-29): with the first tiles taken, a
		// rift revisit put the hero on the arrival exit beside the landing, and standing there sent him straight home.
		for (int i = 0; i < 8; i++) {
			Point position = player.position.tile + offset[i];
			if (PosOkPlayer(player, position) && !onTrigger(position))
				return position;
		}

		std::optional<Point> nearPosition = FindClosestValidPosition(
		    [&player](Point testPosition) {
			    for (int i = 0; i < numtrigs; i++) {
				    if (trigs[i].position == testPosition)
					    return false;
			    }
			    return PosOkPlayer(player, testPosition) && !PosOkPortal(currlevel, testPosition);
		    },
		    player.position.tile,
		    1, // skip the starting tile since that was checked in the previous loop
		    50);

		return nearPosition.value_or(Point { 0, 0 });
	}();

	player.position.tile = position;
	dPlayer[position.x][position.y] = player.getId() + 1;
	player.position.future = position;

	if (&player == MyPlayer) {
		ViewPosition = position;
	}
}

void SyncInitPlr(Player &player)
{
	SetPlrAnims(player);
	SyncInitPlrPos(player);
	if (&player != MyPlayer)
		player.lightId = NO_LIGHT;
}

void CheckStats(Player &player)
{
	for (auto attribute : enum_values<CharacterAttribute>()) {
		int maxStatPoint = 255;
		switch (attribute) {
		case CharacterAttribute::Strength:
			player._pBaseStr = clamp(player._pBaseStr, 0, maxStatPoint);
			break;
		case CharacterAttribute::Magic:
			player._pBaseMag = clamp(player._pBaseMag, 0, maxStatPoint);
			break;
		case CharacterAttribute::Dexterity:
			player._pBaseDex = clamp(player._pBaseDex, 0, maxStatPoint);
			break;
		case CharacterAttribute::Vitality:
			player._pBaseVit = clamp(player._pBaseVit, 0, maxStatPoint);
			break;
		}
	}
}

void ResetPlayerStats(Player &player)
{
	if (gbIsMultiplayer || &player != MyPlayer || !*sgOptions.Oracool.resetStatsButton)
		return;

	// Only undo points the player manually spent via the "+" buttons - permanent bonuses from
	// quests, shrines, and items reached _pBaseStr/Mag/Dex/Vit through a different call path and
	// are never tracked here, so they survive the reset untouched.
	//
	// Stat by stat through RefundStatPoints (audit, 2026-09-27), which holds the two rules this used to break: it
	// returned every spent point although ModifyPlr* stops at a base of 0 (a curse that took the base below what was
	// spent made points), and it took Vitality's life away with no floor (a hurt hero died from the click).
	const int maximumRefund = std::numeric_limits<int>::max();
	for (auto attribute : enum_values<CharacterAttribute>())
		RefundStatPoints(player, attribute, maximumRefund);
}

int StatPointsToSpend(const Player &player, CharacterAttribute attribute, int requested)
{
	constexpr int BaseCap = 255;
	const int roomBelowCap = BaseCap - player.GetBaseAttributeValue(attribute);
	return std::max(0, std::min({ requested, player._pStatPts, roomBelowCap }));
}

int RefundStatPoints(Player &player, CharacterAttribute attribute, int count)
{
	if (gbIsMultiplayer || &player != MyPlayer || count <= 0)
		return 0;
	int *spent = nullptr;
	switch (attribute) {
	case CharacterAttribute::Strength:
		spent = &player._pStatPtsSpentStr;
		break;
	case CharacterAttribute::Magic:
		spent = &player._pStatPtsSpentMag;
		break;
	case CharacterAttribute::Dexterity:
		spent = &player._pStatPtsSpentDex;
		break;
	case CharacterAttribute::Vitality:
		spent = &player._pStatPtsSpentVit;
		break;
	}
	if (spent == nullptr)
		return 0;
	// Never more than was spent, nor more than the base holds (a curse or a death's loss can have taken the base
	// below what was put in).
	int refund = std::min({ count, *spent, player.GetBaseAttributeValue(attribute) });
	// Vitality takes its life away with it, current life included: never so much that the hero is left with less than
	// one point (audit, 2026-09-27 - a Warrior on 15 life shift-clicking the - lost 20 and died from a UI click).
	if (attribute == CharacterAttribute::Vitality) {
		const int lifePerPoint = PlayersData[static_cast<size_t>(player._pClass)].chrLife;
		if (lifePerPoint > 0)
			refund = std::min(refund, std::max(0, (player._pHitPoints - (1 << 6)) / lifePerPoint));
	}
	if (refund <= 0)
		return 0;
	// Read before any ModifyPlr*: each recalculates the gear inside it, and by the end life was already gone and the
	// floor below never fired (round 19 audit of v1.12.243).
	const bool wasAlive = player._pHitPoints >> 6 > 0;
	switch (attribute) {
	case CharacterAttribute::Strength:
		ModifyPlrStr(player, -refund);
		break;
	case CharacterAttribute::Magic:
		ModifyPlrMag(player, -refund);
		// Never below an empty orb: the refund takes its mana off the current pool too, and on an empty one it went
		// negative (round 4 audit, v1.12.229). The base moves with it, so the gap items make stays the same.
		if (player._pMana < 0) {
			player._pManaBase -= player._pMana;
			player._pMana = 0;
		}
		break;
	case CharacterAttribute::Dexterity:
		ModifyPlrDex(player, -refund);
		break;
	case CharacterAttribute::Vitality:
		ModifyPlrVit(player, -refund);
		break;
	}
	*spent -= refund;
	player._pStatPts += refund;
	CalcPlrInv(player, true);
	// Never below 1 life, whatever else the recalculation takes: Endurance's and Perfect Vessel's share of base life
	// shrank with a Vitality refund, and a Strength refund could switch off +life gear - the hero died in the dungeon
	// on the next tick (round 18 audit, v1.12.243). The kill check reads life only after this returns.
	if (wasAlive && player._pHitPoints < 64)
		SetPlayerHitPoints(player, 64);
	RedrawEverything();
	return refund;
}

void ModifyPlrStr(Player &player, int l)
{
	const int maximum = 255;
	l = clamp(l, 0 - player._pBaseStr, maximum - player._pBaseStr);

	player._pStrength += l;
	player._pBaseStr += l;

	CalcPlrInv(player, true);

	if (&player == MyPlayer) {
		NetSendCmdParam1(false, CMD_SETSTR, player._pBaseStr);
	}
}

void ModifyPlrMag(Player &player, int l)
{
	const int maximum = 255;
	l = clamp(l, 0 - player._pBaseMag, maximum - player._pBaseMag);

	player._pMagic += l;
	player._pBaseMag += l;

	int ms = l;
	ms *= PlayersData[static_cast<size_t>(player._pClass)].chrMana;

	player._pMaxManaBase += ms;
	player._pMaxMana += ms;
	if (HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		player._pManaBase += ms;
		player._pMana += ms;
	}

	CalcPlrInv(player, true);

	if (&player == MyPlayer) {
		NetSendCmdParam1(false, CMD_SETMAG, player._pBaseMag);
	}
}

void ModifyPlrDex(Player &player, int l)
{
	const int maximum = 255;
	l = clamp(l, 0 - player._pBaseDex, maximum - player._pBaseDex);

	player._pDexterity += l;
	player._pBaseDex += l;
	CalcPlrInv(player, true);

	if (&player == MyPlayer) {
		NetSendCmdParam1(false, CMD_SETDEX, player._pBaseDex);
	}
}

void ModifyPlrVit(Player &player, int l)
{
	const int maximum = 255;
	l = clamp(l, 0 - player._pBaseVit, maximum - player._pBaseVit);

	player._pVitality += l;
	player._pBaseVit += l;

	int ms = l;
	ms *= PlayersData[static_cast<size_t>(player._pClass)].chrLife;

	player._pHPBase += ms;
	player._pMaxHPBase += ms;
	player._pHitPoints += ms;
	player._pMaxHP += ms;

	CalcPlrInv(player, true);

	if (&player == MyPlayer) {
		NetSendCmdParam1(false, CMD_SETVIT, player._pBaseVit);
	}
}

void CalcPlrInvKeepingLife(Player &player)
{
	const int before = player._pHitPoints;
	CalcPlrInv(player, false);
	// Only kept alive, not kept whole: gaining the bonus adds it to current life (CalcPlrItemVals), so keeping the life on
	// the way out made every off/on - Endurance swapped away and back, Battle Orders run out and recast - a free heal of
	// the whole bonus (round 16 audit, v1.12.241). A bonus ending at low life still never kills (round 7).
	if (before > 0 && player._pHitPoints < 64)
		SetPlayerHitPoints(player, 64);
}

void SetPlayerHitPoints(Player &player, int val)
{
	player._pHitPoints = val;
	player._pHPBase = val + player._pMaxHPBase - player._pMaxHP;

	if (&player == MyPlayer) {
		RedrawComponent(PanelDrawComponent::Health);
	}
}

void SetPlrStr(Player &player, int v)
{
	player._pBaseStr = v;
	CalcPlrInv(player, true);
}

void SetPlrMag(Player &player, int v)
{
	player._pBaseMag = v;

	int m = v;
	m *= PlayersData[static_cast<size_t>(player._pClass)].chrMana;

	player._pMaxManaBase = m;
	player._pMaxMana = m;
	CalcPlrInv(player, true);
}

void SetPlrDex(Player &player, int v)
{
	player._pBaseDex = v;
	CalcPlrInv(player, true);
}

void SetPlrVit(Player &player, int v)
{
	player._pBaseVit = v;

	int hp = v;
	hp *= PlayersData[static_cast<size_t>(player._pClass)].chrLife;

	player._pHPBase = hp;
	player._pMaxHPBase = hp;
	CalcPlrInv(player, true);
}

void InitDungMsgs(Player &player)
{
	player.pDungMsgs = 0;
	player.pDungMsgs2 = 0;
}

enum {
	// clang-format off
	DungMsgCathedral = 1 << 0,
	DungMsgCatacombs = 1 << 1,
	DungMsgCaves     = 1 << 2,
	DungMsgHell      = 1 << 3,
	DungMsgDiablo    = 1 << 4,
	// clang-format on
};

void PlayDungMsgs()
{
	assert(MyPlayer != nullptr);
	Player &myPlayer = *MyPlayer;

	if (!setlevel && currlevel == 1 && !myPlayer._pLvlVisited[1] && (myPlayer.pDungMsgs & DungMsgCathedral) == 0) {
		myPlayer.Say(HeroSpeech::TheSanctityOfThisPlaceHasBeenFouled, 40);
		myPlayer.pDungMsgs = myPlayer.pDungMsgs | DungMsgCathedral;
	} else if (!setlevel && currlevel == 5 && !myPlayer._pLvlVisited[5] && (myPlayer.pDungMsgs & DungMsgCatacombs) == 0) {
		myPlayer.Say(HeroSpeech::TheSmellOfDeathSurroundsMe, 40);
		myPlayer.pDungMsgs |= DungMsgCatacombs;
	} else if (!setlevel && currlevel == 9 && !myPlayer._pLvlVisited[9] && (myPlayer.pDungMsgs & DungMsgCaves) == 0) {
		myPlayer.Say(HeroSpeech::ItsHotDownHere, 40);
		myPlayer.pDungMsgs |= DungMsgCaves;
	} else if (!setlevel && currlevel == 13 && !myPlayer._pLvlVisited[13] && (myPlayer.pDungMsgs & DungMsgHell) == 0) {
		myPlayer.Say(HeroSpeech::IMustBeGettingClose, 40);
		myPlayer.pDungMsgs |= DungMsgHell;
	} else if (!setlevel && currlevel == 16 && !myPlayer._pLvlVisited[16] && (myPlayer.pDungMsgs & DungMsgDiablo) == 0) {
		sfxdelay = 40;
		sfxdnum = PS_DIABLVLINT;
		myPlayer.pDungMsgs |= DungMsgDiablo;
	} else if (!setlevel && currlevel == 17 && !myPlayer._pLvlVisited[17] && (myPlayer.pDungMsgs2 & 1) == 0) {
		sfxdelay = 10;
		sfxdnum = USFX_DEFILER1;
		Quests[Q_DEFILER]._qactive = QUEST_ACTIVE;
		Quests[Q_DEFILER]._qlog = true;
		Quests[Q_DEFILER]._qmsg = TEXT_DEFILER1;
		NetSendCmdQuest(true, Quests[Q_DEFILER]);
		myPlayer.pDungMsgs2 |= 1;
	} else if (!setlevel && currlevel == 19 && !myPlayer._pLvlVisited[19] && (myPlayer.pDungMsgs2 & 4) == 0) {
		sfxdelay = 10;
		sfxdnum = USFX_DEFILER3;
		myPlayer.pDungMsgs2 |= 4;
	} else if (!setlevel && currlevel == 21 && !myPlayer._pLvlVisited[21] && (myPlayer.pDungMsgs & 32) == 0) {
		myPlayer.Say(HeroSpeech::ThisIsAPlaceOfGreatPower, 30);
		myPlayer.pDungMsgs |= 32;
	} else if (setlevel && setlvlnum == SL_SKELKING && !gbIsSpawn && !myPlayer._pSLvlVisited[SL_SKELKING] && Quests[Q_SKELKING]._qactive == QUEST_ACTIVE) {
		sfxdelay = 10;
		sfxdnum = USFX_SKING1;
	} else {
		sfxdelay = 0;
	}
	// The Defiler is logged on the first Hive floor the hero sets foot on, not only on 17: a waypoint kept from an earlier
	// game goes straight to 18-20, and the quest stayed out of the log until it was done (round 22 audit, v1.12.247).
	if (!setlevel && currlevel >= 18 && currlevel <= 20 && IsAnyOf(Quests[Q_DEFILER]._qactive, QUEST_INIT, QUEST_ACTIVE) && !Quests[Q_DEFILER]._qlog) {
		Quests[Q_DEFILER]._qactive = QUEST_ACTIVE;
		Quests[Q_DEFILER]._qlog = true;
		Quests[Q_DEFILER]._qmsg = TEXT_DEFILER1;
		NetSendCmdQuest(true, Quests[Q_DEFILER]);
	}
}

#ifdef BUILD_TESTING
bool TestPlayerDoGotHit(Player &player)
{
	return DoGotHit(player);
}

bool TestShouldDropGoldOnDeath(Player &player)
{
	return ShouldDropGoldOnDeath(player);
}
#endif

bool PlayerStrikesMonster(Player &player, Monster &monster)
{
	// A swing's whole on-hit layer, as DoAttack gives a blow (audit, 2026-09-29: a spin struck plain physical only - no
	// fire or lightning from the weapon, no passives, no curses). Not the skill latches, which belong to the swing that
	// armed them, and not the weapon wear: four blows a second would eat a weapon.
	oracool::NoteRageCombat(player);
	const Point position = monster.position.tile;
	const size_t playerId = player.getId();
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FireDamage) || player._pIFMaxDam > 0)
		AddMissile(position, { 1, 0 }, Direction::South, MissileID::WeaponExplosion, TARGET_MONSTERS, playerId, 0, 0);
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::LightningDamage) || player._pILMaxDam > 0)
		AddMissile(position, { 2, 0 }, Direction::South, MissileID::WeaponExplosion, TARGET_MONSTERS, playerId, 0, 0);
	int hitDamage = 0;
	if (!PlrHitMonst(player, monster, false, &hitDamage))
		return false;
	oracool::OnPassiveHit(player, monster, hitDamage, true);
	oracool::OnRfa12Hit(player, monster, hitDamage, true);
	oracool::ApplyVengeanceCold(player, monster);
	oracool::OnCursedMonsterStruck(monster, player, nullptr, hitDamage);
	return true;
}

} // namespace devilution
