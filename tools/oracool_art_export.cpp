/**
 * @file oracool_art_export.cpp
 *
 * Exports the original game art from diabdat.mpq / hellfire.mpq (and the Hellfire side archives)
 * as PNG, through the engine's own loaders - so every CEL and CL2 is decoded with the width the
 * game uses for it and drawn with the palette its level uses. Built as a CMake target beside
 * oracool_mpq_pack; it runs from the build directory, where the archives are.
 *
 * Usage:  oracool_art_export.exe <output-root> [category ...]
 *
 * Categories (default: all): ui pcx cutscenes items objects towners missiles monsters levels
 *
 * Output layout under <output-root> (the user's 00-original-game-art folder):
 *   <cel-name>/<cel-name>_frameNN.png    UI CELs, one PNG per frame (the layout already in use)
 *   ui_art/<name>/<name>_frameNN.png     the front-end PCX art, one PNG per frame
 *   gendata/<name>.png                   the loading screens, each with its own palette
 *   items/<drop>.png                     drop animations, frames left to right
 *   objects/<name>.png                   object animations, frames left to right
 *   towners/<name>.png                   towner animations, frames left to right
 *   missiles/<name>.png                  one row per direction, frames left to right
 *   monsters/<folder>/<name>_<anim>.png  one row per direction (n w a h d s), frames left to right
 *   levels/<type>/piece_NNNN.png         every dungeon piece of the tileset, assembled from its blocks
 *
 * Anything whose output already exists is skipped, so the tool can be re-run to fill gaps. Every
 * PNG is palettized (8-bit, the palette embedded, index 0 transparent) - bit-exact with the game.
 */

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <SDL.h>

#include "cursor.h"
#include "diablo.h"
#include "engine/assets.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/load_cl2.hpp"
#include "engine/load_file.hpp"
#include "engine/load_pcx.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/dun_render.hpp"
#include "engine/surface.hpp"
#include "init.h"
#include "items.h"
#include "levels/gendung.h"
#include "lighting.h"
#include "misdat.h"
#include "monstdat.h"
#include "monster.h"
#include "objdat.h"
#include "utils/paths.h"
#include "utils/png.h"
#include "utils/str_cat.hpp"

using namespace devilution;

