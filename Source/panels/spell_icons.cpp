#include "panels/spell_icons.hpp"

#include <algorithm>
#include <cstdint>

#include "engine.h"
#include "engine/load_cel.hpp"
#include "engine/load_clx.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "init.h"
#include "oracool/sprite_scale.h"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

namespace {

#ifdef UNPACKED_MPQS
OptionalOwnedClxSpriteList LargeSpellIconsBackground;
OptionalOwnedClxSpriteList SmallSpellIconsBackground;
#endif

OptionalOwnedClxSpriteList SmallSpellIcons;
OptionalOwnedClxSpriteList LargeSpellIcons;

uint8_t SplTransTbl[256];

/** Maps from SpellID to spelicon.cel frame number. */
const uint8_t SpellITbl[] = {
	26,
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	27,
	12,
	11,
	17,
	15,
	13,
	17,
	18,
	10,
	19,
	14,
	20,
	22,
	23,
	24,
	21,
	25,
	28,
	36,
	37,
	38,
	41,
	40,
	39,
	9,
	35,
	29,
	50,
	50,
	49,
	45,
	46,
	42,
	44,
	47,
	48,
	43,
	34,
	34,
	34,
	34,
	34,
	// Oracool: the seven Paladin skills - Charge, then Zeal, Hammer of Faith, Blessed Shield, Fist of
	// the Heavens, Shield Bash, Blessed Hammer. Frame 26 is the EMPTY plate, the same square every
	// skill icon sits on.
	//
	// Their real art is not in this sheet at all: it is ui\paladin_skill_icons.png, drawn through
	// oracool::DrawPaladinSkillIcon, and every site that draws a readied spell's icon asks
	// oracool::TryDrawSkillSpellIcon first and only falls back here. So these entries are what shows
	// if a NEW draw site forgets to ask - a bare plate rather than another spell's symbol, which is
	// the failure mode that retired the old Heal Other stand-in. They are deliberately not pointed at
	// a lookalike frame for that reason.
	26,
	26,
	26,
	26,
	26,
	26,
	26,
	// Ice Bolt, and the same argument as the seven above it: the Sorceress's tree draws its own art
	// (sorc_tree_icons.png, through TryDrawSkillSpellIcon), so this entry is only what shows if a new
	// draw site forgets to ask. A bare plate says "unfinished"; another spell's symbol would say
	// something false.
	26,
	// The eight Round 2 cold spells, same argument.
	26,
	26,
	26,
	26,
	26,
	26,
	26,
	26,
	// The ten Round 3 bow skills, same argument - the Rogue's tree art is rogue_tree_icons.png.
	26,
	26,
	26,
	26,
	26,
	26,
	26,
	26,
	26,
	26,
};
static_assert(sizeof(SpellITbl) / sizeof(SpellITbl[0]) == MAX_SPELLS,
    "every SpellID needs an icon frame - this table is indexed by the enum");

} // namespace

void LoadLargeSpellIcons()
{
	if (!gbIsHellfire) {
#ifdef UNPACKED_MPQS
		LargeSpellIcons = LoadClx("ctrlpan\\spelicon_fg.clx");
		LargeSpellIconsBackground = LoadClx("ctrlpan\\spelicon_bg.clx");
#else
		LargeSpellIcons = LoadCel("ctrlpan\\spelicon", SPLICONLENGTH);
#endif
	} else {
#ifdef UNPACKED_MPQS
		LargeSpellIcons = LoadClx("data\\spelicon_fg.clx");
		LargeSpellIconsBackground = LoadClx("data\\spelicon_bg.clx");
#else
		LargeSpellIcons = LoadCel("data\\spelicon", SPLICONLENGTH);
#endif
	}
	SetSpellTrans(SpellType::Skill);
}

void FreeLargeSpellIcons()
{
#ifdef UNPACKED_MPQS
	LargeSpellIconsBackground = std::nullopt;
#endif
	LargeSpellIcons = std::nullopt;
}

void LoadSmallSpellIcons()
{
#ifdef UNPACKED_MPQS
	SmallSpellIcons = LoadClx("data\\spelli2_fg.clx");
	SmallSpellIconsBackground = LoadClx("data\\spelli2_bg.clx");
#else
	SmallSpellIcons = LoadCel("data\\spelli2", 37);
#endif
}

void FreeSmallSpellIcons()
{
#ifdef UNPACKED_MPQS
	SmallSpellIconsBackground = std::nullopt;
#endif
	SmallSpellIcons = std::nullopt;
}

void DrawLargeSpellIcon(const Surface &out, Point position, SpellID spell)
{
#ifdef UNPACKED_MPQS
	ClxDrawTRN(out, position, (*LargeSpellIconsBackground)[0], SplTransTbl);
#endif
	ClxDrawTRN(out, position, (*LargeSpellIcons)[SpellITbl[static_cast<int8_t>(spell)]], SplTransTbl);
}

