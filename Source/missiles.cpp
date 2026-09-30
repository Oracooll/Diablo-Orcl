/**
 * @file missiles.cpp
 *
 * Implementation of missile functionality.
 */
#include "missiles.h"
#include "oracool/sat_math.h" // AddPercentSat - the damage passives past int (round 27 audit)

#include <vector>
#include <unordered_map> // ScaledMissileSprites
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <limits> // ScaleSpellEffect saturates rather than wrapping

#include "control.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "dead.h"
#ifdef _DEBUG
#include "debug.h"
#endif
#include "engine/backbuffer_state.hpp"
#include "engine/load_file.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/random.hpp"
#include "init.h"
#include "inv.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "monster.h"
#include "oracool/aura_field.h"
#include "oracool/endgame_boss.h"    // OnBossDealtDamage - Devouring drains from a missile too
#include "oracool/lesser_uniques.h"  // OnLesserUniqueDealtDamage, GetMonsterDisplayName
#include "oracool/monster_variants.h" // VariantPoisonsOnHit
#include "oracool/venom.h"
#include "oracool/combat_odds.h"
#include "oracool/curses.h"
#include "oracool/minions.h" // MinionOwner, OnMinionBlow: a minion's bolt is its owner's blow
#include "oracool/rift.h"    // TryEnterRiftFromTown: the town-side rift portal is a door
#include "oracool/chill.h"
#include "oracool/class_tree.h" // SlowPlayer - a cold hit's chill on the stride
#include "oracool/cold.h"
#include "oracool/companion.h"
#include "oracool/monster_scale.h" // GetScaledAnim - a sized charger keeps its size
#include "oracool/passives.h"
#include "oracool/rfa12_effects.h"
#include "oracool/rogue_arrows.h"
#include "oracool/warcries.h"
#include "engine/path.h"
#include "oracool/event_log.h"
#include "oracool/divine_trn.h"
#include "oracool/skill_sounds.h"
#include "oracool/paladin_ranged.h"
#include "oracool/sprite_scale.h"
#include "spells.h"
#include "utils/str_cat.hpp"

