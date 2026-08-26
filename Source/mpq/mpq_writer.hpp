/**
 * @file mpq/mpq_writer.hpp
 *
 * Interface of functions for creating and editing MPQ files.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <string>

#include "mpq/mpq_common.hpp"
#include "utils/logged_fstream.hpp"
#include "utils/stdcompat/cstddef.hpp"

namespace devilution {
class MpqWriter {
public:
	explicit MpqWriter(const char *path);
	explicit MpqWriter(const std::string &path)
	    : MpqWriter(path.c_str())
	{
	}
	MpqWriter(MpqWriter &&other) = default;
	MpqWriter &operator=(MpqWriter &&other) = default;
	~MpqWriter();

	bool HasFile(const char *name) const;

	void RemoveHashEntry(const char *filename);
	void RemoveHashEntries(bool (*fnGetName)(uint8_t, char *));
	bool WriteFile(const char *filename, const byte *data, size_t size);
	void RenameFile(const char *name, const char *newName);

	/**
	 * @brief Makes the writes that follow ALL-OR-NOTHING until the matching commit.
	 *
	 * A save is several records - hero, hotkeys, items, inventory tabs - and until now only each
	 * one was individually survivable. Injecting a write failure part-way through the pre-fix
	 * writer produced a hero at the NEW level whose sidecar records had not been written: a torn
	 * save that looks perfectly fine and is not, which is a nastier thing to hand a player than an
	 * obviously missing file.
	 *
	 * Inside a transaction every record is written under its own temporary name and NOTHING is
	 * swapped in. The commit then performs the swaps, and that is the property the whole design
	 * rests on: a swap is a hash-table edit with no disk write in it, so once the data is safely on
	 * disk the visible change cannot fail half way. All the risk is spent before the commit begins.
	 *
	 * Not the default, because it must not change what a lone WriteFile does for every existing
	 * caller. Outside a transaction WriteFile behaves exactly as before: write temp, swap, done.
	 */
	void BeginTransaction();

	/**
	 * @brief Swaps in every record written since BeginTransaction. False if any of them failed.
	 *
	 * A false return means nothing was swapped in and every previous record is untouched - the
	 * archive still holds the last save that fully succeeded.
	 */
	bool CommitTransaction();

	/** @brief Discards every record written since BeginTransaction, leaving the originals alone. */
	void AbortTransaction();

private:
	/** @brief A record written under a temporary name, waiting for the commit to swap it in. */
	struct PendingSwap {
		std::string temp;
		std::string target;
	};

	std::string NextTempName();

	bool IsValidMpqHeader(MpqFileHeader *hdr) const;
	uint32_t GetHashIndex(uint32_t index, uint32_t hashA, uint32_t hashB) const;
	uint32_t FetchHandle(const char *filename) const;

	bool ReadMPQHeader(MpqFileHeader *hdr);
	MpqBlockEntry *AddFile(const char *filename, MpqBlockEntry *block, uint32_t blockIndex);
	bool WriteFileContents(const char *filename, const byte *fileData, size_t fileSize, MpqBlockEntry *block);

	// Returns an unused entry in the block entry table.
	MpqBlockEntry *NewBlock(uint32_t *blockIndex = nullptr);

	// Marks space at `blockOffset` of size `blockSize` as free (unused) space.
	void AllocBlock(uint32_t blockOffset, uint32_t blockSize);

	// Returns the file offset that is followed by empty space of at least the given size.
	uint32_t FindFreeBlock(uint32_t size);

	bool WriteHeaderAndTables();
	bool WriteHeader();
	bool WriteBlockTable();
	bool WriteHashTable();
	void InitDefaultMpqHeader(MpqFileHeader *hdr);

	LoggedFStream stream_;
	/** @brief The file actually being written - the shadow copy when there is one. */
	std::string name_;
	/** @brief Where the finished archive must end up. Equal to `name_` when writing in place. */
	std::string target_;
	/**
	 * @brief True when `name_` is a shadow beside `target_`, to be swapped in at close.
	 *
	 * False means the shadow could not be made and the writer fell back to editing the real
	 * archive - the pre-2026-08-26 behaviour, kept as a fallback rather than failing the save.
	 */
	bool usingShadow_ = false;
	std::uintmax_t size_ {};
	std::unique_ptr<MpqHashEntry[]> hashTable_;
	std::unique_ptr<MpqBlockEntry[]> blockTable_;

	bool inTransaction_ = false;
	/** @brief Set when any write inside the transaction failed, so the commit refuses. */
	bool transactionFailed_ = false;
	std::vector<PendingSwap> pending_;
	/** @brief Distinguishes concurrently-staged temporaries; several are outstanding at once. */
	uint32_t tempCounter_ = 0;

// Amiga cannot Seekp beyond EOF.
// See https://github.com/bebbo/libnix/issues/30
#ifndef __AMIGA__
#define CAN_SEEKP_BEYOND_EOF
#endif

#ifndef CAN_SEEKP_BEYOND_EOF
	long streamBegin_;
#endif
};

} // namespace devilution
