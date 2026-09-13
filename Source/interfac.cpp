/**
 * @file interfac.cpp
 *
 * Implementation of load screens.
 */

#include <algorithm> // std::clamp - the loading bar's gradient
#include <cstdint>

#include <SDL.h>

#include "control.h"
#include "engine.h"
#include "engine/clx_sprite.hpp"
#include "engine/demomode.h"
#include "engine/dx.h"
#include "engine/events.hpp"
#include "engine/load_cel.hpp"
#include "engine/load_clx.hpp"
#include "engine/load_pcx.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "hwcursor.hpp"
#include "init.h"
#include "loadsave.h"
#include "oracool/auto_save.h"
#include "oracool/oracool.h"
#include "pfile.h"
#include "plrmsg.h"
#include "utils/png.h"
#include "utils/sdl_geometry.h"
#include "utils/sdl_wrap.h"
#include "utils/str_cat.hpp"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

namespace {

constexpr uint32_t MaxProgress = 534;

OptionalOwnedClxSpriteList sgpBackCel;

bool IsProgress;
uint32_t sgdwProgress;
int progress_id;

/** The color used for the progress bar as an index into the palette (the indexed fallback only, since 2026-09-08). */
const uint8_t BarColor[3] = { 138, 43, 254 };
/** The bar's colour on the 32-bit screen: RGB204.183.117, the legend's gold (user, 2026-09-08). One colour for every screen. */
constexpr uint32_t BarColorRgb = 0xCCB775;

/**
 * @brief The loading bar's gradient: the item-quality ladder, weakest to strongest (user, 2026-09-13:
 * "make the loading bar gradient between basic,magic,rare,unique, set, primal items colours").
 *
 * It was dark red to bright green (2026-09-12). Now six evenly spaced stops, in the user's order, each
 * the colour that tier's names are written in - so a load reads as loot climbing the ladder.
 *
 * Rare, set and primal are the exact top shades of their font bands (text_render.cpp: YL-3, the set
 * green, BE-2). Basic white, magic blue and unique gold are drawn from vanilla's .trn files, which carry
 * no RGB, so their stops are the nearest values to how those names read on screen.
 *
 * Permille rather than percent, so a stop can sit off the tens without rounding if the spacing is ever
 * tuned.
 */
struct BarGradientStop {
	int atPermille;
	uint8_t r, g, b;
};
constexpr BarGradientStop BarGradient[] = {
	{ 0, 243, 243, 243 },    // basic: white
	{ 200, 120, 120, 255 },  // magic: blue
	{ 400, 254, 251, 36 },   // rare: YL-3, 0xFEFB24
	{ 600, 221, 196, 126 },  // unique: gold
	{ 800, 140, 190, 140 },  // set: green, 0x8CBE8C
	{ 1000, 232, 202, 202 }, // primal: BE-2, 0xE8CACA
};

/** @brief The gradient's colour at @p permille along the bar's FULL track, as 0x00RRGGBB. */
uint32_t BarGradientColorAt(int permille)
{
	constexpr size_t StopCount = sizeof(BarGradient) / sizeof(BarGradient[0]);
	permille = std::clamp(permille, 0, BarGradient[StopCount - 1].atPermille);
	size_t i = 1;
	while (i + 1 < StopCount && permille > BarGradient[i].atPermille)
		i++;
	const BarGradientStop &lo = BarGradient[i - 1];
	const BarGradientStop &hi = BarGradient[i];
	const int span = hi.atPermille - lo.atPermille;
	// 0..256 rather than 0..100, so the step between adjacent columns stays smooth on a wide screen.
	const int t = span > 0 ? (permille - lo.atPermille) * 256 / span : 256;
	const uint32_t r = static_cast<uint32_t>(lo.r + (hi.r - lo.r) * t / 256);
	const uint32_t g = static_cast<uint32_t>(lo.g + (hi.g - lo.g) * t / 256);
	const uint32_t b = static_cast<uint32_t>(lo.b + (hi.b - lo.b) * t / 256);
	return (r << 16) | (g << 8) | b;
}
// BarPos - the per-screen top-left corner of the bar, authored at 640x480 as { 53, 37 }, { 53, 421 },
// { 53, 37 } - is GONE (2026-09-12). The bar spans the whole screen along its floor now, so there is
// no per-screen position left to hold. progress_id survives it: BarColor is still indexed by it on
// the 8-bit path.

OptionalOwnedClxSpriteList ArtCutsceneWidescreen;

/**
 * Oracool (user, 2026-09-07: "make loading screen use these images, fit to height, respecting aspect
 * ratio"): the cutscene painting drawn in TRUE COLOUR straight onto the 32-bit screen (v1.11), scaled
 * to the screen's height with the aspect kept, black bars either side. The first art in the game
 * that never passes through a palette lookup at draw time.
 *
 * Built at load from the ORIGINAL CEL and its palette in the player's own diabdat.mpq - never from
 * a copy this project ships. The paintings are Blizzard's; a PNG of them in oracool.mpq would have
 * been a redistribution (user, 2026-09-07: "are we allowed to pack them in my mpq file [...] they
 * are intelectual property of blizzard"). The same pixels reach the screen either way.
 */
SDLSurfaceUniquePtr CutsceneRgb;
/** @brief Where the scaled painting sits on the screen. */
SDL_Rect CutsceneRgbRect { 0, 0, 0, 0 };
// CutsceneRgbSourceWidth/Height were here, and only the progress bar ever read them - to find the
// painting's centred 4:3 core and place itself inside it. The bar spans the screen now, so they went
// with BarPos (2026-09-12).

/**
 * @brief The user's own 16:9 redo of a painting. `gendata\<name>.png`, 1280x720; when one is absent
 * the CEL path below takes over, which is what the Hive and the Crypt still do.
 *
 * These SHIP, in the public `oracool.mpq` (2026-09-12). The comment here used to say they were kept
 * in a private archive "with other blizzard IP" per the 2026-09-07 conversation; two things have
 * since changed and the note was stale on both counts. The private archive was dissolved, and the
 * user settled the question directly (2026-09-12): *"This is a non-profit add-on to diablo so using
 * blizzard IP is considered tolerable by them and among the modding community."*
 *
 * They are also not Blizzard's pixels. They are original AI-generated paintings of the same scenes,
 * checksum-verified as differing from the extracted originals in `00-original-game-art/gendata/` -
 * which is what made publishing them safe to begin with, independently of the tolerance above.
 *
 * The distinction that still holds is a different one, and it is about the commercial GAME rather
 * than about IP: a release never packs `diabdat.mpq`, `hellfire.mpq` or the five `hf*.mpq` archives,
 * and `tools\BuildReleasePackage.ps1` sweeps the staged folder for all seven. The player supplies
 * those because they own the game, not because of a licensing worry about art.
 *
 * The painting's 4:3 core is assumed centred, which is where the progress bar used to sit.
 */
bool LoadCutscenePng(const char *celPath)
{
	const std::string pngPath = StrCat(celPath, ".png");
	SDL_Surface *png = LoadPNG(pngPath.c_str());
	if (png == nullptr)
		return false;
	CutsceneRgb = SDLSurfaceUniquePtr { SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_RGB888, 0) };
	SDL_FreeSurface(png);
	if (CutsceneRgb == nullptr)
		return false;
	return true;
}

