#include "oracool/hero_chunks.h"

#include <algorithm>
#include <cstring>
#include <iterator>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "oracool/event_log.h"
#include "oracool/readied_spells.h"
#include "oracool/skill_points.h"
#include "panels/spell_book.hpp" // AbilityFKeyCount
#include "player.h"
#include "spells.h" // IsValidSpell

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

/**
 * @brief The superseded tag-5 payload: twenty Paladin aura investments. Migrated rather than
 * dropped - the Paladin's auras sit at positions 9-28 of its class list, so aura i lands at
 * class slot 9 + i and a hero saved between the two builds keeps the points it paid for.
 */
void ApplyPaladinAuras(Player &player, const uint8_t *payload, size_t len)
{
	if (len < 1)
		return;
	constexpr size_t PaladinFirstAuraSlot = 9;
	const size_t count = std::min<size_t>({ payload[0], len - 1, 20 });
	for (size_t i = 0; i < count; i++) {
		const size_t slot = PaladinFirstAuraSlot + i;
		if (slot >= std::size(player._pClassTreeInvestment))
			break;
		player._pClassTreeInvestment[slot] = payload[1 + i];
	}
}

void ApplyClassTree(Player &player, const uint8_t *payload, size_t len)
{
	if (len < 1)
		return;
	const size_t count = std::min<size_t>({ payload[0], len - 1,
	    std::size(player._pClassTreeInvestment) });
	std::memset(player._pClassTreeInvestment, 0, sizeof(player._pClassTreeInvestment));
	std::memcpy(player._pClassTreeInvestment, payload + 1, count);
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

	{
		const size_t at = BeginChunk(out, HeroChunkActiveAura);
		out.push_back(player._pOracoolActiveAura);
		EndChunk(out, at);
	}

	{
		const size_t at = BeginChunk(out, HeroChunkClassTree);
		const auto count = static_cast<uint8_t>(std::size(player._pClassTreeInvestment));
		out.push_back(count);
		out.insert(out.end(), player._pClassTreeInvestment, player._pClassTreeInvestment + count);
		EndChunk(out, at);
	}

	{
		const size_t at = BeginChunk(out, HeroChunkSpellHotkeys);
		out.push_back(static_cast<uint8_t>(AbilityFKeyCount));
		for (size_t i = 0; i < AbilityFKeyCount; i++)
			out.push_back(PackReadiedSpell(player._pSplHotKey[i]));
		EndChunk(out, at);
	}

	{
		const size_t at = BeginChunk(out, HeroChunkSpellHotkeysLeft);
		out.push_back(static_cast<uint8_t>(AbilityFKeyCount));
		for (size_t i = 0; i < AbilityFKeyCount; i++)
			out.push_back(PackReadiedSpell(player._pSplLHotKey[i]));
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
	//
	// Subtraction-form bounds checks throughout (external audit, 2026-08-17): the additive form
	// `offset + 6 + chunkLen > len` can WRAP when chunkLen approaches UINT32_MAX on a 32-bit
	// size_t, passing the check and then advancing the walk by the same wrapped-tiny amount -
	// an attacker-steered loop over a hostile hero file. `chunkLen > len - offset - 6` compares
	// the same fact and cannot overflow, because both operands are provably in range when it runs.
	size_t offset = 4;
	while (offset < len) {
		if (len - offset < 6) {
			LogEvent("Hero extension tail rejected: truncated chunk header");
			return;
		}
		const uint32_t chunkLen = GetU32(data + offset + 2);
		if (chunkLen > len - offset - 6) {
			LogEvent("Hero extension tail rejected: truncated chunk payload");
			return;
		}
		offset += size_t { 6 } + chunkLen;
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
		case HeroChunkActiveAura:
			// First byte only; anything after it belongs to a newer build's larger payload.
			if (chunkLen >= 1)
				player._pOracoolActiveAura = payload[0];
			break;
		case HeroChunkPaladinAuras:
			ApplyPaladinAuras(player, payload, chunkLen);
			break;
		case HeroChunkClassTree:
			ApplyClassTree(player, payload, chunkLen);
			break;
		case HeroChunkSpellHotkeysLeft:
			if (chunkLen >= 1) {
				const size_t count = std::min<size_t>({ payload[0], chunkLen - 1, AbilityFKeyCount });
				for (size_t i = 0; i < count; i++) {
					SpellID spell = SpellID::Invalid;
					SpellType type = SpellType::Invalid;
					UnpackReadiedSpell(player, payload[1 + i], spell, type);
					if (IsValidSpell(spell)) {
						player._pSplLHotKey[i] = spell;
						player._pSplLTHotKey[i] = type;
					}
				}
			}
			break;
		case HeroChunkSpellHotkeys:
			if (chunkLen >= 1) {
				const size_t count = std::min<size_t>({ payload[0], chunkLen - 1, AbilityFKeyCount });
				for (size_t i = 0; i < count; i++) {
					// UnpackReadiedSpell leaves both outputs untouched on an empty byte, so an
					// unbound slot stays exactly as the fixed struct left it. The TYPE is re-derived
					// from the spell masks, which ApplyHeroChunks' caller has already loaded.
					SpellID spell = SpellID::Invalid;
					SpellType type = SpellType::Invalid;
					UnpackReadiedSpell(player, payload[1 + i], spell, type);
					if (IsValidSpell(spell)) {
						player._pSplHotKey[i] = spell;
						player._pSplTHotKey[i] = type;
					}
				}
			}
			break;
		default:
			// An unknown tag is a chunk from a newer build - skipped, by design.
			break;
		}
		offset += size_t { 6 } + chunkLen; // widened for the same reason as the validation walk
	}

	// AFTER every chunk, because it reads _pSkillInvestment and _pUnspentSkillPoints, and the
	// chunks that fill them may arrive in any order.
	//
	// Points could reach a book spell two ways before the 2026-08-20 rule ("Spells cant be affected
	// by skill points, only by books"): the Abilities window's Spells sheet spent straight into
	// them, and the Sorceress's tree rows were castable spells whose investment was keyed by
	// SpellID. Both doors are shut now, so anything still sitting there is stranded where the player
	// cannot reach it - and for a Sorceress that could be her entire pool. Handing it back is the
	// only honest migration.
	//
	// Idempotent: it zeroes what it refunds, so the next load finds nothing to do. That matters
	// because there is no version gate here - the repair IS its own gate.
	if (const int refunded = RefundBookSpellInvestment(player); refunded > 0 && &player == MyPlayer) {
		LogEvent(fmt::format("Spells are raised by books now - {:d} skill point{:s} returned",
		             refunded, refunded == 1 ? "" : "s"),
		    UiFlags::ColorWhitegold);
	}
}

} // namespace devilution::oracool