void DrawSmallSpellIcon(const Surface &out, Point position, SpellID spell)
{
	// Oracool: guarded. The Abilities window now draws the empty plate (SpellID::Null, frame 26)
	// behind every skill icon, and that runs from the row renderer rather than from the spell list -
	// a path that can be reached before LoadSmallSpellIcons() has run.
	if (!SmallSpellIcons)
		return;
#ifdef UNPACKED_MPQS
	ClxDrawTRN(out, position, (*SmallSpellIconsBackground)[0], SplTransTbl);
#endif
	ClxDrawTRN(out, position, (*SmallSpellIcons)[SpellITbl[static_cast<int8_t>(spell)]], SplTransTbl);
}

Size GetSmallSpellIconSize()
{
	if (!SmallSpellIcons)
		return { 37, 38 }; // nominal fallback; only hit if called before LoadSmallSpellIcons()
	return { (*SmallSpellIcons)[0].width(), (*SmallSpellIcons)[0].height() };
}

void DrawLargeSpellIconBorder(const Surface &out, Point position, uint8_t color)
{
	const int width = (*LargeSpellIcons)[0].width();
	const int height = (*LargeSpellIcons)[0].height();
	UnsafeDrawBorder2px(out, Rectangle { Point { position.x, position.y - height + 1 }, Size { width, height } }, color);
}

void DrawSmallSpellIconBorder(const Surface &out, Point position)
{
	const int width = (*SmallSpellIcons)[0].width();
	const int height = (*SmallSpellIcons)[0].height();
	UnsafeDrawBorder2px(out, Rectangle { Point { position.x, position.y - height + 1 }, Size { width, height } }, SplTransTbl[PAL8_YELLOW + 2]);
}

void SetSpellTrans(SpellType t)
{
	if (t == SpellType::Skill) {
		for (int i = 0; i < 128; i++)
			SplTransTbl[i] = i;
	}
	for (int i = 128; i < 256; i++)
		SplTransTbl[i] = i;
	SplTransTbl[255] = 0;

	switch (t) {
	case SpellType::Spell:
		SplTransTbl[PAL8_YELLOW] = PAL16_BLUE + 1;
		SplTransTbl[PAL8_YELLOW + 1] = PAL16_BLUE + 3;
		SplTransTbl[PAL8_YELLOW + 2] = PAL16_BLUE + 5;
		for (int i = PAL16_BLUE; i < PAL16_BLUE + 16; i++) {
			SplTransTbl[PAL16_BEIGE - PAL16_BLUE + i] = i;
			SplTransTbl[PAL16_YELLOW - PAL16_BLUE + i] = i;
			SplTransTbl[PAL16_ORANGE - PAL16_BLUE + i] = i;
		}
		break;
	case SpellType::Scroll:
		SplTransTbl[PAL8_YELLOW] = PAL16_BEIGE + 1;
		SplTransTbl[PAL8_YELLOW + 1] = PAL16_BEIGE + 3;
		SplTransTbl[PAL8_YELLOW + 2] = PAL16_BEIGE + 5;
		for (int i = PAL16_BEIGE; i < PAL16_BEIGE + 16; i++) {
			SplTransTbl[PAL16_YELLOW - PAL16_BEIGE + i] = i;
			SplTransTbl[PAL16_ORANGE - PAL16_BEIGE + i] = i;
		}
		break;
	case SpellType::Charges:
		SplTransTbl[PAL8_YELLOW] = PAL16_ORANGE + 1;
		SplTransTbl[PAL8_YELLOW + 1] = PAL16_ORANGE + 3;
		SplTransTbl[PAL8_YELLOW + 2] = PAL16_ORANGE + 5;
		for (int i = PAL16_ORANGE; i < PAL16_ORANGE + 16; i++) {
			SplTransTbl[PAL16_BEIGE - PAL16_ORANGE + i] = i;
			SplTransTbl[PAL16_YELLOW - PAL16_ORANGE + i] = i;
		}
		break;
	case SpellType::Invalid:
		SplTransTbl[PAL8_YELLOW] = PAL16_GRAY + 1;
		SplTransTbl[PAL8_YELLOW + 1] = PAL16_GRAY + 3;
		SplTransTbl[PAL8_YELLOW + 2] = PAL16_GRAY + 5;
		for (int i = PAL16_GRAY; i < PAL16_GRAY + 15; i++) {
			SplTransTbl[PAL16_BEIGE - PAL16_GRAY + i] = i;
			SplTransTbl[PAL16_YELLOW - PAL16_GRAY + i] = i;
			SplTransTbl[PAL16_ORANGE - PAL16_GRAY + i] = i;
		}
		SplTransTbl[PAL16_BEIGE + 15] = 0;
		SplTransTbl[PAL16_YELLOW + 15] = 0;
		SplTransTbl[PAL16_ORANGE + 15] = 0;
		break;
	case SpellType::Skill:
		break;
	}
}