/** @brief Decodes the loaded CEL through the loaded palette into an XRGB8888 surface. Call after LoadPalette. */
void BuildCutsceneRgb(const char *celPath)
{
	CutsceneRgb = nullptr;
	if (LoadCutscenePng(celPath))
		return;
	if (!sgpBackCel)
		return;
	const ClxSprite sprite = (*sgpBackCel)[0];
	const int width = sprite.width();
	const int height = sprite.height();
	OwnedSurface indexed(width, height);
	SDL_FillRect(indexed.surface, nullptr, 0);
	ClxDraw(indexed, { 0, height - 1 }, sprite);
	SDLSurfaceUniquePtr rgb = SDLWrap::CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGB888);
	for (int y = 0; y < height; y++) {
		const uint8_t *src = indexed.at<uint8_t>(0, y);
		auto *dst = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(rgb->pixels) + static_cast<ptrdiff_t>(y) * rgb->pitch);
		for (int x = 0; x < width; x++) {
			const SDL_Color &c = orig_palette[src[x]];
			dst[x] = (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
		}
	}
	CutsceneRgb = std::move(rgb);
}

/** @brief Fit to height, aspect kept, centred: the rect the painting scales into. */
SDL_Rect FitToHeight(int srcWidth, int srcHeight, int screenWidth, int screenHeight)
{
	const int height = screenHeight;
	const int width = srcWidth * screenHeight / srcHeight;
	return MakeSdlRect((screenWidth - width) / 2, 0, width, height);
}

