# Oracool asset pipeline: cuts ui\paladin_skill_icons.png - the Paladin's skill icons - out of the
# delivered contact sheet, as one horizontal strip the game indexes by skill.
#
# REWRITTEN 2026-08-15 for the second delivery. The first arrived as one framed icon per FILE
# (charge.png, zeal.png) and this script cut two of them; the new one is a single 4x2 GRID with a
# text label under each icon, and it re-draws Charge and Zeal in the same hand as the five new
# skills. So all seven come from the one sheet - a strip mixing two deliveries would have shown two
# different line weights side by side on the same list.
#
# Two things the grid makes harder than the framed files did, and how each is handled:
#
#   The labels are drawn in the same white as the icons, so they cannot be told apart by colour.
#   They are separated by GEOMETRY instead: the sheet has four clean horizontal bands (icons, labels,
#   icons, labels) with empty green between them, and every bbox is taken inside an icon band only.
#   The band edges are found by scanning, not hardcoded, so a re-export at a different size still
#   cuts correctly - and the script fails loudly if it does not find exactly four bands.
#
#   The glyphs are unframed and their proportions differ - Charge is wide, Fist of the Heavens is
#   tall. The old script asserted each cut was square, which was right when a FRAME defined the cut
#   and would be wrong now. Each bbox is instead padded to square around its centre before scaling,
#   so nothing is stretched and every icon keeps its own silhouette inside a common cell.
#
# Keyed by colour threshold rather than by the border flood fill the old version used. That fill
# existed because the first icons had gold highlights a threshold could punch holes through; this
# artwork is white and black only, with nothing near the key, so the simpler test is also the safe
# one here.
#
# Strip order IS PaladinSkill's enum order - GetPaladinSkillIconIndex is the identity, so the sheet
# and the enum cannot drift. One grid cell is deliberately not emitted; see $layout for which and
# why. Skipping it in the CUTTER rather than filtering downstream is what keeps that true: the strip
# gets seven cells rather than eight with a hole to index around.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutPaladinSkills.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$srcPath = "..\Oracool.MPQ\Paladin Skills.png"

# Matches the aura and Barbarian sheets, which match the small spell icon the Spells sheet uses, so
# every sheet in the Abilities window keeps the same row rhythm.
$ICON = 38

# Grid cell -> the GAME SKILL that takes that cell's drawing. $null means "on the sheet, not in the
# game". Row-major, 4 columns x 2 rows, exactly as delivered.
#
# Cells 5 and 6 do not match their printed labels, deliberately (user request, 2026-08-15): the sheet
# labels cell 5 "Smite" and cell 6 "Shield Bash", but cell 5's drawing - a shield driven into a
# recoiling figure - is the one that reads as a bash, while cell 6 is a shield with an impact burst
# beside it. So Shield Bash takes cell 5's art and cell 6 is the one held back.
#
# This is the reason the mapping is a table of cell -> skill rather than a list of names to skip: the
# sheet's own labels are the artist's, and where the game disagrees with one, the disagreement should
# be visible on one line instead of hidden in an index somewhere downstream.
$layout = @(
    "Charge", "Zeal", "Hammer of Faith", "Blessed Shield",
    "Fist of the Heavens", "Shield Bash", $null, "Blessed Hammer"
)

# The delivered key measures (25,218,25). Generous on green, strict on the other two channels: the
# artwork is white and black, so nothing in it comes close to a high-green low-red pixel.
$KEY_G_MIN = 150
$KEY_RB_MAX = 120

