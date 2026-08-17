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
#include <string>
#include <vector>

#include <SDL.h>

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

	// Always start from scratch. MpqWriter opens an existing archive for editing, so leaving a
	// stale file in place would silently keep assets that have since been removed from the source.
	devilution::RemoveFile(outPath.c_str());

	devilution::MpqWriter writer(outPath.c_str());
	size_t total = 0;
	int packed = 0;

	for (std::string rel : relPaths) {
		// The relative path is both the read location under sourceDir and the name inside the
		// archive - it must actually BE relative and stay inside the source tree. A '..' segment
		// or an absolute path would read files from anywhere on disk into the archive (external
		// audit, 2026-08-17). The substring test over-rejects a filename that merely contains
		// "..", which no asset has and which is the right side to err on for a guard.
		if (rel.find("..") != std::string::npos || rel[0] == '/' || rel[0] == '\\'
		    || (rel.size() > 1 && rel[1] == ':')) {
			std::fprintf(stderr, "ERROR: refusing path outside the source dir: %s\n", rel.c_str());
			return 1;
		}
		const std::string diskPath = sourceDir + "/" + rel;
		for (char &ch : rel)
			if (ch == '/')
				ch = '\\';

		SDL_RWops *rw = SDL_RWFromFile(diskPath.c_str(), "rb");
		if (rw == nullptr) {
			std::fprintf(stderr, "ERROR: cannot read %s: %s\n", diskPath.c_str(), SDL_GetError());
			return 1;
		}
		const Sint64 size = SDL_RWsize(rw);
		if (size < 0) {
			std::fprintf(stderr, "ERROR: cannot size %s\n", diskPath.c_str());
			SDL_RWclose(rw);
			return 1;
		}
		std::vector<devilution::byte> data(static_cast<size_t>(size));
		const size_t read = size == 0 ? 0 : SDL_RWread(rw, data.data(), 1, static_cast<size_t>(size));
		SDL_RWclose(rw);
		if (read != static_cast<size_t>(size)) {
			std::fprintf(stderr, "ERROR: short read on %s\n", diskPath.c_str());
			return 1;
		}

		if (!writer.WriteFile(rel.c_str(), data.data(), data.size())) {
			std::fprintf(stderr, "ERROR: failed to write %s into the archive\n", rel.c_str());
			return 1;
		}
		std::printf("  %-52s %8zu bytes\n", rel.c_str(), data.size());
		total += data.size();
		packed++;
	}

	std::printf("packed %d file(s), %zu bytes -> %s\n", packed, total, outPath.c_str());
	return 0;
}
