#include "oracool/hud_layout.h"

#include "oracool/game_clock.h"

#include <string>

#include <fmt/format.h>

#include "control.h"
#include "engine/backbuffer_state.hpp"
#include "inv.h"
#include "oracool/event_log.h"
#include "oracool/hud_menu.h"
#include "oracool/oracool.h"
#include "oracool/xp_counter.h"
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
// The art's own 1505x274 band in hud-plate-v3.png - see tools\CutHudPlate.ps1, which asserts the
// crop against the master before cutting. The height went 272 -> 274 with v3 and moves NOTHING:
// ScalePlate divides by width only, so no source rect shifts, and 272 and 274 both scale to a
// 64-pixel PlateScreenSize.height. The width is the load-bearing number and it did not change,
// which is exactly why v3 was a drop-in where the limestone package was not.
constexpr Size PlateSrcSize { 1505, 274 };
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
/**
 * @brief Thickness of the metal bezel drawn around a skill well's opening, in screen pixels.
 *
 * The well rects above are the OPENINGS - deliberately, because that is what the readied-spell
 * icon has to fit inside. The level-up icon is sized to the button's full footprint instead, so it
 * needs the opening plus this.
 */
constexpr int SkillWellBezelPx = 5;

// The level-up icon is cut to the LMB button's full footprint. LevelUpIconSize has to be a literal
// in the header (hud_art blits the PNG unscaled, so the art is cut to exactly that size), so tie it
// to the real geometry here instead of trusting two numbers to stay in step by hand.
static_assert(LevelUpIconSize.width == ScalePlateRect(LmbWellSrc).size.width + 2 * SkillWellBezelPx
        && LevelUpIconSize.height == ScalePlateRect(LmbWellSrc).size.height + 2 * SkillWellBezelPx,
    "Level-up icon size no longer matches the LMB well plus its bezel - recut the art and update "
    "LevelUpIconSize in hud_layout.h");
constexpr Rectangle RmbWellSrc { { 1271, 26 }, { 207, 217 } };

/** @brief Integer division rounding to nearest. @p denominator must be positive. */
constexpr int RoundedDiv(int numerator, int denominator)
{
	return numerator >= 0 ? (numerator + denominator / 2) / denominator
	                      : -((-numerator + denominator / 2) / denominator);
}

/**
 * @brief Plate-local top-left origin that centres @p content on a well's TRUE opening.
 *
 * Not "centred inside ScalePlateRect(srcWell)", which is the obvious thing and is measurably wrong.
 * ScalePlateRect truncates each edge to a whole pixel, and for both wells all four edges truncate
 * DOWNWARD - so the rect's centre lands up to a pixel left of and above the opening it stands for:
 *
 *     LMB opening   x  5.914..55.115   y  6.150..57.480   true centre (30.514, 31.815)
 *     LMB rect      x  5..55           y  6..57           rect centre (30.0,   31.5)
 *     RMB opening   x  300.649..349.613                   true centre (325.131, 31.815)
 *     RMB rect      x  300..349                           rect centre (324.5,   31.5)
 *
 * Centring in the rect therefore put the icon a pixel left and a pixel high in BOTH wells - visible
 * once the icon grew enough to nearly fill the recess, and the reason DrawSpell's readied-spell icon
 * has carried a hand-tuned +1 x nudge since the art pass. This works from the unrounded source
 * geometry instead and rounds once, at the end.
 */
constexpr Point CentreInWell(Rectangle srcWell, Size content)
{
	constexpr int Denominator = 2 * PlateSrcSize.width;
	return { RoundedDiv((2 * srcWell.position.x + srcWell.size.width) * PlateScreenWidth
	             - content.width * PlateSrcSize.width,
	             Denominator),
		RoundedDiv((2 * srcWell.position.y + srcWell.size.height) * PlateScreenWidth
		        - content.height * PlateSrcSize.width,
		    Denominator) };
}

