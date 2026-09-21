/**
 * @file stores.h
 *
 * Interface of functionality for stores and towner dialogs.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <SDL.h>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "engine.h"
#include "engine/clx_sprite.hpp"
#include "utils/attributes.h"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

// Oracool (user request, 2026-08-23: "fill the basic, magic, and supplies shops full of items").
// The shop is a 10x16 grid now - 160 cells - and vanilla's stock counts were sized for a text list
// that showed four rows at a time. At an average of four cells an item, filling the grid takes
// roughly forty, so these are the counts the grid can actually hold rather than the counts the old
// list could scroll through.
// Adria's shelf is sized to FILL the grid rather than to a count that looked generous (user,
// 2026-08-27: "adria shop to fill as much as it can the 10x16 grid"). The grid is 160 cells and her
// wares are small - a potion or a rune is one cell, a book four, a staff six - so forty-five items
// left most of it empty. Ninety over-supplies it on purpose: PlaceStock lays out what fits and the
// remainder was never on the shelf (see its note), so the shelf ends where the page does.
#define WITCH_ITEMS 90
#define SMITH_ITEMS 45
#define SMITH_PREMIUM_ITEMS 30
#define STORE_LINES 104

enum class TalkID : uint8_t {
	None,
	Smith,
	SmithBuy,
	SmithSell,
	SmithRepair,
	SmithUniqueBuy,
	SmithRareBuy,
	SmithSetBuy,
	SmithConsumables,
	SmithRecharge,
	Witch,
	WitchBuy,
	WitchSell,
	WitchRecharge,
	NoMoney,
	NoRoom,
	Confirm,
	Boy,
	BoyBuy,
	Healer,
	Storyteller,
	HealerBuy,
	StorytellerIdentify,
	SmithPremiumBuy,
	Gossip,
	StorytellerIdentifyShow,
	Tavern,
	Drunk,
	Barmaid,
	// Oracool 2026-09-20 (Levski's Cube, decision D8): Griswold's Forge tab - the gear recipes the
	// Roar used to hold, opened in the transmute window rather than as a store screen.
	SmithTransmute,
	// Oracool 2026-09-20: Wirt's Gamble tab - unidentified bases by slot, rolled on the purchase
	// (BoyBuy is his Shop tab now, a grid of what he has to sell).
	BoyGamble,
};

/** Currently active store */
extern DVL_API_FOR_TEST TalkID stextflag;

/**
 * @brief How many items the sell/repair/recharge screens can hold at once.
 *
 * Named because it was the literal 48 in five places - the two array bounds and three separate
 * caller-side guards - and a bound that lives in the callers is a bound that a sixth caller does not
 * have. See AddStoreHoldRecharge, which now checks it itself.
 */
constexpr int StoreHoldCapacity = 48;

/** Current index into storehidx/storehold */
extern DVL_API_FOR_TEST int storenumh;
/** Map of inventory items being presented in the store */
extern int8_t storehidx[StoreHoldCapacity];
/** Copies of the players items as presented in the store */
extern DVL_API_FOR_TEST Item storehold[StoreHoldCapacity];

/** Simulates confirming "identify which item?" on storehold[index], exactly as the real UI would. */
DVL_API_FOR_TEST void SimulateStorytellerIdentifyForTest(size_t index);

/** Items sold by Griswold */
extern DVL_API_FOR_TEST Item smithitem[SMITH_ITEMS];
/** Number of premium items for sale by Griswold */
extern DVL_API_FOR_TEST int numpremium;
/** Base level of current premium items sold by Griswold */
extern int premiumlevel;
/** Premium items sold by Griswold */
extern DVL_API_FOR_TEST Item premiumitems[SMITH_PREMIUM_ITEMS];

/** Items sold by Pepin */
extern DVL_API_FOR_TEST Item healitem[20];

/** Items sold by Adria */
extern DVL_API_FOR_TEST Item witchitem[WITCH_ITEMS];

