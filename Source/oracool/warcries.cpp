#include "oracool/warcries.h"

#include <algorithm>
#include <array>
#include <cstdlib>

#include "engine/backbuffer_state.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/random.hpp"
#include "levels/gendung.h"
#include "items.h"
#include "missiles.h"
#include "monster.h"
#include "oracool/aura_field.h"
#include "oracool/chill.h"
#include "oracool/stat_sheet.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

using Skill = ClassTreeSkill;

constexpr int TicksPerSecond = 20;

// ---- the caster's buffs ----------------------------------------------------------------------

struct Buff {
	SpellID spell = SpellID::Invalid;
	int ticksLeft = 0;
	int rank = 0;
};

constexpr size_t MaxBuffs = 8;
std::array<std::array<Buff, MaxBuffs>, MAX_PLRS> Buffs;

Buff *FindBuff(const Player &player, SpellID spell)
{
	for (Buff &buff : Buffs[player.getId()]) {
		if (buff.spell == spell && buff.ticksLeft > 0)
			return &buff;
	}
	return nullptr;
}

/** @brief Whether @p spell changes the sheet, and so wants a recompute when it starts and stops. */
bool IsSheetBuff(SpellID spell)
{
	return IsAnyOf(spell, SpellID::Shout, SpellID::BattleOrders, SpellID::BattleCommand, SpellID::PurifyingBreath, SpellID::Vengeance);
}

/** @brief Starts or refreshes a buff. False if the caster already carries it at (nearly) full length. */
bool StartBuff(Player &player, SpellID spell, int rank, int ticks)
{
	Buff *slot = FindBuff(player, spell);
	if (slot != nullptr) {
		// Recasting a buff you carry refreshes it - but not while it is still nearly full, which
		// would be paying for nothing.
		if (slot->ticksLeft > ticks * 9 / 10)
			return false;
		slot->ticksLeft = ticks;
		slot->rank = rank;
		return true;
	}
	for (Buff &buff : Buffs[player.getId()]) {
		if (buff.ticksLeft <= 0) {
			buff = { spell, ticks, rank };
			if (IsSheetBuff(spell))
				CalcPlrInv(player, false);
			return true;
		}
	}
	return false;
}

// ---- the monsters' debuffs -------------------------------------------------------------------

struct Debuff {
	int ticksLeft = 0;
	int damagePercent = 0; // negative
	int armorPercent = 0;  // negative
	int convertTicks = 0;  // Conversion: ticks left on the Paladin's side
};

std::array<Debuff, MaxMonsters> Debuffs;

Debuff &DebuffOf(const Monster &monster)
{
	return Debuffs[monster.getId()];
}

} // namespace

void DebuffMonster(const Monster &monster, int ticks, int damagePercent, int armorPercent)
{
	Debuff &debuff = DebuffOf(monster);
	// The stronger of the two, not the sum: a second cry over the first lengthens it, and if it is
	// the deeper cry it deepens it, but two cries are not twice one.
	debuff.ticksLeft = std::max(debuff.ticksLeft, ticks);
	debuff.damagePercent = std::min(debuff.damagePercent, damagePercent);
	debuff.armorPercent = std::min(debuff.armorPercent, armorPercent);
}

