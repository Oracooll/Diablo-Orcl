#include "oracool/run_toggle.h"

#include "oracool/event_log.h"
#include "oracool/oracool.h"

namespace devilution::oracool {

namespace {

bool RunEnabled = false;

} // namespace

bool IsRunEnabled()
{
	return RunEnabled && IsSinglePlayer();
}

void ToggleRun()
{
	RunEnabled = !RunEnabled;
	LogEvent(RunEnabled ? "Running" : "Walking", UiFlags::ColorWhitegold);
}

} // namespace devilution::oracool
