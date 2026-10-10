/**
 * @file oracool/spell_glow.cpp
 *
 * Glowing Spells - see spell_glow.h.
 */
#include "oracool/spell_glow.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <string>

#include "engine/palette.h"
#include "levels/gendung.h"
#include "misdat.h"
#include "missiles.h"
#include "monster.h"
#include "options.h"
#include "oracool/missile_tint.h"

// Called once a drawn missile a frame, from the Debug build the game is played on: optimised there too, as
// oracool/colour_light.cpp is.
#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

namespace devilution::oracool {

namespace {

enum class Glow : uint8_t {
	Unknown,
	No,
	Yes,
};

/** The fixed answers: every vanilla sheet and the Orcl sheets registered before RfA-27 (whose names are not all telling). */
Glow GraphicGlow(MissileGraphicID graphic)
{
	switch (graphic) {
	// Light: fire, lightning, holy, magic orbs and flares, their bursts, portals.
	case MissileGraphicID::Fireball:
	case MissileGraphicID::Lightning:
	case MissileGraphicID::FireWall:
	case MissileGraphicID::MagmaBallExplosion:
	case MissileGraphicID::TownPortal:
	case MissileGraphicID::FlashBottom:
	case MissileGraphicID::FlashTop:
	case MissileGraphicID::FireArrow: // vanilla's fire arrow is a flame, not a shaft
	case MissileGraphicID::ApocalypseBoom:
	case MissileGraphicID::BigExplosion:
	case MissileGraphicID::Inferno:
	case MissileGraphicID::ThinLightning:
	case MissileGraphicID::BloodStar: // the succubi's orbs are magic light, whatever the name
	case MissileGraphicID::BloodStarExplosion:
	case MissileGraphicID::MagmaBall:
	case MissileGraphicID::ChargedBolt:
	case MissileGraphicID::HolyBolt:
	case MissileGraphicID::HolyBoltExplosion:
	case MissileGraphicID::LightningArrow:
	case MissileGraphicID::FireArrowExplosion:
	case MissileGraphicID::Elemental:
	case MissileGraphicID::Resurrect:
	case MissileGraphicID::RedPortal:
	case MissileGraphicID::DiabloApocalypseBoom:
	case MissileGraphicID::BloodStarBlue:
	case MissileGraphicID::BloodStarBlueExplosion:
	case MissileGraphicID::BloodStarYellow:
	case MissileGraphicID::BloodStarYellowExplosion:
	case MissileGraphicID::BloodStarRed:
	case MissileGraphicID::BloodStarRedExplosion:
	case MissileGraphicID::OrangeFlare:
	case MissileGraphicID::BlueFlare:
	case MissileGraphicID::RedFlare:
	case MissileGraphicID::YellowFlare:
	case MissileGraphicID::Rune:
	case MissileGraphicID::YellowFlareExplosion:
	case MissileGraphicID::BlueFlareExplosion:
	case MissileGraphicID::RedFlareExplosion:
	case MissileGraphicID::BlueFlare2:
	case MissileGraphicID::OrangeFlareExplosion:
	case MissileGraphicID::BlueFlareExplosion2:
	// The Cold pack's glints (its frozen floor and the hero's ice shell below are not light).
	case MissileGraphicID::IceBolt:
	case MissileGraphicID::IceImpact:
	case MissileGraphicID::IceBlast:
	case MissileGraphicID::GlacialSpike:
	case MissileGraphicID::GlacialShatter:
	case MissileGraphicID::FrostNova:
	case MissileGraphicID::BlizzardShard:
	case MissileGraphicID::FrozenOrb:
	case MissileGraphicID::IceArmorBreak:
	case MissileGraphicID::FrostArrow:
	case MissileGraphicID::FreezingBurst:
	// The early Orcl sheets.
	case MissileGraphicID::FistOfHeavensBolt:
	case MissileGraphicID::HolySpark:
	case MissileGraphicID::MagicArrowLight:
	case MissileGraphicID::GuidedArrowGold:
	case MissileGraphicID::HitFire:
	case MissileGraphicID::HitLightning:
	case MissileGraphicID::HitCold:
	case MissileGraphicID::Meteor:
	case MissileGraphicID::MeteorImpact:
	case MissileGraphicID::ThunderBolt:
	case MissileGraphicID::RiftPortalGold:
	case MissileGraphicID::RiftPortalPurple:
	case MissileGraphicID::TownPortalInTown:
	case MissileGraphicID::RiftPortalGoldInTown:
	case MissileGraphicID::RiftPortalPurpleInTown:
		return Glow::Yes;
	// Solid things, splashes and dark art.
	case MissileGraphicID::Arrow:
	case MissileGraphicID::Guardian:
	case MissileGraphicID::ManaShield:
	case MissileGraphicID::BloodHit:
	case MissileGraphicID::BoneHit:
	case MissileGraphicID::MetalHit:
	case MissileGraphicID::DoomSerpents:
	case MissileGraphicID::Golem:
	case MissileGraphicID::Spurt:
	case MissileGraphicID::StoneCurseShatter:
	case MissileGraphicID::Krull:
	case MissileGraphicID::Acid:
	case MissileGraphicID::AcidSplat:
	case MissileGraphicID::AcidPuddle:
	case MissileGraphicID::Etherealize:
	case MissileGraphicID::BoneSpirit:
	case MissileGraphicID::HorkSpawn:
	case MissileGraphicID::Reflect:
	case MissileGraphicID::IceArmorShell:
	case MissileGraphicID::BlessedHammerSpin:
	case MissileGraphicID::BlessedShieldSpin:
	case MissileGraphicID::WarcryRing:
	case MissileGraphicID::ThrownSword:
	case MissileGraphicID::ThrownAxe:
	case MissileGraphicID::AcidJavelin:
	case MissileGraphicID::AcidCloud:
	case MissileGraphicID::Grenade:
	case MissileGraphicID::BoneTooth:
	case MissileGraphicID::BoneSpear:
	case MissileGraphicID::BoneSpiritNecro:
	case MissileGraphicID::BoneHitNecro:
	case MissileGraphicID::BoneWall:
	case MissileGraphicID::BoneSpikes:
	case MissileGraphicID::BoneArmorShell:
	case MissileGraphicID::BoneStorm:
	case MissileGraphicID::PoisonBolt:
	case MissileGraphicID::CorpseExplosion:
	case MissileGraphicID::RaiseDead:
	case MissileGraphicID::CurseCast:
	case MissileGraphicID::IceGround:
		return Glow::No;
	default:
		return Glow::Unknown;
	}
}

/**
 * The later sheets (RfA-27 on) by their file names, worked out once a graphic. The dark words are asked first: a
 * "death_nova" is not light for its "nova", and smoke or dust drawn additively would vanish over black.
 */
bool NameGlows(MissileGraphicID graphic)
{
	static std::array<Glow, 256> cache {};
	const auto i = static_cast<size_t>(graphic);
	if (i >= cache.size() || graphic >= MissileGraphicID::None)
		return false;
	if (cache[i] != Glow::Unknown)
		return cache[i] == Glow::Yes;
	std::string name = GetMissileSpriteData(graphic).name;
	for (char &c : name)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	const auto has = [&](std::initializer_list<const char *> words) {
		for (const char *word : words) {
			if (name.find(word) != std::string::npos)
				return true;
		}
		return false;
	};
	bool glows = false;
	if (name.empty()) {
		glows = false;
	} else if (has({ "arrow", "javelin", "spear", "harpoon", "thrown", "grenade", "hammer", "shield", "bone", "blood", "poison",
	               "acid", "venom", "ashen", "dust", "smoke", "shadow", "dark", "unholy", "curse", "death", "dead", "corpse",
	               "raise", "stomp", "quake", "seismic", "earth", "prison", "offering", "mending", "dagger" })) {
		glows = false;
	} else {
		glows = has({ "fire", "flame", "ember", "burn", "infern", "meteor", "lava", "magma", "furnace", "funeral", "blaze",
		    "cinder", "immolat", "lightn", "storm", "thunder", "spark", "static", "shock", "conduit", "faraday", "volt",
		    "holy", "divine", "heaven", "smite", "sanct", "wrath", "judgment", "votive", "oath", "light", "flash", "radian",
		    "frost", "ice_", "glacial", "frozen", "freez", "blizz", "zero", "whiteout", "chill", "nova", "flare", "glow",
		    "wisp", "astral", "mantra", "serenity", "redemption", "chi_", "explo", "portal" });
	}
	cache[i] = glows ? Glow::Yes : Glow::No;
	return glows;
}

/** True when the missile wears sprites that are not its own graphic's: a charging monster, a creature bolt, a tumble. */
bool WearsBorrowedSprites(const Missile &missile)
{
	if (missile.oracoolTrn != nullptr)
		return true; // a recoloured item sprite (Missile::oracoolTrn)
	if ((static_cast<uint8_t>(missile._miAnimFlags) & static_cast<uint8_t>(MissileGraphicsFlags::MonsterOwned)) != 0)
		return true;
	if (missile._miAnimType >= MissileGraphicID::None)
		return true; // a charger's body (AddRhino's row has no graphic of its own)
	if (!missile._miAnimData)
		return false; // nothing drawn yet: the graphic decides (DrawMissilePrivate never gets here without one)
	if (missile.oracoolScalePercent != 100)
		return false; // ScaleMissile drew its own copy of the graphic's frames; the graphic decides
	const MissileFileData &file = GetMissileSpriteData(missile._miAnimType);
	if (!file.sprites)
		return true;
	const uint8_t *begin = file.sprites->isSheet() ? file.sprites->sheet().data() : file.sprites->list().data();
	const uint8_t *end = begin + file.sprites->dataSize();
	const uint8_t *at = missile._miAnimData->data();
	return at < begin || at >= end;
}

} // namespace

bool MissileArtGlows(const Missile &missile)
{
	if (WearsBorrowedSprites(missile))
		return false;
	const Glow fixed = GraphicGlow(missile._miAnimType);
	if (fixed != Glow::Unknown)
		return fixed == Glow::Yes;
	return NameGlows(missile._miAnimType);
}

bool MissileGlows(const Missile &missile)
{
	if (!*sgOptions.Oracool.glowingSpells || leveltype == DTYPE_TOWN)
		return false;
	return MissileArtGlows(missile);
}

uint8_t MissileGlowStrength(const Missile &missile)
{
	const int alpha = missile.oracoolAlpha >= 256 ? 256 : missile.oracoolAlpha;
	return static_cast<uint8_t>(SpellGlowStrength * alpha / 256);
}

const uint32_t *MissileGlowTable(const Missile &missile, int lightTableIndex)
{
	// The caster's unique recolour, as ClxDrawTRN would draw it (that path is unlit too).
	if (missile._miUniqTrans != 0 && missile._misource >= 0 && static_cast<size_t>(missile._misource) < MaxMonsters) {
		const uint8_t *trn = Monsters[missile._misource].uniqueMonsterTRN.get();
		if (trn != nullptr) {
			static std::array<uint32_t, 256> table;
			for (size_t i = 0; i < 256; i++)
				table[i] = PaletteRGB[trn[i]];
			return table.data();
		}
	}
	return LitPaletteTable(missile._miLightFlag ? lightTableIndex : 0);
}

} // namespace devilution::oracool

#ifdef _MSC_VER
#pragma optimize("", on)
#pragma runtime_checks("", restore)
#endif