size_t GetSmithConsumablesStockCountForTest();
item_misc_id GetSmithConsumablesStockMiscIdForTest(size_t index);
bool IsSmithConsumablesStockFromPepinForTest(size_t index);
/** @brief Test surface for the Sell All click-routing regression - see stores.cpp. */
int GetSellAllLineForTest();
int GetPremiumRefreshLineForTest();
bool StoreLineHasTextForTest(int line);
void SetStoreSelectionForTest(int line);
void RescrollStoreForTest();
/** @brief Which line a click at @p mouseX on Back's shared row actually targets (Back itself, or
 * one of the border-hugging buttons: Refresh/Refresh Until/Repair all/Sell all). Exported so the
 * routing is directly testable - see stores.cpp for the bug that made that necessary. */
int ResolveBackRowClickLine(int mouseX, int uiLeft);
void UpdateSmithConsumablesStockAfterPurchaseForTest(size_t index);
/** Simulates clicking a SmithConsumables item and confirming the purchase, exactly as the real UI would. Returns whether the confirm screen was reached (false = probe reported no room). */
bool SimulateSmithConsumablesPurchaseForTest(size_t combinedIndex);
/** Sets the held store selection to visible row @p selectedIndex and runs the premium-buy
 * completion, exactly as ConfirmEnter would. Exposed so the audit regression tests can pin the
 * stale-row guard: a selection past the live premium stock must charge nothing, place nothing and
 * clear nothing (self-audit, 2026-08-15). */
void SimulateSmithPremiumBuyForTest(int selectedIndex, Item &item);

/**
 * @brief Test hook: the sale-fit gate, at the EXACT price a sale would credit.
 *
 * @param itemFreeingCells the item whose backpack cells the sale vacates, or nullptr for a sale
 *        from the cursor, which vacates nothing.
 */
bool StoreGoldFitForTest(int price, const Item *itemFreeingCells);

/** @brief Test hook: trims Adria's array against the tighter of the two pages it appears on. */
void TrimWitchStockToOnePageForTest();

/** Current level of the item sold by Wirt */
extern int boylevel;
/** Current item sold by Wirt (the old one-item table; SStartBoyBuy still reads it, nothing routes there) */
extern Item boyitem;
/** @brief Wirt's Shop tab (2026-09-20): what he has to sell, rolled as his one item always was, identified. */
#define BOY_ITEMS 60 // over-supplied so the 10x16 grid comes out FULL (user, 2026-09-20: "Fill entire grid with stock in Wirt's shop/gamble grids"); TrimShopStockToOnePage cuts the rest
extern DVL_API_FOR_TEST Item boyitems[BOY_ITEMS];
/** @brief Wirt's Gamble tab: one unidentified base per slot, priced by slot and hero level; the roll happens on the purchase. */
#define GAMBLE_ITEMS 60 // the eleven slot types cycled until the page is full, then trimmed (2026-09-20)
extern DVL_API_FOR_TEST Item gambleitems[GAMBLE_ITEMS];

/**
 * @brief Removes entry @p idx from a fixed vendor array and closes the gap, bounded by @p capacity.
 *
 * Exported for the full-array regression test. All three vendors' purchase paths go through this, so
 * testing it is testing all three - which is the point of there being one of it.
 */
void RemoveFromVendorStock(Item *stock, int capacity, int idx);

void AddStoreHoldRepair(Item *itm, int8_t i);

/** Clears premium items sold by Griswold and Wirt. */
void InitStores();

/** Spawns items sold by vendors, including premium items sold by Griswold and Wirt. */
void SetupTownStores();

void FreeStoreMem();

void PrintSString(const Surface &out, int margin, int line, string_view text, UiFlags flags, int price = 0, int cursId = -1, bool cursIndent = false);
void DrawSLine(const Surface &out, int sy);
void DrawSTextHelp();
void ClearSText(int s, int e);
void StartStore(TalkID s);
/** @brief Wirt's Refresh button (2026-09-20): a fresh stock on @p tab (BoyBuy or BoyGamble), trimmed to the page, free. */
void RefreshBoyStock(TalkID tab);

/**
 * @brief Griswold's CURATED shelves - the ones generated once per game rather than restocked.
 *
 * Unique came first; Rare and Set joined it on 2026-08-27. They share every behaviour that is not
 * "how is one item made": the page-sized array, the buy-and-do-not-refill rule, the stale-row
 * guards, the scroll arithmetic. Only the generator, the INI switch and the tab name differ, and
 * each of those is one function keyed on this enum - which is what stopped the second and third
 * shelves from being two more copies of the first.
 */
