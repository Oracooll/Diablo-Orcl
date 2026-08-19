# Oracool asset pipeline: cuts ui\middle_hud.png from the in-use bottom-HUD master.
#
#     powershell -ExecutionPolicy Bypass -File tools\CutHudPlate.ps1
#
# MPQ Unit E. Source is Oracool.MPQ\01-in-use\bottom-hud\hud-plate-v3.png - a 1536x1024 RGBA master
# whose art occupies a 1505x274 band at (16, 336).
#
# ## Why v3 is a drop-in and the earlier limestone package was not
#
# The limestone package (oracool-bottom-hud-limestone-package-v1.0.0.zip) measured 1497x297, aspect
# 5.04, against this plate's 5.53 - and it was 24bpp RGB on black, so it needed keying as well as
# scaling. Its eight slots did not sit where hud_layout's source rects expect them, which would have
# meant re-deriving all eight and moving the HUD's hit-testing with them.
#
# v3 is drawn on the SAME grid the current art uses: 1505 wide, the exact value of
# PlateSrcSize.width in oracool/hud_layout.cpp, with every slot opening landing inside the existing
# LmbWellSrc / RmbWellSrc / BeltCellSrcX rects. Verified by scaling the crop to 356x64 and drawing
# those rects over it before any code was touched - they hug the openings on all eight.
#
# So nothing in the layout moves. The one constant that changes is PlateSrcSize.height, 272 -> 274,
# because v3's band is two rows taller than the old art's. ScalePlate divides by WIDTH only, so that
# does not move a single source rect; it feeds PlateScreenSize.height, and 272 and 274 both scale to
# 64. Cropping to 272 instead would have clipped two rows off the bottom frame for no gain.
#
# ## Do not add a procedural bottom offset
#
# Standing note from the bottom-HUD packages, and PlateBottomMargin is already 0: the artwork is
# drawn screen-bottom flush. Anything that nudges it up leaves a gap the art was made to avoid.
#
# ## The Menu and Portal boxes are BLANK on purpose
#
# They are drawn into at runtime - DrawBurgerMenuButton and DrawTownPortalIcon in oracool/hud_art.cpp
# blit the three-state strips cut by tools\CutHudStateIcons.ps1 into belt cells 0 and 5. The plate
# supplies the frame; the icons supply what is inside it.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$master = Join-Path (Split-Path -Parent $root) 'Oracool.MPQ\01-in-use\bottom-hud\hud-plate-v3.png'
if (-not (Test-Path $master)) { throw "missing master: $master" }

# The art's own alpha bounding box, and the size hud_layout calls PlateSrcSize. Written out rather
# than re-detected each run: if a future master moves the band, this script should FAIL loudly on the
# assertion below rather than silently cut a different crop and leave the layout pointing at nothing.
$cropX = 16
$cropY = 336
$srcW = 1505
$srcH = 274
$outW = 356
$outH = 64

$img = [System.Drawing.Bitmap]::FromFile($master)
try {
    if ($img.Width -ne 1536 -or $img.Height -ne 1024) {
        throw "master is $($img.Width)x$($img.Height), expected 1536x1024 - re-measure before cutting"
    }

    # Confirm the band really is where we think. One row above the crop must be empty and the first
    # row of the crop must not be, or the layout's source rects are being applied to the wrong pixels.
    $rowHasArt = {
        param($y)
        for ($x = $cropX; $x -lt $cropX + $srcW; $x += 4) {
            if ($img.GetPixel($x, $y).A -gt 8) { return $true }
        }
        return $false
    }
    if (& $rowHasArt ($cropY - 1)) { throw "art found above y=$cropY - the band moved, re-measure" }
    if (-not (& $rowHasArt $cropY)) { throw "no art at y=$cropY - the band moved, re-measure" }

    $plate = New-Object System.Drawing.Bitmap -ArgumentList $outW, $outH, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($plate)
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.DrawImage($img,
        (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $outW, $outH),
        (New-Object System.Drawing.Rectangle -ArgumentList $cropX, $cropY, $srcW, $srcH),
        [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose()

    foreach ($dir in @('Packaging\resources\oracool_assets\ui', 'Packaging\resources\assets\ui')) {
        $plate.Save((Join-Path $root "$dir\middle_hud.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    }
    $plate.Dispose()
    Write-Host "middle_hud.png <- hud-plate-v3.png crop ${cropX},${cropY} ${srcW}x${srcH} -> ${outW}x${outH}"
} finally {
    $img.Dispose()
}
