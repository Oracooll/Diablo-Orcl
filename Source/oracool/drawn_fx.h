/**
 * @file oracool/drawn_fx.h
 *
 * Skill effects drawn by code instead of painted sheets - experiments the user judges from a render before any is wired
 * into the game (user, 2026-10-12: "can you suggest a game-drawn Ground Stomp animation for Barb?").
 */
#pragma once

#include "engine/displacement.hpp"
#include "engine/point.hpp"

namespace devilution {
struct Surface;
} // namespace devilution

namespace devilution::oracool {

/** @brief How long the drawn Ground Stomp lasts, in seconds. */
constexpr float GroundStompSeconds = 1.4F;

/**
 * @brief The drawn Ground Stomp at @p age seconds after the foot came down at @p foot (the floor point under the hero).
 * @p floorLayer: the parts on the floor (cracks, the shockwave's light and dust, the air's ripple, the flash's pool),
 * drawn before the hero; otherwise the parts in the air (stone chips, dust clouds, the flash's core), drawn after him.
 */
void DrawGroundStompFx(const Surface &out, Point foot, float age, bool floorLayer);

/** @brief How long the drawn Seismic Slam lasts, in seconds; its wave reaches the end of its path at SeismicTravelSeconds. */
constexpr float SeismicSlamSeconds = 1.8F;
constexpr float SeismicTravelSeconds = 0.6F;

/**
 * @brief The drawn Seismic Slam (user, 2026-10-12: "invent a good looking game drawn seismic slam effect and wave"): a wave of
 * earth rolling @p tiles tiles from @p foot along the tile direction (@p dirX, @p dirY) - a rupture tearing open behind a
 * ridge of heaving stone, chips and dust thrown up all along it, a burst at its end - at @p age seconds after the slam.
 * @p floorLayer as DrawGroundStompFx.
 */
void DrawSeismicSlamFx(const Surface &out, Point foot, int dirX, int dirY, int tiles, float age, bool floorLayer);

/** @brief The drawn Earthquake: four pulses a second apart, then a second for the fissures to cool. */
constexpr int EarthquakePulses = 4;
constexpr float EarthquakeSeconds = 5.0F;

/**
 * @brief The drawn Earthquake (user, 2026-10-12: "invent a proper earthquake animation"), round @p centre at @p age seconds:
 * on each pulse a shockwave to the 3-tile reach, the air rippling, stones jumping all over the area and the fissures
 * flaring; fissures out from under him growing with each pulse and arc cracks opening at each pulse's front, molten light
 * in them and a warm glow over the area; dust hanging over it. @p floorLayer as DrawGroundStompFx.
 */
void DrawEarthquakeFx(const Surface &out, Point centre, float age, bool floorLayer);
/** @brief The picture's shake at @p age: a jolt on each pulse, a tremor between (the game's screen shake would carry it). */
Displacement EarthquakeShake(float age);

/** @brief How long the Hammer's glowing cracks last, in seconds. */
constexpr float GlowCracksSeconds = 0.9F;

/**
 * @brief Hammer of the Ancients' cracks (the Skill Eye Candy page, 2026-10-12): eight molten cracks running out from where
 * the blow landed (@p centre, its floor point) at @p age seconds, cooling to dark scars and fading. On the floor only;
 * @p seed picks the cracks' shape, so no two blows crack alike.
 */
void DrawGlowCracksFx(const Surface &out, Point centre, float age, uint32_t seed);

/** @brief The drawn Holy Lance: a gather before the thrust, the thrust's travel, and the whole, in seconds. */
constexpr float HolyLanceGatherSeconds = 0.12F;
constexpr float HolyLanceTravelSeconds = 0.12F;
constexpr float HolyLanceSeconds = 0.9F;

/**
 * @brief The drawn Holy Lance (user, 2026-10-12: "invent holy lance animation. i dont like the asset"): a spear of holy light
 * thrust level at chest height from in front of the Paladin standing at @p foot along the tile direction (@p dirX, @p dirY),
 * through @p tiles tiles - a white-hot core in a gold halo, two ribbons of light winding round it, a star at its tip; each
 * body it pierces flashes; a starburst, a ring of light on the floor and sparks where it ends; then it dissolves from the
 * tail into rising motes. @p age from -HolyLanceGatherSeconds (the gather) to HolyLanceSeconds. @p floorLayer as
 * DrawGroundStompFx.
 */
void DrawHolyLanceFx(const Surface &out, Point foot, int dirX, int dirY, int tiles, float age, bool floorLayer);

/** @brief How long the drawn Chill Touch, a struck body's rime and the drawn Frost Nova last, in seconds. */
constexpr float ChillTouchSeconds = 1.4F;
constexpr float RimeSeconds = 0.9F;
constexpr float FrostNovaSeconds = 1.3F;

/**
 * @brief The drawn Chill Touch (user, 2026-10-12: "reinvent chill touch. i dont like the asset"): a breath of frost from the
 * Sorcerer at @p foot along the tile direction (@p dirX, @p dirY) - icy mist rolling out through a cone 1.7 tiles long and
 * sinking to the floor, ice crystals glittering in it, hoarfrost ferns growing on the floor as it reaches them under a cold
 * sheen, and melting. @p floorLayer as DrawGroundStompFx.
 */
void DrawChillTouchFx(const Surface &out, Point foot, int dirX, int dirY, float age, bool floorLayer);
/** @brief A body the cold struck, standing at @p foot: a white-blue flash and ice splinters; a ring of frost at its feet. */
void DrawRimeFx(const Surface &out, Point foot, float age, bool floorLayer);

/**
 * @brief The drawn Frost Nova (user, 2026-10-12: "suggest a reinvented animated frost nova"): round @p centre, a ring of
 * frost light and mist rolling out to its reach with the air bending, ice spikes bursting from the floor as it passes and
 * shattering into falling shards, hoarfrost left behind under a cold sheen, crystals glittering. @p floorLayer as
 * DrawGroundStompFx.
 */
void DrawFrostNovaFx(const Surface &out, Point centre, float age, bool floorLayer);

/** @brief The drawn Frozen Orb's size, its burst's length, and Absolute Zero's whole length and a frozen body's, in seconds. */
constexpr float FrozenOrbRadius = 15.0F;
constexpr float FrozenOrbBurstSeconds = 0.9F;
constexpr float AbsoluteZeroSeconds = 8.0F;
constexpr float IceEncaseSeconds = 2.0F;

/**
 * @brief The drawn Frozen Orb (user, 2026-10-12: "make it better and make it roll in direction it is fired at"): a faceted
 * ball of ice resting on the floor at @p at, rolled there from @p start - its facets turning the way it goes, lit from
 * above with a cold glow inside and bright cracks between the faces; frost left along its track, mist streaming off its
 * back, crystals flung off as it turns, its cold light on the floor. @p floorLayer as DrawGroundStompFx.
 */
void DrawFrozenOrbFx(const Surface &out, Point start, Point at, float age, bool floorLayer);
/** @brief The orb shattering at @p centre: a flash, shards of it flying, a star of hoarfrost and a ring of cold light. */
void DrawFrozenOrbBurstFx(const Surface &out, Point centre, float age, bool floorLayer);

/**
 * @brief The drawn Absolute Zero (user, 2026-10-12: "it need to be very impactful. it is the ultimate cold spell. invent
 * something overwhelming"): cast at @p castAt - the warmth drawn in to a swelling core, a blinding detonation and a
 * shockwave, great spires of ice erupting round her and the floor frozen to cracked glassy ice; then the vortex round her
 * at @p heroAt for seven seconds - a funnel of snow spinning wider as it rises, shards of ice orbiting, arms of frost light
 * turning on the ice and a ring pulsing with each strike; the spires shattering at the end of the freeze. With
 * ApplyAbsoluteZeroGrade over the whole picture. @p floorLayer as DrawGroundStompFx.
 */
void DrawAbsoluteZeroFx(const Surface &out, Point castAt, Point heroAt, float age, bool floorLayer);
/** @brief A body Absolute Zero froze, at @p foot: sealed in a block of ice for IceEncaseSeconds, then the block shattering. */
void DrawIceEncaseFx(const Surface &out, Point foot, float age, bool floorLayer);
/** @brief Absolute Zero over the whole picture: the colours draining to a frozen blue, frost creeping in over its edges. */
void ApplyAbsoluteZeroGrade(const Surface &out, float age);

/** @brief Static Charge's discharge, Cinder Touch's touch and burn, and one breath of the furnace, in seconds. */
constexpr float StaticDischargeSeconds = 0.3F;
constexpr float CinderTouchSeconds = 3.3F;
constexpr float FurnaceFlameSeconds = 0.9F;

/**
 * @brief The drawn Static Charge (user, 2026-10-12: "reinvent static charge"), on the Sorcerer at @p foot while it lasts,
 * @p age into it: arcs crawling over the body, new ones fourteen times a second, one leaping to the floor now and then,
 * charged motes circling, a blue glow under him and sparks crackling on the floor, all surging every so often.
 * @p floorLayer as DrawGroundStompFx.
 */
void DrawStaticChargeFx(const Surface &out, Point foot, float age, bool floorLayer);
/** @brief The charge discharging into whoever struck him: a bolt from @p from to @p to (both feet), its flash and sparks. */
void DrawStaticDischargeFx(const Surface &out, Point from, Point to, float age);

/**
 * @brief The drawn Cinder Touch (user, 2026-10-12: "reinvent cinder touch. current asset is ugly"): embers swept from the
 * Sorcerer's hand at @p foot onto the enemy at @p target and a flash, then it burns its three seconds - tongues of flame
 * licking up it, embers and smoke rising, its light flickering on the floor, a scorched ring left where it stood.
 * @p floorLayer as DrawGroundStompFx.
 */
void DrawCinderTouchFx(const Surface &out, Point foot, Point target, float age, bool floorLayer);

/**
 * @brief The drawn breath of Furnace Mouth (user, 2026-10-12: "better and longer lasting flamethrow animation"): from the
 * dragon's mouth of the furnace standing at @p floor, facing the tile direction (@p dirX, @p dirY), a stream of fire three
 * tiles long for FurnaceFlameSeconds - white-hot at the mouth through yellow and orange to dying red and smoke, widening,
 * curling and falling toward the floor, embers thrown; its light along the floor, a scorch where it lands, the air
 * shimmering. @p floorLayer as DrawGroundStompFx.
 */
void DrawFurnaceFlameFx(const Surface &out, Point floor, int dirX, int dirY, float age, bool floorLayer);
/** @brief The furnace itself alive between breaths: coals glowing through it (brighter while it breathes), its mouth
 * smouldering, heat shimmering over it and smoke from its back. @p sinceBreath: seconds since its last breath began. */
void DrawFurnaceLife(const Surface &out, Point floor, int dirX, int dirY, float sinceBreath, float age);

} // namespace devilution::oracool
