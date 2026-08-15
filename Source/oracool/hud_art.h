/**
 * @file oracool/hud_art.h
 *
 * Oracool: user request - HUD overhaul art pass. Loads the user-supplied HUD art (the middle HUD
 * plate and the Health/Mana orb compositions, assets/ui/*.png) and draws them as the new HUD.
 *
 * The engine renders to an 8-bit palettized surface, so each 32-bit PNG is quantized to palette
 * indices on first draw. Quantization deliberately uses only the GLOBAL half of the palette
 * (entries 128-255 - see engine/palette.h: "Entry 128-255 are global"), which is identical across
 * town and every dungeon type, so one quantization pass works everywhere and never hits the
 * level-specific/color-cycled 0-127 range. Matching runs against orig_palette (NOT logical_palette,
 * which still holds the loading screen's cutscene palette when the first frame draws - see the
 * bug postmortem in hud_art.cpp), and everything requantizes automatically if the global palette
 * half ever actually changes.
 *
 * The orbs get a drain effect: a darkened variant of each composition - dimmed only inside the
 * orb's sphere circle, so the statue/frame art stays lit - is drawn as the base, and the bright
 * version is revealed bottom-up across the sphere's vertical span proportionally to current
 * HP/mana. One source image per orb is enough; the empty state is generated, not authored.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/**
 * @brief Draws the middle HUD plate at hud_layout's GetMiddleHudRect() position. Lazily loads and
 * quantizes the PNG on the first call (the level palette is guaranteed loaded by then, since this
 * runs mid-frame). No-op if the asset is missing - the dynamic content (items, spell icon) still
 * draws, just frameless.
 */
void DrawMiddleHudArt(const Surface &out);

/** @brief Whether the plate asset loaded successfully (after the first DrawMiddleHudArt call). */
bool HasMiddleHudArt();

/** @brief Draws the Health orb composition at hud_layout's GetHealthOrbRect(), with the sphere
 * filled proportionally to the player's current hit points. No-op if the asset is missing. */
void DrawHealthOrb(const Surface &out);

/** @brief Mana counterpart of DrawHealthOrb (GetManaOrbRect(), current mana). */
void DrawManaOrb(const Surface &out);

/**
 * @brief Draws one burger-menu icon at `position`. `iconIndex` selects the sprite sheet row (see
 * hud_menu.cpp's entry list) and `state` the column: 0 inactive, 1 hovered, 2 lit (the panel this
 * entry opens is currently showing, or it was just clicked). No-op if the sheet is missing.
 */
void DrawMenuIcon(const Surface &out, int iconIndex, int state, Point position);

/**
 * @brief Draws the inventory window background at inventory_layout's GetInventoryPanelRect().
 *
 * One flat composition: the stone background, the paladin silhouette, every equipment slot frame
 * and the class sygil are baked in at asset-build time by tools/InvCompose.cs. Only the tabs, the
 * SORT button and the items themselves are drawn on top at runtime.
 */
void DrawInventoryPanelArt(const Surface &out);

/** @brief Whether the inventory panel asset loaded (so callers can fall back to the old panel). */
bool HasInventoryPanelArt();

/** @brief Draws the 340x660 waypoint list panel with its top-left corner at @p origin. */
void DrawWaypointPanelArt(const Surface &out, Point origin);

/** @brief Whether the waypoint panel asset loaded (so callers can fall back to the old panel). */
bool HasWaypointPanelArt();

/** @brief Draws one waypoint pad at @p origin - the active pad if @p active, else the dormant one. */
void DrawWaypointIcon(const Surface &out, Point origin, bool active);

/** @brief On-screen size of a single waypoint pad, or {0,0} if the asset is missing. */
Size GetWaypointIconSize();

/**
 * @brief Draws Paladin aura icon @p auraIndex (oracool::Aura order) at @p origin.
 *
 * An @p unlocked icon is blitted opaquely; a locked one is blended into the panel at half strength,
 * which is this sheet's equivalent of the Spells sheet greying out an unlearned spell. It cannot
 * use SetSpellTrans for that: these are full-colour paintings rather than single-ramp icons, so
 * there is no ramp to remap onto grey.
 */
void DrawAuraIcon(const Surface &out, Point origin, int auraIndex, bool unlocked);

/** @brief On-screen size of one aura icon, or {0,0} if the asset is missing. */
Size GetAuraIconSize();

/** @brief Draws Barbarian skill icon @p skillIndex (oracool::BarbSkill order). Same locked
 * treatment as DrawAuraIcon - the two sheets share one implementation. */
void DrawBarbSkillIcon(const Surface &out, Point origin, int skillIndex, bool unlocked);

/** @brief On-screen size of one Barbarian skill icon, or {0,0} if the asset is missing. */
Size GetBarbSkillIconSize();

