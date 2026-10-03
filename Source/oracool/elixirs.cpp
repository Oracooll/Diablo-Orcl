#include "oracool/elixirs.h"

#include <algorithm>

#include "items.h"
#include "oracool/rage.h" // UsesRage - a Barbarian refuses the Elixir of Magic
#include "player.h"
#include "plrmsg.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

size_t Slot(CharacterAttribute attribute)
{
	return static_cast<size_t>(attribute);
}

} // namespace

std::optional<CharacterAttribute> ElixirAttribute(item_misc_id miscId)
{
	switch (miscId) {
	case IMISC_ELIXSTR:
		return CharacterAttribute::Strength;
	case IMISC_ELIXMAG:
		return CharacterAttribute::Magic;
	case IMISC_ELIXDEX:
		return CharacterAttribute::Dexterity;
	case IMISC_ELIXVIT:
		return CharacterAttribute::Vitality;
	default:
		return std::nullopt;
	}
}

bool RefuseElixir(Player &player, const Item &item)
{
	const std::optional<CharacterAttribute> attribute = ElixirAttribute(item._iMiscId);
	if (!attribute)
		return false;
	const bool barbarianMagic = *attribute == CharacterAttribute::Magic && UsesRage(player);
	const bool full = ElixirStacks(player, *attribute) >= ElixirMaxStacks;
	if (!barbarianMagic && !full)
		return false;
	// Said and logged by the local hero only - the one at the keyboard (and a test's stand-in hero has no voice to play).
	if (&player == MyPlayer) {
		player.Say(HeroSpeech::ICantDoThat);
		EventPlrMsg(barbarianMagic ? _("A Barbarian puts nothing into Magic.")
		                           : _("Ten of these already stand - wait for them to run out."),
		    UiFlags::ColorRed);
	}
	return true;
}

void DrinkElixir(Player &player, CharacterAttribute attribute)
{
	const size_t slot = Slot(attribute);
	player._pElixirStacks[slot] = static_cast<uint8_t>(std::min(player._pElixirStacks[slot] + 1, ElixirMaxStacks));
	player._pElixirTicks[slot] = static_cast<uint16_t>(ElixirDurationTicks);
	CalcPlrInvKeepingLife(player);
}

int ElixirStacks(const Player &player, CharacterAttribute attribute)
{
	const size_t slot = Slot(attribute);
	return player._pElixirTicks[slot] > 0 ? std::min<int>(player._pElixirStacks[slot], ElixirMaxStacks) : 0;
}

int ElixirBonus(const Player &player, CharacterAttribute attribute)
{
	return ElixirStacks(player, attribute) * ElixirBonusPerStack;
}

int ElixirTicksLeft(const Player &player, CharacterAttribute attribute)
{
	return ElixirStacks(player, attribute) > 0 ? player._pElixirTicks[Slot(attribute)] : 0;
}

item_cursor_graphic ElixirCursor(CharacterAttribute attribute)
{
	switch (attribute) {
	case CharacterAttribute::Strength:
		return ICURS_ELIXIR_OF_STRENGTH;
	case CharacterAttribute::Magic:
		return ICURS_ELIXIR_OF_MAGIC;
	case CharacterAttribute::Dexterity:
		return ICURS_ELIXIR_OF_DEXTERITY;
	case CharacterAttribute::Vitality:
	default:
		return ICURS_ELIXIR_OF_VITALITY;
	}
}

void ProcessElixirsTick(Player &player)
{
	bool ended = false;
	for (size_t slot = 0; slot < 4; slot++) {
		if (player._pElixirTicks[slot] == 0)
			continue;
		player._pElixirTicks[slot]--;
		if (player._pElixirTicks[slot] == 0) {
			player._pElixirStacks[slot] = 0;
			ended = true;
		}
	}
	if (ended)
		CalcPlrInvKeepingLife(player);
}

void ResetElixirs(Player &player)
{
	for (size_t slot = 0; slot < 4; slot++) {
		player._pElixirStacks[slot] = 0;
		player._pElixirTicks[slot] = 0;
	}
}

} // namespace devilution::oracool
