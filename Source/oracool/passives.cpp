#include "oracool/passives.h"

#include <algorithm>
#include <array>
#include <optional>

#include "engine/backbuffer_state.hpp"
#include "engine/random.hpp"
#include "misdat.h"
#include "missiles.h"
#include "monster.h"
#include "oracool/chill.h"
#include "oracool/melee_skills.h"
#include "oracool/rage.h"
#include "oracool/rfa12_actives.h"
#include "oracool/warcries.h"
#include "player.h"

namespace devilution::oracool {

namespace {

using Skill = ClassTreeSkill;

constexpr int TicksPerSecond = 20;

/** The clocks, per player index. */
struct Clocks {
	int stillTicks = 0;         // consecutive ticks not walking
	int walkTicks = 0;          // consecutive ticks walking - Momentum
	int rampageStacks = 0;      // Rampage: kills stacked
	int rampageTicks = 0;       // ...and ticks left before they fall off
	int cheatDeathCooldown = 0; // ticks until the next save is allowed
	int cadenceCount = 0;       // Cadence: melee blows since the last beat
	int inspireTicks = 0;       // Inspiring Presence: ticks under a warcry blessing, for the per-second mend
	int juggernautCooldown = 0; // Juggernaut: ticks until the next heal may fire
	int hasteTicks = 0;         // Illusionist, Tactical Advantage, Hot Pursuit: a burst of speed
	int hastePercent = 0;       // ...and how much
	int unharmedTicks = 0;      // Galvanizing Ward: ticks since the last blow taken
	int dominanceStacks = 0;    // Dominance: kills stacked
	int dominanceTicks = 0;     // ...and ticks left before they fall off
	int dynamoCharges = 0;      // Arcane Dynamo: cheap spells cast
	int dynamoTicks = 0;        // ...and ticks the charged spell damage lasts
	int sharpshooterTicks = 0;  // Sharpshooter: ticks since the last critical blow
	int counterTicks = 0;       // Counterstroke: ticks the empowered blow waits
	int counterPercent = 0;     // ...and its bonus
	int momentumBlows = 0;      // Momentum: empowered blows left
	int mythicCount = 0;        // Mythic Rhythm: melee skill blows since the last charge
	int mythicTicks = 0;        // ...and ticks the charged spell damage lasts
	ClassTreeSkill heldAura = ClassTreeSkill::None; // Bard songs: the song being held
	int heldAuraTicks = 0;      // ...and for how long
	int crescendoStacks = 0;    // Crescendo: blows landed under this song
	std::array<SpellID, 3> comboSpells { SpellID::Invalid, SpellID::Invalid, SpellID::Invalid }; // Combination Strike
	std::array<int, 3> comboTicks {};
	int arrowsLoosed = 0;       // Grenadier: arrows since the last grenade
};

std::array<Clocks, MAX_PLRS> ClocksOf;

Clocks &ClocksFor(const Player &player)
{
	return ClocksOf[player.getId()];
}

/** Per monster: which elements struck it lately - Conflagration, Elemental Exposure. */
constexpr size_t ElementCount = 5; // physical, fire, lightning, magic, cold
struct MonsterMarks {
	std::array<int, ElementCount> elementTicks {};
};
std::array<MonsterMarks, MaxMonsters> MarksOf;

constexpr int StillnessTicks = 30;      // a moment: a second and a half
constexpr int RampageHoldTicks = 100;   // five seconds
constexpr int RampageMaxStacks = 5;
constexpr int CheatDeathCooldownTicks = 1200; // a minute
constexpr int JuggernautCooldownTicks = 200;  // ten seconds
constexpr int CheapSpellMana = 6 << 6;        // Prodigy and Arcane Dynamo: "simple" spells

std::optional<size_t> ElementIndex(DamageType type)
{
	switch (type) {
	case DamageType::Physical: return 0;
	case DamageType::Fire: return 1;
	case DamageType::Lightning: return 2;
	case DamageType::Magic: return 3;
	case DamageType::Cold: return 4;
	default: return std::nullopt;
	}
}

/** @brief The marks of @p monster, or nullptr for a monster that is not in the Monsters table (a test's local one). */
MonsterMarks *MarksFor(const Monster &monster)
{
	if (&monster < &Monsters[0] || &monster >= &Monsters[0] + MaxMonsters)
		return nullptr;
	return &MarksOf[monster.getId()];
}

void MarkElement(const Monster &monster, DamageType type, int ticks)
{
	MonsterMarks *marks = MarksFor(monster);
	if (marks == nullptr)
		return;
	if (const std::optional<size_t> e = ElementIndex(type))
		marks->elementTicks[*e] = std::max(marks->elementTicks[*e], ticks);
}

bool HoldsType(const Player &player, ItemType type)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (!item.isEmpty() && item._iStatFlag && item._itype == type)
			return true;
	}
	return false;
}