namespace {

namespace fs = std::filesystem;

int Written = 0;
int Skipped = 0;
int Missing = 0;

bool WantCategory(const std::vector<std::string> &wanted, const char *category)
{
	if (wanted.empty())
		return true;
	for (const std::string &w : wanted)
		if (w == category)
			return true;
	return false;
}

bool AssetExists(const char *path)
{
	AssetHandle handle = OpenAsset(path);
	return handle.ok();
}

/** @brief Writes @p buf as a palettized PNG with the current logical palette, index 0 transparent. */
bool WritePng(const fs::path &path, const Surface &buf, const std::array<SDL_Color, 256> &palette)
{
	fs::create_directories(path.parent_path());
	SDLSurfaceUniquePtr surface { SDL_CreateRGBSurfaceWithFormat(0, buf.w(), buf.h(), 8, SDL_PIXELFORMAT_INDEX8) };
	if (surface == nullptr)
		return false;
	SDL_Color colors[256];
	for (int i = 0; i < 256; i++) {
		colors[i] = palette[i];
		colors[i].a = i == 0 ? SDL_ALPHA_TRANSPARENT : SDL_ALPHA_OPAQUE;
	}
	if (SDL_SetPaletteColors(surface->format->palette, colors, 0, 256) < 0)
		return false;
	const uint8_t *src = buf.begin();
	auto *dst = static_cast<uint8_t *>(surface->pixels);
	for (int y = 0; y < buf.h(); y++)
		std::memcpy(dst + static_cast<ptrdiff_t>(y) * surface->pitch, src + static_cast<ptrdiff_t>(y) * buf.pitch(), buf.w());
	if (IMG_SavePNG(surface.get(), path.string().c_str()) < 0) {
		std::fprintf(stderr, "  ! could not write %s: %s\n", path.string().c_str(), SDL_GetError());
		return false;
	}
	Written++;
	return true;
}

/** @brief One sprite per PNG. */
void WriteFrames(const fs::path &dir, const std::string &stem, ClxSpriteList list, const std::array<SDL_Color, 256> &palette)
{
	if (fs::exists(dir)) {
		Skipped++;
		return;
	}
	for (uint32_t i = 0; i < list.numSprites(); i++) {
		const ClxSprite sprite = list[i];
		OwnedSurface surf { static_cast<int>(sprite.width()), static_cast<int>(sprite.height()) };
		SDL_FillRect(surf.surface, nullptr, 0);
		ClxDraw(surf, { 0, static_cast<int>(sprite.height()) }, sprite);
		char name[64];
		std::snprintf(name, sizeof(name), "%s_frame%02u.png", stem.c_str(), static_cast<unsigned>(i));
		WritePng(dir / name, surf, palette);
	}
}

/** @brief One PNG: each list a row, its sprites left to right on a common cell size. */
void WriteSheet(const fs::path &file, const std::vector<ClxSpriteList> &rows, const std::array<SDL_Color, 256> &palette)
{
	if (fs::exists(file)) {
		Skipped++;
		return;
	}
	int cellW = 1;
	int cellH = 1;
	uint32_t cols = 1;
	for (const ClxSpriteList &row : rows) {
		cols = std::max<uint32_t>(cols, row.numSprites());
		for (uint32_t i = 0; i < row.numSprites(); i++) {
			cellW = std::max<int>(cellW, row[i].width());
			cellH = std::max<int>(cellH, row[i].height());
		}
	}
	OwnedSurface surf { static_cast<int>(cellW * cols), static_cast<int>(cellH * rows.size()) };
	SDL_FillRect(surf.surface, nullptr, 0);
	for (size_t r = 0; r < rows.size(); r++) {
		const ClxSpriteList &row = rows[r];
		for (uint32_t i = 0; i < row.numSprites(); i++) {
			const ClxSprite sprite = row[i];
			// Bottom-left anchored, like the game draws them: the feet sit on the cell's floor.
			ClxDraw(surf, { static_cast<int>(i * cellW), static_cast<int>((r + 1) * cellH) }, sprite);
		}
	}
	WritePng(file, surf, palette);
}

void WriteListOrSheet(const fs::path &file, ClxSpriteListOrSheet sprites, const std::array<SDL_Color, 256> &palette)
{
	std::vector<ClxSpriteList> rows;
	if (sprites.isSheet()) {
		const ClxSpriteSheet sheet = sprites.sheet();
		for (uint16_t i = 0; i < sheet.numLists(); i++)
			rows.push_back(sheet[i]);
	} else {
		rows.push_back(sprites.list());
	}
	WriteSheet(file, rows, palette);
}

bool UsePalette(const char *path)
{
	if (!AssetExists(path))
		return false;
	LoadPalette(path, /*blend=*/false);
	return true;
}

const char *PaletteForDungeonLevel(int level)
{
	if (level <= 0)
		return "levels\\towndata\\town.pal";
	if (level <= 4)
		return "levels\\l1data\\l1_1.pal";
	if (level <= 8)
		return "levels\\l2data\\l2_1.pal";
	if (level <= 12)
		return "levels\\l3data\\l3_1.pal";
	if (level <= 16)
		return "levels\\l4data\\l4_1.pal";
	if (level <= 20)
		return "nlevels\\l6data\\l6base.pal";
	return "nlevels\\l5data\\l5base.pal";
}

// ------------------------------------------------------------------ UI CELs

struct CelEntry {
	const char *path;   // without extension
	const char *folder; // output folder = the file's own name
	uint16_t width;
	const uint16_t *widths; // per-frame widths when not uniform
};

constexpr uint16_t CharButtonWidths[] = { 95, 41, 41, 41, 41, 41, 41, 41, 41 };

const CelEntry UiCels[] = {
	{ "ctrlpan\\panel8", "panel8", 640, nullptr },
	{ "ctrlpan\\talkpanl", "talkpanl", 640, nullptr },
	{ "ctrlpan\\golddrop", "golddrop", 261, nullptr },
	{ "ctrlpan\\spelicon", "spelicon", 56, nullptr },
	{ "ctrlpan\\p8bulbs", "p8bulbs", 88, nullptr },
	{ "ctrlpan\\p8but2", "p8but2", 33, nullptr },
	{ "ctrlpan\\panel8bu", "panel8bu", 71, nullptr },
	{ "ctrlpan\\talkbutt", "talkbutt", 61, nullptr },
	{ "ctrlpan\\smaltext", "smaltext", 13, nullptr },
	{ "data\\charbut", "charbut", 0, CharButtonWidths },
	{ "data\\quest", "quest", 320, nullptr },
	{ "data\\square", "square", 64, nullptr },
	{ "data\\pentspn2", "pentspn2", 12, nullptr },
	{ "data\\pentspin", "pentspin", 48, nullptr },
	{ "data\\option", "option", 27, nullptr },
	{ "data\\optbar", "optbar", 287, nullptr },
	{ "data\\textbox", "textbox", 591, nullptr },
	{ "data\\textslid", "textslid", 12, nullptr },
	{ "data\\textbox2", "textbox2", 271, nullptr },
	{ "data\\spelicon", "spelicon_hf", 56, nullptr },
	{ "data\\spelli2", "spelli2", 37, nullptr },
	{ "data\\medtexts", "medtexts", 22, nullptr },
	{ "data\\bigtgold", "bigtgold", 46, nullptr },
	{ "data\\hintbox", "hintbox", 288, nullptr },
	{ "data\\hintbox2", "hintbox2", 288, nullptr },
	{ "data\\hintbox3", "hintbox3", 288, nullptr },
	{ "data\\inv\\inv", "inv", 320, nullptr },
	{ "data\\inv\\inv_rog", "inv_rog", 320, nullptr },
	{ "data\\inv\\inv_sor", "inv_sor", 320, nullptr },
	{ "items\\duricons", "duricons", 32, nullptr },
	{ "items\\map\\mapztown", "mapztown", 640, nullptr },
	{ "items\\map\\mapz0000", "mapz0000", 640, nullptr },
	{ "levels\\towndata\\towns", "towns", 64, nullptr },
	{ "levels\\l1data\\l1s", "l1s", 64, nullptr },
	{ "levels\\l2data\\l2s", "l2s", 64, nullptr },
	{ "nlevels\\l5data\\l5s", "l5s", 64, nullptr },
};

void ExportUiCels(const fs::path &root)
{
	UsePalette("levels\\towndata\\town.pal");
	for (const CelEntry &entry : UiCels) {
		const std::string path = StrCat(entry.path, ".cel");
		if (!AssetExists(path.c_str())) {
			std::printf("absent  %s\n", entry.path);
			Missing++;
			continue;
		}
		const fs::path dir = root / entry.folder;
		if (fs::exists(dir)) {
			Skipped++;
			continue;
		}
		std::printf("ui      %s\n", entry.path);
		const OwnedClxSpriteListOrSheet los = entry.widths != nullptr
		    ? LoadCelListOrSheet(entry.path, PointerOrValue<uint16_t> { entry.widths })
		    : LoadCelListOrSheet(entry.path, PointerOrValue<uint16_t> { entry.width });
		const ClxSpriteListOrSheet view { los };
		if (view.isSheet())
			WriteListOrSheet(dir / StrCat(entry.folder, ".png"), view, orig_palette);
		else
			WriteFrames(dir, entry.folder, view.list(), orig_palette);
	}
	// The cow is a sheet (eight directions) - out with the towners.
}

/** @brief The item-cursor sheets, cut by the engine's own per-frame width tables. */
void ExportCursors(const fs::path &root)
{
	UsePalette("levels\towndata\town.pal");
	const fs::path dir1 = root / "objcurs";
	const fs::path dir2 = root / "objcurs2";
	if (fs::exists(dir1) && fs::exists(dir2)) {
		Skipped += 2;
		return;
	}
	std::puts("ui      objcurs (+ objcurs2)");
	InitCursor();
	const size_t first = GetNumInvItemsInSheet(1);
	const size_t second = GetNumInvItemsInSheet(2);
	auto writeRange = [&](const fs::path &dir, const char *stem, size_t from, size_t count) {
		if (fs::exists(dir)) {
			Skipped++;
			return;
		}
		for (size_t i = 0; i < count; i++) {
			const ClxSprite sprite = GetInvItemSprite(static_cast<int>(from + i + 1));
			OwnedSurface surf { static_cast<int>(sprite.width()), static_cast<int>(sprite.height()) };
			SDL_FillRect(surf.surface, nullptr, 0);
			ClxDraw(surf, { 0, static_cast<int>(sprite.height()) }, sprite);
			char name[64];
			std::snprintf(name, sizeof(name), "%s_frame%02u.png", stem, static_cast<unsigned>(i));
			WritePng(dir / name, surf, orig_palette);
		}
	};
	writeRange(dir1, "objcurs", 0, first);
	writeRange(dir2, "objcurs2", first, second);
	FreeCursor();
}

// ------------------------------------------------------------------ PCX

struct PcxEntry {
	const char *path;
	int frames; // 1 = a single image; N = a vertical strip of N frames
};

const PcxEntry Pcxs[] = {
	{ "ui_art\\but_sml", 15 }, { "ui_art\\heros", 1 }, { "ui_art\\logo", 15 }, { "ui_art\\smlogo", 15 },
	{ "ui_art\\r1_gry", 1 }, { "ui_art\\focus16", 8 }, { "ui_art\\focus", 8 }, { "ui_art\\focus42", 8 },
	{ "ui_art\\cursor", 1 }, { "ui_art\\lrpopup", 1 }, { "ui_art\\lpopup", 1 }, { "ui_art\\spopup", 1 },
	{ "ui_art\\prog_bg", 1 }, { "ui_art\\prog_fil", 1 }, { "ui_art\\sb_bg", 1 }, { "ui_art\\sb_thumb", 1 },
	{ "ui_art\\sb_arrow", 4 }, { "ui_art\\title", 1 }, { "ui_art\\mainmenu", 1 }, { "ui_art\\credits", 1 },
	{ "ui_art\\selhero", 1 }, { "ui_art\\selgame", 1 }, { "ui_art\\selconn", 1 }, { "ui_art\\selyesno", 1 },
	{ "ui_art\\black", 1 }, { "ui_art\\hf_logo1", 1 }, { "ui_art\\hf_logo2", 1 }, { "ui_art\\hf_logo3", 1 },
	{ "ui_art\\hf_titlew", 1 }, { "ui_art\\creditsw", 1 }, { "ui_art\\font16g", 1 }, { "ui_art\\font16s", 1 },
	{ "ui_art\\font24g", 1 }, { "ui_art\\font24s", 1 }, { "ui_art\\font30g", 1 }, { "ui_art\\font30s", 1 },
	{ "ui_art\\font42g", 1 }, { "ui_art\\font42y", 1 }, { "ui_art\\smbutton", 1 },
};

void ExportPcx(const fs::path &root)
{
	for (const PcxEntry &entry : Pcxs) {
		const std::string path = StrCat(entry.path, ".pcx");
		if (!AssetExists(path.c_str())) {
			std::printf("absent  %s\n", entry.path);
			Missing++;
			continue;
		}
		const char *slash = std::strrchr(entry.path, '\\');
		const std::string stem = slash != nullptr ? slash + 1 : entry.path;
		const bool cutscene = std::strncmp(entry.path, "gendata", 7) == 0;
		const fs::path target = cutscene ? root / "gendata" / (stem + ".png") : root / "ui_art" / stem;
		if (fs::exists(target)) {
			Skipped++;
			continue;
		}
		std::array<SDL_Color, 256> palette {};
		OptionalOwnedClxSpriteList list = LoadPcxSpriteList(entry.path, entry.frames, std::nullopt, palette.data(), /*logError=*/false);
		if (!list) {
			std::printf("absent  %s\n", entry.path);
			Missing++;
			continue;
		}
		std::printf("pcx     %s\n", entry.path);
		if (cutscene || entry.frames == 1) {
			const ClxSprite sprite = (*list)[0];
			OwnedSurface surf { static_cast<int>(sprite.width()), static_cast<int>(sprite.height()) };
			SDL_FillRect(surf.surface, nullptr, 0);
			ClxDraw(surf, { 0, static_cast<int>(sprite.height()) }, sprite);
			WritePng(cutscene ? target : target / (stem + ".png"), surf, palette);
		} else {
			WriteFrames(target, stem, ClxSpriteList { *list }, palette);
		}
	}
}

// ------------------------------------------------------------------ cutscenes

/** The loading screens: a 640-wide CEL and a palette of the same name (interfac.cpp). */
const char *const Cutscenes[] = {
	"cutstart", "cut2", "cut3", "cut4", "cutgate", "cutl1d", "cutportl", "cutportr", "cuttt", "cutl5", "cutl6",
};

void ExportCutscenes(const fs::path &root)
{
	for (const char *name : Cutscenes) {
		const std::string cel = StrCat("gendata\\", name, ".cel");
		const std::string pal = StrCat("gendata\\", name, ".pal");
		if (!AssetExists(cel.c_str()) || !AssetExists(pal.c_str())) {
			std::printf("absent  %s\n", cel.c_str());
			Missing++;
			continue;
		}
		const fs::path file = root / "gendata" / StrCat(name, ".png");
		if (fs::exists(file)) {
			Skipped++;
			continue;
		}
		std::printf("cut     %s\n", name);
		UsePalette(pal.c_str());
		const std::string path = StrCat("gendata\\", name);
		const OwnedClxSpriteListOrSheet los = LoadCelListOrSheet(path.c_str(), PointerOrValue<uint16_t> { static_cast<uint16_t>(640) });
		WriteListOrSheet(file, ClxSpriteListOrSheet { los }, orig_palette);
	}
}

// ------------------------------------------------------------------ items, objects, towners

void ExportItems(const fs::path &root)
{
	UsePalette("levels\\towndata\\town.pal");
	for (int i = 0; i < ITEMTYPES; i++) {
		const std::string path = StrCat("items\\", GetItemDropName(i));
		if (!AssetExists(StrCat(path, ".cel").c_str())) {
			std::printf("absent  %s\n", path.c_str());
			Missing++;
			continue;
		}
		const fs::path file = root / "items" / StrCat(GetItemDropName(i), ".png");
		if (fs::exists(file)) {
			Skipped++;
			continue;
		}
		std::printf("item    %s\n", path.c_str());
		const OwnedClxSpriteListOrSheet los = LoadCelListOrSheet(path.c_str(), PointerOrValue<uint16_t> { static_cast<uint16_t>(ItemAnimWidth) });
		WriteListOrSheet(file, ClxSpriteListOrSheet { los }, orig_palette);
	}
}

void ExportObjects(const fs::path &root)
{
	// Every object file the master list names, with the width its data row carries. Objects are
	// level-specific art: the palette of the object's first level.
	for (const ObjectData &data : AllObjects) {
		const char *file = ObjMasterLoadList[data.ofindex];
		const std::string path = StrCat("objects\\", file);
		if (!AssetExists(StrCat(path, ".cel").c_str())) {
			std::printf("absent  %s\n", path.c_str());
			Missing++;
			continue;
		}
		const fs::path out = root / "objects" / StrCat(file, ".png");
		if (fs::exists(out)) {
			Skipped++;
			continue;
		}
		std::printf("object  %s (%d)\n", path.c_str(), data.animWidth);
		UsePalette(PaletteForDungeonLevel(data.minlvl));
		const OwnedClxSpriteListOrSheet los = LoadCelListOrSheet(path.c_str(), PointerOrValue<uint16_t> { static_cast<uint16_t>(data.animWidth) });
		WriteListOrSheet(out, ClxSpriteListOrSheet { los }, orig_palette);
	}
}

struct TownerEntry {
	const char *path;
	uint16_t width;
	bool sheet;
};

const TownerEntry Towners[] = {
	{ "towners\\smith\\smithn", 96, false },
	{ "towners\\smith\\smithw", 96, false },
	{ "towners\\twnf\\twnfn", 96, false },
	{ "towners\\twnf\\twnfw", 96, false },
	{ "towners\\butch\\deadguy", 96, false },
	{ "towners\\townwmn1\\witch", 96, false },
	{ "towners\\townwmn1\\wmnn", 96, false },
	{ "towners\\townwmn1\\wmnw", 96, false },
	{ "towners\\townboy\\pegkid1", 96, false },
	{ "towners\\healer\\healer", 96, false },
	{ "towners\\strytell\\strytell", 96, false },
	{ "towners\\drunk\\twndrunk", 96, false },
	{ "towners\\priest\\priest8", 96, false },
	{ "towners\\farmer\\farmrn2", 96, false },
	{ "towners\\farmer\\cfrmrn2", 96, false },
	{ "towners\\farmer\\mfrmrn2", 96, false },
	{ "towners\\girl\\girlw1", 96, false },
	{ "towners\\girl\\girls1", 96, false },
	{ "towners\\animals\\cow", 128, true },
};

void ExportTowners(const fs::path &root)
{
	UsePalette("levels\\towndata\\town.pal");
	for (const TownerEntry &entry : Towners) {
		if (!AssetExists(StrCat(entry.path, ".cel").c_str())) {
			std::printf("absent  %s\n", entry.path);
			Missing++;
			continue;
		}
		const char *slash = std::strrchr(entry.path, '\\');
		const fs::path file = root / "towners" / StrCat(slash + 1, ".png");
		if (fs::exists(file)) {
			Skipped++;
			continue;
		}
		std::printf("towner  %s\n", entry.path);
		const OwnedClxSpriteListOrSheet los = LoadCelListOrSheet(entry.path, PointerOrValue<uint16_t> { entry.width });
		WriteListOrSheet(file, ClxSpriteListOrSheet { los }, orig_palette);
	}
}

// ------------------------------------------------------------------ missiles and monsters

void ExportMissiles(const fs::path &root)
{
	UsePalette("levels\\l1data\\l1_1.pal");
	for (size_t mi = 0; MissileSpriteData[mi].animFAmt != 0; mi++) {
		MissileFileData &data = MissileSpriteData[mi];
		if (data.name[0] == '\0')
			continue;
		const fs::path file = root / "missiles" / StrCat(data.name, ".png");
		if (fs::exists(file)) {
			Skipped++;
			continue;
		}
		// A one-direction missile is one CL2; a multi-direction one is <name>1..16.cl2.
		const std::string probe = data.animFAmt == 1 ? StrCat("missiles\\", data.name, ".cl2") : StrCat("missiles\\", data.name, "1.cl2");
		if (!AssetExists(probe.c_str())) {
			std::printf("absent  %s\n", probe.c_str());
			Missing++;
			continue;
		}
		std::printf("missile %s\n", data.name);
		data.LoadGFX();
		if (!data.sprites) {
			std::printf("absent  %s\n", data.name);
			Missing++;
			continue;
		}
		WriteListOrSheet(file, ClxSpriteListOrSheet { *data.sprites }, orig_palette);
		data.FreeGFX();
	}
}

void ExportOneMonster(const fs::path &root, int m)
{
	constexpr char Letters[] = "nwahds";
	{
		const MonsterData &data = MonstersData[m];
		if (data.assetsSuffix == nullptr)
			return;
		// The folder is the asset path's own folder, the file its own name.
		std::string suffix = data.assetsSuffix;
		for (char &c : suffix)
			if (c == '\\')
				c = '/';
		const fs::path base = root / "monsters" / suffix; // e.g. monsters/zombie/zombie
		bool anyMissing = false;
		bool allPresent = true;
		for (size_t a = 0; a < 6; a++) {
			if (data.frames[a] == 0)
				continue;
			const fs::path file = base.parent_path() / StrCat(base.filename().string(), "_", std::string(1, Letters[a]), ".png");
			if (!fs::exists(file))
				allPresent = false;
		}
		if (allPresent) {
			Skipped++;
			return;
		}
		// Every animation the row declares must be there: the loader has no missing-file path of
		// its own, and the dark mage (Hellfire's unused Malignus) ships a stand and little else.
		bool complete = true;
		for (size_t a = 0; a < 6; a++) {
			if (data.frames[a] == 0)
				continue;
			const std::string probe = StrCat("monsters\\", data.assetsSuffix, std::string(1, Letters[a]), ".cl2");
			if (!AssetExists(probe.c_str())) {
				std::printf("absent  %s\n", probe.c_str());
				complete = false;
			}
		}
		if (!complete) {
			Missing++;
			return;
		}
		std::printf("monster %s\n", data.assetsSuffix);
		UsePalette(PaletteForDungeonLevel(data.minDunLvl));
		CMonster type {};
		type.type = static_cast<_monster_id>(m);
		InitMonsterGFX(type);
		for (size_t a = 0; a < 6; a++) {
			const AnimStruct &anim = type.anims[a];
			if (anim.frames == 0 || !anim.sprites)
				continue;
			const fs::path file = base.parent_path() / StrCat(base.filename().string(), "_", std::string(1, Letters[a]), ".png");
			WriteListOrSheet(file, *anim.sprites, orig_palette);
		}
		(void)anyMissing;
		type.animData = nullptr;
	}
}

std::string SelfPath;

void ExportMonsters(const fs::path &root)
{
	for (int m = 0; m < NUM_MTYPES; m++) {
		_putenv_s("ORACOOL_EXPORT_ROOT", root.string().c_str());
		_putenv_s("ORACOOL_EXPORT_MONSTER", std::to_string(m).c_str());
		const std::string command = StrCat("\"", SelfPath, "\"");
		const int rc = std::system(command.c_str());
		if (rc != 0)
			std::printf("crashed monster #%d (%s) - skipped, exit %d\n", m, MonstersData[m].assetsSuffix != nullptr ? MonstersData[m].assetsSuffix : "?", rc);
	}
}

// ------------------------------------------------------------------ level tilesets

struct LevelEntry {
	const char *name;
	dungeon_type type;
	const char *cel;
	const char *min;
	const char *pal;
};

const LevelEntry Levels[] = {
	{ "town", DTYPE_TOWN, "levels\\towndata\\town.cel", "levels\\towndata\\town.min", "levels\\towndata\\town.pal" },
	{ "l1", DTYPE_CATHEDRAL, "levels\\l1data\\l1.cel", "levels\\l1data\\l1.min", "levels\\l1data\\l1_1.pal" },
	{ "l2", DTYPE_CATACOMBS, "levels\\l2data\\l2.cel", "levels\\l2data\\l2.min", "levels\\l2data\\l2_1.pal" },
	{ "l3", DTYPE_CAVES, "levels\\l3data\\l3.cel", "levels\\l3data\\l3.min", "levels\\l3data\\l3_1.pal" },
	{ "l4", DTYPE_HELL, "levels\\l4data\\l4.cel", "levels\\l4data\\l4.min", "levels\\l4data\\l4_1.pal" },
	{ "town_hellfire", DTYPE_TOWN, "nlevels\\towndata\\town.cel", "nlevels\\towndata\\town.min", "levels\\towndata\\town.pal" },
	{ "l5_crypt", DTYPE_CRYPT, "nlevels\\l5data\\l5.cel", "nlevels\\l5data\\l5.min", "nlevels\\l5data\\l5base.pal" },
	{ "l6_nest", DTYPE_NEST, "nlevels\\l6data\\l6.cel", "nlevels\\l6data\\l6.min", "nlevels\\l6data\\l6base.pal" },
};

void ExportLevels(const fs::path &root)
{
	for (const LevelEntry &entry : Levels) {
		if (!AssetExists(entry.cel) || !AssetExists(entry.min)) {
			std::printf("absent  %s\n", entry.cel);
			Missing++;
			continue;
		}
		const fs::path dir = root / "levels" / entry.name;
		if (fs::exists(dir)) {
			Skipped++;
			continue;
		}
		std::printf("level   %s\n", entry.name);
		leveltype = entry.type;
		UsePalette(entry.pal);
		MakeLightTable();
		pDungeonCels = LoadFileInMem(entry.cel);
		size_t count = 0;
		std::unique_ptr<uint16_t[]> min = LoadFileInMem<uint16_t>(entry.min, &count);
		SetDungeonMicros();
		const size_t blocksPerPiece = MicroTileLen;
		const size_t pieces = count / blocksPerPiece;
		const int rowsOfBlocks = static_cast<int>(blocksPerPiece / 2);
		for (size_t piece = 0; piece < pieces; piece++) {
			OwnedSurface surf { 64, 32 * rowsOfBlocks };
			SDL_FillRect(surf.surface, nullptr, 0);
			bool any = false;
			for (size_t b = 0; b < blocksPerPiece; b++) {
				const uint16_t raw = min[piece * blocksPerPiece + b];
				const LevelCelBlock block { raw };
				if (!block.hasValue())
					continue;
				any = true;
				// Blocks pair up left/right, from the piece's floor upward; RenderTile draws from
				// the block's bottom-left, like every other sprite.
				const int x = static_cast<int>((b % 2) * 32);
				const int y = 32 * rowsOfBlocks - static_cast<int>((b / 2) * 32);
				RenderTile(surf, { x, y }, block, MaskType::Solid, LightTables[0].data());
			}
			if (!any)
				continue;
			char name[32];
			std::snprintf(name, sizeof(name), "piece_%04u.png", static_cast<unsigned>(piece));
			WritePng(dir / name, surf, orig_palette);
		}
		pDungeonCels = nullptr;
	}
}

} // namespace

