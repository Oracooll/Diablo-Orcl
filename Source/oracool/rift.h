/**
 * @file oracool/rift.h
 *
 * Oracool: the rifts behind the Stonegate (plan page "Nephalem and Guardian Rifts", 2026-09-20;
 * the user's answers r1-r10 are on it).
 *
 * A NEPHALEM Rift is free: a click on the gate lights the golden portal, a click on the portal (from within two tiles) lands the
 * hero on a freshly generated floor of a random tileset, filled with a mix of monsters from every
 * dungeon at the rift's tier. Kills fill a bar; at 100% the Skeleton King or the Butcher rises near
 * the hero. Killing him drops a pile and opens the way back. A GUARDIAN Rift is the same run with
 * a fifteen-minute clock, no town portal, and Diablo or Na-Krul at the end.
 *
 * The level is a SET LEVEL that is generated rather than loaded from a .dun (r1): SL_RIFT_NEPHALEM
 * and SL_RIFT_GUARDIAN in levels/gendung.h, built by BuildRiftLevel from the rift's own seed, so
 * every rift is new (the Sealed Maps' fresh-generation trick clears _pSLvlVisited when a rift
 * opens) and a rift re-entered after a death (r10: the hero wakes in town, the rift stays open)
 * comes back exactly as it was left, through the engine's ordinary level save.
 *
 * Nothing here is saved: the rift's state is this file's static, alive while the game runs and
 * reset for a new game like every other per-game static. Leaving the game ends the rift.
 */
#pragma once

#include <cstdint>

#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "levels/gendung.h"
#include "monstdat.h"

namespace devilution {
struct Player;
struct Monster;
struct Item;
} // namespace devilution

