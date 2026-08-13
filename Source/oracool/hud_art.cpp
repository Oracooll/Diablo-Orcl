#include "oracool/hud_art.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include <SDL.h>

#include "engine/palette.h"
#include "oracool/hud_layout.h"
#include "oracool/inventory_layout.h"
#include "player.h"
#include "utils/log.hpp"
#include "utils/png.h"
#include "utils/sdl_geometry.h"

namespace devilution::oracool {

namespace {

struct ArtAsset {
	const char *assetPath;
	std::vector<uint8_t> rgba;
	int width = 0;
	int height = 0;
	bool loadAttempted = false;
	std::optional<OwnedSurface> bright;
	/**
	 * Orbs only. The composition MINUS the sphere: everything outside the glass circle, with the
	 * circle itself left transparent. Drawn first and opaquely, so the ornament and the rim are
	 * never affected by whatever happens inside the glass.
	 */
	std::optional<OwnedSurface> frame;
	/**
	 * Orbs only. The mirror image of `frame`: the sphere circle alone, dimmed, everything else
	 * transparent. This is what gets blended into the empty part of the orb - see DrawOrb.
	 */
	std::optional<OwnedSurface> sphereDim;
};

ArtAsset PlateArt { "ui\\middle_hud.png" };
ArtAsset HealthOrbArt { "ui\\health_orb.png" };
ArtAsset ManaOrbArt { "ui\\mana_orb.png" };
ArtAsset MenuIconsArt { "ui\\menu_icons.png" };
/**
 * Oracool V1 inventory window. The panel is one flat composition (background, paladin
 * silhouette, slot frames and the class sygil are all baked in by tools/InvCompose.cs); the tabs
 * and SORT button ship separately because they change state at runtime.
 */
ArtAsset InventoryPanelArt { "ui\\inventory_panel.png" };
// v3 of the tab strip: arabic 1-9 and X, square and borderless, replacing the roman-numeral
// set. Same 280x84 shape and same 28px cells as the sheet it replaces, so this is a pure
// asset swap - see tools/CutTabButtonsV3.ps1, which cuts it to that format deliberately.
ArtAsset InventoryTabsArt { "ui\\inventory_tabs_v3.png" };
ArtAsset InventorySortArt { "ui\\inventory_sort.png" };
/** The belt's Town Portal button. Drawn over the portal ring painted into the plate art. */
ArtAsset TownPortalIconArt { "ui\\town_portal_icon.png" };
constexpr Size TownPortalIconSize { 27, 29 };
/** The belt's burger-menu button. Same treatment as the Portal cell. */
ArtAsset BurgerMenuButtonArt { "ui\\burger_menu_button.png" };
/** The level-up indicator that appears under the clock when attribute points are unspent. */
ArtAsset LevelUpIconArt { "ui\\level_up_icon.png" };
/**
 * Oracool V1 waypoint list. The panel is one flat 340x660 composition - stone texture, segmented
 * border and the baked "WAYPOINT" label - built by tools/BuildWaypointPanel.ps1. The per-row pads
 * ship separately because which state each row draws depends on the player's unlocked waypoints.
 * waypoint_icons.png is two 30x30 cells: column 0 dormant, column 1 active.
 */
ArtAsset WaypointPanelArt { "ui\\waypoint_panel.png" };
ArtAsset WaypointIconsArt { "ui\\waypoint_icons.png" };
constexpr Size BurgerMenuButtonSize { 27, 29 };
/**
 * User request: nudge the burger button up by a pixel. Centring it in the cell puts it a touch
 * low against the neighbouring cells, because the cell rect includes the plate's label strip
 * along its top edge.
 */
constexpr int BurgerMenuButtonRise = 2;

// Bug postmortem (2026-08-10): the first quantization attempt matched against logical_palette on
// the first drawn frame - but at that moment logical_palette still holds the *loading screen's*
// cutscene palette; the level palette only reaches logical_palette inside PaletteFadeIn, which
// runs AFTER the first frame renders. The result was the art quantized against a red-heavy
// cutscene palette - pure color noise in game. orig_palette, by contrast, is written synchronously
// by LoadPalette during level load, so it's always the real level palette. This snapshot of its
// global half (entries 128-255, the only ones used for matching - identical across level types by
// design, see engine/palette.h) triggers a requantize if it ever actually changes.
std::array<SDL_Color, 128> PaletteSnapshot;
bool QuantizedOnce = false;

bool GlobalPaletteChanged()
{
	return !QuantizedOnce || std::memcmp(PaletteSnapshot.data(), &orig_palette[128], sizeof(PaletteSnapshot)) != 0;
}

/** @brief Nearest global-palette (128-255) index for an opaque RGB color; 0 means transparent.
 * Cached per RGB555 bucket so the full-image pass stays cheap. */
uint8_t NearestGlobalPaletteIndex(uint8_t r, uint8_t g, uint8_t b, std::vector<uint8_t> &cache)
{
	const uint16_t key = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
	if (cache[key] != 0)
		return cache[key];

	int best = 128;
	int bestDist = INT32_MAX;
	for (int i = 128; i < 256; i++) {
		const SDL_Color &c = orig_palette[i];
		const int dr = static_cast<int>(c.r) - r;
		const int dg = static_cast<int>(c.g) - g;
		const int db = static_cast<int>(c.b) - b;
		// human-vision-ish channel weighting, same idea most palette quantizers use
		const int dist = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}
	cache[key] = static_cast<uint8_t>(best);
	return cache[key];
}

void LoadPixels(ArtAsset &asset)
{
	asset.loadAttempted = true;

	SDL_Surface *png = LoadPNG(asset.assetPath);
	if (png == nullptr) {
		LogWarn("Oracool HUD art: asset {:s} not found - element renders without art", asset.assetPath);
		return;
	}

	SDL_Surface *rgba = SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(png);
	if (rgba == nullptr) {
		LogWarn("Oracool HUD art: pixel format conversion failed for {:s}: {:s}", asset.assetPath, SDL_GetError());
		return;
	}

	asset.width = rgba->w;
	asset.height = rgba->h;
	asset.rgba.resize(static_cast<size_t>(asset.width) * asset.height * 4);
	const auto *srcPixels = static_cast<const uint8_t *>(rgba->pixels);
	for (int y = 0; y < asset.height; y++) {
		std::memcpy(&asset.rgba[static_cast<size_t>(y) * asset.width * 4],
		    srcPixels + static_cast<size_t>(y) * rgba->pitch,
		    static_cast<size_t>(asset.width) * 4);
	}
	SDL_FreeSurface(rgba);
}

/**
 * @brief (Re)quantizes an asset's RGBA pixels into its 8-bit surfaces.
 *
 * If `dimCircle` is set (orbs), the composition is also split in two along that circle: `frame`
 * gets everything outside it, `sphereDim` gets the inside, dimmed. Splitting rather than producing
 * one whole-image "dark" variant is what lets DrawOrb blend the empty glass against the world
 * behind it without the ornament and rim going translucent too.
 *
 * @param dimCircle Sphere circle in asset-local pixels: {center, radius packed as Size.width}.
 */
void QuantizeAsset(ArtAsset &asset, std::optional<Rectangle> dimCircle)
{
	if (asset.rgba.empty())
		return;

	asset.bright.emplace(asset.width, asset.height);
	if (dimCircle) {
		asset.frame.emplace(asset.width, asset.height);
		asset.sphereDim.emplace(asset.width, asset.height);
	}

	std::vector<uint8_t> cache(1 << 15, 0);

	for (int y = 0; y < asset.height; y++) {
		const uint8_t *srcRow = &asset.rgba[static_cast<size_t>(y) * asset.width * 4];
		uint8_t *brightRow = &(*asset.bright)[Point { 0, y }];
		uint8_t *frameRow = dimCircle ? &(*asset.frame)[Point { 0, y }] : nullptr;
		uint8_t *sphereRow = dimCircle ? &(*asset.sphereDim)[Point { 0, y }] : nullptr;
		for (int x = 0; x < asset.width; x++) {
			const uint8_t r = srcRow[x * 4 + 0];
			const uint8_t g = srcRow[x * 4 + 1];
			const uint8_t b = srcRow[x * 4 + 2];
			const uint8_t a = srcRow[x * 4 + 3];

			if (a < 128) {
				brightRow[x] = 0;
				if (frameRow != nullptr) {
					frameRow[x] = 0;
					sphereRow[x] = 0;
				}
				continue;
			}

			brightRow[x] = NearestGlobalPaletteIndex(r, g, b, cache);

			if (frameRow != nullptr) {
				const int dx = x - dimCircle->position.x;
				const int dy = y - dimCircle->position.y;
				const int radius = dimCircle->size.width;
				if (dx * dx + dy * dy <= radius * radius) {
					frameRow[x] = 0;
					sphereRow[x] = NearestGlobalPaletteIndex(r * 2 / 5, g * 2 / 5, b * 2 / 5, cache);
				} else {
					frameRow[x] = brightRow[x];
					sphereRow[x] = 0;
				}
			}
		}
	}
}

// Bug postmortem (2026-08-11): assets load lazily per draw call, but quantization used to be a
// single all-assets pass triggered only by palette change. The orbs draw earlier in the frame
// than the plate (DrawView's tail vs. the belt block), so the first quantize pass ran before the
// plate's pixels were even loaded - leaving the plate's surface permanently unbuilt and its draw
// dereferencing an empty optional ("Debug Assertion Failed ... operator*() called on empty
// optional" at session start). Loading everything together and requantizing whenever any loaded
// asset is missing its surfaces makes the order of first draws irrelevant.
void EnsureLoadedAll()
{
	if (!PlateArt.loadAttempted)
		LoadPixels(PlateArt);
	if (!HealthOrbArt.loadAttempted)
		LoadPixels(HealthOrbArt);
	if (!ManaOrbArt.loadAttempted)
		LoadPixels(ManaOrbArt);
	if (!MenuIconsArt.loadAttempted)
		LoadPixels(MenuIconsArt);
	if (!InventoryPanelArt.loadAttempted)
		LoadPixels(InventoryPanelArt);
	if (!InventoryTabsArt.loadAttempted)
		LoadPixels(InventoryTabsArt);
	if (!InventorySortArt.loadAttempted)
		LoadPixels(InventorySortArt);
	if (!TownPortalIconArt.loadAttempted)
		LoadPixels(TownPortalIconArt);
	if (!BurgerMenuButtonArt.loadAttempted)
		LoadPixels(BurgerMenuButtonArt);
	if (!LevelUpIconArt.loadAttempted)
		LoadPixels(LevelUpIconArt);
	if (!WaypointPanelArt.loadAttempted)
		LoadPixels(WaypointPanelArt);
	if (!WaypointIconsArt.loadAttempted)
		LoadPixels(WaypointIconsArt);
}

bool NeedsQuantize()
{
	if (GlobalPaletteChanged())
		return true;
	if (!PlateArt.rgba.empty() && !PlateArt.bright)
		return true;
	if (!HealthOrbArt.rgba.empty() && !HealthOrbArt.bright)
		return true;
	if (!ManaOrbArt.rgba.empty() && !ManaOrbArt.bright)
		return true;
	if (!MenuIconsArt.rgba.empty() && !MenuIconsArt.bright)
		return true;
	if (!InventoryPanelArt.rgba.empty() && !InventoryPanelArt.bright)
		return true;
	if (!InventoryTabsArt.rgba.empty() && !InventoryTabsArt.bright)
		return true;
	if (!InventorySortArt.rgba.empty() && !InventorySortArt.bright)
		return true;
	if (!TownPortalIconArt.rgba.empty() && !TownPortalIconArt.bright)
		return true;
	if (!BurgerMenuButtonArt.rgba.empty() && !BurgerMenuButtonArt.bright)
		return true;
	if (!LevelUpIconArt.rgba.empty() && !LevelUpIconArt.bright)
		return true;
	if (!WaypointPanelArt.rgba.empty() && !WaypointPanelArt.bright)
		return true;
	if (!WaypointIconsArt.rgba.empty() && !WaypointIconsArt.bright)
		return true;
	return false;
}

void EnsureQuantized()
{
	if (!NeedsQuantize())
		return;

	QuantizeAsset(PlateArt, std::nullopt);
	QuantizeAsset(HealthOrbArt, Rectangle { GetHealthOrbSphereCenterLocal(), Size { GetOrbSphereRadius(), 0 } });
	QuantizeAsset(ManaOrbArt, Rectangle { GetManaOrbSphereCenterLocal(), Size { GetOrbSphereRadius(), 0 } });
	QuantizeAsset(MenuIconsArt, std::nullopt);
	QuantizeAsset(InventoryPanelArt, std::nullopt);
	QuantizeAsset(InventoryTabsArt, std::nullopt);
	QuantizeAsset(InventorySortArt, std::nullopt);
	QuantizeAsset(TownPortalIconArt, std::nullopt);
	QuantizeAsset(BurgerMenuButtonArt, std::nullopt);
	QuantizeAsset(LevelUpIconArt, std::nullopt);
	QuantizeAsset(WaypointPanelArt, std::nullopt);
	QuantizeAsset(WaypointIconsArt, std::nullopt);

	std::memcpy(PaletteSnapshot.data(), &orig_palette[128], sizeof(PaletteSnapshot));
	QuantizedOnce = true;
}

/**
 * @brief Blits rows [srcTop, srcBottom) of an 8-bit surface at half opacity, skipping index 0.
 *
 * Oracool: user request - "can you make orbs transparent as they deplete?". There is no alpha in
 * an 8-bit palettized renderer, so translucency means blending through
 * `paletteTransparencyLookup`, the engine's own 256x256 table of "the palette entry whose colour
 * is the average of these two" - the same mechanism behind DrawHalfTransparentRectTo. Half is the
 * only strength the table gives in one pass; applying it twice moves toward the source, not away,
 * so it would make the glass LESS transparent rather than more.
 */
void BlitHalfTransparentSkipZero(const Surface &out, const Surface &src, Point position, int srcTop, int srcBottom)
{
	for (int y = srcTop; y < srcBottom; y++) {
		const int dstY = position.y + y;
		if (dstY < 0 || dstY >= out.h())
			continue;
		const uint8_t *srcRow = &src[Point { 0, y }];
		uint8_t *dstRow = &out[Point { 0, dstY }];
		for (int x = 0; x < src.w(); x++) {
			if (srcRow[x] == 0)
				continue;
			const int dstX = position.x + x;
			if (dstX < 0 || dstX >= out.w())
				continue;
			dstRow[dstX] = paletteTransparencyLookup[dstRow[dstX]][srcRow[x]];
		}
	}
}

void DrawOrb(const Surface &out, ArtAsset &asset, Point position, Point sphereCenterLocal, int currValue, int maxValue)
{
	EnsureLoadedAll();
	if (asset.rgba.empty())
		return;
	EnsureQuantized();
	if (!asset.frame || !asset.sphereDim || !asset.bright)
		return;

	const int radius = GetOrbSphereRadius();
	const int span = 2 * radius;
	const int64_t curr = std::clamp<int64_t>(currValue, 0, maxValue > 0 ? maxValue : 0);
	const int filledRows = (maxValue > 0) ? static_cast<int>(span * curr / maxValue) : 0;
	const int revealTop = std::clamp(sphereCenterLocal.y + radius - filledRows, 0, asset.height);

	// 1. The composition around the glass - ornament, rim, mount - always fully opaque. Drawn from
	//    its own surface rather than from the whole image, so step 2 cannot touch it.
	out.BlitFromSkipColorIndexZero(*asset.frame, MakeSdlRect(0, 0, asset.width, asset.height), position);

	// 2. The empty part of the glass, blended into whatever the world drew behind it. This is the
	//    change the user asked for: the orb used to paint an opaque dimmed sphere here, so a
	//    near-dead character still had a solid black ball in the corner of the screen. Now the
	//    glass genuinely empties.
	BlitHalfTransparentSkipZero(out, *asset.sphereDim, position, 0, revealTop);

	// 3. The filled part, opaque, bottom-up. Full rows: outside the sphere these pixels are
	//    identical to what step 1 already drew, so overwriting them is invisible.
	if (revealTop >= asset.height)
		return;
	out.BlitFromSkipColorIndexZero(*asset.bright,
	    MakeSdlRect(0, revealTop, asset.width, asset.height - revealTop),
	    position + Displacement { 0, revealTop });
}

} // namespace

bool HasMiddleHudArt()
{
	return PlateArt.bright.has_value();
}

void DrawMiddleHudArt(const Surface &out)
{
	EnsureLoadedAll();
	if (PlateArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!PlateArt.bright)
		return;

	const Point position = GetMiddleHudRect().position;
	out.BlitFromSkipColorIndexZero(*PlateArt.bright, MakeSdlRect(0, 0, PlateArt.width, PlateArt.height), position);
}

void DrawMenuIcon(const Surface &out, int iconIndex, int state, Point position)
{
	if (iconIndex < 0 || iconIndex >= MenuIconCount || state < 0 || state > 2)
		return;

	EnsureLoadedAll();
	if (MenuIconsArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!MenuIconsArt.bright)
		return;

	// The sheet is a plain grid: column = state, row = entry.
	out.BlitFromSkipColorIndexZero(*MenuIconsArt.bright,
	    MakeSdlRect(state * MenuIconSize.width, iconIndex * MenuIconSize.height, MenuIconSize.width, MenuIconSize.height),
	    position);
}

void DrawInventoryPanelArt(const Surface &out)
{
	EnsureLoadedAll();
	if (InventoryPanelArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!InventoryPanelArt.bright)
		return;

	out.BlitFromSkipColorIndexZero(*InventoryPanelArt.bright,
	    MakeSdlRect(0, 0, InventoryPanelArt.width, InventoryPanelArt.height),
	    GetInventoryPanelRect().position);
}

bool HasInventoryPanelArt()
{
	EnsureLoadedAll();
	return !InventoryPanelArt.rgba.empty();
}

void DrawInventoryTab(const Surface &out, int index, int state)
{
	if (index < 0 || index >= TabCount || state < 0 || state > 2)
		return;

	EnsureLoadedAll();
	if (InventoryTabsArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!InventoryTabsArt.bright)
		return;

	// Uniform grid: column = tab, row = state. Every cell is the *selected* size and the
	// unselected states are inset within their cell with transparent padding, so the blit
	// position is the same whatever the state - see TabCellSize in inventory_layout.h.
	const Point origin = GetInventoryPanelRect().position + Displacement { GetTabCellOrigin(index).x, GetTabCellOrigin(index).y };
	out.BlitFromSkipColorIndexZero(*InventoryTabsArt.bright,
	    MakeSdlRect(index * TabCellSize.width, state * TabCellSize.height, TabCellSize.width, TabCellSize.height),
	    origin);
}

void DrawTownPortalIcon(const Surface &out, int state)
{
	if (state < 0 || state > 2)
		return;

	EnsureLoadedAll();
	if (TownPortalIconArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!TownPortalIconArt.bright)
		return;

	// Centred in the belt cell, which is slightly larger than the icon - that margin is the
	// cell's own carved bevel from the plate art, deliberately left showing.
	const Rectangle cell = GetBeltSlotRect(BeltTownPortalSlotIndex);
	const Point position {
		cell.position.x + (cell.size.width - TownPortalIconSize.width) / 2,
		cell.position.y + (cell.size.height - TownPortalIconSize.height) / 2
	};
	out.BlitFromSkipColorIndexZero(*TownPortalIconArt.bright,
	    MakeSdlRect(state * TownPortalIconSize.width, 0, TownPortalIconSize.width, TownPortalIconSize.height),
	    position);
}

void DrawBurgerMenuButton(const Surface &out, int state)
{
	if (state < 0 || state > 2)
		return;

	EnsureLoadedAll();
	if (BurgerMenuButtonArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!BurgerMenuButtonArt.bright)
		return;

	const Rectangle cell = GetBeltSlotRect(BeltMenuSlotIndex);
	const Point position {
		cell.position.x + (cell.size.width - BurgerMenuButtonSize.width) / 2,
		cell.position.y + (cell.size.height - BurgerMenuButtonSize.height) / 2 - BurgerMenuButtonRise
	};
	out.BlitFromSkipColorIndexZero(*BurgerMenuButtonArt.bright,
	    MakeSdlRect(state * BurgerMenuButtonSize.width, 0, BurgerMenuButtonSize.width, BurgerMenuButtonSize.height),
	    position);
}

void DrawLevelUpIconArt(const Surface &out, int state)
{
	if (state < 0 || state > 2)
		return;

	EnsureLoadedAll();
	if (LevelUpIconArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!LevelUpIconArt.bright)
		return;

	const Rectangle rect = GetLevelUpIconRect();
	out.BlitFromSkipColorIndexZero(*LevelUpIconArt.bright,
	    MakeSdlRect(state * LevelUpIconSize.width, 0, LevelUpIconSize.width, LevelUpIconSize.height),
	    rect.position);
}

void DrawWaypointPanelArt(const Surface &out, Point origin)
{
	EnsureLoadedAll();
	if (WaypointPanelArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!WaypointPanelArt.bright)
		return;

	out.BlitFromSkipColorIndexZero(*WaypointPanelArt.bright,
	    MakeSdlRect(0, 0, WaypointPanelArt.width, WaypointPanelArt.height), origin);
}

bool HasWaypointPanelArt()
{
	EnsureLoadedAll();
	return !WaypointPanelArt.rgba.empty();
}

void DrawWaypointIcon(const Surface &out, Point origin, bool active)
{
	EnsureLoadedAll();
	if (WaypointIconsArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!WaypointIconsArt.bright)
		return;

	// Two equal cells side by side: column 0 dormant, column 1 active. Derived from the sheet's own
	// width rather than a hardcoded 30 so a recut at a different icon size still lines up.
	const int cell = WaypointIconsArt.width / 2;
	out.BlitFromSkipColorIndexZero(*WaypointIconsArt.bright,
	    MakeSdlRect(active ? cell : 0, 0, cell, WaypointIconsArt.height), origin);
}

Size GetWaypointIconSize()
{
	EnsureLoadedAll();
	if (WaypointIconsArt.rgba.empty())
		return { 0, 0 };
	return { WaypointIconsArt.width / 2, WaypointIconsArt.height };
}

void DrawHealthOrb(const Surface &out)
{
	const Player &player = *MyPlayer;
	DrawOrb(out, HealthOrbArt, GetHealthOrbRect().position, GetHealthOrbSphereCenterLocal(),
	    player._pHitPoints >> 6, player._pMaxHP >> 6);
}

void DrawManaOrb(const Surface &out)
{
	const Player &player = *MyPlayer;
	DrawOrb(out, ManaOrbArt, GetManaOrbRect().position, GetManaOrbSphereCenterLocal(),
	    player._pMana >> 6, player._pMaxMana >> 6);
}

} // namespace devilution::oracool
