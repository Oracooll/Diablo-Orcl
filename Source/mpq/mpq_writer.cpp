#include "mpq/mpq_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include "appfat.h"
#include "encrypt.h"
#include "engine.h"
#include "oracool/save_status.h"
#include "utils/endian_write.hpp"
#include "utils/file_util.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

// Validates that a Type is of a particular size and that its alignment is <= the size of the type.
// Done with templates so that error messages include actual size.
template <size_t A, size_t B>
struct AssertEq : std::true_type {
	static_assert(A == B, "A == B not satisfied");
};
template <size_t A, size_t B>
struct AssertLte : std::true_type {
	static_assert(A <= B, "A <= B not satisfied");
};
template <typename T, size_t S>
struct CheckSize : AssertEq<sizeof(T), S>, AssertLte<alignof(T), sizeof(T)> {
};

// Check sizes and alignments of the structs that we decrypt and encrypt.
// The decryption algorithm treats them as a stream of 32-bit uints, so the
// sizes must be exact as there cannot be any padding.
static_assert(CheckSize<MpqHashEntry, static_cast<size_t>(4 * 4)>::value, "sizeof(MpqHashEntry) == 4 * 4 && alignof(MpqHashEntry) <= 4 * 4 not satisfied");
static_assert(CheckSize<MpqBlockEntry, static_cast<size_t>(4 * 4)>::value, "sizeof(MpqBlockEntry) == 4 * 4 && alignof(MpqBlockEntry) <= 4 * 4 not satisfied");

// We use fixed size block and hash entry tables.
constexpr uint32_t HashEntriesCount = 2048;
constexpr uint32_t BlockEntriesCount = 2048;
constexpr uint32_t BlockEntrySize = HashEntriesCount * sizeof(MpqBlockEntry);
constexpr uint32_t HashEntrySize = BlockEntriesCount * sizeof(MpqHashEntry);

// We store the block and the hash entry tables immediately after the header.
// This is unlike most other MPQ archives, that store these at the end of the file.
constexpr long MpqBlockEntryOffset = sizeof(MpqFileHeader);
constexpr long MpqHashEntryOffset = MpqBlockEntryOffset + BlockEntrySize;

// Special return value for `GetHashIndex` and `GetHandle`.
constexpr uint32_t HashEntryNotFound = -1;

// We use 4096-byte blocks, generally.
constexpr uint16_t BlockSizeFactor = 3;
constexpr uint32_t BlockSize = 512 << BlockSizeFactor; // 4096

// Sometimes we can end up with smaller blocks.
constexpr uint32_t MinBlockSize = 1024;

void ByteSwapHdr(MpqFileHeader *hdr)
{
	hdr->signature = SDL_SwapLE32(hdr->signature);
	hdr->headerSize = SDL_SwapLE32(hdr->headerSize);
	hdr->fileSize = SDL_SwapLE32(hdr->fileSize);
	hdr->version = SDL_SwapLE16(hdr->version);
	hdr->blockSizeFactor = SDL_SwapLE16(hdr->blockSizeFactor);
	hdr->hashEntriesOffset = SDL_SwapLE32(hdr->hashEntriesOffset);
	hdr->blockEntriesOffset = SDL_SwapLE32(hdr->blockEntriesOffset);
	hdr->hashEntriesCount = SDL_SwapLE32(hdr->hashEntriesCount);
	hdr->blockEntriesCount = SDL_SwapLE32(hdr->blockEntriesCount);
}

bool IsAllocatedUnusedBlock(const MpqBlockEntry *block)
{
	return block->offset != 0 && block->flags == 0 && block->unpackedSize == 0;
}

bool IsUnallocatedBlock(const MpqBlockEntry *block)
{
	return block->offset == 0 && block->packedSize == 0 && block->unpackedSize == 0 && block->flags == 0;
}

} // namespace

