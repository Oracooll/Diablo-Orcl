/**
 * @file monster.cpp
 *
 * Implementation of monster functionality, AI, actions, spawning, loading, etc.
 */
#include "monster.h"
#include "utils/log.hpp"

#include <climits>
#include <cstdint>

#include <algorithm>
#include <array>

#include <fmt/core.h>
#include <fmt/format.h>

#include "cursor.h"
#include "dead.h"
#include "engine/load_cl2.hpp"
#include "engine/load_file.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/random.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/backbuffer_state.hpp"
#include "engine/sound_position.hpp"
#include "engine/world_tile.hpp"
#include "init.h"
#include "levels/crypt.h"
#include "levels/drlg_l4.h"
#include "levels/themes.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "minitext.h"
#include "missiles.h"
#include "movie.h"
#include "options.h"
#include "oracool/auto_save.h" // SaveOnExit - the ending saves the kill
#include "oracool/chill.h"
#include "oracool/combat_odds.h"
#include "oracool/cold.h"
#include "oracool/event_log.h"
#include "oracool/gems.h"
#include "oracool/aura_field.h"
#include "oracool/warcries.h"
#include "oracool/monster_difficulty.h"
#include "oracool/companion.h"
#include "oracool/corpses.h"
#include "oracool/curses.h"
#include "oracool/minions.h"
#include "oracool/necro_summoning.h" // ClearNecromancerSummoningState on level load
#include "oracool/passives.h"
#include "oracool/rfa12_effects.h"
#include "oracool/warcries.h"
#include "oracool/monster_variants.h"
#include "oracool/venom.h"
#include "oracool/endgame_boss.h"
#include "oracool/monster_scale.h"
#include "oracool/named_encounters.h"
#include "oracool/rift.h" // the rift floor's roster, its guardian, its kill bar (2026-09-20)
#include "oracool/signets.h"
#include "oracool/telemetry.h"

#include "qol/floatingnumbers.h"
#include "spelldat.h"
#include "storm/storm_net.hpp"
#include "towners.h"
#include "utils/cl2_to_clx.hpp"
#include "utils/file_name_generator.hpp"
#include "utils/language.h"
#include "utils/stdcompat/string_view.hpp"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

#ifdef _DEBUG
#include "debug.h"
#endif

namespace devilution {

CMonster LevelMonsterTypes[MaxLvlMTypes];
size_t LevelMonsterTypeCount;
Monster Monsters[MaxMonsters];
DVL_API_FOR_TEST int ActiveMonsters[MaxMonsters];
size_t ActiveMonsterCount;
/** Tracks the total number of monsters killed per monster_id. */
int MonsterKillCounts[NUM_MTYPES];
bool sgbSaveSoundOn;

namespace {

/** Which slots stand in a skill's stun (StunMonster) rather than an AI's own pause, the same MonsterMode::Delay. */
std::array<bool, MaxMonsters> StunnedBySkill {};
/** @brief The companion regroup watchdog, per slot; cleared with the level (round 36 audit: statics in CompanionAi outlived it). */
std::array<uint8_t, MaxMonsters> RegroupBestDistance {};
std::array<uint8_t, MaxMonsters> RegroupTriesWithoutGain {};

/** @brief @p monster's stun flag, or nullptr for a monster outside the pool (a test's local), as the chill table guards. */
bool *StunFlagOf(const Monster &monster)
{
	const size_t id = monster.getId();
	return id < StunnedBySkill.size() ? &StunnedBySkill[id] : nullptr;
}

constexpr int NightmareToHitBonus = 85;
constexpr int HellToHitBonus = 120;

constexpr int NightmareAcBonus = 50;
constexpr int HellAcBonus = 80;

/** Tracks which missile files are already loaded */
size_t totalmonsters;
int monstimgtot;
int uniquetrans;

constexpr const std::array<_monster_id, 12> SkeletonTypes {
	MT_WSKELAX,
	MT_TSKELAX,
	MT_RSKELAX,
	MT_XSKELAX,
	MT_WSKELBW,
	MT_TSKELBW,
	MT_RSKELBW,
	MT_XSKELBW,
	MT_WSKELSD,
	MT_TSKELSD,
	MT_RSKELSD,
	MT_XSKELSD,
};

/** Maps from monster action to monster animation letter. */
constexpr char Animletter[7] = "nwahds";

size_t GetNumAnims(const MonsterData &monsterData)
{
	return monsterData.hasSpecial ? 6 : 5;
}

void InitMonsterTRN(CMonster &monst)
{
	char path[64];
	*BufCopy(path, "monsters\\", monst.data->trnFile, ".trn") = '\0';
	std::array<uint8_t, 256> colorTranslations;
	LoadFileInMem(path, colorTranslations);
	std::replace(colorTranslations.begin(), colorTranslations.end(), 255, 0);

	const size_t numAnims = GetNumAnims(*monst.data);
	for (size_t i = 0; i < numAnims; i++) {
		if (i == 1 && IsAnyOf(monst.type, MT_COUNSLR, MT_MAGISTR, MT_CABALIST, MT_ADVOCATE)) {
			continue;
		}

		AnimStruct &anim = monst.anims[i];
		if (anim.sprites->isSheet()) {
			ClxApplyTrans(ClxSpriteSheet { anim.sprites->sheet() }, colorTranslations.data());
		} else {
			ClxApplyTrans(ClxSpriteList { anim.sprites->list() }, colorTranslations.data());
		}
	}
}

// `ordinary` is false for a monster about to become a unique or a champion (audit, 2026-09-19): the
// variant used to be applied to it here and then PrepareUniqueMonst wrote its identity over the
// top - an orphaned Luminous light at the spawn tile, a champion scaled from Ironhide armour.
void InitMonster(Monster &monster, Direction rd, size_t typeIndex, Point position, bool ordinary = true)
{
	static uint32_t NextSpawnSerial = 0;
	monster.spawnSerial = ++NextSpawnSerial; // Monster::spawnSerial - a new spawn in this slot
	if (bool *stunned = StunFlagOf(monster); stunned != nullptr)
		*stunned = false; // a new spawn is in no stun (round 20 audit)
	// Oracool: a slot being (re)used starts with no cry on it - see ClearWarcryStateForMonster.
	oracool::ClearWarcryStateForMonster(monster);
	oracool::ClearRfa12StateForMonster(monster);
	oracool::ClearColdStateForMonster(monster);
	oracool::ClearPassiveMarksForMonster(monster);
	monster.direction = rd;
	monster.position.tile = position;
	monster.position.future = position;
	monster.position.old = position;
	monster.levelType = typeIndex;
	monster.mode = MonsterMode::Stand;
	monster.animInfo = {};
	monster.changeAnimationData(MonsterGraphic::Stand);
	monster.animInfo.tickCounterOfCurrentFrame = GenerateRnd(monster.animInfo.ticksPerFrame - 1);
	monster.animInfo.currentFrame = GenerateRnd(monster.animInfo.numberOfFrames - 1);

	int maxhp = monster.data().hitPointsMinimum + GenerateRnd(monster.data().hitPointsMaximum - monster.data().hitPointsMinimum + 1);
	if (monster.type().type == MT_DIABLO && !gbIsHellfire) {
		maxhp /= 2;
	}
	monster.maxHitPoints = maxhp << 6;

	if (!gbIsMultiplayer)
		monster.maxHitPoints = std::max(monster.maxHitPoints / 2, 64);

	monster.hitPoints = monster.maxHitPoints;
	monster.ai = monster.data().ai;
	monster.intelligence = monster.data().intelligence;
	monster.goal = MonsterGoal::Normal;
	monster.goalVar1 = 0;
	monster.goalVar2 = 0;
	monster.goalVar3 = 0;
	monster.pathCount = 0;
	monster.isInvalid = false;
	monster.uniqueType = UniqueMonsterType::None;
	// Oracool: cleared beside uniqueType and for the same reason - Monster slots are reused between
	// levels, so an ordinary monster taking a champion's old slot would inherit its modifier.
	monster.lesserAffix = LesserUniqueAffix::None;
	monster.lesserNameSeed = 0;
	// And the palette translation, for the SAME reason - which only became a reason when recoloured
	// variants started giving ordinary monsters one (Phase 3). Before that only uniques carried a
	// TRN and only uniques were re-tinted, so a stale one could never be seen; now an ordinary
	// monster taking a variant's old slot would wear its colour. Cleared here and rebuilt at the end
	// of this function if this monster is itself a variant; a unique's own TRN is loaded later still,
	// by InitTRNForUniqueMonster, so this cannot strand one.
	monster.uniqueMonsterTRN = nullptr;
	monster.activeForTicks = 0;
	monster.lightId = NO_LIGHT;
	monster.rndItemSeed = AdvanceRndSeed();
	monster.aiSeed = AdvanceRndSeed();
	monster.whoHit = 0;
	monster.minDamage = monster.data().minDamage;
	monster.maxDamage = monster.data().maxDamage;
	monster.minDamageSpecial = monster.data().minDamageSpecial;
	monster.maxDamageSpecial = monster.data().maxDamageSpecial;
	monster.armorClass = monster.data().armorClass;
	// Phase 3.3: the whole difficulty ladder in one call, including Nightmare's new middle rung.
	// The Hell and Torment blocks below used to re-assign this; they no longer need to.
	monster.resistance = oracool::MonsterResistancesFor(monster.data(), sgGameInitInfo.nDifficulty);
	monster.leader = Monster::NoLeader;
	monster.leaderRelation = LeaderRelation::None;
	monster.flags = monster.data().abilityFlags;
	monster.talkMsg = TEXT_NONE;

	if (monster.ai == MonsterAIID::Gargoyle) {
		monster.changeAnimationData(MonsterGraphic::Special);
		monster.animInfo.currentFrame = 0;
		monster.flags |= MFLAG_ALLOW_SPECIAL;
		monster.mode = MonsterMode::SpecialMeleeAttack;
	}

	if (sgGameInitInfo.nDifficulty == DIFF_NIGHTMARE) {
		monster.maxHitPoints = 3 * monster.maxHitPoints;
		if (gbIsHellfire)
			monster.maxHitPoints += (gbIsMultiplayer ? 100 : 50) << 6;
		else
			monster.maxHitPoints += 100 << 6;
		monster.hitPoints = monster.maxHitPoints;
		monster.minDamage = 2 * (monster.minDamage + 2);
		monster.maxDamage = 2 * (monster.maxDamage + 2);
		monster.minDamageSpecial = 2 * (monster.minDamageSpecial + 2);
		monster.maxDamageSpecial = 2 * (monster.maxDamageSpecial + 2);
		monster.armorClass += NightmareAcBonus;
	} else if (sgGameInitInfo.nDifficulty == DIFF_HELL) {
		monster.maxHitPoints = 4 * monster.maxHitPoints;
		if (gbIsHellfire)
			monster.maxHitPoints += (gbIsMultiplayer ? 200 : 100) << 6;
		else
			monster.maxHitPoints += 200 << 6;
		monster.hitPoints = monster.maxHitPoints;
		monster.minDamage = 4 * monster.minDamage + 6;
		monster.maxDamage = 4 * monster.maxDamage + 6;
		monster.minDamageSpecial = 4 * monster.minDamageSpecial + 6;
		monster.maxDamageSpecial = 4 * monster.maxDamageSpecial + 6;
		monster.armorClass += HellAcBonus;
	} else if (sgGameInitInfo.nDifficulty == DIFF_TORMENT) {
		const float multiplier = GetTormentDifficultyMultiplier();
		monster.maxHitPoints = 4 * monster.maxHitPoints;
		if (gbIsHellfire)
			monster.maxHitPoints += (gbIsMultiplayer ? 200 : 100) << 6;
		else
			monster.maxHitPoints += 200 << 6;
		monster.maxHitPoints = static_cast<int>(monster.maxHitPoints * multiplier);
		monster.hitPoints = monster.maxHitPoints;
		// minDamage/maxDamage/armorClass are uint8_t - Hell's formula already fits comfortably,
		// but scaling further by up to 5.0x can genuinely exceed 255, so every result here is
		// clamped instead of silently wrapping (which would make a high multiplier setting
		// unpredictably *weaker* for some monsters instead of stronger).
		monster.minDamage = static_cast<uint8_t>(std::min(static_cast<int>((4 * monster.minDamage + 6) * multiplier), 255));
		monster.maxDamage = static_cast<uint8_t>(std::min(static_cast<int>((4 * monster.maxDamage + 6) * multiplier), 255));
		monster.minDamageSpecial = static_cast<uint8_t>(std::min(static_cast<int>((4 * monster.minDamageSpecial + 6) * multiplier), 255));
		monster.maxDamageSpecial = static_cast<uint8_t>(std::min(static_cast<int>((4 * monster.maxDamageSpecial + 6) * multiplier), 255));
		monster.armorClass = static_cast<uint8_t>(std::min(monster.armorClass + static_cast<int>(HellAcBonus * multiplier), 255));
	}

	// Phase 3: the recoloured variants, LAST - after the difficulty ladder, so a Feral monster is
	// 30% harder than whatever this difficulty already made it rather than 30% harder than the
	// Normal baseline. Derived from rndItemSeed, so it costs no per-monster state and reproduces on
	// a revisit; see oracool/monster_variants.h. Not for a unique or champion in the making.
	if (ordinary)
		oracool::ApplyMonsterVariant(monster);
}

bool CanPlaceMonster(Point position)
{
	return InDungeonBounds(position)
	    && dMonster[position.x][position.y] == 0
	    && dPlayer[position.x][position.y] == 0
	    && !IsTileVisible(position)
	    && !TileContainsSetPiece(position)
	    && !IsTileOccupied(position);
}

void PlaceMonster(int i, size_t typeIndex, Point position, bool ordinary = true)
{
	if (LevelMonsterTypes[typeIndex].type == MT_NAKRUL) {
		for (size_t j = 0; j < ActiveMonsterCount; j++) {
			if (Monsters[j].levelType == typeIndex) {
				return;
			}
		}
	}
	dMonster[position.x][position.y] = i + 1;

	auto rd = static_cast<Direction>(GenerateRnd(8));
	InitMonster(Monsters[i], rd, typeIndex, position, ordinary);
}

void PlaceGroup(size_t typeIndex, unsigned num, Monster *leader = nullptr, bool leashed = false)
{
	unsigned placed = 0;

	for (int try1 = 0; try1 < 10; try1++) {
		while (placed != 0) {
			ActiveMonsterCount--;
			placed--;
			Monster &undone = Monsters[ActiveMonsters[ActiveMonsterCount]]; // the slot, not the count (round 49 audit)
			const auto &position = undone.position.tile;
			dMonster[position.x][position.y] = 0;
			// Its light too (audit, 2026-09-27): a Luminous monster taken back left its glow on the floor for good.
			if (undone.lightId != NO_LIGHT) {
				AddUnLight(undone.lightId);
				undone.lightId = NO_LIGHT;
			}
		}

		int xp;
		int yp;
		if (leader != nullptr) {
			int offset = GenerateRnd(8);
			auto position = leader->position.tile + static_cast<Direction>(offset);
			xp = position.x;
			yp = position.y;
		} else {
			// BOUNDED (audit, 2026-09-02). This was `do { ... } while (!CanPlaceMonster(...))` - an
			// unbounded hunt for a free tile, which spins forever on a level that has none.
			//
			// Vanilla could rely on never getting there because it placed a vanilla number of
			// monsters. Monster Density defaults to 300% and champion packs land five bodies at a
			// time on top of that, so "the level is full" is a state this fork actually approaches -
			// and the same shape cost two sessions this week already (CreateSpellBook, and the
			// corpse table). A crowded level should drop a group, not hang.
			bool found = false;
			for (int spot = 0; spot < 500; spot++) {
				xp = GenerateRnd(80) + 16;
				yp = GenerateRnd(80) + 16;
				if (CanPlaceMonster({ xp, yp })) {
					found = true;
					break;
				}
			}
			if (!found)
				return; // nowhere left on this floor - the caller's own bound stops it asking again
		}
		int x1 = xp;
		int y1 = yp;

		// Never past the floor's total, and never a wrapped count (round 49 audit): a rift guardian's pack is placed mid-game,
		// when the army, theme rooms and raised skeletons can already stand past totalmonsters - the unsigned difference
		// wrapped to four billion and the placement overwrote live monsters.
		if (ActiveMonsterCount >= totalmonsters)
			break;
		num = std::min<unsigned>(num, static_cast<unsigned>(totalmonsters - ActiveMonsterCount));

		unsigned j = 0;
		for (unsigned try2 = 0; j < num && try2 < 100; xp += Displacement(static_cast<Direction>(GenerateRnd(8))).deltaX, yp += Displacement(static_cast<Direction>(GenerateRnd(8))).deltaX) { /// BUGFIX: `yp += Point.y`
			if (!CanPlaceMonster({ xp, yp })
			    || (dTransVal[xp][yp] != dTransVal[x1][y1])
			    || (leashed && (abs(xp - x1) >= 4 || abs(yp - y1) >= 4))) {
				try2++;
				continue;
			}

			// Through ActiveMonsters (round 49 audit): at level build the slot IS the count, mid-game DeleteMonster has
			// shuffled them and the count's own index can be a live monster.
			const int slot = ActiveMonsters[ActiveMonsterCount];
			PlaceMonster(slot, typeIndex, { xp, yp });
			if (leader != nullptr) {
				auto &minion = Monsters[slot];
				minion.maxHitPoints *= 2;
				minion.hitPoints = minion.maxHitPoints;
				minion.intelligence = leader->intelligence;

				if (leashed) {
					minion.setLeader(leader);
				}

				if (minion.ai != MonsterAIID::Gargoyle) {
					minion.changeAnimationData(MonsterGraphic::Stand);
					minion.animInfo.currentFrame = GenerateRnd(minion.animInfo.numberOfFrames - 1);
					minion.flags &= ~MFLAG_ALLOW_SPECIAL;
					minion.mode = MonsterMode::Stand;
				}
			}
			ActiveMonsterCount++;
			placed++;
			j++;
		}

		if (placed >= num) {
			break;
		}
	}

	if (leashed) {
		leader->packSize = placed;
	}
}

size_t GetMonsterTypeIndex(_monster_id type)
{
	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (LevelMonsterTypes[i].type == type)
			return i;
	}
	return LevelMonsterTypeCount;
}

std::optional<Point> GetUniqueMonstPosition(UniqueMonsterType uniqindex)
{
	if (setlevel) {
		switch (uniqindex) {
		case UniqueMonsterType::Lazarus:
			return Point { 32, 46 };
		case UniqueMonsterType::RedVex:
			return Point { 40, 45 };
		case UniqueMonsterType::BlackJade:
			return Point { 38, 49 };
		case UniqueMonsterType::SkeletonKing:
			return Point { 35, 47 };
		default:
			break;
		}
	}

	switch (uniqindex) {
	case UniqueMonsterType::SnotSpill:
		return SetPiece.position.megaToWorld() + Displacement { 8, 12 };
	case UniqueMonsterType::WarlordOfBlood:
		return SetPiece.position.megaToWorld() + Displacement { 6, 7 };
	case UniqueMonsterType::Zhar:
		for (int i = 0; i < themeCount; i++) {
			if (i == zharlib) {
				return themeLoc[i].room.position.megaToWorld() + Displacement { 4, 4 };
			}
		}
		break;
	case UniqueMonsterType::Lazarus:
		return SetPiece.position.megaToWorld() + Displacement { 3, 6 };
	case UniqueMonsterType::RedVex:
		return SetPiece.position.megaToWorld() + Displacement { 5, 3 };
	case UniqueMonsterType::BlackJade:
		return SetPiece.position.megaToWorld() + Displacement { 5, 9 };
	case UniqueMonsterType::Butcher:
		return SetPiece.position.megaToWorld() + Displacement { 4, 4 };
	case UniqueMonsterType::NaKrul:
		if (UberRow == 0 || UberCol == 0) {
			UberDiabloMonsterIndex = -1;
			break;
		}
		UberDiabloMonsterIndex = static_cast<int>(ActiveMonsterCount);
		return Point { UberRow - 2, UberCol };
	default:
		break;
	}

	Point position;
	int count = 0;
	int tries = 0;
	do {
		// BOUNDED (round 49 audit), as PlaceGroup's hunt since 2026-09-02: every champion and boss asks here, and a floor
		// with no free tile spun forever. The caller drops the placement.
		if (++tries > 5000)
			return std::nullopt;
		position = Point { GenerateRnd(80), GenerateRnd(80) } + Displacement { 16, 16 };
		int count2 = 0;
		for (int x = position.x - 3; x < position.x + 3; x++) {
			for (int y = position.y - 3; y < position.y + 3; y++) {
				if (InDungeonBounds({ x, y }) && CanPlaceMonster({ x, y })) {
					count2++;
				}
			}
		}

		if (count2 < 9) {
			count++;
			if (count < 1000) {
				continue;
			}
		}
	} while (!CanPlaceMonster(position));

	return position;
}

void PlaceUniqueMonst(UniqueMonsterType uniqindex, size_t minionType, int bosspacksize)
{
	const auto &uniqueMonsterData = UniqueMonstersData[static_cast<size_t>(uniqindex)];
	const size_t typeIndex = GetMonsterTypeIndex(uniqueMonsterData.mtype);
	const std::optional<Point> spot = GetUniqueMonstPosition(uniqindex);
	if (!spot) {
		LogWarn("PlaceUniqueMonst: no free tile for unique {:d} - not placed", static_cast<int>(uniqindex));
		return; // a full floor drops him rather than hang (round 49 audit)
	}
	const Point position = *spot;
	PlaceMonster(ActiveMonsterCount, typeIndex, position, /*ordinary=*/false);

	Monster &monster = Monsters[ActiveMonsterCount];
	ActiveMonsterCount++;
	PrepareUniqueMonst(monster, uniqindex, minionType, bosspacksize, uniqueMonsterData);
}

size_t AddMonsterType(_monster_id type, placeflag placeflag)
{
	const size_t typeIndex = GetMonsterTypeIndex(type);
	CMonster &monsterType = LevelMonsterTypes[typeIndex];

	if (typeIndex == LevelMonsterTypeCount) {
		LevelMonsterTypeCount++;
		monsterType.type = type;
		monsterType.minionOnly = false; // AddMinionBody marks the one it adds
		// No body until InitCorpses gives it one. A type added mid-level (a raised minion's body) kept the corpse id
		// this slot held on the previous floor, and left another monster's corpse (round 3 audit, v1.12.228).
		monsterType.corpseId = 0;
		monstimgtot += MonstersData[type].image;
		InitMonsterGFX(monsterType);
		InitMonsterSND(monsterType);
	}

	monsterType.placeFlags |= placeflag;
	return typeIndex;
}

inline size_t AddMonsterType(UniqueMonsterType uniqueType, placeflag placeflag)
{
	return AddMonsterType(UniqueMonstersData[static_cast<size_t>(uniqueType)].mtype, placeflag);
}

void ClearMVars(Monster &monster)
{
	monster.var1 = 0;
	monster.var2 = 0;
	monster.var3 = 0;
	monster.position.temp = { 0, 0 };
}

void ClrAllMonsters()
{
	for (auto &monster : Monsters) {
		ClearMVars(monster);
		monster.goal = MonsterGoal::None;
		monster.mode = MonsterMode::Stand;
		monster.var1 = 0;
		monster.var2 = 0;
		monster.position.tile = { 0, 0 };
		monster.position.future = { 0, 0 };
		monster.position.old = { 0, 0 };
		monster.direction = static_cast<Direction>(GenerateRnd(8));
		monster.animInfo = {};
		monster.flags = 0;
		monster.isInvalid = false;
		monster.enemy = GenerateRnd(gbActivePlayers);
		monster.enemyPosition = Players[monster.enemy].position.future;
	}
}

/**
 * @brief Oracool: places the lesser uniques this level hosts, if any.
 *
 * Deliberately built on PlaceUniqueMonst rather than beside it: everything that makes a champion a
 * champion - the authored name and palette, the AI, the resistances, the minion escort through
 * PlaceGroup - is already in PrepareUniqueMonst, and duplicating it would mean two definitions of
 * "unique" drifting apart. The lesser part is the stats, and those are step 2 of the plan.
 *
 * The pack size is smaller than the scripted 8. A lesser unique is an encounter, not a set piece,
 * and its escort comes out of the same MaxMonsters pool the scattered monsters and the density dial
 * both draw on.
 */
/**
 * @brief Oracool: places one lesser unique, scaled to THIS floor rather than to its own.
 *
 * The trick is the capture. PlaceMonster runs InitMonster, which sets the ordinary stats a monster of
 * this type has on this level - already difficulty-scaled, because InitMonster does that itself.
 * PrepareUniqueMonst then overwrites them with the champion's authored numbers. So we capture the
 * ordinary values in between, and put back a multiple of them afterwards.
 *
 * That is what "stats appropriate for uniques compare to local mobs on current dungeon level" (user,
 * 2026-08-15) means, and it is why the authored numbers are wrong for this: a level-16 champion's
 * 2,000 hit points standing on level 3 is the exact problem being solved. The table supplies
 * IDENTITY - name, palette, AI, resistances; the floor supplies POWER.
 *
 * Capturing rather than recomputing also means the difficulty ladder is inherited for free, instead
 * of this needing its own copy of the Nightmare/Hell/Torment arithmetic to drift out of step.
 */
/**
 * @brief Places one champion. @p boss builds it at the endgame-boss profile instead.
 *
 * One function rather than two, because a boss IS a champion with heavier numbers - every step
 * below is the same step, and a second copy of it would be a second place for the escort scaling,
 * the tint and the name seed to drift.
 */
void PlaceLesserUniqueMonst(UniqueMonsterType uniqindex, size_t minionType, int packSize, bool boss = false)
{
	// A champion is worth several ordinary monsters, but is still something a player at this depth is
	// meant to beat. Three times the health is the felt difference; damage rises more gently, because
	// a monster that hits three times as hard on level 2 does not read as a champion, it reads as a
	// mistake.
	constexpr int LesserUniqueHealthPercent = 300;
	constexpr int LesserUniqueDamagePercent = 150;
	constexpr int LesserUniqueArmorBonus = 4;
	// 75, not 150: PlaceGroup has already doubled a leader's escort (vanilla), so this nets the 150% meant; the two stacked to 300%,
	// the champion's own life (round 36 audit).
	constexpr int MinionHealthPercent = 75;
	constexpr int MinionDamagePercent = 120;

	const auto &uniqueMonsterData = UniqueMonstersData[static_cast<size_t>(uniqindex)];
	const size_t typeIndex = GetMonsterTypeIndex(uniqueMonsterData.mtype);
	const std::optional<Point> spot = GetUniqueMonstPosition(uniqindex);
	if (!spot) {
		LogWarn("PlaceLesserUniqueMonst: no free tile for unique {:d} - not placed", static_cast<int>(uniqindex));
		return; // a full floor drops the champion rather than hang (round 49 audit)
	}
	const Point position = *spot;

	const size_t championIndex = ActiveMonsterCount;
	PlaceMonster(championIndex, typeIndex, position, /*ordinary=*/false);
	Monster &monster = Monsters[championIndex];
	ActiveMonsterCount++;

	// What an ordinary one of these is worth on this floor, before the champion data lands on it.
	const int ordinaryHealth = monster.maxHitPoints;
	const uint8_t ordinaryMinDamage = monster.minDamage;
	const uint8_t ordinaryMaxDamage = monster.maxDamage;
	const uint8_t ordinaryMinSpecial = monster.minDamageSpecial;
	const uint8_t ordinaryMaxSpecial = monster.maxDamageSpecial;
	const uint8_t ordinaryArmor = monster.armorClass;

	const size_t firstMinionIndex = ActiveMonsterCount;
	PrepareUniqueMonst(monster, uniqindex, minionType, packSize, uniqueMonsterData);

	const auto scaleDamage = [](uint8_t base, int percent) {
		return static_cast<uint8_t>(std::min(base * percent / 100, 255));
	};

	const int healthPercent = boss ? oracool::BossHealthPercent() : LesserUniqueHealthPercent;
	const int damagePercent = boss ? oracool::BossDamagePercent() : LesserUniqueDamagePercent;
	const int armorBonus = boss ? oracool::BossArmorBonus() : LesserUniqueArmorBonus;

	monster.maxHitPoints = std::max(ordinaryHealth * healthPercent / 100, 64);
	monster.hitPoints = monster.maxHitPoints;
	monster.minDamage = scaleDamage(ordinaryMinDamage, damagePercent);
	monster.maxDamage = scaleDamage(ordinaryMaxDamage, damagePercent);
	monster.minDamageSpecial = scaleDamage(ordinaryMinSpecial, damagePercent);
	monster.maxDamageSpecial = scaleDamage(ordinaryMaxSpecial, damagePercent);
	monster.armorClass = static_cast<uint8_t>(std::min(ordinaryArmor + armorBonus, 255));

	// The modifier last, so anything it adds sits on top of the floor-scaled numbers rather than
	// being overwritten by them. Setting it is also what MARKS this monster as a lesser unique:
	// uniqueType alone cannot tell Garbud from a champion borrowing his shape.
	//
	// Dread is never ROLLED - RollLesserUniqueAffix cannot produce it - so this assignment is the
	// only way a boss comes into existence, and reading `lesserAffix == Dread` anywhere else is a
	// safe test for one.
	monster.lesserAffix = boss ? LesserUniqueAffix::Dread : oracool::RollLesserUniqueAffix(uniqindex);
	// Rolled once, here, and then saved - see Monster::lesserNameSeed. Both the name and the tint read
	// it, which is also what keeps them consistent with each other: two champions that look alike are
	// named alike only if they really are the same roll.
	monster.lesserNameSeed = oracool::RollLesserUniqueNameSeed();
	oracool::TintLesserUnique(monster);
	oracool::ApplyLesserUniqueAffix(monster);
	// A boss's SECOND trait, derived from the name seed rolled just above - so it must come after
	// that roll, and after the profile, for the same reason the affix does: it adds to the finished
	// numbers rather than to the ordinary ones. Does nothing unless this is a boss.
	oracool::ApplyBossTrait(monster);

	// The escort PlaceGroup just created. They are ordinary monsters of the same type, so they are
	// already floor-correct - this only lifts them enough to read as a champion's retinue rather than
	// as the wandering monsters they are standing next to.
	for (size_t i = firstMinionIndex; i < ActiveMonsterCount; i++) {
		Monster &minion = Monsters[i];
		minion.maxHitPoints = std::max(minion.maxHitPoints * MinionHealthPercent / 100, 64);
		minion.hitPoints = minion.maxHitPoints;
		minion.minDamage = scaleDamage(minion.minDamage, MinionDamagePercent);
		minion.maxDamage = scaleDamage(minion.maxDamage, MinionDamagePercent);
		minion.minDamageSpecial = scaleDamage(minion.minDamageSpecial, MinionDamagePercent);
		minion.maxDamageSpecial = scaleDamage(minion.maxDamageSpecial, MinionDamagePercent);
	}
}

void PlaceLesserUniques()
{
	constexpr int LesserUniquePackSize = 4;

	const int wanted = oracool::LesserUniqueCountForLevel();
	for (int placed = 0; placed < wanted; placed++) {
		const std::optional<UniqueMonsterType> choice = oracool::ChooseLesserUnique();
		if (!choice) {
			// This level hosts no monster type with a champion written for it at all. Running out of
			// DISTINCT ones no longer ends up here - since 1.6.1 ChooseLesserUnique falls back to a
			// repeat with a different modifier, and since 1.6.2 that repeat is renamed and recoloured,
			// so it reads as another champion rather than as a duplication bug.
			return;
		}

		const size_t minionType = GetMonsterTypeIndex(UniqueMonstersData[static_cast<size_t>(*choice)].mtype);
		if (minionType == LevelMonsterTypeCount)
			return;
		// The same headroom check the scattered monsters get. A champion plus escort is five bodies,
		// and the pool is shared.
		if (ActiveMonsterCount + LesserUniquePackSize + 1 > MaxEnemyMonsters - 10)
			return;

		PlaceLesserUniqueMonst(*choice, minionType, LesserUniquePackSize);
	}
}

/**
 * @brief Places the floor's endgame boss, if it has earned one.
 *
 * Runs AFTER PlaceLesserUniques deliberately. The champions take the floor's distinct monster types
 * first, so the boss gets whatever is left - which is the right way round: a boss repeating a
 * champion's identity reads as the same fight twice, and it is the boss that should be the one
 * standing out. ChooseLesserUnique's own repeat fallback still covers the case where the floor has
 * fewer types than it wants bodies.
 */
/**
 * @brief Fills a named-encounter arena: one Dread boss and its escort, and nothing else.
 *
 * The arena .dun files are PvP rooms with no monster spawn points authored, so the type is loaded
 * explicitly here - which is exactly what every quest set level does (see the Warlord and Lazarus
 * branches below). ChooseLesserUnique can then find a unique riding that type, because it only ever
 * picks from what the level has loaded.
 *
 * Nothing else is placed. The encounter IS the fight, and a room of ordinary monsters would dilute
 * the one thing the player came for.
 */
void PlaceNamedEncounter()
{
	oracool::NamedEncounter encounter;
	if (!oracool::CurrentNamedEncounter(encounter))
		return;

	// AddMonsterType loads the sprites and sounds itself, which is what makes this work on a room
	// that has none - an arena .dun is a PvP map and loads no monster types at all.
	//
	// No "did it fit" check here, deliberately: the `index == LevelMonsterTypeCount` idiom used
	// elsewhere belongs to GetMonsterTypeIndex, where that value means NOT FOUND. AddMonsterType
	// increments the count as it adds, so the same comparison after it can never be true - it was
	// written here first and was inert, which is worse than absent because it reads like a guard.
	const size_t typeIndex = AddMonsterType(oracool::NamedEncounterMonster(encounter), PLACE_UNIQUE);

	const std::optional<UniqueMonsterType> choice = oracool::ChooseLesserUnique(/*excludeLevelOwned=*/false);
	if (!choice)
		return;
	if (ActiveMonsterCount + oracool::BossPackSize() + 1 > MaxEnemyMonsters - 10)
		return;

	PlaceLesserUniqueMonst(*choice, typeIndex, oracool::BossPackSize(), /*boss=*/true);
}

void PlaceEndgameBoss()
{
	for (int placed = 0; placed < oracool::BossCountForLevel(); placed++) {
		const std::optional<UniqueMonsterType> choice = oracool::ChooseLesserUnique();
		if (!choice)
			return;
		const size_t minionType = GetMonsterTypeIndex(UniqueMonstersData[static_cast<size_t>(*choice)].mtype);
		if (minionType == LevelMonsterTypeCount)
			return;
		// A boss plus its larger escort is seven bodies, against a champion's five - the same
		// headroom check, with the boss's own pack size in it rather than the champion's.
		if (ActiveMonsterCount + oracool::BossPackSize() + 1 > MaxEnemyMonsters - 10)
			return;

		PlaceLesserUniqueMonst(*choice, minionType, oracool::BossPackSize(), /*boss=*/true);
	}
}

void PlaceUniqueMonsters()
{
	for (size_t u = 0; UniqueMonstersData[u].mtype != -1; u++) {
		if (UniqueMonstersData[u].mlevel != currlevel)
			continue;

		const size_t minionType = GetMonsterTypeIndex(UniqueMonstersData[u].mtype);
		if (minionType == LevelMonsterTypeCount)
			continue;

		UniqueMonsterType uniqueType = static_cast<UniqueMonsterType>(u);
		if (uniqueType == UniqueMonsterType::Garbud && Quests[Q_GARBUD]._qactive == QUEST_NOTAVAIL)
			continue;
		if (uniqueType == UniqueMonsterType::Zhar && Quests[Q_ZHAR]._qactive == QUEST_NOTAVAIL)
			continue;
		if (uniqueType == UniqueMonsterType::SnotSpill && Quests[Q_LTBANNER]._qactive == QUEST_NOTAVAIL)
			continue;
		if (uniqueType == UniqueMonsterType::Lachdan && Quests[Q_VEIL]._qactive == QUEST_NOTAVAIL)
			continue;
		if (uniqueType == UniqueMonsterType::WarlordOfBlood && Quests[Q_WARLORD]._qactive == QUEST_NOTAVAIL)
			continue;

		PlaceUniqueMonst(uniqueType, minionType, 8);
	}
}

/**
 * @brief Fills a rift floor (oracool/rift.h) the way InitMonsters fills a dungeon floor - champions,
 * a Dread boss and the scatter - since the `!setlevel` block below deliberately skips set levels.
 * Then the tier's scaling over everything standing. The guardian is NOT placed here: he rises when
 * the kill bar fills (SpawnRiftGuardian).
 */
void PlaceRiftMonsters()
{
	if (!oracool::InRift())
		return;

	// The arrival is kept out of sight, as a floor's stairs are through their triggers: a rift has none, so packs landed
	// three tiles from the hero (round 13 audit, v1.12.238). Vision is raised for the placement and dropped after.
	const Point arrival = ViewPosition;
	for (int s = -2; s < 2; s++) {
		for (int t = -2; t < 2; t++)
			DoVision(arrival + Displacement { s, t }, 15, MAP_EXP_NONE, false);
	}

	PlaceLesserUniques();
	PlaceEndgameBoss();

	int na = 0;
	for (int s = 16; s < 96; s++) {
		for (int t = 16; t < 96; t++) {
			if (!IsTileSolid({ s, t }))
				na++;
		}
	}
	// TWICE a floor's count (user, 2026-09-20: "I reached 100% very fast. Make it require twice as
	// many kills"): the bar is a share of the floor's total credit (RiftBarPercentOfFloor), so doubling
	// the floor doubles the kills it takes while the bar stays reachable - a share above 100% of one
	// floor never could be. The cap below still holds.
	int numplacemonsters = 2 * (na / 30 * *sgOptions.Oracool.monsterDensityPercent / 100);
	// Room held back for the rift's theme rooms, as InitMonsters holds it for a floor's (round 13): at the default density
	// the doubled scatter filled to the cap and the theme rooms stood empty (round 17 audit, v1.12.242).
	const size_t themeReserve = std::min<size_t>(static_cast<size_t>(std::max(numthemes, 0)) * 8, 40);
	const size_t scatterCap = MaxEnemyMonsters - 10 - themeReserve;
	if (ActiveMonsterCount >= scatterCap)
		numplacemonsters = 0;
	else if (ActiveMonsterCount + numplacemonsters > scatterCap)
		numplacemonsters = static_cast<int>(scatterCap - ActiveMonsterCount);
	totalmonsters = ActiveMonsterCount + numplacemonsters;

	size_t scattertypes[NUM_MTYPES];
	int numscattypes = 0;
	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if ((LevelMonsterTypes[i].placeFlags & PLACE_SCATTER) != 0)
			scattertypes[numscattypes++] = i;
	}
	while (numscattypes > 0 && ActiveMonsterCount < totalmonsters) {
		const size_t before = ActiveMonsterCount;
		const size_t typeIndex = scattertypes[GenerateRnd(numscattypes)];
		const int na2 = FlipCoin() ? 1 : GenerateRnd(3) + 3;
		PlaceGroup(typeIndex, na2);
		if (ActiveMonsterCount == before)
			break;
	}

