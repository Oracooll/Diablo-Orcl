/**
 * @file oracool/rift_bosses.h
 *
 * Oracool: the rift boss roster (RTM dev note, 2026-10-04: "we need to diversify rift bosses by tinting them and
 * resizing them ... introduce more monster sprites as bosses ... play with their stats, speeds, attack speeds, dmg").
 * Proposed on the Rift Boss Roster page and approved whole (all 32, 2026-10-04), with four answers:
 *   Q1 "Let them stay. Have them have different versions with altered stats, tints and size." - the four faces of old
 *      (Leoric, the Butcher, Diablo, Na-Krul) stay as they were, and six altered versions of them join.
 *   Q2 "They may go bigger. Up to 250%." - Guardian bosses stand 180-250%; MaxBossSizePercent.
 *   Q3 "Sure." - a boss is sometimes met outside its home tilesets (OffHomePercent).
 *   Q4 "No." - nothing new is drawn or recorded: every boss is a vanilla or Hellfire sprite, every trick an engine piece
 *      that already exists.
 *
 * ## One table, read everywhere the guardian was a coin flip
 *
 * OpenCommon draws a row (PickRiftBoss) and the rift keeps its index; the row says which monster type rises, which
 * unique row dresses it (the four faces and their versions), how big, in what colour, how strong, how fast, what it
 * does, who comes with it and where it lives. Nothing is stored on the monster: the guardian is known by its slot
 * (IsRiftGuardian), so every hook here asks ActiveRiftBossFor and gets nullptr for anything else.
 *
 * ## Strength
 *
 * Multipliers are on the monster AFTER the rift's tier scaling. A new boss's base life and damage are first brought up
 * to the rift's floor band (a Cathedral zombie in a Hell-band rift is a Hell monster's size before its x8), so a boss
 * stays a boss at every rung. The four faces of old are left exactly as they were - no band, no multiplier.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "levels/gendung.h"
#include "misdat.h"
#include "missiles.h"
#include "monstdat.h"
#include "oracool/missile_tint.h"
#include "oracool/rift.h"

namespace devilution {
struct Monster;
struct Player;
enum class MonsterGraphic : uint8_t;
} // namespace devilution

namespace devilution::oracool {

/** @brief What a boss does beyond its monster's own AI. Each is a small behaviour on existing engine pieces. */
enum class BossSignature : uint8_t {
	None,
	AcidPoolOnHit,      ///< Gorefeast: an acid puddle under the hero on every landed blow.
	RaiseDead,          ///< Rattlebone: two skeletons risen beside him every 8 s.
	GraveRaise,         ///< The Gravewarden: a skeleton risen beside him every 4 s.
	Knockback,          ///< Kharzug the Ram: his blows knock the hero back (MFLAG_KNOCKBACK, the vanilla push).
	ChillOnHit,         ///< Frostmaw: a landed blow chills twice as long.
	AcidFanVenom,       ///< The Swarm Mother: her acid in a fan of three, venom on every hit.
	Ambush,             ///< Hollowhand: the first blow after he vanishes does triple damage.
	CasterBlink,        ///< Magistrate Vael: blinks away when struck; fire, lightning and fire again in turn.
	FireTrail,          ///< The Pyre Lord: flames left on the tiles he walks.
	StormChain,         ///< The Thunder Throne: every bolt doubled; a ring of lightning at half life.
	StoneRest,          ///< Grimwing: turns to stone and heals below 30%, twice.
	FireballFan,        ///< Magmaheart: his magma in a fan of three.
	Cleave,             ///< Captain Sareth: every fourth landed blow a cleave, double and knocking back.
	LifeOnHit,          ///< The Endless Maw: 2% of his life back for every hit.
	BurningPools,       ///< Kragh: a landed blow leaves flames under the hero.
	LightningOnHit,     ///< Stormscale: lightning strikes where his bite lands.
	HorkBrood,          ///< The Hork Matriarch: two Hork Spawn every 6 s.
	VenomOnHit,         ///< The Stinger Queen: venom on every hit.
	BoneRing,           ///< The Bone Colossus: a ring of bone bolts every 10 s.
	BlinkBoltRing,      ///< Malignus Unbound, the Shadow of Terror: blinks every 6 s, a ring of bolts at each landing.
	NovaWhenStruck,     ///< The Balrog Ascendant, Diablo the Prime Evil: a ring of fire when struck, once a second.
	ManaBurn,           ///< The Vortex Sovereign, Na-Krul the Void Prisoner: each hit burns as much mana.
	FrostNovaRaise,     ///< Arch-Lich Zhar'ul: a cold ring every 8 s; four skeletons at half life.
	CallGuard,          ///< Doom Commander Vargoth: four of his escort called at 66% and 33%.
	Bleed,              ///< Gutrend, the Abattoir King: a landed blow bleeds the hero over five seconds.
	GroundSlam,         ///< Biclops Warlord Ghorm, Na-Krul Unchained: a slam around him every 7 s that staggers.
	Regenerate,         ///< The Flesh Colossus: 1% a second unless fire struck him in the last 3 s.
	Burrow,             ///< The Defiler Empress: surfaces beside the hero every 9 s.
	Split,              ///< Reality Weaver Prime: two weak copies at 75% and 40%, gone after 10 s.
	LifeSteal,          ///< The Pale Reaper: 5% of each hit as life.
};

/** @brief Home tilesets, one bit per dungeon_type. */
constexpr uint8_t HomeOf(dungeon_type type) { return static_cast<uint8_t>(1u << static_cast<unsigned>(type)); }