uint32_t CustomEventsBegin = SDL_USEREVENT;
constexpr uint32_t NumCustomEvents = WM_LAST - WM_FIRST + 1;

Cutscenes GetCutSceneFromLevelType(dungeon_type type)
{
	switch (type) {
	case DTYPE_TOWN:
		return CutTown;
	case DTYPE_CATHEDRAL:
		return CutLevel1;
	case DTYPE_CATACOMBS:
		return CutLevel2;
	case DTYPE_CAVES:
		return CutLevel3;
	case DTYPE_HELL:
		return CutLevel4;
	case DTYPE_NEST:
		return CutLevel6;
	case DTYPE_CRYPT:
		return CutLevel5;
	default:
		return CutLevel1;
	}
}

Cutscenes PickCutscene(interface_mode uMsg)
{
	switch (uMsg) {
	case WM_DIABLOADGAME:
	case WM_DIABNEWGAME:
		return CutStart;
	case WM_DIABRETOWN:
		return CutTown;
	case WM_DIABNEXTLVL:
	case WM_DIABPREVLVL:
	case WM_DIABTOWNWARP:
	case WM_DIABTWARPUP: {
		int lvl = MyPlayer->plrlevel;
		if (lvl == 1 && uMsg == WM_DIABNEXTLVL)
			return CutTown;
		if (lvl == 16 && uMsg == WM_DIABNEXTLVL)
			return CutGate;
		return GetCutSceneFromLevelType(GetLevelType(lvl));
	}
	case WM_DIABWARPLVL:
		return CutPortal;
	case WM_DIABSETLVL:
	case WM_DIABRTNLVL:
		if (setlvlnum == SL_BONECHAMB)
			return CutLevel2;
		if (setlvlnum == SL_VILEBETRAYER)
			return CutPortalRed;
		if (IsArenaLevel(setlvlnum)) {
			if (uMsg == WM_DIABSETLVL)
				return GetCutSceneFromLevelType(setlvltype);
			return CutTown;
		}
		return CutLevel1;
	default:
		app_fatal("Unknown progress mode");
	}
}

void LoadCutsceneBackground(interface_mode uMsg)
{
	const char *celPath;
	const char *palPath;

	switch (PickCutscene(uMsg)) {
	case CutStart:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutstartw.clx");
		celPath = "gendata\\cutstart";
		palPath = "gendata\\cutstart.pal";
		progress_id = 1;
		break;
	case CutTown:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutttw.clx");
		celPath = "gendata\\cuttt";
		palPath = "gendata\\cuttt.pal";
		progress_id = 1;
		break;
	case CutLevel1:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutl1dw.clx");
		celPath = "gendata\\cutl1d";
		palPath = "gendata\\cutl1d.pal";
		progress_id = 0;
		break;
	case CutLevel2:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cut2w.clx");
		celPath = "gendata\\cut2";
		palPath = "gendata\\cut2.pal";
		progress_id = 2;
		break;
	case CutLevel3:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cut3w.clx");
		celPath = "gendata\\cut3";
		palPath = "gendata\\cut3.pal";
		progress_id = 1;
		break;
	case CutLevel4:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cut4w.clx");
		celPath = "gendata\\cut4";
		palPath = "gendata\\cut4.pal";
		progress_id = 1;
		break;
	case CutLevel5:
		ArtCutsceneWidescreen = LoadOptionalClx("nlevels\\cutl5w.clx");
		celPath = "nlevels\\cutl5";
		palPath = "nlevels\\cutl5.pal";
		progress_id = 1;
		break;
	case CutLevel6:
		ArtCutsceneWidescreen = LoadOptionalClx("nlevels\\cutl6w.clx");
		celPath = "nlevels\\cutl6";
		palPath = "nlevels\\cutl6.pal";
		progress_id = 1;
		break;
	case CutPortal:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutportlw.clx");
		celPath = "gendata\\cutportl";
		palPath = "gendata\\cutportl.pal";
		progress_id = 1;
		break;
	case CutPortalRed:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutportrw.clx");
		celPath = "gendata\\cutportr";
		palPath = "gendata\\cutportr.pal";
		progress_id = 1;
		break;
	case CutGate:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutgatew.clx");
		celPath = "gendata\\cutgate";
		palPath = "gendata\\cutgate.pal";
		progress_id = 1;
		break;
	}

	assert(!sgpBackCel);
	sgpBackCel = LoadCel(celPath, 640);
	LoadPalette(palPath);
	BuildCutsceneRgb(celPath); // after the palette: the CEL conversion reads it

	sgdwProgress = 0;
}

