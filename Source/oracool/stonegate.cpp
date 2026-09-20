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
#include "player.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

/**
 * @brief The seventeen frames of objects\orclgate.cel (tools/build_stonegate_cel.cmd, batch 44):
 * 1 the closed gate, 2-9 lit gold, 10-17 lit violet. Object frames are 1-based.
 */
constexpr uint32_t ClosedFrame = 1;
constexpr uint32_t GoldFirst = 2;
constexpr uint32_t PurpleFirst = 10;
constexpr uint32_t LitFrames = 8;
constexpr uint32_t FrameCount = 17;
/** @brief Ticks per lit frame: the artist's sinusoidal breathing over eight frames, slow. */
constexpr int LitDelay = 4;

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

/** @brief Lights the gate for @p kind: the portal in the opening, the stone's glow, the sound. */
void LightGate(Object &gate, RiftKind kind, bool sound)
{
	RemovePortalMissiles();
	if (MyPlayer != nullptr)
		AddMissile(gate.position, gate.position, Direction::South, PortalFor(kind), TARGET_MONSTERS, MyPlayer->getId(), 0, 0);
	ShowFrame(gate, kind == RiftKind::Nephalem ? GoldFirst : PurpleFirst);
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
		// the way home. A cleared rift ends here; an unfinished one relights the portal, silently.
		if (ActiveRift() != RiftKind::None) {
			if (RiftDone())
				EndRift();
			else
				LightGate(*gate, ActiveRift(), /*sound=*/false);
		}
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
	Object *gate = Gate();
	if (gate == nullptr || MyPlayer == nullptr)
		return;
	Player &player = *MyPlayer;

	// Closed: a Nephalem Rift, free (plan r6). Its tier is the deepest floor the hero has reached.
	if (ActiveRift() == RiftKind::None) {
		if (!OpenNephalemRift(player)) {
			LogEvent("The Stonegate does not answer.", UiFlags::ColorRed);
			return;
		}
		LightGate(*gate, RiftKind::Nephalem, /*sound=*/true);
		LogEvent(StrCat("A golden portal opens in the Stonegate: a Nephalem Rift, tier ", RiftTier(), ". Walk in; ",
		             RiftGuardianName(RiftGuardian()), " waits at the end and drops a keystone."),
		    UiFlags::ColorWhitegold);
		return;
	}

	// Open: the click closes it and ends the rift. A Guardian Rift is opened by USING a keystone, not
	// by clicking (plan r5), so the gate never cycles.
	CloseStonegate();
	LogEvent("The Stonegate falls dark; the rift is gone.", UiFlags::ColorWhitegold);
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
	Object *gate = Gate();
	const RiftKind open = ActiveRift();
	if (gate == nullptr || open == RiftKind::None)
		return;
	if (++gate->_oAnimCnt < LitDelay)
		return;
	gate->_oAnimCnt = 0;
	const uint32_t first = open == RiftKind::Nephalem ? GoldFirst : PurpleFirst;
	uint32_t frame = gate->_oAnimFrame + 1;
	if (frame < first || frame >= first + LitFrames)
		frame = first;
	gate->_oAnimFrame = frame;
}

} // namespace devilution::oracool
