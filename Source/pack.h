/**
 * @file pack.h
 *
 * Interface of functions for minifying player data structure.
 */
#pragma once

#include <cstdint>

#include "inv.h"
#include "items.h"
#include "msg.h"
#include "player.h"

namespace devilution {

#pragma pack(push, 1)
struct ItemPack {
	uint32_t iSeed;
	uint16_t iCreateInfo;
	uint16_t idx;
	uint8_t bId;
	uint8_t bDur;
	uint8_t bMDur;
	uint8_t bCh;
	uint8_t bMCh;
	uint16_t wValue;
	uint32_t dwBuff;
};

struct PlayerPack {
	uint32_t dwLowDateTime;
	uint32_t dwHighDateTime;
	int8_t destAction;
	int8_t destParam1;
	int8_t destParam2;
	uint8_t plrlevel;
	uint8_t px;
	uint8_t py;
	uint8_t targx;
	uint8_t targy;
	char pName[PlayerNameLength];
	uint8_t pClass;
	uint8_t pBaseStr;
	uint8_t pBaseMag;
	uint8_t pBaseDex;
	uint8_t pBaseVit;
	int8_t pLevel;
	uint8_t pStatPts;
	/** @brief Oracool: widened to uint64_t - the extended level-99 curve exceeds UINT32_MAX. */
	uint64_t pExperience;
	int32_t pGold;
	int32_t pHPBase;
	int32_t pMaxHPBase;
	int32_t pManaBase;
	int32_t pMaxManaBase;
	uint8_t pSplLvl[37]; // Should be MAX_SPELLS but set to 37 to make save games compatible
	uint64_t pMemSpells;
	ItemPack InvBody[NUM_INVLOC];
	ItemPack InvList[InventoryGridCells];
	int8_t InvGrid[InventoryGridCells];
	uint8_t _pNumInv;
	ItemPack SpdList[MaxBeltItems];
	int8_t pTownWarps;
	int8_t pDungMsgs;
	int8_t pLvlLoad;
	/**
	 * @brief Oracool: user request (2026-08-15) - "make the readied spells persist across saves".
	 * The RIGHT mouse button's readied spell; pReadiedSpellLeft below is the left button's.
	 *
	 * Repurposed from pBattleNet rather than appended, deliberately. Growing this struct changes
	 * sizeof(PlayerPack) and pfile.cpp's ReadHero accepts only an exact size match, so appending
	 * would have invalidated every existing hero for a convenience - see the waypoint comment below
	 * for the one time that price was worth paying. pBattleNet was dead weight instead: PackPlayer
	 * memsets it and never writes it, loadsave.cpp only file.Skip(1)s past it, and the sole writer
	 * that ever set it was the original 1.09 game, whose .sv heroes this fork stopped being able to
	 * read when it moved to Hellfire's .hsv. UnPackPlayer validates what it reads regardless.
	 *
	 * Encoding lives in oracool/readied_spells.cpp: 0 means nothing readied - which is what the
	 * memset already wrote, so every pre-existing hero decodes correctly - and any other value is
	 * the SpellID plus one. The spell TYPE is not stored; it is re-derived from the character's own
	 * skills and memorised spells, which costs no bytes and drops bindings they no longer have.
	 */
	uint8_t pReadiedSpellRight; // was pBattleNet
	uint8_t pManaShield;
	uint8_t pDungMsgs2;
	/** The format the charater is in, 0: Diablo, 1: Hellfire */
	int8_t bIsHellfire;
	uint8_t pReadiedSpellLeft; // was reserved; same encoding as pReadiedSpellRight above
	uint16_t wReflections;
	/**
	 * @brief Oracool: user request - per-difficulty waypoint unlock bitmask, same repurposed-
	 * reserved-bytes pattern as pStatPtsSpent* below (same reasoning: starting a New Game with an
	 * existing hero never re-reads the full save via loadsave.cpp's LoadPlayer, only "Continue"
	 * does, so anything meant to survive a New Game has to live in this compact struct too). Bit
	 * (i-1) is waypoint list index i (1-24, matching currlevel numbering); index 0 (Tristram) is
	 * always unlocked and never stored. See player.h's Player::_pWaypointUnlocked and
	 * pack.cpp's Pack/UnPackPlayer for where these get read/written.
	 *
	 * WIDENED 16 -> 32 BITS at 1.5.0 for Hellfire's Nest and Crypt waypoints (levels 17-24). This
	 * is the first change to actually GROW this struct rather than repurpose spare bytes inside it,
	 * and pfile.cpp's ReadHero only accepts a file whose size matches sizeof(PlayerPack) exactly,
	 * so every character saved before it stopped loading. That was affordable precisely then and
	 * probably never again: adding hellfire.mpq had just moved saves from .sv to .hsv, so the old
	 * files were already out of reach. Any further waypoint growth is free (8 spare bits); anything
	 * else wanting space here should still hunt for reserved bytes first.
	 */
	uint32_t pWaypointUnlockedNormal;    // was reserved2[2], widened at 1.5.0
	uint8_t pSplLvl2[10];                // Hellfire spells
	uint32_t pWaypointUnlockedNightmare; // was wReserved8, widened at 1.5.0
	uint32_t pDiabloKillLevel;
	uint32_t pDifficulty;
	uint32_t pDamAcFlags;  // `ItemSpecialEffectHf` is 1 byte but this is 4 bytes.
	/**
	 * @brief Oracool Reset Stats: repurposes 16 of these 20 previously-inert bytes (same pattern
	 * as loadsave.cpp's SavePlayer/LoadPlayer) so starting a New Game with an existing hero
	 * carries over manually-spent stat points too - previously only "Load Game" preserved them,
	 * since it re-reads the full save via LoadPlayer after this compact struct is unpacked, while
	 * starting a fresh New Game never does, silently losing the tracking. See player.h.
	 */
	int32_t pStatPtsSpentStr;
	int32_t pStatPtsSpentMag;
	int32_t pStatPtsSpentDex;
	int32_t pStatPtsSpentVit;
	uint32_t pWaypointUnlockedHell;    // was part of reserved3[4], widened at 1.5.0
	uint32_t pWaypointUnlockedTorment; // was part of reserved3[4], widened at 1.5.0
};

union ItemNetPack {
	TItemDef def;
	TItem item;
	TEar ear;
};

struct PlayerNetPack {
	uint8_t plrlevel;
	uint8_t px;
	uint8_t py;
	char pName[PlayerNameLength];
	uint8_t pClass;
	uint8_t pBaseStr;
	uint8_t pBaseMag;
	uint8_t pBaseDex;
	uint8_t pBaseVit;
	int8_t pLevel;
	uint8_t pStatPts;
	/** @brief Oracool: widened to uint64_t - the extended level-99 curve exceeds UINT32_MAX. */
	uint64_t pExperience;
	int32_t pHPBase;
	int32_t pMaxHPBase;
	int32_t pManaBase;
	int32_t pMaxManaBase;
	uint8_t pSplLvl[MAX_SPELLS];
	uint64_t pMemSpells;
	ItemNetPack InvBody[NUM_INVLOC];
	ItemNetPack InvList[InventoryGridCells];
	int8_t InvGrid[InventoryGridCells];
	uint8_t _pNumInv;
	ItemNetPack SpdList[MaxBeltItems];
	uint8_t pManaShield;
	uint16_t wReflections;
	uint8_t pDiabloKillLevel;
	uint8_t friendlyMode;
	uint8_t isOnSetLevel;

