#include "oracool/inventory_layout.h"

#include "utils/display.h"

namespace devilution {
namespace oracool {

Rectangle GetInventoryPanelRect()
{
	// Flush to the top-right corner. Deliberately no margin: at 660 tall on a 720 screen
	// there is nothing to spare, and any top inset pushes the footer further under the
	// mana orb. See the header for the vertical budget this placement depends on.
	return { { gnScreenWidth - InventoryPanelSize.width, 0 }, InventoryPanelSize };
}

} // namespace oracool
} // namespace devilution
