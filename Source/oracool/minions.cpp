/**
 * @file oracool/minions.cpp
 *
 * See minions.h.
 */
#include "oracool/minions.h"

#include <algorithm>
#include <array>
#include <cstdlib>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine/palette.h"
#include "engine/rectangle.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/backbuffer_state.hpp"
#include "levels/gendung.h"
#include "monster.h"
#include "multi.h"
#include "engine/random.hpp"
#include "oracool/chill.h"
#include "oracool/class_tree.h"
#include "oracool/companion.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

/** Bodies re-formed around the owner in one tick after a level change - a crowd arriving, not a pop of thirty. */
constexpr int ReformPerTick = 4;

struct Record {
	bool active = false;
	/** The monster slot its body holds on this level, or -1 while it waits to re-form. */
	int body = -1;
	uint8_t owner = 0;
	MinionSpec spec {};
	/** Its life in 1/64 points, carried across levels. */
	int lifeNow = 0;
	/** Its place within its group's ring, 0-based, renumbered whenever the group changes. */
	int order = 0;
};

std::array<Record, MaxMinionBodies> Records;

/** Frenzy of the Dead, per owner: ticks left and the bonus. */
struct Frenzy {
	int ticks = 0;
	int percent = 0;
};
std::array<Frenzy, MAX_PLRS> Frenzies;
/** The Fire Golem burns what stands beside it once a second; this is the clock, per record. */
constexpr int FireGolemPulseTicks = 20;
/** Record index by monster slot, or -1 - the brain asks "is this a minion" for every monster, every tick. */
std::array<int8_t, MaxMonsters> RecordOfSlot = [] {
	std::array<int8_t, MaxMonsters> table {};
	table.fill(-1);
	return table;
}();

Record *RecordOf(const Monster &monster)
{
	if (&monster < &Monsters[0] || &monster >= &Monsters[0] + MaxMonsters)
		return nullptr;
	const int8_t index = RecordOfSlot[monster.getId()];
	return index < 0 ? nullptr : &Records[static_cast<size_t>(index)];
}

bool BodyAlive(const Record &record)
{
	if (record.body < 0)
		return false;
	const Monster &body = Monsters[record.body];
	return (body.hitPoints >> 6) > 0 && body.mode != MonsterMode::Death && !body.isInvalid;
}

/** @brief Living or waiting - what counts toward a group's cap. A body in its death animation does not. */
bool Counts(const Record &record)
{
	return record.active && (record.body < 0 || BodyAlive(record));
}

void Release(Record &record)
{
	if (record.body >= 0)
		RecordOfSlot[static_cast<size_t>(record.body)] = -1;
	record = {};
}

void Renumber(uint8_t owner, MinionGroup group)
{
	int order = 0;
	for (Record &record : Records) {
		if (Counts(record) && record.owner == owner && record.spec.group == group)
			record.order = order++;
	}
}

/**
 * @brief The ring a group stands on: the golem at the hero's shoulder, skeletons around him at two tiles, mages
 * outside them at three, the Revived outside those at four. Warriors in, casters out - D2's crowd, in rings
 * because a hero in this game turns on the spot and a "front" would swing thirty bodies round him each time.
 */
int RingRadius(MinionGroup group)
{
	switch (group) {
	case MinionGroup::Golem:
		return 1;
	case MinionGroup::Skeleton:
		return 2;
	case MinionGroup::Mage:
		return 3;
	case MinionGroup::Revived:
		return 4;
	}
	return 2;
}

/** @brief Tile @p step of the square ring of @p radius around the origin, walking its perimeter clockwise from the top-left. */
Displacement RingTile(int radius, int step)
{
	const int side = 2 * radius;
	step = ((step % (4 * side)) + 4 * side) % (4 * side);
	if (step < side)
		return { -radius + step, -radius };
	step -= side;
	if (step < side)
		return { radius, -radius + step };
	step -= side;
	if (step < side)
		return { radius - step, radius };
	step -= side;
	return { -radius, radius - step };
}

