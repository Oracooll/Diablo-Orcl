# Oracool asset pipeline: cuts ui\burger_menu_button.png from the two-state master.
#
#     powershell -ExecutionPolicy Bypass -File tools\CutBurgerMenuIcon.ps1
#
# Source: Resources\02. Oracooll Assets\02. Unused\bottom-hud\burger menu.png - a 428x220 RGBA sheet holding the two
# states side by side, dim and lit.
#
# Output is 52x26: two 26x26 cells in one row, indexed state * 26 by DrawBurgerMenuButton.
#
# ## Two states, not three
#
# The pack this replaces had inactive/hover/active. This master has two, so BurgerMenuButtonSize
# drops to 26x26 and the strip to two cells:
#
#   state 0  <- dim   (idle, and idle-with-cursor - the overlay does the hover)
#   state 1  <- lit   (the popup is open)
#
# hud_menu.cpp keeps the translucent overlay for hover, so the button still has three distinguishable
# looks out of two pictures: plain, plain-under-cursor, and lit-while-open.
#
# ## SUPERSEDES the burger entry in tools\CutHudStateIcons.ps1
#
# That script still cuts town_portal_icon.png and level_up_icon.png and remains the live one for
# those two. Running it now would overwrite this icon with the three-state pack art at 27x29 - the
# wrong size for the code as well as the wrong art - so its burger entry is disabled there rather
# than left as a trap. Same hazard CutLevelUpIcon.ps1 documents about HudIconCut.cs.
#
# ## The common-box rule, inherited
#
# Both states are fitted through ONE box - the larger of the two, re-centred on each panel's own
# centre - so they share a scale factor and land on the same pixel. Fitting them independently gives
# the lit state (whose glow spreads wider) a different scale, and the icon visibly jumps the moment
# the menu opens. CutLevelUpIcon.ps1 found that the hard way; this does not need to find it again.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$master = Join-Path (Split-Path -Parent $root) 'Resources\02. Oracooll Assets\02. Unused\bottom-hud\burger menu.png'
if (-not (Test-Path $master)) { throw "missing master: $master" }

$cell = 26
$img = [System.Drawing.Bitmap]::FromFile($master)
try {
    # Split the sheet into its panels by finding the fully transparent column gutters between them.
    $lit = New-Object 'bool[]' $img.Width
    for ($x = 0; $x -lt $img.Width; $x++) {
        for ($y = 0; $y -lt $img.Height; $y++) {
            if ($img.GetPixel($x, $y).A -gt 8) { $lit[$x] = $true; break }
        }
    }
    $spans = New-Object System.Collections.ArrayList
    $start = -1
    for ($x = 0; $x -lt $img.Width; $x++) {
        if ($lit[$x]) {
            if ($start -lt 0) { $start = $x }
        } elseif ($start -ge 0) {
            [void]$spans.Add([pscustomobject]@{ Left = $start; Right = $x - 1 })
            $start = -1
        }
    }
    if ($start -ge 0) { [void]$spans.Add([pscustomobject]@{ Left = $start; Right = $img.Width - 1 }) }

    # Anything narrower than a tenth of the sheet is a speck, not a panel.
    $panels = @($spans | Where-Object { ($_.Right - $_.Left + 1) -gt ($img.Width / 10) })
    if ($panels.Count -ne 2) {
        throw "expected 2 panels in the master, found $($panels.Count) - re-measure before cutting"
    }

    # Each panel's vertical extent, so the box is the art rather than the canvas.
    $boxes = foreach ($panel in $panels) {
        $top = $img.Height; $bottom = -1
        for ($y = 0; $y -lt $img.Height; $y++) {
            for ($x = $panel.Left; $x -le $panel.Right; $x += 2) {
                if ($img.GetPixel($x, $y).A -gt 8) {
                    if ($y -lt $top) { $top = $y }
                    if ($y -gt $bottom) { $bottom = $y }
                    break
                }
            }
        }
        [pscustomobject]@{
            X      = $panel.Left; Y = $top
            Width  = $panel.Right - $panel.Left + 1
            Height = $bottom - $top + 1
        }
    }

    # One box for both states. See the note above.
    $boxW = ($boxes | Measure-Object -Property Width -Maximum).Maximum
    $boxH = ($boxes | Measure-Object -Property Height -Maximum).Maximum

    $strip = New-Object System.Drawing.Bitmap -ArgumentList ($cell * 2), $cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($strip)
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    # Contain-fit so the whole square panel lands inside the cell without distortion.
    $scale = [Math]::Min($cell / $boxW, $cell / $boxH)
    $drawW = [Math]::Max(1, [int][Math]::Round($boxW * $scale))
    $drawH = [Math]::Max(1, [int][Math]::Round($boxH * $scale))

    for ($i = 0; $i -lt 2; $i++) {
        $b = $boxes[$i]
        $srcX = $b.X + ($b.Width / 2.0) - ($boxW / 2.0)
        $srcY = $b.Y + ($b.Height / 2.0) - ($boxH / 2.0)
        $src = New-Object System.Drawing.RectangleF($srcX, $srcY, $boxW, $boxH)
        $dst = New-Object System.Drawing.RectangleF(
            ($i * $cell + ($cell - $drawW) / 2.0),
            (($cell - $drawH) / 2.0),
            $drawW, $drawH)
        $g.DrawImage($img, $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
    }
    $g.Dispose()

    foreach ($dir in @('Packaging\resources\oracool_assets\ui', 'Packaging\resources\assets\ui')) {
        $strip.Save((Join-Path $root "$dir\burger_menu_button.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    }
    $strip.Dispose()
    Write-Host ("burger_menu_button.png  2 states, common box {0}x{1} -> {2}x{3} in {4}x{4} cells" -f `
            $boxW, $boxH, $drawW, $drawH, $cell)
} finally {
    $img.Dispose()
}
