#include "panels/charpanel.hpp"

#include <cstdint>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include <fmt/format.h>

#include "control.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "panels/ui_panels.hpp"
#include "player.h"
#include "missiles.h" // GetDamageAmtAtLevel - the readied spell's own damage formula
#include "oracool/paladin_melee.h" // ZealToHitBonus - the sheet must quote what PlayerCanHitMonster uses
#include "oracool/paladin_ranged.h" // the three cast skills' damage and type - their spell rows carry no missile
#include "oracool/paladin_skills.h" // a melee class skill swings the weapon, so it reads as weapon damage
#include "oracool/player_resistance.h"
#include "oracool/rage.h"
#include "oracool/essence.h" // the Necromancer's second pool, a row of its own
#include "oracool/class_tree.h"
#include "oracool/aura_field.h" // HolyPulseDamage / SanctuaryDamage - a damaging aura's number on the sheet
#include "oracool/hero_title.h"
#include "oracool/signets.h"
#include "oracool/advanced_stats.h" // the grouped sheet's boxes, and the window its ADVANCED STATS button opens
#include "oracool/shop_grid.h"      // DrawVendorButtonBacking - the grouped sheet's two buttons wear the vendors' face
#include "oracool/ui_sound.h"       // the hover-entry and press clicks on those two buttons
#include "oracool/xp_counter.h"     // GetLevelExperienceSpan - the header's XP bar, the HUD bar's own maths
#include "diablo.h"                 // MousePosition - hover on the grouped sheet's buttons
#include "spells.h" // IsValidSpell
#include "playerdat.hpp"
#include "options.h"
#include "oracool/oracool.h"
#include "oracool/hud_art.h"
#include "oracool/ornate_border.h"
#include "stores.h" // TotalPlayerGold
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution {

OptionalOwnedClxSpriteList pChrButtons;

namespace {

struct StyledText {
	UiFlags style;
	std::string text;
	int spacing = 1;
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

/**
 * @brief What kind of damage a readied spell deals, taken from the missile it actually throws.
 *
 * Derived, never tabulated. The engine already answers this - every missile carries its damage type
 * in its own flags - and a second table here would be one more thing to disagree with the code the
 * first time a spell's missile changed. A spell with no missile (Zeal, Charge and the other melee
 * skills carry MissileID::Null in both slots) swings the weapon, which is physical.
 */
DamageType ReadiedSpellDamageType(SpellID spell)
{
	// The Paladin's three cast skills carry no missile on their spell row - they are cast from their own
	// module - so theirs is asked of that module, which answers from the missile each one really throws
	// (2026-09-11: Blessed Hammer's blue appeared as white until this).
	if (const std::optional<oracool::PaladinSkill> skill = oracool::PaladinSkillForSpell(spell); skill.has_value()) {
		if (const std::optional<DamageType> type = oracool::PaladinCastDamageType(*skill); type.has_value())
			return *type;
	}
	for (const MissileID missile : GetSpellData(spell).sMissiles) {
		if (missile != MissileID::Null)
			return GetMissileData(missile).damageType();
	}
	return DamageType::Physical;
}

/**
 * @brief The weapon-damage reading. Physical, so white.
 *
 * It used to be coloured by GetValueColor(_pIBonusDam) - blue when an item added damage. That now
 * has to go: with the two rows colour-coded BY DAMAGE TYPE, a blue weapon reading would say "cold"
 * to anyone reading the other rows, and the type is the thing these rows exist to communicate. The
 * item bonus is still legible on the item itself and in the "To hit"/AC rows beside this one.
 */
StyledText WeaponDamageText()
{
	const std::pair<int, int> dmg = GetDamage();
	// Tighter letter spacing once the numbers reach three digits, or the pair outgrows its column.
	const int spacing = (dmg.first >= 100) ? -1 : 1;
	return StyledText { UiFlags::ColorWhite, StrCat(dmg.first, "-", dmg.second), spacing };
}

/**
 * @brief What the given mouse button actually does for damage, whatever is sitting on it.
 *
 * Oracool: user request (2026-08-31) - "we need to introduce in the hero stats window a second dmg
 * stat... that is the way D2 does it. it has two DMG text fields and they display the dmg lmb/rmb
 * slots do regardles of it is spell, attack skill or something else assigned to these slotts."
 *
 * Three answers, in the order they are asked:
 *
 *   1. Nothing readied - the basic attack - is the weapon. So is a melee class skill: Zeal and
 *      Hammer of Faith swing what you are holding, so quoting a formula for them would be quoting
 *      the wrong number. Melee is read from the skill's own rangeTiles rather than a list kept
 *      here, so a skill that changes reach changes this with it.
 *   2. A spell with a damage formula answers with that formula, at the level this character has it.
 *   3. Everything else - Town Portal, Identify, an aura, a heal - has no damage to report and says
 *      so with a dash, the same mark DamageRange uses for a damage type the player does not have.
 *
 * A HEAL is deliberately a dash rather than its heal range: this row is labelled damage, and a
 * number under the wrong label is worse than no number. The Abilities window quotes heals in full.
 */
/**
 * @brief The aura burning on the RIGHT button, or None.
 *
 * An aura and the readied right-button skill are one slot (class_tree.h, ClearClassAuraForRightButton):
 * lighting Vigor empties _pRSpell, so every reader of that field alone saw the basic attack - the sheet
 * said "Right button: Attack" over the weapon's damage while the well showed Vigor (user, 2026-09-07:
 * "when assigning vigor to rmb, hero stats right button says ATTACK and shows DMG. this is incorrect").
 * The HUD's hover already checked the aura first (control.cpp); the four sheet readers below now ask
 * this before anything else. The audit found no other state that empties a button: the aura is the
 * only toggle in the game that lives on a mouse button without a SpellID.
 */
oracool::ClassTreeSkill AuraOnButton(bool leftButton)
{
	if (leftButton)
		return oracool::ClassTreeSkill::None;
	return oracool::GetActiveClassAura(*InspectPlayer);
}

/**
 * @brief A DAMAGING aura on the button: its damage type and the range one pulse deals at the points in it.
 *
 * User, 2026-09-25 dev note: "holy fire aura does fire dmg - have its hero stats title written in red text in
 * right button:XXXXXXXXXX. also on AURA ON row type the current dmg it is doing. also in red." The same holds
 * for every aura that strikes: Holy Freeze (cold), Holy Shock (lightning) and Sanctuary (magic, undead only)
 * read through here too - the numbers are the ones AuraFieldFactsAt quotes in the skill's own tooltip.
 */
std::optional<std::pair<DamageType, oracool::AuraDamage>> DamagingAuraOnButton(bool leftButton)
{
	using Skill = oracool::ClassTreeSkill;
	const Skill aura = AuraOnButton(leftButton);
	if (aura != Skill::HolyFire && aura != Skill::HolyFreeze && aura != Skill::HolyShock && aura != Skill::Sanctuary)
		return std::nullopt;
	const int points = std::max(oracool::ClassTreeInvestment(*InspectPlayer, aura), 1);
	if (aura == Skill::Sanctuary)
		return std::make_pair(DamageType::Magic, oracool::SanctuaryDamage(points));
	const DamageType type = aura == Skill::HolyFire ? DamageType::Fire
	    : aura == Skill::HolyFreeze                 ? DamageType::Cold
	                                                : DamageType::Lightning;
	return std::make_pair(type, oracool::HolyPulseDamage(aura, points));
}

/** @brief Whether the button's swing is the weapon's - a basic attack, or a melee class skill. */
bool ReadiedSlotSwingsTheWeapon(SpellID spell)
{
	if (!IsValidSpell(spell))
		return true;
	const std::optional<oracool::PaladinSkill> skill = oracool::PaladinSkillForSpell(spell);
	return skill.has_value()
	    && oracool::GetPaladinSkillData(*skill).rangeTiles == oracool::MeleeSkillRangeTiles;
}

/**
 * @brief The colour BOTH of a button's rows are written in - the name and the number together.
 *
 * One decision, asked once, so the pair can never disagree about what kind of damage it is. That
 * pairing is the point of the colour: the name row is what tells you a red number is Firebolt's.
 */
UiFlags ReadiedSlotColor(bool leftButton)
{
	const Player &player = *InspectPlayer;
	const SpellID spell = leftButton ? player._pLRSpell : player._pRSpell;
	if (const auto damaging = DamagingAuraOnButton(leftButton); damaging.has_value())
		return DamageTypeColor(damaging->first); // a burning aura reads as its element, both rows (2026-09-25)
	if (AuraOnButton(leftButton) != oracool::ClassTreeSkill::None)
		return UiFlags::ColorBlue; // the "Aura:" row's colour, so the two rows agree
	if (ReadiedSlotSwingsTheWeapon(spell))
		return UiFlags::ColorWhite; // the weapon is physical
	if (spell == SpellID::Healing || spell == SpellID::HealOther)
		return UiFlags::ColorOracoolGreen;
	return DamageTypeColor(ReadiedSpellDamageType(spell));
}

StyledText GetReadiedSlotDamage(bool leftButton)
{
	const Player &player = *InspectPlayer;
	const SpellID spell = leftButton ? player._pLRSpell : player._pRSpell;

	// An aura has no number to report; the row says it is burning, under an "Aura" label.
	if (const auto damaging = DamagingAuraOnButton(leftButton); damaging.has_value()) {
		// "On" and what it is doing, in the element's colour: one pulse's range, every three seconds.
		const oracool::AuraDamage &d = damaging->second;
		return StyledText { DamageTypeColor(damaging->first), StrCat(_("On"), ", ", d.min, "-", d.max), (d.min >= 100) ? -1 : 1 };
	}
	if (AuraOnButton(leftButton) != oracool::ClassTreeSkill::None)
		return StyledText { UiFlags::ColorBlue, std::string(_("On")) };
	if (ReadiedSlotSwingsTheWeapon(spell))
		return WeaponDamageText();

	// The Paladin's three cast skills: the per-hit range their own cast rolls. They have no missile on their
	// spell row, so the formula below has nothing to say about them and the row read a dash (user,
	// 2026-09-11: "make sure hero stats screen shows the DMG amount it does, because right now it shows a
	// simple - (dash)").
	if (const std::optional<oracool::PaladinSkill> skill = oracool::PaladinSkillForSpell(spell); skill.has_value()) {
		if (const std::optional<std::pair<int, int>> range = oracool::PaladinCastDamageRange(player, *skill); range.has_value()) {
			return StyledText { ReadiedSlotColor(leftButton), StrCat(range->first, "-", range->second),
				(range->first >= 100) ? -1 : 1 };
		}
	}

	int minDam = -1;
	int maxDam = -1;
	// At least 1: several formulas take the level as a real term, and a spell readied from a staff
	// the character has no book for reads back as level 0.
	GetDamageAmtAtLevel(spell, std::max(player.GetSpellLevel(spell), 1), &minDam, &maxDam);
	if (minDam == -1)
		return StyledText { UiFlags::ColorWhite, "-" };

	// A heal now reports its NUMBERS, in green. Yesterday this row could only be labelled "damage",
	// so a heal had to answer with a dash; the colour is what makes the number legible as something
	// other than damage, which is the whole reason the user asked for the palette.
	return StyledText { ReadiedSlotColor(leftButton), StrCat(minDam, "-", maxDam),
		(minDam >= 100) ? -1 : 1 };
}

/**
 * @brief What the number under a button's name IS - damage, or healing.
 *
 * A heal read "Damage 29-139" until 2026-08-31, which the green it is written in flatly
 * contradicted. The colour was carrying the correction on its own; now the word does.
 */
std::string ReadiedSlotAmountLabel(bool leftButton)
{
	const Player &player = *InspectPlayer;
	const SpellID spell = leftButton ? player._pLRSpell : player._pRSpell;
	if (AuraOnButton(leftButton) != oracool::ClassTreeSkill::None)
		return std::string(_("Aura"));
	if (spell == SpellID::Healing || spell == SpellID::HealOther)
		return std::string(_("Healing"));
	return std::string(_("Damage"));
}

/**
 * @brief The bare name of whatever is on a button - "Zeal", "Holy Freeze", "Attack" - with no
 * "Left button:" in front. The grouped sheet (2026-09-26) writes the button underneath in grey instead,
 * so it needs the name alone; the list sheet's GetReadiedSlotName below wraps this same answer.
 */
string_view ReadiedSlotSkillName(bool leftButton)
{
	const Player &player = *InspectPlayer;
	const SpellID spell = leftButton ? player._pLRSpell : player._pRSpell;
	// SpellID::Invalid IS the basic attack - see attack_skills.h - so it is named rather than left
	// blank, or the row would read as an empty slot the player forgot to fill.
	//
	// pgettext("spell", ...), NOT _(): spelldat marks every name P_("spell", "Zeal"), so the
	// catalogue key carries that context and a plain _() looks up a key that is not there. The
	// failure is silent - every name falls back to English - which is why every other display site
	// in the game (items.cpp, objects.cpp) spells out the same pgettext.
	const oracool::ClassTreeSkill aura = AuraOnButton(leftButton);
	return aura != oracool::ClassTreeSkill::None
	    ? _(oracool::GetClassTreeSkillData(aura).name)
	    : IsValidSpell(spell)
	    ? pgettext("spell", GetSpellData(spell).sNameText)
	    : _(/* TRANSLATORS: the plain weapon swing, when no skill is readied */ "Attack");
}

/** @brief The name of whatever is on a button, for the row above its damage. */
std::string GetReadiedSlotName(bool leftButton)
{
	const string_view name = ReadiedSlotSkillName(leftButton);
	// One format string per button rather than a translated word plus a colon: "Left" alone is
	// ambiguous to translate and several languages need the parts in the other order.
	return leftButton
	    ? fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} is a skill or spell name */ "Left button: {:s}")), name)
	    : fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} is a skill or spell name */ "Right button: {:s}")), name);
}

