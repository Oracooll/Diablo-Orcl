#include "oracool/crafting.h"

#include "oracool/gems.h"
#include "oracool/item_tiers.h" // BandedQlvl - the set pieces' depth gate
#include "oracool/imbuement.h"
#include "oracool/level_requirement.h"
#include "utils/utf8.hpp"
#include "oracool/item_sets.h"
#include "oracool/levski_roar.h"
#include "oracool/runewords.h" // GetActiveRuneword - Punch Sockets must not unmake a word

#include <fmt/format.h>

#include <algorithm>
#include <functional>
#include <vector>

#include "engine/random.hpp"
#include "inv.h"
#include "items.h"
#include "oracool/oracool.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

// The backpack walk (FindMaterials / SameKindUnits / MaterialsFor / OutputFor) lived here until
// v1.9.142, when the monument became the ONLY place a recipe can produce an item (user request).
// Deleted rather than left unreferenced: a second, silent path that can still mint items is exactly
// what "Levski's is the only place" has to mean in the code, not just in the UI. The grid's own
// walks are further down and were always separate - they answer to the twelve slots, which have
// neither InvList's compaction rules nor its footprint grid.

bool IsGem(int idx) { return IsOracoolGemIdx(idx); }
bool IsRune(int idx) { return IsOracoolRuneIdx(idx); }
// Jewels get their own predicate rather than joining IsGem, and their own recipe rather than
// widening Refine Gems. Both would have been one-line changes and both would have been wrong: the
// gem recipe takes three of a (type, quality) pair and the jewel ladder is (family, grade), so a
// shared recipe would have to know which of two decompositions applied to the index in front of it.
bool IsJewel(int idx) { return IsOracoolJewelIdx(idx); }
// Stat charms only. A Charm of Salvaging is a charm structurally - it obeys the same active cap -
// but recipe 2 turns two charms into one random STAT charm, and letting a bought 40,000 gold Primal
// charm be consumed for a Charm of Vigor is a trap, not a recipe.
/**
 * @brief The charms "Rework Charms" may consume: the six BASIC stat charms and nothing else.
 *
 * This was `IsOracoolCharmIdx && !IsOracoolSalvageCharmIdx`, which was correct while those were the
 * only two families. It stopped being correct the moment the charm space grew, and silently: the
 * predicate kept compiling and kept saying yes to everything new.
 *
 * By v1.9.23 that meant the recipe would take two charms of any kind and hand back one random basic
 * charm - so a Chapel Reliquary, a guaranteed named-encounter reward that cannot be farmed for and
 * costs a Sealed Map to earn, could be fed in and come back as a Charm of Vigor. Growing charms the
 * same. A recipe for surplus was quietly a recipe for destroying the best items in the game.
 *
 * Written as an explicit LIST rather than as more exclusions, deliberately. Every "everything except
 * the ones I have thought of" predicate here has eventually been wrong; this one can only be wrong
 * by someone adding a basic charm and not adding it here, which is a change that makes them look at
 * this line.
 */
bool IsCharm(int idx)
{
	return idx == IDI_ORACOOL_CHARM_VIGOR || idx == IDI_ORACOOL_CHARM_EMBERS
	    || idx == IDI_ORACOOL_CHARM_STORMS || idx == IDI_ORACOOL_CHARM_FORTUNE
	    || idx == IDI_ORACOOL_CHARM_LUCK || idx == IDI_ORACOOL_CHARM_GREED;
}


} // namespace

namespace {

/**
 * @brief The salvage material ladder, bottom to top - Darkness of Radament's conversion runs along it (2026-09-20).
 * Ethereal Imbueities are not on it: ethereal is a bargain, not a tier.
 */
constexpr _item_indexes MaterialLadder[] = {
	IDI_ORACOOL_SALVAGE_WHITE_SCALES, IDI_ORACOOL_SALVAGE_MAGIC_POWDER, IDI_ORACOOL_SALVAGE_RARE_FIBRES,
	IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS, IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, IDI_ORACOOL_SALVAGE_PRIMAL_VINES
};
constexpr int MaterialLadderRungs = static_cast<int>(sizeof(MaterialLadder) / sizeof(MaterialLadder[0]));
constexpr int RefineMaterialsCost = 3;
constexpr int BreakDownMaterialsYield = 2;

int MaterialLadderRung(int idx)
{
	for (int rung = 0; rung < MaterialLadderRungs; rung++) {
		if (MaterialLadder[rung] == idx)
			return rung;
	}
	return -1;
}
/** @brief A ladder material with a tier above it - Primal Vines refine into nothing. */
bool IsRefinableMaterial(int idx)
{
	const int rung = MaterialLadderRung(idx);
	return rung >= 0 && rung < MaterialLadderRungs - 1;
}
/** @brief A ladder material with a tier below it - White Scales break down into nothing. */
bool IsBreakableMaterial(int idx)
{
	return MaterialLadderRung(idx) >= 1;
}

/**
 * @brief Reroll Uniques (13) climbs the item to Primal one time in this many (Kanai's Law of Kulle can come out
 * Ancient or Primal; user, 2026-09-20: "make promotion very rare but possible"). Awaken (11) is the sure way up.
 */
constexpr int RerollPromotionOneIn = 50;

} // namespace

const char *CraftingRecipeName(int index)
{
	switch (index) {
	case 0:
		return N_("Refine Gems");
	case 1:
		return N_("Ascend Runes");
	case 2:
		return N_("Rework Charms");
	case 3:
		return N_("Free the Sockets");
	case 4:
		return N_("Temper Jewels");
	case 5:
		return N_("Reforge Gear");
	case 6:
		return N_("Ennoble Rares");
	case 7:
		return N_("Recast Set Pieces");
	case 8:
		return N_("Recolour Gems");
	case 9:
		return N_("Enrich Magic");
	case 10:
		return N_("Consecrate Rares");
	case 11:
		return N_("Awaken Uniques");
	case 12:
		return N_("Reroll Rares");
	case 13:
		return N_("Reroll Uniques");
	case 14:
		return N_("Reroll Primals");
	case 15:
		return N_("Make Ethereal");
	case 16:
		return N_("Mend the Ethereal");
	case 17:
		return N_("Punch Sockets");
	case 18:
		return N_("Cleanse Shards");
	// Levski's Cube (2026-09-20): the Horadric and Kanai additions.
	case RejuvenationRecipe:
		return N_("Rejuvenation");
	case FullRejuvenationRecipe:
		return N_("Full Rejuvenation");
	case UnbindLevelRecipe:
		return N_("Unbind the Level");
	case CraftBloodRecipe:
		return N_("Blood Craft");
	case CraftCasterRecipe:
		return N_("Caster Craft");
	case CraftHitPowerRecipe:
		return N_("Hit Power Craft");
	case CraftSafetyRecipe:
		return N_("Safety Craft");
	// Kanai's Darkness of Radament (2026-09-20): the material ladder.
	case RefineMaterialsRecipe:
		return N_("Refine Materials");
	case BreakDownMaterialsRecipe:
		return N_("Break Down Materials");
	default:
		return "";
	}
}

const char *CraftingRecipeInputs(int index)
{
	switch (index) {
	case 0:
		return N_("3 identical gems -> one of the next quality");
	case 1:
		return N_("2 identical runes -> the next rune up");
	case 2:
		return N_("2 charms of any kind -> a random charm");
	case 3:
		return N_("1 socketed item -> the item, emptied, and its stones back");
	case 4:
		return N_("3 identical jewels -> one of the next grade");
	case 5:
		return N_("1 magic or better item + 3 Unique Encrustments -> the same item, rerolled");
	case 6:
		return N_("1 rare item + 5 Rare Fibres -> a unique of the same kind");
	case 7:
		return N_("1 set piece + 3 Set Engravings -> a different piece of that set");
	case 8:
		return N_("1 gem + 2 Magic Powder -> another type, same quality");
	case 9:
		return N_("1 magic item + 4 Magic Powder -> the same item, rolled as a rare");
	case 10:
		return N_("1 rare item + 6 Set Engravings -> a set piece for that slot");
	case 11:
		return N_("1 unique item + 8 Unique Encrustments -> the same item, rolled as a primal");
	case 12:
		return N_("1 rare item + 3 Rare Fibres -> the same item, rare rolls taken again");
	case 13:
		return N_("1 unique item + 4 Unique Encrustments -> a different unique of the same kind");
	case 14:
		return N_("1 primal item + 6 Primal Vines -> the same item, primal rolls taken again");
	case 15:
		return N_("1 weapon or armour + 5 Ethereal Imbueities -> ethereal: +35%, half durability");
	case 16:
		return N_("1 damaged ethereal item + 12 Ethereal Imbueities -> fully repaired, still ethereal");
	case 17:
		return N_("1 unsocketed wearable item + 1 perfect gem per socket -> sockets to its size, 1 per 28x28 cell (1-6)");
	case 18:
		return N_("1 imbued item -> the same item with every shard gone; nothing comes back");
	case RejuvenationRecipe:
		return N_("3 healing potions + 3 mana potions -> 1 rejuvenation potion");
	case FullRejuvenationRecipe:
		return N_("3 rejuvenation potions -> 1 full rejuvenation potion");
	case UnbindLevelRecipe:
		return N_("1 wearable item + 1 Shard of Ease -> its level requirement removed for good");
	case CraftBloodRecipe:
		return N_("1 wearable item + 1 jewel + 1 rune + 1 perfect gem -> a Rare that always leeches life and adds life");
	case CraftCasterRecipe:
		return N_("1 wearable item + 1 jewel + 1 rune + 1 perfect gem -> a Rare that always adds mana and magic");
	case CraftHitPowerRecipe:
		return N_("1 wearable item + 1 jewel + 1 rune + 1 perfect gem -> a Rare that always knocks back and hits harder");
	case CraftSafetyRecipe:
		return N_("1 wearable item + 1 jewel + 1 rune + 1 perfect gem -> a Rare that always resists all and takes less damage");
	case RefineMaterialsRecipe:
		return N_("3 salvage materials of one kind -> 1 of the tier above (White Scales, Magic Powder, Rare Fibres, Set Engravings, Unique Encrustments, Primal Vines)");
	case BreakDownMaterialsRecipe:
		return N_("1 salvage material -> 2 of the tier below");
	default:
		return "";
	}
}

