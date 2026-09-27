# Oracool asset pipeline: cuts ui\barb_skill_icons.png - the 18 Barbarian skill icons - out of the
# reference sheet, as one horizontal strip the game indexes by skill.
#
# The sheet is a 6x3 grid of stone tiles on a green screen. Each tile is a square artwork panel with
# the skill's name on a recessed plate at the BOTTOM OF THE SAME TILE.
#
# That is the important difference from the Paladin aura sheet, and it means the trick used there
# does not transfer. On the aura sheet the name plate hung BELOW the frame and was visibly narrower,
# so the frame's bottom could be found by scanning for where the card's width collapsed. Here the
# tile's outer border runs full width from top to bottom, straight past the name - a width scan
# finds the tile's real bottom edge every time and keeps the name.
#
# So the cut is geometric instead: the artwork panel is SQUARE, and the name plate is the remainder
# below it. Taking a square of the card's own width from the top of the card therefore lands just
# above the plate. Card is ~229x266, so this discards the bottom ~37px, which is the plate.
#
# The green is keyed at SOURCE resolution, before any downscale. Keying after scaling lets the
# resampler average green into the subject's edge pixels first, which leaves a green fringe that no
# later threshold can cleanly remove - a lesson from the item-icon sheets.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutBarbSkills.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# The sheet lives in the art vault beside the icons cut from it. It used to be read straight out of
# the user's Downloads folder, which is a staging area, not storage - by the time anyone next needed
# to re-cut, the file had been cleared out and this script could not run at all.
$sheet = "..\Resources\02. Oracooll Assets\02. Unused\barb-skills\barb-skills-sheet-6x3-greenscreen.png"
if (-not (Test-Path $sheet)) { throw "barb skill sheet not found: $sheet" }

$COLS = 6; $ROWS = 3; $COUNT = $COLS * $ROWS
# Matches the aura icons and the small spell icon, so every sheet keeps the same row rhythm.
$ICON = 38

# Resolve-Path, because Bitmap::FromFile resolves a relative path against the PROCESS working
# directory rather than PowerShell's - the two are not always the same.
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $sheet))
$W = $src.Width; $H = $src.Height
Write-Host ("sheet {0}x{1}" -f $W, $H)