Point FormationHome(const Player &owner, const Record &record)
{
	const int radius = RingRadius(record.spec.group);
	const int perimeter = 8 * radius;
	const int members = std::max(MinionCount(owner, record.spec.group), 1);
	return owner.position.tile + RingTile(radius, record.order * perimeter / members);
}

bool SpawnBody(Record &record, const Player &owner, Point near)
{
	if (leveltype == DTYPE_TOWN)
		return false;
	if (!CanAddMinionBody(record.spec.type))
		return false; // the pool, or the level's sprite slots, are full: no tile would change that
	for (int radius = 1; radius <= 5; radius++) {
		for (int step = 0; step < 8 * radius; step++) {
			const Point tile = near + RingTile(radius, step);
			if (!InDungeonBounds(tile))
				continue;
			Monster *body = AddMinionBody(tile, GetDirection(tile, owner.position.tile), record.spec.type);
			if (body == nullptr)
				continue; // something stands there
			body->maxHitPoints = std::max(record.spec.life, 1) << 6;
			body->hitPoints = std::clamp(record.lifeNow, 64, body->maxHitPoints);
			body->minDamage = static_cast<uint8_t>(std::clamp(record.spec.minDamage, 0, 255));
			body->maxDamage = static_cast<uint8_t>(std::clamp(record.spec.maxDamage, 0, 255));
			body->golemToHit = static_cast<uint16_t>(std::clamp(record.spec.toHit, 0, 65535));
			body->armorClass = static_cast<uint8_t>(std::clamp(record.spec.armorClass, 0, 255));
			if (record.spec.ramp != 0)
				body->uniqueMonsterTRN = RampTranslation(record.spec.ramp, 2, true);
			record.body = static_cast<int>(body->getId());
			RecordOfSlot[body->getId()] = static_cast<int8_t>(&record - Records.data());
			return true;
		}
	}
	return false;
}

std::array<int, MaxMinionBodies> FireClocks {};

/** @brief A blow of a minion's on @p monster, credited to @p owner: nothing to the immune, a quarter to the resistant. */
void MinionHurts(const Player &owner, Monster &monster, DamageType type, int damage)
{
	if (damage <= 0 || (monster.hitPoints >> 6) <= 0 || monster.isPlayerMinion() || !monster.isPossibleToHit())
		return;
	if (monster.isImmune(MissileID::Null, type))
		return;
	if (monster.isResistant(MissileID::Null, type))
		damage >>= 2;
	if (damage <= 0)
		return;
	ApplyMonsterDamage(type, monster, damage);
	if ((monster.hitPoints >> 6) <= 0) {
		M_StartKill(monster, owner);
		return;
	}
	M_StartHit(monster, owner, damage);
}

/** @brief Once a second, every Fire Golem of @p owner burns everything standing beside it for half a blow. */
void FireGolemsBurn(const Player &owner)
{
	for (size_t i = 0; i < Records.size(); i++) {
		Record &record = Records[i];
		if (!BodyAlive(record) || record.owner != owner.getId() || record.spec.golem != GolemKind::Fire)
			continue;
		if (++FireClocks[i] < FireGolemPulseTicks)
			continue;
		FireClocks[i] = 0;
		const Monster &golem = Monsters[record.body];
		for (size_t m = 0; m < ActiveMonsterCount; m++) {
			Monster &monster = Monsters[ActiveMonsters[m]];
			if (monster.position.tile.WalkingDistance(golem.position.tile) > 1)
				continue;
			const int blow = record.spec.minDamage + GenerateRnd(std::max(record.spec.maxDamage - record.spec.minDamage, 0) + 1);
			MinionHurts(owner, monster, DamageType::Fire, (blow << 6) / 2);
		}
	}
}

} // namespace

int MinionGroupCap(MinionGroup group)
{
	switch (group) {
	case MinionGroup::Skeleton:
	case MinionGroup::Mage:
		return 8;
	case MinionGroup::Golem:
		return 1;
	case MinionGroup::Revived:
		return 10;
	}
	return 0;
}