void SetSpellTransGreen()
{
	// Oracool: user request (2026-08-15) - a green plate for the Skills sheet, replacing the pink
	// the user never warmed to. The green lives in the PAL8_GREEN ramp LoadPalette injects; it is 8
	// shades against the source art's 16, so each pair of source shades shares one green (i / 2),
	// which at plate size reads as the same bevel with slightly simpler shading.
	for (int i = 0; i < 256; i++)
		SplTransTbl[i] = static_cast<uint8_t>(i);
	SplTransTbl[255] = 0;

	SplTransTbl[PAL8_YELLOW] = PAL8_GREEN + 1;
	SplTransTbl[PAL8_YELLOW + 1] = PAL8_GREEN + 2;
	SplTransTbl[PAL8_YELLOW + 2] = PAL8_GREEN + 3;
	for (int i = 0; i < 16; i++) {
		const auto green = static_cast<uint8_t>(PAL8_GREEN + std::min(i / 2, PAL8_GREEN_SHADES - 1));
		SplTransTbl[PAL16_BEIGE + i] = green;
		SplTransTbl[PAL16_YELLOW + i] = green;
		SplTransTbl[PAL16_ORANGE + i] = green;
	}
	// The ramp-end entries follow SetSpellTrans(Invalid)'s convention - transparent, not solid.
	SplTransTbl[PAL16_BEIGE + 15] = 0;
	SplTransTbl[PAL16_YELLOW + 15] = 0;
	SplTransTbl[PAL16_ORANGE + 15] = 0;
}

void SetSpellTransDarkGrey()
{
	// Oracool: user request (2026-08-15) - "make the inactive skill background darker gray". The
	// SpellType::Invalid table above maps each ramp onto PAL16_GRAY at the SAME within-ramp index,
	// which reads pale next to the pink plates around it. This maps four shades further down the
	// ramp instead (higher index = darker in the game's palettes), saturated at the ramp's last
	// opaque shade, so a locked plate reads unmistakably "off" rather than merely faded.
	for (int i = 0; i < 256; i++)
		SplTransTbl[i] = static_cast<uint8_t>(i);
	SplTransTbl[255] = 0;

	constexpr int Darken = 4;
	SplTransTbl[PAL8_YELLOW] = PAL16_GRAY + 5;
	SplTransTbl[PAL8_YELLOW + 1] = PAL16_GRAY + 7;
	SplTransTbl[PAL8_YELLOW + 2] = PAL16_GRAY + 9;
	for (int within = 0; within < 15; within++) {
		const auto dark = static_cast<uint8_t>(PAL16_GRAY + std::min(within + Darken, 14));
		SplTransTbl[PAL16_BEIGE + within] = dark;
		SplTransTbl[PAL16_YELLOW + within] = dark;
		SplTransTbl[PAL16_ORANGE + within] = dark;
		SplTransTbl[PAL16_GRAY + within] = dark;
	}
	// The ramp-end entries stay the Invalid table's transparent 0, or the plate gains a solid corner.
	SplTransTbl[PAL16_BEIGE + 15] = 0;
	SplTransTbl[PAL16_YELLOW + 15] = 0;
	SplTransTbl[PAL16_ORANGE + 15] = 0;
}

void DrawSmallSpellIconScaledTo(const Surface &out, Rectangle cell)
{
	if (!SmallSpellIcons) {
		return;
	}
	const Size natural = GetSmallSpellIconSize();
	if (natural.width <= 0 || natural.height <= 0)
		return;

	// ONE percent for both axes - that is all ScaleClxList takes - so a 37x38 plate cannot land
	// exactly on a 56x56 cell. Rounded UP so the plate always covers the cell rather than falling
	// short: a pixel of overhang reads as a border, a pixel of shortfall reads as the bug being
	// fixed here.
	const int percent = std::max((cell.size.width * 100 + natural.width - 1) / natural.width,
	    (cell.size.height * 100 + natural.height - 1) / natural.height);
	if (percent <= 100) {
		// Already at least as big as the cell; the unscaled draw is exact and costs nothing.
		DrawSmallSpellIcon(out, { cell.position.x, cell.position.y + natural.height - 1 }, SpellID::Null);
		return;
	}

	// Cached on the percentage, not rebuilt per draw - a tree page is up to eighteen cells and they
	// all want the same size. ScaleClxList caps at 400%, which no sane cell reaches.
	static OptionalOwnedClxSpriteList scaled;
	static int scaledPercent = 0;
	const int clamped = std::min(percent, 400);
	if (!scaled || scaledPercent != clamped) {
		scaled = oracool::ScaleClxList(ClxSpriteList { *SmallSpellIcons }, static_cast<unsigned>(clamped));
		scaledPercent = clamped;
	}

	// Through SpellITbl, NOT by casting the SpellID. A SpellID is not a frame number - the table is
	// the mapping, and SpellID::Null lands on frame 26, the empty plate. Indexing the list with the
	// raw enum value instead (as this did when first written, 2026-08-17) picks whatever real spell
	// icon happens to sit at that index, which is exactly what the user saw: "i see other icons on
	// top".
	const ClxSprite plate = (*scaled)[SpellITbl[static_cast<int8_t>(SpellID::Null)]];
	const Point centred { cell.position.x + (cell.size.width - static_cast<int>(plate.width())) / 2,
		cell.position.y + (cell.size.height - static_cast<int>(plate.height())) / 2 };
	// CLX is drawn from the sprite's BOTTOM-left, the convention every other call here follows.
	ClxDrawTRN(out, { centred.x, centred.y + static_cast<int>(plate.height()) - 1 }, plate, SplTransTbl);
}

