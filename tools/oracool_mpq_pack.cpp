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
 * Usage: oracool_mpq_pack <source_dir> <output.mpq> <relative_file>...
 */
#include <cstdio>
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

	// Always start from scratch. MpqWriter opens an existing archive for editing, so leaving a
	// stale file in place would silently keep assets that have since been removed from the source.
	devilution::RemoveFile(outPath.c_str());

	devilution::MpqWriter writer(outPath.c_str());
	size_t total = 0;
	int packed = 0;

	for (int i = 3; i < argc; i++) {
		std::string rel = argv[i];
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
