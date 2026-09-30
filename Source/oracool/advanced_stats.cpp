/**
 * @file oracool/advanced_stats.cpp
 *
 * The ADVANCED STATS window - see advanced_stats.h for what it is, where it lives and the rule for
 * sharing the right-hand slot. User, 2026-09-26, from the approved mock-up: grouped rows under
 * OFFENSE / DEFENSE / RECOVERY / OTHER, every row a full-width box with its text centred, BLUE rows
 * for bonuses the hero has, GREY rows for facts that always show, and - "hide bonuses the hero doesn't
 * have" - no row at all for a bonus that is zero. A section with nothing in it loses its heading too.
 *
 * Every number is derived from the same source the old character sheet's rows read (the helpers are
 * exported from panels/charpanel.cpp rather than copied, so the two cannot disagree), plus the fork's
 * own per-player queries named at each row.
 */
#include "oracool/advanced_stats.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <SDL.h> // SDL_GetTicks - the bars' colour cycle

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "control.h" // sbookflag
#include "diablo.h"  // MousePosition
#include "engine/load_clx.hpp" // the vanilla field bezel
#include "engine/palette.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h" // invflag
#include "multi.h" // sgGameInitInfo - the difficulty, for the resistance penalty
#include "oracool/class_tree.h" // MovementSpeedPercent
#include "oracool/cold.h"       // ColdMasteryDamagePercent, ColdResistanceDivisor
#include "oracool/gems.h"       // GemLifePerKill, RuneManaPerKill
#include "oracool/hud_art.h"    // the shared side-panel canvas
#include "oracool/ornate_border.h"
#include "oracool/player_resistance.h" // ResistanceHardCap, ResistancePenaltyFor
#include "oracool/signets.h"
#include "panels/charpanel.hpp" // the sheet's derived readings - frames, block, steal
#include "player.h"
#include "utils/clx_decode.hpp"
#include "utils/display.h" // gnScreenWidth
#include "utils/language.h"
#include "utils/png.h" // LoadPNG - the user's stat field frame
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

/** @brief The same 340x720 as every other side panel, so it sits exactly over the inventory's slot. */
constexpr Size AdvancedPanelSize { 340, 720 };
/** @brief Title band's side margin - the character sheet's CharPanelMargin, so the two titles match. */
constexpr int AdvancedTitleMargin = 24;
/** @brief The list starts just under the title band (the old sheet's separator rule is gone with the canvas). */
constexpr int AdvancedContentTop = PanelTitleTop + PanelTitleHeight + 6;
/** @brief ...and ends where the mana orb starts drawing over the panel (ornate_border.h). */
constexpr int AdvancedContentBottom = SidePanelContentBottom;
/**
 * @brief The rows sit 4px inside the canvas's painted frame, whose inner edge is x=22 on the left and x=317
 * on the right (user, 2026-09-26 dev note: "we need to fit contents of stats screen within 4px away from
 * frame of canvases"; they used to start at 16, over the frame). The grouped sheet uses the same inset.
 */
constexpr int AdvancedRowsX = 26;
constexpr int AdvancedRowsWidth = AdvancedPanelSize.width - 2 * AdvancedRowsX;
/** @brief The clip starts 2px further left than the rows, so their drop shadow is not cut off. */
constexpr int AdvancedContentX = AdvancedRowsX - 2;
constexpr int AdvancedContentWidth = AdvancedRowsWidth + 2;
constexpr int AdvancedContentHeight = AdvancedContentBottom - AdvancedContentTop;
static_assert(AdvancedContentHeight > 0, "Advanced Stats has no content room left");
/**
 * @brief One line of the list, and the pitch between lines. Plain shadowed text since 2026-09-26 (user: "Remove the
 * frames and backing of advanced stats. Just shadowed text. Category titles also lose frames and backing"), so a
 * row is its 14 rows of ink (glyphs and shadow) and 4 of air.
 */
constexpr int AdvancedRowHeight = 18;
constexpr int AdvancedRowPitch = 18;
/** @brief Air above a category title that follows rows, so OFFENSE / DEFENSE... still read as section breaks. */
constexpr int AdvancedHeadingGap = 10;
/**
 * @brief The line's rect: 22 tall starting 4 rows above the line, so its ink runs rows 2-15 of the 18 (DrawString
 * clips 3px above a rect's bottom; charpanel.cpp's FieldLine has the measurements).
 */
constexpr int AdvancedTextTop = -4;
constexpr int AdvancedTextHeight = 22;
/** @brief Side inset for a long line that steps down a font size. */
constexpr int AdvancedTextInset = 4;
/** @brief One wheel notch: three rows, the conventional three lines. */
constexpr int AdvancedScrollStep = AdvancedRowPitch * 3;
/** @brief The scrollbar, in the panel's right margin - the character sheet's geometry exactly. */
constexpr int AdvancedScrollbarWidth = OrnateBorderWidth;
constexpr int AdvancedScrollbarRightPad = 8;
constexpr int AdvancedScrollbarMinThumb = 24;

bool Open = false;
/** @brief What the window covered when it opened - put back by an ordinary close. At most one is set. */
bool CoveredInventory = false;
bool CoveredAbilities = false;
int ScrollOffset = 0;
/** @brief The list's height at the last draw - what the wheel clamps against between draws. */
int LastListHeight = 0;

enum class RowKind : uint8_t {
	Heading,
	Bonus,
	Fact,
	/** A penalty or a curse: the same box shape, red. */
	Curse,
};

struct Row {
	RowKind kind;
	std::string text;
};

int MaxScrollOffset()
{
	return std::max(0, LastListHeight - AdvancedContentHeight);
}

/** @brief "+N" for a positive number, "-N" for a negative one - the sign always shown. */
std::string Signed(int value)
{
	return value >= 0 ? StrCat("+", value) : StrCat(value);
}

/**
 * @brief OFFENSE: everything that changes what a hit does.
 *
 * The fire and lightning pairs mirror the old sheet's "Fire damage" / "Lightning damage" rows, which
 * showed the range whenever _pIFMaxDam was above zero; the arrow flags reuse the same two fields
 * (a fire-arrow bow deals its fire through _pIFMinDam/_pIFMaxDam), so a bow names itself as arrows.
 */
