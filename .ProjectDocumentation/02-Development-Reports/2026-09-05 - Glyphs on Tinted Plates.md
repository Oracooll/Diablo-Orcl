# Glyphs on Tinted Plates (v1.9.251)

**Date:** 2026-09-05 · **Request:** "what is the color coding of this skill page?" (none: the plate was baked into the eleven frames) → "do it".

## What changed

`tools/ApplyPaladinGlyphDraft.ps1` now writes the eleven Paladin Combat glyphs on transparency, no plate. The strip's other 38 frames are untouched.

`hud_art.cpp` grew `IsGlyphFrame(asset, index, cell)`: a strip cell whose opaque pixels are all either white (243,243,243) or shadow (12,7,7) at full alpha, with at least one of them, is a vanilla-style glyph. Measured once per frame off the loaded RGBA and cached in `ArtAsset::glyphFrames` (cleared with `cellInsets` on a hot reload), so each glyph batch that lands is recognised with no code change and no list to keep in step with the strip.

A glyph frame draws differently in two places:

- **Abilities tree page** (`DrawClassTreeIconOutlined`): the engine's own plate through `ApplyPlateTint`, then the glyph 1:1 centred on it. The coloured set keeps its plateless, scaled-to-cell draw.
- **Wells** (`DrawClassTreeSkillInWell`): the tinted plate was already there; a glyph now sits 1:1 on it instead of being scaled up, which would have eaten the 8px clear border the brief specifies. In the picker's 38px cell the glyph takes the scaled path like everything else.

So the Combat page now carries the coding: gold for ready, light grey for unlocked-but-unspent, red for locked. The aura and passive pages stay plateless until their glyphs arrive.

## Verification

Debug build clean (multi.cpp's five standing C4267 warnings only); 624/625 with the standing `Drlg_l1` failure. Pixel counts still match `pixel-checks.json` for all eleven. Look at the Combat Skills page with one skill invested and one not, and at the RMB well with a combat skill readied.
