#pragma once
/**
 * @file oracool/hidden_classes.h
 *
 * Classes taken out of the mod without being deleted (user, 2026-09-14: "Bard causes too much headache,
 * so remove this class from our mod. I want to introduce Necromancer in his place later ... Hide them,
 * dont remove them. We might resurrect this class later.").
 *
 * A hidden class:
 * - is not offered when a hero is created;
 * - does not appear in the hero list, though its save files are left on disk untouched;
 * - keeps every row of data - class tree, stats, sprites, sounds, its two starting weapons - so turning it
 *   back on is a one-line change here.
 *
 * Its items stay AVAILABLE to IsItemAvailable on purpose: that function also decides what survives loading
 * a save, and a Bard brought back later must still be holding his Sword and Dagger. They cannot spawn
 * anywhere regardless - both are IDROP_NEVER, and a hidden class never starts a new hero.
 */

#include "itemdat.h"
#include "player.h"

namespace devilution::oracool {

constexpr bool IsClassHidden(HeroClass heroClass)
{
	return heroClass == HeroClass::Bard;
}

/**
 * @brief Item bases that exist only for a hidden class, and so no longer generate anywhere.
 *
 * The Bard's two (user, 2026-09-14: "there are items who's assets are bard music instruments. we need to
 * hide these assets from the game" / "search previous RfAs for items that were requested to serve Bard").
 * RfA-04 batch 12 and RfA-08 drew them, and the unique expansion package lists every unique on them as
 * "recommended: Bard":
 * - the War Lute (Sunless Oath, Oath of the Unmoved, Worldroot Severance) - an instrument swung as a mace;
 * - the Canticle (The Ivory Refusal, Twelve-Nail Ward, Last Hearth's Guard) - a hymn book for the shield hand.
 *
 * A GENERATION filter only, like the Town Portal scroll's: every loot, vendor and Smart Loot draw funnels
 * through GetItemIndexForDroppableItem, and the vendor's own unique path asks here too. It is NOT
 * IsItemAvailable - an item already carried keeps loading - and nothing is deleted: rows, uniques, icons
 * and tumbles all stay, ready for the class to return.
 *
 * The package's other 32 "recommended: Bard" uniques sit on shared bases (daggers, helms, rings...) any
 * hero wears, and are not hidden.
 */
constexpr bool IsHiddenItemBase(unique_base_item base)
{
	return IsClassHidden(HeroClass::Bard) && (base == UITYPE_WARLUTE || base == UITYPE_CANTICLE);
}

inline bool IsHiddenItemIdx(int idx)
{
	return idx >= 0 && idx <= IDI_LAST && IsHiddenItemBase(AllItemsList[static_cast<size_t>(idx)].iItemId);
}

} // namespace devilution::oracool
