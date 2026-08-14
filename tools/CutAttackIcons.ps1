# Oracool asset pipeline: cuts ui\attack_icons.png - the two attack icons the HUD's skill wells and
# the Abilities window's Skills sheet draw - out of their reference cards.
#
# Two separate 1254x1254 files rather than one grid sheet, so there is no grid to find: each holds a
# single framed stone tile centred on a green screen, with no name plate under it. The cut is
# therefore just "key the green, take the tile's bounding box, downscale" - simpler than the aura and
# Barbarian sheets, which had to solve for a grid and for a name plate sharing the tile.
#
# The green is keyed at SOURCE resolution, before the downscale, for the same reason as every other
# sheet in this pipeline: scaling first lets the resampler average green into the subject's edge
# pixels, leaving a fringe no later threshold can remove cleanly.
#
# ONE size, 38px, for both destinations. A larger 46px variant was cut for the HUD wells at one point
# - their openings are ~49x51, so 38 leaves a visible margin - and then dropped on an explicit call:
# the RMB well alternates between this icon and the engine's readied-spell icon, which is 37x38 and
# cannot be resized, so a bigger attack icon made that slot change size depending on what was in it.
# 38 also matches the aura and Barbarian strips, so every row of the Abilities window and every icon
# on the HUD plate is the same size. Consistency beat snugness; if that is ever revisited, the well
# geometry is in oracool/hud_layout.h.
#
# Output order IS the oracool::AttackIcon enum order - index 0 Regular Attack, 1 Fist Attack.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutAttackIcons.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$srcDir = "..\Oracool.MPQ\02-source-art\attack-skills"
# Order is the enum's order, not alphabetical - see oracool/attack_skills.h.
$cards = @("regular-attack-card-greenscreen.png", "fist-attack-card-greenscreen.png")

# Matches the engine's small spell icon (37x38) and the aura and Barbarian strips.
$ICON = 38

$strip = New-Object System.Drawing.Bitmap ($ICON * $cards.Count), $ICON, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($strip)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

