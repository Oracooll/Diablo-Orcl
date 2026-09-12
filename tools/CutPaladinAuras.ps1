# Oracool asset pipeline: cuts ui\aura_icons.png - the 24 Paladin aura icons - out of the reference
# sheet, as one horizontal strip the game indexes by aura.
#
# The sheet is a 6x4 grid of cards on white. Each card is a square stone frame holding a circular
# emblem, with a name plate hanging BELOW the frame and overlapping its bottom edge. The plate is
# not wanted: the game draws the aura's name as text beside the icon, so baking it in would give two
# names in two fonts, and would fix the name at cut time where a translation could never reach it.
#
# Finding where the frame ends and the plate begins is done by measurement, not by a guessed
# constant: the frame runs the full card width, the plate is visibly narrower, so scanning each
# row's content width down the card and cutting where it collapses separates them. Measured on the
# supplied sheet this lands at y~205 of a 207-tall card - close enough to "square" that a hardcoded
# square crop would ALMOST work, which is exactly the kind of almost that leaves a sliver of plate
# on one row and not another.
#
# The white background is removed by FLOOD FILL FROM THE CARD BORDER, not by a brightness threshold.
# Several emblems are white or near-white at their centre - Aura Mastery's figure, Cleanse's burst,
# Holy Freeze's snowflake - and a threshold would punch holes straight through them. Only white
# reachable from outside the frame is background.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutPaladinAuras.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# In the art vault beside the icons cut from it, not in Downloads - see the note in
# CutBarbSkills.ps1 about why reading source art out of a staging folder does not survive.
$sheet = "..\Resources\01-in-use-assets\auras\paladin-auras-sheet-6x4-greenscreen.png"
if (-not (Test-Path $sheet)) { throw "aura sheet not found: $sheet" }

$COLS = 6; $ROWS = 4; $COUNT = $COLS * $ROWS
# Matches the small spell icon the Spells sheet uses, so both sheets keep the same row rhythm.
$ICON = 38
# A pixel this bright on every channel is a candidate for background - but only if the flood fill
# can actually reach it from outside the card.
$WHITE = 232

$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $sheet))
$W = $src.Width; $H = $src.Height
Write-Host ("sheet {0}x{1}" -f $W, $H)

