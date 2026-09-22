# CutShopGlyphs.ps1 - the user's chroma-green glyph sheets -> the 28x28 shop glyph files.
#
# WHY THIS EXISTS: shop_glyph_transmute.png was installed on 2026-09-22 by scaling the source file
# straight down. The source is white art on chroma green, so what shipped was a 28x28 GREEN SQUARE
# sitting on Griswold's button plate in three windows - the Cube, Ogden's Craft tab and Gillian's.
# Nothing caught it because a glyph that draws is a glyph that "works". The recipe is written down
# here so the next one cannot be installed the same way.
#
# THE RECIPE, which is CutLevelUpIcon.ps1's and every other chroma cut in this tools folder:
#   1. key green -> fully transparent AT SOURCE RESOLUTION, before any resampling, so the chroma
#      edge is never averaged into the art by the resampler;
#   2. trim to the ink's bounding box;
#   3. fit the longest side to 21px and centre it in a 28x28 ARGB canvas - which is what every
#      glyph already in ui\ measures (refresh 21x20, recharge 21x20, sell 21x19, repair 18x21).
#
# Run from the repo root. Writes into Packaging\resources\oracool_assets\ui, so the MPQ must be
# repacked afterwards (tools\build_oracool_mpq.cmd) - a normal build does not rebuild it.

Add-Type -AssemblyName System.Drawing

$IconDir = "C:\Users\hroga\OneDrive\2. Personal Files\Software\Diablo\Resources\Icons"
$OutDir  = Join-Path (Get-Location) "Packaging\resources\oracool_assets\ui"
# How far green must lead red and blue before a pixel counts as chroma rather than art.
$GreenCut = 40
$Canvas = 28
$InkBox = 21

# source file -> asset name
$Jobs = @(
    @{ src = "Levski Cube - Transmute heart potion glyph 56x56.png"; out = "shop_glyph_transmute.png" },
    @{ src = "Reroll - single die glyph 56x56.png";                  out = "shop_glyph_reroll.png" },
    @{ src = "Reroll - two dice glyph 56x56.png";                    out = "shop_glyph_refresh_until.png" },
    # The pump, over my objection and at the user's word (2026-09-22: "use the pump instead of the
    # star for recharge"). Griswold's Recharge alone - Gillian's Imbue is its own file below.
    @{ src = "Recharge - gas pump glyph 56x56.png";                  out = "shop_glyph_recharge.png" },
    # Her Imbue, which held Griswold's old star for one build while it waited for a drawing of its
    # own (user, 2026-09-22: "use this icon as Imbue button icon - Imbue - rune shield glyph").
    @{ src = "Imbue - rune shield glyph 56x56.png";                  out = "shop_glyph_imbue.png" }
)

if (-not (Test-Path $OutDir)) { throw "asset folder not found: $OutDir - run this from the repo root" }

foreach ($job in $Jobs) {
    $path = Join-Path $IconDir $job.src
    if (-not (Test-Path $path)) { throw "source not found: $path" }
    $im = [System.Drawing.Bitmap]::FromFile($path)

    # 1. chroma out, at source resolution, measuring the ink as we go
    $keyed = New-Object System.Drawing.Bitmap $im.Width, $im.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $x0 = [int]::MaxValue; $x1 = -1; $y0 = [int]::MaxValue; $y1 = -1
    for ($y = 0; $y -lt $im.Height; $y++) {
        for ($x = 0; $x -lt $im.Width; $x++) {
            $p = $im.GetPixel($x, $y)
            if ($p.A -lt 16 -or ($p.G - [Math]::Max($p.R, $p.B)) -ge $GreenCut) {
                $keyed.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
            } else {
                $keyed.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $p.R, $p.G, $p.B))
                if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }
                if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y }
            }
        }
    }
    if ($x1 -lt 0) { throw "$($job.src) is chroma green edge to edge - nothing to cut" }

    # 2. trim, 3. fit and centre
    $iw = $x1 - $x0 + 1; $ih = $y1 - $y0 + 1
    $crop = $keyed.Clone((New-Object System.Drawing.Rectangle $x0, $y0, $iw, $ih), [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $out = New-Object System.Drawing.Bitmap $Canvas, $Canvas, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($out)
    $g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $scale = [Math]::Min($InkBox / [double]$iw, $InkBox / [double]$ih)
    $dw = [Math]::Max(1, [int][Math]::Round($iw * $scale))
    $dh = [Math]::Max(1, [int][Math]::Round($ih * $scale))
    $g.DrawImage($crop, [int](($Canvas - $dw) / 2), [int](($Canvas - $dh) / 2), $dw, $dh)
    $g.Dispose()

    # The check the first install did not make: a finished glyph has transparent corners and no
    # chroma left anywhere in it.
    $green = 0; $clear = 0
    for ($y = 0; $y -lt $Canvas; $y++) {
        for ($x = 0; $x -lt $Canvas; $x++) {
            $p = $out.GetPixel($x, $y)
            if ($p.A -lt 16) { $clear++ }
            elseif (($p.G - [Math]::Max($p.R, $p.B)) -ge $GreenCut) { $green++ }
        }
    }
    if ($green -gt 0) { throw "$($job.out) still carries $green chroma pixels" }
    if ($clear -eq 0) { throw "$($job.out) has no transparent pixel at all - the key did not run" }

    $out.Save((Join-Path $OutDir $job.out), [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host ("  {0,-30} ink {1}x{2} -> {3}x{4}, {5} clear px" -f $job.out, $iw, $ih, $dw, $dh, $clear)
    $out.Dispose(); $crop.Dispose(); $keyed.Dispose(); $im.Dispose()
}

Write-Host "done - repack with tools\build_oracool_mpq.cmd"
