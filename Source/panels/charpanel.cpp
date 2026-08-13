#include "panels/charpanel.hpp"

#include <cstdint>

#include <algorithm>
#include <string>

#include <fmt/format.h>
#include <function_ref.hpp>

#include "control.h"
#include "engine/load_clx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "panels/ui_panels.hpp"
#include "player.h"
#include "playerdat.hpp"
#include "options.h"
#include "oracool/oracool.h"
#include "oracool/ornate_border.h"
#include "qol/stash.h"
#include "utils/display.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/surface_to_clx.hpp"

namespace devilution {

OptionalOwnedClxSpriteList pChrButtons;

namespace {

struct StyledText {
	UiFlags style;
	std::string text;
	int spacing = 1;
};

struct PanelEntry {
	std::string label;
	Point position;
	int length;
	int labelLength;                                               // max label's length - used for line wrapping
	std::optional<tl::function_ref<StyledText()>> statDisplayFunc; // function responsible for displaying stat
};

UiFlags GetBaseStatColor(CharacterAttribute attr)
{
	const int base = InspectPlayer->GetBaseAttributeValue(attr);
	return base >= 255 ? UiFlags::ColorWhitegold : UiFlags::ColorWhite;
}

UiFlags GetCurrentStatColor(CharacterAttribute attr)
{
	UiFlags style = UiFlags::ColorWhite;
	int current = InspectPlayer->GetCurrentAttributeValue(attr);
	int base = InspectPlayer->GetBaseAttributeValue(attr);
	if (current > base)
		style = UiFlags::ColorBlue;
	if (current < base)
		style = UiFlags::ColorRed;
	return style;
}

UiFlags GetValueColor(int value, bool flip = false)
{
	UiFlags style = UiFlags::ColorWhite;
	if (value > 0)
		style = (flip ? UiFlags::ColorRed : UiFlags::ColorBlue);
	if (value < 0)
		style = (flip ? UiFlags::ColorBlue : UiFlags::ColorRed);
	return style;
}

UiFlags GetMaxManaColor()
{
	return InspectPlayer->_pMaxMana > InspectPlayer->_pMaxManaBase ? UiFlags::ColorBlue : UiFlags::ColorWhite;
}

UiFlags GetMaxHealthColor()
{
	return InspectPlayer->_pMaxHP > InspectPlayer->_pMaxHPBase ? UiFlags::ColorBlue : UiFlags::ColorWhite;
}

std::pair<int, int> GetDamage()
{
	int damageMod = InspectPlayer->_pIBonusDamMod;
	if (InspectPlayer->InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow && InspectPlayer->_pClass != HeroClass::Rogue) {
		damageMod += InspectPlayer->_pDamageMod / 2;
	} else {
		damageMod += InspectPlayer->_pDamageMod;
	}
	int mindam = InspectPlayer->_pIMinDam + InspectPlayer->_pIBonusDam * InspectPlayer->_pIMinDam / 100 + damageMod;
	int maxdam = InspectPlayer->_pIMaxDam + InspectPlayer->_pIBonusDam * InspectPlayer->_pIMaxDam / 100 + damageMod;
	return { mindam, maxdam };
}

StyledText GetResistInfo(int8_t resist)
{
	UiFlags style = UiFlags::ColorBlue;
	if (resist == 0)
		style = UiFlags::ColorWhite;
	else if (resist < 0)
		style = UiFlags::ColorRed;
	else if (resist >= MaxResistance)
		style = UiFlags::ColorWhitegold;

	return { style, StrCat(resist, "%") };
}

constexpr int LeftColumnLabelX = 88;
constexpr int TopRightLabelX = 211;
constexpr int RightColumnLabelX = 253;

constexpr int LeftColumnLabelWidth = 76;
constexpr int RightColumnLabelWidth = 68;

// Indices in `panelEntries`.
constexpr unsigned AttributeHeaderEntryIndices[2] = { 5, 6 };
constexpr unsigned GoldHeaderEntryIndex = 16;

PanelEntry panelEntries[] = {
	{ "", { 9, 14 }, 150, 0,
	    []() { return StyledText { UiFlags::ColorWhite, InspectPlayer->_pName }; } },
	{ "", { 161, 14 }, 149, 0,
	    []() { return StyledText { UiFlags::ColorWhite, std::string(_(PlayersData[static_cast<std::size_t>(InspectPlayer->_pClass)].className)) }; } },

	{ N_("Level"), { 57, 52 }, 57, 45,
	    []() { return StyledText { UiFlags::ColorWhite, StrCat(InspectPlayer->_pLevel) }; } },
	{ N_("Experience"), { TopRightLabelX, 52 }, 99, 91,
	    []() {
	        int spacing = ((InspectPlayer->_pExperience >= 10000000000ULL) ? -1 : (InspectPlayer->_pExperience >= 1000000000) ? 0
	                                                                                                                          : 1);
	        return StyledText { UiFlags::ColorWhite, FormatInteger(InspectPlayer->_pExperience), spacing };
	    } },
	{ N_("Next level"), { TopRightLabelX, 80 }, 99, 198,
	    []() {
	        if (InspectPlayer->_pLevel == MaxCharacterLevel) {
		        return StyledText { UiFlags::ColorWhitegold, std::string(_("None")) };
	        }
	        int spacing = ((InspectPlayer->_pNextExper >= 10000000000ULL) ? -1 : (InspectPlayer->_pNextExper >= 1000000000) ? 0
	                                                                                                                        : 1);
	        return StyledText { UiFlags::ColorWhite, FormatInteger(InspectPlayer->_pNextExper), spacing };
	    } },

	{ N_("Base"), { LeftColumnLabelX, /* set dynamically */ 0 }, 0, 44, {} },
	{ N_("Now"), { 135, /* set dynamically */ 0 }, 0, 44, {} },
	{ N_("Strength"), { LeftColumnLabelX, 135 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Strength), StrCat(InspectPlayer->_pBaseStr) }; } },
	{ "", { 135, 135 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Strength), StrCat(InspectPlayer->_pStrength) }; } },
	{ N_("Magic"), { LeftColumnLabelX, 163 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Magic), StrCat(InspectPlayer->_pBaseMag) }; } },
	{ "", { 135, 163 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Magic), StrCat(InspectPlayer->_pMagic) }; } },
	{ N_("Dexterity"), { LeftColumnLabelX, 191 }, 45, LeftColumnLabelWidth, []() { return StyledText { GetBaseStatColor(CharacterAttribute::Dexterity), StrCat(InspectPlayer->_pBaseDex) }; } },
	{ "", { 135, 191 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Dexterity), StrCat(InspectPlayer->_pDexterity) }; } },
	{ N_("Vitality"), { LeftColumnLabelX, 219 }, 45, LeftColumnLabelWidth, []() { return StyledText { GetBaseStatColor(CharacterAttribute::Vitality), StrCat(InspectPlayer->_pBaseVit) }; } },
	{ "", { 135, 219 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Vitality), StrCat(InspectPlayer->_pVitality) }; } },
	{ N_("Points to distribute"), { LeftColumnLabelX, 248 }, 45, LeftColumnLabelWidth,
	    []() {
	        InspectPlayer->_pStatPts = std::min(CalcStatDiff(*InspectPlayer), InspectPlayer->_pStatPts);
	        return StyledText { UiFlags::ColorRed, (InspectPlayer->_pStatPts > 0 ? StrCat(InspectPlayer->_pStatPts) : "") };
	    } },

	{ N_("Gold"), { TopRightLabelX, /* set dynamically */ 0 }, 0, 98, {} },
	{ "", { TopRightLabelX, 127 }, 99, 0,
	    []() {
	        // Oracool: picked-up and sold gold goes to the shared Stash pool now, so the total
	        // shown here needs to include it - matching the store screen's TotalPlayerGold().
	        int gold = InspectPlayer->_pGold;
	        if (oracool::IsSinglePlayer())
	            gold += Stash.gold;
	        return StyledText { UiFlags::ColorWhite, FormatInteger(gold) };
	    } },

	{ N_("Armor class"), { RightColumnLabelX, 163 }, 57, RightColumnLabelWidth,
	    []() { return StyledText { GetValueColor(InspectPlayer->_pIBonusAC), StrCat(InspectPlayer->GetArmor() + InspectPlayer->_pLevel * 2) }; } },
	{ N_("To hit"), { RightColumnLabelX, 191 }, 57, RightColumnLabelWidth,
	    []() { return StyledText { GetValueColor(InspectPlayer->_pIBonusToHit), StrCat(InspectPlayer->InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow ? InspectPlayer->GetRangedToHit() : InspectPlayer->GetMeleeToHit(), "%") }; } },
	{ N_("Damage"), { RightColumnLabelX, 219 }, 57, RightColumnLabelWidth,
	    []() {
	        std::pair<int, int> dmg = GetDamage();
	        int spacing = ((dmg.first >= 100) ? -1 : 1);
	        return StyledText { GetValueColor(InspectPlayer->_pIBonusDam), StrCat(dmg.first, "-", dmg.second), spacing };
	    } },

	{ N_("Life"), { LeftColumnLabelX, 284 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetMaxHealthColor(), StrCat(InspectPlayer->_pMaxHP >> 6) }; } },
	{ "", { 135, 284 }, 45, 0,
	    []() { return StyledText { (InspectPlayer->_pHitPoints != InspectPlayer->_pMaxHP ? UiFlags::ColorRed : GetMaxHealthColor()), StrCat(InspectPlayer->_pHitPoints >> 6) }; } },
	{ N_("Mana"), { LeftColumnLabelX, 312 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetMaxManaColor(), StrCat(InspectPlayer->_pMaxMana >> 6) }; } },
	{ "", { 135, 312 }, 45, 0,
	    []() { return StyledText { (InspectPlayer->_pMana != InspectPlayer->_pMaxMana ? UiFlags::ColorRed : GetMaxManaColor()), StrCat(InspectPlayer->_pMana >> 6) }; } },

	{ N_("Resist magic"), { RightColumnLabelX, 256 }, 57, RightColumnLabelWidth,
	    []() { return GetResistInfo(InspectPlayer->_pMagResist); } },
	{ N_("Resist fire"), { RightColumnLabelX, 284 }, 57, RightColumnLabelWidth,
	    []() { return GetResistInfo(InspectPlayer->_pFireResist); } },
	{ N_("Resist lightning"), { RightColumnLabelX, 313 }, 57, RightColumnLabelWidth,
	    []() { return GetResistInfo(InspectPlayer->_pLghtResist); } },
};

