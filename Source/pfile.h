/**
 * @file pfile.h
 *
 * Interface of the save game encoding functionality.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "DiabloUI/diabloui.h"
#include "player.h"

#ifdef UNPACKED_SAVES
#include "utils/file_util.h"
#else
#include "mpq/mpq_reader.hpp"
#include "mpq/mpq_writer.hpp"
#endif

namespace devilution {

#define MAX_CHARACTERS 99

extern bool gbValidSaveFile;

#ifdef UNPACKED_SAVES
struct SaveReader {
	explicit SaveReader(std::string &&dir)
	    : dir_(std::move(dir))
	{
	}

	const std::string &dir() const
	{
		return dir_;
	}

	std::unique_ptr<byte[]> ReadFile(const char *filename, std::size_t &fileSize, int32_t &error);

	bool HasFile(const char *path)
	{
		return ::devilution::FileExists((dir_ + path).c_str());
	}

private:
	std::string dir_;
};

struct SaveWriter {
	explicit SaveWriter(std::string &&dir)
	    : dir_(std::move(dir))
	{
	}

	~SaveWriter();

	bool WriteFile(const char *filename, const byte *data, size_t size);

	bool HasFile(const char *path)
	{
		return ::devilution::FileExists((dir_ + path).c_str());
	}

	void RenameFile(const char *from, const char *to)
	{
		::devilution::RenameFile((dir_ + from).c_str(), (dir_ + to).c_str());
	}

	void RemoveHashEntry(const char *path)
	{
		RemoveFile((dir_ + path).c_str());
	}

	void RemoveHashEntries(bool (*fnGetName)(uint8_t, char *));

	/**
	 * @brief The same all-or-nothing contract MpqWriter offers, for the unpacked save directory.
	 *
	 * Audit finding, 2026-08-26: pfile.cpp calls BeginTransaction/CommitTransaction unconditionally,
	 * and this backend had neither - so UNPACKED_SAVES (which is how the RG99 port builds) had not
	 * compiled since the transaction landed. The build break is the visible half; the real one is
	 * that a save spread over four separate FILES needs the guarantee at least as much as an
	 * archive does.
	 *
	 * A directory has no hash table to edit, so the swap is a rename per record. That is not one
	 * atomic act the way the archive's single replacing rename is, and this comment is the honest
	 * place to say so: four renames can be interrupted after two. What it does buy is that all the
	 * DATA is safely on disk before any of it becomes visible, which is where the risk actually
	 * lives - a full disk fails during the writing, not during the renaming.
	 */
	void BeginTransaction();
	bool CommitTransaction();
	void AbortTransaction();

	/**
	 * @brief The deferred-publish trio, matching MpqWriter's - see mpq_writer.hpp for the why.
	 *
	 * A directory backend already stages every record under a temporary name, so all of an
	 * archive's risk is spent by the time the records are written and Finish() has nothing left to
	 * do but report whether they made it. Publish() is the batch of renames.
	 */
	bool Finish()
	{
		return !transactionFailed_;
	}
	bool Publish()
	{
		return CommitTransaction();
	}
	void DiscardShadow()
	{
		AbortTransaction();
	}

private:
	/** @brief A record written under a temporary name, waiting for the commit to swap it in. */
	struct PendingSwap {
		std::string temp;
		std::string target;
	};

	std::string dir_;
	bool inTransaction_ = false;
	bool transactionFailed_ = false;
	std::vector<PendingSwap> pending_;
};

#else
using SaveReader = MpqArchive;
using SaveWriter = MpqWriter;
#endif

/**
 * @brief Comparsion result of pfile_compare_hero_demo
 */
struct HeroCompareResult {
	enum Status : uint8_t {
		ReferenceNotFound,
		Same,
		Difference,
	};
	Status status;
	std::string message;
};

std::optional<SaveReader> OpenSaveArchive(uint32_t saveNum);
std::optional<SaveReader> OpenStashArchive();
const char *pfile_get_password();
std::unique_ptr<byte[]> ReadArchive(SaveReader &archive, const char *pszName, size_t *pdwLen = nullptr);
void pfile_write_hero(bool writeGameData = false);

#ifndef DISABLE_DEMOMODE
/**
 * @brief Save a reference game-state (save game) for the demo recording
 * @param demo that is recorded
 */
void pfile_write_hero_demo(int demo);
/**
 * @brief Compares the actual game-state (savegame) with a reference game-state (save game from demo recording)
 * @param demo for the comparsion
 * @param logDetails in case of a difference log details
 * @return The comparsion result.
 */
HeroCompareResult pfile_compare_hero_demo(int demo, bool logDetails);
#endif

void sfile_write_stash();

/**
 * @brief Saves the character AND the stash, publishing neither unless both are ready.
 *
 * Use this rather than `pfile_write_hero(); sfile_write_stash();`. The two files are one state -
 * an item moved between pack and stash exists in both or in neither until they agree - and saving
 * them as two independent acts leaves a window where a full disk lands between them. See the
 * comment on the definition for what this does and does not guarantee.
 */
void SaveHeroAndStash(bool writeGameData = false);
bool pfile_ui_set_hero_infos(bool (*uiAddHeroInfo)(_uiheroinfo *));
void pfile_ui_set_class_stats(unsigned int playerClass, _uidefaultstats *classStats);
uint32_t pfile_ui_get_first_unused_save_num();
bool pfile_ui_save_create(_uiheroinfo *heroinfo);
bool pfile_delete_save(_uiheroinfo *heroInfo);
void pfile_read_player_from_save(uint32_t saveNum, Player &player);
void pfile_save_level();
void pfile_convert_levels();
void pfile_remove_temp_files();
std::unique_ptr<byte[]> pfile_read(const char *pszName, size_t *pdwLen);
void pfile_update(bool forceSave);

} // namespace devilution