bool WearingShield(const Player &player)
{
	return HoldsType(player, ItemType::Shield);
}

bool WieldingMace(const Player &player)
{
	return HoldsType(player, ItemType::Mace);
}

/** @brief A weapon in each hand - The Guardian's Path. */
bool DualWielding(const Player &player)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (item.isEmpty() || !item._iStatFlag || item._iClass != ICLASS_WEAPON || item._itype == ItemType::Shield)
			return false;
	}
	return true;
}

std::optional<unique_base_item> BowBase(const Player &player)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (item.isEmpty() || !item._iStatFlag || item._itype != ItemType::Bow || item.IDidx < 0 || item.IDidx > IDI_LAST)
			continue;
		return AllItemsList[static_cast<size_t>(item.IDidx)].iItemId;
	}
	return std::nullopt;
}

bool Still(const Player &player)
{
	return ClocksFor(player).stillTicks >= StillnessTicks;
}

bool BelowAThird(const Player &player)
{
	return player._pHitPoints * 3 < player._pMaxHP;
}

bool Stunned(const Monster &monster)
{
	return monster.mode == MonsterMode::Delay;
}

/** @brief Whether @p player is playing a song or holding any aura. */
bool SongPlaying(const Player &player)
{
	return GetActiveClassAura(player) != ClassTreeSkill::None;
}

/** @brief Monsters that can be hit within @p range tiles of @p centre, not counting @p except. */
int MonstersNear(Point centre, int range, const Monster *except)
{
	int count = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &other = Monsters[ActiveMonsters[i]];
		if (&other == except || !other.isPossibleToHit() || other.hitPoints >> 6 <= 0)
			continue;
		if (centre.WalkingDistance(other.position.tile) <= range)
			count++;
	}
	return count;
}

/** @brief The player's own summons within @p range tiles - Chorus and Unity. */
int MinionsNear(Point centre, int range)
{
	int count = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &other = Monsters[ActiveMonsters[i]];
		if (other.isPlayerMinion() && other.hitPoints >> 6 > 0 && centre.WalkingDistance(other.position.tile) <= range)
			count++;
	}
	return count;
}

/** @brief The Rogue's three slips share one curve: a tenth, a twenty-fifth more a rank, two fifths at most. */
int SlipChance(const Player &player, Skill skill)
{
	if (!PassiveActive(player, skill))
		return 0;
	const int points = ClassTreeInvestment(player, skill);
	return std::min(10 + 4 * (points - 1), 40);
}

void Heal(Player &player, int amount)
{
	if (amount <= 0 || player._pHitPoints <= 0 || player._pHitPoints >= player._pMaxHP)
		return;
	player._pHitPoints = std::min(player._pHitPoints + amount, player._pMaxHP);
	player._pHPBase = std::min(player._pHPBase + amount, player._pMaxHPBase);
	RedrawComponent(PanelDrawComponent::Health);
}

void RestoreMana(Player &player, int amount)
{
	if (amount <= 0 || player._pMana >= player._pMaxMana || HasAnyOf(player._pIFlags, ItemSpecialEffect::NoMana))
		return;
	player._pMana = std::min(player._pMana + amount, player._pMaxMana);
	player._pManaBase = std::min(player._pManaBase + amount, player._pMaxManaBase);
	RedrawComponent(PanelDrawComponent::Mana);
}

void Haste(Clocks &clocks, int percent, int ticks)
{
	clocks.hastePercent = clocks.hasteTicks > 0 ? std::max(clocks.hastePercent, percent) : percent;
	clocks.hasteTicks = std::max(clocks.hasteTicks, ticks);
}

std::optional<SpellID> ArmedMeleeSpell()
{
	if (const std::optional<ClassMeleeSkill> skill = ArmedClassMeleeSkill())
		return ClassMeleeSkillSpell(*skill);
	return ArmedRfa12Melee();
}

