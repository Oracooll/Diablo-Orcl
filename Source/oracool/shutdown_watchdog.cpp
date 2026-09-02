#include "oracool/shutdown_watchdog.h"

#include <chrono>
#include <cstdlib>
#include <thread>

namespace devilution::oracool {

namespace {

uint32_t WatchdogTimeoutMs = 0;

void WatchdogThread()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(WatchdogTimeoutMs));

	// _Exit, not exit: exit() runs atexit handlers and static destructors, and those are themselves
	// candidates for the hang this exists to escape. _Exit ends the process without running any of
	// them, which is the entire point - by now the save is written and the archives are read-only,
	// so there is nothing left that wants flushing.
	//
	// Reached ONLY when teardown has already overrun by seconds. A healthy shutdown is under a
	// second and the process is gone long before this line.
	std::_Exit(0);
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

	// A PLAIN std::thread, detached, and deliberately not an SDL one (2026-09-02).
	//
	// This thread exists to survive a teardown that is going wrong, and SDL is part of what is being
	// torn down - DiabloDeinit ends in SDL_Quit. A watchdog whose timer and whose thread bookkeeping
	// both belong to the subsystem it is watching is depending on the thing it cannot depend on. The
	// standard library's sleep is a syscall and needs nobody's state.
	//
	// Detached, never joined: joining would mean waiting the full timeout on every clean exit, which
	// is the opposite of what this is for. The SdlThread wrapper is doubly wrong here - its deleter
	// calls app_fatal("Joinable thread destroyed") if it goes out of scope unjoined.
	std::thread(WatchdogThread).detach();
}

} // namespace devilution::oracool