struct RiftBossDef {
	/** The page's name; nullptr for the four faces of old, which keep their unique row's name. */
	const char *name;
	_monster_id type;
	/** The unique row that dresses it (SkeletonKing, Butcher, NaKrul), or the face whose sounds it borrows (Diablo); None otherwise. */
	RiftGuardianType face;
	RiftKind kind;
	uint16_t sizePercent;
	Tint tint;
	uint32_t rgb;
	uint16_t lifePercent;
	uint16_t damagePercent;
	uint16_t armourPercent;
	/** -1: two frames skipped (fast), +1: a quarter of its walking ticks lost (slow), 0: as drawn. */
	int8_t walk;
	int8_t attack;
	/** IMMUNE_* / RESIST_* bits laid over its own. */
	uint16_t resistAdd;
	bool steady;
	BossSignature signature;
	_monster_id escort;
	uint8_t escortCount;
	/** A type the signature raises or calls that is neither the boss nor its escort (skeletons, Hork Spawn), or MT_INVALID. */
	_monster_id summon;
	uint8_t homes;
	/** The page's "tier N+": met from rung 2N-1 of the difficulty's sixteen. */
	uint8_t minTier;
	/** Whether this is one of the four faces of old, left exactly as it was. */
	bool original;
};

/** @brief The largest a boss is drawn (Q2). */
constexpr unsigned MaxBossSizePercent = 250;
/** @brief The chance a boss is met outside its home tilesets (Q3). */
constexpr int OffHomePercent = 15;

size_t RiftBossCount();
const RiftBossDef &RiftBossAt(size_t index);

/** @brief Whether row @p index may be drawn for a rift of @p kind on band rung @p rung (1..16). */
bool RiftBossEligible(size_t index, RiftKind kind, int rung);

/**
 * @brief Draws the boss of a new rift: an eligible row of @p kind at @p rung, never one of the last three met, and its
 * tileset - one of its homes, or any tileset OffHomePercent of the time. Uses the game RNG.
 */
size_t PickRiftBoss(RiftKind kind, int rung, dungeon_type &tileset);

/** @brief The open rift's boss row. */
const RiftBossDef &ActiveRiftBoss();
/** @brief The open rift's row if @p monster is its risen guardian, else nullptr. */
const RiftBossDef *ActiveRiftBossFor(const Monster &monster);
/** @brief The name the rift's boss goes by: the row's, or the unique's for a face of old. */
std::string RiftBossName();

/** @brief The types the open rift's floor loads for its boss beside the boss's own: its escort and what it raises. */
std::vector<_monster_id> RiftBossExtraTypes();

/** @brief The guardian's draw size in percent, or 0 for anything else (monster_scale asks). */
unsigned RiftBossSizePercent(const Monster &monster);
/** @brief Frames the guardian skips from @p graphic (its fast walk or attack). */
int RiftBossSkippedFrames(const Monster &monster, MonsterGraphic graphic);
/** @brief Whether the guardian loses this tick: a slow walker's every fourth walking tick, a resting stone's every tick. */
bool RiftBossSkipsThisTick(const Monster &monster);
/** @brief The guardian's colour glaze for the 32-bit draw; tint None for none. */
struct BossGlaze {
	Tint tint;
	uint32_t rgb;
};
BossGlaze RiftBossGlaze(const Monster &monster);
/** @brief Whether the guardian stands against knockback. */
bool RiftBossIsSteady(const Monster &monster);

/** @brief The boss has risen (rift.cpp, once its slot is named): strength, resists, flags, escort, its look rebound. */
void OnRiftBossRisen(Monster &monster);
/** @brief Per game tick inside a rift: split copies past their ten seconds go. */
void TickRiftConjured();
/** @brief Per game tick while the boss stands on the hero's level: timers, thresholds, rests. */
void TickRiftBoss(Monster &monster);
/**
 * @brief A blow of the boss's landed on @p player for @p landed (64ths). A @p missile blow carries only what rides any hit
 * (venom, bleed, chill, mana burn, life back): the signatures that lay a missile of their own (acid, flames, lightning) or
 * belong to a swing (ambush, cleave) are melee's alone - Stormscale's lightning landing would otherwise call lightning again.
 */
void OnRiftBossDealtBlow(Monster &monster, Player &player, int landed, bool missile = false);
/** @brief The boss took @p damage (64ths) of @p type. */
void OnRiftBossStruck(Monster &monster, DamageType type, int damage);
/** @brief The missile a ranged attack of the boss fires instead of @p missile (Magistrate Vael's turn of casts). */
MissileID RiftBossShotType(const Monster &monster, MissileID missile);
/** @brief After a ranged attack of the boss fired @p missile at @p damage: the fan's side shots, the doubled bolt. */
void RiftBossExtraShots(Monster &monster, MissileID missile, int damage);

/** @brief A split copy, conjured for ten seconds: it drops nothing and pays no experience. */
bool IsRiftConjured(const Monster &monster);
/** @brief @p monster's slot is dying or freed: it is no longer anything this file conjured. */
void ForgetRiftConjured(const Monster &monster);
/** @brief A new rift, a new game: every timer, threshold and conjured slot is forgotten; the last-met list survives a rift. */
void ResetRiftBossRun();
void ResetRiftBossesForNewGame();

/** @brief The `riftboss` debug command: the next rift opened draws row @p index, on one of its homes. */
void ForceNextRiftBoss(size_t index);
/** @brief The row named by @p query: a 1-based number, or a case-blind piece of its name (or its monster's). */
std::optional<size_t> FindRiftBoss(std::string_view query);

/** @brief Test seam: the row a rift of @p kind at @p rung would draw for a forced roll, ignoring the last-met list. */
size_t RiftBossForTest(RiftKind kind, int rung, int roll);

} // namespace devilution::oracool
