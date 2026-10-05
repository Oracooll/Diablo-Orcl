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
#include "oracool/missile_tint.h" // Tint - RogueArrowLook
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

/**
 * @brief How a skill's arrow is drawn (the Rogue Skill Cards page, 2026-10-04): every one is the vanilla arrow now - "replace
 * with regular arrow tinted ..." - in its skill's colour, burning or frosted ones glinting (Tint::Glint, the colour cycling
 * the page asked for), with a light of @p light tiles carried along. Multiple Shot and Strafe stay the plain arrow.
 */
struct RogueArrowLook {
	Tint tint = Tint::None;
	uint32_t rgb = 0;
	int light = 0;
};
RogueArrowLook RogueArrowLookOf(RogueArrow arrow);

/** @brief The spell @p arrow is readied and priced as - the inverse of RogueArrowForSpell. */
SpellID RogueArrowSpell(RogueArrow arrow);

/** @brief Records which bow skill the shot now being launched was thrown with. See paladin_melee.h for the latch's rules. */
void ArmArrowSkill(std::optional<RogueArrow> arrow);

/** @brief The bow skill the shot being resolved was thrown with, if any. */
std::optional<RogueArrow> ArmedArrowSkill();

/**
 * @brief Records that the shot now being launched is one of the eight RfA-12 bow skills (dev note, 2026-10-03: "some bow
 * and crossbow skills seem to be castings ... double check if they need to be regular bow shots"). They were cast with
 * the magic animation; now the click is the ordinary bow shot and DoRangeAttack casts the skill at its release frame.
 * Arming one clears the arrow latch, and ArmArrowSkill (with anything) clears this one, so every place that drops the
 * bow latch drops both.
 */
void ArmBowSkillSpell(SpellID spell);

/** @brief The RfA-12 bow skill the shot being resolved was thrown with, if any. */
std::optional<SpellID> ArmedBowSkillSpell();

/**
 * @brief The bow latch armed now came from a quick-cast key: it is that one shot's (round 95 audit). QuickCast puts a held
 * plain attack back after it, and the held button's repeats shot - and paid - the key's skill until mana ran out.
 */
void MarkBowLatchOneShot(std::optional<RogueArrow> heldArrow, std::optional<SpellID> heldBowSpell);
/** @brief Whether the armed bow latch is a key's one shot - a stagger does not keep it for the held button (round 96). */
bool IsBowLatchOneShot();
/** @brief Under a key's one-shot latch, what the held button had armed - a stagger keeps that, not the key's (round 97). */
std::optional<RogueArrow> HeldArrowUnderKeyShot();
std::optional<SpellID> HeldBowSpellUnderKeyShot();
/** @brief After a release frame: drops a one-shot bow latch (MarkBowLatchOneShot) and puts back what the held button had armed. */
void DropOneShotBowLatch();

// ---- The Javelin & Spear page (the Rogue Skill Cards page, 2026-10-04) ----------------------------------------------
// "all javelin strikes to use staff animations. melee attack." and, for the four throws, "this is a throw skill. we play staff
// attack but hero releases [tinted] arrow towards target. Range - N tiles. doesnt trigger if distance is longer."

/** @brief Whether @p spell is a javelin-page melee strike, swung in the staff's attack sheet (player.cpp's staff look). */
bool IsJavelinStrike(SpellID spell);
/** @brief Whether @p spell is thrown: a staff swing in place that lets the skill go at the hit frame. */
bool IsJavelinThrow(SpellID spell);
/** @brief How far @p spell is thrown, in tiles: 8 for every throw (user, 2026-10-05). */
int JavelinThrowRange(SpellID spell);
/** @brief Records the throw the swing now launched lets go at @p target (nullopt clears). ArmArrowSkill clears it too. */
void ArmJavelinThrow(std::optional<SpellID> spell, Point target = {});
/** @brief The throw the swing being resolved carries, if any, and where it goes. */
std::optional<SpellID> ArmedJavelinThrow();
Point ArmedJavelinThrowTarget();

/** @brief Strafe's share of spin shots aimed at an enemy in view, in percent, at @p spellLevel: 60, +5 a rank, 95 at most. */
int StrafeAimPercent(int spellLevel);

/**
 * @brief One shot of the Strafe spin (oracool/whirlwind.h): an arrow at an enemy in view (StrafeAimPercent of the time,
 * when there is one) or off in a random direction, with the bow's sound. Pays nothing - the spin pays by the second.
 */
void LooseStrafeShot(Player &player);

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

/** @brief What @p arrow does at @p spellLevel beyond its damage: arrows loosed, chill, freeze. For the tooltip. */
std::string RogueArrowFactsAt(RogueArrow arrow, int spellLevel);

} // namespace devilution::oracool
