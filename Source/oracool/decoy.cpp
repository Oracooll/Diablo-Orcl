#include "oracool/decoy.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>

#include <fmt/format.h>

#include "engine/assets.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/load_cl2.hpp"
#include "engine/palette.h"
#include "levels/gendung.h"
#include "player.h"
#include "playerdat.hpp"

namespace devilution::oracool {

namespace {

constexpr size_t AnimCount = 6;

struct DecoySet {
	bool active = false;
	std::array<std::unique_ptr<OwnedClxSpriteListOrSheet>, AnimCount> owned;
	std::array<AnimStruct, AnimCount> anims {};
};

std::array<DecoySet, MAX_PLRS> Decoys;

/**
 * @brief The blue ghost: every colour to the palette's blue ramp by its brightness.
 *
 * The ramp runs light to dark from PAL16_BLUE. Shifted two shades toward the light end, so the double reads as a
 * glow rather than a bruise, and never quite white - the lightest shade stays for its highlights. The monster
 * draw path lights a translated ordinary monster after translating it, so the ghost still darkens in shadow.
 */
std::unique_ptr<uint8_t[]> BlueGhostTranslation()
{
	auto trn = std::make_unique<uint8_t[]>(256);
	for (int i = 0; i < 256; i++) {
		const SDL_Color &c = orig_palette[static_cast<size_t>(i)];
		const int luminance = (c.r * 30 + c.g * 59 + c.b * 11) / 100;
		const int shade = std::clamp(13 - luminance * 14 / 255, 0, 15);
		trn[static_cast<size_t>(i)] = static_cast<uint8_t>(PAL16_BLUE + shade);
	}
	return trn;
}

/** @brief Loads one of @p player's sheets into @p set at @p graphic. False if the archive has no such sheet. */
bool LoadSheet(DecoySet &set, const Player &player, MonsterGraphic graphic, const char *cel, PlayerWeaponGraphic weapon,
    uint16_t width, int frames)
{
	const HeroClass cls = player._pClass;
	const size_t armour = std::min<size_t>(player._pgfxnum >> 4, ArmourChar.size() - 1);
	const char prefix[3] = { CharChar[static_cast<size_t>(cls)], ArmourChar[armour], WepChar[static_cast<size_t>(weapon)] };
	const std::string path = fmt::format(R"(plrgfx\{0}\{1}\{1}{2})", PlayersData[static_cast<size_t>(cls)].classPath,
	    string_view(prefix, 3), cel);
	if (!FindAsset((path + DEVILUTIONX_CL2_EXT).c_str()).ok())
		return false;
	OptionalOwnedClxSpriteSheet sheet = LoadCl2Sheet(path.c_str(), width);
	if (!sheet)
		return false;
	const auto i = static_cast<size_t>(graphic);
	set.owned[i] = std::make_unique<OwnedClxSpriteListOrSheet>(std::move(*sheet));
	AnimStruct &anim = set.anims[i];
	anim.sprites = OptionalClxSpriteListOrSheet { *set.owned[i] };
	anim.width = width;
	anim.frames = static_cast<int8_t>(frames);
	anim.rate = 1;
	return true;
}

int AttackFrames(const PlayerAnimData &data, PlayerWeaponGraphic weapon)
{
	switch (weapon) {
	case PlayerWeaponGraphic::Unarmed: return data.unarmedFrames;
	case PlayerWeaponGraphic::UnarmedShield: return data.unarmedShieldFrames;
	case PlayerWeaponGraphic::Sword: return data.swordFrames;
	case PlayerWeaponGraphic::SwordShield: return data.swordShieldFrames;
	case PlayerWeaponGraphic::Bow: return data.bowFrames;
	case PlayerWeaponGraphic::Axe: return data.axeFrames;
	case PlayerWeaponGraphic::Mace: return data.maceFrames;
	case PlayerWeaponGraphic::MaceShield: return data.maceShieldFrames;
	case PlayerWeaponGraphic::Staff: return data.staffFrames;
	}
	return data.unarmedFrames;
}

} // namespace

void MakeDecoy(const Player &player, Monster &golem)
{
	const size_t id = golem.getId();
	if (id >= MAX_PLRS)
		return;
	ClearDecoy(golem);
	DecoySet &set = Decoys[id];

	const auto cls = static_cast<size_t>(player._pClass);
	const PlayerAnimData &frames = PlayersAnimData[cls];
	const PlayerSpriteData &widths = PlayersSpriteData[cls];
	const auto weapon = static_cast<PlayerWeaponGraphic>(std::min<int>(player._pgfxnum & 0xF, static_cast<int>(PlayerWeaponGraphic::Staff)));
	const bool town = leveltype == DTYPE_TOWN;

	// The stand is the one the decoy cannot do without; everything else falls back to it.
	if (!LoadSheet(set, player, MonsterGraphic::Stand, town ? "st" : "as", weapon, widths.stand, town ? frames.townIdleFrames : frames.idleFrames))
		return;
	const auto orStand = [&set](MonsterGraphic graphic) {
		const auto i = static_cast<size_t>(graphic);
		if (!set.anims[i].sprites)
			set.anims[i] = set.anims[static_cast<size_t>(MonsterGraphic::Stand)];
	};
	LoadSheet(set, player, MonsterGraphic::Walk, town ? "wl" : "aw", weapon, widths.walk, town ? frames.townWalkingFrames : frames.walkingFrames);
	if (!town) {
		LoadSheet(set, player, MonsterGraphic::Attack, "at", weapon,
		    weapon == PlayerWeaponGraphic::Bow ? widths.bow : widths.attack, AttackFrames(frames, weapon));
		LoadSheet(set, player, MonsterGraphic::GotHit, "ht", weapon, widths.swHit, frames.recoveryFrames);
	}
	// The death sheet is only ever authored unarmed - see LoadPlrGFX.
	LoadSheet(set, player, MonsterGraphic::Death, "dt", PlayerWeaponGraphic::Unarmed, widths.death, frames.deathFrames);
	for (MonsterGraphic graphic : { MonsterGraphic::Walk, MonsterGraphic::Attack, MonsterGraphic::GotHit, MonsterGraphic::Death, MonsterGraphic::Special })
		orStand(graphic);

	set.active = true;
	golem.uniqueMonsterTRN = BlueGhostTranslation();
	golem.changeAnimationData(MonsterGraphic::Stand);
}

void ClearDecoy(Monster &golem)
{
	const size_t id = golem.getId();
	if (id >= MAX_PLRS || !Decoys[id].active)
		return;
	Decoys[id] = DecoySet {};
	golem.uniqueMonsterTRN = nullptr;
}

void ClearDecoys()
{
	for (DecoySet &set : Decoys)
		set = DecoySet {};
}

bool IsDecoy(const Monster &monster)
{
	if (&monster < &Monsters[0] || &monster >= &Monsters[0] + MAX_PLRS)
		return false;
	return Decoys[monster.getId()].active;
}

const AnimStruct *GetDecoyAnim(const Monster &monster, MonsterGraphic graphic)
{
	if (!IsDecoy(monster))
		return nullptr;
	const AnimStruct &anim = Decoys[monster.getId()].anims[static_cast<size_t>(graphic)];
	return anim.sprites ? &anim : nullptr;
}

} // namespace devilution::oracool
