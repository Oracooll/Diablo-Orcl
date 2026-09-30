#include "oracool/melee_skills.h"

#include <algorithm>
#include <cstdlib>

#include "engine/backbuffer_state.hpp"
#include "engine/random.hpp"
#include "missiles.h"
#include "monster.h"
#include "oracool/companion.h"
#include "oracool/minions.h"
#include "oracool/passives.h"
#include "oracool/rage.h"
#include "oracool/class_tree.h" // ClassTreeSkillForSpell - the Impact cue's row
#include "oracool/paladin_melee.h" // MeleeHitFrame - the chained swings' hit frame
#include "oracool/rfa12_actives.h"
#include "oracool/skill_sounds.h"
#include "oracool/weapon_throw.h" // one latch at a time
#include "oracool/whirlwind.h"
#include "oracool/warcries.h" // IsMonsterConverted - a turned monster is no target
#include "player.h"
#include "spells.h"
#include "utils/language.h"
#include <fmt/format.h>

namespace devilution::oracool {

namespace {

/** The latch - see the header, and paladin_melee.cpp's ArmedSkill, which this mirrors. */
std::optional<ClassMeleeSkill> ArmedSkill;

/**
 * Double Swing and Frenzy swing twice: the second blow is a second SWING, each compressed so the two take the time of
 * one attack (the Barbarian Skill Cards page, 2026-09-29: "This attack should make two swings in the span of one attack
 * either reduce frames per attack or the ticks per frame"; Frenzy: "One left click is two swings with this skill").
 * Zeal's chain, for these two - see paladin_melee.cpp's TryContinueZealChain. They were two blows in one swing.
 */
bool IsSwingChainSkill(ClassMeleeSkill skill)
{
	return skill == ClassMeleeSkill::DoubleSwing || skill == ClassMeleeSkill::Frenzy;
}
/** Swings still to come in this chain. */
int ChainLeft = 0;
/** The swing in flight is a chained one - it deals the extra blow's share, and a spender is not charged again. */
bool ChainFollowUp = false;
/** Who the chain swings at. */
Monster *ChainTarget = nullptr;

/**
 * @brief The four numbers that describe most of a skill. Percentages are of the swing's own damage.
 */
struct Profile {
	int bonusPercent;        // added to every blow of the swing
	int bonusPerRank;        // ...and this much more per rank
	int extraStrikes;        // blows beyond the first, on the same target
	int extraStrikesPerRank; // one more every N ranks (0 = never)
	int extraStrikesCap;     // total blows at most
	int extraSharePercent;   // what each extra blow deals, of a normal blow
	int extraSharePerRank;   // ...and this much more per rank
};

Profile ProfileOf(ClassMeleeSkill skill)
{
	switch (skill) {
	case ClassMeleeSkill::Bash:
		return { 30, 10, 0, 0, 1, 0, 0 };
	case ClassMeleeSkill::Stun:
		return { 0, 0, 0, 0, 1, 0, 0 };
	case ClassMeleeSkill::DoubleSwing:
		return { 0, 0, 1, 0, 2, 75, 5 };
	case ClassMeleeSkill::Concentrate:
		return { 50, 10, 0, 0, 1, 0, 0 };
	case ClassMeleeSkill::Frenzy:
		return { 0, 10, 1, 0, 2, 100, 0 };
	case ClassMeleeSkill::Berserk:
		return { 100, 20, 0, 0, 1, 0, 0 };
	case ClassMeleeSkill::LeapAttack:
	case ClassMeleeSkill::VaultingStrike:
		return { 50, 10, 0, 0, 1, 0, 0 };
	case ClassMeleeSkill::BreakingCurrent:
		return { 33, 0, 0, 0, 1, 0, 0 };
	case ClassMeleeSkill::SevenReeds:
		return { 0, 0, 2, 3, 7, 60, 0 };
	case ClassMeleeSkill::OpenPalm:
	case ClassMeleeSkill::RadiantPalm:
		return { 20, 10, 0, 0, 1, 0, 0 };
	case ClassMeleeSkill::HundredFists:
		return { 0, 0, 3, 2, 7, 50, 0 };
	case ClassMeleeSkill::Jab:
		return { 0, 0, 2, 0, 3, 50, 5 };
	case ClassMeleeSkill::PowerStrike:
		return { 30, 5, 0, 0, 1, 0, 0 };
	case ClassMeleeSkill::Impale:
		return { 100, 20, 0, 0, 1, 0, 0 };
	case ClassMeleeSkill::Sacrifice:
		return { 150, 20, 0, 0, 1, 0, 0 };
	case ClassMeleeSkill::ChargedStrike:
	case ClassMeleeSkill::LightningStrike:
		return { 20, 5, 0, 0, 1, 0, 0 };
	case ClassMeleeSkill::Fend:
	case ClassMeleeSkill::Whirlwind:
	case ClassMeleeSkill::WheelOfHeaven:
	case ClassMeleeSkill::SweepingReed:
	case ClassMeleeSkill::Leap:
		break;
	}
	return { 0, 0, 0, 0, 1, 0, 0 };
}

int RankOf(const Player &player, ClassMeleeSkill skill)
{
	return std::max(player.GetSpellLevel(ClassMeleeSkillSpell(skill)), 1);
}

/** @brief Blows this swing lands on its target, the first included. */
int StrikeCount(const Player &player, ClassMeleeSkill skill)
{
	const Profile p = ProfileOf(skill);
	int extra = p.extraStrikes;
	if (p.extraStrikesPerRank > 0)
		extra += (RankOf(player, skill) - 1) / p.extraStrikesPerRank;
	return std::min(1 + extra, p.extraStrikesCap);
}

/** @brief Whether @p player can pay for @p skill right now - mana, or the Barbarian's Rage (oracool/rage.h). */
bool CanPay(const Player &player, ClassMeleeSkill skill)
{
	return CanPaySkill(player, ClassMeleeSkillSpell(skill));
}

/** @brief Settles a use that landed: the price paid, or - for a Rage generator - the Rage of each landed blow. */
void Pay(Player &player, ClassMeleeSkill skill, int landedBlows)
{
	SettleSkill(player, ClassMeleeSkillSpell(skill), landedBlows);
}

/** @brief A blow of @p damage on @p monster - killing it, or staggering it. Mirrors paladin_melee.cpp's StrikeMonster. */
void Strike(Player &player, Monster &monster, int damage)
{
	if (damage <= 0 || monster.hitPoints >> 6 <= 0)
		return;
	ApplyMonsterDamage(DamageType::Physical, monster, damage);
	if ((monster.hitPoints >> 6) <= 0)
		M_StartKill(monster, player);
	else
		M_StartHit(monster, player, damage);
}

/** @brief Every monster on the eight tiles around @p centre that can be hit, except @p except. */
int GatherAround(Point centre, const Monster *except, Monster **out, int maxTargets)
{
	int found = 0;
	for (size_t i = 0; i < ActiveMonsterCount && found < maxTargets; i++) {
		Monster &other = Monsters[ActiveMonsters[i]];
		if (&other == except || !other.isPossibleToHit() || other.hitPoints >> 6 <= 0)
			continue;
		// Never the hero's own (audit, 2026-09-27): companions stand at his side, and a spin cut them down - and credited
		// the kill, firing Rampage and Bloodcall for killing an ally. Every other area path already skipped them.
		if (other.isPlayerMinion() || IsMinion(other) || IsCompanion(other) || IsMonsterConverted(other)) // round 19
			continue;
		if (centre.WalkingDistance(other.position.tile) != 1)
			continue;
		out[found++] = &other;
	}
	return found;
}

/** @brief "Uniques shrug it off": the same exemption Shield Bash's stun carries, for the same reason. */
bool ShrugsOffStagger(const Monster &monster)
{
	return monster.isUnique() || monster.lesserAffix != LesserUniqueAffix::None || monster.type().type == MT_DIABLO;
}

} // namespace

std::optional<ClassMeleeSkill> ClassMeleeSkillForSpell(SpellID spell)
{
	switch (spell) {
	case SpellID::Bash:
		return ClassMeleeSkill::Bash;
	case SpellID::Leap:
		return ClassMeleeSkill::Leap;
	case SpellID::DoubleSwing:
		return ClassMeleeSkill::DoubleSwing;
	case SpellID::Stun:
		return ClassMeleeSkill::Stun;
	case SpellID::LeapAttack:
		return ClassMeleeSkill::LeapAttack;
	case SpellID::Concentrate:
		return ClassMeleeSkill::Concentrate;
	case SpellID::Frenzy:
		return ClassMeleeSkill::Frenzy;
	case SpellID::Whirlwind:
		return ClassMeleeSkill::Whirlwind;
	case SpellID::BerserkBlow:
		return ClassMeleeSkill::Berserk;
	case SpellID::SweepingReed:
		return ClassMeleeSkill::SweepingReed;
	case SpellID::BreakingCurrent:
		return ClassMeleeSkill::BreakingCurrent;
	case SpellID::VaultingStrike:
		return ClassMeleeSkill::VaultingStrike;
	case SpellID::WheelOfHeaven:
		return ClassMeleeSkill::WheelOfHeaven;
	case SpellID::SevenReeds:
		return ClassMeleeSkill::SevenReeds;
	case SpellID::OpenPalm:
		return ClassMeleeSkill::OpenPalm;
	case SpellID::HundredFists:
		return ClassMeleeSkill::HundredFists;
	case SpellID::RadiantPalm:
		return ClassMeleeSkill::RadiantPalm;
	case SpellID::Jab:
		return ClassMeleeSkill::Jab;
	case SpellID::PowerStrike:
		return ClassMeleeSkill::PowerStrike;
	case SpellID::Impale:
		return ClassMeleeSkill::Impale;
	case SpellID::ChargedStrike:
		return ClassMeleeSkill::ChargedStrike;
	case SpellID::Fend:
		return ClassMeleeSkill::Fend;
	case SpellID::LightningStrike:
		return ClassMeleeSkill::LightningStrike;
	case SpellID::Sacrifice:
		return ClassMeleeSkill::Sacrifice;
	default:
		return std::nullopt;
	}
}

SpellID ClassMeleeSkillSpell(ClassMeleeSkill skill)
{
	switch (skill) {
	case ClassMeleeSkill::Bash:
		return SpellID::Bash;
	case ClassMeleeSkill::Leap:
		return SpellID::Leap;
	case ClassMeleeSkill::DoubleSwing:
		return SpellID::DoubleSwing;
	case ClassMeleeSkill::Stun:
		return SpellID::Stun;
	case ClassMeleeSkill::LeapAttack:
		return SpellID::LeapAttack;
	case ClassMeleeSkill::Concentrate:
		return SpellID::Concentrate;
	case ClassMeleeSkill::Frenzy:
		return SpellID::Frenzy;
	case ClassMeleeSkill::Whirlwind:
		return SpellID::Whirlwind;
	case ClassMeleeSkill::Berserk:
		return SpellID::BerserkBlow;
	case ClassMeleeSkill::SweepingReed:
		return SpellID::SweepingReed;
	case ClassMeleeSkill::BreakingCurrent:
		return SpellID::BreakingCurrent;
	case ClassMeleeSkill::VaultingStrike:
		return SpellID::VaultingStrike;
	case ClassMeleeSkill::WheelOfHeaven:
		return SpellID::WheelOfHeaven;
	case ClassMeleeSkill::SevenReeds:
		return SpellID::SevenReeds;
	case ClassMeleeSkill::OpenPalm:
		return SpellID::OpenPalm;
	case ClassMeleeSkill::HundredFists:
		return SpellID::HundredFists;
	case ClassMeleeSkill::RadiantPalm:
		return SpellID::RadiantPalm;
	case ClassMeleeSkill::Jab:
		return SpellID::Jab;
	case ClassMeleeSkill::PowerStrike:
		return SpellID::PowerStrike;
	case ClassMeleeSkill::Impale:
		return SpellID::Impale;
	case ClassMeleeSkill::ChargedStrike:
		return SpellID::ChargedStrike;
	case ClassMeleeSkill::Fend:
		return SpellID::Fend;
	case ClassMeleeSkill::LightningStrike:
		return SpellID::LightningStrike;
	case ClassMeleeSkill::Sacrifice:
		return SpellID::Sacrifice;
	}
	return SpellID::Invalid;
}

bool IsLeapSkill(ClassMeleeSkill skill)
{
	return skill == ClassMeleeSkill::Leap || skill == ClassMeleeSkill::LeapAttack || skill == ClassMeleeSkill::VaultingStrike;
}

int LeapRangeTiles(const Player &player, ClassMeleeSkill skill)
{
	return std::min(4 + (RankOf(player, skill) - 1) / 3, 8);
}

void ArmClassMeleeSkill(std::optional<ClassMeleeSkill> skill)
{
	ArmedSkill = skill;
	BeginClassMeleeSwing(); // a new click: no chain carries over
	// One latch at a time: arming or disarming this one drops the RfA-12 swing (rfa12_actives.h), which is
	// armed after it where it is meant.
	ArmRfa12Melee(std::nullopt);
	// And the throw, armed after it where it is meant: left armed, it took over the next skill's swing at its hit frame
	// and threw the weapon at an old tile (round 5 audit, v1.12.230).
	ArmWeaponThrow(std::nullopt);
}

std::optional<ClassMeleeSkill> ArmedClassMeleeSkill()
{
	return ArmedSkill;
}

int ClassMeleeSkillDamagePercent(const Player &player)
{
	// The spin's blows (oracool/whirlwind.h): a share of a normal blow, the latch or none.
	if (IsWhirlwinding(player))
		return WhirlwindDamagePercent(RankOf(player, ClassMeleeSkill::Whirlwind)) - 100;
	if (&player != MyPlayer || !ArmedSkill.has_value() || !CanPay(player, *ArmedSkill))
		return 0;
	const Profile p = ProfileOf(*ArmedSkill);
	const int rank = RankOf(player, *ArmedSkill);
	const int bonus = p.bonusPercent + p.bonusPerRank * (rank - 1);
	// A chained swing is the extra blow: its share of a first blow that carried the bonus.
	if (ChainFollowUp && IsSwingChainSkill(*ArmedSkill))
		return (100 + bonus) * (p.extraSharePercent + p.extraSharePerRank * (rank - 1)) / 100 - 100;
	return bonus;
}

void BeginClassMeleeSwing()
{
	ChainLeft = 0;
	ChainFollowUp = false;
	ChainTarget = nullptr;
}

int ClassMeleeSwingSkipFrames(const Player &player)
{
	if (&player != MyPlayer || !ArmedSkill.has_value() || !IsSwingChainSkill(*ArmedSkill) || !CanPay(player, *ArmedSkill))
		return 0;
	if (StrikeCount(player, *ArmedSkill) < 2)
		return 0;
	// Two swings in one attack's time: each keeps the second half of its windup (at least 3 frames), and the first hands
	// over at its blow, so windup + windup + one recovery is about one whole swing.
	const int hitFrame = MeleeHitFrame(player);
	return std::max(0, std::min(hitFrame / 2, hitFrame - 3));
}

bool TryContinueClassMeleeChain(Player &player)
{
	if (&player != MyPlayer || ChainLeft <= 0 || !ArmedSkill.has_value() || !IsSwingChainSkill(*ArmedSkill) || !CanPay(player, *ArmedSkill)
	    || ChainTarget == nullptr || (ChainTarget->hitPoints >> 6) <= 0 || !ChainTarget->isPossibleToHit()
	    || player.position.tile.WalkingDistance(ChainTarget->position.tile) > 1) {
		ChainLeft = 0;
		ChainFollowUp = false;
		return false;
	}
	ChainLeft--;
	ChainFollowUp = true;
	const Direction d = GetDirection(player.position.tile, ChainTarget->position.tile);
	player._pdir = d;
	// A REAL swing, as Zeal's: the blow lands through DoAttack's hit frame - to-hit roll, damage and all.
	NewPlrAnim(player, player_graphic::Attack, d,
	    static_cast<AnimationDistributionFlags>(AnimationDistributionFlags::ProcessAnimationPending | AnimationDistributionFlags::RepeatedAction),
	    ClassMeleeSwingSkipFrames(player), MeleeHitFrame(player));
	return true;
}

int ClassMeleeSkillBonusPercentFor(const Player &player, SpellID spell)
{
	const std::optional<ClassMeleeSkill> skill = ClassMeleeSkillForSpell(spell);
	if (!skill.has_value())
		return -1;
	// Whirlwind's profile is all zeros: its blows are a share of a normal one, as ClassMeleeSkillDamagePercent says -
	// the sheet showed the full swing, half again the real blow (round 4 audit, v1.12.229).
	if (*skill == ClassMeleeSkill::Whirlwind)
		return WhirlwindDamagePercent(RankOf(player, ClassMeleeSkill::Whirlwind)) - 100;
	const Profile p = ProfileOf(*skill);
	return p.bonusPercent + p.bonusPerRank * (RankOf(player, *skill) - 1);
}

bool ApplyClassMeleeSkillOnSwing(Player &player, Monster *front, bool frontHit, int frontDamage)
{
	if (&player != MyPlayer || !ArmedSkill.has_value())
		return false;
	const ClassMeleeSkill skill = *ArmedSkill;
	// Cannot pay: the swing was a plain swing (the damage bonus already answered zero), and it
	// costs nothing. The latch stays, so the next swing asks again - mana comes back.
	if (!CanPay(player, skill))
		return false;

	const Profile p = ProfileOf(skill);
	const int rank = RankOf(player, skill);
	bool struck = false;
	// Every blow of this swing that struck a monster - the Barbarian's Rage is earned per blow.
	int landedBlows = front != nullptr && frontHit ? 1 : 0;

	// The extra blows on the front target, each a share of what the first one dealt - which already
	// carries the skill's bonus, so a Double Swing's second blow is three quarters of a Bash-sized
	// first, not of a plain one.
	// Double Swing and Frenzy: the extra blows are swings of their own (2026-09-29) - the first swing arms the chain,
	// landed or not, and TryContinueClassMeleeChain delivers the rest.
	if (IsSwingChainSkill(skill)) {
		if (front != nullptr && !ChainFollowUp) {
			ChainLeft = StrikeCount(player, skill) - 1;
			ChainTarget = front;
		}
	} else if (front != nullptr && frontHit && frontDamage > 0) {
		const int share = p.extraSharePercent + p.extraSharePerRank * (rank - 1);
		for (int i = 1; i < StrikeCount(player, skill); i++) {
			if (front->hitPoints >> 6 <= 0)
				break;
			Strike(player, *front, frontDamage * share / 100);
			struck = true;
			landedBlows++;
		}
	}

	switch (skill) {
	case ClassMeleeSkill::Bash:
	case ClassMeleeSkill::OpenPalm:
		// One push a blow: a Knockback item already pushed it in PlrHitMonst, and the two sent it two tiles (round 19).
		if (front != nullptr && frontHit && front->hitPoints >> 6 > 0 && front->mode != MonsterMode::Petrified
		    && !HasAnyOf(player._pIFlags, ItemSpecialEffect::Knockback)) {
			M_GetKnockback(*front);
			struck = true;
		}
		break;
	case ClassMeleeSkill::Stun:
		if (front != nullptr && frontHit && front->hitPoints >> 6 > 0 && !ShrugsOffStagger(*front)) {
			StunMonster(*front, 30 + 4 * (rank - 1)); // a second and a half, a fifth more a rank
			struck = true;
		}
		break;
	case ClassMeleeSkill::BreakingCurrent:
		if (front != nullptr && frontHit && front->hitPoints >> 6 > 0 && !ShrugsOffStagger(*front)) {
			StunMonster(*front, 20);
			struck = true;
		}
		break;
	case ClassMeleeSkill::Whirlwind:
	case ClassMeleeSkill::WheelOfHeaven:
	case ClassMeleeSkill::Fend: {
		// Everything around the PLAYER, at a share of a normal blow - rolled from the bow of the
		// weapon rather than from the front hit, because the front tile may have been empty and the
		// spin still has to mean something. The front target, if it was hit, was hit already.
		Monster *targets[8] = {};
		const int found = GatherAround(player.position.tile, frontHit ? front : nullptr, targets, 8);
		// Fend is the Rogue's spin, and a wider one: four fifths rather than two thirds.
		const int share = (skill == ClassMeleeSkill::Fend ? 80 : 66) + 5 * (rank - 1);
		for (int i = 0; i < found; i++) {
			const int blow = (player._pIMinDam + GenerateRnd(std::max(player._pIMaxDam - player._pIMinDam, 0) + 1)) << 6;
			Strike(player, *targets[i], blow * share / 100);
			struck = true;
			landedBlows++;
		}
	} break;
	case ClassMeleeSkill::SweepingReed: {
		// The two tiles beside the target - the arc of a staff, the three tiles ahead its text names. A normal blow each,
		// not a share. Those two only (audit, 2026-09-29): "beside the target and the player" reached four tiles facing
		// straight along a row.
		const Monster *first = nullptr; // once each: a walker holds both tiles on a diagonal facing (round 7 audit)
		for (const Point tile : { player.position.tile + Left(player._pdir), player.position.tile + Right(player._pdir) }) {
			Monster *m = InDungeonBounds(tile) ? FindMonsterAtPosition(tile) : nullptr;
			if (m == nullptr || m == front || m == first || (m->hitPoints >> 6) <= 0 || m->isPlayerMinion() || !m->isPossibleToHit())
				continue;
			first = m;
			const int blow = (player._pIMinDam + GenerateRnd(std::max(player._pIMaxDam - player._pIMinDam, 0) + 1)) << 6;
			Strike(player, *m, blow);
			struck = true;
			landedBlows++;
		}
	} break;
	case ClassMeleeSkill::RadiantPalm:
		// "Erupts when it falls": if this blow killed it, the blow lands again on every neighbour.
		if (front != nullptr && frontHit && front->hitPoints >> 6 <= 0 && frontDamage > 0) {
			Monster *targets[8] = {};
			const int found = GatherAround(front->position.tile, front, targets, 8);
			for (int i = 0; i < found; i++) {
				Strike(player, *targets[i], frontDamage);
				struck = true;
			}
		}
		break;
	case ClassMeleeSkill::PowerStrike:
		// The charge: lightning on top of the blow, one to four a rank, in its own colour.
		// Lightning obeys lightning's immunity and resistance, as every other strike of it (audit, 2026-09-29).
		if (front != nullptr && frontHit && front->hitPoints >> 6 > 0 && !front->isImmune(MissileID::Null, DamageType::Lightning)) {
			int bolt = (1 + GenerateRnd(4 * rank)) << 6;
			if (front->isResistant(MissileID::Null, DamageType::Lightning))
				bolt >>= 2;
			ApplyMonsterDamage(DamageType::Lightning, *front, bolt);
			if (front->hitPoints >> 6 <= 0)
				M_StartKill(*front, player);
			struck = true;
		}
		break;
	case ClassMeleeSkill::ChargedStrike:
		// The blow throws off charged bolts - two, one more every two ranks - from the Rogue toward
		// the target, at the rank. The engine's own Charged Bolt, so they wander as it wanders.
		if (front != nullptr && frontHit) {
			const Direction dir = GetDirection(player.position.tile, front->position.tile);
			for (int i = 0; i < 2 + (rank - 1) / 2; i++) {
				AddMissile(player.position.tile, front->position.tile, dir, MissileID::ChargedBolt, TARGET_MONSTERS,
				    static_cast<int>(player.getId()), 0, rank);
			}
			struck = true;
		}
		break;
	case ClassMeleeSkill::LightningStrike:
		// The lightning leaps onward: the engine's Chain Lightning, launched from the Rogue through
		// the target, at the rank.
		if (front != nullptr && frontHit) {
			const Direction dir = GetDirection(player.position.tile, front->position.tile);
			AddMissile(player.position.tile, front->position.tile, dir, MissileID::ChainLightning, TARGET_MONSTERS,
			    static_cast<int>(player.getId()), 0, rank);
			struck = true;
		}
		break;
	case ClassMeleeSkill::Sacrifice:
		// The price: a twelfth of what the blow dealt, from the striker's own life. Never the last
		// point of it - a Sacrifice cannot kill the one making it.
		if (front != nullptr && frontHit && frontDamage > 0) {
			const int wound = frontDamage / 12;
			ApplyPlrDamage(DamageType::Physical, player, wound >> 6, /*minHP=*/1, wound & 63);
			// A quarter-size Holy Bolt burst, infrared, on the struck enemy (the Paladin Skill Cards page, 2026-09-28).
			DrawHolyBurst(player, front->position.tile, 25, hue::Infrared);
			struck = true;
		}
		break;
	default:
		break;
	}

	// A landed blow: the skill's burst, where it has one, and its Impact cue (the Barbarian Skill Cards page,
	// 2026-09-29 - the table's Impact cues for these skills were never played until now).
	if (front != nullptr && frontHit) {
		int burst = 0;
		uint32_t burstHue = 0;
		switch (skill) {
		case ClassMeleeSkill::Bash: burst = 25, burstHue = hue::Infrared; break;
		case ClassMeleeSkill::Stun: burst = 25; break; // vanilla's own colours
		case ClassMeleeSkill::Concentrate: burst = 25, burstHue = hue::Infrared; break;
		case ClassMeleeSkill::DoubleSwing: burst = 25; break;
		case ClassMeleeSkill::Frenzy: burst = 50, burstHue = hue::Infrared; break;
		case ClassMeleeSkill::Berserk: burst = 75, burstHue = hue::Infrared; break;
		default: break;
		}
		if (burst > 0)
			DrawHolyBurst(player, front->position.tile, burst, burstHue);
		const ClassTreeSkill row = ClassTreeSkillForSpell(player._pClass, ClassMeleeSkillSpell(skill));
		if (row != ClassTreeSkill::None)
			PlaySkillSound(row, SkillSoundEvent::Impact);
	}

	// A chained swing of a spender was paid for with the first; a generator earns on every landed blow.
	if (ChainFollowUp && IsSwingChainSkill(skill) && RageGain(ClassMeleeSkillSpell(skill)) == 0)
		return struck;

	// Paid when the skill did something, or when its bonus rode a blow that landed - a Bash that
	// connected is a Bash even if the target was too heavy to shove.
	//
	// The Barbarian settles on ANY landed blow (2026-09-14, "make sure every hit that lands delivers
	// rage"). The old test lost Rage three ways: a Stun that the target shrugged off earned nothing,
	// a Double Swing or Frenzy whose first blow dealt no damage earned nothing, and every generator
	// earned once per swing however many of its blows struck.
	if (UsesRage(player)) {
		if (struck || landedBlows > 0)
			Pay(player, skill, landedBlows);
	} else if (struck || (frontHit && p.bonusPercent > 0)) {
		Pay(player, skill, landedBlows);
	}
	return struck;
}

bool LeapToward(Player &player, ClassMeleeSkill skill, Point target)
{
	if (&player != MyPlayer || !CanPay(player, skill))
		return false;
	// From a stand or a walk only. A walk is stopped first: its last frame puts the hero on the step's target tile, so a
	// leap taken mid-step snapped back there when the step ended - paid for (round 6 audit, v1.12.231). Hit recovery and a
	// block are not left early.
	if (IsAnyOf(player._pmode, PM_WALK_NORTHWARDS, PM_WALK_SOUTHWARDS, PM_WALK_SIDEWAYS)) {
		ClrPlrPath(player);
		player.destAction = ACTION_NONE;
		StartStand(player, player._pdir);
	} else if (player._pmode != PM_STAND) {
		return false;
	}
	const int range = LeapRangeTiles(player, skill);
	const Point here = player.position.tile;
	Point dst = target;
	// Clamp the landing to the skill's reach along each axis; the engine's teleport then finds the
	// nearest open tile to it, which is what lets a leap end beside a monster rather than on it.
	const Displacement delta = target - here;
	if (std::abs(delta.deltaX) > range || std::abs(delta.deltaY) > range) {
		const int longest = std::max(std::abs(delta.deltaX), std::abs(delta.deltaY));
		dst = here + Displacement { delta.deltaX * range / longest, delta.deltaY * range / longest };
	}
	if (dst == here)
		return false;
	Missile *missile = AddMissile(here, dst, player._pdir, MissileID::Teleport, TARGET_MONSTERS,
	    static_cast<int>(player.getId()), 0, 0);
	if (missile == nullptr)
		return false;
	Pay(player, skill, /*landedBlows=*/0); // the leap itself strikes nothing
	// Heard and seen (the Barbarian Skill Cards page, 2026-09-29): the Cast cue as he goes, the Impact cue and a
	// half-again-size Holy Bolt burst, pale warm, where he lands - the teleport has already found the tile.
	const ClassTreeSkill row = ClassTreeSkillForSpell(player._pClass, ClassMeleeSkillSpell(skill));
	if (row != ClassTreeSkill::None) {
		PlaySkillSound(row, SkillSoundEvent::Cast);
		PlaySkillSound(row, SkillSoundEvent::Impact);
	}
	if (skill == ClassMeleeSkill::Leap || skill == ClassMeleeSkill::LeapAttack)
		DrawHolyBurst(player, missile->position.tile, 150, hue::PaleWarm);
	return true;
}

const char *ClassMeleeSkillDescription(SpellID spell)
{
	switch (spell) {
	case SpellID::Bash:
		return N_("A heavy blow at +30% damage, +10% per rank, that knocks the target back.");
	case SpellID::Leap:
		return N_("Vault to the spot under the cursor, over anything in the way - four tiles, a tile further every three ranks.");
	case SpellID::DoubleSwing:
		return N_("Two swings in one attack's time, the second at 75% damage, +5% per rank.");
	case SpellID::Stun:
		return N_("A blow that leaves the target reeling for 1.5 seconds, +20% longer per rank. Uniques shrug it off.");
	case SpellID::LeapAttack:
		return N_("Leap onto a distant enemy; the blow you land there is at +50% damage, +10% per rank.");
	case SpellID::Concentrate:
		return N_("A focused blow at +50% damage, +10% per rank.");
	case SpellID::Frenzy:
		return N_("Two swings in one attack's time, both at 100% damage, +10% per rank.");
	case SpellID::Whirlwind:
		return N_("Hold the right button to spin toward the cursor at a run, striking everything beside you four times a second at 66% damage, +5% per rank. Drains 5 Rage a second.");
	case SpellID::BerserkBlow:
		return N_("A blow at +100% damage, +20% per rank.");
	case SpellID::SweepingReed:
		return N_("Sweep your staff through the three tiles ahead - the target and both beside it - at full force.");
	case SpellID::BreakingCurrent:
		return N_("A focused strike at +33% damage that leaves the target reeling for 1 second.");
	case SpellID::VaultingStrike:
		return N_("Vault onto a distant foe; the blow you land there is at +50% damage, +10% per rank.");
	case SpellID::WheelOfHeaven:
		return N_("Every swing strikes everything around you at 66% damage, +5% per rank.");
	case SpellID::SevenReeds:
		return N_("Three blows in one swing, one more every three ranks up to seven, each at 60% damage.");
	case SpellID::OpenPalm:
		return N_("An open-hand strike at +20% damage, +10% per rank, that drives the enemy back a tile.");
	case SpellID::HundredFists:
		return N_("Four blows in one swing, one more every two ranks up to seven, each at 50% damage.");
	case SpellID::RadiantPalm:
		return N_("A strike at +20% damage, +10% per rank; an enemy it kills erupts, dealing the blow again to everything beside it.");
	case SpellID::Jab:
		return N_("Three quick thrusts in one motion, the second and third at 50% damage, +5% per rank.");
	case SpellID::PowerStrike:
		return N_("A thrust at +30% damage, +5% per rank, with 1-4 lightning damage per rank on top of it.");
	case SpellID::Impale:
		return N_("A savage thrust at +100% damage, +20% per rank.");
	case SpellID::ChargedStrike:
		return N_("A thrust at +20% damage, +5% per rank, that throws off two charged bolts toward the target, one more every two ranks.");
	case SpellID::Fend:
		return N_("Every swing also strikes everything around you at 80% damage, +5% per rank.");
	case SpellID::LightningStrike:
		return N_("A thrust at +20% damage, +5% per rank, whose lightning leaps on from the target to the next enemy, and the next.");
	case SpellID::Sacrifice:
		return N_("A blow at +150% damage, +20% per rank, that costs you 8% of the damage it dealt in life. It cannot take your last point of life.");
	default:
		return "";
	}
}

std::string MeleeSkillFactsAt(ClassMeleeSkill skill, int rank)
{
	// THE FACTS (user, 2026-09-05: "they are missing essential information about a skill's effect
	// like SPEED/RANGE/DMG/ETC.... all the stuff that happen in the back of the engine during
	// triggering - let them be known"). Read off the same Profile and the same constants
	// ApplyClassMeleeSkillOnSwing uses, at the rank asked for, so the tooltip cannot promise a
	// number the swing does not roll.
	rank = std::max(rank, 1);
	const Profile p = ProfileOf(skill);
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	const int bonus = p.bonusPercent + p.bonusPerRank * (rank - 1);
	if (bonus != 0)
		line(fmt::format(fmt::runtime(_("Damage: +{:d}%")), bonus));
	int strikes = 1 + p.extraStrikes;
	if (p.extraStrikesPerRank > 0)
		strikes += (rank - 1) / p.extraStrikesPerRank;
	strikes = std::min(strikes, p.extraStrikesCap);
	if (strikes > 1) {
		// Double Swing and Frenzy swing twice since v1.12.222; the Monk's many blows still land in one (audit, 2026-09-29).
		if (IsSwingChainSkill(skill))
			line(fmt::format(fmt::runtime(_("Swings: {:d}, in one attack's time")), strikes));
		else
			line(fmt::format(fmt::runtime(_("Strikes: {:d} in one swing")), strikes));
		line(fmt::format(fmt::runtime(_("Extra strikes at {:d}% damage")), p.extraSharePercent + p.extraSharePerRank * (rank - 1)));
	}
	switch (skill) {
	case ClassMeleeSkill::Bash:
	case ClassMeleeSkill::OpenPalm:
		line(std::string(_("Knocks the target back")));
		break;
	case ClassMeleeSkill::Stun:
		line(fmt::format(fmt::runtime(_("Stun: {:.1f} s")), (30 + 4 * (rank - 1)) / 20.0));
		break;
	case ClassMeleeSkill::BreakingCurrent:
		line(std::string(_("Stun: 1.0 s")));
		break;
	case ClassMeleeSkill::Whirlwind:
		line(fmt::format(fmt::runtime(_("Strikes everything beside you {:d} times a second at {:d}% damage")), 20 / WhirlwindStrikeTicks,
		    WhirlwindDamagePercent(rank)));
		line(fmt::format(fmt::runtime(_("Rage: {:d} a second while spinning")), WhirlwindRagePerSecond));
		line(std::string(_("Held on the right button; moves at run speed")));
		break;
	case ClassMeleeSkill::WheelOfHeaven:
		line(fmt::format(fmt::runtime(_("Hits everything around you at {:d}% damage")), 66 + 5 * (rank - 1)));
		break;
	case ClassMeleeSkill::Fend:
		line(fmt::format(fmt::runtime(_("Also hits everything around you at {:d}% damage")), 80 + 5 * (rank - 1)));
		break;
	case ClassMeleeSkill::SweepingReed:
		line(std::string(_("Also hits everything ahead of you at 100% damage")));
		break;
	case ClassMeleeSkill::RadiantPalm:
		line(std::string(_("A kill deals the blow again to everything beside it")));
		break;
	case ClassMeleeSkill::PowerStrike:
		line(fmt::format(fmt::runtime(_("Lightning: 1 - {:d}")), 4 * rank));
		break;
	case ClassMeleeSkill::ChargedStrike:
		line(fmt::format(fmt::runtime(_("Charged bolts: {:d}")), 2 + (rank - 1) / 2));
		break;
	case ClassMeleeSkill::LightningStrike:
		line(fmt::format(fmt::runtime(_("Chain lightning at level {:d}")), rank));
		break;
	case ClassMeleeSkill::Leap:
	case ClassMeleeSkill::LeapAttack:
	case ClassMeleeSkill::VaultingStrike:
		line(fmt::format(fmt::runtime(_("Range: {:d} tiles")), std::min(4 + (rank - 1) / 3, 8)));
		break;
	case ClassMeleeSkill::Sacrifice:
		line(std::string(_("Costs 8% of the damage dealt in life")));
		break;
	default:
		break;
	}
	return out;
}

} // namespace devilution::oracool