/**
 * @brief Which recolour a skill plate is drawn in, so the sheets are colour-coded by ability kind.
 *
 * Oracool: user request (2026-08-15) - "we need to come up with a color of the background of the
 * skills (not the CLASS SKILLS). Spells have Blue. Class Skills have YELLOW. Maybe we make SPECIFIC
 * SKILLS brown or green or dark blue or something else?"
 *
 * Brown, and the palette picked it rather than taste: the plate is recoloured through the engine's
 * spell TRNs, which only reach the six 16-shade ramps the game ships. Blue is already spells, yellow
 * is class skills, grey is "cannot cast", orange is a staff's charges, and red now marks the left
 * mouse button's assignment. Beige is the one ramp left - and green was never on the table, because
 * the palette has no green ramp to give it.
 */
enum class SkillPlateTint : uint8_t {
	/** The vanilla yellow - Class Skills, the Auras and Barbarian sheets, and the HUD's wells. */
	Yellow,
	/** The Skills sheet. */
	Brown,
};

/** @brief Draws Paladin skill icon @p skillIndex (oracool::PaladinSkill order - 0 Charge, 1 Zeal).
 * Same locked treatment as DrawAuraIcon; same shared strip implementation. */
void DrawPaladinSkillIcon(const Surface &out, Point origin, int skillIndex, bool unlocked,
    SkillPlateTint tint = SkillPlateTint::Yellow);

/** @brief On-screen size of one Paladin skill icon, or {0,0} if the asset is missing. */
Size GetPaladinSkillIconSize();

/**
 * @brief The vanilla empty spell-icon plate every skill icon is drawn on, 37x38.
 *
 * Frame 26 of data\spelli2 through SetSpellTrans(SpellType::Skill) - the game's own empty-slot
 * square and the game's own "this is a skill" yellow. Callers that lay out a row should fall back to
 * THIS when a custom strip is absent, rather than expecting the Get*IconSize functions above to lie
 * about the art: those report {0,0} when there is no art, and the HUD's skill wells depend on that
 * (see the postmortem at StripIconSize).
 */
Size GetSkillIconPlateSize();

/** @brief Draws the plate at @p origin, taking a TOP-left origin like the strip icons. */
void DrawSkillIconPlate(const Surface &out, Point origin, SkillPlateTint tint = SkillPlateTint::Yellow);

/**
 * @brief Draws basic-attack icon @p iconIndex (oracool::AttackIcon order) at @p origin.
 *
 * Third user of the same strip implementation. @p active is DrawStripIcon's "unlocked": the two
 * attack icons are never locked, but exactly one of them is what the player's hand is currently
 * doing, and blending the other is how the pair says which. See oracool/attack_skills.h.
 */
void DrawAttackIcon(const Surface &out, Point origin, int iconIndex, bool active,
    SkillPlateTint tint = SkillPlateTint::Yellow);

/** @brief On-screen size of one basic-attack icon, or {0,0} if the asset is missing. */
Size GetAttackIconSize();

/**
 * @brief Draws the class silhouette behind the inventory's equipment slots.
 *
 * Centred across @p areaWidth, hanging from @p top, both panel-relative to @p panelOrigin. The
 * asset is pre-scaled by its cutter, so nothing is resized at draw time.
 */
void DrawClassSilhouette(const Surface &out, Point panelOrigin, int areaWidth, int top);

/**
 * @brief Draws inventory tab @p index (0-9). @p state is 0 unselected, 1 selected, 2 pressed.
 * Position comes from inventory_layout's GetTabCellOrigin().
 */
void DrawInventoryTab(const Surface &out, int index, int state);

// DrawInventorySortButton is gone: the SORT button has no art of its own. It spent a while as the
// tab row's last position, drawn as the letter "S", and is now a text button in the panel's footer -
// see DrawInventoryFooter in inv.cpp. That freed the tab position to become the tenth storage page.

/**
 * @brief Draws the belt's Town Portal button. @p state is 0 resting, 1 hovered, 2 pressed.
 *
 * The plate art has a portal ring painted into that cell, so this draws *over* it rather than
 * into an empty slot - the icon is opaque and its black floor is lifted at cut time so no pixel
 * quantizes onto palette entry 0 and lets the old ring show through.
 *
 * Note the states do not follow the source sheet's order. The portal is always available, so its
 * resting look is the sheet's ACTIVE (lit); pressing it shows the sheet's INACTIVE (dim), which
 * reads as the button depressing.
 */
void DrawTownPortalIcon(const Surface &out, int state);

/**
 * @brief Draws the belt's burger-menu button. @p state is 0 resting, 1 hovered, 2 open.
 *
 * The Menu cell is a toggle, not a momentary action, so state 2 means "the popup is showing"
 * rather than "just clicked" - it stays lit for as long as the menu is open. Replaces the
 * translucent overlay that cell used to need for lack of any artwork.
 */
void DrawBurgerMenuButton(const Surface &out, int state);

/**
 * @brief Draws the level-up indicator at hud_layout's GetLevelUpIconRect(), under the game clock.
 * @p state is 0 resting, 1 hovered, 2 pressed.
 */
void DrawLevelUpIconArt(const Surface &out, int state);

} // namespace devilution::oracool