OptionalOwnedClxSpriteList Panel;

// Oracool V1: the sheet's own 340x720 rect, matching the waypoint list and quest log, with the
// same title band above a separator rule:
//   0..24     top margin
//   24..74    label band, "CHARACTER"
//   74..77    separator rule
//   77..101   gap below the rule
//   101..696  content area
//   696..720  bottom margin
//
// The content itself is the fixed 320x352 block charbg.clx sizes - field boxes, labels and stat
// buttons, all positioned against it. Rather than re-laying out every entry, the block is centred
// in the content area as a unit and GetPanelPosition(UiPanels::Character, ...) returns its origin,
// so drawing, the stat buttons and their hit-testing all move together.
constexpr Size CharPanelSize { 340, 720 };
constexpr int CharPanelMargin = 24;
constexpr int CharLabelHeight = 50;
constexpr int CharContentTop = CharPanelMargin + CharLabelHeight + oracool::OrnateBorderWidth + CharPanelMargin;
constexpr Size CharContentSize { 320, 352 };
static_assert(CharContentTop + CharContentSize.height + CharPanelMargin <= CharPanelSize.height,
    "Character sheet content does not fit under its title band");

constexpr int PanelFieldHeight = 24;
constexpr int PanelFieldPaddingTop = 3;
constexpr int PanelFieldPaddingBottom = 3;
constexpr int PanelFieldInnerHeight = PanelFieldHeight - PanelFieldPaddingTop - PanelFieldPaddingBottom;

