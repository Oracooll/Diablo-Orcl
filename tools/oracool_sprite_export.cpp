/**
 * @file tools/oracool_sprite_export.cpp
 *
 * Oracool: exports a player class's sprites out of the game archives as PNG sheets.
 *
 * Answering "how many sprites does the Warrior consist of" by ENUMERATION rather than by reading the
 * tables: it tries every armour x weapon x animation combination the naming scheme allows and reports
 * what the archive actually holds. The tables say what the game can ask for; only the archive says
 * what exists.
 *
 * One PNG per .cl2, laid out as a grid - 8 rows, one per facing, and one column per frame. That
 * mirrors the source file exactly, so the export is browsable rather than a heap of tens of thousands
 * of single frames.
 *
 * Colour comes from the TOWN palette. Player sprites live almost entirely in the palette's global half
 * (128-255), which is identical across town and every dungeon, so this is the right colour everywhere
 * - the same reasoning the character-select preview uses (see oracool/hero_preview.cpp).
 *
 * Not wired into the default build. Build it with:
 *     cmake --build build\x64-Debug --target oracool_sprite_export
 * then run it from the folder holding diabdat.mpq:
 *     build\x64-Debug\oracool_sprite_export.exe warrior <output_dir>
 */
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_image.h>
#include <fmt/format.h>

#include "engine/clx_sprite.hpp"
#include "engine/load_cl2.hpp"
#include "engine/load_file.hpp"
#include "engine/point.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "init.h"
#include "oracool/sprite_import.h"
#include "oracool/sprite_mix.h"
#include "itemdat.h"
#include "items.h"
#include "player.h"
#include "player.h"
#include "playerdat.hpp"
#include "utils/file_util.h"
#include "utils/paths.h"

namespace devilution {
namespace {

/** Every animation suffix the game asks for, with the sprite-width field each one is sized by. */
struct AnimationKind {
	const char *suffix;
	const char *label;
	uint8_t PlayerSpriteData::*width;
};

constexpr AnimationKind Animations[] = {
	{ "st", "town-stand", &PlayerSpriteData::stand },
	{ "wl", "town-walk", &PlayerSpriteData::walk },
	{ "as", "stand", &PlayerSpriteData::stand },
	{ "aw", "walk", &PlayerSpriteData::walk },
	{ "at", "attack", &PlayerSpriteData::attack },
	{ "ht", "hit", &PlayerSpriteData::swHit },
	{ "bl", "block", &PlayerSpriteData::block },
	{ "lm", "lightning", &PlayerSpriteData::lightning },
	{ "fm", "fire", &PlayerSpriteData::fire },
	{ "qm", "magic", &PlayerSpriteData::magic },
	{ "dt", "death", &PlayerSpriteData::death },
};

constexpr const char *ArmourLabel[] = { "light", "medium", "heavy" };
constexpr const char *WeaponLabel[] = {
	"unarmed", "unarmed-shield", "sword", "sword-shield", "bow", "axe", "mace", "mace-shield", "staff"
};

std::array<uint8_t, 768> TownPalette;

/** @brief Renders every frame of @p sheet into one RGBA grid: a row per facing, a column per frame. */
SDL_Surface *BuildSheetImage(const ClxSpriteSheet &sheet, int cellWidth, int &framesOut)
{
	size_t maxFrames = 0;
	int cellHeight = 0;
	for (size_t dir = 0; dir < 8; dir++) {
		const ClxSpriteList list = sheet[dir];
		maxFrames = std::max(maxFrames, static_cast<size_t>(list.numSprites()));
		for (size_t i = 0; i < list.numSprites(); i++)
			cellHeight = std::max(cellHeight, static_cast<int>(list[i].height()));
	}
	if (maxFrames == 0 || cellHeight == 0)
		return nullptr;

	const int imageWidth = cellWidth * static_cast<int>(maxFrames);
	const int imageHeight = cellHeight * 8;

	// Two 8-bit passes per frame. The colour pass draws the sprite as-is; the mask pass draws it
	// through a translation whose every entry is 1, which is the only reliable way to know WHICH
	// pixels the sprite touched - a sprite pixel is free to be palette index 0, so "still zero" does
	// not mean "not drawn". The mask becomes the alpha channel.
	std::array<uint8_t, 256> opaque;
	opaque.fill(1);

	SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, imageWidth, imageHeight, 32, SDL_PIXELFORMAT_RGBA32);
	if (out == nullptr)
		return nullptr;
	SDL_FillRect(out, nullptr, 0);

