# Oracool asset pipeline: cuts ui\town_portal_icon.png from the green-keyed three-state master.
#
#     powershell -ExecutionPolicy Bypass -File tools\CutTownPortalIcon.ps1
#
# Source: Oracool.MPQ\01-in-use\bottom-hud\town.portal.png - a 1536x1024 24bpp RGB sheet with three
# orbs on a green key, labelled ACTIVE / HOVER / CLICKED, and a block of descriptive text beneath
# them that is NOT part of the art.
#
# Output is 81x29: three 27x29 cells in one row, indexed state * 27 by DrawTownPortalIcon
# (TownPortalIconSize in oracool/hud_art.cpp). State order matches the sheet, which happens to match
# what hud_menu asks for: 0 idle, 1 hover, 2 pressed.
#
# ## Why the label text has to be excluded by measurement
#
# The captions are gold-on-green, so a plain "not green" test selects them exactly as readily as it
# selects the orbs, and the three columns would each grow a tail of text. The row profile is scanned
# instead and the ART BAND is taken as the first run of non-green rows: y 162..644 in this master,
# with the captions starting at y 706 after a clean 60-row gap. The script asserts that a gap exists
# rather than hard-coding 644, so a re-exported sheet with the orbs at a different height still cuts.
#
# ## Green key, and the fringe
#
# A binary key leaves a green halo wherever the orb's glow was anti-aliased against the background.
# So the test is on GREEN DOMINANCE and the alpha is graded by it: a pixel that is only slightly
# green-dominant survives at partial alpha with its green pulled down, which keeps the glow's soft
# edge instead of trading it for a hard rim.
#
# ## The common-box rule, inherited from CutLevelUpIcon.ps1
#
# All three states are fitted through ONE box - the largest, re-centred on each orb's own centre - so
# they share a scale factor and land on the same pixel. The CLICKED orb is visibly smaller than the
# other two by design ("energy contracts"), which is exactly the case where per-state fitting would
# scale it up to match and throw that design away.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$master = Join-Path (Split-Path -Parent $root) 'Oracool.MPQ\01-in-use\bottom-hud\town.portal.png'
if (-not (Test-Path $master)) { throw "missing master: $master" }

$cellW = 27
$cellH = 29

function Test-Green([System.Drawing.Color]$c) {
    return ($c.G -gt 140 -and $c.R -lt 130 -and $c.B -lt 130)
}

