#include "oracool/paladin_melee.h"

#include "items.h"
#include "monster.h"
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

/**
 * @brief How much longer than a plain swing a full Zeal burst is allowed to take, in percent.
 *
 * "To hit up to 5 times within 150% of frames of regular attack" (user, 2026-08-15). The burst has a
 * TIME budget rather than a fixed gap, so a fast weapon's Zeal finishes sooner than a slow one's and
 * neither ever runs past the swing that started it by more than half again.
 */
constexpr int ZealBurstFramesPercent = 150;

/**
 * @brief A Zeal burst waiting to finish.
 *
 * File-scope, like the latch above and for the same reasons: single-player, one burst at a time, and
 * it must never reach the save format. A burst is abandoned rather than resumed if anything
 * interrupts it - see ProcessZealBurst.
 */
struct ZealBurst {
	int strikesLeft = 0;
	int ticksUntilNext = 0;
	int ticksBetween = 0;
	int damage = 0;
};
ZealBurst PendingZeal;

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
 * The clock runs at 20 ticks a second, so this is about a second and a quarter - long enough to step
 * away or line up the next blow, short enough that it is not a substitute for killing the thing.
 * Shield Bash adds no damage of its own; the stun IS the skill, which is also why it does not scale.
 */
constexpr int ShieldBashStunTicks = 25;

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

/** @brief Who this burst has hit so far, so NextZealTarget can prefer someone else. */
Monster *ZealStruck[MaxZealStrikes] = {};
int ZealStruckCount = 0;

/** @brief Lands one strike of the burst, and reports whether there was anything to hit. */
bool LandZealStrike(Player &player, int damage)
{
	Monster *target = NextZealTarget(player, ZealStruck, ZealStruckCount);
	if (target == nullptr)
		return false;
	// One mana a strike, so the burst's total price is its strike count - which is the pairing the
	// user gave (2 hits / 2 mana, up to 5 / 5). Charged per strike landed, so a burst cut short by a
	// dying crowd costs only what it actually delivered.
	if (!SpendPaladinSkillMana(player, PaladinSkill::Zeal))
		return false;

	if (ZealStruckCount < MaxZealStrikes)
		ZealStruck[ZealStruckCount++] = target;
	StrikeMonster(player, *target, damage);
	return true;
}

/**
 * @brief Zeal - a burst of strikes spread across whoever is in reach.
 *
 * The first lands with the swing itself; the rest are queued and delivered by ProcessZealBurst so
 * they arrive as a rapid succession rather than as one enormous blow.
 */
void ApplyZeal(Player &player, Monster & /*primaryTarget*/, int hitDamage)
{
	ZealStruckCount = 0;
	PendingZeal = {};

	const int strikes = ZealStrikeCount(player);
	if (strikes <= 0)
		return;
	if (!LandZealStrike(player, hitDamage))
		return; // nothing in reach, and nothing charged

	if (strikes <= 1)
		return;

	// The gap is the budget divided by the MAXIMUM strike count, not by this burst's count - so the
	// rhythm of a Zeal burst is the same at every level and only its LENGTH grows. Dividing by
	// `strikes` made a 2-strike burst put half a second between its two blows and a 5-strike burst a
	// tenth, which read as the low-level version being slower rather than shorter.
	//
	// At the cap the whole burst still spans the 150% of a swing the user asked for; below the cap it
	// simply ends sooner. _pAFrames is the swing's own length, so a fast weapon's Zeal is faster.
	const int budget = std::max<int>(player._pAFrames, 1) * ZealBurstFramesPercent / 100;
	PendingZeal.strikesLeft = strikes - 1;
	PendingZeal.ticksBetween = std::max(budget / MaxZealStrikes, 1);
	PendingZeal.ticksUntilNext = PendingZeal.ticksBetween;
	PendingZeal.damage = hitDamage;
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
	if (!SpendPaladinSkillMana(player, PaladinSkill::ShieldBash))
		return;

	StunMonster(primaryTarget, ShieldBashStunTicks);
}

} // namespace

int ZealStrikeCount(const Player &player)
{
	if (player._pLevel < ZealFirstUpgradeLevel)
		return 0;
	const int extra = (player._pLevel - ZealFirstUpgradeLevel) / ZealLevelsPerStrike;
	return std::min(2 + extra, MaxZealStrikes);
}

void ProcessZealBurst(Player &player)
{
	if (PendingZeal.strikesLeft <= 0)
		return;
	// Oracool bug fix (2026-08-15): user report - "i dont see zeal making burst hits". They were
	// being queued and then thrown away, every single time.
	//
	// This used to abandon the burst whenever _pmode was no longer PM_ATTACK, on the reasoning that a
	// burst is one action and should not outlive it. The arithmetic makes that impossible to satisfy:
	// the first strike lands at the swing's HIT frame, a little past its middle, and the remaining
	// strikes are spaced across a budget of 150% of the whole swing - so by construction they fall
	// after the animation has ended. The guard did not trim the burst's tail, it deleted all of it.
	//
	// A burst outliving its swing by a few ticks is what "within 150% of frames of regular attack"
	// asked for in the first place. What must still stop it is the Paladin no longer being there to
	// throw it: dead, or on another floor. Walking away does not, and should not - the strikes are
	// already paid for by the swing that landed, and NextZealTarget re-checks reach before each one,
	// so a Paladin who steps back simply finds nothing to hit and the burst ends itself.
	if ((player._pHitPoints >> 6) <= 0 || player._pLvlChanging || player._pmode == PM_DEATH
	    || player._pmode == PM_NEWLVL || player._pmode == PM_QUIT) {
		PendingZeal = {};
		return;
	}

	if (--PendingZeal.ticksUntilNext > 0)
		return;
	PendingZeal.ticksUntilNext = PendingZeal.ticksBetween;
	PendingZeal.strikesLeft--;

	// A strike with nothing left in reach ends the burst rather than waiting: the crowd is dead or
	// gone, and there is no reason to keep the clock running.
	if (!LandZealStrike(player, PendingZeal.damage))
		PendingZeal = {};
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
	return &player == MyPlayer && ArmedSkill.has_value() && *ArmedSkill == PaladinSkill::ShieldBash;
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

	switch (*ArmedSkill) {
	case PaladinSkill::Zeal:
		ApplyZeal(player, primaryTarget, hitDamage);
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
