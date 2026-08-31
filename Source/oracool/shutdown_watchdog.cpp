#include "oracool/shutdown_watchdog.h"

#include <cstdlib>

#include <SDL.h>

namespace devilution::oracool {

namespace {

uint32_t WatchdogTimeoutMs = 0;

int SDLCALL WatchdogThread(void * /*unused*/)
{
	SDL_Delay(WatchdogTimeoutMs);

	// _Exit, not exit: exit() runs atexit handlers and static destructors, and those are themselves
	// candidates for the hang this exists to escape. _Exit ends the process without running any of
	// them, which is the entire point - by now the save is written and the archives are read-only,
	// so there is nothing left that wants flushing.
	//
	// Reached ONLY when teardown has already overrun by seconds. A healthy shutdown is under a
	// second and the process is gone long before this line.
	std::_Exit(0);
	return 0;
}

} // namespace

void ArmShutdownWatchdog(uint32_t timeoutMs)
{
	// Idempotent, because it is armed inside DiabloDeinit and nothing guarantees that runs once. A
	// second thread would be harmless but would also be a second timer racing the first, which is
	// the sort of thing that makes a shutdown bug hard to read later.
	static bool armed = false;
	if (armed)
		return;
	armed = true;

	WatchdogTimeoutMs = timeoutMs;

	// Raw SDL_CreateThread and DETACHED rather than the SdlThread wrapper: that wrapper's deleter
	// calls app_fatal("Joinable thread destroyed") if it goes out of scope unjoined, and this thread
	// is deliberately never joined - joining it would mean waiting the full timeout on every clean
	// exit, which is the opposite of what it is for.
	SDL_Thread *thread = SDL_CreateThread(WatchdogThread, "oracool_shutdown_watchdog", nullptr);
	if (thread == nullptr)
		return; // No watchdog is the behaviour this build had all along - no worse, so no fuss.
	SDL_DetachThread(thread);
}

} // namespace devilution::oracool
