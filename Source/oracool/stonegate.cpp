#include "oracool/stonegate.h"

#include <algorithm>

#include "effects.h" // PlaySfxLoc - vanilla's portal sound when the gate lights
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
#include "portal.h" // TownPortalLandingTile - where the inactive arch stands
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

/**
 * @brief objects\orclgate.cel (tools/build_stonegate_cel.cmd) is ONE frame since 2026-09-20: the
 * user's own Rift Monument painting (Resources\02. Oracooll Assets\01. Used\Rift Monument.png -> tools/ScalePainting.ps1, 128x149).
 * The name changed with it ("Rename the stonegate to Rift Monument (also in the hover text)"): the
 * player sees "Rift Monument" everywhere; the code keeps its Stonegate identifiers.
 *
 * The painting does not change with the rift's state (user: "use uncut version of the stonegate
 * asset and just overlap it with portal asset when user selects one of the portals"): an open rift is
 * shown by the portal missile alone, drawn over the arch (AddRiftPortal lifts it 20px into the
 * opening). Batch 45's seventeen-frame cut - closed plus eight gold and eight violet glow frames -
 * is superseded and filed under 02. Oracooll Assets\02. Unused. Object frames are 1-based.
 */
constexpr uint32_t ClosedFrame = 1;
constexpr uint32_t FrameCount = 1;

int GateObjectId = -1;
/** @brief The monument's entry tile when town was last built; (0,0) before the first town. Town's layout
 * is fixed, so this only ever changes if the gate fell back to another candidate tile. */
Point LastEntryTile = { 0, 0 };
/** @brief The inactive arch on the town portal's landing tile, or -1 - see AddPortalArch. */
int PortalArchObjectId = -1;

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

/**
 * @brief Ends the rift, and with it every town portal whose far end stood in it. A portal records the
 * set level it leads to, not which rift, so one cast inside a rift that has since closed would lead into
 * a rift level built from the reset state (external audit of v1.12.188, WORLD-01).
 */
void EndRiftAndItsPortals()
{
	for (int i = 0; i < MAXPORTAL; i++) {
		const Portal &portal = Portals[i];
		if (!portal.open || !portal.setlvl || !IsRiftLevel(static_cast<_setlevels>(portal.level)))
			continue;
		DeactivatePortal(i);
		RemovePortalMissile(i);
	}
	EndRift();
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
	// VANILLA's town-portal opening sound, sentinel.wav - the TownPortal missile's own cast sound (user, 2026-09-20:
	// "when opening rift portals use vanilla portal opening sound"); UiEventSound::RiftOpen is retired here.
	if (sound)
		PlaySfxLoc(LS_SENTINEL, gate.position);
}

} // namespace

