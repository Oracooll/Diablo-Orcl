#include "oracool/lesser_uniques.h"

#include <algorithm>
#include <vector>

#include "engine/random.hpp"
#include "levels/gendung.h"
#include "missiles.h"
#include "monster.h"
#include "options.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

/** @brief Armour a Fortified champion adds. Enough to notice, not enough to make it unhittable. */
constexpr int FortifiedArmorBonus = 20;

/** @brief Whether this level already loaded the sprite @p type walks around in. */
bool LevelHasMonsterType(_monster_id type)
{
	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (LevelMonsterTypes[i].type == type)
			return true;
	}
	return false;
}

/**
 * @brief Whether @p data is quest content rather than a champion we may borrow.
 *
 * mtalkmsg is the game's own marker: the uniques that speak are the ones a quest is about - Garbud
 * greeting you, Zhar objecting to being disturbed, Lazarus taunting. Reading the data rather than
 * listing names by hand means a unique added later is classified correctly without anyone
 * remembering to update a list here.
 */
bool IsQuestUnique(const UniqueMonsterData &data)
{
	return data.mtalkmsg != TEXT_NONE;
}

/**
 * @brief Whether a champion of this identity is already standing on the level.
 *
 * Needed once a floor hosts several packs (2-6 by difficulty as of 1.6.1): without it a level could
 * field three of the same named champion, which reads as a bug rather than as variety. Scanning the
 * live monsters rather than tracking picks in a list also catches the floor's SCRIPTED unique for
 * free, since PlaceUniqueMonsters runs before this does.
 */
bool IsUniqueAlreadyPlaced(UniqueMonsterType type)
{
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		if (Monsters[ActiveMonsters[i]].uniqueType == type)
			return true;
	}
	return false;
}

} // namespace

std::optional<UniqueMonsterType> ChooseLesserUnique(bool excludeLevelOwned)
{
	// Collected rather than sampled-until-hit: the candidate set is usually small (a level loads a
	// handful of monster types, and only some have champions written for them), so rejection sampling
	// could spin for a long time or miss a level's only candidate entirely.
	std::vector<UniqueMonsterType> candidates;
	for (size_t i = 0; UniqueMonstersData[i].mtype != -1; i++) {
		const UniqueMonsterData &data = UniqueMonstersData[i];
		if (IsQuestUnique(data))
			continue;
		if (excludeLevelOwned && data.mlevel == currlevel)
			continue;
		if (!LevelHasMonsterType(data.mtype))
			continue;
		if (IsUniqueAlreadyPlaced(static_cast<UniqueMonsterType>(i)))
			continue;
		candidates.push_back(static_cast<UniqueMonsterType>(i));
	}

	if (candidates.empty())
		return std::nullopt;
	return candidates[GenerateRnd(static_cast<int32_t>(candidates.size()))];
}

int LesserUniqueCountForLevel()
{
	// Town has no champions, and neither do the set levels, whose contents are authored rather than
	// generated - dropping a random pack into Lachdanan's tomb would be vandalism, not variety.
	if (currlevel == 0 || setlevel)
		return 0;
	// Oracool: user call after playing it (2026-08-15) - "one pack per level is not ok. increase
	// packs 2-3 per normal. 3-4 nightmare. 4-5 hell. 5-6 torment."
	//
	// A RANGE rather than a fixed count, so two floors at the same difficulty are not the same
	// arithmetic twice - which is what "diversification and freshness" asked for in the first place.
	// The ladder also does something the difficulty ladder alone does not: Hell is not merely the
	// same dungeon with tougher monsters, it is a dungeon with more champions IN it.
	int fewest = 2;
	int most = 3;
	switch (sgGameInitInfo.nDifficulty) {
	case DIFF_NIGHTMARE:
		fewest = 3;
		most = 4;
		break;
	case DIFF_HELL:
		fewest = 4;
		most = 5;
		break;
	case DIFF_TORMENT:
		fewest = 5;
		most = 6;
		break;
	case DIFF_NORMAL:
		break;
	}

	const int packs = fewest + GenerateRnd(most - fewest + 1);
	// The dial multiplies the roll rather than replacing it, so 300% on Torment is genuinely
	// crowded. PlaceLesserUniques stops early when MaxMonsters runs out, which is the real ceiling
	// and is checked per pack rather than trusted to a number here.
	return std::max(packs * *sgOptions.Oracool.lesserUniqueDensityPercent / 100, 1);
}

