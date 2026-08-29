/**
 * @file oracool/hud_layout.h
 *
 * Oracool: user request - Phase 1 of the 960x720 HUD overhaul. Single source of truth for the new
 * screen-anchored HUD geometry (corner-locked Health/Mana orbs, the centered "Middle HUD" row of
 * [LMB button][6-slot belt][RMB button]) and the belt's repurposed slot layout. Every draw call and
 * hit-test that needs one of these positions reads it from here, rather than each duplicating its
 * own copy of the numbers - see hud_layout.cpp's file comment for why that matters for the
 * <=640-wide dirty-rect render path (scrollrt.cpp's DrawMain).
 *
 * GetMainPanel()'s rect (control.h/.cpp) is deliberately left untouched by this HUD overhaul - it's
 * still the correct anchor for LeftPanel/RightPanel (char/quest/inv/spellbook/stash flyouts). The
 * middle HUD plate centers itself on the screen directly, and the orbs pin to the bottom corners -
 * see GetMiddleHudRect/GetHealthOrbRect/GetManaOrbRect below.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "engine/size.hpp"
#include "player.h"

namespace devilution::oracool {

/** @brief On-screen rect of the Health orb composition (assets/ui/health_orb.png, drawn by
 * hud_art.cpp), pinned flush to the screen's bottom-left corner regardless of resolution. */
Rectangle GetHealthOrbRect();

/** @brief Mana counterpart of GetHealthOrbRect, pinned to the bottom-right corner. */
Rectangle GetManaOrbRect();

/** @brief The orb sphere's center in rect-local pixels (the drain effect's dim/fill circle, and
 * where the current/max value text centers). */
Point GetHealthOrbSphereCenterLocal();
Point GetManaOrbSphereCenterLocal();

/** @brief Radius of both orbs' spheres in screen pixels (the processed assets render both at the
 * same diameter). */
int GetOrbSphereRadius();

/** @brief One cell of assets/ui/menu_icons.png - the burger menu's icon row. The sheet is a grid
 * with column = state (0 inactive, 1 hovered, 2 lit) and row = entry. */
inline constexpr Size MenuIconSize { 30, 33 };

/** @brief Number of entries in the burger menu's icon row (rows in the sprite sheet). */
/**
 * @brief Entries in the burger-menu icon row, and therefore usable rows of menu_icons.png.
 *
 * Oracool: was 11; the three mini-map entries (Recenter, Zoom In, Zoom Out) were removed on user
 * request. They were the last three, so rows 0-7 of the sheet still line up with the remaining
 * entries. The row re-centres on the plate automatically - IconRowRect() derives its width from
 * this constant.
 */
inline constexpr int MenuIconCount = 8;

/**
 * @brief The level-up indicator, which appears under the game clock when there are unspent
 * attribute points.
 *
 * Oracool: user request - moved out from beside the old bottom panel, where it sat on top of the
 * new HUD art. The top-left corner already holds the clock and nothing else, so the two read as
 * one small status stack.
 */
/**
 * @brief Level-up icon size: the LMB skill well PLUS its bezel.
 *
 * The well's opening scales to 50x51 on screen and its metal bezel is 5px on every side, so the
 * button's full visual footprint is 60x61. Hardcoded because hud_art blits the art unscaled - the
 * PNG is cut to exactly this size - but hud_layout.cpp static_asserts it against the real scaled
 * LMB rect, so the two cannot drift apart silently.
 */
inline constexpr Size LevelUpIconSize { 60, 61 };
Rectangle GetLevelUpIconRect();

/** @brief On-screen bounding box of the middle HUD plate (assets/ui/middle_hud.png, drawn by
 * hud_art.cpp) - horizontally centered, pinned near the bottom edge. Single source of truth for
 * everything drawn on or hit-tested against the plate, and for the <=640-wide dirty-rect render
 * path. */
Rectangle GetMiddleHudRect();

