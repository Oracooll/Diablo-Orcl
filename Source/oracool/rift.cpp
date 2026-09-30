#include "oracool/rift.h"

#include "effects.h" // PlaySfxLoc - vanilla's portal sound on the way-home portal
#include <algorithm>
#include <cmath>

#include <SDL.h>
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
#include "options.h" // the mini-map option: where the rift bar hangs
#include "oracool/area_level.h"
#include "oracool/endgame_boss.h"
#include "oracool/event_log.h"
#include "oracool/lesser_uniques.h"
#include "oracool/sprite_colours.h"
#include "oracool/oracool.h" // IsSinglePlayer
#include "oracool/ornate_border.h" // DrawLegacyTextBox - the rift bar's gold frame
#include "oracool/skill_sounds.h"
#include "oracool/stairless.h"
#include "oracool/stonegate.h"
#include "oracool/auto_save.h"
#include "inv.h"
#include "player.h"
#include "plrmsg.h" // EventPlrMsg - a refused rift says why on screen
#include "portal.h"
#include "qol/stash.h"
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
	/** The way home is laid on homeTile (LayWayHome): from then on no item lands on it or beside it. */
	bool homeLaid = false;
	/** The way OUT beside the arrival spot (user, 2026-09-24: "make sure exiting rift is possible through
	 * the entry/exit point you spawn next to when first entering it"). Chosen once per rift, on the first
	 * build, and laid again on every visit like the way home. (0,0) until then. */
	Point arrivalTile = { 0, 0 };
	/** The hero walked out through the way home - only then is a cleared rift over. A death after the
	 * kill (the pile and the keystone still on the floor) keeps it open (audit, 2026-09-20). */
	bool returnedHome = false;
	int ticksLeft = 0;
	bool timedOut = false;
	/** The hero clicked the town-side portal and has not clicked anywhere else since (2026-09-24): the
	 * rift is entered once he is within RiftEntryReach of it. Replaced entryArmed, the walk-on door's guard. */
	bool entryRequested = false;
	/** A cleared Nephalem Rift's closing clock (NephalemRiftCloseSeconds): ticks until the rift ends on
	 * its own, 0 when not running. */
	int closeTicks = 0;
	/**
	 * A Guardian Rift's keystone is spent when the hero first steps through, not when it turns (user, 2026-09-27:
	 * "fix all four" - quitting before going in used to lose it). Until then it stays where it was, and this names
	 * it: EnterRift finds it by its seed wherever it has gone - any backpack page, the stash - and takes it.
	 */
	bool keystonePending = false;
	uint32_t keystoneSeed = 0;
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
	// Every town portal into the rift being replaced closes with it (audit, 2026-09-27). A portal records the set
	// level, not which rift: one cast inside the last rift led, after a new one opened, into a level rebuilt from the
	// NEW seed with the OLD monsters restored by type index - and no exit laid, the kind no longer matching.
	for (int i = 0; i < MAXPORTAL; i++) {
		const Portal &portal = Portals[i];
		if (!portal.open || !portal.setlvl || !IsRiftLevel(static_cast<_setlevels>(portal.level)))
			continue;
		DeactivatePortal(i);
		RemovePortalMissile(i);
	}
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
	// THE HERO'S LEVEL, not the deepest floor (user, 2026-09-24, asked "how is area level of nephalem
	// rifts decided? it should somehow be related to char level and difficulty", chose "hero level
	// only" of four rules). One rung per three character levels inside the difficulty's block of
	// sixteen: clvl 3-5 is rung 1, clvl 30 rung 10, clvl 48 and up the block's top. The block is the
	// difficulty's, so the same hero's rift is 16 rungs deeper on Nightmare than on Normal. Diablo
	// III's rule - the rift matches the hero, whatever floors he has or has not walked.
	const int blockBase = AreaLevel(1, sgGameInitInfo.nDifficulty) - 1;
	return blockBase + std::clamp(player._pLevel / 3, 1, RungsPerDifficulty);
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

/**
 * @brief Whether a rift the hero has stepped into is still running (audit, 2026-09-29): opening another silently ended
 * it - a Guardian Rift whose keystone was already spent, or a cleared Nephalem Rift in its sixty seconds to come back
 * for the pile. Refused, and said, instead.
 */
