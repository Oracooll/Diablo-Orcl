#include "oracool/hud_art.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include <SDL.h>

#include "engine/render/primitive_render.hpp"
#include "engine/palette.h"
#include "oracool/class_tree.h" // ClassTreeSkillForSpell - the wells draw the tree's own icons
#include "oracool/hud_layout.h"
#include "oracool/inventory_layout.h"
#include "oracool/paladin_skills.h"
#include "options.h" // the HUD Plate Art switch
#include "oracool/ornate_border.h" // ThemeEdgeColor
#include "panels/spell_icons.hpp" // the vanilla plate behind every skill icon
#include "player.h"
#include "spelldat.h"
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
	/**
	 * Strips only. Per cell, the width of the uniform transparent border around its artwork.
	 *
	 * The icon batches do not agree about margins and never will: batch 01's Paladin combat icons
	 * are an opaque 48x48 centred in a 56x56 cell with an exact 4px surround, while the twenty aura
	 * icons already in the same strip run edge to edge. Scaling the whole CELL onto a skill plate
	 * therefore filled it for one set and left a ring of bare plate around the other - which is what
	 * the difference looked like in play once both were on screen together (user, 2026-08-27: "i
	 * want them to completely overlap the background").
	 *
	 * Measured rather than declared, so a future batch with a 6px margin or none at all needs no
	 * code change and no re-cut art.
	 *
	 * A UNIFORM inset - the largest N with at least N transparent pixels on all four sides - not the
	 * tight bounding box. The tight box of an icon that is 37 wide and 56 tall would be stretched to
	 * a square plate and come out distorted; taking the same N off each side can only remove empty
	 * border, never reshape what is drawn. For edge-to-edge art N is 0 and nothing changes at all.
	 */
	std::vector<uint8_t> cellInsets;
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
	/**
	 * Silhouette only. A 1px band tracing just outside the figure's edge, everything else
	 * transparent. Drawn opaquely after the blended body, which is the whole point: the body is
	 * half-transparent and takes the panel's colour, so without a solid edge the shape has no
	 * definition at all.
	 */
	std::optional<OwnedSurface> outline;
};

/**
 * @brief How far the HUD's chrome is pulled toward the theme's gold, in percent.
 *
 * Shared by every tinted HUD asset so they cannot drift into different golds - the whole point is
 * that they read as one surface. See the note at the QuantizeAsset calls for why it is partial.
 */
constexpr int HudTintStrengthPercent = 50;

/**
 * @brief How much of the class silhouette's own brightness survives quantization, in percent.
 *
 * Oracool: user, 2026-08-19 - "now make it darker to stand out a bit more", straight after the gold
 * tint came off. Untinted, the figure sits at the limestone's own value and stops separating from
 * it; darkening is what gives it back its edge, and it is the right lever rather than a heavier
 * blend, because the silhouette is meant to read as a SHADOW behind the equipment slots. A shadow
 * gets darker, not more opaque - two more transparency passes would have made it a picture in front
 * of the slots instead, which is the note DrawClassSilhouette already carries.
 *
 * This knob was written the same day to pull the bottom HUD plate down and went unused when the user
 * chose no colour treatment there at all. It was kept on the argument that it was the right control
 * for the problem; this is that bet paying off, on a different asset within the hour.
 *
 * Applied BEFORE the palette match, so the art keeps its own shading and simply sits lower. Lower is
 * darker; this is a taste dial against a screenshot.
 */
constexpr int SilhouetteLuminancePercent = 55;

ArtAsset PlateArt { "ui\\middle_hud.png" };
ArtAsset HealthOrbArt { "ui\\health_orb.png" };
ArtAsset ManaOrbArt { "ui\\mana_orb.png" };
ArtAsset MenuIconsArt { "ui\\menu_icons.png" };
/**
 * Oracool V1 inventory window. The panel is one flat composition (background, paladin
 * silhouette, slot frames and the class sygil are all baked in by tools/InvCompose.cs); the tabs
 * and SORT button ship separately because they change state at runtime.
 */
/**
 * The inventory and stash window backgrounds - the "Cathedral Reliquary" pair, 340x720 each, drawn
 * 1:1 with no scaling. They REPLACE the procedural themed fill and ornate border on these two
 * windows only; every other window keeps the shared theme.
 *
 * Opaque art, and safe under BlitFromSkipColorIndexZero because NearestGlobalPaletteIndex only ever
 * searches indices 128-255 - a 0 in the quantized output can only have come from real transparency,
 * never from a colour the art actually uses.
 *
 * The class silhouette stays a SEPARATE overlay drawn on top, as it has been since the composed
 * stone panel was retired: baking a figure into the background would give every class the same one.
 */
ArtAsset InventoryPanelArt { "ui\\inventory_background.png" };
/**
 * The one 340x720 background all six side panels share - the artisan-bezel family's ashen limestone
 * (MPQ sweep 2026-08-18, unit D; the family chosen by the user on review).
 *
 * Untitled by design, which is why it can be shared. A carved-title variant was tried first and
 * gave every window its own background with its name incised into a top rail; that made the windows
 * name themselves in stone but cost the Abilities window its sheet name, and it was not the family
 * the user wanted. One panel, six windows, titles drawn over it as they always were.
 */
ArtAsset SidePanelArt { "ui\\panel_bg.png" };
// The reliquary-chest tabs: a 102x31 atlas of three 34x31 frames - inactive, hover, active - from
// oracool-stash-tab-button-pack. Replaces the numeral strips entirely (v1 roman bordered, v2 roman
// borderless, v3 arabic), and with them the idea that a tab needs a number on it: all ten pages
// wear the same chest, and the open one is told apart by standing proud and lighting red.
ArtAsset InventoryTabsArt { "ui\\inventory_tabs_chest.png" };
ArtAsset InventorySortArt { "ui\\inventory_sort.png" };
/** The belt's Town Portal button. Drawn over the portal ring painted into the plate art. */
ArtAsset TownPortalIconArt { "ui\\town_portal_icon.png" };
constexpr Size TownPortalIconSize { 27, 29 };
/** The belt's burger-menu button. Same treatment as the Portal cell. */
ArtAsset BurgerMenuButtonArt { "ui\\burger_menu_button.png" };
/** The level-up indicator that appears under the clock when attribute points are unspent. */
ArtAsset LevelUpIconArt { "ui\\level_up_icon.png" };
/**
 * The carved stone bezels - see oracool/grid_bezel.h for the family and the (6,6) placement rule.
 * Indexed by the size of the content they frame rather than by name, because that is the only
 * thing a caller knows: DrawGridBezel is handed a rect and has to find the frame that fits it.
 */
struct GridBezelEntry {
	Size content;
	ArtAsset art;
};
GridBezelEntry GridBezels[] = {
	{ { 1 * CellPx, 1 * CellPx }, ArtAsset { "ui\\grid_bezel_1x1.png" } },
	{ { 2 * CellPx, 1 * CellPx }, ArtAsset { "ui\\grid_bezel_2x1.png" } },
	{ { 2 * CellPx, 2 * CellPx }, ArtAsset { "ui\\grid_bezel_2x2.png" } },
	{ { 2 * CellPx, 3 * CellPx }, ArtAsset { "ui\\grid_bezel_2x3.png" } },
	{ { 10 * CellPx, 7 * CellPx }, ArtAsset { "ui\\grid_bezel_inventory.png" } },
	{ { 10 * CellPx, 16 * CellPx }, ArtAsset { "ui\\grid_bezel_stash.png" } },
};
constexpr int GridBezelCount = sizeof(GridBezels) / sizeof(GridBezels[0]);

GridBezelEntry *FindGridBezel(Size contentSize)
{
	for (GridBezelEntry &entry : GridBezels) {
		if (entry.content == contentSize)
			return &entry;
	}
	return nullptr;
}
// The numbered skill-point icons (user, 2026-08-17: "use these icons as a display of how many
// skill points i have available to distribute"). Two 99-frame strips, 64px square cells, frame
// N-1 wearing the numeral N - cut from the user's own level-up icon states with the cross
// replaced by the number. Dark is the resting state, lit the hover.
/**
 * The unspent-skill-points frame: ONE picture with a drawn number in it, replacing the two 99-frame
 * strips (points_icons_dark/lit.png) whose numeral was baked into the art.
 *
 * A drawn number is strictly better here and the swap is why the strips are gone rather than
 * merely unused: baked numerals run out at 100, need regenerating whenever the cap moves, and lock
 * the count to whatever size the strip was cut at. See tools/CutSkillPointsIcon.ps1.
 */
