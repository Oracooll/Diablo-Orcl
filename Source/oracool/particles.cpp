/**
 * @file oracool/particles.cpp
 *
 * Particles - see particles.h.
 */
#include "oracool/particles.h"
#include "oracool/splats.h"

#include <array>
#include <cmath>
#include <cstdint>

#include <SDL.h> // SDL_GetTicks - the pool runs in real time

#include "diablo.h" // PauseMode
#include "engine/surface.hpp"
#include "levels/gendung.h"
#include "misdat.h"
#include "missiles.h"
#include "objects.h"
#include "options.h"
#include "oracool/missile_tint.h"
#include "player.h"

// Drawn once a frame from the Debug build the game is played on: optimised there too, as oracool/colour_light.cpp is.
// The draw loop below is written out by hand (no calls survive /Ob0).
#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

namespace devilution::oracool {

namespace {

/** The last hit burst per tile (hashed into 64 slots): one burst a tile per 150 ms (visual audit round 1). */
std::array<uint32_t, 64> BurstTicks {};
std::array<int32_t, 64> BurstTile {};

enum class Kind : uint8_t {
	Ember,
	Spark,
	Mote,
	Ash,
};

struct Particle {
	float x, y;    // tile coordinates (a tile's centre is its integer position)
	float z;       // pixels above the floor
	float vx, vy;  // tiles a second
	float vz;      // pixels a second, up
	float life;    // seconds left
	float maxLife; // seconds it was born with
	uint32_t rgb;
	uint8_t size;     // 1..3 px
	uint8_t strength; // 0..255 at full life
	Kind kind;
};

std::array<Particle, MaxParticles> Pool;
size_t Count = 0;
uint32_t LastTicks = 0;
uint32_t LevelKey = 0xFFFFFFFFu;
uint32_t RngState = 0;

/** The pool's own xorshift32 - never the game's RNG. */
uint32_t NextRandom()
{
	if (RngState == 0)
		RngState = (SDL_GetTicks() * 2654435761u) | 1u;
	uint32_t s = RngState;
	s ^= s << 13;
	s ^= s >> 17;
	s ^= s << 5;
	RngState = s;
	return s;
}

/** 0..1 */
float Frand()
{
	return static_cast<float>(NextRandom() >> 8) * (1.0F / 16777216.0F);
}

/** lo..hi */
float Frange(float lo, float hi)
{
	return lo + (hi - lo) * Frand();
}

Particle *Spawn()
{
	if (Count >= MaxParticles)
		return nullptr;
	Particle &p = Pool[Count++];
	p = Particle {};
	return &p;
}

uint32_t MakeRgb(int r, int g, int b)
{
	r = r < 0 ? 0 : (r > 255 ? 255 : r);
	g = g < 0 ? 0 : (g > 255 ? 255 : g);
	b = b < 0 ? 0 : (b > 255 ? 255 : b);
	return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

/** Where an object's flame burns, in pixels above its tile, and how many embers a second it gives; 0 for no flame. */
float FlameHeight(_object_id type, float &rate)
{
	switch (type) {
	case OBJ_TORCHL:
	case OBJ_TORCHR:
	case OBJ_TORCHL2:
	case OBJ_TORCHR2:
		rate = 0.7F;
		return 58.0F;
	case OBJ_L1LIGHT: // the Cathedral's hanging braziers
		rate = 0.9F;
		return 90.0F;
	case OBJ_SKFIRE:
		rate = 1.2F;
		return 14.0F;
	case OBJ_BCROSS:
	case OBJ_TBCROSS:
		rate = 1.0F;
		return 60.0F;
	case OBJ_CANDLE1:
	case OBJ_CANDLE2:
	case OBJ_BOOKCANDLE:
	case OBJ_STORYCANDLE:
	case OBJ_L5CANDLE:
		rate = 0.25F;
		return 34.0F;
	default:
		rate = 0;
		return 0;
	}
}

constexpr float EmberReach = 12.0F;
constexpr float KeepReach = 24.0F; // a particle this far from the hero is dropped
constexpr int MoteTarget = 18;
constexpr int AshTarget = 70;

void EmitEmbers(float dt, Point hero)
{
	for (int i = 0; i < ActiveObjectCount; i++) {
		const Object &object = Objects[ActiveObjects[i]];
		float rate;
		const float height = FlameHeight(object._otype, rate);
		if (height <= 0)
			continue;
		const float dx = static_cast<float>(object.position.x - hero.x);
		const float dy = static_cast<float>(object.position.y - hero.y);
		if (dx * dx + dy * dy > EmberReach * EmberReach)
			continue;
		if (Frand() >= rate * dt)
			continue;
		Particle *p = Spawn();
		if (p == nullptr)
			return;
		p->kind = Kind::Ember;
		p->x = static_cast<float>(object.position.x) + Frange(-0.12F, 0.12F);
		p->y = static_cast<float>(object.position.y) + Frange(-0.12F, 0.12F);
		p->z = height + Frange(-4.0F, 4.0F);
		p->vx = Frange(-0.10F, 0.10F);
		p->vy = Frange(-0.10F, 0.10F);
		p->vz = Frange(14.0F, 28.0F);
		p->maxLife = p->life = Frange(1.3F, 2.6F);
		p->rgb = MakeRgb(255, static_cast<int>(Frange(110, 175)), static_cast<int>(Frange(25, 60)));
		p->size = Frand() < 0.25F ? 2 : 1;
		p->strength = static_cast<uint8_t>(Frange(170, 240));
	}
}

void EmitMotes(float dt, Point hero, int alive, int lightRadius)
{
	if (alive >= MoteTarget || Frand() >= 4.0F * dt)
		return;
	Particle *p = Spawn();
	if (p == nullptr)
		return;
	const float r = static_cast<float>(lightRadius < 3 ? 3 : (lightRadius > 9 ? 9 : lightRadius)) * 0.8F;
	const float angle = Frange(0.0F, 6.2831853F);
	const float dist = r * std::sqrt(Frand());
	p->kind = Kind::Mote;
	p->x = static_cast<float>(hero.x) + dist * std::cos(angle);
	p->y = static_cast<float>(hero.y) + dist * std::sin(angle);
	p->z = Frange(8.0F, 70.0F);
	p->vx = Frange(-0.12F, 0.12F);
	p->vy = Frange(-0.12F, 0.12F);
	p->vz = Frange(-3.0F, 3.0F);
	p->maxLife = p->life = Frange(5.0F, 9.0F);
	const int grey = static_cast<int>(Frange(170, 215));
	p->rgb = MakeRgb(grey + 20, grey + 4, grey - 40); // grey-gold
	p->size = 1;
	p->strength = static_cast<uint8_t>(Frange(55, 95));
}

void EmitAsh(float dt, Point hero, int alive)
{
	// Hell starts full: a few flakes a frame until the target is met, then a steady fall.
	const int wanted = AshTarget - alive;
	if (wanted <= 0)
		return;
	int spawn = static_cast<int>(wanted * dt * 1.5F);
	if (spawn < 1)
		spawn = Frand() < wanted * dt * 1.5F ? 1 : 0;
	for (int i = 0; i < spawn; i++) {
		Particle *p = Spawn();
		if (p == nullptr)
			return;
		p->kind = Kind::Ash;
		p->x = static_cast<float>(hero.x) + Frange(-11.0F, 11.0F);
		p->y = static_cast<float>(hero.y) + Frange(-11.0F, 11.0F);
		p->z = Frange(90.0F, 230.0F);
		p->vx = 0.22F + Frange(-0.10F, 0.10F); // a slow wind, east and a little north on screen
		p->vy = -0.08F + Frange(-0.10F, 0.10F);
		p->vz = Frange(-30.0F, -16.0F);
		p->maxLife = p->life = Frange(7.0F, 12.0F);
		if (Frand() < 0.3F) {
			const int k = static_cast<int>(Frange(14, 34));
			p->rgb = MakeRgb(k, k - 2, k - 4);
		} else {
			const int k = static_cast<int>(Frange(70, 150));
			p->rgb = MakeRgb(k, k - 4, k - 8);
		}
		p->size = Frand() < 0.35F ? 2 : 1;
		p->strength = static_cast<uint8_t>(Frange(140, 200));
	}
}

void Advance(float dt, bool haveHero, Point hero)
{
	for (size_t i = 0; i < Count;) {
		Particle &p = Pool[i];
		p.life -= dt;
		p.x += p.vx * dt;
		p.y += p.vy * dt;
		p.z += p.vz * dt;
		switch (p.kind) {
		case Kind::Spark:
			p.vz -= 380.0F * dt;
			p.vx *= 1.0F - 2.0F * dt;
			p.vy *= 1.0F - 2.0F * dt;
			if (p.z < 0) {
				p.z = 0;
				p.vz = -p.vz * 0.3F;
			}
			break;
		case Kind::Ember:
			p.vx += Frange(-0.25F, 0.25F) * dt; // the flicker of rising heat
			p.vy += Frange(-0.25F, 0.25F) * dt;
			break;
		case Kind::Mote:
			p.vx += Frange(-0.04F, 0.04F) * dt;
			p.vy += Frange(-0.04F, 0.04F) * dt;
			if (p.z < 4 || p.z > 80)
				p.vz = -p.vz;
			break;
		case Kind::Ash:
			p.vx += Frange(-0.3F, 0.3F) * dt; // the sway
			if (p.z <= 0 && p.life > 0.4F)
				p.life = 0.4F; // settles and fades
			if (p.z < 0)
				p.z = 0;
			break;
		}
		bool dead = p.life <= 0;
		if (!dead && haveHero) {
			const float dx = p.x - static_cast<float>(hero.x);
			const float dy = p.y - static_cast<float>(hero.y);
			dead = dx * dx + dy * dy > KeepReach * KeepReach;
		}
		if (dead) {
			Pool[i] = Pool[Count - 1];
			Count--;
			continue;
		}
		i++;
	}
}

/** The dot shapes: a 1 px core, a dimmer cross for 2, and the corners too for 3 (weights of 256). */
constexpr int Kernel[3][9] = {
	{ 0, 0, 0, 0, 256, 0, 0, 0, 0 },
	{ 0, 110, 0, 110, 256, 110, 0, 110, 0 },
	{ 50, 150, 50, 150, 256, 150, 50, 150, 50 },
};

void Draw(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	const int w = out.w();
	const int h = out.h();
	if (w <= 0 || h <= 0)
		return;
	uint32_t *base = out.at<uint32_t>(0, 0);
	const int pitch = out.pixelPitch();
	// The inverse of colour_light's GroundMap: firstTile's centre is 32 px right of and 14.5 px above firstTileBuffer;
	// one tile +x is (+32, +16) on screen and one tile +y is (-32, +16).
	const float cx = static_cast<float>(firstTileBuffer.x) + 32.0F;
	const float cy = static_cast<float>(firstTileBuffer.y) - 14.5F;
	const float fx = static_cast<float>(firstTile.x);
	const float fy = static_cast<float>(firstTile.y);
	const Particle *pool = Pool.data();
	for (size_t n = 0; n < Count; n++) {
		const Particle &p = pool[n];
		const float rx = p.x - fx;
		const float ry = p.y - fy;
		const float sxf = cx + (rx - ry) * 32.0F;
		const float syf = cy + (rx + ry) * 16.0F - p.z;
		if (sxf < -2.0F || syf < -2.0F || sxf > static_cast<float>(w + 2) || syf > static_cast<float>(h + 2))
			continue;
		const int sx = static_cast<int>(sxf);
		const int sy = static_cast<int>(syf);
		// How bright it is now: sparks and embers fade as they die, motes fade in and out, ash fades in and settles.
		float f;
		const float age = p.maxLife - p.life;
		if (p.kind == Kind::Mote) {
			const float in = age / 1.2F;
			const float outF = p.life / 1.6F;
			f = in < outF ? in : outF;
		} else if (p.kind == Kind::Ash) {
			const float in = age / 0.8F;
			const float outF = p.life / 0.4F;
			f = in < outF ? in : outF;
		} else {
			f = p.life / p.maxLife;
		}
		f = f < 0 ? 0 : (f > 1 ? 1 : f);
		const int intensity = static_cast<int>(f * static_cast<float>(p.strength) * (256.0F / 255.0F)); // 0..256
		if (intensity <= 0)
			continue;
		const int shape = p.size < 1 ? 0 : (p.size > 3 ? 2 : p.size - 1);
		const int *kernel = Kernel[shape];
		const uint32_t cr = (p.rgb >> 16) & 0xFFu;
		const uint32_t cg = (p.rgb >> 8) & 0xFFu;
		const uint32_t cb = p.rgb & 0xFFu;
		const bool additive = p.kind != Kind::Ash; // ash is dark: it covers, light adds
		std::vector<Splat> *splats = SplatCollection(); // the GPU plan's Stage 2: the card blends them (oracool/splats.h)
		for (int k = 0; k < 9; k++) {
			const int weight = kernel[k];
			if (weight == 0)
				continue;
			const int px = sx + (k % 3) - 1;
			const int py = sy + (k / 3) - 1;
			if (px < 0 || py < 0 || px >= w || py >= h)
				continue;
			const uint32_t a = static_cast<uint32_t>((intensity * weight) >> 8); // 0..256
			if (splats != nullptr) {
				// Relative to the surface's own origin (out.at(0, 0)), as the pixel loop's px and py are.
				if (additive)
					splats->push_back({ static_cast<float>(px), static_cast<float>(py), static_cast<float>((cr * a) >> 8) / 255.0F,
					    static_cast<float>((cg * a) >> 8) / 255.0F, static_cast<float>((cb * a) >> 8) / 255.0F, 0.0F });
				else
					splats->push_back({ static_cast<float>(px), static_cast<float>(py), static_cast<float>(cr * a) / (256.0F * 255.0F),
					    static_cast<float>(cg * a) / (256.0F * 255.0F), static_cast<float>(cb * a) / (256.0F * 255.0F), static_cast<float>(a) / 256.0F });
				continue;
			}
			uint32_t *dst = base + py * pitch + px;
			const uint32_t d = *dst;
			uint32_t r, g, b;
			if (additive) {
				r = ((d >> 16) & 0xFFu) + ((cr * a) >> 8);
				g = ((d >> 8) & 0xFFu) + ((cg * a) >> 8);
				b = (d & 0xFFu) + ((cb * a) >> 8);
				r = r > 255u ? 255u : r;
				g = g > 255u ? 255u : g;
				b = b > 255u ? 255u : b;
			} else {
				r = (cr * a + ((d >> 16) & 0xFFu) * (256u - a)) >> 8;
				g = (cg * a + ((d >> 8) & 0xFFu) * (256u - a)) >> 8;
				b = (cb * a + (d & 0xFFu) * (256u - a)) >> 8;
			}
			*dst = (r << 16) | (g << 8) | b;
		}
	}
}

uint32_t CurrentLevelKey()
{
	return static_cast<uint32_t>(leveltype) | (static_cast<uint32_t>(currlevel) << 8) | (static_cast<uint32_t>(setlevel ? 1 : 0) << 16)
	    | (static_cast<uint32_t>(setlvlnum) << 24);
}

} // namespace

void ForgetParticles()
{
	Count = 0;
	LastTicks = 0;
	BurstTicks.fill(0);
	BurstTile.fill(0);
}

size_t ParticleCount()
{
	return Count;
}

uint32_t SparkRgb(DamageType element)
{
	switch (element) {
	case DamageType::Fire: return hue::FireOrange;
	case DamageType::Lightning: return hue::LightningBlue;
	case DamageType::Magic: return hue::SpectralLavender;
	case DamageType::Acid: return hue::PoisonGreen;
	case DamageType::Cold: return hue::IceBlue;
	default: return SteelSparkRgb;
	}
}

uint32_t MissileSparkRgb(const Missile &missile)
{
	if (missile.oracoolTint != Tint::None && missile.oracoolTintRgb != 0)
		return missile.oracoolTintRgb;
	switch (missile._miAnimType) {
	case MissileGraphicID::HolyBolt:
	case MissileGraphicID::HolyBoltExplosion:
	case MissileGraphicID::YellowFlareExplosion:
		return hue::PaladinGold;
	case MissileGraphicID::BlueFlareExplosion:
	case MissileGraphicID::BlueFlareExplosion2:
		return hue::LightningBlue;
	case MissileGraphicID::RedFlareExplosion:
	case MissileGraphicID::OrangeFlareExplosion:
	case MissileGraphicID::MagmaBallExplosion:
		return hue::FireOrange;
	default:
		break;
	}
	return SparkRgb(GetMissileData(missile._mitype).damageType());
}

void EmitHitSparks(Point tile, uint32_t rgb, int count)
{
	if (!*sgOptions.Oracool.particles || count <= 0)
		return;
	// A fire wall or an inferno hits every monster in it every tick: one burst per tile per 150 ms, or the pool fills
	// with a fountain and the embers and dust starve (visual audit round 1).
	{
		const int32_t key = tile.x * 256 + tile.y + 1;
		const size_t slot = static_cast<size_t>((static_cast<uint32_t>(key) * 2654435761U) >> 26);
		const uint32_t now = SDL_GetTicks();
		if (BurstTile[slot] == key && now - BurstTicks[slot] < 150)
			return;
		BurstTile[slot] = key;
		BurstTicks[slot] = now;
	}
	if (count > 64)
		count = 64;
	const int cr = static_cast<int>((rgb >> 16) & 0xFF);
	const int cg = static_cast<int>((rgb >> 8) & 0xFF);
	const int cb = static_cast<int>(rgb & 0xFF);
	for (int i = 0; i < count; i++) {
		Particle *p = Spawn();
		if (p == nullptr)
			return;
		const float angle = Frange(0.0F, 6.2831853F);
		const float speed = Frange(1.2F, 3.2F);
		p->kind = Kind::Spark;
		p->x = static_cast<float>(tile.x) + Frange(-0.2F, 0.2F);
		p->y = static_cast<float>(tile.y) + Frange(-0.2F, 0.2F);
		p->z = Frange(30.0F, 46.0F);
		p->vx = speed * std::cos(angle);
		p->vy = speed * std::sin(angle);
		p->vz = Frange(30.0F, 110.0F);
		p->maxLife = p->life = Frange(0.22F, 0.5F);
		const float white = Frange(0.0F, 0.4F); // some run hotter, toward white
		p->rgb = MakeRgb(cr + static_cast<int>((255 - cr) * white), cg + static_cast<int>((255 - cg) * white), cb + static_cast<int>((255 - cb) * white));
		p->size = Frand() < 0.3F ? 2 : 1;
		p->strength = 255;
	}
}

void EmitMissileSparks(const Missile &missile, int count)
{
	EmitHitSparks(missile.position.tile, MissileSparkRgb(missile), count);
}

void UpdateAndDrawParticles(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (!*sgOptions.Oracool.particles) {
		ForgetParticles();
		return;
	}
	if (out.isIndexed())
		return;
	const uint32_t key = CurrentLevelKey();
	if (key != LevelKey) {
		LevelKey = key;
		ForgetParticles();
	}

	const uint32_t now = SDL_GetTicks();
	float dt = LastTicks == 0 ? 0.0F : static_cast<float>(now - LastTicks) / 1000.0F;
	LastTicks = now == 0 ? 1 : now;
	dt = dt < 0 ? 0 : (dt > 0.05F ? 0.05F : dt);
	if (PauseMode != 0)
		dt = 0;

	const bool haveHero = MyPlayer != nullptr;
	const Point hero = haveHero ? Point(MyPlayer->position.tile) : Point { 0, 0 };
	if (dt > 0 && haveHero) {
		int motes = 0, ash = 0;
		for (size_t i = 0; i < Count; i++) {
			if (Pool[i].kind == Kind::Mote)
				motes++;
			else if (Pool[i].kind == Kind::Ash)
				ash++;
		}
		EmitEmbers(dt, hero);
		if (leveltype != DTYPE_TOWN)
			EmitMotes(dt, hero, motes, MyPlayer->_pLightRad);
		if (leveltype == DTYPE_HELL)
			EmitAsh(dt, hero, ash);
	}
	if (dt > 0)
		Advance(dt, haveHero, hero);
	Draw(out, firstTile, firstTileBuffer);
}

} // namespace devilution::oracool

#ifdef _MSC_VER
#pragma optimize("", on)
#pragma runtime_checks("", restore)
#endif
