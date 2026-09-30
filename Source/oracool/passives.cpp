#include "oracool/passives.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "engine/backbuffer_state.hpp"
#include "engine/random.hpp"
#include "misdat.h"
#include "missiles.h"
#include "monster.h"
#include "oracool/chill.h"
#include "oracool/companion.h" // IsCompanion - an ally is not a monster near
#include "oracool/melee_skills.h"
#include "oracool/curses.h"
#include "oracool/essence.h"
#include "oracool/minions.h"
#include "oracool/rage.h"
#include "oracool/rfa12_actives.h"
#include "oracool/warcries.h"
#include "oracool/whirlwind.h" // IsWhirlwinding - Weapons Master pays no Rage for spin blows
#include "player.h"
#include "utils/language.h"

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
	// The Necromancer (2026-09-18, phase N8).
	int rathmaTicks = 0;        // Rathma's Shield: ticks of nothing-can-harm-you left
	int rathmaLevel = -1;       // ...and the floor it was spent on (once a floor)
	int finalServiceLevel = -1; // Final Service: the floor it was spent on
	int bloodLost = 0;          // Blood is Power: life lost since the last point of Essence, 1/64 units
	int drawLifeTicks = 0;      // Draw Life: the second's clock
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

// ---- the rules' numbers (2026-09-26). Each is read by its rule below AND by PassiveFactsAt, so the
//      tooltip cannot quote a number the rule does not use. Life and mana amounts are 1/64 units;
//      "divisor" constants are the share of max life/mana the rule takes (3 = a third).
constexpr int FarTiles = 5;                   // Power Hungry, No Escape: "five tiles or further"
constexpr int MarkTicks = 5 * TicksPerSecond;  // an element's mark on a monster...
constexpr int FireMarkTicks = 3 * TicksPerSecond; // ...fire's is shorter
// Damage taken.
constexpr int StandAlonePercent = 15;
constexpr int StandAlonePerMinion = 3;
constexpr int BlurPercent = 17;
constexpr int SixthSensePercent = 25;
constexpr int VigilantPercent = 20;
constexpr int SwordAndBoardPercent = 30;
constexpr int RelentlessPercent = 25;
constexpr int UnwaveringWillTakenPercent = 20;
constexpr int UnwaveringWillDealtPercent = 10;
constexpr int GalvanizingWardTicks = 5 * TicksPerSecond;
constexpr int GalvanizingWardPercent = 50;
constexpr int DominancePerStack = 4;
constexpr int DominanceMaxStacks = 5;
constexpr int DominanceHoldTicks = 5 * TicksPerSecond;
// Damage dealt.
constexpr int SpreadingMaledictionPerCurse = 5;
constexpr int SpreadingMaledictionRange = 6;
constexpr int SpreadingMaledictionMax = 30;
constexpr int LowLifeDivisor = 3;             // Ruthless, Relentless: "below a third of life"
constexpr int RuthlessPercent = 40;
constexpr int AmbushPercent = 40;
constexpr int AmbushLifePercent = 75;         // the target at this share of its life or more
constexpr int BrawlerPercent = 20;
constexpr int BrawlerCrowd = 3;               // enemies adjacent
constexpr int DeterminationPerEnemy = 5;
constexpr int DeterminationMaxEnemies = 4;
constexpr int SteadyAimPercent = 20;
constexpr int SteadyAimRange = 3;
constexpr int AudacityPercent = 15;
constexpr int GlassCannonPercent = 15; // Glass Cannon: +15% damage, every source (its -10% armour is on the sheet)
constexpr int AudacityRange = 2;
constexpr int PowerHungryPercent = 20;
constexpr int ColdBloodedPercent = 10;
constexpr int CullTheWeakPercent = 20;
constexpr int RelentlessAssaultPercent = 30;
constexpr int SingleOutPercent = 25;
constexpr int SingleOutRange = 2;
constexpr int RampagePerStack = 5;
constexpr int BerserkerRagePercent = 25;
constexpr int NoEscapePercent = 25;
constexpr int ConflagrationPercent = 10;
constexpr int ElementalExposurePerElement = 5;
constexpr int ArcaneDynamoPercent = 60;
constexpr int ArcaneDynamoCharges = 5;
constexpr int ArcaneDynamoTicks = 3 * TicksPerSecond;
constexpr int MythicRhythmPercent = 40;
constexpr int MythicRhythmBlows = 3;
constexpr int MythicRhythmTicks = 3 * TicksPerSecond;
constexpr int SharpshooterPerSecond = 4;
constexpr int SharpshooterCritPercent = 100;
constexpr int AllyPercent = 10;               // Chorus, Unity: per minion near...
constexpr int AllyMaxCount = 3;               // ...counted up to three...
constexpr int AllyRange = 5;                  // ...within five tiles
constexpr int SeizeTheInitiativePercent = 30;
constexpr int CombinationStrikePerSkill = 10;
constexpr int CombinationStrikeTicks = 3 * TicksPerSecond;
constexpr int MomentumPercent = 25;
constexpr int MomentumBlows = 3;
constexpr int MomentumWalkTicks = 2 * TicksPerSecond;
constexpr int SanctifiedPercent = 20;
constexpr int ManaAttunementPercent = 15;
// On a landed blow, a missile, a block, a kill, a spend.
constexpr int LeechPercent = 3;
constexpr int DarkReapingMana = 1 << 6;
constexpr int DarkReapingEssence = 1;
constexpr int WeaponsMasterRage = 1;
constexpr int RighteousnessMana = 1 << 6;
constexpr int NightStalkerMana = 1 << 6;
constexpr int ArcheryMana = 1 << 6;
constexpr int ParalysisChance = 15;
constexpr int ParalysisTicks = TicksPerSecond;
constexpr int TemporalFluxTicks = 2 * TicksPerSecond;
constexpr int ThrillOfTheHuntChance = 20;
constexpr int ThrillOfTheHuntTicks = 2 * TicksPerSecond;
constexpr int ResolvePercent = 20;
constexpr int ResolveTicks = 3 * TicksPerSecond;
constexpr int HotPursuitPercent = 20;
constexpr int HotPursuitTicks = 2 * TicksPerSecond;
constexpr int TacticalAdvantagePercent = 40;
constexpr int TacticalAdvantageTicks = 3 * TicksPerSecond;
constexpr int GuardiansPathSlipChance = 15;
constexpr int BloodthirstPercent = 50;        // Bloodthirst and Transcendence
constexpr int WrathfulPercent = 30;
constexpr int ProdigyMana = 3 << 6;
constexpr int LifeFromDeathDivisor = 25;
constexpr int LifeFromDeathRange = 6;
constexpr int PoundOfFleshPercent = 3;
constexpr int BloodVengeanceMana = 3 << 6;
constexpr int InsurmountableMana = 5 << 6;
constexpr int RenewalPercent = 3;
constexpr int CounterstrokeTicks = 3 * TicksPerSecond;
constexpr int IllusionistThresholdPercent = 15;
constexpr int IllusionistPercent = 50;
constexpr int IllusionistTicks = 3 * TicksPerSecond;
constexpr int BloodIsPowerDivisor = 25;
constexpr int RathmaBelowDivisor = 5;         // below a fifth of life
constexpr int RathmaTicks = 4 * TicksPerSecond;
constexpr int FueledByDeathPercent = 30;
constexpr int FueledByDeathTicks = 4 * TicksPerSecond;
// The saves.
constexpr int CheatDeathLifeDivisor = 3;      // a third of life - and, Near Death Experience, of mana
constexpr int AnomalyThrowTiles = 2;
constexpr int FinalServiceLifeDivisor = 4;
// The rest.
constexpr int HoldYourGroundBlock = 20;
constexpr int IronMaidenPercent = 50;
constexpr int CrusadersStridePercent = 15;
constexpr int NumbingTrapsPercent = 25;
constexpr int ChantOfResonancePercent = 50;
constexpr int CommanderPercent = 30;
constexpr int BluntPercent = 25;
constexpr int ToweringShieldPercent = 25;
constexpr int JuggernautShrugChance = 50;
constexpr int JuggernautHealChance = 30;
constexpr int JuggernautHealDivisor = 5;
constexpr int JuggernautSlowPercent = 50;
constexpr int CustomEngineeringLevels = 3;
constexpr int RuneSpareOneIn = 2;             // Custom Engineering: one rune in two is not used up
constexpr int GrenadierEvery = 4;
constexpr int GrenadierLevelDivisor = 4;      // the grenade's spell level: the Rogue's level over four
constexpr int InspiringPresenceDurationPercent = 200;
constexpr int InspiringPresenceDivisor = 100; // life a second under a blessing
constexpr int BroodingDivisor = 100;          // life a second, standing still
constexpr int SereneMindDivisor = 50;         // mana a second, standing still
constexpr int DrawLifePerMille = 5;           // life a second per monster near, in thousandths (a two-hundredth)
constexpr int DrawLifeRange = 4;
constexpr int DrawLifeMaxCount = 5;

