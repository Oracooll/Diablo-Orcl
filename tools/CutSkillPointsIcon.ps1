# Cuts the unspent-skill-points frame from the user's delivery to the HUD's 64px icon.
#
# User, 2026-08-20: "take skill point.png from oracool.mpg and replace the current skill point
# indicator above rmb. in its center area in 40x39px area dead center in the icon i want you to draw
# with your own font the number of skill point available up to 99."
#
# This replaces the two 99-frame numbered strips (points_icons_dark/lit.png), where the NUMERAL was
# baked into the art. One frame plus a drawn number is strictly better here: it cannot run out at
# 100, it needs no strip regenerated when the cap moves, and the count is legible at any size the
# frame is ever scaled to.
#
# The source is a plain opaque square - an ornate border around a dark well - so there is no colour
# key and no alpha to preserve. It scales as a whole.
param(
    [string]$Source = "..\Resources\02-source-art\hud-icons\skill-points-icon-ornate-bezel-empty-master.png",
    [string]$Out    = "Packaging\resources\oracool_assets\ui\skill_points.png"
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repo = Split-Path -Parent $PSScriptRoot
$srcPath = if ([System.IO.Path]::IsPathRooted($Source)) { $Source } else { Join-Path $repo $Source }
$outPath = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $repo $Out }

if (-not (Test-Path $srcPath)) { throw "source not found: $srcPath" }

# 64, matching oracool::PointsIconSize. The number's 40x39 box is centred inside it by
# SkillPointsNumberRect in hud_art.cpp - see the note there on why the two are not both literals.
$Size = 64

$img = [System.Drawing.Image]::FromFile($srcPath)
try {
    $bmp = New-Object System.Drawing.Bitmap $Size, $Size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    # High quality: the source is ~20x the target, so a box filter would alias the border's filigree
    # into noise at this size.
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.DrawImage($img, 0, 0, $Size, $Size)
    $g.Dispose()

    $dir = Split-Path -Parent $outPath
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
} finally {
    $img.Dispose()
}

Write-Output ("wrote {0} ({1}x{1}) from {2}" -f $outPath, $Size, (Split-Path -Leaf $srcPath))
Write-Output "Now run tools\build_oracool_mpq.cmd to repack."