ArtAsset SkillPointsFrameArt { "ui\\skill_points.png" };
/**
 * Oracool V1 waypoint list. The panel is one flat 340x660 composition - stone texture, segmented
 * border and the baked "WAYPOINT" label - built by tools/BuildWaypointPanel.ps1. The per-row pads
 * ship separately because which state each row draws depends on the player's unlocked waypoints.
 * waypoint_icons.png is two 30x30 cells: column 0 dormant, column 1 active.
 */
ArtAsset WaypointPanelArt { "ui\\waypoint_panel.png" };
ArtAsset WaypointIconsArt { "ui\\waypoint_icons.png" };
/**
 * The six class-tree icon strips: 56x56 cells, one frame per skill, each in its own class's
 * ClassTreeSkill order. `ClassTreeIconIndex` is the frame number - a skill's position within its
 * class block - so a strip's ordering IS the table's ordering and the two cannot be read apart.
 *
 * Replaced wholesale on 2026-08-27 from colorful-skill-icons-complete-273. That set is generated
 * against the skill table, and all 273 names were diffed against it in order before installing.
 *
 * The counts matter and were wrong before: Paladin 49, Barbarian 49, Sorceress 48, Rogue 49,
 * Bard 39, Monk 39. The previous strips ran 30, 30, 30, 21 and 21 frames, against class blocks that
 * had grown well past them - so every skill beyond the end of its strip drew NOTHING. Silently, for
 * the reason ClassTreeStrips' own note gives: a missing frame is indistinguishable from a skill that
 * has no icon. Only the Paladin's was current, having been rebuilt the day before.
 */
ArtAsset PaladinTreeIconsArt { "ui\\paladin_tree_icons.png" };
ArtAsset BarbTreeIconsArt { "ui\\barb_tree_icons.png" };
ArtAsset SorcTreeIconsArt { "ui\\sorc_tree_icons.png" };
ArtAsset RogueTreeIconsArt { "ui\\rogue_tree_icons.png" };
ArtAsset BardTreeIconsArt { "ui\\bard_tree_icons.png" };
ArtAsset MonkTreeIconsArt { "ui\\monk_tree_icons.png" };

/**
 * @brief Every class tree strip, as one list the load/quantize/reset passes walk.
 *
 * Bug fix (2026-08-16, user report "i dont see it" about the stash background, which turned out to
 * be the same fault): those three passes name each ArtAsset INDIVIDUALLY, and only the Paladin's
 * strip was ever added. The other five loaded nothing, so DrawStripIcon returned at its
 * `asset.rgba.empty()` guard and five of the six trees drew plates with no icons on them - silently,
 * because a missing strip is indistinguishable from a skill that has no icon.
 *
 * Grouping them means adding a seventh class is one line in one place instead of four lines in four,
 * which is what the individual naming got wrong. SilhouetteArt was already shaped this way.
 */
ArtAsset *const ClassTreeStrips[] = {
	&PaladinTreeIconsArt, &BarbTreeIconsArt, &SorcTreeIconsArt,
	&RogueTreeIconsArt, &BardTreeIconsArt, &MonkTreeIconsArt,
};

/** @brief The strip @p heroClass's tree draws from, or the Paladin's as a harmless fallback. */
ArtAsset &TreeStripFor(HeroClass heroClass)
{
	switch (heroClass) {
	case HeroClass::Barbarian:
		return BarbTreeIconsArt;
	case HeroClass::Sorcerer:
		return SorcTreeIconsArt;
	case HeroClass::Rogue:
		return RogueTreeIconsArt;
	case HeroClass::Bard:
		return BardTreeIconsArt;
	case HeroClass::Monk:
		return MonkTreeIconsArt;
	default:
		return PaladinTreeIconsArt;
	}
}
/** Oracool: the Paladin's seven skills, same 38x38 cells, in oracool::PaladinSkill order. */
ArtAsset PaladinSkillIconsArt { "ui\\paladin_skill_icons.png" };
/**
 * Oracool: the two basic-attack icons - cell 0 Regular Attack, cell 1 Fist Attack, in
 * oracool::AttackIcon order. Same 38x38 cells as the aura and Barbarian sheets, deliberately: this
 * strip is drawn in the Abilities window's rows AND in the HUD's two skill wells, and the wells
 * were sized for the engine's 37x38 small spell icon.
 */
ArtAsset AttackIconsArt { "ui\\attack_icons.png" };
/**
 * The class figures behind the inventory's equipment slots. They used to be baked into
 * ui\inventory_panel.png; the shared theme replaced that composition with a procedural fill and
 * bevel, so they ship separately now and survive future restyles. Cut by
 * tools/CutClassSilhouette.ps1 from the class reference sheet.
 *
 * Named for the reference sheet's figures, not for the classes - the sheet calls the Rogue "Archer"
 * and calls HeroClass::Warrior "Paladin" (which is also what Oracool displays). SilhouetteForClass
 * below is the one place that mapping lives.
 */
ArtAsset SilhouetteArt[] = {
	{ "ui\\silhouette_paladin.png" },
	{ "ui\\silhouette_archer.png" },
	{ "ui\\silhouette_sorcerer.png" },
	{ "ui\\silhouette_barbarian.png" },
};

/**
 * @brief The silhouette for @p heroClass, or nullptr if that class has no figure on the sheet.
 *
 * Monk has no figure at all. Bard shares the Rogue's, matching the sprite set it already borrows
 * (playerdat.cpp gives both "rogue"). A null return simply draws no silhouette, which is what the
 * inventory did for every class before this.
 */
ArtAsset *SilhouetteForClass(HeroClass heroClass)
{
	switch (heroClass) {
	case HeroClass::Warrior:
		return &SilhouetteArt[0];
	case HeroClass::Rogue:
	case HeroClass::Bard:
		return &SilhouetteArt[1];
	case HeroClass::Sorcerer:
		return &SilhouetteArt[2];
	case HeroClass::Barbarian:
		return &SilhouetteArt[3];
	case HeroClass::Monk:
		break;
	}
	return nullptr;
}
/**
 * @brief The burger icon's cell in ui\burger_menu_button.png. TWO states, not three.
 *
 * Oracool: user, 2026-08-19 - "take burger menu.png, scale it to 26x26px and use the two states as
 * burger menu icons, dead centered in the menu slot." Cut by tools\CutBurgerMenuIcon.ps1, which
 * supersedes the burger entry in CutHudStateIcons.ps1.
 *
 *   state 0  dim   - idle, and idle under the cursor (hud_menu's overlay does the hover)
 *   state 1  lit   - the popup is open
 */
constexpr Size BurgerMenuButtonSize { 26, 26 };

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
/**
 * @brief Maps a pixel's luminance onto one of the palette's 16-shade colour ramps.
 *
 * A PAL16 ramp runs LIGHT to DARK as the offset grows (engine/palette.h: "(dark blue):
 * PAL16_BLUE+14, (light red): PAL16_RED+2"), so brighter source pixels take a smaller offset.
 *
 * The range is deliberately 4..14 rather than the full 0..15. The top of a ramp is bright enough
 * to read as a lit object rather than a tinted one, and the point here is a tint. Centring on
 * roughly +9 also puts it in the same part of the ramp as the unique-item backing (PAL16_YELLOW
 * + 10, see inv.cpp's InvDrawSlotBack), which is what makes the two read as the same family.
 */
uint8_t RampIndexFromLuminance(uint8_t rampBase, uint8_t r, uint8_t g, uint8_t b)
{
	constexpr int DarkestOffset = 14;
	constexpr int LightestOffset = 4;
	const int luminance = (299 * r + 587 * g + 114 * b) / 1000;
	const int offset = DarkestOffset - luminance * (DarkestOffset - LightestOffset) / 255;
	return static_cast<uint8_t>(rampBase + offset);
}

/**
 * @brief Nudges a pixel @p strengthPercent of the way toward the ramp's shade of the same luminance.
 *
 * The difference between this and using RampIndexFromLuminance directly is the difference between
 * tinting and repainting. A full remap throws the source's own hue away and puts every pixel on one
 * 16-shade ramp - right for a flat silhouette, wrong for a large piece of modelled art, which comes
 * out looking like a single sheet of metal with the detail flattened out of it.
 *
 * Blending toward the ramp instead keeps the art's own variation and still moves the whole thing
 * into the theme's colour. The target is taken FROM the palette, so the destination is exactly the
 * gold the rest of the UI uses rather than an invented one; only the distance travelled is a knob.
 */
