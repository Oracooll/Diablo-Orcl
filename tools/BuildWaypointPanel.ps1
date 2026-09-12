# Oracool asset pipeline. Does four things, all from the 2026-08-13 art drop:
#   1. crops the seven stone textures  -> %TEMP%\oracool-waypoint-parts\texture_stone_v1..v7.png
#   2. cuts the segmented border kit   -> %TEMP%\oracool-waypoint-parts\border2_*.png
#      (parts only: nothing in the game reads them, so they stopped shipping in the MPQ - audit 2026-09-07)
#   3. cuts the waypoint pad icons     -> ui\waypoint_icons.png  (dormant | active)
#   4. assembles the waypoint panel    -> ui\waypoint_panel.png  (340x660)
#
# PANEL BUDGET, asserted below rather than left as arithmetic in a comment:
#     18 top border + 29 label + 17*35 rows + 18 bottom border = 660 exactly.
#
# All seventeen waypoints are listed (Tristram plus sixteen levels) - the first pass showed sixteen
# and had nowhere to put "17. Hell Level 16". Seventeen rows cost 595px, which is what squeezes the
# label: with the separator removed there are 29px left for it, down from 50. That is a bigger cut
# than "shrink a bit" implies, but at 340x660 with 35px rows it is the only value that fits, and
# shrinking the border instead would thin the frame the design already settled on.
#
# The horizontal separator between the label and the list is gone, on request.
#
# The kit's native bar is 27px; at that thickness this stack needs 678 and overruns, so the bars
# are scaled to 18.
#
# The background is a single 340x660 CROP of the texture, never a tile or a stretch. The textures
# are 1448x1086, so a panel-sized window fits with room to spare - and cropping means no seam can
# exist, which is the defect that shipped in the inventory grid when a small tile was repeated.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\BuildWaypointPanel.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$art  = "..\Resources\01-in-use-assets"
$inUse = "..\Resources\01-in-use-assets" # the folders were rebuilt as 01-in-use/02-concept on 2026-09-12
$dirs = @("Packaging\resources\oracool_assets\ui","Packaging\resources\assets\ui","build\x64-Debug\assets\ui")
foreach ($d in $dirs) { if (Test-Path (Split-Path $d -Parent)) { New-Item -ItemType Directory -Force -Path $d | Out-Null } }
function Save-All([System.Drawing.Bitmap]$bmp, [string]$name) {
    foreach ($d in $dirs) { if (Test-Path $d) { $bmp.Save((Join-Path (Resolve-Path $d) $name), [System.Drawing.Imaging.ImageFormat]::Png) } }
}
# Intermediate parts (textures, border pieces) go to a scratch folder, not the shipped tree
# (audit 2026-09-07: 4.9 MB of the MPQ was parts nothing reads).
$partDir = Join-Path $env:TEMP "oracool-waypoint-parts"
New-Item -ItemType Directory -Force -Path $partDir | Out-Null
function Save-Part([System.Drawing.Bitmap]$bmp, [string]$name) {
    $bmp.Save((Join-Path $partDir $name), [System.Drawing.Imaging.ImageFormat]::Png)
}
function Test-Green([System.Drawing.Color]$c) { return (($c.G - [Math]::Max($c.R, $c.B)) -ge 25) }

# Green-keys a rect out of a sheet at source resolution, before any scaling.
function Get-Keyed([System.Drawing.Bitmap]$src, [int[]]$r) {
    $o = New-Object System.Drawing.Bitmap $r[2], $r[3], ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($y = 0; $y -lt $r[3]; $y++) {
        for ($x = 0; $x -lt $r[2]; $x++) {
            $p = $src.GetPixel($r[0]+$x, $r[1]+$y)
            if (Test-Green $p) { $o.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(0,0,0,0)) }
            else { $o.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(255,$p.R,$p.G,$p.B)) }
        }
    }
    return $o
}

# ---- 1. textures ---------------------------------------------------------------------------
# Cropped, not scaled: a panel-sized window out of the middle of each. 480x720 covers any panel
# in the game today (the tallest is 720) with margin, and keeps the MPQ from ballooning - the
# full 1448x1086 originals are ~2.5MB each.
$TEX_W = 480; $TEX_H = 720
$texNames = @("v1-dark-tiles","v2-warm-tiles","v3-grey-tiles","v4-dark-mottled",
              "v5-light-mottled","v6-pale-veined","v7-pale-marble")
