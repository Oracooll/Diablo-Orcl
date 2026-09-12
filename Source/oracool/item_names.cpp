#include "oracool/item_names.h"

#include <array>

#include <fmt/format.h>

#include "utils/language.h"

namespace devilution::oracool {

namespace {

/**
 * @brief The first word: what the thing is LIKE.
 *
 * Untranslated here and run through _() at the point of use, as every other name table in this
 * codebase is. Sixty-four of each word crossed with sixty-four of the other is 4096 names, which is
 * enough that a player will rarely see the same one twice in a session without the pool being so
 * large that the words stop feeling like they belong to one world.
 */
const std::array<const char *, 64> Adjectives = {
	N_("Ashen"), N_("Bleak"), N_("Grim"), N_("Dread"), N_("Wretched"), N_("Hollow"), N_("Withered"), N_("Blighted"),
	N_("Sable"), N_("Umbral"), N_("Baleful"), N_("Fell"), N_("Gaunt"), N_("Shrouded"), N_("Rotting"), N_("Cursed"),
	N_("Forsaken"), N_("Haunted"), N_("Mournful"), N_("Sundered"), N_("Broken"), N_("Ruined"), N_("Savage"), N_("Feral"),
	N_("Vicious"), N_("Brutal"), N_("Crimson"), N_("Bloodied"), N_("Scarred"), N_("Jagged"), N_("Serrated"), N_("Wicked"),
	N_("Vile"), N_("Foul"), N_("Rancid"), N_("Putrid"), N_("Venomous"), N_("Toxic"), N_("Searing"), N_("Smoldering"),
	N_("Charred"), N_("Frozen"), N_("Glacial"), N_("Bitter"), N_("Howling"), N_("Screaming"), N_("Silent"), N_("Whispering"),
	N_("Eternal"), N_("Undying"), N_("Ancient"), N_("Forgotten"), N_("Buried"), N_("Entombed"), N_("Sacred"), N_("Profane"),
	N_("Unholy"), N_("Damned"), N_("Radiant"), N_("Gilded"), N_("Obsidian"), N_("Iron"), N_("Rusted"), N_("Tarnished")
};

/** @brief The second word: what the thing IS. */
const std::array<const char *, 64> Nouns = {
	N_("Knell"), N_("Verdict"), N_("Coil"), N_("Fang"), N_("Claw"), N_("Brand"), N_("Shard"), N_("Veil"),
	N_("Shroud"), N_("Mark"), N_("Reach"), N_("Grasp"), N_("Whisper"), N_("Howl"), N_("Wail"), N_("Gaze"),
	N_("Weave"), N_("Ward"), N_("Keep"), N_("Watch"), N_("Fall"), N_("Dawn"), N_("Dusk"), N_("Heart"),
	N_("Spine"), N_("Husk"), N_("Shell"), N_("Scar"), N_("Wound"), N_("Seal"), N_("Sigil"), N_("Rune"),
	N_("Oath"), N_("Vow"), N_("Pact"), N_("Curse"), N_("Tithe"), N_("Toll"), N_("Echo"), N_("Dirge"),
	N_("Requiem"), N_("Lament"), N_("Breath"), N_("Pulse"), N_("Thorn"), N_("Barb"), N_("Spike"), N_("Edge"),
	N_("Crest"), N_("Crown"), N_("Bough"), N_("Root"), N_("Ember"), N_("Cinder"), N_("Pyre"), N_("Tomb"),
	N_("Crypt"), N_("Barrow"), N_("Hollow"), N_("Rift"), N_("Abyss"), N_("Throne"), N_("Bane"), N_("Doom")
};

/**
 * @brief splitmix32's finaliser - a full avalanche mix of one 32-bit word.
 *
 * A plain `seed % 64` would NOT do here. Item seeds are handed out by AdvanceRndSeed in a sequence,
 * so consecutive drops carry closely related seeds, and taking the low bits of them directly would
 * march the first word straight down the table - every item in a room named "Ashen ...", then
 * "Bleak ...". Mixing first means neighbouring seeds land nowhere near each other.
 */
uint32_t MixSeed(uint32_t x)
{
	x += 0x9E3779B9;
	x = (x ^ (x >> 16)) * 0x21F0AAAD;
	x = (x ^ (x >> 15)) * 0x735A2D97;
	return x ^ (x >> 15);
}

} // namespace

std::string GenerateOracoolItemName(uint32_t seed)
{
	// Two INDEPENDENT mixes rather than one mix split in half: the halves of a single hash are
	// perfectly good on their own, but salting the second makes the two words independent for
	// certain, so a run of items cannot share a noun while only the adjective moves.
	const uint32_t first = MixSeed(seed);
	const uint32_t second = MixSeed(seed ^ 0x5BF03635);
	const char *adjective = Adjectives[first % Adjectives.size()];
	const char *noun = Nouns[second % Nouns.size()];
	return fmt::format(fmt::runtime("{0} {1}"), _(adjective), _(noun));
}

size_t OracoolNameAdjectiveCount()
{
	return Adjectives.size();
}

size_t OracoolNameNounCount()
{
	return Nouns.size();
}

const char *OracoolNameAdjective(size_t index)
{
	return index < Adjectives.size() ? Adjectives[index] : nullptr;
}

const char *OracoolNameNoun(size_t index)
{
	return index < Nouns.size() ? Nouns[index] : nullptr;
}

} // namespace devilution::oracool