/** @brief What any landed blow does, melee or missile. */
void OnAnyHit(Player &player, const Monster &target, int damage, bool melee)
{
	Clocks &clocks = ClocksFor(player);
	if (PassiveActive(player, Skill::HotPursuit))
		Haste(clocks, 20, 2 * TicksPerSecond);
	if (SongPlaying(player) && PassiveActive(player, Skill::Crescendo))
		clocks.crescendoStacks = std::min(clocks.crescendoStacks + 1, 15);
	if (clocks.momentumBlows > 0)
		clocks.momentumBlows--;
	if (damage > 0 && PassiveActive(player, Skill::Resolve))
		DebuffMonster(target, 3 * TicksPerSecond, -20, 0);
	if (melee)
		MarkElement(target, DamageType::Physical, 5 * TicksPerSecond);
}

} // namespace

bool PassiveActive(const Player &player, Skill skill)
{
	if (skill == Skill::None || skill > Skill::LAST)
		return false;
	const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
	if (!data.implemented || data.heroClass != player._pClass)
		return false;
	if (!IsClassTreeSkillUnlocked(player, skill))
		return false;
	if (IsPassiveSkillRow(skill))
		return PassiveSlotOf(player, skill) >= 0;
	return ClassTreeInvestment(player, skill) > 0;
}

int PassiveDamageTakenPercent(const Player &player, DamageType damageType)
{
	int percent = 0;
	const bool notSteel = damageType != DamageType::Physical;
	const Clocks &clocks = ClocksFor(player);
	if (PassiveActive(player, Skill::Blur))
		percent -= 17;
	if (notSteel && PassiveActive(player, Skill::SixthSense))
		percent -= 25;
	if (notSteel && PassiveActive(player, Skill::Vigilant))
		percent -= 20;
	if (WearingShield(player) && PassiveActive(player, Skill::SwordAndBoard))
		percent -= 30;
	if (BelowAThird(player) && PassiveActive(player, Skill::Relentless))
		percent -= 25;
	if (Still(player) && PassiveActive(player, Skill::UnwaveringWill))
		percent -= 20;
	// 2026-09-14 sweep.
	if (clocks.unharmedTicks >= 5 * TicksPerSecond && PassiveActive(player, Skill::GalvanizingWard))
		percent -= 50;
	if (clocks.dominanceTicks > 0 && PassiveActive(player, Skill::Dominance))
		percent -= 4 * clocks.dominanceStacks;
	if (clocks.heldAuraTicks >= 30 * TicksPerSecond && PassiveActive(player, Skill::MagnumOpus))
		percent -= 10;
	// Whatever stacks, a blow always lands: a quarter of it at the least.
	return std::max(percent, -75);
}

