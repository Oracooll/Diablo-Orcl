#include "oracool/stat_sheet.h"

#include <string>

#include <fmt/format.h>

#include "items.h"
#include "oracool/charms.h"
#include "oracool/gems.h"
#include "oracool/class_tree.h"
#include "oracool/warcries.h"
#include "oracool/item_sets.h"
#include "oracool/runewords.h"
#include "player.h"
#include "spells.h"
#include "utils/language.h"
#include "utils/math.h"
#include "utils/str_cat.hpp"

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
	// The item half of gold find. The charm half already came through ApplyCharmToTotals; this is
	// what lets a worn item, a set rung or a unique contribute the same way.
	goldFind += item._iPLGoldFind;
	magicFind += item._iPLMagicFind;
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
 * @brief Source 3 (Phase 1): gems sitting in the sockets of WORN equipment. Host-dependent - the
 * same Ruby is fire damage in a sword and fire resist in a helm - which is why the walk is per
 * worn item, not per gem.
 */
void ApplySockets(const BonusContext &ctx, ItemBonusTotals &totals)
{
	for (const Item &item : ctx.owner->InvBody) {
		if (item.isEmpty() || !item._iStatFlag || item._iSocketCount == 0)
			continue;
		const SocketHost host = SocketHostForItemType(item._itype);
		for (const uint16_t gemIdx : item._iSocketed) {
			if (gemIdx != Item::EmptySocket)
				ApplyGemToTotals(gemIdx, host, totals);
		}
		// A completed runeword's own bonuses ride ON TOP of the individual runes' effects. The
		// state is derived right here from the sockets, never stored - it cannot desync.
		if (const RunewordDefinition *word = GetActiveRuneword(item); word != nullptr)
			ApplyRunewordToTotals(*word, totals);
	}
}

/**
 * @brief The provider table. Append here to add a bonus source; order is irrelevant because
 * every contribution commutes. Charms join in a later Phase 1 unit; the set-bonus system adds a
 * row whose isActive counts worn pieces.
 */
/** @brief Source 4 (Phase 1): the first CharmActiveCap charms in the backpack. */
void ApplyCharms(const BonusContext &ctx, ItemBonusTotals &totals)
{
	// The player is threaded through the context alongside the totals, because a growing charm's
	// value depends on how many milestones that character has claimed - see oracool/charms.h.
	struct CharmContext {
		const Player *owner;
		ItemBonusTotals *totals;
	} charmContext { ctx.owner, &totals };
	ForEachActiveCharm(*ctx.owner, [](uint16_t charmIdx, void *context) {
		auto *cc = static_cast<CharmContext *>(context);
		ApplyCharmToTotals(*cc->owner, charmIdx, *cc->totals);
	},
	    &charmContext);
}

/**
 * @brief Source 5: the class skill tree - the Paladin's burning aura plus every class's
 * paid-for passives. ApplyClassTreeToTotals re-checks the class, the tier and the invested points
 * itself, so state that outlives the rules it was bought under contributes nothing.
 */
bool AuraIsRelevant(const BonusContext &ctx)
{
	return ClassHasTree(ctx.owner->_pClass);
}

void ApplyAura(const BonusContext &ctx, ItemBonusTotals &totals)
{
	ApplyClassTreeToTotals(*ctx.owner, totals);
	// ...and the timed cries the character is carrying (Round 6) - Shout, Battle Orders and the rest.
	ApplyWarcryBuffsToTotals(*ctx.owner, totals);
}

/**
 * @brief Source 6: the fifteen item sets' tiered bonuses - see oracool/item_sets.h.
 *
 * The condition hook this header describes as existing for "three pieces worn?" finally has its
 * caller. AnySetBonusActive re-derives the worn count from the equipment itself, so a bonus cannot
 * outlive the pieces that earned it.
 */
bool SetBonusIsRelevant(const BonusContext &ctx)
{
	return AnySetBonusActive(*ctx.owner);
}

void ApplySetBonuses(const BonusContext &ctx, ItemBonusTotals &totals)
{
	ApplySetBonusesToTotals(*ctx.owner, totals);
}

