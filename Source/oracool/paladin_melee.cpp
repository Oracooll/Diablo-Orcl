#include "oracool/paladin_melee.h"

#include "items.h"
#include "monster.h"
#include "oracool/class_tree.h"
#include "oracool/paladin_skills.h"
#include "oracool/skill_sounds.h"
#include "oracool/oracool.h"
#include "player.h"

namespace devilution::oracool {

namespace {

std::optional<PaladinSkill> ArmedSkill;

/**
 * @brief Zeal's ceiling - "to hit up to 5 times", whatever the level.
 */
constexpr int MaxZealStrikes = 5;

/** @brief The character level at which Zeal's second strike arrives. Its unlock level, by design. */
constexpr int ZealFirstUpgradeLevel = 6;
/** @brief And one more strike every this many levels after it: 8, 10, 12. */
constexpr int ZealLevelsPerStrike = 2;

/** @brief Follow-up swings still owed by the current Zeal chain. */
int ZealChainLeft = 0;
/** @brief Whether a chain is running, so a landing follow-up swing does not re-initialize it. */
bool ZealChainActive = false;

/**
 * @brief The frame budget of ONE Zeal swing - the user's spec verbatim (2026-08-15):
 *
 *   "Regular attack should be around 20 frames, so 150% of that is 30 frames.
 *    Levels 6-7  Zeal makes 2 attacks so each one should be 15 frames.
 *    Levels 8-9  Zeal makes 3 attacks so each one should be 10 frames.
 *    Levels 10-11 Zeal makes 4 attacks so each one should be 7.5 frames.
 *    Levels 12+  Zeal makes 5 attacks so each one should be 6 frames."
 *
 * Computed from the character's real _pAFrames rather than a hardcoded 20, so a fast weapon's Zeal
 * is proportionally faster. Integer division floors the 7.5 case to 7.
 */
int ZealPerSwingTicks(const Player &player)
{
	const int strikes = std::max(ZealStrikeCount(player), 1);
	return std::max(4, player._pAFrames * 3 / 2 / strikes);
}

/**
 * @brief What Hammer of Faith's splash does, as a percentage of the blow that landed.
 *
 * Half. It is ONE hammer blow whose force carries, not a second attack on each neighbour - full
 * damage all round would make it strictly better than Zeal at every count of enemies, and the two
 * are meant to be different answers rather than a worse one and a better one.
 */
constexpr int HammerOfFaithSplashPercent = 50;

/**
 * @brief How long Shield Bash holds a monster, in game ticks.
 *
 * The clock runs at 20 ticks a second, so this is two seconds - "a couple of seconds stun" (user,
 * 2026-08-16), up from the original second and a quarter. Long enough to step away or line up the
 * next blow, short enough that it is not a substitute for killing the thing. Shield Bash adds no
 * damage of its own; the stun IS the skill, which is also why it does not scale.
 */
constexpr int ShieldBashStunTicks = 40;

/** @brief Applies @p damage to @p monster, killing it or staggering it as the total decides. */
void StrikeMonster(Player &player, Monster &monster, int damage)
{
	ApplyMonsterDamage(DamageType::Physical, monster, damage);
	if ((monster.hitPoints >> 6) <= 0)
		M_StartKill(monster, player);
	else
		M_StartHit(monster, player, damage);
}

/**
 * @brief Collects up to @p maxTargets monsters within one tile of @p centre, excluding @p centre.
 *
 * Gathered BEFORE anything is applied, for two reasons that have both bitten this code: the mana is
 * charged only if the swing actually carries to someone, so the count has to be known first; and
 * killing a monster mid-scan reorders ActiveMonsters underneath the loop.
 */
int GatherAdjacent(const Monster &centre, Monster **out, int maxTargets)
{
	int found = 0;
	for (size_t i = 0; i < ActiveMonsterCount && found < maxTargets; i++) {
		Monster &other = Monsters[ActiveMonsters[i]];
		if (&other == &centre || !other.isPossibleToHit())
			continue;
		if (other.position.tile.WalkingDistance(centre.position.tile) != 1)
			continue;
		out[found++] = &other;
	}
	return found;
}

/**
 * @brief The enemy a Zeal strike should go to next.
 *
 * "To hit different enemies if possible i.e. if within range, else - hit whoever is in range as many
 * hits as the skill currently provides" (user, 2026-08-15). So: prefer a neighbour that has not been
 * struck yet this burst, and fall back to whatever is still standing next to the player.
 *
 * Re-scanned each strike rather than fixed at the start, deliberately - a burst spread over time is
 * a burst during which enemies die and move, and a list captured up front would keep swinging at a
 * corpse while a live monster stood beside it.
 */
Monster *NextZealTarget(const Player &player, Monster **struck, int struckCount)
{
	Monster *fallback = nullptr;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &candidate = Monsters[ActiveMonsters[i]];
		if (!candidate.isPossibleToHit() || (candidate.hitPoints >> 6) <= 0)
			continue;
		if (candidate.position.tile.WalkingDistance(player.position.tile) > MeleeSkillRangeTiles)
			continue;

		bool alreadyStruck = false;
		for (int s = 0; s < struckCount; s++) {
			if (struck[s] == &candidate) {
				alreadyStruck = true;
				break;
			}
		}
		if (!alreadyStruck)
			return &candidate; // a fresh enemy always wins
		if (fallback == nullptr)
			fallback = &candidate;
	}
	return fallback;
}

/** @brief Who this chain has hit so far, so NextZealTarget can prefer someone else. */
Monster *ZealStruck[MaxZealStrikes] = {};
int ZealStruckCount = 0;

/**
 * @brief Zeal - called when a Zeal-armed swing LANDS, real animation and all.
 *
 * The swing's own damage has already been applied by the normal melee path; this charges the
 * strike's mana, remembers who was hit so the chain prefers fresh targets, and - on the burst's
 * first hit - arms the follow-up swings TryContinueZealChain delivers as the animations end.
 */
void ApplyZeal(Player &player, Monster &primaryTarget)
{
	// One mana a strike, the pairing the user gave (2 hits / 2 mana, up to 5 / 5), charged per
	// LANDED swing. A chain the player cannot pay for ends rather than swinging free.
	if (!SpendPaladinSkillMana(player, PaladinSkill::Zeal)) {
		ZealChainActive = false;
		ZealChainLeft = 0;
		return;
	}

	if (!ZealChainActive) {
		ZealChainActive = true;
		ZealStruckCount = 0;
		ZealChainLeft = ZealStrikeCount(player) - 1;
	}
	if (ZealStruckCount < MaxZealStrikes)
		ZealStruck[ZealStruckCount++] = &primaryTarget;
}

/**
 * @brief Hammer of Faith - one blow, and everything around the target takes half of it.
 *
 * The difference from Zeal is the SHAPE, not the machinery: no cap on how many neighbours are
 * caught, because a hammer's impact does not count heads, and half damage rather than full, because
 * they are catching the shockwave rather than the hammer.
 */
void ApplyHammerOfFaith(Player &player, Monster &primaryTarget, int hitDamage)
{
	const int splashDamage = hitDamage * HammerOfFaithSplashPercent / 100;
	if (splashDamage <= 0)
		return;

	// Eight is every square touching the target - the whole ring, since this one does not cap.
	Monster *targets[8] = {};
	const int found = GatherAdjacent(primaryTarget, targets, 8);
	if (found == 0)
		return;
	if (!SpendPaladinSkillMana(player, PaladinSkill::HammerOfFaith))
		return;

	for (int i = 0; i < found; i++)
		StrikeMonster(player, *targets[i], splashDamage);
}

/** @brief Shield Bash - no extra damage, but the target loses its next second and a bit. */
void ApplyShieldBash(Player &player, Monster &primaryTarget)
{
	// No shield check here any more: requiresShield is part of IsPaladinSkillUnlocked now, so a
	// shieldless Paladin cannot arm this skill at all - the row is greyed and inert.
	//
	// Nothing to stun on a corpse, and charging for it would break the rule that mana follows effect.
	if ((primaryTarget.hitPoints >> 6) <= 0)
		return;
	// "Invalid against uniques and bosses" (user, 2026-08-16): a stun that locks down a boss trivially
	// beats every other answer to a boss, so the names are exempt - scripted uniques, our lesser
	// uniques, and Diablo himself, who is placed as a plain MT_DIABLO rather than through
	// PlaceUniqueMonst and so is the one boss isUnique() cannot see. No mana is charged for the
	// refusal, same rule as the corpse above: mana follows effect.
	if (primaryTarget.isUnique() || primaryTarget.lesserAffix != LesserUniqueAffix::None
	    || primaryTarget.type().type == MT_DIABLO)
		return;
	if (!SpendPaladinSkillMana(player, PaladinSkill::ShieldBash))
		return;

	StunMonster(primaryTarget, ShieldBashStunTicks);
}

} // namespace