void DrawPanelField(const Surface &out, Point pos, int len, ClxSprite left, ClxSprite middle, ClxSprite right)
{
	RenderClxSprite(out, left, pos);
	pos.x += left.width();
	len -= left.width() + right.width();
	RenderClxSprite(out.subregion(pos.x, pos.y, len, middle.height()), middle, Point { 0, 0 });
	pos.x += len;
	RenderClxSprite(out, right, pos);
}

void DrawShadowString(const Surface &out, const PanelEntry &entry)
{
	if (entry.label.empty())
		return;

	constexpr int Spacing = 0;
	const string_view textStr = LanguageTranslate(entry.label);
	string_view text;
	std::string wrapped;
	if (entry.labelLength > 0) {
		wrapped = WordWrapString(textStr, entry.labelLength, GameFont12, Spacing);
		text = wrapped;
	} else {
		text = textStr;
	}

	UiFlags style = UiFlags::VerticalCenter;

	Point labelPosition = entry.position;

	if (entry.length == 0) {
		style |= UiFlags::AlignCenter;
	} else {
		style |= UiFlags::AlignRight;
		labelPosition += Displacement { -entry.labelLength - (IsSmallFontTall() ? 2 : 3), 0 };
	}

	// If the text is less tall then the field, we center it vertically relative to the field.
	// Otherwise, we draw from the top of the field.
	const int textHeight = (std::count(wrapped.begin(), wrapped.end(), '\n') + 1) * GetLineHeight(wrapped, GameFont12);
	const int labelHeight = std::max(PanelFieldHeight, textHeight);

	DrawString(out, text, { labelPosition + Displacement { -2, 2 }, { entry.labelLength, labelHeight } }, { style | UiFlags::ColorBlack, Spacing });
	DrawString(out, text, { labelPosition, { entry.labelLength, labelHeight } }, { style | UiFlags::ColorWhite, Spacing });
}

