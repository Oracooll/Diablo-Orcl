#include "player_test.h"

#include <cstdint>
#include <cstdio>
#include <optional>
#include <vector>

#include <SDL_endian.h>
#include <gtest/gtest.h>
#include <picosha2.h>

#include "loadsave.h"
#include "mpq/mpq_reader.hpp"
#include "mpq/mpq_writer.hpp"
#include "pack.h"
#include "oracool/save_status.h"
#include "pfile.h"
#include "qol/stash.h"
#include "utils/file_util.h"
#include "utils/paths.h"

#include "isolated_pref_path.hpp"

namespace devilution {
namespace {

constexpr int SpellDatVanilla[] = {
	0, 1, 1, 4, 5, -1, 3, 3, 6, -1, 7, 6, 8, 9,
	8, 9, -1, -1, -1, -1, 3, 11, -1, 14, -1, -1,
	-1, -1, -1, 8, 1, 1, -1, 2, 1, 14, 9
};

void SwapLE(ItemPack &pack)
{
	pack.iSeed = SDL_SwapLE32(pack.iSeed);
	pack.iCreateInfo = SDL_SwapLE16(pack.iCreateInfo);
	pack.idx = SDL_SwapLE16(pack.idx);
	pack.wValue = SDL_SwapLE16(pack.wValue);
	pack.dwBuff = SDL_SwapLE32(pack.dwBuff);
}

void SwapLE(PlayerPack &player)
{
	player.dwLowDateTime = SDL_SwapLE32(player.dwLowDateTime);
	player.dwHighDateTime = SDL_SwapLE32(player.dwHighDateTime);
	player.pExperience = SDL_SwapLE64(player.pExperience);
	player.pGold = SDL_SwapLE32(player.pGold);
	player.pHPBase = SDL_SwapLE32(player.pHPBase);
	player.pMaxHPBase = SDL_SwapLE32(player.pMaxHPBase);
	player.pManaBase = SDL_SwapLE32(player.pManaBase);
	player.pMaxManaBase = SDL_SwapLE32(player.pMaxManaBase);
	player.pMemSpells = SDL_SwapLE64(player.pMemSpells);
	for (ItemPack &item : player.InvBody) {
		SwapLE(item);
	}
	for (ItemPack &item : player.InvList) {
		SwapLE(item);
	}
	for (ItemPack &item : player.SpdList) {
		SwapLE(item);
	}
	player.wReflections = SDL_SwapLE16(player.wReflections);
	// Oracool: the four waypoint masks widened to 32 bits at 1.5.0 for Hellfire's Nest and Crypt.
	// A no-op on a little-endian host - and the test's fixture leaves them zero - but this helper
	// exists to mirror the struct, and run_big_endian_tests.sh is where that stops being academic.
	player.pWaypointUnlockedNormal = SDL_SwapLE32(player.pWaypointUnlockedNormal);
	player.pWaypointUnlockedNightmare = SDL_SwapLE32(player.pWaypointUnlockedNightmare);
	player.pWaypointUnlockedHell = SDL_SwapLE32(player.pWaypointUnlockedHell);
	player.pWaypointUnlockedTorment = SDL_SwapLE32(player.pWaypointUnlockedTorment);
	player.pDiabloKillLevel = SDL_SwapLE32(player.pDiabloKillLevel);
	player.pDifficulty = SDL_SwapLE32(player.pDifficulty);
	player.pDamAcFlags = SDL_SwapLE32(player.pDamAcFlags);
}

void PackItemUnique(ItemPack *id, int idx)
{
	id->idx = idx;
	id->iCreateInfo = 0x2DE;
	id->bId = 1 + 2 * ITEM_QUALITY_UNIQUE;
	id->bDur = 40;
	id->bMDur = 40;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x1C0C44B0;
}

void PackItemStaff(ItemPack *id)
{
	id->idx = 150;
	id->iCreateInfo = 0x2010;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 75;
	id->bMDur = 75;
	id->bCh = 12;
	id->bMCh = 12;
	id->iSeed = 0x2A15243F;
}

void PackItemBow(ItemPack *id)
{
	id->idx = 145;
	id->iCreateInfo = 0x0814;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 60;
	id->bMDur = 60;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x449D8992;
}

void PackItemSword(ItemPack *id)
{
	id->idx = 122;
	id->iCreateInfo = 0x081E;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 60;
	id->bMDur = 60;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x680FAC02;
}

void PackItemRing1(ItemPack *id)
{
	id->idx = 153;
	id->iCreateInfo = 0xDE;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 0;
	id->bMDur = 0;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x5B41AFA8;
}

void PackItemRing2(ItemPack *id)
{
	id->idx = 153;
	id->iCreateInfo = 0xDE;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 0;
	id->bMDur = 0;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x1E41FEFC;
}

void PackItemAmulet(ItemPack *id)
{
	id->idx = 155;
	id->iCreateInfo = 0xDE;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 0;
	id->bMDur = 0;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x70A0383A;
}

void PackItemArmor(ItemPack *id)
{
	id->idx = 70;
	id->iCreateInfo = 0xDE;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 90;
	id->bMDur = 90;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x63AAC49B;
}

void PackItemFullRejuv(ItemPack *id, int i)
{
	const uint32_t seeds[] = { 0x7C253335, 0x3EEFBFF8, 0x76AFB1A9, 0x38EB45FE, 0x1154E197, 0x5964B644, 0x76B58BEB, 0x002A6E5A };
	id->idx = ItemMiscIdIdx(IMISC_FULLREJUV);
	id->iSeed = seeds[i];
	id->iCreateInfo = 0;
	id->bId = 2 * ITEM_QUALITY_NORMAL;
	id->bDur = 0;
	id->bMDur = 0;
	id->bCh = 0;
	id->bMCh = 0;
}

int PrepareInvSlot(PlayerPack *pPack, int pos, int size, int start = 0)
{
	static char ret = 0;
	if (start)
		ret = 0;
	++ret;
	if (size == 0) {
		pPack->InvGrid[pos] = ret;
	} else if (size == 1) {
		pPack->InvGrid[pos] = ret;
		pPack->InvGrid[pos - 10] = -ret;
		pPack->InvGrid[pos - 20] = -ret;
	} else if (size == 2) {
		pPack->InvGrid[pos] = ret;
		pPack->InvGrid[pos + 1] = -ret;
		pPack->InvGrid[pos - 10] = -ret;
		pPack->InvGrid[pos - 10 + 1] = -ret;
		pPack->InvGrid[pos - 20] = -ret;
		pPack->InvGrid[pos - 20 + 1] = -ret;
	} else if (size == 3) {
		pPack->InvGrid[pos] = ret;
		pPack->InvGrid[pos + 1] = -ret;
		pPack->InvGrid[pos - 10] = -ret;
		pPack->InvGrid[pos - 10 + 1] = -ret;
	} else {
		abort();
	}
	return ret - 1;
}

void PackPlayerTest(PlayerPack *pPack)
{
	memset(pPack, 0, sizeof(*pPack));
	pPack->destAction = -1;
	pPack->destParam1 = 0;
	pPack->destParam2 = 0;
	pPack->plrlevel = 0;
	pPack->pExperience = 1583495809;
	pPack->pLevel = 50;
	pPack->px = 75;
	pPack->py = 68;
	pPack->targx = 75;
	pPack->targy = 68;
	pPack->pGold = 0;
	pPack->pStatPts = 0;
	pPack->pDiabloKillLevel = 3;
	for (auto i = 0; i < InventoryGridCells; i++)
		pPack->InvList[i].idx = -1;
	for (auto i = 0; i < NUM_INVLOC; i++)
		pPack->InvBody[i].idx = -1;
	for (auto i = 0; i < MaxBeltItems; i++)
		PackItemFullRejuv(pPack->SpdList + i, i);
	for (auto i = 1; i < 37; i++) {
		if (SpellDatVanilla[i] != -1) {
			pPack->pMemSpells |= 1ULL << (i - 1);
			pPack->pSplLvl[i] = 15;
		}
	}
	for (auto i = 0; i < NUM_INVLOC; i++)
		pPack->InvBody[i].idx = -1;
	strcpy(pPack->pName, "TestPlayer");
	pPack->pClass = static_cast<uint8_t>(HeroClass::Rogue);
	pPack->pBaseStr = 20 + 35;
	pPack->pBaseMag = 15 + 55;
	pPack->pBaseDex = 30 + 220;
	pPack->pBaseVit = 20 + 60;
	pPack->pHPBase = ((20 + 10) << 6) + ((20 + 10) << 5) + 48 * 128 + (60 << 6);
	pPack->pMaxHPBase = pPack->pHPBase;
	pPack->pManaBase = (15 << 6) + (15 << 5) + 48 * 128 + (55 << 6);
	pPack->pMaxManaBase = pPack->pManaBase;

	PackItemUnique(pPack->InvBody + INVLOC_HEAD, 52);
	PackItemRing1(pPack->InvBody + INVLOC_RING_LEFT);
	PackItemRing2(pPack->InvBody + INVLOC_RING_RIGHT);
	PackItemAmulet(pPack->InvBody + INVLOC_AMULET);
	PackItemArmor(pPack->InvBody + INVLOC_CHEST);
	PackItemBow(pPack->InvBody + INVLOC_HAND_LEFT);

	PackItemStaff(pPack->InvList + PrepareInvSlot(pPack, 28, 2, 1));
	PackItemSword(pPack->InvList + PrepareInvSlot(pPack, 20, 1));

	pPack->_pNumInv = 2;

	SwapLE(*pPack);
}

void AssertPlayer(Player &player)
{
	ASSERT_EQ(CountU8(player._pSplLvl, 64), 23);
	ASSERT_EQ(Count8(player.InvGrid, InventoryGridCells), 9);
	ASSERT_EQ(CountItems(player.InvBody, NUM_INVLOC), 6);
	ASSERT_EQ(CountItems(player.InvList, InventoryGridCells), 2);
	ASSERT_EQ(CountItems(player.SpdList, MaxBeltItems), 8);
	ASSERT_EQ(CountItems(&player.HoldItem, 1), 0);

	ASSERT_EQ(player.position.tile.x, 75);
	ASSERT_EQ(player.position.tile.y, 68);
	ASSERT_EQ(player.position.future.x, 75);
	ASSERT_EQ(player.position.future.y, 68);
	ASSERT_EQ(player.plrlevel, 0);
	ASSERT_EQ(player.destAction, -1);
	ASSERT_STREQ(player._pName, "TestPlayer");
	ASSERT_EQ(player._pClass, HeroClass::Rogue);
	ASSERT_EQ(player._pBaseStr, 55);
	ASSERT_EQ(player._pStrength, 124);
	ASSERT_EQ(player._pBaseMag, 70);
	ASSERT_EQ(player._pMagic, 80);
	ASSERT_EQ(player._pBaseDex, 250);
	ASSERT_EQ(player._pDexterity, 281);
	ASSERT_EQ(player._pBaseVit, 80);
	ASSERT_EQ(player._pVitality, 90);
	ASSERT_EQ(player._pLevel, 50);
	ASSERT_EQ(player._pStatPts, 0);
	ASSERT_EQ(player._pExperience, 1583495809);
	ASSERT_EQ(player._pGold, 0);
	ASSERT_EQ(player._pMaxHPBase, 12864);
	ASSERT_EQ(player._pHPBase, 12864);
	ASSERT_EQ(player._pBaseToBlk, 20);
	ASSERT_EQ(player._pMaxManaBase, 11104);
	ASSERT_EQ(player._pManaBase, 11104);
	ASSERT_EQ(player._pMemSpells, 66309357295);
	ASSERT_EQ(player._pNumInv, 2);
	ASSERT_EQ(player.wReflections, 0);
	ASSERT_EQ(player.pTownWarps, 0);
	ASSERT_EQ(player.pDungMsgs, 0);
	ASSERT_EQ(player.pDungMsgs2, 0);
	ASSERT_EQ(player.pLvlLoad, 0);
	ASSERT_EQ(player.pDiabloKillLevel, 3);
	ASSERT_EQ(player.pManaShield, 0);
	ASSERT_EQ(player.pDamAcFlags, ItemSpecialEffectHf::None);

	ASSERT_EQ(player._pmode, 0);
	ASSERT_EQ(Count8(player.walkpath, MaxPathLength), 25);
	ASSERT_EQ(player._pgfxnum, 36);
	ASSERT_EQ(player.AnimInfo.ticksPerFrame, 4);
	ASSERT_EQ(player.AnimInfo.tickCounterOfCurrentFrame, 1);
	ASSERT_EQ(player.AnimInfo.numberOfFrames, 20);
	ASSERT_EQ(player.AnimInfo.currentFrame, 0);
	ASSERT_EQ(player.queuedSpell.spellId, SpellID::Invalid);
	ASSERT_EQ(player.queuedSpell.spellType, SpellType::Invalid);
	ASSERT_EQ(player.queuedSpell.spellFrom, 0);
	ASSERT_EQ(player.inventorySpell, SpellID::Null);
	ASSERT_EQ(player._pRSpell, SpellID::Invalid);
	ASSERT_EQ(player._pRSplType, SpellType::Invalid);
	ASSERT_EQ(player._pSBkSpell, SpellID::Invalid);
	// EMPTY since the six vanilla class skills were retired (2026-08-19) - see the same assertion in
	// player_test.cpp.
	ASSERT_EQ(player._pAblSpells, 0ULL);
	ASSERT_EQ(player._pScrlSpells, 0);
	ASSERT_EQ(player._pSpellFlags, SpellFlag::None);
	ASSERT_TRUE(player.UsesRangedWeapon());
	ASSERT_EQ(player._pBlockFlag, 0);
	ASSERT_EQ(player._pLightRad, 11);
	ASSERT_EQ(player._pDamageMod, 101);
	ASSERT_EQ(player._pHitPoints, 16640);
	ASSERT_EQ(player._pMaxHP, 16640);
	ASSERT_EQ(player._pMana, 14624);
	ASSERT_EQ(player._pMaxMana, 14624);
	ASSERT_EQ(player._pNextExper, 1530707109); // Oracool: level-51 threshold now that MaxCharacterLevel is 99, not the old level-50 cap value
	// CHANGED 2026-08-19 (v1.8.35). Two of these three were 75 - vanilla's hard cap, which this
	// character's gear was well past. The soft cap now lets the excess through at a third of its
	// value up to a ceiling of 90, so the pinned values move. The difficulty here is Normal, whose
	// penetration penalty is zero, so the whole delta is the soft cap and nothing else. Fire resist
	// is untouched at 16, which is the useful half of this assertion: a total BELOW the soft cap
	// must still be exactly what it always was.
	ASSERT_EQ(player._pMagResist, 89);
	ASSERT_EQ(player._pFireResist, 16);
	ASSERT_EQ(player._pLghtResist, 90);
	ASSERT_EQ(CountBool(player._pLvlVisited, NUMLEVELS), 0);
	ASSERT_EQ(CountBool(player._pSLvlVisited, NUMLEVELS), 0);
	ASSERT_EQ(player._pNFrames, 20);
	ASSERT_EQ(player._pWFrames, 8);
	ASSERT_EQ(player._pAFrames, 0);
	ASSERT_EQ(player._pAFNum, 0);
	ASSERT_EQ(player._pSFrames, 16);
	ASSERT_EQ(player._pSFNum, 12);
	ASSERT_EQ(player._pHFrames, 0);
	ASSERT_EQ(player._pDFrames, 20);
	ASSERT_EQ(player._pBFrames, 0);
	ASSERT_EQ(player._pIMinDam, 1);
	ASSERT_EQ(player._pIMaxDam, 14);
	ASSERT_EQ(player._pIAC, 115);
	ASSERT_EQ(player._pIBonusDam, 0);
	ASSERT_EQ(player._pIBonusToHit, 0);
	ASSERT_EQ(player._pIBonusAC, 0);
	ASSERT_EQ(player._pIBonusDamMod, 0);
	ASSERT_EQ(player._pISpells, 0);
	ASSERT_EQ(player._pIFlags, ItemSpecialEffect::None);
	ASSERT_EQ(player._pIGetHit, 0);
	ASSERT_EQ(player._pISplLvlAdd, 0);
	ASSERT_EQ(player._pIEnAc, 0);
	ASSERT_EQ(player._pIFMinDam, 0);
	ASSERT_EQ(player._pIFMaxDam, 0);
	ASSERT_EQ(player._pILMinDam, 0);
	ASSERT_EQ(player._pILMaxDam, 0);
	ASSERT_EQ(player.pOriginalCathedral, 0);
}

_uiheroinfo LoadedHero {};
bool LoadedHeroFound = false;

// Audit fix, 2026-08-26: MpqWriter::WriteFile now writes under a temporary name and renames it over
// the target, so a failed write cannot destroy the record it was replacing. That reshapes the
// archive's hash and block tables on every save, and the golden hash below cannot tell whether the
// result is still READABLE - it only says the bytes changed.
//
// So this reads it back. It is the test that would have caught a rename that produced a valid-
// looking archive nobody could open, which is the way this change could have gone wrong silently.
TEST(Writehero, HeroSurvivesAWriteAndReadsBack)
{
	UseIsolatedPrefPath();
	const std::string savePath = paths::PrefPath() + "multi_0.sv";
	RemoveFile(savePath.c_str());

	gbVanilla = true;
	gbIsHellfire = false;
	gbIsMultiplayer = true;
	gbIsHellfireSaveGame = false;
	leveltype = DTYPE_TOWN;
	giNumberOfLevels = 17;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	_uiheroinfo info {};
	info.heroclass = HeroClass::Rogue;
	pfile_ui_save_create(&info);
	PlayerPack pks;
	PackPlayerTest(&pks);
	UnPackPlayer(pks, *MyPlayer);

	// Written TWICE, deliberately. The first write creates the record; the second is the one that
	// has to replace an existing entry, which is the case the temp-and-rename path exists for and
	// the case the old code got wrong.
	MyPlayer->_pLevel = 7;
	pfile_write_hero(/*writeGameData=*/false);
	MyPlayer->_pLevel = 23;
	pfile_write_hero(/*writeGameData=*/false);

	ASSERT_TRUE(FileExists(savePath.c_str())) << "the save file is not there at all";

	// Read it back through the same path the game uses to list characters.
	Players[0] = {};
	MyPlayer = &Players[0];
	// A plain function pointer, so the result travels through file scope rather than a capture.
	pfile_ui_set_hero_infos(+[](_uiheroinfo *hero) -> bool {
		if (hero->name[0] != '\0') {
			LoadedHero = *hero;
			LoadedHeroFound = true;
		}
		return true;
	});
	EXPECT_TRUE(LoadedHeroFound) << "the written hero could not be read back - the archive is unreadable";
	EXPECT_EQ(LoadedHero.level, 23) << "the SECOND write did not replace the first";
	// Not asserting on strength: _pStrength is DERIVED (base plus item bonuses) while
	// PackPlayer stores the base, so it reads back as the recomputed total. The level is
	// the honest witness here - it proves both that the archive is readable AND that the
	// second write replaced the first.

	RemoveFile(savePath.c_str());
}

// The failure-injection test the second external audit asked for, and which could not be written at
// the time because nothing in the save path could be made to fail.
//
// It matters more than an ordinary regression test. MpqWriter::WriteFile was changed so that a
// failed write can no longer destroy the record it was replacing - and the only evidence for that
// was that saving still worked, which is evidence about the SUCCESS path. A guarantee about failure
// that has never seen a failure is a guess.
TEST(Writehero, AFailedSaveLeavesThePreviousOneIntact)
{
	UseIsolatedPrefPath();
	const std::string savePath = paths::PrefPath() + "multi_0.sv";
	RemoveFile(savePath.c_str());

	gbVanilla = true;
	gbIsHellfire = false;
	gbIsMultiplayer = true;
	gbIsHellfireSaveGame = false;
	leveltype = DTYPE_TOWN;
	giNumberOfLevels = 17;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	_uiheroinfo info {};
	info.heroclass = HeroClass::Rogue;
	pfile_ui_save_create(&info);
	PlayerPack pks;
	PackPlayerTest(&pks);
	UnPackPlayer(pks, *MyPlayer);

	// A good save the player would not want to lose.
	MyPlayer->_pLevel = 42;
	pfile_write_hero(/*writeGameData=*/false);
	ASSERT_TRUE(FileExists(savePath.c_str()));

	// Now the disk fills up partway through the next one. The count is deliberately small and not
	// zero: failing the very first write would be a disk that was already dead, and the dangerous
	// case is the one that fails PART WAY, after the writer has begun rearranging the archive.
	MyPlayer->_pLevel = 43;
	FailWritesAfter(2);
	pfile_write_hero(/*writeGameData=*/false);
	StopFailingWrites();

	// The file must still be there, and must still be the level-42 hero. Not a stub, not an empty
	// archive, not a hero with no "hero" record in it.
	ASSERT_TRUE(FileExists(savePath.c_str())) << "a failed save deleted the save file";

	Players[0] = {};
	MyPlayer = &Players[0];
	LoadedHeroFound = false;
	LoadedHero = {};
	pfile_ui_set_hero_infos(+[](_uiheroinfo *hero) -> bool {
		if (hero->name[0] != '\0') {
			LoadedHero = *hero;
			LoadedHeroFound = true;
		}
		return true;
	});

	EXPECT_TRUE(LoadedHeroFound)
	    << "a failed save destroyed the previous hero - the archive no longer holds one";
	EXPECT_EQ(LoadedHero.level, 42)
	    << "the failed save left the file holding neither the old hero nor a good new one";

	RemoveFile(savePath.c_str());
	StopFailingWrites();
}

// The transaction itself, tested where it lives rather than through the whole save stack.
//
// A first attempt swept an injected failure across `pfile_write_hero` and asserted the new level
// never appeared. That over-claimed twice over: past the last record the save legitimately
// SUCCEEDS, and a failure in the header/table write at the very end is reported after everything
// has already landed - a conservative report, which is the safe direction but not a torn save. The
// property the transaction actually provides is narrower and worth stating exactly:
//
//   several records written together are swapped in TOGETHER, or none of them are.
TEST(Writehero, ATransactionSwapsInEveryRecordOrNoneOfThem)
{
	const std::string archivePath = paths::PrefPath() + "txn_test.sv";
	RemoveFile(archivePath.c_str());

	const auto bytes = [](const char *s) { return reinterpret_cast<const byte *>(s); };

	// A first, complete save: two records that belong together.
	{
		MpqWriter writer(archivePath);
		writer.BeginTransaction();
		ASSERT_TRUE(writer.WriteFile("recordA", bytes("OLD-A"), 5));
		ASSERT_TRUE(writer.WriteFile("recordB", bytes("OLD-B"), 5));
		EXPECT_TRUE(writer.CommitTransaction());
		EXPECT_TRUE(writer.HasFile("recordA"));
		EXPECT_TRUE(writer.HasFile("recordB"));
	}

	// Now a save where the SECOND record fails. The first one wrote perfectly - and under
	// per-record atomicity alone it would have been swapped in on its own, leaving recordA new and
	// recordB old. That mixture is the torn save.
	{
		MpqWriter writer(archivePath);
		ASSERT_TRUE(writer.HasFile("recordA")) << "the first save did not survive being reopened";

		writer.BeginTransaction();
		EXPECT_TRUE(writer.WriteFile("recordA", bytes("NEW-A"), 5));
		FailWritesAfter(0);
		EXPECT_FALSE(writer.WriteFile("recordB", bytes("NEW-B"), 5))
		    << "the injected failure did not take effect";
		StopFailingWrites();

		EXPECT_FALSE(writer.CommitTransaction())
		    << "a transaction with a failed record committed anyway";

		// Both originals are still the LIVE records - and this reads their CONTENT, not merely
		// whether a record of that name exists. A first version of this test asked HasFile, which
		// is true either way: with staging disabled recordA is swapped in with its NEW bytes under
		// the same name. The test passed against the broken behaviour it was written to catch.
	}
	{
		int32_t error = 0;
		std::optional<MpqArchive> archive = MpqArchive::Open(archivePath.c_str(), error);
		ASSERT_TRUE(archive.has_value()) << "the archive is unreadable after a refused commit";
		std::size_t sizeA = 0;
		std::unique_ptr<byte[]> a = archive->ReadFile("recordA", sizeA, error);
		ASSERT_NE(a, nullptr) << "recordA was lost by a refused commit";
		EXPECT_EQ(std::string(reinterpret_cast<const char *>(a.get()), sizeA), "OLD-A")
		    << "recordA was swapped in on its own - the save is a mixture of two";
		std::size_t sizeB = 0;
		std::unique_ptr<byte[]> b = archive->ReadFile("recordB", sizeB, error);
		ASSERT_NE(b, nullptr) << "recordB was lost by a refused commit";
		EXPECT_EQ(std::string(reinterpret_cast<const char *>(b.get()), sizeB), "OLD-B");
	}

	// And an abandoned transaction - one nobody committed at all - must leave the originals alone
	// rather than swapping in whatever happened to be staged when the writer went away.
	{
		MpqWriter writer(archivePath);
		writer.BeginTransaction();
		EXPECT_TRUE(writer.WriteFile("recordA", bytes("ABND"), 4));
		// no commit; the destructor runs here
	}
	{
		MpqWriter writer(archivePath);
		EXPECT_TRUE(writer.HasFile("recordA"))
		    << "an abandoned transaction destroyed the record it was replacing";
	}

	RemoveFile(archivePath.c_str());
	StopFailingWrites();
}

TEST(Writehero, pfile_write_hero)
{
	UseIsolatedPrefPath();
	const std::string savePath = paths::PrefPath() + "multi_0.sv";
	RemoveFile(savePath.c_str());

	gbVanilla = true;
	gbIsHellfire = false;
	gbIsMultiplayer = true;
	gbIsHellfireSaveGame = false;
	leveltype = DTYPE_TOWN;
	giNumberOfLevels = 17;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	_uiheroinfo info {};
	info.heroclass = HeroClass::Rogue;
	pfile_ui_save_create(&info);
	PlayerPack pks;
	PackPlayerTest(&pks);
	UnPackPlayer(pks, *MyPlayer);
	AssertPlayer(Players[0]);
	pfile_write_hero();

	uintmax_t fileSize;
	ASSERT_TRUE(GetFileSize(savePath.c_str(), &fileSize));
	size_t size = static_cast<size_t>(fileSize);
	FILE *f = OpenFile(savePath.c_str(), "rb");
	ASSERT_TRUE(f != nullptr);
	std::unique_ptr<char[]> data { new char[size] };
	ASSERT_EQ(std::fread(data.get(), size, 1, f), 1);
	std::fclose(f);

	std::vector<unsigned char> s(picosha2::k_digest_size);
	picosha2::hash256(data.get(), data.get() + size, s.begin(), s.end());
	// Oracool: this golden hash has been re-baselined three times, each for a deliberate,
	// documented change to the on-disk "hero" blob:
	//   1. pExperience widened from uint32_t to uint64_t (the level-99 curve exceeds UINT32_MAX).
	//   2. V1's inventory grew from 10x4 to 10x7, so PlayerPack's InvList and InvGrid grew with
	//      InventoryGridCells (40 -> 70). See oracool/inventory_layout.h.
	//   3. Six worn equipment slots added, so PlayerPack's InvBody grew with NUM_INVLOC (7 -> 13).
	//   4. The four per-difficulty waypoint masks widened from uint16_t to uint32_t (1.5.0), so the
	//      travel list could reach Hellfire's Nest and Crypt - levels 17-24 need 24 bits, not 16.
	//      Unlike 1-3 this one also GREW the struct by 8 bytes rather than reshaping it in place,
	//      which is what made pre-1.5.0 characters unloadable (pfile.cpp's ReadHero demands an exact
	//      size match). See PlayerPack::pWaypointUnlockedNormal for why that was affordable.
	//   5. The chunk tail (1.6.27, Megaplan Phase 0.1): the hero blob is now PlayerPack + "OEXT" +
	//      tagged chunks (oracool/hero_chunks.h). Unlike 1-4 this is the LAST planned growth: new
	//      state appends a chunk, old readers skip it, and ReadHero accepts >= the base size - so
	//      pre-tail heroes still load and this hash should only ever move again if a chunk's
	//      CONTENT changes deliberately.
	//   6. Exactly that: Phase 2 Stage 1 appends the ActiveAura chunk (tag 4, one byte - the
	//      Paladin's burning aura). Old readers skip the tag; pre-1.7.12 heroes load with no aura.
	//   7. The PaladinAuras chunk (tag 5): points invested in the skill tree's twenty auras. The
	//      tree's castable skills are NOT here - their points ride in the SkillPoints chunk, keyed
	//      by SpellID. Same additive rules as #5 and #6.
	//   8. Tag 5 retired in favour of the ClassTree chunk (tag 6) when the Paladin's tree
	//      generalized to all four classes: the array is now thirty entries indexed by
	//      position-within-class rather than twenty indexed from the first aura. Tag 5 is still
	//      READ and migrated onto the new slots, so a hero saved in between keeps its points.
	//   9. The ClassTree chunk grew 30 -> 32 entries (2026-08-16), because the Paladin gained two
	//      skills - Hammer of Faith and Blessed Shield - and reached 31, one past the old
	//      oracool::MaxSkillsPerClass that indexes the array. The blob is 2 bytes longer and the
	//      chunk's own count byte reads 32.
	//
	//      Note this is a TAIL change, not a PlayerPack one: the fixed struct is byte-identical, so
	//      ReadHero's exact-size check on the base is unaffected and pre-1.7.40 heroes still load.
	//      ApplyClassTree clamps to the smaller of the chunk's count and the array, so their 30
	//      entries land in the first 30 slots with the rest zeroed.
	// 1.7.61: HeroChunkSpellHotkeys joined the tail - u8 count + 6 PackReadiedSpell bytes, the
	//      F1-F6 ability hotkeys. Additive tail chunk again; the fixed struct is untouched and an
	//      older build skips the unknown tag.
	// 1.7.83: HeroChunkSpellHotkeysLeft joined it, same shape - u8 count + one PackReadiedSpell byte
	//      per slot - carrying the LEFT-button bindings (LShift+F1-F8), and both hotkey chunks grew
	//      from 6 slots to 8. A separate tag rather than a widening of chunk 7, so a hero written
	//      before the left bindings existed still loads its right ones and simply has no left ones.
	// 1.9.20: HeroChunkMilestones (u32) and HeroChunkSignets (u8) joined the tail - D2MXL-to-ORCL
	//      Phase 2's claimed-milestone mask and consumed-signet count. Additive tail chunks, so the
	//      fixed struct is byte-identical again and an older build skips both unknown tags.
	//
	//      They ride the tail rather than growing PlayerPack precisely BECAUSE of what this test
	//      guards: PlayerPack's own header records that growing it at 1.5.0 broke every character
	//      then existing, and the tail exists so that never has to happen twice.
	// 1.9.38: HeroChunkStatPoints (tag 11, one u32) joined the tail - unspent stat points at full
	//      width. Additive tail chunk again, so the fixed struct is byte-identical and an older
	//      build skips the unknown tag.
	//
	//      This one is a BUG FIX rather than a new feature, and worth reading as one. PlayerPack's
	//      pStatPts is a uint8_t while Player::_pStatPts is an int, and the pack narrowed it in
	//      silence: a level-99 character pressing Oracool's own Reset Stats button gets around 490
	//      points back, saved as 490 & 0xFF = 234, and the other 256 were simply gone. The chunk
	//      carries the real number; the fixed byte is still written but CLAMPED, so a reader without
	//      the tail sees a capped pool rather than a wrapped one.
	// 1.9.45: the Passive Skills page (Diablo III-style passives, one sheet per class) moved TWO
	//      tail chunks at once, and neither touched PlayerPack:
	//
	//      HeroChunkClassTree grew 32 -> 64 entries, for the same reason it grew 30 -> 32 at
	//      1.7.40: three classes now hold 49 skills apiece, past the old oracool::MaxSkillsPerClass
	//      that indexes the array. 32 bytes longer, and the chunk's own count byte reads 64.
	//      ApplyClassTree still clamps to the smaller of the count and the array, so a 32-entry
	//      hero lands in the first 32 slots with the rest zeroed.
	//
	//      HeroChunkActiveAura grew 1 -> 2 bytes, which is the less obvious half. The aura is
	//      stored as a ClassTreeSkill, and at 273 rows that enum outgrew uint8_t - 0xFF, which had
	//      been the "no aura" sentinel, became a real Rogue skill. The enum, Player's field and
	//      this chunk widened together. Written little-endian deliberately: an older build reads
	//      byte 0 only, and byte 0 is the low byte, so it still recovers any aura below 256 - and
	//      every aura row is.
	//
	//      Both are TAIL changes. sizeof(PlayerPack) is untouched, so ReadHero's exact-size check
	//      on the base is unaffected and every existing hero still loads.
	// 1.9.46: HeroChunkPassiveSlots (tag 12) joined the tail - a count byte and four
	//      CLASS-RELATIVE skill indices, the passives the character is running. Additive tail chunk,
	//      so the fixed struct is byte-identical again and an older build skips the unknown tag,
	//      which costs it only the slot assignment.
	//
	//      Class-relative on purpose, and it is the lesson from the chunk directly above: the aura
	//      is stored ABSOLUTELY and that is why growing the enum at 1.9.45 cost Bard and Monk heroes
	//      their lit aura. A relative index does not move when another class gains rows.
	// 1.9.48: NOT a format change - an ARCHIVE LAYOUT change, and the distinction matters. Every
	//      record still has the same name and the same decoded bytes; what moved is where inside
	//      the .sv they sit. MpqWriter::WriteFile now writes under a temporary name and renames it
	//      over the target, so a failed write can no longer destroy the record it was replacing
	//      (audit, 2026-08-26 - it used to remove the old entry FIRST and write afterwards, and
	//      discard the result). The extra hash-table churn shifts block offsets, so these bytes
	//      differ while the save's meaning does not.
	//
	//      A hash cannot tell those two apart, which is why Writehero.HeroSurvivesAWriteAndReadsBack
	//      exists above: it writes a hero TWICE and reads it back, so "the archive is still
	//      readable and the second write replaced the first" is asserted rather than assumed.
	// 1.9.57: another ARCHIVE LAYOUT change, not a format one - same records, same decoded bytes,
	//      different offsets. A character's four records (hero, hotkeys, items, inventory tabs) are
	//      now written as one TRANSACTION: each is staged under its own temporary name and none is
	//      swapped in until every one has landed, so they arrive together or not at all. Staging
	//      several records at once allocates blocks in a different order, which is what moves these
	//      bytes.
	//
	//      The reason it was needed: injecting a write failure showed the previous behaviour could
	//      leave a hero at the NEW level whose item record had not been written - a save that
	//      loads, looks entirely normal, and has the wrong things in it. Per-record atomicity does
	//      not cover that; only a transaction does.
	// 1.9.58: HeroChunkActiveAuraRelative (tag 13) joined the tail - two bytes, the hero class and
	//      the aura's index WITHIN that class. A real format addition this time, not a layout shift.
	//
	//      Tag 4 still carries the same aura as an ABSOLUTE enum ordinal and is still written, so an
	//      older build reads what it understands. But an absolute ordinal moves whenever a class
	//      earlier in the enum gains rows, and it already had: the Passive Skills page at 1.9.45
	//      shifted every Bard and Monk aura and those characters lost whatever was burning. The
	//      relative form cannot be reinterpreted that way, because growth only ever appends to a
	//      class block - so when both tags are present, this one wins.
	// 1.9.138: HeroChunkAuraHotkeys (tag 14) joined the tail - a u8 count then eight little-endian
	//      u16 ClassTreeSkill ordinals, 0xFFFF for an unbound slot. Another real format addition.
	//
	//      An aura could not be bound to F1-F8 at all before this (user: "i cant set them on
	//      auras"), and the reason was structural rather than an oversight: the two hotkey arrays
	//      hold SpellIDs, and an aura row carries SpellID::Invalid because it is a toggle rather
	//      than a cast. There was nowhere to put it. Hence its own array and its own chunk.
	//
	//      Two bytes per slot, not one, for the reason HeroChunkActiveAura had to widen in
	//      2026-08-25: the Passive Skills page took the tree past 255 rows.
	// Re-baseline only for a change you intended to make to the save format - if this fires
	// unexpectedly, the format moved without anyone deciding it should.
	EXPECT_EQ(picosha2::bytes_to_hex_string(s.begin(), s.end()),
	    "55acf4c1b698b4e089bc05ccb412c73682a93d7efd6059471f7b93dea1c3f138");
}

} // namespace
} // namespace devilution

