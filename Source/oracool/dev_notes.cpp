#include "oracool/dev_notes.h"

#include <cstdio>
#include <ctime>

#include "config.h" // ORACOOL_VERSION, ORACOOL_SOURCE_DIR
#include "levels/gendung.h"
#include "oracool/event_log.h"
#include "oracool/rift.h"
#include "player.h"
#include "utils/file_util.h"
#include "utils/language.h"
#include "utils/paths.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

/**
 * @brief Where the inbox lives.
 *
 * The REPOSITORY ROOT in a Debug build - the same development.md the notes are processed from, so a
 * note typed in the game and a note typed to Claude land in one file. The build puts the path in
 * config.h; it is read only here and only under _DEBUG, so a release binary never carries the
 * developer's local path and files its notes beside the saves instead.
 */
std::string InboxPath()
{
#ifdef _DEBUG
	return StrCat(ORACOOL_SOURCE_DIR, "/development.md");
#else
	return StrCat(paths::PrefPath(), "development.md");
#endif
}

/** @brief The machine's local date and time, "2026-09-23 14:05:12". */
std::string Stamp()
{
	const std::time_t now = std::time(nullptr);
	std::tm local {};
#ifdef _WIN32
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	char text[32];
	if (std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local) == 0)
		return "unknown time";
	return text;
}

/**
 * @brief Where the hero is standing, in the words a bug report needs.
 *
 * The one thing a note typed in the game can carry that a note typed anywhere else cannot. "The
 * belt icons look off" is a note; "the belt icons look off - town (54,72)" is a note that says which
 * build and which screen it was looking at, and that is most of what processing it needs.
 */
std::string Where()
{
	if (MyPlayer == nullptr)
		return "no game";
	std::string place;
	if (InRift())
		place = "rift";
	else if (leveltype == DTYPE_TOWN)
		place = "town";
	else if (setlevel)
		place = StrCat("set level ", static_cast<int>(setlvlnum));
	else
		place = StrCat("dlvl ", static_cast<int>(currlevel));
	const Point tile = MyPlayer->position.tile;
	return StrCat(place, " (", tile.x, ",", tile.y, ")");
}

} // namespace

std::string LogDevelopmentNote(string_view note)
{
	// Leading spaces are the separator the chat line leaves after "/dev"; the note itself is
	// otherwise filed exactly as typed.
	while (!note.empty() && note.front() == ' ')
		note.remove_prefix(1);
	if (note.empty())
		return std::string(_("Usage: /dev <note>"));

	const std::string path = InboxPath();
	FILE *file = OpenFile(path.c_str(), "ab");
	if (file == nullptr)
		return StrCat(_("Could not open "), path);

	// The same shape the Claude-side /dev files (.claude/commands/dev.md) - a level-two heading
	// with the stamp, a blank line, the note - with the build and the place added, because the game
	// knows them and nothing else does. LF throughout: the file is written in binary so Windows does
	// not turn this into a mixed-ending file the moment both commands have written to it.
	const std::string entry = StrCat("\n## ", Stamp(), " | v", ORACOOL_VERSION, " | ", Where(), "\n\n", note, "\n");
	const bool written = std::fwrite(entry.data(), 1, entry.size(), file) == entry.size();
	std::fclose(file);
	if (!written)
		return StrCat(_("Could not write to "), path);

	LogEvent(StrCat(_("Noted in development.md: "), note), UiFlags::ColorWhitegold);
	return {};
}

} // namespace devilution::oracool