$index = 0
foreach ($name in $cards) {
    $path = Join-Path $srcDir $name
    if (-not (Test-Path $path)) { throw "source card not found: $path" }

    # Resolve-Path, because Bitmap::FromFile resolves a relative path against the PROCESS working
    # directory rather than PowerShell's - the two are not always the same.
    $src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $path))
    $W = $src.Width; $H = $src.Height
    $rect = New-Object System.Drawing.Rectangle 0, 0, $W, $H
    $data = $src.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride = $data.Stride
    $bytes = New-Object byte[] ($stride * $H)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
    $src.UnlockBits($data)
    $src.Dispose()

    # BGRA in memory. Green dominant over BOTH other channels by a wide margin - the same test the
    # aura and Barbarian cutters use, which survives an unevenly lit backdrop that a
    # nearest-to-one-sampled-colour test would not.
    $isGreen = {
        param([int]$x, [int]$y)
        $i = $y * $stride + $x * 4
        $b = $bytes[$i]; $gg = $bytes[$i + 1]; $r = $bytes[$i + 2]
        return ($gg -gt 120 -and $gg -gt $r * 1.6 -and $gg -gt $b * 1.6)
    }

    $minX = $W; $maxX = -1; $minY = $H; $maxY = -1
    for ($y = 0; $y -lt $H; $y++) {
        for ($x = 0; $x -lt $W; $x++) {
            if (-not (& $isGreen $x $y)) {
                if ($x -lt $minX) { $minX = $x }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    if ($maxX -lt 0) { throw "$name is entirely green - the key is wrong for this card" }

    $cardW = $maxX - $minX + 1
    $cardH = $maxY - $minY + 1
    # The tile is drawn square. A bounding box that is not means the key leaked into the artwork or
    # caught something outside the tile, and the downscale below would stretch the icon.
    $ratio = [math]::Abs($cardW - $cardH) / [double]([math]::Max($cardW, $cardH))
    if ($ratio -gt 0.02) {
        throw ("{0}: tile bbox is {1}x{2}, not square (off by {3:P1}) - check the green key" -f $name, $cardW, $cardH, $ratio)
    }

    $cell = New-Object System.Drawing.Bitmap $cardW, $cardH, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($ly = 0; $ly -lt $cardH; $ly++) {
        for ($lx = 0; $lx -lt $cardW; $lx++) {
            $sx = $minX + $lx; $sy = $minY + $ly
            if (& $isGreen $sx $sy) {
                $cell.SetPixel($lx, $ly, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
            } else {
                $i = $sy * $stride + $sx * 4
                $cell.SetPixel($lx, $ly, [System.Drawing.Color]::FromArgb(255, $bytes[$i + 2], $bytes[$i + 1], $bytes[$i]))
            }
        }
    }
    $g.DrawImage($cell, ($index * $ICON), 0, $ICON, $ICON)
    $cell.Dispose()
    Write-Host ("  icon {0}: {1} -> tile {2}x{3} at ({4},{5})" -f $index, $name, $cardW, $cardH, $minX, $minY)
    $index++
}
$g.Dispose()

# Every cell must carry something, and none may keep a background fringe. The green test looks only
# at the outermost ring: that is the only place the backdrop can reach, and the tile's border there
# is always grey stone, so a green pixel on the ring can only be a keying miss. (The Barbarian sheet
# taught this - Natural Resistance's artwork is itself a green glowing figure, and a whole-cell test
# cannot tell that from a leak.)
#
# The opaque bounding box is checked too. The HUD's skill wells centre these icons geometrically, on
# the CELL, so a cell whose art did not fill its own cell would sit off-centre in the well no matter
# how exact the centring maths is. The tile is scaled to fill the cell exactly, so the bbox must be
# the whole cell - asserted rather than assumed.
$BORDER = 2
for ($i = 0; $i -lt $cards.Count; $i++) {
    $opaque = 0; $edgeGreen = 0
    $bx0 = $ICON; $bx1 = -1; $by0 = $ICON; $by1 = -1
    for ($y = 0; $y -lt $ICON; $y++) {
        for ($x = 0; $x -lt $ICON; $x++) {
            $p = $strip.GetPixel($i * $ICON + $x, $y)
            if ($p.A -lt 128) { continue }
            $opaque++
            if ($x -lt $bx0) { $bx0 = $x }; if ($x -gt $bx1) { $bx1 = $x }
            if ($y -lt $by0) { $by0 = $y }; if ($y -gt $by1) { $by1 = $y }
            $onEdge = ($x -lt $BORDER -or $y -lt $BORDER -or $x -ge ($ICON - $BORDER) -or $y -ge ($ICON - $BORDER))
            if ($onEdge -and $p.G -gt 120 -and $p.G -gt $p.R * 1.6 -and $p.G -gt $p.B * 1.6) { $edgeGreen++ }
        }
    }
    if ($opaque -lt 200) { throw "icon $i is blank after scaling ($opaque opaque px)" }
    if ($edgeGreen -gt 0) { throw "icon $i kept $edgeGreen green pixels on its border - the key missed" }
    if ($bx0 -ne 0 -or $by0 -ne 0 -or $bx1 -ne ($ICON - 1) -or $by1 -ne ($ICON - 1)) {
        throw ("icon {0}: art occupies ({1},{2})..({3},{4}) of a {5}x{5} cell - it must fill the cell, or the wells will centre the cell and not the art" -f `
            $i, $bx0, $by0, $bx1, $by1, $ICON)
    }
    Write-Host ("  icon {0}: {1} opaque px, bbox fills the cell" -f $i, $opaque)
}

$strip.Save((Join-Path (Resolve-Path $srcDir) "attack_icons.png"), [System.Drawing.Imaging.ImageFormat]::Png)
foreach ($d in @("Packaging\resources\oracool_assets\ui", "Packaging\resources\assets\ui", "build\x64-Debug\assets\ui")) {
    if (Test-Path $d) {
        $strip.Save((Join-Path (Resolve-Path $d) "attack_icons.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "  wrote $d\attack_icons.png"
    }
}
$strip.Dispose()
Write-Host ("done - {0} icons at {1}x{1}, strip {2}x{1}" -f $cards.Count, $ICON, ($ICON * $cards.Count))