/** @brief Dodge, Avoid and Evade at @p points: a tenth, a twenty-fifth more a rank, two fifths at most. */
int SlipChanceAt(int points)
{
	return std::min(10 + 4 * (std::max(points, 1) - 1), 40);
}

/** @brief Pierce at @p points: 15%, +5% a rank, 60% at most. */
int PierceChanceAt(int points)
{
	return std::min(15 + 5 * (std::max(points, 1) - 1), 60);
}

/** @brief Throwing Mastery at @p points: +10%, +6% a rank. */
int ThrowingMasteryPercentAt(int points)
{
	return 10 + 6 * (std::max(points, 1) - 1);
}

/** @brief Reed in the Wind at @p points: +10% block, +2% a rank. */
int ReedInTheWindBlockAt(int points)
{
	return 10 + 2 * (std::max(points, 1) - 1);
}

/** @brief Counterstroke at @p points: +30% on the empowered blow, +10% a rank. */
int CounterstrokePercentAt(int points)
{
	return 30 + 10 * (std::max(points, 1) - 1);
}

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
	return player._pHitPoints * LowLifeDivisor < player._pMaxHP;
}

bool Stunned(const Monster &monster)
{
	// A skill's stun, not the Delay every AI pauses in between decisions: a plain skeleton gave Relentless Assault its
	// +30% on most blows (round 19 audit, v1.12.244).
	return IsMonsterStunned(monster);
}

/** @brief Whether @p player is playing a song or holding any aura. */
bool SongPlaying(const Player &player)
{
	return GetActiveClassAura(player) != ClassTreeSkill::None;
}

/** @brief The fork's stagger exemption: uniques, champions and Diablo (rfa12's ShrugsOff) - Paralysis stunned Diablo. */
bool ShrugsOffStun(const Monster &monster)
{
	return monster.isUnique() || monster.lesserAffix != LesserUniqueAffix::None || monster.type().type == MT_DIABLO;
}

