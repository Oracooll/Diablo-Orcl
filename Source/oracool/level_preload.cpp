#include "oracool/level_preload.h"

#include <algorithm>
#include <vector>

#include "diablo.h"
#include "levels/gendung.h"
#include "monster.h"
#include "oracool/companion.h"
#include "oracool/minions.h"
#include "oracool/monster_scale.h"
#include "oracool/necro_summoning.h"
#include "oracool/skill_sounds.h"
#include "player.h"

namespace devilution::oracool {

void PreloadForLevel()
{
	if (HeadlessMode)
		return;
	if (MyPlayer != nullptr) {
		PreloadSkillSounds(*MyPlayer);
		// The floor is populated by now, so these take only slots its own monsters left free (PreloadMinionType), and
		// the floor's rolls are what they would have been without an army (user, 2026-10-10).
		if (leveltype != DTYPE_TOWN) {
			std::vector<_monster_id> types;
			AppendWaitingMinionTypes(*MyPlayer, types);
			AppendSummonBodyTypes(*MyPlayer, types);
			std::sort(types.begin(), types.end());
			types.erase(std::unique(types.begin(), types.end()), types.end());
			for (const _monster_id type : types)
				PreloadMinionType(type, MinionPreloadKeepFree);
		}
	}
	PrepareCompanionDress();
	PrebuildMonsterSizes();
}

void PumpLevelPreload()
{
	if (HeadlessMode)
		return;
	PumpMonsterScale();
}

void ForgetLevelPreloads()
{
	ForgetPreparedCompanionDress();
}

} // namespace devilution::oracool
