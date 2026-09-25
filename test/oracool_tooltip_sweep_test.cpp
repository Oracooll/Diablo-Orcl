/**
 * @file oracool_tooltip_sweep_test.cpp
 *
 * The tooltip sweep (user request, 2026-09-25): "imagine you are a player and you have all possible
 * crafting material to craft all possible items and recipes in the game ... double check if it has all
 * the necessary rows of text according to the artefact".
 *
 * The artefact is the published "Orcl Item Tooltips" page. Its rules are written out below as
 * CheckTooltip, one rule id per row family (R0 name, R1 quality/tier, R2 base stats, R3 affixes, R4
 * ethereal, R5 sockets, R6 runeword, R7 unidentified, R8 requirements, R9 use hints, R10 family rows,
 * RG generic hygiene).
 *
 * HOW THE ITEMS ARE MADE - through the game's own creation paths, never hand-built, except the two
 * fields no path can reach from here (a keystone's rift tier, which only a world drop stamps, and a
 * worn-down ethereal's durability for the Mend recipe):
 *
 *   - every available base (IDI_GOLD+1 .. IDI_LAST): the smith's route - seed, StampVendorItemLevel,
 *     GetItemAttrs, ApplyVendorTier - then identified. Oils and books re-rolled until every oil kind and
 *     every book spell the level reaches has been seen. A stack of five of every stackable kind.
 *   - every WORN base, on top: a MAGIC roll (Wirt's gamble, RollGambleResult, retried until magic), an
 *     UNIDENTIFIED copy of it, RARE / BUFFED UNIQUE / PRIMAL (RetierOracoolItem - SetupAllItems' forced
 *     tier, the Enrich/Awaken path), ETHEREAL (MakeItemEthereal) and SOCKETED (the Punch Sockets recipe,
 *     then one stone seated with TrySocketGem - the backpack's own insertion rule).
 *   - every unique through the Unique shelf (CreateUniqueVendorItem), every set piece through the Set
 *     shelf (CreateSetVendorItem, walked until the shelf has nothing left to offer), every runeword rune
 *     by rune (TrySocketGem + TryCompleteRuneword, the socket UI's own pair).
 *   - every one of the 28 recipes through TransmuteLevskiGridWith - the one function Levski's Cube,
 *     Ogden's table and Gillian's hearth all run - fed the reagents each recipe asks for; every item the
 *     grid holds afterwards is an output and is swept.
 *   - Gillian's Mystic services through the public functions her buttons call: Reroll (RollOracoolAffixFor
 *     + RebuildOracoolItemWithAffixes), Imbue (TryImbue, every shard kind), Remove (Capture/Strip/Restore).
 *
 * Each tooltip is captured exactly as the backpack hover builds it (inv.cpp: SetPanelString(name,
 * colour), then PrintItemDetails when identified, else PrintItemDur), rows split on '\n' with their
 * recorded colours. Mouse control mode is entered through DetectInputMethod, because ControlMode itself
 * is not exported to the test DLL; a probe potion confirms it took before the use-hint rules are applied.
 *
 * OUTPUT: tooltip_sweep_report.md (summary, then every FAIL/WARN item with its whole tooltip) and
 * tooltip_sweep_all.md (every item's tooltip, compact) in the working directory. FAILs turn the run red;
 * WARNs (rows longer than 60 characters, a "-1" that is not a stat's printed value) only report.
 */

#include <cstdio>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <SDL.h>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "engine/render/text_render.hpp"
#include "controls/controller.h"
#include "controls/plrctrls.h"
#include "diablo.h"
#include "engine/random.hpp"
#include "init.h"
#include "cursor.h" // InitCursor - item sizes read the cursor sprites
#include "inv.h"
#include "itemdat.h"
#include "items.h"
#include "multi.h"
#include "oracool/charms.h"
#include "oracool/crafting.h"
#include "oracool/gems.h"
#include "oracool/hidden_classes.h"
#include "oracool/imbuement.h"
#include "oracool/item_sets.h"
#include "oracool/item_tiers.h"
#include "oracool/level_requirement.h"
#include "oracool/levski_roar.h"
#include "oracool/runewords.h"
#include "player.h"

using namespace devilution;


