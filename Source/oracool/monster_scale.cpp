#include "oracool/monster_scale.h"

#include <memory>
#include <vector>

#include "dead.h"
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
	case MonsterSize::Normal:
		break;
	}
	return 100;
}

MonsterSize GetMonsterSize(const Monster &monster)
{
	return monster.lesserAffix == LesserUniqueAffix::Colossal ? MonsterSize::Colossal : MonsterSize::Normal;
}

const AnimStruct *GetScaledAnim(const Monster &monster, MonsterGraphic graphic)
{
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
