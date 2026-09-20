/**
 * @file missiles.h
 *
 * Interface of missile functionality.
 */
#pragma once

#include <cstdint>
#include <list>

#include "engine.h"
#include "engine/point.hpp"
#include "misdat.h"
#include "monster.h"
#include "player.h"
#include "spelldat.h"
#include "utils/attributes.h"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

constexpr WorldTilePosition GolemHoldingCell = Point { 1, 0 };

struct MissilePosition {
	Point tile;
	/** Sprite's pixel offset from tile. */
	Displacement offset;
	/** Pixel velocity while moving */
	Displacement velocity;
	/** Start position */
	Point start;
	/** Start position */
	Displacement traveled;

	/**
	 * @brief Specifies the location (tile) while rendering
	 */
	Point tileForRendering;
	/**
	 * @brief Specifies the location (offset) while rendering
	 */
	Displacement offsetForRendering;

	/**
	 * @brief Stops the missile (set velocity to zero and set offset to last renderer location; shouldn't matter cause the missile don't move anymore)
	 */
	void StopMissile()
	{
		velocity = {};
		if (tileForRendering == tile)
			offset = offsetForRendering;
	}
};

/**
 * Represent a more fine-grained direction than the 8 value Direction enum.
 *
 * This is used when rendering projectiles like arrows which have additional sprites for "half-winds" on a 16-point compass.
 * The sprite sheets are typically 0-indexed and use the following layout (relative to the screen projection)
 *
 *      W  WSW   SW  SSW  S
 *               ^
 *     WNW       |       SSE
 *               |
 *     NW -------+------> SE
 *               |
 *     NNW       |       ESE
 *               |
 *      N  NNE   NE  ENE  E
 */
enum class Direction16 : uint8_t {
	South,
	South_SouthWest,
	SouthWest,
	West_SouthWest,
	West,
	West_NorthWest,
	NorthWest,
	North_NorthWest,
	North,
	North_NorthEast,
	NorthEast,
	East_NorthEast,
	East,
	East_SouthEast,
	SouthEast,
	South_SouthEast,
};

enum class MissileSource : uint8_t {
	Player,
	Monster,
	Trap,
};

struct Missile {
	/** Type of projectile */
	MissileID _mitype;
	MissilePosition position;
	int _mimfnum; // The direction of the missile (direction enum)
	int _mispllvl;
	bool _miDelFlag; // Indicate whether the missile should be deleted
	MissileGraphicID _miAnimType;
	MissileGraphicsFlags _miAnimFlags;
	OptionalClxSpriteList _miAnimData;
	int _miAnimDelay; // Tick length of each frame in the current animation
	int _miAnimLen;   // Number of frames in current animation

	// TODO: This field is no longer used and is always equal to
	// (*_miAnimData)[0].width()
	uint16_t _miAnimWidth;

	int16_t _miAnimWidth2;
	int _miAnimCnt; // Increases by one each game tick, counting how close we are to _pAnimDelay
	int _miAnimAdd;
	int _miAnimFrame; // Current frame of animation + 1.
	bool _miDrawFlag;
	bool _miLightFlag;
	bool _miPreFlag;
	uint32_t _miUniqTrans;
	int _mirange; // Time to live for the missile in game ticks, oncs 0 the missile will be marked for deletion via _miDelFlag
	int _misource;
	mienemy_type _micaster;
	int _midam;
	bool _miHitFlag;
	int _midist; // Used for arrows to measure distance travelled (increases by 1 each game tick). Higher value is a penalty for accuracy calculation when hitting enemy
	int _mlid;
	int _mirnd;
	int var1;
	int var2;
	int var3;
	int var4;
	int var5;
	int var6;
	int var7;
	bool limitReached;
	/**
	 * @brief For moving missiles lastCollisionTargetHash contains the last entity (player or monster) that was checked in CheckMissileCol (needed to avoid multiple hits for a entity at the same tile).
	 */
	int16_t lastCollisionTargetHash;
	/**
	 * @brief Oracool: an optional recolour applied when this missile is drawn.
	 *
	 * The renderer already had a TRN path, but it was hardwired to Monsters[_misource]'s unique
	 * palette - usable only by a missile a unique monster fired, and unreachable for a player's. This
	 * is the same idea with the table supplied rather than looked up, which is what lets the Paladin's
	 * borrowed ITEM sprites (a tumbling shield, a falling mace) be brightened into something divine
	 * instead of looking like loot on the floor. Those are only the fallback since 2026-09-11, when
	 * Blessed Shield and Fist of the Heavens got sheets of their own; a build without them still
	 * borrows, and still brightens.
	 *
	 * Points at a 256-byte table owned elsewhere and outliving the missile; null means draw as-is.
	 */
	const uint8_t *oracoolTrn = nullptr;