uint8_t TintedPaletteIndex(uint8_t rampBase, int strengthPercent, uint8_t r, uint8_t g, uint8_t b,
    std::vector<uint8_t> &cache)
{
	if (strengthPercent >= 100)
		return RampIndexFromLuminance(rampBase, r, g, b);
	const SDL_Color &target = orig_palette[RampIndexFromLuminance(rampBase, r, g, b)];
	const auto mix = [strengthPercent](uint8_t src, uint8_t dst) {
		return static_cast<uint8_t>((src * (100 - strengthPercent) + dst * strengthPercent) / 100);
	};
	return NearestGlobalPaletteIndex(mix(r, target.r), mix(g, target.g), mix(b, target.b), cache);
}

/** @brief Whether the source pixel at (@p x, @p y) is opaque. Out of bounds counts as transparent. */
bool IsOpaqueAt(const ArtAsset &asset, int x, int y)
{
	if (x < 0 || y < 0 || x >= asset.width || y >= asset.height)
		return false;
	return asset.rgba[(static_cast<size_t>(y) * asset.width + x) * 4 + 3] >= 128;
}

/**
 * @brief Fills @p asset.outline with a 1px band hugging the OUTSIDE of the figure's edge.
 *
 * Outside rather than inside so the silhouette keeps its full shape - an inner outline would eat a
 * pixel of an already small figure, and on thin parts (fingers, a weapon haft) it would eat the
 * part entirely.
 *
 * The band is one pixel of the source bitmap, so a figure touching the bitmap's own edge simply
 * has no room for an outline there. The cutter leaves a transparent margin, so in practice this
 * only matters if the art is ever recut tight to the subject.
 */
void BuildOutline(ArtAsset &asset, uint8_t outlineIndex)
{
	asset.outline.emplace(asset.width, asset.height);
	for (int y = 0; y < asset.height; y++) {
		uint8_t *row = &(*asset.outline)[Point { 0, y }];
		for (int x = 0; x < asset.width; x++) {
			// A transparent pixel with an opaque 8-neighbour. Eight rather than four, or the band
			// breaks into dashes wherever the edge runs diagonally.
			if (IsOpaqueAt(asset, x, y)) {
				row[x] = 0;
				continue;
			}
			const bool touchesFigure = IsOpaqueAt(asset, x - 1, y) || IsOpaqueAt(asset, x + 1, y)
			    || IsOpaqueAt(asset, x, y - 1) || IsOpaqueAt(asset, x, y + 1)
			    || IsOpaqueAt(asset, x - 1, y - 1) || IsOpaqueAt(asset, x + 1, y - 1)
			    || IsOpaqueAt(asset, x - 1, y + 1) || IsOpaqueAt(asset, x + 1, y + 1);
			row[x] = touchesFigure ? outlineIndex : 0;
		}
	}
}

void QuantizeAsset(ArtAsset &asset, std::optional<Rectangle> dimCircle,
    std::optional<uint8_t> tintRampBase = std::nullopt, int tintStrengthPercent = 100,
    int luminancePercent = 100)
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
			// Scaled before anything reads them, so the ramp lookup, the tint blend and the cache
			// key all see the same value. Scaling after the match would quantize the bright colour
			// and then darken the RESULT, which walks off the ramp the tint just put it on.
			const auto scale = [luminancePercent](uint8_t v) {
				return static_cast<uint8_t>(v * luminancePercent / 100);
			};
			const uint8_t r = scale(srcRow[x * 4 + 0]);
			const uint8_t g = scale(srcRow[x * 4 + 1]);
			const uint8_t b = scale(srcRow[x * 4 + 2]);
			const uint8_t a = srcRow[x * 4 + 3];

			if (a < 128) {
				brightRow[x] = 0;
				if (frameRow != nullptr) {
					frameRow[x] = 0;
					sphereRow[x] = 0;
				}
				continue;
			}

			// A tinted asset has the game, not the art, decide its colour. At full strength it
			// keeps only its shape and shading, so a neutral grey cut-out can be recoloured
			// freely; below that it keeps its own hue too and is merely pulled toward the theme.
			// Nearest-palette matching alone can do neither - given grey it finds grey.
			brightRow[x] = tintRampBase
			    ? TintedPaletteIndex(*tintRampBase, tintStrengthPercent, r, g, b, cache)
			    : NearestGlobalPaletteIndex(r, g, b, cache);

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
	if (!SidePanelArt.loadAttempted)
		LoadPixels(SidePanelArt);
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
	for (GridBezelEntry &entry : GridBezels) {
		if (!entry.art.loadAttempted)
			LoadPixels(entry.art);
	}
	// The numbered points strips. Their absence from this list was the whole of "still the
	// placeholder there" (user, 2026-08-17): DrawUnspentPointsIcon checked rgba, and nothing had
	// ever been asked to fill it - the reset list knew these assets, the load list did not.
	if (!SkillPointsFrameArt.loadAttempted)
		LoadPixels(SkillPointsFrameArt);
	if (!WaypointPanelArt.loadAttempted)
		LoadPixels(WaypointPanelArt);
	if (!WaypointIconsArt.loadAttempted)
		LoadPixels(WaypointIconsArt);
	for (ArtAsset *strip : ClassTreeStrips) {
		if (!strip->loadAttempted)
			LoadPixels(*strip);
	}
	if (!PaladinSkillIconsArt.loadAttempted)
		LoadPixels(PaladinSkillIconsArt);
	if (!AttackIconsArt.loadAttempted)
		LoadPixels(AttackIconsArt);
	for (ArtAsset &silhouette : SilhouetteArt) {
		if (!silhouette.loadAttempted)
			LoadPixels(silhouette);
	}
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
	if (!SidePanelArt.rgba.empty() && !SidePanelArt.bright)
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
	for (const GridBezelEntry &entry : GridBezels) {
		if (!entry.art.rgba.empty() && !entry.art.bright)
			return true;
	}
	if (!WaypointPanelArt.rgba.empty() && !WaypointPanelArt.bright)
		return true;
	if (!WaypointIconsArt.rgba.empty() && !WaypointIconsArt.bright)
		return true;
	for (const ArtAsset *strip : ClassTreeStrips) {
		if (!strip->rgba.empty() && !strip->bright)
			return true;
	}
	if (!PaladinSkillIconsArt.rgba.empty() && !PaladinSkillIconsArt.bright)
		return true;
	if (!AttackIconsArt.rgba.empty() && !AttackIconsArt.bright)
		return true;
	for (const ArtAsset &silhouette : SilhouetteArt) {
		if (!silhouette.rgba.empty() && !silhouette.bright)
			return true;
	}
	return false;
}

