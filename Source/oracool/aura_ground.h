/**
 * @file oracool/aura_ground.h
 *
 * Oracool: the ring of light a lit aura casts on the floor.
 *
 * In Diablo II a Paladin with an aura burning stands inside a glowing ellipse. It is how the player
 * knows the aura is on, and which one. This fork had twenty auras and no such effect at all - an
 * active aura was visible only as a lit icon in the interface.
 *
 * The art is thirty 512x256 RGBA images (`ui\aura_*.png`), one per aura, commissioned to the brief
 * in `01-Project-Overview/Asset Brief - Paladin Aura Ground Effects.md` - the Paladin's twenty, then
 * the Bard's nine songs and the Monk's Healing Mantra (batch 8, 2026-09-11). They arrive already
 * quantised to the shared palette - measured error zero against every entry in 128-255 - which is
 * what lets them go into the world at all.
 *
 * ## Why this is not just a blit
 *
 * Two problems the ordinary HUD art path cannot solve.
 *
 * **Alpha.** The render target is 8-bit indexed. `oracool/hud_art.cpp` treats alpha as a yes/no at
 * 128, which is right for an interface plate and wrong for a glow: it would turn a soft gradient
 * into a hard-edged blob wherever it crossed the threshold, and delete everything softer. What the
 * engine actually has is `paletteTransparencyLookup`, a fixed FIFTY per cent blend of two indices.
 * Applying it once gives 50%, twice 75%, and nothing in between - so the in-between is dithered,
 * with a 4x4 ordered matrix choosing per pixel which of the two neighbouring levels to use. At the
 * distances these are seen from that reads as a smooth ramp.
 *
 * **Depth.** A ground decal spanning sixteen tiles cannot be drawn by the per-tile renderer, which
 * interleaves entities with terrain for depth sorting. It is drawn between the two passes instead -
 * after every floor tile, before any wall, monster or player - so everything in the world stands on
 * top of it, which is what "on the floor" means.
 *
 * ## Why it walks the tiles instead of computing a position
 *
 * DrawAuraGround takes the same four arguments as DrawFloor and repeats its walk exactly, drawing
 * when it reaches the player's tile. Deriving the screen position independently would mean a second
 * copy of the isometric mapping, and this project has already been bitten twice by two copies of
 * one bound drifting apart. Same walk, same answer, by construction.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"
#include "oracool/class_tree.h"
#include "utils/attributes.h"

namespace devilution::oracool {

/**
 * @brief The file id of @p aura's ground ring (`ui\aura_<id>.png`), or nullptr if the table has none.
 *
 * Exposed for the audit that every Kind::Aura row has a ring - RfA-12 added 27 auras and songs that
 * burned with no ring, and nothing noticed, because a missing ring is only a verbose log line.
 */
const char *AuraRingFileId(ClassTreeSkill aura);

/**
 * @brief Draws the lit aura's ground ring, if there is one.
 *
 * Call between DrawFloor and DrawTileContent with the same arguments both receive. Does nothing
 * when no aura is lit, when the art is missing, or when the player is not on the drawn level.
 */
void DrawAuraGround(const Surface &out, Point tilePosition, Point targetBufferPosition,
    int rows, int columns);

/**
 * @brief Frenzy of the Dead's ring under every frenzied minion (the Necromancer Skill Cards page, 2026-10-03): Holy Fire's
 * ring in orange, colour-cycled like the auras', at FrenzyRingPercent of the Paladin's ring. Same arguments and walk as
 * DrawAuraGround, and called beside it, so everything stands on it. Nothing in town or while the art is missing.
 */
void DrawMinionFrenzyRings(const Surface &out, Point tilePosition, Point targetBufferPosition, int rows, int columns);

/**
 * @brief The rings' glow (user, 2026-10-12: "can we now add glowing effect on aura rings? can they light the floor with their
 * asset color while colorcycling?"): every ring the 32-bit path draws is noted; DrawAuraGlow then adds, as light, the ring's
 * own colours and a glow of its colour on the floor under the whole circle - steady, not in the cycle's bands, and alive like
 * a candle's flame (user, 2026-10-12). Behind the Aura Glow option.
 */