MpqWriter::MpqWriter(const char *requestedPath)
{
	const std::string dir = std::string(Dirname(requestedPath));
	RecursivelyCreateDir(dir.c_str());

	// Built BESIDE the archive, not inside it. Audit finding, 2026-08-26, and the last hole in the
	// save transaction.
	//
	// The in-archive transaction made the RECORDS all-or-nothing, but the header, block table and
	// hash table are written at close, one after another, straight into the live file. A failure
	// between the block table and the hash table leaves the old hash entries - which name the old
	// records - pointing at block entries the commit has already changed. The archive is then
	// neither the old save nor the new one, and it is not detectably either: the names resolve,
	// they just resolve to the wrong bytes. No amount of care ordering three writes fixes that,
	// because the guarantee needed is "all three or none" and a file cannot give it.
	//
	// So the whole session works on a copy, and the finished archive is swapped in by a single
	// replacing rename. Every failure - a record, a table, the resize - leaves the original file
	// untouched, because nothing ever wrote to it.
	target_ = requestedPath;
	std::string workPath = target_ + ".tmp";
	// A shadow left behind by a save that was interrupted (a crash, a power cut). It describes a
	// save that was never published, so it is worth nothing and is in the way.
	if (FileExists(workPath.c_str()))
		RemoveFile(workPath.c_str());

	// FAILS CLOSED, in both directions. Audit finding, 2026-08-26, and the previous version of this
	// got both halves wrong:
	//
	// - It fell back to "writing in place" when the copy failed. But a copy fails because the disk
	//   is full, which is precisely the condition the shadow exists to survive - so the fallback
	//   removed the protection at the only moment it was needed, and mutated the live archive.
	// - It folded a FAILED GetFileSize into "the target does not exist". An existing archive whose
	//   size could not be read was therefore treated as new, and a new archive holds only the
	//   records this save writes. Publishing that over a real save would have discarded everything
	//   else in it - the level records among them.
	//
	// So: any doubt about the target, or any failure staging the shadow, and the writer opens
	// nothing at all. WriteFile then refuses, nothing is published, and the archive on disk is left
	// exactly as it was. A save that does not happen and says so is the good outcome here.
	std::uintmax_t targetSize = 0;
	bool staged = true;
	if (FileExists(target_.c_str())) {
		if (!GetFileSize(target_.c_str(), &targetSize)) {
			LogError("Could not measure {} - refusing to write over an archive we cannot read", target_);
			staged = false;
		} else if (targetSize > 0) {
			CopyFileOverwrite(target_.c_str(), workPath.c_str());
			std::uintmax_t copiedSize = 0;
			// Checked rather than assumed: CopyFileOverwrite reports nothing back. Editing a shadow
			// that is a TRUNCATED copy and then swapping it over the real archive would destroy the
			// save, so a copy that did not land byte for byte disqualifies the whole attempt.
			if (!GetFileSize(workPath.c_str(), &copiedSize) || copiedSize != targetSize) {
				LogError("Could not stage a copy of {} - the save will not be attempted", target_);
				RemoveFile(workPath.c_str());
				staged = false;
			}
		}
		// A zero-length file is not an archive. Nothing to copy, and nothing to lose by replacing.
	}

	if (!staged) {
		// Deliberately leaves the writer inert rather than calling app_fatal: this runs from the
		// autosave, and killing the game because one save could not be staged would turn a
		// recoverable full disk into a lost session.
		oracool::NoteSaveWriteFailed(target_);
		return;
	}

	usingShadow_ = true;
	const std::string pathStorage = workPath;
	const char *path = pathStorage.c_str();

	LogVerbose("Opening {}", path);
	bool isNewFile = false;
	std::string error;

	// ONE open, not three. Upstream created the file with "ab", closed it, measured it, and
	// reopened it "r+b" - a dance that exists because it cannot know whether the file is already
	// there. The shadow always can: this path has just established that `workPath` does not exist,
	// because the constructor removed any stale one a few lines above.
	//
	// Worth removing rather than tidying. Creating a file and immediately reopening it is the
	// classic way to meet a transient sharing violation from a virus scanner or a sync client
	// reacting to the new file - and since 2026-08-25 every single save creates a brand new file
	// this way, where before it reopened a long-lived archive. That multiplied the exposure by
	// every save the game makes.
	isNewFile = !FileExists(path);
	if (isNewFile) {
		size_ = 0;
		// "w+b" creates and gives read/write in one call. It truncates, which would be wrong for a
		// file that might already hold an archive - and is exactly right for one we know does not.
		if (!stream_.Open(path, "w+b")) {
			error = "Failed to create file";
			goto on_error;
		}
	} else {
		if (!GetFileSize(path, &size_)) {
			error = R"(GetFileSize failed: "{}")";
			LogError(error, path, std::strerror(errno));
			goto on_error;
		}
		isNewFile = size_ == 0;
		LogVerbose("GetFileSize(\"{}\") = {}", path, size_);

		if (!stream_.Open(path, "r+b")) {
			stream_.Close();
			error = "Failed to open file";
			goto on_error;
		}
	}

	name_ = path;

	if (blockTable_ == nullptr || hashTable_ == nullptr) {
		MpqFileHeader fhdr;
		if (isNewFile) {
			InitDefaultMpqHeader(&fhdr);
		} else if (!ReadMPQHeader(&fhdr)) {
			error = "Failed to read MPQ header";
			goto on_error;
		}
		blockTable_ = std::make_unique<MpqBlockEntry[]>(BlockEntriesCount);
		std::memset(blockTable_.get(), 0, BlockEntriesCount * sizeof(MpqBlockEntry));
		if (fhdr.blockEntriesCount > 0) {
			if (!stream_.Read(reinterpret_cast<char *>(blockTable_.get()), static_cast<size_t>(fhdr.blockEntriesCount * sizeof(MpqBlockEntry)))) {
				error = "Failed to read block table";
				goto on_error;
			}
			uint32_t key = Hash("(block table)", 3);
			Decrypt(reinterpret_cast<uint32_t *>(blockTable_.get()), fhdr.blockEntriesCount * sizeof(MpqBlockEntry), key);
		}
		hashTable_ = std::make_unique<MpqHashEntry[]>(HashEntriesCount);

		// We fill with 0xFF so that the `block` field defaults to -1 (a null block pointer).
		std::memset(hashTable_.get(), 0xFF, HashEntriesCount * sizeof(MpqHashEntry));

		if (fhdr.hashEntriesCount > 0) {
			if (!stream_.Read(reinterpret_cast<char *>(hashTable_.get()), static_cast<size_t>(fhdr.hashEntriesCount * sizeof(MpqHashEntry)))) {
				error = "Failed to read hash entries";
				goto on_error;
			}
			uint32_t key = Hash("(hash table)", 3);
			Decrypt(reinterpret_cast<uint32_t *>(hashTable_.get()), fhdr.hashEntriesCount * sizeof(MpqHashEntry), key);
		}

#ifndef CAN_SEEKP_BEYOND_EOF
		if (!stream_.Seekp(0, SEEK_SET))
			goto on_error;

		// Memorize stream begin, we'll need it for calculations later.
		if (!stream_.Tellp(&streamBegin_))
			goto on_error;

		// Write garbage header and tables because some platforms cannot `Seekp` beyond EOF.
		// The data is incorrect at this point, it will be overwritten on Close.
		if (isNewFile)
			WriteHeaderAndTables();
#endif
	}
	return;
on_error:
	// NOT app_fatal. This is the change the crash report forced, and it is the same judgement
	// already written a few lines above for the staging step: a save that cannot be written is a
	// failed save, not a reason to end the session.
	//
	// It used to kill the game outright, and it is reachable from the ordinary autosave - which in
	// single-player fires on picking up gold, because gold goes to the stash. So one unlucky file
	// operation, of the kind a virus scanner or a sync client causes routinely, took the player's
	// whole session with it. Killing the process also guaranteed the loss it was reacting to: the
	// hero archive at that moment is finished and waiting to be published, and a dead process never
	// publishes it.
	//
	// Left inert instead. WriteFile refuses, nothing is published, the archive on disk is untouched,
	// and the player is told the save did not happen.
	LogError("Failed to open archive for writing: {} ({})", path, error);
	stream_.Close();
	if (usingShadow_ && FileExists(path))
		RemoveFile(path);
	usingShadow_ = false;
	name_.clear();
	oracool::NoteSaveWriteFailed(target_);
}

