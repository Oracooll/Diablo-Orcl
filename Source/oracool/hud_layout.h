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
inline constexpr Size LevelUpIconSize { 53, 53 };
Rectangle GetLevelUpIconRect();

/** @brief On-screen bounding box of the middle HUD plate (assets/ui/middle_hud.png, drawn by
 * hud_art.cpp) - horizontally centered, pinned near the bottom edge. Single source of truth for
 * everything drawn on or hit-tested against the plate, and for the <=640-wide dirty-rect render
 * path. */
Rectangle GetMiddleHudRect();

/** @brief Absolute screen rect of the plate's LMB well. Currently only a hover/click dead zone -
 * the LMB assign-and-cast mechanic is a later phase; the art's empty well is its placeholder. */
Rectangle GetLmbSkillButtonRect();

/** @brief Absolute screen rect of the plate's RMB well - used both to draw the relocated
 * readied-spell/speedbook indicator (see panels/spell_list.cpp's DrawSpell) and to hit-test clicks
 * on it (control.cpp's DoPanBtn/CheckPanelInfo). */
Rectangle GetRmbSkillButtonRect();

/** @brief Absolute screen rect of belt cell `visibleIndex` (0 = Menu, 1-4 = items, 5 = Town
 * Portal) on the plate art. Replaces the old InvRect-plus-panel-offset math for every belt draw
 * call and hit-test - InvRect itself stays untouched (its values are save/hit-test-shared
 * constants for the inventory grid). */
Rectangle GetBeltSlotRect(int visibleIndex);

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

} // namespace devilution::oracool
