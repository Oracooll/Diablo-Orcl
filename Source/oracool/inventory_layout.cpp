#include "oracool/inventory_layout.h"

#include "oracool/ornate_border.h" // BottomDockedTop - the shared docking rule
#include "utils/display.h"

namespace devilution {
namespace oracool {

Rectangle GetInventoryPanelRect()
{
	// Flush to the BOTTOM-right corner (user, 2026-08-27: the docking rule was about these panels -
	// "the limestone windows i wanted docked at the botom were inventory, hero stats, etc").
	//
	// A no-op at 720p, where the panel is exactly as tall as the screen, and the whole point on
	// anything taller: the panel then sits with the HUD it belongs to instead of floating at the top
	// with a gap underneath it.
	return { { gnScreenWidth - InventoryPanelSize.width, BottomDockedTop(InventoryPanelSize.height) },
		InventoryPanelSize };
}

} // namespace oracool
} // namespace devilution
