/**
 * @file oracool_skill_rules_test.cpp
 *
 * THE TWO SKILL RULES (user, 2026-09-26):
 *   1. "all skills must provide main skill + a level up increase of some sort."
 *   2. "all skills pop-up tooltip must declare the main stat they provide and the level up buff they provide."
 * and the check the user asked for on top: "simulate a virtual hero of each class and build up each of their skill
 * to its maximum level and make sure each level of each skill actually levels up the level up stat ... confirm their
 * tooltips align with the level up stats. i dont want any missalignmens or discrepancies."
 *
 * So: one hero per visible class at level 99, and for every row of that class's tree, points bought one at a time
 * through InvestClassTreePoint - the game's own path, with its tier gate and the Rule of Rangs - up to the row's cap.
 * At EVERY rank:
 *   - the level-up stat (if the row has one) is larger than at the rank before, in every field it touches;
 *   - the hero really receives it: ApplyClassTreeToTotals - what the character sheet sums - carries at least it;
 *   - its tooltip line quotes exactly the numbers the hero receives;
 *   - the hover's "Current Skill Level" block holds this rank's line, and "Next Level" the next rank's;
 *   - the block states a MAIN effect: at least one line that is neither the cost nor the level-up stat;
 *   - a row with no level-up stat has a main effect whose numbers change from this rank to the next.
 * Passive Skills page rows are one rank and exempt from growing (user, 2026-09-26) but must print what they give.
 * Book rows (spells raised by books, not points) must show the Spells sheet's numbers level by level.
 * The hidden Bard is exempt (oracool/hidden_classes.h), as are rows off every page and rows still inert.
 *
 * OUTPUT: skill_rules_report.md in the working directory - every row, its cap, its level-up stat and its first main
 * line at rank 1 and at the cap, and every discrepancy found.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "init.h" // gbIsHellfire
#include "items.h"
#include "oracool/class_tree.h"
#include "oracool/hidden_classes.h"
#include "oracool/skill_facts.h"
#include "oracool/stat_sheet.h"
#include "player.h"
#include "spelldat.h"
#include "spells.h"

using namespace devilution;
using oracool::ClassTreeKind;
using oracool::ClassTreeSkill;

namespace {

struct Field {
	const char *name;
	int value;
};

/** @brief Every number a totals carries, life and mana in whole points, as a player reads them. */
std::vector<Field> Fields(const oracool::ItemBonusTotals &t)
{
	return {
		{ "minDamage", t.minDamage },
		{ "maxDamage", t.maxDamage },
		{ "armor", t.armor },
		{ "bonusDamage", t.bonusDamage },
		{ "bonusToHit", t.bonusToHit },
		{ "bonusArmor", t.bonusArmor },
		{ "strength", t.strength },
		{ "magic", t.magic },
		{ "dexterity", t.dexterity },
		{ "vitality", t.vitality },
		{ "fireResist", t.fireResist },
		{ "lightningResist", t.lightningResist },
		{ "magicResist", t.magicResist },
		{ "damageMod", t.damageMod },
		{ "getHit", t.getHit },
		{ "lightRadius", t.lightRadius },
		{ "hitPoints", t.hitPoints / 64 },
		{ "mana", t.mana / 64 },
		{ "spellLevelAdd", t.spellLevelAdd },
		{ "enhancedAccuracy", t.enhancedAccuracy },
		{ "fireMin", t.fireMin },
		{ "fireMax", t.fireMax },
		{ "lightningMin", t.lightningMin },
		{ "lightningMax", t.lightningMax },
		{ "magicFind", t.magicFind },
		{ "goldFind", t.goldFind },
		{ "moveSpeed", t.moveSpeed },
		{ "fastCast", t.fastCast },
		{ "armorPercent", t.armorPercent },
	};
}

std::vector<std::string> Lines(const std::string &text)
{
	std::vector<std::string> out;
	std::istringstream in(text);
	for (std::string line; std::getline(in, line);) {
		if (!line.empty())
			out.push_back(line);
	}
	return out;
}