enum class CuratedShelf : uint8_t {
	Unique,
	Rare,
	Set,
	Count,
};

/**
 * @brief Whether Griswold offers @p shelf at all.
 *
 * Exposed so the shop tab strip can ask rather than re-derive it. The rule is "single-player, and
 * the option is on", and a second copy of that in the tab code would be a second opinion about
 * whether a tab exists - which shows up as a tab that opens an empty screen.
 */
bool HasCuratedShelf(CuratedShelf shelf);

/**
 * @brief Whether @p shelf is switched on AND actually has something on it.
 *
 * The tab strip asks THIS, not HasCuratedShelf. A shelf can be enabled and empty - the set pieces
 * start at required level 18, so a character below that has earned none of them - and a tab over an
 * empty shelf does not open: the text-store's start returns false and drops the player back to
 * Griswold's dialog. Reported the day the Set tab shipped (user, 2026-08-27: "SET button sends me
 * back to dialog window of Griswold").
 */
bool CuratedShelfHasStock(CuratedShelf shelf);

/** @brief Which shelf a shop screen shows, if it shows one at all. */
std::optional<CuratedShelf> CuratedShelfFor(TalkID id);

/** @brief The screen that shows @p shelf. */
TalkID TalkIdForCuratedShelf(CuratedShelf shelf);

/** @brief Kept as the unique shelf's own name, since the tab strip and the menu both still ask. */
bool HasSmithUniqueShop();

namespace oracool {

/**
 * @brief One item on a shop tab, paired with the index that tab's transaction path expects.
 *
 * The two are NOT the same number, and that is the whole reason this type exists. Griswold's basic
 * stock is indexed by array slot (holes included), his premium stock by visible position (holes
 * skipped), and the sell/repair/recharge screens by position in storehold. A grid that invented its
 * own numbering would sell the wrong item on whichever of those it guessed wrong.
 */
struct ShopSlot {
	Item *item;
	int index;
	int price;
	/**
	 * @brief This entry may be shown but never CLEARED by the one-page trim.
	 *
	 * Pepin's four potions on the Supplies tab. They restock rather than sell out, so they are not
	 * stock in the sense the trim means - clearing one would delete a permanent fixture rather than
	 * an overflowing item. It is a property of the entry rather than of the tab because Supplies is
	 * the only mixed shelf there is: four protected potions followed by Adria's real array, and the
	 * trim has to be able to tell them apart within one placement pass (external audit of v1.9.97,
	 * finding 2).
	 */
	bool neverTrim = false;
};

/**
 * @brief A bulk action a shop tab offers beside its grid - Sell all, Repair all, Refresh.
 *
 * @p line is the store text line the action still owns. The grid does not render that line, but the
 * tab's Enter handler dispatches on it, so it is what identifies the action.
 */
struct ShopAction {
	const char *label;
	int line;
};

} // namespace oracool

/** @brief The stock behind a shop tab, in the order the text list shows it. Empty for non-shop screens. */
std::vector<oracool::ShopSlot> GetShopStock(TalkID id);

/**
 * @brief Picks item @p index on tab @p id, exactly as pressing Enter on its row would.
 *
 * Implemented by putting the scroll state where the tab's own Enter handler would have found that
 * index and then calling it, rather than by duplicating its checks. Those checks - can the player
 * afford it, will it fit, is the row stale - have each been a bug at least once, and a second copy
 * of them behind the grid would be a second place for them to drift.
 */
void ShopSelectIndex(TalkID id, int index);

/**
 * @brief Ctrl+right click: buys a STACK of the restocking potion at entry @p index of tab @p id - up to
 * 99, as many as the gold pays for, and as many as fit the belt or the backpack in one stack.
 *
 * @return how many were bought; -1 when the gesture does not apply (not a restocking potion, no gold
 * for one, no room for one), in which case the caller runs the ordinary single purchase, whose own
 * NoMoney/NoRoom screens say why.
 */
int ShopBuyPotionStack(TalkID id, int index);

