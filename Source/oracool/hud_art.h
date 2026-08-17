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
#include "player.h" // HeroClass - the class tree draws from a per-class icon strip
#include "spelldat.h"

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
 * Background ONLY. The old composition baked the silhouette, the slot frames and the class sygil
 * into one flat image; ui\inventory_background.png carries none of them - just the painted panel,
 * its arches and its border. Everything else (title, silhouette, slots, grid, tabs, footer) is
 * drawn on top at runtime, which is what keeps the layout the code's rather than the art's.
 */
void DrawInventoryPanelArt(const Surface &out);

/** @brief Whether the inventory panel asset loaded (so callers can fall back to the shared theme). */
bool HasInventoryPanelArt();

/**
 * @brief Which of the 340x720 side panels is being drawn.
 *
 * Every one of these has its OWN background since the 2026-08-18 MPQ sweep (unit D): the delivered
 * family carves the window's name into the top rail of the stone rather than leaving it to be
 * drawn, so five of the six differ only in that incised title.
 *
 * Abilities is the exception and gets `Plain`, a rail-less variant. Its title band is not decorative
 * - it names the current SHEET (Skills, Spells, Class Skills, Auras) and carries the page arrows, so
 * a title baked into the stone would have to lie on three sheets out of four.
 */
enum class SidePanel : uint8_t {
	Inventory,
	Stash,
	Quests,
	Waypoints,
	HeroStats,
	/** The Abilities window - no carved title, because it draws its own. */
	Plain,
	LAST = Plain,
};

/** @brief Draws @p which side panel's background at @p origin, 1:1. Nothing if the art is missing. */
void DrawSidePanelArt(const Surface &out, Point origin, SidePanel which);

/** @brief Whether @p which panel's background loaded, so callers can fall back to the shared theme. */
bool HasSidePanelArt(SidePanel which);

/**
 * @brief Height of the carved title rail, and therefore the first row content may use.
 *
 * The pack's own contract: "the carved title is already part of the relief... all interactive
 * content must begin at y=44 or lower". Every panel's content already started below this line, so
 * nothing had to move - but a future layout that creeps upward would be writing over incised stone,
 * which is why the number is named here rather than left in the art's README.
 */
constexpr int SidePanelTitleRailHeight = 44;

/** @brief Draws the 340x660 waypoint list panel with its top-left corner at @p origin. */
void DrawWaypointPanelArt(const Surface &out, Point origin);

/** @brief Whether the waypoint panel asset loaded (so callers can fall back to the old panel). */
bool HasWaypointPanelArt();

/** @brief Draws one waypoint pad at @p origin - the active pad if @p active, else the dormant one. */
void DrawWaypointIcon(const Surface &out, Point origin, bool active);

/** @brief On-screen size of a single waypoint pad, or {0,0} if the asset is missing. */
Size GetWaypointIconSize();

/**
 * @brief Which recolour a skill plate is drawn in, so the sheets are colour-coded by ability kind.
 *
 * Oracool: user request (2026-08-15) - "we need to come up with a color of the background of the
 * skills (not the CLASS SKILLS). Spells have Blue. Class Skills have YELLOW. Maybe we make SPECIFIC
 * SKILLS brown or green or dark blue or something else?"
 *
 * The palette used to pick the colour rather than taste: the plate is recoloured through the
 * engine's spell TRNs, which reached only the six 16-shade ramps the game ships - so the sheets
 * wore PAL16_BEIGE ("pink", the user's word) purely because it was the ramp left over. The user
 * never warmed to it, and Belzebub settled the argument that the palette itself is editable ("they
 * found a way. so should we", 2026-08-15): LoadPalette injects a GREEN ramp over the barely-used
 * PAL8_YELLOW mini-ramp, and the sheets wear that instead.
 */
enum class SkillPlateTint : uint8_t {
	/** The vanilla yellow - Class Skills and the HUD's wells. */
	Yellow,
	/** Every ability sheet drawn on plates: Skills, Auras and Barbarian - the injected green ramp. */
	Green,
	/**
	 * Cannot be performed RIGHT NOW - out of mana, missing shield, wrong place - as opposed to Grey's
	 * "not earned yet". User request (2026-08-16): "skills unable to perform due to whatever reason to
	 * have their background turned into pink until able to perform again." The PAL16_BEIGE ramp - the
	 * colour the user has always called pink - which stopped being any sheet's resting colour when the
	 * plates went green, freeing it to mean exactly this.
	 */
	Pink,
	/**
	 * Earned and spendable, but nothing invested yet - so the skill exists and does nothing (user
	 * request, 2026-08-17: "Unlocked skills with 0 points in them are unavailable and inactive, ergo
	 * need to have red background, not green").
	 *
	 * The third of three "you cannot use this" colours, and the only one the player can clear by
	 * spending a point: Grey is not earned, Pink is earned but blocked right now, Red is earned and
	 * empty.
	 */
	Red,
	/**
	 * Not earned yet (user request, 2026-08-15: "not yet learned skills to have gray background").
	 *
	 * The same grey the Spells sheet has always given an unlearned spell - SpellType::Invalid's ramp,
	 * PAL16_GRAY - so "you cannot use this" looks identical whether it is a spell you have not read
	 * or a skill you have not levelled into. The dimmed ICON already said so; the plate was still
	 * being drawn at full strength underneath it, which undercut that at a glance.
	 *
	 * Chosen per ROW rather than derived inside the plate drawing, because "locked" is not the only
	 * reason an icon is dimmed: the two basic attacks blend the one NOT in your hand, and neither of
	 * them is ever unlearned. Their rows keep the Green plate deliberately.
	 */
	Grey,
};

