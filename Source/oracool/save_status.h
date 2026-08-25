/**
 * @file oracool/save_status.h
 *
 * Oracool: whether the last save actually reached the disk.
 *
 * Exists because of an audit finding (2026-08-26): every write in the save path discarded its
 * result. `EncodeHero` ignored `SaveWriter::WriteFile`, `SaveHelper`'s destructor ignored it too,
 * and the stash cleared its dirty flag whether or not the write succeeded. A full disk, a failing
 * drive or a device pulled mid-save therefore produced a save that reported success, logged
 * "Game saved", and had written nothing.
 *
 * A destructor cannot report failure to its caller, and threading a bool back through half a dozen
 * void functions would have touched far more code than the problem is worth. So failures are
 * recorded here, at file scope, and the save path asks afterwards.
 *
 * The contract is deliberately narrow:
 *
 *   BeginSaveAttempt()   before the first write of a save
 *   NoteSaveWriteFailed()  from wherever a write returns false
 *   SaveAttemptFailed()  after the last write, to decide what to tell the player
 *
 * Nothing here retries or repairs. MpqWriter::WriteFile keeps the PREVIOUS record intact when a
 * write fails, so the character on disk is still the one from the last good save - this is about
 * saying so rather than about recovering.
 */
#pragma once

#include <string>

#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

/** @brief Opens a save attempt, clearing any failure recorded by the previous one. */
void BeginSaveAttempt();

/** @brief Records that @p fileName could not be written. Safe to call more than once. */
void NoteSaveWriteFailed(string_view fileName);

/** @brief Whether any write since BeginSaveAttempt failed. */
bool SaveAttemptFailed();

/** @brief The first file that failed in this attempt, for the message. Empty if none did. */
const std::string &FailedSaveFileName();

} // namespace devilution::oracool
