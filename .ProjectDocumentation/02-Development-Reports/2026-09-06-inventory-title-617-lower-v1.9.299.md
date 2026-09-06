# The INVENTORY title 617px lower (v1.9.299)

**Date:** 2026-09-06
**Request:** "move inventory title 617px lower. just the inventory. leave other canvas windows alone."

inv.cpp: `InventoryTitleDrop = 617` added to the title band's y, so the band runs 645..683 of the 720px canvas. The shared `PanelTitleTop` and every other side panel are untouched.