/**
 * @brief The HUD row's real outer footprint, which is not always the plate's.
 *
 * Identical to GetMiddleHudRect while the plate art is on. With it off the row is laid out from the
 * cells instead and comes out wider, so anything that needs to sit CLEAR of the HUD - the orbs -
 * must ask for this rather than for the plate.
 */
Rectangle GetHudRowRect();

/** @brief Absolute screen rect of the plate's LMB well. Currently only a hover/click dead zone -
 * the LMB assign-and-cast mechanic is a later phase; the art's empty well is its placeholder. */
Rectangle GetLmbSkillButtonRect();

/** @brief Absolute screen rect of the plate's RMB well - used both to draw the relocated
 * readied-spell/speedbook indicator (see panels/spell_list.cpp's DrawSpell) and to hit-test clicks
 * on it (control.cpp's DoPanBtn/CheckPanelInfo). */
Rectangle GetRmbSkillButtonRect();

/**
 * @brief The NET area of a skill well - the flat floor inside the bezel, and a hard boundary.
 *
 * User rule (2026-08-18): "measure how many px is the net area within the bezels and don't spill out
 * of it. I believe it is 46x46 pixels. Consider this the hard boundary of these slots and don't ever
 * spill over it, just like you dont spill over the 28x28 grid boxes."
 *
 * So it is a stated constant, not a derived one, and it is deliberately SMALLER than the well rect
 * the plate art scales to: that rect is the opening including the moulding that rings it, and art
 * centred in it still laps onto the bezel. Everything a well draws - the tinted plate behind a skill
 * and the icon on top of it - is centred in this instead.
 */
constexpr Size SkillWellNetSize { 46, 46 };

/** @brief The net square of the LEFT well, centred in its opening. */
Rectangle GetLmbSkillWellNetRect();

/** @brief The net square of the RIGHT well, centred in its opening. */
Rectangle GetRmbSkillWellNetRect();

/**
 * @brief Cell size of ui\attack_icons.png, the strip the two skill wells draw.
 *
 * NOT sized to the wells, which are ~49x51 and could take a much larger icon. Sized to the engine's
 * small spell icon - 37x38, the one Item Repair and every readied spell draws at, and not resizable
 * since it comes from the game's own CEL. The RMB well alternates between that icon and this one
 * depending on whether a spell is readied, so a bigger attack icon made the slot's contents change
 * size with its state. It also matches the aura and Barbarian strips, which keeps every icon in the
 * Abilities window on one rhythm.
 *
 * 38 rather than 37 because DrawStripIcon takes square cells (it derives the cell size from the
 * strip's height); a pixel of extra width against the spell icon is invisible next to the state
 * change it avoids.
 *
 * A literal here for the same reason as LevelUpIconSize: hud_art blits the art unscaled, so the PNG
 * is cut to exactly this. hud_layout.cpp static_asserts it fits the wells and pins the centring it
 * produces; attack_skills.cpp asserts the loaded art actually matches it. Between them, nothing here
 * can drift without something failing.
 */
inline constexpr Size SkillWellIconSize { 38, 38 };

/**
 * @brief Top-left origin that puts a @p content-sized sprite dead centre in the LMB well.
 *
 * Centred on the well's TRUE opening rather than inside GetLmbSkillButtonRect(), whose edges are
 * each truncated to a whole pixel - all four in the same direction, which moves its centre up to a
 * pixel up and left of the opening it describes. See CentreInWell in hud_layout.cpp for the numbers.
 */
Point GetLmbSkillIconOrigin(Size content);

/** @brief GetLmbSkillIconOrigin's twin for the RMB well. */
Point GetRmbSkillIconOrigin(Size content);

/** @brief Absolute screen rect of belt cell `visibleIndex` (0 = Menu, 1-4 = items, 5 = Town
 * Portal) on the plate art. Replaces the old InvRect-plus-panel-offset math for every belt draw
 * call and hit-test - InvRect itself stays untouched (its values are save/hit-test-shared
 * constants for the inventory grid). */
Rectangle GetBeltSlotRect(int visibleIndex);