int PassiveDamageDealtPercent(const Player &player, const Monster &target, bool melee)
{
	int percent = 0;
	const int targetLife = target.hitPoints;
	const int targetMax = std::max(target.maxHitPoints, 1);
	const int distance = player.position.tile.WalkingDistance(target.position.tile);
	const bool chilled = IsMonsterChilled(target) || IsMonsterFrozen(target);
	Clocks &clocks = ClocksFor(player);

	if (PassiveActive(player, Skill::Ruthless) && targetLife * 3 < targetMax)
		percent += 40;
	if (PassiveActive(player, Skill::Ambush) && targetLife * 4 >= targetMax * 3)
		percent += 40;
	if (PassiveActive(player, Skill::Brawler) && MonstersNear(player.position.tile, 1, nullptr) >= 3)
		percent += 20;
	if (PassiveActive(player, Skill::Determination))
		percent += std::min(MonstersNear(player.position.tile, 1, nullptr), 4) * 5;
	if (PassiveActive(player, Skill::SteadyAim) && MonstersNear(player.position.tile, 3, nullptr) == 0)
		percent += 20;
	if (PassiveActive(player, Skill::Audacity) && distance <= 2)
		percent += 15;
	if (PassiveActive(player, Skill::PowerHungry) && distance >= 5)
		percent += 20;
	if (PassiveActive(player, Skill::ColdBlooded) && chilled)
		percent += 10;
	if (PassiveActive(player, Skill::CullTheWeak) && chilled)
		percent += 20;
	if (PassiveActive(player, Skill::RelentlessAssault) && (IsMonsterFrozen(target) || Stunned(target)))
		percent += 30;
	if (PassiveActive(player, Skill::SingleOut) && MonstersNear(target.position.tile, 2, &target) == 0)
		percent += 25;
	if (PassiveActive(player, Skill::Rampage))
		percent += clocks.rampageStacks * 5;
	if (PassiveActive(player, Skill::UnwaveringWill) && Still(player))
		percent += 10;
	// The beat: the third blow since the last one. Counted in OnPassiveHit, read here, so the
	// blow that IS the beat carries the bonus and the count restarts after it lands.
	if (melee && PassiveActive(player, Skill::Cadence) && clocks.cadenceCount == 2)
		percent += 50;
	// The Barbarian's (2026-09-14). Berserker Rage reads the pool at the moment of the blow.
	if (PassiveActive(player, Skill::BerserkerRage) && UsesRage(player) && player._pRage * 2 >= MaxRage(player))
		percent += 25;
	if (PassiveActive(player, Skill::NoEscape) && distance >= 5)
		percent += 25;

	// ---- the all-heroes sweep (2026-09-14) ----
	static const MonsterMarks NoMarks {};
	const MonsterMarks *found = MarksFor(target);
	const MonsterMarks &marks = found != nullptr ? *found : NoMarks;
	if (PassiveActive(player, Skill::Conflagration) && marks.elementTicks[1] > 0)
		percent += 10;
	if (PassiveActive(player, Skill::ElementalExposure)) {
		for (const int ticks : marks.elementTicks) {
			if (ticks > 0)
				percent += 5;
		}
	}
	if (!melee && clocks.dynamoTicks > 0 && PassiveActive(player, Skill::ArcaneDynamo))
		percent += 60;
	if (!melee && clocks.mythicTicks > 0 && PassiveActive(player, Skill::MythicRhythm))
		percent += 40;
	if (PassiveActive(player, Skill::Sharpshooter)) {
		// Four points of chance a second without one; a critical blow doubles and starts the count again.
		const int chance = std::min(4 * clocks.sharpshooterTicks / TicksPerSecond, 100);
		if (chance > 0 && GenerateRnd(100) < chance) {
			percent += 100;
			clocks.sharpshooterTicks = 0;
		}
	}
	if (SongPlaying(player)) {
		if (PassiveActive(player, Skill::PerfectPitch))
			percent += std::min(2 * (clocks.heldAuraTicks / (5 * TicksPerSecond)), 20);
		if (PassiveActive(player, Skill::Crescendo))
			percent += clocks.crescendoStacks;
		if (clocks.heldAuraTicks >= 30 * TicksPerSecond && PassiveActive(player, Skill::MagnumOpus))
			percent += 15;
	}
	if (PassiveActive(player, Skill::Chorus) || PassiveActive(player, Skill::Unity))
		percent += std::min(MinionsNear(player.position.tile, 5), 3) * 10;
	if (melee && clocks.counterTicks > 0 && PassiveActive(player, Skill::Counterstroke))
		percent += clocks.counterPercent;
	if (PassiveActive(player, Skill::SeizeTheInitiative) && targetLife >= targetMax)
		percent += 30;
	if (melee && PassiveActive(player, Skill::CombinationStrike)) {
		int distinct = 0;
		for (size_t i = 0; i < clocks.comboSpells.size(); i++) {
			if (clocks.comboTicks[i] > 0 && clocks.comboSpells[i] != SpellID::Invalid)
				distinct++;
		}
		percent += 10 * distinct;
	}
	if (clocks.momentumBlows > 0 && PassiveActive(player, Skill::Momentum))
		percent += 25;
	// The replacements for the rows the engine could not carry (2026-09-14).
	if (PassiveActive(player, Skill::Sanctified) && IsAnyOf(target.data().monsterClass, MonsterClass::Undead, MonsterClass::Demon))
		percent += 20;
	if (!melee && player._pMana * 2 > player._pMaxMana && PassiveActive(player, Skill::ManaAttunement))
		percent += 15;
	// Throwing Mastery (Barbarian, 2026-09-14): a missile from a Barbarian with no bow is a thrown weapon.
	if (!melee && player._pClass == HeroClass::Barbarian && !player.UsesRangedWeapon() && PassiveActive(player, Skill::ThrowingMastery))
		percent += 10 + 6 * (ClassTreeInvestment(player, Skill::ThrowingMastery) - 1);
	return percent;
}

