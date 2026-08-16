/**
 * @file oracool/paladin_tree.h
 *
 * Oracool: Diablo II's Paladin skill tree - three pages, twenty-nine skills - as the Paladin's
 * authoritative ability set.
 *
 * This SUPERSEDES the invented 24-aura list that shipped as data in v1.1.80 and gained effects in
 * v1.7.12. That list was built from an early art delivery and never matched Diablo II; the user
 * supplied the real tree ("Paladin Skill Tree.png", 2026-08-16) and asked for D2's own contents, so
 * the invented auras are gone and these are the Paladin's skills.
 *
 * ## The three pages
 *
 * Combat Skills (9), Offensive Auras (10), Defensive Auras (10). Diablo II's own Combat page has a
 * tenth, Holy Shield; the supplied sheet has no icon for it, so it is not listed rather than being
 * listed with borrowed art.
 *
 * ## Gating: level tiers, not a prerequisite graph
 *
 * Every skill sits in one of Diablo II's six tiers - character level 1, 6, 12, 18, 24, 30 - and the
 * tier is the whole gate. Diablo II ALSO has a per-skill prerequisite graph, including cross-tree
 * links; that graph is deliberately NOT reproduced, because reproducing it from memory would mean
 * inventing edges and presenting them as D2's. The tier requirements are certain, they are D2's
 * primary gate, and they already make the tree read as a progression. If the exact graph is wanted
 * later it is one field per row.
 *
 * ## Investment: one accessor over two stores
 *
 * Points come from Phase 2.1's pool (oracool/skill_points.h). Where a tree skill has a SpellID, its
 * investment lives in Player::_pSkillInvestment, keyed by that SpellID - which means it flows into
 * Player::GetSpellLevel and therefore into every ladder that already scales with spell level, for
 * free. The twenty auras have no SpellID, so theirs lives in Player::_pPaladinAuraInvestment,
 * persisted by the HeroChunkPaladinAuras chunk. PaladinTreeInvestment() hides which store a skill
 * uses; nothing outside this module should reach for either array directly.
 *
 * ## What the engine can and cannot do
 *
 * Each row's description states D2's effect. Effects reach the character through the "aura" bonus
 * provider (stat_sheet.cpp) and the per-tick hook, and several D2 effects have no channel in this
 * engine - there is no cold damage accumulator, no cold resistance, and no poison/curse duration
 * model. Those skills are listed, described, and INERT rather than approximated, and IsImplemented
 * says which is which so the UI can be honest about it. Resist Cold is the one deliberate
 * remapping: it feeds magic resistance, D1's third resist, and says so in its own description.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "spelldat.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution {

struct Player;

namespace oracool {

struct ItemBonusTotals;

/**
 * @brief The 29 skills, in the order of the icon strip (ui\paladin_tree_icons.png).
 *
 * Enum order IS icon order - GetPaladinTreeIconIndex is the identity - so the strip and this list
 * cannot drift. tools/CutPaladinTree.ps1 emits the strip in exactly this sequence.
 */
enum class PaladinTreeSkill : uint8_t {
	// --- Combat Skills ---
	Sacrifice,
	FIRST = Sacrifice,
	Smite,
	HolyBolt,
	Zeal,
	Charge,
	Vengeance,
	BlessedHammer,
	Conversion,
	FistOfTheHeavens,
	// --- Offensive Auras ---
	Might,
	FIRST_AURA = Might,
	HolyFire,
	Thorns,
	BlessedAim,
	Concentration,
	HolyFreeze,
	HolyShock,
	Sanctuary,
	Fanaticism,
	Conviction,
	// --- Defensive Auras ---
	Prayer,
	ResistFire,
	Defiance,
	ResistCold,
	Cleansing,
	ResistLightning,
	Vigor,
	Meditation,
	Redemption,
	Salvation,
	LAST = Salvation,

	None = 0xFF,
};

constexpr size_t PaladinTreeSkillCount = 29;
/** @brief The twenty auras, which are the tail of the enum - their own investment array's size. */
constexpr size_t PaladinAuraCount = 20;
/** @brief Points a single tree skill accepts, matching the spell-investment cap. */
constexpr int MaxTreeInvestment = 20;

enum class PaladinTreePage : uint8_t {
	Combat,
	OffensiveAuras,
	DefensiveAuras,
	LAST = DefensiveAuras,
};
constexpr size_t PaladinTreePageCount = 3;