/** @brief The bulk actions tab @p id offers right now, already gated on the options that hide them. */
std::vector<oracool::ShopAction> GetShopActions(TalkID id);

/** @brief Runs the bulk action on @p line, exactly as pressing Enter on its row would. */
void ShopActivateAction(TalkID id, int line);

/**
 * @brief Whether tab @p id currently offers a bulk sale, or a fresh stock.
 *
 * Griswold's redesigned page (2026-09-21) draws all six of his service buttons on every tab, so it
 * needs to ask which of them the tab can actually do rather than being handed a list of the ones it
 * can. Both answers are derived from GetShopActions' own rows, so a gating rule can only ever be
 * changed in the one place - the pair below cannot drift away from the list they read.
 */
bool ShopTabHasSellAll(TalkID id);
bool ShopTabHasRefresh(TalkID id);
/**
 * @brief Whether "Refresh until" is available - which is also the answer to "is that option on".
 *
 * Its button is drawn apart from the other six, below the grid beside the gold (user, 2026-09-21:
 * "Refresh until is a bit of a cheat, so if someone activates it put a button somewhere bellow the
 * grid near the gold counter"), and only when this is true.
 */
bool ShopTabHasRefreshUntil(TalkID id);
/** @brief Runs those, if the tab has them. Does nothing if it does not. */
void ShopRunSellAll(TalkID id);
void ShopRunRefresh(TalkID id);
void ShopRunRefreshUntil(TalkID id);

/**
 * @brief Sells the item in the player's hand to the shop that is open. False if it is not taken.
 *
 * Selling is a drag now: pick an item out of the inventory and drop it on the shop panel. False
 * means the vendor does not deal in it, and the caller should leave the item in the player's hand
 * rather than swallowing it.
 */
bool ShopSellHeldItem();

/**
 * @brief Sells the backpack item at @p cii to the open vendor. False if it will not be taken.
 *
 * Right-clicking an item where it lies (user, 2026-08-26). Refuses anything outside the backpack
 * grid - a worn item, a belt slot - so the gesture cannot strip a character by accident.
 */
bool ShopSellInventoryItem(int cii);

/** @brief Repairs the item in the player's hand, charging for it. False if there is nothing to do. */
bool ShopRepairHeldItem();

/**
 * @brief Which shop service, if any, has borrowed the cursor.
 *
 * The shop's Repair and Recharge buttons arm the vanilla Repair and Recharge SKILL cursors, so the
 * hammer/lightning graphic, the click-an-item targeting and TryIconCurs' inventory/tab/stash routing
 * all come for free. This is the one piece of state that tells the shop's version apart from the
 * skill's at the click - the skill acts partially and free, the shop acts fully and charges.
 *
 * One enum rather than two flags: they are mutually exclusive by construction (one cursor), and two
 * independent bools would let a stale Recharge outlive a Repair click.
 */
enum class ShopServiceCursor : uint8_t {
	None,
	Repair,
	Recharge,
	/**
	 * Sell ONE item, picked where it lies (user, 2026-09-21: "Sell an item is a new button - sells
	 * specific item, so when clicked use the Repair Item hammer cursor and when clicked on an item it
	 * is sold"). It wears the HAMMER, the same graphic Repair does, because that is what was asked
	 * for - so this flag is the only thing separating a sell click from a repair click, exactly as it
	 * already separates a paid repair from the Repair skill.
	 */
	Sell,
};

void ArmShopRepairCursor();
void ArmShopRechargeCursor();
void ArmShopSellCursor();
/** @brief Whether the paid service may run - armed AND a shop still open to charge for it. */
bool IsShopRepairCursorArmed();
bool IsShopRechargeCursorArmed();
bool IsShopSellCursorArmed();

/**
 * @brief Sells the item at @p index of @p tab (-1 for the backpack) to the open vendor.
 *
 * What the Sell button's hammer does on a click. False when the vendor will not take it, when there
 * is no room for the gold, or when the slot is empty - and on a false the item stays exactly where
 * it is, because swallowing an item a vendor refuses is how you lose one.
 */
