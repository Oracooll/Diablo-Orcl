# The salvage message's second line wraps beside the plate (v1.12.098)

**Date:** 2026-09-21 · **Version:** v1.12.098 · **Tests:** 832/832

User: "word wrapp the word Salvaged from the Frame + Icon row to fit it within the Salvage results frame."

"7 Unique Encrustments Salvaged" beside a 62 px frame did not fit the 260 px box. `DrawSalvageWindow` now wraps
the second line with `WordWrapString` to the room left of the frame (box width less the frame and the 8 px gap), so
it reads "7 Unique Encrustments" over "Salvaged" when one line will not do; the lines sit vertically centred on the
plate and the row is centred in the box by its widest line. Short names ("7 Rare Fibres Salvaged") stay on one line.