// ---------------------------------------------------------------------------------------------
// Levski's Roar - every recipe, run against the monument's grid. The ONLY place items are made.
// ---------------------------------------------------------------------------------------------
//
// These walks were always separate from the backpack ones that used to sit above: they answer to
// twelve fixed slots, which have neither InvList's compaction rules nor its footprint grid, and
// trying to serve both from one function is how a compaction rule ends up being applied to an array
// that does not compact. Since v1.9.142 they are the only walks there are.

namespace {

constexpr int GridSlots = LevskiGridSlots;

/** @brief Indices of the grid slots holding an item that satisfies @p matches. */
std::vector<int> FindGridMaterials(const Item *grid, bool (*matches)(int idx))
{
	std::vector<int> found;
	for (int i = 0; i < GridSlots; i++) {
		if (!grid[i].isEmpty() && matches(grid[i].IDidx))
			found.push_back(i);
	}
	return found;
}

/**
 * @brief How many stack UNITS each monument material recipe consumes.
 *
 * The same numbers the match side passes to LargestSameKindGridGroup, kept in one place so the two
 * halves cannot drift - the mistake ReagentFor's own comment warns about, where a recipe matches on
 * three and consumes two and quietly hands out free crafts.
 *
 * Recipes whose materials are single items rather than stacks answer with their slot count, which
 * is the same number when every stack is one.
 */
int MaterialUnitCostFor(int recipe)
{
	switch (recipe) {
	case 0: // Refine Gems - three identical gems
		return 3;
	case 1: // Ascend Runes - two identical runes
		return 2;
	case 2: // Transmute Charms - two charms of any kind
		return 2;
	case 3: // Free the Sockets - one socketed item
		return 1;
	case 4: // Temper Jewels - three identical jewels
		return 3;
	case RefineMaterialsRecipe: // three of one ladder material
		return RefineMaterialsCost;
	case BreakDownMaterialsRecipe: // one ladder material
		return 1;
	default:
		return 1;
	}
}

/**
 * @brief The slots holding the largest same-IDidx group among @p indices worth @p groupSize UNITS.
 *
 * Counted in stack UNITS rather than in occupied slots (audit, 2026-08-26), and the old slot count
 * was wrong in both directions at once. One stack of three gems is three gems, and Refine Gems
 * refused it because it saw a single slot. Three separate stacks of five looked like exactly three,
 * were accepted - and the consume then cleared all three SLOTS, destroying fifteen gems to make one
 * output that costs three.
 *
 * Only enough slots to cover the cost are returned, so the consume has nothing spare to throw away,
 * and the backpack implementations of the same three recipes have counted units correctly all
 * along - this brings the monument in line with them rather than inventing a rule.
 */
std::vector<int> LargestSameKindGridGroup(const Item *grid, const std::vector<int> &indices, size_t groupSize)
{
	const auto units = [grid](const std::vector<int> &slots) {
		int total = 0;
		for (const int slot : slots)
			total += std::max(1, grid[slot].stackCount());
		return total;
	};

	std::vector<int> best;
	int bestUnits = 0;
	for (const int anchor : indices) {
		std::vector<int> group;
		for (const int candidate : indices) {
			if (grid[candidate].IDidx == grid[anchor].IDidx)
				group.push_back(candidate);
		}
		const int groupUnits = units(group);
		if (groupUnits >= static_cast<int>(groupSize) && groupUnits > bestUnits) {
			bestUnits = groupUnits;
			best = std::move(group);
		}
	}
	if (bestUnits < static_cast<int>(groupSize))
		return {};

	// Trimmed to the slots the cost actually reaches into. A fourth stack that is not needed must
	// not be handed to the consume at all - that is the half that destroyed the surplus.
	std::vector<int> needed;
	int owed = static_cast<int>(groupSize);
	for (const int slot : best) {
		if (owed <= 0)
			break;
		needed.push_back(slot);
		owed -= std::max(1, grid[slot].stackCount());
	}
	return needed;
}

/**
 * @brief What each transform recipe charges. ONE table, read by the match and by the consume.
 *
 * The two halves would otherwise be a cost written twice - a recipe that matched on three
 * engravings and then consumed two would work perfectly and quietly hand out free crafts.
 *
 * The materials are the salvage tiers, and which tier pays for what is deliberate: you salvage
 * uniques to reforge, rares to ennoble, set pieces to recast. Each recipe is funded by the kind of
 * item it operates on, so the loop closes on itself.
 */
struct ReagentSpec {
	_item_indexes material;
	int count;
};

ReagentSpec ReagentFor(int recipe)
{
	switch (recipe) {
	case 5:
		return { IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, 3 };
	case 6:
		return { IDI_ORACOOL_SALVAGE_RARE_FIBRES, 5 };
	case 7:
		return { IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS, 3 };
	case 8:
		return { IDI_ORACOOL_SALVAGE_MAGIC_POWDER, 2 };
	// The tier ladder (v1.9.18). Climbing costs more than rerolling at the same rung, which is the
	// whole shape of it: you can chase a better roll cheaply, or pay to change what the item IS.
	case 9:
		return { IDI_ORACOOL_SALVAGE_MAGIC_POWDER, 4 };
	case 10:
		return { IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS, 6 };
	case 11:
		return { IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, 8 };
	case 12:
		return { IDI_ORACOOL_SALVAGE_RARE_FIBRES, 3 };
	case 13:
		return { IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, 4 };
	case 14:
		return { IDI_ORACOOL_SALVAGE_PRIMAL_VINES, 6 };
	case 15:
		return { IDI_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES, 5 };
	case 16:
		// The expensive one, and deliberately the most expensive recipe in the game. Ethereal is a
		// bargain - more of everything for half the lifespan and no smith will touch it - so a
		// repair removes the only price the item was paying. It should cost more than making one.
		return { IDI_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES, 12 };
	default:
		return { IDI_NONE, 0 };
	}
}

/**
 * @brief @p count slots holding @p materialIdx, or empty if the grid does not hold that many.
 *
 * Reagents STACK, so "three Unique Encrustments" may be one slot holding three or three slots
 * holding one. Both are spelled here, and the stack case returns the one slot - TransmuteLevskiGrid
 * decrements it rather than clearing it, which is the whole reason this returns slots and lets the
 * caller decide what consuming means.
 */
std::vector<int> FindGridReagents(const Item *grid, _item_indexes materialIdx, int count)
{
	std::vector<int> found;
	int have = 0;
	for (int i = 0; i < GridSlots && have < count; i++) {
		if (grid[i].isEmpty() || grid[i].IDidx != materialIdx)
			continue;
		found.push_back(i);
		have += grid[i].stackCount();
	}
	return have >= count ? found : std::vector<int> {};
}

/**
 * @brief Spends @p count of the reagent held across @p slots, emptying slots as they run dry.
 *
 * Stack-aware, because a reagent stack of five sitting in one slot has to be able to pay a cost of
 * three and leave two behind. Clearing the slot outright would silently confiscate the remainder.
 */
void ConsumeGridReagents(Item *grid, const std::vector<int> &slots, int count)
{
	int owed = count;
	for (const int slot : slots) {
		if (owed <= 0)
			break;
		const int taken = std::min(owed, grid[slot].stackCount());
		if (taken >= grid[slot].stackCount())
			grid[slot].clear();
		else
			grid[slot].setStackCount(grid[slot].stackCount() - taken);
		owed -= taken;
	}
}

/**
 * @brief Whether @p item sits on one of the quest uniques' own base rows (IMISC_UNIQUE: the Butcher's Cleaver, Griswold's
 * Edge, Arkaine's Valor, the Undead Crown ...). Such a row is the unique itself, not a base: rolling it again through
 * SetupAllItems clears the item (its seed check), so Reforge and Awaken took it in place and handed back nothing - the
 * unique gone, the reagents kept, no word in the log (tooltip sweep, 2026-09-25). The recipes refuse it instead.
 */
bool IsQuestUniqueBase(const Item &item)
{
	return item.IDidx >= 0 && item.IDidx <= IDI_LAST && AllItemsList[item.IDidx].iMiscId == IMISC_UNIQUE;
}

/** @brief A magic-or-better item the reforge could reroll, or -1. */
int FindGridReforgeTarget(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		const Item &item = grid[i];
		// Gear only, and only gear that HAS rolls to reroll. A white item has nothing to change,
		// and a socketable is not gear at all.
		if (item.isEmpty() || item._iMagical == ITEM_QUALITY_NORMAL)
			continue;
		if (IsQuestUniqueBase(item))
			continue;
		// Not a set piece: the reroll rebuilds the bare base and can hand back a white item - Awaken and Reroll Uniques
		// refuse set pieces for the same reason (round 8 audit, v1.12.233).
		if (IsSetItem(item))
			continue;
		if (IsOracoolGemIdx(item.IDidx) || IsOracoolRuneIdx(item.IDidx) || IsOracoolJewelIdx(item.IDidx)
		    || IsOracoolSalvageIdx(item.IDidx) || IsOracoolCharmIdx(item.IDidx))
			continue;
		// A completed runeword is NOT rerolled. Its stats come from the word, not from a seed, so
		// rerolling would silently strip it - and the runes are already inside it and would go too.
		if (item.socketedCount() > 0)
			continue;
		return i;
	}
	return -1;
}

