/**
 * @file scrollrt.cpp
 *
 * Implementation of functionality for rendering the dungeons, monsters and calling other render routines.
 */
#include "engine/render/scrollrt.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "dead.h"
#include "doom.h"
#include "engine/backbuffer_state.hpp"
#include "engine/dx.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/dun_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/trn.hpp"
#include "error.h"
#include "gmenu.h"
#include "help.h"
#include "hwcursor.hpp"
#include "init.h"
#include "inv.h"
#include "lighting.h"
#include "minitext.h"
#include "missiles.h"
#include "monster.h"
#include "nthread.h"
#include "options.h"
#include "oracool/attack_skills.h"
#include "oracool/cursor_tooltip.h"
#include "oracool/event_log.h"
#include "oracool/hud_art.h"
#include "oracool/game_clock.h"
#include "oracool/hud_layout.h"
#include "oracool/hud_menu.h"
#include "oracool/save_indicator.h"
#include "oracool/crafting_menu.h"
#include "oracool/waypoint_menu.h"
#include "oracool/xp_counter.h"
#include "oracool/xp_gain_indicator.h"
#include "panels/charpanel.hpp"
#include "panels/spell_book.hpp"
#include "plrmsg.h"
#include "qol/chatlog.h"
#include "qol/floatingnumbers.h"
#include "qol/itemlabels.h"
#include "qol/monhealthbar.h"
#include "qol/stash.h"
#include "qol/xpbar.h"
#include "stores.h"
#include "towners.h"
#include "utils/bitset2d.hpp"
#include "utils/display.h"
#include "utils/endian_read.hpp"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

#ifndef USE_SDL1
#include "controls/touch/renderers.h"
#endif

#ifdef _DEBUG
#include "debug.h"
#endif

#ifdef DUN_RENDER_STATS
#include "utils/format_int.hpp"
#endif