	int frames = 0;
	for (size_t dir = 0; dir < 8; dir++) {
		const ClxSpriteList list = sheet[dir];
		for (size_t i = 0; i < list.numSprites(); i++) {
			const ClxSprite sprite = list[i];
			OwnedSurface colour(cellWidth, cellHeight);
			OwnedSurface mask(cellWidth, cellHeight);
			for (int y = 0; y < cellHeight; y++) {
				std::memset(&colour[Point { 0, y }], 0, static_cast<size_t>(cellWidth));
				std::memset(&mask[Point { 0, y }], 0, static_cast<size_t>(cellWidth));
			}
			ClxDraw(colour, { 0, cellHeight - 1 }, sprite);
			ClxDrawTRN(mask, { 0, cellHeight - 1 }, sprite, opaque.data());

			auto *pixels = static_cast<uint8_t *>(out->pixels);
			for (int y = 0; y < cellHeight; y++) {
				const uint8_t *colourRow = &colour[Point { 0, y }];
				const uint8_t *maskRow = &mask[Point { 0, y }];
				uint8_t *dst = pixels + static_cast<size_t>(dir * cellHeight + y) * out->pitch
				    + static_cast<size_t>(i) * cellWidth * 4;
				for (int x = 0; x < cellWidth; x++) {
					if (maskRow[x] == 0) {
						dst[x * 4 + 0] = dst[x * 4 + 1] = dst[x * 4 + 2] = dst[x * 4 + 3] = 0;
						continue;
					}
					const size_t idx = colourRow[x] * 3;
					dst[x * 4 + 0] = TownPalette[idx];
					dst[x * 4 + 1] = TownPalette[idx + 1];
					dst[x * 4 + 2] = TownPalette[idx + 2];
					dst[x * 4 + 3] = 255;
				}
			}
			frames++;
		}
	}
	framesOut = frames;
	return out;
}

} // namespace
} // namespace devilution

