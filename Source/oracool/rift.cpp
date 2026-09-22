#include "oracool/rift.h"

#include "effects.h" // PlaySfxLoc - vanilla's portal sound on the way-home portal
#include <algorithm>
#include <array>
#include <string>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "diablo.h"
#include "engine/palette.h"
#include "engine/path.h" // IsTileSolid: the way home beside the guardian's corpse
#include "engine/random.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "interfac.h"
#include "items.h"
#include "levels/gendung.h"
#include "levels/themes.h"
#include "levels/trigs.h"
#include "misdat.h"
#include "missiles.h"
#include "monster.h"
#include "multi.h"
#include "objects.h"
#include "oracool/area_level.h"
#include "oracool/endgame_boss.h"
#include "oracool/event_log.h"
#include "oracool/lesser_uniques.h"
#include "oracool/oracool.h" // IsSinglePlayer
#include "oracool/skill_sounds.h"
#include "oracool/stonegate.h"
#include "player.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

struct RiftState {
	RiftKind kind = RiftKind::None;
	int tier = 1;
	uint32_t seed = 0;
	dungeon_type tileset = DTYPE_CATHEDRAL;
	RiftGuardianType guardian = RiftGuardianType::SkeletonKing;
	int creditNeeded = 0;
	int credit = 0;
	bool guardianSpawned = false;
	int guardianId = -1;
	bool done = false;
	/** Where the guardian fell: the way home is laid there, and laid AGAIN on a revisit (triggers and
	 * missiles are not in the level save). */
	Point homeTile = { 0, 0 };
	/** The hero walked out through the way home - only then is a cleared rift over. A death after the
	 * kill (the pile and the keystone still on the floor) keeps it open (audit, 2026-09-20). */
	bool returnedHome = false;
	int ticksLeft = 0;
	bool timedOut = false;
	/** The entry tile in town has to be LEFT before it can be walked into: the hero stands beside
	 * the gate to click it, and the tile in front is where they may already be standing. */
	bool entryArmed = false;
	/** A cleared Nephalem Rift's closing clock (NephalemRiftCloseSeconds): ticks until the rift ends on
	 * its own, 0 when not running. */
	int closeTicks = 0;
	/** Ticks until the next "the guardian found no ground" note, so a stuck spawn says so once in a while. */
	int spawnNoteTicks = 0;
};

RiftState State;

/** @brief The floor the DRLG is told it is building, per tileset: one with no quest set piece of its own
 * (the Catacombs and Caves have a quest on every floor; 7 and 11 carry the smallest stamps). */
int GenerationFloorFor(dungeon_type tileset)
{
	switch (tileset) {
	case DTYPE_CATHEDRAL:
		return 2;
	case DTYPE_CATACOMBS:
		return 7;
	case DTYPE_CAVES:
		return 11;
	case DTYPE_HELL:
		return 13;
	case DTYPE_NEST:
		return 18;
	case DTYPE_CRYPT:
		return 22;
	default:
		return 2;
	}
}

dungeon_type RandomTileset()
{
	constexpr dungeon_type Sets[] = { DTYPE_CATHEDRAL, DTYPE_CATACOMBS, DTYPE_CAVES, DTYPE_HELL, DTYPE_NEST, DTYPE_CRYPT };
	return Sets[GenerateRnd(6)];
}

/** @brief The rift's rung inside a difficulty block: 1..16. */
int RungOf(int tier)
{
	return ((std::max(tier, 1) - 1) % RungsPerDifficulty) + 1;
}

MissileID PortalFor(RiftKind kind)
{
	return kind == RiftKind::Guardian ? MissileID::RiftPortalPurple : MissileID::RiftPortalGold;
}