// External audit, 2026-08-26 (P1): the last hole in the save transaction.
//
// The transaction makes the RECORDS all-or-nothing, but the header, block table and hash table are
// written at close, one after another, into the live file. A failure between the block table and
// the hash table leaves old hash entries - which name the old records - pointing at block entries
// the commit has already changed: an archive that is neither save, and that says so nowhere.
//
// The fix builds the whole session on a copy and swaps it in with one replacing rename, so this
// test injects a failure at exactly the boundary the earlier transaction test deliberately excluded
// and asserts the ORIGINAL archive is still readable and still holds the original bytes.
TEST(Writehero, AFailureWritingTheTablesLeavesThePreviousArchiveIntact)
{
	const std::string archivePath = paths::PrefPath() + "tables_test.sv";
	RemoveFile(archivePath.c_str());
	RemoveFile((archivePath + ".tmp").c_str());

	const auto bytes = [](const char *s) { return reinterpret_cast<const byte *>(s); };

	// SWEPT across every boundary rather than aimed at one, because the first version of this test
	// aimed at one and was vacuous. It let the header through and failed the block table - and with
	// the fix reverted it still PASSED, because old hash entries plus an old block table still
	// resolve to old data that the new records were appended clear of. Only the hash table landing
	// while the block table did not - or either landing against a resized file - actually tears.
	//
	// So the test no longer needs to be right about which write is the dangerous one. It asserts
	// the property directly: at EVERY point the publish can fail, the previous save survives whole.
	for (int failAfter = 0; failAfter <= 2; failAfter++) {
		SCOPED_TRACE("failing the publish after " + std::to_string(failAfter) + " successful writes");

		RemoveFile(archivePath.c_str());
		RemoveFile((archivePath + ".tmp").c_str());

		{
			MpqWriter writer(archivePath);
			writer.BeginTransaction();
			ASSERT_TRUE(writer.WriteFile("recordA", bytes("OLD-A"), 5));
			ASSERT_TRUE(writer.WriteFile("recordB", bytes("OLD-B"), 5));
			ASSERT_TRUE(writer.CommitTransaction());
		}

		// A second save whose records all write and whose commit succeeds - the failure lands only
		// once the writer starts publishing its metadata, which is the case the transaction alone
		// cannot cover because by then it has already said yes.
		{
			MpqWriter writer(archivePath);
			writer.BeginTransaction();
			EXPECT_TRUE(writer.WriteFile("recordA", bytes("NEW-A"), 5));
			EXPECT_TRUE(writer.WriteFile("recordB", bytes("NEW-B"), 5));
			EXPECT_TRUE(writer.CommitTransaction())
			    << "the records themselves should have written cleanly";
			// From here the destructor writes the header, the block table and the hash table.
			FailWritesAfter(failAfter);
		}
		StopFailingWrites();

		{
			int32_t error = 0;
			std::optional<MpqArchive> archive = MpqArchive::Open(archivePath.c_str(), error);
			ASSERT_TRUE(archive.has_value())
			    << "a failure publishing the tables destroyed the archive outright";
			std::size_t sizeA = 0;
			std::unique_ptr<byte[]> a = archive->ReadFile("recordA", sizeA, error);
			ASSERT_NE(a, nullptr) << "recordA is gone after a failed table write";
			EXPECT_EQ(std::string(reinterpret_cast<const char *>(a.get()), sizeA), "OLD-A")
			    << "the archive holds neither the old save nor a good new one - this is the tear";
			std::size_t sizeB = 0;
			std::unique_ptr<byte[]> b = archive->ReadFile("recordB", sizeB, error);
			ASSERT_NE(b, nullptr) << "recordB is gone after a failed table write";
			EXPECT_EQ(std::string(reinterpret_cast<const char *>(b.get()), sizeB), "OLD-B");
		}

		// And the shadow must not be left lying around: it describes a save never published.
		EXPECT_FALSE(FileExists((archivePath + ".tmp").c_str()))
		    << "a failed save left its working copy on disk";
	}

	// The other half of the contract: when nothing fails, the new save is actually published.
	// Without this the test could be satisfied by a writer that never wrote anything at all.
	{
		MpqWriter writer(archivePath);
		writer.BeginTransaction();
		EXPECT_TRUE(writer.WriteFile("recordA", bytes("NEW-A"), 5));
		EXPECT_TRUE(writer.CommitTransaction());
	}
	{
		int32_t error = 0;
		std::optional<MpqArchive> archive = MpqArchive::Open(archivePath.c_str(), error);
		ASSERT_TRUE(archive.has_value());
		std::size_t size = 0;
		std::unique_ptr<byte[]> a = archive->ReadFile("recordA", size, error);
		ASSERT_NE(a, nullptr);
		EXPECT_EQ(std::string(reinterpret_cast<const char *>(a.get()), size), "NEW-A")
		    << "a save that succeeded end to end was not published";
	}

	RemoveFile(archivePath.c_str());
	StopFailingWrites();
}

