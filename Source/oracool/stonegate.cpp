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
#include "oracool/skill_sounds.h"
#include "player.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

/**
 * @brief The seventeen frames of objects\orclgate.cel (tools/build_stonegate_cel.cmd, batch 42):
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
RiftKind Open = RiftKind::None;

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

} // namespace

void AddStonegateObject()
{
	GateObjectId = -1;
	Open = RiftKind::None;
	if (currlevel != 0 || setlevel)
		return;

	// OBJ_STAND like the Roar: an ordinary type wearing its own art, so nothing in the object
	// tables moves. The rock stand's own sheet is loaded first because SetupObject asks for it.
	PrepareStonegateCarrier();

	// The open ground south-east of the well, away from the Roar's plaza junction (55, 66) and the
	// path to the cathedral; the sprite is three tiles wide, so the neighbours must be clear too.
	constexpr Point Candidates[] = { { 62, 74 }, { 63, 75 }, { 61, 73 }, { 64, 76 }, { 60, 72 }, { 65, 77 } };
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
	return Open;
}

void CloseStonegate()
{
	Object *gate = Gate();
	if (Open != RiftKind::None) {
		RemovePortalMissiles();
		PlayUiEventSound(UiEventSound::RiftClose);
	}
	Open = RiftKind::None;
	if (gate != nullptr)
		ShowFrame(*gate, ClosedFrame);
}

void ToggleStonegate()
{
	Object *gate = Gate();
	if (gate == nullptr || MyPlayer == nullptr)
		return;

	const RiftKind next = Open == RiftKind::None ? RiftKind::Nephalem : Open == RiftKind::Nephalem ? RiftKind::Guardian : RiftKind::None;
	if (next == RiftKind::None) {
		CloseStonegate();
		LogEvent("The Stonegate falls dark.", UiFlags::ColorWhitegold);
		return;
	}

	// Swap rather than stack: one portal in the opening at a time.
	RemovePortalMissiles();
	Open = next;
	AddMissile(gate->position, gate->position, Direction::South, PortalFor(next), TARGET_MONSTERS, MyPlayer->getId(), 0, 0);
	ShowFrame(*gate, next == RiftKind::Nephalem ? GoldFirst : PurpleFirst);
	PlayUiEventSound(UiEventSound::RiftOpen);
	LogEvent(next == RiftKind::Nephalem ? "A golden portal opens in the Stonegate: a Nephalem Rift. (The rift itself is not built yet.)"
	                                    : "A violet portal opens in the Stonegate: a Guardian Rift. (The rift itself is not built yet.)",
	    UiFlags::ColorWhitegold);
}

void ProcessStonegate()
{
	Object *gate = Gate();
	if (gate == nullptr || Open == RiftKind::None)
		return;
	if (++gate->_oAnimCnt < LitDelay)
		return;
	gate->_oAnimCnt = 0;
	const uint32_t first = Open == RiftKind::Nephalem ? GoldFirst : PurpleFirst;
	uint32_t frame = gate->_oAnimFrame + 1;
	if (frame < first || frame >= first + LitFrames)
		frame = first;
	gate->_oAnimFrame = frame;
}

} // namespace devilution::oracool
