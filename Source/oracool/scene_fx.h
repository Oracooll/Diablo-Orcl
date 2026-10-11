/**
 * @file oracool/scene_fx.h
 *
 * The third graphics batch (user, 2026-10-11: "i like all of your suggestions. you can start implementing all of them"),
 * each behind its own option, all purely visual - their own clock (real time, standing still while the game is paused),
 * their own hashes, no game state touched:
 *
 *  - Figure shadows: every figure in a lit place casts a soft silhouette away from up to three lights near it (the
 *    torches, spells, lamps; the sun in Tristram by day), longer and fainter the further the light. Beside the shadow
 *    painted into the sprite, never instead of it: the new one fades out where that one lies, under the feet.
 *  - Exploration memory (off by default): explored floor that is dark now stays faintly lit (colour_light.h's planes and
 *    the floor's light table ask ExplorationMemoryLight).
 *  - Weather in Tristram: rain or snow now and then, the ground wet-dark or white under it, distant lightning on rainy
 *    nights.
 *  - Ground fog in the Caves and the Crypt: drifting mist laid over the world before the light passes, so it shows
 *    where there is light and vanishes in the dark.
 *  - Water reflections: a figure by water mirrored in it, faint and rippling, on the water's own tiles only.
 *  - Living town: Tristram's trees sway (the river's flow is post_fx's water light, which since this batch reads the
 *    town's river too).
 *  - Afterimages: Charge, Leap, Teleport and Whirlwind leave fading ghosts of the hero.
 *  - Dissolve: a rift guardian burns away into embers as he dies; what he conjures fades in.
 *  - Status visuals: frost sparkles on the frozen, flames on the burning, green drips on the poisoned, motes on the chilled.
 *  - The low health warning (user, 2026-10-11: "reddish pulsating screen border bleeding inward making it clear hero is
 *    approaching 0 health"): under a third of his life, a red glow creeps in from the screen's edges with each heartbeat.
 *  - Debanding (colour_light.cpp: the smooth light's multiply dithered before it rounds) and the CRT look (scanlines, a phosphor mask, corners; off by default).
 *  - The rift guardian's entrance (user, 2026-10-11: "screen shake when rift boss appears ... a bit further away from
 *    hero with some sort of large-scale light beam ... fade-in or erecting from the ground"): a column of light where he
 *    rises, the ground shaking, the guardian rising out of the floor as he fades in, black bars and his name.
 *
 * Nothing here draws on an 8-bit surface. Two deliberate game changes ride with the entrance and the dissolve, at the
 * user's word: the guardian rises seven tiles or more away and stands still while he rises (monster.cpp), and a guardian
 * who burns away leaves no corpse (MonsterDeath asks DissolvesOnDeath) - so a Necromancer cannot raise him.
 */
#pragma once

#include <cstdint>
#include <optional>

#include "engine/clx_sprite.hpp"
#include "engine/displacement.hpp"
#include "engine/point.hpp"

namespace devilution {
struct Surface;
struct Monster;
struct Player;
} // namespace devilution