bool EnteredRiftStillOpen()
{
	if (State.kind == RiftKind::None || !RiftEntered() || State.returnedHome)
		return false;
	// Only the two that would be lost: a Guardian Rift still on its clock (its keystone is spent), and a cleared Nephalem
	// Rift in its sixty seconds. An abandoned Nephalem Rift has no clock and a timed-out Guardian Rift no keystone left to
	// win - refusing those would keep the monument shut for good.
	const bool liveGuardian = State.kind == RiftKind::Guardian && !State.done && !State.timedOut;
	// A cleared Guardian Rift too, until the hero walks out through an exit (returnedHome): its pile and the next keystone
	// wait inside for a hero who died or ran out of room, and a new rift wiped them (round 9 audit, v1.12.234).
	const bool clearedPile = (State.kind == RiftKind::Nephalem && State.done && State.closeTicks > 0)
	    || (State.kind == RiftKind::Guardian && State.done && !State.returnedHome);
	if (!liveGuardian && !clearedPile)
		return false;
	// A cleared Guardian Rift never closes on its own: it ends through its own way home (round 17 audit).
	const std::string why = State.kind == RiftKind::Guardian && State.done
	    ? std::string("The Guardian Rift you cleared is still open - walk out through its portal to close it before opening another.")
	    : StrCat("The ", RiftKindName(State.kind), " you entered is still open - finish it, or let it close, before opening another.");
	LogEvent(why, UiFlags::ColorRed);
	// On screen and aloud too, as the waypoints' refusals since round 18: the event log is closed by default (round 19).
	EventPlrMsg(why, UiFlags::ColorRed);
	if (MyPlayer != nullptr)
		MyPlayer->Say(HeroSpeech::ICantDoThat);
	return true;
}

bool OpenNephalemRift(Player &player)
{
	if (!IsSinglePlayer() || !player.isOnLevel(0) || EnteredRiftStillOpen())
		return false;
	OpenCommon(player, RiftKind::Nephalem, NephalemRiftTierFor(player));
	return true;
}

bool OpenGuardianRift(Player &player, int tier)
{
	if (!IsSinglePlayer() || !player.isOnLevel(0) || EnteredRiftStillOpen())
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
	State.keystonePending = true;
	State.keystoneSeed = keystone._iSeed;
	LightStonegate(RiftKind::Guardian);
	LogEvent(StrCat("The keystone turns: a violet portal opens in the Rift Monument - a Guardian Rift, tier ", tier,
	             ", fifteen minutes. ", RiftGuardianName(State.guardian), " waits at the end. The keystone is spent when you step through."),
	    UiFlags::ColorWhitegold);
	return true;
}

bool SpendPendingKeystone(Player &player)
{
	if (!State.keystonePending)
		return true;
	const auto isIt = [](const Item &item) {
		return !item.isEmpty() && item._iMiscId == IMISC_ORACOOL_KEYSTONE && item._iSeed == State.keystoneSeed;
	};
	bool spent = false;
	for (int i = 0; i < player._pNumInv && !spent; i++) {
		if (isIt(player.InvList[i])) {
			player.RemoveInvItem(i);
			spent = true;
		}
	}
	for (int t = 0; t < Player::NumExtraInventoryTabs && !spent; t++) {
		for (int i = 0; i < player._pNumInvTab[t] && !spent; i++) {
			if (isIt(player.InvTabList[t][i])) {
				RemoveExtraTabItem(player, t, i);
				spent = true;
			}
		}
	}
	// The belt too, for a key placed there before keys were kept off it (round 12 audit, v1.12.237).
	for (int i = 0; i < MaxBeltItems && !spent; i++) {
		if (isIt(player.SpdList[i])) {
			player.RemoveSpdBarItem(i);
			spent = true;
		}
	}
	for (size_t i = 0; i < Stash.stashList.size() && !spent; i++) {
		if (isIt(Stash.stashList[i])) {
			Stash.RemoveStashItem(static_cast<StashStruct::StashCell>(i));
			Stash.dirty = true;
			ScheduleAutoSaveForStashChange();
			spent = true;
		}
	}
	if (!spent)
		return false;
	State.keystonePending = false;
	ScheduleAutoSaveForItemDrop();
	return true;
}

const Item *FindBestKeystoneInBackpack(const Player &player)
{
	// Every backpack page, as SpendPendingKeystone searches: a keystone auto-placed onto page 2 left the row red, "no
	// keystone in the pack" (round 18 audit, v1.12.243).
	const Item *best = nullptr;
	const auto consider = [&best](const Item &item) {
		if (item.isEmpty() || item._iMiscId != IMISC_ORACOOL_KEYSTONE)
			return;
		if (best == nullptr || item._iOracoolRiftTier > best->_iOracoolRiftTier)
			best = &item;
	};
	for (int i = 0; i < player._pNumInv; i++)
		consider(player.InvList[i]);
	for (int t = 0; t < Player::NumExtraInventoryTabs; t++) {
		for (int i = 0; i < player._pNumInvTab[t]; i++)
			consider(player.InvTabList[t][i]);
	}
	return best;
}

