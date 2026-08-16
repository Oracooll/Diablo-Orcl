/**
 * @file oracool/rng_streams.h
 *
 * Oracool: Megaplan Phase 0.3 - named RNG streams.
 *
 * The engine has ONE vanilla LCG, and everything draws from it: dungeon layout, item recreation,
 * loot rolls - and also purely visual effects. That sharing has produced a whole class of bugs in
 * this project's history, all with the same shape: something COSMETIC drew from the stream inside a
 * window where something GAMEPLAY-DETERMINISTIC (usually "SetRndSeed(rndItemSeed) then replay the
 * drop") depended on the stream's exact position. Thunderous's death burst shifting SpawnLoot's
 * items (fixed by reordering, 1.6.x) and the drop-pool recreation break (fixed by pool exclusion)
 * are both members.
 *
 * Phase 1 of the megaplan adds four new roll sources at once (sockets, gems, gambling, crafting),
 * so the rule is established NOW, while the call sites are few:
 *
 *   - New purely-visual randomness uses CosmeticRnd()/CosmeticFlipCoin(). Its generator is
 *     seeded from the wall clock, never saved, never replayed - it CANNOT be load-bearing.
 *   - Legacy visual effects that internally reach the vanilla LCG through deep call chains
 *     (AddMissile picking animation frames, etc.) are wrapped in a MainSeedGuard, which restores
 *     the vanilla engine state on scope exit - the burst becomes invisible to every deterministic
 *     replay no matter what it calls.
 *   - Gameplay rolls (loot, affixes, monster placement, dungeon gen) stay on the vanilla LCG,
 *     untouched - determinism there IS the save format.
 *
 * MainSeedGuard's one sharp edge, stated plainly: rolls made inside the guard are re-dealt to
 * whoever rolls next after the scope ends (the stream rewinds). That is exactly the point for
 * cosmetic bursts, and exactly why the guard must never wrap anything whose outcome persists.
 */
#pragma once

#include <cstdint>

#include "engine/random.hpp"

namespace devilution::oracool {

/**
 * @brief A random non-negative integer in [0, v) from the COSMETIC stream. Returns 0 for v <= 0.
 *
 * Independent of the vanilla LCG: calling this any number of times leaves every deterministic
 * gameplay stream exactly where it was. Seeded once per session from the wall clock.
 */
int32_t CosmeticRnd(int32_t v);

/** @brief Cosmetic-stream coin flip: true 1 time in @p frequency. */
bool CosmeticFlipCoin(unsigned frequency = 2);

/**
 * @brief Test seam: reseed the cosmetic stream deterministically. Production code never calls
 * this - the stream's whole identity is that nothing may depend on its sequence.
 */
void SeedCosmeticRngForTest(uint32_t seed);

/**
 * @brief RAII guard: snapshots the vanilla LCG on construction, restores it on destruction.
 *
 * For legacy effect bursts that reach GenerateRnd through call chains too deep to migrate
 * (e.g. AddMissile's random animation frames). Wrapping the burst makes it stream-neutral:
 * SpawnLoot-style seeded replays behind it can never be shifted by it again, regardless of
 * call order. Do NOT wrap anything that rolls persistent gameplay outcomes.
 */
class MainSeedGuard {
public:
	MainSeedGuard()
	    : savedState_(GetLCGEngineState())
	{
	}
	~MainSeedGuard()
	{
		SetRndSeed(savedState_);
	}
	MainSeedGuard(const MainSeedGuard &) = delete;
	MainSeedGuard &operator=(const MainSeedGuard &) = delete;

private:
	uint32_t savedState_;
};

} // namespace devilution::oracool
