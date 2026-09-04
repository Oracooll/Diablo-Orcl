# Oracool asset pipeline: cuts ui\middle_hud.png from the in-use bottom-HUD master.
#
#     powershell -ExecutionPolicy Bypass -File tools\CutHudPlate.ps1
#
# ## The fourth plate (2026-09-04)
#
# hud-plate-v4-947x263.png in Oracool.MPQ\01-in-use\bottom-hud - two square wells flanking one long
# bar, in the gold pinstripe of the second Levski painting, on transparent ground. Its opaque band
# is 897x195 at (22,30); that band is what hud_layout.cpp calls PlateSrcSize and every source rect
# there (LmbWellSrc, RmbWellSrc, BeltCellSrcX) is measured inside it. The band is asserted below, so
# a re-exported master whose band has moved fails here instead of quietly shifting the hit rects.
#
# Nothing is baked into this plate - no labels, no cell frames, no icons. The belt's six cells are
# the bar divided in six by the code; DrawBeltBacking frames them and the burger and portal icons
# draw into the end cells at runtime, exactly as they did on the third plate's blank Menu and
# Portal boxes.
#
# ## The earlier plates
#
# v2 and v3 (1536x1024 masters, band 1505x274 at (16,336), cut to 356x64) are still in the same
# folder; this script's git history has the cut and its notes for them. The layout for them is in
# hud_layout.cpp's history too - they cannot be swapped back in by this argument alone any more.
#
# ## Do not add a procedural bottom offset
#
# Standing note from the bottom-HUD packages, and PlateBottomMargin is already 0: the artwork is
# drawn screen-bottom flush. Anything that nudges it up leaves a gap the art was made to avoid.

param(
    [string] $Master = 'hud-plate-v4-947x263.png'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$master = Join-Path (Split-Path -Parent $root) "Oracool.MPQ\01-in-use\bottom-hud\$Master"
if (-not (Test-Path $master)) { throw "missing master: $master" }

# The art's alpha bounding box - PlateSrcSize and its origin in hud_layout.cpp. Written out rather
# than re-detected each run, and asserted, so the layout and the crop cannot disagree silently.
$cropX = 22
$cropY = 30
$srcW = 897
$srcH = 195
$outW = 356                                  # PlateScreenWidth
$outH = [int][math]::Floor($srcH * $outW / $srcW)   # ScalePlate(PlateSrcSize.height) - 77

$img = [System.Drawing.Bitmap]::FromFile($master)
try {
    if ($img.Width -ne 947 -or $img.Height -ne 263) {
        throw "master is $($img.Width)x$($img.Height), expected 947x263 - re-measure before cutting"
    }
    # The band: the bounding box of alpha >= 128, stepping by 1 so a one-pixel shift is caught.
    $minX = $img.Width; $maxX = -1; $minY = $img.Height; $maxY = -1
    for ($y = 0; $y -lt $img.Height; $y++) {
        for ($x = 0; $x -lt $img.Width; $x++) {
            if ($img.GetPixel($x, $y).A -ge 128) {
                if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
                if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    # The rects were measured against a crop one pixel wider than the alpha>=128 box on either side
    # (the outermost column is half-transparent); the crop keeps that origin, so the box must sit
    # exactly one pixel inside it.
    $bw = $maxX - $minX + 1; $bh = $maxY - $minY + 1
    if ($minX -ne $cropX + 1 -or $minY -ne $cropY -or $bw -ne $srcW - 2 -or $bh -ne $srcH) {
        throw "alpha band is ${bw}x${bh} at ($minX,$minY), expected $($srcW - 2)x${srcH} at ($($cropX + 1),$cropY) - re-measure hud_layout.cpp's rects before cutting"
    }

    $plate = New-Object System.Drawing.Bitmap -ArgumentList $outW, $outH, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($plate)
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
    $g.DrawImage($img,
        (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $outW, $outH),
        (New-Object System.Drawing.Rectangle -ArgumentList $cropX, $cropY, $srcW, $srcH),
        [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose()

    $plate.Save((Join-Path $root 'Packaging\resources\oracool_assets\ui\middle_hud.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    $plate.Dispose()
    Write-Host "middle_hud.png <- $Master crop ${cropX},${cropY} ${srcW}x${srcH} -> ${outW}x${outH}"
} finally {
    $img.Dispose()
}
