#pragma once

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#include "utils/attributes.h"
#include "utils/file_util.h"
#include "utils/log.hpp"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

/**
 * @brief Test seam: makes writes start failing after this many more succeed. -1 disables it.
 *
 * Every byte the save path puts on disk goes through LoggedFStream::Write - the hero file, the
 * stash, the level blobs - so this one counter is enough to simulate a full disk, a failing drive
 * or a device pulled mid-save.
 *
 * It exists because the second external audit (2026-08-26) asked for failure-injection tests and
 * there was no way to write them. That mattered: the save path had just been changed so a failed
 * write could no longer destroy the record it was replacing, and the only evidence for that fix was
 * that saving still worked - which is evidence about the SUCCESS path. A guarantee about failure
 * that has never seen a failure is a guess.
 *
 * Deliberately compiled into every build rather than hidden behind a debug macro, so the code the
 * test exercises is the code that ships. The cost is one comparison against a static int per write,
 * against a function that has just called fwrite.
 */
DVL_API_FOR_TEST extern int WriteFailureCountdown;

/** @brief Arms the seam above: after @p successesBeforeFailure more writes, every write fails. */
DVL_API_FOR_TEST void FailWritesAfter(int successesBeforeFailure);

/** @brief Disarms it. Tests must call this, or a later test inherits a broken disk. */
DVL_API_FOR_TEST void StopFailingWrites();

// A wrapper around `FILE *` that logs errors.
struct LoggedFStream {
public:
	bool Open(const char *path, const char *mode)
	{
		s_ = OpenFile(path, mode);
		return CheckError(s_ != nullptr, "fopen(\"{}\", \"{}\")", path, mode);
	}

	void Close()
	{
		if (s_ != nullptr) {
			std::fclose(s_);
			s_ = nullptr;
		}
	}

	[[nodiscard]] bool IsOpen() const
	{
		return s_ != nullptr;
	}

	bool Seekp(long pos, int dir = SEEK_SET)
	{
		return CheckError(std::fseek(s_, pos, dir) == 0,
		    "fseek({}, {})", pos, DirToString(dir));
	}

	bool Tellp(long *result)
	{
		*result = std::ftell(s_);
		return CheckError(*result != -1L,
		    "ftell() = {}", *result);
	}

	bool Write(const char *data, size_t size)
	{
		// The injected failure happens INSTEAD of the write, not after it - a short write that
		// still put bytes on disk would be a different fault, and the one being simulated is the
		// one where the record does not arrive.
		if (WriteFailureCountdown >= 0) {
			if (WriteFailureCountdown == 0)
				return false;
			WriteFailureCountdown--;
		}
		return CheckError(std::fwrite(data, size, 1, s_) == 1,
		    "fwrite(data, {})", size);
	}

	bool Read(char *out, size_t size)
	{
		return CheckError(std::fread(out, size, 1, s_) == 1,
		    "fread(out, {})", size);
	}

private:
	static const char *DirToString(int dir);

	template <typename... PrintFArgs>
	bool CheckError(bool ok, const char *fmt, PrintFArgs... args)
	{
		if (!ok) {
			std::string fmtWithError = fmt;
			fmtWithError.append(": failed with \"{}\"");
			const char *errorMessage = std::strerror(errno);
			if (errorMessage == nullptr)
				errorMessage = "";
			LogError(LogCategory::System, fmtWithError.c_str(), args..., errorMessage);
		} else {
			LogVerbose(LogCategory::System, fmt, args...);
		}
		return ok;
	}

	FILE *s_ = nullptr;
};

} // namespace devilution
