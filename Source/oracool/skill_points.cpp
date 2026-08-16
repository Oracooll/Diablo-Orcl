#include "oracool/skill_points.h"

#include <algorithm>

#include <fmt/format.h>

#include "oracool/event_log.h"
#include "player.h"
#include "spells.h"

namespace devilution::oracool {

bool IsSkillInvestable(const Player &player, SpellID spell)
{
	if (spell == SpellID::Invalid || static_cast<size_t>(spell) >= MAX_SPELLS)
		return false;
	if (player._pSplLvl[static_cast<size_t>(spell)] > 0)
		return true;
	// Innate abilities only - knowledge that cannot walk away. Item-granted spells (_pISpells) are
	// deliberately excluded: unequipping the staff would strand the points in a skill the
	// character no longer has.
	return (player._pAblSpells & GetSpellBitmask(spell)) != 0;
}

bool CanInvestSkillPoint(const Player &player, SpellID spell)
{
	return player._pUnspentSkillPoints > 0
	    && IsSkillInvestable(player, spell)
	    && player._pSkillInvestment[static_cast<size_t>(spell)] < MaxSkillInvestment;
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

int TotalInvestedSkillPoints(const Player &player)
{
	int total = 0;
	for (const uint8_t invested : player._pSkillInvestment)
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
	player._pUnspentSkillPoints = static_cast<uint16_t>(
	    std::min<int>(player._pUnspentSkillPoints + refunded, UINT16_MAX));
}

} // namespace devilution::oracool