void MpqWriter::MakeInert()
{
	// The husk left behind by a move. It must not write, publish, or delete anything.
	//
	// `finished_ = true` is what does the work: the destructor reads that as "the caller has
	// already decided what to do with this archive", and so decides nothing itself. The cleared
	// paths and the cleared shadow flag mean that even if it did, there is nothing to act on.
	usingShadow_ = false;
	finished_ = true;
	finishedCleanly_ = false;
	inTransaction_ = false;
	transactionFailed_ = false;
	pending_.clear();
	name_.clear();
	target_.clear();
}

MpqWriter::MpqWriter(MpqWriter &&other) noexcept
    : stream_(std::move(other.stream_))
    , name_(std::move(other.name_))
    , target_(std::move(other.target_))
    , usingShadow_(other.usingShadow_)
    , finished_(other.finished_)
    , finishedCleanly_(other.finishedCleanly_)
    , size_(other.size_)
    , hashTable_(std::move(other.hashTable_))
    , blockTable_(std::move(other.blockTable_))
    , inTransaction_(other.inTransaction_)
    , transactionFailed_(other.transactionFailed_)
    , pending_(std::move(other.pending_))
    , tempCounter_(other.tempCounter_)
{
	other.MakeInert();
}

MpqWriter &MpqWriter::operator=(MpqWriter &&other) noexcept
{
	if (this != &other) {
		// Whatever this writer was holding is published first. Dropping an open archive on the
		// floor because something was assigned over it would silently lose a save.
		if (!finished_ && stream_.IsOpen())
			Publish();
		stream_ = std::move(other.stream_);
		name_ = std::move(other.name_);
		target_ = std::move(other.target_);
		usingShadow_ = other.usingShadow_;
		finished_ = other.finished_;
		finishedCleanly_ = other.finishedCleanly_;
		size_ = other.size_;
		hashTable_ = std::move(other.hashTable_);
		blockTable_ = std::move(other.blockTable_);
		inTransaction_ = other.inTransaction_;
		transactionFailed_ = other.transactionFailed_;
		pending_ = std::move(other.pending_);
		tempCounter_ = other.tempCounter_;
		other.MakeInert();
	}
	return *this;
}