namespace devilution {

std::list<Missile> Missiles;
bool MissilePreFlag;

namespace {

int AddClassHealingBonus(int hp, HeroClass heroClass)
{
	switch (heroClass) {
	case HeroClass::Warrior:
	case HeroClass::Monk:
	case HeroClass::Barbarian:
		return hp * 2;
	case HeroClass::Rogue:
	case HeroClass::Bard:
		return hp + hp / 2;
	default:
		return hp;
	}
}

/**
 * @brief Blessed Hammer's spiral, in the three numbers that describe it.
 *
 * Tuned against the skill's range of 10 tiles (oracool/paladin_skills.h): 60 ticks at 2.2px a tick
 * reaches 132 screen pixels, and one tile step is 32px across, so the hammer winds out to roughly
 * four tiles over three seconds. Well inside the range cap, because the spiral has to be watchable -
 * a hammer that reached the range limit would be a blur.
 *
 * The angle step gives a little over three full turns in that time, which is what makes it read as a
 * spiral rather than as a wide arc.
 */
/** @brief The pace AddHolyBolt launches at - Blessed Shield is specified as twice it. */
constexpr int HolyBoltSpeed = 16;
constexpr int BlessedShieldSpeedMultiplier = 2;
/**
 * @brief How long a thrown shield stays in the air.
 *
 * Comfortably longer than the skill's 10-tile reach needs at this speed; the throw ends when it hits
 * something, and this is only the backstop for a throw that hits nothing at all.
 */
constexpr int BlessedShieldRangeTicks = 255;

constexpr int BlessedHammerTicks = 60;
constexpr float BlessedHammerPixelsPerTick = 2.2F;
constexpr float BlessedHammerRadiansPerTick = 0.35F;

/**
 * @brief Grows @p base by an eighth per spell level. SATURATING - see below.
 *
 * This is EXPONENTIAL: it multiplies by 9/8 for every level, so it doubles roughly every six. That
 * was safe in vanilla, where a spell level could not exceed 15 - a base of 99 comes out at 558.
 *
 * This fork raised the ceiling to 98 (MaxSpellLevel), and Player::GetSpellLevel sums three stores -
 * books, invested points and item +spell levels - while clamping none of them, so the effective
 * level can pass even that. At 98 this returns nearly ten million; by 135, that times Flash's own
 * x3 passes INT_MAX and wraps NEGATIVE. Signed overflow is undefined behaviour, not just a silly
 * number, and it surfaced as Flash reporting negative damage (audit, 2026-08-31).
 *
 * The ceiling is INT_MAX/8, chosen to leave headroom for the largest multiplier any caller applies
 * to the result afterwards (x5, in the Elemental case). It sits far above every value a legitimate
 * spell level produces, so nothing reachable today changes - this only stops the wrap.
 *
 * It does NOT address how large these numbers get before that: ~10 million damage at spell level 98
 * is a balance question about an exponential curve meeting a raised cap, and that is the user's
 * call, not an overflow fix's.
 */
int ScaleSpellEffect(int base, int spellLevel)
{
	constexpr int64_t Ceiling = std::numeric_limits<int>::max() / 8;
	int64_t value = base;
	for (int i = 0; i < spellLevel; i++) {
		value += value / 8;
		if (value >= Ceiling)
			return static_cast<int>(Ceiling);
	}

	return static_cast<int>(value);
}

int GenerateRndSum(int range, int iterations)
{
	int value = 0;
	for (int i = 0; i < iterations; i++) {
		value += GenerateRnd(range);
	}

	return value;
}

bool CheckBlock(Point from, Point to)
{
	while (from != to) {
		from += GetDirection(from, to);
		if (TileHasAny(dPiece[from.x][from.y], TileProperties::Solid))
			return true;
	}

	return false;
}

Monster *FindClosest(Point source, int rad)
{
	// Oracool: town's dMonster holds towner ids, not monsters; and the hero's own minions and companions are
	// no quarry (Bone Spirit and the Elemental homed on them - the round 3 audit, v1.12.228).
	if (leveltype == DTYPE_TOWN)
		return nullptr;
	std::optional<Point> monsterPosition = FindClosestValidPosition(
	    [&source](Point target) {
		    // search for a monster with clear line of sight
		    if (!InDungeonBounds(target) || dMonster[target.x][target.y] <= 0 || CheckBlock(source, target))
			    return false;
		    const Monster &monster = Monsters[dMonster[target.x][target.y] - 1];
		    return !monster.isPlayerMinion() && !oracool::IsCompanion(monster);
	    },
	    source, 1, rad);

	if (monsterPosition) {
		int mid = dMonster[monsterPosition->x][monsterPosition->y];
		return &Monsters[mid - 1];
	}

	return nullptr;
}

constexpr Direction16 Direction16Flip(Direction16 x, Direction16 pivot)
{
	std::underlying_type_t<Direction16> ret = (2 * static_cast<std::underlying_type_t<Direction16>>(pivot) + 16 - static_cast<std::underlying_type_t<Direction16>>(x)) % 16;

	return static_cast<Direction16>(ret);
}

void UpdateMissileVelocity(Missile &missile, Point destination, int velocityInPixels)
{
	missile.position.velocity = { 0, 0 };

	if (missile.position.tile == destination)
		return;

	// Get the normalized vector in isometric projection
	Displacement fixed16NormalVector = (missile.position.tile - destination).worldToNormalScreen();

	// Multiplying by the target velocity gives us a scaled velocity vector.
	missile.position.velocity = fixed16NormalVector * velocityInPixels;
}

/**
 * @brief Add the missile to the lookup tables
 * @param missile The missile to add
 */
void PutMissile(Missile &missile)
{
	Point position = missile.position.tile;

	if (!InDungeonBounds(position))
		missile._miDelFlag = true;

	if (missile._miDelFlag) {
		return;
	}

	DungeonFlag &flags = dFlags[position.x][position.y];
	flags |= DungeonFlag::Missile;
	if (missile._mitype == MissileID::FireWall)
		flags |= DungeonFlag::MissileFireWall;
	if (missile._mitype == MissileID::LightningWall)
		flags |= DungeonFlag::MissileLightningWall;

	if (missile._miPreFlag)
		MissilePreFlag = true;
}

void UpdateMissilePos(Missile &missile)
{
	Displacement pixelsTravelled = missile.position.traveled >> 16;

	Displacement tileOffset = pixelsTravelled.screenToMissile();
	missile.position.tile = missile.position.start + tileOffset;

	missile.position.offset = pixelsTravelled + tileOffset.worldToScreen();

	Displacement absoluteLightOffset = pixelsTravelled.screenToLight();
	ChangeLightOffset(missile._mlid, absoluteLightOffset - tileOffset * 8);
}

/**
 * @brief Dodgy hack used to correct the position for charging monsters.
 *
 * If the monster represented by this missile is *not* facing north in some way it gets shifted to the south.
 * This appears to compensate for some visual oddity or invalid calculation earlier in the ProcessRhino logic.
 * @param missile MissileStruct representing a charging monster.
 */
void MoveMissilePos(Missile &missile)
{
	Direction moveDirection;

	switch (static_cast<Direction>(missile._mimfnum)) {
	case Direction::East:
		moveDirection = Direction::SouthEast;
		break;
	case Direction::West:
		moveDirection = Direction::SouthWest;
		break;
	case Direction::South:
	case Direction::SouthWest:
	case Direction::SouthEast:
		moveDirection = Direction::South;
		break;
	default:
		return;
	}

	auto target = missile.position.tile + moveDirection;
	if (IsTileAvailable(*missile.sourceMonster(), target)) {
		missile.position.tile = target;
		missile.position.offset += Displacement(moveDirection).worldToScreen();
	}
}

/**
 * @brief The damage range of the monster that fired @p missile, with its pack's Might, while its
 * slot still holds that spawn; false once it is gone. Round 28 audit: arrows, elemental arrows, Fireball, Lightning
 * Control and Inferno read the raw bytes of whatever held the slot, so a Relentless pack's archers and Storm Riders hit at
 * base damage while the health bar showed the raised range.
 */
bool LiveMonsterDamageRange(Missile &missile, int &minDamage, int &maxDamage)
{
	const Monster *monster = missile.liveSourceMonster();
	if (monster == nullptr)
		return false;
	// Not Frenzy of the Dead: a minion's missile takes it where it strikes a monster (MoveMissileAndCheckMissileCol).
	minDamage = oracool::PackAdjustedDamage(*monster, monster->minDamage);
	maxDamage = std::max<int>(oracool::PackAdjustedDamage(*monster, monster->maxDamage), minDamage);
	return true;
}

int ProjectileMonsterDamage(Missile &missile)
{
	const Monster &monster = *missile.sourceMonster();
	// The Might pack aura reaches the arrows too: the health bar showed it on a Relentless pack's archers while their
	// shots hit at base damage (round 11 audit, v1.12.236).
	const int minDamage = oracool::PackAdjustedDamage(monster, monster.minDamage);
	const int maxDamage = std::max<int>(oracool::PackAdjustedDamage(monster, monster.maxDamage), minDamage);
	return minDamage + GenerateRnd(maxDamage - minDamage + 1);
}

/**
 * @brief A monster's missile landed on @p player for @p dam: the on-hit traits the melee path fires in
 * MonsterAttackPlayer. Vampiric, Devouring and Venomous did nothing on an archer or a caster (round 11 audit, v1.12.236).
 */
void OnMonsterMissileLanded(Player &player, Monster &monster, int dam, int poisonBase)
{
	if (oracool::VariantPoisonsOnHit(monster))
		oracool::PoisonPlayer(player, poisonBase, 100);
	if (monster.mode == MonsterMode::Death || monster.hitPoints <= 0)
		return;
	oracool::OnLesserUniqueDealtDamage(monster, dam);
	oracool::OnBossDealtDamage(monster, dam);
}

int ProjectileTrapDamage(Missile &missile)
{
	return currlevel + GenerateRnd(2 * currlevel);
}

namespace {

/** @brief Oracool: the percent of its owner's damage the companion arrow being checked deals - see CheckMissileCol. */
int CompanionHitPercent = 0;

/**
 * @brief Oracool (2026-09-26): whether @p missile lands in a cold impact sheet of its own - Ice Bolt and Ice Blast in
 * ice_impact, Glacial Spike in glacial_shatter, Freezing Arrow in freezing_burst. Every other cold hit gets the
 * hit_cold flash instead (AddColdHitFlash), so no hit shows two.
 */
bool HasOwnColdImpact(const Missile &missile)
{
	switch (missile._mitype) {
	case MissileID::IceBolt:
	case MissileID::IceBlast:
	case MissileID::GlacialSpike:
		return true;
	case MissileID::FrostArrow:
		return static_cast<oracool::RogueArrow>(missile.var5) == oracool::RogueArrow::FreezingArrow;
	default:
		return false;
	}
}

/** @brief An AddArtEffect sheet whose var1 is this keeps to its caster's tile while it plays (ProcessCensusEffect). */
constexpr int ArtEffectFollowsCaster = 1;

} // namespace

bool MonsterMHit(int pnum, int monsterId, int mindam, int maxdam, int dist, MissileID t, DamageType damageType, bool shift, int spellLevel)
{
	auto &monster = Monsters[monsterId];

	// A companion is never struck by its own side's missiles - its owner's, or its own arrows (oracool/companion.h). Nor a
	// minion (audit, 2026-09-27): a Necromancer's Nova or Inferno cut down his own army; Apocalypse, Frost Nova and the
	// Chain Lightning search already spared it.
	if (oracool::IsCompanion(monster) || oracool::IsMinion(monster) || oracool::IsMonsterConverted(monster)) // converted: round 19
		return false;

	if (!monster.isPossibleToHit() || monster.isImmune(t, damageType))
		return false;

	int hit = GenerateRnd(100);
	int hper = 0;
	const Player &player = Players[pnum];
	const MissileData &missileData = GetMissileData(t);
	// Hunter's Claim (RfA-12): her arrows pass by what is not the claimed one.
	// The hero's own arrows only: the Valkyrie's shot through her own target (round 17 audit).
	if (missileData.isArrow() && CompanionHitPercent == 0 && oracool::Rfa12ArrowIgnores(player, monster))
		return false;
	if (missileData.isArrow()) {
		hper = player.GetRangedPiercingToHit();
		hper -= player.CalculateArmorPierce(oracool::EffectiveMonsterArmor(monster), false);
		hper -= (dist * dist) / 2;
		// The hero's own shots only: a companion's arrows fly with the hero as their source (CompanionHitPercent set), and
		// noting them moved the hero's To hit bar onto the bow formula and a monster the hero never attacked (audit,
		// 2026-09-27).
		if (CompanionHitPercent == 0)
			oracool::NotePlayerAttackedMonster(player, monster, /*arrow=*/true, (dist * dist) / 2); // the sheet's To hit bar
	} else {
		hper = player.GetMagicToHit() - (monster.level(sgGameInitInfo.nDifficulty) * 2) - dist;
	}

	hper = clamp(hper, 5, 95);

	if (monster.mode == MonsterMode::Petrified)
		hit = 0;
	// Guided Arrow cannot miss (Oracool, Round 3): the whole of the skill, and the stone-curse line
	// above is the precedent for a roll that is not rolled.
	if (t == MissileID::GuidedArrow)
		hit = 0;
	// Diablo II's rule, on trial: a spell that reaches a monster lands (see SpellsNeverMiss). Only the
	// miss is gone - immunities above, and the resistances in the damage, still apply.
	// The weapon's fire and lightning too: it is spawned only on a landed blow since round 38.
	if (SpellsNeverMiss && !missileData.isArrow())
		hit = 0;

	if (monster.tryLiftGargoyle())
		return true;

	if (hit >= hper) {
#ifdef _DEBUG
		if (!DebugGodMode)
#endif
			return false;
	}

	int dam;
	if (t == MissileID::BoneSpirit) {
		dam = monster.hitPoints / 3 >> 6;
	} else {
		dam = mindam + GenerateRnd(maxdam - mindam + 1);
	}

	if (missileData.isArrow() && damageType == DamageType::Physical) {
		dam = player._pIBonusDamMod + dam * player._pIBonusDam / 100 + dam;
		if (player._pClass == HeroClass::Rogue)
			dam += player._pDamageMod;
		else
			dam += player._pDamageMod / 2;
		if (monster.data().monsterClass == MonsterClass::Demon && HasAnyOf(player._pIFlags, ItemSpecialEffect::TripleDemonDamage))
			dam *= 3;
	}
	bool resist = monster.isResistant(t, damageType);
	if (!shift)
		dam <<= 6;
	if (resist) {
		// A quarter, as it always was - except for cold, where Cold Mastery decides how much of the
		// penalty the monster keeps (Round 2). The divisor is 4 with no mastery, so the two branches
		// agree for a Sorceress who has not bought it and for every other class.
		dam /= damageType == DamageType::Cold ? oracool::ColdResistanceDivisor(player) : 4;
	}
	// Oracool, Round 5: the passives that read the situation - Steady Aim, Power Hungry, Cull the
	// Weak and the rest - on every missile a player lands.
	// Saturating: a high-level Fireball in 1/64 units wrapped int from about +40% (round 27 audit).
	dam = oracool::AddPercentSat(dam, oracool::PassiveDamageDealtPercent(player, monster, false) + oracool::Rfa12DamageDealtPercent(player, monster, false)
	        + (damageType == DamageType::Cold ? oracool::Rfa12ColdDamagePercent(monster) : 0));

	// A companion's arrow: its share of the whole blow, bonuses and passives included.
	if (CompanionHitPercent > 0)
		dam = std::max(dam * CompanionHitPercent / 100, 1);
	if (&player == MyPlayer)
		ApplyMonsterDamage(damageType, monster, dam);
	if (&player == MyPlayer && missileData.isArrow())
		oracool::OnPassiveHit(*MyPlayer, monster, dam, false);
	if (&player == MyPlayer && dam > 0) {
		oracool::OnRfa12Hit(*MyPlayer, monster, dam, false);
		oracool::OnCursedMonsterStruck(monster, *MyPlayer, nullptr, dam); // Life Tap (oracool/curses.h)
	}
	// The all-heroes sweep (2026-09-14): Paralysis, Temporal Flux, Thrill of the Hunt, the element marks.
	if (&player == MyPlayer && dam > 0)
		oracool::OnPassiveMissileHit(*MyPlayer, monster, dam, damageType, missileData.isArrow(),
		    /*sharedRulesDone=*/t == MissileID::WeaponExplosion); // the swing's part, not a blow of its own: Momentum (round 37)

	// COLD CHILLS (Oracool, Round 1) - and from Round 2, freezes, depending on the missile. Every
	// cold missile does it, rather than Ice Bolt doing it: the slow is what the damage type MEANS,
	// and hanging it on the element is what lets each new cold row inherit it without a line.
	//
	// After the damage and before the death check, so a killing blow does not chill a corpse: the
	// branch below either kills the monster or starts its hit reaction, and a chill applied past that
	// point would sit in the table until the level ended.
	if (damageType == DamageType::Cold && monster.hitPoints >> 6 > 0)
		oracool::ApplyColdHit(t, spellLevel, monster);

	if (monster.hitPoints >> 6 <= 0) {
		M_StartKill(monster, player);
	} else if (resist) {
		monster.tag(player);
		PlayEffect(monster, MonsterSound::Hit);
	} else {
		if (monster.mode != MonsterMode::Petrified && missileData.isArrow() && HasAnyOf(player._pIFlags, ItemSpecialEffect::Knockback))
			M_GetKnockback(monster);
		if (monster.type().type != MT_GOLEM)
			M_StartHit(monster, player, dam);
	}

	if (monster.activeForTicks == 0) {
		monster.activeForTicks = UINT8_MAX;
		monster.position.last = player.position.tile;
	}

	return true;
}

bool Plr2PlrMHit(const Player &player, int p, int mindam, int maxdam, int dist, MissileID mtype, DamageType damageType, bool shift, bool *blocked)
{
	Player &target = Players[p];

	if (sgGameInitInfo.bFriendlyFire == 0 && player.friendlyMode)
		return false;

	*blocked = false;

	if (target.isOnArenaLevel() && target._pmode == PM_WALK_SIDEWAYS)
		return false;

	if (target._pInvincible) {
		return false;
	}

	if (mtype == MissileID::HolyBolt) {
		return false;
	}

	const MissileData &missileData = GetMissileData(mtype);

	if (HasAnyOf(target._pSpellFlags, SpellFlag::Etherealize) && missileData.isArrow()) {
		return false;
	}

	int8_t resper;
	switch (damageType) {
	case DamageType::Fire:
		resper = target._pFireResist;
		break;
	case DamageType::Lightning:
		resper = target._pLghtResist;
		break;
	case DamageType::Magic:
	case DamageType::Acid:
		resper = target._pMagResist;
		break;
	default:
		resper = 0;
		break;
	}

	int hper = GenerateRnd(100);

	int hit;
	if (missileData.isArrow()) {
		hit = player.GetRangedToHit()
		    - (dist * dist / 2)
		    - target.GetArmor();
	} else {
		hit = player.GetMagicToHit()
		    - (target._pLevel * 2)
		    - dist;
	}

	hit = clamp(hit, 5, 95);

	if (hper >= hit) {
		return false;
	}

	int blkper = 100;
	if (!shift && (target._pmode == PM_STAND || target._pmode == PM_ATTACK) && target._pBlockFlag) {
		blkper = GenerateRnd(100);
	}

	int blk = target.GetBlockChance() - (player._pLevel * 2);
	blk = clamp(blk, 0, 100);

	int dam;
	if (mtype == MissileID::BoneSpirit) {
		dam = target._pHitPoints / 3;
	} else {
		dam = mindam + GenerateRnd(maxdam - mindam + 1);
		if (missileData.isArrow() && damageType == DamageType::Physical)
			dam += player._pIBonusDamMod + player._pDamageMod + dam * player._pIBonusDam / 100;
		if (!shift)
			dam <<= 6;
	}
	if (!missileData.isArrow())
		dam /= 2;
	// A NEGATIVE resistance amplifies the hit (D2 rules, 2026-09-13), then falls through to the ordinary
	// block-or-hit below - the resisted branch is a soft landing with no hit recovery and no block roll.
	if (resper < 0)
		dam -= (dam * resper) / 100;

	if (resper > 0) {
		dam -= (dam * resper) / 100;
		if (&player == MyPlayer)
			NetSendCmdDamage(true, p, dam, damageType);
		target.Say(HeroSpeech::ArghClang);
		return true;
	}

	if (blkper < blk) {
		StartPlrBlock(target, GetDirection(target.position.tile, player.position.tile));
		*blocked = true;
	} else {
		if (&player == MyPlayer)
			NetSendCmdDamage(true, p, dam, damageType);
		StartPlrHit(target, dam, false);
	}

	return true;
}

void RotateBlockedMissile(Missile &missile)
{
	int rotation = PickRandomlyAmong({ -1, 1 });

	if (missile._miAnimType == MissileGraphicID::Arrow) {
		int dir = missile._miAnimFrame + rotation;
		missile._miAnimFrame = (dir + 15) % 16 + 1;
		return;
	}

	int dir = missile._mimfnum + rotation;
	int mAnimFAmt = GetMissileSpriteData(missile._miAnimType).animFAmt;
	if (dir < 0)
		dir = mAnimFAmt - 1;
	else if (dir >= mAnimFAmt)
		dir = 0;

	SetMissDir(missile, dir);
}

void CheckMissileCol(Missile &missile, DamageType damageType, int minDamage, int maxDamage, bool isDamageShifted, Point position, bool dontDeleteOnCollision)
{
	if (!InDungeonBounds(position))
		return;

	int mx = position.x;
	int my = position.y;

	// Oracool: user request - every spell is now castable in town (see SpellData::isAllowedInTown),
	// but none of them should deal damage there - town has no monsters to hit anyway, but this
	// also protects other players/Towners standing nearby. Missiles still travel, animate, and
	// collide with tiles/objects normally; only the monster/player damage below is skipped.
	const bool suppressDamage = leveltype == DTYPE_TOWN;

	bool isMonsterHit = false;
	int mid = dMonster[mx][my];
	if (!suppressDamage && (mid > 0 || (mid != 0 && Monsters[abs(mid) - 1].mode == MonsterMode::Petrified))) {
		mid = abs(mid) - 1;
		// A trap's missile spares the hero's own side (round 34 audit: a Nova chest, an Oily shrine's fire wall, a wall trap
		// killed his skeletons and companions), as his own missiles do since 2026-09-27.
		// Exclusive (round 35 audit): as a disjunct, a trap that met a minion fell through to the faction test below, which
		// a source-less trap passes against the army, and on to Monsters[-1] for a berserked golem slot.
		if (missile.IsTrap()
		        ? (!Monsters[mid].isPlayerMinion() && !oracool::IsCompanion(Monsters[mid]))
		        : (missile._micaster == TARGET_PLAYERS && (                                        // or was fired by a monster and
		              Monsters[mid].isPlayerMinion() != missile.sourceMinion                       //  the monsters are on opposing factions
		              || (Monsters[missile._misource].flags & MFLAG_BERSERK) != 0                  //  or the attacker is berserked
		              || (Monsters[mid].flags & MFLAG_BERSERK) != 0                                //  or the target is berserked
		              )
		              // but a converted monster's shot spares the hero's side (round 40 audit: a converted Succubus's
		              // Bloodstar struck the skeletons in its line)
		              && !(missile._misource >= 0 && oracool::IsMonsterConverted(Monsters[missile._misource])
		                  && (Monsters[mid].isPlayerMinion() || oracool::IsCompanion(Monsters[mid]) || oracool::IsMonsterConverted(Monsters[mid]))))) {
			// then the missile can potentially hit this target.
			// A minion's bolt is its owner's blow (audit, 2026-09-19): tag the target BEFORE the hit,
			// since MonsterTrapHit runs MonsterDeath itself and the kill's experience goes to whoever
			// is tagged, and credit Life Tap and the army's on-blow effects after, as the melee seam does.
			// Only while the slot still holds the minion that fired it (Missile::sourceMinion).
			// The mage that FIRED it, not whoever holds its slot now (liveSourceMonster checks the spawn): a reused slot gave
			// another minion's habits or no credit at all (round 25 audit).
			Monster *const shooter = !missile.IsTrap() && missile._micaster == TARGET_PLAYERS ? missile.liveSourceMonster() : nullptr;
			const Player *armyOwner = shooter != nullptr && missile.sourceMinion && shooter->isPlayerMinion()
			    ? oracool::MinionOwner(*shooter)
			    : nullptr;
			// An enemy's shot at the army is blunted by Weaken and Decrepify, as its blow is since round 20 (round 25 audit).
			if (shooter != nullptr && !missile.sourceMinion && Monsters[mid].isPlayerMinion()) {
				if (const int weakened = oracool::MonsterDebuffDamagePercent(*shooter); weakened != 0) {
					minDamage = std::max(minDamage + minDamage * weakened / 100, 1);
					maxDamage = std::max(maxDamage + maxDamage * weakened / 100, minDamage);
				}
			}
			if (armyOwner != nullptr) {
				Monsters[mid].tag(*armyOwner);
				// Frenzy of the Dead's "minion damage" reaches a Skeletal Mage's bolt too, not only a blow (round 14 audit).
				const int frenzy = oracool::MinionDamagePercent(Monsters[missile._misource]);
				if (frenzy != 100 && frenzy > 0) {
					minDamage = minDamage * frenzy / 100;
					maxDamage = std::max(maxDamage * frenzy / 100, minDamage);
				}
			}
			int dealt = 0;
			isMonsterHit = MonsterTrapHit(mid, minDamage, maxDamage, missile._midist, missile._mitype, damageType, isDamageShifted, &dealt);
			if (armyOwner != nullptr && dealt > 0) {
				oracool::OnMinionBlow(Monsters[missile._misource], Monsters[mid], dealt);
				oracool::OnCursedMonsterStruck(Monsters[mid], Players[armyOwner->getId()], &Monsters[missile._misource], dealt);
			}
		} else if (IsAnyOf(missile._micaster, TARGET_BOTH, TARGET_MONSTERS)) {
			CompanionHitPercent = missile.companionPercent;
			oracool::SetCompanionBlowInFlight(missile.companionPercent > 0);
			isMonsterHit = MonsterMHit(missile._misource, mid, minDamage, maxDamage, missile._midist, missile._mitype, damageType, isDamageShifted, missile._mispllvl);
			oracool::SetCompanionBlowInFlight(false);
			CompanionHitPercent = 0;
			// A cold hit with no impact art of its own - a Blizzard shard, a Cold or Ice Arrow - flashes where it landed.
			if (isMonsterHit && damageType == DamageType::Cold && !HasOwnColdImpact(missile))
				AddColdHitFlash({ mx, my }, missile._misource);
			// Blessed Hammer's Impact cue on each monster it strikes (audit, 2026-09-29: the sound page's pick was never played).
			if (isMonsterHit && missile._mitype == MissileID::BlessedHammer && missile.sourcePlayer() == MyPlayer) {
				const oracool::ClassTreeSkill row = oracool::ClassTreeSkillForSpell(MyPlayer->_pClass, SpellID::BlessedHammer);
				if (row != oracool::ClassTreeSkill::None)
					oracool::PlaySkillSound(row, oracool::SkillSoundEvent::Impact);
			}
			// A Frozen Sentinel's bolt sounds the sentinel's Impact cue (2026-09-30, the Sorcerer Skill Cards page).
			if (isMonsterHit && missile.sentinelBolt && missile.sourcePlayer() == MyPlayer) {
				const oracool::ClassTreeSkill row = oracool::ClassTreeSkillForSpell(MyPlayer->_pClass, SpellID::FrozenSentinel);
				if (row != oracool::ClassTreeSkill::None)
					oracool::PlaySkillSound(row, oracool::SkillSoundEvent::Impact);
			}
			// A thrown weapon sounds Weapon Throw's Impact cue where it lands (2026-09-29, the Barbarian Skill Cards page).
			if (isMonsterHit && missile._mitype == MissileID::Arrow && IsAnyOf(missile._miAnimType, MissileGraphicID::ThrownAxe, MissileGraphicID::ThrownSword)
			    && &Players[missile._misource] == MyPlayer) {
				const oracool::ClassTreeSkill row = oracool::ClassTreeSkillForSpell(MyPlayer->_pClass, SpellID::WeaponThrow);
				if (row != oracool::ClassTreeSkill::None)
					oracool::PlaySkillSound(row, oracool::SkillSoundEvent::Impact);
			}
		}
	}

	if (isMonsterHit) {
		// The missile stops here - unless it is a Rogue's arrow and Pierce says it carries on (Round 5).
		if (!dontDeleteOnCollision && !oracool::ArrowPierces(missile))
			missile._mirange = 0;
		missile._miHitFlag = true;
	}

	bool isPlayerHit = false;
	bool blocked = false;
	const int8_t pid = dPlayer[mx][my];
	if (!suppressDamage && pid > 0) {
		if (missile._micaster != TARGET_BOTH && !missile.IsTrap()) {
			if (missile._micaster == TARGET_MONSTERS) {
				if ((pid - 1) != missile._misource)
					isPlayerHit = Plr2PlrMHit(Players[missile._misource], pid - 1, minDamage, maxDamage, missile._midist, missile._mitype, damageType, isDamageShifted, &blocked);
			} else if (Monster *const live = missile.liveSourceMonster(); live == nullptr && !missile.sourceMinion) {
				// Its caster is gone and the slot holds another spawn: the shot lands as a trap's would, answering to
				// no one (round 12 audit, v1.12.237 - the new occupant's to-hit, heal and name were read).
				isPlayerHit = PlayerMHit(pid - 1, nullptr, missile._midist, minDamage, maxDamage, missile._mitype, damageType, isDamageShifted, DeathReason::MonsterOrTrap, &blocked);
			} else if (!missile.sourceMinion && !oracool::IsMonsterConverted(*live)) {
				// A hero's own minions never shoot the hero (2026-09-26): their shots are aimed at monsters and
				// used to wound any player standing in the line - the Skeletal Mage's firebolts made it plain.
				Monster &monster = *live;
				// The Snow Witch's blue star and a Glacial monster's missiles strike a hero as cold (2026-09-26).
				const DamageType element = oracool::MonsterMissileElement(monster, damageType);
				isPlayerHit = PlayerMHit(pid - 1, &monster, missile._midist, minDamage, maxDamage, missile._mitype, element, isDamageShifted, DeathReason::MonsterOrTrap, &blocked);
				// Chilling Armor answers a RANGED hit; the other two armours do not, which is the
				// difference between them (Oracool, Round 2). A blocked shot is not a hit.
				// Not every tick of an acid puddle: each landed tick fired a free Ice Bolt, eight a second (round 10 audit).
				if (isPlayerHit && !blocked && missile._mitype != MissileID::AcidPuddle && (Players[pid - 1]._pHitPoints >> 6) > 0) // not from a corpse (round 39)
					oracool::OnColdArmourStruckAtRange(Players[pid - 1], monster);
			}
		} else {
			DeathReason deathReason = missile.sourceType() == MissileSource::Player ? DeathReason::Player : DeathReason::MonsterOrTrap;
			isPlayerHit = PlayerMHit(pid - 1, nullptr, missile._midist, minDamage, maxDamage, missile._mitype, damageType, isDamageShifted, deathReason, &blocked);
		}
	}

	if (isPlayerHit) {
		if (gbIsHellfire && blocked) {
			RotateBlockedMissile(missile);
		} else if (!dontDeleteOnCollision) {
			missile._mirange = 0;
		}
		missile._miHitFlag = true;
	}

	if (IsMissileBlockedByTile({ mx, my })) {
		Object *object = FindObjectAtPosition({ mx, my });
		if (object != nullptr && object->IsBreakable()) {
			BreakObjectMissile(missile.sourcePlayer(), *object);
		}

		if (!dontDeleteOnCollision)
			missile._mirange = 0;
		missile._miHitFlag = false;
	}

	const MissileData &missileData = GetMissileData(missile._mitype);
	// Oracool: and lands with its own impact cue instead of the Firebolt impact it borrows.
	if (missile._mirange == 0 && missileData.miSFX != -1 && !oracool::PlayColdMissileSound(missile, /*impact=*/true))
		PlaySfxLoc(missileData.miSFX, missile.position.tile);
}

bool MoveMissile(Missile &missile, tl::function_ref<bool(Point)> checkTile, bool ifCheckTileFailsDontMoveToTile = false)
{
	Point prevTile = missile.position.tile;
	missile.position.traveled += missile.position.velocity;
	UpdateMissilePos(missile);

	int possibleVisitTiles;
	if (missile.position.velocity.deltaX == 0 || missile.position.velocity.deltaY == 0)
		possibleVisitTiles = prevTile.WalkingDistance(missile.position.tile);
	else
		possibleVisitTiles = prevTile.ManhattanDistance(missile.position.tile);

	if (possibleVisitTiles == 0)
		return false;

	// Did the missile skip a tile?
	if (possibleVisitTiles > 1) {
		auto speed = abs(missile.position.velocity);
		float denominator = (2 * speed.deltaY >= speed.deltaX) ? 2 * speed.deltaY : speed.deltaX;
		auto incVelocity = missile.position.velocity * ((32 << 16) / denominator);
		auto traveled = missile.position.traveled - missile.position.velocity;
		// Adjust the traveled vector to start on the next smallest multiple of incVelocity
		if (incVelocity.deltaY != 0)
			traveled.deltaY = (traveled.deltaY / incVelocity.deltaY) * incVelocity.deltaY;
		if (incVelocity.deltaX != 0)
			traveled.deltaX = (traveled.deltaX / incVelocity.deltaX) * incVelocity.deltaX;
		do {
			auto initialDiff = missile.position.traveled - traveled;
			traveled += incVelocity;
			auto incDiff = missile.position.traveled - traveled;

			// we are at the original calculated position => resume with normal logic
			if ((initialDiff.deltaX < 0) != (incDiff.deltaX < 0))
				break;
			if ((initialDiff.deltaY < 0) != (incDiff.deltaY < 0))
				break;

			// calculate in-between tile
			Displacement pixelsTraveled = traveled >> 16;
			Displacement tileOffset = pixelsTraveled.screenToMissile();
			Point tile = missile.position.start + tileOffset;

			// we haven't quite reached the missile's current position,
			// but we can break early to avoid checking collisions in this tile twice
			if (tile == missile.position.tile)
				break;

			// skip collision logic if the missile is on a corner between tiles
			if (pixelsTraveled.deltaY % 16 == 0
			    && pixelsTraveled.deltaX % 32 == 0
			    && abs(pixelsTraveled.deltaY / 16) % 2 != abs(pixelsTraveled.deltaX / 32) % 2) {
				continue;
			}

			// don't call checkTile more than once for a tile
			if (prevTile == tile)
				continue;

			prevTile = tile;

			if (!checkTile(tile)) {
				missile.position.traveled = traveled;
				if (ifCheckTileFailsDontMoveToTile) {
					missile.position.traveled -= incVelocity;
					UpdateMissilePos(missile);
					missile.position.StopMissile();
				} else {
					UpdateMissilePos(missile);
				}
				return true;
			}

		} while (true);
	}

	if (!checkTile(missile.position.tile) && ifCheckTileFailsDontMoveToTile) {
		missile.position.traveled -= missile.position.velocity;
		UpdateMissilePos(missile);
		missile.position.StopMissile();
	}

	return true;
}

void MoveMissileAndCheckMissileCol(Missile &missile, DamageType damageType, int mindam, int maxdam, bool ignoreStart, bool ifCollidesDontMoveToHitTile)
{
	auto checkTile = [&](Point tile) {
		if (ignoreStart && missile.position.start == tile)
			return true;

		CheckMissileCol(missile, damageType, mindam, maxdam, false, tile, false);

		// Did missile hit anything?
		if (missile._mirange != 0)
			return true;

		if (missile._miHitFlag && GetMissileData(missile._mitype).movementDistribution == MissileMovementDistribution::Blockable)
			return false;

		return !IsMissileBlockedByTile(tile);
	};

	bool tileChanged = MoveMissile(missile, checkTile, ifCollidesDontMoveToHitTile);

	int16_t tileTargetHash = dMonster[missile.position.tile.x][missile.position.tile.y] ^ dPlayer[missile.position.tile.x][missile.position.tile.y];

	// missile didn't change the tile... check that we perform CheckMissileCol only once for any monster/player to avoid multiple hits for slow missiles
	if (!tileChanged && missile.lastCollisionTargetHash != tileTargetHash) {
		CheckMissileCol(missile, damageType, mindam, maxdam, false, missile.position.tile, false);
	}

	// remember what target CheckMissileCol was checked against
	missile.lastCollisionTargetHash = tileTargetHash;
}

void SetMissAnim(Missile &missile, MissileGraphicID animtype)
{
	int dir = missile._mimfnum;

	if (animtype > MissileGraphicID::None) {
		animtype = MissileGraphicID::None;
	}

	const MissileFileData &missileData = GetMissileSpriteData(animtype);
	// A facing past the sheet's own rows falls back to its first (audit, 2026-09-29): Seismic Slam's flames ride the
	// 16-way art-bolt carrier on Fire Wall's two-row sheet, so most aims indexed past it - an assert in Debug, and a read
	// past the sheet's frame-length table. The missile's own facing (_mimfnum) is left as it is: some missiles steer by it.
	if (missileData.animFAmt > 0 && dir >= missileData.animFAmt)
		dir = 0;

	missile._miAnimType = animtype;
	missile._miAnimFlags = missileData.flags;
	if (!HeadlessMode) {
		missile._miAnimData = missileData.spritesForDirection(static_cast<size_t>(dir));
	}
	missile.oracoolColours = missileData.colours.get(); // the sheet's own colours, when it has them
	missile._miAnimDelay = missileData.animDelay(dir);
	missile._miAnimLen = missileData.animLen(dir);
	missile._miAnimWidth = missileData.animWidth;
	missile._miAnimWidth2 = missileData.animWidth2;
	missile._miAnimCnt = 0;
	missile._miAnimFrame = 1;
	if (missile.oracoolScalePercent != 100)
		ScaleMissile(missile, missile.oracoolScalePercent, missile.oracoolScaleFloor); // a turn keeps the size
}

void AddRune(Missile &missile, Point dst, MissileID missileID)
{
	if (LineClearMissile(missile.position.start, dst)) {
		std::optional<Point> runePosition = FindClosestValidPosition(
		    [](Point target) {
			    if (!InDungeonBounds(target)) {
				    return false;
			    }
			    if (IsObjectAtPosition(target)) {
				    return false;
			    }
			    if (TileContainsMissile(target)) {
				    return false;
			    }
			    if (TileHasAny(dPiece[target.x][target.y], TileProperties::Solid)) {
				    return false;
			    }
			    return true;
		    },
		    dst, 0, 9);

		if (runePosition) {
			missile.position.tile = *runePosition;
			missile.var1 = static_cast<int>(missileID); // not int8_t: the fork's ids run past 127 (round 11 audit)
			// Custom Engineering (Rogue, 2026-09-14): the rune strikes as if three levels stronger.
			if (missile.sourceType() == MissileSource::Player)
				missile._mispllvl += oracool::PassiveRuneLevelBonus(*missile.sourcePlayer());
			missile._mlid = AddLight(missile.position.tile, 8);
			return;
		}
	}

	missile._miDelFlag = true;
}

bool CheckIfTrig(Point position)
{
	for (int i = 0; i < numtrigs; i++) {
		if (trigs[i].position.WalkingDistance(position) < 2)
			return true;
	}
	return false;
}

bool GuardianTryFireAt(Missile &missile, Point target)
{
	Point position = missile.position.tile;

	// Town's dMonster holds towner ids, not monsters (round 6 audit, v1.12.231). The Guardian fired on Griswold and Ogden.
	if (leveltype == DTYPE_TOWN)
		return false;
	if (!LineClearMissile(position, target))
		return false;
	int mid = dMonster[target.x][target.y] - 1;
	if (mid < 0)
		return false;
	const Monster &monster = Monsters[mid];
	if (monster.isPlayerMinion())
		return false;
	if (monster.hitPoints >> 6 <= 0)
		return false;

	Player &player = Players[missile._misource];
	int dmg = GenerateRnd(10) + (player._pLevel / 2) + 1;
	dmg = ScaleSpellEffect(dmg, missile._mispllvl);

	Direction dir = GetDirection(position, target);
	AddMissile(position, target, dir, MissileID::Firebolt, TARGET_MONSTERS, missile._misource, missile._midam, missile.sourcePlayer()->GetSpellLevel(SpellID::Guardian), &missile);
	SetMissDir(missile, 2);
	missile.var2 = 3;

	return true;
}

bool GrowWall(int playerId, Point position, Point target, MissileID type, int spellLevel, int damage)
{
	int dp = dPiece[position.x][position.y];
	assert(dp <= MAXTILES && dp >= 0);

	if (TileHasAny(dp, TileProperties::BlockMissile) || !InDungeonBounds(target)) {
		return false;
	}

	AddMissile(position, position, Players[playerId]._pdir, type, TARGET_BOTH, playerId, damage, spellLevel);
	return true;
}

/** @brief Sync missile position with parent missile */
void SyncPositionWithParent(Missile &missile, const AddMissileParameter &parameter)
{
	const Missile *parent = parameter.pParent;
	if (parent == nullptr)
		return;

	missile.position.offset = parent->position.offset;
	missile.position.traveled = parent->position.traveled;
}

void SpawnLightning(Missile &missile, int dam)
{
	missile._mirange--;
	MoveMissile(
	    missile, [&](Point tile) {
		    // Oracool: a bolt aimed at the edge of the map walks straight off dPiece. Easy to do in
		    // town, where the player can stand far closer to the boundary than any dungeon lets them.
		    // This was the one checkTile in the file that ASSERTED on leaving the dungeon rather than
		    // treating it as a stop, which is what the others all do - so do the same here.
		    if (!InDungeonBounds(tile)) {
			    missile._mirange = 0;
			    return false;
		    }
		    int pn = dPiece[tile.x][tile.y];
		    assert(pn >= 0 && pn <= MAXTILES);

		    if (!missile.IsTrap() || tile != missile.position.start) {
			    if (TileHasAny(pn, TileProperties::BlockMissile)) {
				    missile._mirange = 0;
				    return false;
			    }
		    }

		    return true;
	    });

	// MoveMissile parks the missile ON the tile that failed the check, so this can be the very tile
	// the lambda above just rejected. The dPiece read has to be inside the bounds test, not after it
	// - the one that used to guard AddMissile was a line too late to protect this read.
	const Point position = missile.position.tile;
	if (InDungeonBounds(position) && !TileHasAny(dPiece[position.x][position.y], TileProperties::BlockMissile)) {
		if (position != Point { missile.var1, missile.var2 }) {
			MissileID type = MissileID::Lightning;
			if (missile.sourceType() == MissileSource::Monster
			    && IsAnyOf(missile.sourceMonster()->type().type, MT_STORM, MT_RSTORM, MT_STORML, MT_MAEL)) {
				type = MissileID::ThinLightning;
			}
			AddMissile(
			    position,
			    missile.position.start,
			    Direction::South,
			    type,
			    missile._micaster,
			    missile._misource,
			    dam,
			    missile._mispllvl,
			    &missile);
			missile.var1 = position.x;
			missile.var2 = position.y;
		}
	}

	if (missile._mirange == 0) {
		missile._miDelFlag = true;
	}
}

} // namespace

#ifdef BUILD_TESTING
void TestRotateBlockedMissile(Missile &missile)
{
	RotateBlockedMissile(missile);
}
#endif

bool IsMissileBlockedByTile(Point tile)
{
	if (!InDungeonBounds(tile)) {
		return true;
	}

	if (TileHasAny(dPiece[tile.x][tile.y], TileProperties::BlockMissile)) {
		return true;
	}

	Object *object = FindObjectAtPosition(tile);
	// _oMissFlag is true if the object allows missiles to pass through so we need to invert the check here...
	return object != nullptr && !object->_oMissFlag;
}

// Oracool (2026-09-26): the book spells' per-level terms, one formula each. The Add/Process
// function below that used to compute each one inline now calls it, and so does the tooltip
// (oracool/skill_facts.cpp BookSpellFactsAt), so the Next Level block cannot quote a number the
// cast does not use. Every duration is in game ticks, 20 a second; each of these missiles loses one
// tick of _mirange per ProcessMissiles pass.

int LightningLingerTicks(int spellLevel)
{
	return (spellLevel / 2) + 6;
}

int ChainLightningLeapRadius(int spellLevel)
{
	return std::min<int>(spellLevel + 3, MaxCrawlRadius);
}

int FireWallDurationTicks(int spellLevel)
{
	return 16 * (spellLevel > 0 ? 10 * (spellLevel + 1) : 10);
}

int LightningWallDurationTicks(int spellLevel)
{
	return 255 * (spellLevel + 1);
}

int FlameWaveSideTiles(int spellLevel)
{
	return (spellLevel / 2) + 2;
}

int HolyBoltSpeedAtLevel(int spellLevel)
{
	return HolyBoltSpeed + std::min(spellLevel * 2, 47);
}

int GuardianDurationTicks(int spellLevel, int characterLevel)
{
	const int range = std::min(spellLevel + (characterLevel / 2), 30) * 16;
	return std::max(range, 30);
}

int StoneCurseDurationTicks(int spellLevel)
{
	return std::min(spellLevel + 6, 15) * 16;
}

int InfravisionDurationTicks(int spellLevel)
{
	return ScaleSpellEffect(1584, spellLevel);
}

int EtherealizeDurationTicks(int spellLevel)
{
	// A tenth of Infravision's base, because the description promises "briefly untouchable" and this
	// is untouchable, not merely dark-sighted.
	return ScaleSpellEffect(160, spellLevel);
}

int SearchDurationTicks(int spellLevel, int characterLevel)
{
	return (2 * characterLevel) + (10 * spellLevel) + 245;
}

int ReflectCharges(int spellLevel, int characterLevel)
{
	return (spellLevel != 0 ? spellLevel : 2) * characterLevel;
}

int BerserkDamageBonus(int spellLevel)
{
	return spellLevel;
}

namespace {
// The Mana spell's dice: 1d10, plus 1d4 per character level, plus 1d6 per spell level (AddMana).
constexpr int ManaSpellBaseDie = 10;
constexpr int ManaSpellCharacterLevelDie = 4;
constexpr int ManaSpellSpellLevelDie = 6;
} // namespace

int ManaSpellClassAmount(HeroClass heroClass, int amount)
{
	if (IsAnyOf(heroClass, HeroClass::Sorcerer, HeroClass::Necromancer))
		amount *= 2;
	if (heroClass == HeroClass::Rogue || heroClass == HeroClass::Bard)
		amount += amount / 2;
	return amount;
}

void ManaSpellAmountRange(const Player &player, int spellLevel, int &minAmount, int &maxAmount)
{
	const int characterLevels = std::max<int>(player._pLevel, 0);
	const int spellLevels = std::max(spellLevel, 0);
	const int lowest = 1 + characterLevels + spellLevels;
	const int highest = ManaSpellBaseDie + (ManaSpellCharacterLevelDie * characterLevels) + (ManaSpellSpellLevelDie * spellLevels);
	// In the cast's own 1/64 units, so the class bonus rounds the way it does there.
	minAmount = ManaSpellClassAmount(player._pClass, lowest << 6) >> 6;
	maxAmount = ManaSpellClassAmount(player._pClass, highest << 6) >> 6;
}

void GetDamageAmt(SpellID i, int *mind, int *maxd)
{
	assert(MyPlayer != nullptr);
	GetDamageAmtAtLevel(i, MyPlayer->GetSpellLevel(i), mind, maxd);
}

// Oracool: the spell level is a PARAMETER here rather than read from the player, so the Abilities
// window can ask "and what would this do one level from now?" - see the hover panel in
// panels/spell_book.cpp. Everything else still goes through the overload above, which supplies the
// player's current level and is what every pre-existing caller uses.
//
// The parameter is deliberately named `sl`, the name the body already used for the local it
// replaces, so the hundred-odd formulas below are untouched.
void GetDamageAmtAtLevel(SpellID i, int sl, int *mind, int *maxd)
{
	assert(MyPlayer != nullptr);
	assert(i >= SpellID::FIRST && i <= SpellID::LAST);

	Player &myPlayer = *MyPlayer;

	// Oracool: bug postmortem (2026-08-15) - the Abilities window showed "Damage: -858993460" for
	// Mana, which is 0xCCCCCCCC, MSVC's uninitialised-stack marker.
	//
	// The switch below has an explicit "no damage" bucket that writes -1, but it was never
	// EXHAUSTIVE: Mana, the Magi, the Jester and four of the five runes match no case at all, so the
	// function returned without writing either output and the caller printed its own uninitialised
	// locals. That was invisible until those spells became listable, which happened this morning when
	// they were given books.
	//
	// Defaulting here rather than adding them to the bucket makes the contract total - "-1 means no
	// damage" now holds for every SpellID, including any added later - and no caller needed changing,
	// because they all already test for -1.
	*mind = -1;
	*maxd = -1;

	// Oracool: the cold line's numbers come from the one function its missiles read (see
	// oracool/cold.h), so the sheet and the hit cannot disagree. Before the switch, so a cold spell
	// never falls into a vanilla case by accident.
	if (oracool::IsColdSpell(i)) {
		oracool::ColdSpellDamage(myPlayer, i, sl, *mind, *maxd);
		return;
	}
	// The bow skills' numbers too (Round 3): the bow's range plus the rank bonus, from the function
	// the arrows are rolled from.
	if (oracool::RogueArrowForSpell(i).has_value()) {
		oracool::RogueArrowDamage(myPlayer, i, sl, *mind, *maxd);
		return;
	}

	switch (i) {
	case SpellID::Firebolt:
		*mind = (myPlayer._pMagic / 8) + sl + 1;
		*maxd = *mind + 9;
		break;
	case SpellID::Healing:
	case SpellID::HealOther:
		// AddHealing's own range: 1 + L + sl up to 10 + 4L + 6sl, then the class bonus. The table took one off both ends
		// (round 12 audit, v1.12.237).
		*mind = AddClassHealingBonus(myPlayer._pLevel + sl + 1, myPlayer._pClass);
		*maxd = AddClassHealingBonus((4 * myPlayer._pLevel) + (6 * sl) + 10, myPlayer._pClass);
		break;
	case SpellID::RuneOfLight:
	case SpellID::Lightning:
	case SpellID::LightningBoltSkill: // the Rogue's bolt fires LightningControl, which rolls this
		*mind = 2;
		*maxd = 2 + myPlayer._pLevel;
		break;
	case SpellID::Flash:
		*mind = ScaleSpellEffect(myPlayer._pLevel, sl);
		*mind += *mind / 2;
		*maxd = *mind * 2;
		break;
	case SpellID::Identify:
	case SpellID::TownPortal:
	case SpellID::StoneCurse:
	case SpellID::Infravision:
	case SpellID::Phasing:
	case SpellID::ManaShield:
	case SpellID::DoomSerpents:
	case SpellID::BloodRitual:
	case SpellID::Invisibility:
	case SpellID::Rage:
	case SpellID::Teleport:
	case SpellID::Etherealize:
	case SpellID::ItemRepair:
	case SpellID::StaffRecharge:
	case SpellID::TrapDisarm:
	case SpellID::Resurrect:
	case SpellID::Telekinesis:
	case SpellID::BoneSpirit:
	case SpellID::Warp:
	case SpellID::Reflect:
	case SpellID::Berserk:
	case SpellID::Search:
	case SpellID::RuneOfStone:
		*mind = -1;
		*maxd = -1;
		break;
	case SpellID::FireWall:
	case SpellID::LightningWall:
	case SpellID::RingOfFire:
		*mind = 2 * myPlayer._pLevel + 4;
		*maxd = *mind + 36;
		break;
	case SpellID::Fireball:
	case SpellID::RuneOfFire: {
		int base = (2 * myPlayer._pLevel) + 4;
		*mind = ScaleSpellEffect(base, sl);
		*maxd = ScaleSpellEffect(base + 36, sl);
	} break;
	case SpellID::Guardian:
		// The Guardian shoots Firebolts, and AddFirebolt rolls a hero's bolt as magic/8 + level + 1 plus 0-9 at the
		// Guardian's spell level - the level-based roll the table showed is computed and never used (round 12 audit).
		*mind = (myPlayer._pMagic / 8) + sl + 1;
		*maxd = *mind + 9;
		break;
	case SpellID::ChainLightning:
		// Each bolt is a LightningControl rolling 2 to 2 + L, as Lightning's are; the table showed twice that (round 12).
		*mind = 2;
		*maxd = 2 + myPlayer._pLevel;
		break;
	case SpellID::FlameWave:
		*mind = 6 * (myPlayer._pLevel + 1);
		*maxd = *mind + 54;
		break;
	case SpellID::Nova:
	case SpellID::Immolation:
	case SpellID::RuneOfImmolation:
	case SpellID::RuneOfNova:
	case SpellID::LightningFury: // the Rogue's Lightning Fury is fired as Nova (spelldat), so it IS Nova's number
		// One ball's roll, as the other rows show one bolt's: the table's x5 read as one hit (round 29 audit) - only an enemy
		// against the caster takes more than one ball.
		*mind = ScaleSpellEffect((myPlayer._pLevel + 5) / 2, sl);
		*maxd = ScaleSpellEffect((myPlayer._pLevel + 30) / 2, sl);
		break;
	case SpellID::Inferno:
		*mind = 3;
		*maxd = myPlayer._pLevel + 4;
		*maxd += *maxd / 2;
		break;
	case SpellID::Golem: {
		// The golem's own blow, from the function SpawnGolem stands it up with (was a constant 11-17).
		const GolemStats golem = GolemStatsAt(myPlayer, sl);
		*mind = golem.minDamage;
		*maxd = golem.maxDamage;
	} break;
	case SpellID::Apocalypse:
		*mind = myPlayer._pLevel;
		*maxd = *mind * 6;
		break;
	case SpellID::Elemental:
		// Halved, as AddElemental halves the missile's damage (round 12 audit: the table showed twice the hit).
		*mind = ScaleSpellEffect(2 * myPlayer._pLevel + 4, sl) / 2;
		*maxd = ScaleSpellEffect(2 * myPlayer._pLevel + 40, sl) / 2;
		break;
	case SpellID::ChargedBolt:
		// AddChargedBolt rolls GenerateRnd(magic / 4) + 1: its top is magic / 4, not one more (round 16 audit).
		*mind = 1;
		*maxd = std::max(1, myPlayer._pMagic / 4);
		break;
	case SpellID::HolyBolt:
		*mind = myPlayer._pLevel + 9;
		*maxd = *mind + 9;
		break;
	case SpellID::BloodStar:
		*mind = (myPlayer._pMagic / 2) + 3 * sl - (myPlayer._pMagic / 8);
		*maxd = *mind;
		break;
	default:
		break;
	}
}

Direction16 GetDirection16(Point p1, Point p2)
{
	Displacement offset = p2 - p1;
	Displacement absolute = abs(offset);

	bool flipY = offset.deltaX != absolute.deltaX;
	bool flipX = offset.deltaY != absolute.deltaY;

	bool flipMedian = false;
	if (absolute.deltaX > absolute.deltaY) {
		std::swap(absolute.deltaX, absolute.deltaY);
		flipMedian = true;
	}

	Direction16 ret = Direction16::South;
	if (3 * absolute.deltaX <= (absolute.deltaY * 2)) { // mx/my <= 2/3, approximation of tan(33.75)
		if (5 * absolute.deltaX < absolute.deltaY)      // mx/my < 0.2, approximation of tan(11.25)
			ret = Direction16::SouthWest;
		else
			ret = Direction16::South_SouthWest;
	}

	Direction16 medianPivot = Direction16::South;
	if (flipY) {
		ret = Direction16Flip(ret, Direction16::SouthWest);
		medianPivot = Direction16Flip(medianPivot, Direction16::SouthWest);
	}
	if (flipX) {
		ret = Direction16Flip(ret, Direction16::SouthEast);
		medianPivot = Direction16Flip(medianPivot, Direction16::SouthEast);
	}
	if (flipMedian)
		ret = Direction16Flip(ret, medianPivot);
	return ret;
}

bool MonsterTrapHit(int monsterId, int mindam, int maxdam, int dist, MissileID t, DamageType damageType, bool shift, int *damageDealt)
{
	auto &monster = Monsters[monsterId];
	if (damageDealt != nullptr)
		*damageDealt = 0;

	if (!monster.isPossibleToHit() || monster.isImmune(t, damageType))
		return false;

	int hit = GenerateRnd(100);
	int hper = 90 - oracool::EffectiveMonsterArmor(monster) - dist;
	hper = clamp(hper, 5, 95);
	if (monster.tryLiftGargoyle())
		return true;
	if (hit >= hper && monster.mode != MonsterMode::Petrified) {
#ifdef _DEBUG
		if (!DebugGodMode)
#endif
			return false;
	}

	bool resist = monster.isResistant(t, damageType);
	int dam = mindam + GenerateRnd(maxdam - mindam + 1);
	if (!shift)
		dam <<= 6;
	if (resist)
		dam /= 4;
	ApplyMonsterDamage(damageType, monster, dam);
	if (damageDealt != nullptr)
		*damageDealt = dam;
#ifdef _DEBUG
	if (DebugGodMode)
		monster.hitPoints = 0;
#endif
	if (monster.hitPoints >> 6 <= 0) {
		MonsterDeath(monster, monster.direction, true);
	} else if (resist) {
		PlayEffect(monster, MonsterSound::Hit);
	} else if (monster.type().type != MT_GOLEM) {
		M_StartHit(monster, dam);
	}
	return true;
}

bool PlayerMHit(int pnum, Monster *monster, int dist, int mind, int maxd, MissileID mtype, DamageType damageType, bool shift, DeathReason deathReason, bool *blocked)
{
	*blocked = false;

	Player &player = Players[pnum];

	if (player._pHitPoints >> 6 <= 0) {
		return false;
	}

	if (player._pInvincible) {
		return false;
	}

	const MissileData &missileData = GetMissileData(mtype);

	if (HasAnyOf(player._pSpellFlags, SpellFlag::Etherealize) && missileData.isArrow()) {
		return false;
	}

	int hit = GenerateRnd(100);
#ifdef _DEBUG
	if (DebugGodMode)
		hit = 1000;
#endif
	int hper = 40;
	if (missileData.isArrow()) {
		int tac = player.GetArmor();
		if (monster != nullptr) {
			hper = monster->toHit(sgGameInitInfo.nDifficulty)
			    + ((monster->level(sgGameInitInfo.nDifficulty) - player._pLevel) * 2)
			    + 30
			    - (dist * 2) - tac
			    - oracool::MonsterDebuffToHit(*monster); // Weaken's aim reaches the archers too (round 28 audit)
		} else {
			hper = 100 - (tac / 2) - (dist * 2);
		}
	} else if (monster != nullptr) {
		hper += (monster->level(sgGameInitInfo.nDifficulty) * 2) - (player._pLevel * 2) - (dist * 2);
	}

	int minhit = 10;
	if (currlevel == 14)
		minhit = 20;
	if (currlevel == 15)
		minhit = 25;
	if (currlevel == 16)
		minhit = 30;
	hper = std::max(hper, minhit);

	int blk = 100;
	if ((player._pmode == PM_STAND || player._pmode == PM_ATTACK) && player._pBlockFlag) {
		blk = GenerateRnd(100);
	}

	if (shift)
		blk = 100;
	if (mtype == MissileID::AcidPuddle)
		blk = 100;

	int blkper = player.GetBlockChance(false) + oracool::Rfa12BlockBonus(player) + oracool::PassiveBlockBonus(player);
	if (monster != nullptr)
		blkper -= (monster->level(sgGameInitInfo.nDifficulty) - player._pLevel) * 2;
	blkper = clamp(blkper, 0, 100);

	int8_t resper;
	switch (damageType) {
	case DamageType::Fire:
		resper = player._pFireResist;
		break;
	case DamageType::Lightning:
		resper = player._pLghtResist;
		break;
	case DamageType::Magic:
	case DamageType::Acid:
		resper = player._pMagResist;
		break;
	case DamageType::Cold:
		// A resistance of its own since 2026-09-26 (user: "make it as real as it is in Diablo 2"); magic stood in.
		resper = player._pColdResist;
		break;
	default:
		resper = 0;
		break;
	}

	if (hit >= hper) {
		return false;
	}
	// Oracool, Round 5: Avoid - an arrow that would have landed slips instead. After the to-hit roll since v1.12.235:
	// before it, arrows that missed anyway still set off Tactical Advantage (round 10 audit).
	if (missileData.isArrow() && (oracool::PassiveEvadesMissile(player) || oracool::SlowMissilesTurnsAside(player)))
		return false;

	// What a cry, a song, Resolve or Numbing Traps has done to the shooter weakens its shots too, as its blows: the
	// melee path alone read them (round 13 audit, v1.12.238).
	if (monster != nullptr) {
		if (const int weakened = oracool::MonsterDebuffDamagePercent(*monster) + oracool::Rfa12MonsterDamagePercent(*monster) + oracool::PassiveMonsterDamagePercent(*monster); weakened != 0) {
			mind += mind * weakened / 100;
			maxd = std::max(maxd + maxd * weakened / 100, mind);
		}
	}

	int dam;
	if (mtype == MissileID::BoneSpirit) {
		dam = player._pHitPoints / 3;
	} else {
		if (!shift) {
			dam = (mind << 6) + GenerateRnd(((maxd - mind) << 6) + 1);
			if (monster == nullptr)
				if (HasAnyOf(player._pIFlags, ItemSpecialEffect::HalfTrapDamage))
					dam /= 2;
			dam += player._pIGetHit * 64;
		} else {
			dam = mind + GenerateRnd(maxd - mind + 1);
			if (monster == nullptr)
				if (HasAnyOf(player._pIFlags, ItemSpecialEffect::HalfTrapDamage))
					dam /= 2;
			// Vanilla's 64th of a point, kept on purpose: this path is the per-TICK damage of fire walls, acid puddles and
			// Inferno, and a whole point a tick floored every tick at 1 life under -3, or added 6 life a tick under Fool's
			// Crest (round 23 audit of v1.12.247, which had made it whole points).
			dam += player._pIGetHit;
		}

		dam = std::max(dam, 64);
	}

	if ((resper <= 0 || gbIsHellfire) && blk < blkper) {
		Direction dir = player._pdir;
		if (monster != nullptr) {
			dir = GetDirection(player.position.tile, monster->position.tile);
		}
		*blocked = true;
		StartPlrBlock(player, dir);
		return true;
	}

	if (&player == MyPlayer) {
		// The name the player saw - a champion's own, with its variant - as the melee path says it (round 11 audit).
		oracool::NotePendingDeathSource(monster != nullptr ? oracool::GetMonsterDisplayName(*monster) : std::string("a trap"));
	}

	// Cold slows (user, 2026-09-07: "curses and cold spells decrease it"): a cold hit that LANDS - past
	// the to-hit and the block above - puts a chill on the stride, a quarter off for three seconds.
	// The sheet's Move speed row turns red for it. Every cold missile that reaches a player comes
	// through here, so whatever casts cold at players from now on slows them without another line.
	// Since 2026-09-26 the Diablo II chill: half speed on the walk and on every action, shortened by cold
	// resistance (oracool::ChillPlayer). It was a quarter off the walk alone.
	if (damageType == DamageType::Cold)
		oracool::ChillPlayer(player);

	// A NEGATIVE resistance amplifies the hit (D2 rules, 2026-09-13) - a hero with no resistance gear on
	// Torment stands at -90 and takes 190%. Applied HERE and then left to the ordinary hit below, not
	// folded into the resisted branch: that branch is a soft landing (ArghClang, no hit recovery), and a
	// hero with negative resistance must still be staggered by the harder blow.
	// Venom takes the blow before resistance, as melee does: PoisonPlayer resists it itself, and a resisted shot was
	// resisted twice (round 28 audit).
	const int poisonBase = dam;
	if (resper < 0)
		dam -= dam * resper / 100;

	if (resper > 0) {
		dam -= dam * resper / 100;
		if (&player == MyPlayer) {
			ApplyPlrDamage(damageType, player, 0, 0, dam, deathReason);
			if (monster != nullptr) {
				if ((player._pHitPoints >> 6) > 0) // not from the corpse (round 38 audit)
					oracool::OnRfa12MissileStruck(player, *monster, dam); // Feedback
				OnMonsterMissileLanded(player, *monster, dam, poisonBase);
			}
		}

		if (player._pHitPoints >> 6 > 0) {
			player.Say(HeroSpeech::ArghClang);
		}
		return true;
	}

	if (&player == MyPlayer) {
		ApplyPlrDamage(damageType, player, 0, 0, dam, deathReason);
		if (monster != nullptr) {
			if ((player._pHitPoints >> 6) > 0) // not from the corpse (round 38 audit)
				oracool::OnRfa12MissileStruck(player, *monster, dam); // Feedback
			OnMonsterMissileLanded(player, *monster, dam, poisonBase);
		}
	}

	if (player._pHitPoints >> 6 > 0) {
		StartPlrHit(player, dam, false);
	}

	return true;
}

void SetMissDir(Missile &missile, int dir)
{
	missile._mimfnum = dir;
	SetMissAnim(missile, missile._miAnimType);
}

namespace {

/**
 * @brief The scaled sheets, one per graphic, facing and size, built from the loaded sheet the first time a missile asks.
 * Each owns its pixels, so a level's FreeMissileGFX leaves it whole; a true-colour sheet's indices still read the same
 * colours when it loads again.
 */
std::unordered_map<uint32_t, OwnedClxSpriteList> ScaledMissileSprites;

} // namespace

void ScaleMissile(Missile &missile, int percent, int floor)
{
	// The scaler's own range (ScaleClxList clamps to 25-400%), so the lift below is worked from the size actually drawn
	// (audit, 2026-09-29).
	missile.oracoolScalePercent = static_cast<uint16_t>(std::clamp(percent, 25, 400));
	missile.oracoolScaleFloor = static_cast<int16_t>(floor);
	missile.oracoolScaleLift = 0;
	if (HeadlessMode || missile.oracoolScalePercent == 100 || missile._miAnimType == MissileGraphicID::None)
		return;
	const MissileFileData &data = GetMissileSpriteData(missile._miAnimType);
	// The row SetMissAnim drew: a facing past the sheet's rows is its first, and a one-row sheet has only the one - so its
	// scaled copy is cached once, not once per facing (audit, 2026-09-29).
	const size_t facing = data.animFAmt > 1 && missile._mimfnum >= 0 && missile._mimfnum < data.animFAmt ? static_cast<size_t>(missile._mimfnum) : 0;
	const OptionalClxSpriteList sheet = data.spritesForDirection(facing);
	// Only the missile's own sheet: one wearing borrowed sprites (an item tumble) is left as it is.
	if (!sheet || !missile._miAnimData || (*missile._miAnimData)[0].width() != (*sheet)[0].width())
		return;
	const uint32_t key = (static_cast<uint32_t>(missile._miAnimType) << 20) | (static_cast<uint32_t>(facing & 0xFF) << 12) | missile.oracoolScalePercent;
	auto it = ScaledMissileSprites.find(key);
	if (it == ScaledMissileSprites.end())
		it = ScaledMissileSprites.emplace(key, oracool::ScaleClxList(*sheet, missile.oracoolScalePercent)).first;
	const ClxSpriteList scaled { it->second };
	const int fullHeight = (*sheet)[0].height();
	missile._miAnimData = scaled;
	missile._miAnimWidth = scaled[0].width();
	missile._miAnimWidth2 = CalculateWidth2(missile._miAnimWidth);
	// A sprite hangs from its tile by its bottom edge. Kept centre: lifted by half the height it lost (or lowered by half
	// what it gained). Kept floor point: lifted by the part of that point's height above the bottom it lost.
	missile.oracoolScaleLift = static_cast<int16_t>(floor < 0 ? (fullHeight - static_cast<int>(scaled[0].height())) / 2
	                                                          : floor * (100 - missile.oracoolScalePercent) / 100);
}

void InitMissiles(bool keepHeroTimedSpells)
{
	Player &myPlayer = *MyPlayer;

	// Infravision, Etherealize and Search keep their clock on the missile that carries them, and the clear
	// below deleted every missile on every level entry - so a stair ended each of them at once, whatever
	// time was left. Kept by value and put back after the clear, in single-player and only for a level
	// change inside a running game (the caller decides): a loaded save or a new character starts clean.
	// All three are invisible effects on the caster - no tile, no light, no dFlags mark - so nothing about
	// the level they were cast on travels with them.
	std::vector<Missile> carried;
	if (keepHeroTimedSpells && !gbIsMultiplayer) {
		for (Missile &missile : Missiles) {
			if (missile._miDelFlag || missile._mirange <= 0 || missile.sourcePlayer() != &myPlayer)
				continue;
			// ...and the cold armours (audit, 2026-09-29): their clock ticks on this missile too, so a stair left the armour
			// on the hero for good - frozen attackers, Chilling Armor's bolts and the tint forever.
			if (IsAnyOf(missile._mitype, MissileID::Infravision, MissileID::Etherealize, MissileID::Search, MissileID::ColdArmor))
				carried.push_back(missile);
		}
	}

	AutoMapShowItems = false;
	myPlayer._pSpellFlags &= ~SpellFlag::Etherealize;
	if (myPlayer._pInfraFlag) {
		for (auto &missile : Missiles) {
			if (missile._mitype == MissileID::Infravision) {
				if (missile.sourcePlayer() == MyPlayer)
					CalcPlrItemVals(myPlayer, true);
			}
		}
	}

	if (HasAnyOf(myPlayer._pSpellFlags, SpellFlag::RageActive | SpellFlag::RageCooldown)) {
		myPlayer._pSpellFlags &= ~SpellFlag::RageActive;
		myPlayer._pSpellFlags &= ~SpellFlag::RageCooldown;
		for (auto &missile : Missiles) {
			if (missile._mitype == MissileID::Rage) {
				if (missile.sourcePlayer() == MyPlayer) {
					int missingHP = myPlayer._pMaxHP - myPlayer._pHitPoints;
					CalcPlrItemVals(myPlayer, true);
					ApplyPlrDamage(DamageType::Physical, myPlayer, 0, 1, missingHP + missile.var2);
				}
			}
		}
	}

	Missiles.clear();
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) { // NOLINT(modernize-loop-convert)
			dFlags[i][j] &= ~(DungeonFlag::Missile | DungeonFlag::MissileFireWall | DungeonFlag::MissileLightningWall);
		}
	}

	// The kept effects go back with their time left, and their flags are restored now rather than on the
	// next tick, so the new level's first frame is already drawn with the sight or the ghost form on.
	for (const Missile &missile : carried) {
		Missiles.push_back(missile);
		if (missile._mitype == MissileID::Infravision)
			myPlayer._pInfraFlag = true;
		else if (missile._mitype == MissileID::Etherealize)
			myPlayer._pSpellFlags |= SpellFlag::Etherealize;
		else if (missile._mitype == MissileID::Search)
			AutoMapShowItems = true;
	}
}

