# BuildEncyclopediaArt.ps1
#
# Cuts the wiki encyclopedia's pictures out of the art-export dump:
#   * one south-facing idle frame per monster family  -> wiki/encyclopedia/monsters/<family>.png
#   * every inventory icon, named by its curs index   -> wiki/encyclopedia/items/curs_<n>.png
#
# The monster sheets are one row per direction, frames left to right. Frame width comes from
# MonsterData.width (field 5) rather than being guessed, and the row height is the sheet height
# divided by the eight directions - both verified to divide evenly before anything is cut.
#
# Row 0 is south (DIR_S), which is the frame that faces the reader.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

<#
.SYNOPSIS
    The bounding box of everything painted in a bitmap, or $null if nothing is.

    A monster frame is mostly empty - the median is under a tenth painted, and a scavenger fills
    about a fortieth - because the sheet's cell has to hold the largest pose of the animation. Shown
    at a fixed height in a table that makes a bat a speck beside Diablo, so each portrait is trimmed
    to what is actually drawn.
#>
function Get-PaintedBounds([System.Drawing.Bitmap]$bmp) {
    $minX = $bmp.Width; $minY = $bmp.Height; $maxX = -1; $maxY = -1
    for ($y = 0; $y -lt $bmp.Height; $y++) {
        for ($x = 0; $x -lt $bmp.Width; $x++) {
            if ($bmp.GetPixel($x, $y).A -eq 0) { continue }
            if ($x -lt $minX) { $minX = $x }
            if ($x -gt $maxX) { $maxX = $x }
            if ($y -lt $minY) { $minY = $y }
            if ($y -gt $maxY) { $maxY = $y }
        }
    }
    if ($maxX -lt 0) { return $null }
    return @{ X = $minX; Y = $minY; W = $maxX - $minX + 1; H = $maxY - $minY + 1 }
}

$repo   = Split-Path -Parent $PSScriptRoot
# Where oracool_art_export.exe was told to write. Run that tool from the build directory - it reads
# the archives sitting beside the exe - with the categories: monsters items ui
$export = "C:\Diablo Orcl\art-export"
$out    = Join-Path $repo 'wiki\encyclopedia'

$monOut  = Join-Path $out 'monsters'
$itemOut = Join-Path $out 'items'
foreach ($d in @($monOut, $itemOut)) { New-Item -ItemType Directory -Path $d -Force | Out-Null }

# ---------------------------------------------------------------- monsters
$src = Get-Content (Join-Path $repo 'Source\monstdat.cpp') -Raw
$body = $src.Substring($src.IndexOf('const MonsterData MonstersData[]'))
$body = $body.Substring(0, $body.IndexOf('const UniqueMonsterData'))

$families = [ordered]@{}
foreach ($line in ($body -split "`n")) {
    if ($line -notmatch '^/\*\s*(MT_\w+)\s*\*/\s*\{') { continue }
    $flat = $line -replace 'P_\("[^"]*",\s*"[^"]*"\)', 'NAME'
    $flat = $flat -replace '\{[^{}]*\}', 'ARR'
    $parts = $flat.Substring($flat.IndexOf('{') + 1) -split ','
    if ($parts.Count -lt 12) { continue }
    $suffix = $parts[1].Trim().Trim('"')
    $width  = [int]$parts[5].Trim()
    if (-not $suffix) { continue }
    if (-not $families.Contains($suffix)) { $families[$suffix] = $width }
}

