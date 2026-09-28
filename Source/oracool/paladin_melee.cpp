#include "oracool/paladin_melee.h"

#include "items.h"
#include "monster.h"
#include "oracool/class_tree.h"
#include "oracool/companion.h"
#include "oracool/furious_charge.h"
#include "oracool/minions.h"
#include "oracool/paladin_skills.h"
#include "oracool/passives.h" // Towering Shield
#include "oracool/missile_tint.h" // hue:: - the bursts' colours
#include "oracool/rfa12_actives.h" // ArmedRfa12Melee - Aegis Slam strikes with the shield too
#include "playerdat.hpp"
#include "oracool/skill_sounds.h"
#include "oracool/oracool.h"
#include "player.h"
#include "spells.h" // IsValidSpell - a readied slot may hold Invalid
#include <fmt/format.h>
#include "utils/language.h"

namespace devilution::oracool {

namespace {

std::optional<PaladinSkill> ArmedSkill;

/**
 * @brief Zeal's ceiling - "to hit up to 5 times", whatever the level.
 */
// FOUR, down from five (user, 2026-08-30: "we need to nurf zeal to a maximum of 4 strikes per
// hit. so make it every level of zeal add up to 4 strikes per hot, but also add +1% chance to hit").
//
// The cut is real - a fifth strike is a fifth of the burst's whole damage - and the to-hit is what
// keeps the levels past the fourth strike worth buying. Without it a Zeal at 2 points and a Zeal at
// 40 would be identical, which is the shape that made the five-strike version worth nerfing in the
// first place: everything arrived at once and nothing came after.
constexpr int MaxZealStrikes = 4;

/** @brief The character level Zeal itself unlocks at. A GATE, not a source of power - see below. */
constexpr int ZealFirstUpgradeLevel = 6;

/**
 * @brief The ladder, in SKILL levels (user, 2026-08-30):
 *
 *     at lvl 1 - +1 hit, +1% cth        at lvl 4 - +1% cth
 *     at lvl 2 - +1% cth                at lvl 5 - +1 hit, +1% cth
 *     at lvl 3 - +1 hit, +1% cth        at lvl 6 and onwards - +1% cth
 *
 * The base is ONE strike - a plain swing - so the rungs at skill level 1, 3 and 5 build it to two,
 * three and four. Accuracy is paid at EVERY skill level including those three, and keeps being paid
 * after the strikes stop.
 *
 * This is the same ladder as the earlier "2 hits at lvl 6, 3@8, 4@10", stated the other way round.
 * Zeal unlocks at character level 6 and the Rule of Rangs wants character level 6 + R - 1 for rank
 * R, so skill level 1 IS character level 6, skill 3 is character 8, and skill 5 is character 10.
 * The two readings meet because of the standing rule the user gave with them: "all benefits from
 * skills come from skill levels, not hero levels. Hero levels are just a gate to reaching higher
 * skill levels."
 *
 * Skill level, not points invested: Player::GetSpellLevel adds _pISplLvlAdd and _pSplLvl on top of
 * the investment, so an item granting +spell levels deepens Zeal exactly as investing would. That
 * is what the standing rule is for.
 */
constexpr int ZealFirstStrikeRungSkillLevel = 1;
constexpr int ZealStrikeRungSpacing = 2;
/** @brief The skill level the last strike arrives at. Accuracy carries on past it, alone. */
constexpr int ZealLastStrikeRungSkillLevel = ZealFirstStrikeRungSkillLevel
    + (MaxZealStrikes - 2) * ZealStrikeRungSpacing;
/** @brief Paid at every skill level, from the first - not only after the strikes stop. */
constexpr int ZealToHitPercentPerSkillLevel = 1;

static_assert(ZealLastStrikeRungSkillLevel == 5,
    "the user's ladder puts the extra hits at skill levels 1, 3 and 5 - equivalently character "
    "levels 6, 8 and 10. If the cap or the spacing moves the rungs move with it, and that is a "
    "balance change to re-agree rather than to absorb silently");

/**
 * @brief Zeal's skill level - the one number both halves of the ladder read.
 *
 * Player::GetSpellLevel, not the raw investment: it adds item +spell levels and any book levels on
 * top, which is what "all benefits from skills come from skill levels" asks for. Asked in one place
 * so the strike ladder and the accuracy ladder cannot end up reading different numbers.
 */
int ZealSkillLevel(const Player &player)
{
	return player.GetSpellLevel(GetPaladinSkillData(PaladinSkill::Zeal).spellId);
}

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
// Half at level 1 and +2 points a level since 2026-09-12: HammerOfFaithSplashPercentAt, paladin_melee.h.

/**
 * @brief How long Shield Bash holds a monster, in game ticks.
 *
 * The clock runs at 20 ticks a second, so this is two seconds - "a couple of seconds stun" (user,
 * 2026-08-16), up from the original second and a quarter. Long enough to step away or line up the
 * next blow, short enough that it is not a substitute for killing the thing. The stun does not
 * scale; the blow does, +15% a level since 2026-09-12 (SmiteDamagePercentAt).
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
		// Never the hero's own (audit, 2026-09-27): companions stand at his side, and a spin cut them down - and credited
		// the kill, firing Rampage and Bloodcall for killing an ally. Every other area path already skipped them.
		if (other.isPlayerMinion() || IsMinion(other) || IsCompanion(other))
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
		if (candidate.isPlayerMinion() || IsMinion(candidate) || IsCompanion(candidate))
			continue; // Zeal swings at enemies only (audit, 2026-09-27)
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
	const int rank = std::max(player.GetSpellLevel(GetPaladinSkillData(PaladinSkill::HammerOfFaith).spellId), 1);
	const int splashDamage = hitDamage * HammerOfFaithSplashPercentAt(rank) / 100;
	if (splashDamage <= 0)
		return;

	// Eight is every square touching the target - the whole ring, since this one does not cap.
	Monster *targets[8] = {};
	const int found = GatherAdjacent(primaryTarget, targets, 8);
	if (found == 0)
		return;
	if (!SpendPaladinSkillMana(player, PaladinSkill::HammerOfFaith))
		return;

	// The shockwave's own sound, once per splash - the blow itself already sounded as a hit. With it, Holy Bolt's burst on
	// the target at full size, tinted blue (the sound page's remark, 2026-09-28: "Nova. Add Holy Bolt explosion on impact.
	// Scaled 100%. Tinted blue.").
	PlaySkillSound(ClassTreeSkill::HammerOfFaith, SkillSoundEvent::Impact);
	DrawHolyBurst(player, primaryTarget.position.tile, 100, hue::HolyBlue);
	for (int i = 0; i < found; i++)
		StrikeMonster(player, *targets[i], splashDamage);
}

/**
 * @brief Smite (Shield Bash) - the blow lands +15% a level harder (PaladinMeleeDamagePercent, in the
 * swing's own damage), and the target loses its next two seconds.
 */
void ApplyShieldBash(Player &player, Monster &primaryTarget)
{
	// No shield check here: requiresShield is part of CanUsePaladinSkill (since 2026-09-27), and
	// CheckPlrSpell refuses a shieldless Smite before it is armed.
	//
	// Mana follows the effect, and the effect is the harder blow now as well as the stun (2026-09-12) -
	// so it is charged whenever the bash lands, on a boss or a killing blow too.
	if (!SpendPaladinSkillMana(player, PaladinSkill::ShieldBash))
		return;
	if ((primaryTarget.hitPoints >> 6) <= 0)
		return; // nothing left to stun
	// "Invalid against uniques and bosses" (user, 2026-08-16) - the STUN is: one that locks down a boss
	// trivially beats every other answer to a boss. Scripted uniques, our lesser uniques, and Diablo
	// himself, placed as a plain MT_DIABLO and so the one boss isUnique() cannot see.
	if (primaryTarget.isUnique() || primaryTarget.lesserAffix != LesserUniqueAffix::None
	    || primaryTarget.type().type == MT_DIABLO)
		return;
	StunMonster(primaryTarget, ShieldBashStunTicks);
}

} // namespace

int PaladinMeleeDamagePercent(const Player &player)
{
	if (&player != MyPlayer || !ArmedSkill.has_value())
		return 0;
	const int rank = std::max(player.GetSpellLevel(GetPaladinSkillData(*ArmedSkill).spellId), 1);
	// Charge's mana went at the dash's launch, so its blow asks only whether a dash ended in it.
	if (*ArmedSkill == PaladinSkill::Charge)
		return IsChargeBlowArmed() ? ChargeBlowPercentAt(rank) : 0;
	if (*ArmedSkill == PaladinSkill::ShieldBash && CanUsePaladinSkill(player, PaladinSkill::ShieldBash))
		return SmiteDamagePercentAt(rank) + PassiveSkillDamagePercent(player, SpellID::ShieldBash); // Towering Shield (2026-09-14)
	return 0;
}

int ZealStrikeCount(const Player &player)
{
	if (player._pLevel < ZealFirstUpgradeLevel)
		return 0;
	const int skillLevel = ZealSkillLevel(player);
	if (skillLevel <= 0)
		return 0;
	// One strike, plus one for each rung the skill level has reached. Written as the walk rather
	// than as arithmetic because the ladder is the specification: rungs at 6, 8 and 10.
	int strikes = 1;
	for (int rung = ZealFirstStrikeRungSkillLevel;
	     rung <= skillLevel && strikes < MaxZealStrikes;
	     rung += ZealStrikeRungSpacing) {
		strikes++;
	}
	return strikes;
}

int ZealToHitBonusAtRank(const Player &player)
{
	// Gated on the skill being UNLOCKED rather than merely invested in, so it cannot be bought
	// before the skill itself exists.
	if (player._pClass != HeroClass::Warrior || player._pLevel < ZealFirstUpgradeLevel)
		return 0;
	// One point per skill level, from the FIRST - the levels that also buy a strike pay it too, and
	// it carries on alone once the strikes stop ("at lvl 6 and onwards - +1% cth"). At the rank cap
	// that is +30%, which is where the per-point version topped out as well.
	return std::max(0, ZealSkillLevel(player)) * ZealToHitPercentPerSkillLevel;
}

int ZealToHitBonus(const Player &player)
{
	if (player._pClass != HeroClass::Warrior || player._pLevel < ZealFirstUpgradeLevel)
		return 0;
	// ZEAL'S OWN accuracy, not the Paladin's (user, 2026-08-30: "narrow it to Zeal"). It used to be
	// added in PlrHitMonst for every Paladin melee hit without asking what the swing was thrown
	// with, so an ordinary swing and every other melee skill quietly carried it too - while both
	// descriptions called it Zeal's. The latch is the same one IsShieldBashSwing reads.
	const std::optional<PaladinSkill> armed = ArmedMeleeSkill();
	if (&player != MyPlayer || !armed.has_value() || *armed != PaladinSkill::Zeal)
		return 0;
	return ZealToHitBonusAtRank(player);
}

bool IsZealReadied(const Player &player)
{
	if (player._pClass != HeroClass::Warrior || player._pLevel < ZealFirstUpgradeLevel)
		return false;
	// The BUTTONS, not the latch. ArmedMeleeSkill above answers "is a Zeal swing in flight right
	// now", which is the right question for a hit roll and the wrong one for the character sheet:
	// standing in a menu, nothing is in flight, so the sheet reported no bonus and looked broken
	// (user, 2026-09-02, twice). What a sheet can honestly say is whether Zeal is the thing a mouse
	// button would swing.
	for (const SpellID readied : { player._pRSpell, player._pLRSpell }) {
		if (!IsValidSpell(readied))
			continue;
		if (const std::optional<PaladinSkill> skill = PaladinSkillForSpell(readied);
		    skill.has_value() && *skill == PaladinSkill::Zeal)
			return true;
	}
	return false;
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
	return player._pBlockFlag && player._pBFrames > 0 && !SwingsShieldAttackSheet(player);
}

bool SwingsShieldAttackSheet(const Player &player)
{
	if (&player != MyPlayer)
		return false;
	const bool bash = ArmedSkill.has_value() && *ArmedSkill == PaladinSkill::ShieldBash;
	const std::optional<SpellID> rfa12 = ArmedRfa12Melee();
	const bool slam = rfa12.has_value() && *rfa12 == SpellID::AegisSlam;
	if (!bash && !slam)
		return false;
	// The sheet asked for, not assumed: headless, in town, or missing from the archive, the swing keeps the old sheet.
	return HasShieldEquipped(player) && player.AnimationData[static_cast<size_t>(player_graphic::ShieldAttack)].sprites.has_value();
}

int MeleeHitFrame(const Player &player)
{
	if (SwingsShieldAttackSheet(player))
		return PlayersAnimData[static_cast<size_t>(player._pClass)].unarmedShieldActionFrame;
	if (!IsShieldBashSwing(player) || player._pBFrames <= 0)
		return player._pAFNum;
	// Never past the block animation's last frame. _pBFrames is only non-zero with a shield equipped,
	// which Shield Bash requires anyway - the guard above is for the frame between unequipping and
	// the swing resolving.
	return std::min<int>(player._pAFNum, player._pBFrames);
}

bool PlayArmedSwingCue(const Player &player)
{
	if (&player != MyPlayer || !ArmedSkill.has_value() || *ArmedSkill != PaladinSkill::HammerOfFaith)
		return false;
	// An unaffordable Hammer of Faith is a plain swing (ApplyMeleeSkillOnHit), so it sounds like one.
	if (!CanUsePaladinSkill(player, *ArmedSkill))
		return false;
	return PlaySkillSound(ClassTreeSkill::HammerOfFaith, SkillSoundEvent::Cast);
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
		// A quarter-size Holy Bolt burst, pale warm, on every blow that lands (the Paladin Skill Cards page, 2026-09-28).
		DrawHolyBurst(player, primaryTarget.position.tile, 25, hue::PaleWarm);
		ApplyZeal(player, primaryTarget);
		break;
	case PaladinSkill::HammerOfFaith:
		ApplyHammerOfFaith(player, primaryTarget, hitDamage);
		break;
	case PaladinSkill::ShieldBash:
		// Smite: a quarter-size Holy Bolt burst, Paladin gold, on the bashed enemy (the Skill Cards page, 2026-09-28).
		DrawHolyBurst(player, primaryTarget.position.tile, 25, hue::PaladinGold);
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

std::string PaladinMeleeFactsAt(PaladinSkill skill, int rank)
{
	rank = std::max(rank, 1);
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	switch (skill) {
	case PaladinSkill::Zeal: {
		// The ladder ZealStrikeCount walks, at the rank asked for: rungs at skill levels 1, 3, 5.
		int strikes = 1;
		for (int rung = ZealFirstStrikeRungSkillLevel; rung <= rank && strikes < MaxZealStrikes; rung += ZealStrikeRungSpacing)
			strikes++;
		line(fmt::format(fmt::runtime(_("Strikes: {:d} in one swing")), strikes));
		line(fmt::format(fmt::runtime(_("To hit: +{:d}%")), rank * ZealToHitPercentPerSkillLevel));
		break;
	}
	case PaladinSkill::ShieldBash:
		line(std::string(_("Always hits")));
		line(fmt::format(fmt::runtime(_("Damage: +{:d}%")), SmiteDamagePercentAt(rank)));
		line(fmt::format(fmt::runtime(_("Stun: {:.1f} s")), ShieldBashStunTicks / 20.0));
		break;
	case PaladinSkill::HammerOfFaith:
		line(fmt::format(fmt::runtime(_("Splash: {:d}% to everything around the target")), HammerOfFaithSplashPercentAt(rank)));
		break;
	default:
		break;
	}
	return out;
}

} // namespace devilution::oracool