namespace devilution {

/**
 * Specifies the current light entry.
 */
int LightTableIndex;

bool AutoMapShowItems;

// DevilutionX extension.
extern void DrawControllerModifierHints(const Surface &out);

bool frameflag;

namespace {

/**
 * @brief Hash algorithm for point
 */
struct PointHash {
	std::size_t operator()(Point const &s) const noexcept
	{
		return s.x ^ (s.y << 1);
	}
};

/**
 * @brief Contains all Missile at rendering position
 */
std::unordered_multimap<Point, Missile *, PointHash> MissilesAtRenderingTile;

/**
 * @brief Could the missile (at the next game tick) collide? This method is a simplified version of CheckMissileCol (for example without random).
 */
bool CouldMissileCollide(Point tile, bool checkPlayerAndMonster)
{
	if (!InDungeonBounds(tile))
		return true;
	if (checkPlayerAndMonster) {
		if (dMonster[tile.x][tile.y] > 0)
			return true;
		if (dPlayer[tile.x][tile.y] > 0)
			return true;
	}

	return IsMissileBlockedByTile(tile);
}

void UpdateMissilePositionForRendering(Missile &m, int progress)
{
	DisplacementOf<int64_t> velocity = m.position.velocity;
	velocity *= progress;
	velocity /= AnimationInfo::baseValueFraction;
	Displacement pixelsTravelled = (m.position.traveled + Displacement { static_cast<int>(velocity.deltaX), static_cast<int>(velocity.deltaY) }) >> 16;
	Displacement tileOffset = pixelsTravelled.screenToMissile();

	// calculcate the future missile position
	m.position.tileForRendering = m.position.start + tileOffset;
	m.position.offsetForRendering = pixelsTravelled + tileOffset.worldToScreen();
}

void UpdateMissileRendererData(Missile &m)
{
	m.position.tileForRendering = m.position.tile;
	m.position.offsetForRendering = m.position.offset;

	const MissileMovementDistribution missileMovement = GetMissileData(m._mitype).movementDistribution;
	// don't calculate missile position if they don't move
	if (missileMovement == MissileMovementDistribution::Disabled || m.position.velocity == Displacement {})
		return;

	int progress = ProgressToNextGameTick;
	UpdateMissilePositionForRendering(m, progress);

	// In some cases this calculcated position is invalid.
	// For example a missile shouldn't move inside a wall.
	// In this case the game logic don't advance the missile position and removes the missile or shows an explosion animation at the old position.
	// For the animation distribution logic this means we are not allowed to move to a tile where the missile could collide, cause this could be a invalid position.

	// If we are still at the current tile, this tile was already checked and is a valid tile
	if (m.position.tileForRendering == m.position.tile)
		return;

	// If no collision can happen at the new tile we can advance
	if (!CouldMissileCollide(m.position.tileForRendering, missileMovement == MissileMovementDistribution::Blockable))
		return;

	// The new tile could be invalid, so don't advance to it.
	// We search the last offset that is in the old (valid) tile.
	// Implementation note: If someone knows the correct math to calculate this without the loop, I would really appreciate it.
	while (m.position.tile != m.position.tileForRendering) {
		progress -= 1;

		if (progress <= 0) {
			m.position.tileForRendering = m.position.tile;
			m.position.offsetForRendering = m.position.offset;
			return;
		}

		UpdateMissilePositionForRendering(m, progress);
	}
}

void UpdateMissilesRendererData()
{
	MissilesAtRenderingTile.clear();

	for (auto &m : Missiles) {
		UpdateMissileRendererData(m);
		MissilesAtRenderingTile.insert(std::make_pair(m.position.tileForRendering, &m));
	}
}

/**
 * @brief Keeps track of which tiles have been rendered already.
 */
Bitset2d<MAXDUNX, MAXDUNY> dRendered;

int lastFpsUpdateInMs;

const char *const PlayerModeNames[] = {
	"standing",
	"walking (1)",
	"walking (2)",
	"walking (3)",
	"attacking (melee)",
	"attacking (ranged)",
	"blocking",
	"getting hit",
	"dying",
	"casting a spell",
	"changing levels",
	"quitting"
};

Rectangle PrevCursorRect;

void BlitCursor(uint8_t *dst, uint32_t dstPitch, uint8_t *src, uint32_t srcPitch, uint32_t srcWidth, uint32_t srcHeight)
{
	for (std::uint32_t i = 0; i < srcHeight; ++i, src += srcPitch, dst += dstPitch) {
		memcpy(dst, src, srcWidth);
	}
}

/**
 * @brief Remove the cursor from the buffer
 */
void UndrawCursor(const Surface &out)
{
	DrawnCursor &cursor = GetDrawnCursor();
	BlitCursor(&out[cursor.rect.position], out.pitch(), cursor.behindBuffer, cursor.rect.size.width, cursor.rect.size.width, cursor.rect.size.height);
	PrevCursorRect = cursor.rect;
}

bool ShouldShowCursor()
{
	if (ControlMode == ControlTypes::KeyboardAndMouse)
		return true;
	if (pcurs == CURSOR_TELEPORT)
		return true;
	if (invflag)
		return true;
	if (chrflag && MyPlayer->_pStatPts > 0)
		return true;

	return false;
}

/**
 * @brief Blit CL2 sprite, and apply lighting, to the given buffer at the given coordinates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
void ClxDrawLight(const Surface &out, Point position, ClxSprite clx, int lightTableIndex)
{
	if (lightTableIndex != 0) {
		ClxDrawTRN(out, position, clx, LightTables[lightTableIndex].data());
	} else {
		ClxDraw(out, position, clx);
	}
}

/**
 * @brief Blit CL2 sprite, and apply lighting and transparency blending, to the given buffer at the given coordinates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
void ClxDrawLightBlended(const Surface &out, Point position, ClxSprite clx, int lightTableIndex)
{
	if (lightTableIndex != 0) {
		ClxDrawBlendedTRN(out, position, clx, LightTables[lightTableIndex].data());
	} else {
		ClxDrawBlended(out, position, clx);
	}
}

/**
 * @brief Save the content behind the cursor to a temporary buffer, then draw the cursor.
 */
void DrawCursor(const Surface &out)
{
	DrawnCursor &cursor = GetDrawnCursor();
	if (IsHardwareCursor()) {
		SetHardwareCursorVisible(ShouldShowCursor());
		cursor.rect.size = { 0, 0 };
		return;
	}

	if (pcurs <= CURSOR_NONE || !ShouldShowCursor()) {
		cursor.rect.size = { 0, 0 };
		return;
	}

	Size cursSize = GetInvItemSize(pcurs);
	if (cursSize.width == 0 || cursSize.height == 0) {
		cursor.rect.size = { 0, 0 };
		return;
	}

	constexpr auto Clip = [](int &pos, int &length, int posEnd) {
		if (pos + length <= 0 || pos >= posEnd) {
			pos = 0;
			length = 0;
		} else if (pos < 0) {
			length += pos;
			pos = 0;
		} else if (pos + length > posEnd) {
			length = posEnd - pos;
		}
	};

	// Copy the buffer before the item cursor and its 1px outline are drawn to a temporary buffer.
	const int outlineWidth = !MyPlayer->HoldItem.isEmpty() ? 1 : 0;
	Displacement offset = !MyPlayer->HoldItem.isEmpty() ? Displacement { cursSize / 2 } : Displacement { 0 };
	Point cursPosition = MousePosition - offset;

	Rectangle &rect = cursor.rect;
	rect.position.x = cursPosition.x - outlineWidth;
	rect.size.width = cursSize.width + 2 * outlineWidth;
	Clip(rect.position.x, rect.size.width, out.w());

	rect.position.y = cursPosition.y - outlineWidth;
	rect.size.height = cursSize.height + 2 * outlineWidth;
	Clip(rect.position.y, rect.size.height, out.h());

	if (rect.size.width == 0 || rect.size.height == 0)
		return;

	BlitCursor(cursor.behindBuffer, rect.size.width, &out[rect.position], out.pitch(), rect.size.width, rect.size.height);
	DrawSoftwareCursor(out, cursPosition + Displacement { 0, cursSize.height - 1 }, pcurs);
}

/**
 * @brief Render a missile sprite
 * @param out Output buffer
 * @param missile Pointer to Missile struct
 * @param targetBufferPosition Output buffer coordinate
 * @param pre Is the sprite in the background
 */
void DrawMissilePrivate(const Surface &out, const Missile &missile, Point targetBufferPosition, bool pre, int lightTableIndex)
{
	if (missile._miPreFlag != pre || !missile._miDrawFlag)
		return;

	const Point missileRenderPosition { targetBufferPosition + missile.position.offsetForRendering - Displacement { missile._miAnimWidth2, 0 } };
	const ClxSprite sprite = (*missile._miAnimData)[missile._miAnimFrame - 1];
	// Oracool: a caller-supplied recolour, checked before the unique-monster one because a player's
	// missile can never have the latter. See Missile::oracoolTrn.
	if (missile.oracoolTrn != nullptr)
		ClxDrawTRN(out, missileRenderPosition, sprite, missile.oracoolTrn);
	else if (missile._miUniqTrans != 0)
		ClxDrawTRN(out, missileRenderPosition, sprite, Monsters[missile._misource].uniqueMonsterTRN.get());
	else if (missile._miLightFlag)
		ClxDrawLight(out, missileRenderPosition, sprite, lightTableIndex);
	else
		ClxDraw(out, missileRenderPosition, sprite);
}

/**
 * @brief Render a missile sprites for a given tile
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param pre Is the sprite in the background
 */
void DrawMissile(const Surface &out, Point tilePosition, Point targetBufferPosition, bool pre, int lightTableIndex)
{
	const auto range = MissilesAtRenderingTile.equal_range(tilePosition);
	for (auto it = range.first; it != range.second; it++) {
		DrawMissilePrivate(out, *it->second, targetBufferPosition, pre, lightTableIndex);
	}
}

/**
 * @brief Render a monster sprite
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param monster Monster reference
 */
void DrawMonster(const Surface &out, Point tilePosition, Point targetBufferPosition, const Monster &monster)
{
	if (!monster.animInfo.sprites) {
		Log("Draw Monster \"{}\": NULL Cel Buffer", monster.name());
		return;
	}

	const ClxSprite sprite = monster.animInfo.currentSprite();

	if (!IsTileLit(tilePosition)) {
		ClxDrawTRN(out, targetBufferPosition, sprite, GetInfravisionTRN());
		return;
	}
	uint8_t *trn = nullptr;
	if (monster.isUnique())
		trn = monster.uniqueMonsterTRN.get();
	if (monster.mode == MonsterMode::Petrified)
		trn = GetStoneTRN();
	if (MyPlayer->_pInfraFlag && LightTableIndex > 8)
		trn = GetInfravisionTRN();
	if (trn != nullptr)
		ClxDrawTRN(out, targetBufferPosition, sprite, trn);
	else
		ClxDrawLight(out, targetBufferPosition, sprite, LightTableIndex);
}

/**
 * @brief Helper for rendering a specific player icon (Mana Shield or Reflect)
 */
void DrawPlayerIconHelper(const Surface &out, MissileGraphicID missileGraphicId, Point position, bool lighting, bool infraVision)
{
	position.x -= GetMissileSpriteData(missileGraphicId).animWidth2;

	const ClxSprite sprite = (*GetMissileSpriteData(missileGraphicId).sprites).list()[0];

	if (!lighting) {
		ClxDraw(out, position, sprite);
		return;
	}

	if (infraVision) {
		ClxDrawTRN(out, position, sprite, GetInfravisionTRN());
		return;
	}

	ClxDrawLight(out, position, sprite, LightTableIndex);
}

/**
 * @brief Helper for rendering player icons (Mana Shield and Reflect)
 * @param out Output buffer
 * @param player Player reference
 * @param position Output buffer coordinates
 * @param infraVision Should infravision be applied
 */
void DrawPlayerIcons(const Surface &out, const Player &player, Point position, bool infraVision)
{
	if (player.pManaShield)
		DrawPlayerIconHelper(out, MissileGraphicID::ManaShield, position, &player != MyPlayer, infraVision);
	if (player.wReflections > 0)
		DrawPlayerIconHelper(out, MissileGraphicID::Reflect, position + Displacement { 0, 16 }, &player != MyPlayer, infraVision);
}

/**
 * @brief Render a player sprite
 * @param out Output buffer
 * @param player Player reference
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 */
void DrawPlayer(const Surface &out, const Player &player, Point tilePosition, Point targetBufferPosition)
{
	if (!IsTileLit(tilePosition) && !MyPlayer->_pInfraFlag && !MyPlayer->isOnArenaLevel() && leveltype != DTYPE_TOWN) {
		return;
	}

	const ClxSprite sprite = player.previewCelSprite ? *player.previewCelSprite : player.AnimInfo.currentSprite();

	Point spriteBufferPosition = targetBufferPosition - Displacement { CalculateWidth2(sprite.width()), 0 };

	if (static_cast<size_t>(pcursplr) < Players.size() && &player == &Players[pcursplr])
		ClxDrawOutlineSkipColorZero(out, 165, spriteBufferPosition, sprite);

	if (&player == MyPlayer && IsNoneOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
		ClxDraw(out, spriteBufferPosition, sprite);
		DrawPlayerIcons(out, player, targetBufferPosition, false);
		return;
	}

	if (!IsTileLit(tilePosition) || ((MyPlayer->_pInfraFlag || MyPlayer->isOnArenaLevel()) && LightTableIndex > 8)) {
		ClxDrawTRN(out, spriteBufferPosition, sprite, GetInfravisionTRN());
		DrawPlayerIcons(out, player, targetBufferPosition, true);
		return;
	}

	int l = LightTableIndex;
	if (LightTableIndex < 5)
		LightTableIndex = 0;
	else
		LightTableIndex -= 5;

	ClxDrawLight(out, spriteBufferPosition, sprite, LightTableIndex);
	DrawPlayerIcons(out, player, targetBufferPosition, false);

	LightTableIndex = l;
}

/**
 * @brief Render a player sprite
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 */
void DrawDeadPlayer(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	dFlags[tilePosition.x][tilePosition.y] &= ~DungeonFlag::DeadPlayer;

	for (Player &player : Players) {
		if (player.plractive && player._pHitPoints == 0 && player.isOnActiveLevel() && player.position.tile == tilePosition) {
			dFlags[tilePosition.x][tilePosition.y] |= DungeonFlag::DeadPlayer;
			const Point playerRenderPosition { targetBufferPosition };
			DrawPlayer(out, player, tilePosition, playerRenderPosition);
		}
	}
}

/**
 * @brief Which of the three passes an object's sprite belongs in.
 *
 * Vanilla has two, selected by `_oPreFlag`, and both run inside the per-tile content loop: one
 * before that tile's characters and one after. Oracool adds a third that runs during the whole
 * viewport's floor pass, i.e. beneath every tile's contents rather than just its own.
 */
enum class ObjectDrawPass : uint8_t {
	Floor,
	BeforeCharacters,
	AfterCharacters,
};

/**
 * @brief Oracool: user report - the waypoint platform drew on top of the player and of nearby
 * monsters.
 *
 * `_oPreFlag` only orders an object against its OWN tile's contents. The waypoint's sprite is
 * 106px tall, roughly three tile-rows, so anything standing on a tile behind it - drawn earlier in
 * the back-to-front sweep - was painted over by a sprite that visually belongs on the floor. That
 * is the ordinary fate of any object drawn much larger than its tile.
 *
 * Rather than shrink the art or give the object a multi-tile footprint, the waypoint moves to the
 * floor pass: it is a floor platform, so drawing it with the floor is both correct and what the
 * user asked for ("it just needs to render way behind"). The cost is that its pillars also pass
 * under anything standing in front of them, which is the expected behaviour for a floor decal.
 */
bool IsFloorPassObject(const Object &object)
{
	return object._otype == _object_id::OBJ_WAYPOINT;
}

ObjectDrawPass GetObjectDrawPass(const Object &object)
{
	if (IsFloorPassObject(object))
		return ObjectDrawPass::Floor;
	return object._oPreFlag ? ObjectDrawPass::BeforeCharacters : ObjectDrawPass::AfterCharacters;
}

/**
 * @brief Render an object sprite
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param pass Which draw pass is currently running
 */
void DrawObject(const Surface &out, Point tilePosition, Point targetBufferPosition, ObjectDrawPass pass)
{
	if (LightTableIndex >= LightsMax) {
		return;
	}

	Object *object = FindObjectAtPosition(tilePosition);
	if (object == nullptr) {
		return;
	}

	const Object &objectToDraw = *object;
	if (GetObjectDrawPass(objectToDraw) != pass) {
		return;
	}

	const ClxSprite sprite = (*objectToDraw._oAnimData)[objectToDraw._oAnimFrame - 1];

	Point screenPosition = targetBufferPosition - Displacement { CalculateWidth2(sprite.width()), 0 };
	if (objectToDraw.position != tilePosition) {
		// drawing a large or offset object, calculate the correct position for the center of the sprite
		Displacement worldOffset = objectToDraw.position - tilePosition;
		screenPosition -= worldOffset.worldToScreen();
	}

	if (&objectToDraw == ObjectUnderCursor) {
		ClxDrawOutlineSkipColorZero(out, 194, screenPosition, sprite);
	}
	if (objectToDraw.applyLighting) {
		ClxDrawLight(out, screenPosition, sprite, LightTableIndex);
	} else {
		ClxDraw(out, screenPosition, sprite);
	}
}

static void DrawDungeon(const Surface & /*out*/, Point /*tilePosition*/, Point /*targetBufferPosition*/);

/**
 * @brief Render a cell
 * @param out Target buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinates
 */
void DrawCell(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	const uint16_t levelPieceId = dPiece[tilePosition.x][tilePosition.y];
	const MICROS *pMap = &DPieceMicros[levelPieceId];

	const uint8_t *tbl = LightTables[LightTableIndex].data();
#ifdef _DEBUG
	if (DebugPath && MyPlayer->IsPositionInPath(tilePosition))
		tbl = GetPauseTRN();
#endif

	bool transparency = TileHasAny(levelPieceId, TileProperties::Transparent) && TransList[dTransVal[tilePosition.x][tilePosition.y]];
#ifdef _DEBUG
	if ((SDL_GetModState() & KMOD_ALT) != 0)
		transparency = false;
#endif
	const bool foliage = !TileHasAny(levelPieceId, TileProperties::Solid);

	const auto getFirstTileMaskLeft = [=](TileType tile) -> MaskType {
		if (transparency) {
			switch (tile) {
			case TileType::LeftTrapezoid:
			case TileType::TransparentSquare:
				return TileHasAny(levelPieceId, TileProperties::TransparentLeft)
				    ? MaskType::Left
				    : MaskType::Solid;
			case TileType::LeftTriangle:
				return MaskType::Solid;
			default:
				return MaskType::Transparent;
			}
		}
		if (foliage)
			return MaskType::LeftFoliage;
		return MaskType::Solid;
	};

	const auto getFirstTileMaskRight = [=](TileType tile) -> MaskType {
		if (transparency) {
			switch (tile) {
			case TileType::RightTrapezoid:
			case TileType::TransparentSquare:
				return TileHasAny(levelPieceId, TileProperties::TransparentRight)
				    ? MaskType::Right
				    : MaskType::Solid;
			case TileType::RightTriangle:
				return MaskType::Solid;
			default:
				return MaskType::Transparent;
			}
		}
		if (foliage)
			return MaskType::RightFoliage;
		return MaskType::Solid;
	};

	// The first micro tile may be rendered with a foliage mask.
	// Only `TransparentSquare` tiles are rendered when `foliage` is true.
	{
		{
			const LevelCelBlock levelCelBlock { pMap->mt[0] };
			const TileType tileType = levelCelBlock.type();
			const MaskType maskType = getFirstTileMaskLeft(tileType);
			if (levelCelBlock.hasValue()) {
				if (maskType != MaskType::LeftFoliage || tileType == TileType::TransparentSquare) {
					RenderTile(out, targetBufferPosition,
					    levelCelBlock, maskType, tbl);
				}
			}
		}
		{
			const LevelCelBlock levelCelBlock { pMap->mt[1] };
			const TileType tileType = levelCelBlock.type();
			const MaskType maskType = getFirstTileMaskRight(tileType);
			if (levelCelBlock.hasValue()) {
				if (transparency || !foliage || levelCelBlock.type() == TileType::TransparentSquare) {
					if (maskType != MaskType::RightFoliage || tileType == TileType::TransparentSquare) {
						RenderTile(out, targetBufferPosition + Displacement { TILE_WIDTH / 2, 0 },
						    levelCelBlock, maskType, tbl);
					}
				}
			}
		}
		targetBufferPosition.y -= TILE_HEIGHT;
	}

	for (uint_fast8_t i = 2, n = MicroTileLen; i < n; i += 2) {
		{
			const LevelCelBlock levelCelBlock { pMap->mt[i] };
			if (levelCelBlock.hasValue()) {
				RenderTile(out, targetBufferPosition,
				    levelCelBlock,
				    transparency ? MaskType::Transparent : MaskType::Solid, tbl);
			}
		}
		{
			const LevelCelBlock levelCelBlock { pMap->mt[i + 1] };
			if (levelCelBlock.hasValue()) {
				RenderTile(out, targetBufferPosition + Displacement { TILE_WIDTH / 2, 0 },
				    levelCelBlock,
				    transparency ? MaskType::Transparent : MaskType::Solid, tbl);
			}
		}
		targetBufferPosition.y -= TILE_HEIGHT;
	}
}

/**
 * @brief Render a floor tile.
 * @param out Target buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinate
 */
void DrawFloor(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	LightTableIndex = dLight[tilePosition.x][tilePosition.y];

	const uint8_t *tbl = LightTables[LightTableIndex].data();
#ifdef _DEBUG
	if (DebugPath && MyPlayer->IsPositionInPath(tilePosition))
		tbl = GetPauseTRN();
#endif

	const uint16_t levelPieceId = dPiece[tilePosition.x][tilePosition.y];
	{
		const LevelCelBlock levelCelBlock { DPieceMicros[levelPieceId].mt[0] };
		if (levelCelBlock.hasValue()) {
			RenderTile(out, targetBufferPosition,
			    levelCelBlock, MaskType::Solid, tbl);
		}
	}
	{
		const LevelCelBlock levelCelBlock { DPieceMicros[levelPieceId].mt[1] };
		if (levelCelBlock.hasValue()) {
			RenderTile(out, targetBufferPosition + Displacement { TILE_WIDTH / 2, 0 },
			    levelCelBlock, MaskType::Solid, tbl);
		}
	}

	// Oracool: floor-pass objects (see IsFloorPassObject) draw here, on top of their own tile but
	// beneath every tile's characters, items and missiles - the whole floor sweep completes before
	// DrawTileContent starts.
	DrawObject(out, tilePosition, targetBufferPosition, ObjectDrawPass::Floor);
}

/**
 * @brief Draw item for a given tile
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param pre Is the sprite in the background
 */
void DrawItem(const Surface &out, Point tilePosition, Point targetBufferPosition, bool pre)
{
	int8_t bItem = dItem[tilePosition.x][tilePosition.y];

	if (bItem <= 0)
		return;

	auto &item = Items[bItem - 1];
	if (item._iPostDraw == pre)
		return;

	const ClxSprite sprite = item.AnimInfo.currentSprite();
	int px = targetBufferPosition.x - CalculateWidth2(sprite.width());
	const Point position { px, targetBufferPosition.y };
	if (stextflag == TalkID::None && (bItem - 1 == pcursitem || AutoMapShowItems)) {
		ClxDrawOutlineSkipColorZero(out, GetOutlineColor(item, false), position, sprite);
	}
	ClxDrawLight(out, position, sprite, LightTableIndex);
	// Oracool: a broken (0 durability) item dropped on the ground (e.g. from a player death) also
	// gets the red X - this is a completely separate rendering path from cursor.cpp's DrawItem
	// (used only for UI panels: inventory/belt/equipped/tabs), so it needed its own call to the
	// shared DrawBrokenItemMarker helper.
	if (item._iOracoolBroken) {
		const int width = static_cast<int>(sprite.width());
		const int height = static_cast<int>(sprite.height());
		DrawBrokenItemMarker(out, { position.x, position.y - height }, width, height);
	}
	if (item.AnimInfo.isLastFrame() || item._iCurs == ICURS_MAGIC_ROCK)
		AddItemToLabelQueue(bItem - 1, position);
}

/**
 * @brief Oracool: monster outlines queued by DrawMonsterHelper for a monster whose body is
 * currently hidden behind a wall (or other architecture drawn after it in the normal scene
 * sweep). Drawn in a second pass, once the whole scene is composited, so the outline lands on
 * top instead of being covered by whatever occluded the body - the same deferred-queue pattern
 * qol/itemlabels.cpp already uses for item name labels.
 */
struct HiddenMonsterOutline {
	Point position;
	ClxSprite sprite;
};
std::vector<HiddenMonsterOutline> HiddenMonsterOutlineQueue;

/**
 * @brief Oracool: draws the red outline for every monster queued this frame as hidden behind
 * architecture. Always called once per frame from DrawView so the queue never accumulates stale
 * entries, regardless of whether Monster Wall Outline is enabled.
 */
void DrawMonsterWallOutlines(const Surface &out)
{
	for (const HiddenMonsterOutline &entry : HiddenMonsterOutlineQueue) {
		ClxDrawOutlineSkipColorZero(out, 233, entry.position, entry.sprite);
	}
	HiddenMonsterOutlineQueue.clear();
}

/**
 * @brief Check if and how a monster should be rendered
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 */
void DrawMonsterHelper(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	int mi = dMonster[tilePosition.x][tilePosition.y];
	bool isNegativeMonster = mi < 0;
	mi = abs(mi) - 1;

	if (leveltype == DTYPE_TOWN) {
		if (isNegativeMonster)
			return;
		auto &towner = Towners[mi];
		int px = targetBufferPosition.x - CalculateWidth2(towner._tAnimWidth);
		const Point position { px, targetBufferPosition.y };
		const ClxSprite sprite = towner.currentSprite();
		if (mi == pcursmonst) {
			ClxDrawOutlineSkipColorZero(out, 166, position, sprite);
		}
		ClxDraw(out, position, sprite);
		return;
	}

	if (static_cast<size_t>(mi) >= MaxMonsters) {
		Log("Draw Monster: tried to draw illegal monster {}", mi);
		return;
	}

	const auto &monster = Monsters[mi];
	if ((monster.flags & MFLAG_HIDDEN) != 0) {
		return;
	}

	const ClxSprite sprite = monster.animInfo.currentSprite();

	Displacement offset = {};
	if (monster.isWalking()) {
		bool isSideWalkingToLeft = monster.mode == MonsterMode::MoveSideways && monster.direction == Direction::West;
		if (isNegativeMonster && !isSideWalkingToLeft)
			return;
		if (!isNegativeMonster && isSideWalkingToLeft)
			return;
		offset = GetOffsetForWalking(monster.animInfo, monster.direction);
		if (isSideWalkingToLeft)
			offset -= Displacement { 64, 0 };
	} else if (isNegativeMonster) {
		return;
	}

	const Point monsterRenderPosition { targetBufferPosition + offset - Displacement { CalculateWidth2(sprite.width()), 0 } };
	const bool tileLit = IsTileLit(tilePosition) || MyPlayer->_pInfraFlag;
	// Oracool: user request - the same red outline normally shown only for the hovered monster
	// (pcursmonst) also applies to any monster within the configured range, so nearby threats
	// stand out even before the cursor finds them. 0 (OFF) never triggers this extra check.
	const int monsterRangeHighlight = *sgOptions.Oracool.monsterRangeHighlight;
	const bool inHighlightRange = monsterRangeHighlight > 0
	    && monster.position.tile.WalkingDistance(MyPlayer->position.tile) <= monsterRangeHighlight;
	// Oracool bug fix: user report - both outlines below used to be skipped entirely whenever the
	// monster's own tile wasn't currently lit, because the tile-lit check used to run before any of
	// this and return immediately. That defeated the whole point of an "always on top" outline - a
	// monster hidden by darkness is exactly the same "can't normally see it, but still want the
	// outline" situation as one hidden by a wall. The lit check (tileLit, above) is now only used
	// to gate the monster's own sprite at the very end of this function, never these outlines.
	const bool shouldOutlineMonster = mi == pcursmonst || inHighlightRange;
	if (shouldOutlineMonster) {
		ClxDrawOutlineSkipColorZero(out, 233, monsterRenderPosition, sprite);
	}
	// Oracool bug fix: user report - in cave tilesets (e.g. Poisoned Water Supply), a monster's
	// Range Highlight/hover outline could still get painted over by a neighboring tile's wall even
	// though the monster's own tile was fully lit with clear line of sight. Cave wall graphics are
	// taller/more overhanging than cathedral or catacombs walls and visually overlap into adjacent
	// tiles well beyond what the tileLit/LineClearMissile check (designed to approximate "is this
	// monster hidden") accounts for - that check is a reasonable heuristic for whether to bother
	// queuing a plain hidden-behind-a-wall outline, but it isn't a reliable predictor of whether
	// architecture will visually paint over an outline drawn in this same first pass. Any outline
	// already drawn above is therefore also unconditionally queued for the guaranteed-on-top second
	// pass (see DrawMonsterWallOutlines) - redrawing pixels that are already visible is a harmless
	// no-op, so there's no downside to always doing it regardless of tileset.
	if (shouldOutlineMonster || (*sgOptions.Oracool.monsterWallOutline && (!tileLit || !LineClearMissile(MyPlayer->position.tile, monster.position.tile)))) {
		HiddenMonsterOutlineQueue.push_back({ monsterRenderPosition, sprite });
	}

	if (!tileLit)
		return;

	DrawMonster(out, tilePosition, monsterRenderPosition, monster);
}

/**
 * @brief Check if and how a player should be rendered
 * @param out Output buffer
 * @param player Player reference
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 */
void DrawPlayerHelper(const Surface &out, const Player &player, Point tilePosition, Point targetBufferPosition)
{
	Displacement offset = {};
	if (player.isWalking()) {
		offset = GetOffsetForWalking(player.AnimInfo, player._pdir);
	}

	const Point playerRenderPosition { targetBufferPosition + offset };

	DrawPlayer(out, player, tilePosition, playerRenderPosition);
}

/**
 * @brief Render object sprites
 * @param out Target buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinates
 */
void DrawDungeon(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	assert(InDungeonBounds(tilePosition));

	if (dRendered.test(tilePosition.x, tilePosition.y))
		return;
	dRendered.set(tilePosition.x, tilePosition.y);

	LightTableIndex = dLight[tilePosition.x][tilePosition.y];

	DrawCell(out, tilePosition, targetBufferPosition);

	int8_t bDead = dCorpse[tilePosition.x][tilePosition.y];
	int8_t bMap = dTransVal[tilePosition.x][tilePosition.y];

#ifdef _DEBUG
	if (DebugVision && IsTileLit(tilePosition)) {
		ClxDraw(out, targetBufferPosition, (*pSquareCel)[0]);
	}
#endif

	if (MissilePreFlag) {
		DrawMissile(out, tilePosition, targetBufferPosition, true, LightTableIndex);
	}

	if (LightTableIndex < LightsMax && bDead != 0) {
		Corpse &corpse = Corpses[(bDead & 0x1F) - 1];
		const Point position { targetBufferPosition.x - CalculateWidth2(corpse.width), targetBufferPosition.y };
		const ClxSprite sprite = corpse.spritesForDirection(static_cast<Direction>((bDead >> 5) & 7))[corpse.frame];
		if (corpse.translationPaletteIndex != 0) {
			const uint8_t *trn = Monsters[corpse.translationPaletteIndex - 1].uniqueMonsterTRN.get();
			ClxDrawTRN(out, position, sprite, trn);
		} else {
			ClxDrawLight(out, position, sprite, LightTableIndex);
		}
	}
	DrawObject(out, tilePosition, targetBufferPosition, ObjectDrawPass::BeforeCharacters);
	DrawItem(out, tilePosition, targetBufferPosition, true);

	if (TileContainsDeadPlayer(tilePosition)) {
		DrawDeadPlayer(out, tilePosition, targetBufferPosition);
	}
	int8_t playerId = dPlayer[tilePosition.x][tilePosition.y];
	if (static_cast<size_t>(playerId - 1) < Players.size()) {
		DrawPlayerHelper(out, Players[playerId - 1], tilePosition, targetBufferPosition);
	}
	if (dMonster[tilePosition.x][tilePosition.y] != 0) {
		DrawMonsterHelper(out, tilePosition, targetBufferPosition);
	}
	DrawMissile(out, tilePosition, targetBufferPosition, false, LightTableIndex);
	DrawObject(out, tilePosition, targetBufferPosition, ObjectDrawPass::AfterCharacters);
	DrawItem(out, tilePosition, targetBufferPosition, false);

	if (leveltype != DTYPE_TOWN) {
		char bArch = dSpecial[tilePosition.x][tilePosition.y];
		if (bArch != 0) {
			bool transparency = TransList[bMap];
#ifdef _DEBUG
			// Turn transparency off here for debugging
			transparency = transparency && (SDL_GetModState() & KMOD_ALT) == 0;
#endif
			if (transparency) {
				ClxDrawLightBlended(out, targetBufferPosition, (*pSpecialCels)[bArch - 1], LightTableIndex);
			} else {
				ClxDrawLight(out, targetBufferPosition, (*pSpecialCels)[bArch - 1], LightTableIndex);
			}
		}
	} else {
		// Tree leaves should always cover player when entering or leaving the tile,
		// So delay the rendering until after the next row is being drawn.
		// This could probably have been better solved by sprites in screen space.
		if (tilePosition.x > 0 && tilePosition.y > 0 && targetBufferPosition.y > TILE_HEIGHT) {
			char bArch = dSpecial[tilePosition.x - 1][tilePosition.y - 1];
			if (bArch != 0) {
				ClxDraw(out, targetBufferPosition + Displacement { 0, -TILE_HEIGHT }, (*pSpecialCels)[bArch - 1]);
			}
		}
	}
}

/**
 * @brief Render a row of tiles
 * @param out Buffer to render to
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinates
 * @param rows Number of rows
 * @param columns Tile in a row
 */
void DrawFloor(const Surface &out, Point tilePosition, Point targetBufferPosition, int rows, int columns)
{
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			if (InDungeonBounds(tilePosition)) {
				if (!TileHasAny(dPiece[tilePosition.x][tilePosition.y], TileProperties::Solid))
					DrawFloor(out, tilePosition, targetBufferPosition);
			} else {
				world_draw_black_tile(out, targetBufferPosition.x, targetBufferPosition.y);
			}
			tilePosition += Direction::East;
			targetBufferPosition.x += TILE_WIDTH;
		}
		// Return to start of row
		tilePosition += Displacement(Direction::West) * columns;
		targetBufferPosition.x -= columns * TILE_WIDTH;

