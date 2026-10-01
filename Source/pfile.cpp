/**
 * @file pfile.cpp
 *
 * Implementation of the save game encoding functionality.
 */
#include "pfile.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include <fmt/core.h>

#include "codec.h"
#include "engine.h"
#include "engine/load_file.hpp"
#include "init.h"
#include "levels/gendung.h" // setlevel - a level save that failed un-marks its floor
#include "loadsave.h"
#include "menu.h"
#include "mpq/mpq_common.hpp"
#include "oracool/class_skills.h"   // RefreshInnateSpells - the chunks decide what the character HAS
#include "oracool/hero_chunks.h"
#include "oracool/rfa12_actives.h" // ClearRfa12PlayerBuffs - the preview shows no last-game buffs
#include "oracool/warcries.h"      // ClearWarcryBuffs
#include "oracool/passives.h"      // PassiveUnconditionalDamagePercent - the hero-select damage
#include "oracool/sprite_mix.h"
#include "oracool/rage.h"
#include "oracool/readied_spells.h" // UnpackReadiedSpell - re-decoded once the chunks have landed
#include "oracool/save_status.h"
#include "oracool/event_log.h"
#include "error.h"
#include "pack.h"
#include "playerdat.hpp"
#include "qol/stash.h"
#include "utils/endian_read.hpp"
#include "utils/file_util.h"
#include "utils/language.h"
#include "utils/paths.h"
#include "utils/stdcompat/abs.hpp"
#include "utils/stdcompat/string_view.hpp"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"
#include "utils/utf8.hpp"

#ifdef UNPACKED_SAVES
#include "utils/file_util.h"
#else
#include "mpq/mpq_reader.hpp"
#endif