void AddOffense(const Player &p, std::vector<Row> &rows)
{
	if (p._pISplLvlAdd != 0) {
		rows.push_back({ p._pISplLvlAdd > 0 ? RowKind::Bonus : RowKind::Curse,
		    fmt::format(fmt::runtime(_("{:s} to all spell levels")), Signed(p._pISplLvlAdd)) });
	}
	// Cold Mastery, both halves (oracool/cold.h). The divisor is 4 without it - a resisted cold hit is
	// quartered - and a row appears only once the mastery has actually bought some of that back.
	if (const int coldPercent = ColdMasteryDamagePercent(p); coldPercent > 0)
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("+{:d}% cold damage (Cold Mastery)")), coldPercent) });
	if (const int divisor = ColdResistanceDivisor(p); divisor < 4) {
		rows.push_back({ RowKind::Bonus, divisor <= 1
		        ? std::string(_("Cold ignores resistance"))
		        : std::string(_("Resisted cold hits lose half, not three quarters")) });
	}

	if (HasAnyOf(p._pIFlags, ItemSpecialEffect::FireArrows)) {
		rows.push_back({ RowKind::Bonus, p._pIFMaxDam > 0
		        ? fmt::format(fmt::runtime(_("Fire arrows: {:d}-{:d} fire damage")), p._pIFMinDam, p._pIFMaxDam)
		        : std::string(_("Fire arrows")) });
	} else if (p._pIFMaxDam > 0) {
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("Fire damage {:d}-{:d} on every hit")), p._pIFMinDam, p._pIFMaxDam) });
	}
	if (HasAnyOf(p._pIFlags, ItemSpecialEffect::LightningArrows)) {
		rows.push_back({ RowKind::Bonus, p._pILMaxDam > 0
		        ? fmt::format(fmt::runtime(_("Lightning arrows: {:d}-{:d} lightning damage")), p._pILMinDam, p._pILMaxDam)
		        : std::string(_("Lightning arrows")) });
	} else if (p._pILMaxDam > 0) {
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("Lightning damage {:d}-{:d} on every hit")), p._pILMinDam, p._pILMaxDam) });
	}
	if (HasAnyOf(p._pIFlags, ItemSpecialEffect::MultipleArrows))
		rows.push_back({ RowKind::Bonus, std::string(_("Fires multiple arrows")) });
	if (HasAnyOf(p._pIFlags, ItemSpecialEffect::TripleDemonDamage))
		rows.push_back({ RowKind::Bonus, std::string(_("300% damage to demons")) });
	// As a percentage (2026-09-27): the field is a tier, and tier 2 halves the armour - "Ignores 2" said otherwise.
	if (p._pIEnAc > 0)
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("Ignores {:d}% of the target's armor")), GetSheetArmorPiercePercentFor(p)) });

	// The fixed 3/5% and the random drain are independent and can both be active - the old sheet's Life
	// steal row, word for word in its arithmetic (player.cpp's steal branch; the random one is
	// GenerateRnd(dam / 8), zero to just under an eighth of the damage dealt).
	{
		const int fixedPct = GetSheetLifeStealPercent();
		const bool random = HasAnyOf(p._pIFlags, ItemSpecialEffect::RandomStealLife);
		// Melee only: the steal lives in PlrHitMonst, so a bow never steals (round 12 audit).
		if (fixedPct > 0 && random)
			rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("{:d}% +0-12% life stolen per melee hit")), fixedPct) });
		else if (fixedPct > 0)
			rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("{:d}% life stolen per melee hit")), fixedPct) });
		else if (random)
			rows.push_back({ RowKind::Bonus, std::string(_("0-12% life stolen per melee hit")) });
	}
	// NoMana zeroes this whatever the jewellery says (GetSheetManaStealPercent answers 0 then).
	if (const int manaPct = GetSheetManaStealPercent(); manaPct > 0)
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("{:d}% mana stolen per hit")), manaPct) });

	// Animation frames, so LOWER is faster: the old sheet's Now/Base pair in one line.
	if (const int skipped = GetSheetAttackFramesSkipped(); skipped > 0) {
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("Attack speed: {:d} frames (base {:d})")),
		                                     p._pAFrames - skipped, p._pAFrames) });
	}
	if (p._pIFastCast > 0)
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("+{:d}% faster cast rate")), p._pIFastCast) });
	if (HasAnyOf(p._pIFlags, ItemSpecialEffect::Knockback))
		rows.push_back({ RowKind::Bonus, std::string(_("Knocks the target back")) });

	// Hellfire's weapon specials, in pDamAcFlags - described in the item tooltip's own words
	// (items.cpp's PrintItemPower), with the special's name after so the row can be matched to the item.
	if (HasAnyOf(p.pDamAcFlags, ItemSpecialEffectHf::Devastation))
		rows.push_back({ RowKind::Bonus, std::string(_("Occasional triple damage (Devastation)")) });
	if (HasAnyOf(p.pDamAcFlags, ItemSpecialEffectHf::Decay))
		rows.push_back({ RowKind::Bonus, std::string(_("Damage bonus that decays (Decay)")) });
	if (HasAnyOf(p.pDamAcFlags, ItemSpecialEffectHf::Peril))
		rows.push_back({ RowKind::Bonus, std::string(_("2x damage to monsters, 1x to you (Peril)")) });
	if (HasAnyOf(p.pDamAcFlags, ItemSpecialEffectHf::Jesters))
		rows.push_back({ RowKind::Bonus, std::string(_("Random 0 - 600% damage (Jester's)")) });
	if (HasAnyOf(p.pDamAcFlags, ItemSpecialEffectHf::Doppelganger))
		rows.push_back({ RowKind::Bonus, std::string(_("10% of hits clone the foe (Doppelganger)")) });

	// A FACT, grey, always shown: while the Diablo II rule is on trial (player.h's SpellsNeverMiss) the
	// percentage would name a roll that is no longer made - the old sheet's "Spell to hit: Always".
	if (SpellsNeverMiss)
		rows.push_back({ RowKind::Fact, std::string(_("Spells always hit")) });
	else
		rows.push_back({ RowKind::Fact, fmt::format(fmt::runtime(_("Spell to hit {:d}%")), p.GetMagicToHit()) });
}

