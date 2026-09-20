#include "oracool/stonegate.h"

#include <algorithm>

#include "engine/point.hpp"
#include "levels/gendung.h"
#include "lighting.h"
#include "misdat.h"
#include "missiles.h"
#include "objdat.h"
#include "objects.h"
#include "oracool/event_log.h"
#include "oracool/rift.h"
#include "oracool/skill_sounds.h"
#include "oracool/stonegate_menu.h"
#include "player.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

/**
 * @brief The seventeen frames of objects\orclgate.cel (tools/build_stonegate_cel.cmd, batch 45):
 * 1 the closed gate, 2-9 lit gold, 10-17 lit violet. Object frames are 1-based.
 *
 * ONLY FRAME 1 IS SHOWN since 2026-09-20 (user: "use uncut version of the stonegate asset and just
 * overlap it with portal asset when user selects one of the portals"). The sixteen lit frames -
 * the artist's gold and violet glow baked into the stones, breathing over eight steps - stay in the
 * file but are never selected: the gate is the plain painting in every state, and an open rift is
 * shown by the portal missile alone, standing in the arch. The frames are left in the CEL rather
 * than cut out so the file needs no rebuild and the constants below keep describing it.
 */
constexpr uint32_t ClosedFrame = 1;
constexpr uint32_t FrameCount = 17;

int GateObjectId = -1;

Object *Gate()
{
	if (GateObjectId < 0 || GateObjectId >= MAXOBJECTS)
		return nullptr;
	Object &object = Objects[GateObjectId];
	if (object._otype != OBJ_STAND)
		return nullptr;
	return &object;
}

MissileID PortalFor(RiftKind kind)
{
	return kind == RiftKind::Guardian ? MissileID::RiftPortalPurple : MissileID::RiftPortalGold;
}

void RemovePortalMissiles()
{
	for (Missile &missile : Missiles) {
		if (missile._mitype != MissileID::RiftPortalGold && missile._mitype != MissileID::RiftPortalPurple)
			continue;
		if (missile._mlid != NO_LIGHT)
			AddUnLight(missile._mlid);
		missile._miDelFlag = true;
	}
}

void ShowFrame(Object &gate, uint32_t frame)
{
	gate._oAnimFlag = 0;
	gate._oAnimCnt = 0;
	gate._oAnimLen = FrameCount;
	gate._oAnimFrame = std::clamp<uint32_t>(frame, 1, FrameCount);
}

/** @brief Lights the gate for @p kind: the portal in the opening over the plain painting, the sound. */
void LightGate(Object &gate, RiftKind kind, bool sound)
{
	RemovePortalMissiles();
	if (MyPlayer != nullptr)
		AddMissile(gate.position, gate.position, Direction::South, PortalFor(kind), TARGET_MONSTERS, MyPlayer->getId(), 0, 0);
	// The painting itself does not change - see the frame note at the top of the file.
	ShowFrame(gate, ClosedFrame);
	if (sound)
		PlayUiEventSound(UiEventSound::RiftOpen);
}

} // namespace

void AddStonegateObject()
{
	GateObjectId = -1;
	if (currlevel != 0 || setlevel)
		return;

	// OBJ_STAND like the Roar: an ordinary type wearing its own art, so nothing in the object
	// tables moves. The rock stand's own sheet is loaded first because SetupObject asks for it.
	PrepareStonegateCarrier();

	// Beside the town portal's own landing tile (user, 2026-09-20: "next to the default town portal
	// spawning location in tristram, just a couple or three tiles southeast of it, to avoid
	// overlapping"). The portal lands on WarpDrop[0] = (57, 40) in portal.cpp; south-east is +x on
	// this map, so three tiles along is (60, 40), then the tiles around it. (59, 40), (61, 40) and
	// (63, 40) are the other players' portal slots and are avoided even though V1 is single-player.
	constexpr Point Candidates[] = { { 60, 40 }, { 60, 41 }, { 61, 41 }, { 60, 39 }, { 62, 41 }, { 62, 42 } };
	for (const Point &position : Candidates) {
		if (!InDungeonBounds(position) || dObject[position.x][position.y] != 0 || TileHasAny(dPiece[position.x][position.y], TileProperties::Solid))
			continue;
		// The entry tile in front must be walkable too, or the rift is unenterable with no message
		// (audit, 2026-09-20).
		const Point entry = position + Displacement { 1, 1 };
		if (!InDungeonBounds(entry) || dObject[entry.x][entry.y] != 0 || TileHasAny(dPiece[entry.x][entry.y], TileProperties::Solid))
			continue;
		Object *gate = AddObject(OBJ_STAND, position);
		if (gate == nullptr)
			continue;
		gate->_oSelFlag = 3;
		gate->_oBreak = 0;
		gate->_oSolidFlag = true;
		ApplyStonegateGraphics(*gate);
		ShowFrame(*gate, ClosedFrame);
		GateObjectId = gate->GetId();
		if (position != Candidates[0])
			LogEvent(StrCat("The Stonegate fell back to (", position.x, ", ", position.y, ")"), UiFlags::ColorRed);

		// Town rebuilt with a rift still open: the hero died in it (plan r10) or came back through
		// the way home. Only the way home ends it - a death AFTER the kill leaves the pile and the
		// keystone on the floor, and the gate stays lit so they can be fetched (audit, 2026-09-20).
		// Nothing to light on the stone any more (the painting is one frame): InitMissiles runs after
		// this and would clear the portal, so RelightStonegateIfNeeded adds it from the first town
		// tick, and that portal is the whole of the open state.
		if (ActiveRift() != RiftKind::None && RiftReturnedHome())
			EndRift();
		return;
	}
	LogEvent("The Stonegate found no ground to stand on", UiFlags::ColorRed);
}

