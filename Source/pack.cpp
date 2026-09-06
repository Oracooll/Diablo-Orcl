/**
 * @file pack.cpp
 *
 * Implementation of functions for minifying player data structure.
 */
#include "pack.h"

#include <algorithm>
#include <cstdint>

#include "DiabloUI/diabloui.h"
#include "engine/random.hpp"
#include "init.h"
#include "items/validation.h"
#include "loadsave.h"
#include "oracool/readied_spells.h"
#include "playerdat.hpp"
#include "plrmsg.h"
#include "stores.h"
#include "utils/endian_read.hpp"
#include "utils/log.hpp"
#include "utils/utf8.hpp"

#define ValidateField(logValue, condition)                         \
	do {                                                           \
		if (!(condition)) {                                        \
			LogFailedJoinAttempt(#condition, #logValue, logValue); \
			EventFailedJoinAttempt(player._pName);                 \
			return false;                                          \
		}                                                          \
	} while (0)

#define ValidateFields(logValue1, logValue2, condition)                                     \
	do {                                                                                    \
		if (!(condition)) {                                                                 \
			LogFailedJoinAttempt(#condition, #logValue1, logValue1, #logValue2, logValue2); \
			EventFailedJoinAttempt(player._pName);                                          \
			return false;                                                                   \
		}                                                                                   \
	} while (0)

namespace devilution {

namespace {

void EventFailedJoinAttempt(const char *playerName)
{
	std::string message = fmt::format("Player '{}' sent invalid player data during attempt to join the game.", playerName);
	EventPlrMsg(message);
}

template <typename T>
void LogFailedJoinAttempt(const char *condition, const char *name, T value)
{
	LogDebug("Remote player validation failed: ValidateField({}: {}, {})", name, value, condition);
}

template <typename T1, typename T2>
void LogFailedJoinAttempt(const char *condition, const char *name1, T1 value1, const char *name2, T2 value2)
{
	LogDebug("Remote player validation failed: ValidateFields({}: {}, {}: {}, {})", name1, value1, name2, value2, condition);
}

void VerifyGoldSeeds(Player &player)
{
	for (int i = 0; i < player._pNumInv; i++) {
		if (player.InvList[i].IDidx != IDI_GOLD)
			continue;
		for (int j = 0; j < player._pNumInv; j++) {
			if (i == j)
				continue;
			if (player.InvList[j].IDidx != IDI_GOLD)
				continue;
			if (player.InvList[i]._iSeed != player.InvList[j]._iSeed)
				continue;
			player.InvList[i]._iSeed = AdvanceRndSeed();
			j = -1;
		}
	}
}

/**
 * @brief Oracool: user request - see PlayerPack::pWaypointUnlockedNormal's doc comment. Bit
 * (i-1) is waypoint list index i (1-24); index 0 (Tristram) is always unlocked and never stored.
 */
// Capped at 32: MaxWaypointSlots is 64 now (Phase 0.2), but these fixed u32 fields only carry
// slots 1-32 - the full width travels in the hero file's HeroChunkWaypoints64 chunk, which
// overrides these on load when present. The fixed fields stay valid so a chunkless hero still
// decodes every waypoint that exists today (25 < 32).
constexpr int PackedWaypointBits = std::min<int>(static_cast<int>(Player::MaxWaypointSlots) - 1, 32);

uint32_t PackWaypointUnlocked(const Player &player, int difficulty)
{
	uint32_t mask = 0;
	for (int i = 1; i <= PackedWaypointBits; i++) {
		if (player._pWaypointUnlocked[difficulty][i])
			mask |= static_cast<uint32_t>(1u << (i - 1));
	}
	return SDL_SwapLE32(mask);
}

void UnpackWaypointUnlocked(Player &player, int difficulty, uint32_t packedMask)
{
	const uint32_t mask = SDL_SwapLE32(packedMask);
	for (int i = 1; i <= PackedWaypointBits; i++)
		player._pWaypointUnlocked[difficulty][i] = (mask & (1u << (i - 1))) != 0;
	// Slots past the fixed mask's reach: cleared here so a chunkless hero never inherits stale
	// unlocks from whatever character occupied this Player slot before. The Waypoints64 chunk,
	// applied AFTER unpack, overwrites the full range for heroes that carry it.
	for (size_t i = static_cast<size_t>(PackedWaypointBits) + 1; i < Player::MaxWaypointSlots; i++)
		player._pWaypointUnlocked[difficulty][i] = false;
}

} // namespace

bool RecreateHellfireSpellBook(const Player &player, const TItem &packedItem, Item *item)
{
	Item spellBook {};
	RecreateItem(player, packedItem, spellBook);

	// Hellfire uses the spell book level when generating items via CreateSpellBook()
	int spellBookLevel = GetSpellBookLevel(spellBook._iSpell);

	// CreateSpellBook() adds 1 to the spell level for ilvl
	spellBookLevel++;

	if (spellBookLevel >= 1 && (spellBook._iCreateInfo & CF_LEVEL) == spellBookLevel * 2) {
		// The ilvl matches the result for a spell book drop, so we confirm the item is legitimate
		if (item != nullptr)
			*item = spellBook;
		return true;
	}

	ValidateFields(spellBook._iCreateInfo, spellBook.dwBuff, IsDungeonItemValid(spellBook._iCreateInfo, spellBook.dwBuff));
	if (item != nullptr)
		*item = spellBook;
	return true;
}

void PackItem(ItemPack &packedItem, const Item &item, bool isHellfire)
{
	packedItem = {};
	// Arena potions don't exist in vanilla so don't save them to stay backward compatible
	if (item.isEmpty() || item._iMiscId == IMISC_ARENAPOT) {
		packedItem.idx = 0xFFFF;
	} else {
		auto idx = item.IDidx;
		if (!isHellfire) {
			idx = RemapItemIdxToDiablo(idx);
		}
		if (gbIsSpawn) {
			idx = RemapItemIdxToSpawn(idx);
		}
		packedItem.idx = SDL_SwapLE16(idx);
		if (item.IDidx == IDI_EAR) {
			packedItem.iCreateInfo = SDL_SwapLE16(item._iIName[1] | (item._iIName[0] << 8));
			packedItem.iSeed = SDL_SwapLE32(LoadBE32(&item._iIName[2]));
			packedItem.bId = item._iIName[6];
			packedItem.bDur = item._iIName[7];
			packedItem.bMDur = item._iIName[8];
			packedItem.bCh = item._iIName[9];
			packedItem.bMCh = item._iIName[10];
			packedItem.wValue = SDL_SwapLE16(item._ivalue | (item._iIName[11] << 8) | ((item._iCurs - ICURS_EAR_SORCERER) << 6));
			packedItem.dwBuff = SDL_SwapLE32(LoadBE32(&item._iIName[12]));
		} else {
			packedItem.iSeed = SDL_SwapLE32(item._iSeed);
			packedItem.iCreateInfo = SDL_SwapLE16(item._iCreateInfo);
			packedItem.bId = (item._iMagical << 1) | (item._iIdentified ? 1 : 0);
			if (item._iMaxDur > 255)
				packedItem.bMDur = 254;
			else
				packedItem.bMDur = item._iMaxDur;
			// Clamped at BOTH ends: bDur is a byte, so a negative _iDurability - which a save from
			// before WearDurabilityPoint can still hold - would wrap to a large positive on the way
			// in and come back out as a nearly-full item.
			packedItem.bDur = std::clamp<int32_t>(item._iDurability, 0, packedItem.bMDur);

			packedItem.bCh = item._iCharges;
			packedItem.bMCh = item._iMaxCharges;
			if (item.IDidx == IDI_GOLD)
				packedItem.wValue = SDL_SwapLE16(item._ivalue);
			packedItem.dwBuff = item.dwBuff;
		}
	}
}

void PackPlayer(PlayerPack &packed, const Player &player)
{
	memset(&packed, 0, sizeof(packed));
	packed.destAction = player.destAction;
	packed.destParam1 = player.destParam1;
	packed.destParam2 = player.destParam2;
	packed.plrlevel = player.plrlevel;
	packed.px = player.position.tile.x;
	packed.py = player.position.tile.y;
	if (gbVanilla) {
		packed.targx = player.position.tile.x;
		packed.targy = player.position.tile.y;
	}
	CopyUtf8(packed.pName, player._pName, sizeof(packed.pName));
	packed.pClass = static_cast<uint8_t>(player._pClass);
	packed.pBaseStr = player._pBaseStr;
	packed.pBaseMag = player._pBaseMag;
	packed.pBaseDex = player._pBaseDex;
	packed.pBaseVit = player._pBaseVit;
	packed.pLevel = player._pLevel;
	// CLAMPED, not narrowed. _pStatPts is an int and this field is a uint8_t, so the old implicit
	// conversion wrapped: 260 points wrote 4 (external audit, 2026-08-25). The real value rides
	// HeroChunkStatPoints, which UnPackPlayer prefers when present; this stays filled and sane so a
	// reader without the tail sees a capped pool instead of a wrapped one. Losing points is bad;
	// silently turning 490 into 234 is worse, because it looks like a number.
	packed.pStatPts = static_cast<uint8_t>(std::clamp(player._pStatPts, 0, 255));
	packed.pStatPtsSpentStr = SDL_SwapLE32(player._pStatPtsSpentStr);
	packed.pStatPtsSpentMag = SDL_SwapLE32(player._pStatPtsSpentMag);
	packed.pStatPtsSpentDex = SDL_SwapLE32(player._pStatPtsSpentDex);
	packed.pStatPtsSpentVit = SDL_SwapLE32(player._pStatPtsSpentVit);
	packed.pWaypointUnlockedNormal = PackWaypointUnlocked(player, DIFF_NORMAL);
	packed.pWaypointUnlockedNightmare = PackWaypointUnlocked(player, DIFF_NIGHTMARE);
	packed.pWaypointUnlockedHell = PackWaypointUnlocked(player, DIFF_HELL);
	packed.pWaypointUnlockedTorment = PackWaypointUnlocked(player, DIFF_TORMENT);
	packed.pExperience = SDL_SwapLE64(player._pExperience);
	packed.pGold = SDL_SwapLE32(player._pGold);
	packed.pHPBase = SDL_SwapLE32(player._pHPBase);
	packed.pMaxHPBase = SDL_SwapLE32(player._pMaxHPBase);
	packed.pManaBase = SDL_SwapLE32(player._pManaBase);
	packed.pMaxManaBase = SDL_SwapLE32(player._pMaxManaBase);
	packed.pMemSpells = SDL_SwapLE64(player._pMemSpells.low); // the low word - see SpellMask for why that is whole

	for (int i = 0; i < 37; i++) // Should be MAX_SPELLS but set to 37 to make save games compatible
		packed.pSplLvl[i] = player._pSplLvl[i];
	for (int i = 37; i < 47; i++)
		packed.pSplLvl2[i - 37] = player._pSplLvl[i];

	for (int i = 0; i < NUM_INVLOC; i++)
		PackItem(packed.InvBody[i], player.InvBody[i], gbIsHellfire);

	packed._pNumInv = player._pNumInv;
	for (int i = 0; i < packed._pNumInv; i++)
		PackItem(packed.InvList[i], player.InvList[i], gbIsHellfire);

	for (int i = 0; i < InventoryGridCells; i++)
		packed.InvGrid[i] = player.InvGrid[i];

	for (int i = 0; i < MaxBeltItems; i++)
		PackItem(packed.SpdList[i], player.SpdList[i], gbIsHellfire);

	packed.pReadiedSpellRight = oracool::PackReadiedSpell(player._pRSpell);
	packed.pReadiedSpellLeft = oracool::PackReadiedSpell(player._pLRSpell);
	packed.wReflections = SDL_SwapLE16(player.wReflections);
	packed.pDamAcFlags = SDL_SwapLE32(static_cast<uint32_t>(player.pDamAcFlags));
	packed.pDiabloKillLevel = SDL_SwapLE32(player.pDiabloKillLevel);
	packed.bIsHellfire = gbIsHellfire ? 1 : 0;
}

void PackNetItem(const Item &item, ItemNetPack &packedItem)
{
	if (item.isEmpty()) {
		packedItem.def.wIndx = static_cast<_item_indexes>(0xFFFF);
		return;
	}
	packedItem.def.wIndx = static_cast<_item_indexes>(SDL_SwapLE16(item.IDidx));
	packedItem.def.wCI = SDL_SwapLE16(item._iCreateInfo);
	packedItem.def.dwSeed = SDL_SwapLE32(item._iSeed);
	if (item.IDidx != IDI_EAR)
		PrepareItemForNetwork(item, packedItem.item);
	else
		PrepareEarForNetwork(item, packedItem.ear);
}

void PackNetPlayer(PlayerNetPack &packed, const Player &player)
{
	packed.plrlevel = player.plrlevel;
	packed.px = player.position.tile.x;
	packed.py = player.position.tile.y;
	CopyUtf8(packed.pName, player._pName, sizeof(packed.pName));
	packed.pClass = static_cast<uint8_t>(player._pClass);
	packed.pBaseStr = player._pBaseStr;
	packed.pBaseMag = player._pBaseMag;
	packed.pBaseDex = player._pBaseDex;
	packed.pBaseVit = player._pBaseVit;
	packed.pLevel = player._pLevel;
	// Clamped for the same reason as the hero pack above - see that comment. This is the NETWORK
	// pack, which multiplayer no longer reaches (oracool::MultiplayerEnabled), but a field that
	// wraps is a field that wraps.
	packed.pStatPts = static_cast<uint8_t>(std::clamp(player._pStatPts, 0, 255));
	packed.pExperience = SDL_SwapLE64(player._pExperience);
	packed.pHPBase = SDL_SwapLE32(player._pHPBase);
	packed.pMaxHPBase = SDL_SwapLE32(player._pMaxHPBase);
	packed.pManaBase = SDL_SwapLE32(player._pManaBase);
	packed.pMaxManaBase = SDL_SwapLE32(player._pMaxManaBase);
	packed.pMemSpells = SDL_SwapLE64(player._pMemSpells.low); // the low word - see SpellMask for why that is whole

	// The packet field is MAX_SPELLS wide, the player's book-level store 64: copying MAX_SPELLS
	// read 62 bytes past the array (external audit, 2026-09-06: NET-01). The tail is zero on the
	// wire; the packet layout is unchanged.
	static_assert(std::size(player._pSplLvl) <= std::size(packed.pSplLvl));
	for (size_t i = 0; i < std::size(packed.pSplLvl); i++)
		packed.pSplLvl[i] = i < std::size(player._pSplLvl) ? player._pSplLvl[i] : 0;

	for (int i = 0; i < NUM_INVLOC; i++)
		PackNetItem(player.InvBody[i], packed.InvBody[i]);

	packed._pNumInv = player._pNumInv;
	for (int i = 0; i < packed._pNumInv; i++)
		PackNetItem(player.InvList[i], packed.InvList[i]);

	for (int i = 0; i < InventoryGridCells; i++)
		packed.InvGrid[i] = player.InvGrid[i];

	for (int i = 0; i < MaxBeltItems; i++)
		PackNetItem(player.SpdList[i], packed.SpdList[i]);

	packed.wReflections = SDL_SwapLE16(player.wReflections);
	packed.pDiabloKillLevel = player.pDiabloKillLevel;
	packed.pManaShield = player.pManaShield;
	packed.friendlyMode = player.friendlyMode ? 1 : 0;
	packed.isOnSetLevel = player.plrIsOnSetLevel;

	packed.pStrength = SDL_SwapLE32(player._pStrength);
	packed.pMagic = SDL_SwapLE32(player._pMagic);
	packed.pDexterity = SDL_SwapLE32(player._pDexterity);
	packed.pVitality = SDL_SwapLE32(player._pVitality);
	packed.pHitPoints = SDL_SwapLE32(player._pHitPoints);
	packed.pMaxHP = SDL_SwapLE32(player._pMaxHP);
	packed.pMana = SDL_SwapLE32(player._pMana);
	packed.pMaxMana = SDL_SwapLE32(player._pMaxMana);
	packed.pDamageMod = SDL_SwapLE32(player._pDamageMod);
	packed.pBaseToBlk = SDL_SwapLE32(player._pBaseToBlk);
	packed.pIMinDam = SDL_SwapLE32(player._pIMinDam);
	packed.pIMaxDam = SDL_SwapLE32(player._pIMaxDam);
	packed.pIAC = SDL_SwapLE32(player._pIAC);
	packed.pIBonusDam = SDL_SwapLE32(player._pIBonusDam);
	packed.pIBonusToHit = SDL_SwapLE32(player._pIBonusToHit);
	packed.pIBonusAC = SDL_SwapLE32(player._pIBonusAC);
	packed.pIBonusDamMod = SDL_SwapLE32(player._pIBonusDamMod);
	packed.pIGetHit = SDL_SwapLE32(player._pIGetHit);
	packed.pIEnAc = SDL_SwapLE32(player._pIEnAc);
	packed.pIFMinDam = SDL_SwapLE32(player._pIFMinDam);
	packed.pIFMaxDam = SDL_SwapLE32(player._pIFMaxDam);
	packed.pILMinDam = SDL_SwapLE32(player._pILMinDam);
	packed.pILMaxDam = SDL_SwapLE32(player._pILMaxDam);
}

void UnPackItem(const ItemPack &packedItem, const Player &player, Item &item, bool isHellfire)
{
	if (packedItem.idx == 0xFFFF) {
		item.clear();
		return;
	}

	auto idx = static_cast<_item_indexes>(SDL_SwapLE16(packedItem.idx));

	if (gbIsSpawn) {
		idx = RemapItemIdxFromSpawn(idx);
	}
	if (!isHellfire) {
		idx = RemapItemIdxFromDiablo(idx);
	}

	if (!IsItemAvailable(idx)) {
		item.clear();
		return;
	}

	if (idx == IDI_EAR) {
		uint16_t ic = SDL_SwapLE16(packedItem.iCreateInfo);
		uint32_t iseed = SDL_SwapLE32(packedItem.iSeed);
		uint16_t ivalue = SDL_SwapLE16(packedItem.wValue);
		int32_t ibuff = SDL_SwapLE32(packedItem.dwBuff);

		char heroName[17];
		heroName[0] = static_cast<char>((ic >> 8) & 0x7F);
		heroName[1] = static_cast<char>(ic & 0x7F);
		heroName[2] = static_cast<char>((iseed >> 24) & 0x7F);
		heroName[3] = static_cast<char>((iseed >> 16) & 0x7F);
		heroName[4] = static_cast<char>((iseed >> 8) & 0x7F);
		heroName[5] = static_cast<char>(iseed & 0x7F);
		heroName[6] = static_cast<char>(packedItem.bId & 0x7F);
		heroName[7] = static_cast<char>(packedItem.bDur & 0x7F);
		heroName[8] = static_cast<char>(packedItem.bMDur & 0x7F);
		heroName[9] = static_cast<char>(packedItem.bCh & 0x7F);
		heroName[10] = static_cast<char>(packedItem.bMCh & 0x7F);
		heroName[11] = static_cast<char>((ivalue >> 8) & 0x7F);
		heroName[12] = static_cast<char>((ibuff >> 24) & 0x7F);
		heroName[13] = static_cast<char>((ibuff >> 16) & 0x7F);
		heroName[14] = static_cast<char>((ibuff >> 8) & 0x7F);
		heroName[15] = static_cast<char>(ibuff & 0x7F);
		heroName[16] = '\0';

		RecreateEar(item, ic, iseed, ivalue & 0xFF, heroName);
	} else {
		item = {};
		RecreateItem(player, item, idx, SDL_SwapLE16(packedItem.iCreateInfo), SDL_SwapLE32(packedItem.iSeed), SDL_SwapLE16(packedItem.wValue), isHellfire);
		item._iIdentified = (packedItem.bId & 1) != 0;
		item._iMaxDur = packedItem.bMDur;
		item._iDurability = ClampDurability(item, packedItem.bDur);
		item._iMaxCharges = clamp<int>(packedItem.bMCh, 0, item._iMaxCharges);
		item._iCharges = clamp<int>(packedItem.bCh, 0, item._iMaxCharges);
		// RecreateItem (via InitializeItem) resets dwBuff to 0, then re-derives bit 0
		// (CF_HELLFIRE) fresh from the current gbIsHellfire session flag. That freshly
		// computed bit is correct and must be kept as-is (vanilla hellfire saves don't
		// reliably carry this bit, so the engine intentionally re-derives it rather than
		// trusting the saved byte). Bits 1-31 (e.g. a stackable consumable's stack count)
		// get no such re-derivation and were previously lost entirely on every save/load
		// round trip; restore them from the packed data, mirroring what the network
		// RecreateItem(TItem) overload already does correctly (see msg.cpp). PackItem
		// writes dwBuff for non-ear items as a raw, unswapped copy (see PackItem below),
		// so it's read back the same way.
		item.dwBuff = (item.dwBuff & CF_HELLFIRE) | (packedItem.dwBuff & ~static_cast<uint32_t>(CF_HELLFIRE));
		if (item.isStackableConsumable() && item.stackCount() > Item::MaxStackCount)
			item.setStackCount(Item::MaxStackCount); // defensively clamp corrupt/out-of-range data
	}
}

void UnPackPlayer(const PlayerPack &packed, Player &player)
{
	Point position { packed.px, packed.py };

	player = {};
	player._pLevel = clamp<int8_t>(packed.pLevel, 1, MaxCharacterLevel);
	player._pMaxHPBase = SDL_SwapLE32(packed.pMaxHPBase);
	player._pHPBase = SDL_SwapLE32(packed.pHPBase);
	player._pHPBase = clamp<int32_t>(player._pHPBase, 0, player._pMaxHPBase);
	player._pMaxHP = player._pMaxHPBase;
	player._pHitPoints = player._pHPBase;
	player.position.tile = position;
	player.position.future = position;
	player.setLevel(clamp<int8_t>(packed.plrlevel, 0, NUMLEVELS));

	player._pClass = static_cast<HeroClass>(clamp<uint8_t>(packed.pClass, 0, enum_size<HeroClass>::value - 1));

	ClrPlrPath(player);
	player.destAction = ACTION_NONE;

	CopyUtf8(player._pName, packed.pName, sizeof(player._pName));

	InitPlayer(player, true);

	player._pBaseStr = std::min<uint8_t>(packed.pBaseStr, player.GetMaximumAttributeValue(CharacterAttribute::Strength));
	player._pStrength = player._pBaseStr;
	player._pBaseMag = std::min<uint8_t>(packed.pBaseMag, player.GetMaximumAttributeValue(CharacterAttribute::Magic));
	player._pMagic = player._pBaseMag;
	player._pBaseDex = std::min<uint8_t>(packed.pBaseDex, player.GetMaximumAttributeValue(CharacterAttribute::Dexterity));
	player._pDexterity = player._pBaseDex;
	player._pBaseVit = std::min<uint8_t>(packed.pBaseVit, player.GetMaximumAttributeValue(CharacterAttribute::Vitality));
	player._pVitality = player._pBaseVit;
	player._pStatPts = packed.pStatPts;
	player._pStatPtsSpentStr = SDL_SwapLE32(packed.pStatPtsSpentStr);
	player._pStatPtsSpentMag = SDL_SwapLE32(packed.pStatPtsSpentMag);
	player._pStatPtsSpentDex = SDL_SwapLE32(packed.pStatPtsSpentDex);
	player._pStatPtsSpentVit = SDL_SwapLE32(packed.pStatPtsSpentVit);
	UnpackWaypointUnlocked(player, DIFF_NORMAL, packed.pWaypointUnlockedNormal);
	UnpackWaypointUnlocked(player, DIFF_NIGHTMARE, packed.pWaypointUnlockedNightmare);
	UnpackWaypointUnlocked(player, DIFF_HELL, packed.pWaypointUnlockedHell);
	UnpackWaypointUnlocked(player, DIFF_TORMENT, packed.pWaypointUnlockedTorment);

	player._pExperience = SDL_SwapLE64(packed.pExperience);
	player._pGold = SDL_SwapLE32(packed.pGold);
	player._pBaseToBlk = PlayersData[static_cast<std::size_t>(player._pClass)].blockBonus;
	if ((int)(player._pHPBase & 0xFFFFFFC0) < 64)
		player._pHPBase = 64;

	player._pMaxManaBase = SDL_SwapLE32(packed.pMaxManaBase);
	player._pManaBase = SDL_SwapLE32(packed.pManaBase);
	player._pManaBase = std::min<int32_t>(player._pManaBase, player._pMaxManaBase);
	player._pMemSpells = SDL_SwapLE64(packed.pMemSpells);

	for (int i = 0; i < 37; i++) // Should be MAX_SPELLS but set to 36 to make save games compatible
		player._pSplLvl[i] = packed.pSplLvl[i];
	for (int i = 37; i < 47; i++)
		player._pSplLvl[i] = packed.pSplLvl2[i - 37];

	bool isHellfire = packed.bIsHellfire != 0;

	for (int i = 0; i < NUM_INVLOC; i++)
		UnPackItem(packed.InvBody[i], player, player.InvBody[i], isHellfire);

	// CLAMPED, not copied. `packed._pNumInv` is a uint8_t straight off disk and both InvList arrays
	// hold InventoryGridCells (70) entries, so a corrupt or crafted save saying 255 walked the loop
	// below 185 entries past the end of BOTH of them - reading one array out of bounds and writing
	// the other (external audit, 2026-08-25, P0).
	//
	// A save is untrusted input even when this program wrote it: files get truncated, copied
	// half-way, edited by hand and restored from backups of a different build.
	if (packed._pNumInv > InventoryGridCells) {
		LogError("Hero has an impossible inventory count ({}); clamped to {}",
		    packed._pNumInv, InventoryGridCells);
	}
	player._pNumInv = std::min<int>(packed._pNumInv, InventoryGridCells);
	for (int i = 0; i < player._pNumInv; i++)
		UnPackItem(packed.InvList[i], player, player.InvList[i], isHellfire);

	// The grid holds 1-based InvList references (negative for an item's continuation cells), so a
	// corrupt entry indexes InvList directly wherever it is read - CheckInvHLight, the draw, the
	// paste. Anything pointing past the live item count is cleared rather than trusted: an empty
	// cell is always safe, and a stale reference is not.
	for (int i = 0; i < InventoryGridCells; i++) {
		const int8_t cell = packed.InvGrid[i];
		const int referenced = std::abs(static_cast<int>(cell));
		player.InvGrid[i] = (referenced > player._pNumInv) ? 0 : cell;
	}

	VerifyGoldSeeds(player);

	for (int i = 0; i < MaxBeltItems; i++)
		UnPackItem(packed.SpdList[i], player, player.SpdList[i], isHellfire);

	CalcPlrInv(player, false);
	player.wReflections = SDL_SwapLE16(packed.wReflections);
	player.pDiabloKillLevel = SDL_SwapLE32(packed.pDiabloKillLevel);

	// Last, and not one line earlier: the readied spells are validated against _pAblSpells and
	// _pMemSpells (set by InitPlayer and above), and CalcPlrInv is what auto-readies an equipped
	// staff's charged spell - a hero saved with nothing readied must keep that staff, so decoding
	// has to happen after it and must leave both pairs alone when the byte is zero.
	oracool::UnpackReadiedSpell(player, packed.pReadiedSpellRight, player._pRSpell, player._pRSplType);
	oracool::UnpackReadiedSpell(player, packed.pReadiedSpellLeft, player._pLRSpell, player._pLRSplType);
}

bool UnPackNetItem(const Player &player, const ItemNetPack &packedItem, Item &item)
{
	item = {};
	_item_indexes idx = static_cast<_item_indexes>(SDL_SwapLE16(packedItem.def.wIndx));
	if (idx < 0 || idx > IDI_LAST)
		return true;
	if (idx == IDI_EAR) {
		RecreateEar(item, SDL_SwapLE16(packedItem.ear.wCI), SDL_SwapLE32(packedItem.ear.dwSeed), packedItem.ear.bCursval, packedItem.ear.heroname);
		return true;
	}

	uint16_t creationFlags = SDL_SwapLE16(packedItem.item.wCI);
	uint32_t dwBuff = SDL_SwapLE16(packedItem.item.dwBuff);
	if (idx != IDI_GOLD)
		ValidateField(creationFlags, IsCreationFlagComboValid(creationFlags));
	if ((creationFlags & CF_TOWN) != 0)
		ValidateField(creationFlags, IsTownItemValid(creationFlags));
	else if ((creationFlags & CF_USEFUL) == CF_UPER15)
		ValidateFields(creationFlags, dwBuff, IsUniqueMonsterItemValid(creationFlags, dwBuff));
	else if ((dwBuff & CF_HELLFIRE) != 0 && AllItemsList[idx].iMiscId == IMISC_BOOK)
		return RecreateHellfireSpellBook(player, packedItem.item, &item);
	else
		ValidateFields(creationFlags, dwBuff, IsDungeonItemValid(creationFlags, dwBuff));

	RecreateItem(player, packedItem.item, item);
	return true;
}

bool UnPackNetPlayer(const PlayerNetPack &packed, Player &player)
{
	CopyUtf8(player._pName, packed.pName, sizeof(player._pName));
	ValidateField(packed.pName, UiValidPlayerName(player._pName));

	ValidateField(packed.pClass, packed.pClass < enum_size<HeroClass>::value);
	player._pClass = static_cast<HeroClass>(packed.pClass);

	Point position { packed.px, packed.py };
	ValidateFields(position.x, position.y, InDungeonBounds(position));
	ValidateField(packed.plrlevel, packed.plrlevel < NUMLEVELS);
	ValidateField(packed.pLevel, packed.pLevel >= 1 && packed.pLevel <= MaxCharacterLevel);

	int32_t baseHpMax = SDL_SwapLE32(packed.pMaxHPBase);
	int32_t baseHp = SDL_SwapLE32(packed.pHPBase);
	int32_t hpMax = SDL_SwapLE32(packed.pMaxHP);
	ValidateFields(baseHp, baseHpMax, baseHp >= (baseHpMax - hpMax) && baseHp <= baseHpMax);

	int32_t baseManaMax = SDL_SwapLE32(packed.pMaxManaBase);
	int32_t baseMana = SDL_SwapLE32(packed.pManaBase);
	ValidateFields(baseMana, baseManaMax, baseMana <= baseManaMax);

	ValidateFields(packed.pClass, packed.pBaseStr, packed.pBaseStr <= player.GetMaximumAttributeValue(CharacterAttribute::Strength));
	ValidateFields(packed.pClass, packed.pBaseMag, packed.pBaseMag <= player.GetMaximumAttributeValue(CharacterAttribute::Magic));
	ValidateFields(packed.pClass, packed.pBaseDex, packed.pBaseDex <= player.GetMaximumAttributeValue(CharacterAttribute::Dexterity));
	ValidateFields(packed.pClass, packed.pBaseVit, packed.pBaseVit <= player.GetMaximumAttributeValue(CharacterAttribute::Vitality));

	ValidateField(packed._pNumInv, packed._pNumInv <= InventoryGridCells);

	player._pLevel = packed.pLevel;
	player.position.tile = position;
	player.position.future = position;
	player.plrlevel = packed.plrlevel;
	player.plrIsOnSetLevel = packed.isOnSetLevel != 0;
	player._pMaxHPBase = baseHpMax;
	player._pHPBase = baseHp;
	player._pMaxHP = baseHpMax;
	player._pHitPoints = baseHp;

	ClrPlrPath(player);
	player.destAction = ACTION_NONE;

	InitPlayer(player, true);

	player._pBaseStr = packed.pBaseStr;
	player._pStrength = player._pBaseStr;
	player._pBaseMag = packed.pBaseMag;
	player._pMagic = player._pBaseMag;
	player._pBaseDex = packed.pBaseDex;
	player._pDexterity = player._pBaseDex;
	player._pBaseVit = packed.pBaseVit;
	player._pVitality = player._pBaseVit;
	player._pStatPts = packed.pStatPts;

	player._pExperience = SDL_SwapLE64(packed.pExperience);
	player._pBaseToBlk = PlayersData[static_cast<std::size_t>(player._pClass)].blockBonus;
	player._pMaxManaBase = baseManaMax;
	player._pManaBase = baseMana;
	player._pMemSpells = SDL_SwapLE64(packed.pMemSpells);
	player.wReflections = SDL_SwapLE16(packed.wReflections);
	player.pDiabloKillLevel = packed.pDiabloKillLevel;
	player.pManaShield = packed.pManaShield != 0;
	player.friendlyMode = packed.friendlyMode != 0;

	// Only the 64 the player actually has: the old loop wrote the packet's 62 extra bytes over
	// whatever followed _pSplLvl in Player (external audit, 2026-09-06: NET-01).
	for (size_t i = 0; i < std::size(player._pSplLvl); i++)
		player._pSplLvl[i] = packed.pSplLvl[i];

	for (int i = 0; i < NUM_INVLOC; i++) {
		if (!UnPackNetItem(player, packed.InvBody[i], player.InvBody[i]))
			return false;
		if (player.InvBody[i].isEmpty())
			continue;
		auto loc = static_cast<int8_t>(player.GetItemLocation(player.InvBody[i]));
		switch (i) {
		case INVLOC_HEAD:
			ValidateField(loc, loc == ILOC_HELM);
			break;
		case INVLOC_RING_LEFT:
		case INVLOC_RING_RIGHT:
			ValidateField(loc, loc == ILOC_RING);
			break;
		case INVLOC_AMULET:
			ValidateField(loc, loc == ILOC_AMULET);
			break;
		case INVLOC_HAND_LEFT:
		case INVLOC_HAND_RIGHT:
			ValidateField(loc, IsAnyOf(loc, ILOC_ONEHAND, ILOC_TWOHAND));
			break;
		case INVLOC_CHEST:
			ValidateField(loc, loc == ILOC_ARMOR);
			break;
		// Oracool: the six new worn slots, found by the same SLOTXY_CHEST/INVLOC_CHEST audit grep
		// as the hover and un-equip chains. Without these cases a peer's pack passes with anything
		// in these slots - not a crash, but the only slot-consistency check multiplayer has.
		case INVLOC_SHOULDERS:
			ValidateField(loc, loc == ILOC_SHOULDERS);
			break;
		case INVLOC_BRACERS:
			ValidateField(loc, loc == ILOC_BRACERS);
			break;
		case INVLOC_GLOVES:
			ValidateField(loc, loc == ILOC_GLOVES);
			break;
		case INVLOC_WAIST:
			ValidateField(loc, loc == ILOC_WAIST);
			break;
		case INVLOC_LEGS:
			ValidateField(loc, loc == ILOC_LEGS);
			break;
		case INVLOC_BOOTS:
			ValidateField(loc, loc == ILOC_BOOTS);
			break;
		}
	}

	// REJECT rather than clamp, because this function already answers false for a packet it does not
	// believe and every caller handles that. An inventory count past the array is not a value to
	// salvage; it is a packet that is not what it claims to be.
	if (packed._pNumInv > InventoryGridCells)
		return false;
	player._pNumInv = packed._pNumInv;
	for (int i = 0; i < player._pNumInv; i++) {
		if (!UnPackNetItem(player, packed.InvList[i], player.InvList[i]))
			return false;
	}

	// Grid references past the live item count are cleared - see the matching walk in UnPackPlayer.
	for (int i = 0; i < InventoryGridCells; i++) {
		const int8_t cell = packed.InvGrid[i];
		player.InvGrid[i] = (std::abs(static_cast<int>(cell)) > player._pNumInv) ? 0 : cell;
	}

	for (int i = 0; i < MaxBeltItems; i++) {
		Item &item = player.SpdList[i];
		if (!UnPackNetItem(player, packed.SpdList[i], item))
			return false;
		if (item.isEmpty())
			continue;
		Size beltItemSize = GetInventorySize(item);
		int8_t beltItemType = static_cast<int8_t>(item._itype);
		bool beltItemUsable = item.isUsable();
		ValidateFields(beltItemSize.width, beltItemSize.height, (beltItemSize == Size { 1, 1 }));
		ValidateField(beltItemType, item._itype != ItemType::Gold);
		ValidateField(beltItemUsable, beltItemUsable);
	}

	CalcPlrInv(player, false);
	player._pGold = CalculateGold(player);

	ValidateFields(player._pStrength, SDL_SwapLE32(packed.pStrength), player._pStrength == SDL_SwapLE32(packed.pStrength));
	ValidateFields(player._pMagic, SDL_SwapLE32(packed.pMagic), player._pMagic == SDL_SwapLE32(packed.pMagic));
	ValidateFields(player._pDexterity, SDL_SwapLE32(packed.pDexterity), player._pDexterity == SDL_SwapLE32(packed.pDexterity));
	ValidateFields(player._pVitality, SDL_SwapLE32(packed.pVitality), player._pVitality == SDL_SwapLE32(packed.pVitality));
	ValidateFields(player._pHitPoints, SDL_SwapLE32(packed.pHitPoints), player._pHitPoints == SDL_SwapLE32(packed.pHitPoints));
	ValidateFields(player._pMaxHP, SDL_SwapLE32(packed.pMaxHP), player._pMaxHP == SDL_SwapLE32(packed.pMaxHP));
	ValidateFields(player._pMana, SDL_SwapLE32(packed.pMana), player._pMana == SDL_SwapLE32(packed.pMana));
	ValidateFields(player._pMaxMana, SDL_SwapLE32(packed.pMaxMana), player._pMaxMana == SDL_SwapLE32(packed.pMaxMana));
	ValidateFields(player._pDamageMod, SDL_SwapLE32(packed.pDamageMod), player._pDamageMod == SDL_SwapLE32(packed.pDamageMod));
	ValidateFields(player._pBaseToBlk, SDL_SwapLE32(packed.pBaseToBlk), player._pBaseToBlk == SDL_SwapLE32(packed.pBaseToBlk));
	ValidateFields(player._pIMinDam, SDL_SwapLE32(packed.pIMinDam), player._pIMinDam == SDL_SwapLE32(packed.pIMinDam));
	ValidateFields(player._pIMaxDam, SDL_SwapLE32(packed.pIMaxDam), player._pIMaxDam == SDL_SwapLE32(packed.pIMaxDam));
	ValidateFields(player._pIAC, SDL_SwapLE32(packed.pIAC), player._pIAC == SDL_SwapLE32(packed.pIAC));
	ValidateFields(player._pIBonusDam, SDL_SwapLE32(packed.pIBonusDam), player._pIBonusDam == SDL_SwapLE32(packed.pIBonusDam));
	ValidateFields(player._pIBonusToHit, SDL_SwapLE32(packed.pIBonusToHit), player._pIBonusToHit == SDL_SwapLE32(packed.pIBonusToHit));
	ValidateFields(player._pIBonusAC, SDL_SwapLE32(packed.pIBonusAC), player._pIBonusAC == SDL_SwapLE32(packed.pIBonusAC));
	ValidateFields(player._pIBonusDamMod, SDL_SwapLE32(packed.pIBonusDamMod), player._pIBonusDamMod == SDL_SwapLE32(packed.pIBonusDamMod));
	ValidateFields(player._pIGetHit, SDL_SwapLE32(packed.pIGetHit), player._pIGetHit == SDL_SwapLE32(packed.pIGetHit));
	ValidateFields(player._pIEnAc, SDL_SwapLE32(packed.pIEnAc), player._pIEnAc == SDL_SwapLE32(packed.pIEnAc));
	ValidateFields(player._pIFMinDam, SDL_SwapLE32(packed.pIFMinDam), player._pIFMinDam == SDL_SwapLE32(packed.pIFMinDam));
	ValidateFields(player._pIFMaxDam, SDL_SwapLE32(packed.pIFMaxDam), player._pIFMaxDam == SDL_SwapLE32(packed.pIFMaxDam));
	ValidateFields(player._pILMinDam, SDL_SwapLE32(packed.pILMinDam), player._pILMinDam == SDL_SwapLE32(packed.pILMinDam));
	ValidateFields(player._pILMaxDam, SDL_SwapLE32(packed.pILMaxDam), player._pILMaxDam == SDL_SwapLE32(packed.pILMaxDam));
	ValidateFields(player._pMaxHPBase, player.calculateBaseLife(), player._pMaxHPBase <= player.calculateBaseLife());
	ValidateFields(player._pMaxManaBase, player.calculateBaseMana(), player._pMaxManaBase <= player.calculateBaseMana());

	return true;
}

} // namespace devilution