void FreeCutsceneBackground()
{
	sgpBackCel = std::nullopt;
	ArtCutsceneWidescreen = std::nullopt;
	CutsceneRgb = nullptr;
}

void DrawCutsceneBackground()
{
	const Rectangle &uiRectangle = GetUIRectangle();
	const Surface &out = GlobalBackBuffer();
	SDL_FillRect(out.surface, nullptr, 0x000000);
	if (CutsceneRgb != nullptr && !out.isIndexed()) {
		CutsceneRgbRect = FitToHeight(CutsceneRgb->w, CutsceneRgb->h, out.w(), out.h());
		SDL_Rect dst = CutsceneRgbRect;
		dst.x += out.region.x;
		dst.y += out.region.y;
		if (SDL_BlitScaled(CutsceneRgb.get(), nullptr, out.surface, &dst) < 0)
			LogWarn("Cutscene: could not scale the painting: {:s}", SDL_GetError());
		return;
	}
	if (ArtCutsceneWidescreen) {
		const ClxSprite sprite = (*ArtCutsceneWidescreen)[0];
		RenderClxSprite(out, sprite, { uiRectangle.position.x - (sprite.width() - uiRectangle.size.width) / 2, uiRectangle.position.y });
	}
	ClxDraw(out, { uiRectangle.position.x, 480 - 1 + uiRectangle.position.y }, (*sgpBackCel)[0]);
}

void DrawCutsceneForeground()
{
	const Surface &out = GlobalBackBuffer();
	// 15 REAL screen pixels (user, 2026-09-12: "make it 15px tall"), not 15 authored at 640x480 and
	// scaled: the old 22 became 33 at 720p, and "15px" means fifteen on the screen being looked at.
	constexpr int ProgressHeight = 15;

	// The WHOLE screen, flush to the floor (user, 2026-09-12: "move it flush to the floor", then
	// "make it full screen width"). The bar no longer rides the painting in either axis, and that is
	// what collapsed the two branches this function used to have into one: BarPos - the per-screen
	// table that placed it, authored at 640x480 - and the 4:3-core arithmetic that mapped it onto the
	// 16:9 redo are both gone, along with the painting's own source size, which nothing else read.
	//
	// The length is now the progress as a FRACTION of the screen rather than the authored 534 pixels,
	// so it fills edge to edge on any resolution instead of stopping wherever 534 scaled pixels
	// happened to land.
	const int trackWidth = out.w();
	const int fillWidth = static_cast<int>(sgdwProgress) * trackWidth / static_cast<int>(MaxProgress);
	SDL_Rect rect = MakeSdlRect(out.region.x, out.region.y + out.h() - ProgressHeight, fillWidth, ProgressHeight);

	if (out.isIndexed()) {
		// One palette index, as before. A per-column gradient here would need a nearest-palette match
		// per column, and this is the legacy 8-bit path - the 32-bit screen is what ships.
		//
		// A palette index through the surface's own fill (v1.11): SDL_FillRect with an index on a
		// 32-bit surface wrote the index as a colour - the blue bar in the first-look screenshot.
		FillRectRgb(out, rect.x - out.region.x, rect.y - out.region.y, rect.w, rect.h, BarColorRgb, BarColor[progress_id]);
	} else {
		// The gradient spans the WHOLE track, not the drawn part, and is revealed as the bar grows.
		// Normalising it to the filled width instead would end every partial bar on bright green,
		// which would make the colour say nothing; this way the colour at the bar's leading edge IS
		// how far along the load is.
		const int x0 = rect.x - out.region.x;
		const int y0 = rect.y - out.region.y;
		for (int i = 0; i < rect.w; i++) {
			const int permille = trackWidth > 0 ? i * 1000 / trackWidth : 1000;
			FillRectRgb(out, x0 + i, y0, 1, rect.h, BarGradientColorAt(permille), BarColor[progress_id]);
		}
	}

	if (DiabloUiSurface() == PalSurface)
		BltFast(&rect, &rect);
	RenderPresent();
}

} // namespace