foreach ($t in $texNames) {
    $path = Join-Path $art "textures\texture-stone-$t.png"
    if (-not (Test-Path $path)) { throw "texture not found: $path" }
    $src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $path))
    if ($src.Width -lt $TEX_W -or $src.Height -lt $TEX_H) { throw "$t is $($src.Width)x$($src.Height), too small to crop ${TEX_W}x${TEX_H}" }
    $crop = $src.Clone((New-Object System.Drawing.Rectangle ([int](($src.Width-$TEX_W)/2)),([int](($src.Height-$TEX_H)/2)),$TEX_W,$TEX_H),
                       [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    Save-Part $crop ("texture_stone_" + $t.Split('-')[0] + ".png")
    $crop.Dispose(); $src.Dispose()
}
Write-Host "  textures: $($texNames.Count) shipped as ${TEX_W}x${TEX_H} crops"

# ---- 2. border kit -------------------------------------------------------------------------
# Rects from connected-component detection on the sheet. The four large components it also finds
# (171x171, 382x169, 385x225, 1420x243) are ASSEMBLED EXAMPLES showing how to combine these, not
# elements, so they are not shipped.
$kitPath = Join-Path $art "borders\border-kit-segmented-v2-greenscreen.png"
if (-not (Test-Path $kitPath)) { throw "border kit not found: $kitPath" }
$kit = [System.Drawing.Bitmap]::FromFile((Resolve-Path $kitPath))
$kitParts = [ordered]@{
    "border2_corner_1" = @(62,65,73,77);    "border2_corner_2" = @(198,65,72,77)
    "border2_corner_3" = @(62,246,73,79);   "border2_corner_4" = @(198,246,72,79)
    "border2_hbar_1"   = @(339,65,385,27);  "border2_hbar_2"   = @(339,180,385,27)
    "border2_hbar_3"   = @(339,298,386,26); "border2_hbar_4"   = @(400,408,267,26)
    "border2_vbar_1"   = @(796,66,27,532);  "border2_vbar_2"   = @(885,75,27,431)
    "border2_vbar_3"   = @(975,69,27,344)
    "border2_cross"    = @(124,505,160,154); "border2_stud"    = @(391,564,33,33)
}
foreach ($k in $kitParts.Keys) {
    $img = Get-Keyed $kit $kitParts[$k]
    Save-Part $img "$k.png"
    $img.Dispose()
}
Write-Host "  border kit: $($kitParts.Count) elements cut to $partDir"

# ---- 3. waypoint sigil icons ---------------------------------------------------------------
# Split at the midpoint - left dormant, right active - the same convention WaypointCel.cs uses for
# the in-world object.
#
# Source is the TOP-DOWN sigil, not the isometric floor pad in 01-in-use-assets\world. The pad is roughly
# 2:1, so contain-fitting it into a 30px cell left it about 30x21 and reading as a small lozenge;
# the top-down sigil is near-square and fills the cell.
$ICON_W = 30; $ICON_H = 30
$wpPath = Join-Path $art "hud-icons\waypoint-sigil-topdown-2-states.png"
if (-not (Test-Path $wpPath)) { throw "waypoint art not found: $wpPath" }
$wp = [System.Drawing.Bitmap]::FromFile((Resolve-Path $wpPath))
$half = [int]($wp.Width / 2)

# Tight-crop each half to its own content so the two pads end up the same on-screen size; the
# active one has a glow that would otherwise make it read larger.
function Get-ContentBox([System.Drawing.Bitmap]$b, [int]$x0, [int]$w) {
    $mnX=999999;$mxX=-1;$mnY=999999;$mxY=-1
    for ($y=0; $y -lt $b.Height; $y+=2) {
        for ($x=0; $x -lt $w; $x+=2) {
            $c = $b.GetPixel($x0+$x,$y)
            $lum = 0.299*$c.R + 0.587*$c.G + 0.114*$c.B
            if ($lum -lt 26) { continue }     # near-black backdrop
            if ($x -lt $mnX){$mnX=$x}; if ($x -gt $mxX){$mxX=$x}
            if ($y -lt $mnY){$mnY=$y}; if ($y -gt $mxY){$mxY=$y}
        }
    }
    return @(($x0+$mnX),$mnY,($mxX-$mnX+1),($mxY-$mnY+1))
}
$boxD = Get-ContentBox $wp 0 $half
$boxA = Get-ContentBox $wp $half $half
# One common box for both, so hovering a waypoint cannot make its icon jump - the same fix the
# level-up icon needed when its two states were measured independently.
$bw = [Math]::Max($boxD[2],$boxA[2]); $bh = [Math]::Max($boxD[3],$boxA[3])
$icons = New-Object System.Drawing.Bitmap ($ICON_W*2), $ICON_H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$ig = [System.Drawing.Graphics]::FromImage($icons)
$ig.Clear([System.Drawing.Color]::FromArgb(0,0,0,0))
$ig.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$i = 0
foreach ($bx in @($boxD,$boxA)) {
    $cx = $bx[0] + $bx[2]/2.0; $cy = $bx[1] + $bx[3]/2.0
    $sx = [int][Math]::Round($cx - $bw/2.0); $sy = [int][Math]::Round($cy - $bh/2.0)
    $scale = [Math]::Min($ICON_W / [double]$bw, $ICON_H / [double]$bh)
    $dw = [int][Math]::Round($bw*$scale); $dh = [int][Math]::Round($bh*$scale)
    $ig.DrawImage($wp, (New-Object System.Drawing.Rectangle ($i*$ICON_W + [int](($ICON_W-$dw)/2)),([int](($ICON_H-$dh)/2)),$dw,$dh),
                       (New-Object System.Drawing.Rectangle $sx,$sy,$bw,$bh), [System.Drawing.GraphicsUnit]::Pixel)
    $i++
}
$ig.Dispose(); $wp.Dispose()
Save-All $icons "waypoint_icons.png"
$icons.Dispose()
Write-Host "  waypoint icons: 2 states at ${ICON_W}x${ICON_H}, common box ${bw}x${bh}"

# ---- 4. the panel --------------------------------------------------------------------------
$PW = 340; $PH = 660
$BORDER = 18; $LABEL_H = 29; $ROW_H = 35; $ROWS = 17
$listTop = $BORDER + $LABEL_H
if (($listTop + $ROWS*$ROW_H + $BORDER) -ne $PH) {
    throw "panel budget does not close: $BORDER + $LABEL_H + $ROWS*$ROW_H + $BORDER = $($listTop + $ROWS*$ROW_H + $BORDER), expected $PH"
}

$texPath = Join-Path $art "textures\texture-stone-v7-pale-marble.png"   # most recent of the seven
$tex = [System.Drawing.Bitmap]::FromFile((Resolve-Path $texPath))
$panel = $tex.Clone((New-Object System.Drawing.Rectangle ([int](($tex.Width-$PW)/2)),([int](($tex.Height-$PH)/2)),$PW,$PH),
                    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$tex.Dispose()

$g = [System.Drawing.Graphics]::FromImage($panel)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias

# The background is left UNIFORM. An earlier revision darkened the list area so the pale marble
# would not fight the row text, which made the panel visibly two-tone - label band luma 143 against
# list 82 - and that banding was never asked for. The row text is outlined in black instead (see
# waypoint_menu.cpp), which fixes legibility at its source and leaves the texture as it is.

# Border, built from the kit: corners then bars tiled along the runs between them.
$cornerSrc = Get-Keyed $kit $kitParts["border2_corner_1"]
$hbarSrc   = Get-Keyed $kit $kitParts["border2_hbar_1"]
$vbarSrc   = Get-Keyed $kit $kitParts["border2_vbar_1"]
$CSZ = [int]($cornerSrc.Width * $BORDER / 27.0)   # keep the kit's own corner:bar proportion
function Get-Core([System.Drawing.Bitmap]$bar,[bool]$horiz) {
    if ($horiz) { $x=[int]($bar.Width*0.35); $w=[int]($bar.Width*0.30)
        return $bar.Clone((New-Object System.Drawing.Rectangle $x,0,$w,$bar.Height), $bar.PixelFormat) }
    $y=[int]($bar.Height*0.35); $h=[int]($bar.Height*0.30)
    return $bar.Clone((New-Object System.Drawing.Rectangle 0,$y,$bar.Width,$h), $bar.PixelFormat)
}
$hc = Get-Core $hbarSrc $true; $vc = Get-Core $vbarSrc $false
function Draw-Frame($gfx,[int]$x,[int]$y,[int]$w,[int]$h,[int]$bd,[int]$cs) {
    $cs = [Math]::Min($cs, [int]([Math]::Min($w,$h)/2))
    $runW = $w - 2*$cs; $runH = $h - 2*$cs
    for ($i=0; $i -lt $runW; $i += $hc.Width) {
        $seg=[Math]::Min($hc.Width,$runW-$i)
        $gfx.DrawImage($hc,(New-Object System.Drawing.Rectangle ($x+$cs+$i),$y,$seg,$bd),0,0,$seg,$hc.Height,[System.Drawing.GraphicsUnit]::Pixel)
        $gfx.DrawImage($hc,(New-Object System.Drawing.Rectangle ($x+$cs+$i),($y+$h-$bd),$seg,$bd),0,0,$seg,$hc.Height,[System.Drawing.GraphicsUnit]::Pixel)
    }
    for ($i=0; $i -lt $runH; $i += $vc.Height) {
        $seg=[Math]::Min($vc.Height,$runH-$i)
        $gfx.DrawImage($vc,(New-Object System.Drawing.Rectangle $x,($y+$cs+$i),$bd,$seg),0,0,$vc.Width,$seg,[System.Drawing.GraphicsUnit]::Pixel)
        $gfx.DrawImage($vc,(New-Object System.Drawing.Rectangle ($x+$w-$bd),($y+$cs+$i),$bd,$seg),0,0,$vc.Width,$seg,[System.Drawing.GraphicsUnit]::Pixel)
    }
    foreach ($c in @(@("tl",$x,$y),@("tr",($x+$w-$cs),$y),@("bl",$x,($y+$h-$cs)),@("br",($x+$w-$cs),($y+$h-$cs)))) {
        $cb = New-Object System.Drawing.Bitmap $cornerSrc
        switch ($c[0]) {
            "tr" { $cb.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipX) }
            "bl" { $cb.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY) }
            "br" { $cb.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipXY) }
        }
        $gfx.DrawImage($cb,$c[1],$c[2],$cs,$cs); $cb.Dispose()
    }
}
Draw-Frame $g 0 0 $PW $PH $BORDER $CSZ

