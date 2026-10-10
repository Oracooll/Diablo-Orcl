#include "oracool/monster_scale.h"

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

#include "dead.h"
#include "diablo.h"
#include "levels/gendung.h"
#include "monster.h"
#include "oracool/companion.h"
#include "oracool/minions.h" // GetMinionAnim: the Fire Golem's run sheet
#include "oracool/rift_bosses.h" // RiftBossSizePercent: a rift boss's own size, up to 250%
#include "oracool/sprite_scale.h"
#include "player.h"

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
	/** Handed to the worker and not back yet (user, 2026-10-10). The unscaled sprites stand in meanwhile. */
	bool pending[6] = {};
	bool corpsePending = false;
};

std::vector<std::unique_ptr<ScaledEntry>> Cache;

/** @brief Scales one sprite source, list or sheet, keeping whichever shape it had. Pure: safe on the worker. */
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

// =================================================================================================================
// The worker (user, 2026-10-10: "never stall a frame")
//
// A sized sheet used to be built the first time a sized monster walked, attacked, flinched or died - six animations of
// eight directions, decoded, resampled and re-encoded inside one game tick, mid-fight. Now LoadGameLevel queues every
// size the floor uses, nearest monster first (PrebuildMonsterSizes), a worker builds them while the unscaled sprites
// stand in, and the main thread installs each one at the next tick (PumpMonsterScale) and rebinds the monsters still
// wearing the stand-in.
//
// Thread safety: the worker never touches game state. Each job carries its OWN COPY of the source sprite data, taken on
// the main thread, so FreeMonsters, a level change or a type loaded mid-floor cannot free or move what the worker is
// reading. ScaleClxList and SurfaceToClx are pure (an 8-bit OwnedSurface and plain buffers - the sprite mixer's worker
// runs the same pair). A level change bumps Generation and empties the queue; the result of a job already in hand is
// dropped on delivery by its generation. Everything the two threads share sits behind JobMutex.
// =================================================================================================================

/** The corpse's slot in a job, after the six animations. */
constexpr int CorpseSlot = 6;

struct ScaleJob {
	uint32_t generation;
	size_t typeIndex;
	unsigned percent;
	int slot;
	/** Lower first: the nearest monster's distance times eight, plus the animation's place in BuildOrder. */
	int priority;
	uint32_t sequence;
	std::unique_ptr<OwnedClxSpriteListOrSheet> source;
};

struct ScaleResult {
	uint32_t generation;
	size_t typeIndex;
	unsigned percent;
	int slot;
	std::unique_ptr<OwnedClxSpriteListOrSheet> scaled;
};

enum class WorkerState : uint8_t {
	NotStarted,
	Running,
	/** The thread could not be made: everything is built on the spot, as before. */
	Unavailable,
};

std::mutex JobMutex; // guards Pending, Results and WorkerStop
std::condition_variable JobWake;
std::vector<std::unique_ptr<ScaleJob>> Pending;
std::deque<ScaleResult> Results;
bool WorkerStop = false;
// Main thread only, all three.
WorkerState Worker = WorkerState::NotStarted;
uint32_t Generation = 1;
uint32_t NextSequence = 0;

/** Joined on destruction, as the sprite mixer's is: a joinable static std::thread at exit is std::terminate. */
struct ScaleWorkerHolder {
	std::thread thread;
	~ScaleWorkerHolder()
	{
		{
			const std::lock_guard<std::mutex> lock(JobMutex);
			WorkerStop = true;
			Pending.clear();
		}
		JobWake.notify_all();
		if (thread.joinable())
			thread.join();
	}
};
ScaleWorkerHolder ScaleWorkerThread;

void ScaleWorkerMain()
{
	for (;;) {
		std::unique_ptr<ScaleJob> job;
		{
			std::unique_lock<std::mutex> lock(JobMutex);
			JobWake.wait(lock, [] { return WorkerStop || !Pending.empty(); });
			if (WorkerStop)
				return;
			// A few dozen jobs at most: a scan beats keeping a heap sorted while priorities are revised.
			auto best = Pending.begin();
			for (auto it = Pending.begin(); it != Pending.end(); ++it) {
				if ((*it)->priority < (*best)->priority || ((*it)->priority == (*best)->priority && (*it)->sequence < (*best)->sequence))
					best = it;
			}
			job = std::move(*best);
			Pending.erase(best);
		}
		// An exception leaving this thread would end the game (std::terminate): a sheet that cannot be built - a 250%
		// rift boss is six times its source - comes back empty and the type keeps its stand-in (round 1 audit).
		std::unique_ptr<OwnedClxSpriteListOrSheet> scaled;
		try {
			scaled = std::make_unique<OwnedClxSpriteListOrSheet>(ScaleListOrSheet(*job->source, job->percent));
		} catch (const std::exception &) {
			scaled = nullptr;
		}
		job->source = nullptr;
		const std::lock_guard<std::mutex> lock(JobMutex);
		Results.push_back({ job->generation, job->typeIndex, job->percent, job->slot, std::move(scaled) });
	}
}