// The swap primitive the publish rests on. RenameFile cannot do this job - it returns void, so a
// caller cannot tell a move that happened from one that did not, and on Windows it refuses outright
// when the destination exists, which is the only case that matters here.
TEST(Writehero, ReplaceFileAtomicallyOverwritesAndReportsBack)
{
	const std::string from = paths::PrefPath() + "replace_from.bin";
	const std::string to = paths::PrefPath() + "replace_to.bin";
	RemoveFile(from.c_str());
	RemoveFile(to.c_str());

	const auto write = [](const std::string &path, const char *contents) {
		FILE *f = OpenFile(path.c_str(), "wb");
		ASSERT_NE(f, nullptr);
		ASSERT_EQ(std::fwrite(contents, std::strlen(contents), 1, f), 1u);
		ASSERT_EQ(std::fclose(f), 0);
	};
	write(from, "NEW");
	write(to, "OLD-AND-LONGER");

	EXPECT_TRUE(ReplaceFileAtomically(from.c_str(), to.c_str()))
	    << "the replacing move reported failure over an existing destination";
	EXPECT_FALSE(FileExists(from.c_str())) << "the source survived the move";

	std::uintmax_t size = 0;
	ASSERT_TRUE(GetFileSize(to.c_str(), &size));
	EXPECT_EQ(size, 3u) << "the destination was not replaced by the source's contents";

	RemoveFile(to.c_str());
}

