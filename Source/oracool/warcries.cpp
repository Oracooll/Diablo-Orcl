#include "oracool/warcries.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>

#include "engine/backbuffer_state.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/random.hpp"
#include "levels/gendung.h"
#include "dead.h"
#include "effects.h"
#include "items.h"
#include "missiles.h"
#include "monster.h"
#include "oracool/aura_field.h"
#include "oracool/chill.h"
#include "oracool/passives.h"
#include "oracool/stat_sheet.h"
#include "oracool/rfa12_actives.h"
#include "oracool/rfa12_effects.h"
#include "oracool/skill_sounds.h" // the Impact cues of Conversion and Vengeance
#include "player.h"
#include "utils/language.h"
#include <fmt/format.h>

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
	// Inspiring Presence (Barbarian, 2026-09-14): the blessing lasts twice as long.
	ticks = ticks * PassiveWarcryDurationPercent(player) / 100;
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

/** @brief Taunt and Inner Sight carry two tiles past the other cries. The cast and the tooltip both read this. */
int FarEarshotFor(int rank)
{
	return EarshotFor(rank) + 2;
}

// Tranquility: what stands within TranquilityReach is slowed, and TranquilityHealPercent of the Monk's
// life returns each second. Neither grows; the duration does.
constexpr int TranquilityReach = 2;
constexpr int TranquilityHealPercent = 2;

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

// ---- corpses (Round 9) ------------------------------------------------------------------------

/** @brief The nearest corpse within @p radius of @p centre, if any. */
std::optional<Point> CorpseNear(Point centre, int radius)
{
	std::optional<Point> best;
	int bestDistance = radius + 1;
	for (int y = centre.y - radius; y <= centre.y + radius; y++) {
		for (int x = centre.x - radius; x <= centre.x + radius; x++) {
			const Point tile { x, y };
			if (!InDungeonBounds(tile) || dCorpse[x][y] == 0)
				continue;
			const int distance = centre.WalkingDistance(tile);
			if (distance < bestDistance) {
				bestDistance = distance;
				best = tile;
			}
		}
	}
	return best;
}

/** @brief The corpse at @p tile is used up: it leaves the map, and nothing can find it again. */
void ConsumeCorpse(Point tile)
{
	dCorpse[tile.x][tile.y] = 0;
}

/** @brief Grim Ward: one totem per player - where it stands, how far it reaches, how long it lasts. */
struct Ward {
	Point position;
	int radius = 0;
	int ticksLeft = 0;
};