// Oracool V1: derived readings for stats the engine tracks but the sheet never showed. Each one
// mirrors the arithmetic at the site that actually consumes it - the reference is in the comment,
// because a sheet that quietly disagrees with the combat code is worse than no sheet.

/**
 * @brief Attack animation frames skipped by the speed modifiers on the player's items.
 *
 * Mirrors StartAttack's includesFirstFrame branch (player.cpp:222-241), which is the fresh-swing
 * case. The repeated-swing branch below it skips fewer frames - Fastest is even suppressed
 * entirely when Fast or Faster is also present - so quoting that one would understate the items.
 */
int AttackFramesSkipped()
{
	const ItemSpecialEffect flags = InspectPlayer->_pIFlags;
	if (HasAnyOf(flags, ItemSpecialEffect::FastestAttack) && HasAnyOf(flags, ItemSpecialEffect::QuickAttack | ItemSpecialEffect::FastAttack))
		return 3; // Fastest plus Quick/Fast skips the fourth frame, costing one - see StartAttack
	if (HasAnyOf(flags, ItemSpecialEffect::FastestAttack))
		return 4;
	if (HasAnyOf(flags, ItemSpecialEffect::FasterAttack))
		return 3;
	if (HasAnyOf(flags, ItemSpecialEffect::FastAttack))
		return 2;
	if (HasAnyOf(flags, ItemSpecialEffect::QuickAttack))
		return 1;
	return 0;
}

/** @brief Hit-recovery frames skipped, mirroring StartPlrHit (player.cpp:2737-2745). */
int HitRecoveryFramesSkipped()
{
	const ItemSpecialEffect flags = InspectPlayer->_pIFlags;
	if (HasAnyOf(flags, ItemSpecialEffect::FastestHitRecovery))
		return 3;
	if (HasAnyOf(flags, ItemSpecialEffect::FasterHitRecovery))
		return 2;
	if (HasAnyOf(flags, ItemSpecialEffect::FastHitRecovery))
		return 1;
	return 0;
}

/**
 * @brief Chance to block an attack from an equal-level enemy, as a percentage.
 *
 * PlrHitPlr (player.cpp:784) rolls against `target.GetBlockChance() - attacker._pLevel * 2`,
 * clamped 0..100. GetBlockChance() adds the target's OWN level * 2, so against an equal-level
 * attacker the two level terms cancel exactly and what remains is dexterity plus the class block
 * bonus - which is GetBlockChance(false). A sheet has no attacker to name, so the equal-level
 * reading is the honest one to quote.
 *
 * Zero without a shield: _pBlockFlag gates the roll entirely, so the potential is unreachable.
 */
int BlockChancePercent()
{
	if (!InspectPlayer->_pBlockFlag)
		return 0;
	return std::clamp(InspectPlayer->GetBlockChance(false), 0, 100);
}

/** @brief Fixed life-steal percentage. Both flags present means 5 - the later test overwrites. */
int LifeStealPercent()
{
	if (HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::StealLife5))
		return 5;
	if (HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::StealLife3))
		return 3;
	return 0;
}

/** @brief Fixed mana-steal percentage. NoMana disables the whole branch (player.cpp:720). */
int ManaStealPercent()
{
	if (HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::NoMana))
		return 0;
	if (HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::StealMana5))
		return 5;
	if (HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::StealMana3))
		return 3;
	return 0;
}

/** @brief "0" when a bonus is absent, so a row of zeroes reads as "nothing here" at a glance. */
StyledText PlainValue(int value, string_view suffix = "")
{
	return { value != 0 ? UiFlags::ColorBlue : UiFlags::ColorWhite, StrCat(value, suffix) };
}

/** @brief A min-max damage range, or a dash when the player has none of that damage type. */
StyledText DamageRange(int minDam, int maxDam)
{
	if (maxDam <= 0)
		return { UiFlags::ColorWhite, "-" };
	return { UiFlags::ColorBlue, StrCat(minDam, "-", maxDam) };
}

StyledText GetResistInfo(int8_t resist)
{
	UiFlags style = UiFlags::ColorBlue;
	if (resist == 0)
		style = UiFlags::ColorWhite;
	// Live since v1.11.127: resistance goes negative, D2-style, when the difficulty's penalty exceeds
	// the hero's gear - red, because that school now takes MORE than full damage.
	else if (resist < 0)
		style = UiFlags::ColorRed;
	// Oracool: gold means "there is nothing left to buy", so it marks the HARD cap, not the soft
	// one. Reaching 75 used to be the end of the road and is now the point where each further point
	// costs three - worth showing as ordinary progress rather than as an achievement, or the sheet
	// would tell a player at 75 to stop shopping when 15 points are still on the table.
	else if (resist >= oracool::ResistanceHardCap)
		style = UiFlags::ColorWhitegold;

	return { style, StrCat(resist, "%") };
}

/** @brief The "To hit" reading, for both sheets - the list's row and the grouped sheet's box. */
StyledText ToHitReading()
{
	const bool bow = InspectPlayer->InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow;
	// Zeal's accuracy is added in PlayerCanHitMonster, not folded into GetMeleeToHit, so this
	// row was reporting a number the game does not use (user, 2026-09-02: "zeal doesnt seem
	// to increased my cth according to the hero stats screen"). It was being applied - just
	// never shown, which is indistinguishable from not working when the sheet is how you
	// check.
	//
	// IsZealReadied, not ZealToHitBonus. The latter asks ArmedMeleeSkill(), a latch that
	// describes the swing currently being resolved - and standing in the character sheet
	// there is no swing, so it always answered zero and this row still showed nothing (user,
	// 2026-09-02, after the first attempt). What a sheet can honestly report is whether Zeal
	// is on a button; the magnitude then comes from ZealToHitBonusAtRank, which is the same
	// number the hit roll uses.
	const int zeal = (!bow && oracool::IsZealReadied(*InspectPlayer))
	    ? oracool::ZealToHitBonusAtRank(*InspectPlayer)
	    : 0;
	const int toHit = (bow ? InspectPlayer->GetRangedToHit() : InspectPlayer->GetMeleeToHit()) + zeal;
	return StyledText { zeal > 0 ? UiFlags::ColorBlue : GetValueColor(InspectPlayer->_pIBonusToHit),
		StrCat(toHit, "%") };
}

// Oracool V1: the sheet is a plain two-column list - every label in one right-aligned column, every
// value in a left-aligned column beside it. The vanilla scatter of hand-placed x/y literals (two
// label columns, a separate top block, field boxes) is gone; a row's only positional input is its
// index in CharRows below, and the columns are measured from the font at load time so a longer
// translation widens the label column instead of colliding with the values.

/** @brief One row's value producer. Captureless lambdas convert to this. */
using ValueFunc = StyledText (*)();
/** @brief A per-frame label. Returns an already-translated string - see CharRow::dynamicLabel. */
using LabelFunc = std::string (*)();
/** @brief Whether a row is on the sheet at all for the hero being read (CharRow::visible). */
using VisibleFunc = bool (*)();

/** @brief Rows that own a widget in the right-hand button column. `None` must stay the zero value
 * so rows can omit the field. */
enum class CharRowExtra : uint8_t {
	None,
	StatStrength,
	StatMagic,
	StatDexterity,
	StatVitality,
	Points,
	/**
	 * @brief One line of text spanning the whole content width, with no label and no value column.
	 *
	 * For the readied-skill names above the two damage rows (user, 2026-08-31). They cannot use the
	 * label column: it is a fixed 161px and the longest skill name in the game, "Master of the Long
	 * Staff", measures wider than that - so a name sharing a line with its number would clip exactly
	 * on the skills a player is most likely to be looking up.
	 */
	FullWidthText,
};

// EnsureLayout turns a Stat* value into a ChrBtnsRect index by subtracting StatStrength, so the
// four must stay in CharacterAttribute's order. Getting this wrong would put every +stat button on
// the wrong row - and it would still compile and still be clickable, just wired to the wrong stat.
static_assert(static_cast<int>(CharRowExtra::StatMagic) - static_cast<int>(CharRowExtra::StatStrength) == static_cast<int>(CharacterAttribute::Magic), "CharRowExtra stat order must match CharacterAttribute");
static_assert(static_cast<int>(CharRowExtra::StatDexterity) - static_cast<int>(CharRowExtra::StatStrength) == static_cast<int>(CharacterAttribute::Dexterity), "CharRowExtra stat order must match CharacterAttribute");
static_assert(static_cast<int>(CharRowExtra::StatVitality) - static_cast<int>(CharRowExtra::StatStrength) == static_cast<int>(CharacterAttribute::Vitality), "CharRowExtra stat order must match CharacterAttribute");

struct CharRow {
	/** N_()-marked label, or "" for the Base/Now header row, which has values but no label. */
	const char *label;
	ValueFunc value;
	/** Second value column - the "Now" reading beside "Base", and current beside maximum for
	 * life/mana. nullptr for rows carrying a single number. */
	ValueFunc secondValue = nullptr;
	/** Extra pixels above this row, used to separate groups. */
	int gapAbove = 0;
	CharRowExtra extra = CharRowExtra::None;
	/**
	 * @brief A label computed per frame, overriding `label` when set.
	 *
	 * LAST in the struct on purpose: every other row is built with positional initialisers that end
	 * at `extra`, and a field inserted before that point silently shifts all of them.
	 *
	 * One row needs it - the readied-slot amount row, which has to say "Healing" rather than
	 * "Damage" when a heal is on that button (user screenshot, 2026-08-31: a heal read "Damage
	 * 29-139", which the green the number is written in flatly contradicted). Every other label is
	 * a property of the row rather than of the character, and keeps its const char *.
	 */
	LabelFunc dynamicLabel = nullptr;
	/**
	 * @brief Whether the row is on the sheet at all for the hero being read; nullptr means always.
	 *
	 * After dynamicLabel, for the same positional reason. One row needs it: Essence (2026-09-18), the
	 * Necromancer's second pool, which no other hero has - on their sheet it would read as a stuck zero.
	 * A hidden row takes no height (EnsureLayout) and draws nothing (DrawRow); the layout is redone
	 * when the set of visible rows changes, so a new hero of another class gets a fresh sheet.
	 */
	VisibleFunc visible = nullptr;
};

/**
 * @brief Vertical space one row occupies, before its gapAbove.
 *
 * 24, from 28 (user, 2026-09-03: "reduce the gaps between rows in hero stats window by half"). The
 * air is what shrinks: a row's text is 13px tall, so the blank around it goes from 15 to 11.
 *
 * NOT the 20 that would have halved it exactly, and the floor is not a matter of taste. Four of
 * these rows carry a + button (IncrementAttributeButtonSize, 22 tall) and one carries RESET
 * (ResetStatsButtonSize, 24), each centred in its row by PlaceWidgets. At a 20px pitch the four
 * stat buttons - which sit on four CONSECUTIVE rows - would overlap each other by two pixels, and a
 * click near a boundary would land on the wrong attribute. 24 is the largest widget on the sheet, so
 * it is the smallest pitch that keeps every hit target its own. Halving properly would mean shrinking
 * the buttons, which is a different change to a different asset.
 */
constexpr int CharRowHeight = 24;
static_assert(CharRowHeight >= ResetStatsButtonSize.height,
    "a row is now shorter than the widget centred in it - consecutive rows' buttons would overlap");
/** @brief The blank band inserted between groups of related rows. Halved with the rows, 7 -> 4. */
constexpr int CharRowGroupGap = 4;