$img = [System.Drawing.Bitmap]::FromFile($master)
try {
    # 1. The art band: the first run of rows holding anything that is not background.
    $rowLit = New-Object 'bool[]' $img.Height
    for ($y = 0; $y -lt $img.Height; $y++) {
        $n = 0
        for ($x = 0; $x -lt $img.Width; $x += 2) {
            if (-not (Test-Green $img.GetPixel($x, $y))) { $n++; if ($n -gt 3) { break } }
        }
        $rowLit[$y] = ($n -gt 3)
    }
    $bandTop = -1; $bandBottom = -1
    for ($y = 0; $y -lt $img.Height; $y++) {
        if ($rowLit[$y]) {
            if ($bandTop -lt 0) { $bandTop = $y }
            $bandBottom = $y
        } elseif ($bandTop -ge 0 -and ($y - $bandBottom) -gt 20) {
            break # a clean gap: everything below is caption text
        }
    }
    if ($bandTop -lt 0 -or ($bandBottom - $bandTop) -lt 100) { throw "no art band found - re-measure" }
    if ($bandBottom -ge $img.Height - 1) { throw "art band runs to the sheet edge - the caption gap is missing" }

    # 2. The three orbs, as column runs inside that band.
    $colLit = New-Object 'bool[]' $img.Width
    for ($x = 0; $x -lt $img.Width; $x++) {
        for ($y = $bandTop; $y -le $bandBottom; $y++) {
            if (-not (Test-Green $img.GetPixel($x, $y))) { $colLit[$x] = $true; break }
        }
    }
    $spans = New-Object System.Collections.ArrayList
    $start = -1
    for ($x = 0; $x -lt $img.Width; $x++) {
        if ($colLit[$x]) {
            if ($start -lt 0) { $start = $x }
        } elseif ($start -ge 0) {
            [void]$spans.Add([pscustomobject]@{ Left = $start; Right = $x - 1 })
            $start = -1
        }
    }
    if ($start -ge 0) { [void]$spans.Add([pscustomobject]@{ Left = $start; Right = $img.Width - 1 }) }
    $orbs = @($spans | Where-Object { ($_.Right - $_.Left + 1) -gt 40 })
    if ($orbs.Count -ne 3) { throw "expected 3 orbs, found $($orbs.Count) - re-measure before cutting" }

    # Each orb's own vertical extent inside the band.
    $boxes = foreach ($orb in $orbs) {
        $top = $bandBottom; $bottom = $bandTop
        for ($y = $bandTop; $y -le $bandBottom; $y++) {
            for ($x = $orb.Left; $x -le $orb.Right; $x += 2) {
                if (-not (Test-Green $img.GetPixel($x, $y))) {
                    if ($y -lt $top) { $top = $y }
                    if ($y -gt $bottom) { $bottom = $y }
                    break
                }
            }
        }
        [pscustomobject]@{
            X      = $orb.Left; Y = $top
            Width  = $orb.Right - $orb.Left + 1
            Height = $bottom - $top + 1
        }
    }

    # 3. Key the whole band to RGBA once, so the scaler blends real alpha rather than green.
    $keyed = New-Object System.Drawing.Bitmap -ArgumentList $img.Width, $img.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($y = $bandTop; $y -le $bandBottom; $y++) {
        for ($x = 0; $x -lt $img.Width; $x++) {
            $c = $img.GetPixel($x, $y)
            # How far green leads the other two channels. Full lead is background; none is pure art.
            $lead = $c.G - [Math]::Max($c.R, $c.B)
            if ($lead -le 0) {
                $keyed.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $c.R, $c.G, $c.B))
                continue
            }
            if ($lead -ge 60) { continue } # left transparent
            # Partial: fade it out and pull the green spill down to the other channels' level, so the
            # soft edge of the glow survives without a green rim.
            $a = [int](255 * (60 - $lead) / 60)
            $g2 = [Math]::Max($c.R, $c.B)
            $keyed.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($a, $c.R, $g2, $c.B))
        }
    }

    # 4. One box for all three states.
    $boxW = ($boxes | Measure-Object -Property Width -Maximum).Maximum
    $boxH = ($boxes | Measure-Object -Property Height -Maximum).Maximum
    $scale = [Math]::Min($cellW / $boxW, $cellH / $boxH)
    $drawW = [Math]::Max(1, [int][Math]::Round($boxW * $scale))
    $drawH = [Math]::Max(1, [int][Math]::Round($boxH * $scale))

    $strip = New-Object System.Drawing.Bitmap -ArgumentList ($cellW * 3), $cellH, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($strip)
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    for ($i = 0; $i -lt 3; $i++) {
        $b = $boxes[$i]
        $srcX = $b.X + ($b.Width / 2.0) - ($boxW / 2.0)
        $srcY = $b.Y + ($b.Height / 2.0) - ($boxH / 2.0)
        $src = New-Object System.Drawing.RectangleF($srcX, $srcY, $boxW, $boxH)
        $dst = New-Object System.Drawing.RectangleF(
            ($i * $cellW + ($cellW - $drawW) / 2.0),
            (($cellH - $drawH) / 2.0),
            $drawW, $drawH)
        $g.DrawImage($keyed, $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
    }
    $g.Dispose()

    foreach ($dir in @('Packaging\resources\oracool_assets\ui', 'Packaging\resources\assets\ui')) {
        $strip.Save((Join-Path $root "$dir\town_portal_icon.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    }
    $strip.Dispose(); $keyed.Dispose()
    Write-Host ("town_portal_icon.png  band y {0}..{1}, 3 orbs, common box {2}x{3} -> {4}x{5} in {6}x{7} cells" -f `
            $bandTop, $bandBottom, $boxW, $boxH, $drawW, $drawH, $cellW, $cellH)
} finally {
    $img.Dispose()
}
