/**
 * @file oracool/paladin_melee.h
 *
 * Oracool: the three Paladin skills that ride a melee swing - Zeal, Hammer of Faith, Shield Bash.
 *
 * They share one hook, in DoAttack, because they all answer the same question: "this swing just
 * landed - does the skill on the button that threw it do anything extra?" Charge is not here; it
 * changes how you REACH the target rather than what the blow does, so it lives in
 * oracool/furious_charge.cpp and is intercepted before the swing.
 *
 * Replaces oracool/warrior_splash.{h,cpp}, which was this hook when Zeal was the only skill using it.
 */
#pragma once

#include <optional>

#include "oracool/paladin_skills.h"

namespace devilution {

struct Player;
struct Monster;

namespace oracool {

/**
 * @brief Records which skill the swing now being launched was thrown with.
 *
 * Oracool: user decision (2026-08-15) - Zeal is "active only", meaning it splashes only when Zeal is
 * the skill readied on the button you attacked with. DoAttack cannot work that out for itself: by
 * the time the animation reaches its hit frame, all that survives is _pRSpell / _pLRSpell, and which
 * of the two threw this swing is gone. So the answer is latched at the moment the button acts.
 *
 * Armed from CheckPlrSpell, the single funnel through which a mouse button acts, which already
 * receives the pair for whichever button was pressed. Cleared by LeftMouseCmd, the path a plain
 * swing takes. The controller and hold-to-attack repeat paths deliberately do NOT clear it - you are
 * still holding the same button down.
 *
 * File-scope state rather than a Player field, deliberately: single-player only, one swing resolving
 * at a time, and it must not reach the save format or the net packet. Same shape as
 * IsFuriousChargeOnCooldown's timer.
 */
void ArmMeleeSkill(std::optional<PaladinSkill> skill);

/** @brief The skill the swing being resolved was thrown with, if any. */
std::optional<PaladinSkill> ArmedMeleeSkill();

/**
 * @brief Whether the swing being started is a Shield Bash - i.e. should shove with the shield.
 *
 * Oracool: user request (2026-08-15) - "always play Shield Hit animation, regardless of equipped
 * weapon". Asks the latch, so it is true only for the swing that skill actually threw.
 */
bool IsShieldBashSwing(const Player &player);

/**
 * @brief Whether the swing being started strikes with the shield on the unarmed-with-shield attack sheet
 * (player_graphic::ShieldAttack): Shield Bash or Aegis Slam, a shield held, the sheet loaded (dev note, 2026-09-27).
 * When this holds IsShieldBashSwing is false - the block sheet is only the stand-in for a missing attack sheet.
 */
bool SwingsShieldAttackSheet(const Player &player);

/**
 * @brief The animation frame a swing lands its blow on.
 *
 * Normally the weapon's own _pAFNum. For a Shield Bash it is clamped inside the BLOCK animation's
 * length, because that swing borrows the block graphic and a hit frame past its last frame would
 * never arrive - the blow would silently never land. StartAttack and DoAttack both ask this, which
 * is the point: the frame the animation is distributed around and the frame the hit fires on have to
 * be the same number.
 */
int MeleeHitFrame(const Player &player);

/**
 * @brief Runs the armed skill's extra effect for a swing that just connected.
 *
 * @p hitDamage is the damage the swing itself dealt. Every skill here scales from it rather than
 * rolling its own, which is what makes them WEAPON-damage based (user decision, 2026-08-15) without
 * any of them having to know what a weapon is.
 *
 * Charges mana only when the effect actually happens - a Zeal swing with nobody else in reach, or a
 * Shield Bash on an already-dying monster, costs nothing. That rule predates these skills and is
 * stated at every site that spends for one.
 */
void ApplyMeleeSkillOnHit(Player &player, Monster &primaryTarget, int hitDamage);

/**
 * @brief The swing's own sound, for a swing that carries a skill with one - played INSTEAD of the
 * plain swing whoosh (DoAttack), never on top of it. False, so the whoosh plays, for every other
 * swing. Only Hammer of Faith has a cue (RfA-02 batch 7, 2026-09-11).
 */
bool PlayArmedSwingCue(const Player &player);

/**
 * @brief How many times a Zeal burst strikes at @p player's current level.
 *
 * Oracool: user spec (2026-08-15), confirmed as CHARACTER level with Zeal's own gate left at 6:
 * 6 -> 2 strikes, 8 -> 3, 10 -> 4, 12 -> 5, and capped there. The mana price is the same number,
 * which is why the table prices Zeal at 1 mana and this is charged per strike landed.
 *
 * Public so the Abilities window can show the count the player will actually get, rather than a
 * sentence that goes stale two levels later.
 */
int ZealStrikeCount(const Player &player);

/**
 * @brief Zeal's to-hit bonus, in percentage points - one per invested point.
 *
 * Paid on every point including those past the four-strike cap, which is what keeps a deep Zeal
 * worth buying now that the strikes stop at four (user, 2026-08-30). Zero for anyone who is not a
 * Paladin, or has not reached Zeal's unlock level.
 */
int ZealToHitBonus(const Player &player);

/**
 * @brief Zeal's accuracy at @p player's current rank, WITHOUT asking whether a swing is in flight.
 *
 * The magnitude only. ZealToHitBonus is this plus the combat condition, so the number a hit roll
 * uses and the number a panel prints come from one place and cannot drift.
 */
int ZealToHitBonusAtRank(const Player &player);

/**
 * @brief Whether Zeal is sitting on a mouse button - what the character sheet should ask.
 *
 * Not the same question as ArmedMeleeSkill(), which is a latch describing the swing currently being
 * resolved and is empty whenever the player is standing in a menu. Asking the latch from the sheet
 * is why the sheet showed no Zeal bonus at all.
 */
bool IsZealReadied(const Player &player);

/**
 * @brief Advances any Zeal burst in flight. Called once per tick, per player.
 *
 * The strikes are spread over time rather than landed all at once, because the skill is "up to 5
 * times within 150% of frames of regular attack" - a burst, not a bigger single blow. The first
 * strike lands with the swing; the rest follow on this clock.
 */
/**
 * @brief Continues a running Zeal chain when a swing's animation ends: restarts a REAL attack
 * animation toward the next target with most of the windup skipped. Returns true if it did, in
 * which case DoAttack must not end the attack. Replaces the invisible-tick burst (user, 2026-08-15:
 * "i dont see the hero making rapid atacks").
 */
bool TryContinueZealChain(Player &player);

/**
 * @brief Disarms a running Zeal chain. Call wherever an attack stops being the thing happening.
 *
 * The chain is a latch - "owe this player N more swings" - and nothing used to take it back. An
 * interrupted burst therefore kept it armed across a hit reaction, a broken weapon, a death, a
 * level change; and the next time ANY attack animation reached its hit frame, the leftover swings
 * were injected into that unrelated action (external audit, 2026-08-25).
 *
 * Cheap and idempotent, so the safe thing to do at a new interrupt point is call it.
 */
void ResetZealChain();

/**
 * @brief Frames a Zeal-armed swing skips so every swing - the first included - fits the per-swing
 * budget: 150% of the attack split across the burst's strikes (user spec, 2026-08-15: 2 swings of
 * ~15 frames, 3 of 10, 4 of 7, 5 of 6 on a 20-frame attack). 0 when the swing is not a Zeal one.
 */
int ZealSwingSkipFrames(const Player &player);

/** @brief Smite's blow at @p rank, in percent more damage: 15 a level (2026-09-12). */
constexpr int SmiteDamagePercentAt(int rank)
{
	return 15 * (rank < 1 ? 1 : rank);
}

/** @brief Hammer of Faith's splash at @p rank, in percent of the blow: half, +2 points a level (2026-09-12). */
constexpr int HammerOfFaithSplashPercentAt(int rank)
{
	return 50 + 2 * ((rank < 1 ? 1 : rank) - 1);
}

/**
 * @brief The armed Paladin skill's share on the swing's own damage, in percent - Smite's, and Charge's on
 * a dash's arriving blow. Zero otherwise. Read beside ClassMeleeSkillDamagePercent in the melee roll.
 */
int PaladinMeleeDamagePercent(const Player &player);

/** @brief Zeal's, Smite's and Hammer of Faith's facts at @p rank, one per line. For the tooltip. */
std::string PaladinMeleeFactsAt(PaladinSkill skill, int rank);

} // namespace oracool
} // namespace devilution
