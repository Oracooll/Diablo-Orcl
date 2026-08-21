#include "oracool/treasure_class.h"

#include "monster.h"

namespace devilution::oracool {

namespace {

/**
 * @brief The tables.
 *
 * Every zone gives ONE family the clear majority and a second a real share, so a place is worth
 * going to for a thing and is not merely a slightly different average. The rejected alternative was
 * a gentle tilt everywhere - 30/25/25/20 - which reads as noise in play: nobody farms a 5% edge
 * they cannot feel, and a treasure class nobody can feel is a table that exists for its own sake.
 *
 *  - CATHEDRAL is the gem floor. Gems are the socketable a new character can actually use: small
 *    broad stats, a cheap ladder, and no runeword to look up first. Jewels are ZERO here - not
 *    rare, absent - because the shallow end should not be teaching two socket economies at once.
 *  - CATACOMBS is the charm floor. It is also where the extra backpack tabs start to matter, and
 *    charms are the one family whose power is bounded by inventory rather than by sockets.
 *  - CAVES is the rune floor, and the runeword engine is what makes it the deepest change in the
 *    game to farm for. Fire and forge; it is also where the Anvil sits.
 *  - HELL is the jewel floor, with runes close behind. Both are permanent unconditional power, and
 *    Hell is where a character has the sockets to spend them on.
 *  - CRYPT is runes again, harder. Na-Krul's vault is the one place that should rival the Caves.
 *  - NEST is charms and jewels - the two families that need no host item at all to pay off.
 *
 * The socketable RATE climbs with depth (7, 8, 9, 11) and so does the set rate (3, 3, 4, 5). That
 * climb is separate from the eligibility gate BandedQlvl already applies: depth decides both how
 * OFTEN a socketable falls and how good one is allowed to be, and the two were deliberately not
 * folded together, because the first is a reward for being deep and the second is a cap.
 */
constexpr TreasureClass Classes[] = {
	// name                        rate  gem rune jewel charm  set
	{ "Nothing at all",               0,   0,   0,    0,    0,   0 }, // town
	{ "The Cathedral's Offering",     7,  60,  25,    0,   15,   3 },
	{ "The Catacombs' Reliquary",     8,  25,  20,   10,   45,   3 },
	{ "The Caves' Forge",             9,  15,  55,   10,   20,   4 },
	{ "Hell's Own Hoard",            11,  10,  30,   45,   15,   5 },
	{ "The Nest's Clutch",            9,  10,  15,   35,   40,   4 },
	{ "Na-Krul's Vault",             11,  10,  50,   30,   10,   5 },
};

/** @brief Index into Classes for @p dungeon. Town and anything unknown fall on the empty row. */
size_t ClassIndexFor(dungeon_type dungeon)
{
	switch (dungeon) {
	case DTYPE_CATHEDRAL:
		return 1;
	case DTYPE_CATACOMBS:
		return 2;
	case DTYPE_CAVES:
		return 3;
	case DTYPE_HELL:
		return 4;
	case DTYPE_NEST:
		return 5;
	case DTYPE_CRYPT:
		return 6;
	case DTYPE_TOWN:
	case DTYPE_NONE:
		break;
	}
	return 0;
}

} // namespace

const TreasureClass &TreasureClassFor(dungeon_type dungeon)
{
	return Classes[ClassIndexFor(dungeon)];
}

const TreasureClass &CurrentTreasureClass()
{
	return TreasureClassFor(leveltype);
}

int TreasureBonusFor(const Monster &monster)
{
	// Read in this order deliberately: a unique monster can also carry a lesser affix, and it is
	// the unique half that should decide.
	if (monster.isUnique())
		return 4;
	if (monster.lesserAffix != LesserUniqueAffix::None)
		return 2;
	return 1;
}

int TotalFamilyWeight(const TreasureClass &tc)
{
	return tc.gemWeight + tc.runeWeight + tc.jewelWeight + tc.charmWeight;
}

SocketableFamily FamilyForRoll(const TreasureClass &tc, int roll)
{
	// Walked in enum order so the weights list and the enum cannot drift apart: the first weight IS
	// Gem's share, and there is no second ordering anywhere to disagree with that.
	if (roll < tc.gemWeight)
		return SocketableFamily::Gem;
	roll -= tc.gemWeight;
	if (roll < tc.runeWeight)
		return SocketableFamily::Rune;
	roll -= tc.runeWeight;
	if (roll < tc.jewelWeight)
		return SocketableFamily::Jewel;
	// Everything left is Charm - including a roll past the end, which a caller can only produce by
	// drawing against a total this function did not compute. Answering rather than reading off the
	// end is the safe half of that; the test pins the distribution so it stays the unreachable one.
	return SocketableFamily::Charm;
}

} // namespace devilution::oracool
