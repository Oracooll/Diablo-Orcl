# Item backings at 66% opacity

2026-09-26 — v1.12.193

The user asked for the rim-and-glow backings at 50%, 60%, 75% and 90% opacity side by side, then chose: "go with 66%".

- `RimGlowOpacityPercent` in `inv.cpp`, exported through `inv.h`, replaces the fixed constant in `DrawRimGlowBacking`. It is now 66 (it was 50).
- `OracoolPreview.DISABLED_BackingOpacities`, run by name, renders the same five items at the four opacities into `item_backings_opacity.png` through the game's own code: bow (plain), sword (magic), armour (rare), shield (unique), helm (primal). To compare other values, change the test's `Opacities`.
- The Debug build compiles cleanly. Four test programs were blocked by Smart App Control and relinked. The full suite passes, 847 of 847. Not seen in game yet.