void BeginAuraGlowFrame();
/** @brief The glow of every ring noted this frame, onto @p out (the world region) or the graphics card's splats. */
void DrawAuraGlow(const Surface &out, bool ringItself = true);
/**
 * @brief Keeps the floor under this frame's noted rings (user, 2026-10-12: "lights under auras need to render under houses in
 * town"). Call once the floor pass is done - the rings and the figures' shadows on it - before DrawTileContent.
 */
void KeepAuraGlowFloor(const Surface &out);
/**
 * @brief After DrawTileContent, before anything else is laid over the world: every kept pixel that changed is covered - a
 * roof, a wall, a figure - and DrawAuraGlow lights none of them. Without a keep this frame nothing is covered.
 */
void NoteAuraGlowCover(const Surface &out);
/** @brief Whether the glow is kept off @p bufferPixel (in the whole buffer's pixels) this frame. */
DVL_API_FOR_TEST bool AuraGlowCoveredAt(Point bufferPixel);
/** @brief Test hook: the rings noted this frame. */
size_t AuraGlowRingCount();

/** @brief The frenzy ring's size against the Paladin's aura ring, in percent ("Small scale - 50% of paladin aura rings"). */
constexpr int FrenzyRingPercent = 50;

/**
 * @brief Rain of Arrows' impact rings (user, 2026-10-03: "add a little diameter - 0,5-1 tile of aura ring, you pick color,
 * under each arrow of rain of arrows when an arrow impacts on ground"): Blessed Aim's ring in hue::ArrowAmber under each
 * landed arrow (missiles.h FallingArrowLandedTicks), opening from ArrowRingStartPercent to ArrowRingEndPercent of a tile
 * across over FallingArrowRingTicks as it fades out. Same arguments and walk as DrawAuraGround.
 */
void DrawArrowImpactRings(const Surface &out, Point tilePosition, Point targetBufferPosition, int rows, int columns);
/** The impact ring's width at its start and end, in percent of a tile (64 px). */
constexpr int ArrowRingStartPercent = 50;
constexpr int ArrowRingEndPercent = 100;
/** @brief Tests only: the impact ring's size in pixels @p ticks after the arrow landed. */
DVL_API_FOR_TEST Size ArrowRingSize(int ticks);
/**
 * @brief Fend's ring (the Rogue Skill Cards page, 2026-10-04: "every swing produces expanding aura ring with 1 tile radius which
 * after swing end disappears. aura ring to be orange-ish."): Might's ring in hue::FendOrange under the Rogue while a Fend swing
 * plays, opening with the swing to two tiles across. Same arguments and walk as DrawAuraGround.
 */
void DrawFendRing(const Surface &out, Point tilePosition, Point targetBufferPosition, int rows, int columns);
/** @brief Fend's ring's size at @p progress (0..1) through the swing: nothing to 128x64. */
DVL_API_FOR_TEST Size FendRingSize(double progress);
/** @brief Tests only: Fend's ring at @p progress centred on @p centre. */
DVL_API_FOR_TEST bool DrawFendRingPreview(const Surface &out, Point centre, double progress);
/** @brief Tests only: the impact ring @p ticks after landing, centred on @p centre, as DrawArrowImpactRings draws it. */
DVL_API_FOR_TEST bool DrawArrowRingPreview(const Surface &out, Point centre, int ticks, uint32_t ringHue = 0);
/** @brief The frenzy ring as drawn, in pixels. */
DVL_API_FOR_TEST Size FrenzyRingSize();
/** @brief Tests only: the frenzy ring centred on @p centre, at full pulse. False while the art is missing. */
DVL_API_FOR_TEST bool DrawFrenzyRingPreview(const Surface &out, Point centre);

/** @brief Tests only: the moment the rings' colour cycle is drawn at, in ms; -1 (the default) is the live clock. */
extern DVL_API_FOR_TEST int AuraRingClockOverrideMs;

/** @brief Tests only: @p aura's ring @p diameterHalfTiles half-tiles across, centred on @p centre, at full pulse. */
bool DrawAuraRingPreview(const Surface &out, ClassTreeSkill aura, Point centre, int diameterHalfTiles);

} // namespace devilution::oracool
