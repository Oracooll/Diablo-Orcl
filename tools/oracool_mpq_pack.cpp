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
 * Usage: oracool_mpq_pack <source_dir> <output.mpq> (<relative_file>... | @listfile)
 */
#include <cstdio>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <SDL.h>

#include "mpq/mpq_reader.hpp" // the validation pass reopens what was just written
#include "mpq/mpq_writer.hpp"
#include "utils/file_util.h"

int main(int argc, char **argv)
{
	if (argc < 4) {
		std::fprintf(stderr, "Usage: %s <source_dir> <output.mpq> <relative_file>...\n", argv[0]);
		return 1;
	}
	const std::string sourceDir = argv[1];
	const std::string outPath = argv[2];

	// The file list, from the command line or from a response file.
	std::vector<std::string> relPaths;
	if (argc == 4 && argv[3][0] == '@') {
		const char *listPath = argv[3] + 1;
		std::ifstream list(listPath);
		if (!list) {
			std::fprintf(stderr, "ERROR: cannot read list file %s\n", listPath);
			return 1;
		}
		std::string line;
		while (std::getline(list, line)) {
			// Tolerate CRLF and blank lines: the .cmd writes this file with `echo`, and a stray
			// carriage return would become part of the archive path and make the asset unfindable.
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
				line.pop_back();
			if (!line.empty())
				relPaths.push_back(line);
		}
		if (relPaths.empty()) {
			std::fprintf(stderr, "ERROR: list file %s named no files\n", listPath);
			return 1;
		}
	} else {
		for (int i = 3; i < argc; i++)
			relPaths.emplace_back(argv[i]);
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
	const std::string tempPath = outPath + ".building";
	devilution::RemoveFile(tempPath.c_str());

	devilution::MpqWriter writer(tempPath.c_str());
	size_t total = 0;
	int packed = 0;
	std::vector<std::string> archiveNames;
	archiveNames.reserve(relPaths.size());

	// Any early exit from here on must discard rather than let the destructor publish.
	const auto fail = [&writer, &tempPath](const char *what) {
		writer.DiscardShadow();
		devilution::RemoveFile(tempPath.c_str());
		std::fprintf(stderr, "ERROR: %s - the previous archive is untouched\n", what);
		return 1;
	};

	for (std::string rel : relPaths) {
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
		for (char &ch : rel)
			if (ch == '/')
				ch = '\\';

		SDL_RWops *rw = SDL_RWFromFile(diskPath.c_str(), "rb");
		if (rw == nullptr)
			return fail(("cannot read " + diskPath + ": " + SDL_GetError()).c_str());
		const Sint64 size = SDL_RWsize(rw);
		if (size < 0) {
			SDL_RWclose(rw);
			return fail(("cannot size " + diskPath).c_str());
		}
		std::vector<devilution::byte> data(static_cast<size_t>(size));
		const size_t read = size == 0 ? 0 : SDL_RWread(rw, data.data(), 1, static_cast<size_t>(size));
		SDL_RWclose(rw);
		if (read != static_cast<size_t>(size))
			return fail(("short read on " + diskPath).c_str());

		if (!writer.WriteFile(rel.c_str(), data.data(), data.size()))
			return fail(("failed to write " + rel + " into the archive").c_str());
		std::printf("  %-52s %8zu bytes\n", rel.c_str(), data.size());
		total += data.size();
		packed++;
		// The archive-internal name, kept for the validation pass below.
		archiveNames.push_back(rel);
	}

	// EXPLICIT commit, with both results checked. Leaving this to ~MpqWriter is what made a
	// finalisation failure invisible to the exit code.
	if (!writer.Finish())
		return fail("failed to finalise the archive");
	if (!writer.Publish())
		return fail("failed to publish the archive");

	// VALIDATE the temporary before it is allowed to replace anything. A timestamp proves when a
	// file was written, not what is in it.
	{
		int32_t error = 0;
		std::optional<devilution::MpqArchive> check = devilution::MpqArchive::Open(tempPath.c_str(), error);
		if (!check) {
			devilution::RemoveFile(tempPath.c_str());
			std::fprintf(stderr, "ERROR: the archive just written cannot be opened (%d)\n", error);
			return 1;
		}
		for (const std::string &name : archiveNames) {
			if (!check->HasFile(name.c_str())) {
				devilution::RemoveFile(tempPath.c_str());
				std::fprintf(stderr, "ERROR: %s is missing from the archive that was just written\n", name.c_str());
				return 1;
			}
		}
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