/** @brief What kind of thing a row is, which decides what a click does. */
enum class PaladinTreeKind : uint8_t {
	/** Cast or swung - clicking readies it on a mouse button (if it carries a SpellID). */
	Active,
	/** Burns until switched off - clicking activates it. Exactly one aura at a time. */
	Aura,
};

struct PaladinTreeSkillData {
	/** Untranslated; run through _() at the point of display. */
	const char *name;
	/** What the skill does in Diablo II, plus this engine's adaptation where they differ. */
	const char *description;
	PaladinTreePage page;
	/** 0-5. The character level required is TierMinLevel(tier). */
	int tier;
	/** 0-2, the grid column on its page. */
	int column;
	PaladinTreeKind kind;
	/**
	 * @brief Do not read this directly - call PaladinTreeSpellId(). Five of the tree's actives take
	 * their slot from oracool/paladin_skills.h at call time rather than storing it here, so this
	 * field is Invalid for them even though they are castable.
	 */
	SpellID spellId;
	/** Whether this row does anything yet - false means listed, described, and inert. */
	bool implemented;
};

const PaladinTreeSkillData &GetPaladinTreeSkillData(PaladinTreeSkill skill);

/**
 * @brief The spell slot @p skill readies onto a mouse button, or SpellID::Invalid if it has none
 * (every aura, and the actives whose mechanics are not built yet). The one authority - see the
 * note on PaladinTreeSkillData::spellId.
 */
SpellID PaladinTreeSpellId(PaladinTreeSkill skill);

/** @brief Character level required by @p tier: Diablo II's 1, 6, 12, 18, 24, 30. */
int PaladinTreeTierMinLevel(int tier);

/** @brief Display name of @p page, for the window's title band. */
string_view GetPaladinTreePageName(PaladinTreePage page);

/** @brief Whether this class has this tree at all. Paladin (HeroClass::Warrior) only. */
bool ClassHasPaladinTree(const Player &player);

/** @brief Whether @p player's level meets @p skill's tier (and the class is right). */
bool IsPaladinTreeSkillUnlocked(const Player &player, PaladinTreeSkill skill);

/** @brief Points sunk into @p skill, from whichever store it uses. See the file comment. */
int PaladinTreeInvestment(const Player &player, PaladinTreeSkill skill);

/** @brief Whether an invest click would take: unlocked, a point unspent, cap not reached. */
bool CanInvestPaladinTreePoint(const Player &player, PaladinTreeSkill skill);

/** @brief Spends one of Phase 2.1's unspent points on @p skill. False changes nothing. */
bool InvestPaladinTreePoint(Player &player, PaladinTreeSkill skill);

/** @brief The burning aura, or None. Decoded from Player::_pOracoolActiveAura. */
PaladinTreeSkill GetActivePaladinAura(const Player &player);

/**
 * @brief Click rule for an aura row: activates @p skill, or switches it off if already burning.
 * Refuses a non-aura, a locked tier, the wrong class, or an aura with nothing invested in it -
 * an aura you have not paid for has no strength to give. The caller owns the recalculation.
 */
bool TogglePaladinAura(Player &player, PaladinTreeSkill skill);

/**
 * @brief Contributes the burning aura's standing effects onto @p totals, scaled by the points
 * invested in it. Aura effects that this engine has no channel for contribute nothing.
 */
void ApplyPaladinAuraToTotals(const Player &player, ItemBonusTotals &totals);

/**
 * @brief Whether the burning aura is Vigor, which is how Diablo II's movement-speed aura reaches
 * an engine with no walk-speed modifier: it turns on the same double-speed frame skip the run
 * toggle uses (oracool/run_toggle.h). Consulted by StartWalkAnimation.
 */
bool IsPaladinVigorActive(const Player &player);

/**
 * @brief Per-tick work for the burning aura: Prayer's health regeneration and Meditation's mana
 * regeneration. Called once per game logic tick for the local player, beside ProcessGradualHealing.
 */
void ProcessPaladinAuraTick(Player &player);

/** @brief Index of @p skill's icon in ui\paladin_tree_icons.png - the identity, see the enum. */
inline int GetPaladinTreeIconIndex(PaladinTreeSkill skill)
{
	return static_cast<int>(skill);
}

/**
 * @brief Fills @p out with the skills on @p page in grid order (tier, then column). Returns how
 * many were written; @p out must hold at least PaladinTreeSkillCount.
 */
size_t BuildPaladinTreePage(PaladinTreePage page, PaladinTreeSkill *out);

/** @brief The line the hover panel puts under the description: what the points bought. */
std::string PaladinTreeEffectLine(const Player &player, PaladinTreeSkill skill);

} // namespace oracool
} // namespace devilution