void AddOpenNest(Missile &missile, AddMissileParameter &parameter)
{
	for (int x : { 80, 81 }) {
		for (int y : { 62, 63 }) {
			AddMissile({ x, y }, { 80, 62 }, parameter.midir, MissileID::BigExplosion, missile._micaster, missile._misource, missile._midam, 0);
		}
	}
	missile._miDelFlag = true;
}

void AddRuneOfFire(Missile &missile, AddMissileParameter &parameter)
{
	AddRune(missile, parameter.dst, MissileID::BigExplosion);
}

void AddRuneOfLight(Missile &missile, AddMissileParameter &parameter)
{
	int lvl = (missile.sourceType() == MissileSource::Player) ? missile.sourcePlayer()->_pLevel : 0;
	int dmg = 16 * (GenerateRndSum(10, 2) + lvl + 2);
	missile._midam = dmg;
	AddRune(missile, parameter.dst, MissileID::LightningWall);
}

void AddRuneOfNova(Missile &missile, AddMissileParameter &parameter)
{
	AddRune(missile, parameter.dst, MissileID::Nova);
}

void AddRuneOfImmolation(Missile &missile, AddMissileParameter &parameter)
{
	AddRune(missile, parameter.dst, MissileID::Immolation);
}

void AddRuneOfStone(Missile &missile, AddMissileParameter &parameter)
{
	AddRune(missile, parameter.dst, MissileID::StoneCurse);
}

void AddReflect(Missile &missile, AddMissileParameter & /*parameter*/)
{
	missile._miDelFlag = true;

	if (missile.sourceType() != MissileSource::Player)
		return;

	Player &player = *missile.sourcePlayer();

	int add = ReflectCharges(missile._mispllvl, player._pLevel);
	if (player.wReflections + add >= std::numeric_limits<uint16_t>::max())
		add = 0;
	player.wReflections += add;
	if (&player == MyPlayer)
		NetSendCmdParam1(true, CMD_SETREFLECT, player.wReflections);
}

void AddBerserk(Missile &missile, AddMissileParameter &parameter)
{
	missile._miDelFlag = true;
	parameter.spellFizzled = true;

	// Oracool: town's dMonster holds towner ids, so a town cast would berserk whatever sits in that Monsters slot.
	if (missile.sourceType() == MissileSource::Trap || leveltype == DTYPE_TOWN)
		return;

	std::optional<Point> targetMonsterPosition = FindClosestValidPosition(
	    [](Point target) {
		    if (!InDungeonBounds(target)) {
			    return false;
		    }

		    int monsterId = abs(dMonster[target.x][target.y]) - 1;
		    if (monsterId < 0)
			    return false;

		    const Monster &monster = Monsters[monsterId];
		    if (monster.isPlayerMinion())
			    return false;
		    if ((monster.flags & MFLAG_BERSERK) != 0)
			    return false;
		    if (monster.isUnique() || monster.ai == MonsterAIID::Diablo)
			    return false;
		    if (IsAnyOf(monster.mode, MonsterMode::FadeIn, MonsterMode::FadeOut, MonsterMode::Charge))
			    return false;
		    if ((monster.resistance & IMMUNE_MAGIC) != 0)
			    return false;
		    if ((monster.resistance & RESIST_MAGIC) != 0 && ((monster.resistance & RESIST_MAGIC) != 1 || !FlipCoin()))
			    return false;

		    return true;
	    },
	    parameter.dst, 0, 5);

	if (targetMonsterPosition) {
		auto &monster = Monsters[abs(dMonster[targetMonsterPosition->x][targetMonsterPosition->y]) - 1];
		Player &player = *missile.sourcePlayer();
		const int slvl = BerserkDamageBonus(player.GetSpellLevel(SpellID::Berserk));
		monster.flags |= MFLAG_BERSERK | MFLAG_GOLEM;
		// Clamped to the byte (round 30 audit): Hell and Torment damage over 212 wrapped to near nothing, and the minimum
		// could land above the maximum. Same draws, same order.
		const auto berserked = [](int percent, int value, int bonus) {
			return static_cast<uint8_t>(std::min(percent * value / 100 + bonus, 255));
		};
		monster.minDamage = berserked(GenerateRnd(10) + 120, monster.minDamage, slvl);
		monster.maxDamage = berserked(GenerateRnd(10) + 120, monster.maxDamage, slvl);
		monster.minDamageSpecial = berserked(GenerateRnd(10) + 120, monster.minDamageSpecial, slvl);
		monster.maxDamageSpecial = berserked(GenerateRnd(10) + 120, monster.maxDamageSpecial, slvl);
		monster.maxDamage = std::max(monster.maxDamage, monster.minDamage);
		monster.maxDamageSpecial = std::max(monster.maxDamageSpecial, monster.minDamageSpecial);
		int lightRadius = leveltype == DTYPE_NEST ? 9 : 3;
		// A Luminous monster already carries a light: resized, not orphaned (round 9 audit).
		if (monster.lightId != NO_LIGHT)
			ChangeLightRadius(monster.lightId, lightRadius);
		else
			monster.lightId = AddLight(monster.position.tile, lightRadius);
		parameter.spellFizzled = false;
	}
}

void AddHorkSpawn(Missile &missile, AddMissileParameter &parameter)
{
	UpdateMissileVelocity(missile, parameter.dst, 8);
	missile._mirange = 9;
	missile.var1 = static_cast<int32_t>(parameter.midir);
	PutMissile(missile);
}

void AddJester(Missile &missile, AddMissileParameter &parameter)
{
	MissileID spell = MissileID::Firebolt;
	switch (GenerateRnd(10)) {
	case 0:
	case 1:
		spell = MissileID::Firebolt;
		break;
	case 2:
		spell = MissileID::Fireball;
		break;
	case 3:
		spell = MissileID::FireWallControl;
		break;
	case 4:
		spell = MissileID::Guardian;
		break;
	case 5:
		spell = MissileID::ChainLightning;
		break;
	case 6:
		spell = MissileID::TownPortal;
		break;
	case 7:
		spell = MissileID::Teleport;
		break;
	case 8:
		spell = MissileID::Apocalypse;
		break;
	case 9:
		spell = MissileID::StoneCurse;
		break;
	}
	// Not a portal or a teleport in town: the town branch of AddTownPortal opened one to the LAST portal's destination
	// (Portals[] keeps it after closing) - floor 15, a rift (round 9 audit, v1.12.234).
	if (leveltype == DTYPE_TOWN && IsAnyOf(spell, MissileID::TownPortal, MissileID::Teleport))
		spell = MissileID::Firebolt;
	Missile *randomMissile = AddMissile(missile.position.start, parameter.dst, parameter.midir, spell, missile._micaster, missile._misource, 0, missile._mispllvl);
	parameter.spellFizzled = randomMissile == nullptr;
	missile._miDelFlag = true;
}

void AddStealPotions(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Crawl(0, 2, [&](Displacement displacement) {
		Point target = missile.position.start + displacement;
		if (!InDungeonBounds(target))
			return false;
		int8_t pnum = dPlayer[target.x][target.y];
		if (pnum == 0)
			return false;
		Player &player = Players[abs(pnum) - 1];

		bool hasPlayedSFX = false;
		// Placed after the loop, not in it (audit, 2026-09-27): a downgraded unit put back into the belt could land in a
		// later slot, which this same loop then visited and could steal from or downgrade a second time.
		std::vector<Item> downgradedUnits;
		for (int si = 0; si < MaxBeltItems; si++) {
			Item &beltItem = player.SpdList[si];
			_item_indexes ii = IDI_NONE;
			if (beltItem._itype == ItemType::Misc) {
				if (FlipCoin())
					continue;
				switch (beltItem._iMiscId) {
				case IMISC_FULLHEAL:
					ii = ItemMiscIdIdx(IMISC_HEAL);
					break;
				case IMISC_HEAL:
				case IMISC_MANA:
					// A stolen stack only loses one unit, not the whole stack.
					if (beltItem.isStackableConsumable() && beltItem.stackCount() > 1)
						beltItem.setStackCount(beltItem.stackCount() - 1);
					else
						player.RemoveSpdBarItem(si);
					break;
				case IMISC_FULLMANA:
					ii = ItemMiscIdIdx(IMISC_MANA);
					break;
				case IMISC_REJUV:
					ii = ItemMiscIdIdx(PickRandomlyAmong({ IMISC_HEAL, IMISC_MANA }));
					break;
				case IMISC_FULLREJUV:
					switch (GenerateRnd(3)) {
					case 0:
						ii = ItemMiscIdIdx(IMISC_FULLMANA);
						break;
					case 1:
						ii = ItemMiscIdIdx(IMISC_FULLHEAL);
						break;
					default:
						ii = ItemMiscIdIdx(IMISC_REJUV);
						break;
					}
					break;
				default:
					continue;
				}
			}
			if (ii != IDI_NONE) {
				if (beltItem.isStackableConsumable() && beltItem.stackCount() > 1) {
					// Only downgrade a single split-off unit; the remaining stack in
					// this belt slot must not be collapsed by InitializeItem's reset.
					beltItem.setStackCount(beltItem.stackCount() - 1);
					Item downgraded;
					auto seed = beltItem._iSeed;
					InitializeItem(downgraded, ii);
					downgraded._iSeed = seed;
					downgraded._iStatFlag = true;
					// Best-effort: merges into a matching belt stack or an empty slot;
					// if there's no room the downgraded unit is simply lost, matching
					// existing steal semantics elsewhere in this function.
					downgradedUnits.push_back(downgraded);
				} else {
					auto seed = beltItem._iSeed;
					InitializeItem(beltItem, ii);
					beltItem._iSeed = seed;
					beltItem._iStatFlag = true;
				}
			}
			if (!hasPlayedSFX) {
				PlaySfxLoc(IS_POPPOP2, target);
				hasPlayedSFX = true;
			}
		}
		for (const Item &unit : downgradedUnits)
			AutoPlaceItemInBelt(player, unit, true);
		player.CalcScrolls();
		RedrawEverything();

		return false;
	});
	missile._miDelFlag = true;
}

void AddStealMana(Missile &missile, AddMissileParameter & /*parameter*/)
{
	std::optional<Point> trappedPlayerPosition = FindClosestValidPosition(
	    [](Point target) {
		    return InDungeonBounds(target) && dPlayer[target.x][target.y] != 0;
	    },
	    missile.position.start, 0, 2);

	if (trappedPlayerPosition) {
		Player &player = Players[abs(dPlayer[trappedPlayerPosition->x][trappedPlayerPosition->y]) - 1];

		player._pMana = 0;
		player._pManaBase = player._pMana + player._pMaxManaBase - player._pMaxMana;
		CalcPlrInv(player, false);
		RedrawComponent(PanelDrawComponent::Mana);
		PlaySfxLoc(TSFX_COW7, *trappedPlayerPosition);
	}

	missile._miDelFlag = true;
}

void AddSpectralArrow(Missile &missile, AddMissileParameter &parameter)
{
	int av = 0;

	if (missile.sourceType() == MissileSource::Player) {
		const Player &player = *missile.sourcePlayer();

		if (player._pClass == HeroClass::Rogue)
			av += (player._pLevel - 1) / 4;
		else if (player._pClass == HeroClass::Warrior || player._pClass == HeroClass::Bard)
			av += (player._pLevel - 1) / 8;

		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::QuickAttack))
			av++;
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastAttack))
			av += 2;
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FasterAttack))
			av += 4;
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastestAttack))
			av += 8;
	}

	missile._mirange = 1;
	missile.var1 = parameter.dst.x;
	missile.var2 = parameter.dst.y;
	missile.var3 = av;
}

void AddWarp(Missile &missile, AddMissileParameter &parameter)
{
	// Oracool: every spell is castable in town, and the trigger offsets below end in app_fatal there.
	if (leveltype == DTYPE_TOWN) {
		missile._miDelFlag = true;
		parameter.spellFizzled = true;
		return;
	}

	int minDistanceSq = std::numeric_limits<int>::max();

	int id = missile._misource;
	Player &player = Players[id];
	Point tile = player.position.tile;

	for (int i = 0; i < numtrigs && i < MAXTRIGGERS; i++) {
		TriggerStruct *trg = &trigs[i];
		if (IsNoneOf(trg->_tmsg, WM_DIABTWARPUP, WM_DIABPREVLVL, WM_DIABNEXTLVL, WM_DIABRTNLVL))
			continue;
		Point candidate = trg->position;
		auto getTriggerOffset = [](TriggerStruct *trg) {
			switch (leveltype) {
			case DTYPE_CATHEDRAL:
				if (setlevel && setlvlnum == SL_VILEBETRAYER)
					return Displacement { 1, 1 }; // Portal
				if (IsAnyOf(trg->_tmsg, WM_DIABTWARPUP, WM_DIABPREVLVL, WM_DIABRTNLVL))
					return Displacement { 1, 2 };
				return Displacement { 0, 1 }; // WM_DIABNEXTLVL
			case DTYPE_CATACOMBS:
				if (IsAnyOf(trg->_tmsg, WM_DIABTWARPUP, WM_DIABPREVLVL))
					return Displacement { 1, 1 };
				return Displacement { 0, 1 }; // WM_DIABRTNLVL, WM_DIABNEXTLVL
			case DTYPE_CAVES:
				if (IsAnyOf(trg->_tmsg, WM_DIABTWARPUP, WM_DIABPREVLVL))
					return Displacement { 0, 1 };
				return Displacement { 1, 0 }; // WM_DIABRTNLVL, WM_DIABNEXTLVL
			case DTYPE_HELL:
				return Displacement { 1, 0 };
			case DTYPE_NEST:
				if (IsAnyOf(trg->_tmsg, WM_DIABTWARPUP, WM_DIABPREVLVL, WM_DIABRTNLVL))
					return Displacement { 0, 1 };
				return Displacement { 1, 0 }; // WM_DIABNEXTLVL
			case DTYPE_CRYPT:
				if (IsAnyOf(trg->_tmsg, WM_DIABTWARPUP, WM_DIABPREVLVL, WM_DIABRTNLVL))
					return Displacement { 1, 1 };
				return Displacement { 0, 1 }; // WM_DIABNEXTLVL
			case DTYPE_TOWN:
				app_fatal("invalid leveltype: DTYPE_TOWN");
			case DTYPE_NONE:
				app_fatal("leveltype not set");
			}
			app_fatal(StrCat("invalid leveltype", static_cast<int>(leveltype)));
		};
		const Displacement triggerOffset = getTriggerOffset(trg);
		candidate += triggerOffset;
		const Displacement off = Point { player.position.tile } - candidate;
		const int distanceSq = off.deltaY * off.deltaY + off.deltaX * off.deltaX;
		if (distanceSq < minDistanceSq) {
			minDistanceSq = distanceSq;
			tile = candidate;
		}
	}
	missile._mirange = 2;
	std::optional<Point> teleportDestination = FindClosestValidPosition(
	    [&player](Point target) {
		    for (int i = 0; i < numtrigs; i++) {
			    if (trigs[i].position == target)
				    return false;
		    }
		    return PosOkPlayer(player, target);
	    },
	    tile, 0, 5);

	if (teleportDestination) {
		missile.position.tile = *teleportDestination;
	} else {
		// No valid teleport destination found
		missile._miDelFlag = true;
		parameter.spellFizzled = true;
	}
}

void AddLightningWall(Missile &missile, AddMissileParameter &parameter)
{
	UpdateMissileVelocity(missile, parameter.dst, 16);
	missile._miAnimFrame = GenerateRnd(8) + 1;
	missile._mirange = LightningWallDurationTicks(missile._mispllvl);
	switch (missile.sourceType()) {
	case MissileSource::Trap:
		missile.var1 = missile.position.start.x;
		missile.var2 = missile.position.start.y;
		break;
	case MissileSource::Player: {
		Player &player = *missile.sourcePlayer();
		missile.var1 = player.position.tile.x;
		missile.var2 = player.position.tile.y;
	} break;
	case MissileSource::Monster:
		assert(missile.sourceType() != MissileSource::Monster);
		break;
	}
}

void AddBigExplosion(Missile &missile, AddMissileParameter & /*parameter*/)
{
	if (missile.sourceType() == MissileSource::Player) {
		int dmg = 2 * (missile.sourcePlayer()->_pLevel + GenerateRndSum(10, 2)) + 4;
		dmg = ScaleSpellEffect(dmg, missile._mispllvl);

		missile._midam = dmg;

		const DamageType damageType = GetMissileData(missile._mitype).damageType();
		for (Point position : PointsInRectangleColMajor(Rectangle { missile.position.tile, 1 }))
			CheckMissileCol(missile, damageType, dmg, dmg, false, position, true);
	}
	missile._mlid = AddLight(missile.position.start, 8);
	SetMissDir(missile, 0);
	missile._mirange = missile._miAnimLen - 1;
}

void AddImmolation(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == parameter.dst) {
		dst += parameter.midir;
	}
	int sp = 16;
	if (missile._micaster == TARGET_MONSTERS) {
		sp += std::min(missile._mispllvl, 34);
	}
	UpdateMissileVelocity(missile, dst, sp);
	SetMissDir(missile, GetDirection16(missile.position.start, dst));
	missile._mirange = 256;
	missile._mlid = AddLight(missile.position.start, 8);
}

void AddLightningBow(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == parameter.dst) {
		dst += parameter.midir;
	}
	UpdateMissileVelocity(missile, dst, 32);
	missile._miAnimFrame = GenerateRnd(8) + 1;
	missile._mirange = 255;
	if (missile._misource < 0) {
		missile.var1 = missile.position.start.x;
		missile.var2 = missile.position.start.y;
	} else {
		missile.var1 = Players[missile._misource].position.tile.x;
		missile.var2 = Players[missile._misource].position.tile.y;
	}
	missile._midam <<= 6;
}

void AddMana(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	int manaAmount = (GenerateRnd(ManaSpellBaseDie) + 1) << 6;
	for (int i = 0; i < player._pLevel; i++) {
		manaAmount += (GenerateRnd(ManaSpellCharacterLevelDie) + 1) << 6;
	}
	for (int i = 0; i < missile._mispllvl; i++) {
		manaAmount += (GenerateRnd(ManaSpellSpellLevelDie) + 1) << 6;
	}
	manaAmount = ManaSpellClassAmount(player._pClass, manaAmount);
	player._pMana += manaAmount;
	if (player._pMana > player._pMaxMana)
		player._pMana = player._pMaxMana;
	player._pManaBase += manaAmount;
	if (player._pManaBase > player._pMaxManaBase)
		player._pManaBase = player._pMaxManaBase;
	missile._miDelFlag = true;
	RedrawComponent(PanelDrawComponent::Mana);
}

void AddMagi(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	player._pMana = player._pMaxMana;
	player._pManaBase = player._pMaxManaBase;
	missile._miDelFlag = true;
	RedrawComponent(PanelDrawComponent::Mana);
}

void AddRingOfFire(Missile &missile, AddMissileParameter & /*parameter*/)
{
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	missile._mirange = 7;
}

void AddSearch(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	if (&player == MyPlayer)
		AutoMapShowItems = true;
	// A source-less Search counts as character level 1 (vanilla's `lvl = 2`).
	missile._mirange = SearchDurationTicks(missile._mispllvl, missile._misource >= 0 ? static_cast<int>(player._pLevel) : 1);

	for (auto &other : Missiles) {
		if (&other != &missile && missile.isSameSource(other) && other._mitype == MissileID::Search) {
			int r1 = missile._mirange;
			int r2 = other._mirange;
			if (r2 < INT_MAX - r1)
				other._mirange = r1 + r2;
			missile._miDelFlag = true;
			break;
		}
	}
}

void AddChargedBoltBow(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	missile._mirnd = GenerateRnd(15) + 1;
	if (missile._micaster != TARGET_MONSTERS) {
		missile._midam = 15;
	}

	if (missile.position.start == dst) {
		dst += parameter.midir;
	}
	missile._miAnimFrame = GenerateRnd(8) + 1;
	missile._mlid = AddLight(missile.position.start, 5);
	UpdateMissileVelocity(missile, dst, 8);
	missile.var1 = 5;
	missile.var2 = static_cast<int32_t>(parameter.midir);
	missile._mirange = 256;
}

void AddElementalArrow(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == dst) {
		dst += parameter.midir;
	}
	int av = 32;
	if (missile._micaster == TARGET_MONSTERS) {
		const Player &player = Players[missile._misource];
		if (player._pClass == HeroClass::Rogue)
			av += (player._pLevel) / 4;
		else if (IsAnyOf(player._pClass, HeroClass::Warrior, HeroClass::Bard))
			av += (player._pLevel) / 8;

		if (gbIsHellfire) {
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::QuickAttack))
				av++;
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastAttack))
				av += 2;
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FasterAttack))
				av += 4;
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastestAttack))
				av += 8;
		} else {
			if (IsAnyOf(player._pClass, HeroClass::Rogue, HeroClass::Warrior, HeroClass::Bard))
				av -= 1;
		}
	}
	UpdateMissileVelocity(missile, dst, av);

	SetMissDir(missile, GetDirection16(missile.position.start, dst));
	missile._mirange = 256;
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	missile._mlid = AddLight(missile.position.start, 5);
}

void AddArrow(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == dst) {
		dst += parameter.midir;
	}
	int av = 32;
	if (missile._micaster == TARGET_MONSTERS) {
		const Player &player = Players[missile._misource];

		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::RandomArrowVelocity)) {
			av = GenerateRnd(32) + 16;
		}
		if (player._pClass == HeroClass::Rogue)
			av += (player._pLevel - 1) / 4;
		else if (player._pClass == HeroClass::Warrior || player._pClass == HeroClass::Bard)
			av += (player._pLevel - 1) / 8;

		if (gbIsHellfire) {
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::QuickAttack))
				av++;
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastAttack))
				av += 2;
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FasterAttack))
				av += 4;
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastestAttack))
				av += 8;
		}
	}
	UpdateMissileVelocity(missile, dst, av);
	missile._miAnimFrame = static_cast<int>(GetDirection16(missile.position.start, dst)) + 1;
	missile._mirange = 256;
}

