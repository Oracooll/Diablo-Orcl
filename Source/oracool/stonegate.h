/**
 * @file stonegate.h
 *
 * Oracool: the Stonegate - a stone monument in town with an elliptical opening the rift portals
 * open in (user, 2026-09-19; art RfA-19 / batch 42, applied 2026-09-20).
 *
 * Two rifts use it: a NEPHALEM Rift through a golden portal and a GUARDIAN Rift through a violet
 * one. This first build is the gate itself: the monument stands in town, a click opens the golden
 * portal in its opening and lights the stone gold, a second click swaps it for the violet one, a
 * third closes it. The rift LEVELS behind the portals are a design of their own (the Roadmap card)
 * and are not here yet: walking into a portal does nothing but stand in it.
 *
 * Nothing is saved: the gate is closed whenever town is (re)built, like the Roar's window.
 */
#pragma once

#include <cstdint>

namespace devilution {
struct Object;
} // namespace devilution

namespace devilution::oracool {

enum class RiftKind : uint8_t {
	None,
	Nephalem,
	Guardian,
};

/** @brief Places the Stonegate in town. Town only; after InitTownObjectPool, like the Roar. */
void AddStonegateObject();

/** @brief Whether @p object is the Stonegate (it wears OBJ_STAND like the Roar; this tells them apart). */
bool IsStonegateObject(const Object &object);

/** @brief Swaps the gate onto objects\orclgate.cel (defined in objects.cpp beside the Roar's, which owns the sheet table). */
void ApplyStonegateGraphics(Object &gate);

/** @brief Loads the rock stand's own sheet before AddObject(OBJ_STAND) asks for it (objects.cpp). */
void PrepareStonegateCarrier();

/** @brief A click on the gate: closed -> Nephalem (gold) -> Guardian (violet) -> closed. */
void ToggleStonegate();

/** @brief Closes an open portal, if any, and returns the stone to its cold frame. */
void CloseStonegate();

/** @brief Per game tick: drives the lit loop while a portal is open. Called from ProcessObjects. */
void ProcessStonegate();

/** @brief Which rift's portal stands in the gate right now. */
RiftKind OpenRift();

} // namespace devilution::oracool