void OpenCommon(Player &player, RiftKind kind, int tier)
{
	State = RiftState {};
	State.kind = kind;
	State.tier = std::max(tier, 1);
	State.seed = GenerateSeed();
	if (kind == RiftKind::Guardian) {
		State.guardian = FlipCoin() ? RiftGuardianType::Diablo : RiftGuardianType::NaKrul;
		// The guardian's home (r2): Diablo needs a Hell floor, Na-Krul a Crypt floor.
		State.tileset = State.guardian == RiftGuardianType::Diablo ? DTYPE_HELL : DTYPE_CRYPT;
		State.ticksLeft = GuardianRiftSeconds * RiftTicksPerSecond;
	} else {
		State.guardian = FlipCoin() ? RiftGuardianType::SkeletonKing : RiftGuardianType::Butcher;
		State.tileset = RandomTileset();
	}
	// A rift is always a FRESH floor: the visited flag is what makes LoadGameLevel restore a saved
	// level instead of generating (the Sealed Maps' trick, named_encounters.cpp).
	player._pSLvlVisited[RiftLevelFor(kind)] = false;
	State.entryArmed = false;
}

} // namespace

bool IsRiftLevel(_setlevels level)
{
	return level == SL_RIFT_NEPHALEM || level == SL_RIFT_GUARDIAN;
}

_setlevels RiftLevelFor(RiftKind kind)
{
	return kind == RiftKind::Guardian ? SL_RIFT_GUARDIAN : SL_RIFT_NEPHALEM;
}

RiftKind ActiveRift()
{
	return State.kind;
}

bool InRift()
{
	return State.kind != RiftKind::None && setlevel && setlvlnum == RiftLevelFor(State.kind);
}

bool RiftForbidsTownPortal()
{
	// Nowhere, since 2026-09-20 (user: "Town portals are to be allowed in Guardian Rift"). Plan r9
	// refused it inside a Guardian Rift for one version.
	return false;
}

int RiftTier()
{
	return State.tier;
}

int NephalemRiftTierFor(const Player &player)
{
	int deepest = 1;
	for (int floor = 1; floor <= AreaFloorCount && floor < NUMLEVELS; floor++) {
		if (player._pLvlVisited[floor])
			deepest = floor;
	}
	return AreaLevel(deepest, sgGameInitInfo.nDifficulty);
}

RiftGuardianType RiftGuardian()
{
	return State.guardian;
}

const char *RiftGuardianName(RiftGuardianType guardian)
{
	switch (guardian) {
	case RiftGuardianType::SkeletonKing:
		return "the Skeleton King";
	case RiftGuardianType::Butcher:
		return "the Butcher";
	case RiftGuardianType::Diablo:
		return "Diablo";
	case RiftGuardianType::NaKrul:
		return "Na-Krul";
	}
	return "the guardian";
}

const char *RiftKindName(RiftKind kind)
{
	return kind == RiftKind::Guardian ? "Guardian Rift" : "Nephalem Rift";
}

bool OpenNephalemRift(Player &player)
{
	if (!IsSinglePlayer() || !player.isOnLevel(0))
		return false;
	OpenCommon(player, RiftKind::Nephalem, NephalemRiftTierFor(player));
	return true;
}

bool OpenGuardianRift(Player &player, int tier)
{
	if (!IsSinglePlayer() || !player.isOnLevel(0))
		return false;
	OpenCommon(player, RiftKind::Guardian, tier);
	return true;
}

void EndRift()
{
	State = RiftState {};
}

bool UseGuardianKeystone(Player &player, const Item &keystone)
{
	if (&player != MyPlayer || !player.isOnLevel(0) || setlevel)
		return false;
	const int tier = std::max<int>(1, keystone._iOracoolRiftTier);
	if (!OpenGuardianRift(player, tier))
		return false;
	LightStonegate(RiftKind::Guardian);
	LogEvent(StrCat("The keystone turns: a violet portal opens in the Rift Monument - a Guardian Rift, tier ", tier,
	             ", fifteen minutes. ", RiftGuardianName(State.guardian), " waits at the end."),
	    UiFlags::ColorWhitegold);
	return true;
}

