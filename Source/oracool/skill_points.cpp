#include "oracool/skill_points.h"

#include <algorithm>

#include <fmt/format.h>

#include "oracool/class_tree.h"
#include "oracool/event_log.h"
#include "oracool/skill_sounds.h"
#include "oracool/spell_ranks.h"
#include "player.h"
#include "spells.h"

namespace devilution::oracool {

// The three caps are ONE number wearing three names, and they live in three headers that cannot
// include each other. Asserted here, the one place that already sees all of them, because they only
// mean anything together: MaxSpellLevel bounds what books teach, MaxSkillInvestment what points buy,
// and MaxTreeInvestment what a tree row accepts. Let them drift and a skill's depth would depend on
// which door it came through (user, 2026-08-31).
static_assert(MaxSkillInvestment == MaxTreeInvestment,
    "a tree row must accept exactly as many points as the skill-point cap allows");
static_assert(MaxSkillInvestment == static_cast<int>(MaxSpellLevel),
    "books and points share one ceiling - see MaxSpellLevel's comment in player.h");

bool SpellHasBook(SpellID spell)
{
	if (spell == SpellID::Invalid || static_cast<size_t>(spell) >= MAX_SPELLS)
		return false;
	return GetSpellData(spell).sBookLvl >= 0;
}

int RefundInvestmentOverTheCap(Player &player)
{
	// The cap dropped from 98 to 30 on 2026-08-31, so a hero saved before that can be holding more
	// points in a skill than the skill now accepts. Left alone they would be stranded: nothing
	// spends them, nothing refunds them, and the skill would sit permanently above a ceiling every
	// other character obeys.
	//
	// Idempotent, and self-gating for the same reason RefundBookSpellInvestment is - once the excess
	// is handed back there is nothing over the cap to find, so no version stamp is needed.
	int refunded = 0;
	for (size_t i = 0; i < MAX_SPELLS; i++) {
		const int invested = player._pSkillInvestment[i];
		if (invested <= MaxSkillInvestment)
			continue;
		refunded += invested - MaxSkillInvestment;
		player._pSkillInvestment[i] = static_cast<uint8_t>(MaxSkillInvestment);
	}
	if (refunded > 0) {
		player._pUnspentSkillPoints = static_cast<uint16_t>(
		    std::min<int>(player._pUnspentSkillPoints + refunded, UINT16_MAX));
	}
	return refunded;
}

int RefundBookSpellInvestment(Player &player)
{
	int refunded = 0;
	for (size_t i = 0; i < MAX_SPELLS; i++) {
		if (!SpellHasBook(static_cast<SpellID>(i)))
			continue;
		refunded += player._pSkillInvestment[i];
		player._pSkillInvestment[i] = 0;
	}
	if (refunded > 0) {
		player._pUnspentSkillPoints = static_cast<uint16_t>(
		    std::min<int>(player._pUnspentSkillPoints + refunded, UINT16_MAX));
	}
	return refunded;
}

bool IsSkillInvestable(const Player &player, SpellID spell)
{
	if (spell == SpellID::Invalid || static_cast<size_t>(spell) >= MAX_SPELLS)
		return false;
	// The rule, at the one seam every invest path already funnels through: a book spell is raised by
	// books, never by points. Tested BEFORE the knowledge gate below, because a learned book spell
	// would otherwise pass on the very line that now has to reject it.
	if (SpellHasBook(spell))
		return false;
	// Innate abilities only - knowledge that cannot walk away. Item-granted spells (_pISpells) are
	// deliberately excluded: unequipping the staff would strand the points in a skill the
	// character no longer has.
	return (player._pAblSpells & GetSpellBitmask(spell)) != 0;
}

bool CanInvestSkillPoint(const Player &player, SpellID spell)
{
	if (player._pUnspentSkillPoints <= 0 || !IsSkillInvestable(player, spell))
		return false;
	const int invested = player._pSkillInvestment[static_cast<size_t>(spell)];
	if (invested >= MaxSkillInvestment)
		return false;
	// The Rule of Rangs (user, 2026-08-19): each rank costs one more character level than the one
	// before it. The rank being bought is invested + 1, counting from the spell's own band - so a
	// level-6 spell reaches rank 10 at character level 15. This is what replaced the flat cap of 20:
	// nothing is out of reach for good, but depth is paid for in levels.
	return player._pLevel >= SpellRankRequiredLevel(spell, invested + 1);
}

bool InvestSkillPoint(Player &player, SpellID spell)
{
	if (!CanInvestSkillPoint(player, spell))
		return false;
	player._pUnspentSkillPoints--;
	player._pSkillInvestment[static_cast<size_t>(spell)]++;
	if (&player == MyPlayer) {
		LogEvent(fmt::format("Invested a point in {:s} (level {:d})",
		             std::string(GetSpellData(spell).sNameText),
		             int(player._pSkillInvestment[static_cast<size_t>(spell)])),
		    UiFlags::ColorWhitegold);
	}
	return true;
}

bool CanRefundSkillPoint(const Player &player, SpellID spell)
{
	if (spell == SpellID::Invalid || static_cast<size_t>(spell) >= MAX_SPELLS)
		return false;
	// Deliberately NOT gated on IsSkillInvestable. A spell can stop being investable - a scroll-only
	// spell forgotten, an ability lost with the equipment that granted it - and the points already
	// sunk into it must still be recoverable, or they are stranded.
	return player._pSkillInvestment[static_cast<size_t>(spell)] > 0;
}

bool RefundSkillPoint(Player &player, SpellID spell)
{
	if (!CanRefundSkillPoint(player, spell))
		return false;
	player._pSkillInvestment[static_cast<size_t>(spell)]--;
	player._pUnspentSkillPoints = static_cast<uint16_t>(
	    std::min<int>(player._pUnspentSkillPoints + 1, UINT16_MAX));
	if (&player == MyPlayer) {
		LogEvent(fmt::format("{:s} lowered to {:d}",
		             std::string(GetSpellData(spell).sNameText),
		             int(player._pSkillInvestment[static_cast<size_t>(spell)])),
		    UiFlags::ColorWhitegold);
	}
	return true;
}

int TotalInvestedSkillPoints(const Player &player)
{
	// BOTH stores. Class-tree skills with a SpellID keep their points in _pSkillInvestment (where
	// GetSpellLevel finds them); the slotless passives, masteries and auras keep theirs in
	// _pClassTreeInvestment. Counting only the first store (external audit, 2026-08-17) made every
	// point spent on a slotless skill invisible to this total - so EnsureRetroactiveSkillPoints
	// re-granted those points on every game start (an infinite point loop: invest in passives,
	// relog, repeat), and the respec both undercharged and refunded only half the character.
	int total = 0;
	for (const uint8_t invested : player._pSkillInvestment)
		total += invested;
	for (const uint8_t invested : player._pClassTreeInvestment)
		total += invested;
	return total;
}

void GrantLevelUpSkillPoint(Player &player)
{
	player._pUnspentSkillPoints = static_cast<uint16_t>(
	    std::min<int>(player._pUnspentSkillPoints + SkillPointsPerLevel, UINT16_MAX));
	if (&player == MyPlayer)
		LogEvent("Skill point earned - spend it on the Abilities sheets", UiFlags::ColorWhitegold);
}

void EnsureRetroactiveSkillPoints(Player &player)
{
	const int owed = (player._pLevel - 1) * SkillPointsPerLevel;
	const int have = player._pUnspentSkillPoints + TotalInvestedSkillPoints(player);
	if (have >= owed)
		return;
	player._pUnspentSkillPoints = static_cast<uint16_t>(
	    std::min<int>(player._pUnspentSkillPoints + (owed - have), UINT16_MAX));
	if (&player == MyPlayer) {
		LogEvent(fmt::format("{:d} skill point(s) granted for levels already earned", owed - have),
		    UiFlags::ColorWhitegold);
	}
}

int RespecCost(const Player &player)
{
	// A price that stays affordable early and stings late: 500 gold per sunk point, floor 1000.
	return std::max(1000, TotalInvestedSkillPoints(player) * 500);
}

void RefundAllSkillPoints(Player &player)
{
	const int refunded = TotalInvestedSkillPoints(player);
	if (refunded == 0)
		return;
	for (uint8_t &invested : player._pSkillInvestment)
		invested = 0;
	for (uint8_t &invested : player._pClassTreeInvestment)
		invested = 0;
	player._pUnspentSkillPoints = static_cast<uint16_t>(
	    std::min<int>(player._pUnspentSkillPoints + refunded, UINT16_MAX));

	// Every aura is at rank 0 after a full refund, and an aura the rules would refuse to light must
	// not stay burning - the same teardown RefundClassTreePoint applies one rank at a time.
	if (GetActiveClassAura(player) != ClassTreeSkill::None) {
		player._pOracoolActiveAura = static_cast<uint16_t>(ClassTreeSkill::None);
		if (&player == MyPlayer)
			StopClassAuraLoop();
	}
}

} // namespace devilution::oracool