bool HasLine(const std::string &text, const std::string &line)
{
	const std::vector<std::string> lines = Lines(text);
	return std::find(lines.begin(), lines.end(), line) != lines.end();
}

/** @brief Every whole number written in @p text, signs dropped ("-5 damage taken" gives 5). */
std::set<int> NumbersIn(const std::string &text)
{
	std::set<int> out;
	for (size_t i = 0; i < text.size();) {
		if (std::isdigit(static_cast<unsigned char>(text[i])) != 0) {
			size_t end = i;
			while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])) != 0)
				end++;
			out.insert(std::atoi(text.substr(i, end - i).c_str()));
			i = end;
		} else {
			i++;
		}
	}
	return out;
}

bool IsCostLine(const std::string &line)
{
	for (const char *prefix : { "Mana Cost", "Rage Cost", "Generates ", "Essence Cost" }) {
		if (line.rfind(prefix, 0) == 0)
			return true;
	}
	return false;
}

/** @brief The block's main-effect lines: everything but the cost and the level-up stat. */
std::vector<std::string> MainLines(const std::string &block, const std::string &levelUpLine)
{
	std::vector<std::string> out;
	for (const std::string &line : Lines(block)) {
		if (IsCostLine(line) || line == levelUpLine)
			continue;
		out.push_back(line);
	}
	return out;
}

std::string Join(const std::vector<std::string> &lines)
{
	std::string out;
	for (const std::string &line : lines)
		out += (out.empty() ? "" : " | ") + line;
	return out;
}

const char *ClassName(HeroClass heroClass)
{
	switch (heroClass) {
	case HeroClass::Warrior:
		return "Paladin";
	case HeroClass::Rogue:
		return "Rogue";
	case HeroClass::Sorcerer:
		return "Sorceress";
	case HeroClass::Monk:
		return "Monk";
	case HeroClass::Bard:
		return "Bard";
	case HeroClass::Barbarian:
		return "Barbarian";
	case HeroClass::Necromancer:
		return "Necromancer";
	}
	return "?";
}

const char *KindName(ClassTreeKind kind)
{
	switch (kind) {
	case ClassTreeKind::Active:
		return "active";
	case ClassTreeKind::Aura:
		return "aura";
	case ClassTreeKind::Passive:
		return "passive";
	}
	return "?";
}

/** @brief A fresh level-99 hero of @p heroClass holding a shield (Smite and Blessed Shield ask for one) and nothing invested. */
devilution::Player &ResetHero(HeroClass heroClass)
{
	devilution::Player &hero = Players[0];
	hero._pClass = heroClass;
	hero._pLevel = MaxCharacterLevel;
	hero._pUnspentSkillPoints = 60000;
	hero._pHitPoints = hero._pMaxHP = hero._pHPBase = hero._pMaxHPBase = 500 << 6;
	hero._pMana = hero._pMaxMana = hero._pManaBase = hero._pMaxManaBase = 500 << 6;
	std::fill(std::begin(hero._pSkillInvestment), std::end(hero._pSkillInvestment), 0);
	std::fill(std::begin(hero._pClassTreeInvestment), std::end(hero._pClassTreeInvestment), 0);
	std::fill(std::begin(hero._pSplLvl), std::end(hero._pSplLvl), 0);
	hero._pISplLvlAdd = 0;
	hero._pOracoolActiveAura = static_cast<uint16_t>(ClassTreeSkill::None);
	for (int slot = 0; slot < static_cast<int>(oracool::PassiveSlotCount); slot++)
		oracool::ClearPassiveSlot(hero, slot);
	for (devilution::Item &item : hero.InvBody)
		item.clear();
	devilution::Item &shield = hero.InvBody[INVLOC_HAND_LEFT];
	shield._itype = ItemType::Shield;
	shield._iClass = ICLASS_ARMOR;
	shield._iLoc = ILOC_ONEHAND;
	shield._iStatFlag = true;
	shield._iDurability = shield._iMaxDur = 60;
	return hero;
}