/** @brief DEFENSE: what happens to a hit the hero takes. */
void AddDefense(const Player &p, std::vector<Row> &rows)
{
	// Equal-level reading, zero without a shield - GetSheetBlockChancePercent explains both.
	if (const int block = GetSheetBlockChancePercent(); block > 0) {
		rows.push_back({ RowKind::Bonus, HasAnyOf(p._pIFlags, ItemSpecialEffect::FastBlock)
		        ? fmt::format(fmt::runtime(_("Block chance {:d}%  -  fast block")), block)
		        : fmt::format(fmt::runtime(_("Block chance {:d}%")), block) });
	}
	// _pIGetHit is ADDED to incoming damage (player.cpp), so negative is the good direction.
	if (p._pIGetHit < 0)
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("Damage taken reduced by {:d}")), -p._pIGetHit) });
	else if (p._pIGetHit > 0)
		rows.push_back({ RowKind::Curse, fmt::format(fmt::runtime(_("Damage taken increased by {:d}")), p._pIGetHit) });
	// A flat 1-3 per melee hit taken, not scaled by anything (monster.cpp).
	if (HasAnyOf(p._pIFlags, ItemSpecialEffect::Thorns))
		rows.push_back({ RowKind::Bonus, std::string(_("Attacker takes damage of 1-3")) });
	if (HasAnyOf(p._pIFlags, ItemSpecialEffect::HalfTrapDamage))
		rows.push_back({ RowKind::Bonus, std::string(_("Half damage from traps")) });
	if (const int skipped = GetSheetHitRecoveryFramesSkipped(); skipped > 0) {
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("Hit recovery: {:d} frames (base {:d})")),
		                                     p._pHFrames - skipped, p._pHFrames) });
	}
	if (HasAnyOf(p.pDamAcFlags, ItemSpecialEffectHf::ACAgainstDemons))
		rows.push_back({ RowKind::Bonus, std::string(_("Extra armor vs demons")) });
	if (HasAnyOf(p.pDamAcFlags, ItemSpecialEffectHf::ACAgainstUndead))
		rows.push_back({ RowKind::Bonus, std::string(_("Extra armor vs undead")) });
	// The four contributors behind the sheet's one Armor class number - GetArmor() is _pIBonusAC + _pIAC
	// + dexterity/5 (player.h), and the sheet adds level * 2, so these four sum to exactly that box.
	rows.push_back({ RowKind::Fact, fmt::format(fmt::runtime(_("AC: armor {:d}, magic {:d}, dex {:d}, level {:d}")),
	                                    p._pIAC, p._pIBonusAC, p._pDexterity / 5, p._pLevel * 2) });

	// What makes the sheet's four resistances readable - moved here from the sheet's strip under them (user,
	// 2026-09-26: "Remove the resistance cap from main stats and move it to advanced stats"). The difficulty's
	// penalty in red while there is one, and the ceiling always.
	static constexpr const char *DifficultyNames[] = { N_("Normal"), N_("Nightmare"), N_("Hell"), N_("Torment") };
	if (const int penalty = ResistancePenaltyFor(sgGameInitInfo.nDifficulty); penalty > 0) {
		const int difficulty = std::clamp(static_cast<int>(sgGameInitInfo.nDifficulty), 0, 3);
		rows.push_back({ RowKind::Curse, fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} is the difficulty, "Hell" */ "{:s}: -{:d}% to all resistances")),
		                                     _(DifficultyNames[difficulty]), penalty) });
	}
	rows.push_back({ RowKind::Fact, fmt::format(fmt::runtime(_("Resistances capped at {:d}%")), ResistanceHardCap) });
}

/**
 * @brief RECOVERY: what refills the pools.
 *
 * The auras' and passives' regeneration (Prayer, Meditation, Warmth...) is NOT here: class_tree.cpp
 * applies it inside ProcessClassTreeTick with a file-local RegenPerTick, and no exported query answers
 * "how much is this hero regenerating now". Rebuilding that branch here would be a second copy of the
 * rule to fall out of step with the first, so the row waits for an exported query.
 */
void AddRecovery(const Player &p, std::vector<Row> &rows)
{
	if (const int life = GemLifePerKill(p); life > 0)
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("+{:d} life per kill")), life) });
	// Not under NoMana, which the kill's grant skips (round 12 audit).
	if (const int mana = RuneManaPerKill(p); mana > 0 && !HasAnyOf(p._pIFlags, ItemSpecialEffect::NoMana))
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("+{:d} mana per kill")), mana) });
}

/** @brief OTHER: movement, find, the curses a cursed item carries, and two facts. */
void AddOther(const Player &p, std::vector<Row> &rows)
{
	// 100 is a plain walk (class_tree.h); a slow reads as a penalty, in red.
	if (const int move = EffectiveMovementSpeedPercent(p) - 100; move != 0) { // the run and the clamp included (round 12)
		rows.push_back({ move > 0 ? RowKind::Bonus : RowKind::Curse,
		    fmt::format(fmt::runtime(_("{:s}% movement speed")), Signed(move)) });
	}
	if (p._pMagicFind != 0 && p._pGoldFind != 0) {
		// One line when both are there, as the mock-up drew it - they are read together.
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("{:s}% magic find  -  {:s}% gold find")),
		                                     Signed(p._pMagicFind), Signed(p._pGoldFind)) });
	} else if (p._pMagicFind != 0) {
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("{:s}% magic find")), Signed(p._pMagicFind)) });
	} else if (p._pGoldFind != 0) {
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("{:s}% gold find")), Signed(p._pGoldFind)) });
	}
	// The vanilla curses, in red, only while worn.
	if (HasAnyOf(p._pIFlags, ItemSpecialEffect::NoMana))
		rows.push_back({ RowKind::Curse, std::string(_("No mana")) });
	if (HasAnyOf(p._pIFlags, ItemSpecialEffect::DrainLife))
		rows.push_back({ RowKind::Curse, std::string(_("Life drained over time")) });
	if (HasAnyOf(p._pIFlags, ItemSpecialEffect::ZeroResistance))
		rows.push_back({ RowKind::Curse, std::string(_("All resistances zeroed before the difficulty penalty")) });

	rows.push_back({ RowKind::Fact, fmt::format(fmt::runtime(_("Light radius {:d}")), static_cast<int>(p._pLightRad)) });
	// The one permanent, irreversible choice a character makes - twenty for a lifetime. Red at the cap.
	const int used = SignetsUsed(p);
	rows.push_back({ used >= SignetLifetimeCap ? RowKind::Curse : RowKind::Fact,
	    fmt::format(fmt::runtime(_("Signets of Learning: {:d} of {:d} used")), used, SignetLifetimeCap) });
}

/** @brief Appends @p heading and @p section to @p rows - or nothing at all when the section is empty. */
void AppendSection(std::vector<Row> &rows, const char *heading, std::vector<Row> &&section)
{
	if (section.empty())
		return;
	rows.push_back({ RowKind::Heading, std::string(_(heading)) });
	for (Row &row : section)
		rows.push_back(std::move(row));
}

std::vector<Row> BuildRows(const Player &p)
{
	std::vector<Row> rows;
	std::vector<Row> section;
	AddOffense(p, section);
	AppendSection(rows, N_("OFFENSE"), std::move(section));
	section.clear();
	AddDefense(p, section);
	AppendSection(rows, N_("DEFENSE"), std::move(section));
	section.clear();
	AddRecovery(p, section);
	AppendSection(rows, N_("RECOVERY"), std::move(section));
	section.clear();
	AddOther(p, section);
	AppendSection(rows, N_("OTHER"), std::move(section));
	return rows;
}

