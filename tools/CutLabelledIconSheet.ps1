# Oracool asset pipeline: cuts a strip of NxN icons out of a delivered CONTACT SHEET - a grid of
# white glyphs on a green key with a text label printed under each one.
#
# Shared by every sheet that arrives in that format. It is the format the icon deliveries have
# settled into (Paladin Skills.png, Fist and Regular Attacks.png), and the alternative was a second
# copy of this file per sheet, which would have drifted the first time one of them was fixed.
#
# Two things the format makes harder than a folder of framed icons would, and how each is handled:
#
#   The labels are drawn in the same white as the icons, so they cannot be told apart by colour.
#   They are separated by GEOMETRY instead: a sheet of R rows has 2R clean horizontal bands - icons,
#   labels, icons, labels - with empty green between them, and every bbox is taken inside an icon
#   band only. The band edges are found by SCANNING, not hardcoded, so a re-export at a different
#   size still cuts correctly - and it throws if it does not find exactly 2R bands and C columns,
#   so a re-export with a changed LAYOUT fails loudly rather than quietly cutting somebody's label
#   into an icon.
#
#   The glyphs are unframed and their proportions differ. Each bbox is padded to square before
#   scaling, so nothing is stretched - and the padding box is ONE SIZE FOR THE WHOLE STRIP, taken
#   from the largest glyph on the sheet. Padding each glyph to its own square would scale every one
#   to fill the cell, so a wide icon and a tall one would end up drawn at different
#   strokes-per-pixel and the set would stop looking like a set.
#
# Keyed by colour threshold rather than by a border flood fill. A fill is needed when artwork holds
# colours a threshold could punch holes through (the first Paladin icons had gold highlights); these
# sheets are white and black with nothing near the key, so the simpler test is also the safe one.
#
# -Layout maps GRID CELL -> the game entity that takes that cell's drawing, row-major, with $null
# for a cell that is drawn but not shipped. Cell -> entity rather than a list of names to skip,
# because the sheet's printed labels are the artist's: where the game disagrees with one, the
# disagreement should be visible on one line in the caller instead of hidden in an index downstream.
# A skipped cell is dropped HERE rather than filtered later, so the strip has no hole to index
# around and its slot order stays the consumer's enum order.
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][AllowNull()][object[]]$Layout,
    [Parameter(Mandatory = $true)][int]$Columns,
    [Parameter(Mandatory = $true)][int]$Rows,
    [Parameter(Mandatory = $true)][string]$OutName,
    [Parameter(Mandatory = $true)][string]$VaultDir,
    # Matches the engine's small spell icon, which every Abilities window row is sized around.
    [int]$IconSize = 38
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

if ($Layout.Count -ne ($Columns * $Rows)) {
    throw ("layout has {0} cells but the grid is {1}x{2} = {3}" -f $Layout.Count, $Columns, $Rows, ($Columns * $Rows))
}
if (-not (Test-Path $Source)) { throw "source not found: $Source" }

# The delivered key measures (25,218,25). Generous on green, strict on the other two channels: the
# artwork is white and black, so nothing in it comes close to a high-green low-red pixel.
$KEY_G_MIN = 150
$KEY_RB_MAX = 120

