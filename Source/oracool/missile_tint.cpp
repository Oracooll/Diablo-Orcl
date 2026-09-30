#include "oracool/missile_tint.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <SDL.h>

#include "engine/palette.h"
#include "lighting.h"
#include "oracool/class_tree.h"
#include "player.h"

namespace devilution::oracool {

namespace {

constexpr double Pi = 3.14159265358979;

struct Colour {
	double r, g, b;
};

Colour Unpack(uint32_t c)
{
	return { static_cast<double>((c >> 16) & 0xFF), static_cast<double>((c >> 8) & 0xFF), static_cast<double>(c & 0xFF) };
}

uint32_t Pack(Colour c)
{
	const auto ch = [](double v) { return static_cast<uint32_t>(std::clamp(v, 0.0, 255.0)); };
	return (ch(c.r) << 16) | (ch(c.g) << 8) | ch(c.b);
}

double Luma(Colour c)
{
	return (0.299 * c.r + 0.587 * c.g + 0.114 * c.b) / 255.0;
}

Colour Lerp(Colour a, Colour b, double t)
{
	return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

/** @brief @p hue at the brightness of @p luma: dark stays dark, the brightest parts run on toward white as light does. */
Colour AtBrightness(Colour hue, double luma)
{
	const double k = luma * 1.6;
	const double white = std::max(0.0, luma - 0.7) * 1.6 * 255.0;
	return { hue.r * k + white, hue.g * k + white, hue.b * k + white };
}

/** @brief Seconds on the game clock, for effects that drift with time rather than with their animation. */
double Seconds()
{
	return static_cast<double>(SDL_GetTicks()) / 1000.0;
}

std::array<uint32_t, 256> Result;

} // namespace

const uint32_t *TintedTable(const uint32_t *base, Tint tint, uint32_t rgb, double progress)
{
	const double t = Seconds();
	const Colour hue = Unpack(rgb);
	for (size_t i = 0; i < 256; i++) {
		const Colour c = Unpack(base[i]);
		const double l = Luma(c);
		Colour out = c;
		switch (tint) {
		case Tint::None:
			break;
		case Tint::Hue:
			out = AtBrightness(hue, l);
			break;
		case Tint::HueCycle: {
			// Two bands of light running through the brightness levels as the ring grows, the aura rings' 35% swing:
			// the ring's own shading gives the bands somewhere to travel.
			const double wave = std::cos(2.0 * Pi * (2.0 * l - 1.5 * progress - t / 1.2));
			const Colour lit = AtBrightness(hue, l);
			out = { lit.r * (1.0 + 0.35 * wave), lit.g * (1.0 + 0.35 * wave), lit.b * (1.0 + 0.35 * wave) };
			break;
		}
		case Tint::Earthquake: {
			// Brown to dark orange and back every 1.6 s, with bands rolling through it: molten rock, not a lamp.
			const Colour brown { 112, 64, 28 };
			const Colour darkOrange { 208, 92, 22 };
			const Colour now = Lerp(brown, darkOrange, 0.5 + 0.5 * std::sin(2.0 * Pi * t / 1.6));
			const double wave = std::cos(2.0 * Pi * (2.0 * l - t / 1.2));
			const Colour lit = AtBrightness(now, l);
			out = { lit.r * (1.0 + 0.3 * wave), lit.g * (1.0 + 0.3 * wave), lit.b * (1.0 + 0.3 * wave) };
			break;
		}
		case Tint::Ice:
			out = Lerp(c, AtBrightness({ 150, 200, 255 }, 0.35 + 0.75 * l), 0.55);
			break;
		case Tint::Astral:
			out = Lerp(c, AtBrightness({ 190, 170, 255 }, 0.35 + 0.8 * l), 0.5 + 0.12 * std::sin(2.0 * Pi * t / 1.2));
			break;
		case Tint::Mend:
			out = Lerp(c, AtBrightness({ 185, 160, 255 }, 0.4 + 0.8 * l), 0.65 * std::clamp(progress, 0.0, 1.0));
			break;
		case Tint::Glint: {
			// The clock alone moves the bands: a looping sheet's frame would jump them back at every wrap.
			const double wave = std::cos(2.0 * Pi * (2.0 * l - t / 0.9));
			const Colour base = rgb == 0 ? c : Lerp(c, AtBrightness(hue, 0.35 + 0.75 * l), 0.6);
			out = { base.r * (1.0 + 0.4 * wave), base.g * (1.0 + 0.4 * wave), base.b * (1.0 + 0.4 * wave) };
			break;
		}
		}
		Result[i] = Pack(out);
	}
	return Result.data();
}

const uint32_t *LitPaletteTable(int lightLevel)
{
	static std::array<std::array<uint32_t, 256>, NumLightingLevels> tables;
	static std::array<uint32_t, NumLightingLevels> builtFor {};
	const size_t level = static_cast<size_t>(std::clamp(lightLevel, 0, static_cast<int>(NumLightingLevels) - 1));
	if (builtFor[level] != PaletteRgbGeneration || PaletteRgbGeneration == 0) {
		for (size_t i = 0; i < 256; i++)
			// Level 0 is full light, the palette itself: its table's one odd entry sends white to black (see DrawMonster).
			tables[level][i] = PaletteRGB[level == 0 ? i : LightTables[level][i]];
		builtFor[level] = PaletteRgbGeneration;
	}
	return tables[level].data();
}

uint32_t RingHueForSkill(uint16_t classTreeSkill)
{
	if (classTreeSkill == 0xFFFF || classTreeSkill >= ClassTreeSkillCount)
		return Rgb(236, 220, 186); // no skill known: a warm, pale ring
	const auto skill = static_cast<ClassTreeSkill>(classTreeSkill);
	// The skills whose element says more than their class does.
	switch (skill) {
	case ClassTreeSkill::HolyFire: return Rgb(255, 128, 40);
	case ClassTreeSkill::HolyFreeze: return Rgb(150, 210, 255);
	case ClassTreeSkill::HolyShock:
	case ClassTreeSkill::StaticField:
	case ClassTreeSkill::ThunderStorm: return Rgb(140, 176, 255);
	case ClassTreeSkill::Conversion: return Rgb(170, 230, 120);
	case ClassTreeSkill::Vengeance: return Rgb(255, 196, 96);
	case ClassTreeSkill::SpiritGuardian: return Rgb(196, 176, 255);
	default: break;
	}
	switch (GetClassTreeSkillData(skill).heroClass) {
	case HeroClass::Barbarian: return Rgb(232, 96, 44);    // war-paint red
	case HeroClass::Warrior: return Rgb(244, 204, 96);     // the Paladin's gold
	case HeroClass::Monk: return Rgb(255, 214, 128);       // chi
	case HeroClass::Sorcerer: return Rgb(128, 170, 255);   // arcane blue
	case HeroClass::Rogue: return Rgb(206, 182, 255);      // spectral lavender
	case HeroClass::Necromancer: return Rgb(150, 214, 110); // the grave's green
	default: return Rgb(236, 220, 186);
	}
}

} // namespace devilution::oracool
