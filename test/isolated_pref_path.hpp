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
 * is created under the build root, and a test that FAILS leaves its archive behind deliberately -
 * that archive is the evidence, and the whole tree is disposable anyway.
 *
 * A test that PASSES now removes its own directory (DropIsolatedPrefPath, 2026-09-02). The original
 * rule was "always leave it", which was right about the failing case and wrong about the rest: six
 * days of runs had left 413 of these folders holding 33.7 MB, one per test process, and nothing
 * anywhere removed them. Inside a OneDrive-synced tree that is 413 directories of sync traffic for
 * evidence about runs that went green and nobody will ever read.
 *
 * This is the audit's preferred remedy rather than its containment one. RESOURCE_LOCK or RUN_SERIAL
 * would stop the collisions and keep the shared directory, which leaves the pollution in place and
 * makes the suite slower for a reason nothing in it records.
 */
#pragma once

#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

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
	// Per PROCESS was not enough (external audit, 2026-09-06: QA-04): every test in one executable
	// shares a pid, so the first guard to finish deleted the directory the next test was still
	// pointed at, and a whole-binary shuffle run failed on a missing path. The current test's name
	// makes the directory per test; the guard below restores the previous path on the way out.
	std::string path = paths::BasePath() + "test-saves-" + std::to_string(DVL_TEST_GETPID());
	if (const ::testing::TestInfo *info = ::testing::UnitTest::GetInstance()->current_test_info(); info != nullptr) {
		std::string name = std::string(info->test_suite_name()) + "-" + info->name();
		for (char &c : name) {
			if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
				c = '_';
		}
		path += "-" + name;
	}
	path += "/";
	RecursivelyCreateDir(path.c_str());
	paths::SetPrefPath(path);
	return path;
}

/**
 * @brief Removes this process's isolated directory, unless the test failed.
 *
 * Call from a fixture's TearDown. A FAILING test keeps its archive - that is the whole reason these
 * directories are per-process and on disk at all - so the evidence survives exactly when it is worth
 * having, and a green run leaves nothing behind.
 *
 * Errors are swallowed: this is housekeeping, and a test that passed must not be turned red by a
 * file that would not delete. The worst case is one leftover folder, which is where this started.
 */
inline void DropIsolatedPrefPath()
{
	if (::testing::Test::HasFailure())
		return;
	const std::string path = paths::PrefPath();
	if (path.find("test-saves-") == std::string::npos)
		return; // not ours - never delete a directory this helper did not create
	std::error_code ec;
	std::filesystem::remove_all(std::filesystem::path(path), ec);
}

/**
 * @brief Scope guard: an isolated save directory for as long as it is alive.
 *
 * For the tests written as free TEST() bodies rather than fixtures - most of writehero_test - which
 * have no TearDown to hang the cleanup on. One line replaces the bare UseIsolatedPrefPath() call and
 * the directory goes when the test does.
 */
struct IsolatedPrefPathGuard {
	IsolatedPrefPathGuard()
	    : previous(paths::PrefPath())
	{
		UseIsolatedPrefPath();
	}
	~IsolatedPrefPathGuard()
	{
		DropIsolatedPrefPath();
		paths::SetPrefPath(previous); // the next test starts where this one found things, not in a deleted folder
	}
	std::string previous;
	IsolatedPrefPathGuard(const IsolatedPrefPathGuard &) = delete;
	IsolatedPrefPathGuard &operator=(const IsolatedPrefPathGuard &) = delete;
};

} // namespace devilution