void DrawStatButtons(const Surface &out)
{
	if (InspectPlayer->_pStatPts > 0 && !IsInspectingPlayer()) {
		if (InspectPlayer->_pBaseStr < 255)
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 157 }), (*pChrButtons)[chrbtn[static_cast<size_t>(CharacterAttribute::Strength)] ? 2 : 1]);
		if (InspectPlayer->_pBaseMag < 255)
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 185 }), (*pChrButtons)[chrbtn[static_cast<size_t>(CharacterAttribute::Magic)] ? 4 : 3]);
		if (InspectPlayer->_pBaseDex < 255)
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 214 }), (*pChrButtons)[chrbtn[static_cast<size_t>(CharacterAttribute::Dexterity)] ? 6 : 5]);
		if (InspectPlayer->_pBaseVit < 255)
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 242 }), (*pChrButtons)[chrbtn[static_cast<size_t>(CharacterAttribute::Vitality)] ? 8 : 7]);
	}

	if (*sgOptions.Oracool.resetStatsButton && !gbIsMultiplayer && !IsInspectingPlayer()) {
		// Sits just to the right of the "Points to distribute" value field, which occupies
		// panel-relative x in [LeftColumnLabelX, LeftColumnLabelX + 45] = [88, 133] at this
		// row (see the panelEntries table above) - close enough to read as "reset the points
		// shown right here" rather than a disconnected button elsewhere on the panel.
		// Position/size shared with control.cpp's press/release hit-testing via
		// ResetStatsButtonPosition/ResetStatsButtonSize (control.h) - keep using those
		// constants if this ever moves again, not a new hardcoded literal here.
		const Point position = GetPanelPosition(UiPanels::Character, ResetStatsButtonPosition);
		// Oracool: a circular-arrow glyph (Unicode U+21BA), then a plain "R", didn't read well
		// against the game's actual bitmap font - now a gold "RESET" word label instead, turning
		// white while pressed for visible click feedback.
		DrawString(out, "RESET", { position, ResetStatsButtonSize }, { UiFlags::AlignCenter | UiFlags::VerticalCenter | (resetStatsButtonDown ? UiFlags::ColorWhite : UiFlags::ColorGold) });
	}
}

} // namespace

