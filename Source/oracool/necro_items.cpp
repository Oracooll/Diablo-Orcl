/**
 * @file oracool/necro_items.cpp
 *
 * See necro_items.h.
 */
#include "oracool/necro_items.h"

#include "itemdat.h"
#include "items.h"
#include "oracool/class_tree.h"
#include "oracool/passives.h"
#include "player.h"

namespace devilution::oracool {

bool IsNecroWandIdx(int idx)
{
	return idx >= IDI_ORACOOL_NECRO_WAND_FIRST && idx <= IDI_ORACOOL_NECRO_WAND_LAST;
}

bool IsNecroScytheIdx(int idx)
{
	return idx >= IDI_ORACOOL_NECRO_SCYTHE_FIRST && idx <= IDI_ORACOOL_NECRO_SCYTHE_LAST;
}

bool IsNecroHeadIdx(int idx)
{
	return idx >= IDI_ORACOOL_NECRO_HEAD_FIRST && idx <= IDI_ORACOOL_NECRO_HEAD_LAST;
}

bool IsNecroBaseIdx(int idx)
{
	return idx >= IDI_ORACOOL_NECRO_WAND_FIRST && idx <= IDI_ORACOOL_NECRO_HEAD_LAST;
}

bool IsNecroHeadItem(const Item &item)
{
	return !item.isEmpty() && IsNecroHeadIdx(item.IDidx);
}

bool ClassMayUseItem(const Player &player, const Item &item)
{
	if (IsNecroHeadItem(item))
		return player._pClass == HeroClass::Necromancer;
	return true;
}

bool SwiftHarvestingApplies(const Player &player)
{
	if (!PassiveActive(player, ClassTreeSkill::SwiftHarvesting))
		return false;
	for (const Item &hand : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (!hand.isEmpty() && hand._iStatFlag && (IsNecroWandIdx(hand.IDidx) || IsNecroScytheIdx(hand.IDidx)))
			return true;
	}
	return false;
}

bool NecroHeadsMayDrop()
{
	return MyPlayer != nullptr && MyPlayer->_pClass == HeroClass::Necromancer;
}

} // namespace devilution::oracool