const char *MinionGroupName(MinionGroup group)
{
	switch (group) {
	case MinionGroup::Skeleton:
		return N_("Skeletons");
	case MinionGroup::Mage:
		return N_("Mages");
	case MinionGroup::Golem:
		return N_("Golem");
	case MinionGroup::Revived:
		return N_("Revived");
	}
	return "";
}

bool SummonMinion(Player &owner, const MinionSpec &spec, Point near)
{
	if (leveltype == DTYPE_TOWN)
		return false;
	if (MinionCount(owner, spec.group) >= MinionGroupCap(spec.group))
		return false;
	for (Record &record : Records) {
		if (record.active)
			continue;
		record.active = true;
		record.body = -1;
		record.owner = owner.getId();
		record.spec = spec;
		record.lifeNow = std::max(spec.life, 1) << 6;
		if (!SpawnBody(record, owner, near)) {
			record = {};
			return false;
		}
		Renumber(record.owner, spec.group);
		return true;
	}
	return false;
}

void DismissMinions(Player &owner)
{
	for (size_t group = 0; group < MinionGroupCount; group++)
		DismissMinions(owner, static_cast<MinionGroup>(group));
}

void DismissMinions(Player &owner, MinionGroup group)
{
	for (Record &record : Records) {
		if (!record.active || record.owner != owner.getId() || record.spec.group != group)
			continue;
		if (BodyAlive(record)) {
			// The body dies like any monster and its slot comes back through DeleteMonster, which releases the record.
			M_StartKill(Monsters[record.body], owner);
			continue;
		}
		if (record.body < 0)
			Release(record);
	}
}

int MinionCount(const Player &owner, MinionGroup group)
{
	int count = 0;
	for (const Record &record : Records) {
		if (Counts(record) && record.owner == owner.getId() && record.spec.group == group)
			count++;
	}
	return count;
}

int MinionCount(const Player &owner)
{
	int count = 0;
	for (size_t group = 0; group < MinionGroupCount; group++)
		count += MinionCount(owner, static_cast<MinionGroup>(group));
	return count;
}

size_t ActiveMinionBodies()
{
	size_t bodies = 0;
	for (const Record &record : Records) {
		if (record.active && record.body >= 0)
			bodies++;
	}
	return bodies;
}

bool IsMinion(const Monster &monster)
{
	return RecordOf(monster) != nullptr;
}

const Player *MinionOwner(const Monster &monster)
{
	const Record *record = RecordOf(monster);
	if (record == nullptr || record->owner >= Players.size())
		return nullptr;
	return &Players[record->owner];
}

bool GetMinionOrders(const Monster &monster, CompanionOrders &orders)
{
	const Record *record = RecordOf(monster);
	if (record == nullptr || !BodyAlive(*record) || record->owner >= Players.size())
		return false;
	const Player &owner = Players[record->owner];
	if (!owner.plractive)
		return false;
	orders.valid = true;
	orders.owner = owner.position.tile;
	orders.home = FormationHome(owner, *record);
	orders.attack = record->spec.missile == MissileID::Null ? CompanionAttack::Melee : CompanionAttack::Bow;
	orders.missile = record->spec.missile == MissileID::Null ? MissileID::Arrow : record->spec.missile;
	// Looser than a companion's: thirty bodies cannot all stand within three tiles, and a ring of four is part of
	// the formation. The regroup distance is what keeps a crowd from being left behind a door.
	switch (GetCompanionStance()) {
	case CompanionStance::Follow:
		orders.leash = 6;
		orders.settle = 4;
		orders.regroup = 12;
		orders.attacks = true;
		orders.reach = orders.attack == CompanionAttack::Bow ? 8 : 5;
		break;
	case CompanionStance::Hold:
		orders.leash = 1000;
		orders.settle = 1000;
		orders.regroup = 16;
		orders.attacks = true;
		orders.reach = 1;
		break;
	case CompanionStance::Aggressive:
		orders.leash = 9;
		orders.settle = 5;
		orders.regroup = 14;
		orders.attacks = true;
		orders.reach = 9;
		break;
	case CompanionStance::Passive:
		orders.leash = 5;
		orders.settle = 4;
		orders.regroup = 10;
		orders.attacks = false;
		orders.reach = 0;
		break;
	}
	return true;
}

