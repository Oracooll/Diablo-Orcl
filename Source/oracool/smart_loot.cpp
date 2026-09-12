#include "oracool/smart_loot.h"

#include "engine/random.hpp"
#include "items.h"
#include "oracool/oracool.h"
#include "player.h"
#include "playerdat.hpp"

namespace devilution::oracool {

namespace {

/** @brief Four drops in five are aimed. See SmartLootShouldAimThisDrop. */
constexpr int AimedDropPercent = 80;

/**
 * @brief The class's appetite for each main stat, in per-mille of its appetite for all three.
 *
 * Read out of PlayersData's maxStr/maxMag/maxDex, because that data already IS the statement of
 * what each class is about:
 *
 *     Paladin   250 /  50 /  60   -> 694 / 138 / 166
 *     Rogue      55 /  70 / 250   -> 146 / 186 / 666
 *     Sorcerer   45 / 250 /  85   -> 118 / 657 / 223
 *     Monk      150 /  80 / 150   -> 394 / 210 / 394   an honest Str/Dex hybrid
 *     Bard      120 / 120 / 120   -> 333 / 333 / 333   a generalist, so aiming changes nothing
 *     Barbarian 255 /   0 /  55   -> 822 /   0 / 177
 *
 * These are not caps in this fork - ModifyPlrStr clamps every class to 255 - only a statement of
 * identity, which stays true if the Barbarian's mana becomes Rage. Vitality is left out: every class
 * wants it, and so do Faster Cast and the rest (user, 2026-09-13: "Main stats mainly").
 */
struct Appetite {
	int strength;
	int magic;
	int dexterity;
};

Appetite AppetiteOf(const Player &player)
{
	const PlayerData &data = PlayersData[static_cast<size_t>(player._pClass)];
	const int total = std::max(data.maxStr + data.maxMag + data.maxDex, 1);
	return { 1000 * data.maxStr / total, 1000 * data.maxMag / total, 1000 * data.maxDex / total };
}

} // namespace

bool SmartLootShouldAimThisDrop()
{
	return GenerateRnd(100) < AimedDropPercent;
}

bool SmartLootIsEquipmentBase(_item_indexes idx)
{
	if (idx < 0 || idx > IDI_LAST)
		return false;
	const ItemData &data = AllItemsList[static_cast<size_t>(idx)];
	if (data.iClass == ICLASS_ARMOR || data.iClass == ICLASS_WEAPON)
		return true;
	return data.itype == ItemType::Ring || data.itype == ItemType::Amulet;
}

int SmartLootScoreForBase(_item_indexes idx, const Player &player)
{
	if (!SmartLootIsEquipmentBase(idx))
		return 0;
	const ItemData &data = AllItemsList[static_cast<size_t>(idx)];
	const int asked = data.iMinStr + data.iMinMag + data.iMinDex;
	if (asked == 0)
		return SmartLootNeutralScore;

	// A SHARE, not an amount. The first version scored the weighted SUM of the requirements, and
	// requirements grow with the base - a Great Helm asks for more Strength than a Cap - so aiming
	// always preferred the deepest base in range and quietly pushed drops up the item ladder. As a
	// weighted average it asks only WHICH stats a base wants, never how much: a Cap and a Great Helm
	// that both want Strength alone score the same.
	const Appetite appetite = AppetiteOf(player);
	return (appetite.strength * data.iMinStr + appetite.magic * data.iMinMag + appetite.dexterity * data.iMinDex) / asked;
}

_item_indexes SmartLootAimBase(_item_indexes first, const Player &player, tl::function_ref<_item_indexes(item_equip_type)> drawCandidate)
{
	// Checked before any randomness is consumed, so gold, potions, scrolls and multiplayer drops are
	// not only left alone but leave the RNG stream exactly as it would have been. The chest path
	// rolls gold three times in four; the first version aimed those too, and every equipment
	// candidate outscored gold's zero - chests stopped dropping gold and potions.
	if (!IsSinglePlayer() || !SmartLootIsEquipmentBase(first))
		return first;
	if (!SmartLootShouldAimThisDrop())
		return first;

	// THE SLOT STANDS. Candidates are drawn for the slot the blind roll chose, and anything else a
	// draw returns is discarded, so aiming changes which helm drops and never whether a helm does.
	const item_equip_type slot = AllItemsList[static_cast<size_t>(first)].iLoc;
	_item_indexes best = first;
	int bestScore = SmartLootScoreForBase(first, player);
	for (int attempt = 1; attempt < SmartLootCandidates; attempt++) {
		const _item_indexes candidate = drawCandidate(slot);
		if (!SmartLootIsEquipmentBase(candidate) || AllItemsList[static_cast<size_t>(candidate)].iLoc != slot)
			continue;
		const int score = SmartLootScoreForBase(candidate, player);
		if (score <= bestScore)
			continue;
		best = candidate;
		bestScore = score;
	}
	return best;
}

} // namespace devilution::oracool