namespace {

// ---- who hears a cry -------------------------------------------------------------------------

int RankOf(const Player &player, SpellID spell)
{
	return std::max(player.GetSpellLevel(spell), 1);
}

int EarshotFor(int rank)
{
	return AuraRadiusForPoints(rank);
}

bool ShrugsOff(const Monster &monster)
{
	return monster.isUnique() || monster.lesserAffix != LesserUniqueAffix::None || monster.type().type == MT_DIABLO;
}

/** @brief Every live monster within @p radius tiles of @p centre, through @p fn; how many there were. */
template <typename Fn>
int ForEachInEarshot(Point centre, int radius, Fn fn)
{
	// Read off the tile map rather than the active list, the way Sanctuary's field is - it is the
	// map a cry is shouted across, and it is what a test can stand a monster on.
	int heard = 0;
	for (int y = centre.y - radius; y <= centre.y + radius; y++) {
		for (int x = centre.x - radius; x <= centre.x + radius; x++) {
			const Point tile { x, y };
			if (!InDungeonBounds(tile))
				continue;
			const int id = dMonster[x][y];
			if (id == 0)
				continue;
			Monster &monster = Monsters[std::abs(id) - 1];
			if (monster.position.tile != tile)
				continue; // the second tile of a monster mid-step: counted where it stands
			if (monster.hitPoints >> 6 <= 0 || monster.isPlayerMinion())
				continue;
			if (centre.WalkingDistance(tile) > radius)
				continue;
			fn(monster);
			heard++;
		}
	}
	return heard;
}

void Repel(Monster &monster, Point from, int distance)
{
	if (ShrugsOff(monster) || monster.mode == MonsterMode::Petrified)
		return;
	// Sanctuary's channel: MonsterGoal::Retreat, which the AI clears itself when the retreat ends.
	monster.goal = MonsterGoal::Retreat;
	monster.goalVar1 = static_cast<int16_t>(distance);
	monster.goalVar2 = static_cast<int8_t>(GetDirection(from, monster.position.tile));
}

void Stagger(Monster &monster, int ticks)
{
	if (ShrugsOff(monster) || monster.mode == MonsterMode::Petrified)
		return;
	StunMonster(monster, ticks);
}

void Strike(Player &player, Monster &monster, int damage)
{
	if (damage <= 0 || monster.hitPoints >> 6 <= 0)
		return;
	ApplyMonsterDamage(DamageType::Magic, monster, damage);
	if (monster.hitPoints >> 6 <= 0)
		M_StartKill(monster, player);
	else
		M_StartHit(monster, player, damage);
}

int Roll(int min, int max)
{
	return (min + GenerateRnd(std::max(max - min, 0) + 1)) << 6;
}

} // namespace

bool IsWarcry(SpellID spell)
{
	return WarcrySkill(spell) != Skill::None;
}

Skill WarcrySkill(SpellID spell)
{
	switch (spell) {
	case SpellID::Howl:
		return Skill::Howl;
	case SpellID::Taunt:
		return Skill::Taunt;
	case SpellID::Shout:
		return Skill::Shout;
	case SpellID::BattleCry:
		return Skill::BattleCry;
	case SpellID::BattleOrders:
		return Skill::BattleOrders;
	case SpellID::WarCry:
		return Skill::WarCry;
	case SpellID::BattleCommand:
		return Skill::BattleCommand;
	case SpellID::Lullaby:
		return Skill::Lullaby;
	case SpellID::SoundShock:
		return Skill::SoundShock;
	case SpellID::BardShout:
		return Skill::BardShout;
	case SpellID::Daze:
		return Skill::Daze;
	case SpellID::TempleBell:
		return Skill::TempleBell;
	case SpellID::PurifyingBreath:
		return Skill::PurifyingBreath;
	case SpellID::Tranquility:
		return Skill::Tranquility;
	case SpellID::InnerSight:
		return Skill::InnerSight;
	case SpellID::SlowMissiles:
		return Skill::SlowMissiles;
	case SpellID::Vengeance:
		return Skill::Vengeance;
	case SpellID::Conversion:
		return Skill::Conversion;
	default:
		return Skill::None;
	}
}

bool CastWarcry(Player &player, SpellID spell)
{
	return CastWarcry(player, spell, player.position.tile);
}