/** @brief A rare item that some unique of its own kind could become, or -1. */
int FindGridEnnobleTarget(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		const Item &item = grid[i];
		if (item.isEmpty() || item._iOracoolTier != OracoolItemTier::Rare)
			continue;
		if (item.socketedCount() > 0)
			continue;
		if (HasUniqueForBaseOf(item))
			return i;
	}
	return -1;
}

/**
 * @brief The pieces Recast may turn @p target into: every OTHER piece of its set that this fork can build, of the same
 * item class. One list for the readiness check and the recipe (round 27 audit): the class rule went into the recipe
 * only, so 18 set pieces (every set weapon, the lone rings and amulets) showed Recast ready and made nothing.
 */
std::vector<const SetItemDefinition *> RecastCandidates(const Item &target)
{
	std::vector<const SetItemDefinition *> others;
	const SetItemDefinition *piece = FindSetItemByCursor(target._iCurs);
	if (piece == nullptr)
		return others;
	const ItemSetDefinition *set = FindItemSetOwning(piece->id);
	if (set == nullptr || set->itemCount < 2)
		return others;
	for (int i = 0; i < set->itemCount; i++) {
		const SetItemDefinition &candidate = ItemSetItems[set->firstItem + i];
		// Excluding the one in hand is the whole recipe - "convert" that returned the same piece would be a way to spend
		// three engravings on nothing.
		if (candidate.cursor == target._iCurs)
			continue;
		const int candidateBase = BaseItemForSetPiece(candidate);
		if (candidateBase < 0)
			continue;
		// Of the same class: an imbued armour piece recast into the set's ring lost every shard (the ledger lives on gear
		// only) and its punched sockets (round 26 audit, v1.12.251).
		if (AllItemsList[candidateBase].iClass != target._iClass)
			continue;
		others.push_back(&candidate);
	}
	return others;
}

/** @brief A set piece that Recast can turn into another piece of its set (RecastCandidates), or -1. */
int FindGridSetPieceTarget(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (grid[i].isEmpty() || !IsSetItem(grid[i]))
			continue;
		// Recast rebuilds the piece through InitializeItem, which zeroes the entire item - every
		// socketed gem, rune and jewel with it. Audit finding, 2026-08-26: this selector was the
		// one reroll target with no socket check, so Recast silently destroyed the stones. Every
		// other reroll selector refuses a socketed item for precisely this reason, and
		// IsTierRecipeGear records why.
		if (grid[i].socketedCount() > 0)
			continue;
		if (!RecastCandidates(grid[i]).empty())
			return i;
	}
	return -1;
}

/**
 * @brief Every set piece whose slot suits equip location @p loc.
 *
 * Consecrate turns a rare into a set piece for the SAME SLOT, so a rare helm becomes a set helm.
 * The mapping goes through BaseItemForSetPiece - the piece's own base, as the drops and the Set shelf build it -
 * and that base's ILOC is compared. It went through BaseItemForSetSlot, the slot word's generic base, which maps
 * "main_hand" to a one-handed Short Sword: no two-handed rare (a bow, a scythe) found a piece, and a crafted
 * main-hand piece lost its bow or mace family (tooltip sweep, 2026-09-25).
 */
std::vector<const SetItemDefinition *> SetPiecesForLoc(item_equip_type loc, int itemLevel, item_class cls)
{
	std::vector<const SetItemDefinition *> found;
	if (loc == ILOC_NONE || loc == ILOC_UNEQUIPABLE)
		return found;
	for (const ItemSetDefinition &set : ItemSets) {
		for (int i = 0; i < set.itemCount; i++) {
			const SetItemDefinition &piece = ItemSetItems[set.firstItem + i];
			const int base = BaseItemForSetPiece(piece);
			if (base < 0)
				continue;
			// Only pieces the item's depth could drop, as the drop path gates them: a level-3 ring became the deepest set's
			// ring (round 8 audit, v1.12.233).
			if (BandedQlvl(piece.requiredLevel) > itemLevel)
				continue;
			// The same class too: ILOC_ONEHAND is a shield's slot as well as a sword's, and a rare shield became a set mace
			// "for that slot" (round 26 audit, v1.12.251).
			if (AllItemsList[base].iLoc == loc && AllItemsList[base].iClass == cls)
				found.push_back(&piece);
		}
	}
	return found;
}

/** @brief Whether @p item is gear a tier recipe can act on at all. */
bool IsTierRecipeGear(const Item &item)
{
	if (item.isEmpty())
		return false;
	// Rings and amulets too (user, 2026-09-25: "Allow rings and amulets"). They were refused since v1.9.18 with no
	// reason on record, while Reforge and Ennoble took them and rare rings drop; jewellery is ICLASS_MISC, so it is
	// admitted by its slot.
	const bool jewellery = item._iLoc == ILOC_RING || item._iLoc == ILOC_AMULET;
	if (item._iClass != ICLASS_WEAPON && item._iClass != ICLASS_ARMOR && !jewellery)
		return false;
	// A socketed item is excluded from every reroll and every tier bump, for the reason Reforge
	// already records: its stats would come back without the stones inside it, and a completed
	// runeword's name comes from the word rather than from a seed. Empty it first.
	if (item.socketedCount() > 0)
		return false;

	// An IMBUED item is no longer excluded (2026-09-19). The Mystic Orbs were: they wrote into the
	// same _iPL* fields the affix roller does and the item recorded only how many, so a reroll could
	// not put them back and refusing was the honest answer (audit, 2026-08-26). A shard is a LEDGER
	// on the item, read at sheet time and never written into the fields - TransmuteLevskiGridWith
	// captures it before the rebuild and restores it after, so the recipe keeps what the player paid.
	return true;
}

/** @brief The first grid item matching @p wanted, by the tier a recipe operates on. */
int FindGridItemOfTier(const Item *grid, OracoolItemTier wanted)
{
	for (int i = 0; i < GridSlots; i++) {
		if (IsTierRecipeGear(grid[i]) && grid[i]._iOracoolTier == wanted)
			return i;
	}
	return -1;
}

/** @brief A plain-or-magic item - no Oracool tier - the low recipes act on, or -1. */
int FindGridUntieredItem(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (IsTierRecipeGear(grid[i]) && grid[i]._iOracoolTier == OracoolItemTier::None
		    && grid[i]._iMagical != ITEM_QUALITY_UNIQUE)
			return i;
	}
	return -1;
}

/**
 * @brief A unique item, or -1.
 *
 * BOTH senses of the word: a vanilla unique (`_iMagical == ITEM_QUALITY_UNIQUE`, a named object
 * with fixed powers) and this fork's BuffedUnique TIER (an ordinary item rolled heavily). They are
 * different things that a player calls the same thing, and a recipe that accepted only one of them
 * would refuse half the items its own name describes.
 */
int FindGridUniqueItem(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (!IsTierRecipeGear(grid[i]))
			continue;
		// A NAMED SET PIECE is not a unique, whatever its quality byte says. Audit finding,
		// 2026-08-26: MakeSetItem marks its pieces ITEM_QUALITY_UNIQUE - reasonably, since they are
		// named objects with fixed powers - so this test accepted them, and Awaken and Reroll
		// Uniques would consume a set piece and hand back an ordinary primal or vanilla unique.
		// The set piece, its powers and its place in a set the player was assembling, gone, in
		// exchange for something the recipe rolled.
		//
		// Tested by tier and by IsSetItem rather than by either alone: the tier is what the item
		// carries and the cursor lookup is what MakeSetItem actually keys on, and a piece that has
		// lost one of them is exactly the case worth refusing.
		if (grid[i]._iOracoolTier == OracoolItemTier::Set || IsSetItem(grid[i]))
			continue;
		if (IsQuestUniqueBase(grid[i]))
			continue; // see IsQuestUniqueBase
		if (grid[i]._iMagical == ITEM_QUALITY_UNIQUE || grid[i]._iOracoolTier == OracoolItemTier::BuffedUnique)
			return i;
	}
	return -1;
}

/** @brief Durable gear that is not ethereal yet, or -1. */
int FindGridEtherealTarget(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		const Item &item = grid[i];
		if (item.isEmpty() || item._iOracoolEthereal)
			continue;
		if (item._iClass != ICLASS_WEAPON && item._iClass != ICLASS_ARMOR)
			continue;
		// The same eligibility MakeItemEthereal enforces. Checked here too so the recipe does not
		// offer itself on an item it would then decline - a Transmute that consumes nothing and
		// says nothing is the worst of both.
		if (item._iMaxDur == 0 || item._iMaxDur == DUR_INDESTRUCTIBLE)
			continue;
		return i;
	}
	return -1;
}

/** @brief An ethereal item with durability missing, or -1. */
int FindGridDamagedEthereal(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		const Item &item = grid[i];
		if (item.isEmpty() || !item._iOracoolEthereal)
			continue;
		if (item._iMaxDur == 0 || item._iMaxDur == DUR_INDESTRUCTIBLE)
			continue;
		// Only a DAMAGED one. A full-durability ethereal offered this recipe would take twelve
		// imbueities for nothing.
		if (item._iDurability < item._iMaxDur)
			return i;
	}
	return -1;
}

/** @brief A rare whose slot some set piece could fill, or -1. */
int FindGridConsecrateTarget(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (!IsTierRecipeGear(grid[i]) || grid[i]._iOracoolTier != OracoolItemTier::Rare)
			continue;
		if (!SetPiecesForLoc(grid[i]._iLoc, grid[i]._iOracoolItemLevel, grid[i]._iClass).empty())
			return i;
	}
	return -1;
}

/** @brief Any gem, or -1. */
int FindGridGem(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (!grid[i].isEmpty() && IsOracoolGemIdx(grid[i].IDidx))
			return i;
	}
	return -1;
}

/** @brief The single socketed item with at least one stone in it, or -1. */
int FindGridSocketedItem(const Item *grid)
{
    for (int i = 0; i < GridSlots; i++) {
        if (!grid[i].isEmpty() && grid[i].socketedCount() > 0)
            return i;
    }
    return -1;
}

