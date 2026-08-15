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

#include "utils/stdcompat/string_view.hpp"

namespace devilution {

struct Player;

namespace oracool {

/**
 * @brief The two skills, in the order of the icon sheet (ui\paladin_skill_icons.png).
 *
 * Enum order IS icon order - GetPaladinSkillIconIndex is the identity - so the sheet and this list
 * cannot drift. Also the display order, unlike the auras: with two entries there is nothing to gain
 * from a separate ordering, so there is deliberately no GetPaladinSkillAtDisplayIndex.
 */
enum class PaladinSkill : uint8_t {
	Charge,
	Zeal,
	LAST = Zeal,
};

constexpr size_t PaladinSkillCount = 2;

struct PaladinSkillData {
	/** Untranslated; run through _() at the point of display. */
	const char *name;
	/** One sentence, kept short enough to wrap inside the Abilities window's text column. */
	const char *description;
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
