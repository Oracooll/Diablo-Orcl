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

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