/** @brief Whether background building is on: not in a headless run (the tests want the answer now), and a thread. */
bool WorkerUsable()
{
	if (HeadlessMode || Worker == WorkerState::Unavailable)
		return false;
	if (Worker == WorkerState::NotStarted) {
		try {
			ScaleWorkerThread.thread = std::thread(ScaleWorkerMain);
			Worker = WorkerState::Running;
		} catch (const std::system_error &) {
			Worker = WorkerState::Unavailable;
			return false;
		}
	}
	return true;
}

/** @brief A copy of @p src the worker can own: the level may free the original while it reads. */
std::unique_ptr<OwnedClxSpriteListOrSheet> CopyOf(ClxSpriteListOrSheet src)
{
	const size_t size = src.dataSize();
	std::unique_ptr<uint8_t[]> data { new uint8_t[size] };
	std::memcpy(data.get(), src.isSheet() ? src.sheet().data() : src.list().data(), size);
	return std::make_unique<OwnedClxSpriteListOrSheet>(std::move(data), src.isSheet() ? src.sheet().numLists() : static_cast<uint16_t>(0));
}

/** @brief The source of @p slot of @p entry: one of the type's six animations, or its corpse. Null if not loaded. */
OptionalClxSpriteListOrSheet SourceOf(const ScaledEntry &entry, int slot)
{
	if (entry.typeIndex >= LevelMonsterTypeCount)
		return std::nullopt;
	const CMonster &type = LevelMonsterTypes[entry.typeIndex];
	if (slot == CorpseSlot) {
		if (type.corpseId < 1 || static_cast<size_t>(type.corpseId) > MaxCorpses)
			return std::nullopt;
		return Corpses[type.corpseId - 1].sprites;
	}
	return type.anims[slot].sprites;
}

/**
 * @brief Hands @p slot of @p entry to the worker, or revises its priority if it is already there - to @p priority when
 * @p exact (the level-load scan, which knows where everything stands), else to the more urgent of the two. False when
 * there is no worker (or nothing to scale) - the caller then builds it on the spot.
 */
bool QueueScale(ScaledEntry &entry, int slot, int priority, bool exact = false)
{
	bool &pending = slot == CorpseSlot ? entry.corpsePending : entry.pending[slot];
	if (pending) {
		const std::lock_guard<std::mutex> lock(JobMutex);
		for (const std::unique_ptr<ScaleJob> &job : Pending) {
			if (job->typeIndex == entry.typeIndex && job->percent == entry.percent && job->slot == slot)
				job->priority = exact ? priority : std::min(job->priority, priority);
		}
		return true;
	}
	if (!WorkerUsable())
		return false;
	const OptionalClxSpriteListOrSheet src = SourceOf(entry, slot);
	if (!src)
		return false;
	auto job = std::make_unique<ScaleJob>();
	job->generation = Generation;
	job->typeIndex = entry.typeIndex;
	job->percent = entry.percent;
	job->slot = slot;
	job->priority = priority;
	job->sequence = NextSequence++;
	job->source = CopyOf(*src);
	{
		const std::lock_guard<std::mutex> lock(JobMutex);
		Pending.push_back(std::move(job));
	}
	pending = true;
	JobWake.notify_one();
	return true;
}

/** @brief Binds @p owned as animation @p i of @p entry, from the type's own @p src. */
void InstallAnim(ScaledEntry &entry, size_t i, const AnimStruct &src, std::unique_ptr<OwnedClxSpriteListOrSheet> owned)
{
	entry.owned[i] = std::move(owned);
	entry.anims[i] = src;
	entry.anims[i].sprites = OptionalClxSpriteListOrSheet { *entry.owned[i] };
	// The width CalculateWidth2 centres on. A scaled sprite centred on its unscaled width sits
	// visibly off its own tile.
	entry.anims[i].width = static_cast<uint16_t>(src.width * entry.percent / 100);
}

/** The order a type's animations are built in: what a monster shows first, first. */
constexpr MonsterGraphic BuildOrder[] = { MonsterGraphic::Stand, MonsterGraphic::Walk, MonsterGraphic::Attack,
	MonsterGraphic::GotHit, MonsterGraphic::Death, MonsterGraphic::Special };