const CharRow CharRows[] = {
	{ N_("Name"),
	    []() { return StyledText { UiFlags::ColorWhite, InspectPlayer->_pName }; } },
	// The Sanctified Order (user, 2026-09-13: "put the title in the hero stats screen between name and
	// class row"). Earned by the hardest difficulty Diablo has fallen on - see oracool/hero_title.h.
	{ N_("Title"),
	    []() { return StyledText { oracool::HeroTitleColorFor(InspectPlayer->pDiabloKillLevel), std::string(_(oracool::HeroTitleFor(InspectPlayer->pDiabloKillLevel))) }; } },
	{ N_("Class"),
	    []() { return StyledText { UiFlags::ColorWhite, std::string(_(PlayersData[static_cast<std::size_t>(InspectPlayer->_pClass)].className)) }; } },

	{ N_("Level"),
	    []() { return StyledText { UiFlags::ColorWhite, StrCat(InspectPlayer->_pLevel) }; },
	    nullptr, CharRowGroupGap },
	{ N_("Experience"),
	    []() {
	        int spacing = ((InspectPlayer->_pExperience >= 10000000000ULL) ? -1 : (InspectPlayer->_pExperience >= 1000000000) ? 0
	                                                                                                                          : 1);
	        return StyledText { UiFlags::ColorWhite, FormatInteger(InspectPlayer->_pExperience), spacing };
	    } },
	{ N_("Next level"),
	    []() {
	        if (InspectPlayer->_pLevel == MaxCharacterLevel) {
		        return StyledText { UiFlags::ColorWhitegold, std::string(_("None")) };
	        }
	        int spacing = ((InspectPlayer->_pNextExper >= 10000000000ULL) ? -1 : (InspectPlayer->_pNextExper >= 1000000000) ? 0
	                                                                                                                        : 1);
	        return StyledText { UiFlags::ColorWhite, FormatInteger(InspectPlayer->_pNextExper), spacing };
	    } },
	{ N_("Gold"),
	    []() {
	        // Oracool: picked-up and sold gold goes to the shared Stash pool, so the total has to
	        // include it. This used to re-derive that sum inline; it now calls the one helper the
	        // store screen and the inventory readout also use, so the three cannot drift apart.
	        //
	        // TotalPlayerGold reads MyPlayer, so it is only right when inspecting yourself - which
	        // is also the only case the Stash belongs to. Another player's sheet still shows just
	        // their carried gold.
	        const uint32_t gold = (InspectPlayer == MyPlayer && oracool::IsSinglePlayer())
	            ? TotalPlayerGold()
	            : static_cast<uint32_t>(InspectPlayer->_pGold);
	        return StyledText { UiFlags::ColorWhite, FormatInteger(gold) };
	    } },

	// Header for the two value columns. It has no label of its own, so it renders as two gold
	// words sitting exactly over the numbers they describe. Now comes first: the left column is
	// what the character actually has, the right is the base it was built from - which is also the
	// number the + buttons beside it increase.
	{ "",
	    []() { return StyledText { UiFlags::ColorWhitegold, std::string(_("Now")) }; },
	    []() { return StyledText { UiFlags::ColorWhitegold, std::string(_("Base")) }; },
	    CharRowGroupGap },
	{ N_("Strength"),
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Strength), StrCat(InspectPlayer->_pStrength) }; },
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Strength), StrCat(InspectPlayer->_pBaseStr) }; },
	    0, CharRowExtra::StatStrength },
	{ N_("Magic"),
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Magic), StrCat(InspectPlayer->_pMagic) }; },
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Magic), StrCat(InspectPlayer->_pBaseMag) }; },
	    0, CharRowExtra::StatMagic },
	{ N_("Dexterity"),
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Dexterity), StrCat(InspectPlayer->_pDexterity) }; },
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Dexterity), StrCat(InspectPlayer->_pBaseDex) }; },
	    0, CharRowExtra::StatDexterity },
	{ N_("Vitality"),
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Vitality), StrCat(InspectPlayer->_pVitality) }; },
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Vitality), StrCat(InspectPlayer->_pBaseVit) }; },
	    0, CharRowExtra::StatVitality },
	// Directly under Vitality, with no gap: the points belong to the attribute block they are
	// spent on, and RESET lines up under the Base column the + buttons feed.
	{ N_("Level-up Points"),
	    []() {
	        InspectPlayer->_pStatPts = std::min(CalcStatDiff(*InspectPlayer), InspectPlayer->_pStatPts);
	        return StyledText { UiFlags::ColorRed, (InspectPlayer->_pStatPts > 0 ? StrCat(InspectPlayer->_pStatPts) : "") };
	    },
	    nullptr, 0, CharRowExtra::Points },
	// Skill points, directly under the stat points, because a player looking for "what do I have
	// left to spend" is looking for both and the sheet only ever answered half the question. The
	// number is drawn on the HUD's points frame (control.cpp) but appeared nowhere on the sheet, so
	// "Level-up Points: 0" read as nothing left to spend while five skill points sat unspent
	// (audit, 2026-08-31).
	{ N_("Skill Points"),
	    []() {
	        return StyledText { UiFlags::ColorRed,
	            (InspectPlayer->_pUnspentSkillPoints > 0 ? StrCat(InspectPlayer->_pUnspentSkillPoints) : "") };
	    } },

	{ N_("Armor class"),
	    []() { return StyledText { GetValueColor(InspectPlayer->_pIBonusAC), StrCat(InspectPlayer->GetArmor() + InspectPlayer->_pLevel * 2) }; },
	    nullptr, CharRowGroupGap },
	// The reading lives in ToHitReading (above) since the grouped sheet (2026-09-26) shows the same
	// number in its own box - one function, so the two sheets cannot quote two accuracies.
	{ N_("To hit"),
	    []() { return ToHitReading(); } },
	// TWO damage fields, one per mouse button, each TWO ROWS - the name of what is readied, then
	// what it does (user, 2026-08-31, after Diablo II). Either button can hold a weapon swing, an
	// attack skill or a spell, so the old single row could only ever describe the weapon; and the
	// name needs its own full-width row because the label column cannot hold the longest skill
	// names. Both rows of a pair are coloured by damage type, which is what lets the name explain
	// the number.
	{ "", []() { return StyledText { ReadiedSlotColor(true), GetReadiedSlotName(true) }; },
	    nullptr, CharRowGroupGap, CharRowExtra::FullWidthText },
	{ "", []() { return GetReadiedSlotDamage(/*leftButton=*/true); }, nullptr, 0,
	    CharRowExtra::None, []() { return ReadiedSlotAmountLabel(/*leftButton=*/true); } },
	{ "", []() { return StyledText { ReadiedSlotColor(false), GetReadiedSlotName(false) }; },
	    nullptr, CharRowGroupGap, CharRowExtra::FullWidthText },
	{ "", []() { return GetReadiedSlotDamage(/*leftButton=*/false); }, nullptr, 0,
	    CharRowExtra::None, []() { return ReadiedSlotAmountLabel(/*leftButton=*/false); } },

	{ N_("Resist magic"),
	    []() { return GetResistInfo(InspectPlayer->_pMagResist); },
	    nullptr, CharRowGroupGap },
	{ N_("Resist fire"),
	    []() { return GetResistInfo(InspectPlayer->_pFireResist); } },
	{ N_("Resist lightning"),
	    []() { return GetResistInfo(InspectPlayer->_pLghtResist); } },
	// The two rows that make the three above readable (audit, 2026-08-31). Until now the sheet
	// printed the FINAL resistance and nothing else, so a player on Torment saw "Resist fire: 45"
	// with no way to learn that the difficulty had already taken 60 points off the raw total, that
	// returns diminish above 75, or that the ceiling is 90 rather than vanilla's 75. Three rules
	// this fork invented, all invisible on the one screen that exists to explain the character.
	//
	// The penalty is shown as the negative it is, and only when there is one - a "-0" on Normal
	// would be a row that teaches nothing four times out of five.
	{ N_("Resist penalty"),
	    []() {
	        const int penalty = oracool::ResistancePenaltyFor(sgGameInitInfo.nDifficulty);
	        return StyledText { penalty > 0 ? UiFlags::ColorRed : UiFlags::ColorWhite,
	            penalty > 0 ? StrCat("-", penalty) : "-" };
	    } },
	// The ceiling. A bare number, because the value column is 44px - four digits and a sign - and
	// EnsureLayout asserts anything wider in the Debug build. The soft cap above 75 and its 3-for-1
	// cost are the wiki's job to explain; what the sheet has to say is that the road does not end at
	// 75 the way every Diablo before it did.
	{ N_("Max resist"),
	    []() { return StyledText { UiFlags::ColorWhitegold, StrCat(oracool::ResistanceHardCap) }; } },
	// Movement Speed (user, 2026-09-07: "i dont see my movement speed when i use Vigor" / "movement
	// speed stats to be listed in percentage. abilities and items increase it. curses and cold spells
	// decrease it"): 100% is a plain walk; the worn affixes and the burning Vigor raise it, a slow
	// lowers it. Blue above the walk, red below, white at it. The feet take it in steps
	// (oracool::WalkFrameSkipFor); the number here is what the sources add up to.
	{ N_("Move speed"),
	    []() {
	        const int percent = oracool::MovementSpeedPercent(*InspectPlayer);
	        const UiFlags color = percent > 100 ? UiFlags::ColorBlue : percent < 100 ? UiFlags::ColorRed : UiFlags::ColorWhite;
	        return StyledText { color, StrCat(percent, "%") };
	    } },

	// Life and mana share the attributes' column meaning rather than vanilla's reading order:
	// current on the left with Now, maximum on the right with Base. Reversing these two while
	// leaving the attributes swapped would put "what you have" on a different side depending on
	// which half of the sheet you were reading.
	{ N_("Life"),
	    []() { return StyledText { (InspectPlayer->_pHitPoints != InspectPlayer->_pMaxHP ? UiFlags::ColorRed : GetMaxHealthColor()), StrCat(InspectPlayer->_pHitPoints >> 6) }; },
	    []() { return StyledText { GetMaxHealthColor(), StrCat(InspectPlayer->_pMaxHP >> 6) }; },
	    CharRowGroupGap },
	// Mana - or, for the Barbarian, Rage (2026-09-13, oracool/rage.h). Rage below its maximum is the
	// normal state rather than a wound, so it is not written in red; it takes the orb's orange.
	{ N_("Mana"),
	    []() {
		    if (oracool::UsesRage(*InspectPlayer))
			    return StyledText { UiFlags::ColorOrange, StrCat(InspectPlayer->_pRage) };
		    return StyledText { (InspectPlayer->_pMana != InspectPlayer->_pMaxMana ? UiFlags::ColorRed : GetMaxManaColor()), StrCat(InspectPlayer->_pMana >> 6) };
	    },
	    []() {
		    if (oracool::UsesRage(*InspectPlayer))
			    return StyledText { UiFlags::ColorOrange, StrCat(oracool::MaxRage(*InspectPlayer)) };
		    return StyledText { GetMaxManaColor(), StrCat(InspectPlayer->_pMaxMana >> 6) };
	    },
	    0, CharRowExtra::None,
	    []() { return std::string(oracool::UsesRage(*InspectPlayer) ? _("Rage") : _("Mana")); } },
	// Essence - the Necromancer's second pool (2026-09-18, oracool/essence.h), under Mana in the orb's own
	// green, current beside maximum like Life and Mana. Only on the sheet of a hero who has one.
	{ N_("Essence"),
	    []() { return StyledText { UiFlags::ColorOracoolGreen, StrCat(oracool::CurrentEssence(*InspectPlayer)) }; },
	    []() { return StyledText { UiFlags::ColorOracoolGreen, StrCat(oracool::MaxEssence(*InspectPlayer)) }; },
	    0, CharRowExtra::None, nullptr,
	    []() { return InspectPlayer != nullptr && oracool::UsesEssence(*InspectPlayer); } },

	// ---------------------------------------------------------------------------------------
	// Everything below here the engine tracks and acts on, but no Diablo character sheet has
	// ever shown. See the derived-reading helpers above for where each number comes from.
	// ---------------------------------------------------------------------------------------

	{ N_("Block chance"),
	    []() { return StyledText { InspectPlayer->_pBlockFlag ? UiFlags::ColorWhite : UiFlags::ColorRed, StrCat(BlockChancePercent(), "%") }; },
	    nullptr, CharRowGroupGap },
	{ N_("Damage taken"),
	    // _pIGetHit is added to incoming damage (player.cpp:691), so negative is the good
	    // direction - hence the flipped colouring.
	    []() { return StyledText { GetValueColor(InspectPlayer->_pIGetHit, true), StrCat(InspectPlayer->_pIGetHit) }; } },
	{ N_("Trap damage"),
	    []() {
	        const bool halved = HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::HalfTrapDamage);
	        return StyledText { halved ? UiFlags::ColorBlue : UiFlags::ColorWhite, halved ? "50%" : "100%" };
	    } },
	{ N_("Thorns"),
	    // A flat 1-3 per melee hit taken, not scaled by anything (monster.cpp:1231).
	    []() {
	        const bool thorns = HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::Thorns);
	        return StyledText { thorns ? UiFlags::ColorBlue : UiFlags::ColorWhite, thorns ? "1-3" : "-" };
	    } },

	{ N_("Armor pierce"),
	    []() { return PlainValue(InspectPlayer->_pIEnAc); },
	    nullptr, CharRowGroupGap },
	{ N_("Spell to hit"),
	    // "Always" while the Diablo II rule is on trial (SpellsNeverMiss): the percentage would name a roll
	    // that is no longer made.
	    []() { return StyledText { UiFlags::ColorWhite, SpellsNeverMiss ? std::string(_("Always")) : StrCat(InspectPlayer->GetMagicToHit(), "%") }; } },
	{ N_("Fire damage"),
	    []() { return DamageRange(InspectPlayer->_pIFMinDam, InspectPlayer->_pIFMaxDam); } },
	{ N_("Lightning damage"),
	    []() { return DamageRange(InspectPlayer->_pILMinDam, InspectPlayer->_pILMaxDam); } },

	// Animation frames, so LOWER is faster. Now is what the equipped items give, Base is the
	// class/weapon animation before any speed modifier - the same column meaning as the
	// attributes above.
	{ "",
	    []() { return StyledText { UiFlags::ColorWhitegold, std::string(_("Now")) }; },
	    []() { return StyledText { UiFlags::ColorWhitegold, std::string(_("Base")) }; },
	    CharRowGroupGap },
	{ N_("Attack frames"),
	    []() {
	        const int skipped = AttackFramesSkipped();
	        return StyledText { skipped > 0 ? UiFlags::ColorBlue : UiFlags::ColorWhite,
		        StrCat(InspectPlayer->_pAFrames - skipped) };
	    },
	    []() { return StyledText { UiFlags::ColorWhite, StrCat(InspectPlayer->_pAFrames) }; } },
	{ N_("Recovery frames"),
	    []() {
	        const int skipped = HitRecoveryFramesSkipped();
	        return StyledText { skipped > 0 ? UiFlags::ColorBlue : UiFlags::ColorWhite,
		        StrCat(InspectPlayer->_pHFrames - skipped) };
	    },
	    []() { return StyledText { UiFlags::ColorWhite, StrCat(InspectPlayer->_pHFrames) }; } },

	{ N_("Life steal"),
	    []() {
	        // The fixed 3/5% and the random drain are independent and can both be active, so the
	        // reading has to be able to show both (player.cpp:708-745). The random one is
	        // GenerateRnd(dam / 8) - zero to just under an eighth of the damage dealt.
	        const int fixedPct = LifeStealPercent();
	        const bool random = HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::RandomStealLife);
	        if (fixedPct == 0 && !random)
		        return StyledText { UiFlags::ColorWhite, "0%" };
	        if (fixedPct == 0)
		        return StyledText { UiFlags::ColorBlue, std::string("0-12%") };
	        if (!random)
		        return StyledText { UiFlags::ColorBlue, StrCat(fixedPct, "%") };
	        return StyledText { UiFlags::ColorBlue, StrCat(fixedPct, "% +0-12%") };
	    },
	    nullptr, CharRowGroupGap },
	{ N_("Mana steal"),
	    []() {
	        const int pct = ManaStealPercent();
	        // NoMana zeroes this out no matter what the jewellery says - worth showing as such
	        // rather than as a bonus the character cannot use.
	        return StyledText { pct > 0 ? UiFlags::ColorBlue : UiFlags::ColorWhite, StrCat(pct, "%") };
	    } },

	{ N_("Spell levels"),
	    []() { return PlainValue(InspectPlayer->_pISplLvlAdd); },
	    nullptr, CharRowGroupGap },
	{ N_("Light radius"),
	    []() { return StyledText { UiFlags::ColorWhite, StrCat(InspectPlayer->_pLightRad) }; } },
	{ N_("Demon damage"),
	    []() {
	        const bool triple = HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::TripleDemonDamage);
	        return StyledText { triple ? UiFlags::ColorBlue : UiFlags::ColorWhite, triple ? "300%" : "100%" };
	    } },

	// The four contributors behind the single Armor class number above. GetArmor() is
	// _pIBonusAC + _pIAC + dexterity/5 (player.h:570), and the sheet adds level * 2 on top - so
	// these four sum to exactly what that row shows.
	{ N_("AC from armor"),
	    []() { return PlainValue(InspectPlayer->_pIAC); },
	    nullptr, CharRowGroupGap },
	{ N_("AC from magic"),
	    []() { return PlainValue(InspectPlayer->_pIBonusAC); } },
	{ N_("AC from dexterity"),
	    []() { return StyledText { UiFlags::ColorWhite, StrCat(InspectPlayer->_pDexterity / 5) }; } },
	{ N_("AC from level"),
	    []() { return StyledText { UiFlags::ColorWhite, StrCat(InspectPlayer->_pLevel * 2) }; } },

	// ---------------------------------------------------------------------------------------
	// The fork's own per-character state, which had no readout anywhere on this sheet until the
	// 2026-08-31 audit. Last, because these are the newest and least expected: a reader looking
	// for a vanilla number should not have to pass them to reach it.
	// ---------------------------------------------------------------------------------------

	// Signets are the one PERMANENT, irreversible choice a character makes - twenty for a lifetime,
	// spent and gone - and nothing in the game said how many were left. Shown as spent/cap rather
	// than as remaining, because the cap is the part a player needs to learn once.
	{ N_("Signets used"),
	    []() {
	        const int used = oracool::SignetsUsed(*InspectPlayer);
	        return StyledText { used >= oracool::SignetLifetimeCap ? UiFlags::ColorRed : UiFlags::ColorWhite,
	            StrCat(used, "/", oracool::SignetLifetimeCap) };
	    },
	    nullptr, CharRowGroupGap },
	// The active aura, by name. It is a class-tree investment that changes what every fight looks
	// like, and the only place it was visible was the icon it is readied on.
	//
	// A FULL-WIDTH row, like the readied-slot names above and for the same reason: the value column
	// is 44px and an aura is called "Blessed Aim" or "Holy Shock". A name in that column would clip,
	// and clipping is the failure mode that looks like a rendering fault rather than a layout
	// mistake. Full width means the row carries its own label, so it reads "Aura: Blessed Aim".
	{ "",
	    []() {
	        // GetActiveClassAura, not the raw _pOracoolActiveAura field: it is the one that answers
	        // None for a skill that has stopped being an aura, and for a dead player. Reading the
	        // field would have the sheet name an aura the game is not running.
	        const oracool::ClassTreeSkill aura = oracool::GetActiveClassAura(*InspectPlayer);
	        if (aura == oracool::ClassTreeSkill::None)
	            return StyledText { UiFlags::ColorWhite, std::string(_("Aura: none")) };
	        return StyledText { UiFlags::ColorBlue,
	            fmt::format(fmt::runtime(_("Aura: {:s}")), _(oracool::GetClassTreeSkillData(aura).name)) };
	    },
	    nullptr, 0, CharRowExtra::FullWidthText },
};

