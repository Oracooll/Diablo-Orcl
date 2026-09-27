# BuildRedemptionRise.ps1 - the column of light Redemption raises over each corpse it consumes (dev note, 2026-09-27:
# "play scaled down resurection animation over every corpse consumed by redemption aura ... the animation to be
# recolored mixture of red and blue, representing recovery of mana and health").
#
# Reads vanilla's Resurrect beam, the exported strip Resources/01. Blizzard Assets/Animated Items 2/ressur1.png
# (16 frames of 96x160 side by side), and writes
#   Packaging/resources/oracool_assets/missiles/redemption_rise.png
# at $ScalePercent wide and $HeightPercent tall (60 and 30: 58x48 frames). misdat's row reads animWidth 58 and
# animWidth2 -3, which keeps the column on its tile's centre (58 / 2 + 3 = 32).
#
# The look (user, 2026-09-27, after the first cut read as a solid barber pole: "make it sparser and brighter at the
# core. try following a chesboards pattern maybe 2x2px red, 2x2px blue. use softer coloring"):
#   - each frame is resampled, then each row's span of the column is measured, so every pixel knows how far it is from
#     the centre line;
#   - off the core only every other pixel stays (a 1px checkerboard), so the edges read as a see-through mesh; the core
#     stays whole;
#   - the colour is a chessboard of 2x2 squares, soft rose and soft periwinkle - life and mana - lifted toward white at
#     the centre line. The sheet is quantised to the palette on load; the renders in OracoolPreview.DISABLED_RedemptionRise
#     show what survives.
#
# Usage: powershell -NoProfile -File tools\BuildRedemptionRise.ps1   (from the repo root)
param(
    [string]$Source = "..\Resources\01. Blizzard Assets\Animated Items 2\ressur1.png",
    [string]$Out = "Packaging\resources\oracool_assets\missiles\redemption_rise.png",
    [int]$ScalePercent = 60,
    # Half the column's height since 2026-09-27 (user: "lets reduce the height of the effect to half of what it is now"):
    # 58x48 frames. The width, and so misdat's animWidth 58 / animWidth2 -3, is unchanged.
    [int]$HeightPercent = 30
)
Add-Type -AssemblyName System.Drawing

$frames = 16; $srcW = 96; $srcH = 160
$fw = [int][Math]::Round($srcW * $ScalePercent / 100); $fh = [int][Math]::Round($srcH * $HeightPercent / 100)

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
# The column's own span, row by row in each frame: its centre and half-width, so every pixel knows how far it is from
# the core (0 at the centre line, 1 at the edge).
$spanL = New-Object 'int[,]' $frames, $small.Height
$spanR = New-Object 'int[,]' $frames, $small.Height
for ($f = 0; $f -lt $frames; $f++) {
    for ($y = 0; $y -lt $small.Height; $y++) {
        $spanL[$f, $y] = -1; $spanR[$f, $y] = -1
        for ($fx = 0; $fx -lt $fw; $fx++) {
            if ([int]$bytes[$y * $data.Stride + ($f * $fw + $fx) * 4 + 3] -ge 64) {
                if ($spanL[$f, $y] -lt 0) { $spanL[$f, $y] = $fx }
                $spanR[$f, $y] = $fx
            }
        }
    }
}
# Soft rose and soft periwinkle - the life and the mana - in 2x2 squares laid as a chessboard (user, 2026-09-27: "make
# it sparser and brighter at the core. try following a chesboards pattern maybe 2x2px red, 2x2px blue. use softer
# coloring"). Toward the core both lift to near white.
$rose = @(226, 112, 118); $periwinkle = @(112, 132, 232); $white = @(255, 244, 240)
$kept = 0
for ($y = 0; $y -lt $small.Height; $y++) {
    for ($x = 0; $x -lt $small.Width; $x++) {
        $i = $y * $data.Stride + $x * 4
        $a = [int]$bytes[$i + 3]
        $f = [Math]::Floor($x / $fw); $fx = $x % $fw
        $left = $spanL[$f, $y]; $right = $spanR[$f, $y]
        $keep = $false; $core = 0.0
        if ($a -ge 96 -and $left -ge 0) {
            $half = [Math]::Max(($right - $left) / 2.0, 0.5)
            $core = 1.0 - [Math]::Min(1.0, [Math]::Abs($fx - ($left + $right) / 2.0) / $half)
            # Sparse: off the core only every other pixel stays, on a 1px checkerboard; the centre line stays whole.
            $keep = ($core -ge 0.6) -or ((($x + $y) % 2) -eq 0)
        }
        if (-not $keep) { $bytes[$i] = 0; $bytes[$i + 1] = 0; $bytes[$i + 2] = 0; $bytes[$i + 3] = 0; continue }
        $l = [Math]::Max([int]$bytes[$i + 2], [Math]::Max([int]$bytes[$i + 1], [int]$bytes[$i])) / 255.0
        $bright = [Math]::Min(1.0, 0.55 + 0.45 * $l + 0.25 * $core)
        $tint = if (((([Math]::Floor($fx / 2)) + ([Math]::Floor($y / 2))) % 2) -eq 0) { $rose } else { $periwinkle }
        $w = [Math]::Pow($core, 2) * 0.75 # how far toward white: most at the centre line
        for ($c = 0; $c -lt 3; $c++) {
            $v = ($tint[$c] * (1 - $w) + $white[$c] * $w) * $bright
            $bytes[$i + 2 - $c] = [byte][Math]::Min(255, [Math]::Round($v))
        }
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
