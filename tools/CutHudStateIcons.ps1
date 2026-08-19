# Oracool asset pipeline: rebuilds the three 3-state HUD icon strips from the drop-zone packages.
#
#     powershell -ExecutionPolicy Bypass -File tools\CutHudStateIcons.ps1
#
# MPQ Unit B, the icon refreshes. Three packages arrived in Oracool.MPQ's root, each shipping its
# states as separate 1254x1254 masters:
#
#   oracool-burger-menu-icon-pack-v1.1.0.zip   inactive / hover / active
#   oracool-portal-icon-pack-v1.0.0.zip        inactive / hover / click
#   oracool-level-up-icon-package-v1.0.0.zip   inactive / active          (two, not three)
#
# The engine wants each as ONE strip of three cells side by side, indexed `state * cellWidth` along
# a single row - see DrawBurgerMenuButton, DrawTownPortalIcon and DrawLevelUpIconArt in
# oracool/hud_art.cpp. Cell sizes are fixed by the code and must not change here:
#
#   ui\burger_menu_button.png   3 x 27x29  = 81x29    (BurgerMenuButtonSize, hud_art.cpp)
#   ui\town_portal_icon.png     3 x 27x29  = 81x29    (TownPortalIconSize,   hud_art.cpp)
#   ui\level_up_icon.png        3 x 60x61  = 180x61   (LevelUpIconSize,      hud_layout.h)
#
# ## The common-box rule, inherited from CutLevelUpIcon.ps1
#
# Every state of one icon is fitted through ONE box, not each through its own. That script found the
# bug the hard way and wrote it down: a glowing state's detected bounds are wider than the plain
# one's, so fitting them independently gives them different scale factors, and the icon visibly
# twitches the instant the cursor touches it. One box, one scale, one landing pixel.
#
# Aspect is preserved and the result is centred in its cell rather than stretched to fill it. The
# masters are nothing like the cells' shape - the portal art is TALLER than it is wide (787x1189
# content) against a 27x29 cell - so stretching would be a visible distortion rather than a fit.
#
# ## Level-up has two states for three cells
#
# The package ships inactive and active only. control.cpp asks for 0 = idle, 1 = hover, 2 = pressed,
# so the mapping is the same one CutLevelUpIcon.ps1 chose and for the same reason - the glow IS the
# hover cue:
#
#   state 0 (idle)    <- inactive
#   state 1 (hover)   <- active
#   state 2 (pressed) <- inactive
#
# ## This SUPERSEDES tools\CutLevelUpIcon.ps1 for ui\level_up_icon.png
#
# That script cuts the same asset from the older v5 sheet. Running it after this one silently
# reverts the level-up icon to the previous art at the same size, with no error - exactly the hazard
# it documents about tools\HudIconCut.cs. It is left in place for its history; this is the live one.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$dropZone = Join-Path (Split-Path -Parent $root) 'Oracool.MPQ'
$staging = Join-Path $env:TEMP 'oracool-unit-b'

# Both asset trees are kept byte-identical; writing one and copying is what every other cut script
# in this directory does.
$outPrimary = Join-Path $root 'Packaging\resources\oracool_assets\ui'
$outMirror = Join-Path $root 'Packaging\resources\assets\ui'

$packs = @(
    @{
        zip    = 'oracool-burger-menu-icon-pack-v1.1.0.zip'
        dir    = 'oracool-burger-menu-icon-pack/assets/masters'
        out    = 'burger_menu_button.png'
        cell   = @(27, 29)
        states = @('burger-menu-inactive-master-1254x1254.png',
            'burger-menu-hover-master-1254x1254.png',
            'burger-menu-active-master-1254x1254.png')
    },
    @{
        zip    = 'oracool-portal-icon-pack-v1.0.0.zip'
        dir    = 'oracool-portal-icon-pack/assets/masters'
        out    = 'town_portal_icon.png'
        cell   = @(27, 29)
        states = @('portal-icon-inactive-master-1254x1254.png',
            'portal-icon-hover-master-1254x1254.png',
            'portal-icon-click-master-1254x1254.png')
    },
    @{
        zip    = 'oracool-level-up-icon-package-v1.0.0.zip'
        dir    = 'oracool-level-up-icon-package/assets/masters'
        out    = 'level_up_icon.png'
        cell   = @(60, 61)
        # Three cells, two masters. See the state mapping note above.
        states = @('level-up-icon-inactive-master.png',
            'level-up-icon-active-master.png',
            'level-up-icon-inactive-master.png')
    }
)