bool MpqWriter::WriteOutAndClose()
{
	// A transaction still open here was never committed - the save was abandoned, or an early
	// return skipped the commit. ABORTED rather than committed, deliberately: committing would
	// swap in whichever records happened to have been written, which is the torn save this
	// mechanism exists to prevent. Discarding them leaves the previous save whole.
	if (inTransaction_ || !pending_.empty())
		AbortTransaction();

	if (!stream_.IsOpen())
		return false;

	LogVerbose("Closing {}", name_);

	bool result = true;
	if (!(stream_.Seekp(0, SEEK_SET) && WriteHeaderAndTables()))
		result = false;
	// Checked, not merely performed. Audit finding, 2026-08-26: the tables can all "write"
	// successfully into the C library's buffer and only fail when that buffer is flushed, which
	// happens HERE - and the publish is the very next thing. An unchecked close meant a shadow that
	// had failed to write its last kilobytes was swapped over the good archive.
	if (!stream_.Close())
		result = false;
	if (result && size_ != 0) {
		LogVerbose("ResizeFile(\"{}\", {})", name_, size_);
		result = ResizeFile(name_.c_str(), size_);
	}

	if (!result) {
		LogVerbose("Closing failed {}", name_);
		// Found by the failure sweep (v1.9.57), and it is the last place a save could lie.
		//
		// The header, block table and hash table are written HERE and nowhere else - so every
		// record can write perfectly, the transaction can commit, and the archive on disk still
		// points at the old records because the tables never landed. The old code noted that at
		// verbose level and returned, so the save reported SUCCESS while nothing it had done was
		// visible.
		//
		// Reported through the same seam every other write failure uses, so the player is told the
		// save did not happen rather than discovering it next time they load.
		oracool::NoteSaveWriteFailed(target_);
	}
	return result;
}

bool MpqWriter::Finish()
{
	if (finished_)
		return finishedCleanly_;
	finished_ = true;
	finishedCleanly_ = WriteOutAndClose();
	if (!finishedCleanly_ && usingShadow_ && !name_.empty())
		RemoveFile(name_.c_str());
	return finishedCleanly_;
}

bool MpqWriter::Publish()
{
	if (!Finish())
		return false;
	if (!usingShadow_) {
		// Written straight into the real archive because the shadow could not be staged. It is
		// already as published as it is going to get.
		return true;
	}
	if (!ReplaceFileAtomically(name_.c_str(), target_.c_str())) {
		LogError("Could not publish {} over {}", name_, target_);
		RemoveFile(name_.c_str());
		oracool::NoteSaveWriteFailed(target_);
		return false;
	}
	// Nothing left to discard, and nothing for the destructor to clean up.
	usingShadow_ = false;
	return true;
}

void MpqWriter::DiscardShadow()
{
	Finish();
	if (usingShadow_ && !name_.empty()) {
		RemoveFile(name_.c_str());
		usingShadow_ = false;
	}
}

