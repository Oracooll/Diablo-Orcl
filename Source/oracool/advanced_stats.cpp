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
#include <string>
#include <vector>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "control.h" // sbookflag
#include "diablo.h"  // MousePosition
#include "engine/palette.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h" // invflag
#include "oracool/class_tree.h" // MovementSpeedPercent
#include "oracool/cold.h"       // ColdMasteryDamagePercent, ColdResistanceDivisor
#include "oracool/gems.h"       // GemLifePerKill, RuneManaPerKill
#include "oracool/hud_art.h"    // the shared side-panel canvas
#include "oracool/ornate_border.h"
#include "oracool/signets.h"
#include "panels/charpanel.hpp" // the sheet's derived readings - frames, block, steal
#include "player.h"
#include "utils/display.h" // gnScreenWidth
#include "utils/language.h"
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
/** @brief Clear of the canvas's painted side bezels - the grouped sheet's boxes use the same inset. */
constexpr int AdvancedContentX = 16;
constexpr int AdvancedContentWidth = AdvancedPanelSize.width - 2 * AdvancedContentX;
constexpr int AdvancedContentHeight = AdvancedContentBottom - AdvancedContentTop;
static_assert(AdvancedContentHeight > 0, "Advanced Stats has no content room left");
/** @brief One row's box, and the pitch between rows - 2px of canvas shows between neighbours, as in the mock. */
constexpr int AdvancedRowHeight = 20;
constexpr int AdvancedRowPitch = 22;
/** @brief A heading's strip is narrower than the rows, centred - "OFFENSE" in a small plate. */
constexpr int AdvancedHeadingWidth = 140;
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
	// The raw field, as the old sheet's "Armor pierce" row showed it.
	if (p._pIEnAc > 0)
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("Ignores {:d} of the target's armor")), p._pIEnAc) });

	// The fixed 3/5% and the random drain are independent and can both be active - the old sheet's Life
	// steal row, word for word in its arithmetic (player.cpp's steal branch; the random one is
	// GenerateRnd(dam / 8), zero to just under an eighth of the damage dealt).
	{
		const int fixedPct = GetSheetLifeStealPercent();
		const bool random = HasAnyOf(p._pIFlags, ItemSpecialEffect::RandomStealLife);
		if (fixedPct > 0 && random)
			rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("{:d}% +0-12% life stolen per hit")), fixedPct) });
		else if (fixedPct > 0)
			rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("{:d}% life stolen per hit")), fixedPct) });
		else if (random)
			rows.push_back({ RowKind::Bonus, std::string(_("0-12% life stolen per hit")) });
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
	if (const int mana = RuneManaPerKill(p); mana > 0)
		rows.push_back({ RowKind::Bonus, fmt::format(fmt::runtime(_("+{:d} mana per kill")), mana) });
}

/** @brief OTHER: movement, find, the curses a cursed item carries, and two facts. */
void AddOther(const Player &p, std::vector<Row> &rows)
{
	// 100 is a plain walk (class_tree.h); a slow reads as a penalty, in red.
	if (const int move = MovementSpeedPercent(p) - 100; move != 0) {
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
		rows.push_back({ RowKind::Curse, std::string(_("All resistances zero")) });

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

void DrawRow(const Surface &content, const Row &row, int top)
{
	if (row.kind == RowKind::Heading) {
		const Rectangle strip { { (AdvancedContentWidth - AdvancedHeadingWidth) / 2, top }, { AdvancedHeadingWidth, AdvancedRowHeight } };
		DrawSheetBox(content, strip, SheetBoxTone::Heading);
		DrawSheetTextFitted(content, row.text, strip, UiFlags::ColorGold | UiFlags::AlignCenter | UiFlags::VerticalCenter);
		return;
	}
	const Rectangle box { { 0, top }, { AdvancedContentWidth, AdvancedRowHeight } };
	SheetBoxTone tone = SheetBoxTone::Plain;
	UiFlags color = UiFlags::ColorWhite;
	switch (row.kind) {
	case RowKind::Bonus:
		tone = SheetBoxTone::Bonus;
		color = UiFlags::ColorBlue;
		break;
	case RowKind::Curse:
		tone = SheetBoxTone::Curse;
		color = UiFlags::ColorRed;
		break;
	case RowKind::Fact:
	case RowKind::Heading:
		break;
	}
	DrawSheetBox(content, box, tone);
	// Inset so a long line that steps down a font size still keeps clear of the box's edge.
	const Rectangle textRect { { box.position.x + 6, box.position.y }, { box.size.width - 12, box.size.height } };
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

} // namespace

void DrawSheetBox(const Surface &out, Rectangle rect, SheetBoxTone tone)
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

void DrawSheetBar(const Surface &out, Rectangle rect, uint64_t value, uint64_t maximum, uint32_t rgb, uint8_t fallbackIndex)
{
	DrawSheetBox(out, rect, SheetBoxTone::Recess);
	const int innerWidth = rect.size.width - 2;
	const int innerHeight = rect.size.height - 2;
	if (innerWidth <= 0 || innerHeight <= 0 || maximum == 0)
		return;
	const uint64_t clamped = std::min(value, maximum);
	const int filled = static_cast<int>(clamped * static_cast<uint64_t>(innerWidth) / maximum);
	if (filled > 0)
		FillRectRgb(out, rect.position.x + 1, rect.position.y + 1, filled, innerHeight, rgb, fallbackIndex);
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
	// Cover the slot's current holder and remember it (advanced_stats.h). The flags only - CloseInventory
	// would return a held item and reset things the ordinary close puts back untouched.
	CoveredInventory = invflag;
	CoveredAbilities = !invflag && sbookflag;
	invflag = false;
	sbookflag = false;
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
	// Something opened the inventory or the Abilities window over the top of this one: that window is
	// the player's choice now, so this one closes - and forgets what it covered, since the slot's new
	// holder is already up. See the file comment in advanced_stats.h.
	if (Open && (invflag || sbookflag)) {
		Open = false;
		CoveredInventory = false;
		CoveredAbilities = false;
	}
	return Open;
}

Rectangle GetAdvancedStatsRect()
{
	// Bottom-right, the inventory's and the Abilities window's own corner (inventory_layout.cpp), so it
	// lands exactly over whichever of them it covers.
	return { { gnScreenWidth - AdvancedPanelSize.width, BottomDockedTop(AdvancedPanelSize.height) }, AdvancedPanelSize };
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
	LastListHeight = static_cast<int>(rows.size()) * AdvancedRowPitch;
	ScrollOffset = std::clamp(ScrollOffset, 0, MaxScrollOffset());
	DrawScrollbar(out, panel);

	// Rows draw through a subregion covering only the list, so a row straddling its top or bottom edge
	// is cut there rather than spilling over the title or under the orb - the character sheet's rule.
	const Surface content = out.subregion(panel.position.x + AdvancedContentX, panel.position.y + AdvancedContentTop,
	    AdvancedContentWidth, AdvancedContentHeight);
	for (size_t i = 0; i < rows.size(); i++) {
		const int top = static_cast<int>(i) * AdvancedRowPitch - ScrollOffset;
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