namespace {

/**
 * @brief The inactive copy of the monument on the town portal's landing tile (user, 2026-09-20: "Place
 * one inactive copy of its asset on the tile the vanilla portal opens"): WarpDrop[0] = (57, 40), where
 * AddWarpMissile stands the town portal. Its own OBJ_STAND wearing the same painting, and nothing
 * else - unselectable (no hover, no click, no name), NOT solid (the hero walks into the portal on that
 * tile and lands one tile past it), missiles pass, and drawn in the before-characters pass like the
 * gate so the portal and the hero show over the arch. IsStonegateObject answers for it too, which
 * keeps it out of IsLevskiRoarObject's "the other stand in town".
 */
void AddPortalArch()
{
	PortalArchObjectId = -1;
	// ONE TILE SOUTH-EAST of the portal's tile (2026-09-20, from the user's grid screenshot): both
	// sprites are bottom-anchored, the portal's ink ends 14px above its anchor and the painting's
	// opening floor sits ~39px above its own (the plinth is in front), so on the same tile the portal
	// stood in the plinth. (+1, +1) is the grid's one pure vertical step - 32px down the screen, no
	// sideways shift - and lands the portal's foot 7px above the plinth's edge, inside the arch. The
	// arch draws in the FLOOR pass so the portal on the earlier tile still lands over it.
	const Point tile = TownPortalLandingTile(0) + Displacement { 1, 1 };
	if (!InDungeonBounds(tile) || dObject[tile.x][tile.y] != 0)
		return;
	Object *arch = AddObject(OBJ_STAND, tile);
	if (arch == nullptr)
		return;
	arch->_oSelFlag = 0;
	arch->_oBreak = 0;
	arch->_oSolidFlag = false;
	arch->_oMissFlag = true;
	arch->_oPreFlag = true;
	ApplyStonegateGraphics(*arch);
	ShowFrame(*arch, ClosedFrame);
	PortalArchObjectId = arch->GetId();
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

	// At (31, 56) since 2026-09-20 (user: "Move the Rift Monument to 31:56 tile"), the tiles around it
	// as fallbacks. It stood at (60, 40) before - three tiles south-east of the town portal's landing
	// tile, WarpDrop[0] = (57, 40) - and that tile now carries an inactive copy of the painting
	// instead, so the town portal opens inside an arch too (see below).
	constexpr Point Candidates[] = { { 31, 56 }, { 31, 57 }, { 32, 57 }, { 31, 55 }, { 30, 56 }, { 32, 55 } };
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
		// Drawn in the tile's BEFORE-characters pass, so the portal missile - drawn after it on the
		// same tile - lands OVER the painting (user, 2026-09-20: "when rift portal is opened render it
		// over the Rift monument, not behind"). Nothing stands on the gate's tile (it is solid), so
		// the pass changes nothing else about how the monument sorts.
		gate->_oPreFlag = true;
		ApplyStonegateGraphics(*gate);
		ShowFrame(*gate, ClosedFrame);
		GateObjectId = gate->GetId();
		LastEntryTile = entry;
		if (position != Candidates[0])
			LogEvent(StrCat("The Rift Monument fell back to (", position.x, ", ", position.y, ")"), UiFlags::ColorRed);

		// Town rebuilt with a rift still open: the hero died in it (plan r10) or came back through
		// the way home. Only the way home ends it - a death AFTER the kill leaves the pile and the
		// keystone on the floor, and the gate stays lit so they can be fetched (audit, 2026-09-20).
		// Nothing to light on the stone any more (the painting is one frame): InitMissiles runs after
		// this and would clear the portal, so RelightStonegateIfNeeded adds it from the first town
		// tick, and that portal is the whole of the open state.
		if (ActiveRift() != RiftKind::None && RiftReturnedHome())
			EndRiftAndItsPortals();
		AddPortalArch();
		return;
	}
	LogEvent("The Rift Monument found no ground to stand on", UiFlags::ColorRed);
	AddPortalArch();
}

bool IsStonegatePortalArch(const Object &object)
{
	// Town only (audit, 2026-09-27): the two ids are set when town builds its objects and kept after, and a dungeon
	// floor hands the same Objects slot to a door or a barrel - which then drew in the floor pass, under the heroes.
	return leveltype == DTYPE_TOWN && PortalArchObjectId >= 0 && &object == &Objects[PortalArchObjectId];
}

bool IsStonegateObject(const Object &object)
{
	// The gate, or the inactive arch by the portal's tile - both wear the painting, neither is the Cube.
	return (leveltype == DTYPE_TOWN && GateObjectId >= 0 && &object == &Objects[GateObjectId]) || IsStonegatePortalArch(object);
}

RiftKind OpenRift()
{
	return ActiveRift();
}

bool StonegateLastEntryTile(Point &out)
{
	if (LastEntryTile == Point { 0, 0 })
		return false;
	out = LastEntryTile;
	return true;
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
	EndRiftAndItsPortals();
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
		LogEvent("The Rift Monument does not answer.", UiFlags::ColorRed);
		return false;
	}
	LightGate(*gate, RiftKind::Nephalem, /*sound=*/true);
	LogEvent(StrCat("A golden portal opens in the Rift Monument: a Nephalem Rift, tier ", RiftTier(), ". Walk in; ",
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
