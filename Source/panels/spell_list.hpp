#pragma once

#include <cstddef>
#include <vector>

#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "spelldat.h"

namespace devilution {

struct SpellListItem {
	Point location;
	SpellType type;
	SpellID id;
	bool isSelected;
};

void DrawSpell(const Surface &out);

/**
 * @brief The lit-aura marker in the RMB well's top-left corner.
 *
 * Called right after DrawSpell so it lands over whichever icon that chose. Separate because DrawSpell
 * has early returns and the badge has to survive all of them - and because the aura must NOT own the
 * well: it did for one version, and every assignment made afterwards was invisible.
 */
void DrawRmbAuraBadge(const Surface &out);
void DrawSpellList(const Surface &out);
std::vector<SpellListItem> GetSpellListItems();
void SetSpell();
void SetSpeedSpell(size_t slot);
void ToggleSpell(size_t slot);

/**
 * Draws the "Speed Book": the rows of known spells for quick-setting a spell that
 * show up when you click the spell slot at the control panel.
 */
void DoSpeedBook();

} // namespace devilution