bool PassiveEvadesMelee(const Player &player)
{
	const bool walking = IsAnyOf(player._pmode, PM_WALK_NORTHWARDS, PM_WALK_SOUTHWARDS, PM_WALK_SIDEWAYS);
	int chance = SlipChance(player, walking ? Skill::Evade : Skill::Dodge);
	if (DualWielding(player) && PassiveActive(player, Skill::TheGuardiansPath))
		chance += 15;
	const bool slipped = chance > 0 && GenerateRnd(100) < chance;
	if (slipped && PassiveActive(player, Skill::TacticalAdvantage))
		Haste(ClocksFor(player), 40, 3 * TicksPerSecond);
	return slipped;
}

bool PassiveEvadesMissile(const Player &player)
{
	const int chance = SlipChance(player, Skill::Avoid);
	const bool slipped = chance > 0 && GenerateRnd(100) < chance;
	if (slipped && PassiveActive(player, Skill::TacticalAdvantage))
		Haste(ClocksFor(player), 40, 3 * TicksPerSecond);
	return slipped;
}

bool ArrowPierces(Missile &missile)
{
	if (missile.sourceType() != MissileSource::Player || !GetMissileData(missile._mitype).isArrow())
		return false;
	const Player &player = *missile.sourcePlayer();
	if (!PassiveActive(player, Skill::Pierce))
		return false;
	const int points = ClassTreeInvestment(player, Skill::Pierce);
	const int chance = std::min(15 + 5 * (points - 1), 60);
	return GenerateRnd(100) < chance;
}

bool PassiveCheatsDeath(Player &player)
{
	Clocks &clocks = ClocksFor(player);
	if (clocks.cheatDeathCooldown > 0)
		return false;
	const bool nearDeath = PassiveActive(player, Skill::NearDeathExperience);
	const bool anomaly = PassiveActive(player, Skill::UnstableAnomaly);
	if (!nearDeath && !anomaly && !PassiveActive(player, Skill::Indestructible) && !PassiveActive(player, Skill::NervesOfSteel)
	    && !PassiveActive(player, Skill::Awareness))
		return false;

	SetPlayerHitPoints(player, player._pMaxHP / 3);
	if (nearDeath) {
		player._pMana = player._pMaxMana / 3;
		player._pManaBase = player._pMaxManaBase - (player._pMaxMana - player._pMana);
		RedrawComponent(PanelDrawComponent::Mana);
	}
	// Unstable Anomaly: the save throws back everything that stood close.
	if (anomaly) {
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			Monster &other = Monsters[ActiveMonsters[i]];
			if (!other.isPlayerMinion() && other.hitPoints >> 6 > 0 && player.position.tile.WalkingDistance(other.position.tile) <= 2)
				M_GetKnockback(other);
		}
	}
	clocks.cheatDeathCooldown = CheatDeathCooldownTicks;
	return true;
}

void OnPassiveHit(Player &player, const Monster &target, int damage, bool melee)
{
	Clocks &clocks = ClocksFor(player);
	if (damage > 0 && PassiveActive(player, Skill::Leech))
		Heal(player, damage * 3 / 100);
	if (melee && PassiveActive(player, Skill::Cadence))
		clocks.cadenceCount = (clocks.cadenceCount + 1) % 3;
	// Weapons Master's mace: a point of Rage for every blow that lands, with or without a skill.
	if (melee && PassiveActive(player, Skill::WeaponsMaster) && WieldingMace(player))
		GainRage(player, 1);
	if (melee && PassiveActive(player, Skill::Righteousness))
		RestoreMana(player, 1 << 6);
	if (!melee && PassiveActive(player, Skill::NightStalker))
		RestoreMana(player, 1 << 6);
	if (melee) {
		// Counterstroke's empowered blow has landed.
		clocks.counterTicks = 0;
		if (const std::optional<SpellID> spell = ArmedMeleeSpell(); spell.has_value()) {
			if (PassiveActive(player, Skill::CombinationStrike)) {
				size_t slot = 0;
				for (size_t i = 0; i < clocks.comboSpells.size(); i++) {
					if (clocks.comboSpells[i] == *spell) {
						slot = i;
						break;
					}
					if (clocks.comboTicks[i] < clocks.comboTicks[slot])
						slot = i;
				}
				clocks.comboSpells[slot] = *spell;
				clocks.comboTicks[slot] = 3 * TicksPerSecond;
			}
			if (PassiveActive(player, Skill::MythicRhythm) && ++clocks.mythicCount >= 3) {
				clocks.mythicCount = 0;
				clocks.mythicTicks = 3 * TicksPerSecond;
			}
		}
	}
	OnAnyHit(player, target, damage, melee);
}