LesserUniqueAffix RollLesserUniqueAffix()
{
	// From 1, not 0: a lesser unique always carries something. A champion with no modifier is just a
	// monster with more health, and the point of the system is that each one is a different fight.
	const int count = static_cast<int>(LesserUniqueAffix::LAST);
	return static_cast<LesserUniqueAffix>(1 + GenerateRnd(count));
}

const char *GetLesserUniqueAffixName(LesserUniqueAffix affix)
{
	switch (affix) {
	case LesserUniqueAffix::Warded:
		return N_("Warded");
	case LesserUniqueAffix::Relentless:
		return N_("Relentless");
	case LesserUniqueAffix::Fortified:
		return N_("Fortified");
	case LesserUniqueAffix::Vampiric:
		return N_("Vampiric");
	case LesserUniqueAffix::Thunderous:
		return N_("Thunderous");
	case LesserUniqueAffix::None:
		break;
	}
	return "";
}

void ApplyLesserUniqueAffix(Monster &monster)
{
	switch (monster.lesserAffix) {
	case LesserUniqueAffix::Warded:
		// Resistant, never immune. An immune champion on a floor where the player has one damage type
		// is not a harder fight, it is an unwinnable one they have to walk away from.
		monster.resistance |= RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING;
		break;
	case LesserUniqueAffix::Relentless:
		// Knockback is how a player buys space. Taking it away is what makes this one frightening,
		// and it costs nothing to express - the flag already exists and the combat code already
		// honours it.
		monster.flags |= MFLAG_KNOCKBACK;
		break;
	case LesserUniqueAffix::Fortified:
		// Armour, not speed. The first draft of this list had "Fleet", and it had to go: monster
		// movement is paced by the ANIMATION, and animation timing lives on the shared CMonster rather
		// than on the individual - so making one champion fast would make every monster of its type
		// fast with it. Armour is per-monster, immediate, and reads just as clearly in a fight.
		monster.armorClass = static_cast<uint8_t>(std::min(monster.armorClass + FortifiedArmorBonus, 255));
		break;
	case LesserUniqueAffix::Vampiric:
	case LesserUniqueAffix::Thunderous:
	case LesserUniqueAffix::None:
		// Events rather than fields: they fire when the champion hits or dies.
		break;
	}
}

void OnLesserUniqueDealtDamage(Monster &monster, int damage)
{
	if (monster.lesserAffix != LesserUniqueAffix::Vampiric || damage <= 0)
		return;
	// A third of what it dealt, and never past full - the champion claws back ground during a long
	// fight without being able to out-heal a player who is winning.
	constexpr int VampiricPercent = 33;
	monster.hitPoints = std::min(monster.hitPoints + damage * VampiricPercent / 100, monster.maxHitPoints);
}

void OnLesserUniqueKilled(Monster &monster)
{
	if (monster.lesserAffix != LesserUniqueAffix::Thunderous)
		return;
	// The discharge is the reward for killing it AND the sting for standing next to it - the corpse
	// is not a safe place to be. Reuses the mini-Nova ring built for Fist of the Heavens at 1.5.78.
	AddMissile(monster.position.tile, monster.position.tile, Direction::South,
	    MissileID::MiniNovaBall, TARGET_PLAYERS, -1, monster.maxDamage, 0);
}

} // namespace devilution::oracool
