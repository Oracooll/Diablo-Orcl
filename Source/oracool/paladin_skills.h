/**
 * @file oracool/paladin_skills.h
 *
 * Oracool: the Paladin's two ACTIVE skills - Charge and Zeal.
 *
 * Unlike oracool/barb_skills.h, which is a data-only list waiting on a gameplay
 * pass, both of these DO something. Their mechanics were written months earlier and then switched
 * off (user decision 2026-08-11) pending a way to earn them; this file is that way. It supplies the
 * level gate and the mana price, and the two mechanic modules ask it rather than carrying their own
 * copies of the numbers:
 *
 *   Charge - oracool/furious_charge.cpp, the rush-and-strike on the Paladin's class-skill slot.
 *   Zeal, Hammer of Faith, Shield Bash - oracool/paladin_melee.cpp, the shared melee-swing hook.
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

/**
 * @brief The furthest any skill may reach, in tiles.
 *
 * Oracool: user rule (2026-08-15) - "no more than a 640x480px worth of screen estate", confirmed as
 * a RADIUS rather than a total area. The request is in pixels and the code needs tiles, so:
 *
 * The isometric grid is TILE_WIDTH 64 and TILE_HEIGHT 32, which means one tile step moves the view
 * 32px horizontally and 16px vertically. A 640x480 box centred on the player reaches 320px and 240px
 * from it, so 320/32 = 10 tiles across and 240/16 = 15 tiles down. HORIZONTAL BINDS, so 10.
 *
 * It is also a sane number on its own terms: a little under half the visible width at 960x720, so a
 * ranged skill reaches meaningfully across the screen without out-ranging what the player can see
 * coming at them.
 */
constexpr int MaxSkillRangeTiles = 10;

/** @brief The reach of a skill that only strikes what the character is standing next to. */
constexpr int MeleeSkillRangeTiles = 1;

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
	/**
	 * @brief How far the skill reaches, in tiles, measured as walking (Chebyshev) distance.
	 *
	 * Oracool: user rule (2026-08-15) - "melee skills only initiate when clicked on monsters within
	 * range, else - move command", the same for ranged, and "we need to define range of skills. i
	 * suggest no more than a 640x480px worth of screen estate."
	 *
	 * 1 is melee: the tile you are facing. See MaxSkillRangeTiles for where the upper bound comes
	 * from and why it is 10.
	 */
	int rangeTiles;
	/**
	 * @brief Whether the skill is unusable without a shield equipped.
	 *
	 * Oracool: user rule (2026-08-15) - "Carrying shield is mandatory, else - skill is inactivated."
	 * Since 2026-09-27 (dev note: "must not require shield to level up, only to operate") it is part
	 * of CanUsePaladinSkill, not IsPaladinSkillUnlocked: the row takes points and stays on its button
	 * without a shield, and every use refuses until one is held (the well shows it as blocked).
	 */
	bool requiresShield;
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
 * @brief Whether @p player could reach the monster under the cursor with @p skill right now.
 *
 * Walking (Chebyshev) distance, which is the grid's own notion of "how many steps away" and the one
 * every other reach check in the engine uses. False when nothing is targeted at all, so a caller can
 * treat "no target" and "target too far" as the single case they are: not a cast.
 */
bool IsPaladinSkillTargetInRange(const Player &player, PaladinSkill skill);

/** @brief Whether @p player is holding a shield right now - what requiresShield is tested against. */
bool HasShieldEquipped(const Player &player);

/**
 * @brief Whether @p spell needs a shield that @p player is not holding: Smite and Blessed Shield (requiresShield) and
 * Aegis Slam (Rfa12MeleeUsable). Their plates go red on the LMB/RMB wells and in the skill menus while it is so (user,
 * 2026-09-29: "Shield requiring skills to have red backing in lmb/rmb slots and menus when a shield is not equipped").
 * Since v1.12.243 also a bow skill (the Rogue's arrows and the eight RfA-12 bow skills) with no bow in hand.
 */
bool LacksShieldFor(const Player &player, SpellID spell);

/**
 * @brief Whether @p spell is swung with a melee weapon - a class swing other than plain Leap, a Paladin melee skill or
 * Charge, an RfA-12 swing - and @p player holds a bow. Refused at the click, as D2 refuses it (round 19 audit, v1.12.244).
 */
bool LacksMeleeWeaponFor(const Player &player, SpellID spell);

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
/**
 * @brief Whether a missile can still be allocated. Ask BEFORE spending mana on a cast.
 *
 * AddMissile returns nullptr on a full pool, and the three ranged Paladin skills used to
 * discard that result after already taking the mana.
 */
bool MissilePoolHasRoom();

bool SpendPaladinSkillMana(Player &player, PaladinSkill skill);

/** @brief Index of @p skill's icon in ui\paladin_skill_icons.png - the identity, see the enum. */
inline int GetPaladinSkillIconIndex(PaladinSkill skill)
{
	return static_cast<int>(skill);
}

/** @brief What @p skill does at @p rank, one fact per line: range, shield, strikes, splash, dash. For the tooltip. */
std::string PaladinSkillFactsAt(PaladinSkill skill, int rank);

} // namespace oracool
} // namespace devilution