$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Source))
$W = $src.Width; $H = $src.Height
$rect = New-Object System.Drawing.Rectangle 0, 0, $W, $H
$data = $src.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $data.Stride
$bytes = New-Object byte[] ($stride * $H)
[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
$src.UnlockBits($data); $src.Dispose()
Write-Host ("source {0} ({1}x{2})" -f (Split-Path $Source -Leaf), $W, $H)

# BGRA order in memory.
function Test-Ink([int]$x, [int]$y) {
    $i = $y * $stride + $x * 4
    return -not ($bytes[$i + 1] -ge $KEY_G_MIN -and $bytes[$i + 2] -le $KEY_RB_MAX -and $bytes[$i] -le $KEY_RB_MAX)
}

function Get-Runs([int[]]$arr) {
    $out = @()
    $start = -1
    for ($k = 0; $k -lt $arr.Length; $k++) {
        if ($arr[$k] -gt 0) { if ($start -lt 0) { $start = $k } }
        elseif ($start -ge 0) { $out += , @($start, ($k - 1)); $start = -1 }
    }
    if ($start -ge 0) { $out += , @($start, ($arr.Length - 1)) }
    return $out
}

# --- Find the horizontal bands, then keep the ones that hold icons. ---
$rowInk = New-Object int[] $H
$colInk = New-Object int[] $W
for ($y = 0; $y -lt $H; $y++) {
    $base = $y * $stride
    for ($x = 0; $x -lt $W; $x++) {
        $i = $base + $x * 4
        if (-not ($bytes[$i + 1] -ge $KEY_G_MIN -and $bytes[$i + 2] -le $KEY_RB_MAX -and $bytes[$i] -le $KEY_RB_MAX)) {
            $rowInk[$y]++; $colInk[$x]++
        }
    }
}
$bands = Get-Runs $rowInk
if ($bands.Count -ne ($Rows * 2)) {
    throw ("expected {0} horizontal bands ({1} rows of icons+labels), found {2} - the sheet's layout changed" -f ($Rows * 2), $Rows, $bands.Count)
}
# Bands alternate icons/labels, so the icon bands are the even ones.
$iconBands = @()
for ($r = 0; $r -lt $Rows; $r++) {
    $iconBands += , $bands[$r * 2]
    Write-Host ("  icon band y {0}..{1}" -f $bands[$r * 2][0], $bands[$r * 2][1])
}

$columnBands = Get-Runs $colInk
if ($columnBands.Count -ne $Columns) {
    throw ("expected {0} columns, found {1} - the sheet's layout changed" -f $Columns, $columnBands.Count)
}
# Column runs are the union of icon and label widths, and a label is often the wider of the two
# ("Fist of the Heavens" against a fist). They are only ever used to SPLIT the sheet here; each
# icon's real extent is measured inside its own cell below.
foreach ($c in $columnBands) { Write-Host ("  column x {0}..{1}" -f $c[0], $c[1]) }

$cells = @()
for ($r = 0; $r -lt $Rows; $r++) {
    for ($c = 0; $c -lt $Columns; $c++) {
        $cellIndex = $r * $Columns + $c
        $name = $Layout[$cellIndex]
        if ($null -eq $name) {
            Write-Host ("  cell {0}: skipped - nothing in the game takes this drawing" -f $cellIndex)
            continue
        }

        $y0 = $iconBands[$r][0]; $y1 = $iconBands[$r][1]
        $x0 = $columnBands[$c][0]; $x1 = $columnBands[$c][1]

        # Tight bbox of the glyph inside its own cell - the band already excludes the label.
        $mnX = 999999; $mxX = -1; $mnY = 999999; $mxY = -1
        for ($y = $y0; $y -le $y1; $y++) {
            for ($x = $x0; $x -le $x1; $x++) {
                if (-not (Test-Ink $x $y)) { continue }
                if ($x -lt $mnX) { $mnX = $x }; if ($x -gt $mxX) { $mxX = $x }
                if ($y -lt $mnY) { $mnY = $y }; if ($y -gt $mxY) { $mxY = $y }
            }
        }
        if ($mxX -lt 0) { throw "$name : cell is empty - the key ate the icon or the bands are wrong" }

        $cells += , @{ name = $name; x = $mnX; y = $mnY; w = ($mxX - $mnX + 1); h = ($mxY - $mnY + 1) }
    }
}

# One square cell size for the whole strip - see the header for why this is not per-glyph.
$box = 0
foreach ($cell in $cells) {
    if ($cell.w -gt $box) { $box = $cell.w }
    if ($cell.h -gt $box) { $box = $cell.h }
}
Write-Host ("  common cell {0}x{0} px before scaling to {1}x{1}" -f $box, $IconSize)

$strip = New-Object System.Drawing.Bitmap ($IconSize * $cells.Count), $IconSize, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($strip)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

$index = 0
$emitted = @()
foreach ($cell in $cells) {
    $square = New-Object System.Drawing.Bitmap $box, $box, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $offX = [int](($box - $cell.w) / 2)
    $offY = [int](($box - $cell.h) / 2)
    for ($ly = 0; $ly -lt $cell.h; $ly++) {
        for ($lx = 0; $lx -lt $cell.w; $lx++) {
            # Indices precomputed into variables: arithmetic written inline between the commas of a
            # 2-D indexer is parsed as an ARRAY literal and fails with op_Addition.
            $bx = $cell.x + $lx; $by = $cell.y + $ly
            if (-not (Test-Ink $bx $by)) { continue }
            $i = $by * $stride + $bx * 4
            $px = $offX + $lx; $py = $offY + $ly
            $square.SetPixel($px, $py, [System.Drawing.Color]::FromArgb(255, $bytes[$i + 2], $bytes[$i + 1], $bytes[$i]))
        }
    }
    $g.DrawImage($square, ($index * $IconSize), 0, $IconSize, $IconSize)
    $square.Dispose()
    Write-Host ("  {0,-22} slot {1}  glyph {2}x{3} at +{4},+{5}" -f $cell.name, $index, $cell.w, $cell.h, $cell.x, $cell.y)
    $emitted += $cell.name
    $index++
}
$g.Dispose()

# Every cell must carry something: a silently empty slot would show as a hole on the sheet it feeds.
for ($i = 0; $i -lt $cells.Count; $i++) {
    $opaque = 0
    for ($y = 0; $y -lt $IconSize; $y += 2) {
        for ($x = 0; $x -lt $IconSize; $x += 2) {
            if ($strip.GetPixel($i * $IconSize + $x, $y).A -ge 128) { $opaque++ }
        }
    }
    if ($opaque -lt 20) { throw ("icon {0} ({1}) is blank after scaling ({2} opaque samples)" -f $i, $cells[$i].name, $opaque) }
}

$strip.Save((Join-Path (Resolve-Path $VaultDir) $OutName), [System.Drawing.Imaging.ImageFormat]::Png)
# oracool_assets ONLY, plus the build tree so a test run picks it up without a repack. Deliberately
# NOT Packaging\resources\assets\ui - that is the stock tree, nothing there ships unless it is in
# CMake\Assets.cmake's list, and writing there is exactly what left duplicate dead copies of
# difficulty_bg.png and the yellow TRNs behind (removed 2026-08-15).
foreach ($d in @("Packaging\resources\oracool_assets\ui", "build\x64-Debug\assets\ui")) {
    if (Test-Path $d) {
        $strip.Save((Join-Path (Resolve-Path $d) $OutName), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "  wrote $d\$OutName"
    }
}
$strip.Dispose()
Write-Host ("done - {0} icons at {1}x{1}, strip {2}x{1}: {3}" -f $cells.Count, $IconSize, ($IconSize * $cells.Count), ($emitted -join ", "))