void OnPassiveMissileHit(Player &player, const Monster &target, int damage, DamageType damageType, bool arrow)
{
	if (damage <= 0)
		return;
	MarkElement(target, damageType, damageType == DamageType::Fire ? 3 * TicksPerSecond : 5 * TicksPerSecond);
	if (MarksFor(target) == nullptr)
		return; // not a monster of the level - nothing to stun or slow
	Monster &monster = Monsters[target.getId()];
	const bool alive = monster.hitPoints >> 6 > 0;
	if (alive && damageType == DamageType::Lightning && PassiveActive(player, Skill::Paralysis) && !monster.isUnique()
	    && GenerateRnd(100) < 15)
		StunMonster(monster, TicksPerSecond);
	if (alive && damageType == DamageType::Magic && PassiveActive(player, Skill::TemporalFlux))
		ChillMonster(monster, 2 * TicksPerSecond);
	if (arrow) {
		if (alive && PassiveActive(player, Skill::ThrillOfTheHunt) && GenerateRnd(100) < 20)
			ChillMonster(monster, 2 * TicksPerSecond);
		if (PassiveActive(player, Skill::Archery)) {
			const std::optional<unique_base_item> bow = BowBase(player);
			if (bow && (*bow == UITYPE_COMPBOW || *bow == UITYPE_BATTLEBOW))
				RestoreMana(player, 1 << 6);
		}
	} else {
		// Arrows already reached OnPassiveHit; the spells reach the shared rules here.
		OnAnyHit(player, target, damage, false);
	}
}

void OnPassiveManaSpent(Player &player, int cost)
{
	if (cost <= 0)
		return;
	if (PassiveActive(player, Skill::Bloodthirst) || PassiveActive(player, Skill::Transcendence))
		Heal(player, cost / 2);
	if (PassiveActive(player, Skill::Wrathful))
		Heal(player, cost * 30 / 100);
	const bool cheap = cost <= CheapSpellMana;
	if (cheap && PassiveActive(player, Skill::Prodigy))
		RestoreMana(player, 3 << 6);
	if (PassiveActive(player, Skill::ArcaneDynamo)) {
		Clocks &clocks = ClocksFor(player);
		if (cheap) {
			clocks.dynamoCharges = std::min(clocks.dynamoCharges + 1, 5);
		} else if (clocks.dynamoCharges >= 5) {
			clocks.dynamoCharges = 0;
			clocks.dynamoTicks = 3 * TicksPerSecond;
		}
	}
}

void OnPassiveMonsterKilled(Player &player, const Monster &monster)
{
	Clocks &clocks = ClocksFor(player);
	if (PassiveActive(player, Skill::Rampage)) {
		clocks.rampageStacks = std::min(clocks.rampageStacks + 1, RampageMaxStacks);
		clocks.rampageTicks = RampageHoldTicks;
	}
	if (PassiveActive(player, Skill::Requiem) && player.position.tile.WalkingDistance(monster.position.tile) <= 4)
		Heal(player, player._pMaxHP / 50);
	if (PassiveActive(player, Skill::PoundOfFlesh))
		Heal(player, player._pMaxHP * 3 / 100);
	if (PassiveActive(player, Skill::Dominance)) {
		clocks.dominanceStacks = std::min(clocks.dominanceStacks + 1, 5);
		clocks.dominanceTicks = 5 * TicksPerSecond;
	}
	if (PassiveActive(player, Skill::BloodVengeance))
		RestoreMana(player, 3 << 6);
}

void OnPassiveBlock(Player &player)
{
	if (PassiveActive(player, Skill::Insurmountable))
		RestoreMana(player, 5 << 6);
	if (PassiveActive(player, Skill::Renewal))
		Heal(player, player._pMaxHP * 3 / 100);
	if (PassiveActive(player, Skill::Counterstroke)) {
		Clocks &clocks = ClocksFor(player);
		clocks.counterTicks = 3 * TicksPerSecond;
		clocks.counterPercent = 30 + 10 * (ClassTreeInvestment(player, Skill::Counterstroke) - 1);
	}
}