bool MinionMakesWay(const Player &player, const Monster &monster)
{
	const Record *record = RecordOf(monster);
	return record != nullptr && record->owner == player.getId() && !monster.isWalking() && BodyAlive(*record);
}

bool MinionThinksThisTick(const Monster &monster, uint32_t tick)
{
	return (tick + static_cast<uint32_t>(monster.getId())) % 3 == 0;
}

void ForgetMinions()
{
	for (Record &record : Records)
		record = {};
	RecordOfSlot.fill(-1);
	for (Frenzy &frenzy : Frenzies)
		frenzy = {};
}

void OnMinionLevelLoad()
{
	for (Record &record : Records) {
		if (!record.active)
			continue;
		if (record.body >= 0) {
			if (!BodyAlive(record)) {
				record = {};
				continue;
			}
			record.lifeNow = Monsters[record.body].hitPoints;
			record.body = -1;
		}
	}
	RecordOfSlot.fill(-1);
}

void WithdrawMinionsForLevelSave()
{
	bool removed = false;
	for (Record &record : Records) {
		if (!record.active || record.body < 0)
			continue;
		Monster &body = Monsters[record.body];
		const bool alive = BodyAlive(record);
		if (alive)
			record.lifeNow = body.hitPoints;
		// Unhooked BEFORE the slot is freed, or DeleteMonster would take the record with the body.
		RecordOfSlot[static_cast<size_t>(record.body)] = -1;
		record.body = -1;
		M_ClearSquares(body);
		if (InDungeonBounds(body.position.tile) && dMonster[body.position.tile.x][body.position.tile.y] == static_cast<int>(body.getId() + 1))
			dMonster[body.position.tile.x][body.position.tile.y] = 0;
		body.isInvalid = true;
		removed = true;
		if (!alive)
			record = {};
	}
	if (removed)
		DeleteMonsterList();
}

void OnMonsterSlotFreed(size_t monsterId)
{
	if (monsterId >= MaxMonsters)
		return;
	const int8_t index = RecordOfSlot[monsterId];
	if (index < 0)
		return;
	Record &record = Records[static_cast<size_t>(index)];
	const uint8_t owner = record.owner;
	const MinionGroup group = record.spec.group;
	Release(record);
	Renumber(owner, group);
}

void ProcessMinions(Player &owner)
{
	Frenzy &frenzy = Frenzies[std::min<size_t>(owner.getId(), MAX_PLRS - 1)];
	if (frenzy.ticks > 0 && --frenzy.ticks == 0)
		frenzy.percent = 0;
	// The timed ones run down everywhere, town included: a Revived does not keep for being out of sight.
	for (Record &record : Records) {
		if (!Counts(record) || record.owner != owner.getId() || record.spec.ticksLeft <= 0)
			continue;
		if (--record.spec.ticksLeft > 0)
			continue;
		if (BodyAlive(record))
			M_StartKill(Monsters[record.body], owner);
		else
			Release(record);
	}
	if (leveltype == DTYPE_TOWN || owner._pLvlChanging || !owner.isOnActiveLevel())
		return;
	FireGolemsBurn(owner);
	int formed = 0;
	for (Record &record : Records) {
		if (!record.active || record.body >= 0 || record.owner != owner.getId())
			continue;
		if (!SpawnBody(record, owner, owner.position.tile))
			break; // no room this tick; the rest wait with it
		if (++formed >= ReformPerTick)
			break;
	}
}

// =================================================================================================================
// What the skills and the engine ask of the army
// =================================================================================================================