int RankInBuildOrder(MonsterGraphic graphic)
{
	for (int k = 0; k < 6; k++) {
		if (BuildOrder[k] == graphic)
			return k;
	}
	return 5;
}

int DistanceToHero(const Monster &monster)
{
	if (MyPlayer == nullptr)
		return 0;
	const Point hero = MyPlayer->position.tile;
	return std::max(std::abs(monster.position.tile.x - hero.x), std::abs(monster.position.tile.y - hero.y));
}

/**
 * @brief The scaled version of one animation, or nullptr while it is not there.
 *
 * Per-animation and lazy rather than all six at once, and that is a correctness point rather than a
 * thrift: a monster binds its Stand animation during placement, which can precede the load of the
 * sprites some other animation will use. Building all six eagerly would cache an EMPTY entry for
 * whatever had not loaded yet, and a Colossal champion would be invisible the moment it attacked.
 * An animation that is not ready simply is not scaled yet, and the caller falls back to the shared
 * data - which is what it would have drawn anyway.
 *
 * Since 2026-10-10 (user) "not ready" includes "on the worker": the shared data stands in until PumpMonsterScale
 * installs the sheet and rebinds. Built here and now only when there is no worker.
 */
const AnimStruct *ScaledAnimIn(ScaledEntry &entry, MonsterGraphic graphic, int priority, bool exactPriority = false)
{
	const auto i = static_cast<size_t>(graphic);
	const AnimStruct &src = LevelMonsterTypes[entry.typeIndex].anims[i];
	// Not loaded, or a monster that simply has no special animation.
	if (!src.sprites)
		return nullptr;
	if (entry.owned[i])
		return &entry.anims[i];
	// A revisit's throwaway population (InitMonsters before LoadLevel, lights suppressed) is discarded at once: queueing
	// its sheets would only put jobs, prioritised from the last floor, ahead of the real ones (round 1 audit).
	if (SuppressMonsterLights && WorkerUsable())
		return nullptr;
	if (QueueScale(entry, static_cast<int>(i), priority, exactPriority))
		return nullptr;
	InstallAnim(entry, i, src, std::make_unique<OwnedClxSpriteListOrSheet>(ScaleListOrSheet(*src.sprites, entry.percent)));
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
	// Diablo (and the other Never types) and the hero's own golems and army keep their size, as VariantOf keeps them
	// plain: Diablo stood at 75% or 120% about one game in eight (round 11 audit, v1.12.236).
	if (monster.isPlayerMinion() || monster.data().availability == MonsterAvailability::Never)
		return MonsterSize::Normal;
	return OrdinaryMonsterSize(glSeedTbl[currlevel], monster.levelType, monster.getId());
}

/** @brief The size @p monster is drawn at, in percent: a rift boss's own (oracool/rift_bosses.h), else its MonsterSize's. */
unsigned DrawPercentOf(const Monster &monster)
{
	if (const unsigned boss = RiftBossSizePercent(monster); boss != 0)
		return boss;
	return MonsterSizePercent(GetMonsterSize(monster));
}

const AnimStruct *GetScaledAnim(const Monster &monster, MonsterGraphic graphic)
{
	// A companion wears its hero sheets (oracool/companion.h) - asked here because this is where both binders ask.
	if (const AnimStruct *companion = GetCompanionAnim(monster, graphic))
		return companion;
	// And the Necromancer's Fire Golem the Elemental's run sheet (oracool/minions.h, v1.12.364).
	if (const AnimStruct *minion = GetMinionAnim(monster, graphic))
		return minion;
	const unsigned percent = DrawPercentOf(monster);
	if (percent == 100)
		return nullptr;
	return ScaledAnimIn(EntryFor(monster.levelType, percent), graphic, DistanceToHero(monster) * 8 + RankInBuildOrder(graphic));
}

OptionalClxSpriteListOrSheet GetScaledCorpse(const Monster &monster)
{
	const unsigned percent = DrawPercentOf(monster);
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
		//
		// Built HERE even with a worker (user, 2026-10-10): a body keeps the sprites it was laid with for good, so a
		// stand-in would be a wrong-sized corpse for the rest of the floor - and a corpse is one sprite a facing, cheap.
		// PrebuildMonsterSizes queues it ahead, so this is the rare case; a job still in hand is dropped on delivery.
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
			if (DrawPercentOf(monster) != 100)
				monster.animInfo.sprites = std::nullopt;
		}
	}
	Cache.clear();
	// And the worker's queue (user, 2026-10-10): its jobs are for sprites about to go. A job in hand finishes on its own
	// copy and is dropped on delivery, by its generation.
	Generation++;
	{
		const std::lock_guard<std::mutex> lock(JobMutex);
		Pending.clear();
		Results.clear();
	}
}

