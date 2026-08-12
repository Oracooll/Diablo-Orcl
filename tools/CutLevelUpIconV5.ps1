# Oracool asset pipeline: cuts ui\level_up_icon.png from the v5 level-up sheet (the Blizzard
# original cross on a red plaque), replacing the older 40x60 three-state icon.
#
# Output is 60x20 - three 20x20 states side by side, matching LevelUpIconSize in
# Source/oracool/hud_layout.h and the blit in hud_art.cpp's DrawLevelUpIconArt, which indexes
# state * width along a single row.
#
# STATE MAPPING is deliberate and not one-to-one with the sheet. control.cpp asks for
# 0 = idle, 1 = hover, 2 = pressed, but the sheet only draws DEFAULT and CLICKED. Per instruction:
#   state 0 (idle)    <- DEFAULT
#   state 1 (hover)   <- CLICKED   (the glowing one; the glow is the hover cue)
#   state 2 (pressed) <- DEFAULT
# So the icon lights up under the cursor and drops back to plain while actually held down.
#
# Supersedes CutLevelUp in tools/HudIconCut.cs, which still targets the old
# hud-icons\level-up-icon-3-states.png at 40x60. That function is not wired into any build script;
# running it would overwrite this asset with the old art at the old size.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutLevelUpIconV5.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$sheet = "..\Oracool.MPQ\02-source-art\hud-icons\level-up-icon-v5-red-plaque-flared-cross-2-states.png"
$CELL = 20
$GreenCut = 25

# Button boxes from connected-component detection on the sheet. The third component it finds
# (202x179 at 666,91) is the small unmodified Blizzard reference icon in the header, not a state.
$DEFAULT = @(190, 413, 521, 548)
$CLICKED = @(815, 413, 529, 548)
$order = @($DEFAULT, $CLICKED, $DEFAULT)   # idle, hover, pressed

if (-not (Test-Path $sheet)) { throw "sheet not found: $sheet" }
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $sheet))

$out = New-Object System.Drawing.Bitmap ($CELL*3), $CELL, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($out)
$g.Clear([System.Drawing.Color]::FromArgb(0,0,0,0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

for ($s = 0; $s -lt 3; $s++) {
    $b = $order[$s]
    # Green -> transparent at source resolution, before any downscale, so the chroma edge is never
    # averaged into the stone by the resampler.
    $crop = New-Object System.Drawing.Bitmap $b[2], $b[3], ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($y = 0; $y -lt $b[3]; $y++) {
        for ($x = 0; $x -lt $b[2]; $x++) {
            $p = $src.GetPixel($b[0]+$x, $b[1]+$y)
            if (($p.G - [Math]::Max($p.R, $p.B)) -ge $GreenCut) {
                $crop.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0,0,0,0))
            } else {
                $crop.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $p.R, $p.G, $p.B))
            }
        }
    }
    # Contain-fit: the plaques are 521x548 / 529x548, slightly taller than square, so forcing 20x20
    # would squash them. Fit the long side and centre the rest.
    $scale = [Math]::Min($CELL / [double]$b[2], $CELL / [double]$b[3])
    $dw = [Math]::Max(1, [int][Math]::Round($b[2] * $scale))
    $dh = [Math]::Max(1, [int][Math]::Round($b[3] * $scale))
    $dx = $s*$CELL + [int](($CELL - $dw)/2)
    $dy = [int](($CELL - $dh)/2)
    $g.DrawImage($crop, $dx, $dy, $dw, $dh)
    $crop.Dispose()
    Write-Host ("  state {0}: source {1}x{2} at ({3},{4}) -> {5}x{6}" -f $s,$b[2],$b[3],$b[0],$b[1],$dw,$dh)
}
$g.Dispose(); $src.Dispose()

foreach ($d in @("Packaging\resources\oracool_assets\ui","Packaging\resources\assets\ui","build\x64-Debug\assets\ui")) {
    if (Test-Path $d) {
        $out.Save((Join-Path (Resolve-Path $d) "level_up_icon.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "  wrote $d\level_up_icon.png"
    }
}
$out.Dispose()
Write-Host "done - $($CELL*3)x$CELL, ${CELL}x${CELL} states (idle / hover=clicked / pressed=default)"
