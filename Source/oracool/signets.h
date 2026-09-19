/**
 * @file oracool/signets.h
 *
 * Oracool: D2MXL-to-ORCL Phase 2 - Signets of Learning, and the milestones that pay them.
 *
 * ## Progression that survives your gear
 *
 * A signet grants one PERMANENT stat point, and there is a lifetime cap on how many a character can
 * ever consume. The cap is the whole design: without it a signet is a slower level-up, and with it
 * the pool is a finite resource you can exhaust and then must live with.
 *
 * The point is spent through the ordinary unspent pool rather than into a named stat, so the player
 * chooses where it lands - a signet is a point, not a prescription.
 *
 * ## Milestones are how you reliably get one
 *
 * Median XL's level challenges: a thing you did once, that pays a specific known reward. Here each
 * milestone pays exactly one signet's worth of point, the first time it is met and never again.
 *
 * That ties the two halves into one economy rather than two systems that happen to ship together:
 * challenges are the RELIABLE source, and the drop (Phase 2b) is the bonus on top. It also gives
 * the levelling curve punctuation and gives the endgame bosses a reason to exist beyond their
 * treasure class.
 *
 * ## Where it lives, and what it cost
 *
 * NOTHING. Both values ride the hero chunk tail (oracool/hero_chunks.h), which is a tagged,
 * length-prefixed, forward-compatible list - a reader that does not know a tag skips it. So unlike
 * Phase 1, this phase breaks no save and bumps no version.
 *
 * The plan said PlayerPack, on the memory note that per-character persistent fields belong there.
 * The chunk tail is strictly better and postdates that note: PlayerPack is a fixed struct whose
 * growth invalidates every existing hero file, and the tail was built precisely so that never has
 * to happen again.
 */
#pragma once

#include <cstdint>

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief How many signets one character may ever consume.
 *
 * Twenty, which is roughly a fifth of what levelling to 99 hands out - enough to matter, far too
 * few to replace levelling. A cap that could be reached casually would make this an errand.
 */
constexpr int SignetLifetimeCap = 20;

/**
 * @brief The things a character can do once, each paying one signet's point.
 *
 * Deliberately a mix of DEPTH (levels), COMBAT (a boss) and SYSTEM (a runeword, a filled item, a
 * complete set) - a list that was all levels would just be the levelling curve again, and one that
 * was all systems would be unreachable for a player who has not met them yet.
 */
enum class Milestone : uint8_t {
	Level20,
	Level40,
	Level60,
	Level80,
	/** Killed an endgame boss - see oracool/endgame_boss.h. */
	SlayDreadBoss,
	/** Completed any runeword. */
	CompleteRuneword,
	/** Imbued an item to its limit (was the Mystic Orb cap until 2026-09-19; the saved bit is the same). */
	FillOrbCap,
	/** Wore enough of one named set to earn a bonus rung. */
	WearSetBonus,
	LAST = WearSetBonus,
};

constexpr int MilestoneCount = static_cast<int>(Milestone::LAST) + 1;

/** @brief The milestone's name, untranslated. */
const char *MilestoneName(Milestone milestone);

/** @brief Whether @p player has already claimed @p milestone. */
bool IsMilestoneClaimed(const Player &player, Milestone milestone);

/**
 * @brief Claims @p milestone for @p player, awarding a signet point. False if already claimed.
 *
 * Idempotent by design - every caller is a hook on an event that can happen a thousand times (a
 * level-up, a boss kill), and the claim is what makes the reward happen once.
 */
bool ClaimMilestone(Player &player, Milestone milestone);

/** @brief Checks every milestone whose condition can be read from @p player right now. */
void CheckPassiveMilestones(Player &player);

/** @brief How many signet points @p player has consumed, of SignetLifetimeCap. */
int SignetsUsed(const Player &player);

/** @brief Whether @p player has room for another signet point. */
bool CanConsumeSignet(const Player &player);

/**
 * @brief Grants one permanent stat point. False when the lifetime cap is reached.
 *
 * The point goes to the UNSPENT pool, so the player chooses where it lands.
 */
bool ConsumeSignet(Player &player);

/** @brief The chunk payloads, for oracool/hero_chunks.cpp. */
uint32_t PackMilestones(const Player &player);
void ApplyMilestones(Player &player, uint32_t mask);

/**
 * @brief Clears @p player's claimed milestones and spent signets.
 *
 * Must be called before applying a hero's extension tail, INCLUDING when there is no tail: these
 * tables are keyed by player slot and the character-select screen reuses slot 0 for every preview.
 */
void ResetProgressionState(const Player &player);
uint8_t PackSignetsUsed(const Player &player);
void ApplySignetsUsed(Player &player, uint8_t used);

} // namespace devilution::oracool