bool CastWarcry(Player &player, SpellID spell, Point target)
{
	if (!IsWarcry(spell))
		return false;
	const int rank = RankOf(player, spell);
	const Point here = player.position.tile;
	const int earshot = EarshotFor(rank);
	const int seconds = TicksPerSecond;

	switch (spell) {
	// --- the Barbarian's cries ---
	case SpellID::Howl:
		return ForEachInEarshot(here, earshot, [&](Monster &m) { Repel(m, here, 4 + rank); }) > 0;
	case SpellID::Taunt:
		return ForEachInEarshot(here, earshot + 2, [&](Monster &m) {
			if (m.mode == MonsterMode::Petrified)
				return;
			m.activeForTicks = UINT8_MAX;
			m.enemy = static_cast<uint8_t>(player.getId());
			m.enemyPosition = player.position.tile;
			m.flags &= ~MFLAG_TARGETS_MONSTER;
			m.goal = MonsterGoal::Normal;
		}) > 0;
	case SpellID::Shout:
		return StartBuff(player, spell, rank, (40 + 5 * (rank - 1)) * seconds);
	case SpellID::BattleCry:
		return ForEachInEarshot(here, earshot, [&](Monster &m) {
			DebuffMonster(m, (24 + 2 * (rank - 1)) * seconds, -(25 + 2 * (rank - 1)), -(25 + 2 * (rank - 1)));
		}) > 0;
	case SpellID::BattleOrders:
		return StartBuff(player, spell, rank, (40 + 5 * (rank - 1)) * seconds);
	case SpellID::WarCry:
		return ForEachInEarshot(here, earshot, [&](Monster &m) {
			Strike(player, m, Roll(4 * rank, 8 * rank));
			Stagger(m, 2 * seconds + 4 * (rank - 1));
		}) > 0;
	case SpellID::BattleCommand:
		return StartBuff(player, spell, rank, (30 + 5 * (rank - 1)) * seconds);

	// --- the Bard's ---
	case SpellID::Lullaby:
		return ForEachInEarshot(here, earshot, [&](Monster &m) { Stagger(m, 4 * seconds + 10 * (rank - 1)); }) > 0;
	case SpellID::SoundShock: {
		// The three tiles ahead: the one in front and the two beside it.
		const Point ahead = here + player._pdir;
		int struck = 0;
		ForEachInEarshot(ahead, 1, [&](Monster &m) {
			if (here.WalkingDistance(m.position.tile) != 1)
				return;
			Strike(player, m, Roll(4 + 2 * rank, 10 + 4 * rank));
			Stagger(m, seconds / 2);
			struck++;
		});
		return struck > 0;
	}
	case SpellID::BardShout:
		return ForEachInEarshot(here, 3, [&](Monster &m) { Stagger(m, seconds + 4 * (rank - 1)); }) > 0;
	case SpellID::Daze:
		return ForEachInEarshot(here, earshot, [&](Monster &m) {
			if (ShrugsOff(m) || m.mode == MonsterMode::Petrified)
				return;
			m.goal = MonsterGoal::Retreat;
			m.goalVar1 = 3;
			m.goalVar2 = static_cast<int8_t>(GenerateRnd(8));
		}) > 0;

	// --- the Monk's ---
	case SpellID::TempleBell:
		return ForEachInEarshot(here, earshot, [&](Monster &m) {
			if (m.data().monsterClass != MonsterClass::Undead)
				return;
			Strike(player, m, Roll(3 * rank, 6 * rank));
			Stagger(m, seconds + seconds / 2);
			Repel(m, here, 3);
		}) > 0;
	case SpellID::PurifyingBreath:
		return StartBuff(player, spell, rank, (30 + 5 * (rank - 1)) * seconds);
	case SpellID::Tranquility:
		return StartBuff(player, spell, rank, (12 + rank) * seconds);

	// --- the Rogue's ---
	case SpellID::InnerSight:
		return ForEachInEarshot(here, earshot + 2, [&](Monster &m) {
			DebuffMonster(m, (20 + 2 * (rank - 1)) * seconds, 0, -(30 + 2 * (rank - 1)));
		}) > 0;
	case SpellID::SlowMissiles:
		return StartBuff(player, spell, rank, (20 + 4 * (rank - 1)) * seconds);

	// --- the Paladin's ---
	case SpellID::Vengeance:
		return StartBuff(player, spell, rank, (30 + 5 * (rank - 1)) * seconds);
	case SpellID::Conversion: {
		// One enemy near the cursor turns to the Paladin's side for a while - the same flags the
		// engine's Berserk sets, and the same exemptions, but with a clock, which is what the first
		// attempt lacked and why it was withdrawn. Its own strength is left as it is.
		Monster *turned = nullptr;
		ForEachInEarshot(target, 2, [&](Monster &m) {
			if (turned != nullptr || ShrugsOff(m) || m.ai == MonsterAIID::Diablo)
				return;
			if ((m.flags & MFLAG_BERSERK) != 0 || (m.resistance & IMMUNE_MAGIC) != 0)
				return;
			if (IsAnyOf(m.mode, MonsterMode::FadeIn, MonsterMode::FadeOut, MonsterMode::Charge, MonsterMode::Petrified))
				return;
			turned = &m;
		});
		if (turned == nullptr)
			return false;
		turned->flags |= MFLAG_BERSERK | MFLAG_GOLEM;
		DebuffOf(*turned).convertTicks = (20 + 2 * (rank - 1)) * seconds;
		return true;
	}
	default:
		return false;
	}
}

int WarcryBuffTicks(const Player &player, SpellID spell)
{
	const Buff *buff = FindBuff(player, spell);
	return buff == nullptr ? 0 : buff->ticksLeft;
}