// The hero and the stash are one state in two files, and the shadow made saving them separately
// worse rather than better: the hero's shadow is published BEFORE the stash's records are written,
// so a disk that fills up lands squarely in the gap every time rather than by chance. An item moved
// out of the stash then exists in both places, or in neither.
//
// Finish()/Publish()/DiscardShadow() is what closes that: all the risky work for both archives
// happens first, and only when both are complete does either become visible. This pins the contract
// the paired save is built on.
TEST(Writehero, AFinishedArchiveIsInvisibleUntilItIsPublished)
{
	const std::string archivePath = paths::PrefPath() + "deferred_test.sv";
	const auto bytes = [](const char *s) { return reinterpret_cast<const byte *>(s); };
	const auto readRecord = [&archivePath](const char *name) -> std::string {
		int32_t error = 0;
		std::optional<MpqArchive> archive = MpqArchive::Open(archivePath.c_str(), error);
		if (!archive.has_value())
			return "<no archive>";
		std::size_t size = 0;
		std::unique_ptr<byte[]> data = archive->ReadFile(name, size, error);
		if (data == nullptr)
			return "<no record>";
		return std::string(reinterpret_cast<const char *>(data.get()), size);
	};

	RemoveFile(archivePath.c_str());
	RemoveFile((archivePath + ".tmp").c_str());

	{
		MpqWriter writer(archivePath);
		ASSERT_TRUE(writer.WriteFile("record", bytes("OLD"), 3));
	}
	ASSERT_EQ(readRecord("record"), "OLD") << "test setup: the first save did not land";

	// Finished but not published: the archive on disk must still be the old one. This is the whole
	// point - it is the state the hero sits in while the stash is still being written.
	{
		MpqWriter writer(archivePath);
		writer.BeginTransaction();
		ASSERT_TRUE(writer.WriteFile("record", bytes("NEW"), 3));
		ASSERT_TRUE(writer.CommitTransaction());
		ASSERT_TRUE(writer.Finish()) << "the archive did not finish cleanly";

		EXPECT_EQ(readRecord("record"), "OLD")
		    << "a finished archive became visible before anything published it";

		EXPECT_TRUE(writer.Publish()) << "the publish refused";
		EXPECT_EQ(readRecord("record"), "NEW") << "the publish did not take effect";
	}
	EXPECT_EQ(readRecord("record"), "NEW") << "the destructor undid a completed publish";

	// Discarded after finishing - the case where the OTHER archive failed, so this one must not
	// land even though nothing at all went wrong with it.
	{
		MpqWriter writer(archivePath);
		writer.BeginTransaction();
		ASSERT_TRUE(writer.WriteFile("record", bytes("BAD"), 3));
		ASSERT_TRUE(writer.CommitTransaction());
		ASSERT_TRUE(writer.Finish());
		writer.DiscardShadow();
	}
	EXPECT_EQ(readRecord("record"), "NEW")
	    << "a discarded archive was published anyway - a partner's failure did not hold it back";
	EXPECT_FALSE(FileExists((archivePath + ".tmp").c_str()))
	    << "a discarded archive left its working copy on disk";

	// Finished and then simply abandoned. Nobody made a publish decision, so there is no save to
	// publish - the destructor must not guess.
	{
		MpqWriter writer(archivePath);
		writer.BeginTransaction();
		ASSERT_TRUE(writer.WriteFile("record", bytes("ABN"), 3));
		ASSERT_TRUE(writer.CommitTransaction());
		ASSERT_TRUE(writer.Finish());
	}
	EXPECT_EQ(readRecord("record"), "NEW")
	    << "an abandoned finished archive was published by its destructor";
	EXPECT_FALSE(FileExists((archivePath + ".tmp").c_str()))
	    << "an abandoned finished archive left its working copy on disk";

	// And a writer that is never finished at all still behaves as it always did: it publishes.
	// Every existing caller in the game relies on this.
	{
		MpqWriter writer(archivePath);
		writer.BeginTransaction();
		ASSERT_TRUE(writer.WriteFile("record", bytes("END"), 3));
		ASSERT_TRUE(writer.CommitTransaction());
	}
	EXPECT_EQ(readRecord("record"), "END")
	    << "an ordinary save stopped publishing when Finish() was introduced";

	RemoveFile(archivePath.c_str());
	StopFailingWrites();
}