int ZealStrikeCount(const Player &player)
{
	if (player._pLevel < ZealFirstUpgradeLevel)
		return 0;
	// Phase 2.1: the frame ladder is point-driven now (megaplan: "Zeal's frame ladder becomes
	// point-driven rather than purely character-level-driven"). Unlocking buys the 2-strike burst;
	// every ZealLevelsPerStrike points INVESTED in Zeal buy one more, up to the cap. A character
	// who spreads their points elsewhere keeps the base burst - which also answers the telemetry
	// watch on 5-hit Zeal being too strong for free.
	const auto zealSpell = static_cast<size_t>(GetPaladinSkillData(PaladinSkill::Zeal).spellId);
	const int extra = player._pSkillInvestment[zealSpell] / ZealLevelsPerStrike;
	return std::min(2 + extra, MaxZealStrikes);
}

void ResetZealChain()
{
	ZealChainActive = false;
	ZealChainLeft = 0;
	ZealStruckCount = 0;
}

bool TryContinueZealChain(Player &player)
{
	if (&player != MyPlayer || !ZealChainActive)
		return false;
	if (ZealChainLeft <= 0) {
		ZealChainActive = false;
		return false;
	}
	// The mana and gate re-check, so a chain the player can no longer pay for ends mid-burst rather
	// than swinging free - the same rule every skill follows.
	if (!CanUsePaladinSkill(player, PaladinSkill::Zeal)) {
		ZealChainActive = false;
		ZealChainLeft = 0;
		return false;
	}
	// Re-scanned each swing rather than fixed at the start: enemies die and move between swings, and
	// a list captured up front would keep swinging at a corpse while a live monster stood beside it.
	Monster *target = NextZealTarget(player, ZealStruck, ZealStruckCount);
	if (target == nullptr) {
		ZealChainActive = false;
		ZealChainLeft = 0;
		return false;
	}

	ZealChainLeft--;
	// A REAL follow-up swing, which is the whole point (user, 2026-08-15: "i dont see the hero
	// making rapid atacks"): the attack animation restarts toward the next target compressed to the
	// per-swing budget, and the blow lands through the same DoAttack hit-frame path as any other
	// swing - real animation, real to-hit roll, real damage. The latch is still armed, so the
	// landing hit re-enters ApplyZeal, pays its mana and records its target.
	const Direction d = GetDirection(player.position.tile, target->position.tile);
	player._pdir = d;
	NewPlrAnim(player, player_graphic::Attack, d,
	    static_cast<AnimationDistributionFlags>(AnimationDistributionFlags::ProcessAnimationPending | AnimationDistributionFlags::RepeatedAction),
	    ZealSwingSkipFrames(player), MeleeHitFrame(player));
	return true;
}