void UpdateVileMissPos(Missile &missile, Point dst)
{
	for (int k = 1; k < 50; k++) {
		for (int j = -k; j <= k; j++) {
			int yy = j + dst.y;
			for (int i = -k; i <= k; i++) {
				int xx = i + dst.x;
				if (PosOkPlayer(*MyPlayer, { xx, yy })) {
					missile.position.tile = { xx, yy };
					return;
				}
			}
		}
	}
}

void AddPhasing(Missile &missile, AddMissileParameter &parameter)
{
	missile._mirange = 2;

	Player &player = Players[missile._misource];

	if (missile._micaster == TARGET_BOTH) {
		missile.position.tile = parameter.dst;
		if (!PosOkPlayer(player, parameter.dst))
			UpdateVileMissPos(missile, parameter.dst);
		return;
	}

	std::array<Point, 4 * 9> targets;

	int count = 0;
	for (int y = -6; y <= 6; y++) {
		for (int x = -6; x <= 6; x++) {
			if ((x >= -3 && x <= 3) || (y >= -3 && y <= 3))
				continue; // Skip center

			Point target = missile.position.start + Displacement { x, y };
			if (!PosOkPlayer(player, target))
				continue;

			targets[count] = target;
			count++;
		}
	}

	if (count == 0) {
		missile._miDelFlag = true;
		parameter.spellFizzled = true; // nothing happened, so nothing is paid (round 16 audit)
		return;
	}

	missile.position.tile = targets[std::max<int32_t>(GenerateRnd(count), 0)];
}

void AddFirebolt(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == dst) {
		dst += parameter.midir;
	}
	int sp = 26;
	if (missile._micaster == TARGET_MONSTERS) {
		sp = 16;
		if (!missile.IsTrap()) {
			sp += std::min(missile._mispllvl * 2, 47);
		}
	}
	UpdateMissileVelocity(missile, dst, sp);
	SetMissDir(missile, GetDirection16(missile.position.start, dst));
	missile._mirange = 256;
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	missile._mlid = AddLight(missile.position.start, 8);
	if (missile._midam == 0) {
		switch (missile.sourceType()) {
		case MissileSource::Player: {
			const Player &player = *missile.sourcePlayer();
			if (missile._mitype == MissileID::IceBolt) {
				// Oracool (2026-09-26): an ice bolt - cast, shed by Frozen Orb, or thrown back by
				// Chilling Armor - rolls from ColdSpellDamage, the function the sheet quotes. Without
				// Cold Mastery it is this same roll (magic/8 + level + 1, plus 0-9); with it, the
				// mastery's percentage now reaches the bolt as its description and the sheet promise.
				int minDamage;
				int maxDamage;
				oracool::ColdSpellDamage(player, SpellID::IceBolt, missile._mispllvl, minDamage, maxDamage);
				missile._midam = minDamage + GenerateRnd(maxDamage - minDamage + 1);
			} else {
				missile._midam = GenerateRnd(10) + (player._pMagic / 8) + missile._mispllvl + 1;
			}
		} break;

		case MissileSource::Monster:
			missile._midam = ProjectileMonsterDamage(missile);
			break;
		case MissileSource::Trap:
			missile._midam = ProjectileTrapDamage(missile);
			break;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Oracool, Round 2 of the inert-skill plan (2026-09-03): the Sorceress's cold line.
//
// The damage of every one of these comes from oracool::ColdSpellDamage, which the Abilities window
// reads for its tooltip, so the number on the sheet IS the number that lands. What the hit does to
// the monster - chill, freeze, shatter - is oracool::ApplyColdHit, called by MonsterMHit for every
// cold missile, so it is not repeated here.
// ---------------------------------------------------------------------------------------------

namespace {

/** @brief Rolls @p spell's damage at the missile's own level into _midam, for a player's cast. */
void RollColdDamage(Missile &missile, SpellID spell)
{
	if (missile.sourceType() != MissileSource::Player) {
		missile._midam = ProjectileMonsterDamage(missile);
		return;
	}
	int minDamage;
	int maxDamage;
	oracool::ColdSpellDamage(*missile.sourcePlayer(), spell, missile._mispllvl, minDamage, maxDamage);
	missile._midam = minDamage + GenerateRnd(maxDamage - minDamage + 1);
}

/** @brief Firebolt's launch - aim, speed, light - for a cold projectile of speed @p speed. */
void LaunchColdProjectile(Missile &missile, AddMissileParameter &parameter, int speed)
{
	Point dst = parameter.dst;
	if (missile.position.start == dst)
		dst += parameter.midir;
	UpdateMissileVelocity(missile, dst, speed);
	SetMissDir(missile, GetDirection16(missile.position.start, dst));
	missile._mirange = 256;
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	missile._mlid = AddLight(missile.position.start, 8);
}

} // namespace

/**
 * @brief Ice Blast: heavier and a little slower than Ice Bolt, and it FREEZES what it hits.
 *
 * The freeze is ApplyColdHit's answer to MissileID::IceBlast; here it is only a bolt with a bigger
 * number. Speed 14 to Ice Bolt's 16-plus-level, so the two read as different weights in flight.
 */
void AddIceBlast(Missile &missile, AddMissileParameter &parameter)
{
	LaunchColdProjectile(missile, parameter, 14 + std::min(missile._mispllvl, 12));
	RollColdDamage(missile, SpellID::IceBlast);
}

/**
 * @brief Glacial Spike: the heaviest single projectile in the set. Freezes its target and, when it
 * ends, shatters (AddGlacialShatter) to chill everything beside where it broke.
 */
void AddGlacialSpike(Missile &missile, AddMissileParameter &parameter)
{
	LaunchColdProjectile(missile, parameter, 12 + std::min(missile._mispllvl, 10));
	RollColdDamage(missile, SpellID::GlacialSpike);
	ScaleMissile(missile, 75); // a step down (dev note, 2026-09-30)
}

/**
 * @brief The spike breaking apart: the explosion sprite where it stopped, and a chill on every
 * monster in the eight tiles around it. The direct target was already frozen by the hit.
 */
void AddGlacialShatter(Missile &missile, AddMissileParameter &parameter)
{
	AddMissileExplosion(missile, parameter);
	ScaleMissile(missile, 50); // two steps down (dev note, 2026-09-30)
	const Point centre = missile.position.tile;
	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			const Point tile = centre + Displacement { dx, dy };
			if (!InDungeonBounds(tile))
				continue;
			const int mid = dMonster[tile.x][tile.y];
			if (mid == 0 || leveltype == DTYPE_TOWN) // a townsperson's id is no monster slot (round 39 audit)
				continue;
			Monster &monster = Monsters[abs(mid) - 1];
			if (monster.hitPoints >> 6 <= 0 || monster.isPlayerMinion())
				continue;
			oracool::ApplyColdHit(MissileID::FrostNova, missile._mispllvl, monster);
		}
	}
}

/**
 * @brief Frost Nova: a ring around the caster. Everything within three tiles is hit at once and the
 * 160px burst plays where the caster stands for as long as its nineteen frames last.
 *
 * One hit each rather than a ring of NovaBall projectiles: the art is a single expanding ellipse,
 * and sixteen bolts under it would have been sixteen impact sounds for one effect.
 */
namespace {

/** @brief Frost Nova's ring at this share of its sheet's size (user, 2026-09-27: "2-3x larger"; 250 until the dev note of 2026-09-30, "one step" down). */
constexpr unsigned FrostNovaPercent = 200;

/** @brief The scaled ring, built once from the loaded sheet (see BlessedShieldImpactSprites). */
std::optional<OwnedClxSpriteList> FrostNovaSprites;

/** @brief Swaps @p missile's ring for the scaled one, its centre where the sheet's own would be. */
void ScaleFrostNova(Missile &missile)
{
	if (!missile._miAnimData)
		return; // headless, or the sheet not loaded
	if (!FrostNovaSprites)
		FrostNovaSprites = oracool::ScaleClxList(*missile._miAnimData, FrostNovaPercent);
	const int fullHeight = (*missile._miAnimData)[0].height();
	const ClxSpriteList scaled { *FrostNovaSprites };
	missile._miAnimData = scaled;
	missile._miAnimLen = static_cast<int>(scaled.numSprites());
	missile._miAnimWidth = scaled[0].width();
	missile._miAnimWidth2 = CalculateWidth2(missile._miAnimWidth);
	// A sprite hangs from its tile by its bottom edge: the taller ring comes down by half the height it gained, so its
	// centre stays at the caster's chest, where the sheet was drawn to sit.
	missile.position.offset = { 0, (static_cast<int>(scaled[0].height()) - fullHeight) / 2 };
}

} // namespace

void AddFrostNova(Missile &missile, AddMissileParameter & /*parameter*/)
{
	ScaleFrostNova(missile); // keeps the sheet's colour table: scaling copies its indices
	missile._mirange = std::max<int>(missile._miAnimLen, 1);
	// White and light blue running through the ring as it spreads (dev note, 2026-10-01); ProcessFrostNova fades it.
	missile.oracoolTint = oracool::Tint::HueCycle;
	missile.oracoolTintRgb = oracool::hue::IceBlue;
	if (missile.sourceType() != MissileSource::Player)
		return;
	const Player &player = *missile.sourcePlayer();
	int minDamage;
	int maxDamage;
	oracool::ColdSpellDamage(player, SpellID::FrostNova, missile._mispllvl, minDamage, maxDamage);
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int id = ActiveMonsters[i];
		Monster &monster = Monsters[id];
		if (monster.hitPoints >> 6 <= 0 || monster.isPlayerMinion())
			continue;
		if (monster.position.tile.WalkingDistance(missile.position.tile) > 3)
			continue;
		if (!LineClearMissile(missile.position.tile, monster.position.tile))
			continue; // the ring stops at walls, as the missiles do
		// The ring has no impact sheet: each monster it catches flashes (hit_cold, 2026-09-26).
		if (MonsterMHit(missile._misource, id, minDamage, maxDamage, 0, MissileID::FrostNova, DamageType::Cold, false, missile._mispllvl))
			AddColdHitFlash(monster.position.tile, missile._misource);
	}
}

void ProcessFrostNova(Missile &missile)
{
	missile._mirange--;
	// Fading as it spreads rather than gone in one frame (dev note, 2026-10-01): solid for its first quarter, then
	// down to nothing on its last frame.
	const int length = std::max(missile._miAnimLen, 1);
	const int played = length - std::max(missile._mirange, 0);
	const int fadeFrom = length / 4;
	missile.oracoolAlpha = static_cast<uint16_t>(played <= fadeFrom ? 256 : std::max(0, 256 * (length - played) / std::max(length - fadeFrom, 1)));
	if (missile._mirange <= 0)
		missile._miDelFlag = true;
	PutMissile(missile);
}

/**
 * @brief Blizzard: an invisible controller parked over the target tile for a few seconds, dropping
 * one shard (AddBlizzardShard) every four ticks somewhere within two tiles of it.
 *
 * The damage is rolled per shard, from a range this controller carries in var3/var4 so every shard
 * asks the same table once. Three seconds plus a fifth a rank.
 */
void AddBlizzard(Missile &missile, AddMissileParameter &parameter)
{
	missile.position.tile = parameter.dst;
	missile.var1 = parameter.dst.x;
	missile.var2 = parameter.dst.y;
	missile._mirange = std::min(60 + missile._mispllvl * 4, 140);
	if (missile.sourceType() == MissileSource::Player) {
		int minDamage;
		int maxDamage;
		oracool::ColdSpellDamage(*missile.sourcePlayer(), SpellID::Blizzard, missile._mispllvl, minDamage, maxDamage);
		missile.var3 = minDamage;
		missile.var4 = maxDamage;
	} else {
		missile.var3 = missile.var4 = ProjectileMonsterDamage(missile);
	}
}

void ProcessBlizzard(Missile &missile)
{
	missile._mirange--;
	if (missile._mirange <= 0) {
		missile._miDelFlag = true;
		return;
	}
	// Three shards every four ticks (dev note, 2026-09-30: "increase number 3x"): the one on the fourth strikes as ever,
	// the two between are picture only, so the storm's damage is what it was.
	if (missile._mirange % 4 == 3)
		return;
	const bool strikes = missile._mirange % 4 == 0;
	const Point centre { missile.var1, missile.var2 };
	// Somewhere in the five-by-five, on a tile a shard can land on. A few tries rather than a
	// search: a storm that misses a tick because every roll hit a wall is still a storm.
	for (int attempt = 0; attempt < 4; attempt++) {
		const Point tile = centre + Displacement { GenerateRnd(5) - 2, GenerateRnd(5) - 2 };
		if (!InDungeonBounds(tile) || !IsTileNotSolid(tile))
			continue;
		Missile *shard = AddMissile(tile, tile, Direction::South, MissileID::BlizzardShard, missile._micaster, missile._misource, 0, missile._mispllvl, &missile);
		if (shard != nullptr && !strikes)
			shard->var5 = 1;
		break;
	}
}

/**
 * @brief One falling shard: enters at the top of its frame, strikes at frame nine, breaks. The
 * strike is the one tick it collides on; the rest is picture.
 */
void AddBlizzardShard(Missile &missile, AddMissileParameter &parameter)
{
	// Half size (dev note, 2026-09-30), its tile's centre - 16px above the sheet's foot, where it breaks - kept in place.
	ScaleMissile(missile, 50, 16);
	missile._mirange = std::max<int>(missile._miAnimLen, 1);
	missile.var5 = 0; // 1: a shard for the picture only (ProcessBlizzard)
	if (parameter.pParent != nullptr) {
		missile.var3 = parameter.pParent->var3;
		missile.var4 = parameter.pParent->var4;
	} else {
		missile.var3 = missile.var4 = 1;
	}
}

void ProcessBlizzardShard(Missile &missile)
{
	missile._mirange--;
	// Frame nine of thirteen is the strike - four ticks before the end at one frame a tick.
	if (missile._mirange == 4 && missile.var5 == 0) {
		CheckMissileCol(missile, DamageType::Cold, missile.var3, missile.var4, false, missile.position.tile, true);
		// The landing's own cue (audit 2026-09-26): CheckMissileCol sounds a missile's impact only when its
		// range hits 0, and a shard strikes with four ticks left, so no shard was ever heard. Non-spatial, the
		// local player's own storm only; snd_play_snd drops retriggers inside 80 ms, so one per shard is fine.
		if (missile.sourceType() == MissileSource::Player && static_cast<size_t>(missile._misource) == MyPlayerId)
			oracool::PlaySkillSound(oracool::ClassTreeSkill::Blizzard, oracool::SkillSoundEvent::Impact);
	}
	if (missile._mirange <= 0)
		missile._miDelFlag = true;
	PutMissile(missile);
}

/**
 * @brief Frozen Orb: drifts toward its mark at half a bolt's speed, sheds an Ice Bolt every third
 * tick in a turning direction, and bursts into eight when it ends. The bolts do the damage - each
 * one is a real Ice Bolt at the orb's own rank - which is why the orb itself deals none.
 */
void AddFrozenOrb(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == dst)
		dst += parameter.midir;
	UpdateMissileVelocity(missile, dst, 8);
	SetMissDir(missile, GetDirection16(missile.position.start, dst));
	missile._mirange = 40;
	// Half size, glinting, spinning twice as fast (dev note, 2026-09-30): every other frame of its sixteen.
	ScaleMissile(missile, 50);
	missile.oracoolTint = oracool::Tint::Glint;
	missile.oracoolTintRgb = 0;
	missile._miAnimAdd = 2;
	missile.var1 = 0; // which way the next bolt goes
	missile._mlid = AddLight(missile.position.start, 8);
	missile._midam = 0;
}

void ProcessFrozenOrb(Missile &missile)
{
	missile._mirange--;
	// The orb stops at a wall rather than passing through it: MoveMissile stops it there. Its return
	// only says whether the orb changed tile, which a half-speed orb often does not - so it is no wall test.
	MoveMissile(missile, [](Point tile) { return IsTileNotSolid(tile); }, true);
	if (missile.position.velocity == Displacement {})
		missile._mirange = 0;
	ChangeLight(missile._mlid, missile.position.tile, 8);

	const auto shed = [&missile](Direction direction) {
		const Point dst = missile.position.tile + direction;
		if (Missile *bolt = AddMissile(missile.position.tile, dst, direction, MissileID::IceBolt, missile._micaster, missile._misource, 0, missile._mispllvl, &missile); bolt != nullptr)
			bolt->oracoolImpactPercent = 50; // its splash at half (dev note, 2026-10-01)
	};

	if (missile._mirange > 0) {
		if (missile._mirange % 3 == 0) {
			shed(static_cast<Direction>(missile.var1 % 8));
			missile.var1 += 3; // three of eight a step, so successive bolts fan rather than sweep
		}
		PutMissile(missile);
		return;
	}

	for (int i = 0; i < 8; i++)
		shed(static_cast<Direction>(i));
	missile._miDelFlag = true;
	AddUnLight(missile._mlid);
}

/**
 * @brief The armours: one missile that wears the state for as long as it lasts. Which of the three
 * it is comes from the spell the caster executed - MissileID::ColdArmor serves all three.
 *
 * The state itself is oracool/cold.cpp's; this missile only ages it once a tick and stops when a
 * newer cast has replaced it (the serial), so recasting never double-ticks the same armour.
 */
void AddColdArmor(Missile &missile, AddMissileParameter & /*parameter*/)
{
	missile._miDelFlag = true;
	if (missile.sourceType() != MissileSource::Player)
		return;
	Player &player = *missile.sourcePlayer();
	SpellID spell = player.executedSpell.spellId;
	if (!oracool::IsColdArmourSpell(spell))
		spell = SpellID::FrozenArmor;
	// Twenty seconds plus four a rank.
	const int ticks = 400 + 80 * missile._mispllvl;
	oracool::CastColdArmour(player, spell, missile._mispllvl, ticks);
	missile.var1 = oracool::ColdArmourCastSerial(player);
	missile._mirange = ticks;
	missile._miDelFlag = false;
	RedrawEverything();
}

void ProcessColdArmor(Missile &missile)
{
	if (missile.sourceType() != MissileSource::Player) {
		missile._miDelFlag = true;
		return;
	}
	Player &player = *missile.sourcePlayer();
	if (missile.var1 != oracool::ColdArmourCastSerial(player)) {
		missile._miDelFlag = true; // a newer cast owns the armour now
		return;
	}
	oracool::TickColdArmour(player);
	missile._mirange--;
	if (missile._mirange <= 0 || oracool::ActiveColdArmour(player) == SpellID::Invalid) {
		missile._miDelFlag = true;
		// Its own stop cue when it wears off - nothing sounded at that moment before, so this is a
		// voice, not a second one. Not on death (the armour is cleared with the player), and not on a
		// recast, which returned above and plays the new armour's start instead.
		if (player._pHitPoints >> 6 > 0) {
			// No shell breaks since v1.12.211: the armour is a tint on the hero now, and simply fades (user, 2026-09-27).
			oracool::PlayColdArmourExpirySound(missile);
		}
		RedrawEverything();
	}
}

// ---------------------------------------------------------------------------------------------
// Oracool, Round 3 of the inert-skill plan (2026-09-03): the Rogue's bow skills.
//
// One family of missiles. The ELEMENT is the missile type's (SkillArrow, MagicArrow, FlameArrow,
// FrostArrow, GuidedArrow); the SKILL - which decides the rank bonus and what happens when the arrow
// stops - rides in var5 as a RogueArrow, put there by oracool::FireArrowSkill. The hit itself is the
// ordinary arrow hit: MonsterMHit sees the Arrow flag and rolls ranged to-hit and armour pierce
// exactly as for a plain shot, and for a cold arrow applies the chill through the same seam every
// cold missile uses.
// ---------------------------------------------------------------------------------------------

void AddRogueArrow(Missile &missile, AddMissileParameter &parameter)
{
	// Aim and speed are the plain arrow's. The engine's arrow sheet is NOT animated - its sixteen
	// frames are the sixteen facings, chosen by _miAnimFrame - while Fire Arrow's sheet and the
	// frost arrow's are sixteen facings of four frames each, chosen by SetMissDir. AddArrow and
	// AddElementalArrow each know one of those, so the family asks the right one for its sheet.
	// Magic and Guided Arrow have their own sixteen-facing sheets once delivered (2026-09-11); until
	// then they fly as the plain arrow. Chosen before the branch, because the branch reads it.
	if (missile._mitype == MissileID::MagicArrow && MissileArtLoaded(MissileGraphicID::MagicArrowLight))
		missile._miAnimType = MissileGraphicID::MagicArrowLight;
	else if (missile._mitype == MissileID::GuidedArrow && MissileArtLoaded(MissileGraphicID::GuidedArrowGold))
		missile._miAnimType = MissileGraphicID::GuidedArrowGold;
	if (missile._miAnimType == MissileGraphicID::Arrow)
		AddArrow(missile, parameter);
	else
		AddElementalArrow(missile, parameter);
	missile.var5 = static_cast<int>(oracool::RogueArrow::MagicArrow); // overwritten by the shooter; a safe default
}

void ProcessRogueArrow(Missile &missile)
{
	missile._mirange--;
	missile._midist++;

	const auto arrow = static_cast<oracool::RogueArrow>(missile.var5);
	const int level = missile._mispllvl;

	int mind = 1;
	int maxd = 1;
	const DamageType damageType = GetMissileData(missile._mitype).damageType();
	if (missile.sourceType() == MissileSource::Player) {
		// The bow's own range, plus the rank bonus for an elemental arrow - the same sum
		// RogueArrowDamage shows on the sheet, asked of the spell the skill is.
		const Player &player = *missile.sourcePlayer();
		int sheetMin;
		int sheetMax;
		oracool::RogueArrowDamage(player, oracool::RogueArrowSpell(arrow), level, sheetMin, sheetMax);
		mind = std::max(sheetMin, 1);
		maxd = std::max(sheetMax, mind);
		// A PHYSICAL arrow (Multiple Shot, Guided Arrow, Strafe) takes the +%, the flat bonus and the Strength part in
		// MonsterMHit, so it flies with the bare dice: the sheet's sum here added them twice (round 20 audit, v1.12.245).
		if (damageType == DamageType::Physical) {
			mind = std::max(player._pIMinDam, 1);
			maxd = std::max(player._pIMaxDam, mind);
		}
	}

	MoveMissileAndCheckMissileCol(missile, damageType, mind, maxd, true, false);

	// The glow AddElementalArrow lit at the bow travels with the arrow, as vanilla's fire arrow's does
	// (ProcessElementalArrow) - it stayed at the Rogue's feet until 2026-09-26. var1/var2 are the tile it last lit.
	if (missile._mlid != NO_LIGHT && missile.position.tile != Point { missile.var1, missile.var2 }) {
		missile.var1 = missile.position.tile.x;
		missile.var2 = missile.position.tile.y;
		ChangeLight(missile._mlid, missile.position.tile, 5);
	}

	if (missile._mirange == 0) {
		// An arrow a wall stopped is parked ON the wall tile: its burst, freeze and fire patch are laid from the tile before
		// it, or they reached the far side of a one-tile wall (round 22 audit, v1.12.247).
		Point at = missile.position.tile;
		// Stepped back toward the bow until off the wall (an 8-way step along a wall row can land in the same row), and a
		// door or other blocking object counts as the wall (round 23 audit of v1.12.247).
		for (int step = 0; step < 3 && at != missile.position.start && InDungeonBounds(at) && IsMissileBlockedByTile(at); step++)
			at += GetDirection(at, missile.position.start);
		const bool steppedBack = at != missile.position.tile;
		const Direction dir = static_cast<Direction>(missile._mimfnum);
		// Fire Arrow lands in the burst vanilla's fire arrow lands in (magblos), drawn and lit only: the skill's fire
		// damage is the arrow's own hit, already dealt. Exploding and Immolation Arrow keep their own endings below.
		if (arrow == oracool::RogueArrow::FireArrow && missile._mitype == MissileID::FlameArrow)
			AddMissile(at, at, dir, MissileID::MagmaBallExplosion, missile._micaster, missile._misource, 0, 0, &missile);
		switch (arrow) {
		case oracool::RogueArrow::ExplodingArrow:
			// The burst: fire damage across the eight tiles around the stop, and the magma-ball
			// explosion drawn over it. The stop tile itself was already hit by the arrow.
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					// The centre was the arrow's own hit - unless the burst stepped back off a wall, when the tile in front of it
					// took no hit yet (round 23 audit).
					if (dx == 0 && dy == 0 && !steppedBack)
						continue;
					CheckMissileCol(missile, DamageType::Fire, mind, maxd, false, at + Displacement { dx, dy }, true);
				}
			}
			AddMissile(at, at, dir, MissileID::MagmaBallExplosion, missile._micaster, missile._misource, 0, 0, &missile);
			break;
		case oracool::RogueArrow::ImmolationArrow:
			// A fire wall where it stopped, at the skill's own rank - which is what Fire Wall's own
			// duration and damage scale on.
			AddMissile(at, at, dir, MissileID::FireWall, missile._micaster, missile._misource, 0, level, &missile);
			break;
		case oracool::RogueArrow::IceArrow:
			// The freeze on whatever the arrow stopped in. A chill already landed through the hit. Not after a step back off a
			// wall: that tile's monster was one the arrow MISSED (round 26 audit).
			if (const int mid = steppedBack || leveltype == DTYPE_TOWN ? 0 : dMonster[at.x][at.y]; mid != 0) { // not a townsperson (round 39)
				Monster &monster = Monsters[abs(mid) - 1];
				// Not the hero's own side, as Freezing Arrow and Glacial Shatter ask (round 5 audit).
				if (monster.hitPoints >> 6 > 0 && !monster.isPlayerMinion() && !oracool::IsCompanion(monster))
					oracool::ApplyColdHit(MissileID::IceBlast, level, monster);
			}
			break;
		case oracool::RogueArrow::FreezingArrow:
			// Everything around the stop is frozen, and the freezing_burst sheet plays over it.
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					const Point tile = at + Displacement { dx, dy };
					if (!InDungeonBounds(tile))
						continue;
					const int mid = dMonster[tile.x][tile.y];
					if (mid == 0 || leveltype == DTYPE_TOWN) // a townsperson's id is no monster slot (round 39 audit)
						continue;
					Monster &monster = Monsters[abs(mid) - 1];
					// Nor her own Valkyrie, Decoy or a converted ally, as Ice Arrow spares them (round 20 audit).
					if (monster.hitPoints >> 6 <= 0 || monster.isPlayerMinion() || oracool::IsCompanion(monster) || oracool::IsMinion(monster)
					    || oracool::IsMonsterConverted(monster))
						continue;
					oracool::ApplyColdHit(MissileID::IceBlast, level, monster);
				}
			}
			AddMissile(at, at, dir, MissileID::FreezingBurst, missile._micaster, missile._misource, 0, level, &missile);
			break;
		default:
			break;
		}
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	}
	PutMissile(missile);
}

void AddMagmaBall(Missile &missile, AddMissileParameter &parameter)
{
	UpdateMissileVelocity(missile, parameter.dst, 16);
	missile.position.traveled.deltaX += 3 * missile.position.velocity.deltaX;
	missile.position.traveled.deltaY += 3 * missile.position.velocity.deltaY;
	UpdateMissilePos(missile);
	if (!gbIsHellfire || (missile.position.velocity.deltaX & 0xFFFF0000) != 0 || (missile.position.velocity.deltaY & 0xFFFF0000) != 0)
		missile._mirange = 256;
	else
		missile._mirange = 1;
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	missile._mlid = AddLight(missile.position.start, 8);
	if (missile._midam == 0) {
		switch (missile.sourceType()) {
		case MissileSource::Player:
			// Not typically created by Players
			break;
		case MissileSource::Monster:
			missile._midam = ProjectileMonsterDamage(missile);
			break;
		case MissileSource::Trap:
			missile._midam = ProjectileTrapDamage(missile);
			break;
		}
	}
}

void AddTeleport(Missile &missile, AddMissileParameter &parameter)
{
	Player &player = Players[missile._misource];

	std::optional<Point> teleportDestination = FindClosestValidPosition(
	    [&player](Point target) {
		    return PosOkPlayer(player, target);
	    },
	    parameter.dst, 0, 5);

	if (teleportDestination) {
		missile.position.tile = *teleportDestination;
		missile.position.start = *teleportDestination;
		missile._mirange = 2;
	} else {
		missile._miDelFlag = true;
		parameter.spellFizzled = true;
	}
}

void AddNovaBall(Missile &missile, AddMissileParameter &parameter)
{
	UpdateMissileVelocity(missile, parameter.dst, 16);
	missile._miAnimFrame = GenerateRnd(8) + 1;
	missile._mirange = 255;
	// The small ring (Fist of the Heavens, the Thunderous burst) reaches the 4 tiles its aim points mark, not 255 ticks
	// across the room; and none of its bolts takes the monster on the tile it starts from - the ring spreads from a
	// blast that already struck it, and all 36 landed there on their first tick (round 22 audit, v1.12.247).
	if (missile._mitype == MissileID::MiniNovaBall) {
		missile._mirange = 12;
		const Point start = missile.position.start;
		if (InDungeonBounds(start))
			missile.lastCollisionTargetHash = dMonster[start.x][start.y] ^ dPlayer[start.x][start.y];
	}
	const Point position { missile._misource < 0 ? missile.position.start : Point(Players[missile._misource].position.tile) };
	missile.var1 = position.x;
	missile.var2 = position.y;
}

void AddFireWall(Missile &missile, AddMissileParameter &parameter)
{
	missile._midam = GenerateRndSum(10, 2) + 2;
	missile._midam += missile._misource >= 0 ? Players[missile._misource]._pLevel : currlevel; // BUGFIX: missing parenthesis around ternary (fixed)
	missile._midam <<= 3;
	UpdateMissileVelocity(missile, parameter.dst, 16);
	// (10 x (level + 1), or 10 at level 0, plus the dungeon level for a trap's or monster's) x 16.
	missile._mirange = FireWallDurationTicks(missile._mispllvl);
	if (missile._micaster == TARGET_PLAYERS || missile._misource < 0)
		missile._mirange += 16 * currlevel;
	missile.var1 = missile._mirange - missile._miAnimLen;
}

void AddFireball(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == dst) {
		dst += parameter.midir;
	}
	int sp = 16;
	if (missile._micaster == TARGET_MONSTERS) {
		sp += std::min(missile._mispllvl * 2, 34);
		Player &player = Players[missile._misource];

		int dmg = 2 * (player._pLevel + GenerateRndSum(10, 2)) + 4;
		missile._midam = ScaleSpellEffect(dmg, missile._mispllvl);
	}
	UpdateMissileVelocity(missile, dst, sp);
	SetMissDir(missile, GetDirection16(missile.position.start, dst));
	missile._mirange = 256;
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	missile._mlid = AddLight(missile.position.start, 8);
}

void AddLightningControl(Missile &missile, AddMissileParameter &parameter)
{
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	UpdateMissileVelocity(missile, parameter.dst, 32);
	missile._miAnimFrame = GenerateRnd(8) + 1;
	missile._mirange = 256;
}

void AddLightning(Missile &missile, AddMissileParameter &parameter)
{
	missile.position.start = parameter.dst;

	SyncPositionWithParent(missile, parameter);

	missile._miAnimFrame = GenerateRnd(8) + 1;

	if (missile._micaster == TARGET_PLAYERS || missile.IsTrap()) {
		if (missile.IsTrap() || Monsters[missile._misource].type().type == MT_FAMILIAR)
			missile._mirange = 8;
		else
			missile._mirange = 10;
	} else {
		missile._mirange = LightningLingerTicks(missile._mispllvl);
	}
	missile._mlid = AddLight(missile.position.tile, 4);
}