void ApplyWarcryBuffsToTotals(const Player &player, ItemBonusTotals &totals)
{
	for (const Buff &buff : Buffs[player.getId()]) {
		if (buff.ticksLeft <= 0)
			continue;
		const int rank = std::max(buff.rank, 1);
		switch (buff.spell) {
		case SpellID::Shout:
			totals.bonusArmor += 50 + 10 * (rank - 1);
			break;
		case SpellID::BattleOrders:
			// Flat, in the 1/64 units the life and mana fields keep: twenty points, ten more a rank.
			totals.hitPoints += (20 + 10 * (rank - 1)) << 6;
			totals.mana += (20 + 10 * (rank - 1)) << 6;
			break;
		case SpellID::BattleCommand:
			totals.spellLevelAdd += 1;
			break;
		case SpellID::PurifyingBreath:
			totals.fireResist += 20 + 5 * (rank - 1);
			totals.lightningResist += 20 + 5 * (rank - 1);
			totals.magicResist += 20 + 5 * (rank - 1);
			break;
		case SpellID::Vengeance:
			totals.fireMin += 2 + rank;
			totals.fireMax += 6 + 2 * rank;
			totals.lightningMin += 1 + rank;
			totals.lightningMax += 8 + 2 * rank;
			break;
		default:
			break;
		}
	}
}

bool SlowMissilesTurnsAside(const Player &player)
{
	const Buff *buff = FindBuff(player, SpellID::SlowMissiles);
	if (buff == nullptr)
		return false;
	return GenerateRnd(100) < std::min(50 + 5 * (buff->rank - 1), 80);
}

int MonsterDebuffDamagePercent(const Monster &monster)
{
	int percent = 0;
	const Debuff &debuff = DebuffOf(monster);
	if (debuff.ticksLeft > 0)
		percent += debuff.damagePercent;
	if (const int p = AuraPointsOn(monster, Skill::DirgeOfDread); p > 0)
		percent -= std::min(15 + 2 * (p - 1), 40);
	return std::max(percent, -75);
}

int MonsterDebuffArmorPercent(const Monster &monster)
{
	int percent = 0;
	const Debuff &debuff = DebuffOf(monster);
	if (debuff.ticksLeft > 0)
		percent += debuff.armorPercent;
	if (const int p = AuraPointsOn(monster, Skill::Discord); p > 0)
		percent -= std::min(20 + 2 * (p - 1), 50);
	return std::max(percent, -90);
}

int MonsterDebuffToHit(const Monster &monster)
{
	if (const int p = AuraPointsOn(monster, Skill::Weaken); p > 0)
		return std::min(20 + 2 * (p - 1), 50);
	return 0;
}

int EffectiveMonsterArmor(const Monster &monster)
{
	const int armor = PackAdjustedArmor(monster);
	return armor + armor * MonsterDebuffArmorPercent(monster) / 100;
}

void ProcessWarcriesTick(Player &player)
{
	// The buffs run down; a sheet buff that just ended takes its numbers with it.
	for (Buff &buff : Buffs[player.getId()]) {
		if (buff.ticksLeft <= 0)
			continue;
		if (--buff.ticksLeft == 0 && IsSheetBuff(buff.spell))
			CalcPlrInv(player, false);
	}

	if (&player != MyPlayer || player._pHitPoints <= 0)
		return;

	// The monsters' debuffs run down too - once, from the local player's tick - and a converted
	// monster goes back to its own side when its clock runs out.
	for (size_t i = 0; i < Debuffs.size(); i++) {
		Debuff &debuff = Debuffs[i];
		if (debuff.ticksLeft > 0 && --debuff.ticksLeft == 0) {
			debuff.damagePercent = 0;
			debuff.armorPercent = 0;
		}
		if (debuff.convertTicks > 0 && --debuff.convertTicks == 0)
			Monsters[i].flags &= ~(MFLAG_BERSERK | MFLAG_GOLEM);
	}

	// Tranquility: the ground around the Monk is a sanctuary - what stands beside him is slowed,
	// and every second a fiftieth of his life returns.
	if (const Buff *tranquility = FindBuff(player, SpellID::Tranquility); tranquility != nullptr) {
		ForEachInEarshot(player.position.tile, 2, [&](Monster &m) { ChillMonster(m, 3); });
		if (tranquility->ticksLeft % TicksPerSecond == 0 && player._pHitPoints < player._pMaxHP) {
			const int heal = player._pMaxHP / 50;
			player._pHitPoints = std::min(player._pHitPoints + heal, player._pMaxHP);
			player._pHPBase = std::min(player._pHPBase + heal, player._pMaxHPBase);
			RedrawComponent(PanelDrawComponent::Health);
		}
	}

	// The held auras that have to PUSH or CHILL rather than be asked: Holy Freeze and Weaken chill
	// what stands in them, Dirge of Dread repels the undaunted. Re-set every tick while the monster
	// is in the field, so stepping out ends it without anything to undo - Sanctuary's rule.
	const Skill aura = GetActiveClassAura(player);
	if (aura == Skill::None)
		return;
	const int points = ClassTreeInvestment(player, aura);
	if (points <= 0 || !IsClassTreeSkillUnlocked(player, aura))
		return;
	const int radius = AuraRadiusForPoints(points);
	if (aura == Skill::HolyFreeze || aura == Skill::Weaken) {
		ForEachInEarshot(player.position.tile, radius, [&](Monster &m) { ChillMonster(m, 3); });
	} else if (aura == Skill::DirgeOfDread) {
		ForEachInEarshot(player.position.tile, radius, [&](Monster &m) { Repel(m, player.position.tile, 4); });
	}
}