if (-not (Test-Path $srcPath)) { throw "source not found: $srcPath" }
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $srcPath))
$W = $src.Width; $H = $src.Height
$rect = New-Object System.Drawing.Rectangle 0, 0, $W, $H
$data = $src.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $data.Stride
$bytes = New-Object byte[] ($stride * $H)
[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
$src.UnlockBits($data); $src.Dispose()
Write-Host ("source {0}x{1}" -f $W, $H)

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

# --- Find the four horizontal bands, then keep the two that hold icons. ---
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
if ($bands.Count -ne 4) {
    throw ("expected 4 horizontal bands (icons, labels, icons, labels), found {0} - the sheet's layout changed" -f $bands.Count)
}
# Bands alternate icons/labels, so the icon bands are the even ones.
$iconBands = @($bands[0], $bands[2])
foreach ($b in $iconBands) { Write-Host ("  icon band y {0}..{1}" -f $b[0], $b[1]) }

$columns = Get-Runs $colInk
if ($columns.Count -ne 4) {
    throw ("expected 4 columns, found {0} - the sheet's layout changed" -f $columns.Count)
}
# Column runs are the union of icon and label widths, and a label is often the wider of the two
# ("Fist of the Heavens" against a fist). They are only ever used to SPLIT the sheet here; each
# icon's real extent is measured inside its own cell below.
foreach ($c in $columns) { Write-Host ("  column x {0}..{1}" -f $c[0], $c[1]) }

$emitted = @()
$cells = @()
for ($r = 0; $r -lt 2; $r++) {
    for ($c = 0; $c -lt 4; $c++) {
        $cellIndex = $r * 4 + $c
        $name = $layout[$cellIndex]
        if ($null -eq $name) {
            Write-Host ("  cell {0}: skipped - no game skill takes this drawing" -f $cellIndex)
            continue
        }

        $y0 = $iconBands[$r][0]; $y1 = $iconBands[$r][1]
        $x0 = $columns[$c][0]; $x1 = $columns[$c][1]

        # Tight bbox of the glyph inside its own cell - the band above already excludes the label.
        $mnX = 999999; $mxX = -1; $mnY = 999999; $mxY = -1
        for ($y = $y0; $y -le $y1; $y++) {
            for ($x = $x0; $x -le $x1; $x++) {
                if (-not (Test-Ink $x $y)) { continue }
                if ($x -lt $mnX) { $mnX = $x }; if ($x -gt $mxX) { $mxX = $x }
                if ($y -lt $mnY) { $mnY = $y }; if ($y -gt $mxY) { $mxY = $y }
            }
        }
        if ($mxX -lt 0) { throw "$name : cell is empty - the key ate the icon or the bands are wrong" }

        $fw = $mxX - $mnX + 1; $fh = $mxY - $mnY + 1
        $cells += , @{ name = $name; x = $mnX; y = $mnY; w = $fw; h = $fh }
        $emitted += $name
    }
}

# --- One square cell size for the whole strip. ---
# Padding each glyph to its OWN square would scale each one to fill 38x38, so a wide icon and a tall
# one would end up drawn at different strokes-per-pixel and the set would stop looking like a set.
# One box for all seven, sized to the largest glyph, keeps their relative sizes as drawn.
$box = 0
foreach ($cell in $cells) {
    if ($cell.w -gt $box) { $box = $cell.w }
    if ($cell.h -gt $box) { $box = $cell.h }
}
Write-Host ("  common cell {0}x{0} px before scaling to {1}x{1}" -f $box, $ICON)

$strip = New-Object System.Drawing.Bitmap ($ICON * $cells.Count), $ICON, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($strip)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

$index = 0
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
    $g.DrawImage($square, ($index * $ICON), 0, $ICON, $ICON)
    $square.Dispose()
    Write-Host ("  {0,-22} slot {1}  glyph {2}x{3} at +{4},+{5}" -f $cell.name, $index, $cell.w, $cell.h, $cell.x, $cell.y)
    $index++
}
$g.Dispose()

# Every cell must carry something: a silently empty slot would show as a hole on the Skills sheet.
for ($i = 0; $i -lt $cells.Count; $i++) {
    $opaque = 0
    for ($y = 0; $y -lt $ICON; $y += 2) {
        for ($x = 0; $x -lt $ICON; $x += 2) {
            if ($strip.GetPixel($i * $ICON + $x, $y).A -ge 128) { $opaque++ }
        }
    }
    if ($opaque -lt 20) { throw ("icon {0} ({1}) is blank after scaling ({2} opaque samples)" -f $i, $cells[$i].name, $opaque) }
}

$outName = "paladin_skill_icons.png"
$vault = Resolve-Path "..\Oracool.MPQ\02-source-art\paladin-skills"
$strip.Save((Join-Path $vault $outName), [System.Drawing.Imaging.ImageFormat]::Png)
# oracool_assets ONLY, plus the build tree so a test run picks it up without a repack. Deliberately
# NOT Packaging\resources\assets\ui - that is the stock tree, nothing there ships unless it is in
# CMake\Assets.cmake's list, and writing there is exactly what left duplicate dead copies of
# difficulty_bg.png and the yellow TRNs behind (removed 2026-08-15).
foreach ($d in @("Packaging\resources\oracool_assets\ui", "build\x64-Debug\assets\ui")) {
    if (Test-Path $d) {
        $strip.Save((Join-Path (Resolve-Path $d) $outName), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "  wrote $d\$outName"
    }
}
$strip.Dispose()
Write-Host ("done - {0} icons at {1}x{1}, strip {2}x{1}: {3}" -f $cells.Count, $ICON, ($ICON * $cells.Count), ($emitted -join ", "))
