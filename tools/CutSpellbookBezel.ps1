# Oracool asset pipeline: cuts the vanilla spellbook's ornate bezel into reusable components.
#
# The bezel is the original Diablo spellbook sprite's own border - a chain-of-rings ornament between
# two gold rails - and it has more character than the procedural textbox_frame00 bevel the rest of
# the theme uses. This makes it available at any window size.
#
# IT IS NOT NAIVELY TILEABLE. Two independent tests on the rendered art say so: autocorrelation of
# the ornament row is flat (diffs 12.9-16.1 against a signature stddev of 13.4 - no minimum above
# the noise floor at any lag), and motif-spacing detection gives a scattered gap histogram
# (5px x14, 6px x9, 8px x5, 9px x7, up to 18) with no dominant period. Slicing an arbitrary chunk
# and repeating it would show a seam; stretching would resample and blur.
#
# A search for a seamless unit finds a 12px one whose wrap seam measures 1.84 against a baseline
# neighbour-column difference of 19.82 mean / 49.20 max - a tenth of a normal step.
#
# THAT METRIC IS NOT SUFFICIENT, and rendering the result proves it. Tiling this unit to 200, 340
# and 460px produces obvious vertical banding every 12px and a chain that reads as mechanically
# stamped rather than drawn. The measurement only asked "is the JOIN continuous"; it never asked
# "is the REPETITION visible", and here it plainly is - the stone behind the chain carries
# irregular hand-painted grain that repeats far too often at this size to pass as texture.
#
# So these components are RAW MATERIAL, not a working tileable border. The corners are directly
# usable - they are one-offs and do not repeat. The h/v units are kept because they are the correct
# cut geometry, but anything using them needs either a much longer unit (so repetition falls below
# notice), a purpose-drawn tileable strip, or the existing procedural bevel instead.
#
# Source is the 1:1 screenshot rather than spellbk.cel: the game renders the CEL unscaled, so the
# pixels are the artwork, and this avoids a CEL decoder plus palette resolution for a one-off cut.
# The consequence is that the cut carries the palette the screenshot was taken under.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutSpellbookBezel.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$shot = "build\x64-Debug\Saved_Games\Screenshots\Screenshot from 2026-08-13 08-38-37.png"
if (-not (Test-Path $shot)) { throw "source screenshot not found: $shot" }
$b = [System.Drawing.Bitmap]::FromFile((Resolve-Path $shot))

# Panel bounds, measured: the spellbook is 320 wide at x 960 and its top bezel band is y 118..141.
$PANEL_X = 960; $PANEL_W = 320
$TOP_Y = 118; $BAND_H = 23
# The searched seamless unit.
$H_START = $PANEL_X + 115; $H_LEN = 12

$outDirs = @("Packaging\resources\oracool_assets\ui","Packaging\resources\assets\ui","build\x64-Debug\assets\ui")
$srcDir = "..\Oracool.MPQ\02-source-art\borders"
New-Item -ItemType Directory -Force -Path $srcDir | Out-Null
foreach ($d in $outDirs) { if (Test-Path (Split-Path $d -Parent)) { New-Item -ItemType Directory -Force -Path $d | Out-Null } }

function Save-Part([System.Drawing.Bitmap]$img, [string]$name) {
    $img.Save((Join-Path (Resolve-Path $srcDir) "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    foreach ($d in $outDirs) {
        if (Test-Path $d) { $img.Save((Join-Path (Resolve-Path $d) "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png) }
    }
    Write-Host ("  {0,-28} {1,3}x{2}" -f $name, $img.Width, $img.Height)
}

function Crop([int]$x,[int]$y,[int]$w,[int]$h) {
    return $b.Clone((New-Object System.Drawing.Rectangle $x,$y,$w,$h), [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
}

# Corners: the ornament turns at each corner, so those are one-offs and cannot be tiled.
$CORNER = 24
$tl = Crop $PANEL_X $TOP_Y $CORNER $BAND_H
$tr = Crop ($PANEL_X + $PANEL_W - $CORNER) $TOP_Y $CORNER $BAND_H
$hUnit = Crop $H_START $TOP_Y $H_LEN $BAND_H

Save-Part $tl    "spellbezel_corner_tl"
Save-Part $tr    "spellbezel_corner_tr"
Save-Part $hUnit "spellbezel_h_unit"

# Vertical run, taken from the left edge below the corner. Same band width, rotated so callers can
# tile it downward the same way they tile the horizontal one.
$vSrc = Crop $PANEL_X ($TOP_Y + $BAND_H + 40) $BAND_H $H_LEN
$vUnit = New-Object System.Drawing.Bitmap $vSrc
$vUnit.RotateFlip([System.Drawing.RotateFlipType]::Rotate90FlipNone)
Save-Part $vUnit "spellbezel_v_unit"

$tl.Dispose(); $tr.Dispose(); $hUnit.Dispose(); $vSrc.Dispose(); $vUnit.Dispose(); $b.Dispose()
Write-Host "done - components in $srcDir and shipped to ui\"
