# v1.12.421 - lighting/stutter audit, round 2 (2026-10-10)

Two fresh read-only reviewers over v1.12.420. No P1. Fixed:

## Lighting
- **Most Orcl skill lights were white (P2).** The skill effects ride carrier missiles whose rows are Physical (AddArtEffect's AcidCloud, AddArtBolt's AcidJavelin, the census MeteorImpact/ThunderBolt), and only Hue/HueCycle tints were read. Now any tint that names a hue (Glint included - Guided Arrow's fire glint) is used, and a Physical missile takes the colour its sheet's file name says (`ArtNameHue`: holy, blood, poison, cold, fire, lightning word lists, cached per graphic) - Furnace Flame, Ember Mine, Absolute Zero, Ball Lightning, the Storm skills.
- **The passes made ~25 million calls a frame in the Debug build.** `#pragma optimize` does not lift /Ob0, so Bilinear, std::clamp, std::min, the lambdas and std::array::operator[] all stayed calls (confirmed with dumpbin by the reviewer). Both passes are now written out by hand over raw planes (a per-frame brightness plane from dLight; the four tints read once per sample, three channels interpolated together; ternary clamps).
- **Champions and Luminous monsters took a meaningless TRN hue.** Their TRN is a small shift along their own ramps, and the plain average of the targets was half the palette for everyone. Order is now Diablo, berserk, affix colour, Luminous (warm), then a unique's TRN measured by its SHIFT (targets pushed along target - source).
- **Clipping and darkening under strong hues.** Channel lifts capped at 1.5 (blue 1.35), then the three rescaled to keep a grey pixel's brightness - a red light no longer darkens the floor it reddens.

## Stutter
- **Rift guardians (180-250%) and other sized monsters arriving after the load popped in size per animation and usually died at 100% (P2).** The first ask of a (type, size) now hands all six animations to the worker.
- **A sized corpse rebuilt the whole Death sheet on the main thread at the killing blow, and Prebuild built it twice (P2).** A corpse is its type's Death sheet: once the scaled Death is in, the body is a view of it; the separate corpse job is gone; a synchronous fallback that cannot be built leaves the body at its own size instead of throwing.

## Accepted (P3)
- A LeftTrapezoid/TransparentSquare micro on a see-through non-solid piece still redraws its lower half at the tile's light (unknown whether any piece has one).
- The hero's skill buff light (Static Charge, Conduit, the cold armours) is neutral; a dead champion's kept light turns neutral.
- Ground rings and infravision silhouettes take the colour pass's tint.
- A charging Giant/Colossal can charge at 100% while its Attack sheet is still on the worker.

Build Debug, ctest 971/971, the lighting preview re-checked by eye.