void RegisterCustomEvents()
{
#ifndef USE_SDL1
	CustomEventsBegin = SDL_RegisterEvents(NumCustomEvents);
#endif
}

bool IsCustomEvent(uint32_t eventType)
{
	return eventType >= CustomEventsBegin && eventType < CustomEventsBegin + NumCustomEvents;
}

interface_mode GetCustomEvent(uint32_t eventType)
{
	return static_cast<interface_mode>(eventType - CustomEventsBegin);
}

uint32_t CustomEventToSdlEvent(interface_mode eventType)
{
	return CustomEventsBegin + eventType;
}

void interface_msg_pump()
{
	SDL_Event event;
	uint16_t modState;
	while (FetchMessage(&event, &modState)) {
		if (event.type != SDL_QUIT) {
			HandleMessage(event, modState);
		}
	}
}

void IncProgress()
{
	if (!HeadlessMode && !demo::IsRunning())
		interface_msg_pump();
	if (!IsProgress)
		return;
	sgdwProgress += 23;
	if (sgdwProgress > MaxProgress)
		sgdwProgress = MaxProgress;
	if (!HeadlessMode && !demo::IsRunning())
		DrawCutsceneForeground();
}

void CompleteProgress()
{
	if (HeadlessMode)
		return;
	if (!IsProgress)
		return;
	while (sgdwProgress < MaxProgress)
		IncProgress();
}

