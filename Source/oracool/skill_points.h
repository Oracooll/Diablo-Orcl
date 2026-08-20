/**
 * @file oracool/skill_points.h
 *
 * Oracool: Megaplan Phase 2.1 - skill points on level-up, feeding the existing ladders.
 *
 * The persistence has existed since 1.6.27 (the SkillPoints hero chunk round-trips
 * _pUnspentSkillPoints and _pSkillInvestment through every save); this module is the gameplay that
 * fills those fields. One point per character level, spent one at a time on any skill the
 * character actually knows, and consumed by the ladders through ONE seam: Player::GetSpellLevel
 * adds the investment, so every damage formula, duration and missile count that already scales
 * with spell level scales with investment for free. Zeal's strike ladder - previously pure
 * character level - reads its investment directly (see ZealStrikeCount).
 *
 * Investment keys on SpellID because everything here does: book spells, innate abilities and the
 * Paladin skills all carry one, and it is the index _pSkillInvestment was declared over.
 */
#pragma once

#include "spelldat.h"

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief Points granted per character level - D2's own rate. */
constexpr int SkillPointsPerLevel = 1;

/**
 * @brief Hard cap per skill: the WHOLE pool a character can ever earn (user, 2026-08-19: "a player
 * can invest all 98 possible points they can acquire in one skill/spell").
 *
 * 98 is one point per level from 2 to 99, so this says "everything you have" rather than a number of
 * its own - and _pSkillInvestment is a u8, which it still fits. D2s flat 20 is gone; what actually
 * paces depth now is the Rule of Rangs (oracool/spell_ranks.h), where each rank past the first costs
 * a character level.
 */
constexpr int MaxSkillInvestment = 98;

/**
 * @brief Whether @p spell is a BOOK SPELL - something a Book of it could teach.
 *
 * The line between a spell and a skill, and it was already in the data before this rule existed:
 * spelldat's sBookLvl is -1 for exactly those SpellIDs that no book can teach. The Paladin's seven
 * were authored that way on purpose ("earned at the tree, not bought" - spelldat.cpp:75), which is
 * why this predicate lands where it should without a new table to keep in step.
 *
 * User rule, 2026-08-20: "Spells cant be affected by skill points, only by books. Vanila D1."
 * A book spell's level therefore comes from _pSplLvl and items alone; skill points cannot touch it.
 * A bookless skill is a class skill and takes points as before.
 */
bool SpellHasBook(SpellID spell);

/**
 * @brief Whether @p spell is something @p player can sink points into.
 *
 * Two gates. Knowledge: an ability or class skill they currently possess - points deepen a skill,
 * they never teach one. And SpellHasBook: a spell you learned from a book is raised by more books,
 * never by points.
 */
bool IsSkillInvestable(const Player &player, SpellID spell);

/**
 * @brief One-time repair: returns every point sunk into a BOOK SPELL to the unspent pool.
 *
 * Points could reach book spells two ways before the rule above - the Abilities window's Spells
 * sheet spent directly on them, and the Sorceress's tree rows were castable spells whose investment
 * was stored by SpellID. Both are closed now, so any surviving value is stranded in a place the
 * player can no longer reach, and stranding a character's whole pool is worse than the rule it came
 * from.
 *
 * Idempotent by construction: it zeroes what it refunds, so a second run finds nothing.
 * Returns the number of points handed back.
 */
int RefundBookSpellInvestment(Player &player);

/** @brief Whether an invest click would succeed: investable, a point unspent, cap not reached. */
bool CanInvestSkillPoint(const Player &player, SpellID spell);

/** @brief Spends one unspent point on @p spell. Returns false (and changes nothing) otherwise. */
bool InvestSkillPoint(Player &player, SpellID spell);

/**
 * @brief Whether a refund click would succeed: at least one point sunk in @p spell.
 *
 * The tree has had a per-rank minus since 2026-08-17; the spell list had only the paid respec, so a
 * point put in by accident cost gold to take back out (user, 2026-08-19: "we need to introduce a
 * minus button"). Free, one rank at a time, matching the tree exactly - the two sheets spend from
 * one pool and should not disagree about how it is unspent.
 */
bool CanRefundSkillPoint(const Player &player, SpellID spell);

/** @brief Takes one point back out of @p spell and returns it to the unspent pool. */
bool RefundSkillPoint(Player &player, SpellID spell);

/** @brief All points sunk so far, across every skill. */
int TotalInvestedSkillPoints(const Player &player);

/** @brief The level-up grant - called from NextPlrLevel. */
void GrantLevelUpSkillPoint(Player &player);

/**
 * @brief One-time top-up for characters who levelled before skill points existed: brings
 * unspent + invested up to (level - 1) * SkillPointsPerLevel. A no-op on anyone already granted,
 * so it is safe to run on every load (same slot as MigrateHiddenBeltSlots).
 */
void EnsureRetroactiveSkillPoints(Player &player);

/** @brief Phase 2.3's respec price: gold, scaling with how many points are sunk. */
int RespecCost(const Player &player);

/** @brief Returns every invested point to the unspent pool. The respec, minus the gold. */
void RefundAllSkillPoints(Player &player);

} // namespace devilution::oracool