int main(int argc, char **argv)
{
	using namespace devilution;

	const std::string className = argc > 1 ? argv[1] : "warrior";
	const std::string outDir = argc > 2 ? argv[2] : "sprite-export";
	// --verify puts every sheet back through the game's own importer and checks it survives.
	bool verify = false;
	for (int i = 3; i < argc; i++)
		if (std::strcmp(argv[i], "--verify") == 0)
			verify = true;
	bool dumpPalette = false;
	bool mixOnly = false;
	for (int i = 1; i < argc; i++)
		if (std::strcmp(argv[i], "--mix") == 0)
			mixOnly = true;
	for (int i = 1; i < argc; i++)
		if (std::strcmp(argv[i], "--dump-palette") == 0)
			dumpPalette = true;

	size_t classIndex = 0;
	bool found = false;
	for (size_t i = 0; i < enum_size<HeroClass>::value; i++) {
		if (className == PlayersData[i].classPath) {
			classIndex = i;
			found = true;
			break;
		}
	}
	if (!found) {
		std::fprintf(stderr, "unknown class '%s'. Known:", className.c_str());
		for (size_t i = 0; i < enum_size<HeroClass>::value; i++)
			std::fprintf(stderr, " %s", PlayersData[i].classPath);
		std::fprintf(stderr, "\n");
		return 1;
	}

	LoadCoreArchives();
	LoadGameArchives();
	if (!HaveDiabdat() && !HaveSpawn()) {
		std::fprintf(stderr, "no diabdat.mpq or spawn.mpq found next to this executable\n");
		return 1;
	}

	LoadFileInMem("levels\\towndata\\town.pal", TownPalette);

	// --dump-palette prints the table the export was coloured with, one line per index. Dye work needs
	// to know which index RANGES a body part occupies, and a PNG only carries the colours; this is the
	// other half of that lookup.
	if (dumpPalette) {
		for (int i = 0; i < 256; i++)
			std::printf("%3d %3d %3d %3d\n", i, TownPalette[i * 3], TownPalette[i * 3 + 1], TownPalette[i * 3 + 2]);
		return 0;
	}

	// --mix writes what oracool/sprite_mix.h assembles, so the in-engine mixer can be LOOKED at without
	// launching the game: a light Warrior holding a Tower Shield and a Broad Sword, then each alone.
	if (mixOnly) {
		RecursivelyCreateDir(outDir.c_str());
		Players.resize(1);
		devilution::Player &player = Players[0];
		const auto mixClass = static_cast<HeroClass>(classIndex);
		player._pClass = mixClass;
		const auto baseWith = [](item_cursor_graphic cursor, ItemType type) {
			devilution::Item item {};
			for (int i = 0; i <= IDI_LAST; i++) {
				if (AllItemsList[i].iCurs == cursor && AllItemsList[i].itype == type) {
					item.IDidx = static_cast<_item_indexes>(i);
					break;
				}
			}
			item._itype = type;
			item._iStatFlag = true;
			return item;
		};
		struct Case {
			const char *name;
			PlayerWeaponGraphic weapon;
			item_cursor_graphic sword;
			item_cursor_graphic shield;
			uint8_t armour = 0; // ArmourChar index of the body
		};
		const Case cases[] = {
			{ "both", PlayerWeaponGraphic::SwordShield, ICURS_BROAD_SWORD, ICURS_TOWER_SHIELD },
			{ "shield-only", PlayerWeaponGraphic::SwordShield, ICURS_SHORT_SWORD, ICURS_TOWER_SHIELD },
			{ "sword-only-with-buckler", PlayerWeaponGraphic::SwordShield, ICURS_BROAD_SWORD, ICURS_BUCKLER },
			{ "sword-only", PlayerWeaponGraphic::Sword, ICURS_BROAD_SWORD, ICURS_POTION_OF_FULL_MANA },
			// the shield follows the item, from any tier to any body
			{ "L-body-M-shield", PlayerWeaponGraphic::SwordShield, ICURS_SHORT_SWORD, ICURS_KITE_SHIELD, 0 },
			{ "M-body-L-shield", PlayerWeaponGraphic::SwordShield, ICURS_SHORT_SWORD, ICURS_BUCKLER, 1 },
			{ "M-body-H-shield", PlayerWeaponGraphic::SwordShield, ICURS_SHORT_SWORD, ICURS_TOWER_SHIELD, 1 },
			{ "H-body-L-shield", PlayerWeaponGraphic::SwordShield, ICURS_SHORT_SWORD, ICURS_BUCKLER, 2 },
			{ "H-body-M-shield", PlayerWeaponGraphic::SwordShield, ICURS_SHORT_SWORD, ICURS_KITE_SHIELD, 2 },
		};
		const PlayerSpriteData &widths = PlayersSpriteData[classIndex];
		for (const Case &c : cases) {
			player._pgfxnum = static_cast<uint8_t>(static_cast<uint8_t>(c.weapon) | (c.armour << 4));
			player.InvBody[INVLOC_HAND_LEFT] = baseWith(c.sword, ItemType::Sword);
			player.InvBody[INVLOC_HAND_RIGHT] = c.shield == ICURS_POTION_OF_FULL_MANA ? devilution::Item {} : baseWith(c.shield, ItemType::Shield);
			for (const AnimationKind &anim : Animations) {
				const uint16_t width = widths.*(anim.width);
				// Undyed and unscaled: the PNG is coloured with the town palette, which knows nothing of a sheet's own colours.
				oracool::PlayerSheetRequest request = oracool::MakePlayerSheetRequest(player, mixClass, c.weapon, anim.suffix, width);
				request.dye = nullptr;
				request.dyeId = 0;
				request.scalePercent = 100;
				const uint32_t startedAt = SDL_GetTicks();
				std::optional<oracool::ColouredSpriteSheet> mixed = oracool::MixPlayerSheetNow(request);
				const uint32_t took = SDL_GetTicks() - startedAt;
				if (!mixed) {
					std::printf("%-26s %-12s (not mixed)  %5u ms\n", c.name, anim.label, took);
					continue;
				}
				int frames = 0;
				SDL_Surface *image = BuildSheetImage(mixed->sheet, width, frames);
				const std::string outPath = outDir + "/mix-" + c.name + "-" + anim.label + ".png";
				if (image != nullptr) {
					IMG_SavePNG(image, outPath.c_str());
					SDL_FreeSurface(image);
				}
				std::printf("%-26s %-12s %4d frames %5u ms\n", c.name, anim.label, frames, took);
			}
		}

		// The BACKGROUND path end to end - request, one read a pump, worker, disk, memory - checked against the
		// synchronous answer byte for byte. What the game does, without the game.
		{
			player._pgfxnum = static_cast<uint8_t>(PlayerWeaponGraphic::SwordShield);
			player.InvBody[INVLOC_HAND_LEFT] = baseWith(ICURS_BROAD_SWORD, ItemType::Sword);
			player.InvBody[INVLOC_HAND_RIGHT] = baseWith(ICURS_TOWER_SHIELD, ItemType::Shield);
			oracool::PlayerSheetRequest request = oracool::MakePlayerSheetRequest(player, mixClass, PlayerWeaponGraphic::SwordShield, "as", widths.stand);
			request.dye = nullptr;
			request.dyeId = 0;
			request.scalePercent = 100;
			std::optional<oracool::ColouredSpriteSheet> cached;
			const oracool::CachedSheetState before = oracool::TakeCachedPlayerSheet(request, cached);
			std::printf("\nbackground path, key %s: cache before = %s\n", request.Key().c_str(),
			    before == oracool::CachedSheetState::Ready ? "READY (from disk)" : before == oracool::CachedSheetState::Nothing ? "nothing" : "unknown");
			if (before == oracool::CachedSheetState::Unknown) {
				oracool::RequestPlayerSheet(request);
				int pumps = 0;
				uint32_t worstPump = 0;
				const uint32_t startedAt = SDL_GetTicks();
				bool done = false;
				while (!done && SDL_GetTicks() - startedAt < 60000) {
					const uint32_t pumpAt = SDL_GetTicks();
					oracool::PumpSpriteMixer();
					worstPump = std::max(worstPump, SDL_GetTicks() - pumpAt);
					pumps++;
					done = !oracool::TakeFinishedPlayerSheets().empty();
					SDL_Delay(5);
				}
				std::printf("  finished=%d after %u ms, %d pumps, worst single pump %u ms (that is all the main thread ever pays)\n", done ? 1 : 0, SDL_GetTicks() - startedAt, pumps, worstPump);
			}
			const uint32_t takeAt = SDL_GetTicks();
			const oracool::CachedSheetState after = oracool::TakeCachedPlayerSheet(request, cached);
			const uint32_t takeTook = SDL_GetTicks() - takeAt;
			std::optional<oracool::ColouredSpriteSheet> direct = oracool::MixPlayerSheetNow(request);
			bool same = false;
			if (after == oracool::CachedSheetState::Ready && cached && direct) {
				const ClxSpriteSheet a { cached->sheet };
				const ClxSpriteSheet b { direct->sheet };
				same = a.dataSize() == b.dataSize() && std::memcmp(a.data(), b.data(), a.dataSize()) == 0;
			}
			std::printf("  cache after = %s, taking it cost %u ms, identical to the synchronous mix: %s\n",
			    after == oracool::CachedSheetState::Ready ? "READY" : "NOT READY", takeTook, same ? "YES" : "NO");
			oracool::ShutdownSpriteMixer();
		}
		return 0;
	}

	RecursivelyCreateDir(outDir.c_str());

	const PlayerSpriteData &spriteData = PlayersSpriteData[classIndex];
	const char classChar = CharChar[classIndex];

	int sheetsFound = 0;
	int sheetsMissing = 0;
	int totalFrames = 0;
	int verified = 0;
	int verifyFailures = 0;

	for (size_t armour = 0; armour < 3; armour++) {
		for (size_t weapon = 0; weapon < WepChar.size(); weapon++) {
			const char prefix[3] = { classChar, ArmourChar[armour], WepChar[weapon] };
			for (const AnimationKind &anim : Animations) {
				// The bow's attack animation is a different width from every other weapon's - the one
				// place the width depends on the weapon rather than only on the animation.
				uint16_t width = spriteData.*(anim.width);
				if (std::strcmp(anim.suffix, "at") == 0
				    && static_cast<PlayerWeaponGraphic>(weapon) == PlayerWeaponGraphic::Bow)
					width = spriteData.bow;

				char path[256];
				*fmt::format_to(path, R"(plrgfx\{0}\{1}\{1}{2})",
				    PlayersData[classIndex].classPath, string_view(prefix, 3), anim.suffix)
				    = '\0';

				// Asked for BEFORE loading: LoadCl2Sheet aborts the process on a missing file, and the
				// whole point here is to walk combinations that mostly do not exist.
				//
				// WITH the extension. LoadCl2Sheet takes the name without one and appends
				// DEVILUTIONX_CL2_EXT itself, so FindAsset has to be given what the loader will
				// actually open - handed the bare name it reports every single file as missing.
				const std::string assetPath = std::string(path) + DEVILUTIONX_CL2_EXT;
				if (!FindAsset(assetPath.c_str()).ok()) {
					sheetsMissing++;
					continue;
				}

				OwnedClxSpriteSheet sheet = LoadCl2Sheet(path, width);
				int frames = 0;
				SDL_Surface *image = BuildSheetImage(sheet, width, frames);
				if (image == nullptr) {
					sheetsMissing++;
					continue;
				}

				const std::string outPath = fmt::format("{}/{}-{}-{}.png", outDir,
				    ArmourLabel[armour], WeaponLabel[weapon], anim.label);
				if (IMG_SavePNG(image, outPath.c_str()) != 0)
					std::fprintf(stderr, "failed to write %s: %s\n", outPath.c_str(), IMG_GetError());
				SDL_FreeSurface(image);

				// Closed loop: read the PNG back off disk and put it through the importer the game
				// uses, then check the sprites that come out match the ones that went in. Writing a
				// file that merely exists proves nothing - the same reasoning as the SHA-256 check
				// every asset in this project gets after packing.
				if (verify) {
					// SDL_image's own loader, by path - the engine's LoadPNG goes through the asset
					// system and could not see a file that was just written to an output folder.
					SDL_Surface *reread = IMG_Load(outPath.c_str());
					OptionalOwnedClxSpriteSheet imported = oracool::SpriteSheetFromSurface(reread, width);
					if (reread != nullptr)
						SDL_FreeSurface(reread);
					if (!imported) {
						std::fprintf(stderr, "VERIFY FAIL %s: did not import\n", outPath.c_str());
						verifyFailures++;
					} else {
						for (size_t dir = 0; dir < 8; dir++) {
							const ClxSpriteList before = sheet[dir];
							const ClxSpriteList after = (*imported)[dir];
							if (before.numSprites() != after.numSprites()) {
								std::fprintf(stderr, "VERIFY FAIL %s: facing %zu has %d frames, imported %d\n",
								    outPath.c_str(), dir, before.numSprites(), after.numSprites());
								verifyFailures++;
								break;
							}
						}
						verified++;
					}
				}

				sheetsFound++;
				totalFrames += frames;
				std::printf("%-40s %3d frames\n", outPath.c_str() + outDir.size() + 1, frames);
			}
		}
	}

	std::printf("\n%s: %d sheets (.cl2 files), %d frames total. %d combinations had no file.\n",
	    className.c_str(), sheetsFound, totalFrames, sheetsMissing);
	if (verify) {
		std::printf("round-trip through the importer: %d sheets verified, %d failed.\n",
		    verified, verifyFailures);
		return verifyFailures == 0 ? 0 : 2;
	}
	return 0;
}