struct Report {
	std::vector<std::string> rows;
	std::vector<std::string> failures;
	std::vector<std::string> warnings;

	void Fail(const std::string &what)
	{
		failures.push_back(what);
	}
};

} // namespace

TEST(OracoolSkillRules, EveryHeroEveryRankGrowsAndSaysSo)
{
	const bool savedHellfire = gbIsHellfire;
	gbIsHellfire = true; // the Oracool spells sit past LastDiablo, which IsValidSpell gates on
	Players.resize(2);
	devilution::Player &bystander = Players[1];
	bystander = {};
	Report report;
	int ranksChecked = 0;
	int rowsChecked = 0;

	for (HeroClass heroClass : { HeroClass::Warrior, HeroClass::Rogue, HeroClass::Sorcerer, HeroClass::Monk,
	         HeroClass::Barbarian, HeroClass::Necromancer, HeroClass::Bard }) {
		if (!oracool::ClassHasTree(heroClass))
			continue;
		const bool hidden = oracool::IsClassHidden(heroClass);
		for (size_t i = 0; i < oracool::ClassTreeSkillCount; i++) {
			const auto skill = static_cast<ClassTreeSkill>(i);
			const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skill);
			if (data.heroClass != heroClass)
				continue;
			const std::string name = std::string(ClassName(heroClass)) + " / " + data.name;
			if (hidden) {
				report.rows.push_back("| " + name + " | - | - | exempt: hidden class | | |");
				continue;
			}
			if (data.page == oracool::RetiredFromTreePage) {
				report.rows.push_back("| " + name + " | - | - | exempt: off every page | | |");
				continue;
			}
			if (!data.implemented) {
				report.rows.push_back("| " + name + " | - | - | exempt: inert | | |");
				continue;
			}
			devilution::Player &hero = ResetHero(heroClass);
			MyPlayer = &bystander; // buying points for MyPlayer logs and plays sounds; the tooltip reads MyPlayer below

			// ---- a Passive Skills page row: one rank, no growth asked, but it must say what it gives ----
			if (oracool::IsPassiveSkillRow(skill)) {
				MyPlayer = &hero;
				const std::string block = oracool::ClassTreeRankBlock(hero, skill, 1);
				const std::string tip = oracool::ClassTreeEffectLine(hero, skill, true);
				const std::vector<std::string> main = MainLines(block, "");
				if (main.empty())
					report.Fail(name + ": Passive Skills row prints no effect");
				for (const std::string &line : Lines(block)) {
					if (!HasLine(tip, line))
						report.Fail(name + ": its effect line \"" + line + "\" is not in the hover");
				}
				report.rows.push_back("| " + name + " | passive (page) | 1 | - | " + (main.empty() ? "**NONE**" : main.front()) + " | " + (main.empty() ? "FAIL" : "ok") + " |");
				rowsChecked++;
				continue;
			}

			// ---- a book row: its numbers are the Spells sheet's, level by level ----
			if (oracool::IsClassTreeRowRetiredAsSpell(skill)) {
				const SpellID spell = oracool::ClassTreeSpellId(skill);
				MyPlayer = &hero;
				int flat = 0;
				for (int level = 1; level < MaxSpellLevel; level++) {
					hero._pSplLvl[static_cast<size_t>(spell)] = static_cast<uint8_t>(level);
					const std::string tip = oracool::ClassTreeEffectLine(hero, skill, true);
					const std::string now = oracool::SpellLevelLines(hero, spell, level);
					const std::string next = oracool::SpellLevelLines(hero, spell, level + 1);
					if (tip.find("Current Spell Level: " + std::to_string(level)) == std::string::npos)
						report.Fail(name + " level " + std::to_string(level) + ": the hover does not say the spell level");
					for (const std::string &line : Lines(now + "\n" + next)) {
						if (!HasLine(tip, line))
							report.Fail(name + " level " + std::to_string(level) + ": \"" + line + "\" missing from the hover");
					}
					if (now == next)
						flat++;
					ranksChecked++;
				}
				if (flat > 0)
					report.warnings.push_back(name + ": " + std::to_string(flat) + " spell levels change nothing on the sheet (book spell; cheaper mana counts, user 2026-09-26)");
				report.rows.push_back("| " + name + " | book spell | " + std::to_string(MaxSpellLevel) + " | - | " + Join(MainLines(oracool::SpellLevelLines(hero, spell, 1), "")) + " | ok |");
				rowsChecked++;
				continue;
			}

			// ---- a skill bought with points: every rank to the cap ----
			const int cap = oracool::ClassTreeMaxRank(skill);
			const bool hasLevelUp = oracool::ClassTreeHasLevelUpStat(skill);
			std::string firstMain;
			std::string capMain;
			std::string firstLevelUp;
			std::string capLevelUp;
			bool rowOk = true;
			const auto fail = [&](int rank, const std::string &what) {
				report.Fail(name + " rank " + std::to_string(rank) + ": " + what);
				rowOk = false;
			};
			oracool::ItemBonusTotals previousLevelUp {};
			for (int rank = 1; rank <= cap; rank++) {
				MyPlayer = &bystander;
				if (!oracool::InvestClassTreePoint(hero, skill)) {
					fail(rank, "the game refuses the point at level " + std::to_string(hero._pLevel) + " - the cap cannot be reached");
					break;
				}
				MyPlayer = &hero;
				if (data.kind == ClassTreeKind::Aura)
					hero._pOracoolActiveAura = static_cast<uint16_t>(skill); // its level-up stat burns only while lit
				ranksChecked++;

				const std::string levelUpLine = oracool::ClassTreeLevelUpLine(skill, rank);
				if (hasLevelUp) {
					oracool::ItemBonusTotals levelUp {};
					oracool::ApplyClassTreeLevelUpStat(skill, rank, levelUp);
					oracool::ItemBonusTotals sheet {};
					oracool::ApplyClassTreeToTotals(hero, sheet);
					const std::vector<Field> now = Fields(levelUp);
					const std::vector<Field> before = Fields(previousLevelUp);
					const std::vector<Field> received = Fields(sheet);
					const std::set<int> quoted = NumbersIn(levelUpLine);
					bool grew = false;
					for (size_t f = 0; f < now.size(); f++) {
						const int v = std::abs(now[f].value);
						const int was = std::abs(before[f].value);
						if (v > was)
							grew = true;
						if (v < was)
							fail(rank, std::string("the level-up stat's ") + now[f].name + " fell from " + std::to_string(was) + " to " + std::to_string(v));
						if (now[f].value == 0)
							continue;
						// The hero gets it: the sheet's sum holds at least the level-up share, in its direction.
						const bool reaches = now[f].value > 0 ? received[f].value >= now[f].value : received[f].value <= now[f].value;
						if (!reaches)
							fail(rank, std::string("the hero's ") + now[f].name + " is " + std::to_string(received[f].value) + ", short of the level-up stat's " + std::to_string(now[f].value));
						// And the tooltip quotes that same number.
						if (quoted.count(v) == 0)
							fail(rank, std::string("the tooltip line \"") + levelUpLine + "\" does not quote the " + now[f].name + " of " + std::to_string(v) + " the hero receives");
					}
					if (!grew)
						fail(rank, "the level-up stat did not grow (\"" + levelUpLine + "\")");
					if (levelUpLine.empty())
						fail(rank, "the row has a level-up stat but no tooltip line for it");
					previousLevelUp = levelUp;
				}

				// The hover: this rank's block under "Current Skill Level", the next rank's under "Next Level".
				const std::string tip = oracool::ClassTreeEffectLine(hero, skill, true);
				const std::string heading = "Current Skill Level: " + std::to_string(rank);
				const size_t currentAt = tip.find(heading);
				const size_t nextAt = tip.find("Next Level");
				if (currentAt == std::string::npos) {
					fail(rank, "the hover has no \"" + heading + "\"");
					continue;
				}
				const std::string current = tip.substr(currentAt, nextAt == std::string::npos ? std::string::npos : nextAt - currentAt);
				const std::string block = oracool::ClassTreeRankBlock(hero, skill, rank);
				for (const std::string &line : Lines(block)) {
					if (!HasLine(current, line))
						fail(rank, "\"" + line + "\" is missing from the Current Skill Level block");
				}
				if (hasLevelUp && !HasLine(current, levelUpLine))
					fail(rank, "the Current block lacks the level-up line \"" + levelUpLine + "\"");
				const std::vector<std::string> main = MainLines(block, levelUpLine);
				if (main.empty())
					fail(rank, "the hover states no main effect - only \"" + Join(Lines(block)) + "\"");
				if (rank < cap) {
					if (nextAt == std::string::npos) {
						fail(rank, "the hover has no Next Level block below the cap");
					} else {
						const std::string next = tip.substr(nextAt);
						const std::string nextLevelUp = oracool::ClassTreeLevelUpLine(skill, rank + 1);
						if (hasLevelUp && !HasLine(next, nextLevelUp))
							fail(rank, "the Next Level block lacks \"" + nextLevelUp + "\"");
						for (const std::string &line : Lines(oracool::ClassTreeRankBlock(hero, skill, rank + 1))) {
							if (!HasLine(next, line))
								fail(rank, "\"" + line + "\" is missing from the Next Level block");
						}
					}
					// Without a level-up stat, the main effect itself has to be what grows.
					if (!hasLevelUp) {
						const std::vector<std::string> nextMain = MainLines(oracool::ClassTreeRankBlock(hero, skill, rank + 1), "");
						if (nextMain == main)
							fail(rank, "no level-up stat, and rank " + std::to_string(rank + 1) + " prints the same main effect: " + Join(main));
					}
				} else if (tip.find("Fully invested") == std::string::npos) {
					fail(rank, "the cap is reached but the hover does not say \"Fully invested\"");
				}
				if (rank == 1) {
					firstMain = main.empty() ? "**NONE**" : main.front();
					firstLevelUp = levelUpLine;
				}
				if (rank == cap) {
					capMain = main.empty() ? "**NONE**" : main.front();
					capLevelUp = levelUpLine;
				}
			}
			rowsChecked++;
			report.rows.push_back("| " + name + " | " + KindName(data.kind) + " | " + std::to_string(cap) + " | "
			    + (hasLevelUp ? firstLevelUp + " -> " + capLevelUp : "(main effect grows)") + " | "
			    + firstMain + " -> " + capMain + " | " + (rowOk ? "ok" : "FAIL") + " |");
		}
	}
	MyPlayer = &Players[0];
	gbIsHellfire = savedHellfire;

	const std::filesystem::path path = std::filesystem::absolute("skill_rules_report.md");
	std::ofstream out(path);
	out << "# Skill rules: every hero, every rank\n\n"
	    << rowsChecked << " rows, " << ranksChecked << " ranks checked. " << report.failures.size() << " discrepancies, "
	    << report.warnings.size() << " warnings.\n\n";
	if (!report.failures.empty()) {
		out << "## Discrepancies\n\n";
		for (const std::string &f : report.failures)
			out << "- " << f << "\n";
		out << "\n";
	}
	if (!report.warnings.empty()) {
		out << "## Warnings\n\n";
		for (const std::string &w : report.warnings)
			out << "- " << w << "\n";
		out << "\n";
	}
	out << "## Every row\n\n| Class / skill | Kind | Cap | Level-up stat, rank 1 -> cap | Main effect, rank 1 -> cap | Verdict |\n|---|---|---|---|---|---|\n";
	for (const std::string &row : report.rows)
		out << row << "\n";
	out.close();

	EXPECT_GT(rowsChecked, 300) << "the sweep walked too few rows";
	EXPECT_TRUE(report.failures.empty()) << report.failures.size() << " discrepancies; the first: "
	                                     << (report.failures.empty() ? std::string() : report.failures.front())
	                                     << "\nall of them are in " << path.string();
}