// The two wells are the same art mirrored, so the icon must fit the NARROWER of the two scaled
// openings - the RMB's, which comes out a pixel short of the LMB's purely through where its edges
// fall against the truncation.
//
// Only the upper bound is checked. There is deliberately no "and it must be snug" companion: the
// icon is sized to the engine's small spell icon, which the same well draws whenever a spell IS
// readied, not to the well. Filling the opening would mean the slot changed size with its state.
static_assert(SkillWellIconSize.width <= ScalePlateRect(RmbWellSrc).size.width
        && SkillWellIconSize.height <= ScalePlateRect(RmbWellSrc).size.height,
    "The skill-well icon no longer fits the RMB well's opening - recut ui\\attack_icons.png "
    "smaller (tools/CutAttackIcons.ps1) or fix the plate geometry");
// The centring maths pinned to hand-computed values, so a change to the plate scale cannot quietly
// shift both icons. Derived in the doc comment above.
static_assert(CentreInWell(LmbWellSrc, SkillWellIconSize).x == 12
        && CentreInWell(LmbWellSrc, SkillWellIconSize).y == 13
        && CentreInWell(RmbWellSrc, SkillWellIconSize).x == 306
        && CentreInWell(RmbWellSrc, SkillWellIconSize).y == 13,
    "Skill-well icon centring moved - re-derive it against the plate art before accepting this");

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

Point GetLmbSkillIconOrigin(Size content)
{
	const Point local = CentreInWell(LmbWellSrc, content);
	return GetMiddleHudRect().position + Displacement { local.x, local.y };
}

Point GetRmbSkillIconOrigin(Size content)
{
	const Point local = CentreInWell(RmbWellSrc, content);
	return GetMiddleHudRect().position + Displacement { local.x, local.y };
}

namespace {

/**
 * @brief Centres the net square inside @p well - see SkillWellNetSize for why it is not the well.
 *
 * With a hand nudge (user, 2026-08-19: "move the background and the skill/spell icons in the LMB/RMB
 * 3px down and 1px right to center them"). The bezel painted into middle_hud.png is not perfectly
 * symmetric about the button rect the code derives, so geometric centring reads high and left. One
 * correction here rather than at each drawing site, so every well's content - plate, tree icon,
 * attack icon, spell icon - moves together and stays aligned with the others.
 */
constexpr Displacement NetRectNudge { 1, 3 };

Rectangle NetRectIn(Rectangle well)
{
	return { Point { well.position.x + (well.size.width - SkillWellNetSize.width) / 2,
		         well.position.y + (well.size.height - SkillWellNetSize.height) / 2 }
		    + NetRectNudge,
		SkillWellNetSize };
}

} // namespace

Rectangle GetLmbSkillWellNetRect()
{
	return NetRectIn(GetLmbSkillButtonRect());
}

Rectangle GetRmbSkillWellNetRect()
{
	return NetRectIn(GetRmbSkillButtonRect());
}

Rectangle GetLevelUpIconRect()
{
	// Sits directly above the LMB skill button, matched to that button's full footprint - the well's
	// opening plus its bezel - so the two read as a stack rather than two unrelated widgets.
	//
	// Anchored to GetLmbSkillButtonRect() rather than to a screen corner, so it follows the plate
	// automatically: the plate is centred and bottom-flush, so its position moves with the
	// resolution. Earlier revisions pinned this to the clock (first centred on the clock's ':', then
	// flush to the left edge); both are gone.
	//
	// Lives here rather than beside the clock because control.cpp needs the rect for hit-testing as
	// well as drawing - the two must never disagree.
	const Rectangle lmb = GetLmbSkillButtonRect();
	constexpr int GapAboveButton = 4;
	return { { lmb.position.x + (lmb.size.width - LevelUpIconSize.width) / 2,
	             lmb.position.y - SkillWellBezelPx - GapAboveButton - LevelUpIconSize.height },
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

bool IsPointOverHudChrome(Point mousePosition)
{
	// The four things that actually absorb a click, and nothing else. GetMainPanel() appears only
	// under talkflag because the chat box is still drawn against the vanilla panel rect; the rest of
	// that 640x128 band is empty screen and must behave like it.
	return GetMiddleHudRect().contains(mousePosition)
	    || IsPointOverXpCounter(mousePosition)
	    || IsPointOverHudMenu(mousePosition)
	    || (talkflag && GetMainPanel().contains(mousePosition));
}

} // namespace devilution::oracool