namespace {

/**
 * @brief Moves every monster of @p entry's type and size that wears the unscaled stand-in for @p graphic onto the
 * scaled sheet just installed. Same frame, same frame count (scaling keeps the frames): only the pictures change.
 */
void RebindStandIns(const ScaledEntry &entry, size_t graphic)
{
	const AnimStruct &src = LevelMonsterTypes[entry.typeIndex].anims[graphic];
	const AnimStruct &scaled = entry.anims[graphic];
	if (!src.sprites || !scaled.sprites)
		return;
	for (size_t k = 0; k < ActiveMonsterCount; k++) {
		Monster &monster = Monsters[ActiveMonsters[k]];
		if (monster.levelType != entry.typeIndex || !monster.animInfo.sprites)
			continue;
		if (DrawPercentOf(monster) != entry.percent)
			continue;
		for (size_t d = 0; d < 8; d++) {
			const OptionalClxSpriteList standIn = src.spritesForDirection(static_cast<Direction>(d));
			if (!standIn || standIn->data() != monster.animInfo.sprites->data())
				continue;
			monster.animInfo.sprites = scaled.spritesForDirection(static_cast<Direction>(d));
			break;
		}
	}
}

} // namespace

void PumpMonsterScale()
{
	std::deque<ScaleResult> taken;
	{
		const std::lock_guard<std::mutex> lock(JobMutex);
		if (Results.empty())
			return;
		taken.swap(Results);
	}
	for (ScaleResult &result : taken) {
		if (result.generation != Generation)
			continue; // built for a level that is gone
		ScaledEntry *entry = nullptr;
		for (const std::unique_ptr<ScaledEntry> &candidate : Cache) {
			if (candidate->typeIndex == result.typeIndex && candidate->percent == result.percent)
				entry = candidate.get();
		}
		if (entry == nullptr || result.typeIndex >= LevelMonsterTypeCount)
			continue;
		if (result.scaled == nullptr)
			continue; // the worker could not build it: left pending, so it is never asked again and the stand-in stays
		if (result.slot == CorpseSlot) {
			entry->corpsePending = false;
			if (!entry->corpse)
				entry->corpse = std::move(result.scaled);
			continue;
		}
		const auto i = static_cast<size_t>(result.slot);
		entry->pending[i] = false;
		const AnimStruct &src = LevelMonsterTypes[result.typeIndex].anims[i];
		if (entry->owned[i] || !src.sprites)
			continue;
		InstallAnim(*entry, i, src, std::move(result.scaled));
		RebindStandIns(*entry, i);
	}
}

void PrebuildMonsterSizes()
{
	if (HeadlessMode)
		return;
	// Every (type, size) the floor uses, by its nearest monster - so what the hero sees first is built first.
	struct Want {
		size_t typeIndex;
		unsigned percent;
		int distance;
	};
	std::vector<Want> wants;
	for (size_t k = 0; k < ActiveMonsterCount; k++) {
		const Monster &monster = Monsters[ActiveMonsters[k]];
		if (monster.levelType >= LevelMonsterTypeCount || GetCompanionAnim(monster, MonsterGraphic::Stand) != nullptr
		    || GetMinionAnim(monster, MonsterGraphic::Stand) != nullptr)
			continue;
		const unsigned percent = DrawPercentOf(monster);
		if (percent == 100)
			continue;
		const int distance = DistanceToHero(monster);
		auto it = std::find_if(wants.begin(), wants.end(), [&](const Want &want) { return want.typeIndex == monster.levelType && want.percent == percent; });
		if (it == wants.end())
			wants.push_back({ monster.levelType, percent, distance });
		else
			it->distance = std::min(it->distance, distance);
	}
	std::stable_sort(wants.begin(), wants.end(), [](const Want &a, const Want &b) { return a.distance < b.distance; });
	for (const Want &want : wants) {
		ScaledEntry &entry = EntryFor(want.typeIndex, want.percent);
		// With no worker these build now - on the loading screen, which is still better than mid-fight.
		for (int k = 0; k < 6; k++)
			ScaledAnimIn(entry, BuildOrder[k], want.distance * 8 + k, /*exactPriority=*/true);
		if (!entry.corpse && !QueueScale(entry, CorpseSlot, want.distance * 8 + CorpseSlot, /*exact=*/true)) {
			if (const OptionalClxSpriteListOrSheet src = SourceOf(entry, CorpseSlot))
				entry.corpse = std::make_unique<OwnedClxSpriteListOrSheet>(ScaleListOrSheet(*src, want.percent));
		}
	}
}

} // namespace devilution::oracool
