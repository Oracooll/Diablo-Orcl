#include "oracool/stat_sheet.h"

#include "items.h"
#include "player.h"
#include "spells.h"
#include "utils/math.h"

namespace devilution::oracool {

void ItemBonusTotals::AddItem(const Item &item)
{
	if (item.isEmpty() || !item._iStatFlag)
		return;

	minDamage += item._iMinDam;
	maxDamage += item._iMaxDam;
	armor += item._iAC;

	// A granted spell counts even unidentified - vanilla's rule, kept: you can swing a staff of
	// Firebolt you have not identified and the spell is there.
	if (IsValidSpell(item._iSpell))
		spells |= GetSpellBitmask(item._iSpell);

	if (item._iMagical != ITEM_QUALITY_NORMAL && !item._iIdentified)
		return; // bonuses hide until identified

	bonusDamage += item._iPLDam;
	bonusToHit += item._iPLToHit;
	if (item._iPLAC != 0) {
		// Percent of the item's OWN armor, sign-preserved when the integer division floors to 0.
		int itemBonusAc = item._iAC;
		itemBonusAc *= item._iPLAC;
		itemBonusAc /= 100;
		if (itemBonusAc == 0)
			itemBonusAc = math::Sign(item._iPLAC);
		bonusArmor += itemBonusAc;
	}
	flags |= item._iFlags;
	damAcFlags |= item._iDamAcFlags;
	strength += item._iPLStr;
	magic += item._iPLMag;
	dexterity += item._iPLDex;
	vitality += item._iPLVit;
	fireResist += item._iPLFR;
	lightningResist += item._iPLLR;
	magicResist += item._iPLMR;
	damageMod += item._iPLDamMod;
	getHit += item._iPLGetHit;
	lightRadius += item._iPLLight;
	hitPoints += item._iPLHP;
	mana += item._iPLMana;
	spellLevelAdd += item._iSplLvlAdd;
	enhancedAccuracy += item._iPLEnAc;
	fireMin += item._iFMinDam;
	fireMax += item._iFMaxDam;
	lightningMin += item._iLMinDam;
	lightningMax += item._iLMaxDam;
}

namespace {

/** @brief Source 1: the items worn on the body - the loop CalcPlrItemVals used to inline. */
void ApplyEquipment(const BonusContext &ctx, ItemBonusTotals &totals)
{
	for (const Item &item : ctx.owner->InvBody)
		totals.AddItem(item);
}

/**
 * @brief Source 2: the Barbarian's Rage - the first NON-item provider, and the proof that direct
 * contributions work. Active rage buffs the body; the cooldown debt subtracts the same amounts
 * (the resist half of the cooldown penalty stays in CalcPlrItemVals beside the Barbarian's innate
 * resist bonus, because the two interleave with class logic, not with sources).
 */
bool RageIsRelevant(const BonusContext &ctx)
{
	return HasAnyOf(ctx.owner->_pSpellFlags, SpellFlag::RageActive | SpellFlag::RageCooldown);
}

void ApplyRage(const BonusContext &ctx, ItemBonusTotals &totals)
{
	const Player &player = *ctx.owner;
	if (HasAnyOf(player._pSpellFlags, SpellFlag::RageActive)) {
		totals.strength += 2 * player._pLevel;
		totals.dexterity += player._pLevel + player._pLevel / 2;
		totals.vitality += 2 * player._pLevel;
	}
	if (HasAnyOf(player._pSpellFlags, SpellFlag::RageCooldown)) {
		totals.strength -= 2 * player._pLevel;
		totals.dexterity -= player._pLevel + player._pLevel / 2;
		totals.vitality -= 2 * player._pLevel;
	}
}

/**
 * @brief The provider table. Append here to add a bonus source; order is irrelevant because
 * every contribution commutes. Phase 1 adds sockets/charms rows; the set-bonus system adds a row
 * whose isActive counts worn pieces.
 */
constexpr BonusProvider Providers[] = {
	{ "equipment", nullptr, ApplyEquipment },
	{ "rage", RageIsRelevant, ApplyRage },
};

} // namespace

void AccumulateBonuses(const BonusContext &ctx, ItemBonusTotals &totals)
{
	for (const BonusProvider &provider : Providers) {
		if (provider.isActive != nullptr && !provider.isActive(ctx))
			continue;
		provider.apply(ctx, totals);
	}
}

} // namespace devilution::oracool