// The three fixes from the sixth external audit (2026-08-26) that could not be pinned when they
// were made, because the injection seam reached `Write` and nothing else. It now reaches the close
// and the staging copy as well, so each of these can fail.
//
// All three share one assertion, and it is the only one that matters: whatever goes wrong, the
// archive already on disk is still the last save that fully succeeded.
TEST(Writehero, EveryFailureInThePublishLeavesThePreviousSaveWhole)
{
	const std::string archivePath = paths::PrefPath() + "seam_test.sv";
	const std::string shadowPath = archivePath + ".tmp";
	const auto bytes = [](const char *s) { return reinterpret_cast<const byte *>(s); };
	const auto readRecord = [&archivePath]() -> std::string {
		int32_t error = 0;
		std::optional<MpqArchive> archive = MpqArchive::Open(archivePath.c_str(), error);
		if (!archive.has_value())
			return "<no archive>";
		std::size_t size = 0;
		std::unique_ptr<byte[]> data = archive->ReadFile("record", size, error);
		if (data == nullptr)
			return "<no record>";
		return std::string(reinterpret_cast<const char *>(data.get()), size);
	};
	const auto writeGoodSave = [&]() {
		RemoveFile(archivePath.c_str());
		RemoveFile(shadowPath.c_str());
		MpqWriter writer(archivePath);
		ASSERT_TRUE(writer.WriteFile("record", bytes("OLD"), 3));
	};

	// 1. The CLOSE fails - every individual write succeeded, and the archive is ruined anyway.
	//
	// This is not reachable through the write seam by construction: arming that makes a write fail,
	// which is the case this is not. Buffered data is flushed by fclose, so a disk that fills up on
	// the last few kilobytes reports itself here and at no earlier point - and the publish used to
	// be the very next statement.
	{
		SCOPED_TRACE("close failure");
		writeGoodSave();
		ASSERT_EQ(readRecord(), "OLD");
		{
			MpqWriter writer(archivePath);
			writer.BeginTransaction();
			EXPECT_TRUE(writer.WriteFile("record", bytes("NEW"), 3));
			EXPECT_TRUE(writer.CommitTransaction());
			FailClosesAfter(0);
		}
		StopFailingCloses();
		EXPECT_EQ(readRecord(), "OLD")
		    << "a save that failed at the close was published over the good archive";
		EXPECT_FALSE(FileExists(shadowPath.c_str()))
		    << "the failed shadow was left on disk";
	}

	// 2. The staging COPY fails - the condition that causes it is a full disk, which is exactly
	//    what the copy exists to survive. This used to fall back to editing the live archive.
	{
		SCOPED_TRACE("staging copy failure");
		writeGoodSave();
		ASSERT_EQ(readRecord(), "OLD");
		{
			FailFileCopiesAfter(0);
			MpqWriter writer(archivePath);
			StopFailingFileCopies();
			// The writer never opened, so it must refuse rather than quietly write somewhere.
			EXPECT_FALSE(writer.WriteFile("record", bytes("NEW"), 3))
			    << "a writer whose staging failed accepted a record anyway";
			EXPECT_FALSE(writer.CommitTransaction())
			    << "a writer whose staging failed reported a successful commit";
		}
		StopFailingFileCopies();
		EXPECT_EQ(readRecord(), "OLD")
		    << "an unstageable save damaged the archive it could not copy";
		EXPECT_FALSE(FileExists(shadowPath.c_str()));
	}

	// 3. The target cannot be MEASURED. The subtler half of the same finding: a failed GetFileSize
	//    used to collapse into "the file does not exist", and a non-existent target is staged as a
	//    brand new archive - which holds only the records this save writes. Publishing that over a
	//    real save would have discarded everything else in it.
	{
		SCOPED_TRACE("size query failure");
		writeGoodSave();
		ASSERT_EQ(readRecord(), "OLD");
		{
			// ONE query - the constructor asks twice, and failing both makes the writer give up at
			// an earlier point than the one under test. The first is the one against the target.
			FailNextFileSizeQuery();
			MpqWriter writer(archivePath);
			EXPECT_FALSE(writer.WriteFile("record", bytes("NEW"), 3))
			    << "an archive that could not be measured was written to anyway";
		}
		StopFailingFileSizeQueries();
		EXPECT_EQ(readRecord(), "OLD")
		    << "an archive that could not be measured was replaced by a fresh one";
		EXPECT_FALSE(FileExists(shadowPath.c_str()));
	}

	// And with every seam disarmed the same sequence still saves, so none of the above is being
	// satisfied by a writer that simply stopped working.
	{
		SCOPED_TRACE("no failure");
		writeGoodSave();
		{
			MpqWriter writer(archivePath);
			writer.BeginTransaction();
			EXPECT_TRUE(writer.WriteFile("record", bytes("NEW"), 3));
			EXPECT_TRUE(writer.CommitTransaction());
		}
		EXPECT_EQ(readRecord(), "NEW") << "an ordinary save stopped working";
	}

	RemoveFile(archivePath.c_str());
	RemoveFile(shadowPath.c_str());
	StopFailingWrites();
	StopFailingCloses();
	StopFailingFileCopies();
	StopFailingFileSizeQueries();
}