void AddMissileExplosion(Missile &missile, AddMissileParameter &parameter)
{
	if (missile._micaster != TARGET_MONSTERS && missile._misource >= 0) {
		switch (Monsters[missile._misource].type().type) {
		case MT_SUCCUBUS:
			SetMissAnim(missile, MissileGraphicID::BloodStarExplosion);
			break;
		case MT_SNOWWICH:
			SetMissAnim(missile, MissileGraphicID::BloodStarBlueExplosion);
			break;
		case MT_HLSPWN:
			SetMissAnim(missile, MissileGraphicID::BloodStarRedExplosion);
			break;
		case MT_SOLBRNR:
			SetMissAnim(missile, MissileGraphicID::BloodStarYellowExplosion);
			break;
		default:
			break;
		}
	}

	assert(parameter.pParent != nullptr); // AddMissileExplosion will always be called with a parent associated to the missile.
	auto &parent = *parameter.pParent;
	missile.position.tile = parent.position.tile;
	missile.position.start = parent.position.start;
	missile.position.offset = parent.position.offset;
	missile.position.traveled = parent.position.traveled;
	missile._mirange = missile._miAnimLen;
}

namespace {

/**
 * @brief Oracool (2026-09-26): WeaponExplosion's third kind, after vanilla's fire (1) and lightning (2) - the cold
 * hit flash. A picture only: no hero carries weapon cold damage, so nothing rolls a blow for it. The cold hits
 * that call it are the missiles' and the skills' (AddColdHitFlash).
 */
constexpr int WeaponExplosionColdFlash = 3;

} // namespace

void AddWeaponExplosion(Missile &missile, AddMissileParameter &parameter)
{
	missile.var2 = parameter.dst.x;
	if (missile.var2 == WeaponExplosionColdFlash) {
		if (!MissileArtLoaded(MissileGraphicID::HitCold)) {
			missile._miDelFlag = true; // hit_cold.png not in the archive: no flash, as before
			return;
		}
		SetMissAnim(missile, MissileGraphicID::HitCold);
		missile._mirange = missile._miAnimLen * std::max(missile._miAnimDelay, 1);
		missile.var4 = 1;
		return;
	}
	const bool fire = parameter.dst.x == 1;
	SetMissAnim(missile, fire ? MissileGraphicID::MagmaBallExplosion : MissileGraphicID::ChargedBolt);
	missile._mirange = missile._miAnimLen - 1;
	// Oracool (2026-09-11): the hit flashes from the briefs, once delivered. The range above stays
	// the vanilla graphic's, so a blow gets exactly as many rolls to land as it always did; only the
	// picture changes. var4 marks the flash for ProcessWeaponExplosion.
	const MissileGraphicID flash = fire ? MissileGraphicID::HitFire : MissileGraphicID::HitLightning;
	if (MissileArtLoaded(flash)) {
		SetMissAnim(missile, flash);
		missile.var4 = 1;
	}
}

void AddTownPortal(Missile &missile, AddMissileParameter &parameter)
{
	if (leveltype == DTYPE_TOWN) {
		missile.position.tile = parameter.dst;
		missile.position.start = parameter.dst;
		// Oracool (2026-09-20): IN TOWN the portal wears its 90% sheet with the ground shadow stripped
		// (user: "shrink the town portal asset in-town only, not in dungeons to 90% and remove its
		// shadow") - it stands inside the Rift Monument's arch there. SetMissDir keeps the graphic, so
		// the opening-to-standing switch below and AddWarpMissile's sync both stay on this sheet. The
		// dungeon-side portal, in the else branch, keeps vanilla's CL2 untouched.
		SetMissAnim(missile, MissileGraphicID::TownPortalInTown);
	} else {
		std::optional<Point> targetPosition = FindClosestValidPosition(
		    [](Point target) {
			    if (!InDungeonBounds(target)) {
				    return false;
			    }
			    if (IsObjectAtPosition(target)) {
				    return false;
			    }
			    if (dPlayer[target.x][target.y] != 0) {
				    return false;
			    }
			    if (TileContainsMissile(target)) {
				    return false;
			    }

			    int dp = dPiece[target.x][target.y];
			    if (TileHasAny(dp, TileProperties::Solid | TileProperties::BlockMissile)) {
				    return false;
			    }
			    return !CheckIfTrig(target);
		    },
		    parameter.dst, 0, 5);

		if (targetPosition) {
			missile.position.tile = *targetPosition;
			missile.position.start = *targetPosition;
			missile._miDelFlag = false;
		} else {
			missile._miDelFlag = true;
		}
	}

	missile._mirange = 100;
	missile.var1 = missile._mirange - missile._miAnimLen;
	for (auto &other : Missiles) {
		if (other._mitype == MissileID::TownPortal && &other != &missile && missile.isSameSource(other))
			other._mirange = 0;
	}
	PutMissile(missile);
	if (missile.sourcePlayer() == MyPlayer && !missile._miDelFlag && leveltype != DTYPE_TOWN) {
		if (!setlevel) {
			NetSendCmdLocParam3(true, CMD_ACTIVATEPORTAL, missile.position.tile, currlevel, leveltype, 0);
		} else {
			NetSendCmdLocParam3(true, CMD_ACTIVATEPORTAL, missile.position.tile, setlvlnum, leveltype, 1);
		}
	}
}

void AddFlashBottom(Missile &missile, AddMissileParameter & /*parameter*/)
{
	switch (missile.sourceType()) {
	case MissileSource::Player: {
		Player &player = *missile.sourcePlayer();
		int dmg = GenerateRndSum(20, player._pLevel + 1) + player._pLevel + 1;
		missile._midam = ScaleSpellEffect(dmg, missile._mispllvl);
		missile._midam += missile._midam / 2;
	} break;
	case MissileSource::Monster:
		missile._midam = missile.sourceMonster()->level(sgGameInitInfo.nDifficulty) * 2;
		break;
	case MissileSource::Trap:
		missile._midam = currlevel / 2;
		break;
	}

	missile._mirange = 19;
}

void AddFlashTop(Missile &missile, AddMissileParameter & /*parameter*/)
{
	if (missile._micaster == TARGET_MONSTERS) {
		if (!missile.IsTrap()) {
			int dmg = Players[missile._misource]._pLevel + 1;
			dmg += GenerateRndSum(20, dmg);
			missile._midam = ScaleSpellEffect(dmg, missile._mispllvl);
			missile._midam += missile._midam / 2;
		} else {
			missile._midam = currlevel / 2;
		}
	}
	missile._miPreFlag = true;
	missile._mirange = 19;
}

void AddManaShield(Missile &missile, AddMissileParameter &parameter)
{
	missile._miDelFlag = true;

	Player &player = Players[missile._misource];

	if (player.pManaShield) {
		parameter.spellFizzled = true;
		return;
	}

	player.pManaShield = true;
	if (&player == MyPlayer)
		NetSendCmd(true, CMD_SETSHIELD);
}

void AddFlameWave(Missile &missile, AddMissileParameter &parameter)
{
	missile._midam = GenerateRnd(10) + Players[missile._misource]._pLevel + 1;
	UpdateMissileVelocity(missile, parameter.dst, 16);
	missile._mirange = 255;

	// Adjust missile's position for rendering
	missile.position.tile += Direction::South;
	missile.position.offset.deltaY -= 32;
}

void AddGuardian(Missile &missile, AddMissileParameter &parameter)
{
	Player &player = Players[missile._misource];

	std::optional<Point> spawnPosition = FindClosestValidPosition(
	    [start = missile.position.start](Point target) {
		    if (!InDungeonBounds(target)) {
			    return false;
		    }
		    if (dMonster[target.x][target.y] != 0) {
			    return false;
		    }
		    if (IsObjectAtPosition(target)) {
			    return false;
		    }
		    if (TileContainsMissile(target)) {
			    return false;
		    }

		    int dp = dPiece[target.x][target.y];
		    if (TileHasAny(dp, TileProperties::Solid | TileProperties::BlockMissile)) {
			    return false;
		    }

		    return LineClearMissile(start, target);
	    },
	    parameter.dst, 0, 5);

	if (!spawnPosition) {
		missile._miDelFlag = true;
		parameter.spellFizzled = true;
		return;
	}

	missile._miDelFlag = false;
	missile.position.tile = *spawnPosition;
	missile.position.start = *spawnPosition;

	missile._mlid = AddLight(missile.position.tile, 1);
	missile._mirange = GuardianDurationTicks(missile._mispllvl, player._pLevel);

	missile.var1 = missile._mirange - missile._miAnimLen;
	missile.var3 = 1;
}

void AddChainLightning(Missile &missile, AddMissileParameter &parameter)
{
	missile.var1 = parameter.dst.x;
	missile.var2 = parameter.dst.y;
	missile._mirange = 1;
}

namespace {
void InitMissileAnimationFromMonster(Missile &mis, Direction midir, const Monster &mon, MonsterGraphic graphic)
{
	// The monster's SIZED animation, as its own binders use: a Giant or Colossal charger shrank to 100% for the charge
	// (round 9 audit, v1.12.234).
	const AnimStruct *scaled = oracool::GetScaledAnim(mon, graphic);
	const AnimStruct &anim = scaled != nullptr ? *scaled : mon.type().getAnimData(graphic);
	mis._mimfnum = static_cast<int32_t>(midir);
	mis._miAnimFlags = MissileGraphicsFlags::None;
	// Oracool audit (2026-08-16): the monster helper correctly returns nullopt, and this was the one
	// caller that threw the guard away with a bare `*`. A monster whose animation has not loaded
	// firing a missile that borrows it is the reachable case.
	const OptionalClxSpriteList maybeSprites = anim.spritesForDirection(midir);
	if (!maybeSprites)
		return;
	ClxSpriteList sprites = *maybeSprites;
	const uint16_t width = sprites[0].width();
	mis._miAnimData.emplace(sprites);
	mis.oracoolColours = nullptr; // a monster's own sprites, in the level palette
	mis._miAnimDelay = anim.rate;
	mis._miAnimLen = anim.frames;
	mis._miAnimWidth = width;
	mis._miAnimWidth2 = CalculateWidth2(width);
	mis._miAnimAdd = 1;
	mis.var1 = 0;
	mis.var2 = 0;
	mis._miLightFlag = true;
	mis._mirange = 256;
}
} // namespace

void AddRhino(Missile &missile, AddMissileParameter &parameter)
{
	Monster &monster = Monsters[missile._misource];

	MonsterGraphic graphic = MonsterGraphic::Walk;
	if (IsAnyOf(monster.type().type, MT_HORNED, MT_MUDRUN, MT_FROSTC, MT_OBLORD)) {
		graphic = MonsterGraphic::Special;
	} else if (IsAnyOf(monster.type().type, MT_NSNAKE, MT_RSNAKE, MT_BSNAKE, MT_GSNAKE)) {
		graphic = MonsterGraphic::Attack;
	}
	UpdateMissileVelocity(missile, parameter.dst, 18);
	InitMissileAnimationFromMonster(missile, parameter.midir, monster, graphic);
	if (IsAnyOf(monster.type().type, MT_NSNAKE, MT_RSNAKE, MT_BSNAKE, MT_GSNAKE))
		missile._miAnimFrame = 7;
	// Any charger's light and tint, not only a unique's: a Luminous charger left its light at the start tile and a
	// variant charged in vanilla colours (round 11 audit, v1.12.236). The draw path reads the caster's TRN itself.
	if (monster.lightId != NO_LIGHT)
		missile._mlid = monster.lightId;
	if (!monster.isUnique() && monster.uniqueMonsterTRN != nullptr)
		missile._miUniqTrans = 1;
	PutMissile(missile);
}

void AddGenericMagicMissile(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == dst) {
		dst += parameter.midir;
	}
	UpdateMissileVelocity(missile, dst, 16);
	missile._mirange = 256;
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	missile._mlid = AddLight(missile.position.start, 8);
	if (missile._micaster != TARGET_MONSTERS && missile._misource > 0) {
		auto &monster = Monsters[missile._misource];
		if (monster.type().type == MT_SUCCUBUS)
			SetMissAnim(missile, MissileGraphicID::BloodStar);
		if (monster.type().type == MT_SNOWWICH)
			SetMissAnim(missile, MissileGraphicID::BloodStarBlue);
		if (monster.type().type == MT_HLSPWN)
			SetMissAnim(missile, MissileGraphicID::BloodStarRed);
		if (monster.type().type == MT_SOLBRNR)
			SetMissAnim(missile, MissileGraphicID::BloodStarYellow);
	}

	if (GetMissileSpriteData(missile._miAnimType).animFAmt == 16) {
		SetMissDir(missile, GetDirection16(missile.position.start, dst));
	}

	if (missile._midam == 0) {
		switch (missile.sourceType()) {
		case MissileSource::Player: {
			const Player &player = *missile.sourcePlayer();
			missile._midam = 3 * missile._mispllvl - (player._pMagic / 8) + (player._pMagic / 2);
			break;
		}
		case MissileSource::Monster:
			missile._midam = ProjectileMonsterDamage(missile);
			break;
		case MissileSource::Trap:
			missile._midam = ProjectileTrapDamage(missile);
			break;
		}
	}
}

void AddAcid(Missile &missile, AddMissileParameter &parameter)
{
	UpdateMissileVelocity(missile, parameter.dst, 16);
	SetMissDir(missile, GetDirection16(missile.position.start, parameter.dst));
	if (!gbIsHellfire || (missile.position.velocity.deltaX & 0xFFFF0000) != 0 || (missile.position.velocity.deltaY & 0xFFFF0000) != 0)
		missile._mirange = 5 * (Monsters[missile._misource].intelligence + 4);
	else
		missile._mirange = 1;
	missile._mlid = NO_LIGHT;
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	if (missile._midam == 0) {
		switch (missile.sourceType()) {
		case MissileSource::Player:
			// Not typically created by Players
			break;
		case MissileSource::Monster:
			missile._midam = ProjectileMonsterDamage(missile);
			break;
		case MissileSource::Trap:
			missile._midam = ProjectileTrapDamage(missile);
			break;
		}
	}
	PutMissile(missile);
}

void AddAcidPuddle(Missile &missile, AddMissileParameter & /*parameter*/)
{
	missile._miLightFlag = true;
	int monst = missile._misource;
	missile._mirange = GenerateRnd(15) + 40 * (Monsters[monst].intelligence + 1);
	missile._miPreFlag = true;
}

void AddStoneCurse(Missile &missile, AddMissileParameter &parameter)
{
	// Oracool: town's dMonster holds towner ids, not monsters.
	if (leveltype == DTYPE_TOWN) {
		missile._miDelFlag = true;
		parameter.spellFizzled = true;
		return;
	}
	std::optional<Point> targetMonsterPosition = FindClosestValidPosition(
	    [](Point target) {
		    if (!InDungeonBounds(target)) {
			    return false;
		    }

		    int monsterId = abs(dMonster[target.x][target.y]) - 1;
		    if (monsterId < 0) {
			    return false;
		    }

		    auto &monster = Monsters[monsterId];

		    if (IsAnyOf(monster.type().type, MT_GOLEM, MT_DIABLO, MT_NAKRUL)) {
			    return false;
		    }
		    if (monster.isPlayerMinion() || oracool::IsCompanion(monster)) {
			    return false; // the hero's own side
		    }
		    // Not one already stone: the curse picked it over a valid monster beside it and did nothing (round 16 audit).
		    if (IsAnyOf(monster.mode, MonsterMode::FadeIn, MonsterMode::FadeOut, MonsterMode::Charge, MonsterMode::Petrified)) {
			    return false;
		    }

		    return true;
	    },
	    parameter.dst, 0, 5);

	if (!targetMonsterPosition) {
		missile._miDelFlag = true;
		parameter.spellFizzled = true;
		return;
	}

	// Petrify the targeted monster
	int monsterId = abs(dMonster[targetMonsterPosition->x][targetMonsterPosition->y]) - 1;
	auto &monster = Monsters[monsterId];

	if (monster.mode == MonsterMode::Petrified) {
		// Monster is already petrified and StoneCurse doesn't stack
		missile._miDelFlag = true;
		parameter.spellFizzled = true; // not paid for (round 16 audit)
		return;
	}

	missile.var1 = static_cast<int>(monster.mode);
	missile.var2 = monsterId;
	monster.petrify();

	// And set up the missile to unpetrify it in the future
	missile.position.tile = *targetMonsterPosition;
	missile.position.start = missile.position.tile;
	missile._mirange = StoneCurseDurationTicks(missile._mispllvl);
}

void AddGolem(Missile &missile, AddMissileParameter &parameter)
{
	missile._miDelFlag = true;

	// Oracool: every spell is castable in town, but town (and a quest's set level) never runs InitGolems' slot
	// setup, so Monsters[playerId] is no golem there. Spawning into it drew a monster with no type data -
	// the user's crash casting Valkyrie in town (2026-09-14, access violation in Monster::exp).
	if (!LevelHasGolemSlots()) {
		parameter.spellFizzled = true;
		return;
	}

	int playerId = missile._misource;
	Player &player = Players[playerId];
	Monster &golem = Monsters[playerId];
	// The Golem spell takes this slot: a companion standing in it (a multiplayer owner's own slot) waits for another.
	oracool::ForgetCompanionInSlot(golem);

	if (golem.position.tile != GolemHoldingCell && &player == MyPlayer)
		KillMyGolem();

	if (golem.position.tile == GolemHoldingCell) {
		std::optional<Point> spawnPosition = FindClosestValidPosition(
		    [start = missile.position.start](Point target) {
			    return !IsTileOccupied(target) && LineClearMissile(start, target);
		    },
		    parameter.dst, 0, 5);

		if (spawnPosition) {
			SpawnGolem(player, golem, *spawnPosition, missile);
		}
	}
}

void AddApocalypseBoom(Missile &missile, AddMissileParameter &parameter)
{
	missile.position.tile = parameter.dst;
	missile.position.start = parameter.dst;
	missile._mirange = missile._miAnimLen;
}

void AddHealing(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	int hp = GenerateRnd(10) + 1;
	hp += GenerateRndSum(4, player._pLevel) + player._pLevel;
	hp += GenerateRndSum(6, missile._mispllvl) + missile._mispllvl;
	hp <<= 6;

	if (player._pClass == HeroClass::Warrior || player._pClass == HeroClass::Barbarian || player._pClass == HeroClass::Monk) {
		hp *= 2;
	} else if (player._pClass == HeroClass::Rogue || player._pClass == HeroClass::Bard) {
		hp += hp / 2;
	}

	player._pHitPoints = std::min(player._pHitPoints + hp, player._pMaxHP);
	player._pHPBase = std::min(player._pHPBase + hp, player._pMaxHPBase);

	missile._miDelFlag = true;
	RedrawComponent(PanelDrawComponent::Health);
}

void AddHealOther(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	missile._miDelFlag = true;
	if (&player == MyPlayer) {
		NewCursor(CURSOR_HEALOTHER);
		if (ControlMode != ControlTypes::KeyboardAndMouse)
			TryIconCurs();
	}
}

void AddElemental(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == dst) {
		dst += parameter.midir;
	}

	Player &player = Players[missile._misource];

	int dmg = 2 * (player._pLevel + GenerateRndSum(10, 2)) + 4;
	missile._midam = ScaleSpellEffect(dmg, missile._mispllvl) / 2;

	UpdateMissileVelocity(missile, dst, 16);
	SetMissDir(missile, GetDirection(missile.position.start, dst));
	missile._mirange = 256;
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	missile.var4 = dst.x;
	missile.var5 = dst.y;
	missile._mlid = AddLight(missile.position.start, 8);
}

extern void FocusOnInventory();

void AddIdentify(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	missile._miDelFlag = true;
	if (&player == MyPlayer) {
		if (sbookflag)
			sbookflag = false;
		if (!invflag) {
			invflag = true;
			if (ControlMode != ControlTypes::KeyboardAndMouse)
				FocusOnInventory();
		}
		NewCursor(CURSOR_IDENTIFY);
	}
}

void AddFireWallControl(Missile &missile, AddMissileParameter &parameter)
{
	std::optional<Point> spreadPosition = FindClosestValidPosition(
	    [start = missile.position.start](Point target) {
		    return start != target && IsTileNotSolid(target) && !IsObjectAtPosition(target) && LineClearMissile(start, target);
	    },
	    parameter.dst, 0, 5);

	if (!spreadPosition) {
		missile._miDelFlag = true;
		parameter.spellFizzled = true;
		return;
	}

	missile._miDelFlag = false;
	missile.var1 = spreadPosition->x;
	missile.var2 = spreadPosition->y;
	missile.var5 = spreadPosition->x;
	missile.var6 = spreadPosition->y;
	missile.var3 = static_cast<int>(Left(Left(parameter.midir)));
	missile.var4 = static_cast<int>(Right(Right(parameter.midir)));
	missile._mirange = 7;
}

void AddInfravision(Missile &missile, AddMissileParameter & /*parameter*/)
{
	missile._mirange = InfravisionDurationTicks(missile._mispllvl);
}

/**
 * @brief Oracool: the Paladin's Blessed Hammer - a hammer that spirals outward from the caster.
 *
 * var1 counts ticks, which is what drives both the angle and the radius. var2/var3 hold the last
 * tile damaged, so one pass hurts a monster once rather than every frame it overlaps it; they start
 * on the caster's own tile so the hammer does not strike the ground it launched from.
 */
/**
 * @brief Oracool: Blessed Shield's throw - the shield leaves the hand at twice a Holy Bolt's pace.
 *
 * "with speed of 2x Holy Bolts move" (user, 2026-08-15). AddHolyBolt uses 16, so this uses 32; the
 * number is written as the doubling rather than as 32 so the relationship survives a change to
 * either.
 */
/**
 * @brief Oracool: points a missile at one of the ITEM DROP animations.
 *
 * The user asked "what is the animation played when i drop mace from inventory on ground?", and the
 * answer corrected an earlier claim of mine. I had searched only MissileSpriteData, found no mace or
 * shield among its 42 entries, and reported that neither animation existed. They exist - as
 * items\mace.cel and items\shield.cel, the tumbles an item plays when it lands on the floor. They
 * are the only animations in the game of an OBJECT in flight rather than a character holding one,
 * which is exactly what a thrown shield and a falling mace need.
 *
 * Single-direction, so the missile is left facing frame 0; these tumble rather than aim.
 */
void UseItemDropAnimation(Missile &missile, int8_t animIndex)
{
	OptionalClxSpriteList sprites = GetItemDropAnim(animIndex);
	if (!sprites)
		return; // before InitItems, or after the graphics were freed - keep the misdat sprite
	missile._miAnimData = sprites;
	missile.oracoolColours = nullptr; // an item's sprite, in the level palette
	missile._miAnimLen = static_cast<int>(sprites->numSprites());
	missile._miAnimWidth = (*sprites)[0].width();
	missile._miAnimWidth2 = CalculateWidth2(missile._miAnimWidth);
	missile._miAnimFrame = 1;
	missile._miAnimCnt = 0;
	// The borrowed sprite is painted as LOOT - a plain steel shield reads as something to pick up,
	// not as something a Paladin blessed and threw. The recolour is what makes it divine rather than
	// dropped (user, 2026-08-15: "make them a bit shiny. lightning shiny. divine shyni").
	missile.oracoolTrn = oracool::GetDivineTrn();
}

void AddBlessedShieldThrow(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == dst)
		dst += parameter.midir;
	UpdateMissileVelocity(missile, dst, HolyBoltSpeed * BlessedShieldSpeedMultiplier);
	missile._mirange = BlessedShieldRangeTicks;
	missile.var1 = 0; // strikes so far...
	missile.var2 = 0; // ...and the first two targets' ids + 1 - see ProcessBlessedShieldThrow
	missile.var3 = 0;
	SetMissDir(missile, GetDirection16(missile.position.start, dst));
	// Its own spin sheet once delivered (2026-09-11); until then the shield item's drop tumble,
	// painted divine - "in spinning motion animation, if available".
	if (MissileArtLoaded(MissileGraphicID::BlessedShieldSpin)) {
		SetMissAnim(missile, MissileGraphicID::BlessedShieldSpin);
		// Three quarters size, tinted Paladin gold (the Paladin Skill Cards page, 2026-09-28).
		missile.oracoolTint = oracool::Tint::Hue;
		missile.oracoolTintRgb = oracool::hue::PaladinGold;
		ScaleMissile(missile, 75);
	} else {
		UseItemDropAnimation(missile, ShieldDropAnimIndex);
	}
}

/**
 * @brief Oracool: the mace Fist of the Heavens drops on the target, using the item's own fall.
 *
 * It does not travel - it lands. The animation IS the descent, so the missile sits on the target
 * tile playing it out and fires the impact when the last frame shows.
 */
void AddFallingMace(Missile &missile, AddMissileParameter &parameter)
{
	missile.position.tile = parameter.dst;
	missile.position.start = parameter.dst;
	// The bolt from the sky once delivered (2026-09-11), ten frames ending on the ground; until then
	// the mace item's drop tumble, painted divine.
	if (MissileArtLoaded(MissileGraphicID::FistOfHeavensBolt)) {
		SetMissAnim(missile, MissileGraphicID::FistOfHeavensBolt);
		// Three quarters size, tinted ice blue, its strike still on the ground (the Paladin Skill Cards page, 2026-09-28).
		missile.oracoolTint = oracool::Tint::Hue;
		missile.oracoolTintRgb = oracool::hue::IceBlue;
		ScaleMissile(missile, 75, 0);
	} else {
		UseItemDropAnimation(missile, MaceDropAnimIndex);
	}
	// The animation's own length, so the blast lands exactly when the mace does rather than on a
	// number picked to look about right.
	missile._mirange = missile._miAnimLen;
}

void ProcessFallingMace(Missile &missile)
{
	missile._mirange--;
	if (missile._mirange <= 0) {
		missile._miDelFlag = true;
		Player *player = missile.sourcePlayer();
		if (player != nullptr)
			oracool::FistOfTheHeavensImpact(*player, missile.position.tile, missile._midam, missile._mispllvl);
		return;
	}
	PutMissile(missile);
}

namespace {

/** @brief How far a struck shield looks for the next monster, in tiles. */
constexpr unsigned BlessedShieldBounceTiles = 6;

/** @brief Whether the throw has already struck monster @p monsterId - var2 and var3 hold the first two, + 1. */
bool BlessedShieldHasStruck(const Missile &missile, int monsterId)
{
	return missile.var2 == monsterId + 1 || missile.var3 == monsterId + 1;
}

/**
 * @brief The nearest monster to @p from the shield may bounce to: one it has not struck, not the
 * player's own, hittable, and in sight - FindClosest's rule, less the ones already struck.
 */
Monster *NextBlessedShieldTarget(const Missile &missile, Point from)
{
	const std::optional<Point> found = FindClosestValidPosition(
	    [&missile, from](Point tile) {
		    if (!InDungeonBounds(tile) || tile == missile.position.tile || dMonster[tile.x][tile.y] <= 0 || CheckBlock(from, tile))
			    return false;
		    const int id = dMonster[tile.x][tile.y] - 1;
		    Monster &monster = Monsters[id];
		    // Not a converted ally either: the shield turned toward it, passed through and lost the bounce (round 22 audit).
		    return !BlessedShieldHasStruck(missile, id) && !monster.isPlayerMinion() && !oracool::IsMonsterConverted(monster)
		        && monster.isPossibleToHit();
	    },
	    from, 1, BlessedShieldBounceTiles);
	if (!found)
		return nullptr;
	return &Monsters[dMonster[found->x][found->y] - 1];
}

} // namespace

/**
 * @brief Carries the thrown shield from monster to monster (user, 2026-09-11: "it should bound off of
 * first target in direction to nearest monster and then bouce off to third monster. dmg should reduce
 * with each target - 100% on first, 75% on second, 50% on third").
 *
 * The same message took away the splash it had since 2026-08-15 ("i dont think blessed shield should
 * have splash dmg"), and asked for a flash on every strike - BlessedShieldImpact.
 *
 * Its own collision rather than MoveMissileAndCheckMissileCol's, because the shield must fly THROUGH
 * what it has already struck: the first target stands right behind it when it turns, and the second
 * may stand on the line to the third. Each strike stops it short of its target's tile, as the throw
 * always did, and turns it on NextBlessedShieldTarget - or ends the throw after the third, or when no
 * monster is left to turn on. A wall ends it too; so does the range, for a throw that strikes nothing.
 */
void ProcessBlessedShieldThrow(Missile &missile)
{
	missile._mirange--;
	const int hitsSoFar = missile.var1;
	const int damage = BlessedShieldHitDamage(missile._midam, hitsSoFar);
	const DamageType damageType = GetMissileData(missile._mitype).damageType();
	std::optional<Point> struck;
	int struckId = -1;
	bool walled = false;
	missile._miHitFlag = false;
	MoveMissile(
	    missile, [&](Point tile) {
		    if (!InDungeonBounds(tile)) {
			    walled = true;
			    return false;
		    }
		    if (tile == missile.position.start)
			    return true; // the thrower's own tile, which the throw has always skipped
		    const int occupant = dMonster[tile.x][tile.y];
		    const int occupantId = occupant != 0 ? abs(occupant) - 1 : -1;
		    // A monster already struck is flown through like open floor.
		    if (occupantId < 0 || !BlessedShieldHasStruck(missile, occupantId)) {
			    CheckMissileCol(missile, damageType, damage, damage, false, tile, /*dontDeleteOnCollision=*/true);
			    if (missile._miHitFlag) {
				    struck = tile;
				    struckId = occupantId;
				    return false;
			    }
		    }
		    if (IsMissileBlockedByTile(tile)) {
			    walled = true;
			    return false;
		    }
		    return true;
	    },
	    /*ifCheckTileFailsDontMoveToTile=*/true);

	if (struck) {
		oracool::PlayBlessedShieldImpactSound(missile);
		AddMissile(*struck, *struck, Direction::South, MissileID::BlessedShieldImpact, missile._micaster,
		    missile._misource, 0, 0);
		if (hitsSoFar == 0)
			missile.var2 = struckId + 1;
		else if (hitsSoFar == 1)
			missile.var3 = struckId + 1;
		missile.var1 = hitsSoFar + 1;
		const Monster *next = missile.var1 < BlessedShieldTargets ? NextBlessedShieldTarget(missile, *struck) : nullptr;
		if (next == nullptr) {
			missile._miDelFlag = true;
			return;
		}
		// No SetMissDir: it would re-dress the missile from its table graphic, and the shield spins,
		// so which way it faces never showed.
		UpdateMissileVelocity(missile, next->position.tile, HolyBoltSpeed * BlessedShieldSpeedMultiplier);
		missile._mirange = BlessedShieldRangeTicks;
	} else if (walled || missile._mirange <= 0) {
		if (walled)
			oracool::PlayBlessedShieldImpactSound(missile);
		missile._miDelFlag = true;
		return;
	}
	PutMissile(missile);
}

void AddBlessedHammer(Missile &missile, AddMissileParameter & /*parameter*/)
{
	// Its own sprite since 2026-09-11: MissileGraphicID::BlessedHammerSpin, sixteen frames of the
	// hammer turning, one frame a tick, looped by ProcessMissiles for the whole spiral - a full turn
	// every 16 ticks. (It wore the mace item's drop tumble painted gold from 2026-09-07; that
	// borrowed sprite never rotated, it slid.)
	missile._mirange = BlessedHammerTicks;
	missile.var1 = 0;
	missile.var2 = missile.position.start.x; // the tile it last entered...
	missile.var3 = missile.position.start.y;
	missile.var4 = missile.position.start.x; // ...and the one before that - see ProcessBlessedHammer
	missile.var5 = missile.position.start.y;
	// Three quarters size, tinted Paladin gold (the Paladin Skill Cards page, 2026-09-28).
	if (missile._miAnimType == MissileGraphicID::BlessedHammerSpin) {
		missile.oracoolTint = oracool::Tint::Hue;
		missile.oracoolTintRgb = oracool::hue::PaladinGold;
		ScaleMissile(missile, 75);
	}
}

void UseMissileGraphic(Missile &missile, MissileGraphicID graphic)
{
	SetMissAnim(missile, graphic);
}