	/**
	 * @brief The class-tree skill that fired this missile, as a ClassTreeSkill. 0xFFFF is none.
	 *
	 * Oracool: carries a cast from the moment it is thrown to the moment it lands, so the impact cue
	 * can name the skill responsible. Stamped by AddMissile from the scope CastSpell opens.
	 *
	 * DELIBERATELY NOT SAVED, like oracoolTrn above. Missiles are persisted in the level save at a
	 * fixed 180 bytes each and adding a field would change that; more to the point, a missile in
	 * flight across a save is a fraction of a second of a sound. The cue simply does not play for
	 * it, which is the sound package's own "missing IDs fail softly" rule rather than an exception
	 * to it.
	 */
	uint16_t oracoolSkill = 0xFFFF;

	/**
	 * @brief Oracool: a companion's arrow deals this percent of its owner's damage (oracool/companion.h). 0 is an
	 * ordinary missile. Not saved, like the two above.
	 */
	int16_t companionPercent = 0;

	/** @brief Was the missile generated by a trap? */
	[[nodiscard]] bool IsTrap() const
	{
		return _misource == -1;
	}

	[[nodiscard]] Player *sourcePlayer()
	{
		if (IsNoneOf(_micaster, TARGET_BOTH, TARGET_MONSTERS) || _misource == -1)
			return nullptr;
		return &Players[_misource];
	}

	[[nodiscard]] Monster *sourceMonster()
	{
		if (_micaster != TARGET_PLAYERS || _misource == -1)
			return nullptr;
		return &Monsters[_misource];
	}

	[[nodiscard]] bool isSameSource(Missile &missile)
	{
		return sourceType() == missile.sourceType() && _misource == missile._misource;
	}

	MissileSource sourceType()
	{
		if (_misource == -1)
			return MissileSource::Trap;
		if (_micaster == TARGET_PLAYERS)
			return MissileSource::Monster;
		return MissileSource::Player;
	}
};

extern DVL_API_FOR_TEST std::list<Missile> Missiles;
extern bool MissilePreFlag;

void GetDamageAmt(SpellID i, int *mind, int *maxd);

/**
 * @brief Same, but for an explicit spell level rather than the player's current one.
 *
 * Oracool: what lets the Abilities window show what a spell would do ONE LEVEL FROM NOW beside what
 * it does today. Still reads the player for everything else - Magic, character level, class - since
 * those are not what is being hypothesised about.
 */
void GetDamageAmtAtLevel(SpellID i, int spellLevel, int *mind, int *maxd);

/**
 * @brief Returns the direction a vector from p1(x1, y1) to p2(x2, y2) is pointing to.
 *
 * @code{.unparsed}
 *      W  sW  SW   Sw  S
 *              ^
 *     nW       |       Se
 *              |
 *     NW ------+-----> SE
 *              |
 *     Nw       |       sE
 *              |
 *      N  Ne  NE   nE  E
 * @endcode
 *
 * @param p1 The point from which the vector starts.
 * @param p2 The point from which the vector ends.
 * @return the direction of the p1->p2 vector
 */
Direction16 GetDirection16(Point p1, Point p2);
/** @param damageDealt When given, receives the damage applied (in 64ths), 0 when the hit missed - so a minion's bolt can credit its owner (audit, 2026-09-19). */
bool MonsterTrapHit(int monsterId, int mindam, int maxdam, int dist, MissileID t, DamageType damageType, bool shift, int *damageDealt = nullptr);
bool PlayerMHit(int pnum, Monster *monster, int dist, int mind, int maxd, MissileID mtype, DamageType damageType, bool shift, DeathReason deathReason, bool *blocked);

/**
 * @brief Could the missile collide with solid objects? (like walls or closed doors)
 */
bool IsMissileBlockedByTile(Point position);

/**
 * @brief Sets the missile sprite to the given sheet frame
 * @param missile this object
 * @param dir Sprite frame, typically representing a direction but there are some exceptions (arrows being 1 indexed, directionless spells)
 */
void SetMissDir(Missile &missile, int dir);

/**
 * @brief Sets the sprite for this missile so it matches the given Direction
 * @param missile this object
 * @param dir Desired facing
 */
inline void SetMissDir(Missile &missile, Direction dir)
{
	SetMissDir(missile, static_cast<int>(dir));
}

