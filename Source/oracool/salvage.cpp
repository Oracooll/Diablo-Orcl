#include "oracool/salvage.h"

#include <string>
#include <vector>

#include "inv.h"
#include "items.h"
#include "player.h"
#include "oracool/charms.h"
#include "oracool/event_log.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "DiabloUI/ui_flags.hpp"

namespace devilution::oracool {

namespace {

/**
 * @brief The material each bucket yields, in SalvageTier order.
 *
 * One row per tier rather than a switch, so the table and the enum are read side by side and a
 * missing case is a missing row - visible - rather than a silent fall-through to White Scales.
 */
constexpr uint16_t TierMaterial[SalvageTierCount] = {
	IDI_ORACOOL_SALVAGE_WHITE_SCALES,
	IDI_ORACOOL_SALVAGE_MAGIC_POWDER,
	IDI_ORACOOL_SALVAGE_RARE_FIBRES,
	IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS,
	IDI_ORACOOL_SALVAGE_PRIMAL_VINES,
	IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS,
	IDI_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES,
};

/** @brief The Charm of Salvaging that arms each bucket. Same one-row-per-tier rule as above. */
constexpr uint16_t TierCharm[SalvageTierCount] = {
	IDI_ORACOOL_CHARM_SALVAGE_WHITE_SCALES,
	IDI_ORACOOL_CHARM_SALVAGE_MAGIC_POWDER,
	IDI_ORACOOL_CHARM_SALVAGE_RARE_FIBRES,
	IDI_ORACOOL_CHARM_SALVAGE_UNIQUE_ENCRUSTMENTS,
	IDI_ORACOOL_CHARM_SALVAGE_PRIMAL_VINES,
	IDI_ORACOOL_CHARM_SALVAGE_SET_ENGRAVINGS,
	IDI_ORACOOL_CHARM_SALVAGE_ETHEREAL_IMBUEITIES,
};

constexpr const char *TierName[SalvageTierCount] = {
	N_("Whites"), N_("Magic"), N_("Rare"), N_("Uniques"), N_("Primal"), N_("Set"), N_("Ethereal"),
};

/** @brief How many materials each bucket gives. Deeper tiers give more, so salvage keeps pace. */
constexpr int TierYield[SalvageTierCount] = { 1, 1, 2, 3, 4, 3, 2 };

} // namespace

const char *SalvageTierName(SalvageTier tier)
{
	return TierName[static_cast<int>(tier)];
}

uint16_t SalvageMaterialFor(SalvageTier tier)
{
	return TierMaterial[static_cast<int>(tier)];
}

bool IsSalvageable(const Item &item)
{
	if (item.isEmpty())
		return false;
	// Everything that is not gear declines. A salvage-all that could eat a stack of runes because
	// the wrong button was pressed is not worth having, and the socketable families are exactly the
	// things a player is hoarding for the crafting system this feeds.
	if (item._itype == ItemType::Gold)
		return false;
	if (item._iClass == ICLASS_QUEST)
		return false;
	if (IsOracoolGemIdx(item.IDidx) || IsOracoolRuneIdx(item.IDidx) || IsOracoolSalvageIdx(item.IDidx)
	    || IsOracoolJewelIdx(item.IDidx))
		return false;
	// JEWELLERY, tested before the ICLASS_MISC rejection below because it IS ICLASS_MISC (user
	// report, 2026-08-20: "check why salvaging skips jewelry").
	//
	// Rings and amulets carry ICLASS_MISC in AllItemsList - vanilla files them with the oddments
	// rather than with the armour - so the blanket "misc is not gear" rule swept up every ring and
	// amulet in the game. Their equip LOCATION is what actually says they are worn, and it is the
	// same test the socket system already uses to let a ring take a gem, so this fork was already
	// treating them as first-class equipment everywhere except here.
	// A quest's own reward is never salvage (round 40 audit: a Uniques charm turned Arkaine's Valor and the Butcher's Cleaver
	// into Encrustments on pickup). Named, not by IDROP_NEVER: the fork's set carriers are never-drop bases too.
	for (const _item_indexes quest : { IDI_CLEAVER, IDI_SKCROWN, IDI_HARCREST, IDI_STEELVEIL, IDI_ARMOFVAL, IDI_GRISWOLD, IDI_LGTFORGE })
		if (item.IDidx == quest)
			return false;
	if (item._iLoc == ILOC_RING || item._iLoc == ILOC_AMULET)
		return AllItemsList[item.IDidx].iRnd != IDROP_NEVER; // not a quest's reward: the Auric and Optic Amulets, the Empyrean Band, the Ring of Truth (round 36)
	if (item._iClass == ICLASS_MISC)
		return false;
	// Weapons and armour, which is what is left.
	return item._iClass == ICLASS_WEAPON || item._iClass == ICLASS_ARMOR;
}

SalvageTier SalvageTierOf(const Item &item)
{
	// Most specific first - see the header. Ethereal wins over everything, because an ethereal item
	// is an ethereal item whatever else it also is, and because the alternative is two buttons
	// claiming the same object.
	if (item._iOracoolEthereal)
		return SalvageTier::Ethereal;
	if (item.hasOracoolTier()) {
		switch (item._iOracoolTier) {
		case OracoolItemTier::Set:
			return SalvageTier::Set;
		case OracoolItemTier::Primal:
			return SalvageTier::Primal;
		case OracoolItemTier::BuffedUnique:
			return SalvageTier::Unique;
		case OracoolItemTier::Rare:
			return SalvageTier::Rare;
		default:
			break;
		}
	}
	if (item._iMagical == ITEM_QUALITY_UNIQUE)
		return SalvageTier::Unique;
	if (item._iMagical == ITEM_QUALITY_MAGIC)
		return SalvageTier::Magic;
	return SalvageTier::White;
}

/**
 * @brief Whether @p item is what the @p tier button is asking for, protections included.
 *
 * One predicate for both the collect pass and the "is the button live" test. They used to spell the
 * same condition out separately, which is two places for a protection to be added to only one of -
 * and a button that lights up over stock it then refuses to take is worse than one that never lit.
 */
bool SalvageMatches(const Item &item, SalvageTier tier)
{
	if (!IsSalvageable(item) || SalvageTierOf(item) != tier)
		return false;
	// A SOCKETED WHITE IS NOT SCRAP (user, 2026-08-28: "charm of salvaging white to ignore socket
	// whites. they are precious. we dont want them destroyed").
	//
	// It is the one thing a runeword can be built in, and the white charm is the button most likely
	// to be pressed without looking - a backpack full of grey drops is exactly what it exists to
	// clear, and the socketed base sitting among them is the single item in that pile worth more
	// than the rest together.
	//
	// Scoped to White, as asked. The higher tiers are not swept blind in the same way, and an
	// unasked-for protection on them would quietly change what those buttons do.
	if (tier == SalvageTier::White && item._iSocketCount > 0)
		return false;
	// FILLED sockets at every tier: the empty-socket rule above is the user's, White only; stones are another matter - a
	// punched Rare full of runes was salvaged with every stone in it, and a salvage charm ate one on pickup (round 24).
	if (item.socketedCount() > 0)
		return false;
	return true;
}

int SalvageYield(const Item &item)
{
	return TierYield[static_cast<int>(SalvageTierOf(item))];
}

int SalvageAllInBackpack(Player &player, SalvageTier tier, int *materialsMade)
{
	if (materialsMade != nullptr)
		*materialsMade = 0;
	// EVERY page, not just the one on screen. User report, 2026-08-20: "Salvage buttons to sweet
	// all tabs." The original walk used the GetActive* helpers, which read whichever tab is
	// displayed - so a button pressed on page 1 left the rares on pages 2-10 untouched, and the
	// only way to notice was to page through afterwards.
	//
	// Two passes, and the split matters. Removing from a list compacts it - every index after the
	// removed one shifts down - so collecting first and removing afterwards, highest index first,
	// is what keeps the walk from skipping items. Tab -1 is the main backpack.
	struct Victim {
		int tab;
		int index;
	};
	std::vector<Victim> victims;
	int materials = 0;

	const auto collect = [&](int tab, const Item *list, int count) {
		for (int i = 0; i < count; i++) {
			if (!SalvageMatches(list[i], tier))
				continue;
			victims.push_back({ tab, i });
			materials += SalvageYield(list[i]);
		}
	};
	collect(-1, player.InvList, player._pNumInv);
	for (int tab = 0; tab < Player::NumExtraInventoryTabs; tab++)
		collect(tab, player.InvTabList[tab].data(), player._pNumInvTab[tab]);

	if (victims.empty())
		return 0;

	// Reverse order removes the highest index of each list first. The vector is built tab by tab
	// and ascending within a tab, so walking it backwards satisfies that for every tab at once.
	for (auto it = victims.rbegin(); it != victims.rend(); ++it) {
		if (it->tab < 0)
			player.RemoveInvItem(it->index, false);
		else
			RemoveExtraTabItem(player, it->tab, it->index);
	}

	// The materials go back into the space the gear just vacated, so a pack that was full enough to
	// need salvaging always has room for what salvaging produced. They stack, so a hundred White
	// Scales occupy one cell.
	Item material;
	InitializeItem(material, static_cast<_item_indexes>(SalvageMaterialFor(tier)));
	// Without this the material renders through the infravision TRN - solid RED - until the next
	// inventory action happens to run CalcPlrInv and set the flag. DrawItem picks the palette off
	// _iStatFlag alone, and InitializeItem leaves it false. User report, 2026-08-20: "all of them
	// spawn red then recolor after some interaction with inv."
	material.updateRequiredStatsCacheForPlayer(player);
	//
	// Overflow is close to impossible and is NOT silently swallowed. Each salvaged item frees at
	// least one cell, and the materials of one tier merge into a single stack until they pass 99 -
	// so running out needs a single press to yield more than 99 materials while freeing almost no
	// space. If it ever does happen the loss is real, so it says so in the log rather than leaving
	// the player to notice a short count.
	int placed = 0;
	for (; placed < materials; placed++) {
		Item one = material;
		if (!AutoPlaceItemInInventory(player, one, true))
			break;
	}
	if (placed < materials) {
		LogEvent(StrCat("Salvage: no room for ", materials - placed, " ",
		             _(AllItemsList[SalvageMaterialFor(tier)].iName), " - they were lost"),
		    UiFlags::ColorRed);
	}
	if (materialsMade != nullptr)
		*materialsMade = placed;

	return static_cast<int>(victims.size());
}

bool SalvageSingleItem(Player &player, int tab, int index, SalvageTier *tierOut, int *materialsOut)
{
	if (index < 0)
		return false;
	Item *item = nullptr;
	if (tab < 0) {
		if (index >= player._pNumInv)
			return false;
		item = &player.InvList[index];
	} else {
		if (tab >= Player::NumExtraInventoryTabs || index >= player._pNumInvTab[tab])
			return false;
		item = &player.InvTabList[tab][index];
	}
	// Nor one with stones in it, one at a time either: the plates refuse it since round 24, and the hand-picked salvage
	// ate a Rare's runes with it (round 25 audit). Free the Sockets first.
	if (!IsSalvageable(*item) || item->socketedCount() > 0)
		return false;
	const SalvageTier tier = SalvageTierOf(*item);
	const int materials = SalvageYield(*item);
	if (tierOut != nullptr)
		*tierOut = tier;

	// The item goes first: it frees at least one cell, which is what guarantees the materials have
	// somewhere to land - the same order SalvageAllInBackpack takes for the same reason.
	if (tab < 0)
		player.RemoveInvItem(index, false);
	else
		RemoveExtraTabItem(player, tab, index);

	Item material;
	InitializeItem(material, static_cast<_item_indexes>(SalvageMaterialFor(tier)));
	material.updateRequiredStatsCacheForPlayer(player);
	int placed = 0;
	for (; placed < materials; placed++) {
		Item one = material;
		if (!AutoPlaceItemInInventory(player, one, true))
			break;
	}
	if (placed < materials) {
		LogEvent(StrCat("Salvage: no room for ", materials - placed, " ",
		             _(AllItemsList[SalvageMaterialFor(tier)].iName), " - they were lost"),
		    UiFlags::ColorRed);
	}
	if (materialsOut != nullptr)
		*materialsOut = placed;
	return true;
}

bool AnySalvageableInBackpack(const Player &player, SalvageTier tier)
{
	const auto anyIn = [&](const Item *list, int count) {
		for (int i = 0; i < count; i++) {
			if (SalvageMatches(list[i], tier))
				return true;
		}
		return false;
	};
	if (anyIn(player.InvList, player._pNumInv))
		return true;
	for (int tab = 0; tab < Player::NumExtraInventoryTabs; tab++) {
		if (anyIn(player.InvTabList[tab].data(), player._pNumInvTab[tab]))
			return true;
	}
	return false;
}

uint16_t SalvageCharmFor(SalvageTier tier)
{
	return TierCharm[static_cast<int>(tier)];
}

SalvageTier SalvageTierOfCharm(uint16_t charmIdx)
{
	for (int i = 0; i < SalvageTierCount; i++) {
		if (TierCharm[i] == charmIdx)
			return static_cast<SalvageTier>(i);
	}
	return SalvageTier::White;
}

const char *SalvageCharmEffectLine(uint16_t charmIdx)
{
	if (!IsOracoolSalvageCharmIdx(charmIdx))
		return nullptr;
	// One fixed string per tier rather than a formatted one, because this is returned as a const
	// char * and the tier names are already translatable literals.
	static const char *const Lines[SalvageTierCount] = {
		N_("picked-up white items become White Scales"),
		N_("picked-up magic items become Magic Powder"),
		N_("picked-up rare items become Rare Fibres"),
		N_("picked-up unique items become Unique Encrustments"),
		N_("picked-up primal items become Primal Vines"),
		N_("picked-up set items become Set Engravings"),
		N_("picked-up ethereal items become Ethereal Imbueities"),
	};
	return Lines[static_cast<int>(SalvageTierOfCharm(charmIdx))];
}

bool TrySalvageOnPickup(Player &player, const Item &item)
{
	if (!IsSalvageable(item))
		return false;

	// Which tiers are armed right now. ForEachActiveCharm is the SAME walk the stat sheet uses, so
	// "the first three charms in reading order" means exactly one thing across the whole fork - a
	// salvage charm sitting fourth is as dead as a stat charm sitting fourth, and the item popup
	// already says so.
	bool armed[SalvageTierCount] = {};
	ForEachActiveCharm(
	    player, [](uint16_t charmIdx, void *context) {
		    if (IsOracoolSalvageCharmIdx(charmIdx))
			    static_cast<bool *>(context)[static_cast<int>(SalvageTierOfCharm(charmIdx))] = true;
	    },
	    armed);

	const SalvageTier tier = SalvageTierOf(item);
	if (!armed[static_cast<int>(tier)])
		return false;
	// THE SAME PROTECTIONS THE BUTTONS OBEY (user, 2026-08-30: "again - make whites with sockets
	// immune to turning them into white scales automatically. i think i told you to do it already").
	//
	// They had. v1.9.101 put the socketed-white rule into SalvageMatches and pointed the Levski
	// buttons at it - both of them, the collect pass and the is-the-button-live test - and stopped
	// there. This path never went through it: it asks IsSalvageable and SalvageTierOf itself, so it
	// kept its own older idea of what counts, and it is the path that fires WITHOUT the player
	// pressing anything. So the protection worked exactly where it was visible and failed where it
	// mattered most.
	//
	// Asked here rather than by widening IsSalvageable, because "is this gear at all" and "may this
	// button take it" are different questions - the first is about the item, the second about the
	// rule the player bought.
	if (!SalvageMatches(item, tier))
		return false;

	// The name is read BEFORE anything is destroyed, because the log line is the only trace the
	// player gets - they never see the item itself.
	const std::string consumed = std::string(item.getName());

	const int yield = SalvageYield(item);
	Item material;
	InitializeItem(material, static_cast<_item_indexes>(SalvageMaterialFor(tier)));
	material.updateRequiredStatsCacheForPlayer(player); // see the same call in SalvageAllInBackpack
	// ALL of it, or none. This placed one unit at a time and stopped at the first refusal, declining
	// only when NOTHING fit - so a pack with room for two of a five-material yield took the two,
	// destroyed the gear, and logged the shortfall as if losing it were a normal outcome (external
	// audit, 2026-08-25). It is not: the item is gone and cannot be re-salvaged later.
	//
	// Placed as one STACK rather than in a loop, which is both what these are (salvage materials
	// stack - see Item::isStackableConsumable) and what makes the all-or-nothing check possible:
	// probing a single unit N times just answers "the first one fits" N times, because a probe
	// reserves nothing.
	material.setStackCount(yield);
	if (!AutoPlaceItemInInventory(player, material, /*persistItem=*/false))
		return false;
	AutoPlaceItemInInventory(player, material, /*persistItem=*/true);

	LogEvent(StrCat(_("Salvaged on pickup: "), consumed, " -> ", yield, " ",
	             _(AllItemsList[SalvageMaterialFor(tier)].iName)),
	    UiFlags::ColorWhitegold);
	return true;
}

} // namespace devilution::oracool
