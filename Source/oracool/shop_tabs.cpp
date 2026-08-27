#include "oracool/shop_tabs.h"

#include "multi.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

/** @brief Which vendor a tab belongs to, so a tab strip only ever offers one shop's tabs. */
enum class ShopKind : uint8_t {
	None,
	Smith,
	Witch,
	Healer,
};

ShopKind KindOf(TalkID id)
{
	switch (id) {
	case TalkID::SmithBuy:
	case TalkID::SmithPremiumBuy:
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithSetBuy:
	case TalkID::SmithConsumables:
	case TalkID::SmithSell:
	case TalkID::SmithRepair:
	case TalkID::SmithRecharge:
		return ShopKind::Smith;
	case TalkID::WitchBuy:
	case TalkID::WitchSell:
	case TalkID::WitchRecharge:
		return ShopKind::Witch;
	case TalkID::HealerBuy:
		return ShopKind::Healer;
	default:
		return ShopKind::None;
	}
}

} // namespace

bool IsShopTab(TalkID id)
{
	return KindOf(id) != ShopKind::None;
}

const char *ShopTabName(TalkID id)
{
	switch (id) {
	case TalkID::SmithBuy:
		return N_("Basic");
	case TalkID::SmithPremiumBuy:
		return N_("Magic");
	case TalkID::SmithUniqueBuy:
		return N_("Unique");
	case TalkID::SmithRareBuy:
		return N_("Rare");
	case TalkID::SmithSetBuy:
		return N_("Set");
	case TalkID::SmithConsumables:
	case TalkID::WitchBuy:
	case TalkID::HealerBuy:
		return N_("Supplies");
	case TalkID::SmithSell:
	case TalkID::WitchSell:
		// "Sold", not "Sell": selling is a drag onto the panel, and this tab is the record of what
		// the vendor already bought, offered back at the price they paid.
		return N_("Sold");
	case TalkID::SmithRepair:
		return N_("Repair");
	case TalkID::SmithRecharge:
	case TalkID::WitchRecharge:
		return N_("Recharge");
	default:
		return "";
	}
}

std::vector<TalkID> ShopTabsFor(TalkID id)
{
	// The tab sets mirror the menus these replaced, minus the entries that were never a shop screen
	// - gossip and leave stay on the vendor's dialog, because one is a conversation and the other
	// is the way out of the building.
	//
	// The multiplayer exclusions are the SAME ones SmithMenuEntries applied. V1 is single-player, so
	// they are inert today; they are kept because a tab that appears and then refuses is worse than
	// a tab that is not offered.
	switch (KindOf(id)) {
	case ShopKind::Smith: {
		std::vector<TalkID> tabs { TalkID::SmithBuy, TalkID::SmithPremiumBuy };
		// The three curated shelves, each behind its own INI switch. In quality order after the two
		// rolled shelves, so the strip reads as a ladder rather than as the order they were built.
		//
		// CuratedShelfHasStock, not HasCuratedShelf: the switch says whether the shelf is WANTED,
		// and this says whether there is anything on it. A tab over an empty shelf bounces the
		// player back to Griswold's dialog, which is what the Set tab did for every character below
		// level 18 on the day it shipped.
		if (CuratedShelfHasStock(CuratedShelf::Rare))
			tabs.push_back(TalkID::SmithRareBuy);
		if (CuratedShelfHasStock(CuratedShelf::Set))
			tabs.push_back(TalkID::SmithSetBuy);
		if (CuratedShelfHasStock(CuratedShelf::Unique))
			tabs.push_back(TalkID::SmithUniqueBuy);
		if (!gbIsMultiplayer)
			tabs.push_back(TalkID::SmithConsumables);
		tabs.push_back(TalkID::SmithSell);
		// Repair and Recharge are no longer tabs. They are icon buttons on every one of this
		// vendor's tabs (user request, 2026-08-23), because they are services performed on an item
		// you already have rather than screens with their own stock to browse - you drop the item
		// on the button. TalkID::SmithRepair and SmithRecharge still exist as screens; nothing
		// routes to them any more.
		return tabs;
	}
	case ShopKind::Witch:
		return { TalkID::WitchBuy, TalkID::WitchSell };
	case ShopKind::Healer:
		return { TalkID::HealerBuy };
	case ShopKind::None:
		break;
	}
	return {};
}

} // namespace devilution::oracool