/** @brief Each row's top in the list, unscrolled: a fixed pitch, plus AdvancedHeadingGap above every title but the first. */
std::vector<int> RowTops(const std::vector<Row> &rows, int &listHeight)
{
	std::vector<int> tops;
	tops.reserve(rows.size());
	int y = 0;
	for (size_t i = 0; i < rows.size(); i++) {
		if (i > 0 && rows[i].kind == RowKind::Heading)
			y += AdvancedHeadingGap;
		tops.push_back(y);
		y += AdvancedRowPitch;
	}
	listHeight = y;
	return tops;
}

void DrawRow(const Surface &content, const Row &row, int top)
{
	// The colour is the whole distinction now the boxes are gone: gold titles, blue bonuses, red curses, white facts.
	UiFlags color = UiFlags::ColorWhite;
	switch (row.kind) {
	case RowKind::Heading:
		color = UiFlags::ColorGold;
		break;
	case RowKind::Bonus:
		color = UiFlags::ColorBlue;
		break;
	case RowKind::Curse:
		color = UiFlags::ColorRed;
		break;
	case RowKind::Fact:
		break;
	}
	const Rectangle textRect { { 2 + AdvancedTextInset, top + AdvancedTextTop }, { AdvancedRowsWidth - 2 * AdvancedTextInset, AdvancedTextHeight } };
	DrawSheetTextFitted(content, row.text, textRect, color | UiFlags::AlignCenter | UiFlags::VerticalCenter);
}

/** @brief The theme's scrollbar - the character sheet's own groove and thumb. */
void DrawScrollbar(const Surface &out, const Rectangle &panel)
{
	const int maxOffset = MaxScrollOffset();
	if (maxOffset <= 0)
		return;
	const int x = panel.position.x + AdvancedPanelSize.width - AdvancedScrollbarRightPad - AdvancedScrollbarWidth;
	const int top = panel.position.y + AdvancedContentTop;
	DrawThemedFill(out, { { x, top }, { AdvancedScrollbarWidth, AdvancedContentHeight } }, 2);
	const int thumbHeight = std::max(AdvancedScrollbarMinThumb,
	    AdvancedContentHeight * AdvancedContentHeight / std::max(LastListHeight, 1));
	const int travel = AdvancedContentHeight - thumbHeight;
	const int thumbY = top + travel * ScrollOffset / maxOffset;
	DrawOrnateSeparatorVertical(out, { x, thumbY }, thumbHeight);
}

/** @brief One translucent box's look: its fill (straight alpha) and its 1px edge. */
struct BoxLook {
	uint8_t alpha;
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint32_t edgeRgb;
	uint8_t edgeFallback;
};

/**
 * @brief The five tones, read off the approved mock-up: near-black boxes with a bronze hairline, the
 * bonus rows navy with a blue one, a curse's dark red, the headings' warmer bronze plate, and the
 * recessed strips a shade darker than a plain box.
 */
constexpr BoxLook LookFor(SheetBoxTone tone)
{
	switch (tone) {
	case SheetBoxTone::Bonus:
		return { 205, 22, 26, 66, 0x46508C, PAL16_BLUE + 8 };
	case SheetBoxTone::Curse:
		return { 205, 56, 14, 12, 0x86372C, PAL16_RED + 10 };
	case SheetBoxTone::Heading:
		return { 230, 42, 32, 16, 0x9A7840, ThemeEdgeColor };
	case SheetBoxTone::Recess:
		return { 220, 8, 7, 6, 0x4A3C24, PAL16_GRAY + 13 };
	case SheetBoxTone::Plain:
		break;
	}
	return { 200, 18, 15, 11, 0x6E5632, ThemeEdgeColor };
}

// ---- The stat field frame (user, 2026-09-26 dev note: "we need to use as frame for all the texts the vanilla
// diablo 1 frames use in vanilla stats"; redrawn by the user the next day) ------------------------------------
//
// Three pieces laid end to end, as vanilla's DrawPanelField laid its own under every number on the old sheet: a
// left end, a middle and a right end, the same height. A box of any size is cut from the joined strip - the ends
// kept whole, the middle's columns repeated for width, the rows between the top and bottom frame stretched or
// squeezed for height, so the field's top-to-bottom shading stays one smooth ramp at any height instead of
// repeating in bands.
//
// Two sources, the user's first:
//   - ui\stat_field_left/middle/right.png in oracool.mpq - the user's redesign (2026-09-27), 5x25 / 284x25 / 5x25:
//     no outer grey line, 2px of gold round the top and the right, and at the bottom and the left the gold plus a
//     2px light lip inside it. Read as true colour, so the art may use any colours it likes.
//   - data\boxleftend/boxmiddle/boxrightend.clx in devilutionx.mpq - vanilla's own, 6x27 / 284x27 / 6x27, with a
//     1px outer line and 3px/5px frames, decoded through the palette. Every index in them is in 0xC0-0xFF, the
//     range that is the same in all seventeen level palettes, so a box built in town is still right in Hell.

/**
 * @brief The drop shadow under every field: 3px down and 3px left, the pressed button's direction. Added from a dev
 * note (2px), removed after one render, back at 4px (user, 2026-09-26: "Add back shadow to each frame. Make it 4px"),
 * and 3px since 2026-09-27 ("make the shadows 3px").
 */
constexpr int FieldShadowOffset = 3;
constexpr uint8_t FieldShadowAlpha = 150;

struct FieldStrip {
	bool attempted = false;
	int width = 0;
	int height = 0;
	int leftWidth = 0;
	int middleWidth = 0;
	int rightWidth = 0;
	/** Rows kept whole at the top and at the bottom; the rows between are the ones stretched. */
	int topRows = 0;
	int bottomRows = 0;
	/** The frame's thickness on every side, inside which a tone's wash goes. */
	int bevel = 0;
	/** Straight-alpha ARGB, row-major. Empty when neither source is in the archives. */
	std::vector<uint32_t> argb;
};
FieldStrip Strip;

struct FieldCacheEntry {
	int width;
	int height;
	SheetBoxTone tone;
	std::vector<uint32_t> argb;
};
std::vector<FieldCacheEntry> FieldCache;