/**
 * @brief Draws Paladin tree icon @p skillIndex (oracool::PaladinTreeSkill order) at @p origin.
 *
 * An @p unlocked icon is blitted opaquely; a locked one is blended into the panel at half strength,
 * which is this sheet's equivalent of the Spells sheet greying out an unlearned spell. It cannot
 * use SetSpellTrans for that: these are pictures rather than single-ramp icons, so there is no ramp
 * to remap onto grey.
 */
void DrawClassTreeIcon(const Surface &out, Point origin, HeroClass heroClass, int skillIndex,
    bool unlocked, SkillPlateTint tint = SkillPlateTint::Green);
/**
 * @brief A tree icon drawn into a CELL, with the plate scaled to fill it.
 *
 * The Point overload above sizes the plate from GetSmallSpellIconSize() - 37x38 - which is smaller
 * than a 56x56 tree cell and was why the backing never matched the art. Prefer this one for the
 * tree sheets; see the definition.
 */
void DrawClassTreeIcon(const Surface &out, Rectangle cell, HeroClass heroClass, int skillIndex,
    bool unlocked, SkillPlateTint tint = SkillPlateTint::Green);

/** @brief On-screen size of one tree icon, or {0,0} if that class's strip is missing. */
Size GetClassTreeIconSize(HeroClass heroClass);

/** @brief Draws Paladin skill icon @p skillIndex (oracool::PaladinSkill order - 0 Charge, 1 Zeal).
 * Same locked treatment as DrawAuraIcon; same shared strip implementation. */
void DrawPaladinSkillIcon(const Surface &out, Point origin, int skillIndex, bool unlocked,
    SkillPlateTint tint = SkillPlateTint::Green);

/** @brief On-screen size of one Paladin skill icon, or {0,0} if the asset is missing. */
Size GetPaladinSkillIconSize();

/**
 * @brief Draws @p spell's icon from an Oracool strip if it has one, and reports whether it did.
 *
 * Oracool: user request (2026-08-15) - "their icons appear as they should on LMB/RMB". The Paladin's
 * skills are real SpellIDs, so everything that draws a readied spell reaches for the engine's
 * spelli2 sheet - where they have no frame, and SpellITbl points them at the empty plate. That is
 * what a screenshot showed on the skill wells: a spell assigned, and a blank square drawn for it.
 *
 * Their art is in ui\paladin_skill_icons.png instead, so any site drawing a readied spell asks this
 * first and falls back to DrawSmallSpellIcon when it returns false. @p origin is TOP-left, matching
 * the strip icons rather than the engine's bottom-left spell icons.
 */
bool TryDrawSkillSpellIcon(const Surface &out, Point origin, SpellID spell,
    SkillPlateTint tint = SkillPlateTint::Green);

/**
 * @brief TryDrawSkillSpellIcon for the speedbook's 56px LARGE plate; @p bottomLeft matches
 * DrawLargeSpellIcon's anchor. The engine's large icon sheet has no frames for the Paladin skills,
 * so the speedbook drew seven blank plates (user bug report, 2026-08-15) - this draws the large
 * plate in the Skills-sheet pink and centres the 38px strip icon on it.
 */
bool TryDrawSkillSpellIconLarge(const Surface &out, Point bottomLeft, SpellID spell,
    SkillPlateTint tint = SkillPlateTint::Green);

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
    SkillPlateTint tint = SkillPlateTint::Green);

/** @brief On-screen size of one basic-attack icon, or {0,0} if the asset is missing. */
Size GetAttackIconSize();

/**
 * @brief Draws the class silhouette behind the inventory's equipment slots.
 *
 * Centred across @p areaWidth, hanging from @p top, both panel-relative to @p panelOrigin. The
 * asset is pre-scaled by its cutter, so nothing is resized at draw time.
 */
void DrawClassSilhouette(const Surface &out, Point panelOrigin, int areaWidth, int top);

/** @brief Which of the tab atlas's three frames to draw. Order IS the atlas's column order. */
enum class InventoryTabState : uint8_t {
	Inactive,
	Hover,
	Active,
};

/**
 * @brief Draws inventory tab @p index (0-9) in @p state, from the three-frame chest atlas.
 *
 * The open tab overhangs its neighbours, so callers must draw every other tab first and the open
 * one last. Hit-testing stays on the logical 28x28 GetTabRect, never on the overhang.
 */
void DrawInventoryTab(const Surface &out, int index, InventoryTabState state);

/** @brief Whether the tab atlas loaded, so callers can fall back to the drawn tabs. */
bool HasInventoryTabArt();

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

/**
 * @brief The numbered skill-points icon: the 64px frame whose numeral IS @p count (clamped 1..99),
 * dark at rest and lit on @p lit. Returns false when the strips are not shipped, so the caller can
 * keep its placeholder.
 */
bool DrawUnspentPointsIcon(const Surface &out, Point origin, int count, bool lit);
/** @brief The numbered icons' cell size - one 64px square strip frame. */
constexpr Size PointsIconSize { 64, 64 };

/**
 * @brief Phase 0.8's art hot-reload: drops every cached PNG asset so the next draw re-reads it
 * from disk. Wired to the `reloadassets` debug command - edit a PNG, reload, see it in seconds.
 */
void ResetHudArtCaches();

} // namespace devilution::oracool
