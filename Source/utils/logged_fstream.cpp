#include "utils/logged_fstream.hpp"

namespace devilution {

const char *LoggedFStream::DirToString(int dir)
{
	switch (dir) {
	case SEEK_SET:
		return "SEEK_SET";
	case SEEK_END:
		return "SEEK_END";
	case SEEK_CUR:
		return "SEEK_CUR";
	default:
		return "invalid";
	}
}

} // namespace devilution

namespace devilution {

// -1: the seam is disarmed and Write behaves exactly as it always did. See the header for why this
// is compiled into every build rather than hidden behind a debug macro.
int WriteFailureCountdown = -1;

void FailWritesAfter(int successesBeforeFailure)
{
	WriteFailureCountdown = successesBeforeFailure < 0 ? 0 : successesBeforeFailure;
}

void StopFailingWrites()
{
	WriteFailureCountdown = -1;
}

} // namespace devilution
