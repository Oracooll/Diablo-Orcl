#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>

#include "diablo.h"
#include "engine/demomode.h"
#include "options.h"
#include "pfile.h"
#include "utils/display.h"
#include "utils/paths.h"

using namespace devilution;

namespace {

bool Dummy_GetHeroInfo(_uiheroinfo *pInfo)
{
	return true;
}

void RunTimedemo(std::string timedemoFolderName)
{
	LoadCoreArchives();
	LoadGameArchives();

	// The tests need spawn.mpq or diabdat.mpq
	// Please provide them so that the tests can run successfully
	ASSERT_TRUE(HaveSpawn() || HaveDiabdat());

	std::string unitTestFolderCompletePath = paths::BasePath() + "test/fixtures/timedemo/" + timedemoFolderName;
	paths::SetPrefPath(unitTestFolderCompletePath);
	paths::SetConfigPath(unitTestFolderCompletePath);

	InitKeymapActions();
	LoadOptions();

	const int demoNumber = 0;

	Players.resize(1);
	MyPlayerId = demoNumber;
	MyPlayer = &Players[MyPlayerId];
	*MyPlayer = {};

	// Currently only spawn.mpq is present when building on github actions
	gbIsSpawn = true;
	gbIsHellfire = false;
	gbMusicOn = false;
	gbSoundOn = false;
	HeadlessMode = true;
	demo::InitPlayBack(demoNumber, true);

	pfile_ui_set_hero_infos(Dummy_GetHeroInfo);
	gbLoadGame = true;

	demo::OverrideOptions();

	AdjustToScreenGeometry(forceResolution);

	StartGame(false, true);

	HeroCompareResult result = pfile_compare_hero_demo(demoNumber, true);
	ASSERT_EQ(result.status, HeroCompareResult::Same) << result.message;
	ASSERT_FALSE(gbRunGame);
	gbRunGame = false;
	init_cleanup();
}

} // namespace

/**
 * @brief Vanilla's end-to-end replay golden. Skipped in this fork, with the reason stated.
 *
 * It had been red since the fork began and was carried as one of two "standing baseline failures"
 * for a week without anyone establishing why. External audit QA-01 (2026-08-30) called for a root
 * cause rather than another waiver, and here it is:
 *
 *   test/fixtures/timedemo/WarriorLevel1to2/spawn_0.sv is byte-identical to the DevilutionX 1.5.5
 *   baseline. pfile.cpp's ReadHero accepts a hero blob only when it is at least
 *   sizeof(PlayerPack), and this fork has GROWN that struct: pExperience widened from uint32_t to
 *   uint64_t for the level-99 curve, the four waypoint masks and the four spent-stat-point counters
 *   took over the reserved bytes and then some. A 1.5.5 hero is therefore smaller than the struct
 *   that must be filled from it, ReadHero returns false, and pfile_read_player_from_save calls
 *   app_fatal("Unable to load character") - which is exactly the failure seen.
 *
 * That is not a defect. It is the accepted consequence of a decision already taken, in the user's
 * own words: "i dont care about preserving sdave. i care about robust coding."
 *
 * SKIPPED rather than deleted, and rather than left red. Deleted, the fork would silently lose the
 * only end-to-end replay it has. Left red, a permanently failing suite teaches everyone to ignore a
 * red run, which is how the two real regressions found on 2026-08-30 came to ship. A skip states the
 * reason on every run.
 *
 * Re-enabling is NOT just regenerating the hero. The .dmo replays recorded input against gameplay
 * this fork has changed deliberately and repeatedly - Zeal's ladder, drop rates, monster scaling,
 * the tick rate - so a loadable hero would only move the failure from "cannot load" to "final state
 * differs". Making it green means re-recording the whole demo against current gameplay, and
 * re-recording it again after each balance change. Regenerating the reference state to match
 * whatever the code now does would produce a test that can never fail, which is worse than no test.
 * Whether that is worth it is a call for the user, not for this file.
 */
TEST(Timedemo, WarriorLevel1to2)
{
	GTEST_SKIP() << "the recorded hero predates this fork's PlayerPack growth (pExperience is "
	                "64-bit now) so ReadHero cannot load it, and the recorded input predates the "
	                "gameplay changes it replays - see this test's comment for what re-enabling "
	                "would take";
	RunTimedemo("WarriorLevel1to2");
}