void EnsureQuantized()
{
	if (!NeedsQuantize())
		return;

	// Oracool: user request - "now that our UI theme is predominantly goldish I say we apply
	// goldish tint on the main HUD", then the burger menu with it. Half strength, not full: these
	// are modelled art with their own highlights and recesses, and a full remap onto one ramp would
	// iron those flat. Half moves them unmistakably into the theme while the stone still reads as
	// stone. One constant across all three so they cannot drift into different golds.
	//
	// The ORBS are deliberately left alone. Their red and blue are not decoration - they are how
	// you read your health and mana at a glance - and their ornament already sits warm against the
	// gold. If the frames alone are ever wanted in gold, that needs the tint applied to `frame`
	// while `sphereDim` is spared, which is a separate change from this one.
	// Oracool: user, 2026-08-19 - "dont apply any color enhancing to it. keep it 100% as delivered."
	// So NO tint ramp and NO luminance scale: the plate takes the plain nearest-palette path every
	// untinted asset takes. That is as close to "as delivered" as an 8-bit indexed engine reaches -
	// every pixel still has to become one of 256 palette entries, and the upper half is the only
	// tileset-independent part of it - but nothing here chooses a colour the art did not.
	QuantizeAsset(PlateArt, std::nullopt);
	QuantizeAsset(HealthOrbArt, Rectangle { GetHealthOrbSphereCenterLocal(), Size { GetOrbSphereRadius(), 0 } });
	QuantizeAsset(ManaOrbArt, Rectangle { GetManaOrbSphereCenterLocal(), Size { GetOrbSphereRadius(), 0 } });
	// Same 50% gold as the plate, so the menu the burger button opens matches the HUD it sits on.
	//
	// This sheet is the one place where chrome and content share pixels: each cell is a frame with
	// its pictogram baked inside, so tinting the frame necessarily tints the glyph. That is
	// survivable only because the tint is partial - at 50% each pictogram keeps half its own hue,
	// so the ten entries stay told apart by colour as well as by shape. At full strength they would
	// all collapse to one gold and the row would read as ten identical buttons.
	QuantizeAsset(MenuIconsArt, std::nullopt, PAL16_YELLOW, HudTintStrengthPercent);
	QuantizeAsset(InventoryPanelArt, std::nullopt);
	QuantizeAsset(SidePanelArt, std::nullopt);
	// Pulled toward the palette's GREY ramp at 70% (user request, 2026-08-18: "make Inv Grid Tabs
	// Buttons more Grayscale in colour to match the Limestone Theme better"). The chest art was cut
	// warm, for a panel that used to be warm; the limestone around it is neutral, and a warm tab on
	// cool stone reads as a leftover from the previous theme. Not 100%: at full strength the tab would
	// keep only its shading and lose the red that tells the OPEN page from the nine closed ones.
	QuantizeAsset(InventoryTabsArt, std::nullopt, PAL16_GRAY, 70);
	QuantizeAsset(InventorySortArt, std::nullopt);
	QuantizeAsset(TownPortalIconArt, std::nullopt);
	QuantizeAsset(BurgerMenuButtonArt, std::nullopt, PAL16_YELLOW, HudTintStrengthPercent);
	QuantizeAsset(LevelUpIconArt, std::nullopt);
	// No tint: the bezels arrived already quantised against town.pal (their stone reads as exact
	// palette entries - 30,30,30 and 61,61,61 off the grey ramp), so tinting would move art that is
	// already sitting on the colours it was authored for.
	for (GridBezelEntry &entry : GridBezels)
		QuantizeAsset(entry.art, std::nullopt);
	// No tint, same as the level-up icon these were cut from - the numeral IS the information.
	QuantizeAsset(SkillPointsFrameArt, std::nullopt);
	QuantizeAsset(WaypointPanelArt, std::nullopt);
	QuantizeAsset(WaypointIconsArt, std::nullopt);
	// No tint: the tree icons are the artwork itself, not chrome - their shapes carry the meaning.
	
	for (ArtAsset *strip : ClassTreeStrips)
		QuantizeAsset(*strip, std::nullopt);
	QuantizeAsset(PaladinSkillIconsArt, std::nullopt);
	// Same reasoning, and one more: these two sit in the HUD's skill wells next to the engine's own
	// spell icons, which are drawn untinted. A gold pass here would make the basic attack the one
	// icon on the plate that did not match the icon beside it.
	QuantizeAsset(AttackIconsArt, std::nullopt);
	// Oracool: user, 2026-08-19 - "the silhouette is goldish from the previous theme. remove the tint
	// of the silhouette." So no tint ramp: the plain nearest-palette path, the same one the bottom
	// plate took when the limestone art arrived.
	//
	// It WAS PAL16_YELLOW at full strength, which is not a tint at all but a complete repaint - at
	// 100 percent TintedPaletteIndex returns the ramp shade outright and the figure's own colours are
	// discarded. That was right when the window was gold and the silhouette had to belong to it.
	// The window is limestone now, and a gold figure behind grey stone reads as left over from a
	// theme that is gone - which is exactly what the user saw.
	//
	// The gold OUTLINE was removed separately on 2026-08-18 for the same reason. The two were the
	// last of that theme on this window.
	// No tint ramp, so the third and fourth arguments are the pass-throughs: no ramp, and the tint
	// strength is moot without one. The luminance scale is the only treatment left on this asset.
	for (ArtAsset &silhouette : SilhouetteArt)
		QuantizeAsset(silhouette, std::nullopt, std::nullopt, 100, SilhouetteLuminancePercent);
	// Walked down from +2, which read as near-white, then +6, which was still hot. The shared
	// constant records where it landed and why - and the cursor tooltip's border now reads the same
	// one, so the two thin gold edges on screen cannot drift apart.
	for (ArtAsset &silhouette : SilhouetteArt) {
		if (!silhouette.rgba.empty())
			BuildOutline(silhouette, ThemeEdgeColor);
	}

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
/**
 * @param srcLeft, srcWidth Optional horizontal window into @p src, for blitting one cell out of a
 * sprite strip. Defaulted, so callers blitting a whole surface are unaffected.
 */
void BlitHalfTransparentSkipZero(const Surface &out, const Surface &src, Point position, int srcTop, int srcBottom,
    int srcLeft = 0, int srcWidth = -1)
{
	const int width = srcWidth < 0 ? src.w() : srcWidth;
	for (int y = srcTop; y < srcBottom; y++) {
		const int dstY = position.y + y;
		if (dstY < 0 || dstY >= out.h())
			continue;
		const uint8_t *srcRow = &src[Point { 0, y }];
		uint8_t *dstRow = &out[Point { 0, dstY }];
		for (int x = 0; x < width; x++) {
			if (srcRow[srcLeft + x] == 0)
				continue;
			const int dstX = position.x + x;
			if (dstX < 0 || dstX >= out.w())
				continue;
			dstRow[dstX] = paletteTransparencyLookup[dstRow[dstX]][srcRow[srcLeft + x]];
		}
	}
}

/**
 * @brief Nearest-neighbour blit of one square strip cell INTO @p dest, whatever size that is.
 *
 * Exists because the class-tree strips are cut at 56px while the LMB/RMB wells have a net opening of
 * 46px (user, 2026-08-18: "measure how many px is the net area within the bezels and don't spill out
 * of it [...] consider this the hard boundary of these slots and don't ever spill over it"). Centring
 * a 56px cell in a 46px hole put five pixels of painting over each bezel; clipping instead would eat
 * the icon's own border. Scaling keeps the whole picture and obeys the boundary.
 *
 * Nearest-neighbour rather than a resampler: the source is already palette-quantized, so there are no
 * in-between colours to interpolate toward - a blend would have to re-quantize per pixel, per frame.
 */
void BlitStripCellScaled(const Surface &out, const Surface &src, SDL_Rect srcCell, Rectangle dest,
    bool halfTransparent = false)
{
	if (dest.size.width <= 0 || dest.size.height <= 0 || srcCell.w <= 0 || srcCell.h <= 0)
		return;
	for (int y = 0; y < dest.size.height; y++) {
		const int dstY = dest.position.y + y;
		if (dstY < 0 || dstY >= out.h())
			continue;
		const int sy = srcCell.y + y * srcCell.h / dest.size.height;
		const uint8_t *srcRow = &src[Point { 0, sy }];
		uint8_t *dstRow = &out[Point { 0, dstY }];
		for (int x = 0; x < dest.size.width; x++) {
			const uint8_t value = srcRow[srcCell.x + x * srcCell.w / dest.size.width];
			if (value == 0)
				continue; // the strips key on index 0, exactly as DrawStripIcon does
			const int dstX = dest.position.x + x;
			if (dstX < 0 || dstX >= out.w())
				continue;
			// The locked half of what DrawStripIcon has always offered, carried over so that the
			// scaled path is a drop-in for the native one - see the Rectangle overload of
			// DrawClassTreeIcon. Without it, scaling an Abilities row would have quietly made every
			// unearned skill look earned.
			dstRow[dstX] = halfTransparent ? paletteTransparencyLookup[dstRow[dstX]][value] : value;
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
	// The switch is HERE rather than at the call site, so nothing else has to know about it: the
	// belt, the wells, the XP readout and the orbs all draw from their own calls and are untouched.
	// Turning this off removes the plate and only the plate (user, 2026-08-30: "remove the hud
	// asset... dont remove the orbs").
	if (!*sgOptions.Oracool.hudPlateArt)
		return;
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

void DrawSidePanelArt(const Surface &out, Point origin)
{
	EnsureLoadedAll();
	if (SidePanelArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!SidePanelArt.bright)
		return;

	out.BlitFromSkipColorIndexZero(*SidePanelArt.bright,
	    MakeSdlRect(0, 0, SidePanelArt.width, SidePanelArt.height), origin);
}

// DrawSidePanelBackdrop is gone (user, 2026-09-02: "remove the dark transparent rectangle from all
// canvases, which we used with the limestone background"). It dimmed the inner opening on all six
// windows, and it existed because the OLD limestone was light enough to fight the text laid over it.
// The stone delivered on 2026-09-02 is dark already, so the screen was subtracting from art that no
// longer needs it.
//
// Deleted rather than left parked: it dimmed a rect measured against one particular frame, so a
// future caller would be drawing a retired artwork's opening onto whatever panel is current. It had
// already been re-measured once, hours earlier, for exactly that reason.

bool HasSidePanelArt()
{
	EnsureLoadedAll();
	return !SidePanelArt.rgba.empty();
}

void DrawInventoryTab(const Surface &out, int index, InventoryTabState state)
{
	if (index < 0 || index >= TabCount)
		return;

	EnsureLoadedAll();
	if (InventoryTabsArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!InventoryTabsArt.bright)
		return;

	// A three-frame atlas, one frame per state, NOT one column per tab: the reliquary chest is the
	// same on all ten pages, so the tab that is open is told apart by its own art rather than by a
	// number printed on it. Which is also why the frames are uniform - every state blits at the
	// same place and only the source column moves.
	//
	// The cell is larger than the 28x28 logical tab and is drawn 3px up and left of it, so the open
	// tab's raised lip overhangs its neighbours. Inactive and hover carry that overhang as
	// transparent padding, which is what lets one blit position serve all three.
	const Rectangle logical = GetTabRect(index);
	const Point origin = GetInventoryPanelRect().position
	    + Displacement { logical.position.x + TabCellOffset.deltaX, logical.position.y + TabCellOffset.deltaY };
	out.BlitFromSkipColorIndexZero(*InventoryTabsArt.bright,
	    MakeSdlRect(static_cast<int>(state) * TabCellSize.width, 0, TabCellSize.width, TabCellSize.height),
	    origin);
}

bool HasInventoryTabArt()
{
	EnsureLoadedAll();
	return !InventoryTabsArt.rgba.empty();
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
	// Two cells in the strip now, so 2 is out of range rather than the pressed state it used to be.
	if (state < 0 || state > 1)
		return;

	EnsureLoadedAll();
	if (BurgerMenuButtonArt.rgba.empty())
		return;
	EnsureQuantized();
	if (!BurgerMenuButtonArt.bright)
		return;

	// Centred, then nudged one pixel right and one down - user, 2026-08-19, looking at it in place.
	//
	// The nudge is not arbitrary and it is not fighting the centring. The cell is 33x36 and the icon
	// 26x26, so the horizontal leftover is 7: an odd number, and floor put the spare pixel on the
	// right, leaving the icon a pixel left of true centre. +1 lands it on the other side of that
	// half-pixel, which is as centred as an odd gap allows. Vertically the gap is even and this is
	// simply a pixel low, because the cell rect includes the plate's label strip along its top edge -
	// the same asymmetry the old BurgerMenuButtonRise was compensating for in the other direction
	// when the art was 27x29.
	constexpr Displacement BurgerMenuNudge { 1, 1 };
	const Rectangle cell = GetBeltSlotRect(BeltMenuSlotIndex);
	const Point position = Point {
		cell.position.x + (cell.size.width - BurgerMenuButtonSize.width) / 2,
		cell.position.y + (cell.size.height - BurgerMenuButtonSize.height) / 2
	} + BurgerMenuNudge;
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

bool HasGridBezel(Size contentSize)
{
	GridBezelEntry *entry = FindGridBezel(contentSize);
	if (entry == nullptr)
		return false;
	EnsureLoadedAll();
	return !entry->art.rgba.empty();
}

void DrawGridBezel(const Surface &out, Rectangle contentRect)
{
	GridBezelEntry *entry = FindGridBezel(contentRect.size);
	if (entry == nullptr)
		return;

	EnsureLoadedAll();
	if (entry->art.rgba.empty())
		return;
	EnsureQuantized();
	if (!entry->art.bright)
		return;

	// FRAME ONLY - four bands, with the art's own interior deliberately not blitted.
	//
	// The delivered PNG is opaque all the way through, interior included, so one whole-rect blit
	// would make every slot and both grids a solid stone recess. That would be faithful to the art
	// and would quietly delete two things this panel already does: the class silhouette drawn behind
	// the paperdoll (inv.cpp draws it BEFORE the slots and relies on their half-transparent fill to
	// show it through), and the item-quality backings behind occupied cells. Blitting the border
	// bands alone changes the frame and nothing else, which is what the pack's own integration guide
	// asks for - "replace only the outside group frame/background treatment".
	//
	// The interior colour is still there in the asset if a solid recess is ever wanted; it is one
	// blit away.
	const int w = entry->art.width;
	const int h = entry->art.height;
	const int inset = GridBezelInset;
	const Point outer = contentRect.position - Displacement { inset, inset };

	// Top and bottom run the full width; the sides fill in between them, so the corners belong to
	// the horizontal bands and no pixel is drawn twice.
	out.BlitFromSkipColorIndexZero(*entry->art.bright, MakeSdlRect(0, 0, w, inset), outer);
	out.BlitFromSkipColorIndexZero(*entry->art.bright, MakeSdlRect(0, h - inset, w, inset),
	    Point { outer.x, contentRect.position.y + contentRect.size.height });
	out.BlitFromSkipColorIndexZero(*entry->art.bright, MakeSdlRect(0, inset, inset, h - 2 * inset),
	    Point { outer.x, contentRect.position.y });
	out.BlitFromSkipColorIndexZero(*entry->art.bright, MakeSdlRect(w - inset, inset, inset, h - 2 * inset),
	    Point { contentRect.position.x + contentRect.size.width, contentRect.position.y });
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

void DrawClassSilhouette(const Surface &out, Point panelOrigin, int areaWidth, int top)
{
	EnsureLoadedAll();
	// Oracool: user request - each class shows its own figure. InspectPlayer rather than MyPlayer,
	// so inspecting another character in multiplayer shows theirs and not yours.
	ArtAsset *silhouette = SilhouetteForClass(InspectPlayer->_pClass);
	if (silhouette == nullptr || silhouette->rgba.empty())
		return;
	EnsureQuantized();
	if (!silhouette->bright)
		return;

	// Centred across the panel's width, hanging from `top`. The asset is pre-scaled by its cutter
	// to the equipment area's height, so nothing is resized here.
	//
	// Blended, NOT blitted opaquely. An opaque blit put a solid mid-grey figure on the panel that
	// dominated the window - the quantizer maps the cutter's dark fill to the nearest entry in the
	// shared upper palette, and the nearest is lighter than the half-transparent panel it lands on,
	// so the "shadow" came out brighter than its background. Blending through
	// paletteTransparencyLookup darkens whatever is behind instead of replacing it, which is what a
	// silhouette actually is - and it costs nothing extra, since the orbs' drain effect already
	// needed this exact blit.
	const Point origin { panelOrigin.x + (areaWidth - silhouette->width) / 2, panelOrigin.y + top };

	// TWO passes, not one (user request, 2026-08-18: "make the silhouette darker and its details more
	// visible"). Each pass blends the figure halfway toward its own colours, so a second one lands at
	// 3/4 rather than 1/2 - darker against the pale limestone, and the art's internal value steps
	// survive the blend proportionally instead of being washed halfway out. Still not opaque: the
	// figure has to read as a shadow behind the slots, not as a picture in front of them.
	for (int pass = 0; pass < 2; pass++)
		BlitHalfTransparentSkipZero(out, *silhouette->bright, origin, 0, silhouette->height);

	// No outline. It was a gold edge added when the body was a single washed-out pass and the shape
	// had nothing else to define it; at two passes the figure defines its own edge, and the gold read
	// as a sticker on the limestone (user request, 2026-08-18: "remove golden outline of silhouette").
	// The outline asset is still cut and still loaded - only this blit is gone - so restoring it is
	// one line.
}

/**
 * @brief Draws cell @p index of a square-cell icon strip, opaque when @p unlocked, else blended.
 *
 * Shared by the aura and Barbarian-skill sheets, which are the same asset shape and want the same
 * locked treatment - blended rather than drawn from a second, greyed copy of the art. The Spells
 * sheet greys unlearned entries through SetSpellTrans, which works because spell icons are one
 * palette ramp; these are full-colour paintings with no ramp to remap, so halving them into the
 * panel is the honest equivalent - visible, clearly inert, and no second asset to keep in step.
 */
void DrawStripIcon(const Surface &out, ArtAsset &asset, Point origin, int index, bool unlocked)
{
	EnsureLoadedAll();
	if (asset.rgba.empty())
		return;
	EnsureQuantized();
	if (!asset.bright)
		return;

	// One square cell per entry, so the cell size is the strip's height - derived rather than
	// hardcoded, so a recut at a different icon size still indexes correctly.
	const int cell = asset.height;
	const int cells = asset.width / cell;
	if (index < 0 || index >= cells)
		return;

	const SDL_Rect src = MakeSdlRect(index * cell, 0, cell, cell);
	if (unlocked) {
		out.BlitFromSkipColorIndexZero(*asset.bright, src, origin);
		return;
	}
	BlitHalfTransparentSkipZero(out, *asset.bright, origin, 0, cell, src.x, src.w);
}

Size StripIconSize(ArtAsset &asset)
{
	EnsureLoadedAll();
	// {0,0} means "no custom art", and it has to keep meaning exactly that.
	//
	// Bug postmortem (2026-08-15): this briefly returned the PLATE's size instead, so that the
	// Abilities window's rows kept a sensible text column after the four icon sheets were removed.
	// That broke the HUD, because GetAttackIconSize feeds the LMB/RMB wells too, and those assert
	// their art is exactly SkillWellIconSize (38x38) or absent. The plate is 37x38 - one pixel
	// narrower - so the assert fired the moment a game started.
	//
	// The mistake was giving one function two meanings. It reports what the ART is; a caller that
	// wants to fall back to the plate asks GetSkillIconPlateSize for itself, which is what the
	// Abilities window now does. The wells get their 0 back and draw nothing, as they always did.
	if (asset.rgba.empty())
		return { 0, 0 };
	return { asset.height, asset.height };
}

/**
 * @brief Draws the vanilla empty spell-icon plate - the yellow square every skill now sits on.
 *
 * Oracool: user request (2026-08-15) - "take the vanilla yellow square background of skills and
 * apply it behind every skill from now on [...] we need consistency in skills icons. Right now we
 * dont have it."
 *
 * No new asset was needed. The plate is frame 26 of data\spelli2, which the engine already maps
 * SpellID::Null to (SpellITbl[0] == 26) as its empty-slot icon, and the yellow is not painted in -
 * it comes from SetSpellTrans(SpellType::Skill), the same recolour the speedbook uses to say
 * "skill" rather than "spell". So this is the game's own plate through the game's own TRN.
 *
 * DrawSmallSpellIcon anchors at the BOTTOM-left, unlike the strips above, which is what the height
 * term corrects for.
 */
Size GetSkillIconPlateSize()
{
	return GetSmallSpellIconSize();
}

// Each tint is a SpellType chosen for the RAMP its translation table lands on, not for what the
// type is called: Scroll is PAL16_BEIGE (the pink), Skill is the identity and so the vanilla
// yellow. No scroll is involved or implied - see SkillPlateTint for why these were the ramps left
// to choose from.
void ApplyPlateTint(SkillPlateTint tint)
{
	switch (tint) {
	case SkillPlateTint::Green:
		// The injected PAL8_GREEN ramp (see LoadPalette), which replaced the pink the user never
		// warmed to - "i dont like the pink" (2026-08-15). Belzebub's green was the proof the
		// palette could be taught a colour it never shipped.
		SetSpellTransGreen();
		break;
	case SkillPlateTint::Grey:
		// The DARKER grey, not SpellType::Invalid's pale one - "make the inactive skill background
		// darker gray" (user, 2026-08-15). Invalid's table maps ramps 1:1 onto grey, which read as
		// merely faded next to the pink plates; this one shifts four shades down the ramp.
		SetSpellTransDarkGrey();
		break;
	case SkillPlateTint::Pink:
		// "Unable to perform right now" (user, 2026-08-16). SpellType::Scroll's table is the engine's
		// own mapping onto PAL16_BEIGE - the ramp the user calls pink - so no new table is needed.
		SetSpellTrans(SpellType::Scroll);
		break;
	case SkillPlateTint::Red:
		// "Unlocked but unspent" (user, 2026-08-17). PAL16_RED is one of the game's own ramps, so
		// unlike the green this needs no palette injection.
		SetSpellTransRed();
		break;
	case SkillPlateTint::Yellow:
		SetSpellTrans(SpellType::Skill);
		break;
	}
}

void ResetHudArtCaches()
{
	// Phase 0.8: the art-iteration hot-reload. Every cached PNG asset is dropped back to its
	// never-loaded state; the next draw call re-reads the file from disk and re-quantizes against
	// the palette, exactly as on first use. Editing a PNG and pressing the reload debug command
	// shows the change in seconds instead of restart-per-tweak.
	const auto reset = [](ArtAsset &asset) {
		asset.rgba.clear();
		asset.width = 0;
		asset.height = 0;
		asset.loadAttempted = false;
		asset.bright.reset();
		asset.frame.reset();
		asset.sphereDim.reset();
		asset.outline.reset();
		// The per-frame crop cache, which this used to leave behind (external audit of v1.9.88,
		// finding 4). StripCellInset only builds it when it is EMPTY and then indexes it with an
		// index it has validated against the NEWLY loaded frame count - so reloading a strip that
		// grew read past the end of a vector sized for the old one. That is not hypothetical here:
		// v1.9.88 replaced five class strips and every one of them got longer (30 -> 49, 30 -> 49,
		// 30 -> 48, 21 -> 39, 21 -> 39).
		//
		// Even at an unchanged frame count it was wrong, just quietly: the insets are measured from
		// the art's transparent margins, so an edited strip kept the old crop and drew misaligned.
		asset.cellInsets.clear();
	};
	reset(PlateArt);
	reset(HealthOrbArt);
	reset(ManaOrbArt);
	reset(MenuIconsArt);
	reset(InventoryPanelArt);
	reset(SidePanelArt);
	reset(InventoryTabsArt);
	reset(InventorySortArt);
	reset(TownPortalIconArt);
	reset(BurgerMenuButtonArt);
	reset(LevelUpIconArt);
	for (GridBezelEntry &entry : GridBezels)
		reset(entry.art);
	reset(SkillPointsFrameArt);
	reset(WaypointPanelArt);
	reset(WaypointIconsArt);
	for (ArtAsset *strip : ClassTreeStrips)
		reset(*strip);
	reset(PaladinSkillIconsArt);
	reset(AttackIconsArt);
	for (ArtAsset &silhouette : SilhouetteArt)
		reset(silhouette);
}

void DrawSkillIconPlate(const Surface &out, Point origin, SkillPlateTint tint)
{
	const Size plate = GetSmallSpellIconSize();
	if (plate.height <= 0)
		return;
	ApplyPlateTint(tint);
	DrawSmallSpellIcon(out, { origin.x, origin.y + plate.height - 1 }, SpellID::Null);
}

/**
 * @brief The plate, then whatever custom art exists on top of it.
 *
 * Both halves are deliberate. The plate goes down for EVERY skill so the sheets read as one set -
 * which they did not while the aura, Barbarian, Paladin and attack strips were four unrelated
 * paintings. DrawStripIcon is then a no-op when its sheet is missing, which is what lets the user's
 * new icons drop in with no code change: ship the strip and it appears on the plate.
 */
// Pink default, matching the header's exported defaults (user report, 2026-08-15: the wells showed
// the basic attack on yellow - the sheets all pass a tint explicitly, so a default only ever fires
// on a well path, and the wells are Skills-sheet content).
void DrawIconOnPlate(const Surface &out, ArtAsset &asset, Point origin, int index, bool unlocked,
    SkillPlateTint tint = SkillPlateTint::Green)
{
	DrawSkillIconPlate(out, origin, tint);
	DrawStripIcon(out, asset, origin, index, unlocked);
}

void DrawPaladinSkillIcon(const Surface &out, Point origin, int skillIndex, bool unlocked, SkillPlateTint tint)
{
	DrawIconOnPlate(out, PaladinSkillIconsArt, origin, skillIndex, unlocked, tint);
}

Size GetPaladinSkillIconSize()
{
	return StripIconSize(PaladinSkillIconsArt);
}

void DrawClassTreeIcon(const Surface &out, Point origin, HeroClass heroClass, int skillIndex,
    bool unlocked, SkillPlateTint tint)
{
	DrawIconOnPlate(out, TreeStripFor(heroClass), origin, skillIndex, unlocked, tint);
}

Rectangle SkillPointsNumberRect(Point origin)
{
	// Dead centre of the frame, at the size the user specified (2026-08-20: "in its center area in
	// 40x39px area dead center in the icon"). Derived from PointsIconSize rather than written as a
	// second pair of literals, so moving the frame's size cannot leave the number off-centre - the
	// exact drift the cut script's own comment warns about.
	constexpr Size NumberArea { 40, 39 };
	// Four pixels down (user, 2026-08-20), an OPTICAL correction rather than a geometry fix: the
	// box is still centred on the frame, but the glyphs do not fill their line box - the font's
	// ascent leaves more air above a digit than its descent leaves below - so a mathematically
	// centred box reads high. Applied here, at the one place the rect is defined, so the draw and
	// anything that later measures this box cannot disagree about where the number actually sits.
	constexpr int OpticalDrop = 4;
	return { { origin.x + (PointsIconSize.width - NumberArea.width) / 2,
		         origin.y + (PointsIconSize.height - NumberArea.height) / 2 + OpticalDrop },
		NumberArea };
}

bool DrawUnspentPointsIcon(const Surface &out, Point origin, int count, bool lit)
{
	// One frame now, with the count DRAWN into it by the caller - not one of 99 pictures whose
	// numeral was part of the art. @p count survives in the signature only so a caller still has to
	// have one in hand to draw the frame at all.
	(void)count;
	(void)lit;
	ArtAsset &asset = SkillPointsFrameArt;
	EnsureLoadedAll();
	EnsureQuantized();
	// Report whether a draw can actually HAPPEN, not merely whether pixels were read. The first
	// version tested rgba alone, and when the quantize list had not been taught these assets it
	// claimed the draw, suppressed the caller's placeholder, and painted nothing - "now no picture
	// loads at all. i cant tell how many point i have" (user, 2026-08-17). hud_art keeps THREE
	// hand-maintained per-asset lists (load, quantize, reset); testing the END of that pipeline is
	// what makes missing any of them degrade to the placeholder instead of to blank.
	if (asset.rgba.empty() || !asset.bright)
		return false;
	// Index 0: the file is one 64x64 picture, so the "strip" has exactly one frame. Drawn through
	// DrawStripIcon anyway rather than a bespoke blit, because that is where the quantize/bright
	// pipeline this function just tested actually lands the pixels.
	DrawStripIcon(out, asset, origin, /*index=*/0, /*unlocked=*/true);
	return true;
}

bool DrawSkillWellBacking(const Surface &out, Rectangle well)
{
	// The same 64x64 frame the stat-point and skill-point counters wear, borrowed as the backing
	// for the two skill wells (user, 2026-08-30: "use the stats/skills points backing behind the
	// LMB/RMB skills. i wanna see how it fits there").
	//
	// CENTRED on the well rather than aligned to it, and the numbers say why: the well rects are
	// the plate's OPENINGS - 50x51 after scaling - while the frame is 64x64, so it overhangs by
	// seven pixels a side. Centring puts the skill icon exactly where the counter's numeral sits,
	// which is what makes the two read as the same component.
	//
	// Same end-of-pipeline test as DrawUnspentPointsIcon: report whether a draw can HAPPEN, so a
	// missing entry in any of hud_art's three hand-maintained asset lists degrades to "no backing"
	// rather than to a silent blank where the caller thought it had drawn one.
	ArtAsset &asset = SkillPointsFrameArt;
	EnsureLoadedAll();
	EnsureQuantized();
	if (asset.rgba.empty() || !asset.bright)
		return false;

	const Point origin { well.position.x + (well.size.width - PointsIconSize.width) / 2,
		well.position.y + (well.size.height - PointsIconSize.height) / 2 };
	DrawStripIcon(out, asset, origin, /*index=*/0, /*unlocked=*/true);
	return true;
}

// Defined further down, next to the other scaled drawing. Declared here because the Rectangle
// overload of DrawClassTreeIcon below needs it, and moving the definitions up would separate them
// from BlitStripCellScaled, which is the thing they are all about.
void DrawStripIconScaledTo(const Surface &out, ArtAsset &asset, Rectangle dest, int index,
    bool unlocked = true);
void DrawClassTreeIconScaledTo(const Surface &out, Rectangle dest, HeroClass heroClass, int skillIndex,
    bool unlocked = true);

void DrawBeltBacking(const Surface &out)
{
	// The same frame the wells wear, sized down to a belt cell and repeated across the row (user,
	// 2026-08-30: "scale this backing a bit in order to use it as a backing for the belt. we need 6
	// sized-down pieces of it aligned next to each other").
	//
	// SIX cells, which is the row's own count - Menu, the four item slots, Town Portal - and not the
	// four that can hold an item. The row is what is being decorated, so BeltVisibleSlotCount is the
	// right question; using IsRealBeltItemSlot here would frame the potions and leave the two
	// buttons at either end bare, which is the opposite of "aligned next to each other".
	//
	// Scaled, unlike the wells: a belt cell is about 33x35 against the art's native 64x64, so drawn
	// unscaled each frame would cover its neighbours entirely. The wells kept their overhang because
	// nothing sits beside them; these cannot.
	//
	// Positioned from GetBeltSlotRect, so the frames inherit the plate's own cell spacing and stay
	// aligned with the items drawn into them by DrawInvBelt - which is the one thing that would look
	// broken if this invented its own row geometry.
	ArtAsset &asset = SkillPointsFrameArt;
	EnsureLoadedAll();
	EnsureQuantized();
	if (asset.rgba.empty() || !asset.bright)
		return;

	for (int i = 0; i < BeltVisibleSlotCount; i++)
		DrawStripIconScaledTo(out, asset, GetBeltSlotRect(i), /*index=*/0, /*unlocked=*/true);
}

void DrawClassTreeIcon(const Surface &out, Rectangle cell, HeroClass heroClass, int skillIndex,
    bool unlocked, SkillPlateTint tint)
{
	// Bug (fixed 2026-08-17, user: "Fix the damn background of the skills. it has been like this
	// forever. Dont you see it. Make it fit the skill picture.").
	//
	// DrawSkillIconPlate sizes itself from GetSmallSpellIconSize() - the vanilla 37x38 spell icon -
	// while a tree cell is 56x56 and the class strips draw at their own natural size on top. So the
	// plate has been ~19px too narrow and ~18px too short under every tree icon since the plates went
	// in, which is why it never read as a backing and always read as a smaller square peeking out.
	//
	// The scaling lives in spell_icons.cpp because the plate list and the translation table are both
	// file-local there; reaching for them from here would have meant exporting two internals to fix
	// one drawing call.
	ApplyPlateTint(tint);
	DrawSmallSpellIconScaledTo(out, cell);
	// SCALED to the cell, like the plate under it (user, 2026-08-27: "i want them to completely
	// overlap the background"). It used to draw at native size, which was indistinguishable from
	// filling the cell for as long as every icon ran edge to edge - and stopped being so the moment
	// a batch arrived with a transparent surround inside its cell.
	DrawStripIconScaledTo(out, TreeStripFor(heroClass), cell, skillIndex, unlocked);
}

/**
 * @brief The uniform transparent border around cell @p index, measured once and remembered.
 *
 * See ArtAsset::cellInsets. Reads the loaded RGBA rather than the quantised surface, because that is
 * where alpha still exists - the quantiser has already collapsed it to "index 0 or not" by then.
 */
uint8_t StripCellInset(ArtAsset &asset, int index, int cell)
{
	if (cell <= 0 || asset.rgba.empty())
		return 0;
	const int cells = asset.width / cell;
	if (index < 0 || index >= cells)
		return 0;

	// Rebuilt whenever it does not MATCH the strip in front of us, not merely when it is missing.
	// "Empty" alone trusted every caller to have cleared it first, and the index below is validated
	// against the strip's current frame count - so a cache built for a shorter strip was indexed
	// past its end. ResetHudArtCaches now clears it too, but this is the check that does not depend
	// on anyone remembering to.
	if (asset.cellInsets.size() != static_cast<size_t>(cells)) {
		asset.cellInsets.assign(static_cast<size_t>(cells), 0);
		for (int c = 0; c < cells; c++) {
			// Grow the ring outward while every pixel on it is transparent. Stops one short of
			// half the cell, so a fully transparent cell reports a border rather than nothing.
			int inset = 0;
			while (inset < cell / 2) {
				bool ringClear = true;
				for (int i = inset; i < cell - inset && ringClear; i++) {
					const int xs[] = { inset, cell - 1 - inset, i, i };
					const int ys[] = { i, i, inset, cell - 1 - inset };
					for (int k = 0; k < 4; k++) {
						const size_t px = (static_cast<size_t>(ys[k]) * asset.width
						                      + static_cast<size_t>(c * cell + xs[k]))
						    * 4;
						if (px + 3 < asset.rgba.size() && asset.rgba[px + 3] != 0) {
							ringClear = false;
							break;
						}
					}
				}
				if (!ringClear)
					break;
				inset++;
			}
			asset.cellInsets[static_cast<size_t>(c)] = static_cast<uint8_t>(inset);
		}
	}
	return asset.cellInsets[static_cast<size_t>(index)];
}

void DrawStripIconScaledTo(const Surface &out, ArtAsset &asset, Rectangle dest, int index, bool unlocked)
{
	EnsureLoadedAll();
	if (asset.rgba.empty())
		return;
	EnsureQuantized();
	if (!asset.bright)
		return;
	// One square cell per entry, derived from the strip's height, exactly as DrawStripIcon does - a
	// recut at another icon size stays correct here for the same reason it does there.
	const int cell = asset.height;
	const int cells = cell > 0 ? asset.width / cell : 0;
	if (index < 0 || index >= cells)
		return;

	// The ARTWORK is scaled onto the plate, not the cell it was delivered in. An icon with a
	// transparent surround would otherwise be scaled surround and all, so it would sit inside the
	// plate with a ring of plate showing while an edge-to-edge icon beside it covered the same plate
	// completely - see ArtAsset::cellInsets.
	//
	// This costs nothing in quality: the cell was already being resampled to reach `dest`, so all
	// that changes is which source rectangle goes through the same single scale.
	const int inset = StripCellInset(asset, index, cell);
	const int src = cell - 2 * inset;
	if (src <= 0)
		return;
	BlitStripCellScaled(out, *asset.bright, MakeSdlRect(index * cell + inset, inset, src, src), dest,
	    !unlocked);
}

void DrawClassTreeIconScaledTo(const Surface &out, Rectangle dest, HeroClass heroClass, int skillIndex,
    bool unlocked)
{
	DrawStripIconScaledTo(out, TreeStripFor(heroClass), dest, skillIndex, unlocked);
}

Size GetClassTreeIconSize(HeroClass heroClass)
{
	return StripIconSize(TreeStripFor(heroClass));
}

bool TryDrawSkillSpellIcon(const Surface &out, Rectangle well, SpellID spell, SkillPlateTint tint)
{
	// FILLS @p well, plate and icon both, rather than centring naturally-sized art in it (user,
	// 2026-08-18: "make backgrounds of skills/auras fit and FILL the net part of the RMB/LMB slots
	// [...] stay confined within the net area [...] avoid overlapping the bezel"). The callers pass
	// the wells' 46x46 net opening; the class strips are cut at 56 and the Paladin strip at 38, so
	// neither one matched it - one spilled over the bezel, the other left a moat of plate.
	//
	// Scaled, not clipped: clipping a 56px cell to 46 would shave the icon's own painted border off,
	// which is the part that makes it read as an icon at all.
	// The CLASS TREE's own icon first (user, 2026-08-18: "the new Abilities sheets came with their
	// own set of icons and we must use them with the HUD ui now"). The well used to ask the retired
	// Skills sheet's seven-icon Paladin strip, so a skill readied off a tree page wore one picture in
	// the page you clicked and a different one in the well it landed in.
	//
	// Scoped to the player's own class, since spell ids are global and tree rows are not.
	const ClassTreeSkill treeSkill = ClassTreeSkillForSpell(InspectPlayer->_pClass, spell);
	if (treeSkill != ClassTreeSkill::None) {
		ApplyPlateTint(tint);
		// ...unless the spell is a LEGACY one that merely also has a tree row - Fire Bolt, Lightning,
		// Nova, Teleport and the rest of the Sorceress's two pages (user, 2026-09-03: "please use
		// legacy icons for legacy spells everywhere. use new icon assets only for spells we introduce
		// into the game"). Those wore the tree strip's picture here and the engine's own in the
		// speedbook, which is the same spell with two faces. The strip is for what this fork added.
		//
		// Still through the tint's ramp, so a readied legacy spell keeps the well's state colour.
		if (IsLegacySpell(spell)) {
			DrawSmallSpellIconFittedTo(out, well, spell);
			return true;
		}
		DrawSmallSpellIconFittedTo(out, well);
		DrawClassTreeIconScaledTo(out, well, InspectPlayer->_pClass, ClassTreeIconIndex(treeSkill));
		return true;
	}

	// The old strip stays as the fallback, not as the answer. It still covers the seven Paladin
	// skills when they are readied by a class whose tree does not list them - the debug commands can
	// do that - and deleting it would trade a wrong icon for no icon.
	const std::optional<PaladinSkill> skill = PaladinSkillForSpell(spell);
	if (!skill.has_value())
		return false;
	// Always drawn as unlocked: this is the readied-spell path, and a spell cannot be readied unless
	// the player has it. The dimmed variant belongs to the Abilities window's own rows, where it says
	// what has not been earned yet.
	ApplyPlateTint(tint);
	DrawSmallSpellIconFittedTo(out, well);
	DrawStripIconScaledTo(out, PaladinSkillIconsArt, well, GetPaladinSkillIconIndex(*skill));
	return true;
}

void DrawLegacySpellIconInCell(const Surface &out, Rectangle cell, SpellID spell, SkillPlateTint tint)
{
	ApplyPlateTint(tint);
	DrawSmallSpellIconFittedTo(out, cell, spell);
}

bool TryDrawSkillSpellIconLarge(const Surface &out, Point bottomLeft, SpellID spell, SkillPlateTint tint)
{
	// Tree icon first, exactly as the small well does - the two must not disagree about what a
	// readied skill looks like, or the speedbook and the HUD would each be right on their own terms.
	const ClassTreeSkill treeSkill = ClassTreeSkillForSpell(InspectPlayer->_pClass, spell);
	const std::optional<PaladinSkill> skill = PaladinSkillForSpell(spell);
	if (treeSkill == ClassTreeSkill::None && !skill.has_value())
		return false;
	// A legacy spell with a tree row keeps the engine's own large frame - see the small well for the
	// request. Returning false hands it back to the caller's ordinary DrawLargeSpellIcon path, which
	// is exactly the drawing that was wanted, with the caller's own ramp.
	if (IsLegacySpell(spell))
		return false;

	// The large empty plate through the tint's ramp - the same square the speedbook draws for every
	// other entry, so the seven skills sit in the grid rather than on their own kind of card.
	ApplyPlateTint(tint);
	DrawLargeSpellIcon(out, bottomLeft, SpellID::Null);

	// The strip icon is 38px against the 56px plate; centred, with the plate's bottom-left anchor
	// converted to the strip's top-left.
	const bool fromTree = treeSkill != ClassTreeSkill::None;
	Size iconSize = fromTree ? GetClassTreeIconSize(InspectPlayer->_pClass) : GetPaladinSkillIconSize();
	if (iconSize.width == 0)
		return true; // no strip shipped: the plate alone is still better than a blank tile
	const Point iconOrigin {
		bottomLeft.x + (SPLICONLENGTH - iconSize.width) / 2,
		bottomLeft.y - SPLICONLENGTH + 1 + (SPLICONLENGTH - iconSize.height) / 2
	};
	if (fromTree) {
		// FILLS the plate, like the wells do (user, 2026-08-27). A tree cell is the same 56px as
		// this plate, so drawing it at native size looked like full coverage - and was, right up
		// until an icon arrived with a transparent surround baked into its cell, which then showed
		// as a ring of plate here and nowhere else. Routed through the scaled path so the artwork
		// decides the size rather than the cell it was delivered in.
		//
		// The legacy 38px fallback below is deliberately NOT changed: it is a genuinely smaller icon
		// centred on a larger plate, which is a different thing from one that only looks smaller.
		const Rectangle plate {
			Point { bottomLeft.x, bottomLeft.y - SPLICONLENGTH + 1 },
			Size { SPLICONLENGTH, SPLICONLENGTH }
		};
		DrawClassTreeIconScaledTo(out, plate, InspectPlayer->_pClass, ClassTreeIconIndex(treeSkill));
	} else {
		DrawStripIcon(out, PaladinSkillIconsArt, iconOrigin, GetPaladinSkillIconIndex(*skill), /*unlocked=*/true);
	}
	return true;
}

void DrawAttackIcon(const Surface &out, Point origin, int iconIndex, bool active, SkillPlateTint tint)
{
	DrawIconOnPlate(out, AttackIconsArt, origin, iconIndex, active, tint);
}

void DrawAttackIconScaledTo(const Surface &out, Rectangle well, int iconIndex, bool active, SkillPlateTint tint)
{
	ApplyPlateTint(tint);
	DrawSmallSpellIconFittedTo(out, well);
	// The dim pass the Point overload gets from DrawStripIcon has no scaled twin, and the wells never
	// need one: a well shows what its button does right now, and that is always the active state.
	if (!active) {
		DrawStripIcon(out, AttackIconsArt,
		    { well.position.x + (well.size.width - StripIconSize(AttackIconsArt).width) / 2,
		        well.position.y + (well.size.height - StripIconSize(AttackIconsArt).height) / 2 },
		    iconIndex, /*unlocked=*/false);
		return;
	}
	DrawStripIconScaledTo(out, AttackIconsArt, well, iconIndex);
}

void DrawClassTreeSkillInWell(const Surface &out, Rectangle well, HeroClass heroClass, int skillIndex,
    SkillPlateTint tint)
{
	ApplyPlateTint(tint);
	DrawSmallSpellIconFittedTo(out, well);
	DrawClassTreeIconScaledTo(out, well, heroClass, skillIndex);
}

Size GetAttackIconSize()
{
	return StripIconSize(AttackIconsArt);
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