int GatherMinions(Player &owner)
{
	int moved = 0;
	for (Record &record : Records) {
		if (!BodyAlive(record) || record.owner != owner.getId())
			continue;
		Monster &body = Monsters[record.body];
		if (body.position.tile.WalkingDistance(owner.position.tile) <= 2)
			continue;
		if (PlaceCompanionNear(body, owner.position.tile, 4))
			moved++;
	}
	return moved;
}

int HealMinions(Player &owner, int radius, int percent)
{
	int healed = 0;
	for (Record &record : Records) {
		if (!BodyAlive(record) || record.owner != owner.getId())
			continue;
		Monster &body = Monsters[record.body];
		if (body.position.tile.WalkingDistance(owner.position.tile) > radius || body.hitPoints >= body.maxHitPoints)
			continue;
		body.hitPoints = std::min(body.hitPoints + body.maxHitPoints * percent / 100, body.maxHitPoints);
		healed++;
	}
	return healed;
}

void FrenzyMinions(Player &owner, int ticks, int percent)
{
	Frenzy &frenzy = Frenzies[std::min<size_t>(owner.getId(), MAX_PLRS - 1)];
	frenzy.ticks = std::max(frenzy.ticks, ticks);
	frenzy.percent = std::max(frenzy.percent, percent);
}

int SacrificeMinion(Player &owner, Point tile)
{
	Record *nearest = nullptr;
	int bestDistance = 0;
	for (Record &record : Records) {
		if (!BodyAlive(record) || record.owner != owner.getId())
			continue;
		const int distance = Monsters[record.body].position.tile.WalkingDistance(tile);
		if (nearest == nullptr || distance < bestDistance) {
			nearest = &record;
			bestDistance = distance;
		}
	}
	if (nearest == nullptr)
		return 0;
	const int life = Monsters[nearest->body].maxHitPoints;
	M_StartKill(Monsters[nearest->body], owner);
	return life;
}

int MinionDamageTaken(const Monster &monster, DamageType type, int damage)
{
	const Record *record = RecordOf(monster);
	if (record == nullptr || record->owner >= Players.size())
		return damage;
	const Player &owner = Players[record->owner];
	// The Fire Golem drinks fire.
	if (type == DamageType::Fire && record->spec.golem == GolemKind::Fire) {
		Monster &body = Monsters[record->body];
		body.hitPoints = std::min(body.hitPoints + damage / 2, body.maxHitPoints);
		return 0;
	}
	// Summon Resist: fire, lightning and magic, a fifth at the first point and up to three quarters.
	if (IsAnyOf(type, DamageType::Fire, DamageType::Lightning, DamageType::Magic) && IsClassTreeSkillUnlocked(owner, ClassTreeSkill::SummonResist)) {
		const int points = ClassTreeInvestment(owner, ClassTreeSkill::SummonResist);
		if (points > 0)
			damage -= damage * std::min(20 + 5 * (points - 1), 75) / 100;
	}
	return damage;
}

int MinionDamagePercent(const Monster &monster)
{
	const Record *record = RecordOf(monster);
	if (record == nullptr)
		return 100;
	const Frenzy &frenzy = Frenzies[std::min<size_t>(record->owner, MAX_PLRS - 1)];
	return frenzy.ticks > 0 ? 100 + frenzy.percent : 100;
}

void OnMinionBlow(Monster &minion, Monster &target, int damage)
{
	Record *record = RecordOf(minion);
	if (record == nullptr || record->owner >= Players.size() || damage <= 0)
		return;
	Player &owner = Players[record->owner];
	switch (record->spec.golem) {
	case GolemKind::Clay:
		// Two seconds of chill for a blow that lands: the Clay Golem's whole point.
		if ((target.hitPoints >> 6) > 0)
			ChillMonster(target, 40);
		break;
	case GolemKind::Blood: {
		// A quarter of what it takes to itself, a quarter to its owner.
		const int share = damage / 4;
		minion.hitPoints = std::min(minion.hitPoints + share, minion.maxHitPoints);
		if (owner._pHitPoints > 0) {
			owner._pHitPoints = std::min(owner._pHitPoints + share, owner._pMaxHP);
			owner._pHPBase = std::min(owner._pHPBase + share, owner._pMaxHPBase);
			RedrawComponent(PanelDrawComponent::Health);
		}
		break;
	}
	default:
		break;
	}
}

