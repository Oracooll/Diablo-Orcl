#include "oracool/smart_loot.h"

#include <algorithm>
#include <array>

#include "engine/random.hpp"
#include "items.h"
#include "player.h"
#include "playerdat.hpp"

namespace devilution::oracool {

namespace {

/** @brief Four drops in five are aimed. See SmartLootShouldAimThisDrop. */
constexpr int AimedDropPercent = 80;

/**
 * @brief The class's appetite for one stat, as a percentage of its appetite for all of them.
 *
 * Read out of PlayersData's maxStr/maxMag/maxDex rather than a hand-written class-to-stat table,
 * because that data already IS the statement of what each class is about, and it is maintained:
 *
 *     Paladin   250 /  50 /  60   -> Strength
 *     Rogue      55 /  70 / 250   -> Dexterity
 *     Sorcerer   45 / 250 /  85   -> Magic
 *     Monk      150 /  80 / 150   -> an honest Strength/Dexterity hybrid
 *     Bard      120 / 120 / 120   -> a generalist, and weighted like one
 *     Barbarian 255 /   0 /  55   -> Strength, with no interest in Magic at all
 *
 * Monk and Bard are the reason this is a WEIGHT and not a single "primary stat" per class: a table
 * of primaries would have to pick one for each of them and would be lying twice.
 *
 * These are no longer caps in this fork - ModifyPlrStr clamps to a flat 255 for every class, so any
 * class can reach any stat (user, 2026-09-13: "in Orcl mod we don't have stats limits"). They are
 * used here purely as a statement of class identity, which is what they still are. That also keeps
 * this correct through a change like the Barbarian's mana becoming Rage: magic gear is not a
 * Barbarian's axis either way, and maxMag 0 goes on saying so.
 *
 * Vitality is deliberately excluded: every class wants it, so including it would add the same
 * number to every candidate's score and change no comparison.
 */
struct ClassStatAppetite {
	int strength;
	int magic;
	int dexterity;
};

ClassStatAppetite AppetiteOf(const Player &player)
{
	const PlayerData &data = PlayersData[static_cast<size_t>(player._pClass)];
	return { data.maxStr, data.maxMag, data.maxDex };
}

} // namespace

bool SmartLootShouldAimThisDrop()
{
	return GenerateRnd(100) < AimedDropPercent;
}

bool SmartLootConsiders(const Item &item)
{
	if (item.isEmpty())
		return false;
	if (item._iClass == ICLASS_ARMOR || item._iClass == ICLASS_WEAPON)
		return true;
	// Rings and amulets are ICLASS_MISC but are worn, and carry stat affixes like anything else.
	return item._itype == ItemType::Ring || item._itype == ItemType::Amulet;
}

int SmartLootScore(const Item &item, const Player &player)
{
	if (!SmartLootConsiders(item))
		return 0;

	const ClassStatAppetite appetite = AppetiteOf(player);

	// WHAT THE ITEM ASKS FOR is the clearest statement of who it is built for, and it needs no
	// class-to-item-type table: a staff asks for Magic, a war bow asks for Dexterity, a great axe
	// asks for Strength. The base item data already encodes the whole relationship.
	//
	// Requirements are weighted more heavily than bonuses because they describe the item's KIND,
	// which is the thing a player notices first about a drop, where a stat bonus is one line on it.
	constexpr int RequirementWeight = 2;
	int score = RequirementWeight
	    * (appetite.strength * item._iMinStr
	        + appetite.magic * item._iMinMag
	        + appetite.dexterity * item._iMinDex);

	// WHAT THE ITEM GIVES. The rolled stat affixes, weighted the same way, so a +Magic ring scores
	// for a Sorcerer and a +Strength one does not.
	score += appetite.strength * std::max<int>(item._iPLStr, 0)
	    + appetite.magic * std::max<int>(item._iPLMag, 0)
	    + appetite.dexterity * std::max<int>(item._iPLDex, 0);

	return std::max<int>(score, 0);
}


/**
 * @brief The same judgement applied to a BASE item index, before anything is rolled onto it.
 *
 * The chest and barrel path chooses its index first and generates once, so there is no finished
 * item to score there; this scores what the base itself asks for. Only the requirement half of
 * SmartLootScore exists at this point, which is the larger half anyway - it is what decides whether
 * the drop is a staff or a great axe.
 */
int SmartLootScoreForBase(_item_indexes idx, const Player &player)
{
	if (idx < 0 || idx > IDI_LAST)
		return 0;
	const ItemData &data = AllItemsList[static_cast<size_t>(idx)];
	if (data.iClass != ICLASS_ARMOR && data.iClass != ICLASS_WEAPON)
		return 0;
	const ClassStatAppetite appetite = AppetiteOf(player);
	return appetite.strength * data.iMinStr
	    + appetite.magic * data.iMinMag
	    + appetite.dexterity * data.iMinDex;
}
} // namespace devilution::oracool