int ZealSwingSkipFrames(const Player &player)
{
	// Compression applies to EVERY Zeal swing including the first - the user's arithmetic divides
	// the whole 150% budget across all of them. The swing keeps its last (budget - 1) windup frames
	// so the blow still lands on its true hit frame; at least 3 always survive, or the swing stops
	// reading as a swing at all.
	if (&player != MyPlayer || !ArmedSkill.has_value() || *ArmedSkill != PaladinSkill::Zeal)
		return 0;
	if (ZealStrikeCount(player) < 2)
		return 0;
	// Oracool bug fix (2026-08-16): user report - "zeal works even after mana is depleted. should
	// convert to regular hit until there is at least 1 mana." The chain and the mana charge were
	// already gated (ApplyZeal refuses, TryContinueZealChain re-checks), but THIS was not: the first
	// swing compressed on the strength of the latch alone, so an unaffordable Zeal still LOOKED like
	// Zeal - one fast swing, over and over, for free. An unaffordable Zeal is now a regular attack in
	// every way, speed included.
	if (!CanUsePaladinSkill(player, PaladinSkill::Zeal))
		return 0;
	const int hitFrame = MeleeHitFrame(player);
	const int keptWindup = std::max(3, ZealPerSwingTicks(player) - 1);
	return std::max(0, hitFrame - keptWindup);
}

