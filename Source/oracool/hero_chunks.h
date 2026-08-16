/**
 * @file oracool/hero_chunks.h
 *
 * Oracool: Megaplan Phase 0.1 - the self-describing hero-file extension tail.
 *
 * The hero file was a single fixed struct (PlayerPack) guarded by an exact-size check, which made
 * every new piece of persistent character state a choice between hunting spare bytes and breaking
 * every save. This tail ends that: after the fixed struct, the file may carry
 *
 *     u32 magic "OEXT"
 *     repeated { u16 tag, u32 length, payload[length] }
 *
 * A reader SKIPS tags it does not know, so adding a chunk never breaks an older build, and a file
 * without the tail (every pre-1.6.27 hero) loads exactly as before - the tail is additive, not a
 * break. All values little-endian; a malformed tail (bad magic, truncated chunk) is dropped whole
 * rather than half-applied, and logged.
 *
 * The fixed struct remains authoritative for everything it already stores. A chunk may WIDEN a
 * fixed field (Waypoints64 overrides the u32 masks when present) but the writer keeps the fixed
 * field valid too, so a chunkless reader still sees a sane character.
 *
 * Adding a chunk: take the next tag number, document the payload here, write it in
 * BuildHeroChunkTail, apply it in ApplyHeroChunks. Nothing else to touch - that is the point.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief "OEXT" read as a little-endian u32. */
constexpr uint32_t HeroChunkMagic = 0x5458454FU;

enum HeroChunkTag : uint16_t {
	/**
	 * @brief Phase 2's skill points, persisted from day one so the break never has to happen:
	 * u16 unspent points, u8 count, then count bytes of per-spell invested levels (SpellID order).
	 * Count-prefixed so MAX_SPELLS can grow without a new tag.
	 */
	HeroChunkSkillPoints = 1,
	/**
	 * @brief The waypoint unlock masks at full width: u64[4], one per difficulty, bit (i-1) for
	 * waypoint slot i (slot 0, Tristram, is always unlocked and never stored). Overrides the fixed
	 * struct's u32 masks when present; the fixed masks still carry slots 1-32 for a chunkless
	 * reader. Widened once, generously, so zones stop re-widening it (16 -> 24 -> 32 -> this).
	 */
	HeroChunkWaypoints64 = 2,
	/**
	 * @brief RESERVED for Phase 6's companion (identity + packed equipment). Not written yet;
	 * the tag is claimed here so nothing else takes it.
	 */
	HeroChunkCompanion = 3,
	/**
	 * @brief The burning aura, one u8 (the oracool::PaladinTreeSkill byte, 0xFF = none). The reader
	 * takes the first byte and skips any remainder, so the payload can grow without a new tag.
	 */
	HeroChunkActiveAura = 4,
	/**
	 * @brief SUPERSEDED by HeroChunkClassTree. Held the Paladin's twenty aura investments in
	 * PaladinTreeSkill::FIRST_AURA order, before the tree generalized to four classes and the
	 * array was re-indexed by position-within-class. Still READ, and migrated onto the new slots
	 * (aura i sat at class position 9 + i), so a hero saved between the two builds keeps its
	 * points. Never written any more.
	 */
	HeroChunkPaladinAuras = 5,
	/**
	 * @brief Points invested in class-tree skills that have no spell slot: u8 count, then count
	 * bytes indexed by position-within-class. Count-prefixed so a class's skill list can grow
	 * without a new tag. The tree's castable skills are NOT here - their points ride in the
	 * SkillPoints chunk, keyed by SpellID, which is what lets GetSpellLevel see them.
	 */
	HeroChunkClassTree = 6,
};

/** @brief Serializes every chunk the current player state wants persisted. */
std::vector<uint8_t> BuildHeroChunkTail(const Player &player);

/**
 * @brief Parses and applies a chunk tail onto an already-unpacked player. Unknown tags are
 * skipped; a malformed tail is rejected whole (the player keeps the fixed-struct state) and
 * logged. Safe to call with len == 0.
 */
void ApplyHeroChunks(Player &player, const uint8_t *data, size_t len);

} // namespace devilution::oracool
