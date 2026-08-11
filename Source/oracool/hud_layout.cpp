#include "oracool/hud_layout.h"

#include "oracool/game_clock.h"

#include <string>

#include <fmt/format.h>

#include "control.h"
#include "engine/backbuffer_state.hpp"
#include "inv.h"
#include "oracool/event_log.h"
#include "oracool/oracool.h"
#include "utils/ui_fwd.h"

namespace devilution::oracool {

namespace {

// Oracool: HUD art pass (2026-08-11) - the middle HUD's geometry derives from the user's third
// plate design ("Middle HUD v2.png", 1536x1024 source, plate cropped to 1498x260): two large skill
// wells flanking six cells, each cell carrying its own baked-in label (LMB / Menu / 1-4 / Portal /
// RMB) plus baked-in hamburger and portal-ring icons for the two button cells. No XP groove - the
// user dropped the XP bar in favour of the existing XP counter under the mini-map.
//
// The constants below are pixel measurements from the source (scratchpad measure_hud_v2*.ps1),
// scaled to 356px on screen - the previous 324px plate plus the 10% the user asked for, since the
// old one read as cramped. That gives ~33px belt cells and ~49px skill wells; the RMB readied-spell
// indicator therefore uses the engine's SMALL (37x38) spell icon rather than the 56px large one
// (see GetRmbSkillButtonDrawPosition and DrawSpell).
constexpr Size PlateSrcSize { 1505, 272 };
constexpr int PlateScreenWidth = 356;
constexpr int PlateBottomMargin = 0; // user: "flush with the bottom"

constexpr int ScalePlate(int v)
{
	return v * PlateScreenWidth / PlateSrcSize.width;
}

/**
 * @brief Scales a source-space rect by scaling its EDGES, not its position and size separately.
 *
 * Oracool: bug postmortem (2026-08-11) - scaling position and size independently truncates twice,
 * so a rect could come out a pixel short on the right/bottom. That showed up as a thin black strip
 * down the right side of the Menu/Portal click highlight, and left the skill wells a pixel narrower
 * than the art's actual opening. Deriving the far edge from the source's far edge keeps every rect
 * flush with the art and makes adjacent cells tile without seams.
 */
constexpr Rectangle ScalePlateRect(Rectangle src)
{
	const int left = ScalePlate(src.position.x);
	const int top = ScalePlate(src.position.y);
	const int right = ScalePlate(src.position.x + src.size.width);
	const int bottom = ScalePlate(src.position.y + src.size.height);
	return { { left, top }, { right - left, bottom - top } };
}

constexpr Size PlateScreenSize { PlateScreenWidth, ScalePlate(PlateSrcSize.height) };

// Source-space rects measured from the PNG (plate-local coordinates, i.e. after the (16,337) crop
// the prep script applies - that origin is the art's own alpha>=128 bounding box). The two skill
// wells are both taller and wider than the belt cells, and sit higher up the plate.
constexpr Rectangle LmbWellSrc { { 25, 26 }, { 208, 217 } };
constexpr Rectangle RmbWellSrc { { 1271, 26 }, { 207, 217 } };
constexpr int BeltCellSrcX[6] = { 263, 434, 599, 766, 931, 1099 };
constexpr int BeltCellSrcY = 91;
constexpr Size BeltCellSrcSize { 139, 150 };

// Oracool: HUD art pass, orb v2 (2026-08-11) - the user's second orb designs, replacing the first
// pair whose gargoyle/angel read as too large against their spheres. Now the sphere dominates and
// the ornament (dragon head, winged mask) is a pedestal beneath it. Corner-anchored flush, exactly
// as before.
//
// Sizes and circles come from the processed assets (scratchpad OrbPrep.cs). Both are scaled by
// their GLASS sphere rather than the coloured core inside it: the core is a different fraction of
// the glass in each orb, so scaling by it left health at 104px and mana at 128px - visibly
// mismatched. Scaled by the glass, both spheres are 88px across (matching the flask orbs they
// replaced) and both compositions are 96px tall. The sphere circle is what hud_art.cpp's drain
// effect dims and refills, so it must track the glass, not the core, or the rim would never dim.
constexpr Size HealthOrbScreenSize { 105, 96 };
constexpr Size ManaOrbScreenSize { 97, 96 };
constexpr Point HealthOrbSphereCenter { 53, 44 }; // rect-local
constexpr Point ManaOrbSphereCenter { 49, 44 };   // rect-local
constexpr int OrbSphereRadiusPx = 44;

// Oracool: user request (2026-08-11) - the orbs were pinned to the screen's bottom corners, which
// stranded them far out to the sides on a wide canvas. They now sit flush against the plate's
// outer edges instead, so the whole HUD reads as one centred unit at any width. Both are
// bottom-aligned with the plate (which is itself flush with the screen bottom), leaving the
// spheres rising above the shorter plate.
constexpr int OrbGapFromPlate = 0;

} // namespace

Rectangle GetHealthOrbRect()
{
	const Rectangle plate = GetMiddleHudRect();
	const int bottom = plate.position.y + plate.size.height;
	return { { plate.position.x - OrbGapFromPlate - HealthOrbScreenSize.width, bottom - HealthOrbScreenSize.height }, HealthOrbScreenSize };
}

Rectangle GetManaOrbRect()
{
	const Rectangle plate = GetMiddleHudRect();
	const int bottom = plate.position.y + plate.size.height;
	return { { plate.position.x + plate.size.width + OrbGapFromPlate, bottom - ManaOrbScreenSize.height }, ManaOrbScreenSize };
}

Point GetHealthOrbSphereCenterLocal()
{
	return HealthOrbSphereCenter;
}

Point GetManaOrbSphereCenterLocal()
{
	return ManaOrbSphereCenter;
}

int GetOrbSphereRadius()
{
	return OrbSphereRadiusPx;
}

Rectangle GetMiddleHudRect()
{
	return { { (gnScreenWidth - PlateScreenSize.width) / 2, gnScreenHeight - PlateScreenSize.height - PlateBottomMargin }, PlateScreenSize };
}

Rectangle GetLmbSkillButtonRect()
{
	const Rectangle scaled = ScalePlateRect(LmbWellSrc);
	return { GetMiddleHudRect().position + Displacement { scaled.position.x, scaled.position.y }, scaled.size };
}

Rectangle GetRmbSkillButtonRect()
{
	const Rectangle scaled = ScalePlateRect(RmbWellSrc);
	return { GetMiddleHudRect().position + Displacement { scaled.position.x, scaled.position.y }, scaled.size };
}

Rectangle GetLevelUpIconRect()
{
	// Hangs directly under the clock's ':' rather than under the clock's box. The colon is a fixed
	// visual anchor; the box is not, because the text is left-aligned inside it and changes width
	// with the hour and the 12/24-hour option. GetClockColonCentreX measures the real glyphs.
	//
	// Lives here rather than in game_clock.cpp because control.cpp needs the rect for hit-testing
	// as well as drawing - the two must never disagree.
	constexpr int ClockMargin = 8;
	constexpr int ClockHeight = 20;
	constexpr int GapBelowClock = 2;
	return { { GetClockColonCentreX() - LevelUpIconSize.width / 2, ClockMargin + ClockHeight + GapBelowClock },
		LevelUpIconSize };
}

Rectangle GetBeltSlotRect(int visibleIndex)
{
	const Rectangle scaled = ScalePlateRect(Rectangle { Point { BeltCellSrcX[visibleIndex], BeltCellSrcY }, BeltCellSrcSize });
	return { GetMiddleHudRect().position + Displacement { scaled.position.x, scaled.position.y }, scaled.size };
}

void MigrateHiddenBeltSlots(Player &player)
{
	if (!IsSinglePlayer())
		return;

	const int hiddenIndices[] = { BeltMenuSlotIndex, BeltTownPortalSlotIndex, 6, 7 };
	bool migratedAny = false;

	for (int hidden : hiddenIndices) {
		Item &source = player.SpdList[hidden];
		if (source.isEmpty())
			continue;

		Item item = source;
		bool placed = false;
		for (int i = 1; i <= 4; i++) {
			if (player.SpdList[i].isEmpty()) {
				player.SpdList[i] = item;
				placed = true;
				break;
			}
		}
		if (!placed)
			placed = AutoPlaceItemInInventory(player, item, true);

		if (placed) {
			source.clear();
			migratedAny = true;
		} else {
			LogEvent(fmt::format("Could not migrate {:s} out of repurposed belt slot {:d} for the new HUD - left in place", std::string(item.getName()), hidden + 1));
		}
	}

	if (migratedAny) {
		player.CalcScrolls();
		RedrawComponent(PanelDrawComponent::Belt);
	}
}

} // namespace devilution::oracool