// Found by PLAYING the game, 2026-08-26: it crashed on picking up gold, and on leaving to the main
// menu. One bug behind both. In single-player, gold goes straight to the stash and marks it dirty,
// so a save then takes the stash branch - which held the writer in a std::optional and put it there
// with emplace(GetStashWriter()).
//
// That MOVES an MpqWriter. LoggedFStream wrapped a raw FILE* with a compiler-written move, so the
// handle was COPIED and the source kept it; and MpqWriter's move was defaulted too, so the husk
// left behind still looked live to a destructor that - since the shadow landed - writes the tables,
// closes the file and publishes the archive. Destroying the husk therefore closed the file the real
// writer was still using, and the next write went through a closed handle.
//
// The lesson is not "do not move writers". It is that a defaulted move stopped being safe the
// moment the destructor started doing real work, and nothing said so.
TEST(Writehero, MovingAWriterDoesNotCloseOrPublishTheOriginal)
{
	const std::string archivePath = paths::PrefPath() + "move_test.sv";
	const std::string shadowPath = archivePath + ".tmp";
	const auto bytes = [](const char *s) { return reinterpret_cast<const byte *>(s); };
	const auto readRecord = [&archivePath]() -> std::string {
		int32_t error = 0;
		std::optional<MpqArchive> archive = MpqArchive::Open(archivePath.c_str(), error);
		if (!archive.has_value())
			return "<no archive>";
		std::size_t size = 0;
		std::unique_ptr<byte[]> data = archive->ReadFile("record", size, error);
		if (data == nullptr)
			return "<no record>";
		return std::string(reinterpret_cast<const char *>(data.get()), size);
	};

	RemoveFile(archivePath.c_str());
	RemoveFile(shadowPath.c_str());
	{
		MpqWriter writer(archivePath);
		ASSERT_TRUE(writer.WriteFile("record", bytes("OLD"), 3));
	}
	ASSERT_EQ(readRecord(), "OLD") << "test setup: the first save did not land";

	// Exactly the shape that crashed: a writer moved into an optional, the husk destroyed at the
	// end of the full expression, and then the REAL writer used afterwards.
	{
		std::optional<MpqWriter> writer;
		writer.emplace(MpqWriter(archivePath));

		// If the husk closed the shared handle, this write goes through a dead FILE*.
		writer->BeginTransaction();
		EXPECT_TRUE(writer->WriteFile("record", bytes("NEW"), 3))
		    << "the writer could not write after being moved - the handle was closed under it";
		EXPECT_TRUE(writer->CommitTransaction());

		// And the husk must not have published anything either. Nothing has been published yet, so
		// the archive on disk is still the old one.
		EXPECT_EQ(readRecord(), "OLD")
		    << "destroying a moved-from writer published the archive early";
	}
	EXPECT_EQ(readRecord(), "NEW")
	    << "the surviving writer did not publish - the move left it unable to finish";

	// Move-assignment has the same hazard and the same contract.
	{
		MpqWriter first(archivePath);
		first.BeginTransaction();
		EXPECT_TRUE(first.WriteFile("record", bytes("ASN"), 3));
		EXPECT_TRUE(first.CommitTransaction());

		MpqWriter second(archivePath + ".other");
		second = std::move(first);
		// `first` is now a husk; letting it go must not disturb what `second` owns.
	}
	EXPECT_EQ(readRecord(), "ASN") << "a move-assigned writer lost its archive";
	EXPECT_FALSE(FileExists(shadowPath.c_str())) << "a shadow was left behind";

	RemoveFile(archivePath.c_str());
	RemoveFile(shadowPath.c_str());
	RemoveFile((archivePath + ".other").c_str());
	RemoveFile((archivePath + ".other.tmp").c_str());
	StopFailingWrites();
	StopFailingCloses();
	StopFailingFileCopies();
	StopFailingFileSizeQueries();
}