void OnPassiveDamaged(Player &player, int damage)
{
	if (damage <= 0)
		return;
	Clocks &clocks = ClocksFor(player);
	clocks.unharmedTicks = 0;
	if (damage * 100 >= player._pMaxHP * 15 && PassiveActive(player, Skill::Illusionist))
		Haste(clocks, 50, 3 * TicksPerSecond);
}

int PassiveBlockBonus(const Player &player)
{
	int bonus = 0;
	if (PassiveActive(player, Skill::HoldYourGround))
		bonus += 20;
	if (HoldsType(player, ItemType::Staff) && PassiveActive(player, Skill::ReedInTheWind))
		bonus += 10 + 2 * (ClassTreeInvestment(player, Skill::ReedInTheWind) - 1);
	return bonus;
}

int PassiveThornsPercent(const Player &player)
{
	return PassiveActive(player, Skill::IronMaiden) ? 50 : 0;
}

int PassiveMoveSpeedBonus(const Player &player)
{
	const Clocks &clocks = ClocksFor(player);
	int bonus = clocks.hasteTicks > 0 ? clocks.hastePercent : 0;
	// Crusader's Stride (2026-09-14): while an aura burns.
	if (SongPlaying(player) && PassiveActive(player, Skill::CrusadersStride))
		bonus += 15;
	return bonus;
}

int PassiveMonsterDamagePercent(const Monster &monster)
{
	if (MyPlayer == nullptr)
		return 0;
	const Player &player = *MyPlayer;
	int percent = 0;
	if ((IsMonsterChilled(monster) || IsMonsterFrozen(monster)) && PassiveActive(player, Skill::NumbingTraps))
		percent -= 25;
	if (SongPlaying(player) && PassiveActive(player, Skill::Dissonance)
	    && player.position.tile.WalkingDistance(monster.position.tile) <= 3)
		percent -= 20;
	return percent;
}

int PassiveManaCostPercent(const Player &player, SpellID spell)
{
	if (IsAnyOf(spell, SpellID::MantraOfClarity, SpellID::MantraOfEvasion, SpellID::MantraOfRetribution)
	    && PassiveActive(player, Skill::ChantOfResonance))
		return -50;
	return 0;
}

int PassiveSkillDamagePercent(const Player &player, SpellID spell)
{
	int percent = 0;
	if (spell == SpellID::BlessedHammer && PassiveActive(player, Skill::Blunt))
		percent += 25;
	if (IsAnyOf(spell, SpellID::BlessedShield, SpellID::ShieldBash) && PassiveActive(player, Skill::ToweringShield))
		percent += 25;
	return percent;
}

bool PassiveShrugsOffStagger(Player &player)
{
	if (SongPlaying(player) && PassiveActive(player, Skill::Stagecraft))
		return true;
	if (!PassiveActive(player, Skill::Juggernaut))
		return false;
	if (GenerateRnd(100) < 50)
		return true;
	// The stagger lands - and sometimes gives something back.
	Clocks &clocks = ClocksFor(player);
	if (clocks.juggernautCooldown == 0 && GenerateRnd(100) < 30) {
		Heal(player, player._pMaxHP / 5);
		clocks.juggernautCooldown = JuggernautCooldownTicks;
	}
	return false;
}

int PassiveSlowShortenPercent(const Player &player)
{
	return PassiveActive(player, Skill::Juggernaut) ? 50 : 0;
}

int PassiveRuneLevelBonus(const Player &player)
{
	return PassiveActive(player, Skill::CustomEngineering) ? 3 : 0;
}

bool PassiveSparesRune(const Player &player)
{
	return PassiveActive(player, Skill::CustomEngineering) && GenerateRnd(2) == 0;
}

void OnPassiveArrowLoosed(Player &player, Point target)
{
	if (!PassiveActive(player, Skill::Grenadier))
		return;
	if (++ClocksFor(player).arrowsLoosed % 4 != 0)
		return;
	// The grenade: the engine's Fireball, lobbed after the arrow at a quarter of the Rogue's level, wearing RfA-16's
	// tumbling clay bomb in flight. Its burst is still the Fireball's own explosion (ProcessFireball swaps to it).
	const Point from = player.position.tile;
	const Point dst = target == from ? from + player._pdir : target;
	Missile *grenade = AddMissile(from, dst, GetDirection(from, dst), MissileID::Fireball, TARGET_MONSTERS,
	    static_cast<int>(player.getId()), 0, std::max(player._pLevel / 4, 1));
	if (grenade != nullptr && MissileArtLoaded(MissileGraphicID::Grenade))
		UseMissileGraphic(*grenade, MissileGraphicID::Grenade);
}

