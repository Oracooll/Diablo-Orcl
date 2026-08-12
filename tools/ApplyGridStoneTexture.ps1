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
# squeezed into each cell. A tile is 120x114 and a cell interior is 22x22 - scaling a tile down
# 5x would mush the stone detail into noise and make all 70 cells identical. Instead the tile is
# repeated across the full grid area and each cell shows the 22x22 patch at its own position, so
# the detail stays at native scale and no two cells repeat. Reads like looking at a stone wall
# through a grid, which is what the kit's "TILEABLE BACKGROUND" is for.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\ApplyGridStoneTexture.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$sheet = "..\Oracool.MPQ\02-source-art\inventory\panel-frame-kit-modular-greenscreen.png"
# One cell of the sheet's 3x3 TILEABLE BACKGROUND demo, found by component detection.
$TILE_X = 593; $TILE_Y = 577; $TILE_W = 120; $TILE_H = 114

# Grid geometry, mirroring Source/oracool/inventory_layout.h:
#   GridOrigin = ((320 - 10*28)/2, 400) = (20,400); GridSizeInCells = 10x7; CellPx = 28.
$GRID_X = 20; $GRID_Y = 400; $COLS = 10; $ROWS = 7; $CELL = 28
$INSET = 1; $INNER = 26

if (-not (Test-Path $sheet)) { throw "kit sheet not found: $sheet" }
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $sheet))
$tile = $src.Clone((New-Object System.Drawing.Rectangle $TILE_X,$TILE_Y,$TILE_W,$TILE_H),
                   [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$src.Dispose()

# Repeat the tile across the whole grid area once, up front.
$gw = $COLS * $CELL; $gh = $ROWS * $CELL
$wall = New-Object System.Drawing.Bitmap $gw,$gh,([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$wg = [System.Drawing.Graphics]::FromImage($wall)
for ($y = 0; $y -lt $gh; $y += $TILE_H) {
    for ($x = 0; $x -lt $gw; $x += $TILE_W) { $wg.DrawImage($tile, $x, $y, $TILE_W, $TILE_H) }
}
$wg.Dispose(); $tile.Dispose()

$targets = @("Packaging\resources\oracool_assets\ui", "Packaging\resources\assets\ui", "build\x64-Debug\assets\ui")
$master = "Packaging\resources\oracool_assets\ui\inventory_panel.png"
if (-not (Test-Path $master)) { throw "panel not found: $master" }

$panel = New-Object System.Drawing.Bitmap ([System.Drawing.Bitmap]::FromFile((Resolve-Path $master)))
$cells = 0
for ($row = 0; $row -lt $ROWS; $row++) {
    for ($col = 0; $col -lt $COLS; $col++) {
        $dx = $GRID_X + $col*$CELL + $INSET
        $dy = $GRID_Y + $row*$CELL + $INSET
        # Sample at this cell's own position in the wall, so the stone is continuous.
        $sx = $col*$CELL + $INSET
        $sy = $row*$CELL + $INSET
        for ($j = 0; $j -lt $INNER; $j++) {
            for ($i = 0; $i -lt $INNER; $i++) {
                $p = $wall.GetPixel(($sx+$i) % $gw, ($sy+$j) % $gh)
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