	for (int s = -2; s < 2; s++) {
		for (int t = -2; t < 2; t++)
			DoUnVision(arrival + Displacement { s, t }, 15);
	}

	for (size_t i = 0; i < ActiveMonsterCount; i++)
		oracool::ScaleRiftMonster(Monsters[ActiveMonsters[i]]);
}

void PlaceQuestMonsters()
{
	if (!setlevel) {
		if (Quests[Q_BUTCHER].IsAvailable()) {
			PlaceUniqueMonst(UniqueMonsterType::Butcher, 0, 0);
		}

		if (currlevel == Quests[Q_SKELKING]._qlevel && UseMultiplayerQuests()) {
			for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
				if (IsSkel(LevelMonsterTypes[i].type)) {
					PlaceUniqueMonst(UniqueMonsterType::SkeletonKing, i, 30);
					break;
				}
			}
		}

		if (Quests[Q_LTBANNER].IsAvailable()) {
			auto dunData = LoadFileInMem<uint16_t>("levels\\l1data\\banner1.dun");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld());
		}
		if (Quests[Q_BLOOD].IsAvailable()) {
			auto dunData = LoadFileInMem<uint16_t>("levels\\l2data\\blood2.dun");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld());
		}
		if (Quests[Q_BLIND].IsAvailable()) {
			auto dunData = LoadFileInMem<uint16_t>("levels\\l2data\\blind2.dun");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld());
		}
		if (Quests[Q_ANVIL].IsAvailable()) {
			auto dunData = LoadFileInMem<uint16_t>("levels\\l3data\\anvil.dun");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld() + Displacement { 2, 2 });
		}
		if (Quests[Q_WARLORD].IsAvailable()) {
			auto dunData = LoadFileInMem<uint16_t>("levels\\l4data\\warlord.dun");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld());
			AddMonsterType(UniqueMonsterType::WarlordOfBlood, PLACE_SCATTER);
		}
		if (Quests[Q_VEIL].IsAvailable()) {
			AddMonsterType(UniqueMonsterType::Lachdan, PLACE_SCATTER);
		}
		if (Quests[Q_ZHAR].IsAvailable() && zharlib == -1) {
			Quests[Q_ZHAR]._qactive = QUEST_NOTAVAIL;
		}

		if (currlevel == Quests[Q_BETRAYER]._qlevel && UseMultiplayerQuests()) {
			AddMonsterType(UniqueMonsterType::Lazarus, PLACE_UNIQUE);
			AddMonsterType(UniqueMonsterType::RedVex, PLACE_UNIQUE);
			PlaceUniqueMonst(UniqueMonsterType::Lazarus, 0, 0);
			PlaceUniqueMonst(UniqueMonsterType::RedVex, 0, 0);
			PlaceUniqueMonst(UniqueMonsterType::BlackJade, 0, 0);
			auto dunData = LoadFileInMem<uint16_t>("levels\\l4data\\vile1.dun");
			SetMapMonsters(dunData.get(), SetPiece.position.megaToWorld());
		}

		if (currlevel == 24) {
			UberDiabloMonsterIndex = -1;
			const size_t typeIndex = GetMonsterTypeIndex(MT_NAKRUL);
			if (typeIndex < LevelMonsterTypeCount) {
				for (size_t i = 0; i < ActiveMonsterCount; i++) {
					Monster &monster = Monsters[i];
					if (monster.isUnique() || monster.levelType == typeIndex) {
						UberDiabloMonsterIndex = static_cast<int>(i);
						break;
					}
				}
			}
			if (UberDiabloMonsterIndex == -1)
				PlaceUniqueMonst(UniqueMonsterType::NaKrul, 0, 0);
		}
	} else if (setlvlnum == SL_SKELKING) {
		PlaceUniqueMonst(UniqueMonsterType::SkeletonKing, 0, 0);
	} else if (setlvlnum == SL_VILEBETRAYER) {
		AddMonsterType(UniqueMonsterType::Lazarus, PLACE_UNIQUE);
		AddMonsterType(UniqueMonsterType::RedVex, PLACE_UNIQUE);
		AddMonsterType(UniqueMonsterType::BlackJade, PLACE_UNIQUE);
		PlaceUniqueMonst(UniqueMonsterType::Lazarus, 0, 0);
		PlaceUniqueMonst(UniqueMonsterType::RedVex, 0, 0);
		PlaceUniqueMonst(UniqueMonsterType::BlackJade, 0, 0);
	}
}

void LoadDiabMonsts()
{
	{
		auto dunData = LoadFileInMem<uint16_t>("levels\\l4data\\diab1.dun");
		SetMapMonsters(dunData.get(), DiabloQuad1.megaToWorld());
	}
	{
		auto dunData = LoadFileInMem<uint16_t>("levels\\l4data\\diab2a.dun");
		SetMapMonsters(dunData.get(), DiabloQuad2.megaToWorld());
	}
	{
		auto dunData = LoadFileInMem<uint16_t>("levels\\l4data\\diab3a.dun");
		SetMapMonsters(dunData.get(), DiabloQuad3.megaToWorld());
	}
	{
		auto dunData = LoadFileInMem<uint16_t>("levels\\l4data\\diab4a.dun");
		SetMapMonsters(dunData.get(), DiabloQuad4.megaToWorld());
	}
}

void DeleteMonster(size_t activeIndex)
{
	const auto &monster = Monsters[ActiveMonsters[activeIndex]];
	if (bool *stunned = StunFlagOf(monster); stunned != nullptr)
		*stunned = false;
	if ((monster.flags & MFLAG_BERSERK) != 0) {
		AddUnLight(monster.lightId);
	}

	// Oracool: the slot's cry state goes with the monster, so nothing waits there for the next one.
	oracool::ClearWarcryStateForMonster(monster);
	oracool::ClearRfa12StateForMonster(monster);
	oracool::OnMonsterSlotFreed(monster.getId());
	oracool::ClearCurseForMonster(monster);
	oracool::ClearColdStateForMonster(monster);
	oracool::ClearPassiveMarksForMonster(monster);
	oracool::OnCompanionFocusSlotFreed(monster.getId());
	oracool::TelemetryForgetMonster(monster.getId()); // a minion's kill clock did not pass to the slot's next hostile (round 12)

	ActiveMonsterCount--;
	std::swap(ActiveMonsters[activeIndex], ActiveMonsters[ActiveMonsterCount]); // This ensures alive monsters are before ActiveMonsterCount in the array and any deleted monster after
}

void NewMonsterAnim(Monster &monster, MonsterGraphic graphic, Direction md, AnimationDistributionFlags flags = AnimationDistributionFlags::None, int8_t numSkippedFrames = 0, int8_t distributeFramesBeforeFrame = 0)
{
	// Oracool Phase 3.2: the ONE place a monster binds its sprites, and therefore the one place a
	// size can change them. A normal monster gets the shared CMonster data exactly as before.
	const AnimStruct *scaled = oracool::GetScaledAnim(monster, graphic);
	const AnimStruct &animData = scaled != nullptr ? *scaled : monster.type().getAnimData(graphic);
	// The Frenzied and Fleet variants (2026-09-19): FRAMES skipped from the attack or the walk, the
	// way the player's fast attack and run are built. The first version took a tick off each frame,
	// floored at one - and every one of the 138 types already runs its walk and attack at one tick
	// per frame, so it never did anything (audit, 2026-09-19). Here because this is the one place
	// every animation of every monster starts. The skip is bounded so the animation keeps at least
	// two frames, and an attack keeps its hit frame (animFrameNum, 1-based) with a frame to spare
	// before it; the attack distributes the skip before the hit frame, as NewPlrAnim does.
	int8_t skipped = numSkippedFrames;
	int8_t distributeBefore = distributeFramesBeforeFrame;
	if (const int extra = oracool::VariantSkippedFrames(monster, graphic); extra > 0) {
		if (graphic == MonsterGraphic::Attack) {
			const int hitFrame = monster.data().animFrameNum;
			skipped = static_cast<int8_t>(std::clamp<int>(skipped + extra, 0, std::max(0, hitFrame - 3)));
			if (distributeBefore == 0)
				distributeBefore = static_cast<int8_t>(hitFrame);
		} else {
			skipped = static_cast<int8_t>(std::clamp<int>(skipped + extra, 0, std::max(0, animData.frames - 2)));
		}
	}
	monster.animInfo.setNewAnimation(animData.spritesForDirection(md), animData.frames, animData.rate, flags, skipped, distributeBefore);
	monster.flags &= ~(MFLAG_LOCK_ANIMATION | MFLAG_ALLOW_SPECIAL);
	monster.direction = md;
}

void StartMonsterGotHit(Monster &monster)
{
	// A companion does not flinch - and this would also snap a walking one back to the tile it left.
	if (oracool::IsCompanion(monster))
		return;
	// Nor a golem mid-step: it does not flinch, so its walk ran on from a tile snapped back to the one it left, and the walk's
	// end cleared its own grid mark - an unclickable, unhittable golem until its next step (round 62 audit; the Necromancer's
	// army walks in formation all the time).
	if (monster.type().type == MT_GOLEM && monster.isWalking())
		return;
	// A skill-stunned monster is moved (a knockback still pushes it) but stays stunned: M_GetKnockback reaches here
	// without M_StartHit's stun test, and Bash or a knockback weapon ended every stun (round 24 audit, v1.12.249).
	if (monster.type().type != MT_GOLEM && !IsMonsterStunned(monster)) {
		auto animationFlags = gGameLogicStep < GameLogicStep::ProcessMonsters ? AnimationDistributionFlags::ProcessAnimationPending : AnimationDistributionFlags::None;
		int8_t numSkippedFrames = (gbIsHellfire && monster.type().type == MT_DIABLO) ? 4 : 0;
		NewMonsterAnim(monster, MonsterGraphic::GotHit, monster.direction, animationFlags, numSkippedFrames);
		monster.mode = MonsterMode::HitRecovery;
	}
	monster.position.tile = monster.position.old;
	monster.position.future = monster.position.old;
	M_ClearSquares(monster);
	dMonster[monster.position.tile.x][monster.position.tile.y] = monster.getId() + 1;
	// Its light with it, as StunMonster moves it since round 17: a flinch mid-step and a knockback left the glow a tile off
	// until the next walk (round 19 audit, v1.12.244).
	if (monster.lightId != NO_LIGHT) {
		ChangeLightXY(monster.lightId, monster.position.tile);
		ChangeLightOffset(monster.lightId, {});
	}
}

bool IsRanged(Monster &monster)
{
	return IsAnyOf(monster.ai, MonsterAIID::SkeletonRanged, MonsterAIID::GoatRanged, MonsterAIID::Succubus, MonsterAIID::LazarusSuccubus);
}

void UpdateEnemy(Monster &monster)
{
	WorldTilePosition target;
	int menemy = -1;
	int bestDist = -1;
	bool bestsameroom = false;
	const WorldTilePosition position = monster.position.tile;
	const bool isPlayerMinion = monster.isPlayerMinion();
	// A converted monster fights for the Paladin while its clock runs: no player is a target (dev note, 2026-09-27).
	if (!isPlayerMinion && !oracool::IsMonsterConverted(monster)) {
		for (size_t pnum = 0; pnum < Players.size(); pnum++) {
			const Player &player = Players[pnum];
			if (!player.plractive || !player.isOnActiveLevel() || player._pLvlChanging
			    || (((player._pHitPoints >> 6) == 0) && gbIsMultiplayer))
				continue;
			const bool sameroom = (dTransVal[position.x][position.y] == dTransVal[player.position.tile.x][player.position.tile.y]);
			const int dist = position.WalkingDistance(player.position.tile);
			if ((sameroom && !bestsameroom)
			    || ((sameroom || !bestsameroom) && dist < bestDist)
			    || (menemy == -1)) {
				monster.flags &= ~MFLAG_TARGETS_MONSTER;
				menemy = static_cast<int>(pnum);
				target = player.position.future;
				bestDist = dist;
				bestsameroom = sameroom;
			}
		}
	}
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int monsterId = ActiveMonsters[i];
		Monster &otherMonster = Monsters[monsterId];
		if (&otherMonster == &monster)
			continue;
		if ((otherMonster.hitPoints >> 6) <= 0)
			continue;
		if (otherMonster.position.tile == GolemHoldingCell)
			continue;
		if (otherMonster.talkMsg != TEXT_NONE && M_Talker(otherMonster))
			continue;
		if (isPlayerMinion && otherMonster.isPlayerMinion()) // prevent golems from fighting each other
			continue;
		// Nor the hero's army and a monster Conversion turned: the same side (round 36 audit: they fought, and a golem's kill of
		// the convert paid its experience and loot).
		if ((isPlayerMinion || oracool::IsMonsterConverted(monster)) && (otherMonster.isPlayerMinion() || oracool::IsMonsterConverted(otherMonster)))
			continue;

		const int dist = otherMonster.position.tile.WalkingDistance(position);
		if (((monster.flags & MFLAG_GOLEM) == 0
		        && (monster.flags & MFLAG_BERSERK) == 0
		        && dist >= 2
		        && !IsRanged(monster))
		    || ((monster.flags & MFLAG_GOLEM) == 0
		        && (monster.flags & MFLAG_BERSERK) == 0
		        && (otherMonster.flags & MFLAG_GOLEM) == 0)) {
			continue;
		}
		const bool sameroom = dTransVal[position.x][position.y] == dTransVal[otherMonster.position.tile.x][otherMonster.position.tile.y];
		if ((sameroom && !bestsameroom)
		    || ((sameroom || !bestsameroom) && dist < bestDist)
		    || (menemy == -1)) {
			monster.flags |= MFLAG_TARGETS_MONSTER;
			menemy = monsterId;
			target = otherMonster.position.future;
			bestDist = dist;
			bestsameroom = sameroom;
		}
	}
	// A companion guard or decoy holds the attention of what comes near it (oracool/companion.h).
	if (!isPlayerMinion) {
		if (const int guard = oracool::CompanionTauntTarget(monster); guard >= 0) {
			monster.flags |= MFLAG_TARGETS_MONSTER;
			menemy = guard;
			target = Monsters[guard].position.future;
		} else if (const int lure = oracool::CurseLureTarget(monster); lure >= 0) {
			// Attract (oracool/curses.h): the cursed one is what its neighbours go for.
			monster.flags |= MFLAG_TARGETS_MONSTER;
			menemy = lure;
			target = Monsters[lure].position.future;
		}
	}
	if (menemy != -1) {
		monster.flags &= ~MFLAG_NO_ENEMY;
		monster.enemy = menemy;
		monster.enemyPosition = target;
	} else {
		monster.flags |= MFLAG_NO_ENEMY;
	}
}

/**
 * @brief Make the AI wait a bit before thinking again
 * @param monster The monster that will wait
 * @param len
 */
void AiDelay(Monster &monster, int len)
{
	if (len <= 0) {
		return;
	}

	if (monster.ai == MonsterAIID::Lazarus) {
		return;
	}

	if (bool *stunned = StunFlagOf(monster); stunned != nullptr)
		*stunned = false; // an AI's own pause, not a stun - StunMonster sets it after
	monster.var2 = len;
	monster.mode = MonsterMode::Delay;
}

/**
 * @brief Get the direction from the monster to its current enemy
 */
Direction GetMonsterDirection(Monster &monster)
{
	return GetDirection(monster.position.tile, monster.enemyPosition);
}

