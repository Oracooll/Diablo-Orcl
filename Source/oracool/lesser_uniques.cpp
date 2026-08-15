#include "oracool/lesser_uniques.h"

#include <vector>

#include "engine/random.hpp"
#include "levels/gendung.h"
#include "monster.h"

namespace devilution::oracool {

namespace {

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
		candidates.push_back(static_cast<UniqueMonsterType>(i));
	}

	if (candidates.empty())
		return std::nullopt;
	return candidates[GenerateRnd(static_cast<int32_t>(candidates.size()))];
}

int LesserUniqueCountForLevel()
{
	// One, until the rate option lands in step 3. Town has no champions and neither do the set
	// levels, whose contents are authored rather than generated.
	if (currlevel == 0 || setlevel)
		return 0;
	return 1;
}

} // namespace devilution::oracool