	// For validation
	int32_t pStrength;
	int32_t pMagic;
	int32_t pDexterity;
	int32_t pVitality;
	int32_t pHitPoints;
	int32_t pMaxHP;
	int32_t pMana;
	int32_t pMaxMana;
	int32_t pDamageMod;
	int32_t pBaseToBlk;
	int32_t pIMinDam;
	int32_t pIMaxDam;
	int32_t pIAC;
	int32_t pIBonusDam;
	int32_t pIBonusToHit;
	int32_t pIBonusAC;
	int32_t pIBonusDamMod;
	int32_t pIGetHit;
	int32_t pIEnAc;
	int32_t pIFMinDam;
	int32_t pIFMaxDam;
	int32_t pILMinDam;
	int32_t pILMaxDam;
};
#pragma pack(pop)

bool RecreateHellfireSpellBook(const Player &player, const TItem &packedItem, Item *item = nullptr);
void PackPlayer(PlayerPack &pPack, const Player &player);
void UnPackPlayer(const PlayerPack &pPack, Player &player);
void PackNetPlayer(PlayerNetPack &packed, const Player &player);
bool UnPackNetPlayer(const PlayerNetPack &packed, Player &player);

/**
 * @brief Save the attributes needed to recreate this item into an ItemPack struct
 *
 * @param packedItem The destination packed struct
 * @param item The source item
 * @param isHellfire Whether the item is from Hellfire or not
 */
void PackItem(ItemPack &packedItem, const Item &item, bool isHellfire);

/**
 * Expand a ItemPack in to a Item
 *
 * @param packedItem The source packed item
 * @param item The destination item
 * @param isHellfire Whether the item is from Hellfire or not
 */
void UnPackItem(const ItemPack &packedItem, const Player &player, Item &item, bool isHellfire);

/**
 * @brief Save the attributes needed to recreate this item into an ItemNetPack struct
 * @param item The source item
 * @param packedItem The destination packed struct
 */
void PackNetItem(const Item &item, ItemNetPack &packedItem);

/**
 * @brief Expand a ItemPack in to a Item
 * @param player The player holding the item
 * @param packedItem The source packed item
 * @param item The destination item
 * @return True if the item is valid
 */
bool UnPackNetItem(const Player &player, const ItemNetPack &packedItem, Item &item);

} // namespace devilution
