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
 * @brief Draws the shared 340x720 side-panel background at @p origin, 1:1.
 *
 * ui\panel_bg.png - the artisan-bezel family's ashen limestone (MPQ sweep 2026-08-18, unit D).
 * ONE background for all six windows: inventory, stash, quests, waypoints, character and abilities.
 * It carries no title, which is exactly what lets it be shared - each window still draws its own.
 *
 * A carved-title family was tried first, one background per window with the name incised into a top
 * rail. It was not the collection the user wanted, and it cost the Abilities window its sheet name;
 * see the 2026-08-18 dev report.
 *
 * Nothing is drawn if the art is missing; every caller keeps a procedural fill as its fallback.
 */
void DrawSidePanelArt(const Surface &out, Point origin);

// DrawSidePanelBackdrop was declared here and is gone (user, 2026-09-02: "remove the dark
// transparent rectangle from all canvases, which we used with the limestone background"). It dimmed
// the panel's inner opening on all six windows, from 2026-08-19 ("put a dark transparent screen
// 299x609px in the hero stats screen", then "put this screen on all limestone screens in the game")
// until the stone was recut dark and it had nothing left to do. Its own doc promised that its rect
// moved with the artwork; the recut is what proved the rect was the weak part of the arrangement.

/** @brief Whether the background loaded, so callers can fall back to the shared theme. */
bool HasSidePanelArt();

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
bool TryDrawSkillSpellIcon(const Surface &out, Rectangle well, SpellID spell,
    SkillPlateTint tint = SkillPlateTint::Green);

/**
 * @brief TryDrawSkillSpellIcon for the speedbook's 56px LARGE plate; @p bottomLeft matches
 * DrawLargeSpellIcon's anchor. The engine's large icon sheet has no frames for the Paladin skills,
 * so the speedbook drew seven blank plates (user bug report, 2026-08-15) - this draws the large
 * plate in the Skills-sheet pink and centres the 38px strip icon on it.
 */
/**
 * @brief A legacy spell's OWN icon, fitted to @p cell through @p tint's ramp.
 *
 * The Abilities page's twin of the legacy branch in TryDrawSkillSpellIcon, so the tree cell and the
 * well a skill is dragged into cannot disagree about a spell's picture (user, 2026-09-03).
 */
void DrawLegacySpellIconInCell(const Surface &out, Rectangle cell, SpellID spell, SkillPlateTint tint);

/**
 * @brief Charges left on the staff @p player has equipped, if it casts @p spell; -1 when it does not.
 *
 * ONE source for the number, because the badge below has to appear in four places and a second copy
 * of "which staff, which spell" is a second place for them to disagree. The staff is always the
 * left-hand slot - that is where _pISpells is built from (see CalcPlrItemVals).
 */
/**
 * @brief A thick red X across @p icon - the mark for "this cannot be used".
 *
 * Lived in spell_book.cpp as DrawUnbuiltCross from 2026-08-18, where it meant one thing: a tree row
 * that is listed but not built. It has a second meaning now - a staff with no charges left (user,
 * 2026-09-03: "when charges reach 0, put the red X over the icon, like we do with broken items") -
 * and two callers in two files is the moment it stops being one window's private helper.
 *
 * Drawn as horizontal runs rather than through a line primitive because the engine has only
 * axis-aligned ones. Each row of the icon gets a short run on each diagonal, Thickness wide, which
 * is both simpler than a Bresenham walk and gives the stroke a constant horizontal width - the
 * chunky look a marked-out icon wants.
 */
void DrawRedCross(const Surface &out, Rectangle icon);

int StaffChargesFor(const Player &player, SpellID spell);

/**
 * @brief The staff-charge badge, bottom-left of @p host. Draws nothing unless @p player holds a
 * staff of @p spell.
 *
 * Its own function rather than four copies of the call, because the request was that the badge
 * "travel with skill/spell icon everywhere they appear" (user, 2026-09-03) - which is a rule about
 * every draw site at once, and a rule like that survives only if there is one thing to call.
 *
 * Red below ten charges, white at ten and above, and bottom-LEFT because the other three corners
 * are taken: the two top ones are the hotkey badges and bottom-right is the rank.
 */
