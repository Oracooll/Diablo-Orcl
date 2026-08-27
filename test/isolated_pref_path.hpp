/**
 * @file isolated_pref_path.hpp
 *
 * A private save directory for a save-mutating test process.
 *
 * External audit of v1.9.88, finding 8. The save fixtures all pointed `paths::SetPrefPath` at
 * `paths::BasePath()` - one directory, shared by every test binary - and then wrote fixed names into
 * it: `single_0.sv`, `multi_0.sv`, `stash.sv`. `gtest_discover_tests` registers every TEST as its own
 * CTest case, so `ctest -j` runs them concurrently and they overwrite each other's archives.
 *
 * The symptom was not a stable failure, which is what made it expensive: `ctest -j 8` failed a
 * DIFFERENT set of save tests on each run, and every one of them passed serially. That cost real time
 * across this session - each parallel run had to be re-run serially to find out whether a red result
 * meant anything, and a genuine regression hiding among the noise would have been easy to wave away.
 *
 * Each CTest case is a separate PROCESS, so the process id is enough to separate them. The directory
 * is created under the build root and left behind deliberately: a test that fails is easier to
 * diagnose with its archive still on disk, and the whole tree is disposable.
 *
 * This is the audit's preferred remedy rather than its containment one. RESOURCE_LOCK or RUN_SERIAL
 * would stop the collisions and keep the shared directory, which leaves the pollution in place and
 * makes the suite slower for a reason nothing in it records.
 */
#pragma once

#include <string>

#include "utils/file_util.h"
#include "utils/paths.h"

#ifdef _WIN32
#include <process.h>
#define DVL_TEST_GETPID _getpid
#else
#include <unistd.h>
#define DVL_TEST_GETPID getpid
#endif

namespace devilution {

/**
 * @brief Points PrefPath at a directory private to this process, and returns it.
 *
 * Call from a save-mutating fixture's SetUp instead of `paths::SetPrefPath(paths::BasePath())`.
 */
inline std::string UseIsolatedPrefPath()
{
	std::string path = paths::BasePath() + "test-saves-" + std::to_string(DVL_TEST_GETPID()) + "/";
	RecursivelyCreateDir(path.c_str());
	paths::SetPrefPath(path);
	return path;
}

} // namespace devilution
