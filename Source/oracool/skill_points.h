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

/** @brief Hard cap per skill, D2's own. _pSkillInvestment is a u8, so this also guards it. */
constexpr int MaxSkillInvestment = 20;

/**
 * @brief Whether @p spell is something @p player can sink points into: a spell they have actually
 * learned (book level > 0) or an ability/class skill they currently possess. Knowing is the gate -
 * points deepen a skill, they never teach one.
 */
bool IsSkillInvestable(const Player &player, SpellID spell);

/** @brief Whether an invest click would succeed: investable, a point unspent, cap not reached. */
bool CanInvestSkillPoint(const Player &player, SpellID spell);

/** @brief Spends one unspent point on @p spell. Returns false (and changes nothing) otherwise. */
bool InvestSkillPoint(Player &player, SpellID spell);

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