namespace {

/**
 * @brief Where a census effect's sheet meets the floor, as a still missile's offset. AddWarcryRing's rule: a
 * sprite hangs from its tile by its cell's bottom edge, +y moves it down, and the tile's centre is 16px above
 * that edge. The anchors are the delivery's notes (RfA-16 batch 35).
 */
Displacement CensusEffectOffset(MissileID type)
{
	switch (type) {
	case MissileID::MeteorFall:
	case MissileID::ThunderBolt:
		return { 0, -13 }; // touchdown 3px above the cell bottom (meteor y 157 of 160, bolt y 189 of 192)
	case MissileID::MeteorImpact:
		return { 0, 22 }; // the burst's floor centre is y 90 of 128, 38px above the bottom
	case MissileID::AcidCloud:
		return { 0, -3 }; // the vapour's baseline is y 91 of 96
	// The Necromancer's (batch 38), from the delivery's anchors: (cell height - anchor y) - 16, the ring's own rule.
	case MissileID::BoneWallEffect:
	case MissileID::BoneSpikesEffect:
		return { 0, -6 }; // baseline y 91 of 96
	case MissileID::CorpseBurst:
		return { 0, 8 }; // the body's floor point y 104 of 128
	case MissileID::RaiseDeadEffect:
		return { 0, -8 }; // floor point y 120 of 128
	case MissileID::CurseCastEffect:
	case MissileID::BoneStormEffect:
		return { 0, 38 }; // floor ellipse centre y 74 of 128
	default:
		return { 0, 0 };
	}
}

/** @brief Meteor impact: frames 1-10 burst once, 11-14 are the ground burn, looped for as long as it burns. */
constexpr int MeteorImpactBurnFrame = 11;
/** @brief Bone wall: frames 1-6 rise once, 7-10 stand, looped for as long as the wall holds (batch 38's notes). */
constexpr int BoneWallStandFrame = 7;

/**
 * @brief The sheet AddArtEffect is spawning, for AddCensusEffect to wear in place of its row's own - set for the one
 * AddMissile call and cleared after it. None the rest of the time, which leaves every census row exactly as it was.
 */
MissileGraphicID CensusArtOverride = MissileGraphicID::None;

/** @brief AddArtEffect's carrier: a census row that loops its sheet for the ticks it is given and does nothing else. */
constexpr MissileID ArtEffectCarrier = MissileID::AcidCloud;

/** @brief AddArtBolt's carrier: the javelin's row, which flies to its tile and ends there (AddAcidJavelin). */
constexpr MissileID ArtBoltCarrier = MissileID::AcidJavelin;

/** @brief AddArtBolt's speed for the one AddMissile call it makes; 0 the rest of the time (the javelin's own 32). */
int ArtBoltSpeed = 0;

/**
 * @brief Where an RfA-27 sheet meets its tile, as a still missile's offset - the delivery's draw anchors (batches
 * 52-57 notes.txt), by AddWarcryRing's rule: (cell height - anchor y) - 16. The strike flashes and the arc's spark
 * anchor on the struck body's centre, lifted 32px off the floor. Every other sheet, and every flying one, is {0, 0}.
 */
Displacement ArtEffectAnchor(MissileGraphicID art)
{
	switch (art) {
	case MissileGraphicID::ArcSpark:
		return { 0, -16 };
	case MissileGraphicID::EmberMine:
		return { 0, -8 };
	case MissileGraphicID::StaticCharge:
	case MissileGraphicID::Conduit:
	case MissileGraphicID::Immolate:
	case MissileGraphicID::MantraOfClarity:
	case MissileGraphicID::MantraOfEvasion:
	case MissileGraphicID::MantraOfRetribution:
	case MissileGraphicID::AstralProjection:
	case MissileGraphicID::PoisonDagger:
	case MissileGraphicID::FrenzyOfTheDead:
	case MissileGraphicID::Serenity:
	case MissileGraphicID::DarkMending:
	case MissileGraphicID::UnholyOffering:
		return { 0, -4 }; // feet at y 116 of 128 (84 of 96)
	case MissileGraphicID::StormArc:
	case MissileGraphicID::FuneralStarCharge:
		return { 0, -1 }; // y 49 of 64
	case MissileGraphicID::LightningRodBurst:
	case MissileGraphicID::FaradayRing:
	case MissileGraphicID::LightningRod:
	case MissileGraphicID::StormConductor:
	case MissileGraphicID::ShadowStep:
	case MissileGraphicID::VaultDust:
	case MissileGraphicID::LeapingCrane:
	case MissileGraphicID::ShoulderGate:
	case MissileGraphicID::GatherTheDead:
		return { 0, 6 }; // y 74 of 96
	case MissileGraphicID::GroundStomp:
	case MissileGraphicID::MountainPole:
	case MissileGraphicID::FlameRing:
	case MissileGraphicID::AbsoluteZero:
	case MissileGraphicID::BlindingFlash:
	case MissileGraphicID::DeathNova:
	case MissileGraphicID::EmberBurst:
	case MissileGraphicID::AshenBurst:
	case MissileGraphicID::ExplodingPalmBurst:
	case MissileGraphicID::ValkyrieBurst:
	case MissileGraphicID::RainOfArrows:
	case MissileGraphicID::ArmyOfTheDead:
	case MissileGraphicID::HeavensDescent:
	case MissileGraphicID::BonePrison:
	case MissileGraphicID::FuneralStarBurst:
	case MissileGraphicID::AncestralCourt:
	case MissileGraphicID::Earthquake:
		return { 0, 13 }; // floor point y 99 of 128
	case MissileGraphicID::WaveOfLight:
		return { 0, 20 }; // y 124 of 160
	case MissileGraphicID::WrathPillar:
		return { 0, 27 }; // y 149 of 192
	case MissileGraphicID::HammerOfTheAncients:
	case MissileGraphicID::CleaveArc:
	case MissileGraphicID::BackhandArc:
	case MissileGraphicID::AegisSlam:
	case MissileGraphicID::SweepArc:
	case MissileGraphicID::LowBranch:
	case MissileGraphicID::RearwardReach:
	case MissileGraphicID::TurningPike:
	case MissileGraphicID::CrusadeSweep:
	case MissileGraphicID::DragonTailSweep:
	case MissileGraphicID::WhirlingKick:
		return { 0, 48 }; // the feet at the cell's centre, y 64 of 128
	case MissileGraphicID::HolyLance:
	case MissileGraphicID::LongThrust:
	case MissileGraphicID::ReapingPoint:
	case MissileGraphicID::ChillTouch:
	case MissileGraphicID::FurnaceMouth:
		return { 0, 80 }; // the feet at the cell's centre, y 96 of 192
	default:
		return { 0, 0 };
	}
}

/** @brief The RfA-27 sheets that lie flat on the floor, drawn under whoever stands in them (_miPreFlag). */
bool ArtEffectOnFloor(MissileGraphicID art)
{
	return IsAnyOf(art, MissileGraphicID::GroundStomp, MissileGraphicID::MountainPole, MissileGraphicID::FlameRing,
	    MissileGraphicID::AbsoluteZero, MissileGraphicID::DeathNova, MissileGraphicID::Earthquake, MissileGraphicID::FaradayRing,
	    MissileGraphicID::StormArc, MissileGraphicID::EmberMine);
}

} // namespace

/**
 * @brief Oracool (2026-09-14, RfA-16): a census skill's effect standing on a tile - Meteor's fall and impact,
 * Thunder Storm's bolt, the javelins' acid cloud. Drawn only; the skill's blows land in rfa12_actives and
 * aura_field. The fall and the bolt play their frames once; the impact and the cloud last the caller's
 * duration, passed as the missile's damage, in ticks.
 */
void AddCensusEffect(Missile &missile, AddMissileParameter &parameter)
{
	if (CensusArtOverride != MissileGraphicID::None)
		SetMissAnim(missile, CensusArtOverride); // AddArtEffect's sheet, riding this row
	if (!MissileArtLoaded(missile._miAnimType)) {
		missile._miDelFlag = true; // sheet not in the archive: the caller shows its placeholder
		return;
	}
	missile.position.tile = parameter.dst;
	missile.position.start = parameter.dst;
	missile.position.offset = CensusEffectOffset(missile._mitype);
	// A whole play of the sheet is its frames times the ticks each one holds - raise_dead steps every second tick,
	// and a range of one tick a frame (2026-09-26 audit) cut it off halfway. A longer duration asked for in the
	// missile's damage (a cloud, a wall, a burn) still wins; the fall and the bolt play exactly once.
	const int onePlay = missile._miAnimLen * std::max<int>(missile._miAnimDelay, 1);
	if (IsAnyOf(missile._mitype, MissileID::MeteorFall, MissileID::ThunderBolt))
		missile._mirange = onePlay;
	else
		missile._mirange = std::max(missile._midam, onePlay);
}

Missile *AddArtEffect(Point tile, MissileGraphicID art, int playerId, int ticks)
{
	if (!MissileArtLoaded(art) || !InDungeonBounds(tile))
		return nullptr;
	CensusArtOverride = art;
	Missile *effect = AddMissile(tile, tile, Direction::South, ArtEffectCarrier, TARGET_MONSTERS, playerId, ticks, 0);
	CensusArtOverride = MissileGraphicID::None;
	if (effect == nullptr || effect->_miDelFlag)
		return nullptr;
	// The carrier's own anchor is the acid cloud's. An RfA-27 sheet takes its delivery anchor (and a flat one lies on the
	// floor); every other sheet is {0, 0} and its caller sets what it needs.
	effect->position.offset = ArtEffectAnchor(art);
	effect->_miPreFlag = ArtEffectOnFloor(art);
	return effect;
}

Missile *AddArtEffectFacing(Point tile, MissileGraphicID art, int playerId, int dir16, int ticks)
{
	Missile *effect = AddArtEffect(tile, art, playerId, ticks);
	if (effect != nullptr)
		SetMissDir(*effect, std::clamp(dir16, 0, 15)); // the row; the length and the delay are every row's
	return effect;
}

void ArtEffectFollowsItsCaster(Missile &effect)
{
	effect.var1 = ArtEffectFollowsCaster;
}

void EndArtEffects(Point tile, MissileGraphicID art, int playerId)
{
	for (Missile &missile : Missiles) {
		if (missile._mitype == ArtEffectCarrier && missile._miAnimType == art && missile._misource == playerId
		    && missile.position.tile == tile)
			missile._miDelFlag = true;
	}
}

Missile *AddArtBolt(Point from, Point to, MissileGraphicID art, int playerId, int speed, MissileGraphicID arrivalArt, oracool::ClassTreeSkill impactSkill)
{
	if (!MissileArtLoaded(art) || !InDungeonBounds(from) || !InDungeonBounds(to))
		return nullptr;
	CensusArtOverride = art;
	ArtBoltSpeed = std::max(speed, 1);
	Missile *bolt = AddMissile(from, to, from == to ? Direction::South : GetDirection(from, to), ArtBoltCarrier, TARGET_MONSTERS, playerId, 0, 0);
	CensusArtOverride = MissileGraphicID::None;
	ArtBoltSpeed = 0;
	if (bolt == nullptr || bolt->_miDelFlag)
		return nullptr;
	// What it leaves where it lands (var4) and the cue it lands with (var3); both 0, nothing, for the javelin's own flights.
	bolt->var4 = arrivalArt < MissileGraphicID::None ? static_cast<int>(arrivalArt) + 1 : 0;
	bolt->var3 = impactSkill != oracool::ClassTreeSkill::None ? static_cast<int>(impactSkill) + 1 : 0;
	return bolt;
}

Missile *AddCreatureBolt(Point from, Point to, const CMonster &creature, int playerId, int speed, MissileGraphicID arrivalArt)
{
	if (from == to)
		return nullptr;
	const AnimStruct &walk = creature.getAnimData(MonsterGraphic::Walk);
	const OptionalClxSpriteList sprites = walk.spritesForDirection(GetDirection(from, to));
	if (!sprites || sprites->numSprites() == 0)
		return nullptr;
	// The arrival sheet carries the flight (AddArtBolt wants a loaded sheet), and the creature's walk is worn over it.
	Missile *bolt = AddArtBolt(from, to, arrivalArt, playerId, speed, arrivalArt);
	if (bolt == nullptr)
		return nullptr;
	const uint16_t width = (*sprites)[0].width();
	bolt->_miAnimData.emplace(*sprites);
	bolt->oracoolColours = nullptr; // the creature's own sprites, in the level palette
	bolt->_miAnimFlags = MissileGraphicsFlags::None;
	bolt->_miAnimDelay = std::max<int>(walk.rate, 1);
	bolt->_miAnimLen = std::max<int>(walk.frames, 1);
	bolt->_miAnimCnt = 0;
	bolt->_miAnimFrame = 1;
	bolt->_miAnimAdd = 1;
	bolt->_miAnimWidth = width;
	bolt->_miAnimWidth2 = CalculateWidth2(width);
	return bolt;
}

void AddColdHitFlash(Point tile, int playerId, int percent)
{
	if (!MissileArtLoaded(MissileGraphicID::HitCold) || !InDungeonBounds(tile))
		return;
	Missile *flash = AddMissile(tile, { WeaponExplosionColdFlash, 0 }, Direction::South, MissileID::WeaponExplosion, TARGET_MONSTERS, playerId, 0, 0);
	if (flash != nullptr && percent != 100)
		ScaleMissile(*flash, percent);
}

void ProcessCensusEffect(Missile &missile)
{
	missile._mirange--;
	if (missile._mirange <= 0) {
		missile._miDelFlag = true;
		if (missile._mlid != NO_LIGHT)
			AddUnLight(missile._mlid);
		return;
	}
	if (missile._mlid == NO_LIGHT && IsAnyOf(missile._mitype, MissileID::MeteorImpact, MissileID::ThunderBolt))
		missile._mlid = AddLight(missile.position.tile, 8);
	if (missile._mitype == MissileID::BoneWallEffect && missile._miAnimFrame >= BoneWallStandFrame) {
		if (missile._miAnimFrame >= missile._miAnimLen && missile._miAnimCnt + 1 >= missile._miAnimDelay)
			missile._miAnimFrame = BoneWallStandFrame - 1;
	}
	// The bone storm follows its caster (rfa12_actives moves the field the same way), as does an AddArtEffect sheet
	// marked to (the cold armour's break).
	if ((missile._mitype == MissileID::BoneStormEffect || (missile._mitype == ArtEffectCarrier && missile.var1 == ArtEffectFollowsCaster))
	    && missile._micaster == TARGET_MONSTERS && missile._misource >= 0 && static_cast<size_t>(missile._misource) < Players.size()) {
		const Point here = Players[missile._misource].position.tile;
		missile.position.tile = here;
		missile.position.start = here;
	}
	if (missile._mitype == MissileID::MeteorImpact && missile._miAnimFrame >= MeteorImpactBurnFrame) {
		missile._miAnimDelay = 3; // the embers flicker slower than the burst
		// On the last frame, about to step: step back to the burn's first frame instead of the burst's.
		if (missile._miAnimFrame >= missile._miAnimLen && missile._miAnimCnt + 1 >= missile._miAnimDelay)
			missile._miAnimFrame = MeteorImpactBurnFrame - 1;
	}
	PutMissile(missile);
}

/**
 * @brief Oracool (2026-09-14, RfA-16): Poison and Plague Javelin's javelin, flying from the Rogue to where
 * the skill already struck. Drawn only. Sixteen facings, like the arrows.
 */
void AddAcidJavelin(Missile &missile, AddMissileParameter &parameter)
{
	// The missile's OWN sheet: the Necromancer's three bolts fly this way too, and asked for the javelin's until
	// 2026-09-26 - so a build without acid_javelin.png lost all three, with sheets of their own in the archive.
	// AddArtBolt's sheet (RfA-27) rides this row the way AddArtEffect's rides the cloud's.
	if (CensusArtOverride != MissileGraphicID::None)
		SetMissAnim(missile, CensusArtOverride);
	if (!MissileArtLoaded(missile._miAnimType)) {
		missile._miDelFlag = true;
		return;
	}
	Point dst = parameter.dst;
	if (missile.position.start == dst)
		dst += parameter.midir;
	const int speed = ArtBoltSpeed > 0 ? ArtBoltSpeed : 32;
	UpdateMissileVelocity(missile, dst, speed);
	SetMissDir(missile, GetDirection16(missile.position.start, dst));
	missile.var1 = dst.x;
	missile.var2 = dst.y;
	// Arrow speed covers a tile in a tick or two; the destination check below normally ends it first. A slower
	// AddArtBolt (a rolling wave) gets the ticks its speed needs, in the same proportion.
	missile._mirange = std::max(64 / speed, 2) * missile.position.start.WalkingDistance(dst) + 4;
}

namespace {

/** @brief AddArtBolt's landing: the sheet it leaves (var4) and the cue it lands with (var3). Nothing for the javelin's own. */
void ArtBoltLands(const Missile &missile)
{
	const Point dst { missile.var1, missile.var2 };
	if (missile.var4 > 0 && missile.var4 <= static_cast<int>(MissileGraphicID::None)) {
		// What it leaves wears its tint (v1.12.211: the Army's green skeletons burst into green bone).
		if (Missile *left = AddArtEffect(dst, static_cast<MissileGraphicID>(missile.var4 - 1), missile._misource); left != nullptr) {
			left->oracoolTint = missile.oracoolTint;
			left->oracoolTintRgb = missile.oracoolTintRgb;
		}
	}
	if (missile.var3 > 0)
		oracool::PlaySkillSound(static_cast<oracool::ClassTreeSkill>(missile.var3 - 1), oracool::SkillSoundEvent::Impact);
}

} // namespace

void ProcessAcidJavelin(Missile &missile)
{
	missile._mirange--;
	MoveMissile(missile, [](Point) { return true; }); // nothing stops it: the blow has already landed
	if (missile._mirange <= 0 || missile.position.tile == Point { missile.var1, missile.var2 }) {
		missile._miDelFlag = true;
		ArtBoltLands(missile);
		return;
	}
	PutMissile(missile);
}

/**
 * @brief Oracool (2026-09-11): the shockwave a cry leaves on the floor. Drawn, never hits - the cry's
 * effect is CastWarcry's. Plays its twelve frames once under the crier, then goes.
 */
void AddWarcryRing(Missile &missile, AddMissileParameter & /*parameter*/)
{
	if (!MissileArtLoaded(MissileGraphicID::WarcryRing)) {
		missile._miDelFlag = true; // warcry_ring.png not delivered yet: the cry stays unseen, as before
		return;
	}
	missile._miPreFlag = true; // on the floor, under whoever stands in it
	// A sprite hangs from its tile by its bottom edge, and the ring's ellipse is centred in a 160px
	// cell - left there, it would float 80px up, round the crier's chest (Frost Nova's art sits that
	// high on purpose; this one is a floor wave). 64 puts its centre on the tile's centre, 16px above
	// the bottom edge. A still missile renders at exactly this offset (UpdateMissileRendererData).
	missile.position.offset = { 0, 64 };
	missile._mirange = missile._miAnimLen;
	// Tinted for the skill that raised it and colour-cycled as it grows (user, 2026-09-27: "tint appropriately according
	// to the skill utilizing it. apply colorcycling to it as it grows in diameter"). oracoolSkill is stamped before this
	// runs; a ring raised outside a cast (a field's pulse, a companion) takes the warm neutral hue.
	missile.oracoolTint = oracool::Tint::HueCycle;
	missile.oracoolTintRgb = oracool::RingHueForSkill(missile.oracoolSkill);
}

void ProcessWarcryRing(Missile &missile)
{
	missile._mirange--;
	if (missile._mirange <= 0) {
		missile._miDelFlag = true;
		return;
	}
	PutMissile(missile);
}

namespace {

/** @brief Blessed Shield's hit flash is holyexpl at this share of its size (user: "maybe scaled down a bit"). */
constexpr unsigned BlessedShieldImpactPercent = 60;

/**
 * @brief The scaled flash, built once from the loaded holyexpl. It owns its pixels, so a level change
 * that frees and reloads the missile graphics it was scaled from leaves it whole.
 */
std::optional<OwnedClxSpriteList> BlessedShieldImpactSprites;

} // namespace

/**
 * @brief Oracool (2026-09-11): the flash on each Blessed Shield strike - "confirmation feedback in the
 * form of small short animation. Effect - HolyBoltExplosion (holyexpl) - this one looks adequate, maybe
 * scaled down a bit". Holy Bolt's own burst at BlessedShieldImpactPercent of its size, eight frames at
 * one a tick, lit by ProcessMissileExplosion. It never hits: the shield dealt the damage.
 */
void AddBlessedShieldImpact(Missile &missile, AddMissileParameter & /*parameter*/)
{
	missile._mirange = missile._miAnimLen;
	if (!missile._miAnimData)
		return; // headless, or holyexpl not loaded: an unseen flash still times out
	if (!BlessedShieldImpactSprites)
		BlessedShieldImpactSprites = oracool::ScaleClxList(*missile._miAnimData, BlessedShieldImpactPercent);
	const int fullHeight = (*missile._miAnimData)[0].height();
	const ClxSpriteList scaled { *BlessedShieldImpactSprites };
	missile._miAnimData = scaled;
	missile._miAnimLen = static_cast<int>(scaled.numSprites());
	missile._miAnimWidth = scaled[0].width();
	missile._miAnimWidth2 = CalculateWidth2(missile._miAnimWidth);
	// A sprite hangs from its tile by its bottom edge, so a smaller one would sit lower on the monster.
	// Lifted by half the height it lost, its centre stays where Holy Bolt's full-size burst puts it.
	missile.position.offset = { 0, -(fullHeight - static_cast<int>(scaled[0].height())) / 2 };
	missile._mirange = missile._miAnimLen;
}

/**
 * @brief Oracool: Etherealize had no behaviour at all until now - see ProcessEtherealize.
 *
 * The effect itself was already fully wired: SpellFlag::Etherealize makes arrows pass through
 * (missiles.cpp's CheckMissileCol paths), monsters miss (monster.cpp's PlayerHit) and melee miss
 * (player.cpp's PlrHitPlr). Vanilla set the flag from nowhere and cleared it in InitMissiles, which
 * is the shape of a spell that was cut before its caster was written. This is that caster.
 */
void AddEtherealize(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];
	player._pSpellFlags |= SpellFlag::Etherealize;
	// 9 seconds at spell level 1 on the 20-tick clock (160 x 9/8 = 180 ticks).
	missile._mirange = EtherealizeDurationTicks(missile._mispllvl);
	RedrawEverything();
}

void AddFlameWaveControl(Missile &missile, AddMissileParameter &parameter)
{
	missile.var1 = parameter.dst.x;
	missile.var2 = parameter.dst.y;
	missile._mirange = 1;
	missile._miAnimFrame = 4;
}

void AddNova(Missile &missile, AddMissileParameter &parameter)
{
	missile.var1 = parameter.dst.x;
	missile.var2 = parameter.dst.y;

	if (!missile.IsTrap()) {
		Player &player = Players[missile._misource];
		int dmg = GenerateRndSum(6, 5) + player._pLevel + 5;
		missile._midam = ScaleSpellEffect(dmg / 2, missile._mispllvl);
	} else {
		missile._midam = (currlevel / 2) + GenerateRndSum(3, 3);
	}

	missile._mirange = 1;
}

void AddRage(Missile &missile, AddMissileParameter &parameter)
{
	Player &player = Players[missile._misource];

	if (HasAnyOf(player._pSpellFlags, SpellFlag::RageActive | SpellFlag::RageCooldown) || player._pHitPoints <= player._pLevel << 6) {
		missile._miDelFlag = true;
		parameter.spellFizzled = true;
		return;
	}

	int tmp = 3 * player._pLevel;
	tmp <<= 7;
	player._pSpellFlags |= SpellFlag::RageActive;
	missile.var2 = tmp;
	int lvl = player._pLevel * 2;
	missile._mirange = lvl + 10 * missile._mispllvl + 245;
	CalcPlrItemVals(player, true);
	RedrawEverything();
	player.Say(HeroSpeech::Aaaaargh);
}

void AddItemRepair(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	missile._miDelFlag = true;
	if (&player == MyPlayer) {
		if (sbookflag)
			sbookflag = false;
		if (!invflag) {
			invflag = true;
			if (ControlMode != ControlTypes::KeyboardAndMouse)
				FocusOnInventory();
		}
		NewCursor(CURSOR_REPAIR);
	}
}

void AddStaffRecharge(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	missile._miDelFlag = true;
	if (&player == MyPlayer) {
		if (sbookflag)
			sbookflag = false;
		if (!invflag) {
			invflag = true;
			if (ControlMode != ControlTypes::KeyboardAndMouse)
				FocusOnInventory();
		}
		NewCursor(CURSOR_RECHARGE);
	}
}

void AddTrapDisarm(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	missile._miDelFlag = true;
	if (&player == MyPlayer) {
		NewCursor(CURSOR_DISARM);
		if (ControlMode != ControlTypes::KeyboardAndMouse) {
			if (ObjectUnderCursor != nullptr)
				NetSendCmdLoc(MyPlayerId, true, CMD_DISARMXY, cursPosition);
			else
				NewCursor(CURSOR_HAND);
		}
	}
}

void AddApocalypse(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	missile.var1 = 8;
	missile.var2 = std::max(missile.position.start.y - 8, 1);
	missile.var3 = std::min(missile.position.start.y + 8, MAXDUNY - 1);
	missile.var4 = std::max(missile.position.start.x - 8, 1);
	missile.var5 = std::min(missile.position.start.x + 8, MAXDUNX - 1);
	missile.var6 = missile.var4;
	int playerLevel = player._pLevel;
	missile._midam = GenerateRndSum(6, playerLevel) + playerLevel;
	missile._mirange = 255;
}

void AddInferno(Missile &missile, AddMissileParameter &parameter)
{
	missile.var2 = 5 * missile._midam;
	missile.position.start = parameter.dst;

	SyncPositionWithParent(missile, parameter);

	missile._mirange = missile.var2 + 20;
	missile._mlid = AddLight(missile.position.start, 1);
	if (missile._micaster == TARGET_MONSTERS) {
		int i = GenerateRnd(Players[missile._misource]._pLevel) + GenerateRnd(2);
		missile._midam = 8 * i + 16 + ((8 * i + 16) / 2);
	} else {
		int minDamage = 0;
		int maxDamage = 0;
		if (!LiveMonsterDamageRange(missile, minDamage, maxDamage)) {
			minDamage = currlevel; // a caster gone: a trap's, as the arrows fall back (round 29 audit)
			maxDamage = 2 * currlevel;
		}
		missile._midam = minDamage + GenerateRnd(maxDamage - minDamage + 1);
	}
}

void AddInfernoControl(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == parameter.dst) {
		dst += parameter.midir;
	}
	UpdateMissileVelocity(missile, dst, 32);
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	missile._mirange = 256;
}

void AddChargedBolt(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	missile._mirnd = GenerateRnd(15) + 1;
	missile._midam = (missile._micaster == TARGET_MONSTERS) ? (GenerateRnd(Players[missile._misource]._pMagic / 4) + 1) : 15;

	if (missile.position.start == dst) {
		dst += parameter.midir;
	}
	missile._miAnimFrame = GenerateRnd(8) + 1;
	missile._mlid = AddLight(missile.position.start, 5);

	UpdateMissileVelocity(missile, dst, 8);
	missile.var1 = 5;
	missile.var2 = static_cast<int>(parameter.midir);
	missile._mirange = 256;
}

void AddHolyBolt(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == dst) {
		dst += parameter.midir;
	}
	int sp = 16;
	if (!missile.IsTrap()) {
		sp = HolyBoltSpeedAtLevel(missile._mispllvl);
	}

	Player &player = Players[missile._misource];

	UpdateMissileVelocity(missile, dst, sp);
	SetMissDir(missile, GetDirection16(missile.position.start, dst));
	missile._mirange = 256;
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	missile._mlid = AddLight(missile.position.start, 8);
	missile._midam = GenerateRnd(10) + player._pLevel + 9;
}

void AddResurrect(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	if (&player == MyPlayer) {
		NewCursor(CURSOR_RESURRECT);
		if (ControlMode != ControlTypes::KeyboardAndMouse)
			TryIconCurs();
	}
	missile._miDelFlag = true;
}

void AddResurrectBeam(Missile &missile, AddMissileParameter &parameter)
{
	missile.position.tile = parameter.dst;
	missile.position.start = parameter.dst;
	missile._mirange = GetMissileSpriteData(MissileGraphicID::Resurrect).animLen(0);
}

void AddTelekinesis(Missile &missile, AddMissileParameter & /*parameter*/)
{
	Player &player = Players[missile._misource];

	missile._miDelFlag = true;
	if (&player == MyPlayer)
		NewCursor(CURSOR_TELEKINESIS);
}

void AddBoneSpirit(Missile &missile, AddMissileParameter &parameter)
{
	Point dst = parameter.dst;
	if (missile.position.start == dst) {
		dst += parameter.midir;
	}
	UpdateMissileVelocity(missile, dst, 16);
	SetMissDir(missile, GetDirection(missile.position.start, dst));
	missile._mirange = 256;
	missile.var1 = missile.position.start.x;
	missile.var2 = missile.position.start.y;
	missile.var4 = dst.x;
	missile.var5 = dst.y;
	missile._mlid = AddLight(missile.position.start, 8);
}

void AddRedPortal(Missile &missile, AddMissileParameter & /*parameter*/)
{
	missile._mirange = 100;
	missile.var1 = 100 - missile._miAnimLen;
	PutMissile(missile);
}

void AddRiftPortal(Missile &missile, AddMissileParameter & /*parameter*/)
{
	// Oracool (2026-09-20): stands until the Stonegate closes - no range, no close animation;
	// oracool::CloseStonegate releases the light and flags it. The art is VANILLA's town portal
	// recoloured (user: "i want to use vanilla portal animation"): file 0 is the opening blossom,
	// file 1 the standing loop, and ProcessRiftPortal switches to file 1 once the opening has run,
	// as ProcessTownPortal does by its countdown.
	missile._mirange = 1;
	// Lifted 27px into the arch of the Rift Monument painting (2026-09-20): the missile is bottom-anchored
	// on the gate's tile like the object, and the painting's opening floor sits well above its footprint
	// (the plinth is in front), so unlifted the portal stood in the plinth. Measured with
	// tools/ScalePainting.ps1's opening readout and a composite preview.
	// Only in the monument (dev note, 2026-09-30): a rift's own exit stands on its floor like any portal, not 27px over it.
	if (leveltype == DTYPE_TOWN)
		missile.position.offset = { 0, -27 }; // 27 = the 20 of the first fit plus the user's "move up 7px" on the 90% sheet
	missile._mlid = AddLight(missile.position.tile, 6);
	PutMissile(missile);
}

void AddDiabloApocalypse(Missile &missile, AddMissileParameter & /*parameter*/)
{
	for (const Player &player : Players) {
		if (!player.plractive)
			continue;
		if (!LineClearMissile(missile.position.start, player.position.future))
			continue;

		AddMissile({ 0, 0 }, player.position.future, Direction::South, MissileID::DiabloApocalypseBoom, missile._micaster, missile._misource, missile._midam, 0);
	}
	missile._miDelFlag = true;
}