/** @brief Decodes one CLX frame into @p strip at column @p x0 through the palette (CLX rows run bottom-up). */
void DecodeInto(ClxSprite sprite, FieldStrip &strip, int x0)
{
	const int w = sprite.width();
	const int h = sprite.height();
	const uint8_t *src = sprite.pixelData();
	const uint8_t *const end = src + sprite.pixelDataSize();
	int i = 0;
	const auto put = [&](uint8_t value) {
		const int row = h - 1 - i / w;
		if (row >= 0 && row < strip.height) {
			const SDL_Color c = logical_palette[value];
			strip.argb[static_cast<size_t>(row * strip.width + x0 + i % w)] = PackArgb(255, c.r, c.g, c.b);
		}
		i++;
	};
	while (src < end && i < w * h) {
		const uint8_t control = *src++;
		if (!IsClxOpaque(control)) {
			i += control;
		} else if (IsClxOpaqueFill(control)) {
			if (src >= end)
				break;
			const uint8_t value = *src++;
			for (int k = GetClxOpaqueFillWidth(control); k > 0; k--)
				put(value);
		} else {
			for (int k = GetClxOpaquePixelsWidth(control); k > 0 && src < end; k--)
				put(*src++);
		}
	}
}

/** @brief One PNG piece as straight-alpha ARGB rows, or empty when it is not in the archives. */
std::vector<uint32_t> LoadFieldPiece(const char *path, int &width, int &height)
{
	std::vector<uint32_t> pixels;
	SDL_Surface *png = LoadPNG(path);
	if (png == nullptr)
		return pixels;
	SDL_Surface *argb = SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_ARGB8888, 0);
	SDL_FreeSurface(png);
	if (argb == nullptr)
		return pixels;
	width = argb->w;
	height = argb->h;
	pixels.resize(static_cast<size_t>(width) * height);
	for (int y = 0; y < height; y++)
		std::memcpy(&pixels[static_cast<size_t>(y) * width], static_cast<const uint8_t *>(argb->pixels) + static_cast<size_t>(y) * argb->pitch,
		    static_cast<size_t>(width) * 4);
	SDL_FreeSurface(argb);
	return pixels;
}

/** @brief The user's PNG frame (2026-09-27). False when any piece is missing or the three disagree on height. */
bool LoadUserFieldStrip()
{
	int lw = 0, lh = 0, mw = 0, mh = 0, rw = 0, rh = 0;
	const std::vector<uint32_t> left = LoadFieldPiece("ui\\stat_field_left.png", lw, lh);
	const std::vector<uint32_t> middle = LoadFieldPiece("ui\\stat_field_middle.png", mw, mh);
	const std::vector<uint32_t> right = LoadFieldPiece("ui\\stat_field_right.png", rw, rh);
	constexpr int TopRows = 2;    // the gold
	constexpr int BottomRows = 4; // the light lip, then the gold
	if (left.empty() || middle.empty() || right.empty() || lh != mh || mh != rh || mh <= TopRows + BottomRows || mw <= 0)
		return false;
	Strip.leftWidth = lw;
	Strip.middleWidth = mw;
	Strip.rightWidth = rw;
	Strip.width = lw + mw + rw;
	Strip.height = mh;
	Strip.topRows = TopRows;
	Strip.bottomRows = BottomRows;
	Strip.bevel = 2;
	Strip.argb.assign(static_cast<size_t>(Strip.width * Strip.height), 0);
	for (int y = 0; y < Strip.height; y++) {
		uint32_t *row = &Strip.argb[static_cast<size_t>(y * Strip.width)];
		std::copy_n(&left[static_cast<size_t>(y * lw)], lw, row);
		std::copy_n(&middle[static_cast<size_t>(y * mw)], mw, row + lw);
		std::copy_n(&right[static_cast<size_t>(y * rw)], rw, row + lw + mw);
	}
	return true;
}

/** @brief Vanilla's CLX frame, the fallback. False when it is not in the archives either. */
bool LoadVanillaFieldStrip()
{
	// The palette is loaded with the first level; decoded before that, the frame would be cached black.
	const SDL_Color probe = logical_palette[0xC3];
	if (probe.r == 0 && probe.g == 0 && probe.b == 0)
		return false;
	const OptionalOwnedClxSpriteList left = LoadOptionalClx("data\\boxleftend.clx");
	const OptionalOwnedClxSpriteList middle = LoadOptionalClx("data\\boxmiddle.clx");
	const OptionalOwnedClxSpriteList right = LoadOptionalClx("data\\boxrightend.clx");
	if (!left || !middle || !right)
		return false;
	const ClxSprite l = (*left)[0];
	const ClxSprite m = (*middle)[0];
	const ClxSprite r = (*right)[0];
	constexpr int TopRows = 3;    // the outer line, then the gold
	constexpr int BottomRows = 5; // the light lip, the gold, the outer line
	if (l.height() != m.height() || m.height() != r.height() || m.width() == 0 || m.height() <= TopRows + BottomRows)
		return false;
	Strip.leftWidth = l.width();
	Strip.middleWidth = m.width();
	Strip.rightWidth = r.width();
	Strip.width = Strip.leftWidth + Strip.middleWidth + Strip.rightWidth;
	Strip.height = m.height();
	Strip.topRows = TopRows;
	Strip.bottomRows = BottomRows;
	Strip.bevel = 3;
	Strip.argb.assign(static_cast<size_t>(Strip.width * Strip.height), 0);
	DecodeInto(l, Strip, 0);
	DecodeInto(m, Strip, Strip.leftWidth);
	DecodeInto(r, Strip, Strip.leftWidth + Strip.middleWidth);
	return true;
}

/** @brief Loads the frame once - the user's, else vanilla's. False when neither is there (the fallback box then). */
bool LoadFieldStrip()
{
	if (!Strip.attempted) {
		if (LoadUserFieldStrip() || LoadVanillaFieldStrip())
			Strip.attempted = true;
		// Not marked attempted when vanilla's failed for want of a palette: the next draw tries again.
		else if (logical_palette[0xC3].r != 0 || logical_palette[0xC3].g != 0 || logical_palette[0xC3].b != 0)
			Strip.attempted = true;
	}
	return !Strip.argb.empty();
}

/** @brief The tone's wash over the field's interior, as straight-alpha ARGB - or 0 for none (Plain, Recess). */
uint32_t FieldToneWash(SheetBoxTone tone)
{
	switch (tone) {
	case SheetBoxTone::Bonus:
		return PackArgb(150, 24, 30, 84);
	case SheetBoxTone::Curse:
		return PackArgb(150, 70, 16, 12);
	case SheetBoxTone::Heading:
		return PackArgb(160, 60, 44, 18);
	case SheetBoxTone::Plain:
	case SheetBoxTone::Recess:
		break;
	}
	return 0;
}

