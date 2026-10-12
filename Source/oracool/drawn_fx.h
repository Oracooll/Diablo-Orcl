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

/**
 * @brief A weapon's swing drawn as a crescent of light (user, 2026-10-12, the Rogue's and the Monk's weapon skills): the tip
 * going round @p centre on the floor from angle @p from to @p to (radians on the floor, y doubled; 0 east), @p radius flat
 * pixels out and @p height above the floor, in @p swing seconds; the trail in @p rgb, fading behind the tip, a white edge
 * and a glint at the tip; @p dust (0..1) kicked along a low one.
 */
struct WeaponArc {
	Point centre;
	float from, to, radius, height, swing, dust;
	uint32_t rgb;
};
void DrawWeaponArcFx(const Surface &out, const WeaponArc &arc, float age, bool floorLayer);
/** @brief A blow landing on the body standing at @p foot: a slash of light across it in @p rgb, and sparks. */
void DrawStrikeFlashFx(const Surface &out, Point foot, uint32_t rgb, float age);

/** @brief A thrust from @p from to @p to (screen points at the weapon's height, @p height above the floor) in @p rgb. */
struct Thrust {
	Point from, to;
	float height;
	uint32_t rgb;
};
/** @brief The thrust drawn: a tapered streak shooting out, rings of pressure riding it, a flash at its point. */
void DrawThrustFx(const Surface &out, const Thrust &th, float age, bool floorLayer);

/** @brief How long a hurled spear's landing lasts, and Tiger Claw's rakes and bleeding, in seconds. */
constexpr float HurledSpearImpactSeconds = 0.8F;
constexpr float ClawRakeSeconds = 3.3F;
/**
 * @brief The drawn Valkyrie's Spear: a spear hurled from @p from to @p to (feet) on a shallow arc - its shaft and steel head,
 * a trail of pale gold and two wings of light swept back from its head; planted slanting in the floor where it lands, a burst
 * of rays, a ring of light across the tile and the air bending.
 */
void DrawHurledSpearFx(const Surface &out, Point from, Point to, float age, bool floorLayer);
/** @brief A column of ki light where a staff is planted at @p foot (Mountain Pole), motes rising up it. */
void DrawKiPillarFx(const Surface &out, Point foot, uint32_t rgb, float age);
/** @brief Rings going out from the body at @p foot one after another, like a struck bell (Staff of Echoes). */
void DrawEchoRingsFx(const Surface &out, Point foot, uint32_t rgb, float age);
/** @brief Slivers of jade light drifting down round @p centre like bamboo leaves (Bamboo Rain). */
void DrawJadeLeavesFx(const Surface &out, Point centre, float age);
/**
 * @brief The drawn Heaven Splitter: an overhead arc of light coming down in front of the Monk at @p foot, then a white-gold
 * fissure racing @p tiles tiles along the tile direction (@p dirX, @p dirY) with a curtain of light rising behind its front
 * and stone thrown up either side.
 */
void DrawHeavenSplitterFx(const Surface &out, Point foot, int dirX, int dirY, int tiles, float age, bool floorLayer);
/** @brief A flurry from body to body through @p stops (feet), a streak of ki in @p rgb along each leg, a blur at each stop. */
void DrawFlurryPathFx(const Surface &out, const Point *stops, int count, uint32_t rgb, float age);
/** @brief Tiger Claw on the body at @p foot: three glowing rakes one after another, then the wounds dripping, a pool under it. */
void DrawClawRakeFx(const Surface &out, Point foot, float age, bool floorLayer);

/** @brief How long Leaping Crane's flight takes, in seconds. */
constexpr float LeapingCraneFlight = 0.45F;
/** @brief The Monk's skills drawn (user, 2026-10-12), each a proposal on the Skill Eye Candy page: */
/** Leaping Crane: he flies from @p from to @p to on a high arc, wings of light spread and beating, feathers of light falling; a ring where he lands. */
void DrawLeapingCraneFx(const Surface &out, Point from, Point to, float age, bool floorLayer);
/** Pressure Point on the body at @p foot: a needle of violet ki, its meridians lighting through it, its armour cracking off in shards. */
void DrawPressurePointFx(const Surface &out, Point foot, float age);
/** Shoulder Gate: streaks of speed as he rushes from @p from, then a wall of gold light slamming up at the enemy at @p to. */
void DrawShoulderGateFx(const Surface &out, Point from, Point to, float age, bool floorLayer);
/** Seven-Sided Strike's seal: a seven-pointed star of gold drawn on the floor round @p centre, stroke by stroke, flaring. */
void DrawHeptagramFx(const Surface &out, Point centre, float radius, float age);
/** Dragon's Wrath: a serpent of gold fire racing @p tiles tiles from the Monk at @p foot along (@p dirX, @p dirY), scorching its track. */
void DrawDragonWaveFx(const Surface &out, Point foot, int dirX, int dirY, int tiles, float age, bool floorLayer);
/** A mantra on the Monk at @p foot: a turning mandala in @p rgb as he chants, then motes rising (Clarity) or wind round him (@p evasion). */
void DrawMantraFx(const Surface &out, Point foot, uint32_t rgb, bool evasion, float age, bool floorLayer);
/** Chi Wave: a two-toned orb of chi hopping from body to body through @p stops (feet), a ring on each it strikes. */
void DrawChiWaveFx(const Surface &out, const Point *stops, int count, float age);
/** Blinding Flash: rays of white light bursting from his raised hand at @p foot, the area flooded with light. */
void DrawBlindingFlashFx(const Surface &out, Point foot, float age, bool floorLayer);
/** A blinded or dazed body at @p foot: three little stars circling over its head. */
void DrawDazedFx(const Surface &out, Point foot, float age);
/** Wave of Light: a great spectral bell of gold light dropping onto @p centre, its toll ringing out, a shockwave across the floor. */
void DrawWaveOfLightFx(const Surface &out, Point centre, float age, bool floorLayer);
/** Ancestral Court: three hooded shades forming out of mist round @p centre, then rushing inward together to strike. */
void DrawAncestralCourtFx(const Surface &out, Point centre, float age, bool floorLayer);

} // namespace devilution::oracool