<#
.SYNOPSIS
The bounding box of everything visible in $bmp.

.DESCRIPTION
Alpha where the master has it, luminance where it does not - the level-up masters ship as 24bpp RGB
on black, so an alpha test there would select the entire canvas and the art would be scaled to
nothing. Sampled every other pixel: these are ~1250px masters and a one-pixel error in a box that
will be divided by 40 cannot reach the output.
#>
function Get-ContentBox([System.Drawing.Bitmap]$bmp) {
    $hasAlpha = $bmp.PixelFormat.ToString() -match 'Argb'
    $minX = $bmp.Width; $maxX = -1; $minY = $bmp.Height; $maxY = -1
    for ($y = 0; $y -lt $bmp.Height; $y += 2) {
        for ($x = 0; $x -lt $bmp.Width; $x += 2) {
            $c = $bmp.GetPixel($x, $y)
            $visible = if ($hasAlpha) { $c.A -gt 8 } else { ($c.R + $c.G + $c.B) -gt 24 }
            if ($visible) {
                if ($x -lt $minX) { $minX = $x }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    if ($maxX -lt 0) { throw "no visible content found" }
    return [pscustomobject]@{
        X      = $minX; Y = $minY
        Width  = $maxX - $minX + 1
        Height = $maxY - $minY + 1
    }
}

if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Path $staging -Force | Out-Null

foreach ($pack in $packs) {
    $zip = Join-Path $dropZone $pack.zip
    if (-not (Test-Path $zip)) { throw "missing package: $zip" }
    $unpack = Join-Path $staging ([System.IO.Path]::GetFileNameWithoutExtension($pack.zip))
    Expand-Archive -Path $zip -DestinationPath $unpack -Force

    $masters = @()
    foreach ($state in $pack.states) {
        $path = Join-Path $unpack ($pack.dir.Replace('/', '\') + '\' + $state)
        if (-not (Test-Path $path)) { throw "missing master: $path" }
        $masters += [System.Drawing.Bitmap]::FromFile($path)
    }

    # ONE box for every state. The largest of the three, centred on each master's own centre, so
    # they share a scale factor and land on the same pixel. See the common-box note above.
    $boxes = $masters | ForEach-Object { Get-ContentBox $_ }
    $boxW = ($boxes | Measure-Object -Property Width -Maximum).Maximum
    $boxH = ($boxes | Measure-Object -Property Height -Maximum).Maximum

    $cellW = $pack.cell[0]
    $cellH = $pack.cell[1]
    # Contain-fit: the smaller of the two ratios, so the whole box lands inside the cell.
    $scale = [Math]::Min($cellW / $boxW, $cellH / $boxH)
    $drawW = [Math]::Max(1, [int][Math]::Round($boxW * $scale))
    $drawH = [Math]::Max(1, [int][Math]::Round($boxH * $scale))

    $strip = New-Object System.Drawing.Bitmap(($cellW * $masters.Count), $cellH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($strip)
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    for ($i = 0; $i -lt $masters.Count; $i++) {
        $b = $boxes[$i]
        # The common box, re-centred on THIS state's own content.
        $srcX = $b.X + ($b.Width / 2.0) - ($boxW / 2.0)
        $srcY = $b.Y + ($b.Height / 2.0) - ($boxH / 2.0)
        $src = New-Object System.Drawing.RectangleF($srcX, $srcY, $boxW, $boxH)
        $dst = New-Object System.Drawing.RectangleF(
            ($i * $cellW + ($cellW - $drawW) / 2.0),
            (($cellH - $drawH) / 2.0),
            $drawW, $drawH)
        $g.DrawImage($masters[$i], $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
    }

    $g.Dispose()
    foreach ($dir in @($outPrimary, $outMirror)) {
        $strip.Save((Join-Path $dir $pack.out), [System.Drawing.Imaging.ImageFormat]::Png)
    }
    $strip.Dispose()
    $masters | ForEach-Object { $_.Dispose() }

    Write-Host ("{0,-24} {1} states, common box {2}x{3} -> {4}x{5} in {6}x{7} cells" -f `
            $pack.out, $masters.Count, $boxW, $boxH, $drawW, $drawH, $cellW, $cellH)
}

Remove-Item $staging -Recurse -Force
Write-Host "wrote to Packaging\resources\oracool_assets\ui and the assets\ui mirror"
