#include "oracool/rng_streams.h"

#include <SDL.h>

namespace devilution::oracool {

namespace {

/**
 * @brief The cosmetic stream's own engine - xorshift32, chosen because it is tiny, decent for
 * visuals, and shares NOTHING with the vanilla LCG, so the two cannot be confused for one
 * another in a debugger.
 *
 * Seeded lazily from the wall clock on first use. 0 is xorshift's one fixed point, so it is
 * nudged; the OR with 1 also covers SDL_GetTicks() returning 0 in the first millisecond.
 */
uint32_t CosmeticState = 0;

uint32_t NextCosmetic()
{
	if (CosmeticState == 0)
		CosmeticState = static_cast<uint32_t>(SDL_GetTicks()) | 1U;
	uint32_t x = CosmeticState;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	CosmeticState = x;
	return x;
}

} // namespace

int32_t CosmeticRnd(int32_t v)
{
	if (v <= 0)
		return 0;
	return static_cast<int32_t>(NextCosmetic() % static_cast<uint32_t>(v));
}

bool CosmeticFlipCoin(unsigned frequency)
{
	if (frequency <= 1)
		return true;
	return CosmeticRnd(static_cast<int32_t>(frequency)) == 0;
}

void SeedCosmeticRngForTest(uint32_t seed)
{
	CosmeticState = seed == 0 ? 1 : seed;
}

} // namespace devilution::oracool
