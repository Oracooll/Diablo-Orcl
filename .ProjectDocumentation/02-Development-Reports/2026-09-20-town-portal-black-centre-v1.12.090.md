# The town portal's black centre is back (v1.12.090)

**Date:** 2026-09-20 · **Version:** v1.12.090 · **Tests:** 831/831

## The hole

"the main blue town portal asset lost its black center when you shrunk it to 90% ... Now there is a transparent
hole in the center of the asset." Town only, because only the town uses the PNG sheet (`missiles/portal_town.png`,
`MissileGraphicID::TownPortalInTown` since v1.12.086); the dungeon-side portal still draws vanilla's CL2.

The cause is not the scale but the format. Vanilla's oval is hollow in the data: its interior is palette index 0,
which the CL2 renderer paints as opaque black. The exporter writes index 0 as transparent, and the PNG missile
loader (`oracool/sprite_import.cpp`, `TransparentIndex = 0`) reads alpha 0 as "not drawn" - so the interior that
was black in the CL2 became a hole in the sheet. The gold and violet rift sheets never showed it because they get
an explicit centre fill (96,66,8 and 0,0,60) in `tools/BuildRiftPortals.ps1`; the town sheet was built with no fill.

## The fix

`tools/BuildRiftPortals.ps1`: the town sheet's call gets a fill of opaque black, `@(0, 0, 0)`, through the same
row-by-row enclosure fill the rift sheets use (every transparent pixel between the ring's opaque pixels on its
row). Opaque black quantises in the loader to a near-black index in the upper half of the palette (the nearest
search starts at index 128), never to the transparent 0, so the fill draws. The three sheets were rebuilt; the gold
and violet ones are byte-for-byte the same recipe as before. A four-frame preview over green showed the black
interior with the blue rim and no shadow.

Nothing in the engine changed; the archive picks up the new sheet at the configure-time repack.
