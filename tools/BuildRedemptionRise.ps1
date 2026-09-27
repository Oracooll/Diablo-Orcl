# BuildRedemptionRise.ps1 - the column of light Redemption raises over each corpse it consumes (dev note, 2026-09-27:
# "play scaled down resurection animation over every corpse consumed by redemption aura ... the animation to be
# recolored mixture of red and blue, representing recovery of mana and health").
#
# Reads vanilla's Resurrect beam, the exported strip Resources/01. Blizzard Assets/Animated Items 2/ressur1.png
# (16 frames of 96x160 side by side), and writes
#   Packaging/resources/oracool_assets/missiles/redemption_rise.png
# at $ScalePercent (60: 58x96 frames, so misdat's row reads animWidth 58 and animWidth2 -3, which keeps the column on
# its tile's centre, 58 / 2 + 3 = 32).
#
# The beam is a stipple - a checkerboard of grey pixels that reads as translucent light - and a resample smears a
# checkerboard into a half-alpha grey, which the missile loader's binary alpha (>= 128 opaque) would turn into blotches.
# So each frame is resampled first and then DITHERED BACK: a pixel's resampled coverage is tested against an ordered
# 2x2 threshold, and the column comes out stippled at the new size the way it was at the old.
#
# The colour: every kept pixel keeps its lightness and takes red or blue by a diagonal band four pixels wide, so the
# column reads as two strands twisted together - life and mana - through the palette's red and blue ramps (the sheet is
# quantised to the palette on load; both ramps exist, there is no green one).
#
# Usage: powershell -NoProfile -File tools\BuildRedemptionRise.ps1   (from the repo root)
param(
    [string]$Source = "..\Resources\01. Blizzard Assets\Animated Items 2\ressur1.png",
    [string]$Out = "Packaging\resources\oracool_assets\missiles\redemption_rise.png",
    [int]$ScalePercent = 60
)
Add-Type -AssemblyName System.Drawing

$frames = 16; $srcW = 96; $srcH = 160
$fw = [int][Math]::Round($srcW * $ScalePercent / 100); $fh = [int][Math]::Round($srcH * $ScalePercent / 100)

$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Source))
if ($src.Width -ne $frames * $srcW -or $src.Height -ne $srcH) { throw "unexpected strip size $($src.Width)x$($src.Height)" }
$small = New-Object System.Drawing.Bitmap ($frames * $fw), $fh, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($small)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = 'HighQualityBilinear'; $g.PixelOffsetMode = 'HighQuality'; $g.CompositingMode = 'SourceCopy'
# Frame by frame, so no frame bleeds into its neighbour at the resample.
for ($f = 0; $f -lt $frames; $f++) {
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle ($f * $fw), 0, $fw, $fh), (New-Object System.Drawing.Rectangle ($f * $srcW), 0, $srcW, $srcH), [System.Drawing.GraphicsUnit]::Pixel)
}
$g.Dispose(); $src.Dispose()

$rect = New-Object System.Drawing.Rectangle 0, 0, $small.Width, $small.Height
$data = $small.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadWrite, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$n = $data.Stride * $small.Height
$bytes = New-Object byte[] $n
[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $n)
# 2x2 ordered thresholds on the 0-255 coverage: a checkerboard's ~50% keeps half its pixels, a solid core keeps all.
$thresholds = @(64, 160, 208, 112)
$kept = 0
for ($y = 0; $y -lt $small.Height; $y++) {
    for ($x = 0; $x -lt $small.Width; $x++) {
        $i = $y * $data.Stride + $x * 4
        $a = [int]$bytes[$i + 3]
        $t = $thresholds[($y % 2) * 2 + ($x % 2)]
        if ($a -lt $t) { $bytes[$i] = 0; $bytes[$i + 1] = 0; $bytes[$i + 2] = 0; $bytes[$i + 3] = 0; continue }
        # Lightness from the un-premultiplied grey, lifted a little so the thinned stipple does not read dimmer.
        $l = [Math]::Min(1.0, ([Math]::Max([int]$bytes[$i + 2], [Math]::Max([int]$bytes[$i + 1], [int]$bytes[$i])) / 255.0) * 1.15)
        $l = [Math]::Max($l, 0.35)
        $fx = $x % $fw
        $red = ([Math]::Floor(($fx + [Math]::Floor($y / 2)) / 4) % 2) -eq 0
        if ($red) { $r = 255 * $l; $gg = 48 * $l; $b = 40 * $l } else { $r = 56 * $l; $gg = 88 * $l; $b = 255 * $l }
        $bytes[$i + 2] = [byte][Math]::Round($r); $bytes[$i + 1] = [byte][Math]::Round($gg); $bytes[$i] = [byte][Math]::Round($b)
        $bytes[$i + 3] = 255
        $kept++
    }
}
[System.Runtime.InteropServices.Marshal]::Copy($bytes, 0, $data.Scan0, $n)
$small.UnlockBits($data)
$outPath = Join-Path (Resolve-Path (Split-Path $Out -Parent)).Path (Split-Path $Out -Leaf)
$small.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
$small.Dispose()
Write-Host ("{0}: {1} frames of {2}x{3}, {4} pixels kept" -f $outPath, $frames, $fw, $fh, $kept)