# Read once into a byte array: per-pixel GetPixel over 1.5M pixels in PowerShell is far too slow to
# do repeatedly, and the flood fill below touches most of the sheet.
$rect = New-Object System.Drawing.Rectangle 0, 0, $W, $H
$data = $src.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $data.Stride
$bytes = New-Object byte[] ($stride * $H)
[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
$src.UnlockBits($data)
$src.Dispose()

# BGRA order in memory.
function Get-A([int]$x, [int]$y) { return $bytes[$y * $stride + $x * 4 + 3] }
function Is-Content([int]$x, [int]$y) {
    $i = $y * $stride + $x * 4
    if ($bytes[$i + 3] -le 128) { return $false }
    return ($bytes[$i] -lt $WHITE -or $bytes[$i + 1] -lt $WHITE -or $bytes[$i + 2] -lt $WHITE)
}

# --- Grid: derive the card bands from the sheet rather than hardcoding them. ---
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
    for ($y = 0; $y -lt $H; $y += 2) { if (Is-Content $x $y) { $colHas[$x] = $true; break } }
}
$rowHas = New-Object bool[] $H
for ($y = 0; $y -lt $H; $y++) {
    for ($x = 0; $x -lt $W; $x += 2) { if (Is-Content $x $y) { $rowHas[$y] = $true; break } }
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

        $rowWidth = @{}
        for ($y = $cy0; $y -le $cy1; $y++) {
            $mn = -1; $mx = -1
            for ($x = $cx0; $x -le $cx1; $x++) {
                if (Is-Content $x $y) { if ($mn -lt 0) { $mn = $x }; $mx = $x }
            }
            $rowWidth[$y] = if ($mx -lt 0) { 0 } else { $mx - $mn + 1 }
        }

        # The frame's own width, taken from the top 60% of the card where there is certainly no
        # plate. Measuring it over the whole card would let the plate influence the very number the
        # plate is supposed to be detected against.
        $frameWidth = 0
        $probeBottom = $cy0 + [int](($cy1 - $cy0) * 0.6)
        for ($y = $cy0; $y -le $probeBottom; $y++) { if ($rowWidth[$y] -gt $frameWidth) { $frameWidth = $rowWidth[$y] } }

        # Scan UP from the bottom of the card for the last row still at full frame width. The frame
        # holds its width right down to its bottom edge; the plate is about 88% of it and tapering.
        # Searching downward for the first NARROW row instead - which is what the first version did -
        # stops at the first row the emblem's glow happens to inset, somewhere up in the artwork.
        $frameBottom = -1
        for ($y = $cy1; $y -ge $cy0; $y--) {
            if ($rowWidth[$y] -ge ($frameWidth * 0.97)) { $frameBottom = $y; break }
        }
        if ($frameBottom -lt 0) { throw "aura $index : never found the frame's bottom edge" }

        # These frames are square. Asserting it is what catches a bad cut here rather than in a
        # screenshot - the first version silently kept the whole name plate on every icon, which a
        # ratio check would have failed immediately.
        $frameHeight = $frameBottom - $cy0 + 1
        $ratio = $frameHeight / $frameWidth
        if ($ratio -lt 0.9 -or $ratio -gt 1.1) {
            throw ("aura {0}: cut is {1}x{2} (ratio {3:N2}) - not square, so the name plate is probably still attached" -f $index, $frameWidth, $frameHeight, $ratio)
        }

        # --- Flood fill the background inward from the card's border. ---
        $bw = $cx1 - $cx0 + 1; $bh = $frameBottom - $cy0 + 1
        $bg = New-Object 'bool[,]' $bw, $bh
        $q = New-Object System.Collections.Generic.Queue[int]
        $enqueue = {
            param($lx, $ly)
            if ($lx -lt 0 -or $ly -lt 0 -or $lx -ge $bw -or $ly -ge $bh) { return }
            if ($bg[$lx, $ly]) { return }
            if (Is-Content ($cx0 + $lx) ($cy0 + $ly)) { return }
            $bg[$lx, $ly] = $true
            $q.Enqueue($ly * $bw + $lx)
        }
        for ($lx = 0; $lx -lt $bw; $lx++) { & $enqueue $lx 0; & $enqueue $lx ($bh - 1) }
        for ($ly = 0; $ly -lt $bh; $ly++) { & $enqueue 0 $ly; & $enqueue ($bw - 1) $ly }
        while ($q.Count -gt 0) {
            $cur = $q.Dequeue(); $lx = $cur % $bw; $ly = [int][Math]::Floor($cur / $bw)
            & $enqueue ($lx + 1) $ly; & $enqueue ($lx - 1) $ly
            & $enqueue $lx ($ly + 1); & $enqueue $lx ($ly - 1)
        }

        # Tight bbox of what survived, so every icon is cropped to its own frame rather than to the
        # card band - the frames sit at slightly different offsets within their cards.
        $mnX = 9999; $mxX = -1; $mnY = 9999; $mxY = -1
        for ($ly = 0; $ly -lt $bh; $ly++) {
            for ($lx = 0; $lx -lt $bw; $lx++) {
                if ($bg[$lx, $ly]) { continue }
                if ($lx -lt $mnX) { $mnX = $lx }; if ($lx -gt $mxX) { $mxX = $lx }
                if ($ly -lt $mnY) { $mnY = $ly }; if ($ly -gt $mxY) { $mxY = $ly }
            }
        }
        if ($mxX -lt 0) { throw "aura $index : nothing left after keying - the flood fill ate the icon" }
        $fw = $mxX - $mnX + 1; $fh = $mxY - $mnY + 1

        $cell = New-Object System.Drawing.Bitmap $fw, $fh, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        for ($ly = 0; $ly -lt $fh; $ly++) {
            for ($lx = 0; $lx -lt $fw; $lx++) {
                # Indices precomputed into variables: arithmetic written inline between the commas
                # of a 2-D indexer is parsed as an ARRAY literal, and fails with op_Addition.
                $bx = $mnX + $lx; $by = $mnY + $ly
                $sx = $cx0 + $bx; $sy = $cy0 + $by
                if ($bg[$bx, $by]) {
                    $cell.SetPixel($lx, $ly, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
                } else {
                    $i = $sy * $stride + $sx * 4
                    $cell.SetPixel($lx, $ly, [System.Drawing.Color]::FromArgb(255, $bytes[$i + 2], $bytes[$i + 1], $bytes[$i]))
                }
            }
        }
        $g.DrawImage($cell, ($index * $ICON), 0, $ICON, $ICON)
        $cell.Dispose()
        Write-Host ("  aura {0,2}: card {1}x{2} -> frame {3}x{4} at +{5},+{6}" -f $index, $bw, ($cy1 - $cy0 + 1), $fw, $fh, $mnX, $mnY)
        $index++
    }
}
$g.Dispose()
if ($index -ne $COUNT) { throw "cut $index icons, expected $COUNT" }

# Every cell must carry something: a silently empty slot would show as a hole in the Auras sheet.
for ($i = 0; $i -lt $COUNT; $i++) {
    $opaque = 0
    for ($y = 0; $y -lt $ICON; $y += 2) {
        for ($x = 0; $x -lt $ICON; $x += 2) {
            if ($strip.GetPixel($i * $ICON + $x, $y).A -ge 128) { $opaque++ }
        }
    }
    if ($opaque -lt 40) { throw "aura $i is blank after scaling ($opaque opaque samples)" }
}

$srcDir = "..\Resources\01-in-use-assets\auras"
New-Item -ItemType Directory -Force -Path $srcDir | Out-Null
$strip.Save((Join-Path (Resolve-Path $srcDir) "aura_icons.png"), [System.Drawing.Imaging.ImageFormat]::Png)
foreach ($d in @("Packaging\resources\oracool_assets\ui", "Packaging\resources\assets\ui", "build\x64-Debug\assets\ui")) {
    if (Test-Path $d) {
        $strip.Save((Join-Path (Resolve-Path $d) "aura_icons.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "  wrote $d\aura_icons.png"
    }
}
$strip.Dispose()
Write-Host ("done - {0} icons at {1}x{1}, strip {2}x{1}" -f $COUNT, $ICON, ($ICON * $COUNT))