int FindBestKeystoneInBackpack(const Player &player)
{
	int best = -1;
	for (int i = 0; i < player._pNumInv; i++) {
		const Item &item = player.InvList[i];
		if (item.isEmpty() || item._iMiscId != IMISC_ORACOOL_KEYSTONE)
			continue;
		if (best < 0 || item._iOracoolRiftTier > player.InvList[best]._iOracoolRiftTier)
			best = i;
	}
	return best;
}

bool UseBestKeystoneFromBackpack(Player &player)
{
	const int index = FindBestKeystoneInBackpack(player);
	if (index < 0)
		return false;
	if (!UseGuardianKeystone(player, player.InvList[index]))
		return false;
	player.RemoveInvItem(index);
	return true;
}

int NextKeystoneTier(int tier, int ticksLeft, int ticksTotal, bool timedOut)
{
	if (timedOut)
		return 0;
	const bool fast = ticksTotal > 0 && ticksLeft * 2 > ticksTotal;
	return std::max(tier, 1) + (fast ? 3 : 1);
}

namespace {

/** @brief Drops a Guardian Keystone of @p tier at @p tile: the item through the quest-item door, the tier stamped after. */
void DropKeystone(Point tile, int tier)
{
	if (tier <= 0 || ActiveItemCount >= MAXITEMS)
		return;
	const int before = ActiveItemCount;
	SpawnQuestItem(IDI_ORACOOL_KEYSTONE, tile, /*randarea=*/0, /*selflag=*/0, /*sendmsg=*/true);
	if (ActiveItemCount <= before)
		return;
	Item &keystone = Items[ActiveItems[ActiveItemCount - 1]];
	keystone._iOracoolRiftTier = static_cast<uint8_t>(std::min(tier, 255));
}

} // namespace

void DropGuardianKeystone(Point tile, int tier)
{
	DropKeystone(tile, tier);
}

bool EnterRift(Player &player)
{
	if (State.kind == RiftKind::None || !player.isOnLevel(0))
		return false;
	// BEFORE StartNewLvl - the level loads its tileset from this (named_encounters.cpp has the tell).
	setlvltype = State.tileset;
	StartNewLvl(player, WM_DIABSETLVL, RiftLevelFor(State.kind));
	return true;
}

bool TryEnterRiftFromTown()
{
	// A cleared rift is still enterable until the hero has walked out through the way home: the pile
	// and the keystone may be lying where the guardian fell (audit, 2026-09-20).
	if (State.kind == RiftKind::None || State.returnedHome || MyPlayer == nullptr || leveltype != DTYPE_TOWN)
		return false;
	Point entry;
	if (!StonegateEntryTile(entry))
		return false;
	Player &player = *MyPlayer;
	if (player._pmode != PM_STAND || player.position.tile != entry)
		return false;
	if (!State.entryArmed)
		return false;
	ClrPlrPath(player);
	return EnterRift(player);
}

void BuildRiftLevel(bool fresh)
{
	// The DRLG reads currlevel for its floor-keyed rules (quest rooms, the town-warp stairs, floor 16
	// and 24), so it is told a plain floor of the rift's tileset for the duration and given the
	// rift's own seed; the quests themselves are off (Quest::IsAvailable is false on any set level).
	// Kept through the WHOLE build, not only CreateDungeon: InitObjectGFX registers the sprites by
	// currlevel's range and InitObjects reads it for shrines, books and the like, so with the set-level
	// id (9, 10) in it only the Caves' objects loaded and every other tileset's sarcophagus or torch
	// was an unloaded sprite - a crash the moment one scrolled into view (audit, 2026-09-20).
	const uint8_t savedLevel = currlevel;
	currlevel = static_cast<uint8_t>(GenerationFloorFor(leveltype));
	CreateDungeon(State.seed, ENTRY_MAIN); // ViewPosition lands on the up stairs, which lead nowhere here

	InitNoTriggers(); // no stairs: the way back appears when the guardian dies
	// An arrival safe zone: a floor's stairs get one from Freeupstairs through its triggers, and a
	// rift has none, so the scatter could put a pack on the hero's landing tile.
	for (int dy = -2; dy <= 2; dy++) {
		for (int dx = -2; dx <= 2; dx++) {
			const Point tile = ViewPosition + Displacement { dx, dy };
			if (InDungeonBounds(tile))
				dFlags[tile.x][tile.y] |= DungeonFlag::Populated;
		}
	}
	LoadRndLvlPal(leveltype);
	LoadLevelSOLData();
	// The same seed for the themes on every build of this floor, so a revisit computes the rooms the
	// saved objects were placed in.
	SetRndSeed(State.seed ^ 0x5EEDu);
	InitThemes();
	if (!HeadlessMode)
		InitObjectGFX();
	HoldThemeRooms();
	if (fresh)
		InitObjects();
	currlevel = savedLevel;
}