/** @brief A @p w x @p h field in @p tone, built from the strip on first use and kept. Null when unavailable. */
const std::vector<uint32_t> *FieldPixels(int w, int h, SheetBoxTone tone)
{
	if (!LoadFieldStrip())
		return nullptr;
	for (const FieldCacheEntry &entry : FieldCache) {
		if (entry.width == w && entry.height == h && entry.tone == tone)
			return &entry.argb;
	}
	if (FieldCache.size() >= 64)
		FieldCache.clear(); // the sheets use about a dozen sizes; this only stops a runaway from growing forever

	FieldCacheEntry entry { w, h, tone, std::vector<uint32_t>(static_cast<size_t>(w * h)) };
	const int middleRows = Strip.height - Strip.topRows - Strip.bottomRows;
	const int stretchedRows = std::max(h - Strip.topRows - Strip.bottomRows, 1);
	const uint32_t wash = FieldToneWash(tone);
	for (int y = 0; y < h; y++) {
		int sy;
		if (y < Strip.topRows)
			sy = y;
		else if (y >= h - Strip.bottomRows)
			sy = Strip.height - (h - y);
		else
			sy = Strip.topRows + (y - Strip.topRows) * middleRows / stretchedRows;
		sy = std::clamp(sy, 0, Strip.height - 1);
		for (int x = 0; x < w; x++) {
			int sx;
			if (x < Strip.leftWidth && x < w / 2)
				sx = x;
			else if (x >= w - Strip.rightWidth && x >= w / 2)
				sx = Strip.width - (w - x);
			else
				sx = Strip.leftWidth + (x - Strip.leftWidth) % Strip.middleWidth;
			uint32_t pixel = Strip.argb[static_cast<size_t>(sy * Strip.width + sx)];
			// A strip taller than the box (dev note, 2026-09-27: a high-resolution backing, so no box is ever stretched) is
			// squeezed by averaging the source rows each box row covers, not by skipping them.
			if (stretchedRows < middleRows && y >= Strip.topRows && y < h - Strip.bottomRows) {
				const int from = Strip.topRows + (y - Strip.topRows) * middleRows / stretchedRows;
				const int to = std::max(from + 1, Strip.topRows + (y - Strip.topRows + 1) * middleRows / stretchedRows);
				int sum[4] = {};
				for (int r = from; r < to; r++) {
					const uint32_t p = Strip.argb[static_cast<size_t>(r * Strip.width + sx)];
					for (int c = 0; c < 4; c++)
						sum[c] += static_cast<int>((p >> (8 * c)) & 0xFF);
				}
				pixel = 0;
				for (int c = 0; c < 4; c++)
					pixel |= static_cast<uint32_t>(sum[c] / (to - from)) << (8 * c);
			}
			const bool interior = x >= Strip.bevel && x < w - Strip.bevel && y >= Strip.bevel && y < h - Strip.bevel;
			if (wash != 0 && interior && (pixel >> 24) != 0)
				pixel = (pixel & 0xFF000000) | (CompositeArgbOver(wash, pixel, 100) & 0x00FFFFFF);
			entry.argb[static_cast<size_t>(y * w + x)] = pixel;
		}
	}
	FieldCache.push_back(std::move(entry));
	return &FieldCache.back().argb;
}

/** @brief The pre-2026-09-26 box: a translucent fill and a 1px edge. The bars' groove, and the fallback. */
void DrawTranslucentBox(const Surface &out, Rectangle rect, SheetBoxTone tone)
{
	const int w = rect.size.width;
	const int h = rect.size.height;
	if (w <= 0 || h <= 0)
		return;
	const BoxLook look = LookFor(tone);
	// A single ARGB pixel stretched over the rect IS a translucent fill: BlitArgbScaled composites it
	// source-over at every destination pixel, clipped to the surface - so the stone shows through
	// darkened rather than blacked out. It refuses an indexed surface, where the theme's
	// half-transparent passes stand in.
	const uint32_t pixel = PackArgb(look.alpha, look.r, look.g, look.b);
	if (!BlitArgbScaled(out, &pixel, 1, SDL_Rect { 0, 0, 1, 1 }, rect, 100))
		DrawThemedFill(out, rect, 2);
	const int x = rect.position.x;
	const int y = rect.position.y;
	FillRectRgb(out, x, y, w, 1, look.edgeRgb, look.edgeFallback);
	FillRectRgb(out, x, y + h - 1, w, 1, look.edgeRgb, look.edgeFallback);
	FillRectRgb(out, x, y, 1, h, look.edgeRgb, look.edgeFallback);
	FillRectRgb(out, x + w - 1, y, 1, h, look.edgeRgb, look.edgeFallback);
}

// ---- the slab behind the canvas (user, 2026-09-27) -------------------------------------------------------------
//
// "take this backing ... obsidian-stone-slab-background-340x720.png and place it one layer behind the hero stats canvas
// and simulate each frame as a punctured hole through the canvas, revealing whatever part of this layer lays behind
// it." The slab is the side panel's own size and stays put behind it; a frame is a hole cut through the canvas, so its
// interior shows the part of the slab that lies under that spot - and a frame that scrolls slides across the slab like
// a window moved over a wall. The frame's gold (its top and bottom rows and its end caps) stays as the hole's cut edge;
// the canvas's lip casts its shadow INTO the hole, along the top and the right, the sheet's light (shadows fall down
// and left); and a hole casts none outward, so there is no drop shadow under it.

struct Slab {
	bool attempted = false;
	int width = 0;
	int height = 0;
	std::vector<uint32_t> argb;
};
Slab SheetSlab;
/** @brief The panel's top-left on the base surface while the grouped sheet draws; unset, the frames are solid. */
std::optional<Point> SlabOrigin;

bool LoadSheetSlab()
{
	if (!SheetSlab.attempted) {
		SheetSlab.attempted = true;
		SheetSlab.argb = LoadFieldPiece("ui\\hero_sheet_slab.png", SheetSlab.width, SheetSlab.height);
	}
	return !SheetSlab.argb.empty();
}

/**
 * @brief @p rect as a hole down to the slab: the field's frame, the slab inside it, the canvas's shadow across the
 * inside of its top and right edges, the tone's wash over the lot. False when there is no slab to show.
 */