/**
 * @brief How many belt cells the plate shows: Menu, four item slots, Town Portal.
 *
 * The count the ROW has, not the count of usable item slots - IsRealBeltItemSlot answers that and
 * gives four. Anything drawing the row itself (a backing, a frame) wants this one; anything
 * carrying items wants that one.
 */
inline constexpr int BeltVisibleSlotCount = 6;

/** @brief Underlying Player::SpdList index repurposed as the belt's "Menu" button (opens
 * hud_menu.h's popup) - never holds a real item once MigrateHiddenBeltSlots has run once. */
inline constexpr int BeltMenuSlotIndex = 0;

/** @brief Underlying Player::SpdList index repurposed as the belt's permanent Town Portal button
 * (see hud_menu.h's CastTownPortalAtFeet) - never holds a real item once migrated. */
inline constexpr int BeltTownPortalSlotIndex = 5;

/** @brief Whether SpdList index `i` is one of the 4 slots still usable as a real item slot in the
 * new 6-visible-slot belt layout (indices 1-4; 0 and 5 are repurposed above, 6 and 7 are simply
 * hidden and untargeted). Every belt draw loop, drag-drop target search, hover-highlight check, and
 * auto-place function must gate on this so nothing writes an item into a slot the player can no
 * longer see or reach. */
inline bool IsRealBeltItemSlot(int i)
{
	return i >= 1 && i <= 4;
}

/**
 * @brief One-time migration for existing test saves: any character that already has real items
 * sitting in SpdList[0], [5], [6], or [7] from before this HUD change would otherwise find them
 * silently inaccessible, since those slots stop being drag-drop/hotkey targets. Moves each into the
 * first empty slot among {1,2,3,4}, falling back to the general inventory, and logs via LogEvent
 * (event_log.h) if an item can't be placed anywhere and has to stay put. A no-op on any character
 * that's already clean, so it's safe to call unconditionally every session rather than needing a
 * persisted "already migrated" flag. Single-player only, matching the rest of this HUD pass - see
 * its call site in multi.cpp's InitSingle, right after pfile_read_player_from_save.
 */
void MigrateHiddenBeltSlots(Player &player);

/**
 * @brief Whether @p mousePosition is over HUD chrome rather than over the world.
 *
 * Oracool bug fix (2026-08-15): user report - "skills dont seem to work". They worked; they were
 * being eaten. CheckPlrSpell had its own idea of where the UI is, `GetMainPanel().contains(...)`,
 * and GetMainPanel is still the vanilla 640x128 rect at the screen bottom because the flyout panels
 * centre against it. After the HUD overhaul almost none of that rect is chrome any more - it is the
 * gaps either side of a small centre plate - so a click on a monster low on the screen returned from
 * CheckPlrSpell having done nothing at all. Not walked, not cast: nothing. With no skill readied the
 * same click went to LeftMouseCmd and worked fine, which is exactly the shape of "the skills are
 * broken" rather than "the bottom of the screen is dead".
 *
 * So there is one authority now, and both the click router and the cast path ask it. Anything that
 * needs to know where the UI is must call this rather than testing a rect of its own; that is the
 * whole point of it existing.
 *
 * The chat panel is in the list only while it is open, which is why the old panel rect appears here
 * at all - the chat box is still drawn against it.
 */
bool IsPointOverHudChrome(Point mousePosition);

/**
 * @brief Whether @p mousePosition is over one of the FREE-FLOATING windows - the ones centred over
 * the world rather than docked into a panel slot.
 *
 * The docked windows are each excluded from world hover by their own rect test in CheckCursMove.
 * The floating ones had no such test, so the world kept being probed straight through them: with
 * Levski's Roar open over the tavern you could hover Ogden through the panel, and the cursor then
 * invited a click the router had already decided to absorb (user report, 2026-08-20 - "you can see
 * i can hover over ogden").
 *
 * One function rather than a test per window, for the same reason IsPointOverHudChrome exists: the
 * next floating window should join a list, not add a fourth place that has to remember this rule.
 */
bool IsPointOverFloatingWindow(Point mousePosition);

} // namespace devilution::oracool