void FinishRiftLevel(bool fresh)
{
	if (!fresh)
		return;
	const uint8_t savedLevel = currlevel;
	currlevel = static_cast<uint8_t>(GenerationFloorFor(leveltype));
	CreateThemeRooms();
	currlevel = savedLevel;
}

uint32_t RiftRosterSeed()
{
	return State.seed ^ 0x0AC7u;
}

dungeon_type RiftTileset()
{
	return State.tileset;
}

int RiftMonsterBandFloor()
{
	return RungOf(State.tier);
}

bool RiftAcceptsMonster(const MonsterData &data)
{
	if (data.availability == MonsterAvailability::Never)
		return false;
	if (gbIsSpawn && data.availability == MonsterAvailability::Retail)
		return false;
	const int floor = RiftMonsterBandFloor();
	if (floor >= data.minDunLvl && floor <= data.maxDunLvl)
		return true;
	// The Hive's and the Crypt's monsters sit on the Caves' and Hell's rungs (area_level.cpp): a rift
	// on rung 9..16 may draw from floors 17..24 as well.
	const int twin = floor + 2 * FloorsPerArea;
	return floor > 2 * FloorsPerArea && twin >= data.minDunLvl && twin <= data.maxDunLvl;
}

int RiftKillCredit(const Monster &monster)
{
	if (monster.isPlayerMinion() || monster.type().type == MT_GOLEM)
		return 0;
	if (monster.isUnique() || IsEndgameBoss(monster))
		return RiftCreditUnique;
	if (monster.lesserAffix != LesserUniqueAffix::None)
		return RiftCreditChampion;
	return RiftCreditOrdinary;
}

namespace {

/** @brief The return trigger where the guardian fell, and the rift's portal drawn on it. */
void LayWayHome()
{
	numtrigs = 1;
	trigs[0].position = State.homeTile;
	trigs[0]._tmsg = WM_DIABRTNLVL;
	if (MyPlayer != nullptr)
		AddMissile(State.homeTile, State.homeTile, Direction::South, PortalFor(State.kind), TARGET_MONSTERS, MyPlayer->getId(), 0, 0);
	PlaySfxLoc(LS_SENTINEL, State.homeTile); // vanilla's portal opening sound (user, 2026-09-20)
}

} // namespace

void RiftLevelPopulated()
{
	if (!InRift())
		return;
	// A cleared rift entered again (the hero died over the pile): the way home is not in the level
	// save, so it is laid again where the guardian fell.
	if (State.done)
		LayWayHome();
	if (State.creditNeeded > 0)
		return; // a revisit keeps the bar it had
	int total = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++)
		total += RiftKillCredit(Monsters[ActiveMonsters[i]]);
	State.creditNeeded = std::max(RiftBarMinimum, total * RiftBarPercentOfFloor / 100);
}

int RiftScalePercent(int tier, int floorTier)
{
	const int extra = std::max(0, tier - floorTier);
	return 100 + RiftScalePercentPerTier * extra;
}