// The user's crash, from actual play: picking up the first gold drop takes the game down.
//
// SaveHeroAndStash is the function that runs, and NOTHING in this suite has ever called it - which
// is the gap the first fix went into blind. In single-player gold goes to the stash, marking it
// dirty, so this is the branch a real game reaches within a minute of starting, with a brand new
// character and no save file on disk yet.
TEST(Writehero, SaveHeroAndStashWritesBothForANewCharacterWithADirtyStash)
{
	UseIsolatedPrefPath();
	const std::string heroPath = paths::PrefPath() + "single_0.sv";
	const std::string stashPath = paths::PrefPath() + "stash.sv";
	RemoveFile(heroPath.c_str());
	RemoveFile(stashPath.c_str());
	RemoveFile((heroPath + ".tmp").c_str());
	RemoveFile((stashPath + ".tmp").c_str());

	gbVanilla = false;
	gbIsHellfire = false;
	gbIsMultiplayer = false;
	gbIsSpawn = false;
	gbIsHellfireSaveGame = false;
	leveltype = DTYPE_TOWN;
	giNumberOfLevels = 17;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	_uiheroinfo info {};
	info.heroclass = HeroClass::Rogue;
	info.saveNumber = 0;
	ASSERT_TRUE(pfile_ui_save_create(&info)) << "the character could not be created";

	// Exactly what picking up gold does: it lands in the stash pool and marks it dirty.
	Stash.gold += 100;
	Stash.dirty = true;

	// The call that crashes in play.
	SaveHeroAndStash(/*writeGameData=*/false);

	EXPECT_TRUE(FileExists(heroPath.c_str())) << "the character was not saved";
	EXPECT_TRUE(FileExists(stashPath.c_str())) << "the stash was not saved";
	EXPECT_FALSE(Stash.dirty) << "the stash is still dirty after a successful save";
	EXPECT_FALSE(FileExists((heroPath + ".tmp").c_str())) << "a hero shadow was left behind";
	EXPECT_FALSE(FileExists((stashPath + ".tmp").c_str())) << "a stash shadow was left behind";

	// And again, now that both files exist - the second save is the one that has to COPY them,
	// which is a different path from creating them.
	Stash.gold += 100;
	Stash.dirty = true;
	SaveHeroAndStash(/*writeGameData=*/false);
	EXPECT_FALSE(Stash.dirty) << "the second save did not complete";

	RemoveFile(heroPath.c_str());
	RemoveFile(stashPath.c_str());
	StopFailingWrites();
	StopFailingCloses();
	StopFailingFileCopies();
	StopFailingFileSizeQueries();
}