		// Jump to next row
		targetBufferPosition.y += TILE_HEIGHT / 2;
		if ((i & 1) != 0) {
			tilePosition.x++;
			columns--;
			targetBufferPosition.x += TILE_WIDTH / 2;
		} else {
			tilePosition.y++;
			columns++;
			targetBufferPosition.x -= TILE_WIDTH / 2;
		}
	}
}

bool IsWall(Point position)
{
	return TileHasAny(dPiece[position.x][position.y], TileProperties::Solid) || dSpecial[position.x][position.y] != 0;
}

/**
 * @brief Render a row of tile
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Buffer coordinates
 * @param rows Number of rows
 * @param columns Tile in a row
 */
void DrawTileContent(const Surface &out, Point tilePosition, Point targetBufferPosition, int rows, int columns)
{
	// Keep evaluating until MicroTiles can't affect screen
	rows += MicroTileLen;
	dRendered.reset();

	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			if (InDungeonBounds(tilePosition)) {
#ifdef _DEBUG
				DebugCoordsMap[tilePosition.x + tilePosition.y * MAXDUNX] = targetBufferPosition;
#endif
				if (tilePosition.x + 1 < MAXDUNX && tilePosition.y - 1 >= 0 && targetBufferPosition.x + TILE_WIDTH <= gnScreenWidth) {
					// Render objects behind walls first to prevent sprites, that are moving
					// between tiles, from poking through the walls as they exceed the tile bounds.
					// A proper fix for this would probably be to layout the sceen and render by
					// sprite screen position rather than tile position.
					if (IsWall(tilePosition) && (IsWall(tilePosition + Displacement { 1, 0 }) || (tilePosition.x > 0 && IsWall(tilePosition + Displacement { -1, 0 })))) { // Part of a wall aligned on the x-axis
						if (IsTileNotSolid(tilePosition + Displacement { 1, -1 }) && IsTileNotSolid(tilePosition + Displacement { 0, -1 })) {                              // Has walkable area behind it
							DrawDungeon(out, tilePosition + Direction::East, { targetBufferPosition.x + TILE_WIDTH, targetBufferPosition.y });
						}
					}
				}
				DrawDungeon(out, tilePosition, targetBufferPosition);
			}
			tilePosition += Direction::East;
			targetBufferPosition.x += TILE_WIDTH;
		}
		// Return to start of row
		tilePosition += Displacement(Direction::West) * columns;
		targetBufferPosition.x -= columns * TILE_WIDTH;

		// Jump to next row
		targetBufferPosition.y += TILE_HEIGHT / 2;
		if ((i & 1) != 0) {
			tilePosition.x++;
			columns--;
			targetBufferPosition.x += TILE_WIDTH / 2;
		} else {
			tilePosition.y++;
			columns++;
			targetBufferPosition.x -= TILE_WIDTH / 2;
		}
	}
}

