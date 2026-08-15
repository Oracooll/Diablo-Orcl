# Oracool asset pipeline: cuts ui\paladin_skill_icons.png - the Paladin's Charge and Zeal icons -
# out of the two delivered reference images, as one horizontal strip the game indexes by skill.
#
# Unlike the aura and Barbarian sheets these arrive as one framed icon per FILE rather than as a
# grid, so there is no column/row detection to do. What is shared is the keying method: the
# background is removed by FLOOD FILL FROM THE BORDER, not by a colour threshold. Both icons have
# bright gold highlights at their centre and Zeal's sword arc is nearly white; a threshold wide
# enough to catch the key's anti-aliased fringe would punch holes straight through those. Only key
# colour reachable from outside the frame is background.
#
# Strip order IS PaladinSkill's enum order (Charge, Zeal) - GetPaladinSkillIconIndex is the
# identity, so the sheet and the enum cannot drift.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutPaladinSkills.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# In the art vault beside the other source art, not at its root where they were delivered.
$srcDir = "..\Oracool.MPQ\02-source-art\paladin-skills"
$sources = @("charge.png", "zeal.png")

# Matches the aura and Barbarian sheets, which match the small spell icon the Spells sheet uses, so
# every sheet in the Abilities window keeps the same row rhythm.
$ICON = 38

# The delivered key measures (2,251,4). Generous on green and strict on the other two channels: the
# artwork is entirely gold/brown/black, so nothing in it comes close to a high-green low-red pixel.
$KEY_G_MIN = 180
$KEY_RB_MAX = 110

$strip = New-Object System.Drawing.Bitmap ($ICON * $sources.Count), $ICON, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($strip)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

$index = 0
foreach ($name in $sources) {
    $path = Join-Path $srcDir $name
    if (-not (Test-Path $path)) { throw "source not found: $path" }

    $src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $path))
    $W = $src.Width; $H = $src.Height

    # Read once into a byte array: per-pixel GetPixel over 800k pixels in PowerShell is far too slow
    # to do repeatedly, and the flood fill below touches most of the image.
    $rect = New-Object System.Drawing.Rectangle 0, 0, $W, $H
    $data = $src.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride = $data.Stride
    $bytes = New-Object byte[] ($stride * $H)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
    $src.UnlockBits($data)
    $src.Dispose()

    # BGRA order in memory.
    function Test-Key([int]$x, [int]$y) {
        $i = $y * $stride + $x * 4
        return ($bytes[$i + 1] -ge $KEY_G_MIN -and $bytes[$i + 2] -le $KEY_RB_MAX -and $bytes[$i] -le $KEY_RB_MAX)
    }

    # --- Flood fill the key inward from the border. ---
    $bg = New-Object 'bool[,]' $W, $H
    $q = New-Object System.Collections.Generic.Queue[int]
    $enqueue = {
        param($lx, $ly)
        if ($lx -lt 0 -or $ly -lt 0 -or $lx -ge $W -or $ly -ge $H) { return }
        if ($bg[$lx, $ly]) { return }
        if (-not (Test-Key $lx $ly)) { return }
        $bg[$lx, $ly] = $true
        $q.Enqueue($ly * $W + $lx)
    }
    for ($x = 0; $x -lt $W; $x++) { & $enqueue $x 0; & $enqueue $x ($H - 1) }
    for ($y = 0; $y -lt $H; $y++) { & $enqueue 0 $y; & $enqueue ($W - 1) $y }
    while ($q.Count -gt 0) {
        $cur = $q.Dequeue(); $lx = $cur % $W; $ly = [int][Math]::Floor($cur / $W)
        & $enqueue ($lx + 1) $ly; & $enqueue ($lx - 1) $ly
        & $enqueue $lx ($ly + 1); & $enqueue $lx ($ly - 1)
    }

    # Tight bbox of what survived, so the icon is cropped to its own frame rather than to the
    # delivered canvas - the two files differ by 10px in width and the frames are not identically placed.
    $mnX = 999999; $mxX = -1; $mnY = 999999; $mxY = -1
    for ($ly = 0; $ly -lt $H; $ly++) {
        for ($lx = 0; $lx -lt $W; $lx++) {
            if ($bg[$lx, $ly]) { continue }
            if ($lx -lt $mnX) { $mnX = $lx }; if ($lx -gt $mxX) { $mxX = $lx }
            if ($ly -lt $mnY) { $mnY = $ly }; if ($ly -gt $mxY) { $mxY = $ly }
        }
    }
    if ($mxX -lt 0) { throw "$name : nothing left after keying - the flood fill ate the icon" }
    $fw = $mxX - $mnX + 1; $fh = $mxY - $mnY + 1

    # These frames are square. Asserting it catches a bad key here rather than in a screenshot.
    $ratio = $fh / $fw
    if ($ratio -lt 0.9 -or $ratio -gt 1.1) {
        throw ("{0}: cut is {1}x{2} (ratio {3:N2}) - not square, so the key leaked or ate an edge" -f $name, $fw, $fh, $ratio)
    }

    $cell = New-Object System.Drawing.Bitmap $fw, $fh, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($ly = 0; $ly -lt $fh; $ly++) {
        for ($lx = 0; $lx -lt $fw; $lx++) {
            # Indices precomputed into variables: arithmetic written inline between the commas of a
            # 2-D indexer is parsed as an ARRAY literal and fails with op_Addition.
            $bx = $mnX + $lx; $by = $mnY + $ly
            if ($bg[$bx, $by]) {
                $cell.SetPixel($lx, $ly, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
            } else {
                $i = $by * $stride + $bx * 4
                $cell.SetPixel($lx, $ly, [System.Drawing.Color]::FromArgb(255, $bytes[$i + 2], $bytes[$i + 1], $bytes[$i]))
            }
        }
    }
    $g.DrawImage($cell, ($index * $ICON), 0, $ICON, $ICON)
    $cell.Dispose()
    Write-Host ("  {0,-10} canvas {1}x{2} -> frame {3}x{4} at +{5},+{6}" -f $name, $W, $H, $fw, $fh, $mnX, $mnY)
    $index++
}
$g.Dispose()

# Every cell must carry something: a silently empty slot would show as a hole on the Skills sheet.
for ($i = 0; $i -lt $sources.Count; $i++) {
    $opaque = 0
    for ($y = 0; $y -lt $ICON; $y += 2) {
        for ($x = 0; $x -lt $ICON; $x += 2) {
            if ($strip.GetPixel($i * $ICON + $x, $y).A -ge 128) { $opaque++ }
        }
    }
    if ($opaque -lt 40) { throw "icon $i is blank after scaling ($opaque opaque samples)" }
}

$strip.Save((Join-Path (Resolve-Path $srcDir) "paladin_skill_icons.png"), [System.Drawing.Imaging.ImageFormat]::Png)
# oracool_assets ONLY, plus the build tree so a test run picks it up without a repack. Deliberately
# NOT Packaging\resources\assets\ui - that is the stock tree, nothing there ships unless it is in
# CMake\Assets.cmake's list, and writing there is exactly what left duplicate dead copies of
# difficulty_bg.png and the yellow TRNs behind (removed 2026-08-15).
foreach ($d in @("Packaging\resources\oracool_assets\ui", "build\x64-Debug\assets\ui")) {
    if (Test-Path $d) {
        $strip.Save((Join-Path (Resolve-Path $d) "paladin_skill_icons.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "  wrote $d\paladin_skill_icons.png"
    }
}
$strip.Dispose()
Write-Host ("done - {0} icons at {1}x{1}, strip {2}x{1}" -f $sources.Count, $ICON, ($ICON * $sources.Count))
