/**
 * @file tools/oracool_mpq_pack.cpp
 *
 * Packs files into oracool.mpq - Oracool Edition's own asset archive.
 *
 * Deliberately built on the engine's own MpqWriter rather than the external `smpq` tool that
 * devilutionx.mpq uses. The game already writes MPQs (save files are MPQ archives), so the code is
 * right there, and depending on it means the archive can be rebuilt on any machine that can build
 * the game - not true of the smpq path, which is absent here, and is why devilutionx's own assets
 * currently load loose from a directory instead of an archive.
 *
 * Takes an explicit file list rather than walking the directory itself, mirroring how smpq is
 * invoked for devilutionx.mpq and avoiding a directory-iteration dependency in the tool.
 *
 * Paths inside the archive are the relative paths given, with '/' rewritten to '\\' to match how
 * the engine asks for files: LoadCel("objects\\mcirl.cel"), LoadPNG("ui\\inventory_panel.png").
 *
 * A single argument of the form `@listfile` reads the relative paths from that file, one per line.
 * Added 2026-08-16 when the skill-sound library brought the asset count past 300 and the command
 * line the .cmd builds hit Windows' ~8191-character limit - which surfaced as "The input line is
 * too long" from cmd.exe, before the packer was reached at all.
 *
 * A `--verify` first argument checks an EXISTING archive against the same source tree and file list
 * instead of writing one, reading every entry out and comparing it with the file on disk. Release
 * packaging calls it, because a modification time proves when an archive was written and nothing at
 * all about what is in it (external audit of v1.9.97, finding 5).
 *
 * Usage: oracool_mpq_pack [--verify] <source_dir> <output.mpq> (<relative_file>... | @listfile)
 */
#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define ORACOOL_PACK_GETPID _getpid
#else
#include <unistd.h>
#define ORACOOL_PACK_GETPID getpid
#endif

#include <SDL.h>

#include "mpq/mpq_reader.hpp" // the validation pass reopens what was just written
#include "mpq/mpq_writer.hpp"
#include "utils/file_util.h"

namespace {

/**
 * @brief Whether every byte of @p name is plain ASCII.
 *
 * A NAME WITH A HIGH BYTE CRASHES libmpq. `libmpq__file_number` - which is all MpqArchive::HasFile
 * is - faults on any entry name containing a byte >= 0x80 rather than reporting "no such file"
 * (reproduced 2026-08-28 against a valid archive: an ASCII name reports missing and exits 1, the
 * same name with one high byte exits 0xC0000005). libmpq is a third-party dependency and not this
 * repository's to patch, so the guard lives at the caller that can see the name.
 *
 * The first thing to feed it such a name was a UTF-8 BOM: Windows PowerShell 5.1's
 * `Set-Content -Encoding utf8` writes one, so the release packager's generated file list began
 * "\xEF\xBB\xBF" + the first path, and archive verification crashed instead of failing. The reader
 * below strips the BOM, which fixes that case; this guard is what makes every OTHER route to a
 * non-ASCII name an error message rather than a fault.
 *
 * No asset in this project has a non-ASCII name, so refusing them costs nothing.
 */
bool IsPlainAscii(const std::string &name)
{
	for (const char ch : name) {
		if (static_cast<unsigned char>(ch) >= 0x80)
			return false;
	}
	return true;
}

/** @brief Reads a whole file, or nothing if it cannot be read in full. */
std::optional<std::vector<devilution::byte>> ReadWholeFile(const std::string &path)
{
	SDL_RWops *rw = SDL_RWFromFile(path.c_str(), "rb");
	if (rw == nullptr)
		return std::nullopt;
	const Sint64 size = SDL_RWsize(rw);
	if (size < 0) {
		SDL_RWclose(rw);
		return std::nullopt;
	}
	std::vector<devilution::byte> data(static_cast<size_t>(size));
	const size_t read = size == 0 ? 0 : SDL_RWread(rw, data.data(), 1, static_cast<size_t>(size));
	SDL_RWclose(rw);
	if (read != static_cast<size_t>(size))
		return std::nullopt;
	return data;
}

/**
 * @brief Reads every named entry out of @p archivePath and compares it byte for byte with its source.
 *
 * The old check called HasFile and stopped there, which proves the NAME TABLE and nothing else: an
 * archive whose entries are structurally listed and whose sector data is corrupt, truncated or
 * simply from a different build passed it (external audit of v1.9.97, finding 5). Reading the bytes
 * back is what makes "the archive is correct" a statement about contents.
 *
 * @param archiveNames the entry names, backslash-separated, as they were written.
 * @param relPaths the matching source-relative paths, forward-slash separated.
 */
bool VerifyArchiveContents(const std::string &archivePath, const std::string &sourceDir,
    const std::vector<std::string> &archiveNames, const std::vector<std::string> &relPaths)
{
	int32_t error = 0;
	std::optional<devilution::MpqArchive> archive = devilution::MpqArchive::Open(archivePath.c_str(), error);
	if (!archive) {
		std::fprintf(stderr, "ERROR: %s cannot be opened as an MPQ (%d)\n", archivePath.c_str(), error);
		return false;
	}

	for (size_t i = 0; i < archiveNames.size(); i++) {
		const std::string &name = archiveNames[i];
		// Before HasFile, never after - see IsPlainAscii. HasFile does not return false for these,
		// it faults.
		if (!IsPlainAscii(name)) {
			std::fprintf(stderr, "ERROR: entry name is not plain ASCII: %s\n", name.c_str());
			return false;
		}
		if (!archive->HasFile(name.c_str())) {
			std::fprintf(stderr, "ERROR: %s is missing from %s\n", name.c_str(), archivePath.c_str());
			return false;
		}
		std::size_t packedSize = 0;
		int32_t readError = 0;
		std::unique_ptr<devilution::byte[]> packed = archive->ReadFile(name.c_str(), packedSize, readError);
		if (packed == nullptr) {
			std::fprintf(stderr, "ERROR: %s cannot be read back out of %s (%d)\n", name.c_str(),
			    archivePath.c_str(), readError);
			return false;
		}
		const std::optional<std::vector<devilution::byte>> source = ReadWholeFile(sourceDir + "/" + relPaths[i]);
		if (!source) {
			std::fprintf(stderr, "ERROR: cannot read the source of %s to compare against\n", name.c_str());
			return false;
		}
		if (packedSize != source->size()) {
			std::fprintf(stderr, "ERROR: %s is %zu bytes in the archive and %zu on disk\n", name.c_str(),
			    packedSize, source->size());
			return false;
		}
		if (packedSize != 0 && std::memcmp(packed.get(), source->data(), packedSize) != 0) {
			std::fprintf(stderr, "ERROR: %s differs from its source\n", name.c_str());
			return false;
		}
	}
	return true;
}

} // namespace

