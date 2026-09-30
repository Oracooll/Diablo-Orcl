#include "oracool/telemetry.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include <SDL.h>

#include <fmt/format.h>

#include "items.h"
#include "levels/gendung.h"
#include "monster.h"
#include "options.h"
#include "oracool/lesser_uniques.h"
#include "player.h"
#include "utils/file_util.h" // OpenFile - UTF-8 paths
#include "utils/paths.h"

namespace devilution::oracool {

namespace {

/** @brief Per-monster-slot first-hit times (GameTicks + 1), 0 = clock not running. Slot ids recycle
 * across levels; the clock is overwritten on the next first hit, which is exactly right. */
uint32_t FirstHitAtMs[MaxMonsters];
/**
 * @brief Game ticks since the process started, bumped once per GameLogic. The kill clocks count these, not SDL
 * milliseconds: on the wall clock a fight's time included the pause, the Esc menu and alt-tab, and its scale moved
 * with the game speed (round 11 audit, v1.12.236). A tick is written out as 50 ms, the normal speed's tick.
 */
uint32_t GameTicks = 0;
constexpr uint32_t MsPerTick = 50;

/** @brief One id per process run, so rows from different sessions separate cleanly in analysis. */
std::string SessionId;

bool TelemetryEnabled()
{
	return *sgOptions.Oracool.balanceTelemetry;
}

std::string CurrentClock()
{
	std::time_t timeResult = std::time(nullptr);
	const std::tm *localTime = std::localtime(&timeResult);
	if (localTime == nullptr)
		return "--:--:--";
	return fmt::format("{:02d}:{:02d}:{:02d}", localTime->tm_hour, localTime->tm_min, localTime->tm_sec);
}

void EnsureSessionId()
{
	if (!SessionId.empty())
		return;
	std::time_t timeResult = std::time(nullptr);
	const std::tm *localTime = std::localtime(&timeResult);
	if (localTime != nullptr) {
		SessionId = fmt::format("{:04d}{:02d}{:02d}-{:02d}{:02d}{:02d}",
		    localTime->tm_year + 1900, localTime->tm_mon + 1, localTime->tm_mday,
		    localTime->tm_hour, localTime->tm_min, localTime->tm_sec);
	} else {
		SessionId = fmt::format("t{:d}", SDL_GetTicks());
	}
}

/**
 * @brief Appends one row, opening/creating the file per write and closing it again.
 *
 * Per-row open+close rather than a held handle, deliberately: rows are rare (a kill, a death, a
 * pickup), the cost is nothing at that rate, and it guarantees the file is intact after ANY exit -
 * including the crashes this project is hunting. A header row is written when the file is born.
 */
void AppendRow(const std::string &event, const std::string &subject, int value1, int value2)
{
	EnsureSessionId();
	const std::string path = paths::PrefPath() + "balance_telemetry.csv";

	// OpenFile, not fopen: the pref path is UTF-8, and fopen reads it in the ANSI code page - a profile named José had no
	// telemetry at all (round 4 audit, v1.12.229).
	FILE *file = OpenFile(path.c_str(), "ab");
	if (file == nullptr)
		return; // telemetry must never be able to break the game
	// SEEK_END before asking, because in APPEND mode the stream position is not the file size.
	// MSVC leaves it at 0 until the first write (the write is then forced to the end regardless),
	// so the old `ftell(file) == 0` test was true on every single call and wrote the header before
	// every row: 914 header lines against 915 data rows in the first file ever read back.
	//
	// Nothing was lost - the rows are all there and a parser can skip the repeats - but the file was
	// twice the size it should be, and the defect survived five months precisely because a
	// write-only file is never wrong until someone reads it. That is the argument for reading it
	// back on a schedule, not just when a question needs answering.
	std::fseek(file, 0, SEEK_END);
	if (std::ftell(file) == 0) {
		const char header[] = "time,session,event,level,player_level,subject,value1,value2\n";
		std::fwrite(header, sizeof(header) - 1, 1, file);
	}

	const int playerLevel = MyPlayer != nullptr ? MyPlayer->_pLevel : 0;
	const std::string row = fmt::format("{:s},{:s},{:s},{:d},{:d},{:s},{:d},{:d}\n",
	    CurrentClock(), SessionId, event, currlevel, playerLevel,
	    TelemetryEscapeCsvField(subject), value1, value2);
	std::fwrite(row.data(), row.size(), 1, file);
	std::fclose(file);
}

} // namespace

std::string TelemetryEscapeCsvField(const std::string &field)
{
	if (field.find_first_of(",\"\n") == std::string::npos)
		return field;
	std::string escaped = "\"";
	for (const char c : field) {
		if (c == '"')
			escaped += '"';
		escaped += c;
	}
	escaped += '"';
	return escaped;
}

void TelemetryResetLevelTimers()
{
	// Unconditional - NOT gated on TelemetryEnabled(). A clock started while telemetry was on must
	// not survive being switched off and back on, or the first kill after that logs a fight length
	// measured from a different level.
	std::fill(std::begin(FirstHitAtMs), std::end(FirstHitAtMs), 0U);
}

bool TelemetryHasRunningKillClock(const Monster &monster)
{
	const size_t id = static_cast<size_t>(monster.getId());
	return id < MaxMonsters && FirstHitAtMs[id] != 0;
}

void TelemetryRecordFirstHit(const Monster &monster)
{
	if (!TelemetryEnabled())
		return;
	const size_t id = static_cast<size_t>(monster.getId());
	if (id >= MaxMonsters)
		return;
	// Stored BIASED BY ONE, so 0 can still mean "no clock running" without corrupting the value.
	//
	// This was `| 1U`, which sets the sentinel by damaging the measurement: on an even tick it made
	// the stored time one millisecond LATER than the hit, so a kill in that same millisecond
	// computed `now - (t + 1)`, underflowed to about four billion, and was then thrown away by the
	// ten-minute sanity filter as a stale clock. On an odd tick it read zero. Either way a
	// same-millisecond one-shot recorded no time at all - which is exactly the kill the 2026-08-21
	// fix moved this call to ApplyMonsterDamage in order to capture (external audit, 2026-08-25).
	if (FirstHitAtMs[id] == 0)
		FirstHitAtMs[id] = GameTicks + 1U;
}

void TelemetryTick()
{
	GameTicks++;
}

void TelemetryRecordKill(const Monster &monster)
{
	if (!TelemetryEnabled())
		return;
	const size_t id = static_cast<size_t>(monster.getId());
	uint32_t timeToKillMs = 0;
	if (id < MaxMonsters && FirstHitAtMs[id] != 0) {
		// Un-bias by the one TelemetryRecordFirstHit added.
		timeToKillMs = (GameTicks - (FirstHitAtMs[id] - 1U)) * MsPerTick;
		FirstHitAtMs[id] = 0;
		// Audit fix (2026-08-16): monster slots recycle across levels, and a monster that was HIT
		// but never killed leaves its clock running - the next kill in that slot would then log a
		// nonsense hours-long TTK. Anything over ten minutes is a stale clock, not a fight.
		if (timeToKillMs > 10U * 60U * 1000U)
			timeToKillMs = 0;
	}
	AppendRow("kill", GetMonsterDisplayName(monster),
	    static_cast<int>(monster.level(sgGameInitInfo.nDifficulty)), static_cast<int>(timeToKillMs));
}

void TelemetryRecordPlayerDeath(const std::string &source)
{
	if (!TelemetryEnabled())
		return;
	AppendRow("death", source, 0, 0);
}

void TelemetryRecordPickup(const Item &item)
{
	if (!TelemetryEnabled() || item.isEmpty())
		return;
	AppendRow("pickup", std::string(item.getName()),
	    static_cast<int>(item._iOracoolTier), GetItemSellValue(item));
}

void TelemetryRecordDebugCommand(const std::string &command)
{
	if (!TelemetryEnabled())
		return;
	AppendRow("debug", command, 0, 0);
}

} // namespace devilution::oracool