bool UseBestKeystoneFromBackpack(Player &player)
{
	const Item *keystone = FindBestKeystoneInBackpack(player);
	if (keystone == nullptr)
		return false;
	// Turned, not spent: it goes when the hero steps through (SpendPendingKeystone).
	return UseGuardianKeystone(player, *keystone);
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
	if (tier <= 0)
		return;
	// Room is made for it, as for every guaranteed reward since round 15: on a full floor the keystone silently did not
	// drop, and the Guardian ladder fell back to a Nephalem key (round 17 audit, v1.12.242).
	MakeRoomForGuaranteedReward();
	if (ActiveItemCount >= MAXITEMS)
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
	// The keystone that turned the gate is spent now, at the first step through - and without it there is no step.
	if (&player == MyPlayer && !SpendPendingKeystone(player)) {
		LogEvent("The keystone that opened this rift is no longer with you. Bring it back to step through.", UiFlags::ColorRed);
		return false;
	}
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
	// A CLICK, and near enough (user, 2026-09-24: "there is a tile which when walked on sends you in the
	// nephalem/guardian rifts. remove it. i want going in to only happen upon click on their portals
	// click area while 1-2 tiles away"). The entry tile used to be a door you could stroll into; now
	// it is only where the click walks the hero, and he goes in the moment he is within reach of the
	// portal - at once if he clicked from beside it.
	if (!State.entryRequested)
		return false;
	Point entry;
	if (!StonegateEntryTile(entry))
		return false;
	const Point portal = entry - Displacement { 1, 1 }; // the portal stands on the gate's tile
	Player &player = *MyPlayer;
	if (player._pmode == PM_DEATH || player._pmode == PM_NEWLVL || player._pmode == PM_QUIT)
		return false;
	if (player.position.tile.WalkingDistance(portal) > RiftEntryReach)
		return false;
	State.entryRequested = false;
	ClrPlrPath(player);
	return EnterRift(player);
}

namespace {
bool PortalHovered = false;
} // namespace

void SetRiftPortalHovered(bool hovered)
{
	PortalHovered = hovered;
}

bool RiftPortalHovered()
{
	return PortalHovered;
}

void NoteWorldClickForRift()
{
	State.entryRequested = PortalHovered && leveltype == DTYPE_TOWN && State.kind != RiftKind::None && !State.returnedHome;
}

Point RiftReturnTile()
{
	Point entry;
	return StonegateLastEntryTile(entry) ? entry : Point { 32, 57 };
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
	// NO STAIRS (user, 2026-09-24: "if a rift is a single level make sure it has no stairs assets on the
	// map. they are meaningless"). The DRLG still places them - its retry loops and random stream are the
	// floor's own - and then hands their tiles back to the floor under them (oracool/stairless.h).
	GeneratingStairlessLevel = true;
	CreateDungeon(State.seed, ENTRY_MAIN); // ViewPosition lands where the up stairs' landing was
	GeneratingStairlessLevel = false;

	InitNoTriggers(); // the exits are laid by RiftLevelPopulated: one by the arrival spot, one where the guardian falls
	// Before the landing is checked: OpenFloorNear reads the tile properties these load.
	LoadRndLvlPal(leveltype);
	LoadLevelSOLData();
	// The landing on open floor with open floor all round, in case a stairs miniset's landing tile
	// is not floor once the stairs are gone.
	ViewPosition = OpenFloorNear(ViewPosition);
	// An arrival safe zone: a floor's stairs get one from Freeupstairs through its triggers, and a
	// rift has none, so the scatter could put a pack on the hero's landing tile.
	for (int dy = -2; dy <= 2; dy++) {
		for (int dx = -2; dx <= 2; dx++) {
			const Point tile = ViewPosition + Displacement { dx, dy };
			if (InDungeonBounds(tile))
				dFlags[tile.x][tile.y] |= DungeonFlag::Populated;
		}
	}
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
	// A RIFT's, and nothing else's (user, 2026-09-24: "assertion failed ... lighting.cpp:210 ...
	// while entering skeleton king chambers"). The call site is the set-level fresh-entry branch,
	// which every set level takes; without this gate the Skeleton King's lair, the Chamber of Bone
	// and the rest ran CreateThemeRooms on the PREVIOUS floor's theme table - set levels never call
	// InitThemes. A stale Shrine or Library theme that does not fit the new map leaves themex/themey
	// at 0 and places its candles at (-1,0): out of dObject, and into DoLighting's assert. Where a
	// stale theme did fit, it put stray shrines and books into a quest map. Since v1.12.062.
	if (!fresh || !InRift())
		return;
	const uint8_t savedLevel = currlevel;
	currlevel = static_cast<uint8_t>(GenerationFloorFor(leveltype));
	// The theme rooms' packs come after PlaceRiftMonsters scaled the floor's, and AddMonster has no rift hook: they kept
	// their floor stats at any tier (round 5 audit, v1.12.230). Scaled here, only the new ones - scaling is not idempotent.
	const size_t monstersBefore = ActiveMonsterCount;
	CreateThemeRooms();
	for (size_t i = monstersBefore; i < ActiveMonsterCount; i++)
		ScaleRiftMonster(Monsters[ActiveMonsters[i]]);
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
	// The rung inside THIS difficulty's block, held at its ends: RungOf wrapped past the block, so a tier-17 key in Normal
	// drew floor-1 zombies at 148% and the Guardian ladder got easier every sixteenth tier (round 9 audit, v1.12.234). A
	// tier above the block stays at rung 16 and scales 3% a tier, as rift.h promises; one below it takes rung 1.
	const int blockStart = AreaLevel(1, sgGameInitInfo.nDifficulty) - 1;
	return std::clamp(State.tier - blockStart, 1, RungsPerDifficulty);
}