constexpr size_t CharRowCount = std::size(CharRows);

// Oracool V1: the sheet's own 340x720 rect, matching the waypoint list and quest log, with the
// same title band above a separator rule:
//   0..24     top margin
//   24..74    label band, "CHARACTER"
//   74..77    separator rule
//   77..101   gap below the rule
//   101..696  content area - the row list
//   696..720  bottom margin
//
// GetPanelPosition(UiPanels::Character, ...) returns the content area's top-left, and every row,
// stat button and hit-test is expressed against it, so the whole sheet moves as one.
constexpr Size CharPanelSize { 340, 720 };
constexpr int CharPanelMargin = 24;
constexpr int CharLabelHeight = 50;
constexpr int CharContentTop = CharPanelMargin + CharLabelHeight + oracool::OrnateBorderWidth + CharPanelMargin;
// Ends at the health orb's top edge rather than at the panel's height minus a margin: the orb is
// anchored to the screen's bottom-left corner and draws OVER this panel, so the Life and Mana rows
// were rendering underneath it (user, 2026-08-16). The scrolling viewport is built from this size,
// so capping it here is what keeps the content inside the visible area.
constexpr Size CharContentSize { CharPanelSize.width, oracool::SidePanelContentBottom - CharContentTop };
static_assert(CharContentSize.height > 0, "Character sheet has no content room left");

/** @brief Left edge of the label column - flush to the panel edge, clear of its 3px bevel. */
constexpr int CharLabelColumnX = oracool::OrnateBorderWidth + 3;
/** @brief The gap between the label column's right edge and the value column. */
constexpr int CharLabelValueGap = 6;
/**
 * @brief The divider between labels and values, on the panel's dead centre.
 *
 * The label/value boundary is the sheet's strongest vertical line, so it sits on the window's
 * midline rather than wherever the text happens to end.
 */
constexpr int CharColumnDividerX = CharPanelSize.width / 2;
/**
 * @brief Width of the label column - FIXED, deliberately not measured from the labels.
 *
 * An earlier version sized this to the longest translated label. That works, but it makes the
 * column geometry a function of the row list: adding one long-named stat slides every value,
 * every + button and RESET sideways across the whole sheet, so the layout quietly moves as content
 * is edited. Per user instruction, stability wins - the columns are a property of the window, not
 * of the data in it, and here they are a property of its centre line.
 *
 * The result (161px) clears every English label with room to spare - the longest was "Points to
 * distribute" at 139px until it was renamed "Level-up Points" (user, 2026-08-16), which only widens
 * the margin. EnsureLayout asserts nothing exceeds the column, so a longer label added later fails
 * loudly in the Debug build rather than silently clipping.
 */
constexpr int CharLabelColumnWidth = CharColumnDividerX - CharLabelValueGap / 2 - CharLabelColumnX;
/** @brief Width of each value column. Four digits plus a sign or percent sign. */
constexpr int CharValueColumnWidth = 44;
constexpr int CharValueColumnGap = 6;
/** @brief Keeps the button column off the panel's right bevel. */
constexpr int CharRightPad = 8;
/** @brief Clearance between the second value column and the button column. */
constexpr int CharButtonGap = 8;
/** @brief Labels are measured and drawn with no extra letter spacing. */
constexpr int CharLabelSpacing = 0;
/**
 * @brief Every string on this sheet casts a drop shadow (user, 2026-08-29: "put shadows under the
 * fonts in hero stats screen. vanilla diablo has shadows under the texts in that window").
 *
 * Named once and applied at each of the four draws rather than spelled out four times, so a row
 * added later cannot quietly be the one without a shadow. See UiFlags::Shadowed for how it is done:
 * a black copy of the glyph one pixel down and right, NOT the four-sided Outlined ring - which is
 * the difference between text sitting on the parchment and text stuck to it.
 *
 * The panel TITLE is deliberately not included: it goes through DrawOutlinedString, which already
 * rings it, and stacking a shadow under an outline muddies both.
 */
constexpr UiFlags CharTextShadow = UiFlags::Shadowed;
/** @brief Width of the scrollbar drawn in the panel's right margin. */
constexpr int CharScrollbarWidth = oracool::OrnateBorderWidth;
constexpr int CharScrollbarMinThumb = 24;
/** @brief Clearance between the widest content and the scrollbar. */
constexpr int CharScrollbarGap = 6;
/**
 * @brief Right edge available to rows - short of the scrollbar, not of the panel.
 *
 * Every column position is derived back from here, so the scrollbar can never be drawn over a
 * value or, worse, over a + button that would then be unclickable behind it.
 */
constexpr int CharContentRightLimit = CharPanelSize.width - CharRightPad - CharScrollbarWidth - CharScrollbarGap;

/**
 * @brief ClxDraw takes a BOTTOM-left origin, so a stat button's sprite is drawn this far below its
 * hit rect's top. Reproduces the vanilla offset (rect at y 138, sprite at y 157).
 */
constexpr int StatButtonSpriteDrop = 19;

// The whole column geometry is compile-time, so it cannot shift as rows are added or edited.
/** @brief The "Now" column. */
constexpr int ValueColumnX = CharLabelColumnX + CharLabelColumnWidth + CharLabelValueGap;
/** @brief The "Base" column, and the column RESET sits in. */
constexpr int SecondValueColumnX = ValueColumnX + CharValueColumnWidth + CharValueColumnGap;
/** @brief The + buttons, immediately right of Base - the number they increase. */
constexpr int StatButtonColumnX = SecondValueColumnX + CharValueColumnWidth + CharButtonGap;
/** @brief Width available to a row with no second value - experience needs all of it. */
constexpr int SingleValueWidth = CharContentRightLimit - ValueColumnX;
static_assert(SingleValueWidth >= CharValueColumnWidth,
    "single-value rows must be at least as wide as a paired column");
static_assert(CharLabelColumnX + CharLabelColumnWidth + CharLabelValueGap / 2 == CharColumnDividerX,
    "the label/value divider must land on the panel's centre line");
/** @brief Row top within the FULL list, including the gaps above it - before scrolling. */
int CharRowTop[CharRowCount] = {};
/** @brief Total height of the row list. Larger than the content area, hence the scrolling. */
int CharListHeight = 0;
/** @brief How far the list is scrolled up, in pixels. Always within 0..MaxScrollOffset. */
int ScrollOffset = 0;
int MaxScrollOffset = 0;
/** @brief One wheel notch, in the conventional three lines. */
constexpr int CharScrollStep = CharRowHeight * 3;
Point ResetButtonPosition {};
bool LayoutReady = false;
/** @brief The optional rows (CharRow::visible) that were on the sheet at the last layout, one bit each. */
uint64_t LayoutVisibleRows = 0;

bool RowVisible(const CharRow &row)
{
	return row.visible == nullptr || row.visible();
}

// -------------------------------------------------------------------------------------------------
// THE GROUPED SHEET (user, 2026-09-26, approved from a mock-up; the Grouped Hero Sheet option, ON by
// default). The same 340x720 window, canvas, title and red X as the list above, laid out in boxes:
//
//   header      name large, title and "Level N Class" right, an XP bar, "exp of next" / "N to level"
//   left        the four attributes - current value in the box, "base N" and the + button under it -
//               then the unspent points and RESET
//   right       one box per mouse button, armour class, to hit, life, mana (or rage) and essence with
//               bars, the four resistances, and the difficulty penalty with the cap
//   foot        the aura across the width, and ADVANCED STATS, which opens oracool/advanced_stats.h
//
// No gold anywhere (user: "we dont need to show gold amount in our char screen"). Everything else the
// list carried below Mana lives in the Advanced Stats window now.
//
// Every coordinate below is CONTENT-relative, like the list's: the content origin is the panel's left
// edge at GroupedContentTop, and GetCharacterContentOrigin() answers that origin while this layout is
// on - so the + buttons and RESET, which control.cpp hit-tests through GetPanelPosition(Character,
// ChrBtnsRect[i]) and GetResetStatsButtonPosition(), land where this layout draws them with no change
// to the hit-testing at all. PlaceWidgets is the one place that decides, per layout.
// -------------------------------------------------------------------------------------------------

