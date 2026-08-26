#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include "utils/attributes.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution {

#ifdef _WIN32
constexpr char DirectorySeparator = '\\';
#define DIRECTORY_SEPARATOR_STR "\\"
#else
constexpr char DirectorySeparator = '/';
#define DIRECTORY_SEPARATOR_STR "/"
#endif

bool FileExists(const char *path);

inline bool FileExists(const std::string &str)
{
	return FileExists(str.c_str());
}

bool DirectoryExists(const char *path);
string_view Dirname(string_view path);
bool FileExistsAndIsWriteable(const char *path);
bool GetFileSize(const char *path, std::uintmax_t *size);

/**
 * @brief Creates a single directory (non-recursively).
 *
 * @return True if the directory already existed or has been created sucessfully.
 */
bool CreateDir(const char *path);

void RecursivelyCreateDir(const char *path);
bool ResizeFile(const char *path, std::uintmax_t size);
void RenameFile(const char *from, const char *to);

/**
 * @brief Moves `from` onto `to`, replacing `to` if it exists, and says whether it worked.
 *
 * `RenameFile` cannot be used for this. It returns void, so a caller cannot tell a move that
 * happened from one that did not, and on Windows it calls `MoveFileW`, which REFUSES when the
 * destination exists - exactly the case this is for.
 *
 * The point of it is the publish step of a save: the new archive is built beside the old one and
 * then swapped in by a single call that either takes effect or does not. There is no moment where
 * the destination is half of each.
 *
 * @return True if `to` now names the file that was `from`.
 */
bool ReplaceFileAtomically(const char *from, const char *to);

void CopyFileOverwrite(const char *from, const char *to);

/**
 * @brief Test seams for the two operations the save's staging step depends on.
 *
 * The save archive is built on a COPY of the existing one, and whether that copy can be made is
 * decided by `CopyFileOverwrite` and `GetFileSize` - neither of which goes anywhere near
 * `LoggedFStream`, so neither could be reached by the write seam. The sixth external audit
 * (2026-08-26) found two faults in exactly that step and both were untestable:
 *
 *   - a failed copy fell back to mutating the LIVE archive, which is the one thing the copy exists
 *     to prevent, and it did so precisely when the disk was full
 *   - a failed `GetFileSize` was read as "the file does not exist", so an archive that could not be
 *     measured would have been replaced by one holding only the current save's records
 *
 * Both now fail closed, and these seams are what let a test say so. Same shape as the write seam:
 * a countdown, -1 disarmed, compiled into every build so the code under test is the code that ships.
 */
DVL_API_FOR_TEST extern int CopyFailureCountdown;

/** @brief After @p successesBeforeFailure more copies, every copy silently does nothing. */
DVL_API_FOR_TEST void FailFileCopiesAfter(int successesBeforeFailure);

/** @brief Disarms the copy seam. */
DVL_API_FOR_TEST void StopFailingFileCopies();

DVL_API_FOR_TEST extern int FileSizeFailureCountdown;

/** @brief After @p successesBeforeFailure more queries, every GetFileSize reports failure. */
DVL_API_FOR_TEST void FailFileSizeQueriesAfter(int successesBeforeFailure);

/** @brief Disarms the size-query seam. */
DVL_API_FOR_TEST void StopFailingFileSizeQueries();

/**
 * @brief Fails exactly ONE size query, then disarms itself.
 *
 * The countdown above fails every query from its trigger onward, which is right for simulating a
 * dead disk and wrong for simulating a single unreadable file. The distinction is not academic: the
 * archive writer queries a size twice during construction, and failing both makes it give up at a
 * different, earlier point than the one under test - so the interesting path is never reached.
 */
DVL_API_FOR_TEST void FailNextFileSizeQuery();
void RemoveFile(const char *path);
FILE *OpenFile(const char *path, const char *mode);

#if (defined(_WIN64) || defined(_WIN32)) && !defined(NXDK)
std::unique_ptr<wchar_t[]> ToWideChar(string_view path);
#endif

} // namespace devilution
