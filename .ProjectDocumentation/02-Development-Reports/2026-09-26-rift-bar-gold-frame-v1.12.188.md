# The rift bar: a gold frame and colour cycling — v1.12.188

2026-09-26

> "i want to make the rift bars nicer looking - put a gold frame around them and apply color cycling on them to make them more interesting."

- **Frame:** the bar sits in `DrawLegacyTextBox` - the theme's legacy gold pinstripe (dark-bright-dark, lit from above,
  palette 204/194/195 around the black field 223), the same gold on every level. The bar moved down 3px to make room.
- **Colour cycling** (`DrawRiftBarFill`, oracool/rift.cpp): a shimmer wave 28 columns long travels along the fill once
  every 1.8 s, mixing the portal's deep -> base -> pale shades (gold: #A8700E / #D9A21A / #FFE08A; violet: #5A1E94 /
  #8A3FC8 / #D49CF5); a near-white glint six columns wide sweeps across it every 2.2 s; the fill is lit from above
  (top rows toward pale, bottom toward black) and its leading edge glows. Driven by SDL_GetTicks, so it runs at the same
  speed whatever the game speed. 8-bit targets draw the fallback index (PAL16_YELLOW+2 / PAL8_BLUE).
- Previewed offline with the same maths (not seen in the game yet).

v1.12.188 Debug: clean build; oracool_audit_test passes. Drawing only - no logic touched.
