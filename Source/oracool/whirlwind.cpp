#include "oracool/whirlwind.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "cursor.h"
#include "diablo.h"
#include "engine.h"
#include "engine/render/clx_render.hpp"
#include "itemdat.h" // ICURS_SMALL_AXE, ICURS_SHORT_SWORD - the circling blades
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
#include "oracool/sprite_scale.h"
#include "player.h"

#include <SDL.h>

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

bool RightButtonOnly(SpellID spell)
{
	// The Barbarian's spells, as the user counts them (2026-09-29): Whirlwind and Earthquake (the Skill Cards page), Leap,
	// Ground Stomp and Rend (dev notes), and every active on his Warcries page ("warcries to be rmb only").
	if (IsAnyOf(spell, SpellID::Whirlwind, SpellID::Earthquake, SpellID::Leap, SpellID::GroundStomp, SpellID::Rend))
		return true;
	const ClassTreeSkill row = ClassTreeSkillForSpell(HeroClass::Barbarian, spell);
	if (row == ClassTreeSkill::None)
		return false;
	const ClassTreeSkillData &data = GetClassTreeSkillData(row);
	return data.page == BarbarianWarcriesPage && data.kind == ClassTreeKind::Active;
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
	// The start's price, paid once (audit, 2026-09-29: it was checked and never taken, so tapping the button struck for
	// nothing).
	SettleSkill(player, SpellID::Whirlwind, /*landedBlows=*/0);
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
	// ...or another skill readied on the right mid-spin, by an F-key (audit, 2026-09-29: it spun on, draining Rage).
	if (sgbMouseDown != CLICK_RIGHT || outOfRage || leveltype == DTYPE_TOWN || player._pHitPoints >> 6 <= 0
	    || player._pmode == PM_DEATH || player._pRSpell != SpellID::Whirlwind) {
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
	return WhirlFrame(*frames, Clock);
}

ClxSprite WhirlFrame(ClxSpriteList frames, int clock)
{
	// The warrior's cast has 20 frames: the cloud gathers over the first dozen, is whole from 13 to 18 and thins on the
	// last. Those six, forward and back, taken as shares so a sheet of another length picks the same stretch.
	const int count = static_cast<int>(frames.numSprites());
	const int first = std::min(count - 1, count * 13 / 20);
	const int last = std::max(first, std::min(count - 1, count * 18 / 20));
	const int span = last - first;
	int step = span > 0 ? clock % (2 * span) : 0;
	if (step > span)
		step = 2 * span - step;
	return frames[static_cast<size_t>(first + step)];
}

namespace {

/** Steps of a whole turn a circling blade is drawn at. */
constexpr unsigned BladeSteps = 16;
/** The inventory icons are 84px tall; 36% is about the length of the weapon in his hand. */
constexpr unsigned BladePercent = 36;
/** Seconds for a blade to go once round him, and to turn once end over end. */
constexpr double BladeOrbitSeconds = 0.6;
constexpr double BladeTurnSeconds = 0.3;
/** The circle's half-width and half-height on screen (a floor circle, seen from above at the game's angle). */
constexpr int BladeOrbitX = 34;
constexpr int BladeOrbitY = 14;
/** How far above his sprite's bottom edge the circle runs: the cloud's middle. */
constexpr int BladeOrbitHeight = 34;

/** The turned blades, axe then sword, built from the inventory icons on first use. */
std::optional<OwnedClxSpriteList> TurnedBlades[2];

const std::optional<OwnedClxSpriteList> &Blade(int kind)
{
	std::optional<OwnedClxSpriteList> &slot = TurnedBlades[kind];
	if (!slot)
		slot = TurnedClxList(GetInvItemSprite((kind == 0 ? ICURS_SMALL_AXE : ICURS_SHORT_SWORD) + CURSOR_FIRSTITEM), BladePercent, BladeSteps);
	return slot;
}

} // namespace

void DrawWhirlwindBlades(const Surface &out, const Player &player, Point foot, bool front)
{
	if (IsWhirlwinding(player))
		DrawWhirlingBlades(out, foot, front);
}

void DrawWhirlingBlades(const Surface &out, Point foot, bool front)
{
	constexpr double Pi = 3.14159265358979323846;
	// The wall clock, so the circling runs smooth at any frame rate.
	const double t = SDL_GetTicks() / 1000.0;
	for (int i = 0; i < 4; i++) {
		// Axe, sword, axe, sword, a quarter of the circle apart.
		const double angle = 2.0 * Pi * (t / BladeOrbitSeconds) + i * Pi / 2.0;
		const double depth = std::sin(angle); // positive: the camera's side of him
		if ((depth > 0) != front)
			continue;
		const std::optional<OwnedClxSpriteList> &blades = Blade(i % 2);
		if (!blades)
			continue;
		const ClxSpriteList list { *blades };
		const double turn = t / BladeTurnSeconds + i * 0.25;
		const auto k = static_cast<uint32_t>((turn - std::floor(turn)) * BladeSteps) % BladeSteps;
		const ClxSprite sprite = list[k];
		const Point centre = foot + Displacement { static_cast<int>(std::lround(BladeOrbitX * std::cos(angle))),
			static_cast<int>(std::lround(BladeOrbitY * depth)) - BladeOrbitHeight };
		// A sprite is drawn from its bottom-left corner.
		ClxDraw(out, centre + Displacement { -static_cast<int>(sprite.width()) / 2, static_cast<int>(sprite.height()) / 2 }, sprite);
	}
}

} // namespace devilution::oracool