/** @brief Monsters that can be hit within @p range tiles of @p centre, not counting @p except. */
int MonstersNear(Point centre, int range, const Monster *except)
{
	int count = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &other = Monsters[ActiveMonsters[i]];
		if (&other == except || !other.isPossibleToHit() || other.hitPoints >> 6 <= 0)
			continue;
		// Enemies only: the hero's own minions, companions and converts counted - Draw Life healed off the army, and an
		// adjacent Spirit Guardian lit Determination (round 13 audit, v1.12.238).
		if (other.isPlayerMinion() || IsCompanion(other) || IsMonsterConverted(other))
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
	return SlipChanceAt(ClassTreeInvestment(player, skill));
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

/** @brief The floor as "once a floor" counts it: a set level is its own floor. */
int FloorStamp()
{
	return setlevel ? 1000 + static_cast<int>(setlvlnum) : static_cast<int>(currlevel);
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
		Haste(clocks, HotPursuitPercent, HotPursuitTicks);
	if (SongPlaying(player) && PassiveActive(player, Skill::Crescendo))
		clocks.crescendoStacks = std::min(clocks.crescendoStacks + 1, 15);
	if (clocks.momentumBlows > 0)
		clocks.momentumBlows--;
	if (damage > 0 && PassiveActive(player, Skill::Resolve))
		DebuffMonster(target, ResolveTicks, -ResolvePercent, 0);
	if (melee)
		MarkElement(target, DamageType::Physical, MarkTicks);
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
	// Rathma's Shield (the Necromancer): for its few seconds, nothing at all.
	if (clocks.rathmaTicks > 0 && PassiveActive(player, Skill::RathmasShield))
		return -100;
	// Stand Alone: 15% with no minions, 3% less for each one, nothing at five.
	if (PassiveActive(player, Skill::StandAlone))
		percent -= std::max(StandAlonePercent - StandAlonePerMinion * MinionCount(player), 0);
	if (PassiveActive(player, Skill::Blur))
		percent -= BlurPercent;
	if (notSteel && PassiveActive(player, Skill::SixthSense))
		percent -= SixthSensePercent;
	if (notSteel && PassiveActive(player, Skill::Vigilant))
		percent -= VigilantPercent;
	if (WearingShield(player) && PassiveActive(player, Skill::SwordAndBoard))
		percent -= SwordAndBoardPercent;
	if (BelowAThird(player) && PassiveActive(player, Skill::Relentless))
		percent -= RelentlessPercent;
	if (Still(player) && PassiveActive(player, Skill::UnwaveringWill))
		percent -= UnwaveringWillTakenPercent;
	// 2026-09-14 sweep.
	if (clocks.unharmedTicks >= GalvanizingWardTicks && PassiveActive(player, Skill::GalvanizingWard))
		percent -= GalvanizingWardPercent;
	if (clocks.dominanceTicks > 0 && PassiveActive(player, Skill::Dominance))
		percent -= DominancePerStack * clocks.dominanceStacks;
	if (clocks.heldAuraTicks >= 30 * TicksPerSecond && PassiveActive(player, Skill::MagnumOpus))
		percent -= 10;
	// Whatever stacks, a blow always lands: a quarter of it at the least.
	return std::max(percent, -75);
}

int PassiveDamageDealtPercent(const Player &player, const Monster &target, bool melee)
{
	int percent = 0;
	// Spreading Malediction (the Necromancer): 5% a cursed monster within six, 30% at most.
	if (PassiveActive(player, Skill::SpreadingMalediction))
		percent += std::min(SpreadingMaledictionPerCurse * CursedMonstersNear(player.position.tile, SpreadingMaledictionRange), SpreadingMaledictionMax);
	const int targetLife = target.hitPoints;
	const int targetMax = std::max(target.maxHitPoints, 1);
	const int distance = player.position.tile.WalkingDistance(target.position.tile);
	const bool chilled = IsMonsterChilled(target) || IsMonsterFrozen(target);
	Clocks &clocks = ClocksFor(player);

	if (PassiveActive(player, Skill::Ruthless) && targetLife * LowLifeDivisor < targetMax)
		percent += RuthlessPercent;
	if (PassiveActive(player, Skill::Ambush) && static_cast<int64_t>(targetLife) * 100 >= static_cast<int64_t>(targetMax) * AmbushLifePercent)
		percent += AmbushPercent;
	if (PassiveActive(player, Skill::Brawler) && MonstersNear(player.position.tile, 1, nullptr) >= BrawlerCrowd)
		percent += BrawlerPercent;
	if (PassiveActive(player, Skill::Determination))
		percent += std::min(MonstersNear(player.position.tile, 1, nullptr), DeterminationMaxEnemies) * DeterminationPerEnemy;
	if (PassiveActive(player, Skill::SteadyAim) && MonstersNear(player.position.tile, SteadyAimRange, nullptr) == 0)
		percent += SteadyAimPercent;
	if (PassiveActive(player, Skill::Audacity) && distance <= AudacityRange)
		percent += AudacityPercent;
	if (PassiveActive(player, Skill::PowerHungry) && distance >= FarTiles)
		percent += PowerHungryPercent;
	if (PassiveActive(player, Skill::ColdBlooded) && chilled)
		percent += ColdBloodedPercent;
	if (PassiveActive(player, Skill::CullTheWeak) && chilled)
		percent += CullTheWeakPercent;
	if (PassiveActive(player, Skill::RelentlessAssault) && (IsMonsterFrozen(target) || Stunned(target)))
		percent += RelentlessAssaultPercent;
	if (PassiveActive(player, Skill::SingleOut) && MonstersNear(target.position.tile, SingleOutRange, &target) == 0)
		percent += SingleOutPercent;
	if (PassiveActive(player, Skill::Rampage))
		percent += clocks.rampageStacks * RampagePerStack;
	if (PassiveActive(player, Skill::UnwaveringWill) && Still(player))
		percent += UnwaveringWillDealtPercent;
	if (PassiveActive(player, Skill::GlassCannon))
		percent += GlassCannonPercent; // every hit, spells included (round 20 audit)
	// The beat: the third blow since the last one. Counted in OnPassiveHit, read here, so the
	// blow that IS the beat carries the bonus and the count restarts after it lands.
	if (melee && PassiveActive(player, Skill::Cadence) && clocks.cadenceCount == 2)
		percent += 50;
	// The Barbarian's (2026-09-14). Berserker Rage reads the pool at the moment of the blow.
	if (PassiveActive(player, Skill::BerserkerRage) && UsesRage(player) && player._pRage * 2 >= MaxRage(player))
		percent += BerserkerRagePercent;
	if (PassiveActive(player, Skill::NoEscape) && distance >= FarTiles)
		percent += NoEscapePercent;

	// ---- the all-heroes sweep (2026-09-14) ----
	static const MonsterMarks NoMarks {};
	const MonsterMarks *found = MarksFor(target);
	const MonsterMarks &marks = found != nullptr ? *found : NoMarks;
	if (PassiveActive(player, Skill::Conflagration) && marks.elementTicks[1] > 0)
		percent += ConflagrationPercent;
	if (PassiveActive(player, Skill::ElementalExposure)) {
		for (const int ticks : marks.elementTicks) {
			if (ticks > 0)
				percent += ElementalExposurePerElement;
		}
	}
	if (!melee && clocks.dynamoTicks > 0 && PassiveActive(player, Skill::ArcaneDynamo))
		percent += ArcaneDynamoPercent;
	if (!melee && clocks.mythicTicks > 0 && PassiveActive(player, Skill::MythicRhythm))
		percent += MythicRhythmPercent;
	if (PassiveActive(player, Skill::Sharpshooter)) {
		// Four points of chance a second without one; a critical blow doubles and starts the count again.
		const int chance = std::min(SharpshooterPerSecond * clocks.sharpshooterTicks / TicksPerSecond, 100);
		if (chance > 0 && GenerateRnd(100) < chance) {
			percent += SharpshooterCritPercent;
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
		percent += std::min(MinionsNear(player.position.tile, AllyRange), AllyMaxCount) * AllyPercent;
	if (melee && clocks.counterTicks > 0 && PassiveActive(player, Skill::Counterstroke))
		percent += clocks.counterPercent;
	if (PassiveActive(player, Skill::SeizeTheInitiative) && targetLife >= targetMax)
		percent += SeizeTheInitiativePercent;
	if (melee && PassiveActive(player, Skill::CombinationStrike)) {
		int distinct = 0;
		for (size_t i = 0; i < clocks.comboSpells.size(); i++) {
			if (clocks.comboTicks[i] > 0 && clocks.comboSpells[i] != SpellID::Invalid)
				distinct++;
		}
		percent += CombinationStrikePerSkill * distinct;
	}
	if (clocks.momentumBlows > 0 && PassiveActive(player, Skill::Momentum))
		percent += MomentumPercent;
	// The replacements for the rows the engine could not carry (2026-09-14).
	if (PassiveActive(player, Skill::Sanctified) && IsAnyOf(target.data().monsterClass, MonsterClass::Undead, MonsterClass::Demon))
		percent += SanctifiedPercent;
	if (!melee && player._pMana * 2 > player._pMaxMana && PassiveActive(player, Skill::ManaAttunement))
		percent += ManaAttunementPercent;
	// Throwing Mastery (Barbarian, 2026-09-14): a missile from a Barbarian with no bow is a thrown weapon.
	if (!melee && player._pClass == HeroClass::Barbarian && !player.UsesRangedWeapon() && PassiveActive(player, Skill::ThrowingMastery))
		percent += ThrowingMasteryPercentAt(ClassTreeRank(player, Skill::ThrowingMastery));
	return percent;
}

int PassiveMeleeSlipChance(const Player &player, bool walking)
{
	int chance = SlipChance(player, walking ? Skill::Evade : Skill::Dodge);
	if (DualWielding(player) && PassiveActive(player, Skill::TheGuardiansPath))
		chance += GuardiansPathSlipChance;
	return chance;
}

bool PassiveEvadesMelee(const Player &player)
{
	const bool walking = IsAnyOf(player._pmode, PM_WALK_NORTHWARDS, PM_WALK_SOUTHWARDS, PM_WALK_SIDEWAYS);
	const int chance = PassiveMeleeSlipChance(player, walking);
	const bool slipped = chance > 0 && GenerateRnd(100) < chance;
	if (slipped && PassiveActive(player, Skill::TacticalAdvantage))
		Haste(ClocksFor(player), TacticalAdvantagePercent, TacticalAdvantageTicks);
	return slipped;
}

bool PassiveEvadesMissile(const Player &player)
{
	const int chance = SlipChance(player, Skill::Avoid);
	const bool slipped = chance > 0 && GenerateRnd(100) < chance;
	if (slipped && PassiveActive(player, Skill::TacticalAdvantage))
		Haste(ClocksFor(player), TacticalAdvantagePercent, TacticalAdvantageTicks);
	return slipped;
}

bool ArrowPierces(Missile &missile)
{
	if (missile.sourceType() != MissileSource::Player || !GetMissileData(missile._mitype).isArrow())
		return false;
	const Player &player = *missile.sourcePlayer();
	if (!PassiveActive(player, Skill::Pierce))
		return false;
	return GenerateRnd(100) < PierceChanceAt(ClassTreeInvestment(player, Skill::Pierce));
}

bool PassiveCheatsDeath(Player &player)
{
	Clocks &clocks = ClocksFor(player);
	// Final Service (the Necromancer): the army dies in your place, once a floor. Before the cooldown - it has its own.
	if (PassiveActive(player, Skill::FinalService) && clocks.finalServiceLevel != FloorStamp() && MinionCount(player) > 0) {
		clocks.finalServiceLevel = FloorStamp();
		DismissMinions(player);
		SetPlayerHitPoints(player, std::max(64, player._pMaxHP / FinalServiceLifeDivisor)); // at least 1 life (round 12 audit)
		return true;
	}
	if (clocks.cheatDeathCooldown > 0)
		return false;
	const bool nearDeath = PassiveActive(player, Skill::NearDeathExperience);
	const bool anomaly = PassiveActive(player, Skill::UnstableAnomaly);
	if (!nearDeath && !anomaly && !PassiveActive(player, Skill::Indestructible) && !PassiveActive(player, Skill::NervesOfSteel)
	    && !PassiveActive(player, Skill::Awareness))
		return false;

	// At least 1 life: under 3 maximum life (Black Death) the save left under one and the next tick killed (round 12).
	SetPlayerHitPoints(player, std::max(64, player._pMaxHP / CheatDeathLifeDivisor));
	if (nearDeath) {
		// Restores: never lowers a fuller pool (round 4 audit - a Monk at 90% mana came back at 33%).
		player._pMana = std::max(player._pMana, player._pMaxMana / CheatDeathLifeDivisor);
		player._pManaBase = player._pMaxManaBase - (player._pMaxMana - player._pMana);
		RedrawComponent(PanelDrawComponent::Mana);
	}
	// Unstable Anomaly: the save throws back everything that stood close.
	if (anomaly) {
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			Monster &other = Monsters[ActiveMonsters[i]];
			if (!other.isPlayerMinion() && other.hitPoints >> 6 > 0 && player.position.tile.WalkingDistance(other.position.tile) <= AnomalyThrowTiles)
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
		Heal(player, damage * LeechPercent / 100);
	// Dark Reaping (the Necromancer): a point of mana and a point of Essence for every blow that lands.
	if (damage > 0 && PassiveActive(player, Skill::DarkReaping)) {
		RestoreMana(player, DarkReapingMana);
		GainEssence(player, DarkReapingEssence);
	}
	if (melee && PassiveActive(player, Skill::Cadence))
		clocks.cadenceCount = (clocks.cadenceCount + 1) % 3;
	// Weapons Master's mace: a point of Rage for every blow that lands, with or without a skill.
	// Not the blows of a Whirlwind spin (audit, 2026-09-29): four a second on each monster beside him, they paid for the
	// spin and more, and it never ran dry.
	if (melee && PassiveActive(player, Skill::WeaponsMaster) && WieldingMace(player) && !IsWhirlwinding(player))
		GainRage(player, WeaponsMasterRage);
	if (melee && PassiveActive(player, Skill::Righteousness))
		RestoreMana(player, RighteousnessMana);
	if (!melee && PassiveActive(player, Skill::NightStalker))
		RestoreMana(player, NightStalkerMana);
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
				clocks.comboTicks[slot] = CombinationStrikeTicks;
			}
			if (PassiveActive(player, Skill::MythicRhythm) && ++clocks.mythicCount >= MythicRhythmBlows) {
				clocks.mythicCount = 0;
				clocks.mythicTicks = MythicRhythmTicks;
			}
		}
	}
	OnAnyHit(player, target, damage, melee);
}

void OnPassiveMissileHit(Player &player, const Monster &target, int damage, DamageType damageType, bool arrow)
{
	if (damage <= 0)
		return;
	MarkElement(target, damageType, damageType == DamageType::Fire ? FireMarkTicks : MarkTicks);
	if (MarksFor(target) == nullptr)
		return; // not a monster of the level - nothing to stun or slow
	Monster &monster = Monsters[target.getId()];
	const bool alive = monster.hitPoints >> 6 > 0;
	if (alive && damageType == DamageType::Lightning && PassiveActive(player, Skill::Paralysis) && !ShrugsOffStun(monster)
	    && GenerateRnd(100) < ParalysisChance)
		StunMonster(monster, ParalysisTicks);
	if (alive && damageType == DamageType::Magic && PassiveActive(player, Skill::TemporalFlux))
		ChillMonster(monster, TemporalFluxTicks);
	if (arrow) {
		if (alive && PassiveActive(player, Skill::ThrillOfTheHunt) && GenerateRnd(100) < ThrillOfTheHuntChance)
			ChillMonster(monster, ThrillOfTheHuntTicks);
		if (PassiveActive(player, Skill::Archery)) {
			const std::optional<unique_base_item> bow = BowBase(player);
			if (bow && (*bow == UITYPE_COMPBOW || *bow == UITYPE_BATTLEBOW))
				RestoreMana(player, ArcheryMana);
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
		Heal(player, cost * BloodthirstPercent / 100);
	if (PassiveActive(player, Skill::Wrathful))
		Heal(player, cost * WrathfulPercent / 100);
	const bool cheap = cost <= CheapSpellMana;
	if (cheap && PassiveActive(player, Skill::Prodigy))
		RestoreMana(player, ProdigyMana);
	if (PassiveActive(player, Skill::ArcaneDynamo)) {
		Clocks &clocks = ClocksFor(player);
		if (cheap) {
			clocks.dynamoCharges = std::min(clocks.dynamoCharges + 1, ArcaneDynamoCharges);
		} else if (clocks.dynamoCharges >= ArcaneDynamoCharges) {
			clocks.dynamoCharges = 0;
			clocks.dynamoTicks = ArcaneDynamoTicks;
		}
	}
}

void OnPassiveMonsterDied(Player &player, const Monster &monster)
{
	// Life from Death (the Necromancer): a twenty-fifth of your life for a death within six - "a monster that dies", so
	// the army's kills count, which never passed the hero's own kill hook (round 13 audit, v1.12.238).
	if (monster.isPlayerMinion() || player._pHitPoints >> 6 <= 0)
		return;
	if (PassiveActive(player, Skill::LifeFromDeath) && player.position.tile.WalkingDistance(monster.position.tile) <= LifeFromDeathRange)
		Heal(player, player._pMaxHP / LifeFromDeathDivisor);
}

void OnPassiveStruck(Player &player)
{
	ClocksFor(player).unharmedTicks = 0; // Galvanizing Ward's clock: a blow arrived, whatever absorbs it
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
	// Life from Death moved to OnPassiveMonsterDied: any death, the army's kills included (round 13 audit).
	if (PassiveActive(player, Skill::PoundOfFlesh))
		Heal(player, player._pMaxHP * PoundOfFleshPercent / 100);
	if (PassiveActive(player, Skill::Dominance)) {
		clocks.dominanceStacks = std::min(clocks.dominanceStacks + 1, DominanceMaxStacks);
		clocks.dominanceTicks = DominanceHoldTicks;
	}
	if (PassiveActive(player, Skill::BloodVengeance))
		RestoreMana(player, BloodVengeanceMana);
}

void OnPassiveBlock(Player &player)
{
	if (PassiveActive(player, Skill::Insurmountable))
		RestoreMana(player, InsurmountableMana);
	if (PassiveActive(player, Skill::Renewal))
		Heal(player, player._pMaxHP * RenewalPercent / 100);
	if (PassiveActive(player, Skill::Counterstroke)) {
		Clocks &clocks = ClocksFor(player);
		clocks.counterTicks = CounterstrokeTicks;
		clocks.counterPercent = CounterstrokePercentAt(ClassTreeInvestment(player, Skill::Counterstroke));
	}
}

void OnPassiveDamaged(Player &player, int damage)
{
	if (damage <= 0)
		return;
	Clocks &clocks = ClocksFor(player);
	clocks.unharmedTicks = 0;
	if (damage * 100 >= player._pMaxHP * IllusionistThresholdPercent && PassiveActive(player, Skill::Illusionist))
		Haste(clocks, IllusionistPercent, IllusionistTicks);
	// The Necromancer (2026-09-18).
	if (PassiveActive(player, Skill::BloodIsPower)) {
		// Losing life feeds Essence: one point for every twenty-fifth of your life.
		clocks.bloodLost += damage;
		const int step = std::max(player._pMaxHP / BloodIsPowerDivisor, 64);
		if (clocks.bloodLost >= step) {
			GainEssence(player, clocks.bloodLost / step);
			clocks.bloodLost %= step;
		}
	}
	if (PassiveActive(player, Skill::RathmasShield) && clocks.rathmaLevel != FloorStamp() && player._pHitPoints > 0
	    && player._pHitPoints * RathmaBelowDivisor < player._pMaxHP) {
		clocks.rathmaLevel = FloorStamp();
		clocks.rathmaTicks = RathmaTicks;
	}
}

int PassiveBlockBonus(const Player &player)
{
	int bonus = 0;
	if (PassiveActive(player, Skill::HoldYourGround))
		bonus += HoldYourGroundBlock;
	if (HoldsType(player, ItemType::Staff) && PassiveActive(player, Skill::ReedInTheWind))
		bonus += ReedInTheWindBlockAt(ClassTreeInvestment(player, Skill::ReedInTheWind));
	return bonus;
}

int PassiveThornsPercent(const Player &player)
{
	return PassiveActive(player, Skill::IronMaiden) ? IronMaidenPercent : 0;
}

int PassiveMoveSpeedBonus(const Player &player)
{
	const Clocks &clocks = ClocksFor(player);
	int bonus = clocks.hasteTicks > 0 ? clocks.hastePercent : 0;
	// Crusader's Stride (2026-09-14): while an aura burns.
	if (SongPlaying(player) && PassiveActive(player, Skill::CrusadersStride))
		bonus += CrusadersStridePercent;
	return bonus;
}

int PassiveMonsterDamagePercent(const Monster &monster)
{
	if (MyPlayer == nullptr)
		return 0;
	const Player &player = *MyPlayer;
	int percent = 0;
	if ((IsMonsterChilled(monster) || IsMonsterFrozen(monster)) && PassiveActive(player, Skill::NumbingTraps))
		percent -= NumbingTrapsPercent;
	if (SongPlaying(player) && PassiveActive(player, Skill::Dissonance)
	    && player.position.tile.WalkingDistance(monster.position.tile) <= 3)
		percent -= 20;
	return percent;
}

int PassiveManaCostPercent(const Player &player, SpellID spell)
{
	if (IsAnyOf(spell, SpellID::MantraOfClarity, SpellID::MantraOfEvasion, SpellID::MantraOfRetribution)
	    && PassiveActive(player, Skill::ChantOfResonance))
		return -ChantOfResonancePercent;
	// Commander of the Risen Dead (the Necromancer): raising costs less.
	if (IsAnyOf(spell, SpellID::RaiseSkeleton, SpellID::RaiseSkeletalMage) && PassiveActive(player, Skill::CommanderOfTheRisenDead))
		return -CommanderPercent;
	return 0;
}

int PassiveSkillDamagePercent(const Player &player, SpellID spell)
{
	int percent = 0;
	if (spell == SpellID::BlessedHammer && PassiveActive(player, Skill::Blunt))
		percent += BluntPercent;
	if (IsAnyOf(spell, SpellID::BlessedShield, SpellID::ShieldBash) && PassiveActive(player, Skill::ToweringShield))
		percent += ToweringShieldPercent;
	return percent;
}

bool PassiveShrugsOffStagger(Player &player)
{
	if (SongPlaying(player) && PassiveActive(player, Skill::Stagecraft))
		return true;
	if (!PassiveActive(player, Skill::Juggernaut))
		return false;
	if (GenerateRnd(100) < JuggernautShrugChance)
		return true;
	// The stagger lands - and sometimes gives something back.
	Clocks &clocks = ClocksFor(player);
	if (clocks.juggernautCooldown == 0 && GenerateRnd(100) < JuggernautHealChance) {
		Heal(player, player._pMaxHP / JuggernautHealDivisor);
		clocks.juggernautCooldown = JuggernautCooldownTicks;
	}
	return false;
}

int PassiveSlowShortenPercent(const Player &player)
{
	return PassiveActive(player, Skill::Juggernaut) ? JuggernautSlowPercent : 0;
}

int PassiveRuneLevelBonus(const Player &player)
{
	return PassiveActive(player, Skill::CustomEngineering) ? CustomEngineeringLevels : 0;
}

bool PassiveSparesRune(const Player &player)
{
	return PassiveActive(player, Skill::CustomEngineering) && GenerateRnd(RuneSpareOneIn) == 0;
}

void OnPassiveArrowLoosed(Player &player, Point target)
{
	if (!PassiveActive(player, Skill::Grenadier))
		return;
	if (++ClocksFor(player).arrowsLoosed % GrenadierEvery != 0)
		return;
	// The grenade: the engine's Fireball, lobbed after the arrow at a quarter of the Rogue's level, wearing RfA-16's
	// tumbling clay bomb in flight. Its burst is still the Fireball's own explosion (ProcessFireball swaps to it).
	const Point from = player.position.tile;
	const Point dst = target == from ? from + player._pdir : target;
	Missile *grenade = AddMissile(from, dst, GetDirection(from, dst), MissileID::Fireball, TARGET_MONSTERS,
	    static_cast<int>(player.getId()), 0, std::max(player._pLevel / GrenadierLevelDivisor, 1));
	if (grenade != nullptr && MissileArtLoaded(MissileGraphicID::Grenade))
		UseMissileGraphic(*grenade, MissileGraphicID::Grenade);
}

int PassiveWarcryDurationPercent(const Player &player)
{
	return PassiveActive(player, Skill::InspiringPresence) ? InspiringPresenceDurationPercent : 100;
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
	    && (clocks.stillTicks - StillnessTicks) % TicksPerSecond == 0)
		Heal(player, player._pMaxHP / BroodingDivisor);
	// Serene Mind (2026-09-14): still for a moment, and then a fiftieth of your mana every second.
	if (player._pHitPoints > 0 && Still(player) && PassiveActive(player, Skill::SereneMind)
	    && (clocks.stillTicks - StillnessTicks) % TicksPerSecond == 0)
		RestoreMana(player, player._pMaxMana / SereneMindDivisor);
	// Inspiring Presence: under any warcry blessing, a hundredth of your life every second.
	if (player._pHitPoints > 0 && PassiveActive(player, Skill::InspiringPresence) && AnyWarcryBuffActive(player)) {
		if (++clocks.inspireTicks % TicksPerSecond == 0)
			Heal(player, player._pMaxHP / InspiringPresenceDivisor);
	} else {
		clocks.inspireTicks = 0;
	}

	// ---- the Necromancer (2026-09-18) ----
	if (clocks.rathmaTicks > 0)
		clocks.rathmaTicks--;
	// Draw Life: a two-hundredth of your life a second for each monster within four, five at most.
	if (player._pHitPoints > 0 && PassiveActive(player, Skill::DrawLife) && ++clocks.drawLifeTicks % TicksPerSecond == 0) {
		const int near = std::min(MonstersNear(player.position.tile, DrawLifeRange, nullptr), DrawLifeMaxCount);
		if (near > 0)
			Heal(player, player._pMaxHP * near * DrawLifePerMille / 1000);
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
	if (clocks.walkTicks >= MomentumWalkTicks && PassiveActive(player, Skill::Momentum))
		clocks.momentumBlows = MomentumBlows;
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

void OnPassiveCorpseConsumed(Player &player)
{
	// Fueled by Death (the Necromancer): a corpse used quickens the step for four seconds.
	if (PassiveActive(player, Skill::FueledByDeath))
		Haste(ClocksFor(player), FueledByDeathPercent, FueledByDeathTicks);
}

void ClearPassiveState()
{
	ClocksOf.fill(Clocks {});
	MarksOf.fill(MonsterMarks {});
}

void ClearPassiveMarksForMonster(const Monster &monster)
{
	// Freed slots are reused at once (a raised skeleton, Leoric's next skeleton): a burning corpse's 3-5 s of Conflagration
	// and Exposure marks went to its successor (round 5 audit, v1.12.230).
	if (MonsterMarks *marks = MarksFor(monster); marks != nullptr)
		*marks = {};
}

void ClearPassiveMarks()
{
	MarksOf.fill(MonsterMarks {});
}

void ClearPassiveClocks(Player &player)
{
	ClocksFor(player) = Clocks {};
}


std::string PassiveFactsAt(const Player &player, ClassTreeSkill skill, int points)
{
	// Every number below is a named constant or helper the rule above reads (2026-09-26: the user's
	// rule 2 - the hover states the effect with its numbers). Life and mana print in real points.
	points = std::max(points, 1);
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	const auto sec = [](int ticks) { return ticks / TicksPerSecond; };
	const auto shareOf = [](int divisor) { return 100 / divisor; };
	const auto mana = [](int amount) { return amount >> 6; };
	const double stillSeconds = static_cast<double>(StillnessTicks) / TicksPerSecond;
	const auto save = [&](bool withMana) {
		line(fmt::format(fmt::runtime(withMana ? _("Once every {:d} s: a killing blow leaves you at {:d}% life and mana")
		                                       : _("Once every {:d} s: a killing blow leaves you at {:d}% life")),
		    sec(CheatDeathCooldownTicks), shareOf(CheatDeathLifeDivisor)));
	};
	const auto damageVs = [&](std::string_view what, int percent) {
		line(fmt::format(fmt::runtime(_("Damage against {:s}: +{:d}%")), what, percent));
	};
	const auto haste = [&](std::string_view when, int percent, int ticks) {
		line(fmt::format(fmt::runtime(_("{:s}: +{:d}% movement speed for {:d} s")), when, percent, sec(ticks)));
	};

	switch (skill) {
	// ---- the Rogue's and the Monk's ranked rows: these grow with every point ----
	case Skill::Dodge:
		line(fmt::format(fmt::runtime(_("Chance to slip a melee blow while standing: {:d}%")), SlipChanceAt(points)));
		break;
	case Skill::Evade:
		line(fmt::format(fmt::runtime(_("Chance to slip a melee blow while moving: {:d}%")), SlipChanceAt(points)));
		break;
	case Skill::Avoid:
		line(fmt::format(fmt::runtime(_("Chance to slip an arrow: {:d}%")), SlipChanceAt(points)));
		break;
	case Skill::Pierce:
		line(fmt::format(fmt::runtime(_("Chance an arrow flies on: {:d}%")), PierceChanceAt(points)));
		break;
	case Skill::ThrowingMastery:
		line(fmt::format(fmt::runtime(_("Thrown weapon damage: +{:d}%")), ThrowingMasteryPercentAt(points)));
		break;
	case Skill::ReedInTheWind:
		line(fmt::format(fmt::runtime(_("With a staff: chance to block +{:d}%")), ReedInTheWindBlockAt(points)));
		break;
	case Skill::Counterstroke:
		line(fmt::format(fmt::runtime(_("After a block: next melee blow within {:d} s +{:d}% damage")), sec(CounterstrokeTicks),
		    CounterstrokePercentAt(points)));
		break;

	// ---- damage taken ----
	case Skill::Vigilant:
		line(fmt::format(fmt::runtime(_("Fire, lightning and magic damage taken: -{:d}%")), VigilantPercent));
		break;
	case Skill::SixthSense:
		line(fmt::format(fmt::runtime(_("Fire, lightning and magic damage taken: -{:d}%")), SixthSensePercent));
		break;
	case Skill::Blur:
		line(fmt::format(fmt::runtime(_("Damage taken: -{:d}%")), BlurPercent));
		break;
	case Skill::SwordAndBoard:
		line(fmt::format(fmt::runtime(_("Behind a shield: damage taken -{:d}%")), SwordAndBoardPercent));
		break;
	case Skill::Relentless:
		line(fmt::format(fmt::runtime(_("Below {:d}% life: damage taken -{:d}%")), shareOf(LowLifeDivisor), RelentlessPercent));
		break;
	case Skill::UnwaveringWill:
		line(fmt::format(fmt::runtime(_("Standing still {:.1f} s: damage taken -{:d}%")), stillSeconds, UnwaveringWillTakenPercent));
		line(fmt::format(fmt::runtime(_("Standing still {:.1f} s: damage +{:d}%")), stillSeconds, UnwaveringWillDealtPercent));
		break;
	case Skill::GalvanizingWard:
		line(fmt::format(fmt::runtime(_("After {:d} s unharmed: next blow taken -{:d}%")), sec(GalvanizingWardTicks), GalvanizingWardPercent));
		break;
	case Skill::Dominance:
		line(fmt::format(fmt::runtime(_("Each kill: damage taken -{:d}% for {:d} s, up to -{:d}%")), DominancePerStack,
		    sec(DominanceHoldTicks), DominancePerStack * DominanceMaxStacks));
		break;
	case Skill::StandAlone:
		line(fmt::format(fmt::runtime(_("Damage taken: -{:d}%, {:d}% less for each minion")), StandAlonePercent, StandAlonePerMinion));
		line(fmt::format(fmt::runtime(_("Now: -{:d}%")), std::max(StandAlonePercent - StandAlonePerMinion * MinionCount(player), 0)));
		break;
	case Skill::NumbingTraps:
		line(fmt::format(fmt::runtime(_("Chilled or frozen enemies deal -{:d}% damage to you")), NumbingTrapsPercent));
		break;
	case Skill::IronMaiden:
		line(fmt::format(fmt::runtime(_("Melee attackers take {:d}% of the blow back")), IronMaidenPercent));
		break;
	case Skill::HoldYourGround:
		line(fmt::format(fmt::runtime(_("Chance to block: +{:d}%")), HoldYourGroundBlock));
		break;
	case Skill::TheGuardiansPath:
		line(fmt::format(fmt::runtime(_("A weapon in each hand: {:d}% chance to slip a melee blow")), GuardiansPathSlipChance));
		break;
	case Skill::Juggernaut:
		line(fmt::format(fmt::runtime(_("Staggers shrugged off: {:d}%")), JuggernautShrugChance));
		line(fmt::format(fmt::runtime(_("Slows on you: {:d}% shorter")), JuggernautSlowPercent));
		line(fmt::format(fmt::runtime(_("A stagger that lands: {:d}% chance to heal {:d}% life, once every {:d} s")), JuggernautHealChance,
		    shareOf(JuggernautHealDivisor), sec(JuggernautCooldownTicks)));
		break;

	// ---- damage dealt ----
	case Skill::SpreadingMalediction:
		line(fmt::format(fmt::runtime(_("Damage: +{:d}% per cursed monster within {:d} tiles, up to +{:d}%")), SpreadingMaledictionPerCurse,
		    SpreadingMaledictionRange, SpreadingMaledictionMax));
		break;
	case Skill::Ruthless:
		damageVs(fmt::format(fmt::runtime(_("enemies below {:d}% life")), shareOf(LowLifeDivisor)), RuthlessPercent);
		break;
	case Skill::Ambush:
		damageVs(fmt::format(fmt::runtime(_("enemies at {:d}% life or more")), AmbushLifePercent), AmbushPercent);
		break;
	case Skill::Brawler:
		line(fmt::format(fmt::runtime(_("With {:d} or more enemies adjacent: damage +{:d}%")), BrawlerCrowd, BrawlerPercent));
		break;
	case Skill::Determination:
		line(fmt::format(fmt::runtime(_("Damage: +{:d}% per adjacent enemy, up to +{:d}%")), DeterminationPerEnemy,
		    DeterminationPerEnemy * DeterminationMaxEnemies));
		break;
	case Skill::SteadyAim:
		line(fmt::format(fmt::runtime(_("No enemy within {:d} tiles: damage +{:d}%")), SteadyAimRange, SteadyAimPercent));
		break;
	case Skill::GlassCannon:
		// Its +15% lives here since round 20, not on the sheet: the hover showed only the armour cut (round 26 audit).
		line(fmt::format(fmt::runtime(_("Damage: +{:d}%, every hit")), GlassCannonPercent));
		break;
	case Skill::Audacity:
		damageVs(fmt::format(fmt::runtime(_("enemies within {:d} tiles")), AudacityRange), AudacityPercent);
		break;
	case Skill::PowerHungry:
		damageVs(fmt::format(fmt::runtime(_("enemies {:d} or more tiles away")), FarTiles), PowerHungryPercent);
		break;
	case Skill::NoEscape:
		damageVs(fmt::format(fmt::runtime(_("enemies {:d} or more tiles away")), FarTiles), NoEscapePercent);
		break;
	case Skill::ColdBlooded:
		damageVs(_("chilled or frozen enemies"), ColdBloodedPercent);
		break;
	case Skill::CullTheWeak:
		damageVs(_("chilled or frozen enemies"), CullTheWeakPercent);
		break;
	case Skill::RelentlessAssault:
		damageVs(_("frozen or stunned enemies"), RelentlessAssaultPercent);
		break;
	case Skill::SingleOut:
		damageVs(fmt::format(fmt::runtime(_("an enemy alone within {:d} tiles")), SingleOutRange), SingleOutPercent);
		break;
	case Skill::SeizeTheInitiative:
		damageVs(_("enemies at full life"), SeizeTheInitiativePercent);
		break;
	case Skill::Sanctified:
		damageVs(_("undead and demons"), SanctifiedPercent);
		break;
	case Skill::Rampage:
		line(fmt::format(fmt::runtime(_("Each kill: damage +{:d}% for {:d} s, up to +{:d}%")), RampagePerStack, sec(RampageHoldTicks),
		    RampagePerStack * RampageMaxStacks));
		break;
	case Skill::BerserkerRage:
		line(fmt::format(fmt::runtime(_("Rage at half or more: damage +{:d}%")), BerserkerRagePercent));
		break;
	case Skill::Conflagration:
		damageVs(fmt::format(fmt::runtime(_("enemies your fire struck in the last {:d} s")), sec(FireMarkTicks)), ConflagrationPercent);
		break;
	case Skill::ElementalExposure:
		line(fmt::format(fmt::runtime(_("Damage: +{:d}% per element on the enemy, up to +{:d}%")), ElementalExposurePerElement,
		    ElementalExposurePerElement * static_cast<int>(ElementCount)));
		line(fmt::format(fmt::runtime(_("An element stays on it {:d} s (fire {:d} s)")), sec(MarkTicks), sec(FireMarkTicks)));
		break;
	case Skill::ArcaneDynamo:
		line(fmt::format(fmt::runtime(_("Charged by {:d} spells costing {:d} mana or less")), ArcaneDynamoCharges, mana(CheapSpellMana)));
		line(fmt::format(fmt::runtime(_("Next costlier spell: spell damage +{:d}% for {:d} s")), ArcaneDynamoPercent, sec(ArcaneDynamoTicks)));
		break;
	case Skill::MythicRhythm:
		line(fmt::format(fmt::runtime(_("Every {:d} melee skill blows: spell damage +{:d}% for {:d} s")), MythicRhythmBlows,
		    MythicRhythmPercent, sec(MythicRhythmTicks)));
		break;
	case Skill::ManaAttunement:
		line(fmt::format(fmt::runtime(_("Mana above half: spell damage +{:d}%")), ManaAttunementPercent));
		break;
	case Skill::Sharpshooter:
		line(fmt::format(fmt::runtime(_("Critical chance: +{:d}% per second without one")), SharpshooterPerSecond));
		line(fmt::format(fmt::runtime(_("Critical blow: damage +{:d}%")), SharpshooterCritPercent));
		break;
	case Skill::Unity:
		line(fmt::format(fmt::runtime(_("Damage: +{:d}% per minion within {:d} tiles, up to +{:d}%")), AllyPercent, AllyRange,
		    AllyPercent * AllyMaxCount));
		break;
	case Skill::CombinationStrike:
		line(fmt::format(fmt::runtime(_("Damage: +{:d}% per different melee skill used in {:d} s, up to +{:d}%")), CombinationStrikePerSkill,
		    sec(CombinationStrikeTicks), CombinationStrikePerSkill * static_cast<int>(std::tuple_size<decltype(Clocks::comboSpells)>::value)));
		break;
	case Skill::Momentum:
		line(fmt::format(fmt::runtime(_("After {:d} s on the move: next {:d} blows +{:d}% damage")), sec(MomentumWalkTicks), MomentumBlows,
		    MomentumPercent));
		break;
	case Skill::Blunt:
		line(fmt::format(fmt::runtime(_("Blessed Hammer damage: +{:d}%")), BluntPercent));
		break;
	case Skill::ToweringShield:
		line(fmt::format(fmt::runtime(_("Smite and Blessed Shield damage: +{:d}%")), ToweringShieldPercent));
		break;

	// ---- on a blow, a missile, a block, a kill, a spend ----
	case Skill::Leech:
		line(fmt::format(fmt::runtime(_("Life returned: {:d}% of damage dealt")), LeechPercent));
		break;
	case Skill::DarkReaping:
		line(fmt::format(fmt::runtime(_("Each blow that lands: +{:d} mana, +{:d} Essence")), mana(DarkReapingMana), DarkReapingEssence));
		break;
	case Skill::WeaponsMaster:
		line(fmt::format(fmt::runtime(_("With a mace: +{:d} Rage per melee blow that lands")), WeaponsMasterRage));
		break;
	case Skill::Righteousness:
		line(fmt::format(fmt::runtime(_("Each melee blow that lands: +{:d} mana")), mana(RighteousnessMana)));
		break;
	case Skill::NightStalker:
		line(fmt::format(fmt::runtime(_("Each arrow that strikes: +{:d} mana")), mana(NightStalkerMana)));
		break;
	case Skill::Archery:
		line(fmt::format(fmt::runtime(_("Composite or battle bow: +{:d} mana per arrow that strikes")), mana(ArcheryMana)));
		break;
	case Skill::Paralysis:
		line(fmt::format(fmt::runtime(_("Lightning: {:d}% chance to stun for {:d} s, not uniques")), ParalysisChance, sec(ParalysisTicks)));
		break;
	case Skill::TemporalFlux:
		line(fmt::format(fmt::runtime(_("Magic damage slows its target for {:d} s")), sec(TemporalFluxTicks)));
		break;
	case Skill::ThrillOfTheHunt:
		line(fmt::format(fmt::runtime(_("Arrows: {:d}% chance to slow for {:d} s")), ThrillOfTheHuntChance, sec(ThrillOfTheHuntTicks)));
		break;
	case Skill::Resolve:
		line(fmt::format(fmt::runtime(_("Enemies you strike: damage -{:d}% for {:d} s")), ResolvePercent, sec(ResolveTicks)));
		break;
	case Skill::HotPursuit:
		haste(_("After landing a blow"), HotPursuitPercent, HotPursuitTicks);
		break;
	case Skill::TacticalAdvantage:
		haste(_("After slipping a blow"), TacticalAdvantagePercent, TacticalAdvantageTicks);
		break;
	case Skill::Illusionist:
		haste(fmt::format(fmt::runtime(_("Hit for {:d}% of your life or more")), IllusionistThresholdPercent), IllusionistPercent, IllusionistTicks);
		break;
	case Skill::FueledByDeath:
		haste(_("Each corpse used"), FueledByDeathPercent, FueledByDeathTicks);
		break;
	case Skill::CrusadersStride:
		line(fmt::format(fmt::runtime(_("While an aura burns: +{:d}% movement speed")), CrusadersStridePercent));
		break;
	case Skill::FleetFooted:
		line(std::string(_("You always run")));
		break;
	case Skill::Bloodthirst:
		line(fmt::format(fmt::runtime(_("Life returned: {:d}% of Rage spent")), BloodthirstPercent));
		break;
	case Skill::Transcendence:
		line(fmt::format(fmt::runtime(_("Life returned: {:d}% of mana spent")), BloodthirstPercent));
		break;
	case Skill::Wrathful:
		line(fmt::format(fmt::runtime(_("Life returned: {:d}% of mana spent")), WrathfulPercent));
		break;
	case Skill::Prodigy:
		line(fmt::format(fmt::runtime(_("Spells costing {:d} mana or less: +{:d} mana back")), mana(CheapSpellMana), mana(ProdigyMana)));
		break;
	case Skill::ChantOfResonance:
		line(fmt::format(fmt::runtime(_("Mantra mana cost: -{:d}%")), ChantOfResonancePercent));
		break;
	case Skill::CommanderOfTheRisenDead:
		line(fmt::format(fmt::runtime(_("Raise Skeleton and Skeletal Mage mana cost: -{:d}%")), CommanderPercent));
		break;
	case Skill::LifeFromDeath:
		line(fmt::format(fmt::runtime(_("A death within {:d} tiles: heals {:d}% of your life")), LifeFromDeathRange, shareOf(LifeFromDeathDivisor)));
		break;
	case Skill::PoundOfFlesh:
		line(fmt::format(fmt::runtime(_("Each kill: heals {:d}% of your life")), PoundOfFleshPercent));
		break;
	case Skill::BloodVengeance:
		line(fmt::format(fmt::runtime(_("Each kill: +{:d} mana")), mana(BloodVengeanceMana)));
		break;
	case Skill::Insurmountable:
		line(fmt::format(fmt::runtime(_("Each block: +{:d} mana")), mana(InsurmountableMana)));
		break;
	case Skill::Renewal:
		line(fmt::format(fmt::runtime(_("Each block: heals {:d}% of your life")), RenewalPercent));
		break;
	case Skill::BloodIsPower:
		line(fmt::format(fmt::runtime(_("Every {:d}% of your life lost: +1 Essence")), shareOf(BloodIsPowerDivisor)));
		break;
	case Skill::CustomEngineering:
		line(fmt::format(fmt::runtime(_("Rune traps: +{:d} spell levels")), CustomEngineeringLevels));
		line(fmt::format(fmt::runtime(_("Chance a rune is not used up: {:d}%")), 100 / RuneSpareOneIn));
		break;
	case Skill::Grenadier:
		line(fmt::format(fmt::runtime(_("Every {:d} arrows loosed: a fire grenade")), GrenadierEvery));
		line(fmt::format(fmt::runtime(_("Grenade spell level: {:d}")), std::max(player._pLevel / GrenadierLevelDivisor, 1)));
		break;
	case Skill::InspiringPresence:
		line(fmt::format(fmt::runtime(_("Warcry blessings last {:d}% longer")), InspiringPresenceDurationPercent - 100));
		line(fmt::format(fmt::runtime(_("Under a blessing: {:d}% of your life a second")), shareOf(InspiringPresenceDivisor)));
		break;
	case Skill::Brooding:
		line(fmt::format(fmt::runtime(_("Standing still {:.1f} s: {:d}% of your life a second")), stillSeconds, shareOf(BroodingDivisor)));
		break;
	case Skill::SereneMind:
		line(fmt::format(fmt::runtime(_("Standing still {:.1f} s: {:d}% of your mana a second")), stillSeconds, shareOf(SereneMindDivisor)));
		break;
	case Skill::DrawLife:
		line(fmt::format(fmt::runtime(_("{:.1f}% of your life a second per monster within {:d} tiles, up to {:d}")),
		    DrawLifePerMille / 10.0, DrawLifeRange, DrawLifeMaxCount));
		break;

	// ---- the saves ----
	case Skill::Indestructible:
	case Skill::NervesOfSteel:
	case Skill::Awareness:
		save(false);
		break;
	case Skill::NearDeathExperience:
		save(true);
		break;
	case Skill::UnstableAnomaly:
		save(false);
		line(fmt::format(fmt::runtime(_("The save throws back enemies within {:d} tiles")), AnomalyThrowTiles));
		break;
	case Skill::FinalService:
		line(fmt::format(fmt::runtime(_("Once a floor, with minions: a killing blow unmakes them and leaves you at {:d}% life")),
		    shareOf(FinalServiceLifeDivisor)));
		break;
	case Skill::RathmasShield:
		line(fmt::format(fmt::runtime(_("Once a floor, below {:d}% life: no damage for {:d} s")), shareOf(RathmaBelowDivisor), sec(RathmaTicks)));
		break;
	default:
		break;
	}
	return out;
}

} // namespace devilution::oracool
