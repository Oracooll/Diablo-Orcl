#include "oracool/rift_bosses.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <iterator>

#include "diablo.h" // HeadlessMode
#include "engine.h" // GetDirection, IsAnyOf
#include "engine/backbuffer_state.hpp" // RedrawComponent - a burned mana orb
#include "engine/direction.hpp"
#include "engine/displacement.hpp"
#include "engine/random.hpp"
#include "monster.h"
#include "oracool/cold.h"
#include "oracool/monster_difficulty.h" // DemoteImmunitiesToResistances
#include "oracool/rfa12_actives.h" // SightLineClear
#include "oracool/venom.h"
#include "player.h"

namespace devilution::oracool {

namespace {

constexpr uint8_t Cat = HomeOf(DTYPE_CATHEDRAL);
constexpr uint8_t Cata = HomeOf(DTYPE_CATACOMBS);
constexpr uint8_t Cave = HomeOf(DTYPE_CAVES);
constexpr uint8_t Hell = HomeOf(DTYPE_HELL);
constexpr uint8_t Nest = HomeOf(DTYPE_NEST);
constexpr uint8_t Crypt = HomeOf(DTYPE_CRYPT);
constexpr uint8_t Anywhere = Cat | Cata | Cave | Hell | Nest | Crypt;

constexpr RiftKind Neph = RiftKind::Nephalem;
constexpr RiftKind Guard = RiftKind::Guardian;
constexpr RiftGuardianType NoFace = RiftGuardianType::None;
using Sig = BossSignature;

// The roster, as the Rift Boss Roster page has it (approved whole, 2026-10-04). Order is the page's: the four faces of old
// first, their six new versions (Q1), then the page's twenty Nephalem and twelve Guardian bosses. Sizes over 170 are the
// Guardian bosses grown for Q2 ("up to 250%"). Columns:
//   name, type, face, kind, size%, tint, rgb, life%, damage%, armour%, walk, attack, resists added, steady, signature,
//   escort, escort count, summon, homes, tier N+ (rung 2N-1), original
// clang-format off
const RiftBossDef Roster[] = {
	// The four faces of old, exactly as they were (Q1: "Let them stay").
	{ nullptr,                            MT_SKING,    RiftGuardianType::SkeletonKing, Neph,  100, Tint::None,    0,        100, 100, 100,  0,  0, 0,                                             false, Sig::None,           MT_INVALID, 0, MT_INVALID, Anywhere,          1, true  },
	{ nullptr,                            MT_CLEAVER,  RiftGuardianType::Butcher,      Neph,  100, Tint::None,    0,        100, 100, 100,  0,  0, 0,                                             false, Sig::None,           MT_INVALID, 0, MT_INVALID, Anywhere,          1, true  },
	{ nullptr,                            MT_DIABLO,   RiftGuardianType::Diablo,       Guard, 100, Tint::None,    0,        100, 100, 100,  0,  0, 0,                                             false, Sig::None,           MT_INVALID, 0, MT_INVALID, Hell,              1, true  },
	{ nullptr,                            MT_NAKRUL,   RiftGuardianType::NaKrul,       Guard, 100, Tint::None,    0,        100, 100, 100,  0,  0, 0,                                             false, Sig::None,           MT_INVALID, 0, MT_INVALID, Crypt,             1, true  },
	// Their versions (Q1: "different versions with altered stats, tints and size").
	{ "The Mad King",                     MT_SKING,    RiftGuardianType::SkeletonKing, Neph,  135, Tint::Hue,     0xA01818, 140, 140, 120,  0, -1, 0,                                             false, Sig::None,           MT_INVALID, 0, MT_INVALID, Cat | Cata | Crypt, 2, false },
	{ "The Abattoir King",                MT_CLEAVER,  RiftGuardianType::Butcher,      Neph,  140, Tint::Glint,   0x8A1010, 150, 130, 110, -1,  0, 0,                                             false, Sig::Bleed,          MT_INVALID, 0, MT_INVALID, Cat | Cata,        2, false },
	{ "Diablo, the Prime Evil",           MT_DIABLO,   RiftGuardianType::Diablo,       Guard, 250, Tint::Glint,   0xFF3010, 150, 130, 130,  0,  0, 0,                                             true,  Sig::NovaWhenStruck,  MT_INVALID, 0, MT_INVALID, Hell,              3, false },
	{ "The Shadow of Terror",             MT_DIABLO,   RiftGuardianType::Diablo,       Guard, 200, Tint::Hue,     0x3A2060, 120, 120, 110, -1, -1, 0,                                             false, Sig::BlinkBoltRing,  MT_INVALID, 0, MT_INVALID, Hell,              2, false },
	{ "Na-Krul Unchained",                MT_NAKRUL,   RiftGuardianType::NaKrul,       Guard, 230, Tint::Glint,   0xE8C040, 150, 130, 130,  0,  0, 0,                                             true,  Sig::GroundSlam,     MT_INVALID, 0, MT_INVALID, Crypt,             3, false },
	{ "Na-Krul, the Void Prisoner",       MT_NAKRUL,   RiftGuardianType::NaKrul,       Guard, 200, Tint::Hue,     0x6A2AA8, 130, 120, 110,  0,  0, 0,                                             false, Sig::ManaBurn,       MT_INVALID, 0, MT_INVALID, Crypt,             2, false },
	// Nephalem Rifts.
	{ "Gorefeast the Bloated",            MT_FAT,      NoFace, Neph,  155, Tint::Hue,      0x9FB04A, 600, 150, 120,  1,  0, IMMUNE_ACID,                                   false, Sig::AcidPoolOnHit,  MT_FLAYED,   4, MT_INVALID,  Cat | Cata,        1, false },
	{ "Rattlebone, Marshal of the Dead",  MT_XSKELSD,  NoFace, Neph,  145, Tint::Glint,    0xD8E4FF, 500, 180, 130,  0, -1, RESIST_LIGHTNING,                              false, Sig::RaiseDead,      MT_XSKELSD,  3, MT_XSKELSD,  Cat | Cata | Crypt, 1, false },
	{ "The Grey Plague",                  MT_YZOMBIE,  NoFace, Neph,  170, Tint::Hue,      0x7D8A7A, 800, 140, 100,  1,  0, 0,                                             false, Sig::None,           MT_BZOMBIE,  6, MT_INVALID,  Cat | Cata,        1, false },
	{ "Kharzug the Ram",                  MT_HORNED,   NoFace, Neph,  155, Tint::Hue,      0x8C4A2A, 500, 220, 140,  0,  0, RESIST_FIRE,                                   false, Sig::Knockback,      MT_MUDRUN,   2, MT_INVALID,  Cata | Cave,       2, false },
	{ "Frostmaw",                         MT_FROSTC,   NoFace, Neph,  160, Tint::Glint,    0x96D2FF, 500, 180, 130, -1,  0, RESIST_COLD,                                   false, Sig::ChillOnHit,     MT_FROSTC,   3, MT_INVALID,  Cave | Nest,       3, false },
	{ "The Swarm Mother",                 MT_SPIDLORD, NoFace, Neph,  160, Tint::Hue,      0x5FBF3A, 500, 160, 120,  0,  0, IMMUNE_ACID,                                   false, Sig::AcidFanVenom,   MT_ARACHNON, 4, MT_INVALID,  Cave | Nest,       2, false },
	{ "Hollowhand",                       MT_UNSEEN,   NoFace, Neph,  145, Tint::Hue,      0x3A2A4A, 400, 250, 100, -1, -1, RESIST_MAGIC,                                  false, Sig::Ambush,         MT_STALKER,  3, MT_INVALID,  Cata | Cave,       3, false },
	{ "Grand Magistrate Vael",            MT_ADVOCATE, NoFace, Neph,  140, Tint::Hue,      0xE2B356, 400, 180, 100,  0,  0, RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING, false, Sig::CasterBlink,    MT_COUNSLR,  2, MT_INVALID,  Hell | Cata,       4, false },
	{ "The Pyre Lord",                    MT_HELLBURN, NoFace, Neph,  150, Tint::Glint,    0xFFF0C8, 500, 170, 120,  0,  0, IMMUNE_FIRE,                                   false, Sig::FireTrail,      MT_FLAMLRD,  4, MT_INVALID,  Cave | Hell,       4, false },
	{ "The Thunder Throne",               MT_MAEL,     NoFace, Neph,  150, Tint::Electric, 0,        500, 180, 120, -1,  0, IMMUNE_LIGHTNING,                              false, Sig::StormChain,     MT_RSTORM,   3, MT_INVALID,  Hell | Cave,       5, false },
	{ "Grimwing the Pale",                MT_DEATHW,   NoFace, Neph,  150, Tint::Hue,      0xC9C3B8, 500, 160, 150, -1,  0, RESIST_LIGHTNING,                              false, Sig::StoneRest,      MT_GARGOYLE, 4, MT_INVALID,  Cave | Hell,       5, false },
	{ "Magmaheart",                       MT_WMAGMA,   NoFace, Neph,  155, Tint::Glint,    0xFF7A1A, 600, 170, 130,  1,  0, IMMUNE_FIRE,                                   false, Sig::FireballFan,    MT_YMAGMA,   3, MT_INVALID,  Cave | Hell,       4, false },
	{ "Captain Sareth of the Red Watch",  MT_RBLACK,   NoFace, Neph,  145, Tint::Hue,      0xB0121A, 600, 200, 160,  0, -1, 0,                                             true,  Sig::Cleave,         MT_RTBLACK,  3, MT_INVALID,  Hell,              6, false },
	// The page's Devourer and Cave Slugs are worms, cut from the 1996 archives (no worm sheet ships): the Bone Gasher's maw.
	{ "The Endless Maw",                  MT_YSCAV,    NoFace, Neph,  170, Tint::Hue,      0x6A3D2A, 900, 140, 100,  1,  0, 0,                                             false, Sig::LifeOnHit,      MT_WSCAV,    4, MT_INVALID,  Cave,              2, false },
	{ "Kragh, Pit of Embers",             MT_XACID,    NoFace, Neph,  160, Tint::Hue,      0xFF5A2A, 500, 170, 120,  0,  0, IMMUNE_FIRE,                                   false, Sig::BurningPools,   MT_BACID,    4, MT_INVALID,  Cave | Hell,       4, false },
	{ "Stormscale",                       MT_GSNAKE,   NoFace, Neph,  150, Tint::Hue,      0x4AA0FF, 500, 180, 130, -1,  0, IMMUNE_LIGHTNING,                              false, Sig::LightningOnHit, MT_BSNAKE,   3, MT_INVALID,  Cave | Hell,       5, false },
	{ "The Hork Matriarch",               MT_HORKDMN,  NoFace, Neph,  150, Tint::Hue,      0xC07AC8, 600, 150, 120,  1,  0, RESIST_FIRE,                                   false, Sig::HorkBrood,      MT_HORKSPWN, 4, MT_HORKSPWN, Nest,              4, false },
	{ "The Stinger Queen",                MT_VENMTAIL, NoFace, Neph,  155, Tint::Hue,      0xD6E04A, 500, 180, 120, -1, -1, IMMUNE_ACID,                                   false, Sig::VenomOnHit,     MT_STINGER,  4, MT_INVALID,  Nest,              4, false },
	{ "The Gravewarden",                  MT_GRAVEDIG, NoFace, Neph,  150, Tint::Hue,      0x8A7FA8, 500, 160, 130,  0,  0, RESIST_MAGIC,                                  false, Sig::GraveRaise,     MT_TOMBRAT,  3, MT_XSKELAX,  Crypt,             4, false },
	{ "The Bone Colossus",                MT_BONEDEMN, NoFace, Neph,  160, Tint::Hue,      0xEFE6D0, 600, 180, 160,  1,  0, IMMUNE_ACID | RESIST_MAGIC,                    false, Sig::BoneRing,       MT_SKLWING,  3, MT_INVALID,  Crypt,             5, false },
	// Guardian Rifts, grown for Q2.
	{ "Malignus Unbound",                 MT_DARKMAGE, NoFace, Guard, 190, Tint::Glint,    0x7A3CFF, 1000, 200, 130, 0,  0, RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING, false, Sig::BlinkBoltRing,  MT_ADVOCATE, 3, MT_INVALID,  Hell,              1, false },
	{ "The Balrog Ascendant",             MT_BALROG,   NoFace, Guard, 210, Tint::Glint,    0xFF4A10, 1200, 220, 150, 0,  0, IMMUNE_FIRE,                                   false, Sig::NovaWhenStruck, MT_MEGA,     3, MT_INVALID,  Hell,              1, false },
	{ "The Vortex Sovereign",             MT_VTEXLRD,  NoFace, Guard, 200, Tint::Hue,      0x5A2A8A, 1100, 200, 140, 0, -1, IMMUNE_MAGIC,                                  false, Sig::ManaBurn,       MT_GUARD,    3, MT_INVALID,  Hell,              2, false },
	{ "Arch-Lich Zhar'ul",                MT_ARCHLICH, NoFace, Guard, 190, Tint::Glint,    0x9AE07A, 1000, 200, 120, 0,  0, RESIST_COLD | RESIST_MAGIC,                    false, Sig::FrostNovaRaise, MT_LICH,     4, MT_XSKELAX,  Crypt,             1, false },
	{ "Doom Commander Vargoth",           MT_RTBLACK,  NoFace, Guard, 200, Tint::Hue,      0xC9A14A, 1200, 240, 200, 0, -1, 0,                                             true,  Sig::CallGuard,      MT_BTBLACK,  4, MT_INVALID,  Hell,              2, false },
	{ "Leoric Ascended",                  MT_SKING,    RiftGuardianType::SkeletonKing, Guard, 200, Tint::Glint, 0x8FD0FF, 1200, 220, 150, -1, 0, RESIST_LIGHTNING,                  false, Sig::None,           MT_XSKELSD,  4, MT_XSKELSD,  Cat | Cata | Crypt, 1, false },
	{ "Gutrend, the Butcher's Brother",   MT_CLEAVER,  RiftGuardianType::Butcher,      Guard, 180, Tint::Hue,   0x9BB090, 1200, 240, 130, -1, 0, RESIST_FIRE,                       false, Sig::Bleed,          MT_INVALID,  0, MT_INVALID,  Cat | Cata,        1, false },
	{ "Biclops Warlord Ghorm",            MT_BICLOPS,  NoFace, Guard, 220, Tint::Hue,      0x9A9A8E, 1300, 240, 180, 1,  0, 0,                                             true,  Sig::GroundSlam,     MT_REAPER,   2, MT_INVALID,  Crypt,             2, false },
	{ "The Flesh Colossus",               MT_FLESTHNG, NoFace, Guard, 230, Tint::Hue,      0xC23A4A, 1400, 180, 120, 1,  0, IMMUNE_ACID,                                   false, Sig::Regenerate,     MT_FLESTHNG, 4, MT_INVALID,  Crypt | Nest,      2, false },
	{ "The Defiler Empress",              MT_DEFILER,  NoFace, Guard, 200, Tint::Hue,      0x4A6A2A, 1100, 200, 160, 0,  0, IMMUNE_ACID | RESIST_FIRE,                     false, Sig::Burrow,         MT_TORCHANT, 4, MT_INVALID,  Nest,              2, false },
	{ "Reality Weaver Prime",             MT_REALWEAV, NoFace, Guard, 180, Tint::Glint,    0xFFFFFF, 1000, 200, 120, -1, 0, IMMUNE_MAGIC,                                  false, Sig::Split,          MT_PAINMSTR, 3, MT_INVALID,  Hell | Cave,       3, false },
	{ "The Pale Reaper",                  MT_REAPER,   NoFace, Guard, 200, Tint::Hue,      0xD0D6E0, 1100, 230, 130, 0, -1, RESIST_COLD | RESIST_MAGIC,                    false, Sig::LifeSteal,      MT_SKLWING,  3, MT_INVALID,  Crypt,             3, false },
};
// clang-format on
constexpr size_t RosterSize = std::size(Roster);

// The clocks, in game ticks (20 a second).
constexpr int RaiseDeadTicks = 8 * 20;
constexpr int GraveRaiseTicks = 4 * 20;
constexpr int HorkBroodTicks = 6 * 20;
constexpr int BoneRingTicks = 10 * 20;
constexpr int BlinkBoltTicks = 6 * 20;
constexpr int FrostNovaTicks = 8 * 20;
constexpr int GroundSlamTicks = 7 * 20;
constexpr int BurrowTicks = 9 * 20;
constexpr int FireTrailTicks = 8;
constexpr int StruckCooldownTicks = 20;       // once a second: the Balrog's nova
constexpr int BlinkStruckCooldownTicks = 40;  // Vael's blink: twice as patient, or he never casts
constexpr int RegenStepTicks = 20;
constexpr int FireStopsRegenTicks = 3 * 20;
constexpr int StoneRestTicks = 2 * 20;
constexpr int ConjuredTicks = 10 * 20;
constexpr int MaxRaised = 8;
/** How far the hero may be for a timer to run: he has to be in the fight, not three rooms away. */
constexpr int EngageTiles = 12;

struct Conjured {
	int id;
	int ticksLeft;
};

/** One risen boss's running state. A new rift (ResetRiftBossRun) or a new game starts it again. */
struct Run {
	int timer = 0;
	int trail = 0;
	int regen = 0;
	int walkTick = 0;
	int struckCooldown = 0;
	int fireTicks = 0;
	int restTicks = 0;
	int rests = 0;
	int swings = 0;
	int castTurn = 0;
	uint8_t thresholds = 0;
	bool ambushReady = false;
	bool struckPending = false;
	/** What his damage could not fit in its bytes, as a percent laid on at every blow (round 95 audit). 100 when it fit. */
	int damageOverflow = 100;
	std::vector<int> raised;
	std::vector<Conjured> conjured;
};

Run RunState;
/** The rows met lately, newest first, so the same face is not met twice running; survives a rift, not a game. */
std::array<int, 3> LastMet { -1, -1, -1 };
std::optional<size_t> Forced;

int RungForTier(uint8_t minTier)
{
	return 2 * std::max<int>(minTier, 1) - 1;
}

dungeon_type AnyTileset()
{
	constexpr dungeon_type Sets[] = { DTYPE_CATHEDRAL, DTYPE_CATACOMBS, DTYPE_CAVES, DTYPE_HELL, DTYPE_NEST, DTYPE_CRYPT };
	return Sets[GenerateRnd(6)];
}

dungeon_type HomeTileset(const RiftBossDef &def)
{
	dungeon_type homes[6];
	int count = 0;
	for (const dungeon_type type : { DTYPE_CATHEDRAL, DTYPE_CATACOMBS, DTYPE_CAVES, DTYPE_HELL, DTYPE_NEST, DTYPE_CRYPT }) {
		if ((def.homes & HomeOf(type)) != 0)
			homes[count++] = type;
	}
	return count == 0 ? AnyTileset() : homes[GenerateRnd(count)];
}

std::vector<size_t> EligibleRows(RiftKind kind, int rung)
{
	std::vector<size_t> rows;
	for (size_t i = 0; i < RosterSize; i++) {
		if (RiftBossEligible(i, kind, rung))
			rows.push_back(i);
	}
	// A rung below every row's band still meets somebody: the kind's rows of the lowest band.
	if (rows.empty()) {
		for (size_t i = 0; i < RosterSize; i++) {
			if (Roster[i].kind == kind && Roster[i].minTier <= 1)
				rows.push_back(i);
		}
	}
	return rows;
}

std::string Lower(std::string_view text)
{
	std::string out(text);
	for (char &c : out)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return out;
}

int RollDamage(const Monster &monster)
{
	const int rolled = monster.minDamage + GenerateRnd(std::max(monster.maxDamage - monster.minDamage, 0) + 1);
	return rolled * RiftBossDamagePercent(monster) / 100;
}

/**
 * @brief How much a new boss's base life and damage grow to meet the rift's band (percent, 100..400): the band rung against
 * the middle floor its type walks in a dungeon. The Hive's and the Crypt's types sit on the Caves' and Hell's rungs.
 */
int BandPercent(const RiftBossDef &def)
{
	const MonsterData &data = MonstersData[def.type];
	int middle = (std::max<int>(data.minDunLvl, 1) + std::max<int>(data.maxDunLvl, 1)) / 2;
	if (middle > 16)
		middle -= 8;
	middle = std::max(middle, 1);
	return std::clamp(100 * RiftMonsterBandFloor() / middle, 100, 400);
}

uint8_t ScaledByte(int value, int percentA, int percentB)
{
	const int64_t scaled = static_cast<int64_t>(value) * percentA / 100 * percentB / 100;
	return static_cast<uint8_t>(std::clamp<int64_t>(scaled, 0, 255));
}

void Heal(Monster &monster, int amount)
{
	if (amount <= 0 || monster.hitPoints <= 0 || monster.mode == MonsterMode::Death)
		return;
	monster.hitPoints = std::min(monster.hitPoints + amount, monster.maxHitPoints);
}

Player *HeroInReach(const Monster &monster, int tiles)
{
	Player *hero = MyPlayer;
	if (hero == nullptr || !hero->isOnActiveLevel() || hero->_pHitPoints <= 0)
		return nullptr;
	if (monster.position.tile.WalkingDistance(hero->position.tile) > tiles)
		return nullptr;
	// And in his sight (round 101 audit): the slam, the cold ring's chill and every timer ran through walls and closed doors, and
	// he blinked into the next room after a hero he could not see.
	if (!SightLineClear(monster.position.tile, hero->position.tile))
		return nullptr;
	return hero;
}

/** @brief @p count missiles of @p type from the boss to every side, at the boss's own damage. */
void FireRing(Monster &monster, MissileID type, int count)
{
	const Point from = monster.position.tile;
	for (int k = 0; k < count; k++) {
		const auto dir = static_cast<Direction>((k * 8 / count) % 8);
		AddMissile(from, from + Displacement(dir) * 4, dir, type, TARGET_PLAYERS, static_cast<int>(monster.getId()), RollDamage(monster), 0);
	}
}

/** @brief Two more of @p type beside the shot the boss just fired at its enemy, 45 degrees either side, at @p damage. */
void FireSideShots(Monster &monster, MissileID type, int damage)
{
	const Point from = monster.position.tile;
	const Direction aim = GetDirection(from, monster.enemyPosition);
	const int reach = std::max(4, from.WalkingDistance(monster.enemyPosition));
	for (const Direction side : { Left(aim), Right(aim) })
		AddMissile(from, from + Displacement(side) * reach, side, type, TARGET_PLAYERS, static_cast<int>(monster.getId()), damage, 0);
}

int CountRaised()
{
	int alive = 0;
	for (const int id : RunState.raised) {
		const Monster &raised = Monsters[id];
		if (raised.hitPoints > 0 && raised.mode != MonsterMode::Death)
			alive++;
	}
	return alive;
}

void Raise(Monster &monster, _monster_id kind, int count)
{
	const size_t typeIndex = LevelMonsterTypeIndexOf(kind);
	if (typeIndex == LevelMonsterTypeCount)
		return;
	RunState.raised.erase(std::remove_if(RunState.raised.begin(), RunState.raised.end(), [](int id) {
		return Monsters[id].hitPoints <= 0 || Monsters[id].mode == MonsterMode::Death;
	}),
	    RunState.raised.end());
	for (int i = 0; i < count && CountRaised() < MaxRaised; i++) {
		if (Monster *raised = SpawnRiftBossMinion(typeIndex, monster.position.tile, /*rise=*/true))
			RunState.raised.push_back(static_cast<int>(raised->getId()));
	}
}

void CallEscort(Monster &monster, const RiftBossDef &def, int count)
{
	if (def.escort == MT_INVALID)
		return;
	const size_t typeIndex = LevelMonsterTypeIndexOf(def.escort);
	if (typeIndex == LevelMonsterTypeCount)
		return;
	for (int i = 0; i < count; i++)
		SpawnRiftBossMinion(typeIndex, monster.position.tile, /*rise=*/false);
}

void SplitOff(Monster &monster)
{
	for (int i = 0; i < 2; i++) {
		Monster *copy = SpawnRiftBossMinion(monster.levelType, monster.position.tile, /*rise=*/false);
		if (copy == nullptr)
			continue;
		copy->maxHitPoints = std::max(monster.maxHitPoints * 15 / 100, 64);
		copy->hitPoints = copy->maxHitPoints;
		RunState.conjured.push_back({ static_cast<int>(copy->getId()), ConjuredTicks });
	}
}

/** @brief Whether the boss has just fallen through @p percent of its life, once each (bit @p bit). */
bool CrossedOnce(const Monster &monster, int percent, int bit)
{
	if ((RunState.thresholds & (1 << bit)) != 0)
		return false;
	if (static_cast<int64_t>(monster.hitPoints) * 100 > static_cast<int64_t>(monster.maxHitPoints) * percent)
		return false;
	RunState.thresholds |= static_cast<uint8_t>(1 << bit);
	return true;
}

void BlinkNearHero(Monster &monster, int tiles)
{
	const Player *hero = MyPlayer;
	if (hero == nullptr)
		return;
	// Onto a tile the hero can see (round 101 audit: a random aim landed him past a wall, in a sealed pocket) - every direction
	// from a random one, then beside the hero (round 102 audit: in a narrow passage one direction rarely had such a tile, and
	// the blink did nothing).
	const int first = GenerateRnd(8);
	for (int i = 0; i < 8; i++) {
		const auto dir = static_cast<Direction>((first + i) % 8);
		if (MoveMonsterInstantly(monster, hero->position.tile + Displacement(dir) * tiles, hero->position.tile))
			return;
	}
	MoveMonsterInstantly(monster, hero->position.tile, hero->position.tile);
}

} // namespace

void TickRiftConjured()
{
	// Gathered first, then taken away: removing one edits the list being walked (audit, 2026-10-04).
	std::vector<int> expired;
	for (Conjured &entry : RunState.conjured) {
		if (--entry.ticksLeft <= 0)
			expired.push_back(entry.id);
	}
	for (const int id : expired) {
		Monster &copy = Monsters[id];
		// Gone, not killed: no corpse, no kill on the hero's count, no Life from Death or rune mana (audit, 2026-10-04).
		if (copy.hitPoints > 0 && copy.mode != MonsterMode::Death)
			DespawnMonster(copy);
		ForgetRiftConjured(copy);
	}
}

size_t RiftBossCount()
{
	return RosterSize;
}

const RiftBossDef &RiftBossAt(size_t index)
{
	return Roster[std::min(index, RosterSize - 1)];
}

bool RiftBossEligible(size_t index, RiftKind kind, int rung)
{
	if (index >= RosterSize)
		return false;
	const RiftBossDef &def = Roster[index];
	return def.kind == kind && rung >= RungForTier(def.minTier);
}

size_t PickRiftBoss(RiftKind kind, int rung, dungeon_type &tileset)
{
	size_t index;
	if (Forced && *Forced < RosterSize && Roster[*Forced].kind == kind) {
		index = *Forced;
	} else {
		std::vector<size_t> rows = EligibleRows(kind, rung);
		std::vector<size_t> fresh;
		for (const size_t row : rows) {
			if (std::find(LastMet.begin(), LastMet.end(), static_cast<int>(row)) == LastMet.end())
				fresh.push_back(row);
		}
		const std::vector<size_t> &from = fresh.empty() ? rows : fresh;
		index = from.empty() ? 0 : from[GenerateRnd(static_cast<int>(from.size()))];
	}
	const bool forced = Forced.has_value();
	Forced.reset();
	std::rotate(LastMet.rbegin(), LastMet.rbegin() + 1, LastMet.rend());
	LastMet[0] = static_cast<int>(index);
	// Q3: now and then a boss walks a floor that is not his. Never for a forced one - the debug command shows him at home.
	tileset = (!forced && GenerateRnd(100) < OffHomePercent) ? AnyTileset() : HomeTileset(Roster[index]);
	return index;
}

const RiftBossDef &ActiveRiftBoss()
{
	return RiftBossAt(RiftBossIndex());
}

const RiftBossDef *ActiveRiftBossFor(const Monster &monster)
{
	if (!IsRiftGuardian(monster))
		return nullptr;
	return &ActiveRiftBoss();
}

std::string RiftBossName()
{
	const RiftBossDef &def = ActiveRiftBoss();
	return def.name != nullptr ? std::string(def.name) : std::string(RiftGuardianName(def.face));
}

std::vector<_monster_id> RiftBossExtraTypes()
{
	const RiftBossDef &def = ActiveRiftBoss();
	std::vector<_monster_id> types;
	for (const _monster_id type : { def.escort, def.summon }) {
		if (type != MT_INVALID && type != def.type && std::find(types.begin(), types.end(), type) == types.end())
			types.push_back(type);
	}
	return types;
}

/** @brief The row whose LOOK @p monster wears: the risen guardian's, and still his while he dies (audit, 2026-10-04: the kill
 * marks the rift done before the death animation binds, and a 250% Prime Evil fell at 100%, untinted). */
const RiftBossDef *LookRowFor(const Monster &monster)
{
	if (const RiftBossDef *def = ActiveRiftBossFor(monster))
		return def;
	if (!IsFallenRiftGuardian(monster))
		return nullptr;
	const RiftBossDef &def = ActiveRiftBoss();
	return monster.type().type == def.type ? &def : nullptr;
}

unsigned RiftBossSizePercent(const Monster &monster)
{
	const RiftBossDef *def = LookRowFor(monster);
	if (def == nullptr)
		return 0;
	return std::min<unsigned>(def->sizePercent, MaxBossSizePercent);
}

int RiftBossSkippedFrames(const Monster &monster, MonsterGraphic graphic)
{
	const RiftBossDef *def = ActiveRiftBossFor(monster);
	if (def == nullptr)
		return 0;
	// Two frames, as the Frenzied and Fleet variants skip: a quarter of a typical walk, an eighth to a quarter of an attack.
	if (graphic == MonsterGraphic::Walk && def->walk < 0)
		return 2;
	if (graphic == MonsterGraphic::Attack && def->attack < 0)
		return 2;
	return 0;
}

bool RiftBossSkipsThisTick(const Monster &monster)
{
	const RiftBossDef *def = ActiveRiftBossFor(monster);
	if (def == nullptr)
		return false;
	if (RunState.restTicks > 0)
		return true; // stone: the whole turn is the rest's
	// Slow: every fourth tick of a walk is lost, as the chill loses every other one - the walk and its animation together.
	if (def->walk > 0 && monster.isWalking())
		return (++RunState.walkTick % 4) == 0;
	return false;
}

BossGlaze RiftBossGlaze(const Monster &monster)
{
	const RiftBossDef *def = LookRowFor(monster);
	if (def == nullptr)
		return { Tint::None, 0 };
	if (RunState.restTicks > 0)
		return { Tint::Hue, 0x8A8A84 }; // Grimwing's rest: stone grey
	return { def->tint, def->rgb };
}

int RiftBossDamagePercent(const Monster &monster)
{
	// The rift boss's run overflow, times the monster's own (Monster::damageOverflow - every monster's since 2026-10-05: champions,
	// Dread bosses, escorts and variants past 255 on Torment). The one percent every blow and readout lays on.
	const int boss = ActiveRiftBossFor(monster) != nullptr ? std::max(RunState.damageOverflow, 100) : 100;
	return static_cast<int>(static_cast<int64_t>(boss) * std::max<int>(monster.damageOverflow, 100) / 100);
}

bool RiftBossIsSteady(const Monster &monster)
{
	const RiftBossDef *def = ActiveRiftBossFor(monster);
	return def != nullptr && def->steady;
}

void OnRiftBossRisen(Monster &monster)
{
	const RiftBossDef *def = ActiveRiftBossFor(monster);
	if (def == nullptr)
		return;
	RunState = Run {};
	if (!def->original) {
		// The floor band for a roster boss of an ordinary type, and for every non-original row the rift's tier scaling too, which
		// SpawnRiftGuardian leaves to this (round 102 audit: applied there first, it was clamped away with the damage bytes).
		const int tier = RiftMonsterScalePercent();
		const int band = (def->face == RiftGuardianType::None ? BandPercent(*def) : 100) * tier / 100;
		const int64_t life = static_cast<int64_t>(monster.maxHitPoints) * band / 100 * def->lifePercent / 100;
		monster.maxHitPoints = static_cast<int>(std::clamp<int64_t>(life, 64, INT32_MAX / 2));
		monster.hitPoints = monster.maxHitPoints;
		// The damage bytes cap at 255, and on Hell a boss's scaled blow passed it - every Guardian boss struck a flat 255 and his
		// x2.0 or x2.4 meant nothing (round 95 audit). What does not fit is kept as a percent laid on at every blow
		// (RiftBossDamagePercent), and the bytes keep the spread, scaled down together.
		// From the REAL damage, his own overflow included; the run's overflow then carries it all, so his own goes back to 100.
		const auto wanted = [&](uint8_t value) { return MonsterEffectiveDamage(monster, value) * band / 100 * def->damagePercent / 100; };
		const int64_t top = std::max(wanted(monster.maxDamage), wanted(monster.maxDamageSpecial));
		const int overflow = top > 255 ? static_cast<int>((top * 100 + 254) / 255) : 100;
		const auto fit = [&](int value) { return static_cast<uint8_t>(std::clamp<int64_t>(wanted(value) * 100 / overflow, 0, 255)); };
		monster.minDamage = fit(monster.minDamage);
		monster.maxDamage = std::max(fit(monster.maxDamage), monster.minDamage);
		monster.minDamageSpecial = fit(monster.minDamageSpecial);
		monster.maxDamageSpecial = std::max(fit(monster.maxDamageSpecial), monster.minDamageSpecial);
		RunState.damageOverflow = overflow;
		monster.damageOverflow = 100; // carried by the run's now, not twice
		monster.armorClass = ScaledByte(monster.armorClass + (tier - 100) / 6, def->armourPercent, 100); // ScaleRiftMonster's armour rule
		monster.resistance |= def->resistAdd;
		// Never fire, lightning and magic at once, as SpawnRiftGuardian rules for the faces: a caster could not touch him.
		constexpr uint16_t AllThree = IMMUNE_MAGIC | IMMUNE_FIRE | IMMUNE_LIGHTNING;
		if ((monster.resistance & AllThree) == AllThree)
			monster.resistance = DemoteImmunitiesToResistances(monster.resistance) | (monster.resistance & IMMUNE_ACID);
		if (def->signature == BossSignature::Knockback)
			monster.flags |= MFLAG_KNOCKBACK;
		// Gorefeast's puddle is a monster-owned sheet only the acid spitters load; an Overlord does not. Loaded once, freed with
		// the game's missiles.
		if (def->signature == BossSignature::AcidPoolOnHit && !HeadlessMode)
			GetMissileSpriteData(MissileGraphicID::AcidPuddle).LoadGFX();
		CallEscort(monster, *def, def->escortCount);
	}
	// Rebound at his size: InitMonster bound his stand before his slot was named the guardian.
	M_StartStand(monster, monster.direction);
}

void TickRiftBoss(Monster &monster)
{
	const RiftBossDef *def = ActiveRiftBossFor(monster);
	if (def == nullptr)
		return;
	// Stone holds everything (round 101 audit: a petrified Prime Evil struck rang fire every second, and thresholds fired from
	// the stone). A pending answer and an uncrossed threshold wait for him to wake.
	if (monster.mode == MonsterMode::Petrified)
		return;
	Run &run = RunState;
	if (run.struckCooldown > 0)
		run.struckCooldown--;
	if (run.fireTicks > 0)
		run.fireTicks--;
	if (run.restTicks > 0)
		run.restTicks--;
	Player *hero = HeroInReach(monster, EngageTiles);
	const bool engaged = hero != nullptr && monster.mode != MonsterMode::Petrified;

	switch (def->signature) {
	case BossSignature::RaiseDead:
		if (engaged && ++run.timer >= RaiseDeadTicks) {
			run.timer = 0;
			Raise(monster, def->summon, 2);
		}
		break;
	case BossSignature::GraveRaise:
		if (engaged && ++run.timer >= GraveRaiseTicks) {
			run.timer = 0;
			Raise(monster, def->summon, 1);
		}
		break;
	case BossSignature::HorkBrood:
		if (engaged && ++run.timer >= HorkBroodTicks) {
			run.timer = 0;
			Raise(monster, def->summon, 2);
		}
		break;
	case BossSignature::Ambush:
		if ((monster.flags & MFLAG_HIDDEN) != 0)
			run.ambushReady = true;
		break;
	case BossSignature::CasterBlink:
		if (run.struckPending) {
			run.struckPending = false;
			BlinkNearHero(monster, 5);
		}
		break;
	case BossSignature::NovaWhenStruck:
		if (run.struckPending) {
			run.struckPending = false;
			FireRing(monster, MissileID::Firebolt, 8);
		}
		break;
	case BossSignature::FireTrail:
		if (monster.isWalking() && ++run.trail >= FireTrailTicks) {
			run.trail = 0;
			// Inferno at his own feet, the Incinerator's flame: its _midam (8) sets how long it burns, its damage is his.
			AddMissile(monster.position.tile, monster.position.tile, Direction::South, MissileID::Inferno, TARGET_PLAYERS, static_cast<int>(monster.getId()), 8, 0);
		}
		break;
	case BossSignature::StormChain:
		if (CrossedOnce(monster, 50, 0))
			FireRing(monster, MissileID::LightningControl, 8);
		break;
	case BossSignature::StoneRest:
		if (run.restTicks <= 0 && run.rests < 2 && static_cast<int64_t>(monster.hitPoints) * 100 < static_cast<int64_t>(monster.maxHitPoints) * 30) {
			run.rests++;
			run.restTicks = StoneRestTicks;
			Heal(monster, monster.maxHitPoints / 10);
		}
		break;
	case BossSignature::BoneRing:
		if (engaged && ++run.timer >= BoneRingTicks) {
			run.timer = 0;
			FireRing(monster, MissileID::BlueFlare2, 8);
		}
		break;
	case BossSignature::BlinkBoltRing:
		if (engaged && ++run.timer >= BlinkBoltTicks) {
			run.timer = 0;
			BlinkNearHero(monster, 5);
			FireRing(monster, MissileID::BloodStar, 8);
		}
		break;
	case BossSignature::FrostNovaRaise:
		if (engaged && ++run.timer >= FrostNovaTicks) {
			run.timer = 0;
			FireRing(monster, MissileID::YellowFlare, 8);
			if (Player *near = HeroInReach(monster, 6))
				ChillPlayer(*near, PlayerChillBaseTicks);
		}
		if (CrossedOnce(monster, 50, 0))
			Raise(monster, def->summon, 4);
		break;
	case BossSignature::CallGuard:
		if (CrossedOnce(monster, 66, 0))
			CallEscort(monster, *def, 4);
		if (CrossedOnce(monster, 33, 1))
			CallEscort(monster, *def, 4);
		break;
	case BossSignature::GroundSlam:
		if (engaged && ++run.timer >= GroundSlamTicks) {
			run.timer = 0;
			if (Player *near = HeroInReach(monster, 2)) {
				const int dam = (RollDamage(monster) * 3 / 2) << 6;
				ApplyPlrDamage(DamageType::Physical, *near, 0, 0, dam);
				if (near->_pHitPoints > 0)
					StartPlrHit(*near, dam, true);
			}
		}
		break;
	case BossSignature::Regenerate:
		if (++run.regen >= RegenStepTicks) {
			run.regen = 0;
			if (run.fireTicks <= 0)
				Heal(monster, monster.maxHitPoints / 100);
		}
		break;
	case BossSignature::Burrow:
		if (engaged && ++run.timer >= BurrowTicks) {
			run.timer = 0;
			BlinkNearHero(monster, 1);
		}
		break;
	case BossSignature::Split:
		if (CrossedOnce(monster, 75, 0))
			SplitOff(monster);
		if (CrossedOnce(monster, 40, 1))
			SplitOff(monster);
		break;
	default:
		break;
	}
}

void OnRiftBossDealtBlow(Monster &monster, Player &player, int landed, bool missile)
{
	const RiftBossDef *def = ActiveRiftBossFor(monster);
	if (def == nullptr || landed <= 0)
		return;
	const bool heroStands = player._pHitPoints > 0;
	if (missile && IsAnyOf(def->signature, BossSignature::AcidPoolOnHit, BossSignature::BurningPools, BossSignature::LightningOnHit, BossSignature::Ambush, BossSignature::Cleave))
		return;
	switch (def->signature) {
	case BossSignature::AcidPoolOnHit:
		AddMissile(player.position.tile, { 0, 0 }, Direction::South, MissileID::AcidPuddle, TARGET_PLAYERS, static_cast<int>(monster.getId()), monster.intelligence + 1, 0);
		break;
	case BossSignature::ChillOnHit:
		ChillPlayer(player, 2 * PlayerChillBaseTicks);
		break;
	case BossSignature::AcidFanVenom:
	case BossSignature::VenomOnHit:
	case BossSignature::Bleed:
		// As much again as the blow, over five seconds - the Venomous variant's bite (oracool/venom).
		if (heroStands)
			PoisonPlayer(player, landed, 100);
		break;
	case BossSignature::Ambush:
		if (RunState.ambushReady && heroStands) {
			RunState.ambushReady = false;
			ApplyPlrDamage(DamageType::Physical, player, 0, 0, 2 * landed); // the blow and twice again: triple
		}
		break;
	case BossSignature::Cleave:
		if (++RunState.swings % 4 == 0 && heroStands) {
			ApplyPlrDamage(DamageType::Physical, player, 0, 0, landed); // double
			if (player._pHitPoints > 0)
				StartPlrHit(player, landed, true);
		}
		break;
	case BossSignature::LifeOnHit:
		Heal(monster, monster.maxHitPoints * 2 / 100);
		break;
	case BossSignature::LifeSteal:
		Heal(monster, landed * 5 / 100);
		break;
	case BossSignature::BurningPools:
		AddMissile(player.position.tile, player.position.tile, Direction::South, MissileID::Inferno, TARGET_PLAYERS, static_cast<int>(monster.getId()), 8, 0);
		break;
	case BossSignature::LightningOnHit:
		AddMissile(player.position.tile, { player.position.tile.x + 1, 0 }, Direction::South, MissileID::Lightning, TARGET_PLAYERS, static_cast<int>(monster.getId()), RollDamage(monster), 0);
		break;
	case BossSignature::ManaBurn: {
		const int burned = std::min(landed, std::max(player._pMana, 0));
		player._pMana -= burned;
		player._pManaBase -= burned;
		RedrawComponent(PanelDrawComponent::Mana);
	} break;
	default:
		break;
	}
}

void OnRiftBossStruck(Monster &monster, DamageType type, int damage)
{
	const RiftBossDef *def = ActiveRiftBossFor(monster);
	if (def == nullptr || damage <= 0)
		return;
	switch (def->signature) {
	case BossSignature::CasterBlink:
	case BossSignature::NovaWhenStruck:
		// Acted on at his next tick (TickRiftBoss), not inside the blow: a blink mid-hit would move him under M_StartHit.
		if (RunState.struckCooldown <= 0) {
			RunState.struckCooldown = def->signature == BossSignature::CasterBlink ? BlinkStruckCooldownTicks : StruckCooldownTicks;
			RunState.struckPending = true;
		}
		break;
	case BossSignature::Regenerate:
		if (type == DamageType::Fire)
			RunState.fireTicks = FireStopsRegenTicks;
		break;
	default:
		break;
	}
}

MissileID RiftBossShotType(const Monster &monster, MissileID missile)
{
	const RiftBossDef *def = ActiveRiftBossFor(monster);
	if (def == nullptr || def->signature != BossSignature::CasterBlink)
		return missile;
	if (!IsAnyOf(missile, MissileID::Firebolt, MissileID::ChargedBolt, MissileID::LightningControl, MissileID::Fireball))
		return missile;
	// Fire, lightning and fire again, in turn; his Flash stays his own close-up answer.
	constexpr MissileID Turn[] = { MissileID::Fireball, MissileID::LightningControl, MissileID::Firebolt };
	return Turn[RunState.castTurn++ % 3];
}

void RiftBossExtraShots(Monster &monster, MissileID missile, int damage)
{
	const RiftBossDef *def = ActiveRiftBossFor(monster);
	if (def == nullptr)
		return;
	if (def->signature == BossSignature::AcidFanVenom && missile == MissileID::Acid)
		FireSideShots(monster, missile, damage);
	else if (def->signature == BossSignature::FireballFan && missile == MissileID::MagmaBall)
		FireSideShots(monster, missile, damage);
	else if (def->signature == BossSignature::StormChain && IsAnyOf(missile, MissileID::ThinLightningControl, MissileID::LightningControl))
		AddMissile(monster.position.tile, monster.enemyPosition, monster.direction, missile, TARGET_PLAYERS, static_cast<int>(monster.getId()), damage, 0);
}

bool IsRiftConjured(const Monster &monster)
{
	// Only inside the rift (audit, 2026-10-04): a copy alive when the hero left would leave its slot id behind, and the next
	// monster in that slot on a dungeon floor would drop nothing.
	if (!InRift())
		return false;
	const int id = static_cast<int>(monster.getId());
	return std::any_of(RunState.conjured.begin(), RunState.conjured.end(), [id](const Conjured &entry) { return entry.id == id; });
}

void ForgetRiftConjured(const Monster &monster)
{
	// The rift's own slots only (round 95 audit): a dungeon monster dying in a copy's slot number, the copy still alive in the
	// rift left behind, struck the copy off - it never went, and paid in full when killed.
	if (!InRift())
		return;
	const int id = static_cast<int>(monster.getId());
	RunState.conjured.erase(std::remove_if(RunState.conjured.begin(), RunState.conjured.end(), [id](const Conjured &entry) { return entry.id == id; }),
	    RunState.conjured.end());
	RunState.raised.erase(std::remove(RunState.raised.begin(), RunState.raised.end(), id), RunState.raised.end());
}

void ResetRiftBossRun()
{
	RunState = Run {};
}

void ResetRiftBossesForNewGame()
{
	RunState = Run {};
	LastMet = { -1, -1, -1 };
	Forced.reset();
}

void ForceNextRiftBoss(size_t index)
{
	if (index < RosterSize)
		Forced = index;
	else
		Forced.reset();
}

void ClearForcedRiftBoss()
{
	Forced.reset();
}

std::optional<size_t> FindRiftBoss(std::string_view query)
{
	while (!query.empty() && query.front() == ' ')
		query.remove_prefix(1);
	if (query.empty())
		return std::nullopt;
	if (std::all_of(query.begin(), query.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; })) {
		const int number = std::atoi(std::string(query).c_str());
		if (number >= 1 && static_cast<size_t>(number) <= RosterSize)
			return static_cast<size_t>(number - 1);
		return std::nullopt;
	}
	const std::string needle = Lower(query);
	for (size_t i = 0; i < RosterSize; i++) {
		const RiftBossDef &def = Roster[i];
		const std::string name = Lower(def.name != nullptr ? def.name : RiftGuardianName(def.face));
		if (name.find(needle) != std::string::npos)
			return i;
	}
	for (size_t i = 0; i < RosterSize; i++) {
		if (Lower(MonstersData[Roster[i].type].name).find(needle) != std::string::npos)
			return i;
	}
	return std::nullopt;
}

size_t RiftBossForTest(RiftKind kind, int rung, int roll)
{
	const std::vector<size_t> rows = EligibleRows(kind, rung);
	return rows.empty() ? 0 : rows[static_cast<size_t>(std::abs(roll)) % rows.size()];
}

} // namespace devilution::oracool