namespace {

/**
 * @brief Oracool: how big a dimension rendered at native resolution needs to be to end up
 * `fullSize` pixels once scaled up by `zoomFactor` - shared by the render-buffer sizing in
 * DrawGame and the source-region sizing in ZoomScale, so both agree on exactly the same value.
 */
int ScaledDimension(int fullSize, float zoomFactor)
{
	if (zoomFactor <= 1.0f)
		return fullSize;
	return static_cast<int>(std::lround(fullSize / zoomFactor));
}

/** @brief Oracool: reused scratch row buffer for ZoomScale, grown on demand, never shrunk. */
std::vector<uint8_t> zoomScaleScratchRow;

} // namespace

/**
 * @brief Oracool: continuous generalization of the original exact-2x zoom - scales the rendered
 * top-left [0,0)-[srcWidth,srcHeight) region of `out` up to fill `out`, by an arbitrary factor
 * from 1.0 (no-op) up to 2.0, via nearest-neighbor sampling. Diablo's palette-indexed 8bpp pixels
 * can't be filtered/blended like RGB, so nearest-neighbor is the only correct method here - it
 * also keeps the same pixelated look at every zoom level, not just the old exact 2x.
 *
 * Processes destination rows bottom-to-top: since srcY(dstY) <= dstY always (the source region is
 * never taller than the destination), no row is read after it has already been overwritten.
 */
