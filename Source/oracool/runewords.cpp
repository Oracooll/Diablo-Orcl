#include "oracool/runewords.h"

#include <cstring>

#include <fmt/format.h>

#include "items.h"
#include "oracool/gems.h"
#include "oracool/stat_sheet.h"
#include "utils/language.h"
#include "utils/utf8.hpp"

namespace devilution::oracool {

namespace {

// The launch trio - one per host category, each teaching a different lesson about the system:
// Steel is cheap and early (the tutorial word), Lore is the caster's first chase, Ancient's
// Pledge is the wall every melee build wants. Names are D2's own, as homage.
constexpr RunewordDefinition Runewords[] = {
	// name, host, count, runes, dmg%, dmgMod, toHit, allRes, ac, splLvl, mana, hp
	{ N_("Steel"), static_cast<uint8_t>(SocketHost::Weapon), 2,
	    { IDI_ORACOOL_RUNE_TIR, IDI_ORACOOL_RUNE_EL, 0 }, 20, 3, 10, 0, 0, 0, 0, 0 },
	{ N_("Lore"), static_cast<uint8_t>(SocketHost::Armor), 2,
	    { IDI_ORACOOL_RUNE_ORT, IDI_ORACOOL_RUNE_SOL, 0 }, 0, 0, 0, 0, 0, 1, 10, 0 },
	{ N_("Ancient's Pledge"), static_cast<uint8_t>(SocketHost::Shield), 3,
	    { IDI_ORACOOL_RUNE_RAL, IDI_ORACOOL_RUNE_ORT, IDI_ORACOOL_RUNE_EL, }, 0, 0, 0, 13, 8, 0, 0, 0 },
};

const char *HostName(SocketHost host)
{
	switch (host) {
	case SocketHost::Weapon:
		return N_("weapons");
	case SocketHost::Shield:
		return N_("shields");
	case SocketHost::Armor:
		return N_("armor");
	}
	return "";
}

} // namespace

const RunewordDefinition *GetActiveRuneword(const Item &item)
{
	if (item.isEmpty() || item._iSocketCount == 0)
		return nullptr;
	const SocketHost host = SocketHostForItemType(item._itype);
	for (const RunewordDefinition &word : Runewords) {
		if (static_cast<SocketHost>(word.host) != host)
			continue;
		if (item._iSocketCount != word.runeCount || item.socketedCount() != word.runeCount)
			continue;
		bool matches = true;
		for (int i = 0; i < word.runeCount; i++) {
			if (item._iSocketed[i] != word.runes[i]) {
				matches = false;
				break;
			}
		}
		if (matches)
			return &word;
	}
	return nullptr;
}

void ApplyRunewordToTotals(const RunewordDefinition &word, ItemBonusTotals &totals)
{
	totals.bonusDamage += word.bonusDamagePercent;
	totals.damageMod += word.damageMod;
	totals.bonusToHit += word.toHit;
	totals.fireResist += word.allResists;
	totals.lightningResist += word.allResists;
	totals.magicResist += word.allResists;
	totals.bonusArmor += word.bonusAc;
	totals.spellLevelAdd += word.spellLevels;
	totals.mana += word.mana << 6;
	totals.hitPoints += word.hitPoints << 6;
}

std::string RuneTeachingLines(uint16_t runeIdx)
{
	std::string lines;
	for (const RunewordDefinition &word : Runewords) {
		bool contains = false;
		for (int i = 0; i < word.runeCount; i++) {
			if (word.runes[i] == runeIdx)
				contains = true;
		}
		if (!contains)
			continue;
		std::string sequence;
		for (int i = 0; i < word.runeCount; i++) {
			if (i > 0)
				sequence += ' ';
			// "El Rune" -> the bare rune name reads better in a formula.
			sequence += _(AllItemsList[word.runes[i]].iName);
		}
		if (!lines.empty())
			lines += '\n';
		lines += fmt::format(fmt::runtime(_("Runeword '{:s}': {:s}, in {:s}")),
		    _(word.name), sequence, _(HostName(static_cast<SocketHost>(word.host))));
	}
	return lines;
}

bool TryCompleteRuneword(Item &item)
{
	const RunewordDefinition *word = GetActiveRuneword(item);
	if (word == nullptr)
		return false;
	// The item takes the word's name - the one persisted piece, and it rides the name field every
	// save format already carries. The base name stays visible in the description's own lines.
	CopyUtf8(item._iIName, std::string(_(word->name)), sizeof(item._iIName));
	item._iIdentified = true;
	return true;
}

} // namespace devilution::oracool