$rect = New-Object System.Drawing.Rectangle 0, 0, $W, $H
$data = $src.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $data.Stride
$bytes = New-Object byte[] ($stride * $H)
[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
$src.UnlockBits($data)
$src.Dispose()

# BGRA in memory. Green screen measured at RGB(43,222,10): green dominant over BOTH other channels
# by a wide margin. Testing dominance rather than nearness to one sampled colour keeps the key
# working across the sheet's slightly uneven backdrop.
function Is-Green([int]$x, [int]$y) {
    $i = $y * $stride + $x * 4
    $b = $bytes[$i]; $g = $bytes[$i + 1]; $r = $bytes[$i + 2]
    return ($g -gt 120 -and $g -gt $r * 1.6 -and $g -gt $b * 1.6)
}

function Get-Runs([bool[]]$has) {
    $runs = @(); $start = -1
    for ($i = 0; $i -lt $has.Length; $i++) {
        if ($has[$i] -and $start -lt 0) { $start = $i }
        elseif (-not $has[$i] -and $start -ge 0) { $runs += , @($start, ($i - 1)); $start = -1 }
    }
    if ($start -ge 0) { $runs += , @($start, ($has.Length - 1)) }
    return $runs
}

$colHas = New-Object bool[] $W
for ($x = 0; $x -lt $W; $x++) {
    for ($y = 0; $y -lt $H; $y += 3) { if (-not (Is-Green $x $y)) { $colHas[$x] = $true; break } }
}
$rowHas = New-Object bool[] $H
for ($y = 0; $y -lt $H; $y++) {
    for ($x = 0; $x -lt $W; $x += 3) { if (-not (Is-Green $x $y)) { $rowHas[$y] = $true; break } }
}
$colRuns = @(Get-Runs $colHas)
$rowRuns = @(Get-Runs $rowHas)
if ($colRuns.Count -ne $COLS) { throw "expected $COLS columns, found $($colRuns.Count)" }
if ($rowRuns.Count -ne $ROWS) { throw "expected $ROWS rows, found $($rowRuns.Count)" }
Write-Host ("grid: {0} columns x {1} rows" -f $colRuns.Count, $rowRuns.Count)

$strip = New-Object System.Drawing.Bitmap ($ICON * $COUNT), $ICON, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($strip)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

$index = 0
foreach ($rr in $rowRuns) {
    foreach ($cr in $colRuns) {
        $cx0 = $cr[0]; $cx1 = $cr[1]; $cy0 = $rr[0]; $cy1 = $rr[1]
        $cardW = $cx1 - $cx0 + 1
        $cardH = $cy1 - $cy0 + 1

        # The square artwork panel, taken from the top of the tile. Anything below it is the name.
        $side = $cardW
        if ($side -gt $cardH) { $side = $cardH }
        # A tile that is not appreciably taller than it is wide would mean the name plate is missing
        # or the grid detection merged something - either way the square assumption no longer holds.
        if ($cardH -lt $cardW * 1.05) {
            throw ("skill {0}: card is {1}x{2}, too square to contain a name plate - check the grid" -f $index, $cardW, $cardH)
        }

        $cell = New-Object System.Drawing.Bitmap $side, $side, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        for ($ly = 0; $ly -lt $side; $ly++) {
            for ($lx = 0; $lx -lt $side; $lx++) {
                $sx = $cx0 + $lx; $sy = $cy0 + $ly
                if (Is-Green $sx $sy) {
                    $cell.SetPixel($lx, $ly, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
                } else {
                    $i = $sy * $stride + $sx * 4
                    $cell.SetPixel($lx, $ly, [System.Drawing.Color]::FromArgb(255, $bytes[$i + 2], $bytes[$i + 1], $bytes[$i]))
                }
            }
        }
        $g.DrawImage($cell, ($index * $ICON), 0, $ICON, $ICON)
        $cell.Dispose()
        Write-Host ("  skill {0,2}: card {1}x{2} -> square {3}, plate trimmed {4}px" -f $index, $cardW, $cardH, $side, ($cardH - $side))
        $index++
    }
}
$g.Dispose()
if ($index -ne $COUNT) { throw "cut $index icons, expected $COUNT" }

# Every cell must carry something, and none may show a surviving background fringe.
#
# The green check looks ONLY at the outermost ring, and that restriction is load-bearing rather than
# an optimisation: a whole-cell check fails legitimately on Natural Resistance, whose artwork is a
# green glowing figure, and it cannot tell that green from a keying miss. Background green can only
# enter the output where the tile meets the backdrop - the outer edge and its notched corners - so
# that is the only place worth testing, and there the tile is always grey stone.
$BORDER = 2
for ($i = 0; $i -lt $COUNT; $i++) {
    $opaque = 0; $edgeGreen = 0
    for ($y = 0; $y -lt $ICON; $y++) {
        for ($x = 0; $x -lt $ICON; $x++) {
            $p = $strip.GetPixel($i * $ICON + $x, $y)
            if ($p.A -lt 128) { continue }
            $opaque++
            $onEdge = ($x -lt $BORDER -or $y -lt $BORDER -or $x -ge ($ICON - $BORDER) -or $y -ge ($ICON - $BORDER))
            if ($onEdge -and $p.G -gt 120 -and $p.G -gt $p.R * 1.6 -and $p.G -gt $p.B * 1.6) { $edgeGreen++ }
        }
    }
    if ($opaque -lt 200) { throw "skill $i is blank after scaling ($opaque opaque px)" }
    if ($edgeGreen -gt 0) { throw "skill $i kept $edgeGreen green pixels on its border - the key missed" }
}

$srcDir = "..\Resources\02. Oracooll Assets\02. Unused\barb-skills"
New-Item -ItemType Directory -Force -Path $srcDir | Out-Null
$strip.Save((Join-Path (Resolve-Path $srcDir) "barb_skill_icons.png"), [System.Drawing.Imaging.ImageFormat]::Png)
foreach ($d in @("Packaging\resources\oracool_assets\ui", "Packaging\resources\assets\ui", "build\x64-Debug\assets\ui")) {
    if (Test-Path $d) {
        $strip.Save((Join-Path (Resolve-Path $d) "barb_skill_icons.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "  wrote $d\barb_skill_icons.png"
    }
}
$strip.Dispose()
Write-Host ("done - {0} icons at {1}x{1}, strip {2}x{1}" -f $COUNT, $ICON, ($ICON * $COUNT))
