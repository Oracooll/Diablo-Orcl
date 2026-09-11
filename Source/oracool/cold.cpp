#include "oracool/cold.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>

#include "engine.h"
#include "engine/palette.h"
#include "lighting.h"
#include "engine/random.hpp"
#include "missiles.h"
#include "monster.h"
#include "oracool/chill.h"
#include "oracool/class_tree.h"
#include "oracool/skill_sounds.h"
#include "player.h"
#include "utils/language.h"
#include <fmt/format.h>

namespace devilution::oracool {

namespace {

/**
 * What the caster is wearing, per player slot.
 *
 * A side table rather than fields on Player, for the reason the chill table is: this is a status
 * with a duration, and the fixed hero struct is not the place for something that must not survive
 * the session. A static, so it outlives the game - ClearAllColdArmours runs from InitPlayer's
 * first-time path and from the new-game reset, which is the same discipline every other static in
 * this fork keeps.
 */
struct ArmourState {
	SpellID spell = SpellID::Invalid;
	int ticks = 0;
	int level = 0;
	int serial = 0;
};
std::array<ArmourState, MAX_PLRS> Armours {};

ArmourState &StateOf(const Player &player)
{
	return Armours[player.getId()];
}

/** @brief Cold Mastery's rank for @p player - the passive is a tree row, so its investment IS its level. */
int ColdMasteryRank(const Player &player)
{
	if (player._pClass != HeroClass::Sorcerer)
		return 0;
	return ClassTreeInvestment(player, ClassTreeSkill::ColdMastery);
}

/** @brief Ticks of freeze a direct freezing hit inflicts at @p spellLevel: a second and a half, plus a quarter second a rank, capped at five. */
int FreezeTicksFor(int spellLevel)
{
	return std::min(30 + spellLevel * 5, 100);
}

/** @brief Ticks of chill a chilling hit inflicts at @p spellLevel: two seconds, plus a tenth a rank. */
int ChillTicksFor(int spellLevel)
{
	return std::min(40 + spellLevel * 2, 120);
}

} // namespace

bool IsColdSpell(SpellID spell)
{
	switch (spell) {
	case SpellID::IceBolt:
	case SpellID::IceBlast:
	case SpellID::GlacialSpike:
	case SpellID::FrostNova:
	case SpellID::Blizzard:
	case SpellID::FrozenOrb:
	case SpellID::FrozenArmor:
	case SpellID::ShiverArmor:
	case SpellID::ChillingArmor:
		return true;
	default:
		return false;
	}
}

bool IsColdArmourSpell(SpellID spell)
{
	return spell == SpellID::FrozenArmor || spell == SpellID::ShiverArmor || spell == SpellID::ChillingArmor;
}

void ColdSpellDamage(const Player &player, SpellID spell, int spellLevel, int &minDamage, int &maxDamage)
{
	// The rank as the missile carries it (_mispllvl is GetSpellLevel, which is the investment), and
	// as GetDamageAmtAtLevel is handed it - the two must read the same number or the sheet lies.
	const int sl = std::max(spellLevel, 0);
	const int mag = player._pMagic;
	minDamage = -1;
	maxDamage = -1;
	switch (spell) {
	case SpellID::IceBolt:
		// Firebolt's own numbers - the brief calls it "the cold twin of the spell you cast a thousand
		// times", and AddFirebolt is what it fires through, so these MUST agree with that function.
		minDamage = mag / 8 + sl + 1;
		maxDamage = minDamage + 9;
		break;
	case SpellID::IceBlast:
		minDamage = mag / 6 + 2 * sl + 4;
		maxDamage = minDamage + 14;
		break;
	case SpellID::GlacialSpike:
		minDamage = mag / 5 + 3 * sl + 8;
		maxDamage = minDamage + 20;
		break;
	case SpellID::FrostNova:
		// Level-driven like Nova rather than magic-driven like the bolts: a ring around the caster
		// is a panic button, and a panic button should not need a magic build to be worth pressing.
		minDamage = player._pLevel / 2 + 2 * sl + 3;
		maxDamage = minDamage + 10;
		break;
	case SpellID::Blizzard:
		// Per SHARD. A storm lands several, so the number on the sheet is deliberately the small one
		// - the one a monster standing in it takes repeatedly, which is what the player sees.
		minDamage = mag / 8 + 2 * sl + 3;
		maxDamage = minDamage + 10;
		break;
	default:
		break;
	}
	if (minDamage < 0)
		return;
	// Cold Mastery's damage side, on the number the sheet shows as well as on the number the hit
	// deals - they are the same call.
	const int bonus = ColdMasteryDamagePercent(player);
	minDamage += minDamage * bonus / 100;
	maxDamage += maxDamage * bonus / 100;
}

void ApplyColdHit(MissileID type, int level, Monster &monster)
{
	switch (type) {
	case MissileID::IceBlast:
	case MissileID::GlacialSpike:
		// A unique is chilled rather than frozen - see the header - for the freeze's own duration,
		// so it is still the harder hit.
		if (monster.isUnique())
			ChillMonster(monster, FreezeTicksFor(level));
		else
			FreezeMonster(monster, FreezeTicksFor(level));
		break;
	case MissileID::FrostNova:
	case MissileID::BlizzardShard:
		ChillMonster(monster, ChillTicksFor(level) + 20);
		break;
	default:
		// Ice Bolt, Frozen Orb's bolts, the arrows to come, and any cold missile nobody has named
		// yet: the plain chill. Round 1's constant, so nothing that worked yesterday changes.
		ChillMonster(monster, IceBoltChillTicks);
		break;
	}
}

int ColdResistanceDivisor(const Player &player)
{
	const int rank = ColdMasteryRank(player);
	if (rank >= 6)
		return 1;
	if (rank >= 3)
		return 2;
	return 4;
}

int ColdMasteryDamagePercent(const Player &player)
{
	return ColdMasteryRank(player) * 6;
}

// ---------------------------------------------------------------------------------------------
// The armours
// ---------------------------------------------------------------------------------------------

SpellID ActiveColdArmour(const Player &player)
{
	const ArmourState &state = StateOf(player);
	return state.ticks > 0 ? state.spell : SpellID::Invalid;
}

void CastColdArmour(Player &player, SpellID spell, int spellLevel, int ticks)
{
	ArmourState &state = StateOf(player);
	state.spell = spell;
	state.level = spellLevel;
	state.ticks = ticks;
	state.serial++;
}

int ColdArmourCastSerial(const Player &player)
{
	return StateOf(player).serial;
}

uint8_t *ColdTRN()
{
	static std::array<uint8_t, 256> table;
	// Cached on the palette generation (audit, 2026-09-08): this was rebuilt - a 256x16 search - for
	// every monster drawn, every frame, because DrawMonster compares against the pointer.
	static uint32_t builtFor = 0;
	if (builtFor == PaletteRgbGeneration && builtFor != 0)
		return table.data();
	builtFor = PaletteRgbGeneration;
	// The brief's own ramp: 176-191 is the cool blue-grey stone, sixteen steps bright to dark. Each
	// source colour lands on the step nearest its brightness, so a frozen monster keeps its shading
	// and loses its colour - which is what ice over something looks like.
	constexpr int RampFirst = 176;
	constexpr int RampCount = 16;
	int rampLum[RampCount];
	for (int i = 0; i < RampCount; i++) {
		const SDL_Color &c = logical_palette[RampFirst + i];
		rampLum[i] = 2 * c.r + 4 * c.g + 3 * c.b;
	}
	table[0] = 0; // transparent stays transparent
	for (int i = 1; i < 256; i++) {
		const SDL_Color &c = logical_palette[i];
		const int lum = 2 * c.r + 4 * c.g + 3 * c.b;
		int best = 0;
		int bestDist = INT32_MAX;
		for (int j = 0; j < RampCount; j++) {
			const int dist = std::abs(rampLum[j] - lum);
			if (dist < bestDist) {
				bestDist = dist;
				best = j;
			}
		}
		table[i] = static_cast<uint8_t>(RampFirst + best);
	}
	return table.data();
}

const uint32_t *FrozenRgbTable(int lightTableIndex)
{
	static std::array<std::array<uint32_t, 256>, NumLightingLevels> tables;
	static std::array<uint32_t, NumLightingLevels> builtFor {};
	lightTableIndex = std::clamp(lightTableIndex, 0, static_cast<int>(NumLightingLevels) - 1);
	std::array<uint32_t, 256> &table = tables[lightTableIndex];
	if (builtFor[lightTableIndex] == PaletteRgbGeneration && builtFor[lightTableIndex] != 0)
		return table.data();
	const std::array<uint8_t, 256> &light = LightTables[lightTableIndex];
	// Frost over the lit sprite: three quarters of the way to grey, then the grey tinted cold - red
	// held back, blue lifted - and a touch brighter, since ice catches light. The old ColdTRN's
	// "one blue ramp by brightness" threw the lighting away and drew every frozen monster at full
	// brightness, which in a dark hall read as white speckle (user screenshots, 2026-09-08).
	constexpr int GreyPercent = 75;
	constexpr int RedScale = 82, GreenScale = 94, BlueScale = 118;
	table[0] = PaletteRGB[light[0]];
	for (int i = 1; i < 256; i++) {
		const uint32_t lit = PaletteRGB[light[i]];
		int r = (lit >> 16) & 0xFF, g = (lit >> 8) & 0xFF, b = lit & 0xFF;
		const int grey = (299 * r + 587 * g + 114 * b) / 1000;
		r += (grey - r) * GreyPercent / 100;
		g += (grey - g) * GreyPercent / 100;
		b += (grey - b) * GreyPercent / 100;
		r = std::min(255, r * RedScale / 100 + 8);
		g = std::min(255, g * GreenScale / 100 + 12);
		b = std::min(255, b * BlueScale / 100 + 24);
		table[i] = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
	}
	builtFor[lightTableIndex] = PaletteRgbGeneration;
	return table.data();
}

void TickColdArmour(Player &player)
{
	ArmourState &state = StateOf(player);
	if (state.ticks > 0)
		state.ticks--;
	if (state.ticks == 0)
		state.spell = SpellID::Invalid;
}

void ClearColdArmour(Player &player)
{
	StateOf(player) = {};
}

void ClearAllColdArmours()
{
	Armours.fill({});
}

void OnColdArmourStruckInMelee(Player &player, Monster &monster)
{
	const ArmourState &state = StateOf(player);
	if (state.ticks <= 0 || monster.mode == MonsterMode::Death)
		return;
	switch (state.spell) {
	case SpellID::FrozenArmor:
		if (monster.isUnique())
			ChillMonster(monster, FreezeTicksFor(state.level));
		else
			FreezeMonster(monster, FreezeTicksFor(state.level));
		break;
	case SpellID::ShiverArmor: {
		ChillMonster(monster, ChillTicksFor(state.level));
		// Ice Bolt's damage at the armour's rank, through the same table the sheet reads.
		int minDamage;
		int maxDamage;
		ColdSpellDamage(player, SpellID::IceBolt, state.level, minDamage, maxDamage);
		const int dam = (minDamage + GenerateRnd(maxDamage - minDamage + 1)) << 6;
		ApplyMonsterDamage(DamageType::Cold, monster, dam);
		if (monster.hitPoints >> 6 <= 0)
			M_StartKill(monster, player);
		else
			M_StartHit(monster, player, dam);
	} break;
	case SpellID::ChillingArmor:
		ChillMonster(monster, ChillTicksFor(state.level));
		OnColdArmourStruckAtRange(player, monster);
		break;
	default:
		break;
	}
}

void OnColdArmourStruckAtRange(Player &player, Monster &monster)
{
	const ArmourState &state = StateOf(player);
	if (state.ticks <= 0 || state.spell != SpellID::ChillingArmor || monster.mode == MonsterMode::Death)
		return;
	// An Ice Bolt from the caster at whoever did it, at the armour's own rank. Only for the local
	// player: missiles are the caster's to add, and in this fork that is always the one at the
	// keyboard.
	if (&player != MyPlayer)
		return;
	AddMissile(player.position.tile, monster.position.tile, player._pdir, MissileID::IceBolt,
	    TARGET_MONSTERS, static_cast<int>(player.getId()), 0, state.level);
}

int ColdArmourShellFrame(const Player &player)
{
	if (StateOf(player).ticks <= 0)
		return -1;
	// Eight frames at ten a second: the "slow shimmer" the brief asks for, and it loops seamlessly
	// because the sheet was drawn to.
	return GetAnimationFrame(8, 10);
}

const char *ColdSpellDescription(SpellID spell)
{
	switch (spell) {
	case SpellID::IceBolt:
		return N_("A shard of ice that damages and chills what it hits, halving its speed for two seconds.");
	case SpellID::IceBlast:
		return N_("A heavier shard that freezes its target solid for a moment. Uniques are chilled instead.");
	case SpellID::GlacialSpike:
		return N_("A spear of ice that freezes what it strikes and shatters, chilling everything beside it.");
	case SpellID::FrostNova:
		return N_("A ring of ice bursting out from the caster, chilling and damaging everything near.");
	case SpellID::Blizzard:
		return N_("Ice falls over an area for a few seconds, chilling and damaging whatever stands in it.");
	case SpellID::FrozenOrb:
		return N_("An orb that drifts toward its mark shedding ice bolts, then bursts into a ring of them.");
	case SpellID::FrozenArmor:
		return N_("Armour of ice for 24 seconds, +4 per rank: anything that strikes you in melee is frozen in place.");
	case SpellID::ShiverArmor:
		return N_("Armour of ice for 24 seconds, +4 per rank: anything that strikes you in melee is chilled and cut by cold.");
	case SpellID::ChillingArmor:
		return N_("Armour of ice for 24 seconds, +4 per rank: anything that hits you - near or far - is chilled and answered with an ice bolt.");
	default:
		return "";
	}
}

int FreezeSecondsTenths(int spellLevel) { return FreezeTicksFor(spellLevel) / 2; }
int ChillSecondsTenths(int spellLevel) { return ChillTicksFor(spellLevel) / 2; }

std::string ColdSpellFactsAt(SpellID spell, int spellLevel)
{
	// The facts, from the same freeze/chill clocks ApplyColdHit and the armours run.
	const int level = std::max(spellLevel, 0);
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	switch (spell) {
	case SpellID::IceBlast:
	case SpellID::GlacialSpike:
		line(fmt::format(fmt::runtime(_("Freeze: {:.1f} s (uniques are chilled instead)")), FreezeTicksFor(level) / 20.0));
		break;
	case SpellID::FrostNova:
	case SpellID::Blizzard:
		line(fmt::format(fmt::runtime(_("Chill: {:.1f} s")), (ChillTicksFor(level) + 20) / 20.0));
		break;
	case SpellID::IceBolt:
	case SpellID::FrozenOrb:
		line(fmt::format(fmt::runtime(_("Chill: {:.1f} s")), IceBoltChillTicks / 20.0));
		break;
	case SpellID::FrozenArmor:
		line(fmt::format(fmt::runtime(_("Duration: {:d} s")), (400 + 80 * level) / 20)); // AddColdArmor's ticks
		line(fmt::format(fmt::runtime(_("Freeze on being struck: {:.1f} s (uniques are chilled)")), FreezeTicksFor(level) / 20.0));
		break;
	case SpellID::ShiverArmor:
		line(fmt::format(fmt::runtime(_("Duration: {:d} s")), (400 + 80 * level) / 20));
		line(fmt::format(fmt::runtime(_("Chill on being struck: {:.1f} s, plus an Ice Bolt's damage")), ChillTicksFor(level) / 20.0));
		break;
	case SpellID::ChillingArmor:
		line(fmt::format(fmt::runtime(_("Duration: {:d} s")), (400 + 80 * level) / 20));
		line(fmt::format(fmt::runtime(_("Chill on being struck: {:.1f} s, near or far")), ChillTicksFor(level) / 20.0));
		break;
	default:
		break;
	}
	return out;
}

ClassTreeSkill ColdMissileCueSkill(MissileID type, bool impact)
{
	switch (type) {
	case MissileID::IceBolt:
		return ClassTreeSkill::IceBolt;
	case MissileID::IceBlast:
		return ClassTreeSkill::IceBlast;
	case MissileID::GlacialSpike:
		return ClassTreeSkill::GlacialSpike;
	case MissileID::FrostNova:
		return impact ? ClassTreeSkill::None : ClassTreeSkill::FrostNova; // the ring IS the cast
	case MissileID::Blizzard:
		return impact ? ClassTreeSkill::None : ClassTreeSkill::Blizzard;
	case MissileID::BlizzardShard:
		return impact ? ClassTreeSkill::Blizzard : ClassTreeSkill::None; // each shard lands; the storm was cast
	case MissileID::FrozenOrb:
		return impact ? ClassTreeSkill::None : ClassTreeSkill::FrozenOrb;
	default:
		return ClassTreeSkill::None;
	}
}

bool PlayColdMissileSound(const Missile &missile, bool impact)
{
	if (missile._micaster != TARGET_MONSTERS || missile._misource < 0) // a player's, not a trap's
		return false;
	if (missile._mitype == MissileID::ColdArmor) {
		// Three armours, one missile: which one is the tree row the cast was stamped with.
		const auto skill = static_cast<ClassTreeSkill>(missile.oracoolSkill);
		return !impact && skill != ClassTreeSkill::None && PlaySkillSound(skill, SkillSoundEvent::Start);
	}
	const ClassTreeSkill skill = ColdMissileCueSkill(missile._mitype, impact);
	return skill != ClassTreeSkill::None && PlaySkillSound(skill, impact ? SkillSoundEvent::Impact : SkillSoundEvent::Cast);
}

void PlayColdArmourExpirySound(const Missile &missile)
{
	const auto skill = static_cast<ClassTreeSkill>(missile.oracoolSkill);
	if (skill != ClassTreeSkill::None)
		PlaySkillSound(skill, SkillSoundEvent::Stop);
}

} // namespace devilution::oracool