/**
 * @brief Sets the sprite for this missile so it matches the given Direction16
 * @param missile this object
 * @param dir Desired facing at a 22.8125 degree resolution
 */
inline void SetMissDir(Missile &missile, Direction16 dir)
{
	SetMissDir(missile, static_cast<int>(dir));
}

/**
 * @brief Clears every missile for a level entry.
 *
 * @param keepHeroTimedSpells keep the local hero's own Infravision, Etherealize and Search - the timed spells
 * whose clock lives on their missile - with their time left (user, 2026-09-13: "find out why it runs out every
 * time i change dungeon level and fix its countdown timer to survive level changes"). True only for a level
 * change inside a running game, never a new game or a loaded save.
 */
void InitMissiles(bool keepHeroTimedSpells = false);

struct AddMissileParameter {
	Point dst;
	Direction midir;
	Missile *pParent;
	bool spellFizzled;
};

// Oracool, Round 2: the cold line. Each is described where it is defined.
void AddIceBlast(Missile &missile, AddMissileParameter &parameter);
void AddGlacialSpike(Missile &missile, AddMissileParameter &parameter);
void AddGlacialShatter(Missile &missile, AddMissileParameter &parameter);
void AddFrostNova(Missile &missile, AddMissileParameter &parameter);
void ProcessFrostNova(Missile &missile);
void AddBlizzard(Missile &missile, AddMissileParameter &parameter);
void ProcessBlizzard(Missile &missile);
void AddBlizzardShard(Missile &missile, AddMissileParameter &parameter);
void ProcessBlizzardShard(Missile &missile);
void AddFrozenOrb(Missile &missile, AddMissileParameter &parameter);
void ProcessFrozenOrb(Missile &missile);
void AddColdArmor(Missile &missile, AddMissileParameter &parameter);
void ProcessColdArmor(Missile &missile);
// Oracool, Round 3: the bow skills' arrow family - see oracool/rogue_arrows.h.
void AddRogueArrow(Missile &missile, AddMissileParameter &parameter);
void ProcessRogueArrow(Missile &missile);

