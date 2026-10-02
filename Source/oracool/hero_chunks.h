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
	 * Count-prefixed so MAX_SPELLS can grow without a new tag - up to 255 of them; past that, HeroChunkSkillPoints16.
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
	/**
	 * @brief The F1-F8 RIGHT-button ability hotkeys: u8 count, then count bytes, each a PackReadiedSpell byte
	 * for _pSplHotKey slot i. The TYPE is not stored - UnpackReadiedSpell re-derives it from the
	 * already-loaded spell masks, exactly as the readied-spell slots do. Count-prefixed so the key
	 * span can grow without a new tag. V1 loads characters through the hero pack, which never
	 * carried the vanilla hotkey array - without this chunk every binding died with the session.
	 */
	HeroChunkSpellHotkeys = 7,

	/**
	 * @brief The LEFT-button ability hotkeys - _pSplLHotKey, bound with LShift+F1-F8.
	 *
	 * Same shape as HeroChunkSpellHotkeys: u8 count, then one PackReadiedSpell byte per slot, type
	 * re-derived on load. A separate tag rather than a widening of chunk 7, so a hero written by a
	 * build that predates the left bindings still loads its right ones - the reader simply never
	 * sees this tag and leaves the left array empty, which is exactly right.
	 */
	HeroChunkSpellHotkeysLeft = 8,
	/**
	 * @brief D2MXL-to-ORCL Phase 2: the claimed-milestone mask, one u32.
	 *
	 * Bit N is Milestone N (oracool/signets.h). Masked to the milestones this build knows on read,
	 * so a save from a later version with more of them cannot silently mark an unknown one claimed.
	 */
	HeroChunkMilestones = 9,
	/**
	 * @brief D2MXL-to-ORCL Phase 2: signet points consumed, one u8, capped at SignetLifetimeCap.
	 *
	 * The lifetime count rather than a remaining count, so the cap can be re-tuned without every
	 * existing character's pool jumping or vanishing.
	 */
	HeroChunkSignets = 10,
	/**
	 * @brief Unspent stat points at full width, one u32. Overrides PlayerPack's u8 when present.
	 *
	 * `Player::_pStatPts` is an `int`; `PlayerPack::pStatPts` is a `uint8_t`, and the pack narrowed
	 * it without a word. Anything past 255 wrapped: 260 points came back as 4 (external audit,
	 * 2026-08-25).
	 *
	 * Not a theoretical range. A level-99 character has around 490 points to place, and Oracool's
	 * own Reset Stats button hands all of them back at once - so the feature that makes the number
	 * large is one this fork added, and pressing it then saving destroyed the character's entire
	 * progression.
	 *
	 * The writer still fills the fixed u8, clamped, so a chunkless reader sees a sane character
	 * rather than a wrapped one - the same contract HeroChunkWaypoints64 has with the u32 masks.
	 */
	HeroChunkStatPoints = 11,
	/**
	 * @brief The four Passive Skills slots: a count byte, then one CLASS-RELATIVE index each.
	 *
	 * 0xFF is an empty slot. Class-relative rather than the absolute ClassTreeSkill that
	 * HeroChunkActiveAura carries, and for a reason this project has now paid for once: an absolute
	 * id means something different the moment a class earlier in the enum gains rows, which is what
	 * cost Bard and Monk heroes their lit aura when the Passive Skills page landed. A relative index
	 * is stable under exactly that growth.
	 *
	 * Count-prefixed like HeroChunkClassTree so the slot count can grow without a new tag, and read
	 * back clamped to the smaller of the count and the array.
	 */
	HeroChunkPassiveSlots = 12,
	/**
	 * @brief The burning aura as (hero class, class-RELATIVE index) - the stable form.
	 *
	 * Supersedes HeroChunkActiveAura (tag 4), which stores the ABSOLUTE ClassTreeSkill ordinal. That
	 * ordinal shifts whenever a class earlier in the enum gains rows, and it already has: the Passive
	 * Skills page at 1.9.45 moved every Bard and Monk aura, and those characters lost whatever was
	 * burning. A guard in GetActiveClassAura turned that from "somebody else's aura" into "no aura",
	 * which is a seatbelt and not a fix.
	 *
	 * Two bytes: the HeroClass, then the index WITHIN that class. Both are stable under enum growth,
	 * because growth only ever appends to a class block.
	 *
	 * Tag 4 is still WRITTEN, so an older build keeps reading the aura it understands, and still READ
	 * when this tag is absent. When both are present this one wins - it is the one that cannot have
	 * been reinterpreted by a later enum.
	 */
	HeroChunkActiveAuraRelative = 13,
	/**
	 * @brief F1-F8 bound to AURAS - Player::_pAuraHotKey (user, 2026-08-31).
	 *
	 * u8 count, then that many little-endian u16 ClassTreeSkill ordinals, 0xFFFF for an unbound
	 * slot. Count-prefixed like HeroChunkClassTree so NumHotkeys can grow without a new tag, and
	 * two bytes per entry because the tree passed 255 rows in 2026-08-25 - the same widening
	 * HeroChunkActiveAura had to make.
	 *
	 * Absolute ordinals, like the aura chunks above, and guarded on read the same way: a row that
	 * does not belong to this character's class is dropped rather than trusted.
	 */
	HeroChunkAuraHotkeys = 14,
	/**
	 * @brief The readied pair in TWO bytes each (2026-09-18): u16 right, u16 left, PackReadiedSpell16 form - id + 1,
	 * 0 for none. The Necromancer's Summoning page took spell ids past 254, the last a byte can carry, so
	 * PlayerPack's two readied bytes hold 0 for such a spell and this chunk holds the truth. Applied AFTER the
	 * tail's investments (like the hotkeys), validated the same way, and it wins over the bytes when present.
	 */
	HeroChunkReadiedSpells16 = 15,
	/** @brief HeroChunkSpellHotkeys in two bytes a slot: u8 count, then u16 PackReadiedSpell16 entries. Wins over tag 7. */
	HeroChunkSpellHotkeys16 = 16,
	/** @brief HeroChunkSpellHotkeysLeft in two bytes a slot. Wins over tag 8. */
	HeroChunkSpellHotkeysLeft16 = 17,
	/**
	 * @brief HeroChunkSkillPoints with a TWO-byte count (2026-09-18): u16 unspent points, u16 count, then count bytes of
	 * per-spell invested levels. Tag 1's one-byte count wrapped to 3 the day MAX_SPELLS passed 255 (the Necromancer's
	 * Summoning page), and every hero lost its investments past the third spell. Tag 1 is still written with the first
	 * 255 for older builds; a reader that knows this tag takes it instead - it is written after tag 1 and wins.
	 */
	HeroChunkSkillPoints16 = 18,
	/**
	 * @brief Every book-learned spell level (2026-09-30): u8 count, then count bytes of _pSplLvl. PlayerPack carries
	 * ids 0-46 only, and the five runes (47-51) have books since v1.5.49 - a rune read from its book came back at level
	 * 0 in the next game, known and uncastable (round 7 audit, v1.12.232). Applied over the pack's values, each clamped
	 * to MaxSpellLevel.
	 */
	HeroChunkSpellLevels = 19,
	/**
	 * @brief The four base attributes at full width (user, 2026-10-01: the 255 cap lifted to MaxBaseAttribute): u16
	 * Strength, Magic, Dexterity, Vitality. Overrides the fixed struct's bytes, which the writer clamps to 255 so a chunkless
	 * reader still sees a sane hero. Each value is clamped to MaxBaseAttribute on the way in.
	 */
	HeroChunkBaseAttributes = 20,
	/**
	 * @brief A Guardian Keystone owed back (rounds 71-72 audit): u16 tier, 0 for none - Player::_pOracoolOwedKeystoneTier, the
	 * keystone spent on stepping into a Guardian Rift the game was left (or crashed) inside of before it was over. The next
	 * game's first level load refunds it (oracool::RefundOwedGuardianKeystone). Written only while one is owed.
	 */
	HeroChunkOwedKeystone = 21,
};

/** @brief Serializes every chunk the current player state wants persisted. */
std::vector<uint8_t> BuildHeroChunkTail(const Player &player);

/**
 * @brief Parses and applies a chunk tail onto an already-unpacked player. Unknown tags are
 * skipped; a malformed tail is rejected whole (the player keeps the fixed-struct state) and
 * logged. Safe to call with len == 0.
 */
void ApplyHeroChunks(Player &player, const uint8_t *data, size_t len);

/**
 * @brief Decodes the last ApplyHeroChunks' F-key bindings again, against the hero as he stands now (every page loaded).
 * Only adds bindings the first decode could not resolve; a no-op when no chunk tail carried hotkeys.
 */
void ReapplyHeroHotkeys(Player &player);

} // namespace devilution::oracool