constexpr BonusProvider Providers[] = {
	{ "equipment", nullptr, ApplyEquipment },
	{ "rage", RageIsRelevant, ApplyRage },
	{ "sockets", nullptr, ApplySockets },
	{ "charms", nullptr, ApplyCharms },
	{ "class tree", AuraIsRelevant, ApplyAura },
	{ "item sets", SetBonusIsRelevant, ApplySetBonuses },
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

std::string DescribeBonusTotals(const ItemBonusTotals &totals, const char *separator)
{
	std::string out;
	const auto add = [&out, separator](std::string piece) {
		if (!out.empty())
			out.append(separator);
		out.append(std::move(piece));
	};
	// A signed value prints its own sign, so a penalty reads as one. Several of these fields are
	// reduced by real effects - getHit especially, where NEGATIVE is the good direction.
	const auto signedNumber = [](int v) { return v > 0 ? StrCat("+", v) : StrCat(v); };

	// Order is the order a player cares about, not the struct's: what it does to your attacks, then
	// to your defence, then to your body, then the odds and ends.
	if (totals.minDamage != 0 || totals.maxDamage != 0)
		add(fmt::format(fmt::runtime(_("{:s}-{:d} damage")), signedNumber(totals.minDamage), totals.maxDamage));
	if (totals.bonusDamage != 0)
		add(fmt::format(fmt::runtime(_("{:s}% damage")), signedNumber(totals.bonusDamage)));
	if (totals.damageMod != 0)
		add(fmt::format(fmt::runtime(_("{:s} damage")), signedNumber(totals.damageMod)));
	if (totals.bonusToHit != 0)
		add(fmt::format(fmt::runtime(_("{:s}% to hit")), signedNumber(totals.bonusToHit)));
	if (totals.enhancedAccuracy != 0)
		add(fmt::format(fmt::runtime(_("{:s} armour pierce")), signedNumber(totals.enhancedAccuracy)));
	if (totals.fireMin != 0 || totals.fireMax != 0)
		add(fmt::format(fmt::runtime(_("+{:d}-{:d} fire damage")), totals.fireMin, totals.fireMax));
	if (totals.lightningMin != 0 || totals.lightningMax != 0)
		add(fmt::format(fmt::runtime(_("+{:d}-{:d} lightning damage")), totals.lightningMin, totals.lightningMax));

	if (totals.armor != 0 || totals.bonusArmor != 0)
		add(fmt::format(fmt::runtime(_("{:s} armour")), signedNumber(totals.armor + totals.bonusArmor)));
	if (totals.getHit != 0)
		add(fmt::format(fmt::runtime(_("{:s} damage taken")), signedNumber(totals.getHit)));
	if (totals.fireResist != 0)
		add(fmt::format(fmt::runtime(_("{:s}% fire resist")), signedNumber(totals.fireResist)));
	if (totals.lightningResist != 0)
		add(fmt::format(fmt::runtime(_("{:s}% lightning resist")), signedNumber(totals.lightningResist)));
	if (totals.magicResist != 0)
		add(fmt::format(fmt::runtime(_("{:s}% magic resist")), signedNumber(totals.magicResist)));

	if (totals.strength != 0)
		add(fmt::format(fmt::runtime(_("{:s} strength")), signedNumber(totals.strength)));
	if (totals.magic != 0)
		add(fmt::format(fmt::runtime(_("{:s} magic")), signedNumber(totals.magic)));
	if (totals.dexterity != 0)
		add(fmt::format(fmt::runtime(_("{:s} dexterity")), signedNumber(totals.dexterity)));
	if (totals.vitality != 0)
		add(fmt::format(fmt::runtime(_("{:s} vitality")), signedNumber(totals.vitality)));
	if (totals.hitPoints != 0)
		add(fmt::format(fmt::runtime(_("{:s} life")), signedNumber(totals.hitPoints)));
	if (totals.mana != 0)
		add(fmt::format(fmt::runtime(_("{:s} mana")), signedNumber(totals.mana)));

	if (totals.spellLevelAdd != 0)
		add(fmt::format(fmt::runtime(_("{:s} to spell levels")), signedNumber(totals.spellLevelAdd)));
	if (totals.lightRadius != 0)
		add(fmt::format(fmt::runtime(_("{:s} light radius")), signedNumber(totals.lightRadius)));
	if (totals.magicFind != 0)
		add(fmt::format(fmt::runtime(_("{:s}% magic find")), signedNumber(totals.magicFind)));
	if (totals.goldFind != 0)
		add(fmt::format(fmt::runtime(_("{:s}% gold find")), signedNumber(totals.goldFind)));

	return out;
}

} // namespace devilution::oracool
