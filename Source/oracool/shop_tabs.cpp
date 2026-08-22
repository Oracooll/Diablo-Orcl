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
		return N_("Fine");
	case TalkID::SmithUniqueBuy:
		return N_("Unique");
	case TalkID::SmithConsumables:
	case TalkID::WitchBuy:
	case TalkID::HealerBuy:
		return N_("Supplies");
	case TalkID::SmithSell:
	case TalkID::WitchSell:
		return N_("Sell");
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
		if (HasSmithUniqueShop())
			tabs.push_back(TalkID::SmithUniqueBuy);
		if (!gbIsMultiplayer)
			tabs.push_back(TalkID::SmithConsumables);
		tabs.push_back(TalkID::SmithSell);
		tabs.push_back(TalkID::SmithRepair);
		if (!gbIsMultiplayer)
			tabs.push_back(TalkID::SmithRecharge);
		return tabs;
	}
	case ShopKind::Witch:
		return { TalkID::WitchBuy, TalkID::WitchSell, TalkID::WitchRecharge };
	case ShopKind::Healer:
		return { TalkID::HealerBuy };
	case ShopKind::None:
		break;
	}
	return {};
}

} // namespace devilution::oracool