MpqWriter::~MpqWriter()
{
	if (finished_) {
		// Finish() was called explicitly, so the caller owns the publish decision. A shadow still
		// sitting here means they never made one - discarded, because a finished archive nobody
		// published is a save nobody asked for.
		if (usingShadow_ && !name_.empty())
			RemoveFile(name_.c_str());
		return;
	}

	// The ordinary path, for every caller that just wants a save written: finish and publish in
	// one go. Unchanged in behaviour from before Finish()/Publish() existed.
	if (!stream_.IsOpen()) {
		// Nothing was ever opened, so there is nothing to publish - but a shadow may still be
		// sitting on disk from the copy in the constructor, and it must not outlive the writer.
		if (inTransaction_ || !pending_.empty())
			AbortTransaction();
		if (usingShadow_ && !name_.empty())
			RemoveFile(name_.c_str());
		return;
	}
	Publish();
}

uint32_t MpqWriter::FetchHandle(const char *filename) const
{
	return GetHashIndex(Hash(filename, 0), Hash(filename, 1), Hash(filename, 2));
}

void MpqWriter::InitDefaultMpqHeader(MpqFileHeader *hdr)
{
	std::memset(hdr, 0, sizeof(*hdr));
	hdr->signature = MpqFileHeader::DiabloSignature;
	hdr->headerSize = MpqFileHeader::DiabloSize;
	hdr->blockSizeFactor = BlockSizeFactor;
	hdr->version = 0;
	size_ = MpqHashEntryOffset + HashEntrySize;
}

bool MpqWriter::IsValidMpqHeader(MpqFileHeader *hdr) const
{
	return hdr->signature == MpqFileHeader::DiabloSignature
	    && hdr->headerSize == MpqFileHeader::DiabloSize
	    && hdr->version <= 0
	    && hdr->blockSizeFactor == BlockSizeFactor
	    && hdr->fileSize == size_
	    && hdr->hashEntriesOffset == MpqHashEntryOffset
	    && hdr->blockEntriesOffset == sizeof(MpqFileHeader)
	    && hdr->hashEntriesCount == HashEntriesCount
	    && hdr->blockEntriesCount == BlockEntriesCount;
}

bool MpqWriter::ReadMPQHeader(MpqFileHeader *hdr)
{
	const bool hasHdr = size_ >= sizeof(*hdr);
	if (hasHdr) {
		if (!stream_.Read(reinterpret_cast<char *>(hdr), sizeof(*hdr)))
			return false;
		ByteSwapHdr(hdr);
	}
	if (!hasHdr || !IsValidMpqHeader(hdr)) {
		InitDefaultMpqHeader(hdr);
	}
	return true;
}

MpqBlockEntry *MpqWriter::NewBlock(uint32_t *blockIndex)
{
	MpqBlockEntry *blockEntry = blockTable_.get();

	for (unsigned i = 0; i < BlockEntriesCount; ++i, ++blockEntry) {
		if (!IsUnallocatedBlock(blockEntry))
			continue;

		if (blockIndex != nullptr)
			*blockIndex = i;

		return blockEntry;
	}

	app_fatal("Out of free block entries");
}

void MpqWriter::AllocBlock(uint32_t blockOffset, uint32_t blockSize)
{
	MpqBlockEntry *block;
	bool expand;
	do {
		block = blockTable_.get();
		expand = false;
		for (unsigned i = BlockEntriesCount; i-- != 0; ++block) {
			// Expand to adjacent blocks.
			if (!IsAllocatedUnusedBlock(block))
				continue;
			if (block->offset + block->packedSize == blockOffset) {
				blockOffset = block->offset;
				blockSize += block->packedSize;
				memset(block, 0, sizeof(MpqBlockEntry));
				expand = true;
				break;
			}
			if (blockOffset + blockSize == block->offset) {
				blockSize += block->packedSize;
				memset(block, 0, sizeof(MpqBlockEntry));
				expand = true;
				break;
			}
		}
	} while (expand);
	if (blockOffset + blockSize > size_) {
		// Expanded beyond EOF, this should never happen.
		app_fatal("MPQ free list error");
	}
	if (blockOffset + blockSize == size_) {
		size_ = blockOffset;
	} else {
		block = NewBlock();
		block->offset = blockOffset;
		block->packedSize = blockSize;
		block->unpackedSize = 0;
		block->flags = 0;
	}
}

