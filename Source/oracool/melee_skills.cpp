#include "oracool/melee_skills.h"

#include <algorithm>
#include <cstdlib>

#include "engine/backbuffer_state.hpp"
#include "engine/random.hpp"
#include "missiles.h"
#include "monster.h"
#include "oracool/passives.h"
#include "player.h"
#include "spells.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

/** The latch - see the header, and paladin_melee.cpp's ArmedSkill, which this mirrors. */
std::optional<ClassMeleeSkill> ArmedSkill;

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

/** @brief Whether @p player can pay for @p skill right now. The price is asked of the spell table, like every tree skill. */
bool CanPay(const Player &player, ClassMeleeSkill skill)
{
	return player._pMana >= GetManaAmount(player, ClassMeleeSkillSpell(skill));
}

void Pay(Player &player, ClassMeleeSkill skill)
{
	const int cost = GetManaAmount(player, ClassMeleeSkillSpell(skill));
	player._pMana -= cost;
	player._pManaBase -= cost;
	oracool::OnPassiveManaSpent(player, cost);
	RedrawComponent(PanelDrawComponent::Mana);
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
}

std::optional<ClassMeleeSkill> ArmedClassMeleeSkill()
{
	return ArmedSkill;
}

int ClassMeleeSkillDamagePercent(const Player &player)
{
	if (&player != MyPlayer || !ArmedSkill.has_value() || !CanPay(player, *ArmedSkill))
		return 0;
	const Profile p = ProfileOf(*ArmedSkill);
	return p.bonusPercent + p.bonusPerRank * (RankOf(player, *ArmedSkill) - 1);
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

	// The extra blows on the front target, each a share of what the first one dealt - which already
	// carries the skill's bonus, so a Double Swing's second blow is three quarters of a Bash-sized
	// first, not of a plain one.
	if (front != nullptr && frontHit && frontDamage > 0) {
		const int share = p.extraSharePercent + p.extraSharePerRank * (rank - 1);
		for (int i = 1; i < StrikeCount(player, skill); i++) {
			if (front->hitPoints >> 6 <= 0)
				break;
			Strike(player, *front, frontDamage * share / 100);
			struck = true;
		}
	}

	switch (skill) {
	case ClassMeleeSkill::Bash:
	case ClassMeleeSkill::OpenPalm:
		if (front != nullptr && frontHit && front->hitPoints >> 6 > 0 && front->mode != MonsterMode::Petrified) {
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
		}
	} break;
	case ClassMeleeSkill::SweepingReed: {
		// The two tiles beside the target - the arc of a staff. A normal blow each, not a share.
		const Point ahead = player.position.tile + player._pdir;
		Monster *targets[8] = {};
		const int found = GatherAround(ahead, front, targets, 8);
		for (int i = 0; i < found; i++) {
			// Beside the target means also beside the player: the arc does not reach behind it.
			if (player.position.tile.WalkingDistance(targets[i]->position.tile) != 1)
				continue;
			const int blow = (player._pIMinDam + GenerateRnd(std::max(player._pIMaxDam - player._pIMinDam, 0) + 1)) << 6;
			Strike(player, *targets[i], blow);
			struck = true;
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
		if (front != nullptr && frontHit && front->hitPoints >> 6 > 0) {
			const int bolt = (1 + GenerateRnd(4 * rank)) << 6;
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
	default:
		break;
	}

	// Paid when the skill did something, or when its bonus rode a blow that landed - a Bash that
	// connected is a Bash even if the target was too heavy to shove.
	if (struck || (frontHit && p.bonusPercent > 0))
		Pay(player, skill);
	return struck;
}

bool LeapToward(Player &player, ClassMeleeSkill skill, Point target)
{
	if (&player != MyPlayer || !CanPay(player, skill))
		return false;
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
	Pay(player, skill);
	return true;
}

const char *ClassMeleeSkillDescription(SpellID spell)
{
	switch (spell) {
	case SpellID::Bash:
		return N_("A heavy blow: a third again as hard, a tenth more a rank, and it knocks the target back.");
	case SpellID::Leap:
		return N_("Vault to the spot under the cursor, over anything in the way - four tiles, a tile further every three ranks.");
	case SpellID::DoubleSwing:
		return N_("Two blows in one swing, the second at three quarters, a twentieth more a rank.");
	case SpellID::Stun:
		return N_("A blow that leaves the target reeling for a second and a half, a fifth longer a rank. Uniques shrug it off.");
	case SpellID::LeapAttack:
		return N_("Leap onto a distant enemy; the blow you land there is half again as hard, a tenth more a rank.");
	case SpellID::Concentrate:
		return N_("A focused blow half again as hard, a tenth more a rank.");
	case SpellID::Frenzy:
		return N_("Two blows in one swing, both at full force, a tenth more a rank.");
	case SpellID::Whirlwind:
		return N_("Every swing strikes everything around you, at two thirds, a twentieth more a rank.");
	case SpellID::BerserkBlow:
		return N_("A blow twice as hard, a fifth more a rank.");
	case SpellID::SweepingReed:
		return N_("Sweep your staff through the three tiles ahead - the target and both beside it - at full force.");
	case SpellID::BreakingCurrent:
		return N_("A focused strike a third again as hard that leaves the target reeling for a second.");
	case SpellID::VaultingStrike:
		return N_("Vault onto a distant foe; the blow you land there is half again as hard, a tenth more a rank.");
	case SpellID::WheelOfHeaven:
		return N_("Every swing strikes everything around you, at two thirds, a twentieth more a rank.");
	case SpellID::SevenReeds:
		return N_("Three blows in one swing, one more every three ranks up to seven, each at three fifths.");
	case SpellID::OpenPalm:
		return N_("An open-hand strike a fifth harder, a tenth more a rank, that drives the enemy back a tile.");
	case SpellID::HundredFists:
		return N_("Four blows in one swing, one more every two ranks up to seven, each at half.");
	case SpellID::RadiantPalm:
		return N_("A strike a fifth harder, a tenth more a rank; an enemy it kills erupts, dealing the blow again to everything beside it.");
	case SpellID::Jab:
		return N_("Three quick thrusts in one motion, the second and third at half, a twentieth more a rank.");
	case SpellID::PowerStrike:
		return N_("A thrust a third again as hard, a twentieth more a rank, with one to four lightning a rank on top of it.");
	case SpellID::Impale:
		return N_("A savage thrust twice as hard, a fifth more a rank.");
	case SpellID::ChargedStrike:
		return N_("A thrust a fifth harder that throws off two charged bolts toward the target, one more every two ranks.");
	case SpellID::Fend:
		return N_("Every swing also strikes everything around you, at four fifths, a twentieth more a rank.");
	case SpellID::LightningStrike:
		return N_("A thrust a fifth harder whose lightning leaps on from the target to the next enemy, and the next.");
	default:
		return "";
	}
}

} // namespace devilution::oracool