bool ShopSellItemAt(Player &player, int tab, int index);
/** @brief Whether any service cursor state is left to clean up, shop open or not. */
bool IsAnyShopServiceCursorArmed();

/**
 * @brief Clears a service cursor left behind by a shop that has closed. True if it did.
 *
 * Call before either vanilla Repair/Recharge fallback. A true return means the click belonged to a
 * closed shop and must be consumed, NOT reinterpreted as the class skill - the skill versions
 * permanently reduce maximum durability and maximum charges.
 */
bool ConsumeStaleShopServiceCursor();
/** @brief Clears the flag AND restores the hand cursor. See the definition - the second half matters. */
void DisarmShopServiceCursor();

/** @brief Repairs @p item to full and charges for it. False if it was not repaired. */
bool ShopRepairItemAt(Item &item);

/** @brief Recharges @p item to full and charges for it. False if it was not recharged. */
bool ShopRechargeItemAt(Item &item);

/** @brief Recharges the item in the player's hand, charging for it. Same contract. */
bool ShopRechargeHeldItem();

/** @brief Repairs everything the player carries, dearest first, until the gold runs out. */
void ShopRepairAll();

/** @brief What Repair All would charge right now. 0 when there is nothing to repair. */
int ShopRepairAllPrice();

/**
 * @brief What the currently open vendor would pay for @p item. 0 if none is open, or they refuse it.
 *
 * For the inventory hover, so "what is this worth" is answered where the player is already looking.
 */
int ShopSellOfferFor(const Item &item);

/** @brief Buys back entry @p index of the Sold tab, at the price the player was paid for it. */
void ShopBuyBack(int index);

void DrawSText(const Surface &out);
void StoreESC();
void StoreUp();
void StoreDown();
void StorePrior();
void StoreNext();
void TakePlrsMoney(int cost);
/**
 * @brief The player's whole spendable gold: carried plus the shared Stash pool.
 *
 * Oracool: picked-up and sold gold goes to the Stash now, so _pGold alone is NOT the amount the
 * player has - on a character who never picked gold up by hand it reads 0 while the Stash holds all
 * of it. Every place that shows or spends "your gold" must go through this. Exported precisely
 * because it was not: the character sheet had already re-derived the same sum inline, and the
 * inventory's readout was written twice against the wrong field before this was noticed.
 */
uint32_t TotalPlayerGold();

/**
 * @brief Keeps store state honest. Called once a game tick, on every level.
 *
 * Two invariants, both RECONCILED rather than maintained at each exit - see the definition for why
 * that distinction is the whole point:
 *
 *  - a Repair/Recharge service cursor never outlives the shop that armed it (external audit of
 *    v1.9.88, finding 2: the vanilla fallback permanently reduces maximum durability);
 *  - a shop closes when the player walks away from its counter (user, 2026-08-27: "close griswold
 *    and all other vendor shops on walkaway"), because a shop is a PANEL in this fork rather than a
 *    modal screen and the player can walk off with it still covering a third of the screen.
 */
void UpdateStoreState();

void StoreEnter();
void CheckStoreBtn();
void ReleaseStoreBtn();

/**
 * @brief Oracool: whether Griswold Premium's "type the item you're looking for" prompt (Refresh
 * Until, entered in-game instead of hand-edited into diablo.ini) is currently open.
 */
extern bool IsRefreshUntilPromptOpen;
/**
 * @brief The prompt's ONE teardown: stops SDL text input, clears the flag, drops the input state.
 *
 * Every exit must come through here. Clearing `IsRefreshUntilPromptOpen` by hand instead makes this
 * a no-op ever after - it returns early on that flag - so the IME is left running with nothing on
 * screen asking for text (external audit of v1.9.88, finding 6).
 */
void CloseRefreshUntilPrompt();
void RefreshUntilPromptKeyPress(SDL_Keycode vkey);
bool HandleRefreshUntilPromptTextInputEvent(const SDL_Event &event);
void DrawRefreshUntilPrompt(const Surface &out);
/** @brief Oracool: shows a 4-line explainer in the main HUD's info box while hovering (not
 * clicking) Griswold Premium's Refresh Until button. */
void DrawRefreshUntilHoverTooltip(const Surface &out);

} // namespace devilution