void ScaleRiftMonster(Monster &monster)
{
	if (State.kind == RiftKind::None || monster.isPlayerMinion() || monster.type().type == MT_GOLEM)
		return;
	const int pct = RiftScalePercent(State.tier, AreaLevel(RiftMonsterBandFloor(), sgGameInitInfo.nDifficulty));
	if (pct <= 100)
		return;
	const auto scale = [pct](int value) { return static_cast<int>(static_cast<int64_t>(value) * pct / 100); };
	monster.maxHitPoints = scale(monster.maxHitPoints);
	monster.hitPoints = scale(monster.hitPoints);
	monster.minDamage = static_cast<uint8_t>(std::min(255, scale(monster.minDamage)));
	monster.maxDamage = static_cast<uint8_t>(std::min(255, scale(monster.maxDamage)));
	monster.minDamageSpecial = static_cast<uint8_t>(std::min(255, scale(monster.minDamageSpecial)));
	monster.maxDamageSpecial = static_cast<uint8_t>(std::min(255, scale(monster.maxDamageSpecial)));
	monster.armorClass = static_cast<uint8_t>(std::min(255, monster.armorClass + (pct - 100) / 6));
}

bool IsRiftGuardian(const Monster &monster)
{
	return State.kind != RiftKind::None && State.guardianSpawned && State.guardianId == monster.getId();
}

void OnRiftMonsterKilled(const Monster &monster)
{
	if (!InRift())
		return;
	if (IsRiftGuardian(monster)) {
		if (State.done)
			return;
		State.done = true;
		// The way back (r7): a return trigger BESIDE where he fell - his pile and the keystone land on
		// his tile, and a trigger under them would warp a player home mid-pickup (audit, 2026-09-20).
		// The rift's own portal is drawn on it so the tile is not a secret. GetMapReturnLevel answers
		// town for a rift level.
		State.homeTile = monster.position.tile;
		for (int d = 0; d < 8; d++) {
			const Point neighbour = monster.position.tile + static_cast<Direction>(d);
			if (InDungeonBounds(neighbour) && !IsTileSolid(neighbour) && dObject[neighbour.x][neighbour.y] == 0) {
				State.homeTile = neighbour;
				break;
			}
		}
		LayWayHome();
		// The keystone (plan r5): a Nephalem guardian always drops one at the rift's tier; a Guardian
		// guardian drops the next tier's, unless the clock ran out.
		if (State.kind == RiftKind::Nephalem)
			DropKeystone(monster.position.tile, State.tier);
		else
			DropKeystone(monster.position.tile, NextKeystoneTier(State.tier, State.ticksLeft, GuardianRiftSeconds * RiftTicksPerSecond, State.timedOut));
		if (State.kind == RiftKind::Guardian && State.timedOut)
			LogEvent(StrCat(RiftGuardianName(State.guardian), " falls, but the clock had run out: no keystone. The portal leads home."), UiFlags::ColorWhitegold);
		else
			LogEvent(StrCat(RiftGuardianName(State.guardian), " falls. The rift is cleared - the portal leads home."), UiFlags::ColorWhitegold);
		// A cleared NEPHALEM Rift closes on its own a minute later (NephalemRiftCloseSeconds): the
		// countdown runs on the HUD, and at zero a hero still inside is put down on the new-game spawn
		// in town and the portal is gone. Nothing else ends it - the gate's menu no longer can.
		if (State.kind == RiftKind::Nephalem) {
			State.closeTicks = NephalemRiftCloseSeconds * RiftTicksPerSecond;
			LogEvent(StrCat("The rift closes in ", NephalemRiftCloseSeconds, " seconds. Take what is yours."), UiFlags::ColorWhitegold);
		}
		PlayUiEventSound(UiEventSound::EncounterCleared);
		return;
	}
	if (State.guardianSpawned)
		return; // the bar is full; the rest is the fight
	State.credit += RiftKillCredit(monster);
}