void ArmMeleeSkill(std::optional<PaladinSkill> skill)
{
	ArmedSkill = skill;
}

std::optional<PaladinSkill> ArmedMeleeSkill()
{
	return ArmedSkill;
}

bool IsShieldBashSwing(const Player &player)
{
	// MyPlayer only: the latch describes the local player's click, and a remote player's swing has
	// no click here to have armed it. Single-player anyway, but stating it keeps the latch honest.
	if (&player != MyPlayer || !ArmedSkill.has_value() || *ArmedSkill != PaladinSkill::ShieldBash)
		return false;
	// Oracool bug fix (2026-08-16): the crash the user could not reproduce - "assertion failed
	// clx_sprite.hpp:630 value_.data_ != nullptr", mid-fight with an item involved. This function
	// answers "does the swing wear the BLOCK animation", and everything animation asks it: the
	// graphic choice in StartAttack, the hit frame, the tempo stretch. But LoadPlrGFX silently
	// REFUSES to load the block sheet when _pBlockFlag is off - so in the gap between the latch
	// arming and the swing resolving (swap the shield off your arm and click, and the flag drops
	// while the latch holds), StartAttack requested an animation whose sheet was never loaded, and
	// spritesForDirection dereferenced an empty optional. Answering false here routes that swing
	// through the ordinary attack animation instead - same sheet-availability rule LoadPlrGFX
	// itself applies, asked one step earlier.
	return player._pBlockFlag && player._pBFrames > 0;
}

int MeleeHitFrame(const Player &player)
{
	if (!IsShieldBashSwing(player) || player._pBFrames <= 0)
		return player._pAFNum;
	// Never past the block animation's last frame. _pBFrames is only non-zero with a shield equipped,
	// which Shield Bash requires anyway - the guard above is for the frame between unequipping and
	// the swing resolving.
	return std::min<int>(player._pAFNum, player._pBFrames);
}

void ApplyMeleeSkillOnHit(Player &player, Monster &primaryTarget, int hitDamage)
{
	if (!ArmedSkill.has_value() || hitDamage <= 0)
		return;
	// The mana and level gates in one call, so a Paladin who cannot pay simply swings normally -
	// which is the same "still does something" rule the rest of these skills follow.
	if (!CanUsePaladinSkill(player, *ArmedSkill))
		return;

	// Oracool: the impact cue for skills that never make a missile. A swing lands here and nowhere
	// else, and it lands ONCE per connected blow, so this needs no latch of its own - unlike the
	// missile path, which has to ignore a piercing bolt's later victims.
	if (&player == MyPlayer) {
		PlaySkillSound(ClassTreeSkillForSpell(player._pClass, GetPaladinSkillData(*ArmedSkill).spellId),
		    SkillSoundEvent::Impact);
	}

	switch (*ArmedSkill) {
	case PaladinSkill::Zeal:
		ApplyZeal(player, primaryTarget);
		break;
	case PaladinSkill::HammerOfFaith:
		ApplyHammerOfFaith(player, primaryTarget, hitDamage);
		break;
	case PaladinSkill::ShieldBash:
		ApplyShieldBash(player, primaryTarget);
		break;
	// Charge is intercepted before the swing (oracool/furious_charge.cpp); the three that throw
	// something are missiles and never reach a melee hook.
	case PaladinSkill::Charge:
	case PaladinSkill::BlessedShield:
	case PaladinSkill::FistOfTheHeavens:
	case PaladinSkill::BlessedHammer:
		break;
	}
}

} // namespace devilution::oracool