void ZoomScale(const Surface &out, float zoomFactor)
{
	int viewportWidth = out.w();
	int viewportOffsetX = 0;
	if (CanPanelsCoverView()) {
		if (IsLeftPanelOpen()) {
			viewportWidth -= SidePanelSize.width;
			viewportOffsetX = SidePanelSize.width;
		} else if (IsRightPanelOpen()) {
			viewportWidth -= SidePanelSize.width;
		}
	}

	const int dstHeight = out.h();
	const int srcWidth = std::clamp(ScaledDimension(viewportWidth, zoomFactor), 1, viewportWidth);
	const int srcHeight = std::clamp(ScaledDimension(dstHeight, zoomFactor), 1, dstHeight);

	const size_t rowBytes = static_cast<size_t>(viewportWidth);
	if (zoomScaleScratchRow.size() < rowBytes)
		zoomScaleScratchRow.resize(rowBytes);
	uint8_t *scaledRow = zoomScaleScratchRow.data();

	uint8_t *base = out.at(viewportOffsetX, 0);
	const int pitch = out.pitch();

	for (int dstY = dstHeight - 1; dstY >= 0; dstY--) {
		int srcY = (dstY * srcHeight) / dstHeight;
		if (srcY >= srcHeight)
			srcY = srcHeight - 1;
		const uint8_t *srcRow = base + static_cast<ptrdiff_t>(srcY) * pitch;

		for (int dstX = 0; dstX < viewportWidth; dstX++) {
			int srcX = (dstX * srcWidth) / viewportWidth;
			if (srcX >= srcWidth)
				srcX = srcWidth - 1;
			scaledRow[dstX] = srcRow[srcX];
		}

		memcpy(base + static_cast<ptrdiff_t>(dstY) * pitch, scaledRow, rowBytes);
	}
}

Displacement tileOffset;
Displacement tileShift;
int tileColums;
int tileRows;

void CalcFirstTilePosition(Point &position, Displacement &offset)
{
	// Adjust by player offset and tile grid alignment
	Player &myPlayer = *MyPlayer;
	offset = tileOffset;
	if (myPlayer.isWalking())
		offset += GetOffsetForWalking(myPlayer.AnimInfo, myPlayer._pdir, true);

	position += tileShift;

	// Skip rendering parts covered by the panels
	if (CanPanelsCoverView() && (IsLeftPanelOpen() || IsRightPanelOpen())) {
		// Oracool: generalized from the old binary zoom-on-off multiplier (1 or 2) to a continuous
		// factor - 2.0f/zoomFactor reproduces 1 at zoomFactor==2.0 and 2 at zoomFactor==1.0 exactly.
		const float zoomFactor = *sgOptions.Oracool.dungeonZoomLevel;
		const float multiplier = 2.0f / zoomFactor;
		position += Displacement(Direction::East) * multiplier;
		offset.deltaX += static_cast<int>(-TILE_WIDTH * multiplier / 2 / 2);

		if (IsLeftPanelOpen() && zoomFactor < 1.5f) {
			offset.deltaX += SidePanelSize.width;
			// SidePanelSize.width accounted for in ZoomScale()
		}
	}

	// Draw areas moving in and out of the screen
	if (myPlayer.isWalking()) {
		switch (myPlayer._pdir) {
		case Direction::North:
		case Direction::NorthEast:
			offset.deltaY -= TILE_HEIGHT;
			position += Direction::North;
			break;
		case Direction::SouthWest:
		case Direction::West:
			offset.deltaX -= TILE_WIDTH;
			position += Direction::West;
			break;
		case Direction::NorthWest:
			offset.deltaX -= TILE_WIDTH / 2;
			offset.deltaY -= TILE_HEIGHT / 2;
			position += Direction::NorthWest;
		default:
			break;
		}
	}
}

/**
 * @brief Configure render and process screen rows
 * @param fullOut Buffer to render to
 * @param position First tile of view in dPiece coordinate
 * @param offset Amount to offset the rendering in screen space
 */
void DrawGame(const Surface &fullOut, Point position, Displacement offset)
{
	const float zoomFactor = *sgOptions.Oracool.dungeonZoomLevel;

	// Limit rendering to the view area - Oracool: generalized from the old binary zoom's fixed
	// half-height render buffer to a continuous factor via the shared ScaledDimension helper,
	// which ZoomScale() below uses to derive the exact same source height.
	const Surface &out = fullOut.subregionY(0, ScaledDimension(gnViewportHeight, zoomFactor));

	int columns = tileColums;
	int rows = tileRows;

	// Skip rendering parts covered by the panels
	if (CanPanelsCoverView() && (IsLeftPanelOpen() || IsRightPanelOpen())) {
		// Oracool: generalized from the old binary 2-or-4 column trim - 4/zoomFactor reproduces
		// 4 at zoomFactor==1.0 and 2 at zoomFactor==2.0 exactly.
		columns -= static_cast<int>(std::lround(4.0f / zoomFactor));
	}

	UpdateMissilesRendererData();

	// Draw areas moving in and out of the screen
	if (MyPlayer->isWalking()) {
		switch (MyPlayer->_pdir) {
		case Direction::NoDirection:
			break;
		case Direction::North:
		case Direction::South:
			rows += 2;
			break;
		case Direction::NorthEast:
			columns++;
			rows += 2;
			break;
		case Direction::East:
		case Direction::West:
			columns++;
			break;
		case Direction::SouthEast:
		case Direction::SouthWest:
		case Direction::NorthWest:
			columns++;
			rows++;
			break;
		}
	}

#ifdef DUN_RENDER_STATS
	DunRenderStats.clear();
#endif

	DrawFloor(out, position, Point {} + offset, rows, columns);
	DrawTileContent(out, position, Point {} + offset, rows, columns);

	if (zoomFactor > 1.0f) {
		ZoomScale(fullOut.subregionY(0, gnViewportHeight), zoomFactor);
	}

#ifdef DUN_RENDER_STATS
	std::vector<std::pair<DunRenderType, size_t>> sortedStats(DunRenderStats.begin(), DunRenderStats.end());
	std::sort(sortedStats.begin(), sortedStats.end(),
	    [](const std::pair<DunRenderType, size_t> &a, const std::pair<DunRenderType, size_t> &b) {
		    return a.first.maskType == b.first.maskType
		        ? static_cast<uint8_t>(a.first.tileType) < static_cast<uint8_t>(b.first.tileType)
		        : static_cast<uint8_t>(a.first.maskType) < static_cast<uint8_t>(b.first.maskType);
	    });
	Point pos { 100, 20 };
	for (size_t i = 0; i < sortedStats.size(); ++i) {
		const auto &stat = sortedStats[i];
		DrawString(out, StrCat(i, "."), Rectangle(pos, Size { 20, 16 }), { UiFlags::AlignRight });
		DrawString(out, MaskTypeToString(stat.first.maskType), { pos.x + 24, pos.y });
		DrawString(out, TileTypeToString(stat.first.tileType), { pos.x + 184, pos.y });
		DrawString(out, FormatInteger(stat.second), Rectangle({ pos.x + 354, pos.y }, Size(40, 16)), { UiFlags::AlignRight });
		pos.y += 16;
	}
#endif
}