void ProcessRift()
{
	if (State.kind == RiftKind::None)
		return;

	// The Guardian clock runs wherever the hero is (r10: a rift left by dying stays open until the
	// clock runs out), and out of time means no keystone - not no fight.
	if (State.kind == RiftKind::Guardian && !State.done && !State.timedOut && State.ticksLeft > 0) {
		if (--State.ticksLeft == 0) {
			State.timedOut = true;
			LogEvent("The Guardian Rift's time is up. The guardian will still fall, but no keystone comes of it.", UiFlags::ColorRed);
		}
	}

	// The cleared Nephalem Rift's closing clock, wherever the hero is. At zero: a hero still inside is
	// sent to town's new-game spawn (WM_DIABRETOWN lands on ENTRY_MAIN, the (57, 67) a new hero starts
	// on), the portal comes down and the rift is over.
	if (State.closeTicks > 0 && --State.closeTicks == 0) {
		const bool inside = InRift();
		LogEvent(inside ? "The rift closes around you and spits you out in Tristram." : "The rift has closed; the Rift Monument falls dark.", UiFlags::ColorWhitegold);
		CloseStonegate(); // removes the portal missiles, ends the rift, the closing sound
		if (inside && MyPlayer != nullptr)
			StartNewLvl(*MyPlayer, WM_DIABRETOWN, 0);
		return;
	}

	if (leveltype == DTYPE_TOWN) {
		// The portal in the gate after town is rebuilt (a death in the rift): AddStonegateObject runs
		// before InitMissiles clears the list, so the relight happens here, once the missiles exist.
		if (!State.returnedHome)
			RelightStonegateIfNeeded();
		// Arm the entry tile once the hero is off it.
		Point entry;
		if (MyPlayer != nullptr && StonegateEntryTile(entry) && MyPlayer->position.tile != entry)
			State.entryArmed = true;
		return;
	}

	if (!InRift() || State.guardianSpawned || State.creditNeeded <= 0 || State.credit < State.creditNeeded)
		return;
	Monster *guardian = SpawnRiftGuardian();
	if (guardian == nullptr) {
		// No room this tick; tried again next tick - and SAID so every five seconds, because a bar
		// at 100% with no guardian in sight looked like nothing happening (user, 2026-09-20: "Diablo
		// didn't spawn when i hit 100%").
		if (State.spawnNoteTicks <= 0) {
			LogEvent("The guardian finds no ground to rise on near you - move to open floor.", UiFlags::ColorRed);
			State.spawnNoteTicks = 5 * RiftTicksPerSecond;
		}
		State.spawnNoteTicks--;
		return;
	}
	State.guardianSpawned = true;
	State.guardianId = guardian->getId();
	LogEvent(StrCat("The rift shudders: ", RiftGuardianName(State.guardian), " rises!"), UiFlags::ColorRed);
	PlayUiEventSound(UiEventSound::Milestone);
}

bool RiftEntered() { return State.creditNeeded > 0; }
void RiftNoteReturnHome() { State.returnedHome = true; }
bool RiftReturnedHome() { return State.returnedHome; }
bool RiftGuardianSpawned() { return State.guardianSpawned; }
bool RiftDone() { return State.done; }
bool RiftTimedOut() { return State.timedOut; }

int RiftSecondsLeft()
{
	return (State.ticksLeft + RiftTicksPerSecond - 1) / RiftTicksPerSecond;
}

int RiftCloseSecondsLeft()
{
	return (State.closeTicks + RiftTicksPerSecond - 1) / RiftTicksPerSecond;
}

const uint32_t *GuardianPortalRgbTable()
{
	// The violet the palette never had (2026-09-20, user: "Guardian rift portal is not purple and
	// its core background is not purple either"): missiles\portal_purple.png is quantised to the
	// palette on load like every PNG missile, and the shared half has blue, gold, red and grey ramps
	// but no purple, so the hue-shifted sheet came out blue. The sheet is therefore built in
	// vanilla's own BLUE (the PAL8_BLUE ramp, indices 128-135, with a dark blue centre fill) and the
	// Guardian portal is drawn through this table, which sends those eight blues to a violet ramp
	// and everything else to the palette as it is - the green plate's mechanism (SetSpellTransGreen).
	static std::array<uint32_t, 256> table;
	static constexpr uint32_t Violet[8] = { 0xE6B4FF, 0xC080F5, 0x9B50D8, 0x7A34B4, 0x5C2290, 0x40146A, 0x280A46, 0x140424 };
	for (int i = 0; i < 256; i++)
		table[static_cast<size_t>(i)] = PaletteRGB[static_cast<size_t>(i)];
	for (int i = 0; i < 8; i++)
		table[static_cast<size_t>(PAL8_BLUE + i)] = Violet[i];
	return table.data();
}

