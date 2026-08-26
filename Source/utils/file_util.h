#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

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
void RemoveFile(const char *path);
FILE *OpenFile(const char *path, const char *mode);

#if (defined(_WIN64) || defined(_WIN32)) && !defined(NXDK)
std::unique_ptr<wchar_t[]> ToWideChar(string_view path);
#endif

} // namespace devilution