/**
 * @brief Start rendering of screen, town variation
 * @param out Buffer to render to
 * @param startPosition Center of view in dPiece coordinates
 */
void DrawView(const Surface &out, Point startPosition)
{
#ifdef _DEBUG
	DebugCoordsMap.clear();
#endif
	Displacement offset = {};
	CalcFirstTilePosition(startPosition, offset);
	DrawGame(out, startPosition, offset);
	// Oracool: user rule - a window hides the UI behind it until it closes. At 340x720 a right-hand
	// window is the full height of the screen in the top-right corner, which is exactly where the
	// mini-map (306x175 at x=646) and the event log below it live, so they would otherwise sit
	// under it with their edges poking out. Nothing is toggled or saved: they simply stop drawing
	// while a window is open, the same way AutomapActive already suppresses them.
	//
	// Asked of IsRightPanelOpen() rather than of invflag, so this covers the spell book too and any
	// later window that takes the right-hand slot - the rule is about the space being occupied, not
	// about which window occupies it. The left-hand windows do not need this: they sit at x 0..340
	// and the corner HUD is entirely to the right of them.
	const bool cornerHudHidden = IsRightPanelOpen();

	if (AutomapActive) {
		DrawAutomap(out.subregionY(0, gnViewportHeight));
	} else if (*sgOptions.Oracool.miniMapEnabled && !cornerHudHidden
#ifdef _DEBUG
	    && !DebugClearUi
#endif
	) {
		// Oracool: independent of AutomapActive/TAB - always on whenever this option is set and
		// the full map isn't open, not a toggled state. DrawMiniMap sets MiniMapActive itself for
		// the brief duration of this call, purely so DrawAutomapPlr (shared by both draw paths)
		// still knows which marker style to use.
		DrawMiniMap(out.subregionY(0, gnViewportHeight));
	}
	// Oracool: drawn here, alongside the mini-map, rather than at the end of this function with
	// the other Oracool overlays - these two are gameplay-view HUD elements anchored to the
	// mini-map, not dialogs, so they need the same "any panel drawn afterward covers them" behavior
	// the mini-map already has. Previously drawn last, they rendered on top of the inventory,
	// character, quest log, spellbook, and Stash panels instead of being covered by them.
	// Oracool: user request - hidden while the full-screen map is open, since they're anchored to
	// the mini-map (which AutomapActive already suppresses above) and would otherwise float over
	// the full map. No saved/restored state needed - they simply resume drawing the next frame
	// AutomapActive goes false again, same as the mini-map itself.
	if (!AutomapActive && !cornerHudHidden
#ifdef _DEBUG
	    && !DebugClearUi
#endif
	) {
		oracool::DrawEventLogWindow(out);
		oracool::DrawGameClock(out);
		oracool::DrawXpGainIndicator(out);
		oracool::DrawXpCounter(out);
	}
#ifdef _DEBUG
	bool debugGridTextNeeded = IsDebugGridTextNeeded();
	if (debugGridTextNeeded || DebugGrid) {
		// force redrawing or debug stuff stays on panel on 640x480 resolution
		RedrawEverything();
		char debugGridTextBuffer[10];
		bool megaTiles = IsDebugGridInMegatiles();
		const float zoomFactor = *sgOptions.Oracool.dungeonZoomLevel;

		for (auto m : DebugCoordsMap) {
			Point dunCoords = { m.first % MAXDUNX, m.first / MAXDUNX };
			if (megaTiles && (dunCoords.x % 2 == 1 || dunCoords.y % 2 == 1))
				continue;
			Point pixelCoords = m.second;
			if (megaTiles)
				pixelCoords += Displacement { 0, TILE_HEIGHT / 2 };
			pixelCoords *= zoomFactor;
			if (debugGridTextNeeded && GetDebugGridText(dunCoords, debugGridTextBuffer)) {
				Size tileSize = { TILE_WIDTH, TILE_HEIGHT };
				tileSize *= zoomFactor;
				DrawString(out, debugGridTextBuffer, { pixelCoords - Displacement { 0, tileSize.height }, tileSize }, { UiFlags::ColorRed | UiFlags::AlignCenter | UiFlags::VerticalCenter });
			}
			if (DebugGrid) {
				auto DrawDebugSquare = [&out](Point center, Displacement hor, Displacement ver, uint8_t col) {
					auto DrawLine = [&out](Point from, Point to, uint8_t col) {
						int dx = to.x - from.x;
						int dy = to.y - from.y;
						int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
						float ix = dx / (float)steps;
						float iy = dy / (float)steps;
						float sx = from.x;
						float sy = from.y;

						for (int i = 0; i <= steps; i++, sx += ix, sy += iy)
							out.SetPixel({ (int)sx, (int)sy }, col);
					};
					DrawLine(center - hor, center + ver, col);
					DrawLine(center + hor, center + ver, col);
					DrawLine(center - hor, center - ver, col);
					DrawLine(center + hor, center - ver, col);
				};

				Displacement hor = { TILE_WIDTH / 2, 0 };
				Displacement ver = { 0, TILE_HEIGHT / 2 };
				hor *= zoomFactor;
				ver *= zoomFactor;
				Point center = pixelCoords + hor - ver;

				if (megaTiles) {
					hor *= 2;
					ver *= 2;
				}

				uint8_t col = PAL16_BEIGE;

				DrawDebugSquare(center, hor, ver, col);
			}
		}
	}
#endif
#ifdef _DEBUG
	if (!DebugClearUi)
#endif
	{
		DrawItemNameLabels(out);
		DrawMonsterHealthBar(out);
		DrawFloatingNumbers(out, startPosition, offset);
	}
	// Oracool: drawn after the whole scene above so the outline lands on top of any wall that
	// would otherwise hide the monster's body; always called to drain the queue every frame -
	// deliberately NOT gated by DebugClearUi, unlike the other overlays above.
	DrawMonsterWallOutlines(out);

	if (stextflag != TalkID::None && !qtextflag)
		DrawSText(out);
	if (invflag) {
		DrawInv(out);
	} else if (sbookflag) {
		DrawSpellBook(out);
	}

#ifdef _DEBUG
	if (!DebugClearUi)
#endif
		DrawDurIcon(out);

	switch (GetLeftPanelContent()) {
	case LeftPanelContent::Character:
		DrawChr(out);
		break;
	case LeftPanelContent::QuestLog:
		DrawQuestLog(out);
		break;
	case LeftPanelContent::Stash:
		DrawStash(out);
		break;
	case LeftPanelContent::WaypointMenu:
		oracool::DrawWaypointMenu(out);
		break;
	case LeftPanelContent::Crafting:
		oracool::DrawCraftingMenu(out);
		break;
	case LeftPanelContent::None:
		break;
	}
	if (oracool::IsHudMenuOpen()) {
		oracool::DrawHudMenu(out);
	}
	DrawLevelUpIcon(out);
	if (qtextflag) {
		DrawQText(out);
	}
	if (spselflag) {
		DrawSpellList(out);
	}
	if (DropGoldFlag) {
		DrawGoldSplit(out);
	}
	DrawGoldWithdraw(out);
	DrawRefreshUntilPrompt(out);
	if (HelpFlag) {
		DrawHelp(out);
	}
	if (ChatLogFlag) {
		DrawChatLog(out);
	}
	if (IsDiabloMsgAvailable()) {
		DrawDiabloMsg(out);
	}
	oracool::DrawSaveIndicator(out);
	// Oracool: user request - the fixed "item stats" box that used to draw here is gone. Its
	// content now goes into the one cursor-following panel (oracool::DrawCursorTooltip, drawn near
	// DrawCursor further down), so there is no second place for item information to appear.
	if (MyPlayerIsDead) {
		RedBack(out);
	} else if (PauseMode != 0) {
		gmenu_draw_pause(out);
	}

	DrawControllerModifierHints(out);
	DrawPlrMsg(out);
	gmenu_draw(out);
	doom_draw(out);
	UpdateInfoString();
	DrawRefreshUntilHoverTooltip(out);
	control_update_life_mana(); // Update life/mana totals before rendering the orbs.

	// Oracool: user request - "mana orb to draw in front of sort and gold texts". So the footer goes
	// down FIRST and the orb over it, which is the opposite of the order this had until 1.5.23. It
	// still cannot live in DrawInv: that runs long before this point, under half the HUD. No-ops
	// unless the inventory is open.
	DrawInventoryFooter(out);

	// Oracool: HUD art pass - the corner orb compositions (with their sphere drain effect) replace
	// the vanilla flask pair entirely.
	oracool::DrawHealthOrb(out);
	oracool::DrawManaOrb(out);
}

/**
 * @brief Display the current average FPS over 1 sec
 */
void DrawFPS(const Surface &out)
{
	static int framesSinceLastUpdate = 0;
	static string_view formatted {};

	if (!frameflag || !gbActive) {
		return;
	}

	framesSinceLastUpdate++;
	uint32_t runtimeInMs = SDL_GetTicks();
	uint32_t msSinceLastUpdate = runtimeInMs - lastFpsUpdateInMs;
	if (msSinceLastUpdate >= 1000) {
		lastFpsUpdateInMs = runtimeInMs;
		constexpr int FpsPow10 = 10;
		const int fps = 1000 * FpsPow10 * framesSinceLastUpdate / msSinceLastUpdate;
		framesSinceLastUpdate = 0;

		static char buf[15] {};
		const char *end = fps >= 100 * FpsPow10
		    ? BufCopy(buf, fps / FpsPow10, " FPS")
		    : BufCopy(buf, fps / FpsPow10, ".", fps % FpsPow10, " FPS");
		formatted = { buf, static_cast<string_view::size_type>(end - buf) };
	};
	DrawString(out, formatted, Point { 8, 68 }, { UiFlags::ColorRed });
}