int RiftProgressPercent()
{
	if (State.guardianSpawned || State.creditNeeded <= 0)
		return State.guardianSpawned ? 100 : 0;
	return std::clamp(State.credit * 100 / State.creditNeeded, 0, 100);
}

void DrawRiftHud(const Surface &out)
{
	if (!InRift())
		return;
	// UNDER the mini-map, the map's own width (user, 2026-09-20: "Kill bar and timer panel - put
	// these under the minimap, not next to it"). It sat to the left of the map for one version, out
	// of the event log's column; the log opens in that column and would draw under this, so the
	// panel steps aside while the log is open - the log carries the rift's own lines anyway. The
	// chat history draws later than this and simply covers it.
	if (IsEventLogOpen())
		return;
	const Rectangle miniMap = GetMiniMapScreenRect();
	constexpr int BarHeight = 8;
	constexpr int Gap = 4;
	const int BarWidth = miniMap.size.width;
	const int x = miniMap.position.x;
	const int y = miniMap.position.y + miniMap.size.height + Gap;

	std::string label = RiftKindName(State.kind);
	if (State.done && State.closeTicks > 0)
		label += fmt::format(": cleared - closes in {:d}s", RiftCloseSecondsLeft()); // the countdown (user, 2026-09-20)
	else if (State.done)
		label += ": cleared";
	else if (State.guardianSpawned)
		label += StrCat(": ", RiftGuardianName(State.guardian));
	else
		label += fmt::format("  {:d}%", RiftProgressPercent());
	if (State.kind == RiftKind::Guardian && !State.done)
		label += State.timedOut ? "  out of time" : fmt::format("  {:d}:{:02d}", RiftSecondsLeft() / 60, RiftSecondsLeft() % 60);
	// THE LINE IS AS WIDE AS THE LINE NEEDS (user, 2026-09-22: "text with countdown timer doesnt fit
	// the second digit of the time remaining").
	//
	// It was drawn in a box the width of the bar, which is the mini-map's width, and centred in it.
	// "Nephalem Rift: cleared - closes in 30s" is wider than that, so AlignCenter put its start left
	// of the box and the clip took the tail - the second digit of the countdown, then the s.
	//
	// The box now grows from the bar's centre to whatever the text measures, and is pushed back on
	// screen if that runs it off an edge. Measured rather than widened by a guessed margin: the
	// label's length changes with the rift's name, its percentage, its guardian and its clock, and a
	// margin that fits the longest of those today is a margin that clips the next one.
	const int labelWidth = std::max(BarWidth, GetLineWidth(label, GameFont12) + 2);
	int labelX = x + (BarWidth - labelWidth) / 2;
	labelX = std::clamp(labelX, 0, std::max(0, out.w() - labelWidth));
	DrawString(out, label, Rectangle { { labelX, y }, { labelWidth, 12 } },
	    { UiFlags::AlignCenter | UiFlags::FontSize12 | (State.timedOut ? UiFlags::ColorRed : UiFlags::ColorGold) | UiFlags::Shadowed });

	// The bar: a dark trough, the fill in the portal's colour.
	const int barY = y + 14;
	FillRectRgb(out, x, barY, BarWidth, BarHeight, 0x101010u, 0);
	const int fill = BarWidth * RiftProgressPercent() / 100;
	if (fill > 0)
		FillRectRgb(out, x, barY, fill, BarHeight, State.kind == RiftKind::Guardian ? 0x8A3FC8u : 0xD9A21Au, 0);
}

void ResetRiftForNewGame()
{
	State = RiftState {};
}

} // namespace devilution::oracool
