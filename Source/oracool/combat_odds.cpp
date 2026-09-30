/**
 * @file oracool/combat_odds.cpp
 *
 * See combat_odds.h. The frozen monster side of each formula is kept by value - a name, a to-hit or an armour, a
 * level - never a reference into Monsters[], which is reused for the next monster the moment this one dies.
 */
#include "oracool/combat_odds.h"

#include <algorithm>
#include <utility>

#include "items.h" // ItemSpecialEffectHf - the armour-against-demons and -undead bonuses
#include "monster.h"
#include "multi.h" // sgGameInitInfo - a monster's level depends on the difficulty
#include "oracool/lesser_uniques.h" // GetMonsterDisplayName
#include "oracool/paladin_melee.h" // Zeal, which the sheet's To hit already counts
#include "oracool/passives.h"      // Dodge - a blow that lands can still slip
#include "oracool/rfa12_actives.h" // Mantra of Evasion, the same
#include "oracool/rfa12_effects.h" // Rfa12BlockBonus
#include "oracool/warcries.h"      // EffectiveMonsterArmor - a cry's armour cut is part of what the swing met
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

struct Attacker {
	bool known = false;
	std::string name;
	int toHit = 0;
	int level = 0;
	bool demon = false;
	bool undead = false;
	int minimumHit = 0;
};

struct Target {
	bool known = false;
	std::string name;
	int armor = 0;
	bool arrow = false;
	int distancePenalty = 0;
};

Attacker LastAttacker;
Target LastTarget;

/** @brief "Fallen One", or for a unique "Bishibosh (Fallen One)" - the name the player saw and the kind it is. */
std::string MonsterLabel(const Monster &monster)
{
	std::string label = GetMonsterDisplayName(monster); // a champion's own name, not the one it borrowed (round 11)
	if (monster.isUnique())
		label += " (" + std::string(pgettext("monster", monster.data().name)) + ")";
	return label;
}

} // namespace

void NoteAttacker(std::string name, int monsterToHit, int monsterLevel, bool demon, bool undead, int minimumHit)
{
	LastAttacker = { true, std::move(name), monsterToHit, monsterLevel, demon, undead, minimumHit };
}

void NoteTarget(std::string name, int monsterArmor, bool arrow, int distancePenalty)
{
	LastTarget = { true, std::move(name), monsterArmor, arrow, distancePenalty };
}

void NoteMonsterHitPlayer(const Player &player, const Monster &monster, int monsterToHit, int minimumHit)
{
	if (&player != MyPlayer)
		return;
	NoteAttacker(MonsterLabel(monster), monsterToHit, monster.level(sgGameInitInfo.nDifficulty),
	    monster.data().monsterClass == MonsterClass::Demon, monster.data().monsterClass == MonsterClass::Undead, minimumHit);
}

void NotePlayerAttackedMonster(const Player &player, const Monster &monster, bool arrow, int distancePenalty)
{
	if (&player != MyPlayer)
		return;
	NoteTarget(MonsterLabel(monster), EffectiveMonsterArmor(monster), arrow, distancePenalty);
}

bool ChanceToBeHit(const Player &player, int &chance, std::string &name)
{
	if (!LastAttacker.known)
		return false;
	// MonsterAttackPlayer's arithmetic, the monster's half as it was at the blow.
	int armor = player.GetArmor();
	if (LastAttacker.demon && HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::ACAgainstDemons))
		armor += 40;
	if (LastAttacker.undead && HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::ACAgainstUndead))
		armor += 20;
	const int hit = std::max(LastAttacker.toHit + 2 * (LastAttacker.level - player._pLevel) + 30 - armor, LastAttacker.minimumHit);
	const int lands = std::clamp(hit, 0, 100);
	// And what MonsterAttackPlayer does with a blow that lands (user, 2026-09-27: "fix all four" - the bar was the
	// landing chance alone, and overstated how often a shield user is hit): Dodge and Mantra of Evasion slip it, then
	// a shield blocks it. The hero is taken as standing, as he is while reading the sheet - Dodge, not Evade, and
	// the block roll that only a standing or attacking hero gets.
	const int slips = 100 - (100 - PassiveMeleeSlipChance(player, /*walking=*/false)) * (100 - Rfa12ActiveMeleeEvadeChance(player)) / 100;
	int blocks = 0;
	if (player._pBlockFlag)
		blocks = std::clamp(player.GetBlockChance() + Rfa12BlockBonus(player) + PassiveBlockBonus(player) - LastAttacker.level * 2, 0, 100);
	chance = lands * std::clamp(100 - slips, 0, 100) / 100 * (100 - blocks) / 100;
	name = LastAttacker.name;
	return true;
}

bool ChanceToHit(const Player &player, int &chance, std::string &name)
{
	if (!LastTarget.known)
		return false;
	int hit;
	if (LastTarget.arrow) {
		// MonsterMHit's arrow branch.
		hit = player.GetRangedPiercingToHit() - player.CalculateArmorPierce(LastTarget.armor, false) - LastTarget.distancePenalty;
	} else {
		// PlrHitMonst's. Zeal the way the sheet's To hit row reads it - on a button, at its rank - because standing
		// in the sheet there is no swing for the in-combat latch to describe.
		const bool bow = player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow;
		const int zeal = (!bow && IsZealReadied(player)) ? ZealToHitBonusAtRank(player) : 0;
		hit = player.GetMeleePiercingToHit() - player.CalculateArmorPierce(LastTarget.armor, true) + zeal;
	}
	chance = std::clamp(hit, 5, 95);
	name = LastTarget.name;
	return true;
}

void ClearCombatOdds()
{
	LastAttacker = {};
	LastTarget = {};
}

} // namespace devilution::oracool
