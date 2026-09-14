#include "oracool/decoy.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include <fmt/format.h>

#include "engine/assets.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/direction.hpp"
#include "engine/displacement.hpp"
#include "engine/load_cl2.hpp"
#include "engine/palette.h"
#include "engine/path.h"
#include "engine/render/clx_render.hpp"
#include "levels/gendung.h"
#include "player.h"
#include "playerdat.hpp"

namespace devilution::oracool {

namespace {

constexpr size_t AnimCount = 6;

/** @brief A set of hero sheets, owned, bound as monster animations. */
struct HeroSheets {
	bool active = false;
	std::array<std::unique_ptr<OwnedClxSpriteListOrSheet>, AnimCount> owned;
	std::array<AnimStruct, AnimCount> anims {};
};

std::array<HeroSheets, MAX_PLRS> Decoys;

/** @brief The Valkyrie's dress: heavy armour (ArmourChar 'h'), sword and shield. */
constexpr size_t ValkyrieArmour = 2;
constexpr PlayerWeaponGraphic ValkyrieWeapon = PlayerWeaponGraphic::SwordShield;

/**
 * @brief Every colour to one palette ramp by its brightness.
 *
 * The 16-colour ramps run light to dark from their base. @p lightest shifts the result toward the light end, so a
 * summon reads as a glow rather than a bruise, and never quite white. The monster draw path lights a translated
 * ordinary monster after translating it, so a dungeon summon still darkens in shadow.
 */
std::unique_ptr<uint8_t[]> RampTranslation(uint8_t ramp, int lightest)
{
	auto trn = std::make_unique<uint8_t[]>(256);
	for (int i = 0; i < 256; i++) {
		const SDL_Color &c = orig_palette[static_cast<size_t>(i)];
		const int luminance = (c.r * 30 + c.g * 59 + c.b * 11) / 100;
		const int shade = std::clamp(15 - lightest - luminance * (16 - lightest) / 255, 0, 15);
		trn[static_cast<size_t>(i)] = static_cast<uint8_t>(ramp + shade);
	}
	return trn;
}

/** @brief The Decoy's blue ghost: the blue ramp, two shades light. */
std::unique_ptr<uint8_t[]> BlueGhostTranslation()
{
	return RampTranslation(PAL16_BLUE, 2);
}

/** @brief The Valkyrie's gold: the yellow ramp, three shades light. */
std::unique_ptr<uint8_t[]> ValkyrieTranslation()
{
	return RampTranslation(PAL16_YELLOW, 3);
}

/** @brief Loads one sheet of @p cls in @p armour and @p weapon into @p set at @p graphic. False if the archive lacks it. */
bool LoadSheet(HeroSheets &set, HeroClass cls, size_t armour, MonsterGraphic graphic, const char *cel, PlayerWeaponGraphic weapon,
    uint16_t width, int frames)
{
	armour = std::min<size_t>(armour, ArmourChar.size() - 1);
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

/**
 * @brief Loads @p cls's sheets in @p armour and @p weapon into @p set - the town pair (stand, walk) or the dungeon
 * five (stand, walk, attack, hit, death). False, leaving @p set empty, if even the stand is missing.
 */
bool LoadHeroSheets(HeroSheets &set, HeroClass cls, size_t armour, PlayerWeaponGraphic weapon, bool town)
{
	set = HeroSheets {};
	const PlayerAnimData &frames = PlayersAnimData[static_cast<size_t>(cls)];
	const PlayerSpriteData &widths = PlayersSpriteData[static_cast<size_t>(cls)];
	// The stand is the one a double cannot do without; everything else falls back to it.
	if (!LoadSheet(set, cls, armour, MonsterGraphic::Stand, town ? "st" : "as", weapon, widths.stand, town ? frames.townIdleFrames : frames.idleFrames))
		return false;
	LoadSheet(set, cls, armour, MonsterGraphic::Walk, town ? "wl" : "aw", weapon, widths.walk, town ? frames.townWalkingFrames : frames.walkingFrames);
	if (!town) {
		LoadSheet(set, cls, armour, MonsterGraphic::Attack, "at", weapon,
		    weapon == PlayerWeaponGraphic::Bow ? widths.bow : widths.attack, AttackFrames(frames, weapon));
		LoadSheet(set, cls, armour, MonsterGraphic::GotHit, "ht", weapon, widths.swHit, frames.recoveryFrames);
		// The death sheet is only ever authored unarmed - see LoadPlrGFX.
		LoadSheet(set, cls, armour, MonsterGraphic::Death, "dt", PlayerWeaponGraphic::Unarmed, widths.death, frames.deathFrames);
	}
	for (MonsterGraphic graphic : { MonsterGraphic::Walk, MonsterGraphic::Attack, MonsterGraphic::GotHit, MonsterGraphic::Death, MonsterGraphic::Special }) {
		const auto i = static_cast<size_t>(graphic);
		if (!set.anims[i].sprites)
			set.anims[i] = set.anims[static_cast<size_t>(MonsterGraphic::Stand)];
	}
	set.active = true;
	return true;
}

/** @brief The Valkyrie's dress, or the nearest lighter armour the archive has. */
bool LoadValkyrieSheets(HeroSheets &set, bool town)
{
	for (size_t armour = ValkyrieArmour + 1; armour-- > 0;) {
		if (LoadHeroSheets(set, HeroClass::Rogue, armour, ValkyrieWeapon, town))
			return true;
	}
	return false;
}

// ---- the town companion ---------------------------------------------------------------------------------------

struct TownValkyrie {
	HeroSheets sheets;
	std::unique_ptr<uint8_t[]> trn;
	Point tile {};
	Point next {};
	Direction dir = Direction::South;
	int stepTick = 0; // 0 standing, else ticks into a step toward `next`
	int frame = 0;
	int frameTick = 0;
};

std::array<TownValkyrie, MAX_PLRS> TownValkyries;

/** @brief A tile step takes this many ticks - the town walk's eight frames, one a tick. */
constexpr int TownStepTicks = 8;
/** @brief She follows once her Rogue is further than this... */
constexpr int FollowDistance = 2;
/** @brief ...and simply reappears beside her past this (a waypoint, a portal back, a long run). */
constexpr int CatchUpDistance = 14;
constexpr int StandTicksPerFrame = 3;

bool TownTileFree(Point tile)
{
	if (!InDungeonBounds(tile) || !IsTileWalkable(tile) || IsTileOccupied(tile))
		return false;
	for (const TownValkyrie &other : TownValkyries) {
		if (other.sheets.active && (other.tile == tile || (other.stepTick > 0 && other.next == tile)))
			return false;
	}
	return true;
}

std::optional<Point> FreeTileNear(Point centre, int maxRadius)
{
	for (int radius = 0; radius <= maxRadius; radius++) {
		for (int dy = -radius; dy <= radius; dy++) {
			for (int dx = -radius; dx <= radius; dx++) {
				if (std::max(std::abs(dx), std::abs(dy)) != radius)
					continue;
				const Point tile = centre + Displacement { dx, dy };
				if (TownTileFree(tile))
					return tile;
			}
		}
	}
	return std::nullopt;
}

Direction Turned(Direction dir, int eighths)
{
	return static_cast<Direction>((static_cast<int>(dir) + eighths + 8) % 8);
}

const AnimStruct &TownAnim(const TownValkyrie &v)
{
	return v.sheets.anims[static_cast<size_t>(v.stepTick > 0 ? MonsterGraphic::Walk : MonsterGraphic::Stand)];
}

} // namespace

void MakeDecoy(const Player &player, Monster &golem)
{
	const size_t id = golem.getId();
	if (id >= MAX_PLRS)
		return;
	ClearDecoy(golem);
	const auto weapon = static_cast<PlayerWeaponGraphic>(std::min<int>(player._pgfxnum & 0xF, static_cast<int>(PlayerWeaponGraphic::Staff)));
	if (!LoadHeroSheets(Decoys[id], player._pClass, static_cast<size_t>(player._pgfxnum >> 4), weapon, /*town=*/false))
		return;
	golem.uniqueMonsterTRN = BlueGhostTranslation();
	golem.changeAnimationData(MonsterGraphic::Stand);
}

void MakeValkyrie(Monster &golem)
{
	const size_t id = golem.getId();
	if (id >= MAX_PLRS)
		return;
	ClearDecoy(golem);
	if (!LoadValkyrieSheets(Decoys[id], /*town=*/false))
		return;
	golem.uniqueMonsterTRN = ValkyrieTranslation();
	golem.changeAnimationData(MonsterGraphic::Stand);
}

void ClearDecoy(Monster &golem)
{
	const size_t id = golem.getId();
	if (id >= MAX_PLRS || !Decoys[id].active)
		return;
	Decoys[id] = HeroSheets {};
	golem.uniqueMonsterTRN = nullptr;
}

void ClearDecoys()
{
	for (HeroSheets &set : Decoys)
		set = HeroSheets {};
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

bool SummonTownValkyrie(const Player &player, Point target)
{
	const size_t id = player.getId();
	if (leveltype != DTYPE_TOWN || id >= MAX_PLRS)
		return false;
	TownValkyrie &v = TownValkyries[id];
	v = TownValkyrie {}; // a second cast replaces the first
	const std::optional<Point> spot = FreeTileNear(target, 5);
	if (!spot || !LoadValkyrieSheets(v.sheets, /*town=*/true)) {
		v = TownValkyrie {};
		return false;
	}
	v.trn = ValkyrieTranslation();
	v.tile = *spot;
	v.dir = *spot == player.position.tile ? Direction::South : GetDirection(*spot, player.position.tile);
	return true;
}

bool HasTownValkyrie(size_t playerId)
{
	return playerId < MAX_PLRS && TownValkyries[playerId].sheets.active;
}

void ClearTownValkyries()
{
	for (TownValkyrie &v : TownValkyries)
		v = TownValkyrie {};
}

void ProcessTownValkyries()
{
	if (leveltype != DTYPE_TOWN)
		return;
	for (size_t id = 0; id < MAX_PLRS; id++) {
		TownValkyrie &v = TownValkyries[id];
		if (!v.sheets.active)
			continue;
		if (id >= Players.size() || !Players[id].plractive) {
			v = TownValkyrie {};
			continue;
		}
		const Point owner = Players[id].position.tile;

		if (v.stepTick > 0) {
			const int frames = std::max<int>(TownAnim(v).frames, 1);
			v.frame = (v.stepTick * frames / TownStepTicks) % frames;
			if (++v.stepTick > TownStepTicks) {
				v.tile = v.next;
				v.stepTick = 0;
				v.frame = 0;
				v.frameTick = 0;
			}
			continue;
		}

		const int distance = v.tile.WalkingDistance(owner);
		if (distance > CatchUpDistance) {
			if (const std::optional<Point> spot = FreeTileNear(owner, 3))
				v.tile = *spot;
		} else if (distance > FollowDistance) {
			const Direction toward = GetDirection(v.tile, owner);
			for (int turn : { 0, 1, -1, 2, -2 }) {
				const Direction dir = Turned(toward, turn);
				const Point step = v.tile + dir;
				if (TownTileFree(step)) {
					v.dir = dir;
					v.next = step;
					v.stepTick = 1;
					v.frame = 0;
					break;
				}
			}
			if (v.stepTick > 0)
				continue;
		} else if (distance > 0) {
			v.dir = GetDirection(v.tile, owner); // close enough: she turns to face her Rogue
		}

		if (++v.frameTick >= StandTicksPerFrame) {
			v.frameTick = 0;
			v.frame = (v.frame + 1) % std::max<int>(TownAnim(v).frames, 1);
		}
	}
}

void DrawTownValkyries(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	if (leveltype != DTYPE_TOWN)
		return;
	for (const TownValkyrie &v : TownValkyries) {
		if (!v.sheets.active || v.tile != tilePosition)
			continue;
		const AnimStruct &anim = TownAnim(v);
		if (!anim.sprites)
			continue;
		const ClxSpriteList list = anim.sprites->isSheet() ? anim.sprites->sheet()[static_cast<size_t>(v.dir)] : anim.sprites->list();
		if (list.numSprites() == 0)
			continue;
		const ClxSprite sprite = list[static_cast<size_t>(std::clamp<int>(v.frame, 0, static_cast<int>(list.numSprites()) - 1))];
		Point position = targetBufferPosition;
		if (v.stepTick > 0) {
			// Part of the way to the next tile, in screen pixels.
			const Displacement step = Displacement(v.dir).worldToScreen();
			position += Displacement { step.deltaX * v.stepTick / TownStepTicks, step.deltaY * v.stepTick / TownStepTicks };
		}
		// Centred on the tile the way a player's sprite is: half of what the sprite is wider than a 64px tile.
		position.x -= (static_cast<int>(sprite.width()) - 64) / 2;
		ClxDrawTRN(out, position, sprite, v.trn.get());
	}
}

} // namespace devilution::oracool