/** @brief The first item carrying an Imbuement Shard, or -1 - the Cleanse recipe's one input. */
int FindGridImbuedItem(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (!grid[i].isEmpty() && grid[i]._iOracoolImbueCount > 0)
			return i;
	}
	return -1;
}

/**
 * @brief Recipe 17, Punch Sockets (user, 2026-09-13: "Punching Sockets in Items - 1Pgem per socket. Number of
 * socket = number of 28x28px grid the item asset is made of (1-6). Pgems are consumed in the process. All
 * wearable items are eligible for socketing, no matter the type or tier.").
 */
constexpr int PunchSocketsRecipe = 17;
/**
 * @brief Recipe 18, Cleanse Shards (decision D9, 2026-09-19: "Permanent + A recipe strips every shard and
 * returns none"). One imbued item in, the same item with an empty ledger out; the shards are gone.
 */
constexpr int CleanseShardsRecipe = 18;

bool IsPerfectGemIdx(int idx)
{
	return IsOracoolGemIdx(idx) && IsPerfectGem(static_cast<uint16_t>(idx));
}

/**
 * @brief How many sockets Punch Sockets would add to @p item - 0 when it takes none.
 *
 * WEARABLE is the whole test, whatever the item's type or tier (a primal, a set piece and a plain boot are
 * equally eligible): something with a place on the paper doll. Socketables, charms, consumables and gold do not
 * have one, whatever their footprint.
 *
 * UNSOCKETED only (user, the same day: "the item being socketed must not have sockets"). An item that already has
 * even one socket is refused, so the recipe never tops a socketed item up - and that one rule also covers the
 * completed runeword, whose word needs its socket count to match its runes and which always has sockets.
 *
 * The count is then the item's whole footprint (MaxSocketsForItem - the rule drops already follow).
 */
int SocketsToPunch(const Item &item)
{
	if (item.isEmpty())
		return 0;
	if (item._iLoc == ILOC_NONE || item._iLoc == ILOC_UNEQUIPABLE || item._iLoc == ILOC_BELT)
		return 0;
	if (item._itype == ItemType::Misc || item._itype == ItemType::Gold || item._itype == ItemType::None)
		return 0;
	if (item._iSocketCount > 0)
		return 0;
	// Belt and braces for a record whose count reads zero but whose word still resolves.
	if (GetActiveRuneword(item) != nullptr)
		return 0;
	return MaxSocketsForItem(item);
}

/** @brief The first wearable item in @p grid that can take a socket, or -1. */
int FindGridPunchTarget(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (SocketsToPunch(grid[i]) > 0)
			return i;
	}
	return -1;
}

/**
 * @brief Slots holding @p count perfect gems of ANY type between them, or empty if the grid holds fewer.
 *
 * Any type, and mixed: the request charges "1 Pgem per socket", not a set of one kind. Counted in stack units,
 * so a single stack of six pays for a six-socket armour.
 */
std::vector<int> FindGridPerfectGems(const Item *grid, int count)
{
	std::vector<int> found;
	int have = 0;
	for (int i = 0; i < GridSlots && have < count; i++) {
		if (grid[i].isEmpty() || !IsPerfectGemIdx(grid[i].IDidx))
			continue;
		found.push_back(i);
		have += std::max(1, grid[i].stackCount());
	}
	return have >= count ? found : std::vector<int> {};
}

/** @brief The slots recipe @p index would consume from @p grid, empty when it cannot run. */
/** @brief The base row of a potion kind (the rejuvenation rows carry no enumerator of their own). */
_item_indexes FindBaseByMisc(item_misc_id misc)
{
	for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (AllItemsList[i].iMiscId == misc)
			return static_cast<_item_indexes>(i);
	}
	return IDI_NONE;
}

/** @brief Grid slots holding potions of @p misc, enough for @p count units (stack-aware), or empty. */
std::vector<int> FindGridPotions(const Item *grid, item_misc_id misc, int count)
{
	std::vector<int> found;
	int have = 0;
	for (int i = 0; i < GridSlots && have < count; i++) {
		if (grid[i].isEmpty() || grid[i]._iMiscId != misc)
			continue;
		found.push_back(i);
		have += grid[i].stackCount();
	}
	return have >= count ? found : std::vector<int> {};
}

/**
 * @brief The Cube's target (2026-09-20): the first worn-or-wielded item in the grid. With @p plainOnly,
 * only a Basic or Magic item with no Oracool tier and no unique identity - what a Diablo II craft takes.
 */
int FindGridWearable(const Item *grid, bool plainOnly)
{
	for (int i = 0; i < GridSlots; i++) {
		const Item &item = grid[i];
		if (item.isEmpty() || item.IDidx < 0 || item.IDidx > IDI_LAST)
			continue;
		const ItemData &data = AllItemsList[item.IDidx];
		if (data.iLoc == ILOC_UNEQUIPABLE || data.iLoc == ILOC_BELT)
			continue;
		if (plainOnly && (item._iMagical == ITEM_QUALITY_UNIQUE || item._iOracoolTier != OracoolItemTier::None))
			continue;
		// A craft REBUILDS the item (RetierOracoolItem): stones inside it and shards on it would not
		// come back, the rule IsTierRecipeGear already keeps for the rerolls (audit, 2026-09-20).
		if (plainOnly && (item.socketedCount() > 0 || item._iOracoolImbueCount > 0))
			continue;
		return i;
	}
	return -1;
}

std::vector<int> GridMaterialsFor(const Item *grid, int index)
{
	switch (index) {
	case 0: { // three identical gems - same type AND quality, perfect excluded
		std::vector<int> gems = FindGridMaterials(grid, IsGem);
		gems.erase(std::remove_if(gems.begin(), gems.end(),
		                [&](int i) { return IsPerfectGem(static_cast<uint16_t>(grid[i].IDidx)); }),
		    gems.end());
		return LargestSameKindGridGroup(grid, gems, 3);
	}
	case 1: { // two identical runes, Zod excluded - it is the top of the ladder
		std::vector<int> runes = FindGridMaterials(grid, IsRune);
		runes.erase(std::remove_if(runes.begin(), runes.end(),
		                [&](int i) { return IsTopRune(static_cast<uint16_t>(grid[i].IDidx)); }),
		    runes.end());
		return LargestSameKindGridGroup(grid, runes, 2);
	}
	case 2: { // two charms of any kind
		std::vector<int> charms = FindGridMaterials(grid, IsCharm);
		if (charms.size() < 2)
			return {};
		charms.resize(2);
		return charms;
	}
	case 3: { // one socketed item with something in it
		const int socketed = FindGridSocketedItem(grid);
		return socketed < 0 ? std::vector<int> {} : std::vector<int> { socketed };
	}
	case 4: { // three identical jewels, Radiant excluded
		std::vector<int> jewels = FindGridMaterials(grid, IsJewel);
		jewels.erase(std::remove_if(jewels.begin(), jewels.end(),
		                 [&](int i) { return IsTopJewel(static_cast<uint16_t>(grid[i].IDidx)); }),
		    jewels.end());
		return LargestSameKindGridGroup(grid, jewels, 3);
	}
	// ---------------------------------------------------------------------------------------
	// The four adopted from Kanai's Cube (v1.9.17). Each takes ONE item plus a REAGENT, and the
	// reagent is what makes them tell each other apart: with four recipes all eating "one item",
	// an auto-picked transmute needs the inputs to be disjoint or the monument becomes a
	// lottery. The reagents are the salvage materials, which until now had no consumer anywhere
	// in the game - they dropped, stacked, sorted into their stash row, and were never spent.
	// ---------------------------------------------------------------------------------------
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16: {
		// One shape for all four: find the target this recipe operates on, then find its reagent
		// through ReagentFor - the same table the consume step reads, so a recipe cannot match on
		// one cost and charge another.
		int target = -1;
		switch (index) {
		case 5:
			target = FindGridReforgeTarget(grid);
			break;
		case 6:
			target = FindGridEnnobleTarget(grid);
			break;
		case 7:
			target = FindGridSetPieceTarget(grid);
			break;
		case 8:
			target = FindGridGem(grid);
			break;
		case 9:
			target = FindGridUntieredItem(grid);
			break;
		case 10:
			target = FindGridConsecrateTarget(grid);
			break;
		case 11:
		case 13:
			target = FindGridUniqueItem(grid);
			break;
		case 12:
			target = FindGridItemOfTier(grid, OracoolItemTier::Rare);
			break;
		case 14:
			target = FindGridItemOfTier(grid, OracoolItemTier::Primal);
			break;
		case 15:
			target = FindGridEtherealTarget(grid);
			break;
		case 16:
			target = FindGridDamagedEthereal(grid);
			break;
		default:
			return {};
		}
		if (target < 0)
			return {};
		// Imbued items pass (2026-09-19): the shard ledger is captured before the rebuild and put back
		// after it in TransmuteLevskiGridWith. The orbs were refused here from 2026-09-13 because a
		// count could not be put back; a ledger can.

		const ReagentSpec spec = ReagentFor(index);
		std::vector<int> out = FindGridReagents(grid, spec.material, spec.count);
		if (out.empty())
			return {};
		// Target FIRST - TransmuteLevskiGrid reads materials[0] as the thing being transformed and
		// everything after it as reagent slots.
		out.insert(out.begin(), target);
		return out;
	}
	case CleanseShardsRecipe: { // one item carrying at least one shard
		const int imbued = FindGridImbuedItem(grid);
		return imbued < 0 ? std::vector<int> {} : std::vector<int> { imbued };
	}
	case PunchSocketsRecipe: {
		// The item first, then exactly enough perfect gems for the sockets it can still take - the same
		// target-then-materials shape the transform recipes use.
		const int target = FindGridPunchTarget(grid);
		if (target < 0)
			return {};
		std::vector<int> out = FindGridPerfectGems(grid, SocketsToPunch(grid[target]));
		if (out.empty())
			return {};
		out.insert(out.begin(), target);
		return out;
	}
	// Levski's Cube (2026-09-20).
	case RejuvenationRecipe: { // 3 healing + 3 mana
		const std::vector<int> heal = FindGridPotions(grid, IMISC_HEAL, 3);
		const std::vector<int> mana = FindGridPotions(grid, IMISC_MANA, 3);
		if (heal.empty() || mana.empty())
			return {};
		std::vector<int> both = heal;
		both.insert(both.end(), mana.begin(), mana.end());
		return both;
	}
	case FullRejuvenationRecipe: // 3 rejuvenation
		return FindGridPotions(grid, IMISC_REJUV, 3);
	case UnbindLevelRecipe: { // a wearable that asks a level, and one Shard of Ease
		const int target = FindGridWearable(grid, /*plainOnly=*/false);
		if (target < 0 || grid[target]._iOracoolLevelFree || RequiredLevel(grid[target]) <= 1)
			return {};
		const std::vector<int> shard = FindGridReagents(grid, IDI_ORACOOL_SHARD_EASE, 1);
		if (shard.empty())
			return {};
		std::vector<int> all { target };
		all.insert(all.end(), shard.begin(), shard.end());
		return all;
	}
	case CraftBloodRecipe:
	case CraftCasterRecipe:
	case CraftHitPowerRecipe:
	case CraftSafetyRecipe: { // a plain wearable, a jewel, a rune and a perfect gem
		const int target = FindGridWearable(grid, /*plainOnly=*/true);
		if (target < 0)
			return {};
		const std::vector<int> jewels = FindGridMaterials(grid, IsJewel);
		const std::vector<int> runes = FindGridMaterials(grid, IsRune);
		std::vector<int> gems = FindGridMaterials(grid, IsGem);
		gems.erase(std::remove_if(gems.begin(), gems.end(),
		               [&](int i) { return !IsPerfectGem(static_cast<uint16_t>(grid[i].IDidx)); }),
		    gems.end());
		if (jewels.empty() || runes.empty() || gems.empty())
			return {};
		return { target, jewels[0], runes[0], gems[0] };
	}
	// Darkness of Radament (2026-09-20): the largest same-kind group of a ladder material worth the cost, counted in
	// stack units exactly as Refine Gems counts its stones; the top rung cannot refine, the bottom cannot break down.
	case RefineMaterialsRecipe:
		return LargestSameKindGridGroup(grid, FindGridMaterials(grid, IsRefinableMaterial), RefineMaterialsCost);
	case BreakDownMaterialsRecipe:
		return LargestSameKindGridGroup(grid, FindGridMaterials(grid, IsBreakableMaterial), 1);
	default:
		return {};
	}
}