namespace devilution::oracool {

enum class RiftKind : uint8_t {
	None,
	Nephalem,
	Guardian,
};

/** @brief Who waits at 100%: the Nephalem pair, then the Guardian pair. */
enum class RiftGuardianType : uint8_t {
	SkeletonKing,
	Butcher,
	Diablo,
	NaKrul,
};

/** @brief The Guardian Rift's clock (r9): fifteen minutes, Diablo III's number, a first value for play. */
constexpr int GuardianRiftSeconds = 15 * 60;
constexpr int RiftTicksPerSecond = 20;
/**
 * @brief A cleared NEPHALEM Rift stays open this long after its guardian falls, then closes on its own
 * (user, 2026-09-20: "Nephalem rift stays open for 60 seconds after its boss has been killed. It cant be
 * closed any other way. Only New Game is the option ... If the player is in the Nef Rift while countdown
 * hits 0 player is teleported to new-game spawn location and the rift portal is now gone").
 */
constexpr int NephalemRiftCloseSeconds = 60;

/** @brief Kill credit toward the bar (r4): an ordinary monster, a champion or variant, a unique or boss. */
constexpr int RiftCreditOrdinary = 1;
constexpr int RiftCreditChampion = 3;
constexpr int RiftCreditUnique = 5;
/** @brief The bar is this share of the floor's total credit, so a full floor is a little more than the bar. */
constexpr int RiftBarPercentOfFloor = 70;
constexpr int RiftBarMinimum = 12;

/**
 * @brief Monster scaling above the floor's own level: +3% life and damage per tier past what the
 * current difficulty would pay on the rift's rung. A Nephalem Rift, whose tier IS that level,
 * scales nothing; a Guardian Rift climbing past it scales without a ceiling (r5).
 */
constexpr int RiftScalePercentPerTier = 3;

/** @brief Whether @p level is one of the two rift set levels. */
bool IsRiftLevel(_setlevels level);
_setlevels RiftLevelFor(RiftKind kind);

/** @brief The rift that exists right now (opened at the gate and not yet ended), or None. */
RiftKind ActiveRift();
/** @brief Whether the local hero is standing inside the active rift. */
bool InRift();
/**
 * @brief Whether the town portal is refused where the hero stands. ALWAYS FALSE since 2026-09-20 (user:
 * "Town portals are to be allowed in Guardian Rift"); plan r9's refusal is history. Kept so the spell
 * check keeps one question to ask.
 */
bool RiftForbidsTownPortal();

int RiftTier();
/** @brief The Nephalem tier: one rung per three levels of @p player, inside this difficulty's block (2026-09-24). */
int NephalemRiftTierFor(const Player &player);
RiftGuardianType RiftGuardian();
const char *RiftGuardianName(RiftGuardianType guardian);
const char *RiftKindName(RiftKind kind);

/** @brief Opens a rift at the gate: a fresh seed, tileset and guardian; the previous one, if any, ends. */
bool OpenNephalemRift(Player &player);
bool OpenGuardianRift(Player &player, int tier);
/** @brief Ends the rift: the state is cleared; the gate's own closing is the Stonegate's business. */
void EndRift();

/**
 * @brief A Guardian Keystone used in town (plan r5): opens a Guardian Rift at the keystone's tier and
 * lights the gate. The keystone is NOT consumed here - the rift remembers it and SpendPendingKeystone takes it at the
 * first step through (2026-09-27). False anywhere else.
 */
bool UseGuardianKeystone(Player &player, const Item &keystone);
/**
 * @brief Spends the keystone that turned the open Guardian Rift - found by its seed on any backpack page or in the
 * stash - at the hero's first step through. True when it was spent now or nothing was pending; false when it is gone
 * (sold, dropped), and then the hero cannot step through until it is back.
 */
bool SpendPendingKeystone(Player &player);
/** @brief The highest-tier Guardian Keystone in @p player's backpack, or -1. */
int FindBestKeystoneInBackpack(const Player &player);
/** @brief The gate's menu choosing a Guardian Rift: the best keystone in the pack is turned (spent at the first step through). False with none, or off town. */
bool UseBestKeystoneFromBackpack(Player &player);
/**
 * @brief The tier of the keystone a Guardian Rift's guardian drops (plan r5): one up, three up when
 * more than half the clock was left; 0 when the clock had run out (no keystone). Ticks are the
 * clock's, so a test can reason in seconds.
 */
int NextKeystoneTier(int tier, int ticksLeft, int ticksTotal, bool timedOut);
/**
 * @brief Drops a Guardian Keystone of @p tier on @p tile - the guardian's drop, exposed for the
 * `keystone` debug command (user, 2026-09-20: "i need a debug command for Guardian Rift keys").
 */
void DropGuardianKeystone(Point tile, int tier);
/** @brief From town: sets the tileset and starts the set level. False outside town or with no rift open. */
bool EnterRift(Player &player);
/**
 * @brief Called from the town-side portal each tick: enters when the hero has CLICKED the portal and is
 * within RiftEntryReach tiles of it (user, 2026-09-24). Walking past it no longer does anything.
 */
bool TryEnterRiftFromTown();
/** @brief How near the portal's tile the hero must be for a click on it to take him in. */
constexpr int RiftEntryReach = 2;
/** @brief The cursor found a town-side rift portal this frame (cursor.cpp sets it, and clears it). */
void SetRiftPortalHovered(bool hovered);
bool RiftPortalHovered();
/**
 * @brief A left click in the world: a click ON the portal asks to go in, any other click takes the
 * request back, so walking away after clicking it does not land the hero in the rift anyway.
 */
void NoteWorldClickForRift();
/**
 * @brief Where a hero coming back from any rift stands in town: in front of the Rift Monument.
 * Asked by GetMapReturnPosition by the rift's LEVEL NUMBER, because by then the rift may be over
 * (it closed) and the town's objects are not built yet, so neither InRift() nor the gate object can answer.
 */
Point RiftReturnTile();

/**
 * @brief LoadSetMap's rift branch: generates the floor from the rift's seed with the DRLG pretending to
 * be a floor of the rift's tileset, installs no stairs, loads the palette and tile properties, the
 * themes and - on a fresh build - the objects. @p fresh is "not visited yet" (a revisit restores
 * the rest from the level save).
 */
void BuildRiftLevel(bool fresh);
/** @brief After InitItems on a fresh build: the theme rooms. */
void FinishRiftLevel(bool fresh);
/** @brief The tileset the active rift's floor is generated in. */
dungeon_type RiftTileset();
/** @brief The seed the roster pick is made from, so a revisit loads the types the saved monsters wear. */
uint32_t RiftRosterSeed();
/** @brief The dungeon floor the monster roster is drawn from (r2): the tier's rung, with the Hive/Crypt twin. */
int RiftMonsterBandFloor();
/** @brief Whether @p data may appear on the rift floor: available on the band floor or its side-step twin. */
bool RiftAcceptsMonster(const MonsterData &data);

/** @brief After the floor is populated: sizes the bar from the credit standing on it. */
void RiftLevelPopulated();
/** @brief Tier scaling on one monster of the rift floor (or the guardian): life, damage, armour. */
void ScaleRiftMonster(Monster &monster);
/** @brief The percentage scale RiftTier would apply above @p floorTier (100 = none). Exposed for tests. */
int RiftScalePercent(int tier, int floorTier);
/** @brief The credit a kill of @p monster adds to the bar (0 for the hero's own). */
int RiftKillCredit(const Monster &monster);

/** @brief From MonsterDeath: credits the bar; the guardian's death opens the way back. */
void OnRiftMonsterKilled(const Monster &monster);
/** @brief Whether @p monster is the active rift's guardian. */
bool IsRiftGuardian(const Monster &monster);
/**
 * @brief How many random items a rift's guardian drops (user, 2026-09-26 dev note: "rift guardians/bosses to drop random
 * items, not the uniques they drop when killed in quest"): four in a Nephalem Rift, six in a Guardian Rift.
 */
int RiftGuardianItemCount();
/** @brief Per game tick, everywhere: the Guardian clock, the guardian's arrival, the entry tile's arming. */
void ProcessRift();

/** @brief Whether the hero has been inside (the bar was sized): an entered rift is not swapped at the gate. */
bool RiftEntered();
/** @brief From the return trigger: the hero is walking out through the way home, so the rift may end in town. */
void RiftNoteReturnHome();
bool RiftReturnedHome();
bool RiftGuardianSpawned();
bool RiftDone();
bool RiftTimedOut();
int RiftSecondsLeft();
/** @brief Seconds until a cleared Nephalem Rift closes on its own (NephalemRiftCloseSeconds), 0 when no clock runs. */
int RiftCloseSecondsLeft();
/**
 * @brief The Guardian portal's draw table for the 32-bit screen: the palette as it is, except the
 * PAL8_BLUE ramp sent to violet values - the portal's sheet is vanilla's blue, since the palette has no
 * violet to quantise to. scrollrt.cpp draws MissileID::RiftPortalPurple through it.
 */
const uint32_t *GuardianPortalRgbTable();
int RiftProgressPercent();

/** @brief The bar and the clock under the mini-map while the hero is in a rift. */
void DrawRiftHud(const Surface &out);

/** @brief Game teardown: the state is file-local and outlives the game otherwise. */
void ResetRiftForNewGame();
/** @brief Names Monsters[@p monsterId] the open rift's guardian, as its spawn does - for the WORLD-02 test. */
void SetRiftGuardianForTest(int monsterId);

} // namespace devilution::oracool
