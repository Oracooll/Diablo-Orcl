#include "oracool/player_resistance.h"

#include <algorithm>

namespace devilution::oracool {

int ResistancePenaltyFor(_difficulty difficulty)
{
	const auto index = static_cast<size_t>(difficulty);
	constexpr size_t Count = sizeof(ResistancePenaltyPerDifficulty) / sizeof(ResistancePenaltyPerDifficulty[0]);
	if (index >= Count)
		return 0;
	return ResistancePenaltyPerDifficulty[index];
}

int ApplyResistanceCurve(int raw, _difficulty difficulty)
{
	// Penetration first, against the raw total. See the header on why the order is the design.
	int value = raw - ResistancePenaltyFor(difficulty);
	if (value <= ResistanceSoftCap)
		return std::max(value, 0);

	// Past the soft cap, integer division truncates - which is the right way for it to fall. A
	// player who is two points into a three-for-one band has bought nothing yet, and rounding up
	// would hand out the point for free at every band boundary.
	const int excess = (value - ResistanceSoftCap) / ResistanceSoftCapDivisor;
	return std::min(ResistanceSoftCap + excess, ResistanceHardCap);
}

} // namespace devilution::oracool
