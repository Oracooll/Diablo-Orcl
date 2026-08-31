#include "oracool/hero_stats_column.h"

#include <array>
#include <string>

#include <fmt/format.h>

#include "DiabloUI/hero/hero_layout.h"
#include "DiabloUI/ui_flags.hpp"
#include "engine/render/text_render.hpp"
#include "playerdat.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

/** @brief Row pitch. The list opposite uses 52 for a FontSize30 line; these rows are smaller text
 * and there are eleven of them, so they pack tighter while still clearing their own glyphs. */
constexpr int RowHeight = 30;
/** @brief Blank rows inserted between groups, in the same units - identity, attributes, combat. */
constexpr int GroupGap = 14;

/** @brief The two groups' worth of blank space, so the block's height can be known before drawing. */
constexpr int GroupCount = 2;

/** @brief Label on the left, value on the right, both inside the column. */
constexpr int SidePadding = 6;

struct StatRow {
	std::string label;
	std::string value;
	/** @brief Extra space above this row - non-zero on the first row of a group. */
	int gapAbove;
};

/**
 * @brief The rows, in reading order: who this is, then what they are, then what they do.
 *
 * Built as data rather than drawn inline so the block's total height is known before a single line
 * is placed - which is what lets it be centred in the column the way the character list centres
 * itself in the same band.
 */
std::vector<StatRow> BuildRows(const _uiheroinfo &hero)
{
	const auto classIndex = static_cast<size_t>(hero.heroclass);
	const char *className = classIndex < enum_size<HeroClass>::value
	    ? PlayersData[classIndex].className
	    : "";

	std::vector<StatRow> rows;
	rows.push_back({ std::string(_("Class")), std::string(_(className)), 0 });
	rows.push_back({ std::string(_("Level")), StrCat(hero.level), 0 });

	rows.push_back({ std::string(_("Strength")), StrCat(hero.strength), GroupGap });
	rows.push_back({ std::string(_("Magic")), StrCat(hero.magic), 0 });
	rows.push_back({ std::string(_("Dexterity")), StrCat(hero.dexterity), 0 });
	rows.push_back({ std::string(_("Vitality")), StrCat(hero.vitality), 0 });

	rows.push_back({ std::string(_("Life")), StrCat(hero.life), GroupGap });
	rows.push_back({ std::string(_("Mana")), StrCat(hero.mana), 0 });
	rows.push_back({ std::string(_("Armor")), StrCat(hero.armourClass), 0 });
	rows.push_back({ std::string(_("Damage")), StrCat(hero.minDamage, "-", hero.maxDamage), 0 });
	return rows;
}

} // namespace

void DrawHeroStatsColumn(const Surface &out, Rectangle area, const _uiheroinfo &hero)
{
	if (area.size.width <= 0 || area.size.height <= 0)
		return;

	std::vector<StatRow> rows = BuildRows(hero);

	const auto measure = [](const std::vector<StatRow> &r) {
		int total = 0;
		for (const StatRow &row : r)
			total += row.gapAbove + RowHeight;
		return total;
	};

	int blockHeight = measure(rows);
	// The band is HeroContentTop..HeroContentBottom, which shrinks with the window: eleven rows and
	// their group gaps fit comfortably at 960x720 (328 of 433) and do NOT fit at 640x480, where the
	// band is 193. Overflowing would run the last rows through the button row underneath, so the
	// block gives up its spacing first and then its tail - it never draws outside its own column.
	if (blockHeight > area.size.height) {
		for (StatRow &row : rows)
			row.gapAbove = 0;
		blockHeight = measure(rows);
	}
	while (rows.size() > 1 && blockHeight > area.size.height) {
		blockHeight -= rows.back().gapAbove + RowHeight;
		rows.pop_back();
	}

	// Centred in the column, and clamped so a band shorter than even one row pins the block to the
	// top rather than starting it above - the same clamp the list's own vertical centring uses.
	int y = area.position.y + std::max(0, (area.size.height - blockHeight) / 2);

	const int labelX = area.position.x + SidePadding;
	const int width = area.size.width - 2 * SidePadding;
	if (width <= 0)
		return;

	for (const StatRow &row : rows) {
		y += row.gapAbove;
		// Label and value share one row rect, one left-aligned and one right - so the values line up
		// as a column of their own however wide the labels get translated.
		DrawString(out, row.label, Rectangle { { labelX, y }, { width, RowHeight } },
		    { UiFlags::ColorUiSilver | UiFlags::FontSize24 | UiFlags::VerticalCenter });
		DrawString(out, row.value, Rectangle { { labelX, y }, { width, RowHeight } },
		    { UiFlags::ColorUiGold | UiFlags::FontSize24 | UiFlags::AlignRight | UiFlags::VerticalCenter });
		y += RowHeight;
	}
}

} // namespace devilution::oracool
