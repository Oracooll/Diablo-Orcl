#include "oracool/monster_scale.h"

#include <memory>
#include <vector>

#include "dead.h"
#include "diablo.h"
#include "levels/gendung.h"
#include "oracool/decoy.h"
#include "oracool/sprite_scale.h"

namespace devilution::oracool {

namespace {

/**
 * @brief One monster type at one size: the six animations plus the corpse, all owned.
 *
 * The AnimStructs alongside carry views into `owned` with the SCALED width, because the width is
 * what CalculateWidth2 centres on and a scaled sprite centred on its unscaled width sits visibly
 * off its own tile.
 */
struct ScaledEntry {
	size_t typeIndex;
	unsigned percent;
	std::unique_ptr<OwnedClxSpriteListOrSheet> owned[6];
	AnimStruct anims[6];
	std::unique_ptr<OwnedClxSpriteListOrSheet> corpse;
};

std::vector<std::unique_ptr<ScaledEntry>> Cache;

/** @brief Scales one sprite source, list or sheet, keeping whichever shape it had. */
OwnedClxSpriteListOrSheet ScaleListOrSheet(ClxSpriteListOrSheet src, unsigned percent)
{
	if (src.isSheet())
		return OwnedClxSpriteListOrSheet { ScaleClxSheet(src.sheet(), percent) };
	return OwnedClxSpriteListOrSheet { ScaleClxList(src.list(), percent) };
}

/** @brief The cache row for (@p typeIndex, @p percent), built on first ask. */
ScaledEntry &EntryFor(size_t typeIndex, unsigned percent)
{
	for (const std::unique_ptr<ScaledEntry> &entry : Cache) {
		if (entry->typeIndex == typeIndex && entry->percent == percent)
			return *entry;
	}

	auto entry = std::make_unique<ScaledEntry>();
	entry->typeIndex = typeIndex;
	entry->percent = percent;
	Cache.push_back(std::move(entry));
	return *Cache.back();
}

/**
 * @brief The scaled version of one animation, built on first ask, or nullptr if it cannot be.
 *
 * Per-animation and lazy rather than all six at once, and that is a correctness point rather than a
 * thrift: a monster binds its Stand animation during placement, which can precede the load of the
 * sprites some other animation will use. Building all six eagerly would cache an EMPTY entry for
 * whatever had not loaded yet, and a Colossal champion would be invisible the moment it attacked.
 * An animation that is not ready simply is not scaled yet, and the caller falls back to the shared
 * data - which is what it would have drawn anyway.
 */
const AnimStruct *ScaledAnimIn(ScaledEntry &entry, MonsterGraphic graphic)
{
	const auto i = static_cast<size_t>(graphic);
	const AnimStruct &src = LevelMonsterTypes[entry.typeIndex].anims[i];
	// Not loaded, or a monster that simply has no special animation.
	if (!src.sprites)
		return nullptr;
	if (!entry.owned[i]) {
		entry.owned[i] = std::make_unique<OwnedClxSpriteListOrSheet>(ScaleListOrSheet(*src.sprites, entry.percent));
		entry.anims[i] = src;
		entry.anims[i].sprites = OptionalClxSpriteListOrSheet { *entry.owned[i] };
		// The width CalculateWidth2 centres on. A scaled sprite centred on its unscaled width sits
		// visibly off its own tile.
		entry.anims[i].width = static_cast<uint16_t>(src.width * entry.percent / 100);
	}
	return &entry.anims[i];
}

} // namespace

unsigned MonsterSizePercent(MonsterSize size)
{
	switch (size) {
	case MonsterSize::Colossal:
		// 140%, which is where the sprite stops reading as "the same monster, closer" and starts
		// reading as a bigger one. Past about 160% the baked-in shadow separates from the feet.
		return 140;
	case MonsterSize::Giant:
		// Deliberately short of Colossal's 140. A champion has to be the biggest thing in the room,
		// and two large sizes that read alike would make the affix mean less rather than the giant
		// mean more.
		return 120;
	case MonsterSize::Runt:
		// Small enough to read instantly, not so small the monster stops covering its own tile -
		// below about 70% the sprite sits inside the floor diamond and looks like a dropped item.
		return 75;
	case MonsterSize::Normal:
		break;
	}
	return 100;
}

MonsterSize OrdinaryMonsterSize(uint32_t levelSeed, size_t typeIndex, size_t monsterId)
{
	// A fixed-point mix, not the engine's LCG: this must be a pure function of values that are
	// already saved, so that a monster is the same size after a reload as it was before it. Nothing
	// here touches a stream, which also means it can never shift a deterministic replay.
	const auto mix = [](uint32_t a, uint32_t b) {
		uint32_t x = a * 0x9E3779B9u + b * 0x85EBCA6Bu;
		x ^= x >> 16;
		x *= 0x7FEB352Du;
		x ^= x >> 15;
		return x;
	};

	// Does this type have an odd size on this floor at all? Half of them do not - see the header on
	// why this is decided per type rather than per monster.
	const uint32_t typeRoll = mix(levelSeed, static_cast<uint32_t>(typeIndex) + 1) & 3u;
	MonsterSize variant;
	if (typeRoll == 0)
		variant = MonsterSize::Runt;
	else if (typeRoll == 1)
		variant = MonsterSize::Giant;
	else
		return MonsterSize::Normal;

	// And is THIS one of them? A quarter of the type's individuals, so an odd-sized monster reads as
	// an individual rather than as a reskin of the whole floor.
	const uint32_t memberRoll = mix(levelSeed ^ 0xA5A5A5A5u, static_cast<uint32_t>(monsterId) + 1) & 3u;
	return memberRoll == 0 ? variant : MonsterSize::Normal;
}

MonsterSize GetMonsterSize(const Monster &monster)
{
	// A boss is Colossal too, and for the same reason the Colossal affix is: the silhouette is the
	// promise. It has to be the biggest thing in the room before the player reads its name.
	if (monster.lesserAffix == LesserUniqueAffix::Colossal || monster.lesserAffix == LesserUniqueAffix::Dread)
		return MonsterSize::Colossal;
	// A hand-authored unique and an affixed champion both have a silhouette somebody chose. Rolling
	// a size on top of that would overwrite a decision with a coin flip, and would put a Colossal
	// champion beside a giant of its own kind that reads almost the same.
	if (monster.isUnique() || monster.lesserAffix != LesserUniqueAffix::None)
		return MonsterSize::Normal;
	return OrdinaryMonsterSize(glSeedTbl[currlevel], monster.levelType, monster.getId());
}

const AnimStruct *GetScaledAnim(const Monster &monster, MonsterGraphic graphic)
{
	// The Rogue's Decoy wears her sheets (oracool/decoy.h) - asked here because this is where both binders ask.
	if (const AnimStruct *decoy = GetDecoyAnim(monster, graphic))
		return decoy;
	const unsigned percent = MonsterSizePercent(GetMonsterSize(monster));
	if (percent == 100)
		return nullptr;
	return ScaledAnimIn(EntryFor(monster.levelType, percent), graphic);
}

OptionalClxSpriteListOrSheet GetScaledCorpse(const Monster &monster)
{
	const unsigned percent = MonsterSizePercent(GetMonsterSize(monster));
	if (percent == 100)
		return std::nullopt;
	const int corpseId = monster.type().corpseId;
	if (corpseId < 1)
		return std::nullopt;

	ScaledEntry &entry = EntryFor(monster.levelType, percent);
	if (!entry.corpse) {
		// Built lazily and separately from the six animations: most champions are killed exactly
		// once, and a floor that never sees a Colossal die never pays for this. No "already tried"
		// flag, for the same reason the animations have none - an unloaded source must stay
		// retryable rather than cache a miss forever.
		const Corpse &corpse = Corpses[corpseId - 1];
		if (!corpse.sprites)
			return std::nullopt;
		entry.corpse = std::make_unique<OwnedClxSpriteListOrSheet>(ScaleListOrSheet(*corpse.sprites, percent));
	}
	return OptionalClxSpriteListOrSheet { *entry.corpse };
}

void ClearMonsterScaleCache()
{
	// Unbind BEFORE freeing. A sized monster's animInfo holds a raw view into this cache, and
	// dropping the cache under it would leave a dangling pointer rather than an empty optional -
	// which no null check anywhere could catch. Clearing the binding first means the worst case is
	// a monster that draws nothing until it rebinds, which the draw paths handle and log.
	if (!Cache.empty()) {
		for (size_t i = 0; i < MaxMonsters; i++) {
			Monster &monster = Monsters[i];
			if (GetMonsterSize(monster) != MonsterSize::Normal)
				monster.animInfo.sprites = std::nullopt;
		}
	}
	Cache.clear();
}

} // namespace devilution::oracool