void ClearWarcries()
{
	for (auto &perPlayer : Buffs)
		perPlayer.fill(Buff {});
	Debuffs.fill(Debuff {});
}

const char *WarcryDescription(SpellID spell)
{
	switch (spell) {
	case SpellID::Howl:
		return N_("A howl that sends everything in earshot running - four tiles and a tile more a rank. Uniques hold their ground.");
	case SpellID::Taunt:
		return N_("A goad that wakes everything in earshot and turns it on you.");
	case SpellID::Shout:
		return N_("A bellow that hardens you: half again your armour, a tenth more a rank, for forty seconds and five more a rank.");
	case SpellID::BattleCry:
		return N_("A cry that leaves what hears it a quarter weaker in blow and in armour, for twenty-four seconds.");
	case SpellID::BattleOrders:
		return N_("A shout that swells your life and mana by twenty, ten more a rank, for forty seconds and five more a rank.");
	case SpellID::WarCry:
		return N_("A shout that strikes everything in earshot for four to eight a rank and leaves it reeling for two seconds. Uniques shrug off the reeling.");
	case SpellID::BattleCommand:
		return N_("A command that deepens every skill you have by a rank, for thirty seconds and five more a rank.");
	case SpellID::Lullaby:
		return N_("A song that leaves everything in earshot standing asleep for four seconds, half a second more a rank, until it is struck. Uniques do not sleep.");
	case SpellID::SoundShock:
		return N_("A burst of sound through the three tiles ahead, for four to ten and two to four more a rank, that staggers what it strikes.");
	case SpellID::BardShout:
		return N_("A shout that leaves everything within three tiles reeling for a second, a fifth more a rank. Uniques shrug it off.");
	case SpellID::Daze:
		return N_("A verse that sends everything in earshot stumbling off in a direction of its own. Uniques keep their feet.");
	case SpellID::TempleBell:
		return N_("A tone that strikes every undead in earshot for three to six a rank, staggers it and drives it back.");
	case SpellID::PurifyingBreath:
		return N_("Centre yourself: twenty to every resistance, five more a rank, for thirty seconds and five more a rank.");
	case SpellID::Tranquility:
		return N_("A sanctuary about you for twelve seconds and one more a rank: what stands beside you is slowed, and a fiftieth of your life returns each second.");
	case SpellID::InnerSight:
		return N_("Reveals the weak points of everything in earshot: a third of its armour gone, two percent more a rank, for twenty seconds.");
	case SpellID::SlowMissiles:
		return N_("For twenty seconds, four more a rank, half the arrows aimed at you turn aside - a twentieth more a rank.");
	case SpellID::Vengeance:
		return N_("Your blows burn and crackle for thirty seconds, five more a rank: fire and lightning on every hit, more with rank. Cold has no place on the weapon sheet, so it is not added.");
	case SpellID::Conversion:
		return N_("Turns one enemy near the cursor to your side for twenty seconds, two more a rank. Uniques and the magic-immune refuse.");
	default:
		return "";
	}
}

void AddWarcry(Missile &missile, AddMissileParameter &parameter)
{
	missile._miDelFlag = true;
	Player &player = Players[missile._misource];
	// Which cry: the spell the cast was launched with, which the player carries through the
	// animation. One missile for all seventeen rather than seventeen missiles.
	if (!CastWarcry(player, player.executedSpell.spellId, parameter.dst))
		parameter.spellFizzled = true;
}

} // namespace devilution::oracool