void StartSpecialStand(Monster &monster, Direction md)
{
	NewMonsterAnim(monster, MonsterGraphic::Special, md);
	monster.mode = MonsterMode::SpecialStand;
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void WalkNorthwards(Monster &monster, int xadd, int yadd, Direction endDir)
{
	const auto fx = static_cast<WorldTileCoord>(xadd + monster.position.tile.x);
	const auto fy = static_cast<WorldTileCoord>(yadd + monster.position.tile.y);

	dMonster[fx][fy] = -(monster.getId() + 1);
	monster.mode = MonsterMode::MoveNorthwards;
	monster.position.old = monster.position.tile;
	monster.position.future = { fx, fy };
	monster.var1 = xadd;
	monster.var2 = yadd;
	monster.var3 = static_cast<int>(endDir);
	NewMonsterAnim(monster, MonsterGraphic::Walk, endDir, AnimationDistributionFlags::ProcessAnimationPending, -1);
}

void WalkSouthwards(Monster &monster, int xoff, int yoff, int xadd, int yadd, Direction endDir)
{
	const auto fx = static_cast<WorldTileCoord>(xadd + monster.position.tile.x);
	const auto fy = static_cast<WorldTileCoord>(yadd + monster.position.tile.y);

	dMonster[monster.position.tile.x][monster.position.tile.y] = -(monster.getId() + 1);
	monster.var1 = monster.position.tile.x;
	monster.var2 = monster.position.tile.y;
	monster.position.old = monster.position.tile;
	monster.position.tile = { fx, fy };
	monster.position.future = { fx, fy };
	dMonster[fx][fy] = monster.getId() + 1;
	if (monster.lightId != NO_LIGHT)
		ChangeLightXY(monster.lightId, monster.position.tile);
	monster.mode = MonsterMode::MoveSouthwards;
	monster.var3 = static_cast<int>(endDir);
	NewMonsterAnim(monster, MonsterGraphic::Walk, endDir, AnimationDistributionFlags::ProcessAnimationPending, -1);
}

void WalkSideways(Monster &monster, int xoff, int yoff, int xadd, int yadd, int mapx, int mapy, Direction endDir)
{
	const auto fx = static_cast<WorldTileCoord>(xadd + monster.position.tile.x);
	const auto fy = static_cast<WorldTileCoord>(yadd + monster.position.tile.y);
	const auto x = static_cast<WorldTileCoord>(mapx + monster.position.tile.x);
	const auto y = static_cast<WorldTileCoord>(mapy + monster.position.tile.y);

	if (monster.lightId != NO_LIGHT)
		ChangeLightXY(monster.lightId, { x, y });

	dMonster[monster.position.tile.x][monster.position.tile.y] = -(monster.getId() + 1);
	dMonster[fx][fy] = monster.getId() + 1;
	monster.position.temp = { x, y };
	monster.position.old = monster.position.tile;
	monster.position.future = { fx, fy };
	monster.mode = MonsterMode::MoveSideways;
	monster.var1 = fx;
	monster.var2 = fy;
	monster.var3 = static_cast<int>(endDir);
	NewMonsterAnim(monster, MonsterGraphic::Walk, endDir, AnimationDistributionFlags::ProcessAnimationPending, -1);
}

void StartAttack(Monster &monster)
{
	Direction md = GetMonsterDirection(monster);
	NewMonsterAnim(monster, MonsterGraphic::Attack, md, AnimationDistributionFlags::ProcessAnimationPending);
	monster.mode = MonsterMode::MeleeAttack;
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void StartRangedAttack(Monster &monster, MissileID missileType, int dam)
{
	Direction md = GetMonsterDirection(monster);
	NewMonsterAnim(monster, MonsterGraphic::Attack, md, AnimationDistributionFlags::ProcessAnimationPending);
	monster.mode = MonsterMode::RangedAttack;
	monster.var1 = static_cast<int16_t>(missileType); // not int8_t: the fork's ids run past 127 (round 11 audit)
	monster.var2 = dam;
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void StartRangedSpecialAttack(Monster &monster, MissileID missileType, int dam)
{
	Direction md = GetMonsterDirection(monster);
	int8_t distributeFramesBeforeFrame = 0;
	if (monster.ai == MonsterAIID::Mega)
		distributeFramesBeforeFrame = monster.data().animFrameNumSpecial;
	NewMonsterAnim(monster, MonsterGraphic::Special, md, AnimationDistributionFlags::ProcessAnimationPending, 0, distributeFramesBeforeFrame);
	monster.mode = MonsterMode::SpecialRangedAttack;
	monster.var1 = static_cast<int16_t>(missileType); // not int8_t (round 11 audit)
	monster.var2 = 0;
	monster.var3 = dam;
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void StartSpecialAttack(Monster &monster)
{
	Direction md = GetMonsterDirection(monster);
	NewMonsterAnim(monster, MonsterGraphic::Special, md);
	monster.mode = MonsterMode::SpecialMeleeAttack;
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void StartEating(Monster &monster)
{
	NewMonsterAnim(monster, MonsterGraphic::Special, monster.direction);
	monster.mode = MonsterMode::SpecialMeleeAttack;
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
}

void DiabloDeath(Monster &diablo, bool sendmsg)
{
	PlaySFX(USFX_DIABLOD);
	auto &quest = Quests[Q_DIABLO];
	quest._qactive = QUEST_DONE;
	if (sendmsg)
		NetSendCmdQuest(true, quest);
	sgbSaveSoundOn = gbSoundOn;
	gbProcessPlayers = false;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		int monsterId = ActiveMonsters[i];
		Monster &monster = Monsters[monsterId];
		if (monster.type().type == MT_DIABLO || diablo.activeForTicks == 0)
			continue;

		NewMonsterAnim(monster, MonsterGraphic::Death, monster.direction);
		monster.mode = MonsterMode::Death;
		monster.var1 = 0;
		monster.position.tile = monster.position.old;
		monster.position.future = monster.position.tile;
		M_ClearSquares(monster);
		dMonster[monster.position.tile.x][monster.position.tile.y] = monsterId + 1;
	}
	AddLight(diablo.position.tile, 8);
	DoVision(diablo.position.tile, 8, MAP_EXP_NONE, true);
	int dist = diablo.position.tile.WalkingDistance(ViewPosition);
	if (dist > 20)
		dist = 20;
	diablo.var3 = ViewPosition.x << 16;
	diablo.position.temp.x = ViewPosition.y << 16;
	diablo.position.temp.y = (int)((diablo.var3 - (diablo.position.tile.x << 16)) / (double)dist);
	if (!gbIsMultiplayer) {
		Player &myPlayer = *MyPlayer;
		myPlayer.pDiabloKillLevel = std::max(myPlayer.pDiabloKillLevel, static_cast<uint8_t>(sgGameInitInfo.nDifficulty + 1));
	}
}

void SpawnLoot(Monster &monster, bool sendmsg)
{
	if (monster.type().type == MT_HORKSPWN) {
		return;
	}

	if (Quests[Q_GARBUD].IsAvailable() && monster.uniqueType == UniqueMonsterType::Garbud) {
		MakeRoomForGuaranteedReward(); // round 30 audit
		CreateTypeItem(monster.position.tile + Displacement { 1, 1 }, true, ItemType::Mace, IMISC_NONE, sendmsg, false);
	} else if (monster.uniqueType == UniqueMonsterType::Defiler) {
		if (effect_is_playing(USFX_DEFILER8))
			stream_stop();
		SpawnMapOfDoom(monster.position.tile, sendmsg);
		Quests[Q_DEFILER]._qactive = QUEST_DONE;
		NetSendCmdQuest(true, Quests[Q_DEFILER]);
	} else if (monster.uniqueType == UniqueMonsterType::HorkDemon) {
		if (sgGameInitInfo.bTheoQuest != 0) {
			SpawnTheodore(monster.position.tile, sendmsg);
		} else {
			MakeRoomForGuaranteedReward(); // his one reward, as every quest reward (round 22 audit)
			CreateAmulet(monster.position.tile, ItemLevelOfMonster(monster), sendmsg, false); // was 13 on every difficulty (round 11)
		}
	} else if (monster.type().type == MT_NAKRUL && !oracool::IsRiftGuardian(monster)) { // a rift's Na-Krul drops like any rift guardian
		int nSFX = IsUberRoomOpened ? USFX_NAKRUL4 : USFX_NAKRUL5;
		if (sgGameInitInfo.bCowQuest != 0)
			nSFX = USFX_NAKRUL6;
		if (effect_is_playing(nSFX))
			stream_stop();
		UberDiabloMonsterIndex = -2;
		// Room for each of the four (round 30 audit).
		MakeRoomForGuaranteedReward();
		CreateMagicWeapon(monster.position.tile, ItemType::Sword, ICURS_GREAT_SWORD, sendmsg, false);
		MakeRoomForGuaranteedReward();
		CreateMagicWeapon(monster.position.tile, ItemType::Staff, ICURS_WAR_STAFF, sendmsg, false);
		MakeRoomForGuaranteedReward();
		CreateMagicWeapon(monster.position.tile, ItemType::Bow, ICURS_LONG_WAR_BOW, sendmsg, false);
		MakeRoomForGuaranteedReward();
		CreateSpellBook(monster.position.tile, SpellID::Apocalypse, sendmsg, false);
	} else if (!monster.isPlayerMinion()) {
		SpawnItem(monster, monster.position.tile, sendmsg);
		// A rift's guardian: a pile of random items, not his quest drop (user, 2026-09-26: "rift guardians/bosses to drop
		// random items, not the uniques they drop when killed in quest"). SpawnItem skips his special treasure.
		if (oracool::IsRiftGuardian(monster)) {
			for (int i = 1; i < oracool::RiftGuardianItemCount(); i++)
				SpawnItem(monster, monster.position.tile, sendmsg);
		}
		// Oracool: a champion is the reason to fight it, which is what D2 and D3 both understood.
		// A SECOND roll on the same table rather than a better single one - no new item code, just
		// another ticket in the lottery the fork already runs (Unique, then Primal, then Buffed
		// Unique, then Rare, all in Oracool options). Two chances at that ladder is a materially
		// better drop without inventing a rarity tier nobody has balanced.
		// A CHANCE now, not a guarantee (user, 2026-08-30: "they seem to drop uniques very
		// generously. two at a time even"). Two at a time was this line: a champion always took a
		// second ticket in the same lottery, so every champion kill was two rolls at Unique, Primal,
		// Buffed Unique and Rare. The reason it exists still holds - a champion should be worth
		// fighting - so it is thinned rather than removed, and the percentage is an option.
		if (monster.lesserAffix != LesserUniqueAffix::None
		    && GenerateRnd(100) < std::clamp(*sgOptions.Oracool.championExtraDropChance, 0, 100))
			SpawnItem(monster, monster.position.tile, sendmsg);
	}
	// The fork's drop families for EVERY kill of an enemy, the special-branch bosses above included: Gharbad, the Defiler,
	// the Hork Demon and Na-Krul never reached them, and Na-Krul's signet roll was lost (round 19 audit, v1.12.244).
	if (!monster.isPlayerMinion()) {
		// Oracool: the set items' own drop roll, AFTER the vanilla spawns so the rndItemSeed-driven
		// stream above stays byte-identical - see TrySpawnOracoolSetItem for why they cannot ride
		// the ordinary pool.
		TrySpawnOracoolSetItem(monster, sendmsg);
		// The fifteen NAMED sets, which had no drop path at all until 2026-08-21 - the hook above
		// drops the worn TIER ladder despite its name. Same placement, same stream-safety reason.
		TrySpawnNamedSetPiece(monster, sendmsg);
		// Phase 1: and the gems' roll, same placement for the same stream-safety reason.
		TrySpawnOracoolGem(monster, sendmsg);
		// The Necromancer's three item families (2026-09-18): same placement, same reason.
		TrySpawnNecroBase(monster, sendmsg);
		// The Gilded variant's better drop (2026-09-19): same placement, same reason.
		TrySpawnGildedDrop(monster, sendmsg);
		// D2MXL Phase 2b: the signet, which declines outright on an ordinary kill - same placement,
		// same reason.
		// Phase 4: the Sealed Map, which is the ONLY way into a named encounter. Same
		// placement and the same stream-safety reason as every family above it.
		oracool::TrySpawnSealedMap(monster, sendmsg);
		TrySpawnSignet(monster, sendmsg);
	}
}

std::optional<Point> GetTeleportTile(const Monster &monster)
{
	int mx = monster.enemyPosition.x;
	int my = monster.enemyPosition.y;
	int rx = PickRandomlyAmong({ -1, 1 });
	int ry = PickRandomlyAmong({ -1, 1 });

	for (int j = -1; j <= 1; j++) {
		for (int k = -1; k < 1; k++) {
			if (j == 0 && k == 0)
				continue;
			int x = mx + rx * j;
			int y = my + ry * k;
			if (!InDungeonBounds({ x, y }) || x == monster.position.tile.x || y == monster.position.tile.y)
				continue;
			if (IsTileAvailable(monster, { x, y }))
				return Point { x, y };
		}
	}
	return {};
}

void Teleport(Monster &monster)
{
	if (monster.mode == MonsterMode::Petrified)
		return;

	std::optional<Point> position = GetTeleportTile(monster);
	if (!position)
		return;

	M_ClearSquares(monster);
	dMonster[monster.position.tile.x][monster.position.tile.y] = 0;
	dMonster[position->x][position->y] = monster.getId() + 1;
	monster.position.old = *position;
	monster.direction = GetMonsterDirection(monster);

	if (monster.lightId != NO_LIGHT) {
		ChangeLightXY(monster.lightId, *position);
	}
}

bool IsHardHit(Monster &target, unsigned dam)
{
	switch (target.type().type) {
	case MT_SNEAK:
	case MT_STALKER:
	case MT_UNSEEN:
	case MT_ILLWEAV:
		return true;
	default:
		return (dam >> 6) >= target.level(sgGameInitInfo.nDifficulty) + 3;
	}
}

void MonsterHitMonster(Monster &attacker, Monster &target, int dam)
{
	if (IsHardHit(target, dam)) {
		target.direction = Opposite(attacker.direction);
	}

	M_StartHit(target, dam);
}

void StartDeathFromMonster(Monster &attacker, Monster &target)
{
	Direction md = GetDirection(target.position.tile, attacker.position.tile);
	MonsterDeath(target, md, true);

	// Not an attacker that is itself dead (audit, 2026-09-27): a minion's struck-back damage (Iron Golem, Aberrant
	// Animator) or Iron Maiden can kill the attacker in the same blow, and standing it up again left a 0-life monster
	// that could not be hit or removed, walking and blocking its tile.
	if (gbIsHellfire && attacker.mode != MonsterMode::Death && (attacker.hitPoints >> 6) > 0)
		M_StartStand(attacker, attacker.direction);
}

void StartFadein(Monster &monster, Direction md, bool backwards)
{
	NewMonsterAnim(monster, MonsterGraphic::Special, md);
	monster.mode = MonsterMode::FadeIn;
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
	monster.flags &= ~MFLAG_HIDDEN;
	if (backwards) {
		monster.flags |= MFLAG_LOCK_ANIMATION;
		monster.animInfo.currentFrame = monster.animInfo.numberOfFrames - 1;
	}
}

void StartFadeout(Monster &monster, Direction md, bool backwards)
{
	NewMonsterAnim(monster, MonsterGraphic::Special, md);
	monster.mode = MonsterMode::FadeOut;
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
	if (backwards) {
		monster.flags |= MFLAG_LOCK_ANIMATION;
		monster.animInfo.currentFrame = monster.animInfo.numberOfFrames - 1;
	}
}

/**
 * @brief Starts the monster healing procedure.
 *
 * The monster will be healed between 1.47% and 25% of its max HP. The healing amount is stored in _mVar1.
 *
 * This is only used by Gargoyles.
 *
 * @param monster The monster that will be healed.
 */
void StartHeal(Monster &monster)
{
	monster.changeAnimationData(MonsterGraphic::Special);
	monster.animInfo.currentFrame = monster.type().getAnimData(MonsterGraphic::Special).frames - 1;
	monster.flags |= MFLAG_LOCK_ANIMATION;
	monster.mode = MonsterMode::Heal;
	monster.var1 = monster.maxHitPoints / (16 * (GenerateRnd(5) + 4));
}

void SyncLightPosition(Monster &monster)
{
	if (monster.lightId == NO_LIGHT)
		return;

	const WorldTileDisplacement offset = monster.isWalking() ? monster.position.CalculateWalkingOffset(monster.direction, monster.animInfo) : WorldTileDisplacement {};
	ChangeLightOffset(monster.lightId, offset.screenToLight());
}

void MonsterIdle(Monster &monster)
{
	if (monster.type().type == MT_GOLEM && !oracool::IsCompanion(monster)) // see M_StartStand
		monster.changeAnimationData(MonsterGraphic::Walk);
	else
		monster.changeAnimationData(MonsterGraphic::Stand);

	if (monster.animInfo.isLastFrame())
		UpdateEnemy(monster);

	if (monster.var2 < std::numeric_limits<int16_t>::max())
		monster.var2++;
}

/**
 * @brief Continue movement towards new tile
 */
bool MonsterWalk(Monster &monster, MonsterMode variant)
{
	// Check if we reached new tile
	const bool isAnimationEnd = monster.animInfo.isLastFrame();
	if (isAnimationEnd) {
		switch (variant) {
		case MonsterMode::MoveNorthwards:
			dMonster[monster.position.tile.x][monster.position.tile.y] = 0;
			monster.position.tile.x += monster.var1;
			monster.position.tile.y += monster.var2;
			dMonster[monster.position.tile.x][monster.position.tile.y] = monster.getId() + 1;
			break;
		case MonsterMode::MoveSouthwards:
			dMonster[monster.var1][monster.var2] = 0;
			break;
		case MonsterMode::MoveSideways:
			dMonster[monster.position.tile.x][monster.position.tile.y] = 0;
			monster.position.tile = WorldTilePosition { static_cast<WorldTileCoord>(monster.var1), static_cast<WorldTileCoord>(monster.var2) };
			// dMonster is set here for backwards comparability, without it the monster would be invisible if loaded from a vanilla save.
			dMonster[monster.position.tile.x][monster.position.tile.y] = monster.getId() + 1;
			break;
		default:
			break;
		}
		if (monster.lightId != NO_LIGHT)
			ChangeLightXY(monster.lightId, monster.position.tile);
		M_StartStand(monster, monster.direction);
	} else { // We didn't reach new tile so update monster's "sub-tile" position
		if (monster.animInfo.tickCounterOfCurrentFrame == 0) {
			if (monster.animInfo.currentFrame == 0 && monster.type().type == MT_FLESTHNG)
				PlayEffect(monster, MonsterSound::Special);
		}
	}

	if (monster.lightId != NO_LIGHT)
		SyncLightPosition(monster);

	return isAnimationEnd;
}

void MonsterAttackMonster(Monster &attacker, Monster &target, int hper, int mind, int maxd)
{
	if (!target.isPossibleToHit())
		return;

	int hit = GenerateRnd(100);
	if (target.mode == MonsterMode::Petrified)
		hit = 0;
	if (target.tryLiftGargoyle())
		return;
	if (hit >= hper)
		return;

	// Never a negative blow (round 49 audit): a Hollow magma demon's second blow is its minimum less two, -1, and it healed
	// the monster it struck and still made it flinch.
	mind = std::max(mind, 0);
	maxd = std::max(maxd, mind);
	int dam = (mind + GenerateRnd(maxd - mind + 1)) << 6; // a 0 blow still wakes and flinches, as before (round 50 audit)
	// Weaken and Decrepify blunt a cursed monster's blow on the army too, not only on the hero (round 20 audit, v1.12.245).
	// Not on a 0 blow: the floor of 1 made a cursed attacker hit harder than an uncursed one (round 51 audit).
	if (const int weakened = oracool::MonsterDebuffDamagePercent(attacker); weakened != 0 && dam > 0)
		dam = std::max(dam + dam * weakened / 100, 1 << 6);
	ApplyMonsterDamage(DamageType::Physical, target, dam);

	if (const Player *armyOwner = oracool::MinionOwner(attacker); armyOwner != nullptr) {
		// A minion's slot says nothing about whose it is; its record does (oracool/minions.h).
		target.tag(*armyOwner);
		oracool::OnMinionBlow(attacker, target, dam);
		oracool::OnCursedMonsterStruck(target, Players[armyOwner->getId()], &attacker, dam); // Life Tap
	}
	if (oracool::IsMinion(target)) {
		oracool::OnMinionStruck(target, attacker, dam);
		if (attacker.mode != MonsterMode::Death)
			oracool::OnCursedMonsterDealtBlow(attacker, dam); // Iron Maiden
	} else if (attacker.isPlayerMinion() && attacker.getId() < Players.size()) {
		int playerId = attacker.getId();
		const Player &player = Players[playerId];
		target.tag(player);
	}

	if (target.hitPoints >> 6 <= 0) {
		StartDeathFromMonster(attacker, target);
	} else {
		MonsterHitMonster(attacker, target, dam);
	}

	if (target.activeForTicks == 0) {
		target.activeForTicks = UINT8_MAX;
		target.position.last = attacker.position.tile;
	}
}

int CheckReflect(Monster &monster, Player &player, int dam)
{
	player.wReflections--;
	if (player.wReflections <= 0)
		NetSendCmdParam1(true, CMD_SETREFLECT, 0);
	// reflects 20-30% damage
	int mdam = dam * RandomIntBetween(20, 30, true) / 100;
	ApplyMonsterDamage(DamageType::Physical, monster, mdam);
	if (monster.hitPoints >> 6 <= 0)
		M_StartKill(monster, player);
	else
		M_StartHit(monster, player, mdam);

	return mdam;
}

int GetMinHit()
{
	switch (currlevel) {
	case 16:
		return 30;
	case 15:
		return 25;
	case 14:
		return 20;
	default:
		return 15;
	}
}

void MonsterAttackPlayer(Monster &monster, Player &player, int hit, int minDam, int maxDam)
{
	if (player._pHitPoints >> 6 <= 0 || player._pInvincible || HasAnyOf(player._pSpellFlags, SpellFlag::Etherealize))
		return;
	if (oracool::IsMonsterConverted(monster))
		return; // on the Paladin's side: a blow already under way lands on nobody
	if (monster.position.tile.WalkingDistance(player.position.tile) >= 2)
		return;

	// Oracool, Round 6: what a cry or a song has done to this monster - Battle Cry and Dirge of Dread
	// blunt the blow, Weaken the aim.
	if (const int weakened = oracool::MonsterDebuffDamagePercent(monster) + oracool::Rfa12MonsterDamagePercent(monster) + oracool::PassiveMonsterDamagePercent(monster); weakened != 0) {
		minDam += minDam * weakened / 100;
		maxDam = std::max(maxDam + maxDam * weakened / 100, minDam);
	}
	// The snakes' charge passes 500, a blow that cannot miss (MissToMonst): it is not the monster's aim, and noting it pinned
	// the hero sheet's Armor class bar at 100% until another monster landed one (audit, 2026-09-27).
	const bool unmissable = hit >= 500;
	hit -= oracool::MonsterDebuffToHit(monster);

	int hper = GenerateRnd(100);
#ifdef _DEBUG
	if (DebugGodMode)
		hper = 1000;
#endif
	int ac = player.GetArmor();
	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::ACAgainstDemons) && monster.data().monsterClass == MonsterClass::Demon)
		ac += 40;
	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::ACAgainstUndead) && monster.data().monsterClass == MonsterClass::Undead)
		ac += 20;
	const int monsterToHit = hit; // before the level and armour terms - the hero sheet's odds bar keeps it (combat_odds.h)
	hit += 2 * (monster.level(sgGameInitInfo.nDifficulty) - player._pLevel)
	    + 30
	    - ac;
	int minhit = GetMinHit();
	hit = std::max(hit, minhit);
	int blkper = 100;
	if ((player._pmode == PM_STAND || player._pmode == PM_ATTACK) && player._pBlockFlag) {
		blkper = GenerateRnd(100);
	}
	int blk = player.GetBlockChance() + oracool::Rfa12BlockBonus(player) + oracool::PassiveBlockBonus(player) - (monster.level(sgGameInitInfo.nDifficulty) * 2);
	blk = clamp(blk, 0, 100);
	if (hper >= hit)
		return;
	// Oracool, Round 5: Dodge standing, Evade moving - a blow that would have landed slips instead.
	if (oracool::PassiveEvadesMelee(player) || oracool::Rfa12EvadesMelee(player))
		return;
	if (blkper < blk) {
		Direction dir = GetDirection(player.position.tile, monster.position.tile);
		StartPlrBlock(player, dir);
		if (&player == MyPlayer && player.wReflections > 0) {
			int dam = GenerateRnd(((maxDam - minDam) << 6) + 1) + (minDam << 6);
			dam = std::max(dam + (player._pIGetHit << 6), 64);
			CheckReflect(monster, player, dam);
		}
		return;
	}
	// The blow has landed: this monster is now the one the sheet's Armor class bar measures against (2026-09-27).
	if (!unmissable)
		oracool::NoteMonsterHitPlayer(player, monster, monsterToHit, minhit);
	if (monster.type().type == MT_YZOMBIE && &player == MyPlayer) {
		if (player._pMaxHP > 64) {
			if (player._pMaxHPBase > 64) {
				player._pMaxHP -= 64;
				if (player._pHitPoints > player._pMaxHP) {
					player._pHitPoints = player._pMaxHP;
				}
				player._pMaxHPBase -= 64;
				if (player._pHPBase > player._pMaxHPBase) {
					player._pHPBase = player._pMaxHPBase;
				}
			}
		}
	}
	int dam = (minDam << 6) + GenerateRnd(((maxDam - minDam) << 6) + 1);
	dam = std::max(dam + (player._pIGetHit << 6), 64);
	if (&player == MyPlayer) {
		if (player.wReflections > 0) {
			int reflectedDamage = CheckReflect(monster, player, dam);
			dam = std::max(dam - reflectedDamage, 0);
		}
		// GetMonsterDisplayName, not name() (self-audit, 2026-08-15): dying to Warded Malgrith must
		// not report a death at the hands of the champion whose sprite it borrowed. Same authority
		// the health bar and the kill log use, for the same reason.
		oracool::NotePendingDeathSource(oracool::GetMonsterDisplayName(monster));
		// The Searing and Voltaic variants (2026-09-19): a third of the blow is dealt as fire or
		// lightning and meets the player's resistance to it; the rest stays physical. The total at
		// zero resistance is the same blow - the split never adds. ApplyPlrDamage applies no
		// resistance itself (the missiles do that before calling it), so it is done here.
		// ONE ApplyPlrDamage call (audit, 2026-09-19): two calls let the physical part kill, a
		// cheat-death passive restore, and the elemental part kill again - and rang every
		// on-damaged passive twice. The element only decides how much of the third survives.
		// COLD joins them (2026-09-26): the Glacial variant's third, and a quarter of a rift guardian's or an
		// endgame boss's blow (oracool::MonsterColdMeleePercent). A cold part that lands chills, as in Diablo II.
		const DamageType variantElement = oracool::VariantHitElement(monster);
		const int bossCold = oracool::MonsterColdMeleePercent(monster);
		if (variantElement != DamageType::Physical || bossCold > 0) {
			const DamageType element = variantElement != DamageType::Physical ? variantElement : DamageType::Cold;
			const int elemental = variantElement != DamageType::Physical ? dam / 3 : dam * bossCold / 100;
			const int8_t raw = element == DamageType::Fire ? player._pFireResist
			    : element == DamageType::Lightning         ? player._pLghtResist
			                                               : player._pColdResist;
			// The hero's own resistance as it stands - already capped and floored by the curve - as the missiles apply it
			// (audit, 2026-09-27): clamped to 0-75 here, a -60% fire resistance on Hell took a Searing third at 100% where
			// the sheet (and a fireball) said 160%, and 85% cold resisted only 75.
			const int resist = std::clamp<int>(raw, -100, 100);
			int resisted = elemental * (100 - resist) / 100;
			// The ELEMENT's extra reductions (Sixth Sense, Vigilant, Battle Hardened) on the elemental part only: tagging the
			// whole blow with the element let them cut the physical two-thirds (or three-quarters) too (round 10 audit,
			// v1.12.235). The blow itself goes in as Physical, whose reductions are the ones every part shares.
			const int extraPercent = oracool::PassiveDamageTakenPercent(player, element) + oracool::Rfa12DamageTakenPercent(player, element)
			    - (oracool::PassiveDamageTakenPercent(player, DamageType::Physical) + oracool::Rfa12DamageTakenPercent(player, DamageType::Physical));
			resisted = std::max(resisted + resisted * extraPercent / 100, 0);
			ApplyPlrDamage(DamageType::Physical, player, 0, 0, dam - elemental + resisted);
			if (element == DamageType::Cold)
				oracool::ChillPlayer(player);
		} else {
			ApplyPlrDamage(DamageType::Physical, player, 0, 0, dam);
		}
		// The Venomous variant: a bleed after the bite - as much again as the blow, over five seconds.
		if (oracool::VariantPoisonsOnHit(monster))
			oracool::PoisonPlayer(player, dam, 100);
		if ((player._pHitPoints >> 6) > 0) // not from the corpse (round 38 audit: a guardian died to a dead hero's Retribution)
			oracool::OnRfa12Struck(player, monster); // Retaliation's stack, Unfinished Business's memory
		// Oracool: the one seam where "this monster wounded the player, for this much" is known, which
		// is what a Vampiric champion needs. After the reflect subtraction, so it drains what it
		// actually landed rather than what it swung for.
		//
		// Guarded on death mode, and the guard is not hypothetical: CheckReflect above can KILL the
		// monster with the damage it just reflected. Without this, a Vampiric champion or a
		// Devouring boss that died to its own reflected blow drained life afterwards and ended up
		// in MonsterMode::Death with positive hit points - a corpse the health bar says is alive.
		// The Thorns block immediately below has carried exactly this guard, for exactly this
		// reason, the whole time (audit, 2026-08-26).
		if (monster.mode != MonsterMode::Death) {
			oracool::OnCursedMonsterDealtBlow(monster, dam); // Iron Maiden (oracool/curses.h)
		}
		if (monster.mode != MonsterMode::Death) {
			// What the hero's life actually lost (round 44 audit): the blow before resistance, Mana Shield and the damage-taken
			// passives healed a Vampiric champion through a shield that took it all.
			const int landed = LastPlayerLifeLost;
			oracool::OnLesserUniqueDealtDamage(monster, landed);
			// And the boss's own drain, which is a different trait on a different field - a boss's
			// lesserAffix is Dread, so OnLesserUniqueDealtDamage's Vampiric test never fires for one.
			oracool::OnBossDealtDamage(monster, landed);
		}
	}

	// Reflect can also kill a monster, so make sure the monster is still alive
	// Oracool (2026-09-12): the Paladin's Thorns returns a share of the blow - 25%, +10% a level - on top
	// of the items' flat 1-3.
	const int thornsPercent = oracool::ThornsReturnPercent(player);
	if ((HasAnyOf(player._pIFlags, ItemSpecialEffect::Thorns) || thornsPercent > 0) && monster.mode != MonsterMode::Death
	    && (player._pHitPoints >> 6) > 0) { // a dead hero returns nothing (round 38 audit)
		int mdam = HasAnyOf(player._pIFlags, ItemSpecialEffect::Thorns) ? (GenerateRnd(3) + 1) << 6 : 0;
		mdam += dam * thornsPercent / 100;
		ApplyMonsterDamage(DamageType::Physical, monster, mdam);
		if (monster.hitPoints >> 6 <= 0)
			M_StartKill(monster, player);
		else
			M_StartHit(monster, player, mdam);
	}

	// The cold armours' answer to a blow that landed (Oracool, Round 2): Frozen freezes, Shiver
	// chills and cuts, Chilling chills and shoots back. After Thorns and under the same guard,
	// because Thorns can have killed the attacker a line ago.
	if (monster.mode != MonsterMode::Death && (player._pHitPoints >> 6) > 0)
		oracool::OnColdArmourStruckInMelee(player, monster);

	if ((monster.flags & MFLAG_NOLIFESTEAL) == 0 && monster.type().type == MT_SKING && gbIsMultiplayer)
		monster.hitPoints += dam;
	if (player._pHitPoints >> 6 <= 0) {
		// Not if the blow's reflect, thorns, Iron Maiden or Shiver Armor killed it too: stood back up at 0 life it could
		// neither be targeted nor die again (round 3 audit, v1.12.228).
		if (gbIsHellfire && monster.mode != MonsterMode::Death)
			M_StartStand(monster, monster.direction);
		return;
	}
	StartPlrHit(player, dam, false);
	if ((monster.flags & MFLAG_KNOCKBACK) != 0 && !oracool::PlayerIgnoresKnockback(player)) {
		if (player._pmode != PM_GOTHIT)
			StartPlrHit(player, 0, true);

		Point newPosition = player.position.tile + monster.direction;
		oracool::CompanionsMakeWay(player, newPosition); // never onto his own skeleton's tile (round 10 audit)
		if (PosOkPlayer(player, newPosition)) {
			player.position.tile = newPosition;
			FixPlayerLocation(player, player._pdir);
			FixPlrWalkTags(player);
			dPlayer[newPosition.x][newPosition.y] = player.getId() + 1;
			SetPlayerOld(player);
		}
	}
}

void MonsterAttackEnemy(Monster &monster, int hit, int minDam, int maxDam)
{
	// Within reach, as a blow at the hero is: a companion regrouped or a minion that traded places took the blow from
	// across the map (round 15 audit, v1.12.240).
	if ((monster.flags & MFLAG_TARGETS_MONSTER) != 0) {
		if (monster.position.tile.WalkingDistance(Monsters[monster.enemy].position.tile) < 2)
			MonsterAttackMonster(monster, Monsters[monster.enemy], hit, minDam, maxDam);
	} else
		MonsterAttackPlayer(monster, Players[monster.enemy], hit, minDam, maxDam);
}

bool CompanionMeleeAttack(Monster &companion); // defined beside CompanionRangedAttack, below

bool MonsterAttack(Monster &monster)
{
	if (oracool::IsCompanion(monster))
		return CompanionMeleeAttack(monster);
	if (monster.animInfo.currentFrame == monster.data().animFrameNum - 1) {
		// Phase 3.4: a champion's Might reaches its pack here, at the one place an ordinary
		// monster's own damage is read for a swing. Queried, never written - see oracool/aura_field.h.
		// A minion's blow carries Frenzy of the Dead (oracool/minions.h); 100% for everyone else.
		const int minionPercent = oracool::MinionDamagePercent(monster);
		MonsterAttackEnemy(monster, monster.toHit(sgGameInitInfo.nDifficulty),
		    oracool::PackAdjustedDamage(monster, monster.minDamage) * minionPercent / 100,
		    oracool::PackAdjustedDamage(monster, monster.maxDamage) * minionPercent / 100);
		if (monster.ai != MonsterAIID::Snake)
			PlayEffect(monster, MonsterSound::Attack);
	}
	if (IsAnyOf(monster.type().type, MT_NMAGMA, MT_YMAGMA, MT_BMAGMA, MT_WMAGMA) && monster.animInfo.currentFrame == 8) {
		const int minionPercent = oracool::MinionDamagePercent(monster); // as the first blow (round 28 audit)
		MonsterAttackEnemy(monster, monster.toHit(sgGameInitInfo.nDifficulty) + 10,
		    std::max(oracool::PackAdjustedDamage(monster, monster.minDamage) * minionPercent / 100 - 2, 0),
		    std::max(oracool::PackAdjustedDamage(monster, monster.maxDamage) * minionPercent / 100 - 2, 0));

		PlayEffect(monster, MonsterSound::Attack);
	}
	if (IsAnyOf(monster.type().type, MT_STORM, MT_RSTORM, MT_STORML, MT_MAEL) && monster.animInfo.currentFrame == 12) {
		const int minionPercent = oracool::MinionDamagePercent(monster); // as the first blow (round 28 audit)
		MonsterAttackEnemy(monster, monster.toHit(sgGameInitInfo.nDifficulty) - 20,
		    oracool::PackAdjustedDamage(monster, monster.minDamage) * minionPercent / 100 + 4,
		    oracool::PackAdjustedDamage(monster, monster.maxDamage) * minionPercent / 100 + 4);

		PlayEffect(monster, MonsterSound::Attack);
	}
	if (monster.ai == MonsterAIID::Snake && monster.animInfo.currentFrame == 0)
		PlayEffect(monster, MonsterSound::Attack);
	if (monster.animInfo.isLastFrame()) {
		M_StartStand(monster, monster.direction);
		return true;
	}

	return false;
}

/**
 * @brief A companion's shot: the bow drawn on its own sheet, the arrows loosed at the sheet's release frame by
 * CompanionShot - its owner's arrows, at the companion's share of the owner's damage.
 */
bool CompanionRangedAttack(Monster &companion)
{
	const int release = std::min(oracool::CompanionActionFrame(companion), std::max(companion.animInfo.numberOfFrames - 1, 0));
	if (companion.animInfo.currentFrame == release)
		oracool::CompanionShot(companion);
	if (companion.animInfo.isLastFrame()) {
		M_StartStand(companion, companion.direction);
		return true;
	}
	return false;
}

/** @brief A companion's swing: the blow lands at its sheet's action frame, as its owner's (CompanionMeleeHit). */
bool CompanionMeleeAttack(Monster &companion)
{
	const int strike = std::min(oracool::CompanionActionFrame(companion), std::max(companion.animInfo.numberOfFrames - 1, 0));
	if (companion.animInfo.currentFrame == strike)
		oracool::CompanionMeleeHit(companion);
	if (companion.animInfo.isLastFrame()) {
		M_StartStand(companion, companion.direction);
		return true;
	}
	return false;
}

bool MonsterRangedAttack(Monster &monster)
{
	if (oracool::IsCompanion(monster))
		return CompanionRangedAttack(monster);
	if (monster.animInfo.currentFrame == monster.data().animFrameNum - 1) {
		const auto &missileType = static_cast<MissileID>(monster.var1);
		if (missileType != MissileID::Null) {
			// Oracool (2026-09-26): a Skeletal Mage's shot is ONE missile at its own damage, whatever its element.
			// Firebolt, Acid and Arrow already roll the shooter's min-max; a monster's Charged Bolt is three bolts of a
			// flat 15, so a mage's single bolt is given the same min-max roll the other three elements make.
			const bool minionShot = oracool::IsMinion(monster);
			int multimissiles = 1;
			if (missileType == MissileID::ChargedBolt && !minionShot)
				multimissiles = 3;
			for (int mi = 0; mi < multimissiles; mi++) {
				Missile *shot = AddMissile(
				    monster.position.tile,
				    monster.enemyPosition,
				    monster.direction,
				    missileType,
				    TARGET_PLAYERS,
				    monster.getId(),
				    monster.var2,
				    0);
				if (shot == nullptr || !minionShot)
					continue;
				if (missileType == MissileID::ChargedBolt)
					shot->_midam = monster.minDamage + GenerateRnd(monster.maxDamage - monster.minDamage + 1);
				// The bone mage's arrow wears the Necromancer's bone tooth (sixteen facings) once that sheet is in.
				if (missileType == MissileID::Arrow && MissileArtLoaded(MissileGraphicID::BoneTooth)) {
					shot->_miAnimType = MissileGraphicID::BoneTooth;
					SetMissDir(*shot, GetDirection16(monster.position.tile, monster.enemyPosition));
				}
			}
		}
		PlayEffect(monster, MonsterSound::Attack);
	}

	if (monster.animInfo.isLastFrame()) {
		M_StartStand(monster, monster.direction);
		return true;
	}

	return false;
}

bool MonsterRangedSpecialAttack(Monster &monster)
{
	if (monster.animInfo.currentFrame == monster.data().animFrameNumSpecial - 1 && monster.animInfo.tickCounterOfCurrentFrame == 0 && (monster.ai != MonsterAIID::Mega || monster.var2 == 0)) {
		if (AddMissile(
		        monster.position.tile,
		        monster.enemyPosition,
		        monster.direction,
		        static_cast<MissileID>(monster.var1),
		        TARGET_PLAYERS,
		        monster.getId(),
		        monster.var3,
		        0)
		    != nullptr) {
			PlayEffect(monster, MonsterSound::Special);
		}
	}

	if (monster.ai == MonsterAIID::Mega && monster.animInfo.currentFrame == monster.data().animFrameNumSpecial - 1) {
		if (monster.var2++ == 0) {
			monster.flags |= MFLAG_ALLOW_SPECIAL;
		} else if (monster.var2 == 15) {
			monster.flags &= ~MFLAG_ALLOW_SPECIAL;
		}
	}

	if (monster.animInfo.isLastFrame()) {
		M_StartStand(monster, monster.direction);
		return true;
	}

	return false;
}

bool MonsterSpecialAttack(Monster &monster)
{
	if (monster.animInfo.currentFrame == monster.data().animFrameNumSpecial - 1) {
		// Might reaches the special too (round 11 audit, v1.12.236), as it does the plain blow.
		MonsterAttackEnemy(monster, monster.toHitSpecial(sgGameInitInfo.nDifficulty), oracool::PackAdjustedDamage(monster, monster.minDamageSpecial), oracool::PackAdjustedDamage(monster, monster.maxDamageSpecial));
	}

	if (monster.animInfo.isLastFrame()) {
		M_StartStand(monster, monster.direction);
		return true;
	}

	return false;
}

bool MonsterFadein(Monster &monster)
{
	if (((monster.flags & MFLAG_LOCK_ANIMATION) == 0 || monster.animInfo.currentFrame != 0)
	    && ((monster.flags & MFLAG_LOCK_ANIMATION) != 0 || monster.animInfo.currentFrame != monster.animInfo.numberOfFrames - 1)) {
		return false;
	}

	M_StartStand(monster, monster.direction);
	monster.flags &= ~MFLAG_LOCK_ANIMATION;

	return true;
}

bool MonsterFadeout(Monster &monster)
{
	if (((monster.flags & MFLAG_LOCK_ANIMATION) == 0 || monster.animInfo.currentFrame != 0)
	    && ((monster.flags & MFLAG_LOCK_ANIMATION) != 0 || monster.animInfo.currentFrame != monster.animInfo.numberOfFrames - 1)) {
		return false;
	}

	monster.flags &= ~MFLAG_LOCK_ANIMATION;
	monster.flags |= MFLAG_HIDDEN;

	M_StartStand(monster, monster.direction);

	return true;
}

/**
 * @brief Applies the healing effect on the monster.
 *
 * This is triggered by StartHeal()
 *
 * @param monster The monster that will be healed.
 * @return
 */
void MonsterHeal(Monster &monster)
{
	if (monster.animInfo.currentFrame == 0) {
		monster.flags &= ~MFLAG_LOCK_ANIMATION;
		monster.flags |= MFLAG_ALLOW_SPECIAL;
		if (monster.var1 + monster.hitPoints < monster.maxHitPoints) {
			monster.hitPoints = monster.var1 + monster.hitPoints;
		} else {
			monster.hitPoints = monster.maxHitPoints;
			monster.flags &= ~MFLAG_ALLOW_SPECIAL;
			monster.mode = MonsterMode::SpecialMeleeAttack;
		}
	}
}

void MonsterTalk(Monster &monster)
{
	M_StartStand(monster, monster.direction);
	monster.goal = MonsterGoal::Talking;
	if (effect_is_playing(Speeches[monster.talkMsg].sfxnr))
		return;
	InitQTextMsg(monster.talkMsg);
	if (monster.uniqueType == UniqueMonsterType::SnotSpill) {
		if (monster.talkMsg == TEXT_BANNER10 && (monster.flags & MFLAG_QUEST_COMPLETE) == 0) {
			ObjChangeMap(SetPiece.position.x, SetPiece.position.y, SetPiece.position.x + (SetPiece.size.width / 2) + 2, SetPiece.position.y + (SetPiece.size.height / 2) - 2);
			auto tren = TransVal;
			TransVal = 9;
			DRLG_MRectTrans({ SetPiece.position, WorldTileSize(SetPiece.size.width / 2 + 4, SetPiece.size.height / 2) });
			TransVal = tren;
			Quests[Q_LTBANNER]._qvar1 = 2;
			if (Quests[Q_LTBANNER]._qactive == QUEST_INIT)
				Quests[Q_LTBANNER]._qactive = QUEST_ACTIVE;
			monster.flags |= MFLAG_QUEST_COMPLETE;
			NetSendCmdQuest(true, Quests[Q_LTBANNER]);
		}
		if (Quests[Q_LTBANNER]._qvar1 < 2) {
			app_fatal(StrCat("SS Talk = ", monster.talkMsg, ", Flags = ", monster.flags));
		}
	}
	if (monster.uniqueType == UniqueMonsterType::Lachdan) {
		if (monster.talkMsg == TEXT_VEIL9) {
			Quests[Q_VEIL]._qactive = QUEST_ACTIVE;
			Quests[Q_VEIL]._qlog = true;
			NetSendCmdQuest(true, Quests[Q_VEIL]);
		}
	}
	if (monster.uniqueType == UniqueMonsterType::WarlordOfBlood) {
		Quests[Q_WARLORD]._qvar1 = QS_WARLORD_TALKING;
		NetSendCmdQuest(true, Quests[Q_WARLORD]);
	}
	if (monster.uniqueType == UniqueMonsterType::Lazarus && UseMultiplayerQuests()) {
		Quests[Q_BETRAYER]._qvar1 = 6;
		monster.goal = MonsterGoal::Normal;
		monster.activeForTicks = UINT8_MAX;
		monster.talkMsg = TEXT_NONE;
	}
}

bool MonsterGotHit(Monster &monster)
{
	if (monster.animInfo.isLastFrame()) {
		M_StartStand(monster, monster.direction);

		return true;
	}

	return false;
}

void ReleaseMinions(const Monster &leader)
{
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		auto &minion = Monsters[ActiveMonsters[i]];
		// Every follower, not only the leashed: a Separated one was re-leashed later by GroupUnity to the dead leader's
		// slot - and to whatever took it (round 6 audit).
		if (minion.leaderRelation != LeaderRelation::None && minion.getLeader() == &leader) {
			minion.setLeader(nullptr);
		}
	}
}

void ShrinkLeaderPacksize(const Monster &monster)
{
	if (monster.leaderRelation == LeaderRelation::Leashed) {
		monster.getLeader()->packSize--;
	}
}

void MonsterDeath(Monster &monster)
{
	monster.var1++;
	// A rift's Diablo dies as any monster does: no camera pan, no ending (oracool/rift.h).
	if (monster.type().type == MT_DIABLO && !oracool::InRift()) {
		if (monster.position.tile.x < ViewPosition.x) {
			ViewPosition.x--;
		} else if (monster.position.tile.x > ViewPosition.x) {
			ViewPosition.x++;
		}

		if (monster.position.tile.y < ViewPosition.y) {
			ViewPosition.y--;
		} else if (monster.position.tile.y > ViewPosition.y) {
			ViewPosition.y++;
		}

		if (monster.var1 == 140)
			PrepDoEnding();
	} else if (monster.animInfo.isLastFrame()) {
		if (oracool::TitheTakesCorpse(monster)) {
			// Tithe of Ash (RfA-12) took the corpse: nothing is left to raise or search.
		} else if (oracool::IsCompanion(monster)) {
			// A fallen companion leaves no body: it wore the Golem's type, and the Golem's rubble lay where it fell (round 31).
		} else if (monster.isUnique() && monster.corpseId != 0) {
			// The body keeps the look it died with: this slot is freed next and the next summon takes it (dead.h).
			Corpse &corpse = Corpses[monster.corpseId - 1];
			corpse.laidSprites = oracool::GetScaledCorpse(monster);
			corpse.hasLaidTrn = monster.uniqueMonsterTRN != nullptr;
			if (corpse.hasLaidTrn)
				std::copy_n(monster.uniqueMonsterTRN.get(), corpse.laidTrn.size(), corpse.laidTrn.begin());
			corpse.laid = true;
			AddCorpse(monster.position.tile, monster.corpseId, monster.direction);
			oracool::RecordCorpse(monster); // what it was, for the Necromancer (oracool/corpses.h)
		} else if (monster.type().corpseId != 0) {
			// A type loaded mid-floor (a raised skeleton, a mage) has no body sheet: id 0 with the direction bits wrote a
			// non-zero tile the draw read as Corpses[-1] (round 14 audit, v1.12.239).
			AddCorpse(monster.position.tile, monster.type().corpseId, monster.direction);
			oracool::RecordCorpse(monster);
		}

		dMonster[monster.position.tile.x][monster.position.tile.y] = 0;
		monster.isInvalid = true;

		M_UpdateRelations(monster);
	}
}

bool MonsterSpecialStand(Monster &monster)
{
	if (monster.animInfo.currentFrame == monster.data().animFrameNumSpecial - 1)
		PlayEffect(monster, MonsterSound::Special);

	if (monster.animInfo.isLastFrame()) {
		M_StartStand(monster, monster.direction);
		return true;
	}

	return false;
}