int PassiveWarcryDurationPercent(const Player &player)
{
	return PassiveActive(player, Skill::InspiringPresence) ? 200 : 100;
}

bool PassiveRunActive(const Player &player)
{
	return PassiveActive(player, Skill::FleetFooted);
}

void ProcessPassivesTick(Player &player)
{
	Clocks &clocks = ClocksFor(player);
	const bool walking = IsAnyOf(player._pmode, PM_WALK_NORTHWARDS, PM_WALK_SOUTHWARDS, PM_WALK_SIDEWAYS);
	clocks.stillTicks = walking ? 0 : std::min(clocks.stillTicks + 1, 1 << 20);
	if (clocks.rampageTicks > 0 && --clocks.rampageTicks == 0)
		clocks.rampageStacks = 0;
	if (clocks.cheatDeathCooldown > 0)
		clocks.cheatDeathCooldown--;
	if (clocks.juggernautCooldown > 0)
		clocks.juggernautCooldown--;
	// Brooding: still for a moment, and then a hundredth of your life every second.
	if (player._pHitPoints > 0 && Still(player) && PassiveActive(player, Skill::Brooding)
	    && (clocks.stillTicks - StillnessTicks) % 20 == 0)
		Heal(player, player._pMaxHP / 100);
	// Serene Mind (2026-09-14): still for a moment, and then a fiftieth of your mana every second.
	if (player._pHitPoints > 0 && Still(player) && PassiveActive(player, Skill::SereneMind)
	    && (clocks.stillTicks - StillnessTicks) % 20 == 0)
		RestoreMana(player, player._pMaxMana / 50);
	// Inspiring Presence: under any warcry blessing, a hundredth of your life every second.
	if (player._pHitPoints > 0 && PassiveActive(player, Skill::InspiringPresence) && AnyWarcryBuffActive(player)) {
		if (++clocks.inspireTicks % 20 == 0)
			Heal(player, player._pMaxHP / 100);
	} else {
		clocks.inspireTicks = 0;
	}

	// ---- the all-heroes sweep (2026-09-14) ----
	if (clocks.hasteTicks > 0 && --clocks.hasteTicks == 0)
		clocks.hastePercent = 0;
	clocks.unharmedTicks = std::min(clocks.unharmedTicks + 1, 1 << 20);
	if (clocks.dominanceTicks > 0 && --clocks.dominanceTicks == 0)
		clocks.dominanceStacks = 0;
	if (clocks.dynamoTicks > 0)
		clocks.dynamoTicks--;
	if (clocks.mythicTicks > 0)
		clocks.mythicTicks--;
	if (clocks.counterTicks > 0)
		clocks.counterTicks--;
	for (int &ticks : clocks.comboTicks) {
		if (ticks > 0)
			ticks--;
	}
	clocks.sharpshooterTicks = std::min(clocks.sharpshooterTicks + 1, 1 << 20);
	// Momentum: two seconds on the move charge the next three blows.
	clocks.walkTicks = walking ? std::min(clocks.walkTicks + 1, 1 << 20) : 0;
	if (clocks.walkTicks >= 2 * TicksPerSecond && PassiveActive(player, Skill::Momentum))
		clocks.momentumBlows = 3;
	// The song being held, and for how long - Perfect Pitch, Crescendo, Magnum Opus.
	const ClassTreeSkill aura = GetActiveClassAura(player);
	if (aura != clocks.heldAura) {
		clocks.heldAura = aura;
		clocks.heldAuraTicks = 0;
		clocks.crescendoStacks = 0;
	} else if (aura != ClassTreeSkill::None) {
		clocks.heldAuraTicks = std::min(clocks.heldAuraTicks + 1, 1 << 20);
	}
	// The monsters' marks run down, once, from the local player's tick.
	if (&player == MyPlayer) {
		for (MonsterMarks &marks : MarksOf) {
			for (int &ticks : marks.elementTicks) {
				if (ticks > 0)
					ticks--;
			}
		}
	}
}

void ClearPassiveState()
{
	ClocksOf.fill(Clocks {});
	MarksOf.fill(MonsterMarks {});
}

void ClearPassiveClocks(Player &player)
{
	ClocksFor(player) = Clocks {};
}

} // namespace devilution::oracool
