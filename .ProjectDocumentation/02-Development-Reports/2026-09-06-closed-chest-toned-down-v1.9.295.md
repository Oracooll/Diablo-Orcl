# The closed chest toned down (v1.9.295)

**Date:** 2026-09-06
**Request:** "inactive inventory tab - now we need also to tune down their color. this bright white is hitting my eyes too much compared to the surrounding. the inactive tabs icons to grey-ish, but not too dark. a bit brighter than its background."

`DrawTabGlyph`: only the OPEN chest draws 1:1 white now. A closed chest goes through the recolour loop with its white mapped to PAL16_GRAY+2 (0xcccccc), two steps below white and a shade above the light-grey plate's face; the hover keeps PAL16_YELLOW+1. The shadow is untouched in both. Suite result in the commit.
