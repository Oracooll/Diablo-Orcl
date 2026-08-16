#include "oracool/charms.h"

#include <fmt/format.h>

#include "items.h"
#include "oracool/stat_sheet.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

struct CharmData {
	uint16_t idx;
	int hitPoints; // whole HP
	int fireRes;
	int lightningRes;
	int toHit;
	int magicFind;
	int goldFind;
};

constexpr CharmData Charms[] = {
	{ IDI_ORACOOL_CHARM_VIGOR, 20, 0, 0, 0, 0, 0 },
	{ IDI_ORACOOL_CHARM_EMBERS, 0, 15, 0, 0, 0, 0 },
	{ IDI_ORACOOL_CHARM_STORMS, 0, 0, 15, 0, 0, 0 },
	{ IDI_ORACOOL_CHARM_FORTUNE, 0, 0, 0, 12, 0, 0 },
	{ IDI_ORACOOL_CHARM_LUCK, 0, 0, 0, 0, 15, 0 },
	{ IDI_ORACOOL_CHARM_GREED, 0, 0, 0, 0, 0, 30 },
};

const CharmData *FindCharm(uint16_t charmIdx)
{
	for (const CharmData &charm : Charms) {
		if (charm.idx == charmIdx)
			return &charm;
	}
	return nullptr;
}

} // namespace

void ApplyCharmToTotals(uint16_t charmIdx, ItemBonusTotals &totals)
{
	const CharmData *charm = FindCharm(charmIdx);
	if (charm == nullptr)
		return;
	totals.hitPoints += charm->hitPoints << 6;
	totals.fireResist += charm->fireRes;
	totals.lightningResist += charm->lightningRes;
	totals.bonusToHit += charm->toHit;
	totals.magicFind += charm->magicFind;
	totals.goldFind += charm->goldFind;
}

std::string CharmEffectLine(uint16_t charmIdx)
{
	const CharmData *charm = FindCharm(charmIdx);
	if (charm == nullptr)
		return {};
	if (charm->hitPoints > 0)
		return fmt::format(fmt::runtime(_("+{:d} life while in your backpack")), charm->hitPoints);
	if (charm->fireRes > 0)
		return fmt::format(fmt::runtime(_("+{:d}% fire resist while in your backpack")), charm->fireRes);
	if (charm->lightningRes > 0)
		return fmt::format(fmt::runtime(_("+{:d}% lightning resist while in your backpack")), charm->lightningRes);
	if (charm->magicFind > 0)
		return fmt::format(fmt::runtime(_("{:d}% chance a plain drop upgrades to Rare")), charm->magicFind);
	if (charm->goldFind > 0)
		return fmt::format(fmt::runtime(_("+{:d}% gold from monsters")), charm->goldFind);
	return fmt::format(fmt::runtime(_("+{:d}% to hit while in your backpack")), charm->toHit);
}

void ForEachActiveCharm(const Player &player, void (*visit)(uint16_t charmIdx, void *context), void *context)
{
	int live = 0;
	// Reading order: the main backpack first, then the extra tabs in order - the same order the
	// player sees pages, so "which three are live" is answerable by looking.
	for (int i = 0; i < player._pNumInv && live < CharmActiveCap; i++) {
		if (IsOracoolCharmIdx(player.InvList[i].IDidx) && !player.InvList[i].isEmpty()) {
			visit(static_cast<uint16_t>(player.InvList[i].IDidx), context);
			live++;
		}
	}
	for (int tab = 0; tab < Player::NumExtraInventoryTabs && live < CharmActiveCap; tab++) {
		for (int i = 0; i < player._pNumInvTab[tab] && live < CharmActiveCap; i++) {
			const Item &item = player.InvTabList[tab][i];
			if (IsOracoolCharmIdx(item.IDidx) && !item.isEmpty()) {
				visit(static_cast<uint16_t>(item.IDidx), context);
				live++;
			}
		}
	}
}

bool IsCharmActive(const Player &player, int tabIndex, int invListIndex)
{
	int live = 0;
	for (int i = 0; i < player._pNumInv; i++) {
		if (!IsOracoolCharmIdx(player.InvList[i].IDidx) || player.InvList[i].isEmpty())
			continue;
		if (tabIndex < 0 && i == invListIndex)
			return live < CharmActiveCap;
		live++;
	}
	for (int tab = 0; tab < Player::NumExtraInventoryTabs; tab++) {
		for (int i = 0; i < player._pNumInvTab[tab]; i++) {
			const Item &item = player.InvTabList[tab][i];
			if (!IsOracoolCharmIdx(item.IDidx) || item.isEmpty())
				continue;
			if (tab == tabIndex && i == invListIndex)
				return live < CharmActiveCap;
			live++;
		}
	}
	return false;
}

} // namespace devilution::oracool
