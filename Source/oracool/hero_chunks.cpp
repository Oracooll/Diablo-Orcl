#include "oracool/hero_chunks.h"

#include "oracool/class_skills.h" // RefreshInnateSpells - the tail is what makes a tree skill known
#include "oracool/class_tree.h"
#include "oracool/signets.h"

#include <algorithm>
#include <optional>
#include <limits>
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

/** @brief Tag 18: the same with a two-byte count. Written after tag 1, so applying in order lets it win. */
void ApplySkillPoints16(Player &player, const uint8_t *payload, size_t len)
{
	if (len < 4)
		return;
	player._pUnspentSkillPoints = GetU16(payload);
	const size_t count = std::min<size_t>({ GetU16(payload + 2), len - 4, MAX_SPELLS });
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	std::memcpy(player._pSkillInvestment, payload + 4, count);
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
	// Might's class index today: 8 since Holy Bolt's row came out (v1.9.301), which shifted every later row down one and
	// left this 9 naming Holy Fire (round 23 audit, v1.12.248).
	constexpr size_t PaladinFirstAuraSlot = 8;
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

/**
 * @brief Restores the burning aura from (hero class, class-relative index).
 *
 * The stable representation. Refused rather than guessed when the class does not match the
 * character - a save whose aura belongs to somebody else is not a puzzle to solve, it is a
 * value to drop.
 */
void ApplyActiveAuraRelative(Player &player, uint8_t heroClass, uint8_t relativeIndex)
{
	if (heroClass == 0xFF || relativeIndex == 0xFF) {
		player._pOracoolActiveAura = static_cast<uint16_t>(ClassTreeSkill::None);
		return;
	}
	if (static_cast<HeroClass>(heroClass) != player._pClass) {
		player._pOracoolActiveAura = static_cast<uint16_t>(ClassTreeSkill::None);
		return;
	}
	const std::optional<ClassTreeSkill> skill =
	    ClassTreeSkillAtIndex(player._pClass, relativeIndex);
	player._pOracoolActiveAura = static_cast<uint16_t>(
	    skill.has_value() ? *skill : ClassTreeSkill::None);
}

/**
 * @brief Reads a tag-4 aura value written by a build before the relative form existed.
 *
 * Audit finding, 2026-08-26. Tag 4 is an ABSOLUTE ClassTreeSkill ordinal, and v1.9.45 appended 110
 * passive rows - one batch at the end of each class's block - which moved every ordinal after the
 * Paladin's. A Bard who saved at v1.9.44 with Melody of Life stored 121; in the current enum 121 is
 * a Rogue row, so the class check threw it away and she loaded with no song playing. Silent, and
 * for the four classes past the Paladin it was every aura they had.
 *
 * Tag 13 stores the class and the index WITHIN that class instead, which is immune to this, and it
 * is what every save written from 2026-08-25 carries. This function exists only for the saves
 * written before it.
 *
 * The translation is a block-offset one because of a property that was checked rather than assumed:
 * across all 163 legacy rows, every one still sits at the same index within its own class block.
 * The additions went on the END of each block, so relative position was preserved throughout. That
 * makes the legacy ordinal convertible into exactly the (class, relative index) pair tag 13 would
 * have stored, and from there the existing path does the rest.
 *
 * Every released build that had auras used this one layout - v1.9.31, v1.9.42, v1.9.43 and v1.9.44
 * all carry 163 rows with these same block starts - so there is one legacy layout to know about
 * and not a series of them.
 */
void ApplyLegacyAbsoluteAura(Player &player, uint16_t rawValue)
{
	// The pre-v1.9.45 enum: 163 rows in six class blocks, in this order.
	struct LegacyBlock {
		HeroClass heroClass;
		uint16_t first;
		uint16_t count;
	};
	constexpr LegacyBlock LegacyBlocks[] = {
		{ HeroClass::Warrior, 0, 31 }, // displayed as the Paladin
		{ HeroClass::Barbarian, 31, 30 },
		{ HeroClass::Sorcerer, 61, 30 },
		{ HeroClass::Rogue, 91, 30 },
		{ HeroClass::Bard, 121, 21 },
		{ HeroClass::Monk, 142, 21 },
	};
	constexpr uint16_t LegacySkillCount = 163;

	if (rawValue >= LegacySkillCount) {
		// None (0xFFFF), or a value no legacy build could have written. Nothing to recover.
		player._pOracoolActiveAura = static_cast<uint16_t>(ClassTreeSkill::None);
		return;
	}

	for (const LegacyBlock &block : LegacyBlocks) {
		if (rawValue < block.first || rawValue >= block.first + block.count)
			continue;
		if (block.heroClass != player._pClass) {
			// An aura belonging to another class is not a puzzle to solve - the same refusal
			// ApplyActiveAuraRelative makes, for the same reason.
			player._pOracoolActiveAura = static_cast<uint16_t>(ClassTreeSkill::None);
			return;
		}
		const std::optional<ClassTreeSkill> skill =
		    ClassTreeSkillAtIndex(player._pClass, static_cast<uint8_t>(rawValue - block.first));
		player._pOracoolActiveAura = static_cast<uint16_t>(
		    skill.has_value() ? *skill : ClassTreeSkill::None);
		return;
	}
	player._pOracoolActiveAura = static_cast<uint16_t>(ClassTreeSkill::None);
}

/**
 * @brief Reads a two-byte tag-4 aura: already in today's numbering, but still not trusted.
 *
 * The v1.9.45-to-v1.9.57 window, where the field had been widened for the 273-row enum but the
 * class-relative tag 13 did not yet exist. The number means what it says; what it does not carry is
 * any guarantee of being a real row, or of belonging to this character.
 *
 * Both are checked here for the same reason the other two paths check them: a save is untrusted
 * input even when this program wrote it, and an aura belonging to somebody else is a value to drop
 * rather than a puzzle to solve.
 */
void ApplyCurrentAbsoluteAura(Player &player, uint16_t rawValue)
{
	if (rawValue >= static_cast<uint16_t>(ClassTreeSkillCount)) {
		// None (0xFFFF) included - there is no row here to light.
		player._pOracoolActiveAura = static_cast<uint16_t>(ClassTreeSkill::None);
		return;
	}
	const ClassTreeSkill skill = static_cast<ClassTreeSkill>(rawValue);
	if (GetClassTreeSkillData(skill).heroClass != player._pClass) {
		player._pOracoolActiveAura = static_cast<uint16_t>(ClassTreeSkill::None);
		return;
	}
	player._pOracoolActiveAura = rawValue;
}

void ApplyPassiveSlots(Player &player, const uint8_t *payload, size_t len)
{
	if (len < 1)
		return;
	const size_t count = std::min<size_t>({ payload[0], len - 1,
	    std::size(player._pPassiveSlots) });
	// Cleared first, so a save written with FEWER slots than this build has leaves the extra ones
	// empty rather than carrying whatever the character happened to be constructed with.
	std::memset(player._pPassiveSlots, 0xFF, sizeof(player._pPassiveSlots));
	std::memcpy(player._pPassiveSlots, payload + 1, count);
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

/**
 * @brief One button's F-key bindings, from a chunk payload already lifted out of the tail.
 *
 * Takes the arrays rather than a "left" flag: the two buttons differ in nothing but which pair of
 * arrays they write, and a flag would be a second place to get that pairing wrong.
 *
 * UnpackReadiedSpell leaves both outputs untouched on an empty byte, so an unbound slot stays exactly
 * as the fixed struct left it, and it drops a binding the character cannot cast - which is only the
 * right answer if the caller has established what the character CAN cast first. See the call site.
 */
/** @brief The two-byte form of the same (tags 16 and 17). */
void ApplyPackedHotkeys16(Player &player, const std::vector<uint8_t> &packed, SpellID *keys, SpellType *types)
{
	if (packed.empty())
		return;
	const size_t count = std::min<size_t>({ packed[0], (packed.size() - 1) / 2, AbilityFKeyCount });
	for (size_t i = 0; i < count; i++) {
		SpellID spell = SpellID::Invalid;
		SpellType type = SpellType::Invalid;
		UnpackReadiedSpell16(player, GetU16(packed.data() + 1 + i * 2), spell, type);
		if (IsValidSpell(spell)) {
			keys[i] = spell;
			types[i] = type;
		}
	}
}

/** @brief The last tail's F-key chunks, kept for ReapplyHeroHotkeys (round 29 audit). */
struct PendingHotkeys {
	std::vector<uint8_t> right;
	std::vector<uint8_t> left;
	std::vector<uint8_t> right16;
	std::vector<uint8_t> left16;
	bool readied16 = false;
	uint16_t readiedRight16 = 0;
	uint16_t readiedLeft16 = 0;
} LastHotkeys;

void ApplyPackedHotkeys(Player &player, const std::vector<uint8_t> &packed, SpellID *keys, SpellType *types)
{
	if (packed.empty())
		return;
	const size_t count = std::min<size_t>({ packed[0], packed.size() - 1, AbilityFKeyCount });
	for (size_t i = 0; i < count; i++) {
		SpellID spell = SpellID::Invalid;
		SpellType type = SpellType::Invalid;
		UnpackReadiedSpell(player, packed[1 + i], spell, type);
		if (IsValidSpell(spell)) {
			keys[i] = spell;
			types[i] = type;
		}
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
		// One byte of count carries 255 at most; the rest ride tag 18 below, which an older build simply skips.
		const size_t inByte = std::min<size_t>(MAX_SPELLS, 255);
		out.push_back(static_cast<uint8_t>(inByte));
		out.insert(out.end(), player._pSkillInvestment, player._pSkillInvestment + inByte);
		EndChunk(out, at);
	}
	{
		const size_t at = BeginChunk(out, HeroChunkSkillPoints16);
		PutU16(out, player._pUnspentSkillPoints);
		PutU16(out, static_cast<uint16_t>(MAX_SPELLS));
		out.insert(out.end(), player._pSkillInvestment, player._pSkillInvestment + MAX_SPELLS);
		EndChunk(out, at);
	}
	{
		const size_t at = BeginChunk(out, HeroChunkSpellLevels);
		static_assert(std::size(Player {}._pSplLvl) <= 255, "the count is one byte");
		out.push_back(static_cast<uint8_t>(std::size(player._pSplLvl)));
		out.insert(out.end(), std::begin(player._pSplLvl), std::end(player._pSplLvl));
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
		// Two bytes since 2026-08-25 - the aura is a ClassTreeSkill and that enum outgrew uint8_t.
		// Little-endian, which is what keeps an OLDER build reading this file correct rather than
		// merely safe: it takes byte 0 only, and byte 0 is the low byte, so any aura below 256
		// survives the round trip intact. Every aura row in the tree is below 256.
		PutU16(out, player._pOracoolActiveAura);
		EndChunk(out, at);
	}

	{
		// The stable form of the same fact. Written ALONGSIDE tag 4 rather than instead of it, so a
		// build that predates this tag keeps loading the aura it understands.
		const size_t at = BeginChunk(out, HeroChunkActiveAuraRelative);
		const auto aura = static_cast<ClassTreeSkill>(player._pOracoolActiveAura);
		if (aura <= ClassTreeSkill::LAST
		    && GetClassTreeSkillData(aura).heroClass == player._pClass) {
			out.push_back(static_cast<uint8_t>(player._pClass));
			out.push_back(static_cast<uint8_t>(ClassTreeIconIndex(aura)));
		} else {
			// No aura, or one that does not belong to this character. 0xFF for both, which the
			// reader takes as "nothing burning" without having to know why.
			out.push_back(0xFF);
			out.push_back(0xFF);
		}
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
		const size_t at = BeginChunk(out, HeroChunkPassiveSlots);
		const auto count = static_cast<uint8_t>(std::size(player._pPassiveSlots));
		out.push_back(count);
		out.insert(out.end(), player._pPassiveSlots, player._pPassiveSlots + count);
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

	// The same three in two bytes (2026-09-18), because spell ids passed 254. The one-byte forms stay for older
	// builds; a reader that knows these tags takes them instead.
	{
		const size_t at = BeginChunk(out, HeroChunkReadiedSpells16);
		PutU16(out, PackReadiedSpell16(player._pRSpell, player._pRSplType));
		PutU16(out, PackReadiedSpell16(player._pLRSpell, player._pLRSplType));
		EndChunk(out, at);
	}
	{
		const size_t at = BeginChunk(out, HeroChunkSpellHotkeys16);
		out.push_back(static_cast<uint8_t>(AbilityFKeyCount));
		for (size_t i = 0; i < AbilityFKeyCount; i++)
			PutU16(out, PackReadiedSpell16(player._pSplHotKey[i], player._pSplTHotKey[i]));
		EndChunk(out, at);
	}
	{
		const size_t at = BeginChunk(out, HeroChunkSpellHotkeysLeft16);
		out.push_back(static_cast<uint8_t>(AbilityFKeyCount));
		for (size_t i = 0; i < AbilityFKeyCount; i++)
			PutU16(out, PackReadiedSpell16(player._pSplLHotKey[i], player._pSplLTHotKey[i]));
		EndChunk(out, at);
	}

	// The aura bindings, which cannot ride either array above: an aura row carries SpellID::Invalid,
	// so there is no spell to pack (user, 2026-08-31 - F-keys refused auras entirely until then).
	// Two bytes each, because the tree passed 255 rows in 2026-08-25.
	{
		const size_t at = BeginChunk(out, HeroChunkAuraHotkeys);
		out.push_back(static_cast<uint8_t>(AbilityFKeyCount));
		for (size_t i = 0; i < AbilityFKeyCount; i++)
			PutU16(out, player._pAuraHotKey[i]);
		EndChunk(out, at);
	}

	// D2MXL-to-ORCL Phase 2. Both ride the tail rather than growing PlayerPack, which is what the
	// tail was built for - a fixed struct's growth invalidates every existing hero file, and this
	// costs nothing.
	{
		const size_t at = BeginChunk(out, HeroChunkMilestones);
		PutU32(out, PackMilestones(player));
		EndChunk(out, at);
	}
	{
		const size_t at = BeginChunk(out, HeroChunkSignets);
		out.push_back(PackSignetsUsed(player));
		EndChunk(out, at);
	}
	{
		// Always written, not only when it exceeds 255: a chunk that appears at 256 and vanishes at
		// 254 would be a format that changes shape with the character's level, and the reader would
		// have to guess whether its absence meant "small" or "old file".
		const size_t at = BeginChunk(out, HeroChunkStatPoints);
		PutU32(out, static_cast<uint32_t>(std::max(0, player._pStatPts)));
		EndChunk(out, at);
	}

	return out;
}

void ReapplyHeroHotkeys(Player &player)
{
	ApplyPackedHotkeys(player, LastHotkeys.right, player._pSplHotKey, player._pSplTHotKey);
	ApplyPackedHotkeys(player, LastHotkeys.left, player._pSplLHotKey, player._pSplLTHotKey);
	ApplyPackedHotkeys16(player, LastHotkeys.right16, player._pSplHotKey, player._pSplTHotKey);
	ApplyPackedHotkeys16(player, LastHotkeys.left16, player._pSplLHotKey, player._pSplLTHotKey);
	// The readied pair too (round 34 audit): decoded before the items loaded, a Scroll or Staff choice found no scroll or
	// staff and fell back to the learned spell, and the one-byte decode after it re-derived it again. Decoded last, it
	// keeps its kind while the hero still carries the item.
	if (LastHotkeys.readied16) {
		UnpackReadiedSpell16(player, LastHotkeys.readiedRight16, player._pRSpell, player._pRSplType);
		UnpackReadiedSpell16(player, LastHotkeys.readiedLeft16, player._pLRSpell, player._pLRSplType);
	}
	LastHotkeys = {};
}

void ApplyHeroChunks(Player &player, const uint8_t *data, size_t len)
{
	LastHotkeys = {}; // a malformed tail applies nothing, and leaves nothing for ReapplyHeroHotkeys
	// FIRST, and before every early return below. Audit finding, 2026-08-26.
	//
	// The milestone mask and the signet count live in file-static arrays keyed by player SLOT, not
	// on the Player - and the character-select screen previews every save in turn through
	// Players[0]. This function only ever writes the chunks a save actually carries, so a hero with
	// no milestone chunk, a legacy hero with no tail, or a tail rejected for bad magic all left the
	// PREVIOUS hero's progression sitting in slot 0. Create a new character next and its very first
	// save serialised that stale state as its own.
	//
	// Every consequence points the wrong way: milestones already claimed are withheld, the signet
	// lifetime cap arrives partly spent, and growing charms read a progression the character never
	// had.
	//
	// Clearing here rather than at each call site is the point - the three early returns below are
	// exactly the paths a caller-side reset would have been forgotten on.
	ResetProgressionState(player);

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

	// The F-key bindings are settled after the walk too, and for a sharper reason than the aura's -
	// see the decode below.
	std::vector<uint8_t> packedRightHotkeys;
	std::vector<uint8_t> packedLeftHotkeys;
	std::vector<uint8_t> packedRightHotkeys16;
	std::vector<uint8_t> packedLeftHotkeys16;
	uint16_t readiedRight16 = 0;
	uint16_t readiedLeft16 = 0;
	bool sawReadied16 = false;

	// The aura is settled after the walk, not during it - see HeroChunkActiveAura below.
	uint16_t legacyAura = 0;
	bool sawLegacyAura = false;
	uint16_t currentAura = 0;
	bool sawCurrentAura = false;
	bool sawRelativeAura = false;

	offset = 4;
	while (offset < len) {
		const uint16_t tag = GetU16(data + offset);
		const uint32_t chunkLen = GetU32(data + offset + 2);
		const uint8_t *payload = data + offset + 6;
		switch (tag) {
		case HeroChunkSkillPoints:
			ApplySkillPoints(player, payload, chunkLen);
			break;
		case HeroChunkSkillPoints16:
			ApplySkillPoints16(player, payload, chunkLen);
			break;
		case HeroChunkWaypoints64:
			ApplyWaypoints64(player, payload, chunkLen);
			break;
		case HeroChunkActiveAura:
			// Two bytes since 2026-08-25; one byte before that. Both are read - a one-byte payload
			// is a hero saved by an older build, and its value is still meaningful, just not in
			// today's numbering. A longer payload than two belongs to a newer build again.
			//
			// NOT applied here any more (audit, 2026-08-26). This is an absolute ordinal and the
			// enum has been renumbered since it was written, so it cannot be interpreted until the
			// walk is over and we know whether tag 13 - which is renumbering-proof - is also
			// present. Held, and settled below.
			//
			// The WIDTH is the discriminator, and getting that wrong is an audit finding of its
			// own (2026-08-26). Tag 4 was one byte until v1.9.45 and two bytes from v1.9.45 on -
			// the enum passed 255 rows when the passives landed, so the same commit that renumbered
			// everything also widened this field. That makes the two cases entirely different
			// things wearing one tag:
			//
			//   one byte  -> written before the renumbering, so it is a LEGACY ordinal
			//   two bytes -> written after it, so it is already in today's numbering
			//
			// Treating both as legacy - which is what the first version of this did - broke every
			// save written between v1.9.45 and v1.9.57, the window where tag 4 was two bytes and
			// tag 13 did not yet exist. A Bard's Melody of Life is 195 there; the legacy table
			// stops at 163, so it was thrown away.
			if (chunkLen >= 2) {
				currentAura = GetU16(payload);
				sawCurrentAura = true;
			} else if (chunkLen == 1) {
				legacyAura = payload[0];
				sawLegacyAura = true;
			}
			break;
		case HeroChunkPaladinAuras:
			ApplyPaladinAuras(player, payload, chunkLen);
			break;
		case HeroChunkClassTree:
			ApplyClassTree(player, payload, chunkLen);
			break;
		case HeroChunkPassiveSlots:
			ApplyPassiveSlots(player, payload, chunkLen);
			break;
		case HeroChunkActiveAuraRelative:
			// WINS over tag 4 whenever it is present: it is the representation that cannot have
			// been reinterpreted by a later enum. Tag 4 is now held back rather than applied, so
			// this no longer depends on the two arriving in a particular order within the file.
			if (chunkLen >= 2) {
				ApplyActiveAuraRelative(player, payload[0], payload[1]);
				sawRelativeAura = true;
			}
			break;
		case HeroChunkMilestones:
			if (chunkLen >= 4)
				ApplyMilestones(player, GetU32(payload));
			break;
		case HeroChunkSignets:
			if (chunkLen >= 1)
				ApplySignetsUsed(player, payload[0]);
			break;
		case HeroChunkSpellLevels:
			if (chunkLen >= 1) {
				const size_t count = std::min<size_t>({ payload[0], chunkLen - 1, std::size(player._pSplLvl) });
				for (size_t i = 0; i < count; i++)
					player._pSplLvl[i] = std::min<uint8_t>(payload[1 + i], MaxSpellLevel);
			}
			break;
		case HeroChunkStatPoints:
			if (chunkLen >= 4) {
				// Overrides whatever the fixed u8 said, which for anything past 255 was a wrapped
				// value. Clamped to int's range on the way in so a corrupt file cannot hand the
				// character a negative pool - the audit's other half is that saved numbers are not
				// to be trusted just because we wrote them.
				const uint32_t points = GetU32(payload);
				player._pStatPts = static_cast<int>(std::min<uint32_t>(points, std::numeric_limits<int>::max()));
			}
			break;
		case HeroChunkAuraHotkeys:
			if (chunkLen >= 1) {
				// Two bytes per slot, so the payload bound is (chunkLen - 1) / 2.
				const size_t count = std::min<size_t>({ payload[0], (chunkLen - 1) / 2, AbilityFKeyCount });
				for (size_t i = 0; i < count; i++) {
					const uint16_t stored = GetU16(payload + 1 + i * 2);
					if (stored == 0xFFFF)
						continue; // an unbound slot, left exactly as it was
					// VALIDATED, not trusted: the ordinal is absolute, so a row belonging to another
					// class - or to no row at all - is dropped rather than honoured. Same guard
					// GetActiveClassAura applies to _pOracoolActiveAura, and for the same reason.
					if (stored > static_cast<uint16_t>(ClassTreeSkill::LAST))
						continue;
					const auto skill = static_cast<ClassTreeSkill>(stored);
					const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
					if (data.heroClass != player._pClass || data.kind != ClassTreeKind::Aura)
						continue;
					player._pAuraHotKey[i] = stored;
				}
			}
			break;
		// Both hotkey chunks are only COPIED here and decoded after the walk. See the note at the
		// decode for why, and what it cost.
		case HeroChunkSpellHotkeysLeft:
			packedLeftHotkeys.assign(payload, payload + chunkLen);
			break;
		case HeroChunkSpellHotkeys:
			packedRightHotkeys.assign(payload, payload + chunkLen);
			break;
		case HeroChunkSpellHotkeysLeft16:
			packedLeftHotkeys16.assign(payload, payload + chunkLen);
			break;
		case HeroChunkSpellHotkeys16:
			packedRightHotkeys16.assign(payload, payload + chunkLen);
			break;
		case HeroChunkReadiedSpells16:
			if (chunkLen >= 4) {
				readiedRight16 = GetU16(payload);
				readiedLeft16 = GetU16(payload + 2);
				sawReadied16 = true;
			}
			break;
		default:
			// An unknown tag is a chunk from a newer build - skipped, by design.
			break;
		}
		offset += size_t { 6 } + chunkLen; // widened for the same reason as the validation walk
	}

	// Tag 13 present means the save already carries the renumbering-proof form, and tag 4 is a
	// duplicate written for older builds to read - so it is ignored entirely.
	if (!sawRelativeAura) {
		if (sawLegacyAura) {
			// One-byte tag 4: pre-v1.9.45, so the number belongs to the old enum.
			ApplyLegacyAbsoluteAura(player, legacyAura);
		} else if (sawCurrentAura) {
			// Two-byte tag 4 with no tag 13: v1.9.45 to v1.9.57. Already in today's numbering, so
			// it is taken as it stands - but VALIDATED rather than trusted, because a saved number
			// is not to be believed just because we wrote it. An aura belonging to another class,
			// or to no row at all, is dropped exactly as the other two paths drop it.
			ApplyCurrentAbsoluteAura(player, currentAura);
		}
	}

	// THE F-KEY BINDINGS, decoded last (user, 2026-09-02: "hotkeys remembered now only on rmb. lmb
	// still forgets hotkeys").
	//
	// A binding is stored as a spell id and validated on the way back in: UnpackReadiedSpell asks
	// ReadiedSpellType, which asks _pAblSpells, and drops a binding on a spell the character does not
	// have - which is right, because the alternative is a key that fires nothing. But decoded inside
	// the walk, that mask is still the one UnPackPlayer computed, from BEFORE the tail arrived, and
	// the tail is what carries the tree investments. So every hotkey on a class-tree skill was
	// refused: the character did not know Zeal yet at the moment its Zeal binding was being read.
	//
	// It looked like a left-button bug because only the right button had a second source. LoadHotkeys
	// re-supplies _pSplHotKey from the game save without validating anything, so the right button's
	// bindings reappeared and the left button's - which have no such fallback - did not.
	//
	// RefreshInnateSpells first, then decode. Deliberately not "move the decode after the ClassTree
	// case", which would work only for as long as the tail's chunk ORDER holds; buffering makes the
	// two independent, and the tags may be written in any order by design.
	//
	// This is the same fault the readied pair had at pfile.cpp, fixed there the same way on
	// 2026-08-31. Two stores, one mask, one ordering mistake, found twice.
	LastHotkeys = { packedRightHotkeys, packedLeftHotkeys, packedRightHotkeys16, packedLeftHotkeys16, sawReadied16, readiedRight16, readiedLeft16 };
	if (!packedRightHotkeys.empty() || !packedLeftHotkeys.empty() || !packedRightHotkeys16.empty() || !packedLeftHotkeys16.empty() || sawReadied16) {
		RefreshInnateSpells(player);
		ApplyPackedHotkeys(player, packedRightHotkeys, player._pSplHotKey, player._pSplTHotKey);
		ApplyPackedHotkeys(player, packedLeftHotkeys, player._pSplLHotKey, player._pSplLTHotKey);
		// The two-byte forms after the one-byte ones, so they win where both were written.
		ApplyPackedHotkeys16(player, packedRightHotkeys16, player._pSplHotKey, player._pSplTHotKey);
		ApplyPackedHotkeys16(player, packedLeftHotkeys16, player._pSplLHotKey, player._pSplLTHotKey);
		if (sawReadied16) {
			UnpackReadiedSpell16(player, readiedRight16, player._pRSpell, player._pRSplType);
			UnpackReadiedSpell16(player, readiedLeft16, player._pLRSpell, player._pLRSplType);
		}
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

	// And the same shape of migration for the cap drop from 98 to 30 (2026-08-31). AFTER the book
	// refund, so a book spell's points are handed back whole by that one rather than trimmed to 30
	// by this one first.
	if (const int refunded = RefundInvestmentOverTheCap(player); refunded > 0 && &player == MyPlayer) {
		LogEvent(fmt::format("Skills cap at {:d} now - {:d} skill point{:s} returned",
		             MaxSkillInvestment, refunded, refunded == 1 ? "" : "s"),
		    UiFlags::ColorWhitegold);
	}
}

} // namespace devilution::oracool