uint32_t MpqWriter::FindFreeBlock(uint32_t size)
{
	uint32_t result;

	MpqBlockEntry *block = blockTable_.get();
	for (unsigned i = 0; i < BlockEntriesCount; ++i, ++block) {
		// Find a block entry to use space from.
		if (!IsAllocatedUnusedBlock(block) || block->packedSize < size)
			continue;

		result = block->offset;
		block->offset += size;
		block->packedSize -= size;

		// Clear the block entry if we used its entire capacity.
		if (block->packedSize == 0)
			memset(block, 0, sizeof(*block));

		return result;
	}

	result = size_;
	size_ += size;
	return result;
}

uint32_t MpqWriter::GetHashIndex(uint32_t index, uint32_t hashA, uint32_t hashB) const // NOLINT(bugprone-easily-swappable-parameters)
{
	uint32_t i = HashEntriesCount;
	for (unsigned idx = index & 0x7FF; hashTable_[idx].block != MpqHashEntry::NullBlock; idx = (idx + 1) & 0x7FF) {
		if (i-- == 0)
			break;
		if (hashTable_[idx].hashA != hashA)
			continue;
		if (hashTable_[idx].hashB != hashB)
			continue;
		if (hashTable_[idx].block == MpqHashEntry::DeletedBlock)
			continue;

		return idx;
	}

	return HashEntryNotFound;
}

bool MpqWriter::WriteHeaderAndTables()
{
	return WriteHeader() && WriteBlockTable() && WriteHashTable();
}

MpqBlockEntry *MpqWriter::AddFile(const char *filename, MpqBlockEntry *block, uint32_t blockIndex)
{
	uint32_t h1 = Hash(filename, 0);
	uint32_t h2 = Hash(filename, 1);
	uint32_t h3 = Hash(filename, 2);
	if (GetHashIndex(h1, h2, h3) != HashEntryNotFound)
		app_fatal(StrCat("Hash collision between \"", filename, "\" and existing file\n"));
	unsigned int hIdx = h1 & 0x7FF;

	bool hasSpace = false;
	for (unsigned i = 0; i < HashEntriesCount; ++i) {
		if (hashTable_[hIdx].block == MpqHashEntry::NullBlock || hashTable_[hIdx].block == MpqHashEntry::DeletedBlock) {
			hasSpace = true;
			break;
		}
		hIdx = (hIdx + 1) & 0x7FF;
	}
	if (!hasSpace)
		app_fatal("Out of hash space");

	if (block == nullptr)
		block = NewBlock(&blockIndex);

	MpqHashEntry &entry = hashTable_[hIdx];
	entry.hashA = h2;
	entry.hashB = h3;
	entry.locale = 0;
	entry.platform = 0;
	entry.block = blockIndex;

	return block;
}

bool MpqWriter::WriteFileContents(const char *filename, const byte *fileData, size_t fileSize, MpqBlockEntry *block)
{
	const char *tmp;
	while ((tmp = strchr(filename, ':')) != nullptr)
		filename = tmp + 1;
	while ((tmp = strchr(filename, '\\')) != nullptr)
		filename = tmp + 1;
	Hash(filename, 3);

	const uint32_t numSectors = (fileSize + (BlockSize - 1)) / BlockSize;
	const uint32_t offsetTableByteSize = sizeof(uint32_t) * (numSectors + 1);
	block->offset = FindFreeBlock(fileSize + offsetTableByteSize);
	// `packedSize` is reduced at the end of the function if it turns out to be smaller.
	block->packedSize = fileSize + offsetTableByteSize;
	block->unpackedSize = fileSize;
	block->flags = MpqBlockEntry::FlagExists | MpqBlockEntry::CompressPkZip;

	// We populate the table of sector offsets while we write the data.
	// We can't pre-populate it because we don't know the compressed sector sizes yet.
	// First offset is the start of the first sector, last offset is the end of the last sector.
	std::unique_ptr<uint32_t[]> offsetTable { new uint32_t[numSectors + 1] };

#ifdef CAN_SEEKP_BEYOND_EOF
	if (!stream_.Seekp(block->offset + offsetTableByteSize, SEEK_SET))
		return false;
#else
	// Ensure we do not Seekp beyond EOF by filling the missing space.
	long stream_end;
	if (!stream_.Seekp(0, SEEK_END) || !stream_.Tellp(&stream_end))
		return false;
	const std::uintmax_t cur_size = stream_end - streamBegin_;
	if (cur_size < block->offset + offsetTableByteSize) {
		if (cur_size < block->offset) {
			std::unique_ptr<char[]> filler { new char[block->offset - cur_size] };
			if (!stream_.Write(filler.get(), block->offset - cur_size))
				return false;
		}
		if (!stream_.Write(reinterpret_cast<const char *>(offsetTable.get()), offsetTableByteSize))
			return false;
	} else {
		if (!stream_.Seekp(block->offset + offsetTableByteSize, SEEK_SET))
			return false;
	}
#endif

	uint32_t destSize = offsetTableByteSize;
	byte mpqBuf[BlockSize];
	size_t curSector = 0;
	while (true) {
		uint32_t len = std::min<uint32_t>(fileSize, BlockSize);
		memcpy(mpqBuf, fileData, len);
		fileData += len;
		len = PkwareCompress(mpqBuf, len);
		if (!stream_.Write(reinterpret_cast<const char *>(&mpqBuf[0]), len))
			return false;
		offsetTable[curSector++] = SDL_SwapLE32(destSize);
		destSize += len; // compressed length
		if (fileSize <= BlockSize)
			break;

		fileSize -= BlockSize;
	}

	offsetTable[numSectors] = SDL_SwapLE32(destSize);
	if (!stream_.Seekp(block->offset, SEEK_SET))
		return false;
	if (!stream_.Write(reinterpret_cast<const char *>(offsetTable.get()), offsetTableByteSize))
		return false;
	if (!stream_.Seekp(destSize - offsetTableByteSize, SEEK_CUR))
		return false;

	if (destSize < block->packedSize) {
		const uint32_t remainingBlockSize = block->packedSize - destSize;
		if (remainingBlockSize >= MinBlockSize) {
			// Allocate another block if we didn't use all of this one.
			block->packedSize = destSize;
			AllocBlock(block->packedSize + block->offset, remainingBlockSize);
		}
	}
	return true;
}