int main(int argc, char **argv)
{
	const char *childMonster = std::getenv("ORACOOL_EXPORT_MONSTER");
	const char *childRoot = std::getenv("ORACOOL_EXPORT_ROOT");
	if (argc < 2 && childMonster == nullptr) {
		std::fprintf(stderr, "usage: oracool_art_export <output-root> [ui pcx cutscenes items objects towners missiles monsters levels]\n");
		return 2;
	}
	const fs::path root = childMonster != nullptr ? fs::path(childRoot) : fs::path(argv[1]);
	SelfPath = argv[0];
	std::vector<std::string> wanted;
	for (int i = 2; i < argc; i++)
		wanted.emplace_back(argv[i]);
	const bool oneMonster = childMonster != nullptr;

	setvbuf(stdout, nullptr, _IONBF, 0);
	SDL_Init(0);
	InitPNG();
	HeadlessMode = false;
	LoadCoreArchives();
	LoadGameArchives();
	std::printf("archives mounted; hellfire=%d\n", gbIsHellfire ? 1 : 0);

	if (oneMonster) {
		ExportOneMonster(root, std::atoi(childMonster));
		return 0;
	}
	if (WantCategory(wanted, "ui"))
		ExportUiCels(root);
	if (WantCategory(wanted, "ui"))
		ExportCursors(root);
	if (WantCategory(wanted, "pcx"))
		ExportPcx(root);
	if (WantCategory(wanted, "cutscenes"))
		ExportCutscenes(root);
	if (WantCategory(wanted, "items"))
		ExportItems(root);
	if (WantCategory(wanted, "objects"))
		ExportObjects(root);
	if (WantCategory(wanted, "towners"))
		ExportTowners(root);
	if (WantCategory(wanted, "missiles"))
		ExportMissiles(root);
	if (WantCategory(wanted, "monsters"))
		ExportMonsters(root);
	if (WantCategory(wanted, "levels"))
		ExportLevels(root);

	std::printf("done: %d files written, %d already present, %d assets absent\n", Written, Skipped, Missing);
	return 0;
}
