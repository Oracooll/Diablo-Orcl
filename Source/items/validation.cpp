/**
 * @file items/validation.cpp
 *
 * Implementation of functions for validation of player and item data.
 */

#include "items/validation.h"

#include <algorithm>
#include <cstdint>

#include "items.h"
#include "monstdat.h"
#include "player.h"
#include "spells.h"

namespace devilution {

namespace {

bool hasMultipleFlags(uint16_t flags)
{
	return (flags & (flags - 1)) > 0;
}

} // namespace

bool IsCreationFlagComboValid(uint16_t iCreateInfo)
{
	iCreateInfo = iCreateInfo & ~CF_LEVEL;
	const bool isTownItem = (iCreateInfo & CF_TOWN) != 0;
	const bool isPregenItem = (iCreateInfo & CF_PREGEN) != 0;
	const bool isUsefulItem = (iCreateInfo & CF_USEFUL) == CF_USEFUL;

	if (isPregenItem) {
		// Pregen flags are discarded when an item is picked up, therefore impossible to have in the inventory
		return false;
	}
	if (isUsefulItem && (iCreateInfo & ~CF_USEFUL) != 0)
		return false;
	if (isTownItem) {
		// Oracool's Griswold Unique Items shop (CreateUniqueVendorItem, items.cpp) stamps its
		// stock with CF_UNIQUE alongside the usual single town flag, so stores.cpp can tell a
		// Unique-Shop purchase apart from a real dropped Unique for resale/pricing purposes.
		// Vanilla never combines CF_UNIQUE with a town flag, so CF_UNIQUE must be excluded
		// before applying vanilla's "only one towner flag" rule below - otherwise every single
		// item bought from that shop fails this check and vanishes with "sent an invalid
		// packet" the moment it's dropped on the ground.
		if (hasMultipleFlags(iCreateInfo & ~CF_UNIQUE)) {
			// Items from town can only have 1 towner flag
			return false;
		}
	}
	return true;
}

bool IsTownItemValid(uint16_t iCreateInfo)
{
	const uint8_t level = iCreateInfo & CF_LEVEL;
	const bool isBoyItem = (iCreateInfo & CF_BOY) != 0;
	const uint8_t maxTownItemLevel = 30;

	// Wirt items in multiplayer are equal to the level of the player, therefore they cannot exceed the max
	// character level. Oracool: CF_LEVEL is only 6 bits wide (max 63), which is now below
	// MaxCharacterLevel (99) - clamp the comparison to whichever ceiling is actually lower so this
	// stays a meaningful check instead of becoming a no-op once MaxCharacterLevel outgrew the field.
	if (isBoyItem && level <= std::min<int>(MaxCharacterLevel, CF_LEVEL))
		return true;

	return level <= maxTownItemLevel;
}

bool IsShopPriceValid(const Item &item)
{
	const int boyPriceLimit = MaxBoyValue;
	if (!gbIsHellfire && (item._iCreateInfo & CF_BOY) != 0 && item._iIvalue > boyPriceLimit)
		return false;

	const int premiumPriceLimit = MaxVendorValue;
	if (!gbIsHellfire && (item._iCreateInfo & CF_SMITHPREMIUM) != 0 && item._iIvalue > premiumPriceLimit)
		return false;

	const uint16_t smithOrWitch = CF_SMITH | CF_WITCH;
	const int smithAndWitchPriceLimit = gbIsHellfire ? MaxVendorValueHf : MaxVendorValue;
	if ((item._iCreateInfo & smithOrWitch) != 0 && item._iIvalue > smithAndWitchPriceLimit)
		return false;

	return true;
}

bool IsUniqueMonsterItemValid(uint16_t iCreateInfo, uint32_t dwBuff)
{
	const uint8_t level = iCreateInfo & CF_LEVEL;
	const bool isHellfireItem = (dwBuff & CF_HELLFIRE) != 0;

	// Check all unique monster levels to see if they match the item level
	for (int i = 0; UniqueMonstersData[i].mName != nullptr; i++) {
		const auto &uniqueMonsterData = UniqueMonstersData[i];
		const auto &uniqueMonsterLevel = static_cast<uint8_t>(MonstersData[uniqueMonsterData.mtype].level);

		if (IsAnyOf(uniqueMonsterData.mtype, MT_DEFILER, MT_NAKRUL, MT_HORKDMN)) {
			// These monsters don't use their mlvl for item generation
			continue;
		}

		if (level == uniqueMonsterLevel) {
			// If the ilvl matches the mlvl, we confirm the item is legitimate
			return true;
		}
	}

	// Oracool note, 2026-09-12: this exact-match rule cannot describe one of THIS fork's own drops.
	// SpawnUnique branches on difficulty (items.cpp:4674) and above Normal stamps the special
	// treasure at curlv * 2 off the 64-rung area ladder, so the Skeleton King's crown on Nightmare
	// carries ilvl 38 | CF_UPER15 and no unique monster has base mlvl 38.
	//
	// The rule is deliberately left alone anyway, because it is reached by two very different
	// callers and it is correct for one of them: UnPackNetPlayer validates a REMOTE PLAYER'S
	// equipment, where an ilvl that matches no monster really is a forgery (see
	// NetPackTest.UnPackNetPlayer_invalid_uniqueMonsterItemLevel, which pins exactly that). The
	// locally-generated drop that the user hit is handled where it belongs - the !gbIsMultiplayer
	// early-out in IsPItemValid, msg.cpp - rather than by loosening a check that protects the other
	// caller. If multiplayer is ever revived, THIS is where the ladder has to be taught.
	return false;
}

bool IsDungeonItemValid(uint16_t iCreateInfo, uint32_t dwBuff)
{
	const uint8_t level = iCreateInfo & CF_LEVEL;
	const bool isHellfireItem = (dwBuff & CF_HELLFIRE) != 0;

	// Check all monster levels to see if they match the item level
	for (int16_t i = 0; i < static_cast<int16_t>(NUM_MTYPES); i++) {
		const auto &monsterData = MonstersData[i];
		auto monsterLevel = static_cast<uint8_t>(monsterData.level);

		if (i != MT_DIABLO && monsterData.availability == MonsterAvailability::Never) {
			// Skip monsters that are unable to appear in the game
			continue;
		}

		if (i == MT_DIABLO && !isHellfireItem) {
			// Adjust The Dark Lord's mlvl if the item isn't a Hellfire item to match the Diablo mlvl
			monsterLevel -= 15;
		}

		if (level == monsterLevel) {
			// If the ilvl matches the mlvl, we confirm the item is legitimate
			return true;
		}
	}

	if (isHellfireItem) {
		uint8_t hellfireMaxDungeonLevel = 24;

		// Hellfire adjusts the currlevel minus 7 in dungeon levels 20-24 for generating items
		hellfireMaxDungeonLevel -= 7;
		return level <= (hellfireMaxDungeonLevel * 2);
	}

	uint8_t diabloMaxDungeonLevel = 16;

	// Diablo doesn't have containers that drop items in dungeon level 16, therefore we decrement by 1
	diabloMaxDungeonLevel--;

	// Oracool note, 2026-09-12: this ceiling of 30 is vanilla's and does NOT cover this fork's own
	// drops. Items generate at 2 * ItemsGetCurrlevel(), and ItemsGetCurrlevel() returns the AREA
	// level rather than the floor number (items.cpp:534); oracool::AreaLevel adds 16 per difficulty
	// block, so a Nightmare sarcophagus stamps 2 * (floor + 16) = 34 and up.
	//
	// Left as vanilla on purpose - same reasoning as IsUniqueMonsterItemValid above. This function
	// also validates a remote player's equipment through UnPackNetPlayer, where the ceiling is a
	// real check (NetPackTest.UnPackNetPlayer_invalid_monsterItemLevel pins it). The locally
	// generated drop is handled by the !gbIsMultiplayer early-out in IsPItemValid (msg.cpp).
	return level <= (diabloMaxDungeonLevel * 2);
}

bool IsHellfireSpellBookValid(const Item &spellBook)
{
	// Hellfire uses the spell book level when generating items via CreateSpellBook()
	int spellBookLevel = GetSpellBookLevel(spellBook._iSpell);

	// CreateSpellBook() adds 1 to the spell level for ilvl
	spellBookLevel++;

	if (spellBookLevel >= 1 && (spellBook._iCreateInfo & CF_LEVEL) == spellBookLevel * 2) {
		// The ilvl matches the result for a spell book drop, so we confirm the item is legitimate
		return true;
	}

	return IsDungeonItemValid(spellBook._iCreateInfo, spellBook.dwBuff);
}

bool IsItemValid(const Item &item)
{
	if (!gbIsMultiplayer)
		return true;

	if (item.IDidx == IDI_EAR)
		return true;
	if (item.IDidx != IDI_GOLD && !IsCreationFlagComboValid(item._iCreateInfo))
		return false;
	if ((item._iCreateInfo & CF_TOWN) != 0)
		return IsTownItemValid(item._iCreateInfo) && IsShopPriceValid(item);
	if ((item._iCreateInfo & CF_USEFUL) == CF_UPER15)
		return IsUniqueMonsterItemValid(item._iCreateInfo, item.dwBuff);
	if ((item.dwBuff & CF_HELLFIRE) != 0 && AllItemsList[item.IDidx].iMiscId == IMISC_BOOK)
		return IsHellfireSpellBookValid(item);

	return IsDungeonItemValid(item._iCreateInfo, item.dwBuff);
}

} // namespace devilution