bool MonsterDelay(Monster &monster)
{
	monster.changeAnimationData(MonsterGraphic::Stand, GetMonsterDirection(monster));
	if (monster.ai == MonsterAIID::Lazarus) {
		if (monster.var2 > 8 || monster.var2 < 0)
			monster.var2 = 8;
	}

	if (monster.var2-- == 0) {
		int oFrame = monster.animInfo.currentFrame;
		M_StartStand(monster, monster.direction);
		monster.animInfo.currentFrame = oFrame;
		return true;
	}

	return false;
}

void MonsterPetrified(Monster &monster)
{
	if (monster.hitPoints <= 0) {
		// Its pack learns it is gone, as the death animation's end tells it: a follower shattered in stone left its
		// leader's packSize one too high, and DirOK never let that unique walk again; a leader shattered left its pack
		// tied to the slot the next summon takes (round 6 audit, v1.12.231).
		M_UpdateRelations(monster);
		dMonster[monster.position.tile.x][monster.position.tile.y] = 0;
		monster.isInvalid = true;
	}
}

Monster *AddSkeleton(Point position, Direction dir, bool inMap)
{
	size_t typeCount = 0;
	size_t skeletonIndexes[SkeletonTypes.size()];
	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (IsSkel(LevelMonsterTypes[i].type) && !LevelMonsterTypes[i].minionOnly) {
			// A rift's sarcophagi and barrels hold the floor's own skeletons, not the low-floor type a Skeleton King rift loads
			// for the King alone to raise (round 48 audit: floor-1 skeletons at any rung, cheap credit for the bar).
			if (!inMap && oracool::InRift() && (LevelMonsterTypes[i].placeFlags & PLACE_SCATTER) == 0)
				continue;
			skeletonIndexes[typeCount++] = i;
		}
	}

	if (typeCount == 0) {
		return nullptr;
	}

	const size_t typeIndex = skeletonIndexes[GenerateRnd(typeCount)];
	return AddMonster(position, dir, typeIndex, inMap);
}

void SpawnSkeleton(Point position, Direction dir)
{
	Monster *skeleton = AddSkeleton(position, dir, true);
	if (skeleton != nullptr && oracool::InRift())
		oracool::ScaleRiftMonster(*skeleton); // a rift Leoric's raised dead fight at the rift's scale (round 21 audit)
	if (skeleton != nullptr)
		StartSpecialStand(*skeleton, dir);
}

bool IsLineNotSolid(Point startPoint, Point endPoint)
{
	return LineClear(IsTileNotSolid, startPoint, endPoint);
}

void FollowTheLeader(Monster &monster)
{
	if (monster.leaderRelation != LeaderRelation::Leashed)
		return;

	Monster *leader = monster.getLeader();
	if (leader == nullptr)
		return;

	if (leader->activeForTicks > monster.activeForTicks) {
		monster.position.last = leader->position.tile;
		monster.activeForTicks = leader->activeForTicks - 1;
	}

	if (monster.ai != MonsterAIID::Gargoyle || (monster.flags & MFLAG_ALLOW_SPECIAL) == 0)
		return;
	if (leader->mode == MonsterMode::SpecialMeleeAttack)
		return;
	monster.flags &= ~MFLAG_ALLOW_SPECIAL;
	monster.mode = MonsterMode::SpecialMeleeAttack;
}

void GroupUnity(Monster &monster)
{
	if (monster.leaderRelation == LeaderRelation::None)
		return;

	// No unique monster would be a minion of someone else!
	assert(!monster.isUnique());

	// Someone with a leaderRelation should have a leader, if we end up trying to access a nullptr then the relation was already broken...

	auto &leader = *monster.getLeader();
	if (IsLineNotSolid(monster.position.tile, leader.position.future)) {
		if (monster.leaderRelation == LeaderRelation::Separated
		    && monster.position.tile.WalkingDistance(leader.position.future) < 4) {
			// Reunite the separated monster with the pack
			leader.packSize++;
			monster.leaderRelation = LeaderRelation::Leashed;
		}
	} else if (monster.leaderRelation == LeaderRelation::Leashed) {
		leader.packSize--;
		monster.leaderRelation = LeaderRelation::Separated;
	}

	if (monster.leaderRelation == LeaderRelation::Leashed) {
		if (monster.activeForTicks > leader.activeForTicks) {
			leader.position.last = monster.position.tile;
			leader.activeForTicks = monster.activeForTicks - 1;
		}
		if (leader.ai == MonsterAIID::Gargoyle && (leader.flags & MFLAG_ALLOW_SPECIAL) != 0) {
			leader.flags &= ~MFLAG_ALLOW_SPECIAL;
			leader.mode = MonsterMode::SpecialMeleeAttack;
		}
	}
}

bool RandomWalk(Monster &monster, Direction md)
{
	Direction mdtemp = md;

	bool ok = DirOK(monster, md);
	if (FlipCoin())
		ok = ok || (md = Right(mdtemp), DirOK(monster, md)) || (md = Left(mdtemp), DirOK(monster, md));
	else
		ok = ok || (md = Left(mdtemp), DirOK(monster, md)) || (md = Right(mdtemp), DirOK(monster, md));
	if (FlipCoin()) {
		ok = ok
		    || (md = Left(Left(mdtemp)), DirOK(monster, md))
		    || (md = Right(Right(mdtemp)), DirOK(monster, md));
	} else {
		ok = ok
		    || (md = Right(Right(mdtemp)), DirOK(monster, md))
		    || (md = Left(Left(mdtemp)), DirOK(monster, md));
	}
	if (ok)
		Walk(monster, md);
	return ok;
}

bool RandomWalk2(Monster &monster, Direction md)
{
	Direction mdtemp = md;
	bool ok = DirOK(monster, md); // Can we continue in the same direction

	// Randomly go left or right
	if (FlipCoin()) {
		ok = ok || (mdtemp = Right(md), DirOK(monster, Right(md))) || (mdtemp = Left(md), DirOK(monster, Left(md)));
	} else {
		ok = ok || (mdtemp = Left(md), DirOK(monster, Left(md))) || (mdtemp = Right(md), DirOK(monster, Right(md)));
	}

	if (ok)
		Walk(monster, mdtemp);

	return ok;
}

/**
 * @brief Check if a tile is affected by a spell we are vunerable to
 */
bool IsTileSafe(const Monster &monster, Point position)
{
	if (!InDungeonBounds(position))
		return false;

	const bool fearsFire = (monster.resistance & IMMUNE_FIRE) == 0 || monster.type().type == MT_DIABLO;
	const bool fearsLightning = (monster.resistance & IMMUNE_LIGHTNING) == 0 || monster.type().type == MT_DIABLO;

	return !(fearsFire && HasAnyOf(dFlags[position.x][position.y], DungeonFlag::MissileFireWall))
	    && !(fearsLightning && HasAnyOf(dFlags[position.x][position.y], DungeonFlag::MissileLightningWall));
}

/**
 * @brief Check that the given tile is not currently blocked
 */
bool IsTileAvailable(Point position)
{
	if (dPlayer[position.x][position.y] != 0 || dMonster[position.x][position.y] != 0)
		return false;

	if (!IsTileWalkable(position))
		return false;

	return true;
}

/**
 * @brief If a monster can access the given tile (possibly by opening a door)
 */
bool IsTileAccessible(const Monster &monster, Point position)
{
	if (dPlayer[position.x][position.y] != 0 || dMonster[position.x][position.y] != 0)
		return false;

	if (!IsTileWalkable(position, (monster.flags & MFLAG_CAN_OPEN_DOOR) != 0))
		return false;

	return IsTileSafe(monster, position);
}

bool AiPlanWalk(Monster &monster)
{
	int8_t path[MaxPathLength];

	/** Maps from walking path step to facing direction. */
	const Direction plr2monst[9] = { Direction::South, Direction::NorthEast, Direction::NorthWest, Direction::SouthEast, Direction::SouthWest, Direction::North, Direction::East, Direction::South, Direction::West };

	if (FindPath([&monster](Point position) { return IsTileAccessible(monster, position); }, monster.position.tile, monster.enemyPosition, path) == 0) {
		return false;
	}

	RandomWalk(monster, plr2monst[path[0]]);
	return true;
}

Direction Turn(Direction direction, bool turnLeft)
{
	return turnLeft ? Left(direction) : Right(direction);
}

bool RoundWalk(Monster &monster, Direction direction, int8_t *dir)
{
	Direction turn45deg = Turn(direction, *dir != 0);
	Direction turn90deg = Turn(turn45deg, *dir != 0);

	// Turn 90 degrees
	if (Walk(monster, turn90deg)) {
		return true;
	}

	// Only do a small turn
	if (Walk(monster, turn45deg)) {
		return true;
	}

	// Continue straight
	if (Walk(monster, direction)) {
		return true;
	}

	// Try 90 degrees in the opposite than desired direction
	*dir = (*dir == 0) ? 1 : 0;
	return RandomWalk(monster, Opposite(turn90deg));
}

bool AiPlanPath(Monster &monster)
{
	if (monster.type().type != MT_GOLEM) {
		if (monster.activeForTicks == 0)
			return false;
		if (monster.mode != MonsterMode::Stand)
			return false;
		if (IsNoneOf(monster.goal, MonsterGoal::Normal, MonsterGoal::Move, MonsterGoal::Attack))
			return false;
		if (monster.position.tile == GolemHoldingCell)
			return false;
	}

	bool clear = LineClear(
	    [&monster](Point position) { return IsTileAvailable(monster, position); },
	    monster.position.tile,
	    monster.enemyPosition);
	if (!clear || (monster.pathCount >= 5 && monster.pathCount < 8)) {
		if ((monster.flags & MFLAG_CAN_OPEN_DOOR) != 0)
			MonstCheckDoors(monster);
		monster.pathCount++;
		if (monster.pathCount < 5)
			return false;
		if (AiPlanWalk(monster))
			return true;
	}

	if (monster.type().type != MT_GOLEM)
		monster.pathCount = 0;

	return false;
}

void AiAvoidance(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (monster.activeForTicks < UINT8_MAX)
		MonstCheckDoors(monster);
	int v = GenerateRnd(100);
	unsigned distanceToEnemy = monster.distanceToEnemy();
	if (distanceToEnemy >= 2 && monster.activeForTicks == UINT8_MAX && dTransVal[monster.position.tile.x][monster.position.tile.y] == dTransVal[monster.enemyPosition.x][monster.enemyPosition.y]) {
		if (monster.goal == MonsterGoal::Move || (distanceToEnemy >= 4 && FlipCoin(4))) {
			if (monster.goal != MonsterGoal::Move) {
				monster.goalVar1 = 0;
				monster.goalVar2 = GenerateRnd(2);
			}
			monster.goal = MonsterGoal::Move;
			if ((monster.goalVar1++ >= static_cast<int>(2 * distanceToEnemy) && DirOK(monster, md)) || dTransVal[monster.position.tile.x][monster.position.tile.y] != dTransVal[monster.enemyPosition.x][monster.enemyPosition.y]) {
				monster.goal = MonsterGoal::Normal;
			} else if (!RoundWalk(monster, md, &monster.goalVar2)) {
				AiDelay(monster, GenerateRnd(10) + 10);
			}
		}
	} else {
		monster.goal = MonsterGoal::Normal;
	}
	if (monster.goal == MonsterGoal::Normal) {
		if (distanceToEnemy >= 2) {
			if ((monster.var2 > 20 && v < 2 * monster.intelligence + 28)
			    || (IsMonsterModeMove(static_cast<MonsterMode>(monster.var1))
			        && monster.var2 == 0
			        && v < 2 * monster.intelligence + 78)) {
				RandomWalk(monster, md);
			}
		} else if (v < 2 * monster.intelligence + 23) {
			monster.direction = md;
			if (IsAnyOf(monster.ai, MonsterAIID::GoatMelee, MonsterAIID::Gharbad) && monster.hitPoints < (monster.maxHitPoints / 2) && !FlipCoin())
				StartSpecialAttack(monster);
			else
				StartAttack(monster);
		}
	}

	monster.checkStandAnimationIsLoaded(md);
}

MissileID GetMissileType(MonsterAIID ai)
{
	switch (ai) {
	case MonsterAIID::GoatMelee:
		return MissileID::Arrow;
	case MonsterAIID::Succubus:
	case MonsterAIID::LazarusSuccubus:
		return MissileID::BloodStar;
	case MonsterAIID::Acid:
	case MonsterAIID::AcidUnique:
		return MissileID::Acid;
	case MonsterAIID::FireBat:
		return MissileID::Firebolt;
	case MonsterAIID::Torchant:
		return MissileID::Fireball;
	case MonsterAIID::Lich:
		return MissileID::OrangeFlare;
	case MonsterAIID::ArchLich:
		return MissileID::YellowFlare;
	case MonsterAIID::Psychorb:
		return MissileID::BlueFlare;
	case MonsterAIID::Necromorb:
		return MissileID::RedFlare;
	case MonsterAIID::Magma:
		return MissileID::MagmaBall;
	case MonsterAIID::Storm:
		return MissileID::ThinLightningControl;
	case MonsterAIID::Diablo:
		return MissileID::DiabloApocalypse;
	case MonsterAIID::BoneDemon:
		return MissileID::BlueFlare2;
	default:
		return MissileID::Arrow;
	}
}

void AiRanged(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand) {
		return;
	}

	if (monster.activeForTicks == UINT8_MAX || (monster.flags & MFLAG_TARGETS_MONSTER) != 0) {
		Direction md = GetMonsterDirection(monster);
		if (monster.activeForTicks < UINT8_MAX)
			MonstCheckDoors(monster);
		monster.direction = md;
		if (static_cast<MonsterMode>(monster.var1) == MonsterMode::RangedAttack) {
			AiDelay(monster, GenerateRnd(20));
		} else if (monster.distanceToEnemy() < 4) {
			if (GenerateRnd(100) < 10 * (monster.intelligence + 7))
				RandomWalk(monster, Opposite(md));
		}
		if (monster.mode == MonsterMode::Stand) {
			if (LineClearMissile(monster.position.tile, monster.enemyPosition)) {
				MissileID missileType = GetMissileType(monster.ai);
				if (monster.ai == MonsterAIID::AcidUnique)
					StartRangedSpecialAttack(monster, missileType, 0);
				else
					StartRangedAttack(monster, missileType, 0);
			} else {
				monster.checkStandAnimationIsLoaded(md);
			}
		}
		return;
	}

	if (monster.activeForTicks != 0) {
		Direction md = GetDirection(monster.position.tile, monster.position.last);
		RandomWalk(monster, md);
	}
}

void AiRangedAvoidance(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (IsAnyOf(monster.ai, MonsterAIID::Magma, MonsterAIID::Storm, MonsterAIID::BoneDemon) && monster.activeForTicks < UINT8_MAX)
		MonstCheckDoors(monster);
	int lessmissiles = (monster.ai == MonsterAIID::Acid) ? 1 : 0;
	int dam = (monster.ai == MonsterAIID::Diablo) ? 40 : 0;
	MissileID missileType = GetMissileType(monster.ai);
	int v = GenerateRnd(10000);
	unsigned distanceToEnemy = monster.distanceToEnemy();
	if (distanceToEnemy >= 2 && monster.activeForTicks == UINT8_MAX && dTransVal[monster.position.tile.x][monster.position.tile.y] == dTransVal[monster.enemyPosition.x][monster.enemyPosition.y]) {
		if (monster.goal == MonsterGoal::Move || (distanceToEnemy >= 3 && FlipCoin(4 << lessmissiles))) {
			if (monster.goal != MonsterGoal::Move) {
				monster.goalVar1 = 0;
				monster.goalVar2 = GenerateRnd(2);
			}
			monster.goal = MonsterGoal::Move;
			if (monster.goalVar1++ >= static_cast<int>(2 * distanceToEnemy) && DirOK(monster, md)) {
				monster.goal = MonsterGoal::Normal;
			} else if (v < (500 * (monster.intelligence + 1) >> lessmissiles)
			    && (LineClearMissile(monster.position.tile, monster.enemyPosition))) {
				StartRangedSpecialAttack(monster, missileType, dam);
			} else {
				RoundWalk(monster, md, &monster.goalVar2);
			}
		}
	} else {
		monster.goal = MonsterGoal::Normal;
	}
	if (monster.goal == MonsterGoal::Normal) {
		if (((distanceToEnemy >= 3 && v < ((500 * (monster.intelligence + 2)) >> lessmissiles))
		        || v < ((500 * (monster.intelligence + 1)) >> lessmissiles))
		    && LineClearMissile(monster.position.tile, monster.enemyPosition)) {
			StartRangedSpecialAttack(monster, missileType, dam);
		} else if (distanceToEnemy >= 2) {
			v = GenerateRnd(100);
			if (v < 1000 * (monster.intelligence + 5)
			    || (IsMonsterModeMove(static_cast<MonsterMode>(monster.var1))
			        && monster.var2 == 0
			        && v < 1000 * (monster.intelligence + 8))) {
				RandomWalk(monster, md);
			}
		} else if (v < 1000 * (monster.intelligence + 6)) {
			monster.direction = md;
			StartAttack(monster);
		}
	}
	if (monster.mode == MonsterMode::Stand) {
		AiDelay(monster, GenerateRnd(10) + 5);
	}
}

void ZombieAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand) {
		return;
	}

	if (!IsTileVisible(monster.position.tile)) {
		return;
	}
	// Dim Vision reaches the zombies too: this AI never read activeForTicks, so a blinded one hunted as ever (round 24).
	if (oracool::CursedMonsterBlinded(monster) && monster.activeForTicks == 0)
		return;

	if (GenerateRnd(100) < 2 * monster.intelligence + 10) {
		int dist = monster.enemyPosition.WalkingDistance(monster.position.tile);
		if (dist >= 2) {
			if (dist >= 2 * monster.intelligence + 4) {
				Direction md = monster.direction;
				if (GenerateRnd(100) < 2 * monster.intelligence + 20) {
					md = static_cast<Direction>(GenerateRnd(8));
				}
				Walk(monster, md);
			} else {
				RandomWalk(monster, GetMonsterDirection(monster));
			}
		} else {
			StartAttack(monster);
		}
	}

	monster.checkStandAnimationIsLoaded(monster.direction);
}

void OverlordAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	Direction md = GetMonsterDirection(monster);
	monster.direction = md;
	int v = GenerateRnd(100);
	if (monster.distanceToEnemy() >= 2) {
		if ((monster.var2 > 20 && v < 4 * monster.intelligence + 20)
		    || (IsMonsterModeMove(static_cast<MonsterMode>(monster.var1))
		        && monster.var2 == 0
		        && v < 4 * monster.intelligence + 70)) {
			RandomWalk(monster, md);
		}
	} else if (v < 4 * monster.intelligence + 15) {
		StartAttack(monster);
	} else if (v < 4 * monster.intelligence + 20) {
		StartSpecialAttack(monster);
	}

	monster.checkStandAnimationIsLoaded(md);
}

void SkeletonAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	Direction md = GetDirection(monster.position.tile, monster.position.last);
	monster.direction = md;
	if (monster.distanceToEnemy() >= 2) {
		if (static_cast<MonsterMode>(monster.var1) == MonsterMode::Delay || (GenerateRnd(100) >= 35 - 4 * monster.intelligence)) {
			RandomWalk(monster, md);
		} else {
			AiDelay(monster, 15 - 2 * monster.intelligence + GenerateRnd(10));
		}
	} else {
		if (static_cast<MonsterMode>(monster.var1) == MonsterMode::Delay || (GenerateRnd(100) < 2 * monster.intelligence + 20)) {
			StartAttack(monster);
		} else {
			AiDelay(monster, 2 * (5 - monster.intelligence) + GenerateRnd(10));
		}
	}

	monster.checkStandAnimationIsLoaded(md);
}

void SkeletonBowAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	Direction md = GetMonsterDirection(monster);
	monster.direction = md;
	int v = GenerateRnd(100);

	bool walking = false;

	if (monster.distanceToEnemy() < 4) {
		if ((monster.var2 > 20 && v < 2 * monster.intelligence + 13)
		    || (IsMonsterModeMove(static_cast<MonsterMode>(monster.var1))
		        && monster.var2 == 0
		        && v < 2 * monster.intelligence + 63)) {
			walking = Walk(monster, Opposite(md));
		}
	}

	if (!walking) {
		if (GenerateRnd(100) < 2 * monster.intelligence + 3) {
			if (LineClearMissile(monster.position.tile, monster.enemyPosition))
				StartRangedAttack(monster, MissileID::Arrow, 4);
		}
	}

	monster.checkStandAnimationIsLoaded(md);
}

std::optional<Point> ScavengerFindCorpse(const Monster &scavenger)
{
	bool reverseSearch = FlipCoin();
	int first = reverseSearch ? 4 : -4;
	int last = reverseSearch ? -4 : 4;
	int increment = reverseSearch ? -1 : 1;

	for (int y = first; y <= last; y += increment) {
		for (int x = first; x <= last; x += increment) {
			Point position = scavenger.position.tile + Displacement { x, y };
			if (!InDungeonBounds(position))
				continue;
			if (dCorpse[position.x][position.y] == 0)
				continue;
			if (!IsLineNotSolid(scavenger.position.tile, position))
				continue;
			return position;
		}
	}
	return {};
}

void ScavengerAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand)
		return;
	if (monster.hitPoints < (monster.maxHitPoints / 2) && monster.goal != MonsterGoal::Healing) {
		if (monster.leaderRelation != LeaderRelation::None) {
			ShrinkLeaderPacksize(monster);
			monster.leaderRelation = LeaderRelation::None;
		}
		monster.goal = MonsterGoal::Healing;
		monster.goalVar3 = 10;
	}
	if (monster.goal == MonsterGoal::Healing && monster.goalVar3 != 0) {
		monster.goalVar3--;
		if (dCorpse[monster.position.tile.x][monster.position.tile.y] != 0) {
			StartEating(monster);
			if (gbIsHellfire) {
				int mMaxHP = monster.maxHitPoints;
				monster.hitPoints += mMaxHP / 8;
				if (monster.hitPoints > monster.maxHitPoints)
					monster.hitPoints = monster.maxHitPoints;
				if (monster.goalVar3 <= 0 || monster.hitPoints == monster.maxHitPoints) {
					dCorpse[monster.position.tile.x][monster.position.tile.y] = 0;
					oracool::ForgetCorpseAt(monster.position.tile); // eaten (round 53 audit)
				}
			} else {
				monster.hitPoints += 64;
			}
			int targetHealth = monster.maxHitPoints;
			if (!gbIsHellfire)
				targetHealth = (monster.maxHitPoints / 2) + (monster.maxHitPoints / 4);
			if (monster.hitPoints >= targetHealth) {
				monster.goal = MonsterGoal::Normal;
				monster.goalVar1 = 0;
				monster.goalVar2 = 0;
			}
		} else {
			if (monster.goalVar1 == 0) {
				std::optional<Point> position = ScavengerFindCorpse(monster);
				if (position) {
					monster.goalVar1 = position->x + 1;
					monster.goalVar2 = position->y + 1;
				}
			}
			if (monster.goalVar1 != 0) {
				int x = monster.goalVar1 - 1;
				int y = monster.goalVar2 - 1;
				monster.direction = GetDirection(monster.position.tile, { x, y });
				RandomWalk(monster, monster.direction);
			}
		}
	}

	if (monster.mode == MonsterMode::Stand)
		SkeletonAi(monster);
}

void RhinoAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (monster.activeForTicks < UINT8_MAX)
		MonstCheckDoors(monster);
	int v = GenerateRnd(100);
	unsigned distanceToEnemy = monster.distanceToEnemy();
	if (distanceToEnemy >= 2) {
		if (monster.goal == MonsterGoal::Move || (distanceToEnemy >= 5 && !FlipCoin(4))) {
			if (monster.goal != MonsterGoal::Move) {
				monster.goalVar1 = 0;
				monster.goalVar2 = GenerateRnd(2);
			}
			monster.goal = MonsterGoal::Move;
			if (monster.goalVar1++ >= static_cast<int>(2 * distanceToEnemy) || dTransVal[monster.position.tile.x][monster.position.tile.y] != dTransVal[monster.enemyPosition.x][monster.enemyPosition.y]) {
				monster.goal = MonsterGoal::Normal;
			} else if (!RoundWalk(monster, md, &monster.goalVar2)) {
				AiDelay(monster, GenerateRnd(10) + 10);
			}
		}
	} else {
		monster.goal = MonsterGoal::Normal;
	}
	if (monster.goal == MonsterGoal::Normal) {
		if (distanceToEnemy >= 5
		    && v < 2 * monster.intelligence + 43
		    && LineClear([&monster](Point position) { return IsTileAvailable(monster, position); }, monster.position.tile, monster.enemyPosition)) {
			size_t monsterId = monster.getId();
			if (AddMissile(monster.position.tile, monster.enemyPosition, md, MissileID::Rhino, TARGET_PLAYERS, monsterId, 0, 0) != nullptr) {
				if (monster.data().hasSpecialSound)
					PlayEffect(monster, MonsterSound::Special);
				dMonster[monster.position.tile.x][monster.position.tile.y] = -(monsterId + 1);
				monster.mode = MonsterMode::Charge;
			}
		} else {
			if (distanceToEnemy >= 2) {
				v = GenerateRnd(100);
				if (v >= 2 * monster.intelligence + 33
				    && (IsNoneOf(static_cast<MonsterMode>(monster.var1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways)
				        || monster.var2 != 0
				        || v >= 2 * monster.intelligence + 83)) {
					AiDelay(monster, GenerateRnd(10) + 10);
				} else {
					RandomWalk(monster, md);
				}
			} else if (v < 2 * monster.intelligence + 28) {
				monster.direction = md;
				StartAttack(monster);
			}
		}
	}

	monster.checkStandAnimationIsLoaded(monster.direction);
}

void FallenAi(Monster &monster)
{
	if (monster.goal == MonsterGoal::Attack) {
		if (monster.goalVar1 != 0)
			monster.goalVar1--;
		else
			monster.goal = MonsterGoal::Normal;
	}
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	if (monster.goal == MonsterGoal::Retreat) {
		if (monster.goalVar1-- == 0) {
			monster.goal = MonsterGoal::Normal;
			M_StartStand(monster, Opposite(static_cast<Direction>(monster.goalVar2)));
		}
	}

	if (monster.animInfo.isLastFrame()) {
		if (!FlipCoin(4)) {
			return;
		}
		StartSpecialStand(monster, monster.direction);
		if (monster.maxHitPoints - (2 * monster.intelligence + 2) >= monster.hitPoints)
			monster.hitPoints += 2 * monster.intelligence + 2;
		else
			monster.hitPoints = monster.maxHitPoints;
		int rad = 2 * monster.intelligence + 4;
		for (int y = -rad; y <= rad; y++) {
			for (int x = -rad; x <= rad; x++) {
				int xpos = monster.position.tile.x + x;
				int ypos = monster.position.tile.y + y;
				if (InDungeonBounds({ xpos, ypos })) {
					int m = dMonster[xpos][ypos];
					if (m <= 0)
						continue;

					auto &otherMonster = Monsters[m - 1];
					if (otherMonster.ai != MonsterAIID::Fallen)
						continue;

					otherMonster.goal = MonsterGoal::Attack;
					otherMonster.goalVar1 = 30 * monster.intelligence + 105;
				}
			}
		}
	} else if (monster.goal == MonsterGoal::Retreat) {
		monster.direction = static_cast<Direction>(monster.goalVar2);
		RandomWalk(monster, monster.direction);
	} else if (monster.goal == MonsterGoal::Attack) {
		if (monster.distanceToEnemy() < 2)
			StartAttack(monster);
		else
			RandomWalk(monster, GetMonsterDirection(monster));
	} else
		SkeletonAi(monster);
}

void LeoricAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (monster.activeForTicks < UINT8_MAX)
		MonstCheckDoors(monster);
	int v = GenerateRnd(100);
	unsigned distanceToEnemy = monster.distanceToEnemy();
	if (distanceToEnemy >= 2 && monster.activeForTicks == UINT8_MAX && dTransVal[monster.position.tile.x][monster.position.tile.y] == dTransVal[monster.enemyPosition.x][monster.enemyPosition.y]) {
		if (monster.goal == MonsterGoal::Move || (distanceToEnemy >= 3 && FlipCoin(4))) {
			if (monster.goal != MonsterGoal::Move) {
				monster.goalVar1 = 0;
				monster.goalVar2 = GenerateRnd(2);
			}
			monster.goal = MonsterGoal::Move;
			if ((monster.goalVar1++ >= static_cast<int>(2 * distanceToEnemy) && DirOK(monster, md)) || dTransVal[monster.position.tile.x][monster.position.tile.y] != dTransVal[monster.enemyPosition.x][monster.enemyPosition.y]) {
				monster.goal = MonsterGoal::Normal;
			} else if (!RoundWalk(monster, md, &monster.goalVar2)) {
				AiDelay(monster, GenerateRnd(10) + 10);
			}
		}
	} else {
		monster.goal = MonsterGoal::Normal;
	}
	if (monster.goal == MonsterGoal::Normal) {
		if (!gbIsMultiplayer
		    && ((distanceToEnemy >= 3 && v < 4 * monster.intelligence + 35) || v < 6)
		    && LineClearMissile(monster.position.tile, monster.enemyPosition)) {
			Point newPosition = monster.position.tile + md;
			if (IsTileAvailable(monster, newPosition) && EnemyMonsterRoomLeft()) {
				SpawnSkeleton(newPosition, md);
				StartSpecialStand(monster, md);
			}
		} else {
			if (distanceToEnemy >= 2) {
				v = GenerateRnd(100);
				if (v >= monster.intelligence + 25
				    && (IsNoneOf(static_cast<MonsterMode>(monster.var1), MonsterMode::MoveNorthwards, MonsterMode::MoveSouthwards, MonsterMode::MoveSideways) || monster.var2 != 0 || (v >= monster.intelligence + 75))) {
					AiDelay(monster, GenerateRnd(10) + 10);
				} else {
					RandomWalk(monster, md);
				}
			} else if (v < monster.intelligence + 20) {
				monster.direction = md;
				StartAttack(monster);
			}
		}
	}

	monster.checkStandAnimationIsLoaded(md);
}

void BatAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	Direction md = GetDirection(monster.position.tile, monster.position.last);
	monster.direction = md;
	int v = GenerateRnd(100);
	if (monster.goal == MonsterGoal::Retreat) {
		if (monster.goalVar1 == 0) {
			RandomWalk(monster, Opposite(md));
			monster.goalVar1++;
		} else {
			RandomWalk(monster, PickRandomlyAmong({ Right(md), Left(md) }));
			monster.goal = MonsterGoal::Normal;
		}
		return;
	}

	unsigned distanceToEnemy = monster.distanceToEnemy();
	if (monster.type().type == MT_GLOOM
	    && distanceToEnemy >= 5
	    && v < 4 * monster.intelligence + 33
	    && LineClear([&monster](Point position) { return IsTileAvailable(monster, position); }, monster.position.tile, monster.enemyPosition)) {
		size_t monsterId = monster.getId();
		if (AddMissile(monster.position.tile, monster.enemyPosition, md, MissileID::Rhino, TARGET_PLAYERS, monsterId, 0, 0) != nullptr) {
			dMonster[monster.position.tile.x][monster.position.tile.y] = -(monsterId + 1);
			monster.mode = MonsterMode::Charge;
		}
	} else if (distanceToEnemy >= 2) {
		if ((monster.var2 > 20 && v < monster.intelligence + 13)
		    || (IsMonsterModeMove(static_cast<MonsterMode>(monster.var1))
		        && monster.var2 == 0
		        && v < monster.intelligence + 63)) {
			RandomWalk(monster, md);
		}
	} else if (v < 4 * monster.intelligence + 8) {
		StartAttack(monster);
		monster.goal = MonsterGoal::Retreat;
		monster.goalVar1 = 0;
		if (monster.type().type == MT_FAMILIAR) {
			AddMissile(monster.enemyPosition, { monster.enemyPosition.x + 1, 0 }, Direction::South, MissileID::Lightning, TARGET_PLAYERS, monster.getId(), GenerateRnd(10) + 1, 0);
		}
	}

	monster.checkStandAnimationIsLoaded(md);
}

void GargoyleAi(Monster &monster)
{
	Direction md = GetMonsterDirection(monster);
	unsigned distanceToEnemy = monster.distanceToEnemy();
	if (monster.activeForTicks != 0 && (monster.flags & MFLAG_ALLOW_SPECIAL) != 0) {
		UpdateEnemy(monster);
		if (distanceToEnemy < monster.intelligence + 2u) {
			monster.flags &= ~MFLAG_ALLOW_SPECIAL;
		}
		return;
	}

	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	if (monster.hitPoints < (monster.maxHitPoints / 2))
		monster.goal = MonsterGoal::Retreat;
	if (monster.goal == MonsterGoal::Retreat) {
		if (distanceToEnemy >= monster.intelligence + 2u) {
			monster.goal = MonsterGoal::Normal;
			StartHeal(monster);
		} else if (!RandomWalk(monster, Opposite(md))) {
			monster.goal = MonsterGoal::Normal;
		}
	}
	AiAvoidance(monster);
}

void ButcherAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	Direction md = GetDirection(monster.position.tile, monster.position.last);
	monster.direction = md;

	if (monster.distanceToEnemy() >= 2)
		RandomWalk(monster, md);
	else
		StartAttack(monster);

	monster.checkStandAnimationIsLoaded(md);
}

void SneakAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand) {
		return;
	}
	if (dLight[monster.position.tile.x][monster.position.tile.y] == LightsMax) {
		return;
	}
	if (oracool::CursedMonsterBlinded(monster) && monster.activeForTicks == 0)
		return; // and the Sneaks, which read only light and distance (round 24 audit)

	unsigned dist = 5 - monster.intelligence;
	unsigned distanceToEnemy = monster.distanceToEnemy();
	if (static_cast<MonsterMode>(monster.var1) == MonsterMode::HitRecovery) {
		monster.goal = MonsterGoal::Retreat;
		monster.goalVar1 = 0;
	} else if (distanceToEnemy >= dist + 3 || monster.goalVar1 > 8) {
		monster.goal = MonsterGoal::Normal;
		monster.goalVar1 = 0;
	}
	Direction md = GetMonsterDirection(monster);
	if (monster.goal == MonsterGoal::Retreat && (monster.flags & MFLAG_NO_ENEMY) == 0) {
		if ((monster.flags & MFLAG_TARGETS_MONSTER) != 0)
			md = GetDirection(monster.position.tile, Monsters[monster.enemy].position.tile);
		else
			md = GetDirection(monster.position.tile, Players[monster.enemy].position.last);
		md = Opposite(md);
		if (monster.type().type == MT_UNSEEN) {
			md = PickRandomlyAmong({ Right(md), Left(md) });
		}
	}
	monster.direction = md;
	int v = GenerateRnd(100);
	if (distanceToEnemy < dist && (monster.flags & MFLAG_HIDDEN) != 0) {
		StartFadein(monster, md, false);
	} else {
		if ((distanceToEnemy >= dist + 1) && (monster.flags & MFLAG_HIDDEN) == 0) {
			StartFadeout(monster, md, true);
		} else {
			if (monster.goal == MonsterGoal::Retreat
			    || (distanceToEnemy >= 2
			        && ((monster.var2 > 20 && v < 4 * monster.intelligence + 14)
			            || (IsMonsterModeMove(static_cast<MonsterMode>(monster.var1))
			                && monster.var2 == 0 && v < 4 * monster.intelligence + 64)))) {
				monster.goalVar1++;
				RandomWalk(monster, md);
			}
		}
	}
	if (monster.mode == MonsterMode::Stand) {
		if (distanceToEnemy >= 2 || v >= 4 * monster.intelligence + 10)
			monster.changeAnimationData(MonsterGraphic::Stand);
		else
			StartAttack(monster);
	}
}

void GharbadAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);

	if (monster.talkMsg >= TEXT_GARBUD1
	    && monster.talkMsg <= TEXT_GARBUD3
	    && !IsTileVisible(monster.position.tile)
	    && monster.goal == MonsterGoal::Talking) {
		monster.goal = MonsterGoal::Inquiring;
		switch (monster.talkMsg) {
		case TEXT_GARBUD1:
			monster.talkMsg = TEXT_GARBUD2;
			Quests[Q_GARBUD]._qvar1 = QS_GHARBAD_FIRST_ITEM_READY;
			NetSendCmdQuest(true, Quests[Q_GARBUD]);
			break;
		case TEXT_GARBUD2:
			monster.talkMsg = TEXT_GARBUD3;
			Quests[Q_GARBUD]._qvar1 = QS_GHARBAD_SECOND_ITEM_NEARLY_DONE;
			NetSendCmdQuest(true, Quests[Q_GARBUD]);
			break;
		case TEXT_GARBUD3:
			monster.talkMsg = TEXT_GARBUD4;
			Quests[Q_GARBUD]._qvar1 = QS_GHARBAD_SECOND_ITEM_READY;
			NetSendCmdQuest(true, Quests[Q_GARBUD]);
			break;
		default:
			break;
		}
	}

	if (IsTileVisible(monster.position.tile)) {
		if (monster.talkMsg == TEXT_GARBUD4) {
			if (!effect_is_playing(USFX_GARBUD4) && monster.goal == MonsterGoal::Talking) {
				monster.goal = MonsterGoal::Normal;
				monster.activeForTicks = UINT8_MAX;
				monster.talkMsg = TEXT_NONE;
				Quests[Q_GARBUD]._qvar1 = QS_GHARBAD_ATTACKING;
				NetSendCmdQuest(true, Quests[Q_GARBUD]);
			}
		}
	}

	if (IsAnyOf(monster.goal, MonsterGoal::Normal, MonsterGoal::Move))
		AiAvoidance(monster);

	monster.checkStandAnimationIsLoaded(md);
}

void SnotSpilAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);

	if (monster.talkMsg == TEXT_BANNER10 && !IsTileVisible(monster.position.tile) && monster.goal == MonsterGoal::Talking) {
		monster.talkMsg = TEXT_BANNER11;
		monster.goal = MonsterGoal::Inquiring;
	}

	if (monster.talkMsg == TEXT_BANNER11 && Quests[Q_LTBANNER]._qvar1 == 3) {
		monster.talkMsg = TEXT_NONE;
		monster.goal = MonsterGoal::Normal;
	}

	if (IsTileVisible(monster.position.tile)) {
		if (monster.talkMsg == TEXT_BANNER12) {
			if (!effect_is_playing(USFX_SNOT3) && monster.goal == MonsterGoal::Talking) {
				ObjChangeMap(SetPiece.position.x, SetPiece.position.y, SetPiece.position.x + SetPiece.size.width + 1, SetPiece.position.y + SetPiece.size.height + 1);
				Quests[Q_LTBANNER]._qvar1 = 3;
				NetSendCmdQuest(true, Quests[Q_LTBANNER]);
				RedoPlayerVision();
				monster.activeForTicks = UINT8_MAX;
				monster.talkMsg = TEXT_NONE;
				monster.goal = MonsterGoal::Normal;
			}
		}
		if (Quests[Q_LTBANNER]._qvar1 == 3) {
			if (IsAnyOf(monster.goal, MonsterGoal::Normal, MonsterGoal::Attack))
				FallenAi(monster);
		}
	}

	monster.checkStandAnimationIsLoaded(md);
}

void SnakeAi(Monster &monster)
{
	int8_t pattern[6] = { 1, 1, 0, -1, -1, 0 };
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0)
		return;
	// goalVar1 indexes the pattern; a retreat wrote a step count there (Howl: 4 + rank), read past the array (round 15).
	if (monster.goalVar1 < 0 || monster.goalVar1 > 5)
		monster.goalVar1 = 0;
	Direction md = GetDirection(monster.position.tile, monster.position.last);
	monster.direction = md;
	unsigned distanceToEnemy = monster.distanceToEnemy();
	if (distanceToEnemy >= 2) {
		if (distanceToEnemy < 3 && LineClear([&monster](Point position) { return IsTileAvailable(monster, position); }, monster.position.tile, monster.enemyPosition) && static_cast<MonsterMode>(monster.var1) != MonsterMode::Charge) {
			size_t monsterId = monster.getId();
			if (AddMissile(monster.position.tile, monster.enemyPosition, md, MissileID::Rhino, TARGET_PLAYERS, monsterId, 0, 0) != nullptr) {
				PlayEffect(monster, MonsterSound::Attack);
				dMonster[monster.position.tile.x][monster.position.tile.y] = -(monsterId + 1);
				monster.mode = MonsterMode::Charge;
			}
		} else if (static_cast<MonsterMode>(monster.var1) == MonsterMode::Delay || GenerateRnd(100) >= 35 - 2 * monster.intelligence) {
			if (pattern[monster.goalVar1] == -1)
				md = Left(md);
			else if (pattern[monster.goalVar1] == 1)
				md = Right(md);

			monster.goalVar1++;
			if (monster.goalVar1 > 5)
				monster.goalVar1 = 0;

			Direction targetDirection = static_cast<Direction>(monster.goalVar2);
			if (md != targetDirection) {
				int drift = static_cast<int>(md) - monster.goalVar2;
				if (drift < 0)
					drift += 8;

				if (drift < 4)
					md = Right(targetDirection);
				else if (drift > 4)
					md = Left(targetDirection);
				monster.goalVar2 = static_cast<int>(md);
			}

			if (!Walk(monster, md))
				RandomWalk2(monster, monster.direction);
		} else {
			AiDelay(monster, 15 - monster.intelligence + GenerateRnd(10));
		}
	} else {
		if (IsAnyOf(static_cast<MonsterMode>(monster.var1), MonsterMode::Delay, MonsterMode::Charge)
		    || (GenerateRnd(100) < monster.intelligence + 20)) {
			StartAttack(monster);
		} else
			AiDelay(monster, 10 - monster.intelligence + GenerateRnd(10));
	}

	monster.checkStandAnimationIsLoaded(monster.direction);
}

void CounselorAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}
	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (monster.activeForTicks < UINT8_MAX)
		MonstCheckDoors(monster);
	int v = GenerateRnd(100);
	unsigned distanceToEnemy = monster.distanceToEnemy();
	if (monster.goal == MonsterGoal::Retreat) {
		if (monster.goalVar1++ <= 3)
			RandomWalk(monster, Opposite(md));
		else {
			monster.goal = MonsterGoal::Normal;
			StartFadein(monster, md, true);
		}
	} else if (monster.goal == MonsterGoal::Move) {
		if (distanceToEnemy >= 2 && monster.activeForTicks == UINT8_MAX && dTransVal[monster.position.tile.x][monster.position.tile.y] == dTransVal[monster.enemyPosition.x][monster.enemyPosition.y]) {
			if (monster.goalVar1++ < static_cast<int>(2 * distanceToEnemy) || !DirOK(monster, md)) {
				RoundWalk(monster, md, &monster.goalVar2);
			} else {
				monster.goal = MonsterGoal::Normal;
				StartFadein(monster, md, true);
			}
		} else {
			monster.goal = MonsterGoal::Normal;
			StartFadein(monster, md, true);
		}
	} else if (monster.goal == MonsterGoal::Normal) {
		if (distanceToEnemy >= 2) {
			if (v < 5 * (monster.intelligence + 10) && LineClearMissile(monster.position.tile, monster.enemyPosition)) {
				constexpr MissileID MissileTypes[4] = { MissileID::Firebolt, MissileID::ChargedBolt, MissileID::LightningControl, MissileID::Fireball };
				// Both ends raised by the pack's Might, not the minimum only (round 28 audit).
				const int minDamage = oracool::PackAdjustedDamage(monster, monster.minDamage);
				const int maxDamage = std::max<int>(oracool::PackAdjustedDamage(monster, monster.maxDamage), minDamage);
				StartRangedAttack(monster, MissileTypes[monster.intelligence], minDamage + GenerateRnd(maxDamage - minDamage + 1));
			} else if (GenerateRnd(100) < 30) {
				monster.goal = MonsterGoal::Move;
				monster.goalVar1 = 0;
				StartFadeout(monster, md, false);
			} else
				AiDelay(monster, GenerateRnd(10) + 2 * (5 - monster.intelligence));
		} else {
			monster.direction = md;
			if (monster.hitPoints < (monster.maxHitPoints / 2)) {
				monster.goal = MonsterGoal::Retreat;
				monster.goalVar1 = 0;
				StartFadeout(monster, md, false);
			} else if (static_cast<MonsterMode>(monster.var1) == MonsterMode::Delay
			    || GenerateRnd(100) < 2 * monster.intelligence + 20) {
				StartRangedAttack(monster, MissileID::Null, 0);
				size_t monsterId = monster.getId();
				AddMissile(monster.position.tile, { 0, 0 }, monster.direction, MissileID::FlashBottom, TARGET_PLAYERS, monsterId, 4, 0);
				AddMissile(monster.position.tile, { 0, 0 }, monster.direction, MissileID::FlashTop, TARGET_PLAYERS, monsterId, 4, 0);
			} else
				AiDelay(monster, GenerateRnd(10) + 2 * (5 - monster.intelligence));
		}
	}
	if (monster.mode == MonsterMode::Stand) {
		AiDelay(monster, GenerateRnd(10) + 5);
	}
}

void ZharAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);
	if (monster.talkMsg == TEXT_ZHAR1 && !IsTileVisible(monster.position.tile) && monster.goal == MonsterGoal::Talking) {
		monster.talkMsg = TEXT_ZHAR2;
		monster.goal = MonsterGoal::Inquiring;
		Quests[Q_ZHAR]._qvar1 = QS_ZHAR_ANGRY;
		NetSendCmdQuest(true, Quests[Q_ZHAR]);
	}

	if (IsTileVisible(monster.position.tile)) {
		if (monster.talkMsg == TEXT_ZHAR2) {
			if (!effect_is_playing(USFX_ZHAR2) && monster.goal == MonsterGoal::Talking) {
				monster.activeForTicks = UINT8_MAX;
				monster.talkMsg = TEXT_NONE;
				monster.goal = MonsterGoal::Normal;
				Quests[Q_ZHAR]._qvar1 = QS_ZHAR_ATTACKING;
				NetSendCmdQuest(true, Quests[Q_ZHAR]);
			}
		}
	}

	if (IsAnyOf(monster.goal, MonsterGoal::Normal, MonsterGoal::Retreat, MonsterGoal::Move))
		CounselorAi(monster);

	monster.checkStandAnimationIsLoaded(md);
}

void MegaAi(Monster &monster)
{
	unsigned distanceToEnemy = monster.distanceToEnemy();
	if (distanceToEnemy >= 5) {
		SkeletonAi(monster);
		return;
	}

	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	Direction md = GetDirection(monster.position.tile, monster.position.last);
	if (monster.activeForTicks < UINT8_MAX)
		MonstCheckDoors(monster);
	int v = GenerateRnd(100);
	if (distanceToEnemy >= 2 && monster.activeForTicks == UINT8_MAX && dTransVal[monster.position.tile.x][monster.position.tile.y] == dTransVal[monster.enemyPosition.x][monster.enemyPosition.y]) {
		if (monster.goal == MonsterGoal::Move || distanceToEnemy >= 3) {
			if (monster.goal != MonsterGoal::Move) {
				monster.goalVar1 = 0;
				monster.goalVar2 = GenerateRnd(2);
			}
			monster.goal = MonsterGoal::Move;
			monster.goalVar3 = 4;
			if (monster.goalVar1++ < static_cast<int>(2 * distanceToEnemy) || !DirOK(monster, md)) {
				if (v < 5 * (monster.intelligence + 16))
					RoundWalk(monster, md, &monster.goalVar2);
			} else
				monster.goal = MonsterGoal::Normal;
		}
	} else {
		monster.goal = MonsterGoal::Normal;
	}
	if (monster.goal == MonsterGoal::Normal) {
		if (((distanceToEnemy >= 3 && v < 5 * (monster.intelligence + 2)) || v < 5 * (monster.intelligence + 1) || monster.goalVar3 == 4) && LineClearMissile(monster.position.tile, monster.enemyPosition)) {
			StartRangedSpecialAttack(monster, MissileID::InfernoControl, 0);
		} else if (distanceToEnemy >= 2) {
			v = GenerateRnd(100);
			if (v < 2 * (5 * monster.intelligence + 25)
			    || (IsMonsterModeMove(static_cast<MonsterMode>(monster.var1))
			        && monster.var2 == 0
			        && v < 2 * (5 * monster.intelligence + 40))) {
				RandomWalk(monster, md);
			}
		} else {
			if (GenerateRnd(100) < 10 * (monster.intelligence + 4)) {
				monster.direction = md;
				if (FlipCoin())
					StartRangedSpecialAttack(monster, MissileID::InfernoControl, 0);
				else
					StartAttack(monster);
			}
		}
		monster.goalVar3 = 1;
	}
	if (monster.mode == MonsterMode::Stand) {
		AiDelay(monster, GenerateRnd(10) + 5);
	}
}

void LazarusAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);
	if (IsTileVisible(monster.position.tile)) {
		if (!UseMultiplayerQuests()) {
			Player &myPlayer = *MyPlayer;
			if (monster.talkMsg == TEXT_VILE13 && monster.goal == MonsterGoal::Inquiring && myPlayer.position.tile == Point { 35, 46 }) {
				if (!gbIsMultiplayer) {
					// Playing ingame movies is currently not supported in multiplayer
					PlayInGameMovie("gendata\\fprst3.smk");
				}
				monster.mode = MonsterMode::Talk;
				Quests[Q_BETRAYER]._qvar1 = 5;
				NetSendCmdQuest(true, Quests[Q_BETRAYER]);
			}

			if (monster.talkMsg == TEXT_VILE13 && !effect_is_playing(USFX_LAZ1) && monster.goal == MonsterGoal::Talking) {
				ObjChangeMap(1, 18, 20, 24);
				RedoPlayerVision();
				Quests[Q_BETRAYER]._qvar1 = 6;
				monster.goal = MonsterGoal::Normal;
				monster.activeForTicks = UINT8_MAX;
				monster.talkMsg = TEXT_NONE;
				NetSendCmdQuest(true, Quests[Q_BETRAYER]);
			}
		}

		if (UseMultiplayerQuests() && monster.talkMsg == TEXT_VILE13 && monster.goal == MonsterGoal::Inquiring && Quests[Q_BETRAYER]._qvar1 <= 3) {
			monster.mode = MonsterMode::Talk;
		}
	}

	if (IsAnyOf(monster.goal, MonsterGoal::Normal, MonsterGoal::Retreat, MonsterGoal::Move)) {
		if (!UseMultiplayerQuests() && Quests[Q_BETRAYER]._qvar1 == 4 && monster.talkMsg == TEXT_NONE) { // Fix save games affected by teleport bug
			ObjChangeMapResync(1, 18, 20, 24);
			RedoPlayerVision();
			Quests[Q_BETRAYER]._qvar1 = 6;
		}
		monster.talkMsg = TEXT_NONE;
		CounselorAi(monster);
	}

	monster.checkStandAnimationIsLoaded(md);
}

void LazarusMinionAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand)
		return;

	Direction md = GetMonsterDirection(monster);

	if (IsTileVisible(monster.position.tile)) {
		if (!UseMultiplayerQuests()) {
			if (Quests[Q_BETRAYER]._qvar1 <= 5) {
				monster.goal = MonsterGoal::Inquiring;
			} else {
				monster.goal = MonsterGoal::Normal;
				monster.talkMsg = TEXT_NONE;
			}
		} else
			monster.goal = MonsterGoal::Normal;
	}
	if (monster.goal == MonsterGoal::Normal)
		AiRanged(monster);

	monster.checkStandAnimationIsLoaded(md);
}

void LachdananAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);

	if (monster.talkMsg == TEXT_VEIL9 && !IsTileVisible(monster.position.tile) && monster.goal == MonsterGoal::Talking) {
		monster.talkMsg = TEXT_VEIL10;
		monster.goal = MonsterGoal::Inquiring;
		Quests[Q_VEIL]._qvar2 = QS_VEIL_EARLY_RETURN;
		NetSendCmdQuest(true, Quests[Q_VEIL]);
	}

	if (IsTileVisible(monster.position.tile)) {
		if (monster.talkMsg == TEXT_VEIL11) {
			if (!effect_is_playing(USFX_LACH3) && monster.goal == MonsterGoal::Talking) {
				monster.talkMsg = TEXT_NONE;
				Quests[Q_VEIL]._qactive = QUEST_DONE;
				NetSendCmdQuest(true, Quests[Q_VEIL]);
				MonsterDeath(monster, monster.direction, true);
				delta_kill_monster(monster, monster.position.tile, *MyPlayer);
				NetSendCmdLocParam1(false, CMD_MONSTDEATH, monster.position.tile, monster.getId());
			}
		}
	}

	monster.checkStandAnimationIsLoaded(md);
}

void WarlordAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand) {
		return;
	}

	Direction md = GetMonsterDirection(monster);
	if (IsTileVisible(monster.position.tile)) {
		if (monster.talkMsg == TEXT_WARLRD9 && monster.goal == MonsterGoal::Inquiring)
			monster.mode = MonsterMode::Talk;
		if (monster.talkMsg == TEXT_WARLRD9 && !effect_is_playing(USFX_WARLRD1) && monster.goal == MonsterGoal::Talking) {
			monster.activeForTicks = UINT8_MAX;
			monster.talkMsg = TEXT_NONE;
			monster.goal = MonsterGoal::Normal;
			Quests[Q_WARLORD]._qvar1 = QS_WARLORD_ATTACKING;
			NetSendCmdQuest(true, Quests[Q_WARLORD]);
		}
	}

	if (monster.goal == MonsterGoal::Normal)
		SkeletonAi(monster);

	monster.checkStandAnimationIsLoaded(md);
}

void HorkDemonAi(Monster &monster)
{
	if (monster.mode != MonsterMode::Stand || monster.activeForTicks == 0) {
		return;
	}

	Direction md = GetDirection(monster.position.tile, monster.position.last);

	if (monster.activeForTicks < 255) {
		MonstCheckDoors(monster);
	}

	int v = GenerateRnd(100);

	unsigned distanceToEnemy = monster.distanceToEnemy();
	if (distanceToEnemy < 2) {
		monster.goal = MonsterGoal::Normal;
	} else if (monster.goal == MonsterGoal::Move || (distanceToEnemy >= 5 && !FlipCoin(4))) {
		if (monster.goal != MonsterGoal::Move) {
			monster.goalVar1 = 0;
			monster.goalVar2 = GenerateRnd(2);
		}
		monster.goal = MonsterGoal::Move;
		if (monster.goalVar1++ >= static_cast<int>(2 * distanceToEnemy) || dTransVal[monster.position.tile.x][monster.position.tile.y] != dTransVal[monster.enemyPosition.x][monster.enemyPosition.y]) {
			monster.goal = MonsterGoal::Normal;
		} else if (!RoundWalk(monster, md, &monster.goalVar2)) {
			AiDelay(monster, GenerateRnd(10) + 10);
		}
	}

	if (monster.goal == MonsterGoal::Normal) {
		if ((distanceToEnemy >= 3) && v < 2 * monster.intelligence + 43) {
			Point position = monster.position.tile + monster.direction;
			if (IsTileAvailable(monster, position) && EnemyMonsterRoomLeft()) {
				StartRangedSpecialAttack(monster, MissileID::HorkSpawn, 0);
			}
		} else if (distanceToEnemy < 2) {
			if (v < 2 * monster.intelligence + 28) {
				monster.direction = md;
				StartAttack(monster);
			}
		} else {
			v = GenerateRnd(100);
			if (v < 2 * monster.intelligence + 33
			    || (IsMonsterModeMove(static_cast<MonsterMode>(monster.var1))
			        && monster.var2 == 0
			        && v < 2 * monster.intelligence + 83)) {
				RandomWalk(monster, md);
			} else {
				AiDelay(monster, GenerateRnd(10) + 10);
			}
		}
	}

	monster.checkStandAnimationIsLoaded(monster.direction);
}

string_view GetMonsterTypeText(const MonsterData &monsterData)
{
	switch (monsterData.monsterClass) {
	case MonsterClass::Animal:
		return _("Animal");
	case MonsterClass::Demon:
		return _("Demon");
	case MonsterClass::Undead:
		return _("Undead");
	}

	app_fatal(StrCat("Unknown monsterClass ", static_cast<int>(monsterData.monsterClass)));
}

void ActivateSpawn(Monster &monster, Point position, Direction dir)
{
	dMonster[position.x][position.y] = monster.getId() + 1;
	monster.position.tile = position;
	monster.position.future = position;
	monster.position.old = position;
	// A Luminous one's light was placed where it was made, the map's corner (round 30 audit): it rose dark.
	if (monster.lightId != NO_LIGHT) {
		ChangeLightXY(monster.lightId, position);
		ChangeLightOffset(monster.lightId, {});
	}
	StartSpecialStand(monster, dir);
}

/** Maps from monster AI ID to monster AI function. */
void (*AiProc[])(Monster &monster) = {
	/*MonsterAIID::Zombie   */ &ZombieAi,
	/*MonsterAIID::Fat      */ &OverlordAi,
	/*MonsterAIID::SkeletonMelee   */ &SkeletonAi,
	/*MonsterAIID::SkeletonRanged  */ &SkeletonBowAi,
	/*MonsterAIID::Scavenger     */ &ScavengerAi,
	/*MonsterAIID::Rhino    */ &RhinoAi,
	/*MonsterAIID::GoatMelee   */ &AiAvoidance,
	/*MonsterAIID::GoatRanged  */ &AiRanged,
	/*MonsterAIID::Fallen   */ &FallenAi,
	/*MonsterAIID::Magma    */ &AiRangedAvoidance,
	/*MonsterAIID::SkeletonKing */ &LeoricAi,
	/*MonsterAIID::Bat      */ &BatAi,
	/*MonsterAIID::Gargoyle     */ &GargoyleAi,
	/*MonsterAIID::Butcher  */ &ButcherAi,
	/*MonsterAIID::Succubus     */ &AiRanged,
	/*MonsterAIID::Sneak    */ &SneakAi,
	/*MonsterAIID::Storm    */ &AiRangedAvoidance,
	/*MonsterAIID::FireMan  */ &AiRanged, // never spawned in play; the debug spawn command called a null slot (round 15)
	/*MonsterAIID::Gharbad   */ &GharbadAi,
	/*MonsterAIID::Acid     */ &AiRangedAvoidance,
	/*MonsterAIID::AcidUnique */ &AiRanged,
	/*MonsterAIID::Golem    */ &GolumAi,
	/*MonsterAIID::Zhar     */ &ZharAi,
	/*MonsterAIID::Snotspill */ &SnotSpilAi,
	/*MonsterAIID::Snake    */ &SnakeAi,
	/*MonsterAIID::Counselor  */ &CounselorAi,
	/*MonsterAIID::Mega     */ &MegaAi,
	/*MonsterAIID::Diablo   */ &AiRangedAvoidance,
	/*MonsterAIID::Lazarus  */ &LazarusAi,
	/*MonsterAIID::LazarusSuccubus  */ &LazarusMinionAi,
	/*MonsterAIID::Lachdanan  */ &LachdananAi,
	/*MonsterAIID::Warlord  */ &WarlordAi,
	/*MonsterAIID::FireBat  */ &AiRanged,
	/*MonsterAIID::Torchant */ &AiRanged,
	/*MonsterAIID::HorkDemon  */ &HorkDemonAi,
	/*MonsterAIID::Lich     */ &AiRanged,
	/*MonsterAIID::ArchLich */ &AiRanged,
	/*MonsterAIID::Psychorb */ &AiRanged,
	/*MonsterAIID::Necromorb*/ &AiRanged,
	/*MonsterAIID::BoneDemon*/ &AiRangedAvoidance
};

bool IsRelativeMoveOK(const Monster &monster, Point position, Direction mdir)
{
	Point futurePosition = position + mdir;
	if (!InDungeonBounds(futurePosition) || !IsTileAvailable(monster, futurePosition))
		return false;
	if (mdir == Direction::East) {
		if (IsTileSolid(position + Direction::SouthEast))
			return false;
	} else if (mdir == Direction::West) {
		if (IsTileSolid(position + Direction::SouthWest))
			return false;
	} else if (mdir == Direction::North) {
		if (IsTileSolid(position + Direction::NorthEast) || IsTileSolid(position + Direction::NorthWest))
			return false;
	} else if (mdir == Direction::South)
		if (IsTileSolid(position + Direction::SouthWest) || IsTileSolid(position + Direction::SouthEast))
			return false;
	return true;
}

bool IsMonsterAvalible(const MonsterData &monsterData)
{
	if (monsterData.availability == MonsterAvailability::Never)
		return false;

	if (gbIsSpawn && monsterData.availability == MonsterAvailability::Retail)
		return false;

	return currlevel >= monsterData.minDunLvl && currlevel <= monsterData.maxDunLvl;
}

bool UpdateModeStance(Monster &monster)
{
	switch (monster.mode) {
	case MonsterMode::Stand:
		MonsterIdle(monster);
		return false;
	case MonsterMode::MoveNorthwards:
	case MonsterMode::MoveSouthwards:
	case MonsterMode::MoveSideways:
		return MonsterWalk(monster, monster.mode);
	case MonsterMode::MeleeAttack:
		return MonsterAttack(monster);
	case MonsterMode::HitRecovery:
		return MonsterGotHit(monster);
	case MonsterMode::Death:
		MonsterDeath(monster);
		return false;
	case MonsterMode::SpecialMeleeAttack:
		return MonsterSpecialAttack(monster);
	case MonsterMode::FadeIn:
		return MonsterFadein(monster);
	case MonsterMode::FadeOut:
		return MonsterFadeout(monster);
	case MonsterMode::RangedAttack:
		return MonsterRangedAttack(monster);
	case MonsterMode::SpecialStand:
		return MonsterSpecialStand(monster);
	case MonsterMode::SpecialRangedAttack:
		return MonsterRangedSpecialAttack(monster);
	case MonsterMode::Delay:
		return MonsterDelay(monster);
	case MonsterMode::Petrified:
		MonsterPetrified(monster);
		return false;
	case MonsterMode::Heal:
		MonsterHeal(monster);
		return false;
	case MonsterMode::Talk:
		MonsterTalk(monster);
		return false;
	default:
		return false;
	}
}

} // namespace

void InitTRNForUniqueMonster(Monster &monster)
{
	char filestr[64];
	*BufCopy(filestr, R"(monsters\monsters\)", UniqueMonstersData[static_cast<size_t>(monster.uniqueType)].mTrnName, ".trn") = '\0';
	monster.uniqueMonsterTRN = LoadFileInMem<uint8_t>(filestr);
}

void PrepareUniqueMonst(Monster &monster, UniqueMonsterType monsterType, size_t minionType, int bosspacksize, const UniqueMonsterData &uniqueMonsterData)
{
	monster.uniqueType = monsterType;
	monster.maxHitPoints = uniqueMonsterData.mmaxhp << 6;

	if (!gbIsMultiplayer)
		monster.maxHitPoints = std::max(monster.maxHitPoints / 2, 64);

	monster.hitPoints = monster.maxHitPoints;
	monster.ai = uniqueMonsterData.mAi;
	monster.intelligence = uniqueMonsterData.mint;
	monster.minDamage = uniqueMonsterData.mMinDamage;
	monster.maxDamage = uniqueMonsterData.mMaxDamage;
	monster.minDamageSpecial = uniqueMonsterData.mMinDamage;
	monster.maxDamageSpecial = uniqueMonsterData.mMaxDamage;
	// Phase 3.3: UniqueMonsterData has only ONE resistance column where MonsterData has two, so this
	// used to hand a champion its Normal-difficulty set on every difficulty - leaving it softer than
	// the rank and file it leads once Hell switched THEM to the hard set. Unioned with its own
	// type's ladder, so it keeps its authored identity and can never be the weaker of the two.
	monster.resistance = oracool::ChampionResistancesFor(uniqueMonsterData.mMagicRes, monster.data(),
	    sgGameInitInfo.nDifficulty);
	monster.talkMsg = uniqueMonsterData.mtalkmsg;
	if (monsterType == UniqueMonsterType::HorkDemon || SuppressMonsterLights)
		monster.lightId = NO_LIGHT;
	else
		monster.lightId = AddLight(monster.position.tile, 3);

	if (UseMultiplayerQuests()) {
		if (monster.ai == MonsterAIID::LazarusSuccubus)
			monster.talkMsg = TEXT_NONE;
		if (monster.ai == MonsterAIID::Lazarus && Quests[Q_BETRAYER]._qvar1 > 3) {
			monster.goal = MonsterGoal::Normal;
		} else if (monster.talkMsg != TEXT_NONE) {
			monster.goal = MonsterGoal::Inquiring;
		}
	} else if (monster.talkMsg != TEXT_NONE) {
		monster.goal = MonsterGoal::Inquiring;
	}

	if (sgGameInitInfo.nDifficulty == DIFF_NIGHTMARE) {
		monster.maxHitPoints = 3 * monster.maxHitPoints;
		if (gbIsHellfire)
			monster.maxHitPoints += (gbIsMultiplayer ? 100 : 50) << 6;
		else
			monster.maxHitPoints += 100 << 6;
		monster.hitPoints = monster.maxHitPoints;
		monster.minDamage = 2 * (monster.minDamage + 2);
		monster.maxDamage = 2 * (monster.maxDamage + 2);
		monster.minDamageSpecial = 2 * (monster.minDamageSpecial + 2);
		monster.maxDamageSpecial = 2 * (monster.maxDamageSpecial + 2);
	} else if (sgGameInitInfo.nDifficulty == DIFF_HELL) {
		monster.maxHitPoints = 4 * monster.maxHitPoints;
		if (gbIsHellfire)
			monster.maxHitPoints += (gbIsMultiplayer ? 200 : 100) << 6;
		else
			monster.maxHitPoints += 200 << 6;
		monster.hitPoints = monster.maxHitPoints;
		// Clamped at 255 as Torment is (audit, 2026-09-27): Howlingire's 75 made 306, which wrapped to 50 - below his
		// minimum, so every blow did the minimum.
		monster.minDamage = static_cast<uint8_t>(std::min(4 * monster.minDamage + 6, 255));
		monster.maxDamage = static_cast<uint8_t>(std::min(4 * monster.maxDamage + 6, 255));
		monster.minDamageSpecial = static_cast<uint8_t>(std::min(4 * monster.minDamageSpecial + 6, 255));
		monster.maxDamageSpecial = static_cast<uint8_t>(std::min(4 * monster.maxDamageSpecial + 6, 255));
	} else if (sgGameInitInfo.nDifficulty == DIFF_TORMENT) {
		const float multiplier = GetTormentDifficultyMultiplier();
		monster.maxHitPoints = 4 * monster.maxHitPoints;
		if (gbIsHellfire)
			monster.maxHitPoints += (gbIsMultiplayer ? 200 : 100) << 6;
		else
			monster.maxHitPoints += 200 << 6;
		monster.maxHitPoints = static_cast<int>(monster.maxHitPoints * multiplier);
		monster.hitPoints = monster.maxHitPoints;
		monster.minDamage = static_cast<uint8_t>(std::min(static_cast<int>((4 * monster.minDamage + 6) * multiplier), 255));
		monster.maxDamage = static_cast<uint8_t>(std::min(static_cast<int>((4 * monster.maxDamage + 6) * multiplier), 255));
		monster.minDamageSpecial = static_cast<uint8_t>(std::min(static_cast<int>((4 * monster.minDamageSpecial + 6) * multiplier), 255));
		monster.maxDamageSpecial = static_cast<uint8_t>(std::min(static_cast<int>((4 * monster.maxDamageSpecial + 6) * multiplier), 255));
	}

	InitTRNForUniqueMonster(monster);
	monster.uniqTrans = uniquetrans++;

	if (uniqueMonsterData.customArmorClass != 0) {
		monster.armorClass = uniqueMonsterData.customArmorClass;

		if (sgGameInitInfo.nDifficulty == DIFF_NIGHTMARE) {
			monster.armorClass += NightmareAcBonus;
		} else if (sgGameInitInfo.nDifficulty == DIFF_HELL) {
			monster.armorClass += HellAcBonus;
		} else if (sgGameInitInfo.nDifficulty == DIFF_TORMENT) {
			monster.armorClass = static_cast<uint8_t>(std::min(monster.armorClass + static_cast<int>(HellAcBonus * GetTormentDifficultyMultiplier()), 255));
		}
	}

	if (uniqueMonsterData.monsterPack != UniqueMonsterPack::None) {
		PlaceGroup(minionType, bosspacksize, &monster, uniqueMonsterData.monsterPack == UniqueMonsterPack::Leashed);
	}

	if (monster.ai != MonsterAIID::Gargoyle) {
		monster.changeAnimationData(MonsterGraphic::Stand);
		monster.animInfo.currentFrame = GenerateRnd(monster.animInfo.numberOfFrames - 1);
		monster.flags &= ~MFLAG_ALLOW_SPECIAL;
		monster.mode = MonsterMode::Stand;
	}
}