bool MpqWriter::WriteHeader()
{
	MpqFileHeader fhdr;

	memset(&fhdr, 0, sizeof(fhdr));
	fhdr.signature = MpqFileHeader::DiabloSignature;
	fhdr.headerSize = MpqFileHeader::DiabloSize;
	fhdr.fileSize = static_cast<uint32_t>(size_);
	fhdr.version = 0;
	fhdr.blockSizeFactor = BlockSizeFactor;
	fhdr.hashEntriesOffset = MpqHashEntryOffset;
	fhdr.blockEntriesOffset = MpqBlockEntryOffset;
	fhdr.hashEntriesCount = HashEntriesCount;
	fhdr.blockEntriesCount = BlockEntriesCount;
	ByteSwapHdr(&fhdr);

	return stream_.Write(reinterpret_cast<const char *>(&fhdr), sizeof(fhdr));
}

bool MpqWriter::WriteBlockTable()
{
	Encrypt(reinterpret_cast<uint32_t *>(blockTable_.get()), BlockEntrySize, Hash("(block table)", 3));
	const bool success = stream_.Write(reinterpret_cast<const char *>(blockTable_.get()), BlockEntrySize);
	Decrypt(reinterpret_cast<uint32_t *>(blockTable_.get()), BlockEntrySize, Hash("(block table)", 3));
	return success;
}

bool MpqWriter::WriteHashTable()
{
	Encrypt(reinterpret_cast<uint32_t *>(hashTable_.get()), HashEntrySize, Hash("(hash table)", 3));
	const bool success = stream_.Write(reinterpret_cast<const char *>(hashTable_.get()), HashEntrySize);
	Decrypt(reinterpret_cast<uint32_t *>(hashTable_.get()), HashEntrySize, Hash("(hash table)", 3));
	return success;
}

void MpqWriter::RemoveHashEntry(const char *filename)
{
	uint32_t hIdx = FetchHandle(filename);
	if (hIdx == HashEntryNotFound) {
		return;
	}

	MpqHashEntry *hashEntry = &hashTable_[hIdx];
	MpqBlockEntry *block = &blockTable_[hashEntry->block];
	hashEntry->block = MpqHashEntry::DeletedBlock;
	const uint32_t blockOffset = block->offset;
	const uint32_t blockSize = block->packedSize;
	memset(block, 0, sizeof(*block));
	AllocBlock(blockOffset, blockSize);
}

void MpqWriter::RemoveHashEntries(bool (*fnGetName)(uint8_t, char *))
{
	char pszFileName[MaxMpqPathSize];

	for (uint8_t i = 0; fnGetName(i, pszFileName); i++) {
		RemoveHashEntry(pszFileName);
	}
}

