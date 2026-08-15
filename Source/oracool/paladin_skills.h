/**
 * @file oracool/paladin_skills.h
 *
 * Oracool: the Paladin's two ACTIVE skills - Charge and Zeal.
 *
 * Unlike oracool/auras.h and oracool/barb_skills.h, which are data-only lists waiting on a gameplay
 * pass, both of these DO something. Their mechanics were written months earlier and then switched
 * off (user decision 2026-08-11) pending a way to earn them; this file is that way. It supplies the
 * level gate and the mana price, and the two mechanic modules ask it rather than carrying their own
 * copies of the numbers:
 *
 *   Charge - oracool/furious_charge.cpp, the rush-and-strike on the Paladin's class-skill slot.
 *   Zeal   - oracool/warrior_splash.cpp, the melee hit that carries to adjacent enemies.
 *
 * "Paladin" is HeroClass::Warrior. Oracool renames the Warrior in display data only
 * (playerdat.cpp's className), leaving the enum, the sprite folder and every save field on the
 * original name - so reading the class by its displayed name compiles fine and silently never
 * matches. See ClassHasPaladinSkills.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "spelldat.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution {

struct Player;

namespace oracool {

/**
 * @brief The skills, in the order of the icon sheet (ui\paladin_skill_icons.png).
 *
 * Enum order IS icon order - GetPaladinSkillIconIndex is the identity - so the sheet and this list
 * cannot drift. Also the display order, unlike the auras: there is nothing here to gain from a
 * separate ordering, so there is deliberately no GetPaladinSkillAtDisplayIndex.
 *
 * Grew from two to seven on 2026-08-15 when the second art delivery arrived. Smite is drawn on that
 * sheet and is NOT here: the user asked for it to be held back ("ignore this skill for now. Don't
 * add it."), and tools/CutPaladinSkills.ps1 skips its grid cell for the same reason - so the strip
 * has seven cells, not eight, and this list still lines up with it.
 */
enum class PaladinSkill : uint8_t {
	Charge,
	Zeal,
	HammerOfFaith,
	BlessedShield,
	FistOfTheHeavens,
	ShieldBash,
	BlessedHammer,
	LAST = BlessedHammer,
};

constexpr size_t PaladinSkillCount = 7;

struct PaladinSkillData {
	/** Untranslated; run through _() at the point of display. */
	const char *name;
	/** One sentence, kept short enough to wrap inside the Abilities window's text column. */
	const char *description;
	/**
	 * @brief The spell slot this skill occupies.
	 *
	 * Every skill has one as of 2026-08-15 (user request: "Make sure all skills are selectible and
	 * their icons appear as they should on LMB/RMB"). It is what makes the skill assignable: the
	 * readied pair the HUD's wells, the cast path and the save format all speak is a SpellID, so a
	 * skill without one could be listed and described but never put on a button.
	 */
	SpellID spellId;
	/** Character level at which the skill becomes usable. */
	int minLevel;
	/**
	 * @brief Mana spent per use, in whole points.
	 *
	 * Per CHARGE for Charge, and per SPLASHING HIT for Zeal - which is why Zeal's is small. Both are
	 * charged only when the effect actually happens, so a Charge that never launches and a swing
	 * with no second enemy beside it are free.
	 */
	int manaCost;
};

const PaladinSkillData &GetPaladinSkillData(PaladinSkill skill);

/**
 * @brief The skill that owns @p spell, or nullopt if @p spell is not one of these skills.
 *
 * The reverse of PaladinSkillData::spellId, and the hinge everything outside this module turns on:
 * given a readied SpellID it answers "is this a Paladin skill, and which", which is how the HUD's
 * wells know to draw the skill's own art from ui\paladin_skill_icons.png rather than reaching into
 * the vanilla spell icon sheet, where these have no frame.
 */
std::optional<PaladinSkill> PaladinSkillForSpell(SpellID spell);

/**
 * @brief Whether @p skill has mechanics behind it yet, as opposed to being listed and described.
 *
 * Charge and Zeal do; the five added on 2026-08-15 do not - they arrived as art plus one line of
 * description each, the same way the auras and the Barbarian skills did, and the gameplay for them
 * is a separate pass. The level gate and mana price in the table are placeholders until then.
 *
 * Nothing in the Abilities window branches on this: a row is readiable when it carries a SpellID,
 * which is the narrower and more direct test (Zeal is implemented and still cannot be readied,
 * because it applies itself to every swing rather than being cast). This exists so the distinction
 * is stated somewhere rather than being inferred from a table of numbers that all look alike.
 */
bool IsPaladinSkillImplemented(PaladinSkill skill);

/** @brief Whether this class has these skills at all. Paladin (HeroClass::Warrior) only. */
bool ClassHasPaladinSkills(const Player &player);

/** @brief Whether @p player is the right class AND high enough level to use @p skill. */
bool IsPaladinSkillUnlocked(const Player &player, PaladinSkill skill);

/**
 * @brief Whether @p player can pay for @p skill right now - unlocked, and holding enough mana.
 *
 * Single-player only, like every other Oracool combat change. Deliberately a query with no side
 * effect; SpendPaladinSkillMana below is what actually deducts, so a caller can test before
 * committing to an action it might then have to undo.
 */
bool CanUsePaladinSkill(const Player &player, PaladinSkill skill);

/**
 * @brief Deducts @p skill's mana cost from @p player and redraws the mana orb.
 *
 * Call only after CanUsePaladinSkill has agreed. Returns false and spends nothing otherwise, so a
 * caller that forgets the check still cannot drive mana negative.
 */
bool SpendPaladinSkillMana(Player &player, PaladinSkill skill);

/** @brief Index of @p skill's icon in ui\paladin_skill_icons.png - the identity, see the enum. */
inline int GetPaladinSkillIconIndex(PaladinSkill skill)
{
	return static_cast<int>(skill);
}

} // namespace oracool
} // namespace devilution