/** @brief The grouped sheet starts just under the title band: it has no separator rule to clear. */
constexpr int GroupedContentTop = oracool::PanelTitleTop + oracool::PanelTitleHeight + 6;
/** @brief ...and ends at the orb line like the list (see CharContentSize for why the orb, not the panel). */
constexpr Size GroupedContentSize { CharPanelSize.width, oracool::SidePanelContentBottom - GroupedContentTop };
static_assert(GroupedContentSize.height > 0, "Grouped character sheet has no content room left");
/** @brief Clear of the canvas's painted side bezels; the Advanced Stats window uses the same inset. */
constexpr int GroupedMarginX = 16;
constexpr int GroupedColumnGap = 6;
/** @brief The mock-up's two columns: attributes ~118 wide on the left, the rest on the right. */
constexpr int GroupedLeftWidth = 118;
constexpr int GroupedRightX = GroupedMarginX + GroupedLeftWidth + GroupedColumnGap;
constexpr int GroupedRightWidth = CharPanelSize.width - GroupedMarginX - GroupedRightX;
constexpr int GroupedFullWidth = CharPanelSize.width - 2 * GroupedMarginX;
/** @brief Text inset inside a box, both sides. */
constexpr int GroupedPad = 8;
static_assert(GroupedRightX + GroupedRightWidth <= CharContentRightLimit + CharScrollbarGap,
    "the grouped sheet's right column must stay clear of the scrollbar");

constexpr int GroupedHeaderHeight = 62;
/** @brief Where both columns begin, under the header. */
constexpr int GroupedColumnsTop = GroupedHeaderHeight + 6;
/** @brief An attribute: the label box, a 2px gap, the "base N" strip as tall as the + button, 4px of air. */
constexpr int GroupedAttrLabelHeight = 26;
constexpr int GroupedAttrStripGap = 2;
constexpr int GroupedAttrStripHeight = 22; // IncrementAttributeButtonSize's height, so the + fills its cell
constexpr int GroupedAttrPitch = GroupedAttrLabelHeight + GroupedAttrStripGap + GroupedAttrStripHeight + 4;
constexpr int GroupedPointsHeight = 42;
constexpr int GroupedResetHeight = 24; // ResetStatsButtonSize's height - the list's RESET, stretched across the column
/** @brief The right column's box heights. */
constexpr int GroupedButtonBoxHeight = 40;
constexpr int GroupedLineBoxHeight = 24;
constexpr int GroupedPoolBoxHeight = 32;
constexpr int GroupedResistPitch = 27;
constexpr int GroupedPenaltyHeight = 18;
constexpr int GroupedAuraHeight = 26;
constexpr Size GroupedAdvancedButtonSize { 200, 28 };

/** @brief The layout drawn last - read by the draw, written by PlaceWidgets. Content-relative, unscrolled. */
struct GroupedLayout {
	int attrTop[4];
	int pointsTop;
	int resetTop;
	int leftButtonTop;
	int rightButtonTop;
	int armorTop;
	int toHitTop;
	int lifeTop;
	int manaTop;
	int essenceTop; // -1 when the hero has no Essence
	int resistTop;
	int penaltyTop;
	int auraTop;
	int advancedTop;
	int listHeight;
};
GroupedLayout Grouped {};
/** @brief The two buttons the grouped layout adds, content-relative WITH the scroll - like ChrBtnsRect. */
Rectangle GroupedAdvancedButton {};
bool AdvancedButtonPressed = false;
/** @brief Whether the pointer was over each button last frame, so the click sounds once per ENTRY. */
bool ResetButtonHovered = false;
bool AdvancedButtonHovered = false;
/** @brief Which layout the last EnsureLayout built, so flipping the option mid-game re-places the widgets. */
bool LayoutGrouped = false;

bool GroupedSheet()
{
	return *sgOptions.Oracool.heroSheetGrouped;
}

bool GroupedShowsEssence()
{
	return InspectPlayer != nullptr && oracool::UsesEssence(*InspectPlayer);
}

/** @brief Computes every box's top. Only Essence changes it, and EnsureLayout re-runs when that does. */
GroupedLayout ComputeGroupedLayout()
{
	GroupedLayout g {};
	for (int i = 0; i < 4; i++)
		g.attrTop[i] = GroupedColumnsTop + i * GroupedAttrPitch;
	g.pointsTop = GroupedColumnsTop + 4 * GroupedAttrPitch; // the last attribute's 4px of air is the gap
	g.resetTop = g.pointsTop + GroupedPointsHeight + 4;
	const int leftEnd = g.resetTop + GroupedResetHeight;

	int y = GroupedColumnsTop;
	g.leftButtonTop = y;
	y += GroupedButtonBoxHeight + 4;
	g.rightButtonTop = y;
	y += GroupedButtonBoxHeight + 6;
	g.armorTop = y;
	y += GroupedLineBoxHeight + 4;
	g.toHitTop = y;
	y += GroupedLineBoxHeight + 6;
	g.lifeTop = y;
	y += GroupedPoolBoxHeight + 4;
	g.manaTop = y;
	y += GroupedPoolBoxHeight + 4;
	g.essenceTop = -1;
	if (GroupedShowsEssence()) {
		g.essenceTop = y;
		y += GroupedPoolBoxHeight + 4;
	}
	y += 2;
	g.resistTop = y;
	y += 4 * GroupedResistPitch;
	g.penaltyTop = y;
	const int rightEnd = y + GroupedPenaltyHeight;

	g.auraTop = std::max(leftEnd, rightEnd) + 8;
	// ADVANCED STATS sits at the foot of the window, centred, as in the mock-up - pushed down only if
	// the boxes above ever grow into it, and then the sheet scrolls like the list does.
	g.advancedTop = std::max(GroupedContentSize.height - 6 - GroupedAdvancedButtonSize.height,
	    g.auraTop + GroupedAuraHeight + 12);
	g.listHeight = g.advancedTop + GroupedAdvancedButtonSize.height + 6;
	return g;
}

uint64_t VisibleRowsNow()
{
	uint64_t bits = 0;
	for (size_t i = 0; i < CharRowCount; ++i) {
		if (CharRows[i].visible != nullptr && CharRows[i].visible())
			bits |= uint64_t { 1 } << (i % 64);
	}
	return bits;
}

/**
 * @brief Writes the scroll-adjusted widget positions into ChrBtnsRect and ResetButtonPosition.
 *
 * Those two are read by control.cpp, plrctrls.cpp and the touch renderer to hit-test, always via
 * GetPanelPosition(UiPanels::Character, ...) which adds the content origin. So they have to carry
 * the scroll, or a scrolled sheet would draw its buttons in one place and accept clicks in
 * another. Called from EnsureLayout and from every scroll change - the only two things that can
 * move a widget.
 */
void PlaceWidgets()
{
	if (LayoutGrouped) {
		// The grouped sheet's own positions (2026-09-26): each + in the right-hand cell of its
		// attribute's "base N" strip, RESET across the left column under the points, ADVANCED STATS
		// centred at the foot. Same frame and same scroll rule as the list's, so control.cpp,
		// plrctrls.cpp and the touch renderer need no idea which layout is up.
		for (size_t buttonId = 0; buttonId < 4; ++buttonId) {
			Rectangle &rect = ChrBtnsRect[buttonId];
			rect.position = { GroupedMarginX + GroupedLeftWidth - rect.size.width,
				Grouped.attrTop[buttonId] + GroupedAttrLabelHeight + GroupedAttrStripGap - ScrollOffset };
		}
		ResetButtonPosition = { GroupedMarginX, Grouped.resetTop - ScrollOffset };
		GroupedAdvancedButton = { { (CharPanelSize.width - GroupedAdvancedButtonSize.width) / 2, Grouped.advancedTop - ScrollOffset },
			GroupedAdvancedButtonSize };
		return;
	}
	for (size_t i = 0; i < CharRowCount; ++i) {
		const int top = CharRowTop[i] - ScrollOffset;
		switch (CharRows[i].extra) {
		case CharRowExtra::StatStrength:
		case CharRowExtra::StatMagic:
		case CharRowExtra::StatDexterity:
		case CharRowExtra::StatVitality: {
			const size_t buttonId = static_cast<size_t>(CharRows[i].extra) - static_cast<size_t>(CharRowExtra::StatStrength);
			Rectangle &rect = ChrBtnsRect[buttonId];
			rect.position = { StatButtonColumnX, top + (CharRowHeight - rect.size.height) / 2 };
		} break;
		case CharRowExtra::Points:
			// Under the Base column, on the row whose value it resets - directly below the four
			// + buttons that spend those points.
			ResetButtonPosition = { SecondValueColumnX,
				top + (CharRowHeight - ResetStatsButtonSize.height) / 2 };
			break;
		case CharRowExtra::None:
			break;
		}
	}
}

void EnsureLayout()
{
	const bool grouped = GroupedSheet();
	if (LayoutReady && LayoutVisibleRows == VisibleRowsNow() && LayoutGrouped == grouped)
		return;
	// The option flipped in Settings with the sheet open: the other layout's scroll means nothing here.
	if (LayoutReady && LayoutGrouped != grouped)
		ScrollOffset = 0;

	// The columns themselves are compile-time constants (see CharLabelColumnWidth). All that is
	// left to check is that the content still fits them - measured here rather than assumed, so
	// that adding a long-named stat or a longer translation fails loudly instead of clipping.
	for (const CharRow &row : CharRows) {
		if (row.label[0] == '\0')
			continue;
		assert(GetLineWidth(LanguageTranslate(row.label), GameFont12, CharLabelSpacing) <= CharLabelColumnWidth
		    && "character sheet label is wider than the fixed label column - shorten it or widen CharLabelColumnWidth");
	}
	// The two dynamic labels are not in any row's static text, so the loop above cannot see them.
	// Measured by name rather than by calling the function: this runs at layout time, where there is
	// no character to ask, and these are the only two strings it can ever return.
	assert(GetLineWidth(_("Damage"), GameFont12, CharLabelSpacing) <= CharLabelColumnWidth
	    && "the readied-slot amount label does not fit the label column");
	assert(GetLineWidth(_("Healing"), GameFont12, CharLabelSpacing) <= CharLabelColumnWidth
	    && "the readied-slot amount label does not fit the label column");
	assert(GetLineWidth(_("Aura"), GameFont12, CharLabelSpacing) <= CharLabelColumnWidth
	    && "the readied-slot amount label does not fit the label column");
	// The size lives on ChrBtnsRect, which owns it; this code only ever writes positions. Checked
	// rather than assumed, because StatButtonColumnX above was derived against a literal.
	assert(StatButtonColumnX + ChrBtnsRect[0].size.width <= CharContentRightLimit
	    && "stat buttons must stay clear of the scrollbar");

	int y = 0;
	for (size_t i = 0; i < CharRowCount; ++i) {
		if (!RowVisible(CharRows[i])) {
			CharRowTop[i] = y; // no height: the rows below close up over it
			continue;
		}
		y += CharRows[i].gapAbove;
		CharRowTop[i] = y;
		y += CharRowHeight;
	}
	CharListHeight = y;
	LayoutVisibleRows = VisibleRowsNow();

	// The list is deliberately taller than the window now - the hidden stats below Mana do not fit
	// and are not meant to. Overshooting the bottom by less than a row would be an accident,
	// though, so the clamp is against the real total either way.
	MaxScrollOffset = std::max(0, CharListHeight - CharContentSize.height);
	// The grouped sheet measures itself instead (2026-09-26). It fits its window as designed, so its
	// maximum is normally zero and it never shows a scrollbar; the arithmetic is here so that a box
	// added later scrolls rather than slides under the orb.
	LayoutGrouped = grouped;
	if (grouped) {
		Grouped = ComputeGroupedLayout();
		MaxScrollOffset = std::max(0, Grouped.listHeight - GroupedContentSize.height);
	}
	ScrollOffset = std::min(ScrollOffset, MaxScrollOffset);
	PlaceWidgets();
	LayoutReady = true;
}

/**
 * @brief Draws one row into the CONTENT surface - coordinates are content-relative and scrolled.
 *
 * @p content is a subregion covering only the area below the separator, so a row straddling the
 * top or bottom edge is cut cleanly there instead of drawing over the title band or the panel's
 * bottom bevel. That is what makes pixel scrolling safe without a per-row visibility test.
 */