namespace devilution::oracool {

// ---- Figures -------------------------------------------------------------------------------------------------------

/**
 * @brief The figures' ground effects - their shadows, and their reflections by water - laid on the floor in one pass
 * BEFORE the world is kept for the relief and Lit Sprites (audit round 1: drawn later, the relief took them for walls):
 * Begin, then one Add per figure the world draw will show, then End, which blurs the shadow layer once and lays it on.
 * @p firstTile and @p firstTileBuffer are the world draw's; for a figure, @p tile its tile, @p offset its walk offset (the
 * tile box's bottom-left is projected from the first tile's), @p sprite and @p spriteBottomLeft as it is drawn.
 */
void BeginFigureGround(const Surface &out, Point firstTile, Point firstTileBuffer);
void AddFigureGround(const Surface &out, Point tile, Displacement offset, ClxSprite sprite, Point spriteBottomLeft, int light, bool isHero);
void EndFigureGround(const Surface &out);
/** @brief Whether @p monster is rising out of the floor now (his entrance): no shadow while he does. */
bool MonsterRising(const Monster &monster);

/** @brief The hero's afterimages behind him; then notes this frame for the next ones. MyPlayer only. */
void DrawPlayerAfterimages(const Surface &out, const Player &player, Point tile, Point tileBuffer, Displacement offset, ClxSprite sprite);

/**
 * @brief How a monster's body is drawn this frame: as it is, rising out of the ground (the guardian's entrance, a
 * conjured one fading in) or burning away (a dying guardian).
 */
struct MonsterVeil {
	bool active = false;
	int sink = 0;         // pixels the body is drawn below where it stands, the part under the floor cut away
	int clipBottom = 0;   // the floor line in the surface: rows from here down are not drawn while it sinks
	float alpha = 1.0F;   // how much of the drawn body shows
	float dissolve = 0.0F; // 0..1: how much of it has burnt away
};

MonsterVeil PlanMonsterVeil(const Monster &monster, Point spriteBottomLeft, ClxSprite sprite);
/** @brief Before the body is drawn: keeps what lies under it. */
void BeginMonsterVeil(const Surface &out, const MonsterVeil &veil, Point spriteBottomLeft, ClxSprite sprite);
/** @brief After: mixes the body into what lay under it by the veil's alpha, or burns it away by its dissolve. */
void EndMonsterVeil(const Surface &out, const MonsterVeil &veil);

/** @brief Frost, flames, drips and motes on a monster under a status, over its body. */
void DrawMonsterStatus(const Surface &out, const Monster &monster, Point spriteBottomLeft, ClxSprite sprite);

/** @brief A guardian that burns away leaves no body (MonsterDeath asks). */
bool DissolvesOnDeath(const Monster &monster);

// ---- World passes ---------------------------------------------------------------------------------------------------

/** @brief The rain-dark or snow-white ground and the rain's splashes; on the lit floor, before the floor marks. Tristram. */
void ApplyWeatherGround(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief The ground fog over the floor (the Caves, the Crypt); on the floor, before the world is kept. */
void ApplyGroundFog(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief The guardian's column of light; beside the loot beams (splats when the card is collecting them). */
void DrawGuardianBeam(const Surface &out, Point firstTile, Point firstTileBuffer);

/**
 * @brief A tree's micro pair about to be drawn (scrollrt's DrawCell) - its box kept - and, once drawn, which of the box's
 * pixels it drew: only those sway, and only while nothing drawn later covers them (round 2 audit). @p baseY is the
 * piece's floor line.
 */
void BeginFoliageMicro(const Surface &out, int x, int y, int w, int h);
void EndFoliageMicro(const Surface &out, int baseY);
/** @brief Sways the trees noted this frame; after the figures, before the light passes. */
void ApplyTownSway(const Surface &out, Point firstTile, Point firstTileBuffer);

// ---- After the world --------------------------------------------------------------------------------------------------

/** @brief Advances the effects' clock: real time, standing still while the game is paused. Once a frame, before the world. */
void AdvanceSceneClock();
/** @brief The view's shake this frame (the guardian's entrance), in pixels. */
Displacement ScreenShakeOffset();
/**
 * @brief Over the finished world, before the HUD: the shake (the picture moved, its edges repeated so nothing undrawn
 * shows), the falling rain or snow (dimmed by the town's hour), lightning's flash and the low health warning.
 */
void ApplyAfterWorld(const Surface &world);
/** @brief The low health warning's heartbeat now (0..1) and how near death he is (0 none .. 1); the life orb beats with it. */
float LowHealthHeartbeat(float *severity);
/** @brief The effects' clock in seconds (standing still while paused): the living orbs move by it. */
float SceneSeconds();
/** @brief The entrance's black bars and the guardian's name, over the world and the HUD. */
void DrawGuardianEntranceCard(const Surface &out);
/** @brief The CRT look over the whole finished frame - only when @p wholeFrameDrawn (every pixel of it was drawn anew). */
void ApplyCrtLook(const Surface &out, bool wholeFrameDrawn);

// ---- The guardian's entrance ------------------------------------------------------------------------------------------

/** @brief The guardian has just risen (rift.cpp's ProcessRift): his entrance begins. */
void BeginGuardianEntrance(const Monster &guardian);
/** @brief Whether an entrance is under way. */
bool GuardianEntranceActive();

// ---- Weather --------------------------------------------------------------------------------------------------------

enum class Weather : uint8_t {
	Clear,
	Rain,
	Snow,
};
/** @brief Tristram's weather now, and how far it has set in (0..1). */
Weather CurrentWeather(float *intensity = nullptr);
/** @brief Sets Tristram's weather at once (the `weather` debug command). */
void SetWeather(Weather weather);

/** @brief Exploration memory: the light an explored tile keeps when it is dark (LightsMax when the option is off). */
int ExplorationMemoryLight(Point tile, int light);

/** @brief Clears every per-level record (ghosts, entrances, foliage, fades); LoadGameLevel calls it. */
void ForgetSceneFx();

} // namespace devilution::oracool