bool DrawSheetHole(const Surface &out, Rectangle rect, SheetBoxTone tone, const std::vector<uint32_t> &field)
{
	if (!SlabOrigin || !LoadSheetSlab())
		return false;
	const int w = rect.size.width;
	const int h = rect.size.height;
	// Where this box sits over the slab: its place on the base surface, less the panel's.
	const int slabX = out.region.x + rect.position.x - SlabOrigin->x;
	const int slabY = out.region.y + rect.position.y - SlabOrigin->y;
	const uint32_t wash = FieldToneWash(tone);
	const uint32_t shade = PackArgb(FieldShadowAlpha, 0, 0, 0);
	const int rimLeft = std::min(Strip.leftWidth, w / 2);
	const int rimRight = std::min(Strip.rightWidth, w / 2);
	// One scratch buffer reused across boxes and frames, not a fresh copy per box per frame (round 14 audit). Drawing is
	// single-threaded, and the buffer does not outlive this call's blit.
	static std::vector<uint32_t> hole;
	hole.assign(field.begin(), field.end());
	for (int y = Strip.topRows; y < h - Strip.bottomRows; y++) {
		const int sy = std::clamp(slabY + y, 0, SheetSlab.height - 1);
		for (int x = rimLeft; x < w - rimRight; x++) {
			const int sx = std::clamp(slabX + x, 0, SheetSlab.width - 1);
			uint32_t pixel = SheetSlab.argb[static_cast<size_t>(sy * SheetSlab.width + sx)] | 0xFF000000;
			const bool shadowed = y < Strip.topRows + FieldShadowOffset || x >= w - rimRight - FieldShadowOffset;
			if (shadowed)
				pixel = 0xFF000000 | (CompositeArgbOver(shade, pixel, 100) & 0x00FFFFFF);
			if (wash != 0)
				pixel = 0xFF000000 | (CompositeArgbOver(wash, pixel, 100) & 0x00FFFFFF);
			hole[static_cast<size_t>(y * w + x)] = pixel;
		}
	}
	return BlitArgb(out, hole.data(), w, SDL_Rect { 0, 0, w, h }, rect.position, 100);
}

} // namespace

void SetSheetSlabOrigin(std::optional<Point> panelOrigin)
{
	SlabOrigin = panelOrigin;
}

void DrawSheetBox(const Surface &out, Rectangle rect, SheetBoxTone tone, bool castShadow)
{
	const int w = rect.size.width;
	const int h = rect.size.height;
	if (w <= 0 || h <= 0)
		return;
	const std::vector<uint32_t> *field = FieldPixels(w, h, tone);
	if (field == nullptr) {
		DrawTranslucentBox(out, rect, tone);
		return;
	}
	if (DrawSheetHole(out, rect, tone, *field))
		return;
	// The shadow first, the field over it: the field is opaque, so only the L to its lower left shows.
	if (castShadow)
		DrawSheetShadow(out, rect);
	if (!BlitArgb(out, field->data(), w, SDL_Rect { 0, 0, w, h }, rect.position, 100))
		DrawTranslucentBox(out, rect, tone); // an indexed surface: no ARGB, so the palette box
}

bool DrawSheetShadow(const Surface &out, Rectangle rect)
{
	const uint32_t shade = PackArgb(FieldShadowAlpha, 0, 0, 0);
	return BlitArgbScaled(out, &shade, 1, SDL_Rect { 0, 0, 1, 1 },
	    { rect.position + Displacement { -FieldShadowOffset, FieldShadowOffset }, rect.size }, 100);
}

namespace {

/**
 * @brief The bars' frame and ten-percent marks - the HUD XP bar's two greys (qol/xpbar.cpp's FrameColor and
 * TickColor, PAL16_GRAY + 7 and + 10), so the sheet's bars and the HUD's read as one family.
 */
constexpr uint32_t BarFrameRgb = 0x737373;
constexpr uint8_t BarFrameIndex = PAL16_GRAY + 7;
constexpr uint32_t BarTickRgb = 0x4C4C4C;
constexpr uint8_t BarTickIndex = PAL16_GRAY + 10;
/** @brief The colour cycle: a band of light BarCycleWidth px from crest to crest, one crest per BarCycleMs across. */
constexpr float BarCycleWidth = 48.F;
constexpr float BarCycleMs = 1600.F;
/** @brief How far the cycle swings the fill's brightness either side of its own colour. */
constexpr float BarCycleSwing = 0.3F;

/** @brief @p rgb scaled by @p factor per channel, clamped. */
uint32_t ScaleRgb(uint32_t rgb, float factor)
{
	const auto channel = [&](int shift) {
		const float v = static_cast<float>((rgb >> shift) & 0xFF) * factor;
		return static_cast<uint32_t>(std::clamp(v, 0.F, 255.F)) << shift;
	};
	return channel(16) | channel(8) | channel(0);
}

} // namespace

int32_t SheetBarClockOverrideMs = -1;

void DrawSheetBar(const Surface &out, Rectangle rect, uint64_t value, uint64_t maximum, uint32_t rgb, uint8_t fallbackIndex,
    bool fromRight, int segments, int lineWidth, uint32_t lineRgb, uint8_t lineIndex)
{
	// User, 2026-09-26: "Make exp, mana and life bar color cycling. Add a thin frame around them and 10% verticals."
	const int lw = std::max(lineWidth, 1);
	const uint32_t frameRgb = lineRgb != 0 ? lineRgb : BarFrameRgb;
	const uint8_t frameIndex = lineRgb != 0 ? lineIndex : BarFrameIndex;
	const uint32_t tickRgb = lineRgb != 0 ? lineRgb : BarTickRgb;
	const uint8_t tickIndex = lineRgb != 0 ? lineIndex : BarTickIndex;
	const int x0 = rect.position.x;
	const int y0 = rect.position.y;
	const int innerWidth = rect.size.width - 2 * lw;
	const int innerHeight = rect.size.height - 2 * lw;
	if (innerWidth <= 0 || innerHeight <= 0)
		return;

	// The groove: the old recess's dark translucent fill, inside the frame.
	const uint32_t groove = PackArgb(220, 8, 7, 6);
	BlitArgbScaled(out, &groove, 1, SDL_Rect { 0, 0, 1, 1 }, { { x0 + lw, y0 + lw }, { innerWidth, innerHeight } }, 100);

	// The fill, colour-cycling: bands of lighter and darker of the bar's own colour flow left to right, one
	// column at a time. Keyed to the wall clock, so it runs at the same speed whatever the frame rate.
	if (maximum > 0) {
		const uint64_t clamped = std::min(value, maximum);
		const int filled = static_cast<int>(clamped * static_cast<uint64_t>(innerWidth) / maximum);
		const uint32_t now = SheetBarClockOverrideMs >= 0 ? static_cast<uint32_t>(SheetBarClockOverrideMs) : SDL_GetTicks();
		const float phase = static_cast<float>(now % static_cast<uint32_t>(BarCycleMs)) / BarCycleMs;
		constexpr float TwoPi = 6.2831853F;
		// x counts from the edge the fill starts at, so a right-to-left bar's light flows leftwards.
		for (int x = 0; x < filled; x++) {
			const float wave = std::cos(TwoPi * (static_cast<float>(x) / BarCycleWidth - phase));
			const int column = fromRight ? innerWidth - 1 - x : x;
			FillRectRgb(out, x0 + lw + column, y0 + lw, 1, innerHeight, ScaleRgb(rgb, 1.F + BarCycleSwing * wave), fallbackIndex);
		}
	}

	// The marks, over the fill and the groove alike - the HUD bar's ten-percent ones unless told otherwise.
	const int parts = std::max(segments, 1);
	for (int mark = 1; mark < parts; mark++)
		FillRectRgb(out, x0 + lw + innerWidth * mark / parts - (lw - 1) / 2, y0 + lw, lw, innerHeight, tickRgb, tickIndex);

	// The frame on the rect's edge with its corners cut - the HUD bar's, 1px unless told otherwise.
	FillRectRgb(out, x0 + lw, y0, innerWidth, lw, frameRgb, frameIndex);
	FillRectRgb(out, x0 + lw, y0 + rect.size.height - lw, innerWidth, lw, frameRgb, frameIndex);
	FillRectRgb(out, x0, y0 + lw, lw, innerHeight, frameRgb, frameIndex);
	FillRectRgb(out, x0 + rect.size.width - lw, y0 + lw, lw, innerHeight, frameRgb, frameIndex);
}

