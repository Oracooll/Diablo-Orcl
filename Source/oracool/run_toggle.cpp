#include "oracool/run_toggle.h"

#include "options.h" // the toggle's state lives in the INI - see below
#include "oracool/event_log.h"
#include "oracool/oracool.h"

namespace devilution::oracool {

// The state lives in the INI, not in a static here (user, 2026-09-22: "i want Runnning/Walking state
// to be remembered upon exiting the game and resumed on next new game").
//
// A file-local bool survived one game reaching the next - statics outlive a game in this engine -
// but died with the process, so the gait always came back walking. There is no character to hang it
// on either, since V1 always starts a new game; the player's preferred gait is a preference, and
// preferences live in the options.

bool IsRunEnabled()
{
	return *sgOptions.Oracool.runEnabled && IsSinglePlayer();
}

void ToggleRun()
{
	const bool now = !*sgOptions.Oracool.runEnabled;
	sgOptions.Oracool.runEnabled.SetValue(now);
	// And WRITTEN, now (user, 2026-09-23 dev note: "make sure run/walk state is remembered between
	// games"). v1.12.146 put the gait in the INI but only set it in memory; the INI is written at
	// start-up and on a handful of settings screens, never on the way out, so a gait flipped in play
	// was gone by the next launch. A flip is rare enough that writing the file each time costs
	// nothing.
	SaveOptions();
	LogEvent(now ? "Running" : "Walking", UiFlags::ColorWhitegold);
}

} // namespace devilution::oracool