void ShowProgress(interface_mode uMsg)
{
	IsProgress = true;

	gbSomebodyWonGameKludge = false;
	plrmsg_delay(true);

	EventHandler previousHandler = SetEventHandler(DisableInputEventHandler);

	if (!HeadlessMode) {
		assert(ghMainWnd);

		interface_msg_pump();
		ClearScreenBuffer();
		scrollrt_draw_game_screen();

		if (IsHardwareCursor())
			SetHardwareCursorVisible(false);

		BlackPalette();

		// Blit the background once and then free it.
		LoadCutsceneBackground(uMsg);
		DrawCutsceneBackground();
		if (RenderDirectlyToOutputSurface && PalSurface != nullptr) {
			// Render into all the backbuffers if there are multiple.
			const void *initialPixels = PalSurface->pixels;
			if (DiabloUiSurface() == PalSurface)
				BltFast(nullptr, nullptr);
			RenderPresent();
			while (PalSurface->pixels != initialPixels) {
				DrawCutsceneBackground();
				if (DiabloUiSurface() == PalSurface)
					BltFast(nullptr, nullptr);
				RenderPresent();
			}
		}
		FreeCutsceneBackground();

		PaletteFadeIn(8);
		IncProgress();
		sound_init();
		IncProgress();
	}

	Player &myPlayer = *MyPlayer;

	switch (uMsg) {
	case WM_DIABLOADGAME:
		IncProgress();
		IncProgress();
		LoadGame(true);
		IncProgress();
		IncProgress();
		break;
	case WM_DIABNEWGAME:
		myPlayer.pOriginalCathedral = !gbIsHellfire;
		IncProgress();
		FreeGameMem();
		IncProgress();
		pfile_remove_temp_files();
		IncProgress();
		LoadGameLevel(true, ENTRY_MAIN);
		IncProgress();
		break;
	case WM_DIABNEXTLVL:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		setlevel = false;
		currlevel = myPlayer.plrlevel;
		leveltype = GetLevelType(currlevel);
		IncProgress();
		LoadGameLevel(false, ENTRY_MAIN);
		// Oracool: bug postmortem (2026-08-10) - a pending waypoint-spawn reposition (see
		// ApplyPendingWaypointSpawn's doc comment) needs to run exactly once, exactly after the
		// destination level has fully finished loading. WM_DIABNEXTLVL is the interface_mode every
		// waypoint warp uses (see StartNewLvl's call in oracool/waypoint_menu.cpp), and this is the
		// single place that message's LoadGameLevel call returns - a per-tick check placed
		// elsewhere in the main loop was tried first and found to fire too early, before this
		// event had even been processed yet, wasting the pending flag on whatever level the player
		// was still standing on.
		oracool::ApplyPendingWaypointSpawn();
		IncProgress();
		break;
	case WM_DIABPREVLVL:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		currlevel--;
		leveltype = GetLevelType(currlevel);
		assert(myPlayer.isOnActiveLevel());
		IncProgress();
		LoadGameLevel(false, ENTRY_PREV);
		IncProgress();
		break;
	case WM_DIABSETLVL:
		// Note: ReturnLevel, ReturnLevelType and ReturnLvlPosition is only set to ensure vanilla compatibility
		ReturnLevel = GetMapReturnLevel();
		ReturnLevelType = GetLevelType(ReturnLevel);
		ReturnLvlPosition = GetMapReturnPosition();
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		setlevel = true;
		leveltype = setlvltype;
		currlevel = static_cast<uint8_t>(setlvlnum);
		FreeGameMem();
		IncProgress();
		LoadGameLevel(false, ENTRY_SETLVL);
		IncProgress();
		break;
	case WM_DIABRTNLVL:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		setlevel = false;
		FreeGameMem();
		IncProgress();
		currlevel = GetMapReturnLevel();
		leveltype = GetLevelType(currlevel);
		LoadGameLevel(false, ENTRY_RTNLVL);
		IncProgress();
		break;
	case WM_DIABWARPLVL:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		GetPortalLevel();
		IncProgress();
		LoadGameLevel(false, ENTRY_WARPLVL);
		IncProgress();
		break;
	case WM_DIABTOWNWARP:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		setlevel = false;
		currlevel = myPlayer.plrlevel;
		leveltype = GetLevelType(currlevel);
		IncProgress();
		LoadGameLevel(false, ENTRY_TWARPDN);
		IncProgress();
		break;
	case WM_DIABTWARPUP:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		currlevel = myPlayer.plrlevel;
		leveltype = GetLevelType(currlevel);
		IncProgress();
		LoadGameLevel(false, ENTRY_TWARPUP);
		IncProgress();
		break;
	case WM_DIABRETOWN:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		setlevel = false;
		currlevel = myPlayer.plrlevel;
		leveltype = GetLevelType(currlevel);
		IncProgress();
		LoadGameLevel(false, ENTRY_MAIN);
		IncProgress();
		break;
	}

	if (uMsg != WM_DIABNEWGAME && uMsg != WM_DIABLOADGAME)
		oracool::ScheduleAutoSaveForLevelChange();

	if (!HeadlessMode) {
		assert(ghMainWnd);

		if (RenderDirectlyToOutputSurface && PalSurface != nullptr) {
			// Ensure that all back buffers have the full progress bar.
			const void *initialPixels = PalSurface->pixels;
			do {
				DrawCutsceneForeground();
				if (DiabloUiSurface() == PalSurface)
					BltFast(nullptr, nullptr);
				RenderPresent();
			} while (PalSurface->pixels != initialPixels);
		}

		PaletteFadeOut(8);
	}

	previousHandler = SetEventHandler(previousHandler);
	assert(previousHandler == DisableInputEventHandler);
	IsProgress = false;

	NetSendCmdLocParam2(true, CMD_PLAYER_JOINLEVEL, myPlayer.position.tile, myPlayer.plrlevel, myPlayer.plrIsOnSetLevel ? 1 : 0);
	plrmsg_delay(false);

	if (gbSomebodyWonGameKludge && myPlayer.isOnLevel(16)) {
		PrepDoEnding();
	}

	gbSomebodyWonGameKludge = false;
}

} // namespace devilution