void DrawRow(const Surface &content, size_t index)
{
	const CharRow &row = CharRows[index];
	if (!RowVisible(row))
		return; // not this hero's row (Essence on anyone but the Necromancer)
	const int top = CharRowTop[index] - ScrollOffset;
	if (top + CharRowHeight <= 0 || top >= CharContentSize.height)
		return; // entirely scrolled out - skip the work, the clip would have hidden it anyway

	if (row.extra == CharRowExtra::FullWidthText) {
		// Spans label column, value columns and button column - the one row shape that can hold any
		// skill name without measuring it first.
		//
		// CENTRED (user, 2026-08-31, with a screenshot). Left-aligned it started at the panel's own
		// edge while every other label on the sheet is right-aligned against the column divider, so
		// the name and the damage row under it read as two broken rows rather than one heading and
		// its number. Centred, it is plainly a heading for the row beneath - which is what it is.
		const StyledText text = row.value();
		DrawString(content, text.text,
		    { { CharLabelColumnX, top }, { CharContentRightLimit - CharLabelColumnX, CharRowHeight } },
		    { UiFlags::AlignCenter | UiFlags::VerticalCenter | text.style | CharTextShadow, text.spacing });
		return;
	}

	if (row.dynamicLabel != nullptr) {
		// Already translated by the function - it chooses between whole words rather than
		// assembling one, so there is nothing left for LanguageTranslate to do.
		DrawString(content, row.dynamicLabel(),
		    { { CharLabelColumnX, top }, { CharLabelColumnWidth, CharRowHeight } },
		    { UiFlags::AlignRight | UiFlags::VerticalCenter | UiFlags::ColorWhitegold | CharTextShadow,
		        CharLabelSpacing });
	} else if (row.label[0] != '\0') {
		DrawString(content, LanguageTranslate(row.label),
		    { { CharLabelColumnX, top }, { CharLabelColumnWidth, CharRowHeight } },
		    { UiFlags::AlignRight | UiFlags::VerticalCenter | UiFlags::ColorWhitegold | CharTextShadow,
		        CharLabelSpacing });
	}

	// The points row has no second value but does have RESET sitting in the Base column, so it
	// takes a column-width slot rather than the full run out to the scrollbar.
	const int firstWidth = (row.secondValue != nullptr || row.extra == CharRowExtra::Points)
	    ? CharValueColumnWidth
	    : SingleValueWidth;
	const StyledText first = row.value();
	DrawString(content, first.text,
	    { { ValueColumnX, top }, { firstWidth, CharRowHeight } },
	    { UiFlags::VerticalCenter | first.style | CharTextShadow, first.spacing });

	if (row.secondValue != nullptr) {
		const StyledText second = row.secondValue();
		DrawString(content, second.text,
		    { { SecondValueColumnX, top }, { CharValueColumnWidth, CharRowHeight } },
		    { UiFlags::VerticalCenter | second.style | CharTextShadow, second.spacing });
	}
}

/** @brief The theme's scrollbar: a recessed groove in the right margin with a bevelled thumb. */
void DrawScrollbar(const Surface &out, const Rectangle &panel)
{
	if (MaxScrollOffset <= 0)
		return;

	const int x = panel.position.x + CharPanelSize.width - CharRightPad - CharScrollbarWidth;
	const int top = panel.position.y + CharContentTop;
	oracool::DrawThemedFill(out, { { x, top }, { CharScrollbarWidth, CharContentSize.height } }, 2);

	const int thumbHeight = std::max(CharScrollbarMinThumb,
	    CharContentSize.height * CharContentSize.height / CharListHeight);
	const int travel = CharContentSize.height - thumbHeight;
	const int thumbY = top + travel * ScrollOffset / MaxScrollOffset;
	oracool::DrawOrnateSeparatorVertical(out, { x, thumbY }, thumbHeight);
}

/**
 * @brief The four vanilla + sprites, at ChrBtnsRect's scrolled positions - for both layouts.
 *
 * Split out of DrawStatButtons on 2026-09-26 so the grouped sheet draws the very same sprites with
 * the very same show/hide rule (points to spend, not inspecting, base below 255), in the cells its own
 * PlaceWidgets gave them. Its RESET is drawn by the grouped sheet itself.
 */
void DrawPlusButtonSprites(const Surface &content)
{
	if (InspectPlayer->_pStatPts > 0 && !IsInspectingPlayer()) {
		const auto drawButton = [&content](CharacterAttribute attr, int upFrame) {
			const size_t buttonId = static_cast<size_t>(attr);
			const Point position = ChrBtnsRect[buttonId].position + Displacement { 0, StatButtonSpriteDrop };
			ClxDraw(content, position, (*pChrButtons)[chrbtn[buttonId] ? upFrame + 1 : upFrame]);
		};
		if (InspectPlayer->_pBaseStr < 255)
			drawButton(CharacterAttribute::Strength, 1);
		if (InspectPlayer->_pBaseMag < 255)
			drawButton(CharacterAttribute::Magic, 3);
		if (InspectPlayer->_pBaseDex < 255)
			drawButton(CharacterAttribute::Dexterity, 5);
		if (InspectPlayer->_pBaseVit < 255)
			drawButton(CharacterAttribute::Vitality, 7);
	}
}

/**
 * @brief Draws the + buttons and RESET into the CONTENT surface, at their scrolled positions.
 *
 * Content-relative, like DrawRow, for the same two reasons: ChrBtnsRect and ResetButtonPosition
 * already carry the scroll (see PlaceWidgets), and drawing through the clipped subregion means a
 * button scrolled halfway out is cut at the window edge rather than drawn over the title.
 */
void DrawStatButtons(const Surface &content)
{
	DrawPlusButtonSprites(content);

	if (*sgOptions.Oracool.resetStatsButton && !gbIsMultiplayer && !IsInspectingPlayer()) {
		// Sits on the "Level-up Points" row, under the Base column the + buttons feed, so it
		// reads as "reset the points shown right here" rather than a disconnected button elsewhere
		// on the panel. The same ResetButtonPosition drives control.cpp's press/release
		// hit-testing through GetResetStatsButtonPosition() - never a fresh literal.
		// Oracool: a circular-arrow glyph (Unicode U+21BA), then a plain "R", didn't read well
		// against the game's actual bitmap font - now a gold "RESET" word label instead, turning
		// white while pressed for visible click feedback.
		DrawString(content, "RESET", { ResetButtonPosition, ResetStatsButtonSize }, { UiFlags::AlignCenter | UiFlags::VerticalCenter | (resetStatsButtonDown ? UiFlags::ColorWhite : UiFlags::ColorGold) | CharTextShadow });
	}
}

// ---- The grouped sheet's drawing (2026-09-26) ----------------------------------------------------

/** @brief A box's text line: inset by GroupedPad left and right, @p height tall, @p yOffset below its top. */
Rectangle BoxLine(const Rectangle &box, int yOffset, int height)
{
	return { { box.position.x + GroupedPad, box.position.y + yOffset }, { box.size.width - 2 * GroupedPad, height } };
}

/** @brief A content-relative rect in screen space - where the pointer has to be to be over it. */
Rectangle ContentToScreen(const Rectangle &rect)
{
	const Point origin = GetCharacterContentOrigin();
	return { { origin.x + rect.position.x, origin.y + rect.position.y }, rect.size };
}

/** @brief The click every button in this mod sounds on hover ENTRY (user, 2026-09-20), once, not every frame. */
void SoundOnHoverEntry(bool hovered, bool &wasHovered)
{
	if (hovered && !wasHovered)
		oracool::PlayUiMoveSound();
	wasHovered = hovered;
}

/**
 * @brief A label on the left and a value on the right of one line - the grouped sheet's basic row.
 *
 * The value is measured and drawn first; the label gets what is left and steps down a font size rather
 * than run into the number (a translated "Resist lightning" is the long one).
 */
void DrawLabelValue(const Surface &content, const Rectangle &line, string_view label, const StyledText &value)
{
	DrawString(content, value.text, line,
	    { UiFlags::AlignRight | UiFlags::VerticalCenter | value.style | CharTextShadow, value.spacing });
	const int valueWidth = GetLineWidth(value.text, GameFont12, value.spacing);
	const Rectangle labelRect { line.position, { std::max(line.size.width - valueWidth - 6, 16), line.size.height } };
	oracool::DrawSheetTextFitted(content, label, labelRect, UiFlags::ColorWhite | UiFlags::VerticalCenter);
}

/**
 * @brief The grouped sheet's two buttons - RESET and ADVANCED STATS - in the game-wide button feel
 * (user, 2026-09-20/21): the vendors' vanilla face, gold when @p selected, a notch lighter under the
 * pointer, and while held the FACE sinks 2px down and 2px left. The hit rect is never moved - the
 * caller tests the unsunk rect - so a button cannot slide out from under a pointer that has not moved.
 */
void DrawGroupedButton(const Surface &content, const Rectangle &rect, string_view label, bool pressed, bool hovered, bool selected)
{
	const Rectangle face { rect.position + (pressed ? Displacement { -2, 2 } : Displacement { 0, 0 }), rect.size };
	if (!oracool::DrawVendorButtonBacking(content, face, selected, hovered)) {
		// No vanilla button in the player's archive (or an indexed surface): the heading plate stands in.
		oracool::DrawSheetBox(content, face, selected ? oracool::SheetBoxTone::Heading : oracool::SheetBoxTone::Plain);
		if (hovered)
			BrightenRectRgb(content, face.position.x, face.position.y, face.size.width, face.size.height, 115);
	}
	// Gold labels on the grey face, white on the gold one - the crafting book's rule.
	DrawString(content, label, face,
	    { UiFlags::AlignCenter | UiFlags::VerticalCenter | (selected || pressed ? UiFlags::ColorWhite : UiFlags::ColorWhitegold) | CharTextShadow });
}

/** @brief The words for what a button's number is, by element - "fire damage", or plain "damage". */
string_view DamageKindPhrase(DamageType type)
{
	switch (type) {
	case DamageType::Fire:
		return _("fire damage");
	case DamageType::Cold:
		return _("cold damage");
	case DamageType::Lightning:
		return _("lightning damage");
	case DamageType::Magic:
		return _("magic damage");
	case DamageType::Acid:
		return _("acid damage");
	case DamageType::Physical:
		break;
	}
	return _("damage");
}

/**
 * @brief The grey second line of a button's box: "left button  -  damage", "right button  -  magic damage".
 *
 * The mock-up's replacement for the list's "Left button: Zeal" / "Damage" pair. It asks the same four
 * questions ReadiedSlotColor does, in the same order, so the grey words and the coloured number above
 * them always name the same thing: a burning aura's element, a quiet aura, the weapon, a heal, a spell
 * with nothing to report (the dash), and otherwise the spell's own element.
 */
std::string ReadiedSlotKindText(bool leftButton)
{
	const Player &player = *InspectPlayer;
	const SpellID spell = leftButton ? player._pLRSpell : player._pRSpell;
	string_view kind;
	if (const auto damaging = DamagingAuraOnButton(leftButton); damaging.has_value())
		kind = DamageKindPhrase(damaging->first);
	else if (AuraOnButton(leftButton) != oracool::ClassTreeSkill::None)
		kind = _("aura");
	else if (ReadiedSlotSwingsTheWeapon(spell))
		kind = _("damage");
	else if (spell == SpellID::Healing || spell == SpellID::HealOther)
		kind = _("healing");
	else if (GetReadiedSlotDamage(leftButton).text == "-")
		kind = _("no damage");
	else
		kind = DamageKindPhrase(ReadiedSpellDamageType(spell));
	const string_view button = leftButton ? _("left button") : _("right button");
	return StrCat(button, "  -  ", kind);
}

/** @brief One mouse button's box: the readied name and its number on the first line, the grey kind under it. */
void DrawGroupedButtonBox(const Surface &content, int top, bool leftButton)
{
	const Rectangle box { { GroupedRightX, top }, { GroupedRightWidth, GroupedButtonBoxHeight } };
	oracool::DrawSheetBox(content, box);
	const Rectangle line1 = BoxLine(box, 3, 18);
	const StyledText amount = GetReadiedSlotDamage(leftButton);
	DrawString(content, amount.text, line1,
	    { UiFlags::AlignRight | UiFlags::VerticalCenter | amount.style | CharTextShadow, amount.spacing });
	const int amountWidth = GetLineWidth(amount.text, GameFont12, amount.spacing);
	const Rectangle nameRect { line1.position, { std::max(line1.size.width - amountWidth - 8, 16), line1.size.height } };
	// Fitted: "Master of the Long Staff" beside a three-digit range is wider than the box at 12px.
	oracool::DrawSheetTextFitted(content, ReadiedSlotSkillName(leftButton), nameRect, ReadiedSlotColor(leftButton) | UiFlags::VerticalCenter);
	DrawString(content, ReadiedSlotKindText(leftButton), BoxLine(box, 21, 16),
	    { UiFlags::ColorGray5 | UiFlags::FontSize11 | UiFlags::VerticalCenter | CharTextShadow });
}

/**
 * @brief A pool's box: "Life   412 / 450" over a bar. The current number keeps the list's colouring
 * (red below full), the maximum its own (blue when items raised it).
 */
void DrawGroupedPoolBox(const Surface &content, int top, string_view label, const StyledText &current, const StyledText &maximum,
    int value, int maximumValue, uint32_t barRgb, uint8_t barFallback)
{
	const Rectangle box { { GroupedRightX, top }, { GroupedRightWidth, GroupedPoolBoxHeight } };
	oracool::DrawSheetBox(content, box);
	const Rectangle line = BoxLine(box, 3, 16);
	DrawString(content, maximum.text, line, { UiFlags::AlignRight | UiFlags::VerticalCenter | maximum.style | CharTextShadow });
	const int maxWidth = GetLineWidth(maximum.text, GameFont12, 1);
	const std::string lead = StrCat(current.text, " / ");
	const Rectangle leadRect { line.position, { line.size.width - maxWidth - 1, line.size.height } };
	DrawString(content, lead, leadRect, { UiFlags::AlignRight | UiFlags::VerticalCenter | current.style | CharTextShadow });
	const int pairWidth = maxWidth + 1 + GetLineWidth(lead, GameFont12, 1);
	const Rectangle labelRect { line.position, { std::max(line.size.width - pairWidth - 6, 16), line.size.height } };
	oracool::DrawSheetTextFitted(content, label, labelRect, UiFlags::ColorWhite | UiFlags::VerticalCenter);
	oracool::DrawSheetBar(content, BoxLine(box, 22, 6), static_cast<uint64_t>(std::max(value, 0)),
	    static_cast<uint64_t>(std::max(maximumValue, 0)), barRgb, barFallback);
}

