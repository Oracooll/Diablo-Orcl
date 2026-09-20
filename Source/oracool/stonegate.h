/**
 * @file stonegate.h
 *
 * Oracool: the Stonegate - a stone monument in town with an elliptical opening the rift portals
 * open in (user, 2026-09-19; art RfA-19 / batch 42, the second cut RfA-21 / batch 44, 2026-09-20).
 *
 * Two rifts use it (oracool/rift.h): a NEPHALEM Rift through the golden portal and a GUARDIAN Rift
 * through the violet one. A click on the closed gate opens a Nephalem Rift, free; a click while an
 * untouched Nephalem Rift stands swaps it for a Guardian Rift; a click on anything else closes the
 * gate and ends the rift. Walking onto the tile in front of the gate enters the rift; the way back
 * appears in the rift when its guardian dies. The gate relights itself when town is rebuilt while a
 * rift is still open (the hero died in it and woke here).
 *
 * Nothing of the gate is saved: it reads the rift's state, which is not saved either.
 */
#pragma once

#include "engine/point.hpp"
#include "oracool/rift.h" // RiftKind

namespace devilution {
struct Object;
} // namespace devilution

namespace devilution::oracool {

/** @brief Places the Stonegate in town. Town only; after InitTownObjectPool, like the Roar. */
void AddStonegateObject();

/** @brief Whether @p object is the Stonegate (it wears OBJ_STAND like the Roar; this tells them apart). Also true for the inactive arch below. */
bool IsStonegateObject(const Object &object);

/**
 * @brief The INACTIVE copy of the painting the town portal opens in (2026-09-20). Drawn in the FLOOR
 * pass (scrollrt.cpp's IsFloorPassObject) so the portal, standing one tile north-west of it, always
 * draws over it whatever the tile order - the user's rule: "we dont move the portal. we move the
 * monument. we keep the portal overlapping the monument."
 */
bool IsStonegatePortalArch(const Object &object);

/** @brief Swaps the gate onto objects\orclgate.cel (defined in objects.cpp beside the Roar's, which owns the sheet table). */
void ApplyStonegateGraphics(Object &gate);

/** @brief Loads the rock stand's own sheet before AddObject(OBJ_STAND) asks for it (objects.cpp). */
void PrepareStonegateCarrier();

/** @brief A click on the gate: opens the choice menu (oracool/stonegate_menu.h). */
void ToggleStonegate();
/** @brief The menu's first line: a free Nephalem Rift at the deepest floor's tier; the golden portal lights. */
bool OpenNephalemAtGate();

/** @brief Lights the gate for @p kind (a keystone used in town lights the violet one). */
void LightStonegate(RiftKind kind);
/** @brief Town, each tick: a rift is open but no portal missile stands in the gate - add it (silently). */
void RelightStonegateIfNeeded();

/** @brief Closes the gate: the portal goes, the rift ends, the stone goes cold. */
void CloseStonegate();

/** @brief Per game tick: drives the lit loop while a portal is open. Called from ProcessObjects. */
void ProcessStonegate();

/** @brief Which rift's portal stands in the gate right now (the rift module's answer). */
RiftKind OpenRift();

/** @brief The tile in front of the gate that entering means walking onto. False when the gate is not placed. */
bool StonegateEntryTile(Point &out);

} // namespace devilution::oracool