bool MpqWriter::WriteFile(const char *filename, const byte *data, size_t size)
{
	// Oracool (audit, 2026-08-26): written under a TEMPORARY name and only then swapped in, so a
	// failed write cannot destroy the record it was replacing.
	//
	// This used to RemoveHashEntry(filename) FIRST and write afterwards. A short write - a full
	// disk, a failing drive, a device pulled mid-save - therefore deleted the existing "hero" and
	// left nothing in its place, and the caller was told nothing because the result was discarded.
	// The character was gone, and the act that destroyed it was the act of saving.
	//
	// Safe to rename rather than rewrite because the stored bytes are NOT keyed to the filename:
	// blocks are written with FlagExists | CompressPkZip and never the encrypted flag, so the
	// vestigial `Hash(filename, 3)` in WriteFileContents - which in a real MPQ would be the file's
	// encryption key - has no effect on the data. Verified before relying on it.
	// The writer never opened, because staging its shadow failed - see the constructor. Refused
	// here rather than crashing on a null stream, and the refusal travels back to the caller as an
	// ordinary write failure, which is what it is.
	if (!stream_.IsOpen()) {
		transactionFailed_ = true;
		return false;
	}

	const std::string temp = NextTempName();

	// A previous attempt that died between writing and swapping would leave this behind. Reclaimed
	// rather than collided with: AddFile calls app_fatal on a hash collision.
	RemoveHashEntry(temp.c_str());

	MpqBlockEntry *blockEntry = AddFile(temp.c_str(), nullptr, 0);
	if (!WriteFileContents(temp.c_str(), data, size, blockEntry)) {
		RemoveHashEntry(temp.c_str());
		// Inside a transaction the failure is remembered rather than reported and forgotten: the
		// commit has to refuse, or the records that DID write would be swapped in on their own and
		// produce exactly the torn save the transaction exists to prevent.
		transactionFailed_ = true;
		return false;
	}

	if (inTransaction_) {
		// Staged, not swapped. The old record is still the live one until the commit.
		pending_.push_back({ temp, filename });
		return true;
	}

	// Outside a transaction: swap immediately, as this has always done. The old record is given up
	// only now, so both exist for the duration of these two lines and there is no window in which
	// neither does.
	RemoveHashEntry(filename);
	RenameFile(temp.c_str(), filename);
	return true;
}

std::string MpqWriter::NextTempName()
{
	// Unique per staged record, because a transaction holds several at once. The name is deliberately
	// one no real save record can collide with.
	return StrCat("~orcl_stage_", tempCounter_++, ".tmp");
}

void MpqWriter::BeginTransaction()
{
	// Any leftovers from a transaction nobody finished are dropped rather than inherited.
	AbortTransaction();
	inTransaction_ = true;
	transactionFailed_ = false;
}

bool MpqWriter::CommitTransaction()
{
	inTransaction_ = false;
	if (transactionFailed_) {
		// Nothing is swapped in. Every original record stays exactly as it was, so what remains on
		// disk is the last save that fully succeeded rather than a mixture of two.
		AbortTransaction();
		return false;
	}

	// The commit itself. Every one of these is a hash-table edit - no seek, no fwrite, nothing that
	// can fail on a full disk - which is what makes "all or nothing" a real guarantee rather than a
	// smaller window. All the risky I/O already happened, above, under names nobody was reading.
	for (const PendingSwap &swap : pending_) {
		RemoveHashEntry(swap.target.c_str());
		RenameFile(swap.temp.c_str(), swap.target.c_str());
	}
	pending_.clear();
	transactionFailed_ = false;
	return true;
}

void MpqWriter::AbortTransaction()
{
	inTransaction_ = false;
	transactionFailed_ = false;
	for (const PendingSwap &swap : pending_)
		RemoveHashEntry(swap.temp.c_str());
	pending_.clear();
}

void MpqWriter::RenameFile(const char *name, const char *newName) // NOLINT(bugprone-easily-swappable-parameters)
{
	uint32_t index = FetchHandle(name);
	if (index == HashEntryNotFound) {
		return;
	}

	MpqHashEntry *hashEntry = &hashTable_[index];
	uint32_t block = hashEntry->block;
	MpqBlockEntry *blockEntry = &blockTable_[block];
	hashEntry->block = MpqHashEntry::DeletedBlock;
	AddFile(newName, blockEntry, block);
}

bool MpqWriter::HasFile(const char *name) const
{
	return FetchHandle(name) != HashEntryNotFound;
}

} // namespace devilution