namespace {

/** @brief The vendor level every base is stocked at: deep enough that every oil and most book spells roll. */
constexpr int VendorLevel = 30;
/** @brief How many of each tier-recipe's targets are sampled from the worn bases. */
constexpr size_t RecipeSample = 24;
/** @brief A row longer than this may overflow the tooltip; reported as a WARN, never a FAIL. */
constexpr size_t LongRow = 60;

struct TipRow {
	std::string text;
	UiFlags color;
};

struct Made {
	std::string category;
	std::string how;
	std::string name;
	std::vector<TipRow> rows;
	std::vector<std::string> fails;
	std::vector<std::string> warns;
};

const char *ColorName(UiFlags color)
{
	static const char *const Names[] = {
		"(none)", "UiGold", "UiSilver", "UiGoldDark", "UiSilverDark", "DialogWhite", "DialogYellow", "DialogRed",
		"Yellow", "Gold", "Black", "White", "Whitegold", "Red", "Blue", "Orange", "Buttonface", "Buttonpushed",
		"UiYellow", "UiYellowDark", "OracoolGreen", "Gray5", "Beige2", "Yellow3", "BrightRed3", "BrightBlue3",
		"Gold6", "Orange7", "Gray7", "MagicDamage", "Scroll", "Elixir", "Oil", "Trap", "Salvage", "Map",
		"TitleWhite", "TitleBlue", "TitleWhitegold"
	};
	const unsigned index = UiFlagsColorIndex(color);
	return index < sizeof(Names) / sizeof(Names[0]) ? Names[index] : "Color?";
}

bool StartsWith(const std::string &text, const std::string &prefix)
{
	return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(const std::string &text, const std::string &suffix)
{
	return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string NameOf(const devilution::Item &item)
{
	return std::string(item.getName().str());
}

/**
 * @brief The tooltip exactly as the backpack hover builds it (inv.cpp, the item arm of the hover).
 *
 * HeadlessMode is lowered around the print only: PrintItemDetails and PrintItemDur return at once
 * headless. @p coloursMatch is the invariant DrawCursorTooltip asserts on - one colour per row.
 */
std::vector<TipRow> CaptureTooltip(const devilution::Item &item, bool &coloursMatch)
{
	const bool savedHeadless = HeadlessMode;
	HeadlessMode = false;
	ClearPanelStrings();
	SetPanelString(item.getName(), item.getTextColor());
	if (item._iIdentified)
		PrintItemDetails(item);
	else
		PrintItemDur(item);
	HeadlessMode = savedHeadless;

	const std::string text(InfoString.str());
	std::vector<std::string> lines;
	size_t start = 0;
	for (size_t guard = 0; guard <= text.size(); guard++) {
		const size_t end = text.find('\n', start);
		if (end == std::string::npos) {
			lines.push_back(text.substr(start));
			break;
		}
		lines.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	const std::vector<UiFlags> colours = InfoStringLineColors;
	coloursMatch = colours.size() == lines.size();
	std::vector<TipRow> rows;
	rows.reserve(lines.size());
	for (size_t i = 0; i < lines.size(); i++)
		rows.push_back({ lines[i], i < colours.size() ? colours[i] : UiFlags::ColorWhite });
	ClearPanelStrings();
	return rows;
}

bool IsWorn(const devilution::Item &item)
{
	return item._iLoc != ILOC_NONE && item._iLoc != ILOC_UNEQUIPABLE && item._iLoc != ILOC_BELT;
}

bool IsJewellery(const devilution::Item &item)
{
	return item._itype == ItemType::Ring || item._itype == ItemType::Amulet;
}

/** @brief The quality word PrintItemDetails must open the worn-gear block with. */
std::string ExpectedQualityWord(const devilution::Item &item)
{
	if (item.hasOracoolTier()) {
		switch (item._iOracoolTier) {
		case OracoolItemTier::Rare:
			return "rare";
		case OracoolItemTier::BuffedUnique:
			return "unique";
		case OracoolItemTier::Primal:
			return "primal";
		case OracoolItemTier::Set:
			return "set";
		case OracoolItemTier::None:
			break;
		}
	}
	if (item._iMagical == ITEM_QUALITY_UNIQUE)
		return "unique";
	if (item._iMagical == ITEM_QUALITY_MAGIC)
		return "magic";
	return "basic";
}

/** @brief The things a right-click uses (the spec's list: potions, elixirs, oils, trap runes, scrolls, books, signet, keystone, sealed map). */
bool IsRightClickUsable(const devilution::Item &item)
{
	const item_misc_id m = item._iMiscId;
	if (m > IMISC_USEFIRST && m < IMISC_USELAST)
		return true;
	if (m > IMISC_OILFIRST && m < IMISC_OILLAST)
		return true;
	if (m > IMISC_RUNEFIRST && m < IMISC_RUNELAST)
		return true;
	switch (m) {
	case IMISC_ARENAPOT:
	case IMISC_SCROLL:
	case IMISC_SCROLLT:
	case IMISC_BOOK:
	case IMISC_NOTE:
	case IMISC_MAPOFDOOM:
	case IMISC_SPECELIX:
	case IMISC_ORACOOL_SIGNET:
	case IMISC_ORACOOL_KEYSTONE:
	case IMISC_ORACOOL_MAP:
		return true;
	default:
		return false;
	}
}

/**
 * @brief 0 when @p text carries no "-1" value, 1 (WARN) for a signed -1 that may be a real stat,
 * 2 (FAIL) for a -1 printed after ':' or '/' - "Dur: -1/-1", "Charges: -1" - an uninitialised field.
 * A "-1" inside a range ("1-10") or a longer number ("-15") is not a value of -1 and is skipped.
 */
int MinusOneSeverity(const std::string &text)
{
	int worst = 0;
	for (size_t p = text.find("-1"); p != std::string::npos; p = text.find("-1", p + 1)) {
		const char next = p + 2 < text.size() ? text[p + 2] : '\0';
		if (next >= '0' && next <= '9')
			continue;
		const char prev = p > 0 ? text[p - 1] : ' ';
		if (std::isalnum(static_cast<unsigned char>(prev)) != 0)
			continue;
		size_t q = p;
		while (q > 0 && text[q - 1] == ' ')
			q--;
		const char before = q > 0 ? text[q - 1] : '\0';
		worst = std::max(worst, (before == ':' || before == '/') ? 2 : 1);
	}
	return worst;
}

/**
 * @brief The spec, row by row. Every rule reads the ITEM's own state, so it applies to every item
 * whatever category made it.
 */
void CheckTooltip(const devilution::Item &item, const std::vector<TipRow> &rows, bool coloursMatch, bool mouseMode, Made &out)
{
	const auto fail = [&out](const std::string &rule, const std::string &why) { out.fails.push_back(rule + ": " + why); };
	const auto warn = [&out](const std::string &rule, const std::string &why) { out.warns.push_back(rule + ": " + why); };
	const auto findRow = [&rows](const std::string &text) {
		for (size_t i = 1; i < rows.size(); i++) {
			if (rows[i].text == text)
				return static_cast<int>(i);
		}
		return -1;
	};
	const auto findPrefix = [&rows](const std::string &prefix) {
		for (size_t i = 1; i < rows.size(); i++) {
			if (StartsWith(rows[i].text, prefix))
				return static_cast<int>(i);
		}
		return -1;
	};

	if (!coloursMatch)
		fail("RG-COLOURS", "the panel recorded a different number of colours than rows - DrawCursorTooltip asserts on this");
	if (rows.empty() || rows[0].text.empty()) {
		fail("R0", "no name row");
		return;
	}

	const bool identified = item._iIdentified;
	const UiFlags nameColor = item.getTextColor();

	// ---- R0: the name, or "Runeword: {word}" in whitegold ----
	const oracool::RunewordDefinition *word = identified ? oracool::GetActiveRuneword(item) : nullptr;
	if (word != nullptr) {
		const std::string expected = std::string("Runeword: ") + word->name;
		if (rows[0].text != expected)
			fail("R0-RUNEWORD", "row 0 is '" + rows[0].text + "', expected '" + expected + "'");
		if (!HasColor(rows[0].color, UiFlags::ColorWhitegold))
			fail("R0-RUNEWORD", std::string("row 0 is ") + ColorName(rows[0].color) + ", expected Whitegold");
	} else {
		if (rows[0].text != NameOf(item))
			fail("R0", "row 0 is '" + rows[0].text + "', the item's name is '" + NameOf(item) + "'");
		if (!HasColor(rows[0].color, nameColor))
			fail("R0", std::string("row 0 is ") + ColorName(rows[0].color) + ", the name colour is " + ColorName(nameColor));
	}

	// ---- R1: worn gear, identified - "{quality} {noun}" in the name colour, and a Tier row ----
	int qualityRow = -1;
	if (IsWorn(item) && identified) {
		const std::string quality = ExpectedQualityWord(item) + " ";
		for (size_t i = 1; i < rows.size(); i++) {
			if (StartsWith(rows[i].text, quality) && rows[i].text.size() > quality.size() && HasColor(rows[i].color, nameColor)) {
				qualityRow = static_cast<int>(i);
				break;
			}
		}
		if (qualityRow < 0)
			fail("R1-QUALITY", "no '" + quality + "{noun}' row in the name colour " + ColorName(nameColor));
		if (findPrefix("Tier: ") < 0)
			fail("R1-TIER", "no 'Tier: ...' row");
	}

	// ---- R2: the base stat row ----
	if (item._iClass == ICLASS_WEAPON && findPrefix("damage:") < 0)
		fail("R2-DAMAGE", "a weapon with no 'damage:' row");
	if (item._iClass == ICLASS_ARMOR && findPrefix("armor:") < 0)
		fail("R2-ARMOR", "armour with no 'armor:' row");
	if (IsJewellery(item) && (findPrefix("damage:") >= 0 || findPrefix("armor:") >= 0))
		fail("R2-JEWELLERY", "a ring or amulet printing a damage or armor row");

	// Blue rows that can be affixes: not the name, not the quality line, not the unidentified notice.
	int affixBlue = 0;
	for (size_t i = 1; i < rows.size(); i++) {
		if (static_cast<int>(i) == qualityRow || rows[i].text == "Not Identified")
			continue;
		if (HasColor(rows[i].color, UiFlags::ColorBlue))
			affixBlue++;
	}

	if (identified) {
		// ---- R3: the affix rows ----
		if (item.hasOracoolTier() && item._iOracoolTier != OracoolItemTier::Set) {
			if (affixBlue < item._iOracoolAffixCount)
				fail("R3-TIERED", std::to_string(affixBlue) + " blue rows for " + std::to_string(item._iOracoolAffixCount) + " affixes");
		} else if (item._iOracoolTier == OracoolItemTier::Set) {
			const oracool::SetItemDefinition *def = oracool::FindSetItemByCursor(item._iCurs);
			const oracool::ItemSetDefinition *set = def != nullptr ? oracool::FindItemSetOwning(def->id) : nullptr;
			if (def == nullptr || set == nullptr) {
				fail("R3-SET", "a Set-tier item whose piece or set cannot be found");
			} else {
				int ownPowers = 0;
				for (const ItemPower &power : def->powers) {
					if (power.type == IPL_INVALID)
						break;
					ownPowers++;
				}
				if (affixBlue < ownPowers)
					fail("R3-SET", std::to_string(affixBlue) + " blue rows for the piece's " + std::to_string(ownPowers) + " powers");
				const std::string tail = "/" + std::to_string(set->itemCount) + ")";
				bool setRow = false;
				int pieceRows = 0;
				int rungRows = 0;
				for (size_t i = 1; i < rows.size(); i++) {
					const std::string &t = rows[i].text;
					if (StartsWith(t, set->name) && EndsWith(t, tail) && HasColor(rows[i].color, UiFlags::ColorOracoolGreen))
						setRow = true;
					else if (StartsWith(t, "  ("))
						rungRows++;
					else if (StartsWith(t, "  "))
						pieceRows++;
				}
				if (!setRow)
					fail("R3-SET", std::string("no '") + set->name + " (x" + tail + "' row in OracoolGreen");
				if (pieceRows != set->itemCount)
					fail("R3-SET", std::to_string(pieceRows) + " piece rows for a set of " + std::to_string(set->itemCount));
				if (rungRows != set->bonusCount)
					fail("R3-SET", std::to_string(rungRows) + " bonus-rung rows for " + std::to_string(set->bonusCount) + " rungs");
			}
		} else if (item._iMagical == ITEM_QUALITY_UNIQUE) {
			int expected = 0;
			if (item._iUid >= 0 && static_cast<size_t>(item._iUid) < UniqueItemCount) {
				for (const ItemPower &power : UniqueItems[item._iUid].powers) {
					if (power.type == IPL_INVALID)
						break;
					if (power.type != IPL_INVCURS)
						expected++;
				}
			}
			if (affixBlue < expected)
				fail("R3-UNIQUE", std::to_string(affixBlue) + " blue rows for " + std::to_string(expected) + " powers");
		} else if (item._iMagical == ITEM_QUALITY_MAGIC) {
			if (affixBlue < 1)
				fail("R3-MAGIC", "an identified magic item with no blue affix row");
		}

		// ---- R4: ethereal ----
		if (item._iOracoolEthereal && findRow("Ethereal (cannot be repaired)") < 0)
			fail("R4-ETHEREAL", "no 'Ethereal (cannot be repaired)' row");

		// ---- R5: sockets ----
		if (item._iSocketCount > 0) {
			const std::string socketRow = "Sockets: " + std::to_string(item.socketedCount()) + "/" + std::to_string(item._iSocketCount);
			if (findRow(socketRow) < 0)
				fail("R5-SOCKETS", "no '" + socketRow + "' row");
			const oracool::SocketHost host = oracool::SocketHostForItemType(item._itype);
			std::map<std::string, int> wanted;
			for (const uint16_t stone : item._iSocketed) {
				if (stone != devilution::Item::EmptySocket)
					wanted[oracool::GemSocketLine(stone, host)]++;
			}
			for (const auto &[line, count] : wanted) {
				int found = 0;
				for (size_t i = 1; i < rows.size(); i++) {
					if (rows[i].text == line)
						found++;
				}
				if (line.empty())
					fail("R5-SOCKETS", "a seated stone has no socket line to print");
				else if (found < count)
					fail("R5-SOCKETS", "the stone row '" + line + "' is printed " + std::to_string(found) + " times for " + std::to_string(count) + " stones");
			}
		}

		// ---- R6: a completed runeword ----
		if (word != nullptr) {
			const std::vector<std::string> bonus = oracool::RunewordBonusLines(*word);
			if (bonus.empty())
				fail("R6-RUNEWORD", "the word has no bonus row to print");
			for (const std::string &line : bonus) {
				if (findRow(line) < 0)
					fail("R6-RUNEWORD", "the word's bonus row '" + line + "' is missing");
			}
			if (findPrefix("Sockets:") < 0)
				fail("R6-RUNEWORD", "no 'Sockets:' row");
		}
	} else {
		// ---- R7: unidentified ----
		if (item._iMagical != ITEM_QUALITY_NORMAL && (item._iClass == ICLASS_WEAPON || item._iClass == ICLASS_ARMOR || IsJewellery(item))) {
			const int row = findRow("Not Identified");
			if (row < 0)
				fail("R7-UNIDENTIFIED", "no 'Not Identified' row");
			else if (!HasColor(rows[row].color, UiFlags::ColorBlue))
				fail("R7-UNIDENTIFIED", std::string("'Not Identified' is ") + ColorName(rows[row].color) + ", expected Blue");
		}
	}

	// ---- R8: requirements (both printers end in PrintItemInfo) ----
	const int str = oracool::EffectiveRequirement(item, item._iMinStr);
	const int mag = oracool::EffectiveRequirement(item, item._iMinMag);
	const int dex = oracool::EffectiveRequirement(item, item._iMinDex);
	if ((str != 0 || mag != 0 || dex != 0) && findPrefix("Required:") < 0)
		fail("R8-REQUIRED", "requirements of " + std::to_string(str) + " Str / " + std::to_string(mag) + " Mag / " + std::to_string(dex) + " Dex and no 'Required:' row");
	if (const int level = oracool::RequiredLevel(item); level > 1) {
		const std::string levelRow = "Required Level: " + std::to_string(level);
		if (findRow(levelRow) < 0)
			fail("R8-LEVEL", "no '" + levelRow + "' row");
	}

	// ---- R9: use hints, mouse control mode only ----
	if (mouseMode) {
		if (IsRightClickUsable(item) && findPrefix("Right-click to") < 0)
			fail("R9-USE", "a right-click consumable with no 'Right-click to ...' row");
		if (item.isStackableConsumable() && item.stackCount() > 1 && findRow("Shift + right-click to split the stack") < 0)
			fail("R9-SPLIT", "a stack of " + std::to_string(item.stackCount()) + " with no 'Shift + right-click to split the stack' row");
	}

	// ---- R10: the families' own rows ----
	const int idx = item.IDidx;
	if (item._iMiscId == IMISC_ORACOOL_SIGNET && findRow("one permanent stat point") < 0)
		fail("R10-SIGNET", "no 'one permanent stat point' row");
	if (item._iMiscId == IMISC_ORACOOL_KEYSTONE && findPrefix("opens a Guardian Rift of tier") < 0)
		fail("R10-KEYSTONE", "no 'opens a Guardian Rift of tier N' row");
	if (item._iMiscId == IMISC_ORACOOL_MAP && findPrefix("opens ") < 0)
		fail("R10-MAP", "no 'opens {encounter}' row");
	if (identified) {
		if (IsOracoolCharmIdx(idx)) {
			const std::string cap = "only your first " + std::to_string(oracool::CharmActiveCap) + " charms are active";
			if (findRow(cap) < 0)
				fail("R10-CHARM", "no '" + cap + "' row");
		}
		if (IsOracoolShardIdx(idx) && findRow("drop onto a backpack item to imbue it") < 0)
			fail("R10-SHARD", "no 'drop onto a backpack item to imbue it' row");
		if ((IsOracoolGemIdx(idx) || IsOracoolRuneIdx(idx) || IsOracoolJewelIdx(idx))
		    && findPrefix("In weapons:") < 0 && findPrefix("In shields:") < 0 && findPrefix("In armor and jewelry:") < 0)
			fail("R10-SOCKETABLE", "a gem, rune or jewel with no 'In weapons: / In shields: / In armor and jewelry:' row");
	}

	// ---- RG: generic hygiene, every row ----
	std::map<std::string, int> allowedRepeats;
	if (identified && item._iSocketCount > 0) {
		// Two identical stones print two identical socket lines - "Ber Ber" is a real word.
		const oracool::SocketHost host = oracool::SocketHostForItemType(item._itype);
		for (const uint16_t stone : item._iSocketed) {
			if (stone != devilution::Item::EmptySocket)
				allowedRepeats[oracool::GemSocketLine(stone, host)]++;
		}
	}
	std::map<std::string, int> seen;
	for (size_t i = 0; i < rows.size(); i++) {
		const std::string &t = rows[i].text;
		const std::string where = "row " + std::to_string(i);
		if (t.find_first_not_of(' ') == std::string::npos) {
			fail("RG-EMPTY", where + " is empty");
			continue;
		}
		if (t.find("Another ability") != std::string::npos || t.find("(NW)") != std::string::npos)
			fail("RG-PLACEHOLDER", where + " is the unprinted-power placeholder: '" + t + "'");
		if (t.find('{') != std::string::npos || t.find('}') != std::string::npos)
			fail("RG-BRACES", where + " has a leftover format brace: '" + t + "'");
		const int minusOne = MinusOneSeverity(t);
		if (minusOne == 2)
			fail("RG-MINUS-ONE", where + " prints -1 as a value: '" + t + "'");
		else if (minusOne == 1)
			warn("RG-MINUS-ONE", where + " carries a -1: '" + t + "'");
		if (t.size() > LongRow)
			warn("RG-LONG", where + " is " + std::to_string(t.size()) + " characters: '" + t + "'");
		if (i > 0)
			seen[t]++;
	}
	for (const auto &[text, count] : seen) {
		const auto allowed = allowedRepeats.find(text);
		const int limit = std::max(1, allowed == allowedRepeats.end() ? 0 : allowed->second);
		if (count > limit)
			fail("RG-DUPLICATE", "'" + text + "' is printed " + std::to_string(count) + " times");
	}
}

/** @brief The summary's family for a base row. */
std::string BaseFamily(const devilution::Item &item)
{
	const int idx = item.IDidx;
	if (IsOracoolGemIdx(idx))
		return "base: gem";
	if (IsOracoolRuneIdx(idx))
		return "base: rune";
	if (IsOracoolJewelIdx(idx))
		return "base: jewel";
	if (IsOracoolCharmIdx(idx))
		return "base: charm";
	if (IsOracoolShardIdx(idx))
		return "base: imbuement shard";
	if (IsOracoolSalvageIdx(idx))
		return "base: salvage material";
	if (IsOracoolSignetIdx(idx))
		return "base: signet";
	if (IsOracoolEncounterMapIdx(idx))
		return "base: sealed map";
	if (idx == IDI_ORACOOL_KEYSTONE)
		return "base: guardian keystone";
	if (IsJewellery(item))
		return "base: jewellery";
	if (item._iClass == ICLASS_QUEST)
		return "base: quest item";
	if (item._iClass == ICLASS_WEAPON)
		return "base: weapon";
	if (item._iClass == ICLASS_ARMOR)
		return "base: armour";
	const item_misc_id m = item._iMiscId;
	if ((m > IMISC_USEFIRST && m < IMISC_USELAST) || m == IMISC_ARENAPOT || m == IMISC_SPECELIX)
		return "base: potion / elixir";
	if (m == IMISC_SCROLL || m == IMISC_SCROLLT)
		return "base: scroll";
	if (m == IMISC_BOOK)
		return "base: book";
	if (m > IMISC_OILFIRST && m < IMISC_OILLAST)
		return "base: oil";
	if (m > IMISC_RUNEFIRST && m < IMISC_RUNELAST)
		return "base: trap rune";
	return "base: other misc";
}

std::string Escape(std::string text)
{
	for (char &c : text) {
		if (c == '`')
			c = '\'';
		if (c == '|')
			c = '/';
	}
	return text;
}

class TooltipSweep {
public:
	explicit TooltipSweep(bool mouseMode)
	    : mouseMode_(mouseMode)
	{
	}

	void Add(const std::string &category, const std::string &how, const devilution::Item &item)
	{
		if (item.isEmpty()) {
			Note(category, how + ": the path returned an empty item");
			return;
		}
		Made made;
		made.category = category;
		made.how = how;
		made.name = NameOf(item);
		bool coloursMatch = true;
		made.rows = CaptureTooltip(item, coloursMatch);
		CheckTooltip(item, made.rows, coloursMatch, mouseMode_, made);
		Touch(category);
		items_.push_back(std::move(made));
	}

	void Note(const std::string &category, const std::string &what)
	{
		Touch(category);
		notes_[category].push_back(what);
	}

	void Recipe(int recipe, bool ready, bool madeSomething)
	{
		RecipeTally &tally = recipes_[recipe];
		tally.tries++;
		if (!ready)
			tally.notReady++;
		else if (!madeSomething)
			tally.madeNothing++;
		else
			tally.ran++;
	}

	[[nodiscard]] size_t Total() const { return items_.size(); }

	[[nodiscard]] size_t FailedItems() const
	{
		return static_cast<size_t>(std::count_if(items_.begin(), items_.end(), [](const Made &m) { return !m.fails.empty(); }));
	}

	void WriteReports(const std::filesystem::path &report, const std::filesystem::path &all) const
	{
		std::ofstream out(report);
		out << "# Orcl tooltip sweep\n\n";
		out << "Every item the sweep could craft, hovered the way the backpack hovers it, checked against the rules of the "
		       "\"Orcl Item Tooltips\" page (test/oracool_tooltip_sweep_test.cpp, CheckTooltip). FAILs turn the test red; "
		       "WARNs only report.\n\n";
		out << "- Items made: " << items_.size() << "\n";
		out << "- Items with a FAIL: " << FailedItems() << "\n";
		out << "- Items with only WARNs: "
		    << std::count_if(items_.begin(), items_.end(), [](const Made &m) { return m.fails.empty() && !m.warns.empty(); }) << "\n";
		out << "- Mouse control mode: " << (mouseMode_ ? "on - the R9 use-hint rules were applied" : "COULD NOT BE ENTERED - the R9 use-hint rules were skipped") << "\n\n";

		out << "## Summary\n\n| Category | Made | FAIL | WARN | Not made |\n|---|---:|---:|---:|---:|\n";
		for (const std::string &category : order_) {
			size_t made = 0;
			size_t fails = 0;
			size_t warns = 0;
			for (const Made &m : items_) {
				if (m.category != category)
					continue;
				made++;
				if (!m.fails.empty())
					fails++;
				if (!m.warns.empty())
					warns++;
			}
			const auto notes = notes_.find(category);
			out << "| " << Escape(category) << " | " << made << " | " << fails << " | " << warns << " | "
			    << (notes == notes_.end() ? 0 : notes->second.size()) << " |\n";
		}

		std::map<std::string, size_t> byRule;
		for (const Made &m : items_) {
			std::set<std::string> rules;
			for (const std::string &f : m.fails)
				rules.insert("FAIL " + f.substr(0, f.find(':')));
			for (const std::string &w : m.warns)
				rules.insert("WARN " + w.substr(0, w.find(':')));
			for (const std::string &r : rules)
				byRule[r]++;
		}
		out << "\n## By rule\n\n| Rule | Items |\n|---|---:|\n";
		for (const auto &[rule, count] : byRule)
			out << "| " << rule << " | " << count << " |\n";

		out << "\n## Recipes (TransmuteLevskiGridWith)\n\n| # | Recipe | Tries | Made something | Not ready | Ready, made nothing |\n|---:|---|---:|---:|---:|---:|\n";
		for (int r = 0; r < oracool::CraftingRecipeCount; r++) {
			const auto tally = recipes_.find(r);
			const RecipeTally t = tally == recipes_.end() ? RecipeTally {} : tally->second;
			out << "| " << r << " | " << oracool::CraftingRecipeName(r) << " | " << t.tries << " | " << t.ran << " | " << t.notReady << " | " << t.madeNothing << " |\n";
		}

		out << "\n## Could not be made\n\n";
		if (notes_.empty())
			out << "Nothing.\n";
		for (const std::string &category : order_) {
			const auto notes = notes_.find(category);
			if (notes == notes_.end())
				continue;
			out << "### " << Escape(category) << "\n\n";
			for (const std::string &note : notes->second)
				out << "- " << Escape(note) << "\n";
			out << "\n";
		}

		out << "\n## FAIL and WARN items\n\n";
		for (const std::string &category : order_) {
			bool header = false;
			for (const Made &m : items_) {
				if (m.category != category || (m.fails.empty() && m.warns.empty()))
					continue;
				if (!header) {
					out << "### " << Escape(category) << "\n\n";
					header = true;
				}
				out << "#### " << Escape(m.name) << (m.fails.empty() ? " (WARN)" : " (FAIL)") << "\n\n";
				out << "- Made by: " << Escape(m.how) << "\n";
				for (const std::string &f : m.fails)
					out << "- **FAIL** " << Escape(f) << "\n";
				for (const std::string &w : m.warns)
					out << "- WARN " << Escape(w) << "\n";
				out << "\n| # | Row | Colour |\n|---:|---|---|\n";
				for (size_t i = 0; i < m.rows.size(); i++)
					out << "| " << i << " | `" << Escape(m.rows[i].text) << "` | " << ColorName(m.rows[i].color) << " |\n";
				out << "\n";
			}
		}

		std::ofstream every(all);
		every << "# Orcl tooltip sweep - every item\n\n" << items_.size() << " items, grouped by how they were made.\n";
		for (const std::string &category : order_) {
			every << "\n## " << Escape(category) << "\n";
			for (const Made &m : items_) {
				if (m.category != category)
					continue;
				every << "\n- **" << Escape(m.name) << "**" << (m.fails.empty() ? "" : " [FAIL]") << (m.warns.empty() ? "" : " [WARN]")
				      << " - " << Escape(m.how) << "\n";
				for (const TipRow &row : m.rows)
					every << "  - `" << Escape(row.text) << "` " << ColorName(row.color) << "\n";
			}
		}
	}

private:
	struct RecipeTally {
		int tries = 0;
		int ran = 0;
		int notReady = 0;
		int madeNothing = 0;
	};

	void Touch(const std::string &category)
	{
		if (std::find(order_.begin(), order_.end(), category) == order_.end())
			order_.push_back(category);
	}

	bool mouseMode_;
	std::vector<Made> items_;
	std::vector<std::string> order_;
	std::map<std::string, std::vector<std::string>> notes_;
	std::map<int, RecipeTally> recipes_;
};

/**
 * @brief A fresh hero for the whole sweep - the complete reset the backpack tests need (see
 * inv_test.cpp's SetUp: _pNumInv and InvGrid have no initialisers, so `Player {}` leaves them
 * holding garbage), at the level cap with stats that meet every requirement.
 */
devilution::Player &SweepHero()
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = devilution::Player {};
	MyPlayer = &player;
	InspectPlayer = MyPlayer;
	player._pClass = HeroClass::Warrior;
	player._pLevel = static_cast<int8_t>(MaxCharacterLevel);
	player._pStrength = player._pBaseStr = 500;
	player._pMagic = player._pBaseMag = 500;
	player._pDexterity = player._pBaseDex = 500;
	player._pVitality = player._pBaseVit = 500;
	player._pmode = PM_STAND;
	player._pMaxHP = player._pHitPoints = 100 << 6;
	player._pMaxHPBase = player._pHPBase = 100 << 6;
	player._pNumInv = 0;
	std::memset(player.InvGrid, 0, sizeof(player.InvGrid));
	player.InvTabList = {};
	player.InvTabGrid = {};
	player._pNumInvTab = {};
	ActiveInventoryTab = 0;
	return player;
}

/**
 * @brief Keyboard-and-mouse control mode, the way the game enters it: DetectInputMethod on a key
 * event. ControlMode itself is not exported to the test DLL. True when a probe potion then shows its
 * "Right-click to use" row - the only way to know the mode took.
 */
bool EnterMouseMode()
{
	// Set directly: DetectInputMethod resets the cursor on the way, which faults without a window (the first
	// run of this sweep died there, 0xc0000005, before a single item was made). Exported for this since v1.12.178.
	ControlMode = ControlTypes::KeyboardAndMouse;

	devilution::Item potion {};
	InitializeItem(potion, IDI_HEAL);
	potion._iIdentified = true;
	bool coloursMatch = true;
	for (const TipRow &row : CaptureTooltip(potion, coloursMatch)) {
		if (StartsWith(row.text, "Right-click to"))
			return true;
	}
	return false;
}

/** @brief The smith's route: seed, vendor item level, base attributes, the vendor's base-tier roll. */
devilution::Item MakeVendorBase(_item_indexes idx)
{
	devilution::Item item {};
	item._iSeed = AdvanceRndSeed();
	SetRndSeed(item._iSeed);
	oracool::StampVendorItemLevel(item, VendorLevel);
	GetItemAttrs(item, idx, VendorLevel);
	oracool::ApplyVendorTier(item, VendorLevel, item._iSeed, 0);
	item._iCreateInfo = VendorLevel;
	item._iIdentified = true;
	item._iStatFlag = MyPlayer->CanUseItem(item);
	return item;
}

/** @brief @p count units of @p idx in one stack, as a reagent sits in the grid. */
devilution::Item MakeStack(int idx, int count)
{
	devilution::Item item {};
	InitializeItem(item, static_cast<_item_indexes>(idx));
	GenerateNewSeed(item);
	item._iIdentified = true;
	item.setStackCount(count);
	return item;
}

std::string ItemLabel(int idx)
{
	const char *name = AllItemsList[static_cast<size_t>(idx)].iName;
	return std::string(name != nullptr ? name : "?") + " (IDI " + std::to_string(idx) + ")";
}

template <typename T>
std::vector<T> Sample(const std::vector<T> &from, size_t want)
{
	if (from.size() <= want)
		return from;
	std::vector<T> picked;
	const size_t step = from.size() / want;
	for (size_t i = 0; i < from.size() && picked.size() < want; i += step)
		picked.push_back(from[i]);
	return picked;
}

struct RecipeRun {
	bool ready = false;
	std::string result;
	std::vector<devilution::Item> outputs;
};

/** @brief One transmute on a fresh grid holding exactly @p inputs; everything the grid holds after is output. */
RecipeRun RunRecipe(int recipe, const std::vector<devilution::Item> &inputs)
{
	RecipeRun run;
	devilution::Item grid[oracool::LevskiGridSlots] {};
	for (size_t i = 0; i < inputs.size() && i < static_cast<size_t>(oracool::LevskiGridSlots); i++)
		grid[i] = inputs[i];
	run.ready = oracool::CanCraftFromLevskiGrid(grid, recipe);
	if (!run.ready)
		return run;
	run.result = oracool::TransmuteLevskiGridWith(grid, recipe);
	if (run.result.empty() || oracool::IsTransmuteRefusal(run.result))
		return run;
	for (const devilution::Item &item : grid) {
		if (!item.isEmpty())
			run.outputs.push_back(item);
	}
	return run;
}

std::string RecipeCategory(int recipe)
{
	std::string number = std::to_string(recipe);
	if (number.size() < 2)
		number = "0" + number;
	return "recipe " + number + " " + oracool::CraftingRecipeName(recipe);
}

/** @brief Runs @p recipe on @p inputs and sweeps every output. */
void Craft(TooltipSweep &sweep, int recipe, const std::vector<devilution::Item> &inputs, const std::string &what)
{
	const RecipeRun run = RunRecipe(recipe, inputs);
	sweep.Recipe(recipe, run.ready, !run.outputs.empty());
	const std::string category = RecipeCategory(recipe);
	if (!run.ready) {
		sweep.Note(category, "not ready on " + what);
		return;
	}
	if (run.outputs.empty()) {
		sweep.Note(category, "ready on " + what + " but made nothing ('" + run.result + "')");
		return;
	}
	for (const devilution::Item &output : run.outputs)
		sweep.Add(category, what + " -> '" + run.result + "'", output);
}

/** @brief Gillian's Reroll: the first affix, offered a different roll from the pool, then the rebuild her Option buttons run. */
bool MysticReroll(devilution::Item &item)
{
	const int count = item._iOracoolAffixCount;
	if (count <= 0)
		return false;
	std::array<item_effect_type, devilution::Item::MaxOracoolAffixes> exclude {};
	int excludeCount = 0;
	for (int i = 1; i < count; i++)
		exclude[excludeCount++] = item._iOracoolAffixes[i].type;
	OracoolAffix drawn;
	bool offered = false;
	for (int attempt = 0; attempt < 24 && !offered; attempt++) {
		if (!RollOracoolAffixFor(*MyPlayer, item, drawn, exclude.data(), excludeCount))
			break;
		offered = drawn.type != item._iOracoolAffixes[0].type || drawn.param1 != item._iOracoolAffixes[0].param1;
	}
	if (!offered)
		return false;
	std::array<OracoolAffix, devilution::Item::MaxOracoolAffixes> affixes {};
	for (int i = 0; i < count; i++)
		affixes[i] = i == 0 ? drawn : item._iOracoolAffixes[i];
	return RebuildOracoolItemWithAffixes(*MyPlayer, item, affixes.data(), count);
}

/** @brief One worn base and the variants made of it, kept for the recipes to sample. */
struct WornBase {
	int idx;
	devilution::Item normal;
	std::optional<devilution::Item> magic;
	std::optional<devilution::Item> rare;
	std::optional<devilution::Item> buffed;
	std::optional<devilution::Item> primal;
	std::optional<devilution::Item> ethereal;
	std::optional<devilution::Item> socketed;
};

} // namespace

TEST(OracoolTooltipSweep, EveryCraftableItemPrintsTheRowsTheTooltipPageAsksFor)
{
	const bool savedHellfire = gbIsHellfire;
	const bool savedMultiplayer = gbIsMultiplayer;
	const bool savedHeadless = HeadlessMode;
	const _difficulty savedDifficulty = sgGameInitInfo.nDifficulty;
	gbIsHellfire = true;
	gbIsMultiplayer = false;
	sgGameInitInfo.nDifficulty = DIFF_NORMAL;
	SetRndSeed(0x5EEDC0DEU);

	// The archives, as the audit tests mount them (MountTestArchives): item NAMES are fitted to the panel with the
	// game font (a staff's "Short Staff of Firebolt" faulted without it, v1.12.179), and item sizes read the cursor sprites.
	// Once per PROCESS: the shuffled run repeats this test in one binary, and a second LoadCoreArchives crashes it
	// (the audit tests' MountTestArchives note, QA-01).
	static const bool mounted = [] {
		LoadCoreArchives();
		LoadGameArchives();
		InitCursor();
		return true;
	}();
	(void)mounted;
	if (!HaveDiabdat())
		GTEST_SKIP() << "needs diabdat.mpq - item names and sizes are read from it";
	devilution::Player &hero = SweepHero();
	TooltipSweep sweep(EnterMouseMode());

	// Every socketable, for rotating one stone into each socketed copy.
	std::vector<int> stones;
	for (int i = IDI_GOLD + 1; i <= IDI_LAST; i++) {
		if (IsOracoolGemIdx(i) || IsOracoolRuneIdx(i) || IsOracoolJewelIdx(i))
			stones.push_back(i);
	}
	ASSERT_FALSE(stones.empty()) << "no gems, runes or jewels in the item table";
	const int perfectRuby = oracool::GemIndexFor(oracool::GemType::Ruby, oracool::GemQuality::Perfect);

	// ================= 1. Every base, and every variant of every worn base =================
	std::vector<WornBase> worn;
	std::set<int> oilKinds;
	std::set<int> bookSpells;
	size_t stoneTurn = 0;
	for (int i = IDI_GOLD + 1; i <= IDI_LAST; i++) {
		const auto idx = static_cast<_item_indexes>(i);
		const ItemData &data = AllItemsList[static_cast<size_t>(i)];
		if (data.iName == nullptr)
			continue;
		if (!IsItemAvailable(i)) {
			sweep.Note("base: skipped", ItemLabel(i) + ": IsItemAvailable says no");
			continue;
		}
		if (oracool::IsHiddenItemIdx(i)) {
			sweep.Note("base: skipped", ItemLabel(i) + ": a hidden class's base (oracool/hidden_classes.h)");
			continue;
		}
		if (i == IDI_EAR) {
			sweep.Note("base: skipped", ItemLabel(i) + ": a PvP trophy, built from a player's name - not craftable");
			continue;
		}
		if (data.iMiscId == IMISC_UNIQUE) {
			sweep.Note("base: skipped", ItemLabel(i) + ": a quest unique's base row, not a base - made by the Unique shelf below");
			continue;
		}

		// Starting gear (IDROP_NEVER, below the first quest item) exists only as InitializeItem makes it for a new hero;
		// the vendor route gave the Short Staff of Mana a spell name on top of its own ("... of Mana of Mana").
		// Item level 1, as CreatePlrItems stamps the starting gear since v1.12.180.
		devilution::Item normal = (data.iRnd == IDROP_NEVER && i < IDI_FIRSTQUEST) ? [&] { devilution::Item it {}; InitializeItem(it, idx); it._iOracoolItemLevel = 1; return it; }() : MakeVendorBase(idx);
		if (normal.isEmpty()) {
			sweep.Note("base: skipped", ItemLabel(i) + ": the vendor route built an empty item");
			continue;
		}
		if (idx == IDI_ORACOOL_KEYSTONE) {
			// A keystone's tier is stamped by the world drop (oracool/rift.cpp DropKeystone) and nowhere else.
			for (const int tier : { 1, 7, 25 }) {
				devilution::Item keystone = normal;
				keystone._iOracoolRiftTier = static_cast<uint8_t>(tier);
				sweep.Add(BaseFamily(keystone), "vendor base, rift tier " + std::to_string(tier) + " set as DropKeystone sets it", keystone);
			}
		} else if (normal._iMiscId > IMISC_OILFIRST && normal._iMiscId < IMISC_OILLAST) {
			// Every oil kind: the oil's kind is rolled inside GetItemAttrs, so re-roll until each has shown.
			for (int attempt = 0; attempt < 300; attempt++) {
				devilution::Item oil = attempt == 0 ? normal : MakeVendorBase(idx);
				if (!oilKinds.insert(oil._iMiscId).second)
					continue;
				sweep.Add(BaseFamily(oil), "vendor base (oil kind rolled by GetItemAttrs)", oil);
				devilution::Item stack = oil;
				stack.setStackCount(5);
				sweep.Add("stack of 5", "vendor base, setStackCount(5)", stack);
			}
			continue;
		} else if (normal._iMiscId == IMISC_BOOK) {
			// Every book spell the vendor level reaches: the spell is rolled inside GetItemAttrs.
			for (int attempt = 0; attempt < 400; attempt++) {
				devilution::Item book = attempt == 0 ? normal : MakeVendorBase(idx);
				if (!bookSpells.insert(static_cast<int>(book._iSpell)).second)
					continue;
				sweep.Add(BaseFamily(book), "vendor base (spell rolled by GetItemAttrs)", book);
			}
			continue;
		} else {
			sweep.Add(BaseFamily(normal), "vendor base: GetItemAttrs + ApplyVendorTier at vendor level " + std::to_string(VendorLevel), normal);
		}
		if (normal.isStackableConsumable()) {
			devilution::Item stack = normal;
			stack.setStackCount(5);
			sweep.Add("stack of 5", "vendor base, setStackCount(5)", stack);
		}
		if (!IsWorn(normal) || normal._itype == ItemType::Misc || normal._itype == ItemType::Gold)
			continue;

		WornBase base { i, normal, {}, {}, {}, {}, {}, {} };
		const std::string label = ItemLabel(i);

		// MAGIC - Wirt's gamble on this base, which rolls onlygood; retried until the roll is plain magic.
		for (int attempt = 0; attempt < 30 && !base.magic; attempt++) {
			devilution::Item magic {};
			RollGambleResult(magic, idx, VendorLevel);
			if (!magic.isEmpty() && magic._iMagical == ITEM_QUALITY_MAGIC && !magic.hasOracoolTier() && !magic._iOracoolEthereal)
				base.magic = magic;
		}
		if (base.magic) {
			sweep.Add("magic", "Wirt's gamble (RollGambleResult) on " + label, *base.magic);
			devilution::Item unidentified = *base.magic;
			unidentified._iIdentified = false;
			sweep.Add("magic, unidentified", "Wirt's gamble on " + label + ", left unidentified", unidentified);
		} else {
			sweep.Note("magic", label + ": thirty gambles never came out plain magic");
		}

		// The three rolled tiers - SetupAllItems' forced tier, through RetierOracoolItem.
		const std::pair<OracoolItemTier, const char *> tiers[] = {
			{ OracoolItemTier::Rare, "rare" },
			{ OracoolItemTier::BuffedUnique, "buffed unique" },
			{ OracoolItemTier::Primal, "primal" },
		};
		for (const auto &[tier, word] : tiers) {
			devilution::Item tiered = normal;
			if (!RetierOracoolItem(tiered, tier)) {
				sweep.Note(word, label + ": RetierOracoolItem could not apply the tier");
				continue;
			}
			sweep.Add(word, std::string("RetierOracoolItem(") + word + ") on the vendor base " + label, tiered);
			if (tier == OracoolItemTier::Rare)
				base.rare = tiered;
			else if (tier == OracoolItemTier::BuffedUnique)
				base.buffed = tiered;
			else
				base.primal = tiered;
		}

		// ETHEREAL - the bargain every path shares.
		{
			devilution::Item ethereal = normal;
			if (MakeItemEthereal(ethereal)) {
				sweep.Add("ethereal", "MakeItemEthereal on the vendor base " + label, ethereal);
				base.ethereal = ethereal;
			} else {
				sweep.Note("ethereal", label + ": MakeItemEthereal declines it");
			}
		}

		// SOCKETED - Punch Sockets (recipe 17: one perfect gem per socket), then one stone seated.
		{
			const RecipeRun punched = RunRecipe(17, { normal, MakeStack(perfectRuby, devilution::Item::MaxItemSockets) });
			sweep.Recipe(17, punched.ready, !punched.outputs.empty());
			devilution::Item host {};
			for (const devilution::Item &out : punched.outputs) {
				if (out.IDidx == normal.IDidx && out._iSocketCount > 0)
					host = out;
			}
			if (host.isEmpty()) {
				sweep.Note(RecipeCategory(17), label + ": Punch Sockets " + (punched.ready ? "made nothing" : "not ready"));
			} else {
				sweep.Add(RecipeCategory(17), "Punch Sockets on the vendor base " + label, host);
				const int stoneIdx = stones[stoneTurn++ % stones.size()];
				devilution::Item stone = MakeStack(stoneIdx, 1);
				if (oracool::TrySocketGem(host, stone)) {
					oracool::TryCompleteRuneword(host);
					sweep.Add("socketed", "Punch Sockets on " + label + ", then TrySocketGem(" + ItemLabel(stoneIdx) + ")", host);
					base.socketed = host;
				} else {
					sweep.Note("socketed", label + ": TrySocketGem refused " + ItemLabel(stoneIdx));
				}
			}
		}
		worn.push_back(std::move(base));
	}
	ASSERT_FALSE(worn.empty()) << "no worn base was made - the base walk is broken, not the tooltips";

	// ================= 2. The Unique shelf =================
	std::vector<devilution::Item> uniques;
	hero._pClass = HeroClass::Necromancer; // the shrunken heads' uniques are shelved for a Necromancer only
	// The table ends in an empty UITYPE_INVALID row that UniqueItemCount counts; it is not a unique.
	for (size_t u = 0; u + 1 < UniqueItemCount; u++) {
		const std::string label = std::string(UniqueItems[u].UIName != nullptr ? UniqueItems[u].UIName : "?") + " (uid " + std::to_string(u) + ")";
		if (!IsUniqueAvailable(static_cast<int>(u))) {
			sweep.Note("unique", label + ": IsUniqueAvailable says no");
			continue;
		}
		devilution::Item unique {};
		if (!CreateUniqueVendorItem(hero, unique, static_cast<_unique_items>(u))) {
			sweep.Note("unique", label + ": CreateUniqueVendorItem declines it (hidden class, or no base)");
			continue;
		}
		sweep.Add("unique", "the Unique shelf (CreateUniqueVendorItem) - " + label, unique);
		uniques.push_back(unique);
	}
	hero._pClass = HeroClass::Warrior;

	// ================= 3. The Set shelf, walked until it has nothing left =================
	std::vector<devilution::Item> setPieces;
	std::set<const oracool::SetItemDefinition *> shelved;
	const auto alreadyStocked = [&shelved](const oracool::SetItemDefinition &def) { return shelved.count(&def) != 0; };
	for (size_t guard = 0; guard <= oracool::ItemSetItemCount; guard++) {
		devilution::Item piece {};
		const oracool::SetItemDefinition *chosen = nullptr;
		if (!CreateSetVendorItem(hero, piece, 40, alreadyStocked, &chosen) || chosen == nullptr)
			break;
		shelved.insert(chosen);
		sweep.Add("set piece", std::string("the Set shelf (CreateSetVendorItem) - ") + chosen->id, piece);
		setPieces.push_back(piece);
	}
	for (size_t p = 0; p < oracool::ItemSetItemCount; p++) {
		const oracool::SetItemDefinition &def = oracool::ItemSetItems[p];
		if (shelved.count(&def) == 0)
			sweep.Note("set piece", std::string(def.id) + ": the shelf never offered it (no base for its slot: " + def.slot + ")");
	}

	// ================= 4. Every runeword, rune by rune =================
	for (size_t w = 0; w < oracool::RunewordCount(); w++) {
		const oracool::RunewordDefinition *word = oracool::RunewordAt(w);
		if (word == nullptr)
			break;
		const WornBase *host = nullptr;
		for (const WornBase &base : worn) {
			const devilution::Item &n = base.normal;
			if (n._iMagical != ITEM_QUALITY_NORMAL)
				continue;
			if (static_cast<uint8_t>(oracool::RunewordHostForItemType(n._itype)) != word->host)
				continue;
			if (!oracool::CanItemHaveSockets(n) || oracool::MaxSocketsForItem(n) < word->runeCount)
				continue;
			host = &base;
			break;
		}
		if (host == nullptr) {
			sweep.Note("runeword", std::string(word->name) + ": no plain base of its host holds " + std::to_string(word->runeCount) + " sockets");
			continue;
		}
		devilution::Item item = host->normal;
		// The socket count is the drop's own stamp (TryAddSocketsToDroppedItem); a word needs its host to match it exactly.
		item._iSocketCount = static_cast<uint8_t>(word->runeCount);
		item.normalizeSockets();
		bool seated = true;
		for (int r = 0; r < word->runeCount; r++)
			seated = seated && oracool::TrySocketGem(item, MakeStack(word->runes[r], 1));
		const bool formed = seated && oracool::TryCompleteRuneword(item) && oracool::GetActiveRuneword(item) == word;
		if (!formed) {
			sweep.Note("runeword", std::string(word->name) + ": the runes did not form the word on " + ItemLabel(host->idx));
			continue;
		}
		sweep.Add("runeword", std::string(word->name) + " on " + ItemLabel(host->idx) + ": TrySocketGem x" + std::to_string(word->runeCount) + " + TryCompleteRuneword", item);
	}

	// ================= 5. Every recipe, through the grid every artisan runs =================
	{
		using oracool::GemQuality;
		using oracool::GemType;
		// 0 Refine Gems - three of every gem below perfect.
		for (size_t t = 0; t < oracool::GemTypeCount; t++) {
			for (size_t q = 0; q + 1 < oracool::GemQualityCount; q++) {
				const int gem = oracool::GemIndexFor(static_cast<GemType>(t), static_cast<GemQuality>(q));
				Craft(sweep, 0, { MakeStack(gem, 3) }, "3 x " + ItemLabel(gem));
			}
		}
		// 1 Ascend Runes - two of every rune below Zod.
		for (size_t p = 0; p + 1 < oracool::RuneLadderSize(); p++) {
			const int rune = oracool::RuneAtLadderPosition(p);
			Craft(sweep, 1, { MakeStack(rune, 2) }, "2 x " + ItemLabel(rune));
		}
		// 2 Rework Charms - the six basic stat charms, in pairs.
		const int basicCharms[] = { IDI_ORACOOL_CHARM_VIGOR, IDI_ORACOOL_CHARM_EMBERS, IDI_ORACOOL_CHARM_STORMS,
			IDI_ORACOOL_CHARM_FORTUNE, IDI_ORACOOL_CHARM_LUCK, IDI_ORACOOL_CHARM_GREED };
		for (int c = 0; c < 6; c++) {
			const int a = basicCharms[c];
			const int b = basicCharms[(c + 1) % 6];
			Craft(sweep, 2, { MakeStack(a, 1), MakeStack(b, 1) }, ItemLabel(a) + " + " + ItemLabel(b));
		}
		// 3 Free the Sockets - a few socketed hosts.
		std::vector<devilution::Item> socketedHosts;
		for (const WornBase &base : worn) {
			if (base.socketed && base.socketed->socketedCount() > 0)
				socketedHosts.push_back(*base.socketed);
		}
		for (const devilution::Item &host : Sample(socketedHosts, 6))
			Craft(sweep, 3, { host }, "socketed " + NameOf(host));
		// 4 Temper Jewels - three of every jewel below radiant.
		for (int j = IDI_GOLD + 1; j <= IDI_LAST; j++) {
			if (IsOracoolJewelIdx(j) && !oracool::IsTopJewel(static_cast<uint16_t>(j)))
				Craft(sweep, 4, { MakeStack(j, 3) }, "3 x " + ItemLabel(j));
		}

		// The one-item-plus-reagent recipes, on a deterministic sample of the worn bases.
		const std::vector<WornBase> sample = Sample(worn, RecipeSample);
		const auto withReagent = [](const devilution::Item &target, int reagent, int count) {
			return std::vector<devilution::Item> { target, MakeStack(reagent, count) };
		};
		for (const WornBase &base : sample) {
			const std::string label = ItemLabel(base.idx);
			if (base.magic) {
				Craft(sweep, 5, withReagent(*base.magic, IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, 3), "magic " + label + " + 3 Unique Encrustments");
				Craft(sweep, 9, withReagent(*base.magic, IDI_ORACOOL_SALVAGE_MAGIC_POWDER, 4), "magic " + label + " + 4 Magic Powder");
			}
			Craft(sweep, 9, withReagent(base.normal, IDI_ORACOOL_SALVAGE_MAGIC_POWDER, 4), "basic " + label + " + 4 Magic Powder");
			if (base.rare) {
				if (HasUniqueForBaseOf(*base.rare))
					Craft(sweep, 6, withReagent(*base.rare, IDI_ORACOOL_SALVAGE_RARE_FIBRES, 5), "rare " + label + " + 5 Rare Fibres");
				Craft(sweep, 10, withReagent(*base.rare, IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS, 6), "rare " + label + " + 6 Set Engravings");
				Craft(sweep, 12, withReagent(*base.rare, IDI_ORACOOL_SALVAGE_RARE_FIBRES, 3), "rare " + label + " + 3 Rare Fibres");
			}
			if (base.buffed)
				Craft(sweep, 11, withReagent(*base.buffed, IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, 8), "buffed unique " + label + " + 8 Unique Encrustments");
			if (base.primal)
				Craft(sweep, 14, withReagent(*base.primal, IDI_ORACOOL_SALVAGE_PRIMAL_VINES, 6), "primal " + label + " + 6 Primal Vines");
			Craft(sweep, 15, withReagent(base.normal, IDI_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES, 5), "basic " + label + " + 5 Ethereal Imbueities");
			if (base.ethereal) {
				devilution::Item worn_down = *base.ethereal;
				worn_down._iDurability = 1; // wear - no path reaches it from here
				Craft(sweep, 16, withReagent(worn_down, IDI_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES, 12), "worn-down ethereal " + label + " + 12 Ethereal Imbueities");
			}
			if (oracool::RequiredLevel(base.normal) > 1)
				Craft(sweep, oracool::UnbindLevelRecipe, withReagent(base.normal, IDI_ORACOOL_SHARD_EASE, 1), "basic " + label + " + Shard of Ease");
		}
		// 22-25 the four crafts: a plain wearable, a jewel, a rune, a perfect gem.
		const std::vector<WornBase> craftSample = Sample(worn, 8);
		for (int recipe = oracool::CraftBloodRecipe; recipe <= oracool::CraftSafetyRecipe; recipe++) {
			for (const WornBase &base : craftSample) {
				Craft(sweep, recipe,
				    { base.normal, MakeStack(IDI_ORACOOL_JEWEL_FERVOR_FLAWED, 1), MakeStack(IDI_ORACOOL_RUNE_EL, 1), MakeStack(perfectRuby, 1) },
				    "basic " + ItemLabel(base.idx) + " + jewel + El + perfect ruby");
			}
		}
		// 11 and 13 on the Unique shelf's gear.
		std::vector<devilution::Item> uniqueGear;
		for (const devilution::Item &unique : uniques) {
			if (unique._iClass == ICLASS_WEAPON || unique._iClass == ICLASS_ARMOR)
				uniqueGear.push_back(unique);
		}
		for (const devilution::Item &unique : Sample(uniqueGear, RecipeSample)) {
			Craft(sweep, 11, withReagent(unique, IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, 8), "unique " + NameOf(unique) + " + 8 Unique Encrustments");
			Craft(sweep, 13, withReagent(unique, IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, 4), "unique " + NameOf(unique) + " + 4 Unique Encrustments");
		}
		// 7 Recast, one piece of every set the shelf sold.
		std::set<std::string> recastSets;
		for (const devilution::Item &piece : setPieces) {
			const oracool::SetItemDefinition *def = oracool::FindSetItemByCursor(piece._iCurs);
			const oracool::ItemSetDefinition *set = def != nullptr ? oracool::FindItemSetOwning(def->id) : nullptr;
			if (set == nullptr || !recastSets.insert(set->id).second)
				continue;
			Craft(sweep, 7, withReagent(piece, IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS, 3), "set piece " + NameOf(piece) + " + 3 Set Engravings");
		}
		// 8 Recolour Gems, one ruby of every quality.
		for (size_t q = 0; q < oracool::GemQualityCount; q++) {
			const int gem = oracool::GemIndexFor(GemType::Ruby, static_cast<GemQuality>(q));
			Craft(sweep, 8, withReagent(MakeStack(gem, 1), IDI_ORACOOL_SALVAGE_MAGIC_POWDER, 2), ItemLabel(gem) + " + 2 Magic Powder");
		}
		// 19 and 20 the Horadric potions.
		int rejuv = -1;
		int fullRejuv = -1;
		for (int i = IDI_GOLD + 1; i <= IDI_LAST; i++) {
			if (rejuv < 0 && AllItemsList[static_cast<size_t>(i)].iMiscId == IMISC_REJUV)
				rejuv = i;
			if (fullRejuv < 0 && AllItemsList[static_cast<size_t>(i)].iMiscId == IMISC_FULLREJUV)
				fullRejuv = i;
		}
		Craft(sweep, oracool::RejuvenationRecipe, { MakeStack(IDI_HEAL, 3), MakeStack(IDI_MANA, 3) }, "3 healing + 3 mana potions");
		if (rejuv >= 0)
			Craft(sweep, oracool::FullRejuvenationRecipe, { MakeStack(rejuv, 3) }, "3 x " + ItemLabel(rejuv));
		(void)fullRejuv;
		// 26 and 27 the material ladder.
		const int ladder[] = { IDI_ORACOOL_SALVAGE_WHITE_SCALES, IDI_ORACOOL_SALVAGE_MAGIC_POWDER, IDI_ORACOOL_SALVAGE_RARE_FIBRES,
			IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS, IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, IDI_ORACOOL_SALVAGE_PRIMAL_VINES };
		for (size_t rung = 0; rung < 6; rung++) {
			if (rung + 1 < 6)
				Craft(sweep, oracool::RefineMaterialsRecipe, { MakeStack(ladder[rung], 3) }, "3 x " + ItemLabel(ladder[rung]));
			if (rung > 0)
				Craft(sweep, oracool::BreakDownMaterialsRecipe, { MakeStack(ladder[rung], 1) }, "1 x " + ItemLabel(ladder[rung]));
		}

	// ================= 6. Gillian's Mystic: Imbue every kind, Remove, Reroll; and Cleanse (recipe 18) =================
		std::vector<devilution::Item> imbued;
		for (int k = 0; k < oracool::ShardKindCount; k++) {
			const auto kind = static_cast<oracool::ShardKind>(k);
			const int shardIdx = oracool::ShardDef(kind).itemIndex;
			bool done = false;
			for (const WornBase &base : worn) {
				devilution::Item target = base.normal;
				if (!oracool::CanReceiveShardKind(target, kind))
					continue;
				if (!oracool::TryImbue(hero, target, MakeStack(shardIdx, 1)))
					continue;
				oracool::TryImbue(hero, target, MakeStack(shardIdx, 1)); // a second, where the kind's limit allows it
				sweep.Add("imbued (Mystic: Imbue)", "TryImbue x2 " + ItemLabel(shardIdx) + " on " + ItemLabel(base.idx), target);
				imbued.push_back(target);
				done = true;
				break;
			}
			if (!done)
				sweep.Note("imbued (Mystic: Imbue)", ItemLabel(shardIdx) + ": no worn base accepts it");
		}
		// Three kinds on one item, then the first drawn out - her Remove button's Capture/Strip/Restore.
		for (const WornBase &base : Sample(worn, 6)) {
			devilution::Item target = base.normal;
			int taken = 0;
			for (int k = 0; k < oracool::ShardKindCount && taken < 3; k++) {
				const auto kind = static_cast<oracool::ShardKind>(k);
				if (oracool::CanReceiveShardKind(target, kind) && oracool::TryImbue(hero, target, MakeStack(oracool::ShardDef(kind).itemIndex, 1)))
					taken++;
			}
			if (taken < 2)
				continue;
			sweep.Add("imbued (Mystic: Imbue)", std::to_string(taken) + " kinds of shard on " + ItemLabel(base.idx), target);
			imbued.push_back(target);
			const oracool::ImbuementLedger ledger = oracool::CaptureImbuements(target);
			oracool::ImbuementLedger kept;
			for (int i = 1; i < ledger.count; i++)
				kept.kinds[kept.count++] = ledger.kinds[i];
			devilution::Item removed = target;
			oracool::StripImbuements(removed);
			oracool::RestoreImbuements(removed, kept);
			sweep.Add("Mystic: Remove", "the first of " + std::to_string(taken) + " shards drawn out of " + ItemLabel(base.idx), removed);
		}
		for (const devilution::Item &item : Sample(imbued, 8))
			Craft(sweep, 18, { item }, "imbued " + NameOf(item));
		// Reroll: magic and rare, one affix each.
		for (const WornBase &base : sample) {
			for (const std::optional<devilution::Item> *from : { &base.magic, &base.rare }) {
				if (!*from)
					continue;
				devilution::Item item = **from;
				if (MysticReroll(item))
					sweep.Add("Mystic: Reroll", "RollOracoolAffixFor + RebuildOracoolItemWithAffixes on " + NameOf(**from), item);
				else
					sweep.Note("Mystic: Reroll", NameOf(**from) + ": no affix to reroll, or the pool offered nothing new");
			}
		}
	}

	// ================= Report =================
	const std::filesystem::path report = std::filesystem::absolute("tooltip_sweep_report.md");
	const std::filesystem::path all = std::filesystem::absolute("tooltip_sweep_all.md");
	sweep.WriteReports(report, all);

	ClearUniqueItemFlags();
	ClearPanelStrings();
	HeadlessMode = savedHeadless;
	gbIsHellfire = savedHellfire;
	gbIsMultiplayer = savedMultiplayer;
	sgGameInitInfo.nDifficulty = savedDifficulty;

	EXPECT_GT(sweep.Total(), 1000u) << "the sweep made too few items to mean anything";
	EXPECT_EQ(sweep.FailedItems(), 0u) << sweep.FailedItems() << " of " << sweep.Total()
	                                   << " tooltips break the spec - every one, with its rows, is in " << report.string();
}
