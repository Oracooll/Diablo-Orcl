#include "oracool/save_status.h"

#include "utils/log.hpp"

namespace devilution::oracool {

namespace {

bool Failed = false;
std::string FailedFile;

} // namespace

void BeginSaveAttempt()
{
	Failed = false;
	FailedFile.clear();
}

void NoteSaveWriteFailed(string_view fileName)
{
	// The FIRST failure is kept rather than the last. A save writes several files, and once the
	// disk has refused one it will usually refuse the rest - the first name is the one that says
	// where it started going wrong.
	if (!Failed) {
		Failed = true;
		FailedFile.assign(fileName.data(), fileName.size());
	}
	LogError("Oracool save: writing \"{:s}\" failed", FailedFile);
}

bool SaveAttemptFailed()
{
	return Failed;
}

const std::string &FailedSaveFileName()
{
	return FailedFile;
}

} // namespace devilution::oracool
