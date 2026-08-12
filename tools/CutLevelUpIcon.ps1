# Oracool asset pipeline: cuts ui\level_up_icon.png from the v5 level-up sheet's IMPROVED pair -
# the DEFAULT and CLICKED plaques, not the small unmodified Blizzard reference icon in the header.
#
# Output is 180x61: three 60x61 states side by side, matching LevelUpIconSize in
# Source/oracool/hud_layout.h and the blit in hud_art.cpp, which indexes state * width along one row.
#
# 60x61 is the LMB skill button's full on-screen footprint - its 50x51 opening plus the 5px bezel on
# each side - because the icon is drawn stacked directly above that button and has to match it.
# hud_layout.cpp static_asserts the header constant against the real scaled LMB rect, so if the
# plate geometry ever changes the build fails and this cell size has to be revisited with it.
#
# STATE MAPPING is deliberate and not one-to-one with the sheet. control.cpp asks for
# 0 = idle, 1 = hover, 2 = pressed, but the sheet only draws two plaques. Per instruction:
#   state 0 (idle)    <- DEFAULT
#   state 1 (hover)   <- CLICKED   (the glow is the hover cue)
#   state 2 (pressed) <- DEFAULT
# So the icon lights up under the cursor and drops back to plain while actually held down.
#
# The plaques are 521x548 / 529x548 - slightly taller than square - so they are contain-fitted and
# centred rather than stretched to fill the cell, which would distort them.
#
# They are fitted through a COMMON box, not each through its own. Fitting them independently made
# the plaque jump: CLICKED's detected box is 8px wider than DEFAULT's because its glow spreads
# further, so it scaled to 59px against DEFAULT's 58px and the two centred to x=0 and x=1. The
# right edge matched and the left did not, which reads as the border twitching every time the
# button is hovered. Both states now use one box - the largest, centred on each plaque's own
# centre - so they share a scale factor and land on the same pixel.
#
# Supersedes CutLevelUp in tools/HudIconCut.cs, which still targets the old
# hud-icons\level-up-icon-3-states.png at 40x60. That function is not wired into any build script;
# running it would overwrite this asset with the old art at the old size.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutLevelUpIcon.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$sheet = "..\Oracool.MPQ\02-source-art\hud-icons\level-up-icon-v5-red-plaque-flared-cross-2-states.png"
$CELL_W = 60
$CELL_H = 61
$GreenCut = 25

# Button boxes from connected-component detection on the sheet.
$DEFAULT = @(190, 413, 521, 548)
$CLICKED = @(815, 413, 529, 548)
$order = @($DEFAULT, $CLICKED, $DEFAULT)   # idle, hover, pressed

# One box for every state: the largest of them, re-centred on each plaque's own centre. This is
# what keeps the states from shifting relative to each other - see the header note.
$boxW = [Math]::Max($DEFAULT[2], $CLICKED[2])
$boxH = [Math]::Max($DEFAULT[3], $CLICKED[3])
function Get-CommonBox($b) {
    $cx = $b[0] + $b[2] / 2.0
    $cy = $b[1] + $b[3] / 2.0
    return @([int][Math]::Round($cx - $boxW/2.0), [int][Math]::Round($cy - $boxH/2.0), $boxW, $boxH)
}

if (-not (Test-Path $sheet)) { throw "sheet not found: $sheet" }
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $sheet))

$out = New-Object System.Drawing.Bitmap ($CELL_W*3), $CELL_H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($out)
$g.Clear([System.Drawing.Color]::FromArgb(0,0,0,0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

for ($s = 0; $s -lt 3; $s++) {
    $b = Get-CommonBox $order[$s]
    # Re-centring grows the smaller state's box, so make sure it did not run off the sheet or it
    # would silently read garbage instead of failing.
    if ($b[0] -lt 0 -or $b[1] -lt 0 -or ($b[0]+$b[2]) -gt $src.Width -or ($b[1]+$b[3]) -gt $src.Height) {
        throw "state $s common box ($($b[0]),$($b[1])) $($b[2])x$($b[3]) falls outside the $($src.Width)x$($src.Height) sheet"
    }
    # Green -> transparent at SOURCE resolution, before any downscale, so the chroma edge is never
    # averaged into the plaque by the resampler.
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
    $scale = [Math]::Min($CELL_W / [double]$b[2], $CELL_H / [double]$b[3])
    $dw = [Math]::Max(1, [int][Math]::Round($b[2] * $scale))
    $dh = [Math]::Max(1, [int][Math]::Round($b[3] * $scale))
    $g.DrawImage($crop, ($s*$CELL_W + [int](($CELL_W - $dw)/2)), [int](($CELL_H - $dh)/2), $dw, $dh)
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
Write-Host "done - $($CELL_W*3)x$CELL_H, ${CELL_W}x${CELL_H} states (idle / hover=clicked / pressed=default)"