void DrawStaffChargeBadge(const Surface &out, Rectangle host, const Player &player, SpellID spell);

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

/**
 * @brief DrawAttackIcon scaled to FILL @p well, plate and icon both.
 *
 * For the LMB/RMB wells, whose @p well is the 46x46 net opening between the bezels. The strip is cut
 * at 38, so the Point overload left a moat of plate around it (user, 2026-08-18: "reg attack and fist
 * don't use the 46x46px size").
 */
void DrawAttackIconScaledTo(const Surface &out, Rectangle well, int iconIndex, bool active,
    SkillPlateTint tint = SkillPlateTint::Green);

/**
 * @brief Draws a class-tree skill's own icon, scaled to fill @p well - for skills with no SpellID.
 *
 * The auras are the reason this exists: they are a toggle rather than a cast, so they never appear as
 * a readied spell and TryDrawSkillSpellIcon can never find them.
 */
void DrawClassTreeSkillInWell(const Surface &out, Rectangle well, HeroClass heroClass, int skillIndex,
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
 * @brief The skill-points frame at @p origin: one 64px picture, an ornate border around a dark well.
 *
 * Returns false when the art is not shipped, so the caller can keep its placeholder - and note that
 * this reports whether a draw can HAPPEN, not merely whether pixels were read. See the note in the
 * implementation: hud_art keeps three hand-maintained per-asset lists and testing the end of that
 * pipeline is what makes missing one degrade to the placeholder rather than to blank.
 *
 * The NUMBER is not drawn here. The caller paints it into SkillPointsNumberRect below, because a
 * drawn count is what replaced the old pair of 99-frame strips whose numeral was baked into the art
 * (user, 2026-08-20) - baked numerals run out at 100 and need recutting whenever the cap moves.
 */
bool DrawUnspentPointsIcon(const Surface &out, Point origin, int count, bool lit);

/**
 * @brief Draws the points frame as the backing for a skill well, centred on @p well.
 *
 * The same 64x64 art the stat-point and skill-point counters wear. @p well is the well's OPENING
 * (GetLmbSkillButtonRect / GetRmbSkillButtonRect), which is smaller than the frame, so the frame
 * overhangs it evenly and the skill icon lands where a counter's numeral would.
 *
 * @return false when the art is unavailable, so a caller can tell "no backing" from "drawn".
 */
bool DrawSkillWellBacking(const Surface &out, Rectangle well);

/**
 * @brief Draws the points frame, sized down, into each of the six belt cells.
 *
 * The row's own count (BeltVisibleSlotCount) - Menu, four item slots, Town Portal - so the frames
 * run edge to edge rather than framing the potions and leaving the two buttons bare. Scaled to each
 * cell, unlike DrawSkillWellBacking: a cell is roughly half the art's native size, so an unscaled
 * frame would cover its neighbours.
 *
 * Call BEFORE DrawInvBelt - this is a backing, and the items go on top of it.
 */
void DrawBeltBacking(const Surface &out);

/**
 * @brief The 40x39 box dead centre of the frame at @p origin, where the count is drawn.
 *
 * The size is the user's (2026-08-20: "in its center area in 40x39px area dead center in the
 * icon"); the centring is derived from PointsIconSize so the two cannot drift apart.
 */
Rectangle SkillPointsNumberRect(Point origin);
/** @brief The numbered icons' cell size - one 64px square strip frame. */
constexpr Size PointsIconSize { 64, 64 };

/**
 * @brief Phase 0.8's art hot-reload: drops every cached PNG asset so the next draw re-reads it
 * from disk. Wired to the `reloadassets` debug command - edit a PNG, reload, see it in seconds.
 */
void ResetHudArtCaches();

} // namespace devilution::oracool