Missile *AddMissile(Point src, Point dst, Direction midir, MissileID mitype,
    mienemy_type micaster, int id, int midam, int spllvl,
    Missile *parent, std::optional<_sfx_id> lSFX)
{
	if (Missiles.size() >= Missiles.max_size()) {
		return nullptr;
	}

	Missiles.emplace_back(Missile {});
	auto &missile = Missiles.back();

	const MissileData &missileData = GetMissileData(mitype);

	missile._mitype = mitype;
	missile._micaster = micaster;
	missile._misource = id;
	missile._midam = midam;
	missile._mispllvl = spllvl;
	missile.position.tile = src;
	missile.position.start = src;
	missile._miAnimAdd = 1;
	missile._miAnimType = missileData.mFileNum;
	// A monster's own sheet (Acid, its splash and puddle) loads only with that monster type: the Necromancer's poison
	// mage shot invisible bolts on every floor without acid beasts (round 9 audit, v1.12.234). Loaded here on first use;
	// FreeMissileGFX frees it with the level as it does the monsters'.
	if (MissileFileData &art = GetMissileSpriteData(missileData.mFileNum);
	    !HeadlessMode && !art.sprites && art.flags == MissileGraphicsFlags::MonsterOwned)
		art.LoadGFX();
	missile._miDrawFlag = missileData.isDrawn();
	missile._mlid = NO_LIGHT;
	missile.lastCollisionTargetHash = 0;
	// Oracool: whose cast this is. None outside a class-skill cast, which is most missiles in the
	// game - traps, monster attacks, town portals - and they simply carry no cue.
	// The general impact cue was removed on 2026-09-03 (it layered a second sound over every spell);
	// since 2026-09-11 the cold armours read it back, to play their own start and stop cues in place
	// of the Mana Shield sound they borrowed - see oracool::PlayColdMissileSound.
	missile.oracoolSkill = static_cast<uint16_t>(oracool::CurrentCastSkill());

	if (!missile.IsTrap() && micaster == TARGET_PLAYERS) {
		Monster &monster = Monsters[id];
		if (monster.isUnique()) {
			missile._miUniqTrans = monster.uniqTrans + 1;
		}
		missile.sourceMinion = monster.isPlayerMinion(); // its side, fixed now (Missile::sourceMinion)
		missile.sourceSpawnSerial = monster.spawnSerial; // and which spawn fired it (Missile::liveSourceMonster)
	}

	if (missile._miAnimType == MissileGraphicID::None || GetMissileSpriteData(missile._miAnimType).animFAmt < 8)
		SetMissDir(missile, 0);
	else
		SetMissDir(missile, midir);

	if (!lSFX) {
		lSFX = missileData.mlSFX;
		// Oracool: a cold missile launches with its own cue INSTEAD of the Firebolt / Nova / Mana
		// Shield sound its row borrows - one sound either way. See oracool::ColdMissileCueSkill.
		if (*lSFX != SFX_NONE && (oracool::PlayColdMissileSound(missile, /*impact=*/false) || oracool::PlayPaladinMissileSound(missile)))
			lSFX = SFX_NONE;
	}

	if (*lSFX != SFX_NONE) {
		PlaySfxLoc(*lSFX, missile.position.start);
	}

	// Oracool: mAddProc is null for the missiles vanilla defined but never spawned, and this call was
	// unconditional - so the first spell we made castable that pointed at one of them (Etherealize)
	// crashed on a null function pointer rather than doing nothing. Treat "no behaviour" as a fizzle:
	// nothing to run, nothing to keep, and no way for the next such missile to take the game down.
	AddMissileParameter parameter = { dst, midir, parent, false };
	if (missileData.mAddProc == nullptr) {
		missile._miDelFlag = true;
		return nullptr;
	}
	missileData.mAddProc(missile, parameter);
	if (parameter.spellFizzled) {
		return nullptr;
	}

	return &missile;
}

void ProcessElementalArrow(Missile &missile)
{
	missile._mirange--;
	if (missile._miAnimType == MissileGraphicID::ChargedBolt || missile._miAnimType == MissileGraphicID::MagmaBallExplosion) {
		ChangeLight(missile._mlid, missile.position.tile, missile._miAnimFrame + 5);
	} else {
		int mind;
		int maxd;
		int p = missile._misource;
		missile._midist++;
		if (!missile.IsTrap()) {
			if (missile._micaster == TARGET_MONSTERS) {
				// BUGFIX: damage of missile should be encoded in missile struct; player can be dead/have left the game before missile arrives.
				const Player &player = Players[p];
				mind = player._pIMinDam;
				maxd = player._pIMaxDam;
			} else {
				// While the slot still holds the archer, with its pack's Might (round 28 audit); after, as a trap's.
				if (!LiveMonsterDamageRange(missile, mind, maxd)) {
					mind = currlevel;
					maxd = 2 * currlevel;
				}
			}
		} else {
			mind = GenerateRnd(10) + 1 + currlevel;
			maxd = GenerateRnd(10) + 1 + currlevel * 2;
		}
		MoveMissileAndCheckMissileCol(missile, DamageType::Physical, mind, maxd, true, false);
		if (missile._mirange == 0) {
			missile._mimfnum = 0;
			missile._mirange = missile._miAnimLen - 1;
			missile.position.StopMissile();

			int eMind;
			int eMaxd;
			MissileGraphicID eAnim;
			DamageType damageType;
			switch (missile._mitype) {
			case MissileID::LightningArrow:
				if (!missile.IsTrap()) {
					// BUGFIX: damage of missile should be encoded in missile struct; player can be dead/have left the game before missile arrives.
					const Player &player = Players[p];
					eMind = player._pILMinDam;
					eMaxd = player._pILMaxDam;
				} else {
					eMind = GenerateRnd(10) + 1 + currlevel;
					eMaxd = GenerateRnd(10) + 1 + currlevel * 2;
				}
				eAnim = MissileGraphicID::ChargedBolt;
				damageType = DamageType::Lightning;
				break;
			case MissileID::FireArrow:
				if (!missile.IsTrap()) {
					// BUGFIX: damage of missile should be encoded in missile struct; player can be dead/have left the game before missile arrives.
					const Player &player = Players[p];
					eMind = player._pIFMinDam;
					eMaxd = player._pIFMaxDam;
				} else {
					eMind = GenerateRnd(10) + 1 + currlevel;
					eMaxd = GenerateRnd(10) + 1 + currlevel * 2;
				}
				eAnim = MissileGraphicID::MagmaBallExplosion;
				damageType = DamageType::Fire;
				break;
			default:
				app_fatal(StrCat("wrong missile ID ", static_cast<int>(missile._mitype)));
				break;
			}
			SetMissAnim(missile, eAnim);
			CheckMissileCol(missile, damageType, eMind, eMaxd, false, missile.position.tile, true);
			// A hero's fire arrow carries the lightning as well, when the bow has both from sockets, shards or Enchant
			// (DoRangeAttack sends one fire arrow for the pair; round 12 audit, v1.12.237).
			if (missile._mitype == MissileID::FireArrow && !missile.IsTrap() && missile._micaster == TARGET_MONSTERS && !gbIsMultiplayer) {
				const Player &player = Players[p];
				if (player._pILMaxDam > 0)
					CheckMissileCol(missile, DamageType::Lightning, player._pILMinDam, player._pILMaxDam, false, missile.position.tile, true);
			}
		} else {
			if (missile.position.tile != Point { missile.var1, missile.var2 }) {
				missile.var1 = missile.position.tile.x;
				missile.var2 = missile.position.tile.y;
				ChangeLight(missile._mlid, missile.position.tile, 5);
			}
		}
	}
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	}
	PutMissile(missile);
}

void ProcessArrow(Missile &missile)
{
	missile._mirange--;
	missile._midist++;

	int mind;
	int maxd;
	switch (missile.sourceType()) {
	case MissileSource::Player: {
		// BUGFIX: damage of missile should be encoded in missile struct; player can be dead/have left the game before missile arrives.
		const Player &player = *missile.sourcePlayer();
		mind = player._pIMinDam;
		maxd = player._pIMaxDam;
	} break;
	case MissileSource::Monster: {
		// BUGFIX: damage of missile should be encoded in missile struct; monster can be dead before missile arrives.
		// Oracool: while the slot still holds the archer (round 12 audit); after, the arrow lands as a trap's.
		// With the pack's Might (round 28 audit: round 11 reached only the missiles that read _midam).
		if (!LiveMonsterDamageRange(missile, mind, maxd)) {
			mind = currlevel;
			maxd = 2 * currlevel;
		}
	} break;
	case MissileSource::Trap:
		mind = currlevel;
		maxd = 2 * currlevel;
		break;
	}
	MoveMissileAndCheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), mind, maxd, true, false);
	if (missile._mirange == 0)
		missile._miDelFlag = true;
	PutMissile(missile);
}

void ProcessGenericProjectile(Missile &missile)
{
	missile._mirange--;

	MoveMissileAndCheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, true, true);
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		Point dst = { 0, 0 };
		auto dir = static_cast<Direction>(missile._mimfnum);
		switch (missile._mitype) {
		case MissileID::Firebolt:
		case MissileID::MagmaBall:
			AddMissile(missile.position.tile, dst, dir, MissileID::MagmaBallExplosion, missile._micaster, missile._misource, 0, 0, &missile);
			break;
		// Oracool: the cold impacts. Ice Bolt fell through to `default` in Round 1 and landed with no
		// burst at all; the brief's ice_impact sheet is what it was always meant to land with.
		case MissileID::IceBolt:
		case MissileID::IceBlast:
			if (Missile *impact = AddMissile(missile.position.tile, dst, dir, MissileID::IceImpact, missile._micaster, missile._misource, 0, 0, &missile);
			    impact != nullptr && missile.oracoolImpactPercent != 100)
				ScaleMissile(*impact, missile.oracoolImpactPercent); // the sentinel's and the orb's small splash (dev notes, 2026-10-01)
			break;
		case MissileID::GlacialSpike:
			AddMissile(missile.position.tile, dst, dir, MissileID::GlacialShatter, missile._micaster, missile._misource, 0, missile._mispllvl, &missile);
			break;
		case MissileID::BloodStar:
			AddMissile(missile.position.tile, dst, dir, MissileID::BloodStarExplosion, missile._micaster, missile._misource, 0, 0, &missile);
			break;
		case MissileID::Acid:
			AddMissile(missile.position.tile, dst, dir, MissileID::AcidSplat, missile._micaster, missile._misource, 0, 0, &missile);
			break;
		case MissileID::OrangeFlare:
			AddMissile(missile.position.tile, dst, dir, MissileID::OrangeExplosion, missile._micaster, missile._misource, 0, 0, &missile);
			break;
		case MissileID::BlueFlare:
			AddMissile(missile.position.tile, dst, dir, MissileID::BlueExplosion, missile._micaster, missile._misource, 0, 0, &missile);
			break;
		case MissileID::RedFlare:
			AddMissile(missile.position.tile, dst, dir, MissileID::RedExplosion, missile._micaster, missile._misource, 0, 0, &missile);
			break;
		case MissileID::YellowFlare:
			AddMissile(missile.position.tile, dst, dir, MissileID::YellowExplosion, missile._micaster, missile._misource, 0, 0, &missile);
			break;
		case MissileID::BlueFlare2:
			AddMissile(missile.position.tile, dst, dir, MissileID::BlueExplosion2, missile._micaster, missile._misource, 0, 0, &missile);
			break;
		default:
			break;
		}
		if (missile._mlid != NO_LIGHT)
			AddUnLight(missile._mlid);
		PutMissile(missile);
	} else {
		if (missile.position.tile != Point { missile.var1, missile.var2 }) {
			missile.var1 = missile.position.tile.x;
			missile.var2 = missile.position.tile.y;
			if (missile._mlid != NO_LIGHT)
				ChangeLight(missile._mlid, missile.position.tile, 8);
		}
		PutMissile(missile);
	}
}

void ProcessNovaBall(Missile &missile)
{
	Point targetPosition = { missile.var1, missile.var2 };
	missile._mirange--;
	int j = missile._mirange;
	MoveMissileAndCheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, false, false);
	if (missile._miHitFlag)
		missile._mirange = j;

	if (missile.position.tile == targetPosition) {
		Object *object = FindObjectAtPosition(targetPosition);
		if (object != nullptr && object->IsShrine()) {
			missile._mirange = j;
		}
	}
	if (missile._mirange == 0)
		missile._miDelFlag = true;
	PutMissile(missile);
}

void ProcessAcidPuddle(Missile &missile)
{
	missile._mirange--;
	int range = missile._mirange;
	CheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, true, missile.position.tile, false);
	missile._mirange = range;
	if (range == 0) {
		if (missile._mimfnum != 0) {
			missile._miDelFlag = true;
		} else {
			SetMissDir(missile, 1);
			missile._mirange = missile._miAnimLen;
		}
	}
	PutMissile(missile);
}

void ProcessFireWall(Missile &missile)
{
	constexpr int ExpLight[14] = { 2, 3, 4, 5, 5, 6, 7, 8, 9, 10, 11, 12, 12 };

	missile._mirange--;
	if (missile._mirange == missile.var1) {
		SetMissDir(missile, 1);
		missile._miAnimFrame = GenerateRnd(11) + 1;
	}
	if (missile._mirange == missile._miAnimLen - 1) {
		SetMissDir(missile, 0);
		missile._miAnimFrame = 13;
		missile._miAnimAdd = -1;
	}
	CheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, true, missile.position.tile, true);
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	}
	if (missile._mimfnum != 0 && missile._mirange != 0 && missile._miAnimAdd != -1 && missile.var2 < 12) {
		if (missile.var2 == 0)
			missile._mlid = AddLight(missile.position.tile, ExpLight[0]);
		ChangeLight(missile._mlid, missile.position.tile, ExpLight[missile.var2]);
		missile.var2++;
	}
	PutMissile(missile);
}

void ProcessFireball(Missile &missile)
{
	missile._mirange--;

	if (missile._miAnimType == MissileGraphicID::BigExplosion) {
		if (missile._mirange == 0) {
			missile._miDelFlag = true;
			AddUnLight(missile._mlid);
		}
	} else {
		int minDam = missile._midam;
		int maxDam = missile._midam;

		// The live caster with its pack's Might (round 28 audit); a caster gone keeps the damage it was launched with.
		if (missile._micaster != TARGET_MONSTERS)
			LiveMonsterDamageRange(missile, minDam, maxDam);
		const DamageType damageType = GetMissileData(missile._mitype).damageType();
		MoveMissileAndCheckMissileCol(missile, damageType, minDam, maxDam, true, false);
		if (missile._mirange == 0) {
			const Point missilePosition = missile.position.tile;
			ChangeLight(missile._mlid, missile.position.tile, missile._miAnimFrame);

			constexpr Direction Offsets[] = {
				Direction::NoDirection,
				Direction::SouthWest,
				Direction::NorthEast,
				Direction::SouthEast,
				Direction::East,
				Direction::South,
				Direction::NorthWest,
				Direction::West,
				Direction::North
			};
			for (Direction offset : Offsets) {
				if (!CheckBlock(missile.position.start, missilePosition + offset))
					CheckMissileCol(missile, damageType, minDam, maxDam, false, missilePosition + offset, true);
			}

			if (!TransList[dTransVal[missilePosition.x][missilePosition.y]]
			    || (missile.position.velocity.deltaX < 0 && ((TransList[dTransVal[missilePosition.x][missilePosition.y + 1]] && TileHasAny(dPiece[missilePosition.x][missilePosition.y + 1], TileProperties::Solid)) || (TransList[dTransVal[missilePosition.x][missilePosition.y - 1]] && TileHasAny(dPiece[missilePosition.x][missilePosition.y - 1], TileProperties::Solid))))) {
				missile.position.tile += Displacement { 1, 1 };
				missile.position.offset.deltaY -= 32;
			}
			if (missile.position.velocity.deltaY > 0
			    && ((TransList[dTransVal[missilePosition.x + 1][missilePosition.y]] && TileHasAny(dPiece[missilePosition.x + 1][missilePosition.y], TileProperties::Solid))
			        || (TransList[dTransVal[missilePosition.x - 1][missilePosition.y]] && TileHasAny(dPiece[missilePosition.x - 1][missilePosition.y], TileProperties::Solid)))) {
				missile.position.offset.deltaY -= 32;
			}
			if (missile.position.velocity.deltaX > 0
			    && ((TransList[dTransVal[missilePosition.x][missilePosition.y + 1]] && TileHasAny(dPiece[missilePosition.x][missilePosition.y + 1], TileProperties::Solid))
			        || (TransList[dTransVal[missilePosition.x][missilePosition.y - 1]] && TileHasAny(dPiece[missilePosition.x][missilePosition.y - 1], TileProperties::Solid)))) {
				missile.position.offset.deltaX -= 32;
			}
			missile._mimfnum = 0;
			SetMissAnim(missile, MissileGraphicID::BigExplosion);
			missile._mirange = missile._miAnimLen - 1;
			missile.position.velocity = {};
		} else if (missile.position.tile != Point { missile.var1, missile.var2 }) {
			missile.var1 = missile.position.tile.x;
			missile.var2 = missile.position.tile.y;
			ChangeLight(missile._mlid, missile.position.tile, 8);
		}
	}

	PutMissile(missile);
}

void ProcessHorkSpawn(Missile &missile)
{
	missile._mirange--;
	CheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), 0, 0, false, missile.position.tile, false);
	if (missile._mirange <= 0) {
		missile._miDelFlag = true;

		std::optional<Point> spawnPosition = FindClosestValidPosition(
		    [](Point target) {
			    return !IsTileOccupied(target);
		    },
		    missile.position.tile, 0, 1);

		if (spawnPosition) {
			auto facing = static_cast<Direction>(missile.var1);
			Monster *monster = AddMonster(*spawnPosition, facing, 1, true);
			if (monster != nullptr) {
				M_StartStand(*monster, facing);
			}
		}
	} else {
		missile._midist++;
		missile.position.traveled += missile.position.velocity;
		UpdateMissilePos(missile);
	}
	PutMissile(missile);
}

void ProcessRune(Missile &missile)
{
	Point position = missile.position.tile;
	int mid = dMonster[position.x][position.y];
	int pid = dPlayer[position.x][position.y];
	// Not the hero's own army or companions (the blast spares them - the rune was wasted and could hit the hero beside
	// them), nor a townsperson's id in town (round 9 audit, v1.12.234).
	if (mid != 0
	    && (leveltype == DTYPE_TOWN || Monsters[abs(mid) - 1].isPlayerMinion() || oracool::IsCompanion(Monsters[abs(mid) - 1])))
		mid = 0;
	if (mid != 0 || pid != 0) {
		Point targetPosition = mid != 0 ? Monsters[abs(mid) - 1].position.tile : Players[abs(pid) - 1].position.tile;
		Direction dir = GetDirection(position, targetPosition);

		missile._miDelFlag = true;
		AddUnLight(missile._mlid);

		AddMissile(position, position, dir, static_cast<MissileID>(missile.var1), TARGET_BOTH, missile._misource, missile._midam, missile._mispllvl);
	}

	PutMissile(missile);
}

void ProcessLightningWall(Missile &missile)
{
	missile._mirange--;
	int range = missile._mirange;
	CheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, true, missile.position.tile, false);
	if (missile._miHitFlag)
		missile._mirange = range;
	if (missile._mirange == 0)
		missile._miDelFlag = true;
	PutMissile(missile);
}

void ProcessBigExplosion(Missile &missile)
{
	missile._mirange--;
	if (missile._mirange <= 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	}
	PutMissile(missile);
}

void ProcessLightningBow(Missile &missile)
{
	SpawnLightning(missile, missile._midam);
}

void ProcessRingOfFire(Missile &missile)
{
	missile._miDelFlag = true;
	int8_t src = missile._misource;
	uint8_t lvl = missile._micaster == TARGET_MONSTERS ? Players[src]._pLevel : currlevel;
	int dmg = 16 * (GenerateRndSum(10, 2) + lvl + 2) / 2;

	if (missile.limitReached)
		return;

	Crawl(3, [&](Displacement displacement) {
		Point target = Point { missile.var1, missile.var2 } + displacement;
		if (!InDungeonBounds(target))
			return false;
		int dp = dPiece[target.x][target.y];
		if (TileHasAny(dp, TileProperties::Solid))
			return false;
		if (IsObjectAtPosition(target))
			return false;
		if (!LineClearMissile(missile.position.tile, target))
			return false;
		if (TileHasAny(dp, TileProperties::BlockMissile)) {
			missile.limitReached = true;
			return true;
		}

		AddMissile(target, target, Direction::South, MissileID::FireWall, TARGET_BOTH, src, dmg, missile._mispllvl);
		return false;
	});
}

void ProcessSearch(Missile &missile)
{
	missile._mirange--;
	if (missile._mirange != 0)
		return;

	const Player &player = Players[missile._misource];

	missile._miDelFlag = true;
	PlaySfxLoc(IS_CAST7, player.position.tile);
	if (&player == MyPlayer)
		AutoMapShowItems = false;
}

void ProcessLightningWallControl(Missile &missile)
{
	missile._mirange--;
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		return;
	}

	int id = missile._misource;
	int lvl = !missile.IsTrap() ? Players[id]._pLevel : 0;
	int dmg = 16 * (GenerateRndSum(10, 2) + lvl + 2);

	{
		Point position = { missile.var1, missile.var2 };
		Point target = position + static_cast<Direction>(missile.var3);

		if (!missile.limitReached && GrowWall(id, position, target, MissileID::LightningWall, missile._mispllvl, dmg)) {
			missile.var1 = target.x;
			missile.var2 = target.y;
		} else {
			missile.limitReached = true;
		}
	}

	{
		Point position = { missile.var5, missile.var6 };
		Point target = position + static_cast<Direction>(missile.var4);

		if (missile.var7 == 0 && GrowWall(id, position, target, MissileID::LightningWall, missile._mispllvl, dmg)) {
			missile.var5 = target.x;
			missile.var6 = target.y;
		} else {
			missile.var7 = 1;
		}
	}
}

void ProcessNovaCommon(Missile &missile, MissileID projectileType)
{
	int id = missile._misource;
	int dam = missile._midam;
	Point src = missile.position.tile;
	Direction dir = Direction::South;
	mienemy_type en = TARGET_PLAYERS;
	if (!missile.IsTrap()) {
		dir = Players[id]._pdir;
		en = TARGET_MONSTERS;
	}

	constexpr std::array<WorldTileDisplacement, 9> quarterRadius = { { { 4, 0 }, { 4, 1 }, { 4, 2 }, { 4, 3 }, { 4, 4 }, { 3, 4 }, { 2, 4 }, { 1, 4 }, { 0, 4 } } };
	for (WorldTileDisplacement quarterOffset : quarterRadius) {
		// This ends up with two missiles targeting offsets 4,0, 0,4, -4,0, 0,-4.
		std::array<WorldTileDisplacement, 4> offsets { quarterOffset, quarterOffset.flipXY(), quarterOffset.flipX(), quarterOffset.flipY() };
		for (WorldTileDisplacement offset : offsets)
			AddMissile(src, src + offset, dir, projectileType, en, id, dam, missile._mispllvl);
	}
	missile._mirange--;
	if (missile._mirange == 0)
		missile._miDelFlag = true;
}

void ProcessImmolation(Missile &missile)
{
	ProcessNovaCommon(missile, MissileID::FireballBow);
}

void ProcessNova(Missile &missile)
{
	ProcessNovaCommon(missile, MissileID::NovaBall);
}

void ProcessSpectralArrow(Missile &missile)
{
	int id = missile._misource;
	int dam = missile._midam;
	Point src = missile.position.tile;
	Point dst = { missile.var1, missile.var2 };
	int spllvl = missile.var3;
	MissileID mitype = MissileID::Arrow;
	Direction dir = Direction::South;
	mienemy_type micaster = TARGET_PLAYERS;
	if (!missile.IsTrap()) {
		const Player &player = Players[id];
		dir = player._pdir;
		micaster = TARGET_MONSTERS;

		// The kind is the bow's own (its _iLMinDam, set by IPL_FIREBALL / IPL_ADDACLIFE), not the hero's summed lightning
		// minimum: any other lightning source turned Flambeau's fireball into bolts or plain arrows (round 21 audit).
		int kind = 4;
		for (const Item *hand : { &player.InvBody[INVLOC_HAND_LEFT], &player.InvBody[INVLOC_HAND_RIGHT] }) {
			if (hand->_itype == ItemType::Bow && HasAllOf(hand->_iFlags, ItemSpecialEffect::FireArrows | ItemSpecialEffect::LightningArrows))
				kind = hand->_iLMinDam;
		}
		switch (kind) {
		case 0:
			mitype = MissileID::FireballBow;
			break;
		case 1:
			mitype = MissileID::LightningBow;
			break;
		case 2:
			mitype = MissileID::ChargedBoltBow;
			break;
		case 3:
			mitype = MissileID::HolyBoltBow;
			break;
		}
	}
	AddMissile(src, dst, dir, mitype, micaster, id, dam, spllvl);
	if (mitype == MissileID::ChargedBoltBow) {
		AddMissile(src, dst, dir, mitype, micaster, id, dam, spllvl);
		AddMissile(src, dst, dir, mitype, micaster, id, dam, spllvl);
	}
	missile._mirange--;
	if (missile._mirange == 0)
		missile._miDelFlag = true;
}

void ProcessLightningControl(Missile &missile)
{
	missile._mirange--;

	int dam;
	if (missile.IsTrap()) {
		// BUGFIX: damage of missile should be encoded in missile struct; monster can be dead before missile arrives.
		dam = GenerateRnd(currlevel) + 2 * currlevel;
	} else if (missile._micaster == TARGET_MONSTERS) {
		// BUGFIX: damage of missile should be encoded in missile struct; player can be dead/have left the game before missile arrives.
		dam = (GenerateRnd(2) + GenerateRnd(Players[missile._misource]._pLevel) + 2) << 6;
	} else {
		int minDamage = 0;
		int maxDamage = 0;
		if (LiveMonsterDamageRange(missile, minDamage, maxDamage)) // the live caster, with its pack's Might (round 28 audit)
			dam = 2 * (minDamage + GenerateRnd(maxDamage - minDamage + 1));
		else
			dam = GenerateRnd(currlevel) + 2 * currlevel; // a caster gone: as the trap's bolt above
	}

	SpawnLightning(missile, dam);
}

void ProcessLightning(Missile &missile)
{
	missile._mirange--;
	int j = missile._mirange;
	if (missile.position.tile != missile.position.start)
		CheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, true, missile.position.tile, false);
	if (missile._miHitFlag)
		missile._mirange = j;
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	}
	PutMissile(missile);
}

void ProcessTownPortal(Missile &missile)
{
	int expLight[17] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15 };

	if (missile._mirange > 1)
		missile._mirange--;
	if (missile._mirange == missile.var1)
		SetMissDir(missile, 1);
	if (leveltype != DTYPE_TOWN && missile._mimfnum != 1 && missile._mirange != 0) {
		if (missile.var2 == 0)
			missile._mlid = AddLight(missile.position.tile, 1);
		ChangeLight(missile._mlid, missile.position.tile, expLight[missile.var2]);
		missile.var2++;
	}

	for (Player &player : Players) {
		if (player.plractive && player.isOnActiveLevel() && !player._pLvlChanging && player._pmode == PM_STAND && player.position.tile == missile.position.tile) {
			ClrPlrPath(player);
			if (&player == MyPlayer) {
				NetSendCmdParam1(true, CMD_WARP, missile._misource);
				player._pmode = PM_NEWLVL;
			}
		}
	}

	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	}
	PutMissile(missile);
}

void ProcessFlashBottom(Missile &missile)
{
	if (missile._micaster == TARGET_MONSTERS) {
		if (!missile.IsTrap())
			Players[missile._misource]._pInvincible = true;
	}
	missile._mirange--;

	constexpr Direction Offsets[] = {
		Direction::NorthWest,
		Direction::NoDirection,
		Direction::SouthEast,
		Direction::West,
		Direction::SouthWest,
		Direction::South
	};
	for (Direction offset : Offsets)
		CheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, true, missile.position.tile + offset, true);

	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		if (missile._micaster == TARGET_MONSTERS) {
			if (!missile.IsTrap())
				Players[missile._misource]._pInvincible = false;
		}
	}
	PutMissile(missile);
}

void ProcessFlashTop(Missile &missile)
{
	if (missile._micaster == TARGET_MONSTERS) {
		if (!missile.IsTrap())
			Players[missile._misource]._pInvincible = true;
	}
	missile._mirange--;

	constexpr Direction Offsets[] = {
		Direction::North,
		Direction::NorthEast,
		Direction::East
	};
	for (Direction offset : Offsets)
		CheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, true, missile.position.tile + offset, true);

	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		if (missile._micaster == TARGET_MONSTERS) {
			if (!missile.IsTrap())
				Players[missile._misource]._pInvincible = false;
		}
	}
	PutMissile(missile);
}

void ProcessFlameWave(Missile &missile)
{
	constexpr int ExpLight[14] = { 2, 3, 4, 5, 5, 6, 7, 8, 9, 10, 11, 12, 12 };

	// Adjust missile's position for processing
	missile.position.tile += Direction::North;
	missile.position.offset.deltaY += 32;

	missile.var1++;
	if (missile.var1 == missile._miAnimLen) {
		SetMissDir(missile, 1);
		missile._miAnimFrame = GenerateRnd(11) + 1;
	}
	int j = missile._mirange;
	MoveMissileAndCheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, false, false);
	if (missile._miHitFlag)
		missile._mirange = j;
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	}
	if (missile._mimfnum != 0 || missile._mirange == 0) {
		if (missile.position.tile != Point { missile.var3, missile.var4 }) {
			missile.var3 = missile.position.tile.x;
			missile.var4 = missile.position.tile.y;
			ChangeLight(missile._mlid, missile.position.tile, 8);
		}
	} else {
		if (missile.var2 == 0)
			missile._mlid = AddLight(missile.position.tile, ExpLight[0]);
		ChangeLight(missile._mlid, missile.position.tile, ExpLight[missile.var2]);
		missile.var2++;
	}
	// Adjust missile's position for rendering
	missile.position.tile += Direction::South;
	missile.position.offset.deltaY -= 32;
	PutMissile(missile);
}

void ProcessGuardian(Missile &missile)
{
	missile._mirange--;

	if (missile.var2 > 0) {
		missile.var2--;
	}
	if (missile._mirange == missile.var1 || (missile._mimfnum == 2 && missile.var2 == 0)) {
		SetMissDir(missile, 1);
	}

	Point position = missile.position.tile;

	if ((missile._mirange % 16) == 0) {
		// Guardians pick a target by working backwards along lines originally based on VisionCrawlTable.
		// Because of their rather unique behaviour the points checked have been unrolled here
		constexpr std::array<WorldTileDisplacement, 48> guardianArc {
			{
			    // clang-format off
			    { 6, 0 }, { 5, 0 }, { 4, 0 }, { 3, 0 }, { 2, 0 }, { 1, 0 },
			    { 6, 1 }, { 5, 1 }, { 4, 1 }, { 3, 1 },
			    { 6, 2 }, { 2, 1 },
			    { 5, 2 },
			    { 6, 3 }, { 4, 2 },
			    { 5, 3 }, { 3, 2 }, { 1, 1 },
			    { 6, 4 },
			    { 6, 5 }, { 5, 4 }, { 4, 3 }, { 2, 2 },
			    { 5, 5 }, { 4, 4 }, { 3, 3 },
			    { 6, 6 }, { 5, 6 }, { 4, 5 }, { 3, 4 }, { 2, 3 },
			    { 4, 6 }, { 3, 5 }, { 2, 4 }, { 1, 2 },
			    { 3, 6 }, { 2, 5 }, { 1, 3 }, { 0, 1 },
			    { 2, 6 }, { 1, 4 },
			    { 1, 5 },
			    { 1, 6 },
			    { 0, 2 },
			    { 0, 3 },
			    { 0, 6 }, { 0, 5 }, { 0, 4 },
			    // clang-format on
			}
		};
		for (WorldTileDisplacement offset : guardianArc) {
			if (GuardianTryFireAt(missile, position + offset)
			    || GuardianTryFireAt(missile, position + offset.flipXY())
			    || GuardianTryFireAt(missile, position + offset.flipY())
			    || GuardianTryFireAt(missile, position + offset.flipX()))
				break;
		}
	}

	if (missile._mirange == 14) {
		SetMissDir(missile, 0);
		missile._miAnimFrame = 15;
		missile._miAnimAdd = -1;
	}

	missile.var3 += missile._miAnimAdd;

	if (missile.var3 > 15) {
		missile.var3 = 15;
	} else if (missile.var3 > 0) {
		ChangeLight(missile._mlid, position, missile.var3);
	}

	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	}

	PutMissile(missile);
}

