#include "oracool/hero_chunks.h"

#include <algorithm>
#include <cstring>

#include "oracool/event_log.h"
#include "player.h"

namespace devilution::oracool {

namespace {

void PutU16(std::vector<uint8_t> &out, uint16_t value)
{
	out.push_back(static_cast<uint8_t>(value & 0xFF));
	out.push_back(static_cast<uint8_t>(value >> 8));
}

void PutU32(std::vector<uint8_t> &out, uint32_t value)
{
	PutU16(out, static_cast<uint16_t>(value & 0xFFFF));
	PutU16(out, static_cast<uint16_t>(value >> 16));
}

void PutU64(std::vector<uint8_t> &out, uint64_t value)
{
	PutU32(out, static_cast<uint32_t>(value & 0xFFFFFFFFU));
	PutU32(out, static_cast<uint32_t>(value >> 32));
}

uint16_t GetU16(const uint8_t *p)
{
	return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t GetU32(const uint8_t *p)
{
	return static_cast<uint32_t>(GetU16(p)) | (static_cast<uint32_t>(GetU16(p + 2)) << 16);
}

uint64_t GetU64(const uint8_t *p)
{
	return static_cast<uint64_t>(GetU32(p)) | (static_cast<uint64_t>(GetU32(p + 4)) << 32);
}

/** @brief Opens a chunk in @p out and returns the offset where its length must be patched in. */
size_t BeginChunk(std::vector<uint8_t> &out, uint16_t tag)
{
	PutU16(out, tag);
	const size_t lengthAt = out.size();
	PutU32(out, 0);
	return lengthAt;
}

void EndChunk(std::vector<uint8_t> &out, size_t lengthAt)
{
	const uint32_t length = static_cast<uint32_t>(out.size() - lengthAt - 4);
	out[lengthAt] = static_cast<uint8_t>(length & 0xFF);
	out[lengthAt + 1] = static_cast<uint8_t>((length >> 8) & 0xFF);
	out[lengthAt + 2] = static_cast<uint8_t>((length >> 16) & 0xFF);
	out[lengthAt + 3] = static_cast<uint8_t>(length >> 24);
}

void ApplySkillPoints(Player &player, const uint8_t *payload, size_t len)
{
	if (len < 3)
		return;
	player._pUnspentSkillPoints = GetU16(payload);
	const size_t count = std::min<size_t>({ payload[2], len - 3, MAX_SPELLS });
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	std::memcpy(player._pSkillInvestment, payload + 3, count);
}

void ApplyWaypoints64(Player &player, const uint8_t *payload, size_t len)
{
	if (len < 4 * sizeof(uint64_t))
		return;
	for (int difficulty = 0; difficulty < 4; difficulty++) {
		const uint64_t mask = GetU64(payload + difficulty * sizeof(uint64_t));
		for (size_t i = 1; i < Player::MaxWaypointSlots; i++)
			player._pWaypointUnlocked[difficulty][i] = (mask & (uint64_t(1) << (i - 1))) != 0;
	}
}

} // namespace

std::vector<uint8_t> BuildHeroChunkTail(const Player &player)
{
	std::vector<uint8_t> out;
	PutU32(out, HeroChunkMagic);

	{
		const size_t at = BeginChunk(out, HeroChunkSkillPoints);
		PutU16(out, player._pUnspentSkillPoints);
		out.push_back(static_cast<uint8_t>(MAX_SPELLS));
		out.insert(out.end(), player._pSkillInvestment, player._pSkillInvestment + MAX_SPELLS);
		EndChunk(out, at);
	}

	{
		const size_t at = BeginChunk(out, HeroChunkWaypoints64);
		for (int difficulty = 0; difficulty < 4; difficulty++) {
			uint64_t mask = 0;
			for (size_t i = 1; i < Player::MaxWaypointSlots; i++) {
				if (player._pWaypointUnlocked[difficulty][i])
					mask |= uint64_t(1) << (i - 1);
			}
			PutU64(out, mask);
		}
		EndChunk(out, at);
	}

	return out;
}

void ApplyHeroChunks(Player &player, const uint8_t *data, size_t len)
{
	if (data == nullptr || len == 0)
		return; // a pre-tail hero - the fixed struct said everything it has to say
	if (len < 4 || GetU32(data) != HeroChunkMagic) {
		LogEvent("Hero extension tail rejected: bad magic");
		return;
	}

	// Validate the WHOLE walk before applying anything: a truncated tail must not half-apply.
	size_t offset = 4;
	while (offset < len) {
		if (offset + 6 > len) {
			LogEvent("Hero extension tail rejected: truncated chunk header");
			return;
		}
		const uint32_t chunkLen = GetU32(data + offset + 2);
		if (offset + 6 + chunkLen > len) {
			LogEvent("Hero extension tail rejected: truncated chunk payload");
			return;
		}
		offset += 6 + chunkLen;
	}

	offset = 4;
	while (offset < len) {
		const uint16_t tag = GetU16(data + offset);
		const uint32_t chunkLen = GetU32(data + offset + 2);
		const uint8_t *payload = data + offset + 6;
		switch (tag) {
		case HeroChunkSkillPoints:
			ApplySkillPoints(player, payload, chunkLen);
			break;
		case HeroChunkWaypoints64:
			ApplyWaypoints64(player, payload, chunkLen);
			break;
		default:
			// An unknown tag is a chunk from a newer build - skipped, by design.
			break;
		}
		offset += 6 + chunkLen;
	}
}

} // namespace devilution::oracool