/** @brief How many free slots @p grid has once @p consumed are emptied. */
int GridRoomAfter(const Item *grid, const std::vector<int> &consumed)
{
	int free = 0;
	for (int i = 0; i < GridSlots; i++) {
		if (grid[i].isEmpty() || std::find(consumed.begin(), consumed.end(), i) != consumed.end())
			free++;
	}
	return free;
}

} // namespace

int CraftingRecipeReagentCount(int index)
{
	return ReagentFor(index).count;
}

int CraftingRecipeReagentItem(int index)
{
	return ReagentFor(index).material;
}

bool CanCraftFromLevskiGrid(const Item *grid, int index)
{
	const std::vector<int> materials = GridMaterialsFor(grid, index);
	if (materials.empty())
		return false;
	// Not Free the Sockets on an ethereal Zod host, which refuses (round 39 audit: auto-pick kept choosing it).
	if (index == 3 && grid[materials[0]]._iOracoolEthereal && SocketsMakeIndestructible(grid[materials[0]]))
		return false;
	return true;
}

int FirstReadyLevskiRecipe(const Item *grid)
{
	// MOST SLOTS WINS, ties to the lowest index.
	//
	// This was "the lowest-numbered ready recipe", which was fine while the five recipes had
	// disjoint inputs. It stopped being fine the moment four more arrived that all eat "one item
	// plus a reagent": a socketed rare with three Unique Encrustments beside it satisfies both
	// Free the Sockets (one slot) and Reforge (four), and lowest-index would silently pick the
	// former every time - so the reforge reagents would be unusable on anything socketed and the
	// player would have no way to tell why.
	//
	// Most-slots is the rule because it is the one a player can predict without reading this file:
	// the monument runs the recipe that uses the most of what you put in front of it. Putting in
	// only what a recipe needs is how you choose, which is how the Horadric Cube always worked.
	int best = -1;
	size_t bestSlots = 0;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		const std::vector<int> materials = GridMaterialsFor(grid, i);
		// Reforge never wins a tie, as FirstReadyLevskiRecipeFor rules (round 27 audit: the rule was in the host version only).
		const bool displacesReforgeTie = best == 5 && materials.size() == bestSlots;
		if (materials.empty() || (materials.size() <= bestSlots && !displacesReforgeTie))
			continue;
		best = i;
		bestSlots = materials.size();
	}
	return best;
}

// The transmute's two refusals - answers that consumed nothing. Written once, here, because Levski's
// window has to tell them from a transmute that made something (it sounds only for the latter), and
// comparing against a second copy of the words would break the day either was reworded.
std::string NoRoomToFreeStones()
{
	return std::string(_("not enough room to free the stones"));
}

std::string ZodBoundInEthereal()
{
	return std::string(_("Zod is bound for good in an ethereal item"));
}

std::string NoRoomForResult()
{
	return std::string(_("not enough room for the result"));
}

TransmuteHost HostOfRecipe(int recipe)
{
	// Decision D8 (2026-09-20): the split the Roadmap's artisan cards proposed. Griswold the gear;
	// Ogden the stones and sockets; Gillian the charms, set pieces, magic and shards; the Cube the
	// Horadric and Kanai additions.
	switch (recipe) {
	case 5:
	case 6:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case RefineMaterialsRecipe: // the material ladder sits beside his salvage plates (2026-09-20)
	case BreakDownMaterialsRecipe:
		// The CUBE since 2026-09-21: Griswold's window is the user's painted SALVAGE UI, which has no grid, so his gear
		// recipes and the material ladder live in the Cube's book. TransmuteHost::Smith stays for the salvage window itself.
		return TransmuteHost::Cube;
	case 0:
	case 1:
	case 3:
	case 4:
	case 8:
	case 17:
		return TransmuteHost::Tavern;
	case 2:
	case 7:
	case 9:
	case 18:
		return TransmuteHost::Barmaid;
	default:
		return TransmuteHost::Cube;
	}
}

bool IsReworkableCharmIdx(int idx)
{
	return IsCharm(idx);
}

bool RecipeBelongsTo(int recipe, TransmuteHost host)
{
	return recipe >= 0 && recipe < CraftingRecipeCount && HostOfRecipe(recipe) == host;
}

const char *TransmuteHostTitle(TransmuteHost host)
{
	switch (host) {
	case TransmuteHost::Smith:
		return N_("Griswold's Forge");
	case TransmuteHost::Tavern:
		return N_("Ogden's Table");
	case TransmuteHost::Barmaid:
		return N_("Gillian's Hearth");
	case TransmuteHost::Cube:
		break;
	}
	return N_("Levski's Cube");
}

int FirstReadyLevskiRecipeFor(const Item *grid, TransmuteHost host)
{
	// Reforge (5) never wins a TIE: a unique beside one stack of Unique Encrustments made Reforge, Awaken and Reroll
	// Uniques all two-slot matches, and the lowest index - Reforge, a drop-odds reroll that often yields a white base - ran
	// unasked (round 26 audit, v1.12.251). Chosen from the book, it still runs.
	constexpr int ReforgeGearRecipe = 5;
	int best = -1;
	size_t bestSlots = 0;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		if (!RecipeBelongsTo(i, host))
			continue;
		const std::vector<int> materials = GridMaterialsFor(grid, i);
		const bool displacesReforgeTie = best == ReforgeGearRecipe && materials.size() == bestSlots;
		if (materials.empty() || (materials.size() <= bestSlots && !displacesReforgeTie))
			continue;
		best = i;
		bestSlots = materials.size();
	}
	return best;
}

bool IsTransmuteRefusal(const std::string &result)
{
	return result == NoRoomToFreeStones() || result == NoRoomForResult() || result == ZodBoundInEthereal(); // a refusal (round 39)
}

std::string TransmuteLevskiGrid(Item *grid)
{
	return TransmuteLevskiGridWith(grid, -1);
}