namespace devilution {

#define PASSWORD_SPAWN_SINGLE "adslhfb1"
#define PASSWORD_SPAWN_MULTI "lshbkfg1"
#define PASSWORD_SINGLE "xrgyrkj1"
#define PASSWORD_MULTI "szqnlsk1"

bool gbValidSaveFile;

namespace {

/** List of character names for the character selection screen. */
char hero_names[MAX_CHARACTERS][PlayerNameLength];

std::string GetSavePath(uint32_t saveNum, string_view savePrefix = {})
{
	return StrCat(paths::PrefPath(), savePrefix,
	    gbIsSpawn
	        ? (gbIsMultiplayer ? "share_" : "spawn_")
	        : (gbIsMultiplayer ? "multi_" : "single_"),
	    saveNum,
#ifdef UNPACKED_SAVES
	    gbIsHellfire ? "_hsv" DIRECTORY_SEPARATOR_STR : "_sv" DIRECTORY_SEPARATOR_STR
#else
	    gbIsHellfire ? ".hsv" : ".sv"
#endif
	);
}

std::string GetStashSavePath()
{
	return StrCat(paths::PrefPath(),
	    gbIsSpawn ? "stash_spawn" : "stash",
#ifdef UNPACKED_SAVES
	    gbIsHellfire ? "_hsv" DIRECTORY_SEPARATOR_STR : "_sv" DIRECTORY_SEPARATOR_STR
#else
	    gbIsHellfire ? ".hsv" : ".sv"
#endif
	);
}

bool GetSaveNames(uint8_t index, string_view prefix, char *out)
{
	char suf;
	if (index < giNumberOfLevels)
		suf = 'l';
	else if (index < giNumberOfLevels * 2) {
		index -= giNumberOfLevels;
		suf = 's';
	} else {
		return false;
	}

	*fmt::format_to(out, "{}{}{:02d}", prefix, suf, index) = '\0';
	return true;
}

bool GetPermSaveNames(uint8_t dwIndex, char *szPerm)
{
	return GetSaveNames(dwIndex, "perm", szPerm);
}

bool GetTempSaveNames(uint8_t dwIndex, char *szTemp)
{
	return GetSaveNames(dwIndex, "temp", szTemp);
}

void RenameTempToPerm(SaveWriter &saveWriter)
{
	char szTemp[MaxMpqPathSize];
	char szPerm[MaxMpqPathSize];

	uint32_t dwIndex = 0;
	while (GetTempSaveNames(dwIndex, szTemp)) {
		[[maybe_unused]] bool result = GetPermSaveNames(dwIndex, szPerm); // DO NOT PUT DIRECTLY INTO ASSERT!
		assert(result);
		dwIndex++;
		if (saveWriter.HasFile(szTemp)) {
			if (saveWriter.HasFile(szPerm))
				saveWriter.RemoveHashEntry(szPerm);
			saveWriter.RenameFile(szTemp, szPerm);
		}
	}
	assert(!GetPermSaveNames(dwIndex, szPerm));
}

// Oracool: Megaplan Phase 0.1 - the hero file is the fixed PlayerPack plus an OPTIONAL chunk tail
// (oracool/hero_chunks.h). `read == sizeof` is a pre-tail hero and loads exactly as before;
// `read > sizeof` carries chunks, handed back through @p chunkTail for the caller to apply AFTER
// UnPackPlayer. The tail is parsed and validated there, not here - this function only transports.
bool ReadHero(SaveReader &archive, PlayerPack *pPack, std::vector<uint8_t> *chunkTail = nullptr)
{
	size_t read;

	auto buf = ReadArchive(archive, "hero", &read);
	if (buf == nullptr)
		return false;

	bool ret = false;
	if (read >= sizeof(*pPack)) {
		memcpy(pPack, buf.get(), sizeof(*pPack));
		if (chunkTail != nullptr && read > sizeof(*pPack)) {
			const auto *bytes = reinterpret_cast<const uint8_t *>(buf.get());
			chunkTail->assign(bytes + sizeof(*pPack), bytes + read);
		}
		ret = true;
	}

	return ret;
}

void EncodeHero(SaveWriter &saveWriter, const PlayerPack *pack, const std::vector<uint8_t> &chunkTail = {})
{
	const size_t plainLen = sizeof(*pack) + chunkTail.size();
	size_t packedLen = codec_get_encoded_len(plainLen);
	std::unique_ptr<byte[]> packed { new byte[packedLen] };

	memcpy(packed.get(), pack, sizeof(*pack));
	if (!chunkTail.empty())
		memcpy(packed.get() + sizeof(*pack), chunkTail.data(), chunkTail.size());
	codec_encode(packed.get(), plainLen, packedLen, pfile_get_password());
	// Oracool (audit, 2026-08-26): "hero" is THE file - stats, inventory, equipment, gold, the
	// waypoint table, the chunk tail. Its write result was discarded, so a save that failed here
	// still went on to log "Game saved". Recorded so the save path can say otherwise.
	if (!saveWriter.WriteFile("hero", packed.get(), packedLen))
		oracool::NoteSaveWriteFailed("hero");
}

SaveWriter GetSaveWriter(uint32_t saveNum)
{
	return SaveWriter(GetSavePath(saveNum));
}

SaveWriter GetStashWriter()
{
	return SaveWriter(GetStashSavePath());
}

#ifndef DISABLE_DEMOMODE
void CopySaveFile(uint32_t saveNum, std::string targetPath)
{
	const std::string savePath = GetSavePath(saveNum);
	CopyFileOverwrite(savePath.c_str(), targetPath.c_str());
}
#endif

void Game2UiPlayer(const Player &player, _uiheroinfo *heroinfo, bool bHasSaveFile)
{
	CopyUtf8(heroinfo->name, player._pName, sizeof(heroinfo->name));
	heroinfo->level = player._pLevel;
	heroinfo->heroclass = player._pClass;
	heroinfo->strength = player._pStrength;
	heroinfo->magic = player._pMagic;
	heroinfo->dexterity = player._pDexterity;
	heroinfo->vitality = player._pVitality;
	heroinfo->gfxnum = player._pgfxnum;
	heroinfo->gearLook = oracool::GearLookCode(player);
	// The stats column's combat half (user, 2026-08-31). CalcPlrInv has already run by the time this
	// is called, so every one of these is a copy rather than a computation - the same free ride
	// gfxnum takes. Life and mana are shifted out of the engine's 1/64 fixed point here, so the
	// column never has to know about it. The character sheet's own Armor row adds the level bonus
	// the same way, which is why that term is repeated rather than just GetArmor().
	heroinfo->life = static_cast<uint16_t>(std::max(0, player._pMaxHP >> 6));
	// The Barbarian's column reads Rage, so it carries his Rage pool (oracool/rage.h).
	heroinfo->mana = static_cast<uint16_t>(oracool::UsesRage(player) ? oracool::MaxRage(player) : std::max(0, player._pMaxMana >> 6));
	heroinfo->armourClass = static_cast<uint16_t>(std::max(0, player.GetArmor() + player._pLevel * 2));
	// The character sheet's Damage, not the bare weapon: the sheet's GetDamage adds the % bonus, the flat bonus and the
	// Strength part (halved on a bow outside the Rogue) - hero-select read 10-20 where the sheet read 55-65 (round 4 audit).
	const bool nonRogueBow = player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag
	    && player._pClass != HeroClass::Rogue; // a usable bow only (round 25 audit)
	// The sheet's pool (user, 2026-10-01: Diablo II's rule): the items' +%, the stat share and Glass Cannon in one percentage.
	const int always = oracool::PassiveUnconditionalDamagePercent(player);
	const int minDamage = PooledWeaponDamage(player, player._pIMinDam, always, nonRogueBow ? 50 : 100);
	const int maxDamage = PooledWeaponDamage(player, player._pIMaxDam, always, nonRogueBow ? 50 : 100);
	heroinfo->minDamage = static_cast<uint16_t>(std::clamp(minDamage, 0, 65535));
	heroinfo->maxDamage = static_cast<uint16_t>(std::clamp(maxDamage, 0, 65535));
	heroinfo->hassaved = bHasSaveFile;
	heroinfo->herorank = player.pDiabloKillLevel;
	heroinfo->spawned = gbIsSpawn;
}

bool GetFileName(uint8_t lvl, char *dst)
{
	if (gbIsMultiplayer) {
		if (lvl != 0)
			return false;
		memcpy(dst, "hero", 5);
		return true;
	}
	if (GetPermSaveNames(lvl, dst)) {
		return true;
	}
	if (lvl == giNumberOfLevels * 2) {
		memcpy(dst, "game", 5);
		return true;
	}
	if (lvl == giNumberOfLevels * 2 + 1) {
		memcpy(dst, "hero", 5);
		return true;
	}
	return false;
}

bool ArchiveContainsGame(SaveReader &hsArchive)
{
	if (gbIsMultiplayer)
		return false;

	auto gameData = ReadArchive(hsArchive, "game");
	if (gameData == nullptr)
		return false;

	uint32_t hdr = LoadLE32(gameData.get());

	return IsHeaderValid(hdr);
}

std::optional<SaveReader> CreateSaveReader(std::string &&path)
{
#ifdef UNPACKED_SAVES
	if (!FileExists(path))
		return std::nullopt;
	return SaveReader(std::move(path));
#else
	std::int32_t error;
	return MpqArchive::Open(path.c_str(), error);
#endif
}

#ifndef DISABLE_DEMOMODE
struct CompareInfo {
	std::unique_ptr<byte[]> &data;
	size_t currentPosition;
	size_t size;
	bool isTownLevel;
	bool dataExists;
};

struct CompareCounter {
	int reference;
	int actual;
	int max()
	{
		return std::max(reference, actual);
	}
	void checkIfDataExists(int count, CompareInfo &compareInfoReference, CompareInfo &compareInfoActual)
	{
		if (reference == count)
			compareInfoReference.dataExists = false;
		if (actual == count)
			compareInfoActual.dataExists = false;
	}
};

inline bool string_ends_with(string_view value, string_view suffix)
{
	if (suffix.size() > value.size())
		return false;
	return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

void CreateDetailDiffs(string_view prefix, string_view memoryMapFile, CompareInfo &compareInfoReference, CompareInfo &compareInfoActual, std::unordered_map<std::string, size_t> &foundDiffs)
{
	// Note: Detail diffs are currently only supported in unit tests
	std::string memoryMapFileAssetName = StrCat(paths::BasePath(), "/test/fixtures/memory_map/", memoryMapFile, ".txt");

	SDL_RWops *handle = SDL_RWFromFile(memoryMapFileAssetName.c_str(), "r");
	if (handle == nullptr) {
		app_fatal(StrCat("MemoryMapFile ", memoryMapFile, " is missing"));
		return;
	}

	size_t readBytes = SDL_RWsize(handle);
	std::unique_ptr<byte[]> memoryMapFileData { new byte[readBytes] };
	SDL_RWread(handle, memoryMapFileData.get(), readBytes, 1);
	const string_view buffer(reinterpret_cast<const char *>(memoryMapFileData.get()), readBytes);

	std::unordered_map<std::string, CompareCounter> counter;

	auto getCounter = [&](const std::string &counterAsString) {
		auto it = counter.find(counterAsString);
		if (it != counter.end())
			return it->second;
		int countFromMapFile = std::stoi(counterAsString);
		return CompareCounter { countFromMapFile, countFromMapFile };
	};
	auto addDiff = [&](const std::string &diffKey) {
		auto it = foundDiffs.find(diffKey);
		if (it == foundDiffs.end()) {
			foundDiffs.insert_or_assign(diffKey, 1);
		} else {
			foundDiffs.insert_or_assign(diffKey, it->second + 1);
		}
	};

	auto compareBytes = [&](size_t countBytes) {
		if (compareInfoReference.dataExists && compareInfoReference.currentPosition + countBytes > compareInfoReference.size)
			app_fatal(StrCat("Comparsion failed. Too less bytes in reference to compare. Location: ", prefix));
		if (compareInfoActual.dataExists && compareInfoActual.currentPosition + countBytes > compareInfoActual.size)
			app_fatal(StrCat("Comparsion failed. Too less bytes in actual to compare. Location: ", prefix));
		bool result = true;
		if (compareInfoReference.dataExists && compareInfoActual.dataExists)
			result = memcmp(compareInfoReference.data.get() + compareInfoReference.currentPosition, compareInfoActual.data.get() + compareInfoActual.currentPosition, countBytes) == 0;
		if (compareInfoReference.dataExists)
			compareInfoReference.currentPosition += countBytes;
		if (compareInfoActual.dataExists)
			compareInfoActual.currentPosition += countBytes;
		return result;
	};

	auto read32BitInt = [&](CompareInfo &compareInfo, bool useLE) {
		int32_t value = 0;
		if (!compareInfo.dataExists)
			return value;
		if (compareInfo.currentPosition + sizeof(value) > compareInfo.size)
			app_fatal("read32BitInt failed. Too less bytes to read.");
		memcpy(&value, compareInfo.data.get() + compareInfo.currentPosition, sizeof(value));
		if (useLE)
			value = SDL_SwapLE32(value);
		else
			value = SDL_SwapBE32(value);
		return value;
	};

	for (string_view line : SplitByChar(buffer, '\n')) {
		if (!line.empty() && line.back() == '\r')
			line.remove_suffix(1);
		if (line.empty())
			continue;
		const auto tokens = SplitByChar(line, ' ');
		auto it = tokens.begin();
		const auto end = tokens.end();
		if (it == end)
			continue;

		string_view command = *it;

		bool dataExistsReference = compareInfoReference.dataExists;
		bool dataExistsActual = compareInfoActual.dataExists;

		if (string_ends_with(command, "_HF")) {
			if (!gbIsHellfire)
				continue;
			command.remove_suffix(3);
		}
		if (string_ends_with(command, "_DA")) {
			if (gbIsHellfire)
				continue;
			command.remove_suffix(3);
		}
		if (string_ends_with(command, "_DL")) {
			if (compareInfoReference.isTownLevel && compareInfoActual.isTownLevel)
				continue;
			if (compareInfoReference.isTownLevel)
				compareInfoReference.dataExists = false;
			if (compareInfoActual.isTownLevel)
				compareInfoActual.dataExists = false;
			command.remove_suffix(3);
		}
		if (command == "R" || command == "LT" || command == "LC" || command == "LC_LE") {
			const auto bitsAsString = std::string(*++it);
			const auto comment = std::string(*++it);
			size_t bytes = static_cast<size_t>(std::stoi(bitsAsString) / 8);

			if (command == "LT") {
				int32_t valueReference = read32BitInt(compareInfoReference, false);
				int32_t valueActual = read32BitInt(compareInfoActual, false);
				assert(sizeof(valueReference) == bytes);
				compareInfoReference.isTownLevel = valueReference == 0;
				compareInfoActual.isTownLevel = valueActual == 0;
			}
			if (command == "LC" || command == "LC_LE") {
				int32_t valueReference = read32BitInt(compareInfoReference, command == "LC_LE");
				int32_t valueActual = read32BitInt(compareInfoActual, command == "LC_LE");
				assert(sizeof(valueReference) == bytes);
				counter.insert_or_assign(std::string(comment), CompareCounter { valueReference, valueActual });
			}

			if (!compareBytes(bytes)) {
				std::string diffKey = StrCat(prefix, ".", comment);
				addDiff(diffKey);
			}
		} else if (command == "M") {
			const auto countAsString = std::string(*++it);
			const auto bitsAsString = std::string(*++it);
			string_view comment = *++it;

			CompareCounter count = getCounter(countAsString);
			size_t bytes = static_cast<size_t>(std::stoi(bitsAsString) / 8);
			for (int i = 0; i < count.max(); i++) {
				count.checkIfDataExists(i, compareInfoReference, compareInfoActual);
				if (!compareBytes(bytes)) {
					std::string diffKey = StrCat(prefix, ".", comment);
					addDiff(diffKey);
				}
			}
		} else if (command == "C") {
			const auto countAsString = std::string(*++it);
			auto subMemoryMapFile = std::string(*++it);
			const auto comment = std::string(*++it);

			CompareCounter count = getCounter(countAsString);
			subMemoryMapFile.erase(std::remove(subMemoryMapFile.begin(), subMemoryMapFile.end(), '\r'), subMemoryMapFile.end());
			for (int i = 0; i < count.max(); i++) {
				count.checkIfDataExists(i, compareInfoReference, compareInfoActual);
				std::string subPrefix = StrCat(prefix, ".", comment);
				CreateDetailDiffs(subPrefix, subMemoryMapFile, compareInfoReference, compareInfoActual, foundDiffs);
			}
		}

		compareInfoReference.dataExists = dataExistsReference;
		compareInfoActual.dataExists = dataExistsActual;
	}
}

struct CompareTargets {
	std::string fileName;
	std::string memoryMapFileName;
	bool isTownLevel;
};

HeroCompareResult CompareSaves(const std::string &actualSavePath, const std::string &referenceSavePath, bool logDetails)
{
	std::vector<CompareTargets> possibleFileToCheck;
	possibleFileToCheck.push_back({ "hero", "hero", false });
	possibleFileToCheck.push_back({ "game", "game", false });
	possibleFileToCheck.push_back({ "additionalMissiles", "additionalMissiles", false });
	char szPerm[MaxMpqPathSize];
	for (int i = 0; GetPermSaveNames(i, szPerm); i++) {
		possibleFileToCheck.push_back({ std::string(szPerm), "level", i == 0 });
	}

	SaveReader actualArchive = *CreateSaveReader(std::string(actualSavePath));
	SaveReader referenceArchive = *CreateSaveReader(std::string(referenceSavePath));

	bool compareResult = true;
	std::string message;
	for (const auto &compareTarget : possibleFileToCheck) {
		size_t fileSizeActual = 0;
		auto fileDataActual = ReadArchive(actualArchive, compareTarget.fileName.c_str(), &fileSizeActual);
		size_t fileSizeReference = 0;
		auto fileDataReference = ReadArchive(referenceArchive, compareTarget.fileName.c_str(), &fileSizeReference);
		if (fileDataActual.get() == nullptr && fileDataReference.get() == nullptr) {
			continue;
		}
		if (fileSizeActual == fileSizeReference && memcmp(fileDataReference.get(), fileDataActual.get(), fileSizeActual) == 0)
			continue;
		compareResult = false;
		if (!message.empty())
			message.append("\n");
		if (fileSizeActual != fileSizeReference)
			StrAppend(message, "file \"", compareTarget.fileName, "\" is different size. Expected: ", fileSizeReference, " Actual: ", fileSizeActual);
		else
			StrAppend(message, "file \"", compareTarget.fileName, "\" has different content.");
		if (!logDetails)
			continue;
		std::unordered_map<std::string, size_t> foundDiffs;
		CompareInfo compareInfoReference = { fileDataReference, 0, fileSizeReference, compareTarget.isTownLevel, fileSizeReference != 0 };
		CompareInfo compareInfoActual = { fileDataActual, 0, fileSizeActual, compareTarget.isTownLevel, fileSizeActual != 0 };
		CreateDetailDiffs(compareTarget.fileName, compareTarget.memoryMapFileName, compareInfoReference, compareInfoActual, foundDiffs);
		if (compareInfoReference.currentPosition != fileSizeReference)
			app_fatal(StrCat("Comparsion failed. Uncompared bytes in reference. File: ", compareTarget.fileName));
		if (compareInfoActual.currentPosition != fileSizeActual)
			app_fatal(StrCat("Comparsion failed. Uncompared bytes in actual. File: ", compareTarget.fileName));
		for (auto entry : foundDiffs) {
			StrAppend(message, "\nDiff found in ", entry.first, " count: ", entry.second);
		}
	}
	return { compareResult ? HeroCompareResult::Same : HeroCompareResult::Difference, message };
}
#endif // !DISABLE_DEMOMODE

void pfile_write_hero(SaveWriter &saveWriter, bool writeGameData)
{
	if (writeGameData) {
		SaveGameData(saveWriter);
		RenameTempToPerm(saveWriter);
	}
	PlayerPack pkplr;
	Player &myPlayer = *MyPlayer;

	// A character is FOUR records - the hero itself, the hotkeys, the worn and carried items, and
	// the extra inventory tabs - and they only make sense together. Written as one transaction so
	// they arrive together or not at all.
	//
	// Found by injecting a write failure (v1.9.56): the archive ended up holding a hero at the NEW
	// level whose item record had not been written. Not a missing save - a save that loads, looks
	// entirely normal, and has the wrong things in it. That is a far worse thing to hand a player
	// than an obvious failure, and it is the case each record being individually atomic does not
	// cover.
	saveWriter.BeginTransaction();

	PackPlayer(pkplr, myPlayer);
	EncodeHero(saveWriter, &pkplr, oracool::BuildHeroChunkTail(myPlayer));
	if (!gbVanilla) {
		SaveHotkeys(saveWriter, myPlayer);
		SaveHeroItems(saveWriter, myPlayer);
		if (!gbIsMultiplayer) {
			SaveInventoryTabs(saveWriter, myPlayer);
		}
	}

	// The one place the new save becomes visible. A refusal here means every record is discarded and
	// the archive still holds the last save that fully succeeded.
	if (!saveWriter.CommitTransaction())
		oracool::NoteSaveWriteFailed("hero");
}

void RemoveAllInvalidItems(Player &player)
{
	for (int i = 0; i < NUM_INVLOC; i++)
		RemoveInvalidItem(player.InvBody[i]);
	// Bounded independently of _pNumInv. UnPackPlayer clamps that field now, so this cannot be
	// reached with a bad count today - but this function is a cleanup pass that runs over whatever
	// it is handed, and it is one of the two places the audit found repeating an unvalidated count
	// (2026-08-25). A loop that reads a fixed-size array should say the array's size.
	const int liveItems = std::min(player._pNumInv, InventoryGridCells);
	for (int i = 0; i < liveItems; i++)
		RemoveInvalidItem(player.InvList[i]);
	for (int i = 0; i < MaxBeltItems; i++)
		RemoveInvalidItem(player.SpdList[i]);
	RemoveEmptyInventory(player);
}

} // namespace

#ifdef UNPACKED_SAVES
std::unique_ptr<byte[]> SaveReader::ReadFile(const char *filename, std::size_t &fileSize, int32_t &error)
{
	std::unique_ptr<byte[]> result;
	error = 0;
	const std::string path = dir_ + filename;
	uintmax_t size;
	if (!GetFileSize(path.c_str(), &size)) {
		error = 1;
		return nullptr;
	}
	fileSize = size;
	FILE *file = OpenFile(path.c_str(), "rb");
	if (file == nullptr) {
		error = 1;
		return nullptr;
	}
	result.reset(new byte[size]);
	if (std::fread(result.get(), size, 1, file) != 1) {
		std::fclose(file);
		error = 1;
		return nullptr;
	}
	std::fclose(file);
	return result;
}

SaveWriter::~SaveWriter()
{
	// A transaction still open here was never committed, so the records staged under it describe a
	// save nobody asked to publish. Discarded, exactly as MpqWriter does it: swapping in whichever
	// half happened to be written is the torn save the transaction exists to prevent.
	if (inTransaction_ || !pending_.empty())
		AbortTransaction();
}

void SaveWriter::BeginTransaction()
{
	inTransaction_ = true;
	transactionFailed_ = false;
	pending_.clear();
}

bool SaveWriter::CommitTransaction()
{
	inTransaction_ = false;
	if (transactionFailed_) {
		AbortTransaction();
		return false;
	}
	// Every result checked. Audit finding, 2026-08-26: these were fired off and ignored, so a batch
	// where the second rename failed left the hero at the new generation and the items at the old
	// one - a mixed character - and returned `true`, which is the save reporting success while
	// having produced exactly the torn state the transaction exists to prevent.
	//
	// Checking cannot make the batch atomic; a directory offers no way to swap four files at once.
	// What it does is stop the lie. The caller is told the save did not complete, so it keeps the
	// stash dirty, reports the failure to the player, and tries again - rather than announcing
	// success over a character that is now half of two saves.
	bool published = true;
	for (const PendingSwap &swap : pending_) {
		if (!::devilution::ReplaceFileAtomically((dir_ + swap.temp).c_str(), (dir_ + swap.target).c_str())) {
			oracool::NoteSaveWriteFailed(swap.target);
			published = false;
			// The rest are NOT attempted. Once one record has failed to land the batch is already
			// incomplete, and publishing more of it only widens the mixture.
			break;
		}
	}
	// Whatever is left staged describes a save that will not happen. Removed so it cannot be
	// mistaken for a good record by a later attempt.
	for (const PendingSwap &swap : pending_) {
		if (FileExists((dir_ + swap.temp).c_str()))
			RemoveFile((dir_ + swap.temp).c_str());
	}
	pending_.clear();
	return published;
}

void SaveWriter::AbortTransaction()
{
	inTransaction_ = false;
	transactionFailed_ = false;
	for (const PendingSwap &swap : pending_)
		RemoveFile((dir_ + swap.temp).c_str());
	pending_.clear();
}

bool SaveWriter::WriteFile(const char *filename, const byte *data, size_t size)
{
	// Written under a temporary name whether or not there is a transaction open, so that a failure
	// part way through cannot leave a HALF of the previous record in its place. Outside a
	// transaction the swap happens immediately, which is the behaviour every existing caller had.
	const std::string tempName = std::string(filename) + ".tmp";
	const std::string path = dir_ + tempName;
	FILE *file = OpenFile(path.c_str(), "wb");
	if (file == nullptr) {
		transactionFailed_ = true;
		return false;
	}
	// `fwrite` of a zero-size record returns 0 and is not a failure, so the count is only
	// meaningful when there is something to write.
	if (size > 0 && std::fwrite(data, size, 1, file) != 1) {
		std::fclose(file);
		RemoveFile(path.c_str());
		transactionFailed_ = true;
		return false;
	}
	// Checked, not assumed: buffered data is flushed by fclose, so this is where a full disk
	// actually reports itself. Ignoring it is how a truncated record gets called a success.
	if (std::fclose(file) != 0) {
		RemoveFile(path.c_str());
		transactionFailed_ = true;
		return false;
	}

	if (inTransaction_) {
		pending_.push_back({ tempName, filename });
		return true;
	}
	// Checked for the same reason the transactional path is: an unswapped record means the file on
	// disk is still the previous one, and the caller has to hear that rather than be told the write
	// succeeded because the BYTES were written somewhere.
	if (!::devilution::ReplaceFileAtomically(path.c_str(), (dir_ + filename).c_str())) {
		RemoveFile(path.c_str());
		transactionFailed_ = true;
		return false;
	}
	return true;
}

void SaveWriter::RemoveHashEntries(bool (*fnGetName)(uint8_t, char *))
{
	char pszFileName[MaxMpqPathSize];

	for (uint8_t i = 0; fnGetName(i, pszFileName); i++) {
		RemoveHashEntry(pszFileName);
	}
}
#endif

std::optional<SaveReader> OpenSaveArchive(uint32_t saveNum)
{
	return CreateSaveReader(GetSavePath(saveNum));
}

std::optional<SaveReader> OpenStashArchive()
{
	return CreateSaveReader(GetStashSavePath());
}

bool StashSaveFileExists()
{
	return FileExists(GetStashSavePath());
}

std::unique_ptr<byte[]> ReadArchive(SaveReader &archive, const char *pszName, size_t *pdwLen)
{
	int32_t error;
	std::size_t length;

	std::unique_ptr<byte[]> result = archive.ReadFile(pszName, length, error);
	if (error != 0)
		return nullptr;

	std::size_t decodedLength = codec_decode(result.get(), length, pfile_get_password());
	if (decodedLength == 0)
		return nullptr;

	if (pdwLen != nullptr)
		*pdwLen = decodedLength;

	return result;
}

const char *pfile_get_password()
{
	if (gbIsSpawn)
		return gbIsMultiplayer ? PASSWORD_SPAWN_MULTI : PASSWORD_SPAWN_SINGLE;
	return gbIsMultiplayer ? PASSWORD_MULTI : PASSWORD_SINGLE;
}

void pfile_write_hero(bool writeGameData)
{
	SaveWriter saveWriter = GetSaveWriter(gSaveNumber);
	pfile_write_hero(saveWriter, writeGameData);
}

#ifndef DISABLE_DEMOMODE
void pfile_write_hero_demo(int demo)
{
	std::string savePath = GetSavePath(gSaveNumber, StrCat("demo_", demo, "_reference_"));
	CopySaveFile(gSaveNumber, savePath);
	auto saveWriter = SaveWriter(savePath.c_str());
	pfile_write_hero(saveWriter, true);
}

HeroCompareResult pfile_compare_hero_demo(int demo, bool logDetails)
{
	std::string referenceSavePath = GetSavePath(gSaveNumber, StrCat("demo_", demo, "_reference_"));

	if (!FileExists(referenceSavePath.c_str()))
		return { HeroCompareResult::ReferenceNotFound, {} };

	std::string actualSavePath = GetSavePath(gSaveNumber, StrCat("demo_", demo, "_actual_"));
	{
		CopySaveFile(gSaveNumber, actualSavePath);
		SaveWriter saveWriter(actualSavePath.c_str());
		pfile_write_hero(saveWriter, true);
	}

	return CompareSaves(actualSavePath, referenceSavePath, logDetails);
}
#endif

void sfile_write_stash()
{
	if (!Stash.dirty || StashFileRefused) // a refused stash is left untouched (round 29 audit)
		return;

	{
		SaveWriter stashWriter = GetStashWriter();
		stashWriter.BeginTransaction();
		SaveStash(stashWriter);
		if (!stashWriter.CommitTransaction())
			oracool::NoteSaveWriteFailed("stash");
		// The writer is destroyed HERE, inside the block, so its header and tables are flushed
		// before the dirty flag is decided. Outside the block that decision would be made while the
		// most failure-prone write of all was still pending.
	}

	// Cleared only if the write actually happened. Audit finding, 2026-08-26: this used to clear
	// unconditionally, so a failed stash write marked the stash CLEAN - the next autosave saw
	// nothing to do, skipped it, and announced a successful save. The stash on disk stayed at its
	// previous contents with no further attempt to correct it, which is how an item moved out of
	// the stash exists in both places or in neither.
	if (!oracool::SaveAttemptFailed())
		Stash.dirty = false;
}

void SaveHeroAndStash(bool writeGameData)
{
	// A character and their stash are ONE state spread over two files, and until now they were
	// saved as two independent acts. Move an item from the stash into your pack and the two
	// disagree until both are written: if only the hero lands, the item is in your pack AND still
	// in the stash; if only the stash lands, it is in neither.
	//
	// The shadow (v1.9.59) made each file individually safe and, in doing so, made this pairing
	// WORSE. Before it, both files were being written at roughly the same time. After it, the
	// hero's shadow is published before the stash's records are written at all - so a disk that
	// fills up lands squarely in the gap, every time, rather than by chance.
	//
	// Two files cannot be swapped in one act; a filesystem does not offer that. What it does offer
	// is that a rename needs no disk space, and disk space is what actually runs out. So all the
	// risky work for BOTH archives happens first, and only when both are complete and waiting does
	// either become visible. A full disk now fails while both are still invisible, and publishes
	// neither.
	//
	// The residual window is between the two renames. It is not zero, and this comment is the
	// honest place to say so - but it is as small as this can be made without a journal.
	SaveWriter heroWriter = GetSaveWriter(gSaveNumber);
	// A hero record the archive refused makes the hero NOT ready (audit, 2026-09-29): the writer still finishes cleanly
	// on the aborted transaction's old record, so the old hero was published beside the NEW stash, and a move between
	// the two became a loss or a duplicate. Asked of this write alone - a failure noted before it is not this save's.
	const bool failedBefore = oracool::SaveAttemptFailed();
	pfile_write_hero(heroWriter, writeGameData);
	const bool heroRefused = oracool::SaveAttemptFailed() && !failedBefore;
	const bool heroReady = !heroRefused && heroWriter.Finish();

	// Never over a refused stash (round 29 audit): its writer found the damaged header, started from empty tables and
	// published an empty archive over the file the game had promised to leave untouched - every hero's stash gone on the
	// first purchase (TakePlrsMoney marks the stash dirty).
	const bool stashNeedsWriting = Stash.dirty && !StashFileRefused;
	std::optional<SaveWriter> stashWriter;
	bool stashReady = true;
	if (stashNeedsWriting) {
		// Constructed IN PLACE from the path, not moved in from GetStashWriter(). Passing a writer
		// here move-constructs one and then destroys the husk, and this crashed the game on picking
		// up gold (2026-08-26) - single-player gold goes straight to the stash, which makes this the
		// most-travelled save path there is. The move is safe now, but not making one is better:
		// there is no husk to reason about.
		stashWriter.emplace(GetStashSavePath());
		stashWriter->BeginTransaction();
		SaveStash(*stashWriter);
		stashReady = stashWriter->CommitTransaction() && stashWriter->Finish();
		if (!stashReady)
			oracool::NoteSaveWriteFailed("stash");
	}

	if (!heroReady || !stashReady) {
		// Neither is published. The character on disk is still the last one that fully succeeded,
		// and it still agrees with the stash beside it - which is the property worth protecting.
		heroWriter.DiscardShadow();
		if (stashWriter.has_value())
			stashWriter->DiscardShadow();
		return;
	}

	if (!heroWriter.Publish())
		return;
	if (stashWriter.has_value() && !stashWriter->Publish())
		return;

	// Cleared only once the stash is actually on disk. It used to be cleared whether or not the
	// write succeeded, so a failed stash write marked the stash CLEAN and no later save would try
	// again (audit, 2026-08-26).
	if (stashNeedsWriting)
		Stash.dirty = false;
}

bool pfile_ui_set_hero_infos(bool (*uiAddHeroInfo)(_uiheroinfo *))
{
	memset(hero_names, 0, sizeof(hero_names));

	for (uint32_t i = 0; i < MAX_CHARACTERS; i++) {
		std::optional<SaveReader> archive = OpenSaveArchive(i);
		if (archive) {
			PlayerPack pkplr;
			std::vector<uint8_t> chunkTail;
			if (ReadHero(*archive, &pkplr, &chunkTail)) {
				_uiheroinfo uihero;
				uihero.saveNumber = i;
				// Bounded (audit, 2026-09-27): a hero file whose name is not terminated ran strcpy past the table.
				CopyUtf8(hero_names[i], string_view(pkplr.pName, strnlen(pkplr.pName, sizeof(pkplr.pName))), sizeof(hero_names[i]));
				bool hasSaveGame = ArchiveContainsGame(*archive);
				if (hasSaveGame)
					pkplr.bIsHellfire = gbIsHellfireSaveGame ? 1 : 0;

				Player &player = Players[0];

				// The same sequence pfile_read_player_from_save runs, and against slot i's OWN
				// sidecars (external audit, 2026-08-17): this loop used to call the loaders'
				// gSaveNumber-reading forms, so every hero's preview wore whatever the GLOBAL slot
				// held - wrong equipment, wrong tabs, wrong sockets - and skipped the extension
				// chunks entirely, so tree passives and skill points were missing from the stats.
				UnPackPlayer(pkplr, player);
				// Not the last game's buffs, keyed by the player slot every preview shares: a Shout cast before Save & Exit gave
				// every hero in the list its armour (round 33 audit).
				oracool::ClearWarcryBuffs(player, /*recalc=*/false); // the preview's own CalcPlrInv follows (round 39 audit)
				oracool::ClearRfa12PlayerBuffs(player);
				oracool::ApplyHeroChunks(player, chunkTail.data(), chunkTail.size());
				LoadHeroItems(player, i);
				if (!gbIsMultiplayer) {
					LoadInventoryTabs(player, i);
				}
				RemoveAllInvalidItems(player);
				CalcPlrInv(player, false);
				// A preview's refusal is that slot's alone: left set, it made the next new hero skip clearing his page file,
				// and he inherited the old one (round 29 audit).
				InvTabsFileRefused = false;

				Game2UiPlayer(player, &uihero, hasSaveGame);
				uiAddHeroInfo(&uihero);
			}
		}
	}

	return true;
}

void pfile_ui_set_class_stats(unsigned int playerClass, _uidefaultstats *classStats)
{
	classStats->strength = PlayersData[playerClass].baseStr;
	classStats->magic = PlayersData[playerClass].baseMag;
	classStats->dexterity = PlayersData[playerClass].baseDex;
	classStats->vitality = PlayersData[playerClass].baseVit;
}

uint32_t pfile_ui_get_first_unused_save_num()
{
	uint32_t saveNum;
	for (saveNum = 0; saveNum < MAX_CHARACTERS; saveNum++) {
		// Not a slot whose file is there and only failed to open or read (round 61 audit: a hero file locked for a moment by
		// a sync or a scanner went unlisted, its slot looked free, and a new character was published over it).
		if (hero_names[saveNum][0] == '\0' && !FileExists(GetSavePath(saveNum)))
			break;
	}
	return saveNum;
}

bool pfile_ui_save_create(_uiheroinfo *heroinfo)
{
	PlayerPack pkplr;

	uint32_t saveNum = heroinfo->saveNumber;
	if (saveNum >= MAX_CHARACTERS)
		return false;
	// Never over a file that is there (round 61 audit): a new hero takes an empty slot, and a hero file that would not open
	// for the list is still somebody's character.
	if (FileExists(GetSavePath(saveNum)))
		return false;
	heroinfo->saveNumber = saveNum;
	InvTabsFileRefused = false; // a new hero starts with pages of his own (round 29 audit)

	giNumberOfLevels = gbIsHellfire ? 25 : 17;

	// Audit finding, 2026-08-26. This wrote four records and then returned `true` no matter what
	// any of them did, so a disk that was full at character creation produced a character the menu
	// listed, the game happily entered, and the archive did not actually contain. The name was
	// published into `hero_names` before a single byte was written, which is what made the phantom
	// visible in the first place.
	//
	// Same transaction the ordinary save uses, for the same reason: the four records only mean
	// anything together. The name is now claimed only once everything has landed.
	oracool::BeginSaveAttempt();

	Player &player = Players[0];
	bool committed = false;
	{
		// Scoped deliberately. The commit only swaps records INSIDE the archive; the archive
		// itself is published by the writer's destructor, so "did this save happen" cannot be
		// answered until the writer is gone. Asking before the closing brace is how the old code
		// would have been wrong even with the commit checked.
		SaveWriter saveWriter = GetSaveWriter(saveNum);
		saveWriter.RemoveHashEntries(GetFileName);
		saveWriter.BeginTransaction();

		CreatePlayer(player, heroinfo->heroclass);
		CopyUtf8(player._pName, heroinfo->name, PlayerNameLength);
		PackPlayer(pkplr, player);
		EncodeHero(saveWriter, &pkplr, oracool::BuildHeroChunkTail(player));
		if (!gbVanilla) {
			SaveHotkeys(saveWriter, player);
			SaveHeroItems(saveWriter, player);
			if (!gbIsMultiplayer) {
				SaveInventoryTabs(saveWriter, player);
			}
		}

		committed = saveWriter.CommitTransaction();
	}

	if (!committed || oracool::SaveAttemptFailed()) {
		// No save file, so there must be no character. The slot is left exactly as it was found,
		// free for another attempt once the player has made room on the disk.
		hero_names[saveNum][0] = '\0';
		return false;
	}

	CopyUtf8(hero_names[saveNum], heroinfo->name, sizeof(hero_names[saveNum]));
	Game2UiPlayer(player, heroinfo, false);
	return true;
}

bool pfile_delete_save(_uiheroinfo *heroInfo)
{
	uint32_t saveNum = heroInfo->saveNumber;
	if (saveNum < MAX_CHARACTERS) {
		hero_names[saveNum][0] = '\0';
		RemoveFile(GetSavePath(saveNum).c_str());
	}
	return true;
}

void pfile_read_player_from_save(uint32_t saveNum, Player &player)
{
	PlayerPack pkplr;
	std::vector<uint8_t> chunkTail;
	{
		std::optional<SaveReader> archive = OpenSaveArchive(saveNum);
		if (!archive)
			app_fatal(_("Unable to open archive"));
		if (!ReadHero(*archive, &pkplr, &chunkTail))
			app_fatal(_("Unable to load character"));

		gbValidSaveFile = ArchiveContainsGame(*archive);
		if (gbValidSaveFile)
			pkplr.bIsHellfire = gbIsHellfireSaveGame ? 1 : 0;
	}

	UnPackPlayer(pkplr, player);
	// AFTER unpack: the chunks widen or add to what the fixed struct decoded (skill points, the
	// 64-bit waypoint masks). A pre-tail hero has an empty vector here and this is a no-op.
	oracool::ApplyHeroChunks(player, chunkTail.data(), chunkTail.size());

	// The readied pair is decoded a SECOND time here, and for a class skill this is the decode that
	// counts (user, 2026-08-31: "lmb skills still dont load on new game and are set to regular
	// attack instead").
	//
	// UnPackPlayer decodes it against _pAblSpells as that stood BEFORE the chunk tail - and the tail
	// is what carries _pSkillInvestment, which is the only thing that grants a tree skill. So Zeal
	// on the left button was validated against a character who did not know Zeal yet,
	// UnpackReadiedSpell correctly refused it, and the button fell back to the basic attack. Nothing
	// downstream could recover it: InitPlayer rebuilds _pAblSpells later, but by then the slot is
	// already Invalid and there is no record of what it held.
	//
	// A spell survived the same trip because _pMemSpells rides in the FIXED pack, which is why this
	// read as "left button broken, right button fine" rather than as a save fault.
	//
	// RefreshInnateSpells first, so the validation below is asked of the character the chunks just
	// finished describing rather than the one the fixed struct alone could describe.
	oracool::RefreshInnateSpells(player);
	oracool::UnpackReadiedSpell(player, pkplr.pReadiedSpellRight, player._pRSpell, player._pRSplType);
	oracool::UnpackReadiedSpell(player, pkplr.pReadiedSpellLeft, player._pLRSpell, player._pLRSplType);

	// The real load: an item record of another format stops here, with the message. The hero-select
	// preview above tolerates it (the hero is listed without gear) so one old hero cannot brick the
	// menu for every slot (audit, 2026-09-19).
	if (!LoadHeroItems(player, saveNum))
		app_fatal(_("This save is from an incompatible version of Diablo Orcl and cannot be loaded. Please start a new character."));
	if (!gbIsMultiplayer) {
		LoadInventoryTabs(player, saveNum);
	}
	RemoveAllInvalidItems(player);
	CalcPlrInv(player, false);
	// The F-keys once more, now that every page and every item bonus is in: a left-button key bound to a scroll on page 2,
	// or to a staff whose requirement the pages' bonuses meet, decoded as nothing against page 1 alone - and the left
	// keys have no other source (round 29 audit). Additive: a binding already decoded stays.
	oracool::ReapplyHeroHotkeys(player);
}

void pfile_save_level()
{
	// Checked and said (user, 2026-10-01): a floor whose save failed came back as its previous visit's snapshot.
	oracool::BeginSaveAttempt();
	{
		// Scoped (round 58 audit): the archive is published by the writer's destructor - the header, the tables and the
		// rename, the likeliest writes to fail - so only after the closing brace does "did this save happen" have an answer.
		SaveWriter saveWriter = GetSaveWriter(gSaveNumber);
		SaveLevel(saveWriter);
		if (oracool::SaveAttemptFailed())
			ForgetUnsavedLevel(saveWriter);
	}
	if (oracool::SaveAttemptFailed()) {
		// The archive kept whatever it held before; a floor not marked visited is built anew, so that is never read.
		Player &myPlayer = *MyPlayer;
		if (!setlevel)
			myPlayer._pLvlVisited[currlevel] = false;
		else
			myPlayer._pSLvlVisited[setlvlnum] = false;
		const std::string failed = fmt::format(fmt::runtime(_("SAVE FAILED - this floor could not be saved (\"{:s}\"). It will be built anew when you return.")),
		    oracool::FailedSaveFileName());
		oracool::LogEvent(failed, UiFlags::ColorRed);
		InitDiabloMsg(failed);
	}
}

void pfile_convert_levels()
{
	SaveWriter saveWriter = GetSaveWriter(gSaveNumber);
	ConvertLevels(saveWriter);
}

void pfile_remove_temp_files()
{
	if (gbIsMultiplayer)
		return;

	SaveWriter saveWriter = GetSaveWriter(gSaveNumber);
	saveWriter.RemoveHashEntries(GetTempSaveNames);
}

void pfile_update(bool forceSave)
{
	static Uint32 prevTick;

	if (!gbIsMultiplayer)
		return;

	Uint32 tick = SDL_GetTicks();
	if (!forceSave && tick - prevTick <= 60000)
		return;

	prevTick = tick;
	pfile_write_hero();
	sfile_write_stash();
}

} // namespace devilution