void DrawSheetTextFitted(const Surface &out, string_view text, Rectangle rect, UiFlags flags)
{
	// Largest first. The flag and the metric table must name the same font, or the width measured is
	// not the width drawn and the step-down fails exactly when it is needed.
	struct FontStep {
		GameFontTables table;
		UiFlags flag;
	};
	constexpr FontStep Steps[] = {
		{ GameFont12, UiFlags::FontSize12 },
		{ GameFont11, UiFlags::FontSize11 },
		{ GameFont10, UiFlags::FontSize10 },
		{ GameFont9, UiFlags::FontSize9 },
	};
	UiFlags font = UiFlags::FontSize9; // the smallest, if nothing fits - it will clip, but least
	for (const FontStep &step : Steps) {
		if (GetLineWidth(text, step.table, 1) <= rect.size.width) {
			font = step.flag;
			break;
		}
	}
	DrawString(out, text, rect, { flags | font | UiFlags::Shadowed, 1 });
}

void OpenAdvancedStats()
{
	if (Open)
		return;
	// The inventory and the Abilities window stay open under it (dev note, 2026-09-27) - see advanced_stats.h.
	Open = true;
	ScrollOffset = 0;
}

void CloseAdvancedStats(bool restoreCovered)
{
	if (!Open) {
		CoveredInventory = false;
		CoveredAbilities = false;
		return;
	}
	Open = false;
	if (restoreCovered) {
		if (CoveredInventory)
			invflag = true;
		else if (CoveredAbilities)
			sbookflag = true;
	}
	CoveredInventory = false;
	CoveredAbilities = false;
}

void ToggleAdvancedStats()
{
	if (IsAdvancedStatsOpen())
		CloseAdvancedStats();
	else
		OpenAdvancedStats();
}

bool IsAdvancedStatsOpen()
{
	// It closes with the character sheet it is docked to (the C key, the sheet's X, or another left-panel window
	// taking the slot) - it would otherwise float in the middle of the screen. The inventory and the Abilities window
	// no longer close it (2026-09-27): they share the screen.
	if (Open && GetLeftPanelContent() != LeftPanelContent::Character) {
		Open = false;
		CoveredInventory = false;
		CoveredAbilities = false;
	}
	return Open;
}

Rectangle GetAdvancedStatsRect()
{
	// Flush against the character sheet's right edge (user, 2026-09-26 dev note: "advanced stats to open
	// adjacent flush with right border of main char screen") - Diablo II Resurrected's pairing. It was in
	// the inventory's bottom-right corner. At 960 wide it overlaps the inventory or Abilities window by 60px, and is
	// drawn over it there (2026-09-27).
	const Rectangle sheet = GetCharacterPanelRect();
	return { { sheet.position.x + sheet.size.width, sheet.position.y }, AdvancedPanelSize };
}

void DrawAdvancedStats(const Surface &out)
{
	if (!IsAdvancedStatsOpen() || InspectPlayer == nullptr)
		return;

	const Rectangle panel = GetAdvancedStatsRect();
	if (HasSidePanelArt()) {
		DrawSidePanelArt(out, panel.position);
	} else {
		DrawThemedFill(out, panel);
		DrawOrnateBorder(out, panel);
	}
	const Rectangle labelArea { { panel.position.x + AdvancedTitleMargin, panel.position.y + PanelTitleTop },
		{ panel.size.width - 2 * AdvancedTitleMargin, PanelTitleHeight } };
	DrawOutlinedString(out, _("ADVANCED STATS"), labelArea,
	    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);
	// No close button here: scrollrt.cpp draws the red X for whichever window holds the right-hand
	// slot, this one included, so one call cannot be forgotten and cannot be doubled.

	const std::vector<Row> rows = BuildRows(*InspectPlayer);
	const std::vector<int> tops = RowTops(rows, LastListHeight);
	ScrollOffset = std::clamp(ScrollOffset, 0, MaxScrollOffset());
	DrawScrollbar(out, panel);

	// Rows draw through a subregion covering only the list, so a row straddling its top or bottom edge
	// is cut there rather than spilling over the title or under the orb - the character sheet's rule.
	const Surface content = out.subregion(panel.position.x + AdvancedContentX, panel.position.y + AdvancedContentTop,
	    AdvancedContentWidth, AdvancedContentHeight);
	for (size_t i = 0; i < rows.size(); i++) {
		const int top = tops[i] - ScrollOffset;
		if (top + AdvancedRowHeight <= 0 || top >= AdvancedContentHeight)
			continue;
		DrawRow(content, rows[i], top);
	}
}

bool HandleAdvancedStatsClick(Point mousePosition)
{
	if (!IsAdvancedStatsOpen() || !GetAdvancedStatsRect().contains(mousePosition))
		return false;
	// Nothing in the list is clickable - it is a readout. The click is taken all the same, so the
	// ground under the window never sees it (the new-window rule's first point).
	return true;
}

bool HandleAdvancedStatsScroll(int notches)
{
	if (!IsAdvancedStatsOpen() || !GetAdvancedStatsRect().contains(MousePosition))
		return false;
	ScrollOffset = std::clamp(ScrollOffset + notches * AdvancedScrollStep, 0, MaxScrollOffset());
	return true; // consumed even at an end stop, so the wheel does not zoom the dungeon behind the window
}

} // namespace devilution::oracool