std::string TransmuteLevskiGridWith(Item *grid, int index)
{
	// A CHOSEN recipe that is not ready runs NOTHING, and returns empty like every other
	// nothing-happened path. Falling back to whatever else is ready would be the worst possible
	// answer: the player selected Reroll Rares, is short one fibre, and the monument ennobles the
	// item instead using a different pile of materials.
	//
	// Empty rather than a refusal MESSAGE, though the first draft returned one - the return value's
	// contract is "what was made", and a caller cannot tell a refusal from a success if both are
	// text. The monument asks CanCraftFromLevskiGrid itself to say why nothing happened.
	if (index >= 0 && index < CraftingRecipeCount && !CanCraftFromLevskiGrid(grid, index))
		return {};
	const int recipe = index >= 0 ? index : FirstReadyLevskiRecipe(grid);
	if (recipe < 0)
		return {};
	const std::vector<int> materials = GridMaterialsFor(grid, recipe);
	if (materials.empty())
		return {};

	// Freeing sockets is the one recipe that gives back MORE than it takes, so its room check is
	// its own: the host stays in place and each stone needs a cell of its own.
	//
	// CELLS, not array slots. GridRoomAfter counts free entries in a twelve-long array, which is
	// the right answer for recipes that shrink 1x1 inputs into a 1x1 output and the wrong one here:
	// a 2x3 breastplate holding six stones occupies one array slot and six cells, and "eleven slots
	// free" approved a transmute that needed thirteen cells in a twelve-cell grid. The repack then
	// silently dropped whatever did not fit. The full case - a six-socket 2x3 host - exactly fills
	// the grid with nothing to spare, so anything else sharing the grid is already one cell too
	// many; this is a boundary the player walks straight into, not a corner.
	//
	// So the check is a real packing of the real result, run by the grid's own placement code
	// (LevskiGridCanHold) rather than by arithmetic here that would have to be kept in step with it.
	if (recipe == 3) {
		Item &host = grid[materials[0]];
		const int stones = host.socketedCount();
		// Zod stays in an ethereal item (round 38 audit): freed, the host came back whole at its maximum - a free repair of
		// what no smith repairs, and the same Zod did it again for the next one. The wear before Zod is not kept.
		if (host._iOracoolEthereal && SocketsMakeIndestructible(host))
			return ZodBoundInEthereal();

		std::vector<Item> after;
		after.reserve(GridSlots + Item::MaxItemSockets);
		for (int i = 0; i < GridSlots; i++) {
			if (!grid[i].isEmpty())
				after.push_back(grid[i]);
		}
		for (const uint16_t socketed : host._iSocketed) {
			if (socketed == Item::EmptySocket)
				continue;
			Item stone;
			InitializeItem(stone, static_cast<_item_indexes>(socketed));
			after.push_back(stone);
		}
		if (!LevskiGridCanHold(after.data(), static_cast<int>(after.size())))
			return NoRoomToFreeStones();

		std::string freed;
		int placed = 0;
		// Asked before the stones come out: only a completed word's name is the word's (audit, 2026-09-29).
		const bool hadRuneword = GetActiveRuneword(host) != nullptr;
		for (uint16_t &socketed : host._iSocketed) {
			if (socketed == Item::EmptySocket)
				continue;
			for (int slot = 0; slot < GridSlots && placed <= stones; slot++) {
				if (!grid[slot].isEmpty())
					continue;
				InitializeItem(grid[slot], static_cast<_item_indexes>(socketed));
				GenerateNewSeed(grid[slot]);
				grid[slot]._iIdentified = true;
				placed++;
				break;
			}
			socketed = Item::EmptySocket;
		}
		// The host keeps its sockets - they are empty again, ready to take something else. Zod's
		// stamp is undone by restoring durability from the maximum it deliberately left intact.
		if (host._iMaxDur > 0 && host._iDurability == DUR_INDESTRUCTIBLE)
			host._iDurability = host._iMaxDur;
		// A completed runeword's name came from the word; with the runes gone it is a base item
		// again, so the name has to go back too. A magic, rare or set host keeps its own (audit, 2026-09-29: it was
		// blanked for good).
		if (hadRuneword)
			host._iIName[0] = '\0';
		freed = fmt::format(fmt::runtime(_("{:d} stones freed")), placed);
		return freed;
	}

	// PUNCH SOCKETS. In place, like the transforms below, but its cost is not a fixed reagent count - it is one
	// perfect gem per socket, and the number of sockets depends on the item - so it charges its own count
	// rather than ReagentFor's. Nothing is produced, so the grid only gets emptier and no room check applies.
	// Levski's Cube (2026-09-20): the Horadric potions, Kanai's Work of Cathan, the four crafts.
	if (recipe == RejuvenationRecipe || recipe == FullRejuvenationRecipe) {
		const _item_indexes result = FindBaseByMisc(recipe == RejuvenationRecipe ? IMISC_REJUV : IMISC_FULLREJUV);
		if (result == IDI_NONE)
			return {};
		// Potions STACK, so a consumed slot is free only when the stack is spent: the room question is
		// asked on a copy that has paid, before the real grid pays (audit, 2026-09-20 - a full grid
		// of five-stacks ate six potions and then had nowhere to put the result).
		{
			Item scratch[GridSlots];
			std::copy(grid, grid + GridSlots, scratch);
			if (recipe == RejuvenationRecipe) {
				ConsumeGridReagents(scratch, FindGridPotions(scratch, IMISC_HEAL, 3), 3);
				ConsumeGridReagents(scratch, FindGridPotions(scratch, IMISC_MANA, 3), 3);
			} else {
				ConsumeGridReagents(scratch, materials, 3);
			}
			bool room = false;
			for (int slot = 0; slot < GridSlots && !room; slot++)
				room = scratch[slot].isEmpty();
			if (!room)
				return NoRoomForResult();
		}
		if (recipe == RejuvenationRecipe) {
			ConsumeGridReagents(grid, FindGridPotions(grid, IMISC_HEAL, 3), 3);
			ConsumeGridReagents(grid, FindGridPotions(grid, IMISC_MANA, 3), 3);
		} else {
			ConsumeGridReagents(grid, materials, 3);
		}
		for (int slot = 0; slot < GridSlots; slot++) {
			if (!grid[slot].isEmpty())
				continue;
			InitializeItem(grid[slot], result);
			GenerateNewSeed(grid[slot]);
			grid[slot]._iIdentified = true;
			return std::string(grid[slot].getName());
		}
		return NoRoomForResult();
	}
	// Darkness of Radament (2026-09-20): three of one ladder material become one of the tier above, or one becomes
	// two of the tier below. The result joins a stack of its kind with room, else takes a free slot - asked on a
	// copy that has paid, as the potions do, so a full grid refuses before anything is spent.
	if (recipe == RefineMaterialsRecipe || recipe == BreakDownMaterialsRecipe) {
		const bool up = recipe == RefineMaterialsRecipe;
		const int cost = up ? RefineMaterialsCost : 1;
		const int made = up ? 1 : BreakDownMaterialsYield;
		const int rung = MaterialLadderRung(grid[materials[0]].IDidx);
		if (rung < 0 || (up && rung >= MaterialLadderRungs - 1) || (!up && rung < 1))
			return {};
		const _item_indexes result = MaterialLadder[rung + (up ? 1 : -1)];
		Item scratch[GridSlots];
		std::copy(grid, grid + GridSlots, scratch);
		ConsumeGridReagents(scratch, materials, cost);
		int into = -1;
		for (int slot = 0; slot < GridSlots && into < 0; slot++) {
			if (!scratch[slot].isEmpty() && scratch[slot].IDidx == result && scratch[slot].stackCount() + made <= Item::MaxStackCount)
				into = slot;
		}
		for (int slot = 0; slot < GridSlots && into < 0; slot++) {
			if (scratch[slot].isEmpty())
				into = slot;
		}
		if (into < 0)
			return NoRoomForResult();
		ConsumeGridReagents(grid, materials, cost);
		if (!grid[into].isEmpty() && grid[into].IDidx == result) {
			grid[into].setStackCount(grid[into].stackCount() + made);
		} else {
			InitializeItem(grid[into], result);
			GenerateNewSeed(grid[into]);
			grid[into]._iIdentified = true;
			grid[into]._iStatFlag = true;
			grid[into].setStackCount(made);
		}
		return fmt::format(fmt::runtime(_("{:d} {:s}")), made, std::string(grid[into].getName()));
	}
	if (recipe == UnbindLevelRecipe) {
		Item &target = grid[materials[0]];
		const std::vector<int> shard(materials.begin() + 1, materials.end());
		target._iOracoolLevelFree = true;
		ConsumeGridReagents(grid, shard, 1);
		return fmt::format(fmt::runtime(_("{:s}, unbound from its level")), std::string(target.getName()));
	}
	if (recipe >= CraftBloodRecipe && recipe <= CraftSafetyRecipe) {
		Item &target = grid[materials[0]];
		if (!RetierOracoolItem(target, OracoolItemTier::Rare))
			return {};
		// Inside the Rare's own limits (user, 2026-09-27: "fix the decisions for me too"). The Rare rolled 2-4 affixes and
		// the two powers below came on top - six on an item whose limit is four. It keeps two of its rolls now, and the
		// two powers make four.
		constexpr int CraftPowers = 2;
		if (const int keep = std::max(0, OracoolAffixBudget(target) - CraftPowers); target._iOracoolAffixCount > keep) {
			const std::vector<OracoolAffix> kept(target._iOracoolAffixes.begin(), target._iOracoolAffixes.begin() + keep);
			if (!RebuildOracoolItemWithAffixes(*MyPlayer, target, kept.data(), keep))
				return {};
		}
		// Diablo II's crafts each guarantee two properties on top of the Rare's own rolls. Applied
		// through SaveItemPower and recorded in the affix list exactly as a rolled affix is, so the
		// sheet, the tooltip and the level requirement all see them.
		struct CraftPower {
			const char *word;
			ItemPower first;
			ItemPower second;
		};
		// Ranges as SaveItemPower reads them (audit, 2026-09-20): STEALLIFE keys on param1 == 3 for
		// the 3% flag (a 4 or 5 would have rolled and still been 3%), GETHIT is SUBTRACTED from the
		// damage taken (so a negative range ADDED damage), and ALLRES sits on its lowest table row.
		const CraftPower craft = recipe == CraftBloodRecipe      ? CraftPower { N_("Blood"), { IPL_STEALLIFE, 3, 3 }, { IPL_LIFE, 15, 25 } }
		    : recipe == CraftCasterRecipe                        ? CraftPower { N_("Caster"), { IPL_MANA, 15, 25 }, { IPL_MAG, 3, 5 } }
		    : recipe == CraftHitPowerRecipe                      ? CraftPower { N_("Hit Power"), { IPL_KNOCKBACK, 0, 0 }, { IPL_TOHIT, 10, 15 } }
		                                                         : CraftPower { N_("Safety"), { IPL_ALLRES, 10, 15 }, { IPL_GETHIT, 1, 3 } };
		for (ItemPower power : { craft.first, craft.second }) {
			// And under the item-level ceiling every affix obeys: no larger than a row of its kind at or below the
			// item's level would roll, and none at all where no such row exists yet.
			const int ceiling = LargestAffixRollAtOrBelow(power.type, std::max<int>(1, target._iOracoolItemLevel));
			if (ceiling < 0)
				continue;
			power.param1 = std::min(power.param1, ceiling);
			power.param2 = std::min(power.param2, ceiling);
			const int raw = ApplyOracoolItemPower(*MyPlayer, target, power);
			// Joined to the Rare's own affix of the same kind when it rolled one (tooltip sweep, 2026-09-25: a Safety
			// Craft printed "-1 damage from enemies" twice). The stat was applied twice either way; one row with the
			// sum says what the item does, and keeps the one-row-per-kind rule the roller itself follows.
			// Not life or mana steal: their percentages are flags (3 and 5), which do not add, so a merged 6 or 8 matched no
			// flag and the next rework replayed it as none (audit, 2026-09-29). Those keep their own row.
			OracoolAffix *same = nullptr;
			for (int i = 0; i < target._iOracoolAffixCount && !IsAnyOf(power.type, IPL_STEALLIFE, IPL_STEALMANA); i++) {
				if (target._iOracoolAffixes[i].type == power.type)
					same = &target._iOracoolAffixes[i];
			}
			if (same != nullptr)
				same->param1 += raw;
			else if (target._iOracoolAffixCount < Item::MaxOracoolAffixes)
				target._iOracoolAffixes[target._iOracoolAffixCount++] = OracoolAffix { power.type, raw, 0 };
		}
		const std::string crafted = fmt::format("{:s} {:s}", _(craft.word), std::string(target.getName()));
		CopyUtf8(target._iIName, crafted, sizeof(target._iIName));
		target._iIdentified = true;
		for (size_t i = 1; i < materials.size(); i++)
			ConsumeGridReagents(grid, { materials[i] }, 1);
		return crafted;
	}
	if (recipe == PunchSocketsRecipe) {
		Item &host = grid[materials[0]];
		// Counted BEFORE the host changes, because the count is what the gems pay for.
		const int punched = SocketsToPunch(host);
		if (punched <= 0)
			return {};
		const std::vector<int> gems(materials.begin() + 1, materials.end());
		const int before = host._iSocketCount;
		host._iSocketCount = static_cast<uint8_t>(before + punched);
		// Every punched socket is EMPTY. The host had none before (SocketsToPunch refuses a socketed item), so
		// `before` is zero; written as a range so the loop stays right if that rule is ever relaxed.
		for (int s = before; s < host._iSocketCount; s++)
			host._iSocketed[s] = Item::EmptySocket;
		host.normalizeSockets();
		ConsumeGridReagents(grid, gems, punched);
		return fmt::format(fmt::runtime(_("{:d} sockets punched in {:s}")), punched, std::string(host.getName()));
	}

	// The four adopted from Kanai's Cube. All of them TRANSFORM the target in place and consume
	// their reagent, so the grid can only ever get emptier - no room check is needed or wanted, and
	// there is no "output item" for the generic path below to place.
	if (recipe >= 5) {
		Item &target = grid[materials[0]];
		const std::vector<int> reagents(materials.begin() + 1, materials.end());
		std::string what;
		// ALL OR NOTHING (tooltip sweep, 2026-09-25). Every recipe below changes the target in place, and a refusal
		// part-way - an empty return after the rebuild had begun - left it half made or cleared: the Butcher's
		// Cleaver went into Awaken and came back as nothing. The item is put back unless the recipe reaches its end.
		struct RestoreUnlessDone {
			Item &target;
			const Item before;
			bool done = false;
			~RestoreUnlessDone()
			{
				if (!done)
					target = before;
			}
		} restore { target, target };

		// The Imbuement Shard ledger, captured before any recipe below rebuilds the item from a bare
		// InitializeItem (Reforge, Ennoble, Recast, the tier ladder, the rerolls all do) and put back
		// after - a shard is permanent, and that cuts both ways: a rebuild may not delete it either.
		// The restore is keyed on the ledger having been WIPED, so a recipe that changes the item in
		// place (Recolour, the ethereal pair) neither loses it nor doubles Tempering's durability.
		const ImbuementLedger ledger = CaptureImbuements(target);

		switch (recipe) {
		case CleanseShardsRecipe: // CLEANSE - every shard gone, nothing back (D9)
			if (target._iOracoolImbueCount == 0)
				return {};
			StripImbuements(target);
			what = fmt::format(fmt::runtime(_("{:s}, cleansed")), std::string(target.getName()));
			break;
		case 5: // REFORGE - the same base, every roll taken again at its own item level
			if (!ReforgeOracoolItem(target))
				return {};
			what = std::string(target.getName());
			break;
		case 6: // ENNOBLE - a rare becomes a unique of its own kind
			if (!EnnobleOracoolRare(target))
				return {};
			what = std::string(target.getName());
			break;
		case 7: { // RECAST - a set piece becomes a DIFFERENT piece of the same set
			// Every OTHER piece of the set, of the same class, that this fork can build - the list the readiness check reads.
			const std::vector<const SetItemDefinition *> others = RecastCandidates(target);
			if (others.empty())
				return {};
			// The depth the item was FOUND at, captured before InitializeItem wipes it. Audit
			// finding, 2026-08-26: both recipes rebuilt the item and never put the level or the
			// base tier back, so recasting a deep set piece quietly reset it to a floor-zero item.
			const int keptLevel = target._iOracoolItemLevel;
			const auto keptTier = static_cast<oracool::BaseItemTier>(target._iOracoolBaseTier); // kept too (round 34 audit)
			const SetItemDefinition *chosen = others[GenerateRnd(static_cast<int32_t>(others.size()))];
			const bool wasEthereal = target._iOracoolEthereal;
			// Kanai's Work of Cathan too (audit, 2026-09-29): InitializeItem empties the item, and the keepsake notes name
			// these recipes as ones that must keep it.
			const bool wasLevelFree = target._iOracoolLevelFree;
			// The wear, the broken flag and the empty sockets too (round 8 audit, v1.12.233): the rebuild was a free repair -
			// Mend bypassed - and it deleted sockets bought with Punch Sockets.
			const int oldDurability = target._iDurability;
			const bool wasBroken = target._iOracoolBroken;
			const int oldSockets = target._iSocketCount;
			InitializeItem(target, static_cast<_item_indexes>(BaseItemForSetPiece(*chosen)));
			MakeSetItem(target, *chosen);
			FinalizeSetPiece(target, keptLevel, /*allowEtherealRoll=*/false, keptTier);
			// InitializeItem starts from an empty Item, so the ethereal bargain went with it (audit,
			// 2026-09-13). May decline on an indestructible piece, which then simply stays whole.
			if (wasEthereal)
				MakeItemEthereal(target);
			target._iOracoolLevelFree = wasLevelFree;
			if (oldDurability != DUR_INDESTRUCTIBLE && target._iMaxDur != DUR_INDESTRUCTIBLE)
				target._iDurability = std::min<int>(oldDurability, target._iMaxDur);
			target._iOracoolBroken = wasBroken && target._iDurability == 0;
			target._iSocketCount = static_cast<uint8_t>(std::min(oldSockets, MaxSocketsForItem(target)));
			target._iIdentified = true;
			what = std::string(target.getName());
			break;
		}
		case 8: { // RECOLOUR - a gem keeps its quality and changes its type
			GemType type;
			GemQuality quality;
			if (!GemTypeAndQuality(static_cast<uint16_t>(target.IDidx), type, quality))
				return {};
			if (GemTypeCount < 2)
				return {};
			// Rolled among the OTHER types, so the recipe always changes something.
			const int step = 1 + GenerateRnd(static_cast<int32_t>(GemTypeCount) - 1);
			const auto newType = static_cast<GemType>((static_cast<int>(type) + step) % static_cast<int>(GemTypeCount));
			// The whole stack changes colour: InitializeItem empties the item, count included, and twenty Chipped Rubies came
			// back as one gem (round 8 audit, v1.12.233).
			const int units = target.stackCount();
			InitializeItem(target, static_cast<_item_indexes>(GemIndexFor(newType, quality)));
			target.setStackCount(units);
			GenerateNewSeed(target);
			target._iIdentified = true;
			what = std::string(target.getName());
			break;
		}
		case 9: // ENRICH - a plain or magic item rolled again as a rare
			if (!RetierOracoolItem(target, OracoolItemTier::Rare))
				return {};
			what = std::string(target.getName());
			break;
		case 10: { // CONSECRATE - a rare becomes a set piece for the same SLOT
			const std::vector<const SetItemDefinition *> pieces = SetPiecesForLoc(target._iLoc, target._iOracoolItemLevel, target._iClass);
			if (pieces.empty())
				return {};
			// The depth the item was FOUND at, captured before InitializeItem wipes it. Audit
			// finding, 2026-08-26: both recipes rebuilt the item and never put the level or the
			// base tier back, so recasting a deep set piece quietly reset it to a floor-zero item.
			const int keptLevel = target._iOracoolItemLevel;
			const auto keptTier = static_cast<oracool::BaseItemTier>(target._iOracoolBaseTier); // kept too (round 34 audit)
			const SetItemDefinition *chosen = pieces[GenerateRnd(static_cast<int32_t>(pieces.size()))];
			const bool wasEthereal = target._iOracoolEthereal;
			// Kanai's Work of Cathan too (audit, 2026-09-29): InitializeItem empties the item, and the keepsake notes name
			// these recipes as ones that must keep it.
			const bool wasLevelFree = target._iOracoolLevelFree;
			// The wear, the broken flag and the empty sockets too (round 8 audit, v1.12.233): the rebuild was a free repair -
			// Mend bypassed - and it deleted sockets bought with Punch Sockets.
			const int oldDurability = target._iDurability;
			const bool wasBroken = target._iOracoolBroken;
			const int oldSockets = target._iSocketCount;
			InitializeItem(target, static_cast<_item_indexes>(BaseItemForSetPiece(*chosen)));
			MakeSetItem(target, *chosen);
			FinalizeSetPiece(target, keptLevel, /*allowEtherealRoll=*/false, keptTier);
			// InitializeItem starts from an empty Item, so the ethereal bargain went with it (audit,
			// 2026-09-13). May decline on an indestructible piece, which then simply stays whole.
			if (wasEthereal)
				MakeItemEthereal(target);
			target._iOracoolLevelFree = wasLevelFree;
			if (oldDurability != DUR_INDESTRUCTIBLE && target._iMaxDur != DUR_INDESTRUCTIBLE)
				target._iDurability = std::min<int>(oldDurability, target._iMaxDur);
			target._iOracoolBroken = wasBroken && target._iDurability == 0;
			target._iSocketCount = static_cast<uint8_t>(std::min(oldSockets, MaxSocketsForItem(target)));
			target._iIdentified = true;
			what = std::string(target.getName());
			break;
		}
		case 11: // AWAKEN - a unique rolled again as a primal
			if (!RetierOracoolItem(target, OracoolItemTier::Primal))
				return {};
			what = std::string(target.getName());
			break;
		case 12: // REROLL RARES - the rare rolls taken again at the same rung
			if (!RetierOracoolItem(target, OracoolItemTier::Rare))
				return {};
			what = std::string(target.getName());
			break;
		case 13: // REROLL UNIQUES
			// A VANILLA unique has fixed powers - rerolling its affixes would return the identical
			// item - so for one of those the reroll re-picks WHICH unique it is. A BuffedUnique
			// tier item genuinely is a roll, so that one is rerolled in place. Two behaviours under
			// one name because a player calls both of them "a unique".
			if (target._iMagical == ITEM_QUALITY_UNIQUE) {
				if (!EnnobleOracoolRare(target))
					return {};
			} else if (!RetierOracoolItem(target, OracoolItemTier::BuffedUnique)) {
				return {};
			}
			// Law of Kulle's rare promotion (2026-09-20): one reroll in fifty rises to Primal instead of staying on its rung.
			if (GenerateRnd(RerollPromotionOneIn) == 0 && RetierOracoolItem(target, OracoolItemTier::Primal)) {
				what = fmt::format(fmt::runtime(_("{:s} - and it rose to Primal!")), std::string(target.getName()));
				break;
			}
			what = std::string(target.getName());
			break;
		case 14: // REROLL PRIMALS
			if (!RetierOracoolItem(target, OracoolItemTier::Primal))
				return {};
			what = std::string(target.getName());
			break;
		case 15: // MAKE ETHEREAL - the bargain, stamped into the item's own numbers
			if (!MakeItemEthereal(target))
				return {};
			what = std::string(target.getName());
			break;
		case 16:
			// MEND - full durability, and it STAYS ethereal. The +35% is kept; what is bought is
			// the removal of the only price ethereal charges, which is why this is the most
			// expensive recipe in the game.
			target._iDurability = target._iMaxDur;
			// And it is no longer BROKEN, which is the whole point and was missing (audit,
			// 2026-08-26). Breaking sets durability to zero AND raises _iOracoolBroken, and the
			// flag is what makes an item statless and unusable - so Mend restored the number,
			// left the flag, and handed back an item that was still dead. The most expensive
			// recipe in the game did nothing observable, and there was no other way out: the smith
			// and the Repair skill both refuse ethereals, which is why Mend exists at all.
			target._iOracoolBroken = false;
			// The character's totals still carry the item as contributing nothing until something
			// recalculates them. Doing it here rather than trusting the caller, for the same reason
			// the aura functions were given that responsibility.
			// With the look reloaded: a mended weapon in the hand counts again, and the hero holds it (2026-09-29).
			CalcPlrInv(*MyPlayer, true);
			what = std::string(target.getName());
			break;
		default:
			return {};
		}

		// Every rebuilding recipe re-derives the item from its base (InitializeItem or SetupAllItems),
		// which re-derives its durability too - whether or not the ledger bytes survived the pass. So
		// the ledger goes back on and Tempering's durability with it, for every recipe except the
		// three that change the item in place (Recolour, the ethereal pair) and Cleanse itself.
		const bool rebuilt = recipe != 8 && recipe != 15 && recipe != 16 && recipe != CleanseShardsRecipe;
		if (rebuilt && ledger.count > 0) {
			RestoreImbuements(target, ledger);
			// The restore adds Tempering to the current durability as well, on top of the wear the rebuild kept: a 30/70
			// item came back 40/70, and a broken one at 10/70 still flagged broken (round 11 audit, v1.12.236). The wear
			// is the item's as it went in, under the new maximum; broken follows it.
			if (target._iMaxDur > 0 && target._iMaxDur != DUR_INDESTRUCTIBLE && target._iDurability != DUR_INDESTRUCTIBLE) {
				target._iDurability = std::min<int>(restore.before._iDurability, target._iMaxDur);
				target._iOracoolBroken = target._iDurability == 0;
			}
		}

		ConsumeGridReagents(grid, reagents, ReagentFor(recipe).count);
		restore.done = true;
		return what;
	}

	// Audit finding, 2026-08-26. Everything below asks "will the result fit once the materials are
	// gone", and both checks used to answer it by pretending each material SLOT empties completely.
	// Consumption is unit-accurate and does not: a stack of five gems paying a cost of three leaves
	// two behind, in the slot the check had already written off.
	//
	// On a full grid that is the difference between a craft and a theft. The preflight sees a free
	// slot that will not exist, the materials are consumed, the output loop finds nowhere to put
	// the result and returns empty-handed - and because the grid it leaves behind is perfectly
	// valid, the caller's rollback sees nothing wrong and does not fire. The player pays three gems
	// for nothing and is told nothing.
	//
	// So the simulation is now the real thing: a copy of the grid with the real consumption run
	// against it. Both checks read the copy, and the copy cannot disagree with what follows,
	// because it was produced by the same function.
	Item consumedGrid[GridSlots];
	std::copy(grid, grid + GridSlots, consumedGrid);
	ConsumeGridReagents(consumedGrid, materials, MaterialUnitCostFor(recipe));

	// SAYS SO, rather than returning empty. Audit finding, 2026-08-26: refusing without consuming
	// anything was the fix, but an empty return means the caller logs nothing, so the Transmute
	// button appeared to do nothing at all - which is the ambiguity this fork has now shipped
	// three times. The other no-room path below already returns this same message.
	if (GridRoomAfter(consumedGrid, {}) < 1)
		return NoRoomForResult();

	_item_indexes output = IDI_NONE;
	switch (recipe) {
	case 0:
		output = static_cast<_item_indexes>(NextGemQuality(static_cast<uint16_t>(grid[materials[0]].IDidx)));
		break;
	case 1:
		output = static_cast<_item_indexes>(NextRune(static_cast<uint16_t>(grid[materials[0]].IDidx)));
		break;
	case 2: {
		constexpr _item_indexes CharmPool[] = {
			IDI_ORACOOL_CHARM_VIGOR, IDI_ORACOOL_CHARM_EMBERS, IDI_ORACOOL_CHARM_STORMS,
			IDI_ORACOOL_CHARM_FORTUNE, IDI_ORACOOL_CHARM_LUCK, IDI_ORACOOL_CHARM_GREED
		};
		output = CharmPool[GenerateRnd(6)];
		break;
	}
	case 4:
		output = static_cast<_item_indexes>(NextJewelGrade(static_cast<uint16_t>(grid[materials[0]].IDidx)));
		break;
	default:
		return {};
	}
	if (output == IDI_NONE)
		return {};

	// The same cell check "Free the Sockets" does, for the same reason - and it is NOT redundant
	// here just because these recipes take more items than they give back. They can still grow in
	// CELLS: Charm of Greed wears ICURS_MAGIC_ROCK, which is two cells wide, so two 1x1 charms can
	// transmute into one 2-cell charm. GridRoomAfter's slot count says "room" to that every time.
	//
	// Checked BEFORE anything is consumed, so a refusal costs the player nothing. The transmute
	// button's rollback would catch it anyway, but a recipe that half-runs and is then undone is a
	// worse thing to rely on than one that declines up front.
	{
		std::vector<Item> after;
		after.reserve(GridSlots + 1);
		for (int i = 0; i < GridSlots; i++) {
			// consumedGrid, not grid: a material slot with surplus units left in it is still
			// occupying a cell, and the packing has to be told about it.
			if (consumedGrid[i].isEmpty())
				continue;
			after.push_back(consumedGrid[i]);
		}
		Item produced;
		InitializeItem(produced, output);
		after.push_back(produced);
		if (!LevskiGridCanHold(after.data(), static_cast<int>(after.size())))
			return NoRoomForResult();
	}

	// Unit-accurate, not slot-accurate. Audit finding, 2026-08-26: this cleared every material SLOT
	// outright, so a stack of five gems paid five for a recipe that costs three and the surplus was
	// destroyed without a word. ConsumeGridReagents already does the arithmetic correctly and has
	// done since it was written for the reagent recipes - the refine path simply never used it.
	//
	// The cost is asked of the same table the MATCH used, so the two halves cannot drift: a recipe
	// that matched on three and consumed two would quietly hand out free crafts, which is the note
	// ReagentFor already carries.
	ConsumeGridReagents(grid, materials, MaterialUnitCostFor(recipe));
	for (int slot = 0; slot < GridSlots; slot++) {
		if (!grid[slot].isEmpty())
			continue;
		InitializeItem(grid[slot], output);
		GenerateNewSeed(grid[slot]);
		grid[slot]._iIdentified = true;
		return std::string(_(AllItemsList[output].iName));
	}
	return {};
}
} // namespace devilution::oracool