bool RiftAcceptsMonster(const MonsterData &data)
{
	if (data.availability == MonsterAvailability::Never)
		return false;
	// Not the Hork Demon's spawn: a summon that drops nothing, and a roster of it filled the bar for no loot (round 32 audit).
	if (&data == &MonstersData[MT_HORKSPWN])
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
	// The Dread boss first, then the champion, then the unique: every champion has a uniqueType (PrepareUniqueMonst), so
	// they all earned a unique's credit and RiftCreditChampion never ran (round 23 audit, v1.12.248).
	if (IsEndgameBoss(monster))
		return RiftCreditUnique;
	if (monster.lesserAffix != LesserUniqueAffix::None)
		return RiftCreditChampion;
	if (monster.isUnique())
		return RiftCreditUnique;
	return RiftCreditOrdinary;
}

namespace {

/** @brief One return trigger on @p tile, and the rift's portal drawn on it. */
void LayRiftExit(Point tile)
{
	if (numtrigs >= MAXTRIGGERS)
		return;
	trigs[numtrigs].position = tile;
	trigs[numtrigs]._tmsg = WM_DIABRTNLVL;
	numtrigs++;
	if (MyPlayer != nullptr)
		AddMissile(tile, tile, Direction::South, PortalFor(State.kind), TARGET_MONSTERS, MyPlayer->getId(), 0, 0);
}

/**
 * @brief The way out where the hero arrived: a walkable neighbour of the arrival tile, never the tile
 * itself (a trigger under the landing spot would send him straight back).
 */
void LayArrivalExit()
{
	if (State.arrivalTile == Point { 0, 0 }) {
		for (int d = 0; d < 8; d++) {
			const Point neighbour = ViewPosition + static_cast<Direction>(d);
			if (InDungeonBounds(neighbour) && !IsTileSolid(neighbour) && dObject[neighbour.x][neighbour.y] == 0
			    && dMonster[neighbour.x][neighbour.y] == 0 && dPlayer[neighbour.x][neighbour.y] == 0) {
				State.arrivalTile = neighbour;
				break;
			}
		}
	}
	if (State.arrivalTile != Point { 0, 0 })
		LayRiftExit(State.arrivalTile);
}

/** @brief The return trigger where the guardian fell, and the rift's portal drawn on it. */
void LayWayHome(bool sound)
{
	State.homeLaid = true;
	LayRiftExit(State.homeTile);
	// Vanilla's portal opening sound (user, 2026-09-20) - when it opens, at the guardian's fall, not each time a
	// re-entered level rebuilds it (round 23 audit).
	if (sound)
		PlaySfxLoc(LS_SENTINEL, State.homeTile);
}

/**
 * @brief The guardian arrives (user, 2026-09-24: "i want a more scary sound when rift boss appears and
 * more impacting sound when he is defeated"). Was the Milestone chime - a reward sound for a threat.
 * Now his own voice where the game has one - Leoric's "The warmth of life has entered my tomb", the
 * Butcher's "Ah, fresh meat!" - and for Diablo and Na-Krul, who have no greeting, the sting the game
 * plays on the way into Diablo's own level. Streams: one at a time, so one line each. Under it the
 * Milestone chime stays, so the bar's end still sounds like the bar's end.
 */
void PlayGuardianRisesSound(const Monster &guardian)
{
	(void)guardian;
	switch (State.guardian) {
	case RiftGuardianType::SkeletonKing:
		PlaySFX(USFX_SKING1);
		break;
	case RiftGuardianType::Butcher:
		PlaySFX(USFX_CLEAVER);
		break;
	case RiftGuardianType::Diablo:
	case RiftGuardianType::NaKrul:
		PlaySFX(PS_DIABLVLINT);
		break;
	}
	PlayUiEventSound(UiEventSound::Milestone);
}

/**
 * @brief The guardian falls: Apocalypse's blast under the cleared chime, and for Diablo his own death
 * cry - the one the game ends on. The others' own death sounds already play with the kill.
 */
void PlayGuardianFallsSound()
{
	// A rift Na-Krul's floor-24 speech, started on his notice, stops with him: his SpawnLoot branch that stops it is the
	// quest's, skipped for a guardian (round 21 audit).
	if (State.guardian == RiftGuardianType::NaKrul)
		stream_stop();
	// Not Diablo's own scream on top: his death plays it (PlayEffect, MonsterSound::Death), and he screamed twice
	// (round 23 audit).
	PlaySFX(LS_APOC);
	PlayUiEventSound(UiEventSound::EncounterCleared);
}

} // namespace