# No separator bar between the label and the list - removed on request.

# "WAYPOINT" baked into the label band. Baked rather than drawn at runtime for the same reason the
# inventory sygil is: it never changes, and this way it can use a display face and a carved look
# the in-game bitmap font cannot produce.
#
# The fit loop now constrains HEIGHT as well as width. It only checked width before, which was fine
# against a 50px band but would overflow a 29px one and spill the glyphs into the first row.
$label = "WAYPOINT"
$fs = 30.0
$font = New-Object System.Drawing.Font "Georgia", $fs, ([System.Drawing.FontStyle]::Bold)
$sz = $g.MeasureString($label, $font)
while ((($sz.Width -gt ($PW - 2*$BORDER - 24)) -or ($sz.Height -gt ($LABEL_H - 2))) -and $fs -gt 8) {
    $font.Dispose(); $fs -= 1.0
    $font = New-Object System.Drawing.Font "Georgia", $fs, ([System.Drawing.FontStyle]::Bold)
    $sz = $g.MeasureString($label, $font)
}
Write-Host "  label: '$label' at ${fs}pt, $([int]$sz.Width)x$([int]$sz.Height) in a ${LABEL_H}px band"
$tx = ($PW - $sz.Width)/2.0
$ty = $BORDER + ($LABEL_H - $sz.Height)/2.0
$g.DrawString($label, $font, (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(190,0,0,0))), ($tx+2), ($ty+2))
$g.DrawString($label, $font, (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,226,196,126))), $tx, $ty)
$font.Dispose()
$g.Dispose()

Save-All $panel "waypoint_panel.png"
$panel.Dispose()
$cornerSrc.Dispose(); $hbarSrc.Dispose(); $vbarSrc.Dispose(); $hc.Dispose(); $vc.Dispose(); $kit.Dispose()
Write-Host "  panel: ${PW}x${PH}  border $BORDER  label $LABEL_H  no separator  rows ${ROWS}x${ROW_H} from y $listTop"
Write-Host "done"
