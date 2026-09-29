#include "oracool/whirlwind.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "cursor.h"
#include "diablo.h"
#include "engine.h"
#include "levels/gendung.h"
#include "monster.h"
#include "msg.h"
#include "oracool/class_tree.h"
#include "oracool/companion.h"
#include "oracool/minions.h"
#include "oracool/missile_tint.h"
#include "oracool/rage.h"
#include "oracool/rfa12_actives.h"
#include "oracool/skill_sounds.h"
#include "player.h"

namespace devilution::oracool {

namespace {

bool Active = false;
/** Ticks since the spin started: the strikes, the Rage drain and the turn all count from it. */
int Clock = 0;
/** The way he last glided - kept when the cursor is under him, so he never stops on it. */
Direction Heading = Direction::South;
/** The tile the last walk order went to, so a new one is only sent when it changes or he has stopped. */
Point Ordered { -1, -1 };

constexpr int TicksPerSecond = 20;
/** How far ahead of him the glide aims when the cursor is on his own tile. */
constexpr int OvershootTiles = 3;

Point GlideTarget(const Player &player)
{
	const Point here = player.position.tile;
	if (cursPosition != here && InDungeonBounds(cursPosition)) {
		Heading = GetDirection(here, cursPosition);
		return cursPosition;
	}
	Point ahead = here;
	for (int i = 0; i < OvershootTiles; i++) {
		const Point next = ahead + Heading;
		if (!InDungeonBounds(next))
			break;
		ahead = next;
	}
	return ahead;
}

void Glide(Player &player)
{
	const Point target = GlideTarget(player);
	if (target == player.position.tile)
		return;
	if (target == Ordered && player._pmode != PM_STAND)
		return;
	Ordered = target;
	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, target);
}

/** @brief Every enemy on the eight tiles around @p player that a blow can reach. */
std::vector<Monster *> EnemiesBeside(const Player &player)
{
	std::vector<Monster *> found;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &m = Monsters[ActiveMonsters[i]];
		if ((m.hitPoints >> 6) <= 0 || !m.isPossibleToHit() || m.isPlayerMinion() || IsMinion(m) || IsCompanion(m))
			continue;
		if (player.position.tile.WalkingDistance(m.position.tile) <= 1)
			found.push_back(&m);
	}
	return found;
}

void Strike(Player &player)
{
	const std::vector<Monster *> beside = EnemiesBeside(player);
	if (beside.empty())
		return;
	NoteRageCombat(player); // fighting: the calm drain waits
	bool landed = false;
	for (Monster *m : beside) {
		if (!PlayerStrikesMonster(player, *m))
			continue;
		landed = true;
		DrawHolyBurst(player, m->position.tile, 25, hue::PaleWarm);
	}
	if (landed) {
		const ClassTreeSkill row = ClassTreeSkillForSpell(player._pClass, SpellID::Whirlwind);
		if (row != ClassTreeSkill::None)
			PlaySkillSound(row, SkillSoundEvent::Impact);
	}
}

} // namespace

bool WhirlwindRightButtonOnly(SpellID spell)
{
	return spell == SpellID::Whirlwind;
}

bool IsWhirlwinding(const Player &player)
{
	return Active && &player == MyPlayer;
}

bool StartWhirlwind(Player &player)
{
	if (&player != MyPlayer || leveltype == DTYPE_TOWN || player._pHitPoints >> 6 <= 0)
		return false;
	if (!CanPaySkill(player, SpellID::Whirlwind))
		return false;
	if (Active)
		return true;
	LoadPlrGFX(player, player_graphic::Magic); // the sheet the spin is drawn with
	Active = true;
	Clock = 0;
	Heading = player._pdir;
	Ordered = { -1, -1 };
	const ClassTreeSkill row = ClassTreeSkillForSpell(player._pClass, SpellID::Whirlwind);
	if (row != ClassTreeSkill::None)
		PlaySkillSound(row, SkillSoundEvent::Cast);
	Glide(player);
	return true;
}

void StopWhirlwind(Player &player)
{
	if (!Active)
		return;
	Active = false;
	if (&player == MyPlayer)
		ClrPlrPath(player); // the step under way finishes; nothing after it
}

void ResetWhirlwind()
{
	Active = false;
	Clock = 0;
}

void ProcessWhirlwindTick(Player &player)
{
	if (!Active || &player != MyPlayer)
		return;
	const bool outOfRage = UsesRage(player) && player._pRage <= 0;
	if (sgbMouseDown != CLICK_RIGHT || outOfRage || leveltype == DTYPE_TOWN || player._pHitPoints >> 6 <= 0
	    || player._pmode == PM_DEATH) {
		StopWhirlwind(player);
		return;
	}
	Clock++;
	// The Rage: WhirlwindRagePerSecond a second, a point at a time.
	if (UsesRage(player) && Clock % (TicksPerSecond / WhirlwindRagePerSecond) == 0)
		player._pRage = std::max(player._pRage - 1, 0);
	// Being hit breaks a walk into the hit animation; the glide simply picks up again when it is over.
	if (player._pmode == PM_STAND || player._pmode == PM_WALK_NORTHWARDS || player._pmode == PM_WALK_SOUTHWARDS
	    || player._pmode == PM_WALK_SIDEWAYS)
		Glide(player);
	if (Clock % WhirlwindStrikeTicks == 0)
		Strike(player);
}

int WhirlwindDamagePercent(int rank)
{
	return 66 + 5 * (std::max(rank, 1) - 1);
}

std::optional<ClxSprite> WhirlwindSprite(const Player &player)
{
	if (!IsWhirlwinding(player))
		return std::nullopt;
	const PlayerAnimationData &cast = player.AnimationData[static_cast<size_t>(player_graphic::Magic)];
	const Direction facing = static_cast<Direction>((Clock / WhirlwindTurnTicks) % 8);
	const OptionalClxSpriteList frames = cast.spritesForDirection(facing);
	if (!frames || frames->numSprites() == 0)
		return std::nullopt;
	return (*frames)[static_cast<size_t>(Clock) % frames->numSprites()];
}

} // namespace devilution::oracool
