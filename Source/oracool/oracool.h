#pragma once

#include "multi.h"
#include "spelldat.h"

namespace devilution::oracool {

inline constexpr char EditionName[] = "Diablo Oracool Edition";

inline bool IsSinglePlayer()
{
	return !gbIsMultiplayer;
}

/**
 * @brief Oracool: user request (2026-08-11) - Town Portal is a built-in ability, not a spell in
 * the classical sense. It is cast solely from the HUD's Portal button (see hud_menu.h's
 * CastTownPortalAtFeet), is always available, costs nothing, and never levels up - so it is hidden
 * from every place a real spell would appear: the spell book pages, the SpeedBook ring, the
 * F5-F8 hotkey list, and the Book of Town Portal is no longer generated as loot or shop stock.
 *
 * Single-player only, matching the always-memorized zero-mana behaviour it rides on
 * (Player::CalcSpellCosts / GetManaAmount both special-case it the same way). In multiplayer it
 * stays an ordinary spell, since the Portal button is itself single-player only.
 */
inline bool IsBuiltInPortalAbility(SpellID spellId)
{
	return spellId == SpellID::TownPortal && IsSinglePlayer();
}

/**
 * @brief Oracool: bug postmortem (2026-08-09) - town was never designed to hold Objects[]-pool
 * objects (every vanilla town "prop" is baked directly into the town .dun tile data, not placed
 * via AddObject()), so nothing ever called ClrAllObjects() before town's own object placement -
 * that only happens inside InitObjects() (dungeon-only) and SetMapObjects() (quest/debug map
 * loading, also dungeon-only). AvailableObjects[] therefore stayed at its zero-initialized default
 * (every slot 0, instead of the free-list identity permutation ClrAllObjects() sets up) on a
 * player's very first visit to town in a session, before any dungeon level had ever run
 * InitObjects(). Every AddObject() call in town silently got slot 0 back, each one overwriting
 * whatever the previous town object call had placed there - the Stash Chest and Waypoint sigil
 * fighting over the same object slot. Calls the same ClrAllObjects() dungeon levels already rely
 * on, so town's object pool starts from a properly initialized state exactly like every other
 * level does. Defined in objects.cpp (ClrAllObjects() itself is file-local there); call once,
 * before any other town object placement, only on a genuinely fresh town generation (same guard
 * as AddStashChestObject()/AddWaypointSigilObject() below).
 */
void InitTownObjectPool();

/**
 * @brief Oracool: user request - places the fixed town Stash Chest, a physical object that opens
 * the Stash panel when clicked (see OpenStash() in qol/stash.h). Defined in objects.cpp (needs
 * file-local AddObject()); declared here rather than in a dedicated header since it's a single,
 * self-contained placement call with no other public surface.
 */
void AddStashChestObject();

/**
 * @brief Oracool: user request - resets the Stash Chest's visual back to closed when the Stash
 * panel closes, whatever closed it (clicking away, pressing Escape/I, etc.) - without this the
 * chest would stay looking open even after its contents are no longer accessible. No-op if the
 * chest can't be found (not in town, or somehow not placed yet).
 */
void CloseStashChestObject();

/**
 * @brief Oracool: user request - Waypoints, restart, step 1: places a pure visual placeholder
 * sigil in town (no interactivity yet). Defined in objects.cpp (needs file-local AddObject()).
 */
void AddWaypointSigilObject();

/**
 * @brief Oracool: bug postmortem (2026-08-10) - registers the waypoint sigil's graphic
 * (OFILE_ORCLWAYP) in the current level's object-graphics list if it isn't there already. Must be
 * called unconditionally on every entry to a level that could have a waypoint sigil - fresh
 * generation or a revisit - since only fresh generation places the object itself (via
 * AddWaypointSigilObject, which already does this registration as part of that). A revisit
 * restores the object from its own saved state without ever calling AddWaypointSigilObject, so
 * without a separate, unconditional call here, the graphic never gets registered and the
 * restored object silently ends up without a sprite - see ApplyPendingWaypointSpawn's sibling
 * doc comment below for what that caused. Defined in objects.cpp; called from diablo.cpp's
 * LoadGameLevel, before the fresh-vs-revisit fork.
 */
void EnsureWaypointGraphicsLoaded();

/**
 * @brief Oracool: bug postmortem (2026-08-10) - if a waypoint warp is pending (see
 * waypoint_menu.h's ConsumeWaypointSpawnRequest), relocates the player to wherever this level's
 * waypoint sigil actually is - found by searching the current level's active objects, so this
 * works whether the sigil was freshly placed (AddWaypointSigilObject, town or a first-visit
 * dungeon level) or just restored by LoadLevel() on a revisited dungeon level. No-op if no spawn
 * is pending, or if this level has no waypoint sigil.
 *
 * Call exactly once, exactly after the destination level has fully finished loading - currently
 * from interfac.cpp's WM_DIABNEXTLVL handler, right after its LoadGameLevel() call returns (every
 * waypoint warp uses that interface_mode - see StartNewLvl's call in oracool/waypoint_menu.cpp).
 * Two other timings were tried and rejected:
 *   - Mid-load, from inside LoadGameLevel() itself (right after the sigil was placed/restored):
 *     positionally sound, but easy to mistake for the cause of a real crash that turned out to be
 *     unrelated (see EnsureWaypointGraphicsLoaded's doc comment) - kept a mid-load call as the
 *     working baseline while chasing that bug down the wrong path.
 *   - Once per normal game tick, unconditionally, from the main loop: this actively raced the
 *     event queue. WaypointSpawnRequested gets set synchronously the instant the player clicks a
 *     travel-list entry, but the level transition itself doesn't happen until a *later* tick
 *     processes the queued WM_DIABNEXTLVL message - a per-tick check has no way to tell "warp
 *     requested" apart from "warp requested but not yet underway", so it consumed the flag on
 *     whatever level the player was still standing on, wasting it before the real destination had
 *     even started loading.
 */
void ApplyPendingWaypointSpawn();

} // namespace devilution::oracool