void ProcessChainLightning(Missile &missile)
{
	int id = missile._misource;
	Point position = missile.position.tile;
	Point dst { missile.var1, missile.var2 };
	Direction dir = GetDirection(position, dst);
	AddMissile(position, dst, dir, MissileID::LightningControl, TARGET_MONSTERS, id, 1, missile._mispllvl);
	int rad = ChainLightningLeapRadius(missile._mispllvl);
	Crawl(1, rad, [&](Displacement displacement) {
		Point target = position + displacement;
		// Not to townspeople: town's dMonster holds towner ids (round 6 audit, v1.12.231).
		// Nor at the hero's own army and companions, which drew a harmless bolt each (round 16 audit).
		if (leveltype != DTYPE_TOWN && InDungeonBounds(target) && dMonster[target.x][target.y] > 0
		    && !Monsters[dMonster[target.x][target.y] - 1].isPlayerMinion()) {
			dir = GetDirection(position, target);
			AddMissile(position, target, dir, MissileID::LightningControl, TARGET_MONSTERS, id, 1, missile._mispllvl);
		}
		return false;
	});
	missile._mirange--;
	if (missile._mirange == 0)
		missile._miDelFlag = true;
}

void ProcessWeaponExplosion(Missile &missile)
{
	constexpr int ExpLight[10] = { 9, 10, 11, 12, 11, 10, 8, 6, 4, 2 };

	missile._mirange--;
	if (missile.var2 == WeaponExplosionColdFlash) {
		// The cold flash: its frames once, holding the last, then gone. No roll and no light of its own.
		if (missile._miAnimFrame >= missile._miAnimLen)
			missile._miAnimAdd = 0;
		if (missile._mirange <= 0) {
			missile._miDelFlag = true;
			return;
		}
		PutMissile(missile);
		return;
	}
	const Player &player = Players[missile._misource];
	int mind;
	int maxd;
	DamageType damageType;
	if (missile.var2 == 1) {
		// BUGFIX: damage of missile should be encoded in missile struct; player can be dead/have left the game before missile arrives.
		mind = player._pIFMinDam;
		maxd = player._pIFMaxDam;
		damageType = DamageType::Fire;
	} else {
		// BUGFIX: damage of missile should be encoded in missile struct; player can be dead/have left the game before missile arrives.
		mind = player._pILMinDam;
		maxd = player._pILMaxDam;
		damageType = DamageType::Lightning;
	}
	if (missile.var3 == 0) {
		CheckMissileCol(missile, damageType, mind, maxd, false, missile.position.tile, false);
		// Oracool: a landed blow ends the explosion the tick it lands - which is why vanilla's fire
		// flash vanishes on a hit. A hit FLASH has to be seen, so with the flash art it plays out
		// instead: no second roll (var3), just the frames that are left.
		if (missile.var4 != 0 && missile._miHitFlag) {
			missile.var3 = 1;
			missile._mirange = std::max(missile._miAnimLen - missile._miAnimFrame, 1);
		}
	}
	// And the flash bursts once: it holds its last, nearly empty frame rather than starting again.
	if (missile.var4 != 0 && missile._miAnimFrame >= missile._miAnimLen)
		missile._miAnimAdd = 0;
	if (missile.var1 == 0) {
		missile._mlid = AddLight(missile.position.tile, 9);
	} else {
		if (missile._mirange != 0)
			ChangeLight(missile._mlid, missile.position.tile, ExpLight[std::min(missile.var1, 9)]);
	}
	missile.var1++;
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	} else {
		PutMissile(missile);
	}
}

void ProcessMissileExplosion(Missile &missile)
{
	constexpr int ExpLight[] = { 9, 10, 11, 12, 11, 10, 8, 6, 4, 2, 1, 0, 0, 0, 0 };

	missile._mirange--;
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	} else {
		if (missile.var1 == 0)
			missile._mlid = AddLight(missile.position.tile, 9);
		else
			ChangeLight(missile._mlid, missile.position.tile, ExpLight[missile.var1]);
		missile.var1++;
		PutMissile(missile);
	}
}

void ProcessAcidSplate(Missile &missile)
{
	if (missile._mirange == missile._miAnimLen) {
		missile.position.tile += Displacement { 1, 1 };
		missile.position.offset.deltaY -= 32;
	}
	missile._mirange--;
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		int monst = missile._misource;
		// Oracool (2026-09-26): a poison Skeletal Mage's splash leaves no pool - a puddle is a trap for players, and
		// his army's owner would walk into it. The bolt's own hit is the mage's whole blow.
		if (monst >= 0 && Monsters[monst].isPlayerMinion())
			return;
		int dam = (Monsters[monst].data().level >= 2 ? 2 : 1);
		AddMissile(missile.position.tile, { 0, 0 }, Direction::South, MissileID::AcidPuddle, TARGET_PLAYERS, monst, dam, missile._mispllvl);
	} else {
		PutMissile(missile);
	}
}

void ProcessTeleport(Missile &missile)
{
	missile._mirange--;
	if (missile._mirange <= 0) {
		missile._miDelFlag = true;
		return;
	}

	int id = missile._misource;
	Player &player = Players[id];
	// A hero killed on the tick he leapt stays where he fell: the corpse, its mark, its light and the camera went with the
	// jump (round 28 audit).
	if (player._pmode == PM_DEATH || (player._pHitPoints >> 6) <= 0) {
		missile._miDelFlag = true;
		return;
	}

	std::optional<Point> teleportDestination = FindClosestValidPosition(
	    [&player](Point target) {
		    return PosOkPlayer(player, target);
	    },
	    missile.position.tile, 0, 5);

	if (!teleportDestination)
		return;

	// A hero caught mid-step - a Leap taken under a held walk button, whose stale walk order started a step before this
	// missile ran - stops walking first: the step's end set the tile back beside the take-off point, the light and the
	// camera with it, and a sideways step left a phantom dPlayer mark (round 20 audit, v1.12.245).
	if (player.isWalking()) {
		FixPlrWalkTags(player);
		ClrPlrPath(player);
		player.destAction = ACTION_NONE;
		StartStand(player, player._pdir);
		ChangeLightOffset(player.lightId, {});
	}
	// His own companion or minion on the landing tile gives way, as it does to a walk: they shared a tile (round 20).
	oracool::CompanionsMakeWay(player, *teleportDestination);

	dPlayer[player.position.tile.x][player.position.tile.y] = 0;
	PlrClrTrans(player.position.tile);
	player.position.tile = *teleportDestination;
	player.position.future = player.position.tile;
	player.position.old = player.position.tile;
	PlrDoTrans(player.position.tile);
	missile.var1 = 1;
	dPlayer[player.position.tile.x][player.position.tile.y] = id + 1;
	if (leveltype != DTYPE_TOWN) {
		ChangeLightXY(player.lightId, player.position.tile);
		ChangeVisionXY(player.getId(), player.position.tile);
	}
	if (&player == MyPlayer) {
		ViewPosition = player.position.tile;
	}
}

void ProcessStoneCurse(Missile &missile)
{
	missile._mirange--;
	auto &monster = Monsters[missile.var2];
	if (monster.hitPoints == 0 && missile._miAnimType != MissileGraphicID::StoneCurseShatter) {
		missile._mimfnum = 0;
		missile._miDrawFlag = true;
		SetMissAnim(missile, MissileGraphicID::StoneCurseShatter);
		missile._mirange = 11;
	}
	if (monster.mode != MonsterMode::Petrified) {
		missile._miDelFlag = true;
		return;
	}

	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		if (monster.hitPoints > 0) {
			monster.mode = static_cast<MonsterMode>(missile.var1);
			monster.animInfo.isPetrified = false;
		} else {
			AddCorpse(monster.position.tile, stonendx, monster.direction);
		}
	}
	if (missile._miAnimType == MissileGraphicID::StoneCurseShatter)
		PutMissile(missile);
}

void ProcessApocalypseBoom(Missile &missile)
{
	missile._mirange--;
	if (missile.var1 == 0)
		CheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, false, missile.position.tile, true);
	if (missile._miHitFlag)
		missile.var1 = 1;
	if (missile._mirange == 0)
		missile._miDelFlag = true;
	PutMissile(missile);
}

void ProcessRhino(Missile &missile)
{
	int monst = missile._misource;
	auto &monster = Monsters[monst];
	if (monster.mode != MonsterMode::Charge) {
		missile._miDelFlag = true;
		return;
	}
	UpdateMissilePos(missile);
	Point prevPos = missile.position.tile;
	Point newPosSnake;
	dMonster[prevPos.x][prevPos.y] = 0;
	if (monster.ai == MonsterAIID::Snake) {
		missile.position.traveled += missile.position.velocity * 2;
		UpdateMissilePos(missile);
		newPosSnake = missile.position.tile;
		missile.position.traveled -= missile.position.velocity;
	} else {
		missile.position.traveled += missile.position.velocity;
	}
	UpdateMissilePos(missile);
	Point newPos = missile.position.tile;
	if (!IsTileAvailable(monster, newPos) || (monster.ai == MonsterAIID::Snake && !IsTileAvailable(monster, newPosSnake))) {
		MissToMonst(missile, prevPos);
		missile._miDelFlag = true;
		return;
	}
	monster.position.future = newPos;
	monster.position.old = newPos;
	monster.position.tile = newPos;
	dMonster[newPos.x][newPos.y] = -(monst + 1);
	if (missile._mlid != NO_LIGHT)
		ChangeLightXY(missile._mlid, newPos);
	MoveMissilePos(missile);
	PutMissile(missile);
}

void ProcessFireWallControl(Missile &missile)
{
	missile._mirange--;
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		return;
	}

	int id = missile._misource;

	{
		Point position = { missile.var1, missile.var2 };
		Point target = position + static_cast<Direction>(missile.var3);

		if (!missile.limitReached && GrowWall(id, position, target, MissileID::FireWall, missile._mispllvl, 0)) {
			missile.var1 = target.x;
			missile.var2 = target.y;
		} else {
			missile.limitReached = true;
		}
	}

	{
		Point position = { missile.var5, missile.var6 };
		Point target = position + static_cast<Direction>(missile.var4);

		if (missile.var7 == 0 && GrowWall(id, position, target, MissileID::FireWall, missile._mispllvl, 0)) {
			missile.var5 = target.x;
			missile.var6 = target.y;
		} else {
			missile.var7 = 1;
		}
	}
}

/**
 * @brief Oracool: walks Blessed Hammer one step around its spiral.
 *
 * This is the only missile in the file that does not travel on a velocity vector, so it does not go
 * through MoveMissile at all. It writes position.traveled - the fixed-point pixel displacement from
 * position.start that UpdateMissilePos turns back into a tile and an offset - directly from an angle
 * and a radius, both growing with the tick count. That seam is what makes a spiral expressible here
 * without touching the movement code every other missile depends on.
 *
 * The y term is halved because the dungeon is drawn isometrically: a circle traced on the ground is
 * an ellipse of half the height on screen, so an unhalved circle would look like it was standing up.
 */
Displacement BlessedHammerOffsetAt(float ticks)
{
	const float angle = ticks * BlessedHammerRadiansPerTick;
	const float radius = ticks * BlessedHammerPixelsPerTick;
	return { static_cast<int>(std::cos(angle) * radius), static_cast<int>(std::sin(angle) * radius / 2) };
}

void ProcessBlessedHammer(Missile &missile)
{
	missile._mirange--;
	missile.var1++;

	const Displacement pixels = BlessedHammerOffsetAt(static_cast<float>(missile.var1));
	missile.position.traveled = { pixels.deltaX * (1 << 16), pixels.deltaY * (1 << 16) };
	UpdateMissilePos(missile);

	const Point tile = missile.position.tile;
	// Guarded, and this is the same hazard SpawnLightning was carrying earlier today: a missile that
	// leaves the map indexes dPiece out of bounds. A spiral is if anything likelier to do it, since
	// it walks outward with no target to stop at.
	if (!InDungeonBounds(tile)) {
		missile._miDelFlag = true;
		return;
	}

	// Every tile the hammer passed through since the last tick, not only the one it stands on now
	// (2026-09-11, the user: "are you sure that its touch area travels with its animation"). Checked
	// once a tick, a hammer moving to a DIAGONAL neighbour flew over the side tile between the two
	// without looking at it - on the spiral's outer turns it covers up to 46px a tick - and 4 of the
	// 29 tiles a cast crosses were never checked: a monster standing there was visibly hammered and
	// took nothing. Eight looks a tick miss none; Missiles.BlessedHammerChecksEveryTileItCrosses pins it.
	//
	// A tile is hit as the hammer ENTERS it, never while it stays: without that it would damage
	// whatever it overlaps every tick, which at 20 ticks a second is not a hammer, it is a blender.
	// Nor on stepping straight back into the tile it just left - a path grazing a tile corner can
	// flick A, B, A, and that is one pass, not two hits. var2/var3 is the tile it last entered,
	// var4/var5 the one before; a later turn of the spiral re-entering a tile does hit again.
	const DamageType damageType = GetMissileData(missile._mitype).damageType();
	Point current { missile.var2, missile.var3 };
	Point previous { missile.var4, missile.var5 };
	for (int step = 1; step <= BlessedHammerSubSteps; step++) {
		const float t = static_cast<float>(missile.var1 - 1) + static_cast<float>(step) / BlessedHammerSubSteps;
		const Point crossed = missile.position.start + BlessedHammerOffsetAt(t).screenToMissile();
		if (crossed == current)
			continue;
		const bool flickBack = crossed == previous;
		previous = current;
		current = crossed;
		if (!flickBack)
			CheckMissileCol(missile, damageType, missile._midam, missile._midam, false, crossed, /*dontDeleteOnCollision=*/true);
	}
	missile.var2 = current.x;
	missile.var3 = current.y;
	missile.var4 = previous.x;
	missile.var5 = previous.y;

	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		return;
	}
	PutMissile(missile);
}

void ProcessEtherealize(Missile &missile)
{
	Player &player = Players[missile._misource];
	missile._mirange--;
	// Re-asserted every tick rather than only on cast, matching ProcessInfravision: InitMissiles clears
	// the flag on every level entry. The missile did NOT survive that until 2026-09-13 - the clear deleted
	// it - and now InitMissiles carries it across a level change with its time left (see there).
	player._pSpellFlags |= SpellFlag::Etherealize;
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		player._pSpellFlags &= ~SpellFlag::Etherealize;
		RedrawEverything();
	}
}

void ProcessInfravision(Missile &missile)
{
	Player &player = Players[missile._misource];
	missile._mirange--;
	player._pInfraFlag = true;
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		CalcPlrItemVals(player, true);
	}
}

void ProcessApocalypse(Missile &missile)
{
	// Town's dMonster holds towner ids, not monsters (round 6 audit, v1.12.231). It set a boom on every townsperson in reach.
	if (leveltype == DTYPE_TOWN) {
		missile._miDelFlag = true;
		return;
	}
	for (int j = missile.var2; j < missile.var3; j++) {
		for (int k = missile.var4; k < missile.var5; k++) {
			int mid = dMonster[k][j] - 1;
			if (mid < 0)
				continue;
			if (Monsters[mid].isPlayerMinion())
				continue;
			if (TileHasAny(dPiece[k][j], TileProperties::Solid))
				continue;
			if (gbIsHellfire && !LineClearMissile(missile.position.tile, { k, j }))
				continue;

			int id = missile._misource;
			AddMissile({ k, j }, { k, j }, Players[id]._pdir, MissileID::ApocalypseBoom, TARGET_MONSTERS, id, missile._midam, 0);
			missile.var2 = j;
			missile.var4 = k + 1;
			return;
		}
		missile.var4 = missile.var6;
	}
	missile._miDelFlag = true;
}

void ProcessFlameWaveControl(Missile &missile)
{
	bool f1 = false;
	bool f2 = false;

	int id = missile._misource;
	Point src = missile.position.tile;
	Direction sd = GetDirection(src, { missile.var1, missile.var2 });
	Direction dira = Left(Left(sd));
	Direction dirb = Right(Right(sd));
	Point na = src + sd;
	int pn = dPiece[na.x][na.y];
	assert(pn >= 0 && pn <= MAXTILES);
	if (!TileHasAny(pn, TileProperties::BlockMissile)) {
		Direction pdir = Players[id]._pdir;
		AddMissile(na, na + sd, pdir, MissileID::FlameWave, TARGET_MONSTERS, id, 0, missile._mispllvl);
		na += dira;
		Point nb = src + sd + dirb;
		for (int j = 0; j < FlameWaveSideTiles(missile._mispllvl); j++) {
			pn = dPiece[na.x][na.y]; // BUGFIX: dPiece is accessed before check against dungeon size and 0
			assert(pn >= 0 && pn <= MAXTILES);
			if (TileHasAny(pn, TileProperties::BlockMissile) || f1 || !InDungeonBounds(na)) {
				f1 = true;
			} else {
				AddMissile(na, na + sd, pdir, MissileID::FlameWave, TARGET_MONSTERS, id, 0, missile._mispllvl);
				na += dira;
			}
			pn = dPiece[nb.x][nb.y]; // BUGFIX: dPiece is accessed before check against dungeon size and 0
			assert(pn >= 0 && pn <= MAXTILES);
			if (TileHasAny(pn, TileProperties::BlockMissile) || f2 || !InDungeonBounds(nb)) {
				f2 = true;
			} else {
				AddMissile(nb, nb + sd, pdir, MissileID::FlameWave, TARGET_MONSTERS, id, 0, missile._mispllvl);
				nb += dirb;
			}
		}
	}

	missile._mirange--;
	if (missile._mirange == 0)
		missile._miDelFlag = true;
}

void ProcessRage(Missile &missile)
{
	missile._mirange--;

	if (missile._mirange != 0) {
		return;
	}

	Player &player = Players[missile._misource];

	int hpdif = player._pMaxHP - player._pHitPoints;

	if (HasAnyOf(player._pSpellFlags, SpellFlag::RageActive)) {
		player._pSpellFlags &= ~SpellFlag::RageActive;
		player._pSpellFlags |= SpellFlag::RageCooldown;
		int lvl = player._pLevel * 2;
		missile._mirange = lvl + 10 * missile._mispllvl + 245;
	} else {
		player._pSpellFlags &= ~SpellFlag::RageCooldown;
		missile._miDelFlag = true;
		hpdif += missile.var2;
	}

	CalcPlrItemVals(player, true);
	ApplyPlrDamage(DamageType::Physical, player, 0, 1, hpdif);
	RedrawEverything();
	player.Say(HeroSpeech::HeavyBreathing);
}

void ProcessInferno(Missile &missile)
{
	missile._mirange--;
	missile.var2--;
	int k = missile._mirange;
	CheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, true, missile.position.tile, false);
	if (missile._mirange == 0 && missile._miHitFlag)
		missile._mirange = k;
	if (missile.var2 == 0)
		missile._miAnimFrame = 20;
	if (missile.var2 <= 0) {
		k = missile._miAnimFrame;
		if (k > 11)
			k = 24 - k;
		ChangeLight(missile._mlid, missile.position.tile, k);
	}
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	}
	if (missile.var2 <= 0)
		PutMissile(missile);
}

void ProcessInfernoControl(Missile &missile)
{
	missile._mirange--;
	missile.position.traveled += missile.position.velocity;
	UpdateMissilePos(missile);
	if (missile.position.tile != Point { missile.var1, missile.var2 }) {
		int id = dPiece[missile.position.tile.x][missile.position.tile.y];
		if (!TileHasAny(id, TileProperties::BlockMissile)) {
			AddMissile(
			    missile.position.tile,
			    missile.position.start,
			    Direction::South,
			    MissileID::Inferno,
			    missile._micaster,
			    missile._misource,
			    missile.var3,
			    missile._mispllvl,
			    &missile);
		} else {
			missile._mirange = 0;
		}
		missile.var1 = missile.position.tile.x;
		missile.var2 = missile.position.tile.y;
		missile.var3++;
	}
	if (missile._mirange == 0 || missile.var3 == 3)
		missile._miDelFlag = true;
}

void ProcessChargedBolt(Missile &missile)
{
	missile._mirange--;
	if (missile._miAnimType != MissileGraphicID::Lightning) {
		if (missile.var3 == 0) {
			constexpr int BPath[16] = { -1, 0, 1, -1, 0, 1, -1, -1, 0, 0, 1, 1, 0, 1, -1, 0 };

			auto md = static_cast<Direction>(missile.var2);
			switch (BPath[missile._mirnd]) {
			case -1:
				md = Left(md);
				break;
			case 1:
				md = Right(md);
				break;
			}

			missile._mirnd = (missile._mirnd + 1) & 0xF;
			UpdateMissileVelocity(missile, missile.position.tile + md, 8);
			missile.var3 = 16;
		} else {
			missile.var3--;
		}
		MoveMissileAndCheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), missile._midam, missile._midam, false, false);
		if (missile._miHitFlag) {
			missile.var1 = 8;
			missile._mimfnum = 0;
			missile.position.offset = { 0, 0 };
			missile.position.velocity = {};
			SetMissAnim(missile, MissileGraphicID::Lightning);
			missile._mirange = missile._miAnimLen;
		}
		ChangeLight(missile._mlid, missile.position.tile, missile.var1);
	}
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	}
	PutMissile(missile);
}

void ProcessHolyBolt(Missile &missile)
{
	missile._mirange--;
	if (missile._miAnimType != MissileGraphicID::HolyBoltExplosion) {
		int dam = missile._midam;
		MoveMissileAndCheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), dam, dam, true, true);
		if (missile._mirange == 0) {
			missile._mimfnum = 0;
			SetMissAnim(missile, MissileGraphicID::HolyBoltExplosion);
			missile._mirange = missile._miAnimLen - 1;
			missile.position.StopMissile();
		} else {
			if (missile.position.tile != Point { missile.var1, missile.var2 }) {
				missile.var1 = missile.position.tile.x;
				missile.var2 = missile.position.tile.y;
				ChangeLight(missile._mlid, missile.position.tile, 8);
			}
		}
	} else {
		ChangeLight(missile._mlid, missile.position.tile, missile._miAnimFrame + 7);
		if (missile._mirange == 0) {
			missile._miDelFlag = true;
			AddUnLight(missile._mlid);
		}
	}
	PutMissile(missile);
}

void ProcessElemental(Missile &missile)
{
	missile._mirange--;
	int dam = missile._midam;
	const Point missilePosition = missile.position.tile;
	if (missile._miAnimType == MissileGraphicID::BigExplosion) {
		ChangeLight(missile._mlid, missile.position.tile, missile._miAnimFrame);

		Point startPoint = missile.var3 == 2 ? Point { missile.var4, missile.var5 } : Point(missile.position.start);
		constexpr Direction Offsets[] = {
			Direction::NoDirection,
			Direction::SouthWest,
			Direction::NorthEast,
			Direction::SouthEast,
			Direction::East,
			Direction::South,
			Direction::NorthWest,
			Direction::West,
			Direction::North
		};
		for (Direction offset : Offsets) {
			if (!CheckBlock(startPoint, missilePosition + offset))
				CheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), dam, dam, true, missilePosition + offset, true);
		}

		if (missile._mirange == 0) {
			missile._miDelFlag = true;
			AddUnLight(missile._mlid);
		}
	} else {
		MoveMissileAndCheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), dam, dam, false, false);
		if (missile.var3 == 0 && missilePosition == Point { missile.var4, missile.var5 })
			missile.var3 = 1;
		if (missile.var3 == 1) {
			missile.var3 = 2;
			missile._mirange = 255;
			auto *nextMonster = FindClosest(missilePosition, 19);
			if (nextMonster != nullptr) {
				Direction sd = GetDirection(missilePosition, nextMonster->position.tile);
				SetMissDir(missile, sd);
				UpdateMissileVelocity(missile, nextMonster->position.tile, 16);
			} else {
				Direction sd = Players[missile._misource]._pdir;
				SetMissDir(missile, sd);
				UpdateMissileVelocity(missile, missilePosition + sd, 16);
			}
		}
		if (missilePosition != Point { missile.var1, missile.var2 }) {
			missile.var1 = missilePosition.x;
			missile.var2 = missilePosition.y;
			ChangeLight(missile._mlid, missilePosition, 8);
		}
		if (missile._mirange == 0) {
			missile._mimfnum = 0;
			SetMissAnim(missile, MissileGraphicID::BigExplosion);
			missile._mirange = missile._miAnimLen - 1;
			missile.position.StopMissile();
		}
	}
	PutMissile(missile);
}

void ProcessBoneSpirit(Missile &missile)
{
	missile._mirange--;
	int dam = missile._midam;
	// Oracool (2026-09-18, RfA-17): the Necromancer's Bone Spirit wears the delivered skull - sixteen facings
	// and no burst set - so it turns by sixteenths and ends in the bone-hit burst; the book spell's sklball
	// keeps its eight facings and its ninth, bursting, set.
	const bool necroArt = missile._miAnimType == MissileGraphicID::BoneSpiritNecro;
	if (!necroArt && missile._mimfnum == 8) {
		ChangeLight(missile._mlid, missile.position.tile, missile._miAnimFrame);
		if (missile._mirange == 0) {
			missile._miDelFlag = true;
			AddUnLight(missile._mlid);
		}
		PutMissile(missile);
	} else {
		MoveMissileAndCheckMissileCol(missile, GetMissileData(missile._mitype).damageType(), dam, dam, false, false);
		Point c = missile.position.tile;
		if (missile.var3 == 0 && c == Point { missile.var4, missile.var5 })
			missile.var3 = 1;
		if (missile.var3 == 1) {
			missile.var3 = 2;
			missile._mirange = 255;
			auto *monster = FindClosest(c, 19);
			if (monster != nullptr) {
				missile._midam = monster->hitPoints >> 7;
				if (necroArt)
					SetMissDir(missile, GetDirection16(c, monster->position.tile));
				else
					SetMissDir(missile, GetDirection(c, monster->position.tile));
				UpdateMissileVelocity(missile, monster->position.tile, 16);
			} else {
				Direction sd = Players[missile._misource]._pdir;
				if (necroArt)
					SetMissDir(missile, GetDirection16(c, c + sd));
				else
					SetMissDir(missile, sd);
				UpdateMissileVelocity(missile, c + sd, 16);
			}
		}
		if (c != Point { missile.var1, missile.var2 }) {
			missile.var1 = c.x;
			missile.var2 = c.y;
			ChangeLight(missile._mlid, c, 8);
		}
		if (missile._mirange == 0) {
			if (necroArt) {
				missile._miDelFlag = true;
				AddUnLight(missile._mlid);
				AddMissile(c, c, Direction::South, MissileID::BoneHitBurst, missile._micaster, missile._misource, 0, 0);
				return;
			}
			SetMissDir(missile, 8);
			missile.position.velocity = {};
			missile._mirange = 7;
		}
		PutMissile(missile);
	}
}

void ProcessResurrectBeam(Missile &missile)
{
	missile._mirange--;
	if (missile._mirange == 0)
		missile._miDelFlag = true;
	PutMissile(missile);
}

void ProcessRedPortal(Missile &missile)
{
	int expLight[17] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15 };

	if (missile._mirange > 1)
		missile._mirange--;
	if (missile._mirange == missile.var1)
		SetMissDir(missile, 1);

	if (leveltype != DTYPE_TOWN && missile._mimfnum != 1 && missile._mirange != 0) {
		if (missile.var2 == 0)
			missile._mlid = AddLight(missile.position.tile, 1);
		ChangeLight(missile._mlid, missile.position.tile, expLight[missile.var2]);
		missile.var2++;
	}
	if (missile._mirange == 0) {
		missile._miDelFlag = true;
		AddUnLight(missile._mlid);
	}
	PutMissile(missile);
}

void ProcessRiftPortal(Missile &missile)
{
	// Nothing to count down; the gate removes it. The opening animation (file 0) plays once, then
	// the standing loop (file 1) - checked on the last frame, before the generic advance wraps it.
	if (missile._mimfnum == 0 && missile._miAnimFrame >= missile._miAnimLen)
		SetMissDir(missile, 1);
	// In town the portal is the door (plan r7): the hero walking onto the tile in front of the gate
	// enters the rift, as walking into a town portal warps. Inside a rift the same missile marks the
	// way back and the trigger under it does the work.
	if (leveltype == DTYPE_TOWN)
		oracool::TryEnterRiftFromTown();
	PutMissile(missile);
}

static void DeleteMissiles()
{
	Missiles.remove_if([](Missile &missile) { return missile._miDelFlag; });
}

void ProcessManaShield()
{
	Player &myPlayer = *MyPlayer;
	if (myPlayer.pManaShield && myPlayer._pMana <= 0) {
		myPlayer.pManaShield = false;
		NetSendCmd(true, CMD_REMSHIELD);
	}
}

void ProcessMissiles()
{
	for (auto &missile : Missiles) {
		const auto &position = missile.position.tile;
		if (InDungeonBounds(position)) {
			dFlags[position.x][position.y] &= ~(DungeonFlag::Missile | DungeonFlag::MissileFireWall | DungeonFlag::MissileLightningWall);
		} else {
			missile._miDelFlag = true;
		}
	}

	DeleteMissiles();

	MissilePreFlag = false;

	for (auto &missile : Missiles) {
		const MissileData &missileData = GetMissileData(missile._mitype);
		if (missileData.mProc != nullptr)
			missileData.mProc(missile);
		if (missile._miAnimFlags == MissileGraphicsFlags::NotAnimated)
			continue;

		missile._miAnimCnt++;
		if (missile._miAnimCnt < missile._miAnimDelay)
			continue;

		missile._miAnimCnt = 0;
		missile._miAnimFrame += missile._miAnimAdd;
		if (missile._miAnimFrame > missile._miAnimLen)
			missile._miAnimFrame = 1;
		else if (missile._miAnimFrame < 1)
			missile._miAnimFrame = missile._miAnimLen;
	}

	ProcessManaShield();
	DeleteMissiles();
}

void missiles_process_charge()
{
	for (auto &missile : Missiles) {
		// A facing past the sheet's rows is its first, as SetMissAnim draws it (audit of the fix, 2026-09-29).
		const MissileFileData &sheetData = GetMissileSpriteData(missile._miAnimType);
		const int facing = sheetData.animFAmt > 0 && missile._mimfnum >= sheetData.animFAmt ? 0 : missile._mimfnum;
		missile._miAnimData = sheetData.spritesForDirection(facing);
		missile.oracoolColours = GetMissileSpriteData(missile._miAnimType).colours.get();
		if (missile._mitype != MissileID::Rhino) {
			if (missile.oracoolScalePercent != 100)
				ScaleMissile(missile, missile.oracoolScalePercent, missile.oracoolScaleFloor); // back to its scaled sheet
			continue;
		}
		missile.oracoolColours = nullptr; // the charging monster's own sprites, in the level palette

		const CMonster &mon = Monsters[missile._misource].type();

		MonsterGraphic graphic;
		if (IsAnyOf(mon.type, MT_HORNED, MT_MUDRUN, MT_FROSTC, MT_OBLORD)) {
			graphic = MonsterGraphic::Special;
		} else if (IsAnyOf(mon.type, MT_NSNAKE, MT_RSNAKE, MT_BSNAKE, MT_GSNAKE)) {
			graphic = MonsterGraphic::Attack;
		} else {
			graphic = MonsterGraphic::Walk;
		}
		missile._miAnimData = mon.getAnimData(graphic).spritesForDirection(static_cast<Direction>(missile._mimfnum));
	}
}

void RedoMissileFlags()
{
	for (auto &missile : Missiles) {
		PutMissile(missile);
	}
}

} // namespace devilution
