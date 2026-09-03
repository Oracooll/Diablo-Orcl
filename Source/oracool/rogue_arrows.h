/**
 * @file oracool/rogue_arrows.h
 *
 * Oracool, Round 3 of the inert-skill plan (2026-09-03): the Rogue's bow page - ten rows that are
 * all "an arrow, but".
 *
 * THE MECHANISM. A bow skill readied on a button is not cast, it is SHOT: the click becomes the
 * ordinary ranged attack (bow animation, arrow sound, weapon wear) with a latch that says which skill
 * the shot was thrown with, and DoRangeAttack asks the latch instead of loosing a plain arrow. That is
 * the shape oracool/paladin_melee.h gave the Paladin's swings, and it is the same shape for the same
 * reason: by the time the animation reaches its action frame, which button acted is gone, so it is
 * recorded at the moment the button acts.
 *
 * Ten rows, one latch, one missile family. What differs between them is data on the missile - its
 * element, and what it does when it stops - plus two that fire several arrows instead of one.
 *
 *   Magic Arrow        one arrow, magic damage, and no ammunition (this engine never had any)
 *   Fire Arrow         one arrow, fire damage
 *   Cold Arrow         one arrow, cold damage, chills            (frost_arrow.png)
 *   Multiple Shot      a fan of arrows, physical
 *   Exploding Arrow    fire, and bursts across the tiles around where it stops
 *   Ice Arrow          cold, and FREEZES what it hits
 *   Guided Arrow       physical, and cannot miss
 *   Strafe             one arrow at each enemy in view, nearest first
 *   Immolation Arrow   fire, and leaves a fire wall burning where it stops
 *   Freezing Arrow     cold, and freezes everything around where it stops  (freezing_burst.png)
 */
#pragma once

#include <optional>

#include "engine/point.hpp"
#include "spelldat.h"

namespace devilution {
struct Player;
struct Missile;
} // namespace devilution

namespace devilution::oracool {

enum class RogueArrow : uint8_t {
	MagicArrow,
	FireArrow,
	ColdArrow,
	MultipleShot,
	ExplodingArrow,
	IceArrow,
	GuidedArrow,
	Strafe,
	ImmolationArrow,
	FreezingArrow,
};

/** @brief The bow skill @p spell is, if it is one. */
std::optional<RogueArrow> RogueArrowForSpell(SpellID spell);

/** @brief The spell @p arrow is readied and priced as - the inverse of RogueArrowForSpell. */
SpellID RogueArrowSpell(RogueArrow arrow);

/** @brief Records which bow skill the shot now being launched was thrown with. See paladin_melee.h for the latch's rules. */
void ArmArrowSkill(std::optional<RogueArrow> arrow);

/** @brief The bow skill the shot being resolved was thrown with, if any. */
std::optional<RogueArrow> ArmedArrowSkill();

/**
 * @brief Looses the armed skill's arrow(s) from @p player at @p target - the whole of what a skill
 * does differently from a plain shot. Spends the mana; a shooter who cannot pay looses a plain
 * arrow and the latch is dropped, so holding the button on an empty pool degrades rather than jams.
 */
void FireArrowSkill(Player &player, RogueArrow arrow, Point target);

/**
 * @brief The damage a skill's arrow deals at @p spellLevel, for the sheet. Physical arrows report
 * the bow's own range; elemental ones add their rank bonus to it. -1/-1 for a spell that is not a
 * bow skill.
 */
void RogueArrowDamage(const Player &player, SpellID spell, int spellLevel, int &minDamage, int &maxDamage);

/** @brief One sentence for the Abilities window, untranslated. "" for a spell that is not a bow skill. */
const char *RogueArrowDescription(SpellID spell);

} // namespace devilution::oracool
