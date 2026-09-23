#pragma once
/**
 * @file oracool/dev_notes.h
 *
 * Oracool: the in-game /dev command (user, 2026-09-23: "i need you to create a command /dev which
 * will keep a development.md file in the folder with every message i input after the command.
 * timestamped. datestamped. every once in a while i will feed you this file and you will process
 * the notes in it" - and, asked which /dev: "Type /dev in the game").
 *
 * A note typed while PLAYING, filed without leaving the game. Nothing is acted on when it is filed;
 * development.md is an inbox that is handed back later and worked through in bulk.
 */

#include <string>

#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

/**
 * @brief Appends @p note to development.md, stamped with the date, the time, the build and where the
 * hero is standing.
 *
 * @return An empty string when it was filed - the confirmation goes to the event log - or the
 * reason it was not, for the chat line to show.
 */
std::string LogDevelopmentNote(string_view note);

} // namespace devilution::oracool
