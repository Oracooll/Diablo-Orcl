#include "oracool/recipe_list.h"

#include <algorithm>
#include <string>

#include "DiabloUI/ui_flags.hpp"
#include "engine/palette.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "oracool/crafting.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

/** @brief The indent the explanation sits at, under its name. */
constexpr int Indent = 8;
/** @brief Air between the list and its frame. */
constexpr int Pad = 6;
/** @brief The thumb's width, and how far in from the opening's right edge it runs. */
constexpr int ThumbWidth = 3;
constexpr int ThumbInset = 5;
constexpr uint8_t ThumbColor = PAL16_YELLOW + 10;  // the item grid's own outline gold
constexpr uint8_t SelectedFill = PAL16_GRAY + 14;  // the plate fill every list in this mod selects with

/** @brief One drawn line: a recipe's name, or a wrapped line of its explanation. */
struct Line {
	std::string text;
	bool heading;
	/** @brief Which recipe it belongs to - so a click in the paragraph picks the same one as its title. */
	int recipe;
};

/** @brief How wide the explanations wrap to, inside @p opening. */
int TextWidth(Rectangle opening)
{
	return std::max(1, opening.size.width - 2 * Pad - Indent);
}

/**
 * @brief Every line the list has to show, wrapped.
 *
 * NO ROW LIMIT. A fixed two lines a recipe truncated every explanation longer than the frame is
 * wide, and the longest of these runs past ninety characters.
 */
std::vector<Line> LinesFor(const std::vector<int> &recipes, int width)
{
	std::vector<Line> lines;
	for (const int recipe : recipes) {
		lines.push_back({ std::string { _(CraftingRecipeName(recipe)) }, true, recipe });
		const std::string wrapped = WordWrapString(_(CraftingRecipeInputs(recipe)), static_cast<unsigned>(width), GameFont12);
		size_t start = 0;
		for (;;) {
			const size_t nl = wrapped.find('\n', start);
			lines.push_back({ wrapped.substr(start, nl == std::string::npos ? std::string::npos : nl - start), false, recipe });
			if (nl == std::string::npos)
				break;
			start = nl + 1;
		}
		// Air between recipes, belonging to NOBODY: a click in the gap should not pick the recipe
		// above it just because that is the line the gap follows.
		lines.push_back({ std::string {}, false, -1 });
	}
	return lines;
}

} // namespace

int RecipeListMaxScroll(Rectangle opening, const std::vector<int> &recipes)
{
	if (opening.size.height <= 2 * Pad)
		return 0;
	const int lineHeight = GetLineHeight("A", GameFont12);
	const int content = static_cast<int>(LinesFor(recipes, TextWidth(opening)).size()) * lineHeight;
	return std::max(0, content - (opening.size.height - 2 * Pad));
}

int RecipeListHitTest(Rectangle opening, const std::vector<int> &recipes, int scroll, Point position)
{
	if (!opening.contains(position))
		return -1;
	const int lineHeight = GetLineHeight("A", GameFont12);
	if (lineHeight <= 0)
		return -1;
	const std::vector<Line> lines = LinesFor(recipes, TextWidth(opening));
	const int top = opening.position.y + Pad;
	// The same arithmetic the draw uses, run backwards. Anything else is a second layout that agrees
	// with the first until one of them is edited.
	const int index = (position.y - top + scroll) / lineHeight;
	if (index < 0 || index >= static_cast<int>(lines.size()))
		return -1;
	return lines[index].recipe;
}

void DrawRecipeList(const Surface &out, Rectangle opening, const std::vector<int> &recipes, int &scroll, int selected)
{
	if (opening.size.width <= 2 * Pad || opening.size.height <= 2 * Pad)
		return;

	const int lineHeight = GetLineHeight("A", GameFont12);
	const int width = TextWidth(opening);
	const std::vector<Line> lines = LinesFor(recipes, width);

	// Clamped HERE, every frame, rather than only where the wheel turns - see the header.
	const int maxScroll = RecipeListMaxScroll(opening, recipes);
	scroll = std::clamp(scroll, 0, maxScroll);

	const int top = opening.position.y + Pad;
	const int bottom = opening.position.y + opening.size.height - Pad;
	for (size_t i = 0; i < lines.size(); i++) {
		const int y = top + static_cast<int>(i) * lineHeight - scroll;
		// Wholly inside the opening or not drawn at all. Half a line clipped by the frame reads as a
		// rendering fault rather than as "there is more below" - the thumb is what says that.
		if (y < top || y + lineHeight > bottom)
			continue;
		const Line &line = lines[i];
		// The selected recipe's WHOLE block is banded, its blank tail included, so the selection
		// reads as one paragraph rather than as a stripe behind each line.
		if (selected >= 0 && line.recipe == selected)
			FillRect(out, opening.position.x + Pad - 2, y, opening.size.width - 2 * Pad + 4, lineHeight, SelectedFill);
		if (line.text.empty())
			continue;
		DrawString(out, line.text,
		    Rectangle { { opening.position.x + Pad + (line.heading ? 0 : Indent), y },
		        { width + (line.heading ? Indent : 0), lineHeight } },
		    { (line.heading ? UiFlags::ColorGold : UiFlags::ColorWhite) | UiFlags::FontSize12 });
	}

	if (maxScroll > 0) {
		const int trackHeight = opening.size.height - 2 * Pad;
		const int thumbHeight = std::max(8, trackHeight * trackHeight / (trackHeight + maxScroll));
		const int thumbTop = (trackHeight - thumbHeight) * scroll / maxScroll;
		FillRect(out, opening.position.x + opening.size.width - ThumbInset, top + thumbTop,
		    ThumbWidth, thumbHeight, ThumbColor);
	}
}

} // namespace devilution::oracool