/**
 * @brief Update part of the screen from the back buffer
 * @param x Back buffer coordinate
 * @param y Back buffer coordinate
 * @param w Back buffer coordinate
 * @param h Back buffer coordinate
 */
void DoBlitScreen(int x, int y, int w, int h)
{
#ifdef DEBUG_DO_BLIT_SCREEN
	const Surface &out = GlobalBackBuffer();
	const uint8_t debugColor = PAL8_RED;
	DrawHorizontalLine(out, Point(x, y), w, debugColor);
	DrawHorizontalLine(out, Point(x, y + h - 1), w, debugColor);
	DrawVerticalLine(out, Point(x, y), h, debugColor);
	DrawVerticalLine(out, Point(x + w - 1, y), h, debugColor);
#endif
	SDL_Rect srcRect = MakeSdlRect(x, y, w, h);
	SDL_Rect dstRect = MakeSdlRect(x, y, w, h);
	BltFast(&srcRect, &dstRect);
}

/**
 * @brief Check render pipeline and update individual screen parts
 * @param out Output surface.
 * @param dwHgt Section of screen to update from top to bottom
 * @param drawDesc Render info box
 * @param drawHp Render health bar
 * @param drawMana Render mana bar
 * @param drawSbar Render belt
 */
void DrawMain(const Surface &out, int dwHgt, bool drawDesc, bool drawHp, bool drawMana, bool drawSbar)
{
	if (!gbActive || RenderDirectlyToOutputSurface) {
		return;
	}

	assert(dwHgt >= 0 && dwHgt <= gnScreenHeight);

	if (dwHgt > 0) {
		DoBlitScreen(0, 0, gnScreenWidth, dwHgt);
	}
	if (dwHgt < gnScreenHeight) {
		const Point mainPanelPosition = GetMainPanel().position;
		if (drawSbar) {
			// Oracool: HUD art pass - the plate rect covers the whole middle HUD including its XP
			// groove (single source of truth in hud_layout.cpp).
			const Rectangle middleHud = oracool::GetMiddleHudRect();
			DoBlitScreen(middleHud.position.x, middleHud.position.y, middleHud.size.width, middleHud.size.height);
		}
		if (drawDesc) {
			if (talkflag) {
				// When chat input is displayed, the belt is hidden and the chat moves up.
				DoBlitScreen(mainPanelPosition.x + 171, mainPanelPosition.y + 6, 298, 116);
			} else {
				// Oracool: HUD overhaul - the info box is now a cursor-following tooltip (see
				// oracool/cursor_tooltip.h) instead of a fixed panel-relative box, so the area that
				// needs erasing each frame is wherever it was last drawn, tracked the same way
				// PrevCursorRect tracks the cursor sprite just below.
				const Rectangle prevTooltipRect = oracool::GetPrevCursorTooltipRect();
				if (prevTooltipRect.size.width != 0 && prevTooltipRect.size.height != 0) {
					DoBlitScreen(prevTooltipRect.position.x, prevTooltipRect.position.y, prevTooltipRect.size.width, prevTooltipRect.size.height);
				}
			}
		}
		if (drawMana) {
			const Rectangle orbRect = oracool::GetManaOrbRect();
			DoBlitScreen(orbRect.position.x, orbRect.position.y, orbRect.size.width, orbRect.size.height);
		}
		if (drawHp) {
			const Rectangle orbRect = oracool::GetHealthOrbRect();
			DoBlitScreen(orbRect.position.x, orbRect.position.y, orbRect.size.width, orbRect.size.height);
		}
		if (PrevCursorRect.size.width != 0 && PrevCursorRect.size.height != 0) {
			DoBlitScreen(PrevCursorRect.position.x, PrevCursorRect.position.y, PrevCursorRect.size.width, PrevCursorRect.size.height);
		}
		Rectangle &cursorRect = GetDrawnCursor().rect;
		if (cursorRect.size.width != 0 && cursorRect.size.height != 0) {
			DoBlitScreen(cursorRect.position.x, cursorRect.position.y, cursorRect.size.width, cursorRect.size.height);
		}
	}
}

} // namespace

Displacement GetOffsetForWalking(const AnimationInfo &animationInfo, const Direction dir, bool cameraMode /*= false*/)
{
	// clang-format off
	//                                           South,        SouthWest,    West,         NorthWest,    North,        NorthEast,     East,         SouthEast,
	constexpr Displacement StartOffset[8]    = { {   0, -32 }, {  32, -16 }, {  64,   0 }, {   0,   0 }, {   0,   0 }, {  0,    0 },  { -64,   0 }, { -32, -16 } };
	constexpr Displacement MovingOffset[8]   = { {   0,  32 }, { -32,  16 }, { -64,   0 }, { -32, -16 }, {   0, -32 }, {  32, -16 },  {  64,   0 }, {  32,  16 } };
	// clang-format on

	uint8_t animationProgress = animationInfo.getAnimationProgress();
	Displacement offset = MovingOffset[static_cast<size_t>(dir)];
	offset *= animationProgress;
	offset /= AnimationInfo::baseValueFraction;

	if (cameraMode) {
		offset = -offset;
	} else {
		offset += StartOffset[static_cast<size_t>(dir)];
	}

	return offset;
}

void ClearCursor() // CODE_FIX: this was supposed to be in cursor.cpp
{
	PrevCursorRect = {};
}

void ShiftGrid(int *x, int *y, int horizontal, int vertical)
{
	*x += vertical + horizontal;
	*y += vertical - horizontal;
}

int RowsCoveredByPanel()
{
	auto &mainPanelSize = GetMainPanel().size;
	if (GetScreenWidth() <= mainPanelSize.width) {
		return 0;
	}

	int rows = mainPanelSize.height / TILE_HEIGHT;
	// Oracool: generalized from the old binary /2 to a continuous factor - exactly reproduces the
	// original truncating integer division at zoomFactor==1.0 and ==2.0.
	rows = static_cast<int>(rows / *sgOptions.Oracool.dungeonZoomLevel);

	return rows;
}

void CalcTileOffset(int *offsetX, int *offsetY)
{
	uint16_t screenWidth = GetScreenWidth();
	uint16_t viewportHeight = GetViewportHeight();
	const float zoomFactor = *sgOptions.Oracool.dungeonZoomLevel;

	// Oracool: generalized from the old binary "divide by 2 or don't" to a continuous factor -
	// dividing by 1.0f reproduces the un-zoomed branch exactly, and by 2.0f the zoomed one.
	int x = static_cast<int>(screenWidth / zoomFactor) % TILE_WIDTH;
	int y = static_cast<int>(viewportHeight / zoomFactor) % TILE_HEIGHT;

	if (x != 0)
		x = (TILE_WIDTH - x) / 2;
	if (y != 0)
		y = (TILE_HEIGHT - y) / 2;

	*offsetX = x;
	*offsetY = y;
}

void TilesInView(int *rcolumns, int *rrows)
{
	uint16_t screenWidth = GetScreenWidth();
	uint16_t viewportHeight = GetViewportHeight();

	int columns = screenWidth / TILE_WIDTH;
	if ((screenWidth % TILE_WIDTH) != 0) {
		columns++;
	}
	int rows = viewportHeight / TILE_HEIGHT;
	if ((viewportHeight % TILE_HEIGHT) != 0) {
		rows++;
	}

	// Oracool: generalized from "round up to even, then halve" (the old binary zoom) to
	// ceil(nativeTiles / zoomFactor) - mathematically identical to the original at zoomFactor==2.0
	// (rounding a count up to even then halving it always equals its ceiling-divide-by-2), and a
	// safe (never-under-render) generalization for any factor in between.
	const float zoomFactor = *sgOptions.Oracool.dungeonZoomLevel;
	if (zoomFactor > 1.0f) {
		columns = static_cast<int>(std::ceil(columns / zoomFactor));
		rows = static_cast<int>(std::ceil(rows / zoomFactor));
	}

	*rcolumns = columns;
	*rrows = rows;
}