void LoadCharPanel()
{
	// Oracool V1: charbg.clx is loaded for its DIMENSIONS only - it is no longer rendered. The
	// window's background is now the shared theme (half-transparent fill under the ornate bevel),
	// drawn per-frame in DrawChr. Leaving the surface at index 0 means SurfaceToClx below encodes
	// everything the field boxes and labels do not cover as transparent runs, so this composition
	// becomes an overlay on that background instead of a page with a parchment baked in.
	OptionalOwnedClxSpriteList background = LoadClx("data\\charbg.clx");
	OwnedSurface out((*background)[0].width(), (*background)[0].height());
	background = std::nullopt;

	{
		OwnedClxSpriteList boxLeft = LoadClx("data\\boxleftend.clx");
		OwnedClxSpriteList boxMiddle = LoadClx("data\\boxmiddle.clx");
		OwnedClxSpriteList boxRight = LoadClx("data\\boxrightend.clx");

		const bool isSmallFontTall = IsSmallFontTall();
		const int attributeHeadersY = isSmallFontTall ? 112 : 114;
		for (unsigned i : AttributeHeaderEntryIndices) {
			panelEntries[i].position.y = attributeHeadersY;
		}
		panelEntries[GoldHeaderEntryIndex].position.y = isSmallFontTall ? 105 : 106;

		for (auto &entry : panelEntries) {
			if (entry.statDisplayFunc) {
				DrawPanelField(out, entry.position, entry.length, boxLeft[0], boxMiddle[0], boxRight[0]);
			}
			DrawShadowString(out, entry);
		}
	}

	// Index 0 is TRANSPARENT. SurfaceToClx's transparentColor defaults to nullopt - nothing
	// transparent - so with the charbg.clx render skipped above, every pixel the field boxes and
	// labels do not cover was encoded as opaque black. That rendered the sheet as a solid black
	// rectangle sitting on the themed panel instead of an overlay on it.
	Panel = SurfaceToClx(out, 1, 0);
}

void FreeCharPanel()
{
	Panel = std::nullopt;
}

Rectangle GetCharacterPanelRect()
{
	return { { 0, 0 }, CharPanelSize };
}

Point GetCharacterContentOrigin()
{
	// Top-aligned under the separator, not centred in the content area. Centring it left a large
	// empty band between the title and the first field, which read as a mistake rather than as
	// spacing - the block is 352 tall in a 595 area, so the slack has to go somewhere, and below
	// the content is the less conspicuous place for it.
	const Rectangle panel = GetCharacterPanelRect();
	return { panel.position.x + (CharPanelSize.width - CharContentSize.width) / 2,
		panel.position.y + CharContentTop };
}

void DrawChr(const Surface &out)
{
	// Oracool V1: shared theme and geometry, matching the waypoint list and quest log.
	const Rectangle panel = GetCharacterPanelRect();
	oracool::DrawThemedFill(out, panel);
	oracool::DrawOrnateBorder(out, panel);

	const Rectangle labelArea { { panel.position.x + CharPanelMargin, panel.position.y + CharPanelMargin },
		{ panel.size.width - 2 * CharPanelMargin, CharLabelHeight } };
	oracool::DrawOutlinedString(out, _("CHARACTER"), labelArea,
	    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);
	oracool::DrawOrnateSeparator(out,
	    { panel.position.x + CharPanelMargin, panel.position.y + CharPanelMargin + CharLabelHeight },
	    panel.size.width - 2 * CharPanelMargin);

	// Everything below is positioned against the content origin, which GetPanelPosition returns.
	Point pos = GetPanelPosition(UiPanels::Character, { 0, 0 });
	RenderClxSprite(out, (*Panel)[0], pos);
	for (auto &entry : panelEntries) {
		if (entry.statDisplayFunc) {
			StyledText tmp = (*entry.statDisplayFunc)();
			DrawString(
			    out,
			    tmp.text,
			    { entry.position + Displacement { pos.x, pos.y + PanelFieldPaddingTop }, { entry.length, PanelFieldInnerHeight } },
			    { UiFlags::AlignCenter | UiFlags::VerticalCenter | tmp.style, tmp.spacing });
		}
	}
	DrawStatButtons(out);
}

} // namespace devilution