void InitLevelMonsters()
{
	LevelMonsterTypeCount = 0;
	// A reloaded monster in an AI's own pause read as stunned from an old stun in the same slot on another level (round 20).
	StunnedBySkill.fill(false);
	RegroupBestDistance.fill(0);
	RegroupTriesWithoutGain.fill(0);
	monstimgtot = 0;
	// The scaled sheets are views onto sprite data that is about to be replaced, so they go first.
	oracool::ClearMonsterScaleCache();
	// Companions' bodies and town figures belong to the level being left; the companions come back on the new one.
	oracool::OnCompanionLevelLoad();
	oracool::OnMinionLevelLoad(); // the army's bodies too; its records wait for the next floor (oracool/minions.h)
	oracool::ClearNecromancerSummoningState(); // an Army of the Dead's remaining pulses belong to the floor it erupted on (audit, 2026-09-19)
	oracool::ClearCorpses();      // and the last floor's dead are no use on this one (oracool/corpses.h)
	oracool::ClearAllCurses();
	// So are the telemetry kill clocks, which are keyed by monster SLOT - and the slots are about to
	// be handed to different monsters. A clock left running by a monster that was wounded and never
	// killed would otherwise be read as the next occupant's time-to-kill.
	//
	// The ten-minute sanity filter in TelemetryRecordKill has caught the worst of these since
	// 2026-08-16, but only the worst: a stale clock UNDER ten minutes still logs a wrong fight
	// length, and it looks plausible, so it survives into the balance CSV instead of being thrown
	// away (audit, 2026-08-30). Clearing them here removes the class rather than its tail.
	oracool::TelemetryResetLevelTimers();
	// Every chill forgotten, and the heroes' chill, the passives' marks, the RfA-12 fields and marks and the warcry
	// wards with it: file-local statics that outlive the game. They were cleared in InitMonsters, which town never
	// runs - and a New Game starts in town, so the next hero began with the last one's half-speed chill and a Bone
	// Storm still circling slot 0 (audit, 2026-09-27). Here every level load passes, town and revisits included.
	oracool::ClearChills();
	oracool::ClearPlayerChills(); // a level change thaws the heroes too (2026-09-26)
	oracool::ClearMovementSlows(); // ...and their walk: the chill's other half ran out its ticks after the stairs (round 11)
	oracool::ClearPassiveMarks(); // the monsters' marks; the hero's clocks go down the stairs with him
	oracool::ClearRfa12State();
	oracool::ClearWarcries();

	for (CMonster &levelMonsterType : LevelMonsterTypes) {
		levelMonsterType.placeFlags = 0;
	}

	ClrAllMonsters();
	ActiveMonsterCount = 0;
	totalmonsters = MaxMonsters;

	for (size_t i = 0; i < MaxMonsters; i++) {
		ActiveMonsters[i] = i;
	}

	uniquetrans = 0;
}

void GetLevelMTypes()
{
	AddMonsterType(MT_GOLEM, PLACE_SPECIAL);
	if (currlevel == 16) {
		AddMonsterType(MT_ADVOCATE, PLACE_SCATTER);
		AddMonsterType(MT_RBLACK, PLACE_SCATTER);
		AddMonsterType(MT_DIABLO, PLACE_SPECIAL);
		return;
	}

	if (currlevel == 18)
		AddMonsterType(MT_HORKSPWN, PLACE_SCATTER);
	if (currlevel == 19) {
		AddMonsterType(MT_HORKSPWN, PLACE_SCATTER);
		AddMonsterType(MT_HORKDMN, PLACE_UNIQUE);
	}
	if (currlevel == 20)
		AddMonsterType(MT_DEFILER, PLACE_UNIQUE);
	if (currlevel == 24) {
		AddMonsterType(MT_ARCHLICH, PLACE_SCATTER);
		AddMonsterType(MT_NAKRUL, PLACE_SPECIAL);
	}

	if (!setlevel) {
		if (Quests[Q_BUTCHER].IsAvailable())
			AddMonsterType(MT_CLEAVER, PLACE_SPECIAL);
		if (Quests[Q_GARBUD].IsAvailable())
			AddMonsterType(UniqueMonsterType::Garbud, PLACE_UNIQUE);
		if (Quests[Q_ZHAR].IsAvailable())
			AddMonsterType(UniqueMonsterType::Zhar, PLACE_UNIQUE);
		if (Quests[Q_LTBANNER].IsAvailable())
			AddMonsterType(UniqueMonsterType::SnotSpill, PLACE_UNIQUE);
		if (Quests[Q_VEIL].IsAvailable())
			AddMonsterType(UniqueMonsterType::Lachdan, PLACE_UNIQUE);
		if (Quests[Q_WARLORD].IsAvailable())
			AddMonsterType(UniqueMonsterType::WarlordOfBlood, PLACE_UNIQUE);

		if (UseMultiplayerQuests() && currlevel == Quests[Q_SKELKING]._qlevel) {

			AddMonsterType(MT_SKING, PLACE_UNIQUE);

			int skeletonTypeCount = 0;
			_monster_id skeltypes[NUM_MTYPES];
			for (_monster_id skeletonType : SkeletonTypes) {
				if (!IsMonsterAvalible(MonstersData[skeletonType]))
					continue;

				skeltypes[skeletonTypeCount++] = skeletonType;
			}
			AddMonsterType(skeltypes[GenerateRnd(skeletonTypeCount)], PLACE_SCATTER);
		}

		_monster_id typelist[MaxMonsters];

		int nt = 0;
		for (int i = MT_NZOMBIE; i < NUM_MTYPES; i++) {
			if (!IsMonsterAvalible(MonstersData[i]))
				continue;

			typelist[nt++] = (_monster_id)i;
		}

		while (nt > 0 && LevelMonsterTypeCount < MaxLvlMTypes && monstimgtot < 4000) {
			for (int i = 0; i < nt;) {
				if (MonstersData[typelist[i]].image > 4000 - monstimgtot) {
					typelist[i] = typelist[--nt];
					continue;
				}

				i++;
			}

			if (nt != 0) {
				int i = GenerateRnd(nt);
				AddMonsterType(typelist[i], PLACE_SCATTER);
				typelist[i] = typelist[--nt];
			}
		}
	} else if (oracool::IsRiftLevel(setlvlnum)) {
		// The SAME roster on every build of this floor: a revisit restores each saved monster's type
		// INDEX, so a re-rolled list would dress the survivors in other types' sprites and animation
		// lengths (audit, 2026-09-20).
		SetRndSeed(oracool::RiftRosterSeed());
		// A rift floor is generated, so no .dun ran SetMapMonsters: the golem bodies a set level
		// gets from there are added here, or the companions and the Golem spell have no slot.
		for (int i = 0; i < MAX_PLRS; i++)
			AddMonster(GolemHoldingCell, Direction::South, 0, false);

		// The guardian's type, loaded now so his sprites are in memory when the bar fills. The
		// Skeleton King also wants a skeleton type on the floor to raise.
		switch (oracool::RiftGuardian()) {
		case oracool::RiftGuardianType::SkeletonKing: {
			AddMonsterType(MT_SKING, PLACE_SPECIAL);
			_monster_id skeltypes[NUM_MTYPES];
			int skeletonTypeCount = 0;
			for (_monster_id skeletonType : SkeletonTypes) {
				if (MonstersData[skeletonType].availability == MonsterAvailability::Never)
					continue;
				skeltypes[skeletonTypeCount++] = skeletonType;
			}
			// Raised by the King only, not scattered: the twelve skeleton types span floors 1-6, and a Hell-band rift
			// scattered floor-one skeletons as cheap kill-bar credit (round 13 audit). AddSkeleton finds them by kind.
			if (skeletonTypeCount > 0)
				AddMonsterType(skeltypes[GenerateRnd(skeletonTypeCount)], PLACE_SPECIAL);
		} break;
		case oracool::RiftGuardianType::Butcher:
			AddMonsterType(MT_CLEAVER, PLACE_SPECIAL);
			break;
		case oracool::RiftGuardianType::Diablo:
			AddMonsterType(MT_DIABLO, PLACE_SPECIAL);
			break;
		case oracool::RiftGuardianType::NaKrul:
			AddMonsterType(MT_NAKRUL, PLACE_SPECIAL);
			break;
		}

		// The roster (plan r2): a mix from the WHOLE game in the tier's band - a Caves floor full of
		// Cathedral skeletons and Hell knights - under the same image budget a floor has.
		_monster_id typelist[NUM_MTYPES];
		int nt = 0;
		for (int i = MT_NZOMBIE; i < NUM_MTYPES; i++) {
			const auto id = static_cast<_monster_id>(i);
			if (IsAnyOf(id, MT_GOLEM, MT_DIABLO, MT_NAKRUL, MT_SKING, MT_CLEAVER))
				continue;
			if (!oracool::RiftAcceptsMonster(MonstersData[i]))
				continue;
			typelist[nt++] = id;
		}
		constexpr int RiftRosterSize = 5;
		int picked = 0;
		while (nt > 0 && picked < RiftRosterSize && LevelMonsterTypeCount < MaxLvlMTypes && monstimgtot < 4000) {
			for (int i = 0; i < nt;) {
				if (MonstersData[typelist[i]].image > 4000 - monstimgtot) {
					typelist[i] = typelist[--nt];
					continue;
				}
				i++;
			}
			if (nt == 0)
				break;
			const int i = GenerateRnd(nt);
			AddMonsterType(typelist[i], PLACE_SCATTER);
			typelist[i] = typelist[--nt];
			picked++;
		}
	} else {
		if (setlvlnum == SL_SKELKING) {
			AddMonsterType(MT_SKING, PLACE_UNIQUE);
		}
	}
}

void InitMonsterSND(CMonster &monsterType)
{
	if (!gbSndInited)
		return;

	const char *prefixes[] {
		"a", // Attack
		"h", // Hit
		"d", // Death
		"s", // Special
	};

	const MonsterData &data = MonstersData[monsterType.type];
	string_view soundSuffix = data.soundSuffix != nullptr ? data.soundSuffix : data.assetsSuffix;

	for (int i = 0; i < 4; i++) {
		string_view prefix = prefixes[i];
		if (prefix == "s" && !data.hasSpecialSound)
			continue;

		for (int j = 0; j < 2; j++) {
			char path[64];
			*BufCopy(path, "monsters\\", soundSuffix, prefix, j + 1, ".wav") = '\0';
			monsterType.sounds[i][j] = sound_file_load(path);
		}
	}
}

void InitMonsterGFX(CMonster &monsterType)
{
	const _monster_id mtype = monsterType.type;
	const MonsterData &monsterData = MonstersData[mtype];
	const size_t numAnims = GetNumAnims(monsterData);
	const auto hasAnim = [&monsterData](size_t index) {
		return monsterData.frames[index] != 0;
	};
	constexpr size_t MaxAnims = 6;
	std::array<uint32_t, MaxAnims + 1> animOffsets;
	if (!HeadlessMode) {
		monsterType.animData = MultiFileLoader<MaxAnims> {}(
		    numAnims,
		    FileNameWithCharAffixGenerator({ "monsters\\", monsterData.assetsSuffix }, DEVILUTIONX_CL2_EXT, Animletter),
		    animOffsets.data(),
		    hasAnim);
	}

#ifndef UNPACKED_MPQS
	if (!HeadlessMode) {
		// Convert CL2 to CLX:
		std::vector<std::vector<uint8_t>> clxData;
		size_t accumulatedSize = 0;
		for (size_t i = 0, j = 0; i < numAnims; ++i) {
			if (!hasAnim(i))
				continue;
			const uint32_t begin = animOffsets[j];
			const uint32_t end = animOffsets[j + 1];
			clxData.emplace_back();
			Cl2ToClx(reinterpret_cast<uint8_t *>(&monsterType.animData[begin]), end - begin,
			    PointerOrValue<uint16_t> { monsterData.width }, clxData.back());
			animOffsets[j] = accumulatedSize;
			accumulatedSize += clxData.back().size();
			++j;
		}
		animOffsets[clxData.size()] = accumulatedSize;
		monsterType.animData = nullptr;
		monsterType.animData = std::unique_ptr<byte[]>(new byte[accumulatedSize]);
		for (size_t i = 0; i < clxData.size(); ++i) {
			memcpy(&monsterType.animData[animOffsets[i]], clxData[i].data(), clxData[i].size());
		}
	}
#endif

	for (size_t i = 0, j = 0; i < numAnims; ++i) {
		AnimStruct &anim = monsterType.anims[i];
		if (!hasAnim(i)) {
			anim.frames = 0;
			continue;
		}
		anim.frames = monsterData.frames[i];
		anim.rate = monsterData.rate[i];
		anim.width = monsterData.width;
		if (!HeadlessMode) {
			const uint32_t begin = animOffsets[j];
			const uint32_t end = animOffsets[j + 1];
			auto spritesData = reinterpret_cast<uint8_t *>(&monsterType.animData[begin]);
			const uint16_t numLists = GetNumListsFromClxListOrSheetBuffer(spritesData, end - begin);
			anim.sprites = ClxSpriteListOrSheet { spritesData, numLists };
		}
		++j;
	}

	monsterType.data = &monsterData;

	if (HeadlessMode)
		return;

	if (monsterData.trnFile != nullptr) {
		InitMonsterTRN(monsterType);
	}

	if (IsAnyOf(mtype, MT_NMAGMA, MT_YMAGMA, MT_BMAGMA, MT_WMAGMA))
		GetMissileSpriteData(MissileGraphicID::MagmaBall).LoadGFX();
	if (IsAnyOf(mtype, MT_STORM, MT_RSTORM, MT_STORML, MT_MAEL))
		GetMissileSpriteData(MissileGraphicID::ThinLightning).LoadGFX();
	if (mtype == MT_SNOWWICH) {
		GetMissileSpriteData(MissileGraphicID::BloodStarBlue).LoadGFX();
		GetMissileSpriteData(MissileGraphicID::BloodStarBlueExplosion).LoadGFX();
	}
	if (mtype == MT_HLSPWN) {
		GetMissileSpriteData(MissileGraphicID::BloodStarRed).LoadGFX();
		GetMissileSpriteData(MissileGraphicID::BloodStarRedExplosion).LoadGFX();
	}
	if (mtype == MT_SOLBRNR) {
		GetMissileSpriteData(MissileGraphicID::BloodStarYellow).LoadGFX();
		GetMissileSpriteData(MissileGraphicID::BloodStarYellowExplosion).LoadGFX();
	}
	if (IsAnyOf(mtype, MT_NACID, MT_RACID, MT_BACID, MT_XACID, MT_SPIDLORD)) {
		GetMissileSpriteData(MissileGraphicID::Acid).LoadGFX();
		GetMissileSpriteData(MissileGraphicID::AcidSplat).LoadGFX();
		GetMissileSpriteData(MissileGraphicID::AcidPuddle).LoadGFX();
	}
	if (mtype == MT_LICH) {
		GetMissileSpriteData(MissileGraphicID::OrangeFlare).LoadGFX();
		GetMissileSpriteData(MissileGraphicID::OrangeFlareExplosion).LoadGFX();
	}
	if (mtype == MT_ARCHLICH) {
		GetMissileSpriteData(MissileGraphicID::YellowFlare).LoadGFX();
		GetMissileSpriteData(MissileGraphicID::YellowFlareExplosion).LoadGFX();
	}
	if (IsAnyOf(mtype, MT_PSYCHORB, MT_BONEDEMN))
		GetMissileSpriteData(MissileGraphicID::BlueFlare2).LoadGFX();
	if (mtype == MT_NECRMORB) {
		GetMissileSpriteData(MissileGraphicID::RedFlare).LoadGFX();
		GetMissileSpriteData(MissileGraphicID::RedFlareExplosion).LoadGFX();
	}
	if (mtype == MT_PSYCHORB)
		GetMissileSpriteData(MissileGraphicID::BlueFlareExplosion).LoadGFX();
	if (mtype == MT_BONEDEMN)
		GetMissileSpriteData(MissileGraphicID::BlueFlareExplosion2).LoadGFX();
	if (mtype == MT_DIABLO)
		GetMissileSpriteData(MissileGraphicID::DiabloApocalypseBoom).LoadGFX();
}

void WeakenNaKrul()
{
	if (currlevel != 24 || static_cast<size_t>(UberDiabloMonsterIndex) >= ActiveMonsterCount)
		return;

	auto &monster = Monsters[UberDiabloMonsterIndex];
	PlayEffect(monster, MonsterSound::Death);
	monster.armorClass -= 50;
	int hp = monster.maxHitPoints / 2;
	monster.resistance = 0;
	monster.hitPoints = hp;
	monster.maxHitPoints = hp;
}

bool LevelHasGolemSlots()
{
	// Town never runs InitGolems and has no slot. A set level DOES have its four: SetMapMonsters adds the
	// MT_GOLEM type and four bodies at GolemHoldingCell while `setlevel` is true (that is why InitGolems
	// itself adds nothing there) - the v1.12.012 note that a set level "adds no slot" read InitGolems
	// alone and was wrong, and it kept every companion and the Golem spell out of the Skeleton King's
	// lair, the Chamber of Bone and Lazarus' (audit, 2026-09-19).
	return leveltype != DTYPE_TOWN;
}

void InitGolems()
{
	if (!setlevel) {
		for (int i = 0; i < MAX_PLRS; i++)
			AddMonster(GolemHoldingCell, Direction::South, 0, false);
	}
}

void InitMonsters()
{
	// The chill, passive-mark, RfA-12 and warcry clears that stood here are in InitLevelMonsters since 2026-09-27: this does
	// not run in town, and a New Game starts there.

	if (!gbIsSpawn && !setlevel && currlevel == 16)
		LoadDiabMonsts();

	int nt = numtrigs;
	if (currlevel == 15)
		nt = 1;
	for (int i = 0; i < nt; i++) {
		for (int s = -2; s < 2; s++) {
			for (int t = -2; t < 2; t++)
				DoVision(trigs[i].position + Displacement { s, t }, 15, MAP_EXP_NONE, false);
		}
	}
	if (!gbIsSpawn)
		PlaceQuestMonsters();
	// D2MXL-to-ORCL Phase 4: a named encounter runs on a SET level, so it goes on this side of the
	// branch - the scatter below is what `!setlevel` is guarding, and an encounter arena is
	// deliberately not scattered.
	PlaceNamedEncounter();
	PlaceRiftMonsters(); // a rift floor is a set level too, and wants a floor's population (oracool/rift.h)
	if (!setlevel) {
		if (!gbIsSpawn)
			PlaceUniqueMonsters();
		int na = 0;
		for (int s = 16; s < 96; s++) {
			for (int t = 16; t < 96; t++) {
				if (!IsTileSolid({ s, t }))
					na++;
			}
		}
		// Oracool: lesser uniques go in BEFORE the scatter, deliberately. They and their escorts draw
		// on the same MaxMonsters pool, and the scatter's own clamp a few lines down is what keeps the
		// total inside it - so the champions must already be counted in ActiveMonsterCount when that
		// clamp runs, or a dense level would overrun the pool and lose monsters at random.
		PlaceLesserUniques();
		// And the floor's boss, before the scatter for the same pool-accounting reason.
		PlaceEndgameBoss();

		int numplacemonsters = na / 30;
		if (gbIsMultiplayer)
			numplacemonsters += numplacemonsters / 2;
		// Oracool: user request (2026-08-15) - a monster density dial in the INI, 1x to 3x. Applied
		// to the SCATTERED count only: quest monsters and the named uniques are placed above by their
		// own rules and are not a density question - there is one Butcher whatever this says.
		//
		// Multiplied before the clamp below, deliberately, so the engine's own ceiling on live
		// monsters stays the last word. At 3x a large level reaches that ceiling rather than
		// overrunning it, which is why this needs no separate cap of its own.
		numplacemonsters = numplacemonsters * *sgOptions.Oracool.monsterDensityPercent / 100;
		// Room held back for the theme rooms, populated after this: at the default densities the scatter filled to the
		// cap and every theme room shared the last ten slots - most stood empty (round 13 audit, v1.12.238).
		const size_t themeReserve = std::min<size_t>(static_cast<size_t>(std::max(numthemes, 0)) * 8, 40);
		const size_t scatterCap = MaxEnemyMonsters - 10 - themeReserve;
		if (ActiveMonsterCount >= scatterCap)
			numplacemonsters = 0;
		else if (ActiveMonsterCount + numplacemonsters > scatterCap)
			numplacemonsters = static_cast<int>(scatterCap - ActiveMonsterCount);
		totalmonsters = ActiveMonsterCount + numplacemonsters;
		int numscattypes = 0;
		size_t scattertypes[NUM_MTYPES];
		for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
			if ((LevelMonsterTypes[i].placeFlags & PLACE_SCATTER) != 0) {
				scattertypes[numscattypes] = i;
				numscattypes++;
			}
		}
		// numscattypes CAN be zero - a level whose types all lack PLACE_SCATTER - and
		// GenerateRnd(0) returns 0, so the old code would then read scattertypes[0] uninitialised
		// and index LevelMonsterTypes with whatever was on the stack. Nothing to scatter is a
		// legitimate state; scattering garbage is not.
		while (numscattypes > 0 && ActiveMonsterCount < totalmonsters) {
			const size_t before = ActiveMonsterCount;
			const size_t typeIndex = scattertypes[GenerateRnd(numscattypes)];
			if (currlevel == 1 || FlipCoin())
				na = 1;
			else if (currlevel == 2 || leveltype == DTYPE_CRYPT)
				na = GenerateRnd(2) + 2;
			else
				na = GenerateRnd(3) + 3;
			PlaceGroup(typeIndex, na);
			// PlaceGroup gives up when the floor has no room left (see its own bound). Without this
			// the loop would ask it again forever, having moved the spin one level up rather than
			// removing it - the level simply ends up less full than the density dial asked for.
			if (ActiveMonsterCount == before)
				break;
		}
	}
	for (int i = 0; i < nt; i++) {
		for (int s = -2; s < 2; s++) {
			for (int t = -2; t < 2; t++)
				DoUnVision(trigs[i].position + Displacement { s, t }, 15);
		}
	}
}

void SetMapMonsters(const uint16_t *dunData, Point startPosition)
{
	AddMonsterType(MT_GOLEM, PLACE_SPECIAL);
	if (setlevel)
		for (int i = 0; i < MAX_PLRS; i++)
			AddMonster(GolemHoldingCell, Direction::South, 0, false);

	int width = SDL_SwapLE16(dunData[0]);
	int height = SDL_SwapLE16(dunData[1]);

	int layer2Offset = 2 + width * height;

	// The rest of the layers are at dPiece scale
	width *= 2;
	height *= 2;

	const uint16_t *monsterLayer = &dunData[layer2Offset + width * height];

	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			auto monsterId = static_cast<uint8_t>(SDL_SwapLE16(monsterLayer[j * width + i]));
			if (monsterId != 0) {
				const size_t typeIndex = AddMonsterType(MonstConvTbl[monsterId - 1], PLACE_SPECIAL);
				PlaceMonster(ActiveMonsterCount++, typeIndex, startPosition + Displacement { i, j });
			}
		}
	}
}

bool EnemyMonsterRoomLeft(size_t wanted)
{
	return ActiveMonsterCount + wanted <= MaxMonsters
	    && ActiveMonsterCount - std::min(oracool::ActiveMinionBodies(), ActiveMonsterCount) + wanted <= MaxEnemyMonsters;
}

bool CanAddMinionBody(_monster_id type)
{
	if (leveltype == DTYPE_TOWN || ActiveMonsterCount >= MaxMonsters)
		return false;
	return GetMonsterTypeIndex(type) < LevelMonsterTypeCount || LevelMonsterTypeCount < MaxLvlMTypes;
}

Monster *AddMinionBody(Point position, Direction dir, _monster_id type)
{
	if (!CanAddMinionBody(type) || !InDungeonBounds(position) || !IsTileAvailable(position))
		return nullptr;
	// Loaded on demand rather than with every level (as MT_GOLEM is): a type added at level load counts against the
	// level's sprite budget and would change which monsters a floor rolls - for every hero, army or not.
	const bool fresh = GetMonsterTypeIndex(type) >= LevelMonsterTypeCount;
	const size_t typeIndex = AddMonsterType(type, PLACE_SPECIAL);
	if (fresh)
		LevelMonsterTypes[typeIndex].minionOnly = true; // the floor never rolled it: no enemy spawns as it (round 66 audit)
	Monster &monster = Monsters[ActiveMonsters[ActiveMonsterCount++]];
	dMonster[position.x][position.y] = static_cast<int16_t>(monster.getId() + 1);
	// Not ordinary: a minion takes no variant (audit, 2026-09-27) - a "Hollow Skeleton" in the army, re-rolled every level,
	// and a Luminous one lit a light nothing ever freed.
	InitMonster(monster, dir, typeIndex, position, /*ordinary=*/false);
	// What makes it a minion to the rest of the engine: every damage path, the cursor and UpdateEnemy ask this flag.
	monster.flags |= MFLAG_GOLEM;
	// And not hidden: a Revived Stalker or Unseen kept its type's MFLAG_HIDDEN, which only the Sneak AI clears, and
	// fought its whole life unseen (round 14 audit, v1.12.239).
	monster.flags &= ~(MFLAG_TARGETS_MONSTER | MFLAG_BERSERK | MFLAG_HIDDEN);
	monster.enemy = 0;
	monster.activeForTicks = UINT8_MAX;
	monster.leaderRelation = LeaderRelation::None;
	M_StartStand(monster, dir);
	return &monster;
}

Monster *AddMonster(Point position, Direction dir, size_t typeIndex, bool inMap)
{
	if (EnemyMonsterRoomLeft()) {
		Monster &monster = Monsters[ActiveMonsters[ActiveMonsterCount++]];
		if (inMap)
			dMonster[position.x][position.y] = monster.getId() + 1;
		InitMonster(monster, dir, typeIndex, position);
		return &monster;
	}

	return nullptr;
}

Monster *SpawnRiftGuardian()
{
	if (MyPlayer == nullptr || !EnemyMonsterRoomLeft())
		return nullptr;
	const Point hero = MyPlayer->position.tile;

	// Open ground near the hero (plan r4): the nearest ring of tiles from three out that can take a
	// monster. Close enough to be the event, far enough not to land on the hero's toes. NOT
	// CanPlaceMonster: that one refuses any tile the hero can SEE, which is the point here (audit,
	// 2026-09-20 - in a lit room nothing within twelve tiles passed it and he never rose).
	// Out to forty tiles, not twelve (2026-09-20): a hero in a tight Hell corridor had nothing within
	// twelve that passed, and the guardian never rose - "Diablo didn't spawn when i hit 100%".
	// First a tile the hero can see, so he rises in this room and not the next one over a wall (round 48 audit); only when
	// none qualifies, any open tile, as before.
	std::optional<Point> spot;
	for (const bool sighted : { true, false }) {
		for (int radius = 3; radius <= 40 && !spot; radius++) {
			for (int dx = -radius; dx <= radius && !spot; dx++) {
				for (int dy = -radius; dy <= radius; dy++) {
					if (std::max(std::abs(dx), std::abs(dy)) != radius)
						continue;
					const Point candidate = hero + Displacement { dx, dy };
					if (InDungeonBounds(candidate) && dMonster[candidate.x][candidate.y] == 0 && dPlayer[candidate.x][candidate.y] == 0
					    && !TileContainsSetPiece(candidate) && !IsTileOccupied(candidate) && (!sighted || LineClearMissile(hero, candidate))) {
						spot = candidate;
						break;
					}
				}
			}
		}
		if (spot)
			break;
	}
	if (!spot)
		return nullptr;

	_monster_id type = MT_SKING;
	std::optional<UniqueMonsterType> unique;
	switch (oracool::RiftGuardian()) {
	case oracool::RiftGuardianType::SkeletonKing:
		type = MT_SKING;
		unique = UniqueMonsterType::SkeletonKing;
		break;
	case oracool::RiftGuardianType::Butcher:
		type = MT_CLEAVER;
		unique = UniqueMonsterType::Butcher;
		break;
	case oracool::RiftGuardianType::Diablo:
		type = MT_DIABLO; // no unique row: floor 16 stamps him from its map, and so does this
		break;
	case oracool::RiftGuardianType::NaKrul:
		type = MT_NAKRUL;
		unique = UniqueMonsterType::NaKrul;
		break;
	}
	const size_t typeIndex = GetMonsterTypeIndex(type);
	if (typeIndex == LevelMonsterTypeCount)
		return nullptr; // GetLevelMTypes did not load him - nothing to raise

	// AddMonster's body with ordinary=false: a guardian in the making takes no variant, exactly as
	// a placed unique takes none (the v1.12.052 audit's rule).
	Monster &monster = Monsters[ActiveMonsters[ActiveMonsterCount++]];
	dMonster[spot->x][spot->y] = monster.getId() + 1;
	InitMonster(monster, GetDirection(*spot, hero), typeIndex, *spot, /*ordinary=*/false);
	if (unique) {
		size_t minionType = typeIndex;
		if (*unique == UniqueMonsterType::SkeletonKing) {
			for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
				if (IsSkel(LevelMonsterTypes[i].type) && !LevelMonsterTypes[i].minionOnly) { // not the army's kind (round 66)
					minionType = i;
					break;
				}
			}
		}
		PrepareUniqueMonst(monster, *unique, minionType, 0, UniqueMonstersData[static_cast<size_t>(*unique)]);
		// No guardian is immune to fire, lightning and magic at once: Na-Krul until floor 24's books weaken him, and a rift
		// has none (round 32 audit); the Skeleton King on Hell and Torment, whose Hell row is all three (round 33 audit).
		// Resistant instead, for every guardian: a caster could not touch him, and the rift could only be abandoned.
		// Only the all-three case, and a poison immunity kept (round 34 audit: every single immunity went, and acid with no
		// resistance put back).
		constexpr uint16_t AllThree = IMMUNE_MAGIC | IMMUNE_FIRE | IMMUNE_LIGHTNING;
		if ((monster.resistance & AllThree) == AllThree)
			monster.resistance = oracool::DemoteImmunitiesToResistances(monster.resistance) | (monster.resistance & IMMUNE_ACID);
	}
	// No corpse entry of his own - he rises after InitCorpses - so none inherited either: the slot's last unique's id drew
	// his body over a dead champion's, or drew none (round 20 audit, v1.12.245). His type's plain body is his.
	monster.corpseId = 0;
	oracool::ScaleRiftMonster(monster);
	return &monster;
}

void AddDoppelganger(Monster &monster)
{
	Point target = { 0, 0 };
	for (int d = 0; d < 8; d++) {
		const Point position = monster.position.tile + static_cast<Direction>(d);
		if (!IsTileAvailable(position))
			continue;
		target = position;
	}
	if (target != Point { 0, 0 }) {
		const size_t typeIndex = GetMonsterTypeIndex(monster.type().type);
		Monster *clone = AddMonster(target, monster.direction, typeIndex, true);
		// A rift's clone fights at the rift's scale: at base stats it died in a hit and filled the kill bar as a full
		// monster (round 22 audit, v1.12.247).
		if (clone != nullptr && oracool::InRift())
			oracool::ScaleRiftMonster(*clone);
	}
}

void ApplyMonsterDamage(DamageType damageType, Monster &monster, int damage)
{
	// A companion has life and resistances (user, 2026-09-14: "make her have health instead of invulnerable"). Every
	// blow on a monster lands here, so its resistances are taken here.
	if (oracool::IsCompanion(monster)) {
		damage = oracool::CompanionDamageTaken(monster, damageType, damage);
		if (damage <= 0)
			return;
	}
	// A minion of the army likewise: Summon Resist, and the Fire Golem drinking fire (oracool/minions.h).
	if (oracool::IsMinion(monster)) {
		damage = oracool::MinionDamageTaken(monster, damageType, damage);
		if (damage <= 0)
			return;
	}
	// And a cursed one takes more (oracool/curses.h: Amplify Damage, Lower Resist, Decrepify, Doom).
	damage = oracool::CurseDamageTaken(monster, damageType, damage);
	AddFloatingNumber(damageType, monster, damage);

	// The time-to-kill clock starts HERE, where damage lands, not in M_StartHit where the monster
	// STAGGERS (2026-08-21). M_StartHit is the reaction to a blow that did not kill; a monster
	// killed outright by its first hit never reaches it, so its kill row reported a time of zero.
	//
	// The first read-back of this file found 303 of 360 kills doing exactly that - 84% of the
	// headline metric blank, and blank in a biased way, because the fastest kills are precisely the
	// ones that skip the stagger. Recording at the point of damage makes a one-shot kill report a
	// short time instead of no time.
	oracool::TelemetryRecordFirstHit(monster);

	monster.hitPoints -= damage;
	// Frailty: below a sliver, it simply dies (oracool/curses.h).
	if (oracool::CurseFinishes(monster, monster.hitPoints, monster.maxHitPoints))
		monster.hitPoints = 0;

	if (monster.hitPoints >> 6 <= 0) {
		delta_kill_monster(monster, monster.position.tile, *MyPlayer);
		NetSendCmdLocParam1(false, CMD_MONSTDEATH, monster.position.tile, monster.getId());
		return;
	}

	delta_monster_hp(monster, *MyPlayer);
	NetSendCmdMonDmg(false, monster.getId(), damage);
}

bool M_Talker(const Monster &monster)
{
	return IsAnyOf(monster.ai, MonsterAIID::Lazarus, MonsterAIID::Warlord, MonsterAIID::Gharbad, MonsterAIID::Zhar, MonsterAIID::Snotspill, MonsterAIID::Lachdanan, MonsterAIID::LazarusSuccubus);
}

void M_StartStand(Monster &monster, Direction md)
{
	ClearMVars(monster);
	// The Golem has no stand sheet and stands on its walk. A slot dressed in hero sheets (any companion) has a
	// real stand - user, 2026-09-14: "when she is standing in one place she is still looping walking animation".
	if (monster.type().type == MT_GOLEM && !oracool::IsCompanion(monster))
		NewMonsterAnim(monster, MonsterGraphic::Walk, md);
	else
		NewMonsterAnim(monster, MonsterGraphic::Stand, md);
	monster.var1 = static_cast<int>(monster.mode);
	monster.var2 = 0;
	monster.mode = MonsterMode::Stand;
	monster.position.future = monster.position.tile;
	monster.position.old = monster.position.tile;
	UpdateEnemy(monster);
}