void RiftLevelPopulated()
{
	if (!InRift())
		return;
	// The exits are not in the level save (triggers and missiles never are), so every visit lays them:
	// the way out by the arrival spot always, and the way home where the guardian fell once he has.
	numtrigs = 0;
	LayArrivalExit();
	if (State.done)
		LayWayHome(/*sound=*/false); // rebuilt, not opened
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

int RiftGuardianItemCount()
{
	return State.kind == RiftKind::Guardian ? 6 : 4;
}

void ClearRiftEntryRequest()
{
	State.entryRequested = false;
}

/** @brief Whether a Town Portal stands on @p tile - read from the missiles, not the per-pass missile flag (round 29 audit). */
bool TownPortalOn(Point tile)
{
	for (const Missile &missile : Missiles) {
		if (missile._mitype == MissileID::TownPortal && missile.position.tile == tile)
			return true;
	}
	return false;
}

bool IsBesideRiftWayHome(Point tile)
{
	if (!InRift())
		return false;
	// The arrival exit too: a pile beside it walked the hero out the same way, and a cleared Guardian Rift ended with its
	// pile (round 26 audit).
	if (State.arrivalTile != Point { 0, 0 } && std::max(std::abs(tile.x - State.arrivalTile.x), std::abs(tile.y - State.arrivalTile.y)) <= 1)
		return true;
	// Only once it is laid: the keystone drops on the corpse first, while homeTile still names the corpse.
	if (!State.homeLaid)
		return false;
	return std::max(std::abs(tile.x - State.homeTile.x), std::abs(tile.y - State.homeTile.y)) <= 1;
}

bool IsRiftGuardian(const Monster &monster)
{
	// Inside the rift only (external audit of v1.12.188, WORLD-02): the id is a Monsters[] slot, and a rift left
	// open while the hero walks a normal floor would otherwise crown whatever spawned into that slot there.
	// And only while he stands (audit, 2026-09-29): once he has fallen, his freed slot is the next to be reused, and a
	// Doppelganger clone or a minion taking it "was" the guardian - a clone killed there dropped the guardian's pile again,
	// over and over. His own loot was spawned before the rift was marked done.
	return InRift() && State.guardianSpawned && !State.done && State.guardianId == monster.getId();
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
		// The keystone FIRST, so the search below sees it: laid after the way home, it could land in ring 2 beside a way
		// home at 3 - the pickup-walk trap round 17 set out to close (round 18 audit, v1.12.243).
		if (State.kind == RiftKind::Nephalem)
			DropKeystone(monster.position.tile, State.tier);
		else
			DropKeystone(monster.position.tile, NextKeystoneTier(State.tier, State.ticksLeft, GuardianRiftSeconds * RiftTicksPerSecond, State.timedOut));
		// Nobody standing on it (audit, 2026-09-27): searched from South, it could land under the hero who struck the
		// blow, and his standing still on it fired the trigger - home, the rift over, the pile and the keystone left.
		// OUTSIDE the drop ring, on a tile with no item (round 9 audit, v1.12.234): the loot scatters over the 3x3 around the
		// corpse, and a pickup walk that stopped on a trigger among it sent the hero home - in a Guardian Rift, with the rest
		// of the pile and the next keystone lost.
		if (const std::optional<Point> home = FindClosestValidPosition(
		        [&monster](Point tile) {
			        // Three out, with no item beside it: a pickup walk stops one tile SHORT of its item, so a way home at two -
			        // next to the ring - was where a walk to a ring item ended, and it sent the hero home (round 17 audit).
			        // Nor on a standing Town Portal: the portal ran first and took the hero to town, and the way home was never
			        // walked (round 28 audit).
			        if (!InDungeonBounds(tile) || IsTileSolid(tile) || dObject[tile.x][tile.y] != 0 || dItem[tile.x][tile.y] != 0
			            || dPlayer[tile.x][tile.y] != 0 || dMonster[tile.x][tile.y] != 0 || tile == State.arrivalTile
			            || TownPortalOn(tile)
			            || std::max(std::abs(tile.x - monster.position.tile.x), std::abs(tile.y - monster.position.tile.y)) < 3)
				        return false;
			        for (int d = 0; d < 8; d++) {
				        const Point beside = tile + static_cast<Direction>(d);
				        if (InDungeonBounds(beside) && dItem[beside.x][beside.y] != 0)
					        return false;
			        }
			        // A short walk from the corpse, not merely near it: ring 3 across a wall won over ring 5 down the corridor,
			        // and a cleared Guardian Rift ends only through this tile (round 18 audit).
			        int8_t path[MaxPathLength];
			        const int steps = FindPath([](Point p) { return InDungeonBounds(p) && !IsTileSolid(p); }, monster.position.tile, tile, path);
			        return steps > 0 && steps <= 14;
		        },
		        monster.position.tile, 3, 10);
		    home.has_value()) {
			State.homeTile = *home;
		} else {
			// Two passes: a neighbour off the pile first (round 30 audit), then any valid one - the loot scatters over the 3x3,
			// and with every neighbour carrying an item the first pass alone left the portal on the corpse tile, under the
			// keystone (round 31 audit).
			for (const bool avoidItems : { true, false }) {
				bool found = false;
				for (int d = 0; d < 8; d++) {
					const Point neighbour = monster.position.tile + static_cast<Direction>(d);
					if (InDungeonBounds(neighbour) && !IsTileSolid(neighbour) && dObject[neighbour.x][neighbour.y] == 0
					    && dPlayer[neighbour.x][neighbour.y] == 0 && dMonster[neighbour.x][neighbour.y] == 0 && neighbour != State.arrivalTile
					    && !TownPortalOn(neighbour) && (!avoidItems || dItem[neighbour.x][neighbour.y] == 0)) {
						State.homeTile = neighbour;
						found = true;
						break;
					}
				}
				if (found)
					break;
			}
		}
		// The keystone (plan r5) went down above: a Nephalem guardian always drops one at the rift's tier; a Guardian
		// guardian drops the next tier's, unless the clock ran out.
		LayWayHome(/*sound=*/true);
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
		PlayGuardianFallsSound();
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
	// From the first step inside, not from the keystone's turn: the keystone is kept until that step (2026-09-27), and
	// fifteen minutes of shopping first spent it on a rift already out of time (round 26 audit, v1.12.251).
	if (State.kind == RiftKind::Guardian && RiftEntered() && !State.done && !State.timedOut && State.ticksLeft > 0) {
		if (--State.ticksLeft == 0) {
			State.timedOut = true;
			LogEvent("The Guardian Rift's time is up. The guardian will still fall, but no keystone comes of it.", UiFlags::ColorRed);
		}
	}

	// The cleared Nephalem Rift's closing clock, wherever the hero is. At zero: a hero still inside is
	// sent to town's new-game spawn (WM_DIABRETOWN lands on ENTRY_MAIN, the (57, 67) a new hero starts
	// on), the portal comes down and the rift is over.
	// Held while the hero is between levels (audit, 2026-09-29): a town portal taken on the closing tick was already
	// under way, and landed him in a rift that had just ended - no exits, a different map under the saved monsters.
	// The close waits for him to arrive, then runs as below.
	const bool arriving = MyPlayer != nullptr && (MyPlayer->_pmode == PM_NEWLVL || MyPlayer->_pLvlChanging);
	if (State.closeTicks == 1 && arriving)
		return;
	if (State.closeTicks > 0 && --State.closeTicks == 0) {
		const bool inside = InRift();
		LogEvent(inside ? "The rift closes around you and spits you out in Tristram." : "The rift has closed; the Rift Monument falls dark.", UiFlags::ColorWhitegold);
		CloseStonegate(); // removes the portal missiles, ends the rift, the closing sound
		// Not a dead hero: the close carried the corpse to town and opened the death menu there again. The Respawn that
		// follows takes him home (round 12 audit, v1.12.237).
		if (inside && MyPlayer != nullptr && MyPlayer->_pmode != PM_DEATH && !MyPlayerIsDead)
			// WM_DIABRTNLVL, the way home's own message, not WM_DIABRETOWN (user, 2026-09-24: "when nephalem
			// rif closed and i got teleported away i spawned in [dlvl 9] ... unable to move out"). RETOWN loads
			// the hero's plrlevel, and inside a set level that holds the SET LEVEL's number - 9 is
			// SL_RIFT_NEPHALEM - so the close dropped him on dungeon level 9 as if he had walked there. The
			// return path loads GetMapReturnLevel (town) and puts him in front of the monument.
			StartNewLvl(*MyPlayer, WM_DIABRTNLVL, GetMapReturnLevel());
		return;
	}

	if (leveltype == DTYPE_TOWN) {
		// The portal in the gate after town is rebuilt (a death in the rift): AddStonegateObject runs
		// before InitMissiles clears the list, so the relight happens here, once the missiles exist.
		if (!State.returnedHome)
			RelightStonegateIfNeeded();
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
	PlayGuardianRisesSound(*guardian);
}

bool RiftEntered() { return State.creditNeeded > 0; }
// Only a CLEARED rift ends on the way out (2026-09-24): the way out by the arrival spot is open from the
// start, and leaving through it before the guardian falls is a trip to town, like a town portal - the
// rift and its bar wait for the hero to come back through the monument.
void RiftNoteReturnHome()
{
	// A cleared NEPHALEM Rift is not ended by walking out either (user, 2026-09-24 dev note: "walking
	// through the yellow portal after killing neph rift boss shouldnt close the rift. only the countdown
	// timer closes the portal"). Its sixty-second clock is the only thing that ends it (ProcessRift), so
	// the hero can step out to sell and come back for the rest of the pile. A Guardian Rift has no such
	// clock and still ends on the way out once cleared.
	// Only when the exit taken is the rift's own: every set level's way home (an arena, the Skeleton King's lair)
	// sent this, and ended a cleared Guardian Rift the hero had left for town - its pile and keystone waiting inside
	// were lost to the next rift (round 12 audit, v1.12.237).
	if (!InRift())
		return;
	if (State.done && State.kind != RiftKind::Nephalem)
		State.returnedHome = true;
}
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

const uint32_t *GuardianPortalRgbTable(const SpriteColours *sheetColours)
{
	static constexpr uint32_t Violet[8] = { 0xE6B4FF, 0xC080F5, 0x9B50D8, 0x7A34B4, 0x5C2290, 0x40146A, 0x280A46, 0x140424 };
	// Since v1.12.211 a PNG missile keeps its own colours and its pixels index THEM, not the palette - read through the
	// palette table below, the portal came out in scrambled colours. Each of the sheet's blues goes to the same violet
	// ramp by its brightness (its strongest channel: a blue's luminance is too low to carry it), brightest to 0xE6B4FF.
	if (sheetColours != nullptr) {
		static std::array<uint32_t, 256> shifted;
		const uint32_t *own = sheetColours->Table(0);
		for (size_t i = 0; i < shifted.size(); i++) {
			const uint32_t c = own[i];
			const int v = static_cast<int>(std::max({ (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF }));
			const int pos = (255 - v) * 7 * 256 / 255;
			const int lower = std::min(pos >> 8, 6);
			const int t = pos - (lower << 8);
			const uint32_t a = Violet[lower], b = Violet[lower + 1];
			const auto ch = [&](int shift) {
				const int from = static_cast<int>((a >> shift) & 0xFF), to = static_cast<int>((b >> shift) & 0xFF);
				return static_cast<uint32_t>(std::clamp(from + (to - from) * t / 256, 0, 255)) << shift;
			};
			shifted[i] = (c & 0xFF000000U) | ch(16) | ch(8) | ch(0);
		}
		return shifted.data();
	}
	// The violet the palette never had (2026-09-20, user: "Guardian rift portal is not purple and
	// its core background is not purple either"): missiles\portal_purple.png is quantised to the
	// palette on load like every PNG missile, and the shared half has blue, gold, red and grey ramps
	// but no purple, so the hue-shifted sheet came out blue. The sheet is therefore built in
	// vanilla's own BLUE (the PAL8_BLUE ramp, indices 128-135, with a dark blue centre fill) and the
	// Guardian portal is drawn through this table, which sends those eight blues to a violet ramp
	// and everything else to the palette as it is - the green plate's mechanism (SetSpellTransGreen).
	static std::array<uint32_t, 256> table;
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

namespace {

/** @brief @p a mixed toward @p b by @p t (0..256), per channel. */
uint32_t MixRgb(uint32_t a, uint32_t b, int t)
{
	const auto ch = [&](int shift) {
		const int from = static_cast<int>((a >> shift) & 0xFF);
		const int to = static_cast<int>((b >> shift) & 0xFF);
		return static_cast<uint32_t>(std::clamp(from + (to - from) * t / 256, 0, 255)) << shift;
	};
	return ch(16) | ch(8) | ch(0);
}

/**
 * @brief The rift bar's fill, colour-cycling: a slow shimmer travels along it between the portal's deep and pale shades,
 * a bright glint sweeps across it every 2.2 seconds, it is lit from above (the top rows lighter, the bottom darker), and
 * its leading edge glows so the progress reads at a glance. Gold for a Nephalem Rift, violet for a Guardian Rift.
 */
void DrawRiftBarFill(const Surface &out, Point at, int width, int height, bool guardian)
{
	const uint32_t deep = guardian ? 0x5A1E94u : 0xA8700Eu;
	const uint32_t base = guardian ? 0x8A3FC8u : 0xD9A21Au;
	const uint32_t pale = guardian ? 0xD49CF5u : 0xFFE08Au;
	const uint8_t fallback = guardian ? static_cast<uint8_t>(PAL8_BLUE) : static_cast<uint8_t>(PAL16_YELLOW + 2);
	const uint32_t now = SDL_GetTicks();
	const float phase = static_cast<float>(now % 1800) / 1800.0F; // the shimmer's travel, one cycle in 1.8 s
	const int glintSpan = width + 24;
	const int glint = static_cast<int>(now % 2200) * glintSpan / 2200 - 12; // the glint's centre column
	for (int c = 0; c < width; c++) {
		// Shimmer: a wave 28 columns long travelling right, mixing deep -> base -> pale.
		const float wave = 0.5F + 0.5F * std::sin(6.2831853F * (static_cast<float>(c) / 28.0F - phase));
		uint32_t colour = wave < 0.5F ? MixRgb(deep, base, static_cast<int>(wave * 512.0F))
		                              : MixRgb(base, pale, static_cast<int>((wave - 0.5F) * 384.0F));
		// The glint: a soft band of near-white six columns wide.
		const int d = std::abs(c - glint);
		if (d < 6)
			colour = MixRgb(colour, 0xFFFFF0u, (6 - d) * 30);
		// The leading edge glows.
		if (c >= width - 2)
			colour = MixRgb(colour, pale, 160);
		for (int r = 0; r < height; r++) {
			// Lit from above: +40% toward pale on the top row, down to -35% toward black on the bottom.
			const int shade = r < height / 2 ? (height / 2 - r) * 80 / std::max(height / 2, 1) : 0;
			const int dark = r >= height / 2 ? (r - height / 2 + 1) * 90 / std::max(height - height / 2, 1) : 0;
			uint32_t px = shade > 0 ? MixRgb(colour, pale, shade) : colour;
			if (dark > 0)
				px = MixRgb(px, 0x000000u, dark);
			FillRectRgb(out, at.x + c, at.y + r, 1, 1, px, fallback);
		}
	}
}

} // namespace

void DrawRiftHud(const Surface &out)
{
	// Wherever the rift is still open, not only inside it (user, 2026-09-26 dev notes: "i want to keep seeing the
	// nephalem rift and guardian rift fill bars on when in town and same applied to countdown timers", "countdown
	// timers need to tick while i am in town and i need to be able to see it"). The clocks always ran in town
	// (ProcessRift is called every tick, before the town branch); only this panel was missing there. A rift that is
	// over - none open, or a cleared Guardian Rift the hero has walked home from - shows nothing.
	if (State.kind == RiftKind::None || State.returnedHome)
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
	// At the map's own top when the mini-map is off, not hanging under an empty space (round 27 audit).
	const int y = *sgOptions.Oracool.miniMapEnabled ? miniMap.position.y + miniMap.size.height + Gap : miniMap.position.y;

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

	// The bar (user, 2026-09-26: "make the rift bars nicer looking - put a gold frame around them and apply color
	// cycling on them"): the legacy gold pinstripe box - the gold-amount box's own ring, dark-bright-dark around a black
	// field - framing a fill that shimmers in the portal's colours.
	const int barY = y + 17;
	DrawLegacyTextBox(out, Rectangle { { x - LegacyTextBoxBevel, barY - LegacyTextBoxBevel },
	                           { BarWidth + 2 * LegacyTextBoxBevel, BarHeight + 2 * LegacyTextBoxBevel } });
	const int fill = BarWidth * RiftProgressPercent() / 100;
	if (fill > 0)
		DrawRiftBarFill(out, Point { x, barY }, fill, BarHeight, State.kind == RiftKind::Guardian);
}

void SetRiftGuardianForTest(int monsterId)
{
	State.guardianSpawned = true;
	State.guardianId = monsterId;
}

void ResetRiftForNewGame()
{
	State = RiftState {};
}

} // namespace devilution::oracool