void AddOpenNest(Missile &missile, AddMissileParameter &parameter);
void AddRuneOfFire(Missile &missile, AddMissileParameter &parameter);
void AddRuneOfLight(Missile &missile, AddMissileParameter &parameter);
void AddRuneOfNova(Missile &missile, AddMissileParameter &parameter);
void AddRuneOfImmolation(Missile &missile, AddMissileParameter &parameter);
void AddRuneOfStone(Missile &missile, AddMissileParameter &parameter);
void AddReflect(Missile &missile, AddMissileParameter &parameter);
void AddBerserk(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: Direction to place the spawn
 */
void AddHorkSpawn(Missile &missile, AddMissileParameter &parameter);
void AddJester(Missile &missile, AddMissileParameter &parameter);
void AddStealPotions(Missile &missile, AddMissileParameter &parameter);
void AddStealMana(Missile &missile, AddMissileParameter &parameter);
void AddSpectralArrow(Missile &missile, AddMissileParameter &parameter);
void AddWarp(Missile &missile, AddMissileParameter &parameter);
void AddLightningWall(Missile &missile, AddMissileParameter &parameter);
void AddBigExplosion(Missile &missile, AddMissileParameter &parameter);
void AddImmolation(Missile &missile, AddMissileParameter &parameter);
void AddLightningBow(Missile &missile, AddMissileParameter &parameter);
void AddMana(Missile &missile, AddMissileParameter &parameter);
void AddMagi(Missile &missile, AddMissileParameter &parameter);
void AddRingOfFire(Missile &missile, AddMissileParameter &parameter);
void AddSearch(Missile &missile, AddMissileParameter &parameter);
void AddChargedBoltBow(Missile &missile, AddMissileParameter &parameter);
void AddElementalArrow(Missile &missile, AddMissileParameter &parameter);
void AddArrow(Missile &missile, AddMissileParameter &parameter);
void AddPhasing(Missile &missile, AddMissileParameter &parameter);
void AddFirebolt(Missile &missile, AddMissileParameter &parameter);
void AddMagmaBall(Missile &missile, AddMissileParameter &parameter);
void AddTeleport(Missile &missile, AddMissileParameter &parameter);
void AddNovaBall(Missile &missile, AddMissileParameter &parameter);
void AddFireWall(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: X coordinate of the missile-light
 * var2: Y coordinate of the missile-light
 * var4: X coordinate of the missile-light
 * var5: Y coordinate of the missile-light
 */
void AddFireball(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: X coordinate of the missile
 * var2: Y coordinate of the missile
 */
void AddLightningControl(Missile &missile, AddMissileParameter &parameter);
void AddLightning(Missile &missile, AddMissileParameter &parameter);
void AddMissileExplosion(Missile &missile, AddMissileParameter &parameter);
void AddWeaponExplosion(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: Animation
 */
void AddTownPortal(Missile &missile, AddMissileParameter &parameter);
void AddFlashBottom(Missile &missile, AddMissileParameter &parameter);
void AddFlashTop(Missile &missile, AddMissileParameter &parameter);
void AddManaShield(Missile &missile, AddMissileParameter &parameter);
void AddFlameWave(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: Animation
 * var3: Light strength
 */
void AddGuardian(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: X coordinate of the destination
 * var2: Y coordinate of the destination
 */
void AddChainLightning(Missile &missile, AddMissileParameter &parameter);
void AddRhino(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: X coordinate of the missile-light
 * var2: Y coordinate of the missile-light
 */
void AddGenericMagicMissile(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: X coordinate of the missile-light
 * var2: Y coordinate of the missile-light
 */
void AddAcid(Missile &missile, AddMissileParameter &parameter);
void AddAcidPuddle(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: mmode of the monster
 * var2: mnum of the monster
 */
void AddStoneCurse(Missile &missile, AddMissileParameter &parameter);
void AddGolem(Missile &missile, AddMissileParameter &parameter);
void AddApocalypseBoom(Missile &missile, AddMissileParameter &parameter);
void AddHealing(Missile &missile, AddMissileParameter &parameter);
void AddHealOther(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: X coordinate of the missile-light
 * var2: Y coordinate of the missile-light
 * var4: X coordinate of the destination
 * var5: Y coordinate of the destination
 */
void AddElemental(Missile &missile, AddMissileParameter &parameter);
void AddIdentify(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: X coordinate of the first wave
 * var2: Y coordinate of the first wave
 * var3: Direction of the first wave
 * var4: Direction of the second wave
 * var5: X coordinate of the second wave
 * var6: Y coordinate of the second wave
 */
void AddFireWallControl(Missile &missile, AddMissileParameter &parameter);
void AddInfravision(Missile &missile, AddMissileParameter &parameter);
void AddEtherealize(Missile &missile, AddMissileParameter &parameter);
void AddBlessedHammer(Missile &missile, AddMissileParameter &parameter);
void AddWarcryRing(Missile &missile, AddMissileParameter &parameter);
void AddBlessedShieldImpact(Missile &missile, AddMissileParameter &parameter);
void AddCensusEffect(Missile &missile, AddMissileParameter &parameter);
void AddAcidJavelin(Missile &missile, AddMissileParameter &parameter);
/** @brief Oracool: SetMissAnim for code outside missiles.cpp - dresses a missile in one graphic. */
void UseMissileGraphic(Missile &missile, MissileGraphicID graphic);
void AddBlessedShieldThrow(Missile &missile, AddMissileParameter &parameter);
void AddFallingMace(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: X coordinate of the destination
 * var2: Y coordinate of the destination
 */
void AddFlameWaveControl(Missile &missile, AddMissileParameter &parameter);
void AddNova(Missile &missile, AddMissileParameter &parameter);
void AddRage(Missile &missile, AddMissileParameter &parameter);
void AddItemRepair(Missile &missile, AddMissileParameter &parameter);
void AddStaffRecharge(Missile &missile, AddMissileParameter &parameter);
void AddTrapDisarm(Missile &missile, AddMissileParameter &parameter);
void AddApocalypse(Missile &missile, AddMissileParameter &parameter);
void AddInferno(Missile &missile, AddMissileParameter &parameter);
void AddInfernoControl(Missile &missile, AddMissileParameter &parameter);

/**
 * var1: Light strength
 * var2: Base direction
 */
void AddChargedBolt(Missile &missile, AddMissileParameter &parameter);
void AddHolyBolt(Missile &missile, AddMissileParameter &parameter);
void AddResurrect(Missile &missile, AddMissileParameter &parameter);
void AddResurrectBeam(Missile &missile, AddMissileParameter &parameter);
void AddTelekinesis(Missile &missile, AddMissileParameter &parameter);
void AddBoneSpirit(Missile &missile, AddMissileParameter &parameter);
void AddRedPortal(Missile &missile, AddMissileParameter &parameter);
/** @brief Oracool: a rift portal standing in the Stonegate until the gate closes (oracool/stonegate.h). */
void AddRiftPortal(Missile &missile, AddMissileParameter &parameter);
void ProcessRiftPortal(Missile &missile);
void AddDiabloApocalypse(Missile &missile, AddMissileParameter &parameter);
Missile *AddMissile(Point src, Point dst, Direction midir, MissileID mitype,
    mienemy_type micaster, int id, int midam, int spllvl,
    Missile *parent = nullptr, std::optional<_sfx_id> lSFX = std::nullopt);
void ProcessElementalArrow(Missile &missile);
void ProcessArrow(Missile &missile);
void ProcessGenericProjectile(Missile &missile);
void ProcessNovaBall(Missile &missilei);
void ProcessAcidPuddle(Missile &missile);
void ProcessFireWall(Missile &missile);
void ProcessFireball(Missile &missile);
void ProcessHorkSpawn(Missile &missile);
void ProcessRune(Missile &missile);
void ProcessLightningWall(Missile &missile);
void ProcessBigExplosion(Missile &missile);
void ProcessLightningBow(Missile &missile);
void ProcessRingOfFire(Missile &missile);
void ProcessSearch(Missile &missile);
void ProcessLightningWallControl(Missile &missile);
void ProcessImmolation(Missile &missile);
void ProcessSpectralArrow(Missile &missile);
void ProcessLightningControl(Missile &missile);
void ProcessLightning(Missile &missile);
void ProcessTownPortal(Missile &missile);
void ProcessFlashBottom(Missile &missile);
void ProcessFlashTop(Missile &missile);
void ProcessFlameWave(Missile &missile);
void ProcessGuardian(Missile &missile);
void ProcessChainLightning(Missile &missile);
void ProcessWeaponExplosion(Missile &missile);
void ProcessMissileExplosion(Missile &missile);
void ProcessAcidSplate(Missile &missile);
void ProcessTeleport(Missile &missile);
void ProcessStoneCurse(Missile &missile);
void ProcessApocalypseBoom(Missile &missile);
void ProcessRhino(Missile &missile);
void ProcessFireWallControl(Missile &missile);
void ProcessInfravision(Missile &missile);
void ProcessEtherealize(Missile &missile);
void ProcessBlessedHammer(Missile &missile);
void ProcessWarcryRing(Missile &missile);
void ProcessCensusEffect(Missile &missile);
void ProcessAcidJavelin(Missile &missile);
/**
 * @brief Oracool: where Blessed Hammer is, in screen pixels from its caster's tile, @p ticks after
 * the cast. The one formula both the sprite and the hit read, so the two cannot drift apart.
 */
Displacement BlessedHammerOffsetAt(float ticks);
/** @brief Oracool: how many times a tick ProcessBlessedHammer looks for the tile the hammer is in. */
constexpr int BlessedHammerSubSteps = 8;
/** @brief Oracool: how many monsters one Blessed Shield throw strikes - its target and two bounces (2026-09-11). */
constexpr int BlessedShieldTargets = 3;
/** @brief What each strike carries, in percent of the throw (user: "100% on first, 75% on second, 50% on third"). */
constexpr int BlessedShieldStrikePercent[BlessedShieldTargets] = { 100, 75, 50 };
/** @brief The damage of Blessed Shield's strike @p hitsSoFar (0 = the first) from a throw of @p baseDamage. At least 1; 0 past the last. */
constexpr int BlessedShieldHitDamage(int baseDamage, int hitsSoFar)
{
	if (hitsSoFar < 0 || hitsSoFar >= BlessedShieldTargets)
		return 0;
	const int damage = baseDamage * BlessedShieldStrikePercent[hitsSoFar] / 100;
	return damage < 1 ? 1 : damage;
}
void ProcessBlessedShieldThrow(Missile &missile);
void ProcessFallingMace(Missile &missile);
void ProcessApocalypse(Missile &missile);
void ProcessFlameWaveControl(Missile &missile);
void ProcessNova(Missile &missile);
void ProcessRage(Missile &missile);
void ProcessInferno(Missile &missile);
void ProcessInfernoControl(Missile &missile);
void ProcessChargedBolt(Missile &missile);
void ProcessHolyBolt(Missile &missile);
void ProcessElemental(Missile &missile);
void ProcessBoneSpirit(Missile &missile);
void ProcessResurrectBeam(Missile &missile);
void ProcessRedPortal(Missile &missile);
void ProcessMissiles();
void missiles_process_charge();
void RedoMissileFlags();

#ifdef BUILD_TESTING
void TestRotateBlockedMissile(Missile &missile);
#endif

} // namespace devilution