void M_ClearSquares(const Monster &monster)
{
	for (Point searchTile : PointsInRectangle(Rectangle { monster.position.old, 1 })) {
		if (FindMonsterAtPosition(searchTile) == &monster)
			dMonster[searchTile.x][searchTile.y] = 0;
	}
}

void M_GetKnockback(Monster &monster)
{
	// The one place the player pushes a monster, and therefore the one place immunity to being
	// pushed can mean anything. Relentless champions and Implacable bosses stand their ground here
	// - which is what both of them always claimed to do. See oracool::IsKnockbackImmune for the
	// inversion this replaces.
	if (oracool::IsKnockbackImmune(monster))
		return;

	Direction dir = Opposite(monster.direction);
	if (!IsRelativeMoveOK(monster, monster.position.old, dir)) {
		return;
	}

	M_ClearSquares(monster);
	monster.position.old += dir;
	StartMonsterGotHit(monster);
}

void M_StartHit(Monster &monster, int dam)
{
	PlayEffect(monster, MonsterSound::Hit);

	// A stunned monster does not flinch out of its stun: a hard hit set HitRecovery over it, so Paralysis - stunning inside
	// the very hit that then flinched - was a flinch, and every stun ended at the next blow (round 19 audit, v1.12.244).
	if (IsHardHit(monster, dam) && !IsMonsterStunned(monster)) {
		if (monster.type().type == MT_BLINK) {
			Teleport(monster);
		} else if (IsAnyOf(monster.type().type, MT_NSCAV, MT_BSCAV, MT_WSCAV, MT_YSCAV, MT_GRAVEDIG)) {
			monster.goal = MonsterGoal::Normal;
			monster.goalVar1 = 0;
			monster.goalVar2 = 0;
		}
		if (monster.mode != MonsterMode::Petrified) {
			StartMonsterGotHit(monster);
		}
	}
}

/**
 * @brief The retreat the fork's repels ask for (MonsterGoal::Retreat: Howl, Grim Ward, Sanctuary, Blinding Flash), for
 * the AIs that never read it. Only Fallen, bats, gargoyles, the Sneak and the Counselors (Zhar and Lazarus with them)
 * ever acted on Retreat - it was vanilla's Fallen fear. Every other AI stood frozen at some ranges or walked the wrong
 * way, never cleared the goal, and lost its pathfinding for good (round 15 audit, v1.12.240). goalVar2 is the direction
 * to flee in, goalVar1 the steps left.
 */
void StartRepelRetreat(Monster &monster, Direction away, int steps)
{
	monster.goal = MonsterGoal::Retreat;
	monster.goalVar1 = static_cast<int16_t>(steps);
	monster.goalVar2 = static_cast<int8_t>(away);
	// Bats and sneaks have a retreat of their own on the same goal; the mark tells MonsterTakesRetreatStep this one is a
	// repel. goalVar3 is unused by both AIs.
	if (IsAnyOf(monster.ai, MonsterAIID::Bat, MonsterAIID::Sneak))
		monster.goalVar3 = RepelRetreatMark;
}

bool MonsterTakesRetreatStep(Monster &monster)
{
	if (monster.goal != MonsterGoal::Retreat) {
		// A repel ended by something else - Taunt, the Siren - leaves no mark behind to cancel the monster's own next retreat
		// (round 50 audit).
		if (IsAnyOf(monster.ai, MonsterAIID::Bat, MonsterAIID::Sneak) && monster.goalVar3 == RepelRetreatMark)
			monster.goalVar3 = 0;
		return false;
	}
	// A bat or a sneak driven off by a repel takes the repel's steps (round 49 audit: their own retreat counts goalVar1 UP,
	// so Howl at rank 5 and more was cancelled on the spot and lower ranks fled less; bats only sidestepped once).
	const bool repelled = IsAnyOf(monster.ai, MonsterAIID::Bat, MonsterAIID::Sneak) && monster.goalVar3 == RepelRetreatMark;
	if (!repelled && IsAnyOf(monster.ai, MonsterAIID::Fallen, MonsterAIID::Bat, MonsterAIID::Sneak, MonsterAIID::Zhar, MonsterAIID::Lazarus))
		return false; // they run their own retreat
	// A gargoyle's own retreat is its low-life one, and ends in a heal: a repel (Howl, Grim Ward, Blinding Flash) sent a
	// healthy one there, and it healed to full (round 32 audit). A Counselor's own retreat fades out first, so it is its own
	// only while hidden: a visible one went straight to a fade-in and, inside a Grim Ward, looped it unhittable.
	if (monster.ai == MonsterAIID::Gargoyle && monster.hitPoints < monster.maxHitPoints / 2)
		return false;
	if (monster.ai == MonsterAIID::Counselor && (monster.flags & MFLAG_HIDDEN) != 0)
		return false;
	if (monster.mode != MonsterMode::Stand)
		return repelled; // mid-step: its own AI, which reads goalVar1 the other way, waits for the repel to end
	const auto away = static_cast<Direction>(monster.goalVar2 & 7);
	const Point from = monster.position.tile - Displacement(away);
	if (monster.goalVar1 <= 0 || !MonsterStepAwayFrom(monster, from)) {
		monster.goal = MonsterGoal::Normal;
		monster.goalVar1 = 0;
		monster.goalVar2 = 0;
		if (repelled)
			monster.goalVar3 = 0;
		return false;
	}
	monster.goalVar1--;
	return true;
}

void StunMonster(Monster &monster, int ticks)
{
	// A dead monster is not stunned (dev note, 2026-09-27: "some barb skills/warcries kill mobs but they remain active
	// practically immortal because i cant target them anymore but they keep attacking me"). War Cry struck, the blow
	// killed, and the stun that followed set MonsterMode::Delay over MonsterMode::Death: the death animation never ran,
	// the monster kept 0 life - so no targeting found it - and its AI woke when the stun ran out. Guarded here, once,
	// for every skill that stuns after it strikes.
	if (monster.mode == MonsterMode::Death || (monster.hitPoints >> 6) <= 0)
		return;
	// Not stone (its curse would end early and lose the mode saved under it) and not a charge in flight (round 15 audit).
	if (monster.mode == MonsterMode::Petrified || monster.mode == MonsterMode::Charge)
		return;
	// A walk in progress is undone first, as a flinch undoes it: the walk had marked its tiles in dMonster and only its own
	// end clears them, so a stun mid-step left a phantom tile nobody could enter and, sideways, a body missiles passed
	// through (round 15 audit, v1.12.240).
	if (monster.isWalking()) {
		monster.position.tile = monster.position.old;
		monster.position.future = monster.position.old;
		M_ClearSquares(monster);
		dMonster[monster.position.tile.x][monster.position.tile.y] = monster.getId() + 1;
		// Its light comes back with it: the step had already moved the glow to the tile it was headed for (round 17 audit).
		if (monster.lightId != NO_LIGHT) {
			ChangeLightXY(monster.lightId, monster.position.tile);
			ChangeLightOffset(monster.lightId, {});
		}
	}
	// AiDelay carries the guard this needs: Lazarus is exempt, because his scripted set-piece drives
	// his own mode and a stun would strand it. Inherited rather than restated.
	// The longer stun wins: a Sound Shock or a Paralysis proc cut a War Cry's two seconds to half of one (round 19).
	const int running = IsMonsterStunned(monster) ? monster.var2 : 0;
	// No special move waits under the stun: a perched or healing gargoyle kept MFLAG_ALLOW_SPECIAL, and the next swing
	// lifted it into its special attack mid-stun (round 24 audit).
	monster.flags &= ~(MFLAG_ALLOW_SPECIAL | MFLAG_LOCK_ANIMATION);
	AiDelay(monster, std::max(ticks, running));
	if (bool *stunned = StunFlagOf(monster); stunned != nullptr && monster.mode == MonsterMode::Delay)
		*stunned = true;
}

bool IsMonsterStunned(const Monster &monster)
{
	const bool *stunned = StunFlagOf(monster);
	return monster.mode == MonsterMode::Delay && stunned != nullptr && *stunned;
}

void M_StartHit(Monster &monster, const Player &player, int dam)
{
	monster.tag(player);
	oracool::NoteOwnerStruck(player, monster); // the companions' focus
	// Phase 0.9: the time-to-kill clock starts at the first player hit that connects.
	oracool::TelemetryRecordFirstHit(monster);
	if (IsHardHit(monster, dam)) {
		monster.enemy = player.getId();
		monster.enemyPosition = player.position.future;
		monster.flags &= ~MFLAG_TARGETS_MONSTER;
		if (monster.mode != MonsterMode::Petrified) {
			monster.direction = GetMonsterDirection(monster);
		}
	}

	M_StartHit(monster, dam);
}

namespace {

/**
 * @brief The per-kill socket grants: Tir's D2 mana rule, and the Skull's life-per-kill (its leech
 * substitute - see the gem table). Local player only (mirrors AddPlrMonstExper's locality); the
 * mana half honors the mana-steal path's NoMana guard.
 */
void GrantRuneKillMana(char pmask)
{
	if ((pmask & (1 << MyPlayerId)) == 0)
		return;
	Player &player = *MyPlayer;

	if (!HasAnyOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		const int mana = oracool::RuneManaPerKill(player) << 6; // mana runs in <<6 fixed point
		if (mana > 0) {
			player._pMana = std::min(player._pMana + mana, player._pMaxMana);
			player._pManaBase = std::min(player._pManaBase + mana, player._pMaxManaBase);
			RedrawComponent(PanelDrawComponent::Mana);
		}
	}

	const int life = oracool::GemLifePerKill(player) << 6; // HP runs in the same fixed point
	if (life > 0 && player._pHitPoints > 0) {
		player._pHitPoints = std::min(player._pHitPoints + life, player._pMaxHP);
		player._pHPBase = std::min(player._pHPBase + life, player._pMaxHPBase);
		RedrawComponent(PanelDrawComponent::Health);
	}
}

} // namespace

void MonsterDeath(Monster &monster, Direction md, bool sendmsg)
{
	oracool::OnCursedMonsterDeath(monster); // Death Mark bursts it, Essence Tap pays (oracool/curses.h)
	if (MyPlayer != nullptr)
		oracool::OnPassiveMonsterDied(*MyPlayer, monster); // Life from Death, whoever made the kill (round 13 audit)
	if (!monster.isPlayerMinion())
		AddPlrMonstExper(monster.level(sgGameInitInfo.nDifficulty), monster.exp(sgGameInitInfo.nDifficulty), monster.whoHit);

	if (!monster.isPlayerMinion()) // the army's own deaths are not kills of that kind (round 14 audit)
		MonsterKillCounts[monster.type().type]++;
	monster.hitPoints = 0;
	monster.flags &= ~MFLAG_HIDDEN;
	SetRndSeed(monster.rndItemSeed);

	if (monster.isUnique())
		oracool::LogEvent(fmt::format("Defeated {:s}", oracool::GetMonsterDisplayName(monster)));
	// Phase 0.9: one CSV row per real kill - minions grant no experience and tell no tuning story.
	if (!monster.isPlayerMinion()) {
		oracool::TelemetryRecordKill(monster);
		GrantRuneKillMana(monster.whoHit);
	}

	SpawnLoot(monster, sendmsg);

	// Oracool: AFTER the loot, and the reason is the line above SetRndSeed(monster.rndItemSeed).
	// That seed exists to make a monster's drop reproducible, and Thunderous spends 36 AddMissile
	// calls - each one drawing a random animation frame - so firing it first shifted the item stream
	// out from under SpawnLoot. Still deterministic, but it meant a champion's drop depended on which
	// modifier it happened to be wearing, which is a coupling with nothing to recommend it.
	//
	// Both happen in the same tick, so the ordering the earlier comment defended - the discharge
	// reading as part of the kill - is not something the player can perceive either way.
	oracool::OnLesserUniqueKilled(monster);

	// D2MXL-to-ORCL Phase 2: killing a Dread boss is a milestone. Claimed here rather than from the
	// loot path, because the reward is for the KILL - a boss that dropped nothing still counts, and
	// tying it to the drop would make the milestone depend on a roll.
	if (oracool::IsEndgameBoss(monster) && MyPlayer != nullptr) {
		// The charms grow with it at once - not at the next gear change (round 10 audit) - but only when it was claimed and the
		// hero lives: a recalculation over a dying hero gave the corpse life (round 44 audit). Respawn recalculates anyway.
		if (oracool::ClaimMilestone(*MyPlayer, oracool::Milestone::SlayDreadBoss) && MyPlayer->_pmode != PM_DEATH)
			CalcPlrInv(*MyPlayer, true);
	}

	// Phase 4: a named encounter's guardian pays its reward here. GUARANTEED - the map said what it
	// carries, and a roll would make that a lie. Does nothing off an encounter level.
	oracool::AwardNamedEncounter(monster);
	// The rift's kill bar, and its guardian's fall opening the way back (oracool/rift.h).
	oracool::OnRiftMonsterKilled(monster);

	// Diablo's death is the game's end everywhere but in a rift, where he is the guardian and dies
	// like the Butcher does (oracool/rift.h).
	if (monster.type().type == MT_DIABLO && !oracool::InRift())
		DiabloDeath(monster, true);
	else
		PlayEffect(monster, MonsterSound::Death);

	if (monster.mode != MonsterMode::Petrified) {
		if (monster.type().type == MT_GOLEM)
			md = Direction::South;
		NewMonsterAnim(monster, MonsterGraphic::Death, md, gGameLogicStep < GameLogicStep::ProcessMonsters ? AnimationDistributionFlags::ProcessAnimationPending : AnimationDistributionFlags::None);
		monster.mode = MonsterMode::Death;
	} else if (monster.isUnique()) {
		AddUnLight(monster.lightId);
	}
	monster.goal = MonsterGoal::None;
	monster.var1 = 0;
	monster.position.tile = monster.position.old;
	monster.position.future = monster.position.old;
	// Its light too, killed mid-step: a Luminous or champion glow stayed a tile off the corpse (round 32 audit). Not a
	// petrified unique's, freed just above.
	if (monster.lightId != NO_LIGHT && monster.mode != MonsterMode::Petrified) {
		ChangeLightXY(monster.lightId, monster.position.tile);
		ChangeLightOffset(monster.lightId, {});
	}
	M_ClearSquares(monster);
	dMonster[monster.position.tile.x][monster.position.tile.y] = monster.getId() + 1;
	CheckQuestKill(monster, sendmsg);
	M_FallenFear(monster.position.tile);
	// Not a Revived one (round 61 audit: the puddle hurts only players - the hero, standing among his own army).
	if (IsAnyOf(monster.type().type, MT_NACID, MT_RACID, MT_BACID, MT_XACID, MT_SPIDLORD) && !monster.isPlayerMinion())
		AddMissile(monster.position.tile, { 0, 0 }, Direction::South, MissileID::AcidPuddle, TARGET_PLAYERS, monster.getId(), monster.intelligence + 1, 0);
	// Conversion ends with the body (round 67 audit): its flags stayed on the corpse, and the slot's delete then put out a
	// Luminous variant's own light as if the berserk one.
	oracool::RevertConversionOnDeath(monster);
}

void StartMonsterDeath(Monster &monster, const Player &player, bool sendmsg)
{
	// Once only, as M_SyncStartKill already asks: a strike inside a hit's hooks can kill the monster the hit then kills
	// again, and MonsterDeath paid the experience, the loot and the rift credit twice (round 3 audit, v1.12.229).
	if (monster.mode == MonsterMode::Death)
		return;
	monster.tag(player);
	// Oracool, Round 5: Rampage and Requiem hear the kill.
	if (&player == MyPlayer && monster.hitPoints >> 6 <= 0) {
		oracool::OnPassiveMonsterKilled(*MyPlayer, monster);
		oracool::OnRfa12MonsterKilled(*MyPlayer, monster);
	}
	Direction md = GetDirection(monster.position.tile, player.position.tile);
	MonsterDeath(monster, md, sendmsg);
}

void KillMyGolem()
{
	Monster &golem = Monsters[MyPlayerId];
	delta_kill_monster(golem, golem.position.tile, *MyPlayer);
	NetSendCmdLoc(MyPlayerId, false, CMD_KILLGOLEM, golem.position.tile);
	M_StartKill(golem, *MyPlayer);
}

void M_StartKill(Monster &monster, const Player &player)
{
	StartMonsterDeath(monster, player, true);
}

void M_SyncStartKill(Monster &monster, Point position, const Player &player)
{
	if (monster.hitPoints == 0 || monster.mode == MonsterMode::Death) {
		return;
	}

	if (dMonster[position.x][position.y] == 0) {
		M_ClearSquares(monster);
		monster.position.tile = position;
		monster.position.old = position;
	}

	StartMonsterDeath(monster, player, false);
}

void M_UpdateRelations(const Monster &monster)
{
	if (monster.hasLeashedMinions())
		ReleaseMinions(monster);

	ShrinkLeaderPacksize(monster);
}

void DoEnding()
{
	if (gbIsMultiplayer) {
		SNetLeaveGame(LEAVE_ENDING);
	}

	music_stop();

	if (gbIsMultiplayer) {
		SDL_Delay(1000);
	}

	if (gbIsSpawn)
		return;

	switch (MyPlayer->_pClass) {
	case HeroClass::Sorcerer:
	case HeroClass::Monk:
	case HeroClass::Necromancer:
		play_movie("gendata\\diabvic1.smk", false);
		break;
	case HeroClass::Warrior:
	case HeroClass::Barbarian:
		play_movie("gendata\\diabvic2.smk", false);
		break;
	default:
		play_movie("gendata\\diabvic3.smk", false);
		break;
	}
	play_movie("gendata\\diabend.smk", false);

	bool bMusicOn = gbMusicOn;
	gbMusicOn = true;

	int musicVolume = sound_get_or_set_music_volume(1);
	sound_get_or_set_music_volume(0);

	music_start(TMUSIC_CATACOMBS);
	loop_movie = true;
	play_movie("gendata\\loopdend.smk", true);
	loop_movie = false;
	music_stop();

	sound_get_or_set_music_volume(musicVolume);
	gbMusicOn = bMusicOn;
}

bool SuppressMonsterLights = false;

void RelightLoadedMonsters()
{
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		// The saved id named a slot of the first visit's pool; whatever holds it now is someone else's.
		monster.lightId = NO_LIGHT;
		if ((monster.hitPoints >> 6) <= 0)
			continue;
		if (monster.isUnique()) {
			if (monster.uniqueType != UniqueMonsterType::HorkDemon)
				monster.lightId = AddLight(monster.position.tile, 3);
		} else if (oracool::VariantOf(monster) == oracool::MonsterVariant::Luminous) {
			monster.lightId = AddLight(monster.position.tile, oracool::LuminousRadius);
		}
	}
}

void PrepDoEnding()
{
	gbSoundOn = sgbSaveSoundOn;
	MyPlayerIsDead = false;
	cineflag = true;

	Player &myPlayer = *MyPlayer;

	myPlayer.pDiabloKillLevel = std::max(myPlayer.pDiabloKillLevel, static_cast<uint8_t>(sgGameInitInfo.nDifficulty + 1));
	// Saved HERE, while the game still runs: SaveOnExit (and the autosave) refuse once gbRunGame is false, and the save
	// after the game loop always found it false - so the kill, its experience and the next difficulty's unlock were
	// never written (round 5 audit, v1.12.230).
	if (!gbIsMultiplayer)
		oracool::SaveOnExit();
	gbRunGame = false;

	for (Player &player : Players) {
		player._pmode = PM_QUIT;
		player._pInvincible = true;
		if (gbIsMultiplayer) {
			if (player._pHitPoints >> 6 == 0)
				player._pHitPoints = 64;
			if (player._pMana >> 6 == 0)
				player._pMana = 64;
		}
	}
}

bool Walk(Monster &monster, Direction md)
{
	if (!DirOK(monster, md)) {
		return false;
	}

	switch (md) {
	case Direction::North:
		WalkNorthwards(monster, -1, -1, Direction::North);
		break;
	case Direction::NorthEast:
		WalkNorthwards(monster, 0, -1, Direction::NorthEast);
		break;
	case Direction::East:
		WalkSideways(monster, -32, -16, 1, -1, 1, 0, Direction::East);
		break;
	case Direction::SouthEast:
		WalkSouthwards(monster, -32, -16, 1, 0, Direction::SouthEast);
		break;
	case Direction::South:
		WalkSouthwards(monster, 0, -32, 1, 1, Direction::South);
		break;
	case Direction::SouthWest:
		WalkSouthwards(monster, 32, -16, 0, 1, Direction::SouthWest);
		break;
	case Direction::West:
		WalkSideways(monster, 32, -16, -1, 1, 0, 1, Direction::West);
		break;
	case Direction::NorthWest:
		WalkNorthwards(monster, -1, 0, Direction::NorthWest);
		break;
	case Direction::NoDirection:
		break;
	}
	return true;
}

/**
 * @brief The crowd in a corridor (oracool/minions.h): a minion whose way is blocked by an IDLE minion trades tiles
 * with it. One body wide, a Cathedral passage would otherwise queue an army behind whichever skeleton reached the
 * door first - and the ones with something to fight would be the ones at the back. Only an idle body gives way, and
 * only to one that is going somewhere, so two idlers never swap back and forth.
 */
bool MinionTradesPlaces(Monster &mover, Direction toward)
{
	// "Going somewhere" is having something to fight. A minion merely drifting home waits its turn like anyone.
	if (!oracool::IsMinion(mover) || (mover.flags & MFLAG_TARGETS_MONSTER) == 0)
		return false;
	const Point from = mover.position.tile;
	const Point there = from + toward;
	if (!InDungeonBounds(there))
		return false;
	const int id = dMonster[there.x][there.y];
	if (id <= 0)
		return false;
	Monster &other = Monsters[id - 1];
	if (!oracool::IsMinion(other) || other.mode != MonsterMode::Stand || (other.flags & MFLAG_TARGETS_MONSTER) != 0
	    || (other.hitPoints >> 6) <= 0)
		return false;
	M_ClearSquares(mover);
	M_ClearSquares(other);
	mover.position.tile = mover.position.future = mover.position.old = there;
	other.position.tile = other.position.future = other.position.old = from;
	dMonster[there.x][there.y] = static_cast<int16_t>(mover.getId() + 1);
	dMonster[from.x][from.y] = static_cast<int16_t>(other.getId() + 1);
	mover.direction = toward;
	M_StartStand(mover, toward);
	return true;
}

bool MonsterStepAwayFrom(Monster &monster, Point from)
{
	const Direction away = Opposite(GetDirection(monster.position.tile, from));
	for (Direction dir : { away, Left(away), Right(away), Left(Left(away)), Right(Right(away)) }) {
		if (Walk(monster, dir))
			return true;
	}
	return false;
}

/** @brief One step toward @p to, straight or up to two turns aside. */
bool CompanionStepToward(Monster &companion, Point to)
{
	const Direction toward = GetDirection(companion.position.tile, to);
	for (Direction dir : { toward, Left(toward), Right(toward), Left(Left(toward)), Right(Right(toward)) }) {
		if (Walk(companion, dir))
			return true;
	}
	return MinionTradesPlaces(companion, toward);
}

/**
 * @brief A companion's own brain, in place of the Golem's (oracool/companion.h). GetCompanionOrders says where it
 * belongs and how far it may range for the stance; PickCompanionTarget says what it fights.
 *
 * Staying near comes first: past the regroup distance it reappears beside its owner, past the leash it walks back.
 * Then it fights - an ability when one is ready, else its bow or its blade - and with nothing to fight it settles back
 * into formation. Left behind mid-step, it hurries: a frame more a tick, twice the pace.
 */
void CompanionAi(Monster &companion)
{
	const oracool::CompanionOrders orders = oracool::GetCompanionOrders(companion);
	if (!orders.valid)
		return;
	const int distance = companion.position.tile.WalkingDistance(orders.owner);
	if (companion.isWalking()) {
		if (distance > orders.leash && companion.animInfo.currentFrame < companion.animInfo.numberOfFrames - 2)
			companion.animInfo.currentFrame++;
		return;
	}
	if (IsAnyOf(companion.mode, MonsterMode::Death, MonsterMode::SpecialStand, MonsterMode::MeleeAttack, MonsterMode::RangedAttack))
		return;
	// The regroup first, spin or no spin: hemmed in mid-spin, Talic returned below before ever reaching it and stayed behind
	// while the other two Ancients rejoined the hero (round 31 audit).
	if (distance > orders.regroup && PlaceCompanionNear(companion, orders.owner, 3)) {
		if (oracool::IsCompanionSpinning(companion))
			oracool::StopCompanionSpin(companion);
		oracool::OnCompanionRegrouped(companion);
		return;
	}
	// And a companion that is past the leash and no longer getting closer - a wall between it and a hero inside the regroup
	// distance - is set down beside him after a dozen tries, as a regroup (round 31 audit: it pushed against the wall
	// until the hero walked off).
	{
		std::array<uint8_t, MaxMonsters> &BestDistance = RegroupBestDistance;
		std::array<uint8_t, MaxMonsters> &TriesWithoutGain = RegroupTriesWithoutGain;
		const size_t id = companion.getId();
		// 0 is "not measuring" (the arrays start zeroed).
		if (distance <= orders.leash) {
			TriesWithoutGain[id] = 0;
			BestDistance[id] = 0;
		} else if (BestDistance[id] == 0 || distance < BestDistance[id]) {
			BestDistance[id] = static_cast<uint8_t>(std::min(distance, 254));
			TriesWithoutGain[id] = 0;
		} else if (++TriesWithoutGain[id] >= 12) {
			TriesWithoutGain[id] = 0;
			BestDistance[id] = 0;
			if (PlaceCompanionNear(companion, orders.owner, 3)) {
				if (oracool::IsCompanionSpinning(companion))
					oracool::StopCompanionSpin(companion);
				oracool::OnCompanionRegrouped(companion);
				return;
			}
		}
	}
	// Talic's spin runs itself (oracool/companion.h) until nothing is beside him - unless the stance forbids fighting or
	// his owner is past the leash, which end it at once (audit, 2026-09-29: a spin ignored both).
	if (oracool::IsCompanionSpinning(companion)) {
		if (!orders.attacks) {
			oracool::StopCompanionSpin(companion);
		} else if (distance <= orders.leash) {
			return;
		} else {
			// Past the leash: he breaks off only if he can step away (audit of the fix, 2026-09-29) - stopped and hemmed in,
			// the target picker below restarted the spin every tick and he froze, drawn spinning, never striking.
			if (CompanionStepToward(companion, orders.home))
				oracool::StopCompanionSpin(companion);
			return;
		}
	}

	if (distance > orders.regroup && PlaceCompanionNear(companion, orders.owner, 3)) {
		oracool::OnCompanionRegrouped(companion);
		return;
	}
	if (distance > orders.leash && CompanionStepToward(companion, orders.home))
		return;

	if (Monster *target = oracool::PickCompanionTarget(companion, orders); target != nullptr) {
		companion.enemy = static_cast<uint8_t>(target->getId());
		companion.flags |= MFLAG_TARGETS_MONSTER;
		companion.enemyPosition = target->position.tile;
		companion.direction = GetDirection(companion.position.tile, target->position.tile);
		oracool::AimCompanion(companion, *target);
		switch (oracool::TryCompanionAbility(companion, *target)) {
		case oracool::CompanionAct::Acted:
			return;
		case oracool::CompanionAct::Volley:
			StartRangedAttack(companion, orders.missile, 0);
			return;
		case oracool::CompanionAct::None:
			break;
		}
		// A thrower throws as a bow shoots: the swing on its own sheet, the hammer let go at the release (CompanionShot).
		if (orders.attack == oracool::CompanionAttack::Bow || orders.attack == oracool::CompanionAttack::Throw) {
			// The orders' own missile: a Skeletal Mage's element (necro_summoning's table), an arrow for everyone else.
			// This was MissileID::Arrow for all until 2026-09-26, so every mage shot the same plain arrow.
			StartRangedAttack(companion, orders.missile, 0);
			return;
		}
		if (orders.attack == oracool::CompanionAttack::Melee || orders.attack == oracool::CompanionAttack::Whirl) {
			if (companion.position.tile.WalkingDistance(target->position.tile) <= 1) {
				// A whirler spins instead of swinging (2026-09-29).
				if (orders.attack == oracool::CompanionAttack::Whirl)
					oracool::StartCompanionSpin(companion);
				else
					StartAttack(companion);
				return;
			}
			if (CompanionStepToward(companion, target->position.tile))
				return;
		}
	}

	else if (oracool::IsMinion(companion)) {
		// Nothing to fight: say so, which is what lets a busier minion trade places with this one. The enemy index
		// goes back to 0 with the flag, because without the flag ProcessMonsters reads it as a PLAYER index.
		companion.flags &= ~MFLAG_TARGETS_MONSTER;
		companion.enemy = 0;
	}

	if (distance > orders.settle)
		CompanionStepToward(companion, orders.home);
}

/**
 * @brief A minion's brain: the Companion's, fed the army's orders, on a thinking budget (oracool/minions.h).
 *
 * Unlike a companion a minion FLINCHES - it is a monster, with a monster's hit recovery - so anything but standing
 * or walking is left to finish.
 */
void MinionAi(Monster &minion, uint32_t tick)
{
	if (minion.isWalking()) {
		CompanionAi(minion);
		return;
	}
	if (minion.mode != MonsterMode::Stand || !oracool::MinionThinksThisTick(minion, tick))
		return;
	CompanionAi(minion);
}

void GolumAi(Monster &golem)
{
	if (golem.position.tile.x == 1 && golem.position.tile.y == 0) {
		return;
	}

	if (oracool::IsCompanion(golem)) {
		CompanionAi(golem);
		return;
	}

	if (IsAnyOf(golem.mode, MonsterMode::Death, MonsterMode::SpecialStand) || golem.isWalking()) {
		return;
	}

	if ((golem.flags & MFLAG_TARGETS_MONSTER) == 0)
		UpdateEnemy(golem);

	if (IsAnyOf(golem.mode, MonsterMode::MeleeAttack, MonsterMode::RangedAttack)) {
		return;
	}

	if ((golem.flags & MFLAG_NO_ENEMY) == 0) {
		auto &enemy = Monsters[golem.enemy];
		int mex = golem.position.tile.x - enemy.position.future.x;
		int mey = golem.position.tile.y - enemy.position.future.y;
		golem.direction = GetDirection(golem.position.tile, enemy.position.tile);
		if (abs(mex) < 2 && abs(mey) < 2) {
			golem.enemyPosition = enemy.position.tile;
			if (enemy.activeForTicks == 0) {
				enemy.activeForTicks = UINT8_MAX;
				enemy.position.last = golem.position.tile;
				for (int j = 0; j < 5; j++) {
					for (int k = 0; k < 5; k++) {
						int mx = golem.position.tile.x + k - 2;
						int my = golem.position.tile.y + j - 2;
						if (!InDungeonBounds({ mx, my }))
							continue;
						int enemyId = dMonster[mx][my];
						if (enemyId > 0)
							Monsters[enemyId - 1].activeForTicks = UINT8_MAX;
					}
				}
			}
			StartAttack(golem);
			return;
		}
		if (AiPlanPath(golem))
			return;
	}

	golem.pathCount++;
	if (golem.pathCount > 8)
		golem.pathCount = 5;

	if (RandomWalk(golem, Players[golem.getId()]._pdir))
		return;

	Direction md = Left(golem.direction);
	for (int j = 0; j < 8; j++) {
		md = Right(md);
		if (Walk(golem, md)) {
			break;
		}
	}
}

void DeleteMonsterList()
{
	for (int i = 0; i < MAX_PLRS; i++) {
		auto &golem = Monsters[i];
		if (!golem.isInvalid)
			continue;

		golem.position.tile = GolemHoldingCell;
		golem.position.future = { 0, 0 };
		golem.position.old = { 0, 0 };
		golem.isInvalid = false;
	}

	for (size_t i = MAX_PLRS; i < ActiveMonsterCount;) {
		if (Monsters[ActiveMonsters[i]].isInvalid) {
			if (pcursmonst == ActiveMonsters[i]) // Unselect monster if player highlighted it
				pcursmonst = -1;
			DeleteMonster(i);
		} else {
			i++;
		}
	}
}