void CalcViewportGeometry()
{
	const float zoomFactor = *sgOptions.Oracool.dungeonZoomLevel;
	const int screenWidth = static_cast<int>(GetScreenWidth() / zoomFactor);
	const int screenHeight = static_cast<int>(GetScreenHeight() / zoomFactor);
	const int panelHeight = static_cast<int>(GetMainPanel().size.height / zoomFactor);
	const int pixelsToPanel = screenHeight - panelHeight;
	Point playerPosition { screenWidth / 2, pixelsToPanel / 2 };

	// Oracool: generalized from a fixed TILE_HEIGHT/4 fudge (only applied when the old binary zoom
	// was fully on) to a linear interpolation that is exactly 0 at zoomFactor==1.0 and exactly
	// TILE_HEIGHT/4 at zoomFactor==2.0, matching the original at both endpoints.
	playerPosition.y += static_cast<int>((zoomFactor - 1.0f) * (TILE_HEIGHT / 4.0f));

	const int tilesToTop = (playerPosition.y + TILE_HEIGHT - 1) / TILE_HEIGHT;
	const int tilesToLeft = (playerPosition.x + TILE_WIDTH - 1) / TILE_WIDTH;

	// Location of the center of the tile from which to start rendering, relative to the viewport origin
	Point startPosition = playerPosition - Displacement { tilesToLeft * TILE_WIDTH, tilesToTop * TILE_HEIGHT };

	// Position of the tile from which to start rendering in tile space,
	// relative to the tile the player character occupies
	tileShift = { 0, 0 };
	tileShift += Displacement(Direction::North) * tilesToTop;
	tileShift += Displacement(Direction::West) * tilesToLeft;

	// The rendering loop expects to start on a row with fewer columns
	if (tilesToLeft * TILE_WIDTH >= playerPosition.x) {
		startPosition += Displacement { TILE_WIDTH / 2, -TILE_HEIGHT / 2 };
		tileShift += Displacement(Direction::NorthEast);
	} else if (tilesToTop * TILE_HEIGHT < playerPosition.y) {
		// There is one row above the current row that needs to be rendered,
		// but we skip to the row above it because it has too many columns
		startPosition += Displacement { 0, -TILE_HEIGHT };
		tileShift += Displacement(Direction::North);
	}

	// Location of the bottom-left corner of the bounding box around the
	// tile from which to start rendering, relative to the viewport origin
	tileOffset = { startPosition.x - TILE_WIDTH / 2, startPosition.y + TILE_HEIGHT / 2 - 1 };

	// Compute the number of rows to be rendered as well as
	// the number of columns to be rendered in the first row
	const int viewportHeight = static_cast<int>(GetViewportHeight() / zoomFactor);
	const Point renderStart = startPosition - Displacement { TILE_WIDTH / 2, TILE_HEIGHT / 2 };
	tileRows = (viewportHeight - renderStart.y + TILE_HEIGHT / 2 - 1) / (TILE_HEIGHT / 2);
	tileColums = (screenWidth - renderStart.x + TILE_WIDTH - 1) / TILE_WIDTH;
}

void AdjustDungeonZoom(int steps)
{
	sgOptions.Oracool.dungeonZoomLevel.SetValue(sgOptions.Oracool.dungeonZoomLevel.ValueTenths() + steps);
	CalcViewportGeometry();
}

void ToggleDungeonZoom()
{
	const bool closerToZoomedIn = sgOptions.Oracool.dungeonZoomLevel.ValueTenths() >= 15;
	sgOptions.Oracool.dungeonZoomLevel.SetValue(closerToZoomedIn ? 10 : 20);
	CalcViewportGeometry();
}

extern SDL_Surface *PalSurface;

void ClearScreenBuffer()
{
	if (HeadlessMode)
		return;

	assert(PalSurface != nullptr);
	SDL_FillRect(PalSurface, nullptr, 0);
}

#ifdef _DEBUG
void ScrollView()
{
	if (!MyPlayer->HoldItem.isEmpty())
		return;

	if (MousePosition.x < 20) {
		if (dmaxPosition.y - 1 <= ViewPosition.y || dminPosition.x >= ViewPosition.x) {
			if (dmaxPosition.y - 1 > ViewPosition.y) {
				ViewPosition.y++;
			}
			if (dminPosition.x < ViewPosition.x) {
				ViewPosition.x--;
			}
		} else {
			ViewPosition.y++;
			ViewPosition.x--;
		}
	}
	if (MousePosition.x > gnScreenWidth - 20) {
		if (dmaxPosition.x - 1 <= ViewPosition.x || dminPosition.y >= ViewPosition.y) {
			if (dmaxPosition.x - 1 > ViewPosition.x) {
				ViewPosition.x++;
			}
			if (dminPosition.y < ViewPosition.y) {
				ViewPosition.y--;
			}
		} else {
			ViewPosition.y--;
			ViewPosition.x++;
		}
	}
	if (MousePosition.y < 20) {
		if (dminPosition.y >= ViewPosition.y || dminPosition.x >= ViewPosition.x) {
			if (dminPosition.y < ViewPosition.y) {
				ViewPosition.y--;
			}
			if (dminPosition.x < ViewPosition.x) {
				ViewPosition.x--;
			}
		} else {
			ViewPosition.x--;
			ViewPosition.y--;
		}
	}
	if (MousePosition.y > gnScreenHeight - 20) {
		if (dmaxPosition.y - 1 <= ViewPosition.y || dmaxPosition.x - 1 <= ViewPosition.x) {
			if (dmaxPosition.y - 1 > ViewPosition.y) {
				ViewPosition.y++;
			}
			if (dmaxPosition.x - 1 > ViewPosition.x) {
				ViewPosition.x++;
			}
		} else {
			ViewPosition.x++;
			ViewPosition.y++;
		}
	}
}
#endif

void EnableFrameCount()
{
	frameflag = true;
	lastFpsUpdateInMs = SDL_GetTicks();
}

void scrollrt_draw_game_screen()
{
	if (HeadlessMode)
		return;

	int hgt = 0;

	if (IsRedrawEverything()) {
		RedrawComplete();
		hgt = gnScreenHeight;
	}

	const Surface &out = GlobalBackBuffer();
	UndrawCursor(out);
	DrawMain(out, hgt, false, false, false, false);
#ifdef _DEBUG
	if (!DebugClearUi)
#endif
		DrawCursor(out);

	RenderPresent();
}

void DrawAndBlit()
{
	if (!gbRunGame || HeadlessMode) {
		return;
	}

	int hgt = 0;
	bool drawHealth = IsRedrawComponent(PanelDrawComponent::Health);
	bool drawMana = IsRedrawComponent(PanelDrawComponent::Mana);
	bool drawBelt = IsRedrawComponent(PanelDrawComponent::Belt);
	bool drawChatInput = talkflag;
	bool drawInfoBox = false;

	const Rectangle &mainPanel = GetMainPanel();

	if (gnScreenWidth > mainPanel.size.width || IsRedrawEverything()) {
		drawHealth = true;
		drawMana = true;
		drawBelt = true;
		drawInfoBox = false;
		hgt = gnScreenHeight;
	} else if (IsRedrawViewport()) {
		drawInfoBox = true;
		hgt = gnViewportHeight;
	}

	const Surface &out = GlobalBackBuffer();
	UndrawCursor(out);

	nthread_UpdateProgressToNextGameTick();

	DrawView(out, ViewPosition);
	// Oracool: user request - "hideui" debug command. Skips every always-on HUD element (main
	// panel, orbs, spell icon, control buttons, belt, chat input, XP bar, flask value text) for a
	// clean screenshot; the dungeon view above and the cursor below are unaffected. Debug-build
	// only, like the rest of debug.cpp/debug.h (the whole file is wrapped in #ifdef _DEBUG) - the
	// Release branch below is simply the original, always-on drawing code, unchanged. "clearui"
	// (DebugClearUi) also hides this panel, on top of everything else it hides.
#ifdef _DEBUG
	if (!DebugHideUi && !DebugClearUi)
#endif
	{
		// Oracool: HUD art pass - the middle HUD plate (assets/ui/middle_hud.png) is the background
		// everything else on the row draws onto, so it goes first: before the RMB spell icon
		// (drawMana branch) and the belt items. Hidden while chat input covers the same area,
		// matching DrawInvBelt's own talkflag gate. The corner orbs draw in DrawView's always-on
		// tail (see DrawHealthOrb/DrawManaOrb there); only their value text renders here.
		if (drawBelt && !talkflag) {
			oracool::DrawMiddleHudArt(out);
			// Oracool: the LMB well's content. Part of the plate rather than of the readied-spell
			// state, so it draws here rather than under drawMana with DrawSpell - left click always
			// attacks, whatever the RMB well happens to be holding.
			oracool::DrawLmbSkillWell(out);
		}
		if (drawMana) {
			DrawSpell(out);
		}
		if (drawBelt) {
			DrawInvBelt(out);
			// Oracool: click feedback for the Menu/Portal cells, whose frames and icons are baked
			// into the plate art and so have no state of their own to react with.
			oracool::DrawBeltButtonFeedback(out);
		}
		if (drawChatInput) {
			DrawTalkPan(out);
		}
		DrawXPBar(out);
		if (*sgOptions.Gameplay.showHealthValues) {
			const Rectangle orbRect = oracool::GetHealthOrbRect();
			DrawFlaskValues(out, orbRect.position + Displacement { oracool::GetHealthOrbSphereCenterLocal().x, oracool::GetHealthOrbSphereCenterLocal().y }, MyPlayer->_pHitPoints >> 6, MyPlayer->_pMaxHP >> 6);
		}
		if (*sgOptions.Gameplay.showManaValues) {
			const Rectangle orbRect = oracool::GetManaOrbRect();
			DrawFlaskValues(out, orbRect.position + Displacement { oracool::GetManaOrbSphereCenterLocal().x, oracool::GetManaOrbSphereCenterLocal().y }, MyPlayer->_pMana >> 6, MyPlayer->_pMaxMana >> 6);
		}
	}

#ifdef _DEBUG
	if (!DebugClearUi) {
#endif
		// Above the HUD, deliberately. The Abilities window is drawn far earlier in this function, so
		// its hover panel has to be deferred to here or the belt and orbs paint over it.
		DrawAbilityHoverPanel(out);
		oracool::DrawCursorTooltip(out);
		DrawCursor(out);
		DrawFPS(out);
#ifdef _DEBUG
	}
#endif

	DrawMain(out, hgt, drawInfoBox, drawHealth, drawMana, drawBelt);

	RedrawComplete();
	for (PanelDrawComponent component : enum_values<PanelDrawComponent>()) {
		if (IsRedrawComponent(component)) {
			RedrawComponentComplete(component);
		}
	}

	RenderPresent();
}

} // namespace devilution