void DrawSmallSpellIconFittedTo(const Surface &out, Rectangle cell, SpellID spell)
{
	if (!SmallSpellIcons)
		return;
	const Size natural = GetSmallSpellIconSize();
	if (natural.width <= 0 || natural.height <= 0)
		return;

	// The containment twin of DrawSmallSpellIconScaledTo. That one rounds UP so the plate always
	// covers its cell, which is right for a tree page, where a pixel of overhang lands on more page.
	// It is wrong for the LMB/RMB wells, where the pixel outside the 46x46 net opening lands on the
	// bezel (user, 2026-08-18: "stay away from the bezels [...] don't ever spill out of it"). So this
	// rounds DOWN and centres the remainder: at most one pixel of plate short on an axis, none over.
	const int percent = std::min(cell.size.width * 100 / natural.width,
	    cell.size.height * 100 / natural.height);
	if (percent <= 100) {
		DrawSmallSpellIcon(out, { cell.position.x, cell.position.y + natural.height - 1 }, spell);
		return;
	}

	static OptionalOwnedClxSpriteList fitted;
	static int fittedPercent = 0;
	const int clamped = std::min(percent, 400);
	if (!fitted || fittedPercent != clamped) {
		fitted = oracool::ScaleClxList(ClxSpriteList { *SmallSpellIcons }, static_cast<unsigned>(clamped));
		fittedPercent = clamped;
	}

	const ClxSprite plate = (*fitted)[SpellITbl[static_cast<int8_t>(spell)]];
	const Point centred { cell.position.x + (cell.size.width - static_cast<int>(plate.width())) / 2,
		cell.position.y + (cell.size.height - static_cast<int>(plate.height())) / 2 };
	ClxDrawTRN(out, { centred.x, centred.y + static_cast<int>(plate.height()) - 1 }, plate, SplTransTbl);
}

void SetSpellTransRed()
{
	// Oracool: user request (2026-08-17) - "Unlocked skills with 0 points in them are unavailable
	// and inactive, ergo need to have red background, not green."
	//
	// This is a THIRD state, distinct from the two that already existed and easily confused with
	// both: Grey means "not earned yet" and Pink means "earned but cannot be performed right now"
	// (no mana, no shield). Red means "earned and spendable, but you have put nothing into it" -
	// the only one of the three the player can fix by spending a point.
	//
	// No palette injection needed, unlike the green: PAL16_RED is one of the game's own ramps, so
	// this is the same shape as the Pink case - map the plate's ramps onto it 1:1 and keep the
	// ramp-end entries transparent, or the plate gains a solid corner.
	for (int i = 0; i < 256; i++)
		SplTransTbl[i] = static_cast<uint8_t>(i);
	SplTransTbl[255] = 0;

	SplTransTbl[PAL8_YELLOW] = PAL16_RED + 2;
	SplTransTbl[PAL8_YELLOW + 1] = PAL16_RED + 4;
	SplTransTbl[PAL8_YELLOW + 2] = PAL16_RED + 6;
	for (int within = 0; within < 15; within++) {
		const auto red = static_cast<uint8_t>(PAL16_RED + within);
		SplTransTbl[PAL16_BEIGE + within] = red;
		SplTransTbl[PAL16_YELLOW + within] = red;
		SplTransTbl[PAL16_ORANGE + within] = red;
		SplTransTbl[PAL16_GRAY + within] = red;
	}
	SplTransTbl[PAL16_BEIGE + 15] = 0;
	SplTransTbl[PAL16_YELLOW + 15] = 0;
	SplTransTbl[PAL16_ORANGE + 15] = 0;
}

} // namespace devilution
