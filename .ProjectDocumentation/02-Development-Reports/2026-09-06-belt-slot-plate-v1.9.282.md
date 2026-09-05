# The gold plate under the belt slots (v1.9.282)

**Date:** 2026-09-06
**Request:** "can you fit a gold legacy skill backing scaled down in the belt slots?" then, corrected to the real cell size: "the gold backing to be 34x34 as well, but with a 2px outline within these 34x34px. outer pixel outline to be black. inner 1px outline to be gray. the net 30x30 to be the rest of the 34x34 backing."

## What was done

`oracool::DrawBeltSlotPlate(out, cell)` in hud_art.cpp: a black fill of the whole cell, a grey fill (PAL16_GRAY+8) one pixel in, and the vanilla empty plate (frame 26 of the small spell sheet through the gold Skill translation) over the 30x30 core. `DrawInvBelt` calls it for slots 1..4 before the empty check, so an empty slot wears the plate too, and the item sprite goes on top as before.

`DrawSmallSpellIconCoveringClipped(out, cell)` in spell_icons.cpp is the shrinking twin of the two existing plate scalers. The nearest-neighbour scaler truncates, so no single percentage lands a 37x38 plate on a 30x30 square (81% gives 29x30, 82% gives 30x31). It rounds up to cover and clips to the cell through a subregion surface, so the extra row lands nowhere and a pixel-short plate cannot show the fill as a third ring.

## Test

`OracoolAudit.TheBeltSlotPlateIsBlackThenGreyThenGold` draws onto a 34x34 surface prefilled with a sentinel colour and reads every pixel: ring 0 black, ring 1 grey, the core neither sentinel nor grey and at least a quarter gold-ramp.

Suite 628/630 on the first run; the Writehero failure was the known flake and passed alone. The dungeon-generation failure is the standing one.