void OnMinionStruck(Monster &minion, Monster &attacker, int damage)
{
	const Record *record = RecordOf(minion);
	if (record == nullptr || record->owner >= Players.size() || damage <= 0 || record->spec.golem != GolemKind::Iron)
		return;
	// The Iron Golem gives a third of every blow back to whoever struck it.
	MinionHurts(Players[record->owner], attacker, DamageType::Physical, damage / 3);
}

// =================================================================================================================
// The panel
// =================================================================================================================

namespace {

// Under the companions' panel's own corner (companion.cpp HudX/HudY). The Necromancer has no companions, so in
// practice the two never share the screen; if they ever do, the army's panel gives way downward.
constexpr int HudX = 8;
constexpr int HudY = 8 + 20 + 16 + 6;
constexpr int HudWidth = 156;
constexpr int HeaderHeight = 16;
constexpr int RowHeight = 20;

Rectangle HeaderRect()
{
	return Rectangle { Point { HudX, HudY }, Size { HudWidth, HeaderHeight } };
}

} // namespace

void DrawMinionHud(const Surface &out)
{
	if (MyPlayer == nullptr || MinionCount(*MyPlayer) == 0)
		return;
	int rows = 0;
	for (size_t group = 0; group < MinionGroupCount; group++) {
		if (MinionCount(*MyPlayer, static_cast<MinionGroup>(group)) > 0)
			rows++;
	}
	DrawHalfTransparentRectTo(out, HudX, HudY, HudWidth, HeaderHeight + rows * RowHeight + 4);
	DrawString(out, fmt::format(fmt::runtime(_("Army: {:s}")), _(CompanionStanceName())),
	    Rectangle { Point { HudX + 4, HudY }, Size { HudWidth - 8, HeaderHeight } },
	    { UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::ColorWhitegold });

	int y = HudY + HeaderHeight;
	const int barWidth = HudWidth - 8;
	for (size_t g = 0; g < MinionGroupCount; g++) {
		const auto group = static_cast<MinionGroup>(g);
		const int count = MinionCount(*MyPlayer, group);
		if (count == 0)
			continue;
		int64_t life = 0;
		int64_t maxLife = 0;
		for (const Record &record : Records) {
			if (!Counts(record) || record.owner != MyPlayer->getId() || record.spec.group != group)
				continue;
			maxLife += static_cast<int64_t>(std::max(record.spec.life, 1)) << 6;
			life += record.body >= 0 ? std::max(Monsters[record.body].hitPoints, 0) : record.lifeNow;
		}
		DrawString(out, _(MinionGroupName(group)), Rectangle { Point { HudX + 4, y }, Size { barWidth, 12 } },
		    { UiFlags::FontSize12 | UiFlags::ColorWhite });
		DrawString(out, fmt::format("{:d}/{:d}", count, MinionGroupCap(group)),
		    Rectangle { Point { HudX + 4, y }, Size { barWidth, 12 } }, { UiFlags::FontSize12 | UiFlags::ColorGold | UiFlags::AlignRight });
		const int lifeWidth = static_cast<int>(std::clamp<int64_t>(barWidth * life / std::max<int64_t>(maxLife, 1), 0, barWidth));
		FillRect(out, HudX + 4, y + 14, barWidth, 3, 0);
		FillRect(out, HudX + 4, y + 14, lifeWidth, 3, PAL16_RED + 4);
		y += RowHeight;
	}
}

bool HandleMinionHudClick(Point mouse)
{
	if (MyPlayer == nullptr || MinionCount(*MyPlayer) == 0 || !HeaderRect().contains(mouse))
		return false;
	CycleCompanionStance();
	AnnounceCompanionStance();
	return true;
}

} // namespace devilution::oracool
