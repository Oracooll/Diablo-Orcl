<#
.SYNOPSIS
    Cuts an inventory class silhouette (245x356, dark modelled figure on transparency) from a
    green-screened full-body class figure.
.DESCRIPTION
    The four original silhouettes and the delivered Monk one share a look: the figure in near-black
    greys (RGB 8..105) with faint rim modelling, fully transparent around it, about 8 px of air
    above the head and below the feet. This makes the same thing from one of the painted class
    figures in Resources\01-in-use-assets\class-art (1024x1536, pure green background):
      1. alpha = not-green (a pixel is background when green dominates red and blue by 60+);
      2. the figure's bounding box is scaled to fit 245x(356-16), centred, with high-quality
         resampling - the source is ~4x the target;
      3. each pixel's luminance is mapped onto 8..105, so the painting becomes a dark relief the
         way the Monk pack was normalised; the alpha edge is made binary.
    Written 2026-09-07 for the Bard, who had been borrowing the Rogue's figure.
.EXAMPLE
    .\tools\MakeClassSilhouette.ps1 -Source "..\Resources\01-in-use-assets\class-art\class-bard-greenscreen.png" -Output "Packaging\resources\oracool_assets\ui\silhouette_bard.png"
#>
param(
    [Parameter(Mandatory = $true)] [string]$Source,
    [Parameter(Mandatory = $true)] [string]$Output,
    [int]$Width = 245,
    [int]$Height = 356,
    [int]$Margin = 8
)
Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Source))

# 1. Key the green out into a 32-bit ARGB copy, and find the figure's bounds.
$keyed = New-Object System.Drawing.Bitmap($src.Width, $src.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$minX = $src.Width; $minY = $src.Height; $maxX = -1; $maxY = -1
for ($y = 0; $y -lt $src.Height; $y++) {
    for ($x = 0; $x -lt $src.Width; $x++) {
        $c = $src.GetPixel($x, $y)
        $isGreen = ($c.G - [Math]::Max($c.R, $c.B)) -ge 60
        if ($isGreen) { $keyed.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0)); continue }
        $keyed.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $c.R, $c.G, $c.B))
        if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
        if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
    }
}
$src.Dispose()
if ($maxX -lt 0) { throw "no figure found (everything read as green)" }
$figW = $maxX - $minX + 1; $figH = $maxY - $minY + 1
Write-Host ("figure bounds {0},{1} {2}x{3}" -f $minX, $minY, $figW, $figH)

# 2. Scale to fit, centred, high quality.
$innerH = $Height - 2 * $Margin
$scale = [Math]::Min($innerH / $figH, ($Width - 2) / $figW)
$dstW = [int][Math]::Round($figW * $scale); $dstH = [int][Math]::Round($figH * $scale)
$dstX = [int](($Width - $dstW) / 2); $dstY = [int](($Height - $dstH) / 2)
$out = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($out)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$srcRect = New-Object System.Drawing.Rectangle($minX, $minY, $figW, $figH)
$dstRect = New-Object System.Drawing.Rectangle($dstX, $dstY, $dstW, $dstH)
$g.DrawImage($keyed, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose(); $keyed.Dispose()

# 3. Luminance onto 8..105 (the Monk pack's normalisation), binary alpha at the edge.
$lo = 8; $hi = 105
for ($y = 0; $y -lt $Height; $y++) {
    for ($x = 0; $x -lt $Width; $x++) {
        $c = $out.GetPixel($x, $y)
        if ($c.A -lt 128) { $out.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0)); continue }
        $lum = (0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B) / 255.0
        # A touch of contrast so the relief reads at 30% opacity, then into the dark band.
        $lum = [Math]::Pow($lum, 1.35)
        $v = [int][Math]::Round($lo + ($hi - $lo) * $lum)
        $out.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $v, $v, $v))
    }
}
$dir = Split-Path -Parent (Resolve-Path -LiteralPath (Split-Path -Parent $Output) -ErrorAction SilentlyContinue)
$out.Save((Join-Path (Resolve-Path (Split-Path -Parent $Output)) (Split-Path -Leaf $Output)), [System.Drawing.Imaging.ImageFormat]::Png)
$out.Dispose()
Write-Host "wrote $Output ($Width x $Height, figure $dstW x $dstH at $dstX,$dstY)"