std::array<Ward, MAX_PLRS> Wards;

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
	case SpellID::FindPotion:
		return Skill::FindPotion;
	case SpellID::FindItem:
		return Skill::FindItem;
	case SpellID::GrimWard:
		return Skill::GrimWard;
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
		return ForEachInEarshot(here, FarEarshotFor(rank), [&](Monster &m) {
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
		return ForEachInEarshot(here, FarEarshotFor(rank), [&](Monster &m) {
			DebuffMonster(m, (20 + 2 * (rank - 1)) * seconds, 0, -(30 + 2 * (rank - 1)));
		}) > 0;
	case SpellID::SlowMissiles:
		return StartBuff(player, spell, rank, (20 + 4 * (rank - 1)) * seconds);

	// --- the Paladin's ---
	case SpellID::Vengeance:
		return StartBuff(player, spell, rank, (30 + 5 * (rank - 1)) * seconds);
	// --- the corpse cries (Round 9) ---
	case SpellID::FindPotion: {
		// A corpse near the cursor is searched and used up. A potion, more often with rank; a full
		// one rarely. Nothing found is still a search - the corpse is gone either way.
		const std::optional<Point> corpse = CorpseNear(target, 2);
		if (!corpse)
			return false;
		ConsumeCorpse(*corpse);
		if (GenerateRnd(100) < std::min(50 + 5 * (rank - 1), 90)) {
			const bool full = GenerateRnd(100) < 5 + 2 * (rank - 1);
			const bool mana = FlipCoin();
			const int kind = full ? (mana ? IMISC_FULLMANA : IMISC_FULLHEAL) : (mana ? IMISC_MANA : IMISC_HEAL);
			CreateTypeItem(*corpse, false, ItemType::Misc, kind, true, false);
		}
		return true;
	}
	case SpellID::FindItem: {
		const std::optional<Point> corpse = CorpseNear(target, 2);
		if (!corpse)
			return false;
		ConsumeCorpse(*corpse);
		if (GenerateRnd(100) < std::min(25 + 5 * (rank - 1), 60))
			CreateRndItem(*corpse, false, true, false);
		return true;
	}
	case SpellID::GrimWard: {
		// The corpse becomes a totem of terror: for a while, everything but the uniques that comes
		// within its reach turns and runs. One ward at a time; a second replaces the first.
		const std::optional<Point> corpse = CorpseNear(target, 2);
		if (!corpse)
			return false;
		ConsumeCorpse(*corpse);
		Wards[player.getId()] = { *corpse, earshot, (20 + 2 * (rank - 1)) * seconds };
		return true;
	}
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
		// Its Impact cue, when one turns (audit, 2026-09-29: the sound page's pick was never played).
		if (&player == MyPlayer)
			PlaySkillSound(WarcrySkill(spell), SkillSoundEvent::Impact);
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
			// Flat, in the 1/64 units the life field keeps: twenty points, ten more a rank. Life only -
			// the mana half went with the Barbarian's mana (2026-09-13, oracool/rage.h).
			totals.hitPoints += (20 + 10 * (rank - 1)) << 6;
			break;
		case SpellID::BattleCommand:
			totals.spellLevelAdd += 1;
			break;
		case SpellID::PurifyingBreath:
			totals.fireResist += 20 + 5 * (rank - 1);
			totals.lightningResist += 20 + 5 * (rank - 1);
			totals.magicResist += 20 + 5 * (rank - 1);
			totals.coldResist += 20 + 5 * (rank - 1);
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

/** @brief How long a Vengeance blow's cold holds its target chilled: a second, renewed by the next blow. */
constexpr int VengeanceChillTicks = TicksPerSecond;

int VengeanceColdMin(int rank) { return 1 + rank; }
int VengeanceColdMax(int rank) { return 5 + 2 * rank; }

void ApplyVengeanceCold(Player &player, Monster &monster)
{
	const Buff *buff = FindBuff(player, SpellID::Vengeance);
	if (buff == nullptr || (monster.hitPoints >> 6) <= 0 || monster.isPlayerMinion())
		return;
	const int rank = std::max(buff->rank, 1);
	int damage = Roll(VengeanceColdMin(rank), VengeanceColdMax(rank));
	damage += damage * Rfa12ColdDamagePercent(monster) / 100;
	if (monster.isResistant(MissileID::Null, DamageType::Cold))
		damage >>= 2;
	if (damage <= 0)
		return;
	ApplyMonsterDamage(DamageType::Cold, monster, damage);
	// Vengeance's Impact cue on the blow its cold rides (audit, 2026-09-29: the sound page's pick was never played).
	if (&player == MyPlayer)
		PlaySkillSound(WarcrySkill(SpellID::Vengeance), SkillSoundEvent::Impact);
	if ((monster.hitPoints >> 6) <= 0) {
		M_StartKill(monster, player);
		return;
	}
	ChillMonster(monster, VengeanceChillTicks);
	AddColdHitFlash(monster.position.tile, static_cast<int>(player.getId()), 50); // half size (the Paladin Skill Cards page, 2026-09-28)
}

bool SlowMissilesTurnsAside(const Player &player)
{
	const Buff *buff = FindBuff(player, SpellID::SlowMissiles);
	if (buff == nullptr)
		return false;
	return GenerateRnd(100) < std::min(50 + 5 * (buff->rank - 1), 80);
}

bool IsMonsterConverted(const Monster &monster)
{
	constexpr uint32_t Both = MFLAG_BERSERK | MFLAG_GOLEM;
	return (monster.flags & Both) == Both && DebuffOf(monster).convertTicks > 0;
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
	// Conviction lowers armour as well as resistances since 2026-09-12: 3% a level, to 60%.
	if (const int p = AuraPointsOn(monster, Skill::Conviction); p > 0)
		percent -= ConvictionArmorCutPercent(p);
	return std::max(percent, -90);
}

int RedemptionSharePercent(int points)
{
	return 2 + points;
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
	// Condemnation (RfA-12) strips a share on top of what a cry has.
	return armor + armor * (MonsterDebuffArmorPercent(monster) - Rfa12MonsterArmorCutPercent(monster)) / 100;
}

bool AnyWarcryBuffActive(const Player &player)
{
	for (const Buff &buff : Buffs[player.getId()]) {
		if (buff.ticksLeft > 0)
			return true;
	}
	return false;
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

	// Grim Ward: the totem repels while it stands.
	if (Ward &ward = Wards[player.getId()]; ward.ticksLeft > 0) {
		ward.ticksLeft--;
		ForEachInEarshot(ward.position, ward.radius, [&](Monster &m) { Repel(m, ward.position, 4); });
	}

	// Tranquility: the ground around the Monk is a sanctuary - what stands beside him is slowed,
	// and every second a fiftieth of his life returns.
	if (const Buff *tranquility = FindBuff(player, SpellID::Tranquility); tranquility != nullptr) {
		ForEachInEarshot(player.position.tile, TranquilityReach, [&](Monster &m) { ChillMonster(m, 3); });
		if (tranquility->ticksLeft % TicksPerSecond == 0 && player._pHitPoints < player._pMaxHP) {
			const int heal = player._pMaxHP * TranquilityHealPercent / 100;
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
	// Holy Freeze left this list on 2026-09-12: it pulses cold damage now, and the cold hit chills
	// (aura_field.cpp's ProcessHolyPulse).
	if (aura == Skill::Weaken) {
		ForEachInEarshot(player.position.tile, radius, [&](Monster &m) { ChillMonster(m, 3); });
	} else if (aura == Skill::DirgeOfDread) {
		ForEachInEarshot(player.position.tile, radius, [&](Monster &m) { Repel(m, player.position.tile, 4); });
	} else if (aura == Skill::Redemption) {
		// Once a second, the nearest corpse in the field is consumed for life and mana: a fiftieth
		// of each, and a hundredth more a point.
		static int redemptionClock = 0;
		if (++redemptionClock % TicksPerSecond == 0) {
			if (const std::optional<Point> corpse = CorpseNear(player.position.tile, radius); corpse) {
				ConsumeCorpse(*corpse);
				// Seen and heard (dev note, 2026-09-27): a beam rises where the corpse lay, to the Resurrect spell's own cast
				// sound. Since the Paladin Skill Cards page (2026-09-28) it is vanilla's Resurrect at a quarter size, tinted
				// infrared, its foot on the corpse - in place of the red-and-blue Redemption Rise sheet.
				if (Missile *rise = AddArtEffect(*corpse, MissileGraphicID::Resurrect, static_cast<int>(player.getId())); rise != nullptr) {
					rise->oracoolTint = Tint::Hue;
					rise->oracoolTintRgb = hue::Infrared;
					ScaleMissile(*rise, 25, 0);
				}
				PlaySfxLoc(LS_RESUR, *corpse);
				// And the generic cast under it (user, the sound review, 2026-09-28: "on successful redemption from a corpse
				// during the resurrection animation also play Generic cast (cast8)").
				PlaySfxLoc(IS_CAST8, *corpse);
				const int share = RedemptionSharePercent(points);
				const int heal = player._pMaxHP * share / 100;
				const int gain = player._pMaxMana * share / 100;
				player._pHitPoints = std::min(player._pHitPoints + heal, player._pMaxHP);
				player._pHPBase = std::min(player._pHPBase + heal, player._pMaxHPBase);
				if (HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
					player._pMana = std::min(player._pMana + gain, player._pMaxMana);
					player._pManaBase = std::min(player._pManaBase + gain, player._pMaxManaBase);
				}
				RedrawComponent(PanelDrawComponent::Health);
				RedrawComponent(PanelDrawComponent::Mana);
			}
		}
	}
}

void ClearWarcryStateForMonster(const Monster &monster)
{
	const size_t id = monster.getId();
	if (id < Debuffs.size())
		Debuffs[id] = Debuff {};
}

void RevertConversionsForLevelSave()
{
	for (size_t i = 0; i < Debuffs.size(); i++) {
		if (Debuffs[i].convertTicks <= 0)
			continue;
		Monsters[i].flags &= ~(MFLAG_BERSERK | MFLAG_GOLEM);
		Debuffs[i].convertTicks = 0;
	}
}

void ClearWarcries()
{
	// The monsters' side only: debuffs, conversions and wards belong to the level that is ending. The
	// caster's buffs are the caster's, and walk down the stairs with him - see ClearWarcryBuffs.
	Debuffs.fill(Debuff {});
	Wards.fill(Ward {});
}

void ClearWarcryBuffs(Player &player)
{
	bool sheetMoved = false;
	for (Buff &buff : Buffs[player.getId()]) {
		if (buff.ticksLeft > 0 && IsSheetBuff(buff.spell))
			sheetMoved = true;
		buff = {};
	}
	// A wiped sheet buff has to take its numbers with it, or the last character's Shout would
	// stay baked into this one's armour until something else recomputed the sheet.
	if (sheetMoved)
		CalcPlrInv(player, false);
}

const char *WarcryDescription(SpellID spell)
{
	switch (spell) {
	case SpellID::Howl:
		return N_("A howl that sends everything in earshot running - four tiles and a tile more a rank. Uniques hold their ground.");
	case SpellID::Taunt:
		return N_("A goad that wakes everything in earshot and turns it on you.");
	case SpellID::Shout:
		return N_("A bellow that hardens you: +50% armour, +10% per rank, for 40 seconds, +5 per rank.");
	case SpellID::BattleCry:
		return N_("A cry that leaves what hears it at -25% damage and -25% armour, for 24 seconds.");
	case SpellID::BattleOrders:
		return N_("A shout that swells your life by +20, +10 per rank, for 40 seconds, +5 per rank.");
	case SpellID::WarCry:
		return N_("A shout that strikes everything in earshot for four to eight a rank and leaves it reeling for two seconds. Uniques shrug off the reeling.");
	case SpellID::BattleCommand:
		return N_("A command that deepens every skill you have by a rank, for thirty seconds and five more a rank.");
	case SpellID::Lullaby:
		return N_("A song that leaves everything in earshot standing asleep for 4 seconds, +0.5 per rank, until it is struck. Uniques do not sleep.");
	case SpellID::SoundShock:
		return N_("A burst of sound through the three tiles ahead, for four to ten and two to four more a rank, that staggers what it strikes.");
	case SpellID::BardShout:
		return N_("A shout that leaves everything within three tiles reeling for 1 second, +20% per rank. Uniques shrug it off.");
	case SpellID::Daze:
		return N_("A verse that sends everything in earshot stumbling off in a direction of its own. Uniques keep their feet.");
	case SpellID::TempleBell:
		return N_("A tone that strikes every undead in earshot for three to six a rank, staggers it and drives it back.");
	case SpellID::PurifyingBreath:
		return N_("Centre yourself: +20 to every resistance, +5 per rank, for 30 seconds, +5 per rank.");
	case SpellID::Tranquility:
		return N_("A sanctuary about you for 13 seconds, +1 per rank: what stands beside you is slowed, and 2% of your life returns each second.");
	case SpellID::InnerSight:
		return N_("Reveals the weak points of everything in earshot: -30% armour, -2% more per rank, for 20 seconds.");
	case SpellID::SlowMissiles:
		return N_("For 20 seconds, +4 per rank, 50% of the arrows aimed at you turn aside, +5% per rank.");
	case SpellID::Vengeance:
		return N_("Your blows burn and crackle for thirty seconds, five more a rank: fire, lightning and cold on every hit, more with rank, and the cold chills.");
	case SpellID::Conversion:
		return N_("Turns one enemy near the cursor to your side for 20 seconds, +2 per rank. Uniques and the magic-immune refuse.");
	case SpellID::FindPotion:
		return N_("Search a corpse near the cursor. 50% of the time, +5% per rank, it yields a potion - rarely a full one. The corpse is used up.");
	case SpellID::FindItem:
		return N_("Search a corpse near the cursor. 25% of the time, +5% per rank, it yields an item. The corpse is used up.");
	case SpellID::GrimWard:
		return N_("Raise a corpse near the cursor as a totem of terror: for 20 seconds, +2 per rank, everything but the uniques that comes near it runs.");
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
	// ...and every cast RfA-12 active (2026-09-13), which rides the same missile - see rfa12_actives.h.
	const SpellID spell = player.executedSpell.spellId;
	const bool cast = CastWarcry(player, spell, parameter.dst) || CastRfa12Active(player, spell, parameter.dst);
	if (!cast)
		parameter.spellFizzled = true;
	// A cry's ring goes out on every cast, heard by anything or not (dev note, 2026-09-29: "warcray animation to play
	// every time warcray is cast, not only when mobs are around"); a cry that found no one still fizzles.
	if (!IsWarcry(spell) && !Rfa12CastLeavesRing(spell))
		return;
	// No ring under Vengeance or Conversion (the Paladin Skill Cards page, 2026-09-28: "Remove warcry ring from this skill").
	if (spell == SpellID::Vengeance || spell == SpellID::Conversion)
		return;
	// The shockwave on the floor (2026-09-11) - it removes itself while warcry_ring.png is absent.
	Missile *ring = AddMissile(player.position.tile, player.position.tile, player._pdir, MissileID::WarcryRing,
	    TARGET_MONSTERS, static_cast<int>(player.getId()), 0, 0);
	// Two cries ring wider (the Barbarian Skill Cards page, 2026-09-29): War Cry at 200%, Earthshaker Cry at 150%. The
	// ring's centre stays on the floor: it is 80px above the bottom of its 160px cell.
	const int ringPercent = spell == SpellID::WarCry ? 200 : spell == SpellID::EarthshakerCry ? 150 : 100;
	if (ring != nullptr && !ring->_miDelFlag && ringPercent != 100)
		ScaleMissile(*ring, ringPercent, 80);
}

std::string WarcryFactsAt(SpellID spell, int rank)
{
	// The facts, from the same formulas CastWarcry and ApplyWarcryBuffsToTotals run - kept in step
	// by being written beside them (user, 2026-09-05: "let them be known").
	rank = std::max(rank, 1);
	const int earshot = EarshotFor(rank);
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	const auto radius = [&](int tiles) { line(fmt::format(fmt::runtime(_("Radius: {:d} tiles")), tiles)); };
	const auto duration = [&](int seconds) { line(fmt::format(fmt::runtime(_("Duration: {:d} s")), seconds)); };
	switch (spell) {
	case SpellID::Howl:
		radius(earshot);
		line(fmt::format(fmt::runtime(_("Repels {:d} tiles")), 4 + rank));
		break;
	case SpellID::Taunt:
		radius(FarEarshotFor(rank));
		break;
	case SpellID::Daze:
		radius(earshot);
		break;
	case SpellID::Shout:
		duration(40 + 5 * (rank - 1));
		line(fmt::format(fmt::runtime(_("Armour: +{:d}%")), 50 + 10 * (rank - 1)));
		break;
	case SpellID::BattleCry:
		radius(earshot);
		duration(24 + 2 * (rank - 1));
		line(fmt::format(fmt::runtime(_("Enemy damage and armour: -{:d}%")), 25 + 2 * (rank - 1)));
		break;
	case SpellID::BattleOrders:
		duration(40 + 5 * (rank - 1));
		line(fmt::format(fmt::runtime(_("Life: +{:d}")), 20 + 10 * (rank - 1)));
		break;
	case SpellID::WarCry:
		radius(earshot);
		line(fmt::format(fmt::runtime(_("Damage: {:d} - {:d}")), 4 * rank, 8 * rank));
		line(fmt::format(fmt::runtime(_("Stun: {:.1f} s")), 2.0 + 0.2 * (rank - 1)));
		break;
	case SpellID::BattleCommand:
		duration(30 + 5 * (rank - 1));
		line(std::string(_("All skill levels: +1")));
		break;
	case SpellID::Lullaby:
		radius(earshot);
		line(fmt::format(fmt::runtime(_("Sleep: {:.1f} s")), 4.0 + 0.5 * (rank - 1)));
		break;
	case SpellID::SoundShock:
		line(std::string(_("Strikes the three tiles ahead")));
		line(fmt::format(fmt::runtime(_("Damage: {:d} - {:d}")), 4 + 2 * rank, 10 + 4 * rank));
		break;
	case SpellID::BardShout:
		radius(3);
		line(fmt::format(fmt::runtime(_("Stun: {:.1f} s")), 1.0 + 0.2 * (rank - 1)));
		break;
	case SpellID::TempleBell:
		radius(earshot);
		line(fmt::format(fmt::runtime(_("Damage to undead: {:d} - {:d}")), 3 * rank, 6 * rank));
		line(std::string(_("Stun: 1.5 s, repels 3 tiles")));
		break;
	case SpellID::PurifyingBreath:
		duration(30 + 5 * (rank - 1));
		line(fmt::format(fmt::runtime(_("Fire, lightning, cold and magic resistance: +{:d}")), 20 + 5 * (rank - 1)));
		break;
	case SpellID::Tranquility:
		radius(TranquilityReach);
		duration(12 + rank);
		line(fmt::format(fmt::runtime(_("Slows what stands in reach; heals {:d}% of your life a second")), TranquilityHealPercent));
		break;
	case SpellID::InnerSight:
		radius(FarEarshotFor(rank));
		duration(20 + 2 * (rank - 1));
		line(fmt::format(fmt::runtime(_("Enemy armour: -{:d}%")), 30 + 2 * (rank - 1)));
		break;
	case SpellID::SlowMissiles:
		duration(20 + 4 * (rank - 1));
		line(fmt::format(fmt::runtime(_("Arrows turned aside: {:d}%")), std::min(50 + 5 * (rank - 1), 80)));
		break;
	case SpellID::Vengeance:
		duration(30 + 5 * (rank - 1));
		line(fmt::format(fmt::runtime(_("Fire: +{:d} - {:d}")), 2 + rank, 6 + 2 * rank));
		line(fmt::format(fmt::runtime(_("Lightning: +{:d} - {:d}")), 1 + rank, 8 + 2 * rank));
		line(fmt::format(fmt::runtime(_("Cold: +{:d} - {:d}, and it chills")), VengeanceColdMin(rank), VengeanceColdMax(rank)));
		break;
	case SpellID::FindPotion:
		line(fmt::format(fmt::runtime(_("Chance: {:d}%")), std::min(50 + 5 * (rank - 1), 90)));
		line(fmt::format(fmt::runtime(_("Full potion: {:d}%")), 5 + 2 * (rank - 1)));
		break;
	case SpellID::FindItem:
		line(fmt::format(fmt::runtime(_("Chance: {:d}%")), std::min(25 + 5 * (rank - 1), 60)));
		break;
	case SpellID::GrimWard:
		radius(earshot);
		duration(20 + 2 * (rank - 1));
		break;
	case SpellID::Conversion:
		radius(2);
		duration(20 + 2 * (rank - 1));
		break;
	default:
		break;
	}
	return out;
}

} // namespace devilution::oracool