/** @brief The difficulty's name, for the penalty strip - the same four words the game's menus use. */
string_view GroupedDifficultyName()
{
	static constexpr const char *Names[] = { N_("Normal"), N_("Nightmare"), N_("Hell"), N_("Torment") };
	const int difficulty = std::clamp(static_cast<int>(sgGameInitInfo.nDifficulty), 0, 3);
	return _(Names[difficulty]);
}

/** @brief The header: name, title and level, the XP bar, and the XP line. No gold (user, 2026-09-26). */
void DrawGroupedHeader(const Surface &content, int top)
{
	const Player &p = *InspectPlayer;
	const Rectangle box { { GroupedMarginX, top }, { GroupedFullWidth, GroupedHeaderHeight } };
	oracool::DrawSheetBox(content, box);
	const int innerX = box.position.x + GroupedPad;
	const int innerWidth = box.size.width - 2 * GroupedPad;

	// Right: "Slayer  Level 34 Paladin" - the title in its own colour (oracool/hero_title.h), then the
	// level and class in white.
	const std::string levelText = fmt::format(fmt::runtime(_(/* TRANSLATORS: {:d} is the level, {:s} the class */ "Level {:d} {:s}")),
	    p._pLevel, _(PlayersData[static_cast<std::size_t>(p._pClass)].className));
	const string_view title = _(oracool::HeroTitleFor(p.pDiabloKillLevel));
	const int levelWidth = GetLineWidth(levelText, GameFont12, 1);
	const int titleWidth = GetLineWidth(title, GameFont12, 1);
	const Rectangle rightLine { { innerX, top + 8 }, { innerWidth, 20 } };
	DrawString(content, levelText, rightLine, { UiFlags::AlignRight | UiFlags::VerticalCenter | UiFlags::ColorWhite | CharTextShadow });
	const Rectangle titleLine { rightLine.position, { std::max(innerWidth - levelWidth - 6, 16), rightLine.size.height } };
	DrawString(content, title, titleLine,
	    { UiFlags::AlignRight | UiFlags::VerticalCenter | oracool::HeroTitleColorFor(p.pDiabloKillLevel) | CharTextShadow });

	// Left: the name, large. A long name beside a long title steps down to the small font rather than
	// run under the title.
	const int nameRoom = std::max(innerWidth - levelWidth - 6 - titleWidth - 10, 16);
	const Rectangle nameRect { { innerX, top + 4 }, { nameRoom, 28 } };
	const string_view name = p._pName;
	if (GetLineWidth(name, GameFont24, 1) <= nameRoom)
		DrawString(content, name, nameRect, { UiFlags::FontSize24 | UiFlags::ColorWhitegold | UiFlags::VerticalCenter | CharTextShadow });
	else
		oracool::DrawSheetTextFitted(content, name, nameRect, UiFlags::ColorWhitegold | UiFlags::VerticalCenter);

	// The XP bar: how far into the current level, qol/xpbar.cpp's FilledWidth arithmetic exactly -
	// GetLevelExperienceSpan is the one authority for the span and its bounds. Full at the cap.
	uint64_t into = 1;
	uint64_t span = 1;
	if (p._pLevel >= 1 && p._pLevel < static_cast<int>(MaxCharacterLevel)) {
		span = oracool::GetLevelExperienceSpan(p);
		const uint64_t levelStart = ExpLvlsTbl[p._pLevel - 1];
		into = p._pExperience >= levelStart ? p._pExperience - levelStart : 0;
	}
	oracool::DrawSheetBar(content, { { innerX, top + 35 }, { innerWidth, 8 } }, into, span, 0xC8A04C, PAL16_YELLOW + 4);

	// "1,284,300 of 1,520,000" left, "235,700 to level 35" right.
	const Rectangle xpLine { { innerX, top + 45 }, { innerWidth, 14 } };
	std::string left;
	std::string right;
	if (p._pLevel >= static_cast<int>(MaxCharacterLevel)) {
		left = FormatInteger(p._pExperience);
		right = std::string(_("Max level"));
	} else {
		left = fmt::format(fmt::runtime(_(/* TRANSLATORS: experience, "1,284,300 of 1,520,000" */ "{:s} of {:s}")),
		    FormatInteger(p._pExperience), FormatInteger(p._pNextExper));
		const uint64_t remaining = p._pNextExper > p._pExperience ? p._pNextExper - p._pExperience : 0;
		right = fmt::format(fmt::runtime(_("{:s} to level {:d}")), FormatInteger(remaining), p._pLevel + 1);
	}
	DrawString(content, right, xpLine, { UiFlags::AlignRight | UiFlags::VerticalCenter | UiFlags::ColorWhite | CharTextShadow });
	const int rightWidth = GetLineWidth(right, GameFont12, 1);
	oracool::DrawSheetTextFitted(content, left, { xpLine.position, { std::max(innerWidth - rightWidth - 8, 16), xpLine.size.height } },
	    UiFlags::ColorWhite | UiFlags::VerticalCenter);
}

/** @brief The left column: four attributes, the unspent points, and RESET. */
void DrawGroupedLeftColumn(const Surface &content)
{
	struct AttributeRow {
		const char *label;
		CharacterAttribute attribute;
	};
	// CharacterAttribute's order, which is ChrBtnsRect's - the same pairing the list's static_asserts pin.
	constexpr AttributeRow Attributes[] = {
		{ N_("Strength"), CharacterAttribute::Strength },
		{ N_("Magic"), CharacterAttribute::Magic },
		{ N_("Dexterity"), CharacterAttribute::Dexterity },
		{ N_("Vitality"), CharacterAttribute::Vitality },
	};
	for (size_t i = 0; i < std::size(Attributes); ++i) {
		const CharacterAttribute attribute = Attributes[i].attribute;
		const Rectangle labelBox { { GroupedMarginX, Grouped.attrTop[i] - ScrollOffset }, { GroupedLeftWidth, GroupedAttrLabelHeight } };
		oracool::DrawSheetBox(content, labelBox);
		DrawLabelValue(content, BoxLine(labelBox, 0, GroupedAttrLabelHeight), LanguageTranslate(Attributes[i].label),
		    StyledText { GetCurrentStatColor(attribute), StrCat(InspectPlayer->GetCurrentAttributeValue(attribute)) });

		// "base N" in a recessed strip, and the + button's cell beside it - the cell is drawn whether or
		// not the + is (no points to spend), so the strip keeps its shape either way.
		const Rectangle &plus = ChrBtnsRect[static_cast<size_t>(attribute)];
		const Rectangle strip { { GroupedMarginX, plus.position.y }, { GroupedLeftWidth - plus.size.width - 2, GroupedAttrStripHeight } };
		oracool::DrawSheetBox(content, strip, oracool::SheetBoxTone::Recess);
		DrawString(content, fmt::format(fmt::runtime(_("base {:d}")), InspectPlayer->GetBaseAttributeValue(attribute)),
		    { { strip.position.x + 6, strip.position.y }, { strip.size.width - 8, strip.size.height } },
		    { GetBaseStatColor(attribute) | UiFlags::FontSize11 | UiFlags::VerticalCenter | CharTextShadow });
		oracool::DrawSheetBox(content, plus, oracool::SheetBoxTone::Recess);
	}

	// The unspent points - both kinds, since "what do I have left to spend" is one question (the list's
	// 2026-08-31 audit note). Red while there is something to spend, a plain 0 otherwise.
	const Rectangle pointsBox { { GroupedMarginX, Grouped.pointsTop - ScrollOffset }, { GroupedLeftWidth, GroupedPointsHeight } };
	oracool::DrawSheetBox(content, pointsBox);
	// The list's Level-up Points row clamps the pool to what the stats can still take, every frame it
	// is drawn; the grouped sheet does the same, or the two layouts would show different numbers.
	InspectPlayer->_pStatPts = std::min(CalcStatDiff(*InspectPlayer), InspectPlayer->_pStatPts);
	const auto drawPoints = [&](int yOffset, int count, string_view label) {
		const UiFlags color = count > 0 ? UiFlags::ColorRed : UiFlags::ColorWhite;
		const int y = pointsBox.position.y + yOffset;
		DrawString(content, StrCat(count), { { pointsBox.position.x + 4, y }, { 26, 17 } },
		    { UiFlags::AlignRight | UiFlags::VerticalCenter | color | CharTextShadow });
		oracool::DrawSheetTextFitted(content, label, { { pointsBox.position.x + 36, y }, { pointsBox.size.width - 40, 17 } },
		    color | UiFlags::VerticalCenter);
	};
	drawPoints(3, InspectPlayer->_pStatPts, _("stat points")); // "level-up points" did not fit the column (preview render, 2026-09-26)
	drawPoints(22, InspectPlayer->_pUnspentSkillPoints, _("skill points"));

	// RESET - the list's button (same option, same gates, same press/release in control.cpp), stretched
	// across the column and wearing the button face.
	if (*sgOptions.Oracool.resetStatsButton && !gbIsMultiplayer && !IsInspectingPlayer()) {
		const Rectangle reset { ResetButtonPosition, GetResetStatsButtonSize() };
		const bool hovered = ContentToScreen(reset).contains(MousePosition) && GetCharacterContentRect().contains(MousePosition);
		SoundOnHoverEntry(hovered, ResetButtonHovered);
		DrawGroupedButton(content, reset, _("RESET"), resetStatsButtonDown, hovered, /*selected=*/false);
	} else {
		ResetButtonHovered = false;
	}
}

/** @brief The right column: both mouse buttons, armour, to hit, the pools, the resistances and the penalty. */
void DrawGroupedRightColumn(const Surface &content)
{
	const Player &p = *InspectPlayer;
	DrawGroupedButtonBox(content, Grouped.leftButtonTop - ScrollOffset, /*leftButton=*/true);
	DrawGroupedButtonBox(content, Grouped.rightButtonTop - ScrollOffset, /*leftButton=*/false);

	// Armour class and to hit: the list's two rows' numbers and colours exactly.
	const Rectangle armorBox { { GroupedRightX, Grouped.armorTop - ScrollOffset }, { GroupedRightWidth, GroupedLineBoxHeight } };
	oracool::DrawSheetBox(content, armorBox);
	DrawLabelValue(content, BoxLine(armorBox, 0, GroupedLineBoxHeight), _("Armor class"),
	    StyledText { GetValueColor(p._pIBonusAC), StrCat(p.GetArmor() + p._pLevel * 2) });
	const Rectangle toHitBox { { GroupedRightX, Grouped.toHitTop - ScrollOffset }, { GroupedRightWidth, GroupedLineBoxHeight } };
	oracool::DrawSheetBox(content, toHitBox);
	DrawLabelValue(content, BoxLine(toHitBox, 0, GroupedLineBoxHeight), _("To hit"), ToHitReading());

	// Life, red.
	DrawGroupedPoolBox(content, Grouped.lifeTop - ScrollOffset, _("Life"),
	    StyledText { p._pHitPoints != p._pMaxHP ? UiFlags::ColorRed : GetMaxHealthColor(), StrCat(p._pHitPoints >> 6) },
	    StyledText { GetMaxHealthColor(), StrCat(p._pMaxHP >> 6) },
	    p._pHitPoints, p._pMaxHP, 0xB82828, PAL16_RED + 4);
	// Mana, blue - or the Barbarian's Rage, orange and labelled so (oracool/rage.h). Rage below its
	// maximum is the normal state, not a wound, so it is never written red.
	if (oracool::UsesRage(p)) {
		const int maxRage = oracool::MaxRage(p);
		DrawGroupedPoolBox(content, Grouped.manaTop - ScrollOffset, _("Rage"),
		    StyledText { UiFlags::ColorOrange, StrCat(p._pRage) }, StyledText { UiFlags::ColorOrange, StrCat(maxRage) },
		    p._pRage, maxRage, 0xD07820, PAL16_ORANGE + 4);
	} else {
		DrawGroupedPoolBox(content, Grouped.manaTop - ScrollOffset, _("Mana"),
		    StyledText { p._pMana != p._pMaxMana ? UiFlags::ColorRed : GetMaxManaColor(), StrCat(p._pMana >> 6) },
		    StyledText { GetMaxManaColor(), StrCat(p._pMaxMana >> 6) },
		    p._pMana, p._pMaxMana, 0x3050C8, PAL16_BLUE + 4);
	}
	// Essence, green - the Necromancer's second pool, only on the sheet of a hero who has one.
	if (Grouped.essenceTop >= 0 && oracool::UsesEssence(p)) {
		const int current = oracool::CurrentEssence(p);
		const int maximum = oracool::MaxEssence(p);
		DrawGroupedPoolBox(content, Grouped.essenceTop - ScrollOffset, _("Essence"),
		    StyledText { UiFlags::ColorOracoolGreen, StrCat(current) }, StyledText { UiFlags::ColorOracoolGreen, StrCat(maximum) },
		    current, maximum, 0x3C9C4C, PAL8_GREEN + 2);
	}

	// The four resistances - cold is its own stat since 2026-09-26 (player.h's _pColdResist), resisted
	// and capped like the other three, so it reads through the same GetResistInfo colours.
	struct ResistRow {
		const char *label;
		int8_t value;
	};
	const ResistRow resists[] = {
		{ N_("Resist magic"), p._pMagResist },
		{ N_("Resist fire"), p._pFireResist },
		{ N_("Resist lightning"), p._pLghtResist },
		{ N_("Resist cold"), p._pColdResist },
	};
	for (size_t i = 0; i < std::size(resists); ++i) {
		const Rectangle box { { GroupedRightX, Grouped.resistTop + static_cast<int>(i) * GroupedResistPitch - ScrollOffset },
			{ GroupedRightWidth, GroupedLineBoxHeight } };
		oracool::DrawSheetBox(content, box);
		DrawLabelValue(content, BoxLine(box, 0, GroupedLineBoxHeight), LanguageTranslate(resists[i].label), GetResistInfo(resists[i].value));
	}

	// The strip that makes the four readable (the list's 2026-08-31 audit rows, in one line): what the
	// difficulty has already taken off, in red, and the ceiling. No penalty, no red words - just the cap.
	const Rectangle strip { { GroupedRightX, Grouped.penaltyTop - ScrollOffset }, { GroupedRightWidth, GroupedPenaltyHeight } };
	oracool::DrawSheetBox(content, strip, oracool::SheetBoxTone::Recess);
	const Rectangle stripLine = BoxLine(strip, 0, GroupedPenaltyHeight);
	DrawString(content, fmt::format(fmt::runtime(_("cap {:d}")), oracool::ResistanceHardCap), stripLine,
	    { UiFlags::AlignRight | UiFlags::VerticalCenter | UiFlags::ColorWhite | UiFlags::FontSize11 | CharTextShadow });
	if (const int penalty = oracool::ResistancePenaltyFor(sgGameInitInfo.nDifficulty); penalty > 0) {
		DrawString(content, StrCat(GroupedDifficultyName(), "  -", penalty), stripLine,
		    { UiFlags::VerticalCenter | UiFlags::ColorRed | UiFlags::FontSize11 | CharTextShadow });
	}
}

