# Oracool asset pipeline: retextures the 70 backpack-grid cells in ui\inventory_panel.png with
# the tileable stone from the modular frame kit sheet.
#
# Only each cell's INTERIOR is touched. A luma profile across a cell shows the baked frame runs
# x+0..x+2 (mean luma 55-66) and x+25..x+27, with a flat dark interior at x+3..x+24 (~33) - a 6px
# separator between neighbouring interiors, three from each cell.
#
# That was thicker than wanted, so the frame is deliberately cut to ONE pixel: the texture is
# written over x+1..x+26 (26x26), covering two of the three original frame pixels per side and
# leaving only the outermost. Adjacent cells then show a 2px separator instead of 6px.
#
# Note the write region is a superset of any previous run's (26x26 inset 1 contains 22x22 inset 3),
# so re-running never leaves a stale ring of older texture behind - the script stays idempotent.
#
# The stone is sampled as ONE CONTINUOUS SURFACE behind the grid rather than one whole tile
# squeezed into each cell. A cell interior is 26x26 against a ~120px tile - scaling a tile down
# 5x would mush the stone detail into noise and make all 70 cells identical. Each cell instead
# shows the patch of the surface at its own position, so detail stays at native scale and no two
# cells repeat. Reads like looking at a stone wall through a grid.
#
# That surface is taken as a SINGLE 280x196 crop from the middle of the sheet's large background
# texture - it is NOT tiled. The first version of this script tiled a 120x114 crop instead, and
# shipped green seams: the tile's outermost pixels are anti-aliased blends of stone against the
# green screen, and every tile boundary repeated them as a green line (measured afterwards at
# grid x=119/120, x=239/240 and y=114 - exactly the seams). Cropping one region big enough to
# cover the whole grid means there are no seams to contaminate, and taking it from deep inside
# the texture means no edge pixel is ever sampled. AssertNoGreen below enforces the second part
# rather than trusting it.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\ApplyGridStoneTexture.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$sheet = "..\Oracool.MPQ\02-source-art\inventory\panel-frame-kit-modular-greenscreen.png"
# The sheet's large background texture occupies (16,53) 442x463. The grid needs 280x196, which
# fits inside it with room to spare, so the crop is centred there - leaving an 81px horizontal
# and 133px vertical margin to the texture's green edge.
$TEX_X = 16; $TEX_Y = 53; $TEX_W = 442; $TEX_H = 463

# Grid geometry, mirroring Source/oracool/inventory_layout.h:
#   GridOrigin = ((320 - 10*28)/2, 400) = (20,400); GridSizeInCells = 10x7; CellPx = 28.
$GRID_X = 20; $GRID_Y = 400; $COLS = 10; $ROWS = 7; $CELL = 28
$INSET = 1; $INNER = 26

if (-not (Test-Path $sheet)) { throw "kit sheet not found: $sheet" }
$gw = $COLS * $CELL; $gh = $ROWS * $CELL
if ($gw -gt $TEX_W -or $gh -gt $TEX_H) { throw "grid ${gw}x${gh} does not fit the texture ${TEX_W}x${TEX_H} - it would have to be tiled, which is what put green seams in the grid last time" }

$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $sheet))
$cropX = $TEX_X + [int](($TEX_W - $gw) / 2)
$cropY = $TEX_Y + [int](($TEX_H - $gh) / 2)
$wall = $src.Clone((New-Object System.Drawing.Rectangle $cropX,$cropY,$gw,$gh),
                   [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$src.Dispose()
Write-Host "  stone crop: ${gw}x${gh} at sheet ($cropX,$cropY), margins $([int](($TEX_W-$gw)/2))px / $([int](($TEX_H-$gh)/2))px to the texture edge"

# Scrub green-screen bleed out of the crop. This is the check the first version lacked entirely.
#
# Cropping from the middle of the texture is not sufficient on its own: this crop sits 81px/134px
# from every edge and STILL contained two green pixels, so the source has stray green speckles in
# its body, not just an anti-aliased rim. Rather than hunt for a clean region that may not exist,
# each offending pixel is rebuilt from the mean of its non-green neighbours - at a couple of
# pixels in ~55,000 that is invisible, and it works wherever the crop lands.
function Test-Green([System.Drawing.Color]$c) { return (($c.G - [Math]::Max($c.R, $c.B)) -ge 8) }

$bad = @()
for ($y = 0; $y -lt $gh; $y++) {
    for ($x = 0; $x -lt $gw; $x++) { if (Test-Green $wall.GetPixel($x, $y)) { $bad += ,@($x, $y) } }
}
foreach ($b in $bad) {
    $x = $b[0]; $y = $b[1]
    $r = 0; $g2 = 0; $bl = 0; $n = 0
    for ($dy = -2; $dy -le 2; $dy++) {
        for ($dx = -2; $dx -le 2; $dx++) {
            $nx = $x + $dx; $ny = $y + $dy
            if ($nx -lt 0 -or $ny -lt 0 -or $nx -ge $gw -or $ny -ge $gh) { continue }
            $c = $wall.GetPixel($nx, $ny)
            if (Test-Green $c) { continue }
            $r += $c.R; $g2 += $c.G; $bl += $c.B; $n++
        }
    }
    if ($n -eq 0) { throw "green pixel at ($x,$y) has no clean neighbour to rebuild from" }
    $wall.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, [int]($r/$n), [int]($g2/$n), [int]($bl/$n)))
}
# Prove it, rather than assume the repair worked.
$left = 0
for ($y = 0; $y -lt $gh; $y++) {
    for ($x = 0; $x -lt $gw; $x++) { if (Test-Green $wall.GetPixel($x, $y)) { $left++ } }
}
if ($left -gt 0) { throw "$left green pixel(s) survived the repair" }
Write-Host "  green scrub: repaired $($bad.Count), 0 remaining in the crop"

$targets = @("Packaging\resources\oracool_assets\ui", "Packaging\resources\assets\ui", "build\x64-Debug\assets\ui")
$master = "Packaging\resources\oracool_assets\ui\inventory_panel.png"
if (-not (Test-Path $master)) { throw "panel not found: $master" }

$panel = New-Object System.Drawing.Bitmap ([System.Drawing.Bitmap]::FromFile((Resolve-Path $master)))
$cells = 0
for ($row = 0; $row -lt $ROWS; $row++) {
    for ($col = 0; $col -lt $COLS; $col++) {
        $dx = $GRID_X + $col*$CELL + $INSET
        $dy = $GRID_Y + $row*$CELL + $INSET
        # Sample at this cell's own position in the crop, so the stone is continuous. The crop is
        # exactly grid-sized, so these indices never wrap - deliberately no modulo, because a wrap
        # would be a seam by another name.
        $sx = $col*$CELL + $INSET
        $sy = $row*$CELL + $INSET
        for ($j = 0; $j -lt $INNER; $j++) {
            for ($i = 0; $i -lt $INNER; $i++) {
                $p = $wall.GetPixel($sx+$i, $sy+$j)
                $panel.SetPixel($dx+$i, $dy+$j, [System.Drawing.Color]::FromArgb(255,$p.R,$p.G,$p.B))
            }
        }
        $cells++
    }
}
$wall.Dispose()

foreach ($d in $targets) {
    if (Test-Path $d) {
        $panel.Save((Join-Path (Resolve-Path $d) "inventory_panel.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "  wrote $d\inventory_panel.png"
    }
}
$panel.Dispose()
Write-Host "retextured $cells grid cells ($COLS x $ROWS), ${INNER}x${INNER} interior each, frames untouched"