void ProcessMonsters()
{
	static uint32_t minionTick = 0; // only ever compared modulo 3; its value across games means nothing
	minionTick++;
	DeleteMonsterList();

	assert(ActiveMonsterCount <= MaxMonsters);
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		FollowTheLeader(monster);
		if (gbIsMultiplayer) {
			SetRndSeed(monster.aiSeed);
			monster.aiSeed = AdvanceRndSeed();
		}
		// Lasting Wounds and Deep Wounds (RfA-12) close the wound to regeneration.
		if (monster.hitPoints < monster.maxHitPoints && monster.hitPoints >> 6 > 0 && !oracool::MonsterRegenBlocked(monster)) {
			if (monster.level(sgGameInitInfo.nDifficulty) > 1) {
				monster.hitPoints += monster.level(sgGameInitInfo.nDifficulty) / 2;
			} else {
				monster.hitPoints += monster.level(sgGameInitInfo.nDifficulty);
			}
			monster.hitPoints = std::min(monster.hitPoints, monster.maxHitPoints); // prevent going over max HP with part of a single regen tick
		}

		// Nocturne and Soft Tread (RfA-12): in sight is not yet noticed.
		if (IsTileVisible(monster.position.tile) && monster.activeForTicks == 0 && oracool::MonsterMayNotice(monster)) {
			// Not a rift's Butcher: his arrival already streamed the line, and this cut it off and started it again (round 32
			// audit - round 23 spared Na-Krul the same way).
			if (monster.type().type == MT_CLEAVER && !oracool::IsRiftGuardian(monster)) {
				PlaySFX(USFX_CLEAVER);
			}
			// Not a rift's Na-Krul: his floor-24 sealed-door speech cut the guardian's arrival sting in a rift (round 23).
			if (monster.type().type == MT_NAKRUL && !oracool::IsRiftGuardian(monster)) {
				if (sgGameInitInfo.bCowQuest != 0) {
					PlaySFX(USFX_NAKRUL6);
				} else {
					if (IsUberRoomOpened)
						PlaySFX(USFX_NAKRUL4);
					else
						PlaySFX(USFX_NAKRUL5);
				}
			}
			if (monster.type().type == MT_DEFILER)
				PlaySFX(USFX_DEFILER8);
			UpdateEnemy(monster);
		}

		if ((monster.flags & MFLAG_TARGETS_MONSTER) != 0) {
			assert(monster.enemy >= 0 && monster.enemy < MaxMonsters);
			// BUGFIX: enemy target may be dead at time of access, thus reading garbage data from `Monsters[monster.enemy].position.future`.
			monster.position.last = Monsters[monster.enemy].position.future;
			monster.enemyPosition = monster.position.last;
		} else {
			assert(monster.enemy >= 0 && monster.enemy < MAX_PLRS);
			Player &player = Players[monster.enemy];
			monster.enemyPosition = player.position.future;
			// Dim Vision (oracool/curses.h): a blinded monster keeps no hold on a hero it cannot see. The curse only
			// gated waking, and one cast mid-fight - already active, refreshed every tick - changed nothing (round 20).
			// Not a unique, Diablo or a Dread boss: blinded, they stood idle for the whole fight, and Diablo's death skipped its
			// sweep of the level (round 21 audit of the uncommitted change).
			// Nor one retreating or faded out (a Counselor's fade): its AI returns at activeForTicks 0 before the fade-in, and
			// it stayed invisible and unhittable for the curse's length (round 24 audit).
			const bool lostInTheDark = oracool::CursedMonsterBlinded(monster) && !oracool::FightsAsUnique(monster) && !oracool::MonsterMayNotice(monster)
			    && monster.goal != MonsterGoal::Retreat && (monster.flags & MFLAG_HIDDEN) == 0 // Retreat only (round 25): not a rallied Fallen
			    && monster.mode != MonsterMode::FadeOut && monster.mode != MonsterMode::FadeIn; // a Counselor cursed mid-fade stuck hidden (round 26)
			if (lostInTheDark) {
				monster.activeForTicks = 0;
			} else if (IsTileVisible(monster.position.tile) && (monster.activeForTicks != 0 || oracool::MonsterMayNotice(monster))) {
				monster.activeForTicks = UINT8_MAX;
				monster.position.last = player.position.future;
			} else if (monster.activeForTicks != 0 && monster.type().type != MT_DIABLO) {
				monster.activeForTicks--;
			}
		}
		// CHILL: the ice takes every other tick from a chilled monster (Oracool, Round 1). Placed
		// here, after the enemy and the target position have been updated and before the AI runs, so
		// a chilled monster still SEES - it knows where the player went, it is just slower to answer.
		// Skipping the whole iteration instead would freeze its knowledge as well as its feet, and it
		// would then lurch toward a stale position when the chill ended.
		//
		// The animation below is skipped with it, deliberately: half the AI ticks with a full-speed
		// walk cycle is a monster that moonwalks.
		//
		// Not while dying or already stone (Round 2, when freeze arrived): a frozen corpse would
		// hold its death animation forever, and stone already stops everything this would.
		if (monster.mode != MonsterMode::Death && monster.mode != MonsterMode::Petrified
		    && oracool::ChillTakesThisTick(monster))
			continue;

		while (true) {
			if (oracool::IsMinion(monster)) {
				MinionAi(monster, minionTick);
			} else if (oracool::CursedMonsterFlees(monster)) {
				// Terror (oracool/curses.h): its step away was its whole turn.
			} else if (MonsterTakesRetreatStep(monster)) {
				// Howl, Grim Ward, Sanctuary, Blinding Flash: a step away, for the AIs that never read the goal.
			} else if ((monster.flags & MFLAG_SEARCH) == 0 || !AiPlanPath(monster)) {
				AiProc[static_cast<int8_t>(monster.ai)](monster);
			}

			if (!UpdateModeStance(monster))
				break;

			GroupUnity(monster);
		}
		if (monster.mode != MonsterMode::Petrified && (monster.flags & MFLAG_ALLOW_SPECIAL) == 0) {
			monster.animInfo.processAnimation((monster.flags & MFLAG_LOCK_ANIMATION) != 0);
		}
	}

	DeleteMonsterList();
}

void FreeMonsters()
{
	oracool::ClearMonsterScaleCache();
	for (CMonster &monsterType : LevelMonsterTypes) {
		monsterType.animData = nullptr;
		for (AnimStruct &animData : monsterType.anims) {
			animData.sprites = std::nullopt;
		}

		for (auto &variants : monsterType.sounds) {
			for (auto &sound : variants) {
				sound = nullptr;
			}
		}
	}
}

bool DirOK(const Monster &monster, Direction mdir)
{
	Point position = monster.position.tile;
	Point futurePosition = position + mdir;
	if (!IsRelativeMoveOK(monster, position, mdir))
		return false;
	if (monster.leaderRelation == LeaderRelation::Leashed) {
		return futurePosition.WalkingDistance(monster.getLeader()->position.future) < 4;
	}
	if (!monster.hasLeashedMinions())
		return true;
	int mcount = 0;
	for (int x = futurePosition.x - 3; x <= futurePosition.x + 3; x++) {
		for (int y = futurePosition.y - 3; y <= futurePosition.y + 3; y++) {
			if (!InDungeonBounds({ x, y }))
				continue;
			Monster *minion = FindMonsterAtPosition({ x, y }, true);
			if (minion == nullptr)
				continue;

			if (minion->leaderRelation == LeaderRelation::Leashed && minion->getLeader() == &monster) {
				mcount++;
			}
		}
	}
	return mcount == monster.packSize;
}

bool PosOkMissile(Point position)
{
	return !TileHasAny(dPiece[position.x][position.y], TileProperties::BlockMissile);
}

bool LineClearMissile(Point startPoint, Point endPoint)
{
	return LineClear(PosOkMissile, startPoint, endPoint);
}

bool LineClear(tl::function_ref<bool(Point)> clear, Point startPoint, Point endPoint)
{
	Point position = startPoint;

	int dx = endPoint.x - position.x;
	int dy = endPoint.y - position.y;
	if (abs(dx) > abs(dy)) {
		if (dx < 0) {
			std::swap(position, endPoint);
			dx = -dx;
			dy = -dy;
		}
		int d;
		int yincD;
		int dincD;
		int dincH;
		if (dy > 0) {
			d = 2 * dy - dx;
			dincD = 2 * dy;
			dincH = 2 * (dy - dx);
			yincD = 1;
		} else {
			d = 2 * dy + dx;
			dincD = 2 * dy;
			dincH = 2 * (dx + dy);
			yincD = -1;
		}
		bool done = false;
		while (!done && position != endPoint) {
			if ((d <= 0) ^ (yincD < 0)) {
				d += dincD;
			} else {
				d += dincH;
				position.y += yincD;
			}
			position.x++;
			done = position != startPoint && !clear(position);
		}
	} else {
		if (dy < 0) {
			std::swap(position, endPoint);
			dy = -dy;
			dx = -dx;
		}
		int d;
		int xincD;
		int dincD;
		int dincH;
		if (dx > 0) {
			d = 2 * dx - dy;
			dincD = 2 * dx;
			dincH = 2 * (dx - dy);
			xincD = 1;
		} else {
			d = 2 * dx + dy;
			dincD = 2 * dx;
			dincH = 2 * (dy + dx);
			xincD = -1;
		}
		bool done = false;
		while (!done && position != endPoint) {
			if ((d <= 0) ^ (xincD < 0)) {
				d += dincD;
			} else {
				d += dincH;
				position.x += xincD;
			}
			position.y++;
			done = position != startPoint && !clear(position);
		}
	}
	return position == endPoint;
}

void SyncMonsterAnim(Monster &monster)
{
#ifdef _DEBUG
	// fix for saves with debug monsters having type originally not on the level
	CMonster &monsterType = LevelMonsterTypes[monster.levelType];
	if (monsterType.data == nullptr) {
		InitMonsterGFX(monsterType);
		monsterType.corpseId = 1;
	}
#endif
	if (monster.isUnique()) {
		InitTRNForUniqueMonster(monster);
		// Oracool: and re-apply the tint, because the line above just threw it away. The champion's
		// palette is reloaded from its .trn file here rather than saved, so a lesser unique's recolour
		// - which lives only in that in-memory buffer - did not survive a save/load or a walk back up
		// the stairs. Found while fixing the changing name; same root cause, that both were derived
		// from state nobody had checked the lifetime of.
		//
		// Safe to call every time precisely BECAUSE the reload just happened: the tint always shifts a
		// freshly loaded palette, never an already-shifted one.
		oracool::TintLesserUnique(monster);
	} else {
		oracool::RestoreVariantTint(monster);
	}
	MonsterGraphic graphic = MonsterGraphic::Stand;

	switch (monster.getVisualMonsterMode()) {
	case MonsterMode::Stand:
	case MonsterMode::Delay:
	case MonsterMode::Talk:
		break;
	case MonsterMode::MoveNorthwards:
	case MonsterMode::MoveSouthwards:
	case MonsterMode::MoveSideways:
		graphic = MonsterGraphic::Walk;
		break;
	case MonsterMode::MeleeAttack:
	case MonsterMode::RangedAttack:
		graphic = MonsterGraphic::Attack;
		break;
	case MonsterMode::HitRecovery:
		graphic = MonsterGraphic::GotHit;
		break;
	case MonsterMode::Death:
		graphic = MonsterGraphic::Death;
		break;
	case MonsterMode::SpecialMeleeAttack:
	case MonsterMode::FadeIn:
	case MonsterMode::FadeOut:
	case MonsterMode::SpecialStand:
	case MonsterMode::SpecialRangedAttack:
	case MonsterMode::Heal:
		graphic = MonsterGraphic::Special;
		break;
	case MonsterMode::Charge:
		graphic = MonsterGraphic::Attack;
		monster.animInfo.currentFrame = 0;
		break;
	default:
		monster.animInfo.currentFrame = 0;
		break;
	}

	monster.changeAnimationData(graphic);
}

void M_FallenFear(Point position)
{
	const Rectangle fearArea = Rectangle { position, 4 };
	for (const Point tile : PointsInRectangle(fearArea)) {
		if (!InDungeonBounds(tile))
			continue;
		int m = dMonster[tile.x][tile.y];
		if (m == 0)
			continue;
		Monster &monster = Monsters[abs(m) - 1];
		if (monster.ai != MonsterAIID::Fallen || monster.hitPoints >> 6 <= 0)
			continue;

		int runDistance = std::max((8 - monster.data().level), 2);
		monster.goal = MonsterGoal::Retreat;
		monster.goalVar1 = runDistance;
		monster.goalVar2 = static_cast<int>(GetDirection(position, monster.position.tile));
	}
}

void PlayEffect(Monster &monster, MonsterSound mode)
{
	if (MyPlayer->pLvlLoad != 0) {
		return;
	}

	int sndIdx = GenerateRnd(2);
	if (!gbSndInited || !gbSoundOn || gbBufferMsgs != 0) {
		return;
	}

	TSnd *snd = monster.type().sounds[static_cast<size_t>(mode)][sndIdx].get();
	if (snd == nullptr || snd->isPlaying()) {
		return;
	}

	int lVolume = 0;
	int lPan = 0;
	if (!CalculateSoundPosition(monster.position.tile, &lVolume, &lPan))
		return;

	snd_play_snd(snd, lVolume, lPan);
}

void MissToMonst(Missile &missile, Point position)
{
	int monsterId = missile._misource;

	assert(static_cast<size_t>(monsterId) < MaxMonsters);
	auto &monster = Monsters[monsterId];

	Point oldPosition = missile.position.tile;
	dMonster[position.x][position.y] = monsterId + 1;
	monster.direction = static_cast<Direction>(missile._mimfnum);
	monster.position.tile = position;
	M_StartStand(monster, monster.direction);
	M_StartHit(monster, 0);

	if (monster.type().type == MT_GLOOM)
		return;

	if ((monster.flags & MFLAG_TARGETS_MONSTER) == 0) {
		if (dPlayer[oldPosition.x][oldPosition.y] <= 0 || oracool::IsMonsterConverted(monster))
			return;

		int pnum = dPlayer[oldPosition.x][oldPosition.y] - 1;
		Player &player = Players[pnum];
		MonsterAttackPlayer(monster, player, 500,
		    oracool::PackAdjustedDamage(monster, monster.minDamageSpecial),
		    oracool::PackAdjustedDamage(monster, monster.maxDamageSpecial));

		if (IsAnyOf(monster.type().type, MT_NSNAKE, MT_RSNAKE, MT_BSNAKE, MT_GSNAKE))
			return;

		if (player._pmode != PM_GOTHIT && player._pmode != PM_DEATH)
			StartPlrHit(player, 0, true);
		// Immovable and Heavy Foot hold against a charge too, as against a knockback blow (round 10 audit). And a hero the
		// charge killed stays where he fell: the corpse was shoved a tile, off its DeadPlayer mark (round 28 audit).
		if (player._pmode == PM_DEATH || oracool::PlayerIgnoresKnockback(player))
			return;
		Point newPosition = oldPosition + monster.direction;
		oracool::CompanionsMakeWay(player, newPosition);
		if (PosOkPlayer(player, newPosition)) {
			player.position.tile = newPosition;
			FixPlayerLocation(player, player._pdir);
			FixPlrWalkTags(player);
			dPlayer[newPosition.x][newPosition.y] = pnum + 1;
			SetPlayerOld(player);
		}
		return;
	}

	Monster *target = FindMonsterAtPosition(oldPosition, true);

	if (target == nullptr)
		return;

	MonsterAttackMonster(monster, *target, 500, monster.minDamageSpecial, monster.maxDamageSpecial);

	if (IsAnyOf(monster.type().type, MT_NSNAKE, MT_RSNAKE, MT_BSNAKE, MT_GSNAKE))
		return;

	// The TARGET is thrown back, not the charger (a vanilla slip the fork now reaches: a Decoy or a taunting guard draws
	// charges from 5+ tiles). It moved the target's grid mark and put the charger on it, leaving the target unclickable
	// and a phantom blocking tile behind (round 10 audit, v1.12.235).
	Point newPosition = oldPosition + monster.direction;
	// Not a walker (round 32 audit): its walk would end relative to the pushed tile, into a wall, leaving the old
	// destination's marker as an invisible block.
	// Nor an Unyielding, Relentless or Implacable one, which no knockback moves; and its light goes with it (round 62 audit).
	if (target->mode != MonsterMode::Death && !target->isWalking() && !oracool::IsKnockbackImmune(*target) && IsTileAvailable(*target, newPosition)) {
		dMonster[oldPosition.x][oldPosition.y] = 0;
		dMonster[newPosition.x][newPosition.y] = static_cast<int16_t>(target->getId() + 1);
		target->position.tile = newPosition;
		target->position.future = newPosition;
		target->position.old = newPosition;
		if (target->lightId != NO_LIGHT)
			ChangeLightXY(target->lightId, newPosition);
	}
}

Monster *FindMonsterAtPosition(Point position, bool ignoreMovingMonsters)
{
	if (!InDungeonBounds(position)) {
		return nullptr;
	}

	auto monsterId = dMonster[position.x][position.y];

	if (monsterId == 0 || (ignoreMovingMonsters && monsterId < 0)) {
		// nothing at this position, return a nullptr
		return nullptr;
	}

	return &Monsters[abs(monsterId) - 1];
}

Monster *FindUniqueMonster(UniqueMonsterType monsterType)
{
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		int monsterId = ActiveMonsters[i];
		auto &monster = Monsters[monsterId];
		if (monster.uniqueType == monsterType)
			return &monster;
	}
	return nullptr;
}

bool IsTileAvailable(const Monster &monster, Point position)
{
	if (!IsTileAvailable(position))
		return false;

	return IsTileSafe(monster, position);
}

bool IsSkel(_monster_id mt)
{
	return std::find(std::begin(SkeletonTypes), std::end(SkeletonTypes), mt) != std::end(SkeletonTypes);
}

bool IsGoat(_monster_id mt)
{
	return IsAnyOf(mt,
	    MT_NGOATMC, MT_BGOATMC, MT_RGOATMC, MT_GGOATMC,
	    MT_NGOATBW, MT_BGOATBW, MT_RGOATBW, MT_GGOATBW);
}

void ActivateSkeleton(Monster &monster, Point position)
{
	if (IsTileAvailable(position)) {
		ActivateSpawn(monster, position, Direction::SouthWest);
		return;
	}

	constexpr std::array<Direction, 8> spawnDirections {
		Direction::North, Direction::NorthEast, Direction::East, Direction::NorthWest, Direction::SouthEast, Direction::West, Direction::SouthWest, Direction::South
	};
	std::bitset<8> spawnOk;

	for (size_t i = 0; i < spawnDirections.size(); i++) {
		if (IsTileAvailable(position + spawnDirections[i]))
			spawnOk.set(i);
	}
	if (spawnOk.none())
		return;

	// this is used in the following loop to find the nth set bit.
	int spawnChoice = GenerateRnd(15) % spawnOk.count();

	for (size_t i = 0; i < spawnOk.size(); i++) {
		if (!spawnOk.test(i))
			continue;

		if (spawnChoice > 0) {
			spawnChoice--;
			continue;
		}

		ActivateSpawn(monster, position + spawnDirections[i], Opposite(spawnDirections[i]));
		return;
	}
}

Monster *PreSpawnSkeleton()
{
	Monster *skeleton = AddSkeleton({ 0, 0 }, Direction::South, false);
	if (skeleton != nullptr)
		M_StartStand(*skeleton, Direction::South);

	return skeleton;
}

void TalktoMonster(Player &player, Monster &monster)
{
	if (&player == MyPlayer)
		monster.mode = MonsterMode::Talk;

	if (monster.uniqueType == UniqueMonsterType::SnotSpill
	    && Quests[Q_LTBANNER].IsAvailable() && Quests[Q_LTBANNER]._qvar1 == 2) {
		if (RemoveInventoryItemById(player, IDI_BANNER)) {
			Quests[Q_LTBANNER]._qactive = QUEST_DONE;
			monster.talkMsg = TEXT_BANNER12;
			monster.goal = MonsterGoal::Inquiring;
			NetSendCmdQuest(true, Quests[Q_LTBANNER]);
		}
	}
	if (monster.uniqueType == UniqueMonsterType::Lachdan
	    && Quests[Q_VEIL].IsAvailable() && monster.talkMsg >= TEXT_VEIL9) {
		if (RemoveInventoryItemById(player, IDI_GLDNELIX) && (monster.flags & MFLAG_QUEST_COMPLETE) == 0) {
			monster.talkMsg = TEXT_VEIL11;
			monster.goal = MonsterGoal::Inquiring;
			monster.flags |= MFLAG_QUEST_COMPLETE;
			if (MyPlayer == &player) {
				SpawnUnique(UITEM_STEELVEIL, monster.position.tile + Direction::South);
				Quests[Q_VEIL]._qvar2 = QS_VEIL_ITEM_SPAWNED;
				NetSendCmdQuest(true, Quests[Q_VEIL]);
			}
		}
	}
	if (monster.uniqueType == UniqueMonsterType::Zhar
	    && monster.talkMsg == TEXT_ZHAR1
	    && (monster.flags & MFLAG_QUEST_COMPLETE) == 0) {
		if (MyPlayer == &player) {
			Quests[Q_ZHAR]._qactive = QUEST_ACTIVE;
			Quests[Q_ZHAR]._qlog = true;
			Quests[Q_ZHAR]._qvar1 = QS_ZHAR_ITEM_SPAWNED;
			MakeRoomForGuaranteedReward(); // guaranteed drops land on a full floor (round 30 audit)
			CreateTypeItem(monster.position.tile + Displacement { 1, 1 }, false, ItemType::Misc, IMISC_BOOK, false, false, true);
			monster.flags |= MFLAG_QUEST_COMPLETE;
			NetSendCmdQuest(true, Quests[Q_ZHAR]);
		}
	}

	if (monster.uniqueType == UniqueMonsterType::Garbud && MyPlayer == &player) {
		if (monster.talkMsg == TEXT_GARBUD1) {
			Quests[Q_GARBUD]._qactive = QUEST_ACTIVE;
			Quests[Q_GARBUD]._qlog = true;
			NetSendCmdQuest(true, Quests[Q_GARBUD]);
		}
		if (monster.talkMsg == TEXT_GARBUD2 && (monster.flags & MFLAG_QUEST_COMPLETE) == 0) {
			SpawnItem(monster, monster.position.tile + Displacement { 1, 1 }, false, true);
			monster.flags |= MFLAG_QUEST_COMPLETE;
			Quests[Q_GARBUD]._qvar1 = QS_GHARBAD_FIRST_ITEM_SPAWNED;
			NetSendCmdQuest(true, Quests[Q_GARBUD]);
		}
	}
}

void SpawnCompanionBody(Monster &slot, Point position, Direction facing)
{
	dMonster[position.x][position.y] = slot.getId() + 1;
	slot.position.tile = position;
	slot.position.future = position;
	slot.position.old = position;
	slot.pathCount = 0;
	slot.isInvalid = false;
	slot.flags |= MFLAG_GOLEM;
	StartSpecialStand(slot, facing);
	UpdateEnemy(slot);
}

void ReleaseCompanionBody(Monster &slot)
{
	if (slot.position.tile != GolemHoldingCell && InDungeonBounds(slot.position.tile)) {
		M_ClearSquares(slot);
		dMonster[slot.position.tile.x][slot.position.tile.y] = 0;
	}
	slot.position.tile = GolemHoldingCell;
	slot.position.future = { 0, 0 };
	slot.position.old = { 0, 0 };
	slot.isInvalid = false;
	// Not targeting a monster means enemy is a PLAYER index, and ProcessMonsters asserts exactly that - clearing the flag
	// with a monster id left in enemy stopped the game the moment a Valkyrie's time ran out (user, 2026-09-14).
	slot.flags &= ~MFLAG_TARGETS_MONSTER;
	slot.flags |= MFLAG_NO_ENEMY;
	slot.enemy = 0;
	slot.enemyPosition = {};
	slot.mode = MonsterMode::Stand;
	// Anything that was hunting this body would take one step toward the holding cell before its next
	// stand re-aimed it (audit, 2026-09-19): re-aim it now.
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &other = Monsters[ActiveMonsters[i]];
		if (&other != &slot && (other.flags & MFLAG_TARGETS_MONSTER) != 0 && static_cast<size_t>(other.enemy) == slot.getId())
			UpdateEnemy(other);
	}
}

void MoveCompanionTo(Monster &companion, Point tile)
{
	M_ClearSquares(companion);
	dMonster[companion.position.tile.x][companion.position.tile.y] = 0;
	companion.position.tile = tile;
	companion.position.future = tile;
	companion.position.old = tile;
	dMonster[tile.x][tile.y] = companion.getId() + 1;
	M_StartStand(companion, companion.direction);
}

bool PlaceCompanionNear(Monster &companion, Point centre, int maxRadius)
{
	for (int radius = 1; radius <= maxRadius; radius++) {
		for (int dy = -radius; dy <= radius; dy++) {
			for (int dx = -radius; dx <= radius; dx++) {
				if (std::max(std::abs(dx), std::abs(dy)) != radius)
					continue;
				const Point tile = centre + Displacement { dx, dy };
				if (!InDungeonBounds(tile) || !IsTileAvailable(companion, tile))
					continue;
				MoveCompanionTo(companion, tile);
				companion.direction = GetDirection(tile, centre);
				return true;
			}
		}
	}
	return false;
}

GolemStats GolemStatsAt(const Player &player, int spellLevel)
{
	GolemStats stats;
	stats.maxHitPoints = 2 * (320 * spellLevel + player._pMaxMana / 3);
	stats.toHit = 5 * (spellLevel + 8) + 2 * player._pLevel;
	// Held at the byte the monster keeps them in (round 44 audit: past spell level 119 the maximum wrapped to 0 under a
	// minimum of 248), and the tooltip reads these same numbers.
	stats.minDamage = std::min(2 * (spellLevel + 4), 255);
	stats.maxDamage = std::min(2 * (spellLevel + 8), 255);
	return stats;
}

void SpawnGolem(Player &player, Monster &golem, Point position, Missile &missile)
{
	dMonster[position.x][position.y] = golem.getId() + 1;
	golem.position.tile = position;
	golem.position.future = position;
	golem.position.old = position;
	golem.pathCount = 0;
	// Oracool (2026-09-26): the numbers come from GolemStatsAt, which the Golem tooltip reads too.
	const GolemStats stats = GolemStatsAt(player, missile._mispllvl);
	golem.maxHitPoints = stats.maxHitPoints;
	golem.hitPoints = golem.maxHitPoints;
	golem.armorClass = 25;
	golem.golemToHit = stats.toHit;
	golem.minDamage = stats.minDamage;
	golem.maxDamage = stats.maxDamage;
	golem.flags |= MFLAG_GOLEM;
	StartSpecialStand(golem, Direction::South);
	UpdateEnemy(golem);
	if (&player == MyPlayer) {
		NetSendCmdGolem(
		    golem.position.tile.x,
		    golem.position.tile.y,
		    golem.direction,
		    golem.enemy,
		    golem.hitPoints,
		    GetLevelForMultiplayer(player));
	}
}

bool CanTalkToMonst(const Monster &monster)
{
	return IsAnyOf(monster.goal, MonsterGoal::Inquiring, MonsterGoal::Talking);
}

int encode_enemy(Monster &monster)
{
	if ((monster.flags & MFLAG_TARGETS_MONSTER) != 0)
		return monster.enemy + MAX_PLRS;

	return monster.enemy;
}

void decode_enemy(Monster &monster, int enemyId)
{
	if (enemyId < MAX_PLRS) {
		monster.flags &= ~MFLAG_TARGETS_MONSTER;
		monster.enemy = enemyId;
		monster.enemyPosition = Players[enemyId].position.future;
	} else {
		monster.flags |= MFLAG_TARGETS_MONSTER;
		enemyId -= MAX_PLRS;
		monster.enemy = enemyId;
		monster.enemyPosition = Monsters[enemyId].position.future;
	}
}

[[nodiscard]] size_t Monster::getId() const
{
	return std::distance<const Monster *>(&Monsters[0], this);
}

Monster *Monster::getLeader() const
{
	if (leader == Monster::NoLeader)
		return nullptr;

	return &Monsters[leader];
}

void Monster::setLeader(const Monster *leader)
{
	if (leader == nullptr) {
		// really we should update this->leader to NoLeader to avoid leaving a dangling reference to a dead monster
		// when passed nullptr. So that buffed minions are drawn with a distinct colour in monhealthbar we leave the
		// reference and hope that no code tries to modify the leader through this instance later.
		leaderRelation = LeaderRelation::None;
		return;
	}

	this->leader = leader->getId();
	leaderRelation = LeaderRelation::Leashed;
	ai = leader->ai;
}

[[nodiscard]] unsigned Monster::distanceToEnemy() const
{
	int mx = position.tile.x - enemyPosition.x;
	int my = position.tile.y - enemyPosition.y;
	return std::max(std::abs(mx), std::abs(my));
}

void Monster::checkStandAnimationIsLoaded(Direction mdir)
{
	if (IsAnyOf(mode, MonsterMode::Stand, MonsterMode::Talk)) {
		direction = mdir;
		changeAnimationData(MonsterGraphic::Stand);
	}
}

void Monster::petrify()
{
	mode = MonsterMode::Petrified;
	animInfo.isPetrified = true;
}

bool Monster::isWalking() const
{
	switch (getVisualMonsterMode()) {
	case MonsterMode::MoveNorthwards:
	case MonsterMode::MoveSouthwards:
	case MonsterMode::MoveSideways:
		return true;
	default:
		return false;
	}
}

bool Monster::isImmune(MissileID missileType, DamageType missileElement) const
{
	// Oracool Phase 3.4: Conviction. Computed from the player's live state rather than written into
	// this monster, so walking out of the field ends it with nothing to undo - see
	// oracool/aura_field.h. Deep Conviction breaks an immunity down to a mere resistance; the
	// resistance itself is stripped in isResistant below.
	const uint16_t effective = oracool::EffectiveResistances(*this);
	if (((effective & IMMUNE_MAGIC) != 0 && missileElement == DamageType::Magic)
	    || ((effective & IMMUNE_FIRE) != 0 && missileElement == DamageType::Fire)
	    || ((effective & IMMUNE_LIGHTNING) != 0 && missileElement == DamageType::Lightning)
	    || ((effective & IMMUNE_ACID) != 0 && missileElement == DamageType::Acid))
		return true;
	if (missileType == MissileID::HolyBolt && type().type != MT_DIABLO && data().monsterClass != MonsterClass::Undead)
		return true;
	return false;
}

bool Monster::isResistant(MissileID missileType, DamageType missileElement) const
{
	const uint16_t effective = oracool::EffectiveResistances(*this);
	if (((effective & RESIST_MAGIC) != 0 && missileElement == DamageType::Magic)
	    || ((effective & RESIST_FIRE) != 0 && missileElement == DamageType::Fire)
	    || ((effective & RESIST_LIGHTNING) != 0 && missileElement == DamageType::Lightning)
	    || ((effective & RESIST_COLD) != 0 && missileElement == DamageType::Cold))
		return true;
	if (gbIsHellfire && missileType == MissileID::HolyBolt && IsAnyOf(type().type, MT_DIABLO, MT_BONEDEMN))
		return true;
	return false;
}

bool Monster::isPlayerMinion() const
{
	return (flags & MFLAG_GOLEM) != 0 && (flags & MFLAG_BERSERK) == 0;
}

bool Monster::isPossibleToHit() const
{
	return !(hitPoints >> 6 <= 0
	    || talkMsg != TEXT_NONE
	    // Its own Sneak retreat only, not a repel's walk back (round 67 audit: Howl, Grim Ward, Daze and Blinding Flash made a
	    // visible weaver unhittable for the whole repel - the Counselors' round-40 case).
	    || (type().type == MT_ILLWEAV && goal == MonsterGoal::Retreat && goalVar3 != RepelRetreatMark)
	    || (IsAnyOf(mode, MonsterMode::Charge, MonsterMode::Death))
	    || (IsAnyOf(type().type, MT_COUNSLR, MT_MAGISTR, MT_CABALIST, MT_ADVOCATE) && goal != MonsterGoal::Normal
	        // A visible Counselor walked back by a repel (Howl, Grim Ward, Daze, Blinding Flash) can be hit: vanilla's own
	        // retreat is always hidden (round 40 audit - every blow passed through it for the repel's steps).
	        && !(goal == MonsterGoal::Retreat && (flags & MFLAG_HIDDEN) == 0 && !IsAnyOf(mode, MonsterMode::FadeIn, MonsterMode::FadeOut))));
}

void Monster::tag(const Player &tagger)
{
	whoHit |= 1 << tagger.getId();
}

bool Monster::tryLiftGargoyle()
{
	if (ai == MonsterAIID::Gargoyle && (flags & MFLAG_ALLOW_SPECIAL) != 0) {
		flags &= ~MFLAG_ALLOW_SPECIAL;
		mode = MonsterMode::SpecialMeleeAttack;
		return true;
	}
	return false;
}

MonsterMode Monster::getVisualMonsterMode() const
{
	if (mode != MonsterMode::Petrified)
		return mode;
	size_t monsterId = this->getId();
	for (auto &missile : Missiles) {
		// Search the missile that will restore the original monster mode and use the saved/original monster mode from it
		if (missile._mitype == MissileID::StoneCurse && static_cast<size_t>(missile.var2) == monsterId) {
			return static_cast<MonsterMode>(missile.var1);
		}
	}
	return MonsterMode::Petrified;
}

unsigned int Monster::exp(_difficulty difficulty) const
{
	unsigned int monsterExp = data().exp;

	if (difficulty == DIFF_NIGHTMARE) {
		monsterExp = 2 * (monsterExp + 1000);
	} else if (difficulty == DIFF_HELL) {
		monsterExp = 4 * (monsterExp + 1000);
	} else if (difficulty == DIFF_TORMENT) {
		// Oracool: Hell's own formula, scaled further by the adjustable Torment multiplier.
		monsterExp = static_cast<unsigned int>(4 * (monsterExp + 1000) * GetTormentDifficultyMultiplier());
	}

	if (isUnique()) {
		monsterExp *= 2;
	}

	return monsterExp;
}

unsigned int Monster::level(_difficulty difficulty) const
{
	unsigned int baseLevel = data().level;
	if (isUnique()) {
		baseLevel = UniqueMonstersData[static_cast<int8_t>(uniqueType)].mlevel;
		if (baseLevel != 0) {
			baseLevel *= 2;
		} else {
			baseLevel = data().level + 5;
		}
	}

	if (type().type == MT_DIABLO && !gbIsHellfire) {
		baseLevel -= 15;
	}

	if (difficulty == DIFF_NIGHTMARE) {
		baseLevel += 15;
	} else if (difficulty == DIFF_HELL) {
		baseLevel += 30;
	} else if (difficulty == DIFF_TORMENT) {
		// Oracool: Hell's own +30 level offset, scaled further by the Torment multiplier - this single
		// offset drives combat to-hit math (missiles.cpp), which consumes Monster::level(). It does NOT
		// drive loot: an item's level is ItemLevelOfMonster, the area ladder (see SpawnItem). This comment
		// claimed otherwise until 2026-09-13, while the drop code read monster.data().level instead.
		baseLevel += static_cast<unsigned int>(30 * GetTormentDifficultyMultiplier());
	}

	return baseLevel;
}

unsigned int Monster::toHit(_difficulty difficulty) const
{
	if (isPlayerMinion())
		return golemToHit;

	unsigned int baseToHit = data().toHit;
	if (isUnique() && UniqueMonstersData[static_cast<size_t>(uniqueType)].customToHit != 0) {
		baseToHit = UniqueMonstersData[static_cast<size_t>(uniqueType)].customToHit;
	}

	if (difficulty == DIFF_NIGHTMARE) {
		baseToHit += NightmareToHitBonus;
	} else if (difficulty == DIFF_HELL) {
		baseToHit += HellToHitBonus;
	} else if (difficulty == DIFF_TORMENT) {
		baseToHit += static_cast<unsigned int>(HellToHitBonus * GetTormentDifficultyMultiplier());
	}

	return baseToHit;
}

unsigned int Monster::toHitSpecial(_difficulty difficulty) const
{
	unsigned int baseToHitSpecial = data().toHitSpecial;
	if (isUnique() && UniqueMonstersData[static_cast<size_t>(uniqueType)].customToHit != 0) {
		baseToHitSpecial = UniqueMonstersData[static_cast<size_t>(uniqueType)].customToHit;
	}

	if (difficulty == DIFF_NIGHTMARE) {
		baseToHitSpecial += NightmareToHitBonus;
	} else if (difficulty == DIFF_HELL) {
		baseToHitSpecial += HellToHitBonus;
	} else if (difficulty == DIFF_TORMENT) {
		baseToHitSpecial += static_cast<unsigned int>(HellToHitBonus * GetTormentDifficultyMultiplier());
	}

	return baseToHitSpecial;
}

} // namespace devilution
