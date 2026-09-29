#include <filesystem>
#include <random>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "diablo.h"
#include "options.h"
#include "utils/paths.h"

int main(int argc, char **argv)
{
	// Disable error dialogs.
	devilution::HeadlessMode = true;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	// Disable hardware cursor while testing.
	devilution::sgOptions.Graphics.hardwareCursor.SetValue(false);
#endif

	// Store listings render an item's sprite next to its description when this is on, which
	// asserts on a null ClxSprite in tests since no real item graphics are ever loaded here.
	devilution::sgOptions.Gameplay.showItemGraphicsInStores.SetValue(false);

#ifdef __APPLE__
	devilution::paths::SetAssetsPath(
	    devilution::paths::BasePath() + "devilutionx.app/Contents/Resources/");
#endif

	// Every test process gets its own config and pref folder (round 6 audit, v1.12.231). On Windows the config path is the
	// exe's folder, which the test exes SHARE with the game's Debug build: any test that reached SaveOptions (a tree
	// point's autosave hook, gamemenu_off, the run toggle) wrote the test process's defaults over the player's own
	// diablo.ini - no key bindings, Width=0, deadzone 0. The pref path (saves, telemetry) is sandboxed with it. Tests that
	// need a path of their own still set it themselves.
	std::error_code ec;
	const std::filesystem::path sandbox = std::filesystem::temp_directory_path(ec)
	    / (std::string("orcl-test-") + std::to_string(std::random_device {}()) + "-" + std::to_string(std::random_device {}()));
	std::filesystem::create_directories(sandbox, ec);
	if (!ec) {
		const std::string dir = sandbox.string() + "/";
		devilution::paths::SetConfigPath(dir);
		devilution::paths::SetPrefPath(dir);
	}

	testing::InitGoogleTest(&argc, argv);
	const int result = RUN_ALL_TESTS();
	if (!ec)
		std::filesystem::remove_all(sandbox, ec);
	return result;
}