/** @brief The grouped sheet's scrollbar - the list's groove and thumb against its own content rect. */
void DrawGroupedScrollbar(const Surface &out, const Rectangle &panel)
{
	if (MaxScrollOffset <= 0)
		return;
	const int x = panel.position.x + CharPanelSize.width - CharRightPad - CharScrollbarWidth;
	const int top = panel.position.y + GroupedContentTop;
	oracool::DrawThemedFill(out, { { x, top }, { CharScrollbarWidth, GroupedContentSize.height } }, 2);
	const int thumbHeight = std::max(CharScrollbarMinThumb,
	    GroupedContentSize.height * GroupedContentSize.height / std::max(Grouped.listHeight, 1));
	const int travel = GroupedContentSize.height - thumbHeight;
	const int thumbY = top + travel * ScrollOffset / MaxScrollOffset;
	oracool::DrawOrnateSeparatorVertical(out, { x, thumbY }, thumbHeight);
}

/** @brief The whole grouped sheet, under the title DrawChr has already drawn. */
void DrawGroupedSheet(const Surface &out, const Rectangle &panel)
{
	DrawGroupedScrollbar(out, panel);

	// Through the clipped content subregion, as the list draws - the same frame ChrBtnsRect,
	// ResetButtonPosition and GroupedAdvancedButton are expressed in.
	const Rectangle contentRect = GetCharacterContentRect();
	const Surface content = out.subregion(contentRect.position.x, contentRect.position.y,
	    contentRect.size.width, contentRect.size.height);

	DrawGroupedHeader(content, -ScrollOffset);
	DrawGroupedLeftColumn(content);
	DrawGroupedRightColumn(content);

	// The aura across the width, by name (GetActiveClassAura, not the raw field - see the list's row).
	const Rectangle auraBox { { GroupedMarginX, Grouped.auraTop - ScrollOffset }, { GroupedFullWidth, GroupedAuraHeight } };
	oracool::DrawSheetBox(content, auraBox);
	const oracool::ClassTreeSkill aura = oracool::GetActiveClassAura(*InspectPlayer);
	DrawLabelValue(content, BoxLine(auraBox, 0, GroupedAuraHeight), _("Aura"),
	    aura == oracool::ClassTreeSkill::None
	        ? StyledText { UiFlags::ColorWhite, std::string(_("none")) }
	        : StyledText { UiFlags::ColorBlue, std::string(_(oracool::GetClassTreeSkillData(aura).name)) });

	// ADVANCED STATS, gold while its window is open - the vendors' "this is the page you are on".
	const bool hovered = ContentToScreen(GroupedAdvancedButton).contains(MousePosition) && contentRect.contains(MousePosition);
	SoundOnHoverEntry(hovered, AdvancedButtonHovered);
	DrawGroupedButton(content, GroupedAdvancedButton, _("ADVANCED STATS  >"), AdvancedButtonPressed, hovered,
	    oracool::IsAdvancedStatsOpen());

	// Last, over their cells.
	DrawPlusButtonSprites(content);
}

} // namespace

/**
 * @brief The colour a damage type is written in (user, 2026-08-31, after Diablo II).
 *
 * THE table. Since 1.11.080 the floating damage numbers over a monster's head defer to it too
 * (qol/floatingnumbers.cpp), so an element cannot be described two ways in one game - it was for
 * months, the sheet calling fire red while the number drew grey.
 *
 * Diablo II's palette, which is what the user asked for: white physical, blue cold, red fire,
 * yellow lightning, green poison. All five are now honoured literally (user, 2026-09-12, "make sure
 * cold and physical dmg floating texts use proper color"). Two earlier compromises are gone:
 *
 *   - Cold was WHITE, and blue went to magic, on the reasoning that "this engine has no cold damage
 *     at all". That stopped being true: Holy Freeze, Frost Nova and the Round 1 cold missiles all
 *     deal DamageType::Cold. Cold takes its own blue back.
 *   - Magic then moved off blue anyway, on 2026-09-11, to RGB 104,49,49 ("let's make Magic DMG font
 *     color RGB:104,49,49") and the same day to 208,98,98 ("way too dark make it brighter") -
 *     UiFlags::ColorMagicDamage. That is what freed blue.
 *
 * Acid is D2's poison seat: green. It is monster-only - no player spell carries it - so it never
 * appears on this sheet, but it needs a colour of its own for the floating numbers, and sharing
 * lightning's yellow there meant two elements in one ink.
 *
 * Blue is also this sheet's "buffed/active" colour on twenty-odd other rows. That is a real
 * ambiguity and an accepted one: on THIS sheet blue on a damage row is the row being coloured by
 * element, which is the whole point of the feature; over a monster's head there is no buffed row
 * for it to be confused with.
 */
UiFlags DamageTypeColor(DamageType type)
{
	switch (type) {
	case DamageType::Fire:
		return UiFlags::ColorRed;
	case DamageType::Cold:
		return UiFlags::ColorBlue;
	case DamageType::Acid:
		return UiFlags::ColorOracoolGreen;
	case DamageType::Lightning:
		// ColorYellow, NOT ColorUiYellow (user, 2026-08-31: "Charged Bolt renders indeed dark
		// blue, instead of yellow").
		//
		// The two are not interchangeable and the difference is the PALETTE. oracool_yellow.trn was
		// generated against ui_art\diablo.pal for the front end's focus glow - see
		// tools/MakeYellowFontTrn.ps1, which says so - and it maps the font's ink onto indices
		// 128-135 because those are a yellow ramp IN THAT PALETTE. The character sheet draws in the
		// level palette, where the same indices are something else entirely, and what came out was
		// dark blue.
		//
		// ColorYellow is the in-game yellow: it is what a rare item's name is written in, which is
		// the one the user asked to be "bright YELLOW" in the first place.
		return UiFlags::ColorYellow;
	case DamageType::Magic:
		return UiFlags::ColorMagicDamage;
	case DamageType::Physical:
		// White, D2's own physical, and the only element that WANTS the default.
		break;
	}
	return UiFlags::ColorWhite;
}

std::string GetReadiedSlotDamageText(bool leftButton)
{
	return GetReadiedSlotDamage(leftButton).text;
}

std::string GetReadiedSlotNameText(bool leftButton)
{
	return GetReadiedSlotName(leftButton);
}

UiFlags GetReadiedSlotColor(bool leftButton)
{
	return ReadiedSlotColor(leftButton);
}

void LoadCharPanel()
{
	// Oracool V1: there is no longer anything to load. charbg.clx and the boxleftend/boxmiddle/
	// boxrightend field bezels are gone with the panel image they framed, and the labels they used
	// to be baked alongside are drawn per-frame now, because their column position depends on
	// measured text. What used to be an image composition is just a layout measurement.
	LayoutReady = false;
	EnsureLayout();
}

void FreeCharPanel()
{
	// Re-measure on the next open: a language change between sessions moves the label column.
	LayoutReady = false;
}

Rectangle GetCharacterPanelRect()
{
	// BOTTOM-left, with the stash, quest log and waypoint list it shares the slot with
	// (user, 2026-08-27: "inventory, hero stats, etc").
	return { { 0, oracool::BottomDockedTop(CharPanelSize.height) }, CharPanelSize };
}

Point GetCharacterContentOrigin()
{
	const Rectangle panel = GetCharacterPanelRect();
	// The grouped sheet starts higher - it has no separator rule under the title to clear - and
	// everything that positions against the sheet asks here, so drawing and hit-testing move together.
	return { panel.position.x, panel.position.y + (GroupedSheet() ? GroupedContentTop : CharContentTop) };
}

Point GetResetStatsButtonPosition()
{
	EnsureLayout();
	return ResetButtonPosition;
}

Size GetResetStatsButtonSize()
{
	// The list's RESET is the 44x24 word in the Base column; the grouped sheet's spans its column.
	return GroupedSheet() ? Size { GroupedLeftWidth, GroupedResetHeight } : ResetStatsButtonSize;
}

Rectangle GetCharacterContentRect()
{
	const Point origin = GetCharacterContentOrigin();
	return { origin, GroupedSheet() ? GroupedContentSize : CharContentSize };
}

bool PressCharacterSheetAdvancedButton(Point mousePosition)
{
	if (!chrflag || !GroupedSheet())
		return false;
	EnsureLayout();
	if (!ContentToScreen(GroupedAdvancedButton).contains(mousePosition))
		return false;
	// A press only sinks the face and clicks; the window opens on the release (the game-wide rule).
	AdvancedButtonPressed = true;
	oracool::PlayUiMoveSound();
	return true;
}

void ReleaseCharacterSheetAdvancedButton()
{
	const bool wasPressed = AdvancedButtonPressed;
	AdvancedButtonPressed = false; // always taken, so a press that outlived the sheet cannot fire late
	if (!wasPressed || !chrflag || !GroupedSheet())
		return;
	// Released inside the button it was pressed on - the unsunk rect - or it is "let me think a bit more".
	if (ContentToScreen(GroupedAdvancedButton).contains(MousePosition) && GetCharacterContentRect().contains(MousePosition))
		oracool::ToggleAdvancedStats();
}

int GetSheetAttackFramesSkipped()
{
	return AttackFramesSkipped();
}

int GetSheetHitRecoveryFramesSkipped()
{
	return HitRecoveryFramesSkipped();
}

int GetSheetBlockChancePercent()
{
	return BlockChancePercent();
}

int GetSheetLifeStealPercent()
{
	return LifeStealPercent();
}

int GetSheetManaStealPercent()
{
	return ManaStealPercent();
}

void ScrollCharacterSheet(int notches)
{
	EnsureLayout();
	const int offset = std::clamp(ScrollOffset + notches * CharScrollStep, 0, MaxScrollOffset);
	if (offset == ScrollOffset)
		return;
	ScrollOffset = offset;
	// The + and RESET buttons moved with the rows, so their hit rects have to move too - they are
	// what control.cpp, plrctrls.cpp and the touch renderer test against.
	PlaceWidgets();
}

void ResetCharacterSheetScroll()
{
	EnsureLayout();
	if (ScrollOffset == 0)
		return;
	ScrollOffset = 0;
	PlaceWidgets();
}

void DrawChr(const Surface &out)
{
	EnsureLayout();

	// Oracool V1: shared theme and geometry, matching the waypoint list and quest log.
	// Oracool (2026-08-16): the shared painted side-panel background - see quests.cpp for the note.
	// The rule under the title went with it; the art brings its own header framing.
	const Rectangle panel = GetCharacterPanelRect();
	if (oracool::HasSidePanelArt()) {
		oracool::DrawSidePanelArt(out, panel.position); // dims its opening too - see DrawSidePanelDim
	} else {
		oracool::DrawThemedFill(out, panel);
		oracool::DrawOrnateBorder(out, panel);
	}

	const Rectangle labelArea { { panel.position.x + CharPanelMargin, panel.position.y + oracool::PanelTitleTop },
		{ panel.size.width - 2 * CharPanelMargin, oracool::PanelTitleHeight } };
	oracool::DrawOutlinedString(out, _("CHARACTER"), labelArea,
	    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);

	// The grouped sheet (Grouped Hero Sheet option, 2026-09-26) - same window, title and X, its own
	// body. Off, everything below is the list exactly as it was.
	if (LayoutGrouped) {
		DrawGroupedSheet(out, panel);
		return;
	}

	DrawScrollbar(out, panel);

	// Rows and widgets draw through a subregion covering only the scrolling area, so anything
	// straddling its top or bottom edge is clipped there rather than spilling onto the title band
	// or the bottom bevel. Inside it, coordinates are content-relative - the same frame
	// ChrBtnsRect and ResetButtonPosition are expressed in.
	const Rectangle contentRect = GetCharacterContentRect();
	const Surface content = out.subregion(contentRect.position.x, contentRect.position.y,
	    contentRect.size.width, contentRect.size.height);

	for (size_t i = 0; i < CharRowCount; ++i)
		DrawRow(content, i);

	DrawStatButtons(content);
}

} // namespace devilution