$cut = 0; $missing = @(); $odd = @(); $blank = @(); $trimmed = @()
foreach ($suffix in $families.Keys) {
    $bits   = $suffix -split '\\\\'
    $folder = $bits[0]
    $leaf   = $bits[-1]
    $png    = Join-Path $export "monsters\$folder\${leaf}_n.png"
    $key    = ($suffix -replace '\\\\', '_')
    if (-not (Test-Path $png)) { $missing += $suffix; continue }

    $img = [System.Drawing.Image]::FromFile($png)
    try {
        $frameW = [int]$families[$suffix]
        $rowH   = [int]($img.Height / 8)
        if ($frameW -le 0 -or ($img.Width % $frameW) -ne 0 -or ($img.Height % 8) -ne 0) {
            $odd += "$suffix (sheet $($img.Width)x$($img.Height), frameW $frameW)"
            continue
        }
        $dst = New-Object System.Drawing.Bitmap($frameW, $rowH)
        $g   = [System.Drawing.Graphics]::FromImage($dst)
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.DrawImage($img, (New-Object System.Drawing.Rectangle(0, 0, $frameW, $rowH)),
                           (New-Object System.Drawing.Rectangle(0, 0, $frameW, $rowH)),
                           [System.Drawing.GraphicsUnit]::Pixel)
        $g.Dispose()

        # Trim to what is painted, with a pixel of air so nothing touches the edge.
        $bounds = Get-PaintedBounds $dst
        if ($null -eq $bounds) {
            $blank += $suffix
            $dst.Dispose()
            continue
        }
        $margin = 1
        $bx = [Math]::Max(0, $bounds.X - $margin)
        $by = [Math]::Max(0, $bounds.Y - $margin)
        $bw = [Math]::Min($dst.Width - $bx, $bounds.W + 2 * $margin)
        $bh = [Math]::Min($dst.Height - $by, $bounds.H + 2 * $margin)
        $trim = New-Object System.Drawing.Bitmap($bw, $bh)
        $tg = [System.Drawing.Graphics]::FromImage($trim)
        $tg.Clear([System.Drawing.Color]::Transparent)
        $tg.DrawImage($dst, (New-Object System.Drawing.Rectangle(0, 0, $bw, $bh)),
                            (New-Object System.Drawing.Rectangle($bx, $by, $bw, $bh)),
                            [System.Drawing.GraphicsUnit]::Pixel)
        $tg.Dispose()
        $trim.Save((Join-Path $monOut "$key.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        $trimmed += [pscustomobject]@{ Key = $key; From = "$frameW x $rowH"; To = "$bw x $bh" }
        $trim.Dispose()
        $dst.Dispose()
        $cut++
    } finally { $img.Dispose() }
}

# ---------------------------------------------------------------- item icons
# objcurs holds the first sheet's frames, objcurs2 continues after it. An item's curs index maps
# straight onto that run, so the icons are renamed by index here and the pages never have to know
# which sheet a given item came from.
# Three sheets run end to end into one frame space: vanilla objcurs, then Hellfire's objcurs2, then
# Oracool's own oracool_items.cel - which is where every gem, rune, jewel, Mystic Orb, charm and
# signet lives.
#
# But a frame index is NOT an item's _iCurs. The width tables begin with the eleven non-item cursors
# (hand, identify, repair ... hourglass) before the items start, so the engine's own rule - see the
# loop at cursor.cpp:693 - is
#
#     _iCurs = frameIndex - (CURSOR_FIRSTITEM - 1)      with CURSOR_FIRSTITEM = 12
#
# Getting this wrong shifts EVERY icon by eleven, which is not obviously broken at a glance: each
# item simply wears a neighbour's picture. The check that it is right: sheet 3 starts at frame 240,
# and 240 - 11 = 229 = ICURS_ORACOOL_FIRST, exactly as cursor.cpp's static_assert states.
$CursorFrames = 11          # CURSOR_FIRSTITEM - 1
$first = (Get-ChildItem (Join-Path $export 'objcurs') -Filter *.png).Count
$second = (Get-ChildItem (Join-Path $export 'objcurs2') -Filter *.png).Count
$icons = 0
$sheets = @(
    @{ dir = 'objcurs';  offset = -$CursorFrames }
    @{ dir = 'objcurs2'; offset = $first - $CursorFrames }
    @{ dir = 'objcurs3'; offset = $first + $second - $CursorFrames }
)
foreach ($sheet in $sheets) {
    $dir = Join-Path $export $sheet.dir
    if (-not (Test-Path $dir)) { continue }
    foreach ($f in Get-ChildItem $dir -Filter *.png) {
        if ($f.Name -notmatch 'frame(\d+)\.png$') { continue }
        $curs = $sheet.offset + [int]$matches[1]
        if ($curs -lt 0) { continue }   # the eleven cursors themselves; no item wears them
        Copy-Item $f.FullName (Join-Path $itemOut ("curs_{0}.png" -f $curs)) -Force
        $icons++
    }
}

# ---------------------------------------------------------------- report
"families        : $($families.Count)"
"monster frames  : $cut"
"missing art     : $($missing.Count)"
$missing | ForEach-Object { "    $_" }
if ($blank.Count) { "BLANK frames    : $($blank.Count)"; $blank | ForEach-Object { "    $_" } }
if ($trimmed.Count) {
    "trimmed         : $($trimmed.Count) portraits"
    $trimmed | Select-Object -First 5 | ForEach-Object { "    {0,-22} {1,-12} -> {2}" -f $_.Key, $_.From, $_.To }
}
if ($odd.Count) { "ODD GEOMETRY    : $($odd.Count)"; $odd | ForEach-Object { "    $_" } }
"item icons      : $icons (objcurs $first + objcurs2 $second + oracool_items)"
$mf = Get-ChildItem $monOut -Filter *.png
$if2 = Get-ChildItem $itemOut -Filter *.png
"monsters on disk: {0} files, {1:N2} MB" -f $mf.Count, (($mf | Measure-Object Length -Sum).Sum / 1MB)
"items on disk   : {0} files, {1:N2} MB" -f $if2.Count, (($if2 | Measure-Object Length -Sum).Sum / 1MB)
