#include "oracool/shop_tabs.h"

#include "control.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "multi.h"
#include "oracool/ornate_border.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

constexpr int StripHeight = 21;
/** @brief The store panel's own left and right edges - see DrawSTextBack and CheckStoreBtn. */
constexpr int PanelLeft = 24;
constexpr int PanelRight = 616;
/** @brief Above the panel, which starts at y+28. This is the gap that lets the strip cost no reflow. */
constexpr int StripTop = 5;

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

Rectangle GetShopTabStripRect()
{
	if (!IsShopTab(stextflag))
		return Rectangle { { 0, 0 }, { 0, 0 } };
	const Point ui = GetUIRectangle().position;
	return Rectangle { { ui.x + PanelLeft, ui.y + StripTop }, { PanelRight - PanelLeft, StripHeight } };
}

namespace {

/** @brief One tab's rect. Widths are equal and derived, so a vendor with six tabs still fits. */
Rectangle TabRect(const Rectangle &strip, size_t index, size_t count)
{
	if (count == 0)
		return Rectangle { { 0, 0 }, { 0, 0 } };
	const int width = strip.size.width / static_cast<int>(count);
	return Rectangle { { strip.position.x + static_cast<int>(index) * width, strip.position.y },
		{ width, strip.size.height } };
}

} // namespace

void DrawShopTabs(const Surface &out)
{
	const Rectangle strip = GetShopTabStripRect();
	if (strip.size.width <= 0)
		return;
	const std::vector<TalkID> tabs = ShopTabsFor(stextflag);
	if (tabs.empty())
		return;

	for (size_t i = 0; i < tabs.size(); i++) {
		const Rectangle rect = TabRect(strip, i, tabs.size());
		const bool active = tabs[i] == stextflag;
		// The ACTIVE tab is filled and white; the rest are bordered and dim. One cue, not two - the
		// player has to be able to tell where they are at a glance while the panel below is dense
		// with item text.
		if (active) {
			FillRect(out, rect.position.x + 1, rect.position.y + 1,
			    rect.size.width - 2, rect.size.height - 2, PAL8_YELLOW + 6);
		} else {
			DrawHalfTransparentRectTo(out, rect.position.x + 1, rect.position.y + 1,
			    rect.size.width - 2, rect.size.height - 2);
		}
		DrawOrnateBorder(out, rect);
		DrawString(out, _(ShopTabName(tabs[i])), rect,
		    { (active ? UiFlags::ColorWhite : UiFlags::ColorWhitegold)
		        | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}
}

bool CheckShopTabClick(Point position)
{
	const Rectangle strip = GetShopTabStripRect();
	if (strip.size.width <= 0 || !strip.contains(position))
		return false;
	const std::vector<TalkID> tabs = ShopTabsFor(stextflag);
	for (size_t i = 0; i < tabs.size(); i++) {
		if (!TabRect(strip, i, tabs.size()).contains(position))
			continue;
		// The tab you are already on absorbs the click rather than restarting the screen - a
		// restart would reset the scroll position under the player's cursor for no reason.
		if (tabs[i] != stextflag)
			StartStore(tabs[i]);
		return true;
	}
	// Inside the strip but between two tabs: absorbed, not passed through. The strip is a control
	// surface, and a click that falls in a one-pixel gap should do nothing rather than reach
	// whatever happens to be under it.
	return true;
}

} // namespace devilution::oracool