bool IsStonegateObject(const Object &object)
{
	return GateObjectId >= 0 && &object == &Objects[GateObjectId];
}

RiftKind OpenRift()
{
	return ActiveRift();
}

bool StonegateEntryTile(Point &out)
{
	const Object *gate = Gate();
	if (gate == nullptr)
		return false;
	// The tile in front of the opening: south on this map is +1,+1, toward the camera.
	out = gate->position + Displacement { 1, 1 };
	return true;
}

void CloseStonegate()
{
	Object *gate = Gate();
	if (ActiveRift() != RiftKind::None) {
		RemovePortalMissiles();
		PlayUiEventSound(UiEventSound::RiftClose);
	}
	EndRift();
	if (gate != nullptr)
		ShowFrame(*gate, ClosedFrame);
}

void ToggleStonegate()
{
	// The click asks (user, 2026-09-20): the menu offers the Nephalem Rift, the Guardian Rift by keystone,
	// and closing the gate.
	if (Gate() == nullptr || MyPlayer == nullptr)
		return;
	OpenStonegateMenu();
}

bool OpenNephalemAtGate()
{
	Object *gate = Gate();
	if (gate == nullptr || MyPlayer == nullptr)
		return false;
	Player &player = *MyPlayer;
	// Free (plan r6). Its tier is the deepest floor the hero has reached. A rift already standing ends.
	if (!OpenNephalemRift(player)) {
		LogEvent("The Stonegate does not answer.", UiFlags::ColorRed);
		return false;
	}
	LightGate(*gate, RiftKind::Nephalem, /*sound=*/true);
	LogEvent(StrCat("A golden portal opens in the Stonegate: a Nephalem Rift, tier ", RiftTier(), ". Walk in; ",
	             RiftGuardianName(RiftGuardian()), " waits at the end and drops a keystone."),
	    UiFlags::ColorWhitegold);
	return true;
}

void RelightStonegateIfNeeded()
{
	Object *gate = Gate();
	const RiftKind kind = ActiveRift();
	if (gate == nullptr || kind == RiftKind::None || leveltype != DTYPE_TOWN)
		return;
	for (const Missile &missile : Missiles) {
		if (missile._mitype == MissileID::RiftPortalGold || missile._mitype == MissileID::RiftPortalPurple)
			return; // the portal stands
	}
	LightGate(*gate, kind, /*sound=*/false);
}

void LightStonegate(RiftKind kind)
{
	Object *gate = Gate();
	if (gate == nullptr || kind == RiftKind::None)
		return;
	LightGate(*gate, kind, /*sound=*/true);
}

void ProcessStonegate()
{
	// Nothing to animate since 2026-09-20: the gate holds its one painted frame in every state and
	// the portal missile carries the motion. Kept as the tick hook so the call site stays wired for
	// the day the painting gets a state of its own again; it also pins the frame, so a stray
	// _oAnimFrame from an older save cannot show a lit stone.
	Object *gate = Gate();
	if (gate == nullptr)
		return;
	if (gate->_oAnimFrame != ClosedFrame)
		ShowFrame(*gate, ClosedFrame);
}

} // namespace devilution::oracool