int main(int argc, char **argv)
{
	// --verify checks an existing archive instead of writing one. Same arguments after it, so a
	// caller can verify exactly what it packed by repeating the command with the flag added.
	int arg = 1;
	bool verifyOnly = false;
	if (argc > 1 && std::strcmp(argv[1], "--verify") == 0) {
		verifyOnly = true;
		arg = 2;
	}

	if (argc < arg + 3) {
		std::fprintf(stderr, "Usage: %s [--verify] <source_dir> <output.mpq> <relative_file>...\n", argv[0]);
		return 1;
	}
	const std::string sourceDir = argv[arg];
	const std::string outPath = argv[arg + 1];
	const int firstFileArg = arg + 2;

	// The file list, from the command line or from a response file.
	std::vector<std::string> relPaths;
	if (argc == firstFileArg + 1 && argv[firstFileArg][0] == '@') {
		const char *listPath = argv[firstFileArg] + 1;
		std::ifstream list(listPath);
		if (!list) {
			std::fprintf(stderr, "ERROR: cannot read list file %s\n", listPath);
			return 1;
		}
		std::string line;
		bool firstLine = true;
		while (std::getline(list, line)) {
			// Tolerate CRLF and blank lines: the .cmd writes this file with `echo`, and a stray
			// carriage return would become part of the archive path and make the asset unfindable.
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
				line.pop_back();
			// And tolerate a UTF-8 BOM on the first line, for the same reason and from the same
			// class of writer: Windows PowerShell 5.1's `Set-Content -Encoding utf8` emits one, so
			// a generated list began "\xEF\xBB\xBF" + the first path. Without this the first entry
			// is a name no archive holds - and see IsPlainAscii for why that was a crash rather
			// than a message.
			if (firstLine && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF
			    && static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
				line.erase(0, 3);
			}
			firstLine = false;
			if (!line.empty())
				relPaths.push_back(line);
		}
		if (relPaths.empty()) {
			std::fprintf(stderr, "ERROR: list file %s named no files\n", listPath);
			return 1;
		}
	} else {
		for (int i = firstFileArg; i < argc; i++)
			relPaths.emplace_back(argv[i]);
	}

	// The archive-internal names: the same relative paths with '/' rewritten to '\\'.
	std::vector<std::string> allArchiveNames;
	allArchiveNames.reserve(relPaths.size());
	for (const std::string &rel : relPaths) {
		std::string name = rel;
		for (char &ch : name)
			if (ch == '/')
				ch = '\\';
		allArchiveNames.push_back(std::move(name));
	}

	if (verifyOnly) {
		if (!VerifyArchiveContents(outPath, sourceDir, allArchiveNames, relPaths))
			return 1;
		std::printf("verified %zu file(s) in %s\n", relPaths.size(), outPath.c_str());
		return 0;
	}

	// BUILD INTO A TEMPORARY, and do not touch the real archive until the temporary has been proved
	// complete (external audit of v1.9.92, finding 2).
	//
	// The old shape deleted the final archive FIRST and wrote straight to it, treating scope exit as
	// a transaction boundary. It is not one: ~MpqWriter calls Publish() when Finish() was not called
	// explicitly, so an early `return 1` partway through still unwound the stack, still published,
	// and left a structurally valid archive holding the first N entries - over the top of the
	// known-good one, which had already been deleted. The wrapper reported failure; the next
	// packaging run saw a fresh, plausible file and accepted it.
	//
	// The success path had the mirror problem: `return 0` was decided before the destructor ran, so a
	// failure inside the final table write or the atomic replace could not reach the exit code.
	//
	// So: a unique temporary, explicit Finish/Publish with their results checked, validation, and
	// only then the replace. Every failure below discards the shadow and leaves the previous archive
	// exactly as it was.
	// UNIQUE PER PROCESS, not a fixed suffix (external audit of v1.9.97, finding 4). It used to be
	// `outPath + ".building"`, which the comment above called a unique temporary and which is not
	// one: two packers aimed at the same archive shared that path, shared the MpqWriter shadow
	// derived from it, and shared cleanup that cannot tell whose file it is deleting. One could
	// delete the other's work in progress, validate against the other's archive, or publish a
	// generation built from the wrong response list. Ninja serialises one producer inside one build,
	// but a manual repack beside a running build is an ordinary thing to do.
	//
	// The process id is what makes it exclusive: two live processes cannot share one. The
	// performance counter distinguishes successive runs of the same pid after a crash left a shadow
	// behind, so a stale temporary is never mistaken for this run's.
	const std::string tempPath = outPath + "." + std::to_string(ORACOOL_PACK_GETPID()) + "."
	    + std::to_string(SDL_GetPerformanceCounter()) + ".building";
	devilution::RemoveFile(tempPath.c_str());

	devilution::MpqWriter writer(tempPath.c_str());
	size_t total = 0;
	int packed = 0;
	// Any early exit from here on must discard rather than let the destructor publish.
	const auto fail = [&writer, &tempPath](const char *what) {
		writer.DiscardShadow();
		devilution::RemoveFile(tempPath.c_str());
		std::fprintf(stderr, "ERROR: %s - the previous archive is untouched\n", what);
		return 1;
	};

	for (size_t i = 0; i < relPaths.size(); i++) {
		const std::string &rel = relPaths[i];
		// The relative path is both the read location under sourceDir and the name inside the
		// archive - it must actually BE relative and stay inside the source tree. A '..' segment
		// or an absolute path would read files from anywhere on disk into the archive (external
		// audit, 2026-08-17). The substring test over-rejects a filename that merely contains
		// "..", which no asset has and which is the right side to err on for a guard.
		if (rel.find("..") != std::string::npos || rel[0] == '/' || rel[0] == '\\'
		    || (rel.size() > 1 && rel[1] == ':')) {
			return fail(("refusing path outside the source dir: " + rel).c_str());
		}
		const std::string diskPath = sourceDir + "/" + rel;
		const std::string &name = allArchiveNames[i];

		const std::optional<std::vector<devilution::byte>> data = ReadWholeFile(diskPath);
		if (!data)
			return fail(("cannot read " + diskPath + ": " + SDL_GetError()).c_str());

		if (!writer.WriteFile(name.c_str(), data->data(), data->size()))
			return fail(("failed to write " + name + " into the archive").c_str());
		std::printf("  %-52s %8zu bytes\n", name.c_str(), data->size());
		total += data->size();
		packed++;
	}

	// EXPLICIT commit, with both results checked. Leaving this to ~MpqWriter is what made a
	// finalisation failure invisible to the exit code.
	if (!writer.Finish())
		return fail("failed to finalise the archive");
	if (!writer.Publish())
		return fail("failed to publish the archive");

	// VALIDATE the temporary before it is allowed to replace anything. A timestamp proves when a
	// file was written, not what is in it - and neither does a name table, which is all this used to
	// check (external audit of v1.9.97, finding 5). Every entry is now read back out and compared
	// with its source.
	if (!VerifyArchiveContents(tempPath, sourceDir, allArchiveNames, relPaths)) {
		devilution::RemoveFile(tempPath.c_str());
		std::fprintf(stderr, "ERROR: the archive just written does not match its sources\n");
		return 1;
	}

	// Only now does the real archive change. ReplaceFileAtomically is the same primitive MpqWriter
	// publishes its own shadow with.
	if (!devilution::ReplaceFileAtomically(tempPath.c_str(), outPath.c_str())) {
		devilution::RemoveFile(tempPath.c_str());
		std::fprintf(stderr, "ERROR: could not move the finished archive into place\n");
		return 1;
	}

	std::printf("packed %d file(s), %zu bytes -> %s\n", packed, total, outPath.c_str());
	return 0;
}
