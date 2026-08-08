<#
.SYNOPSIS
    Decodes a raw Diablo/DevilutionX .CEL sprite (single-group; the format used by
    UI panel/button/background assets) plus a 256-color .PAL palette into one .png
    per frame.

.DESCRIPTION
    Oracool UI-overhaul asset pipeline: to redesign the HUD for the new 960x720
    canvas, the original panel/button/background art needs to be pulled out of
    diabdat.mpq/hellfire.mpq (via oracool_mpq_extract.exe, this same folder) and
    made viewable/editable. .CEL is Diablo's own indexed-color sprite format - not
    a standard image format - so this decoder exists for the same reason
    oracool_pcx_to_png.ps1 does for screenshots.

    Format, verified against DevilutionX's own from-scratch CEL parser
    (Source/utils/cel_to_clx.cpp) rather than guessed:
      - A CEL file holds one or more "frames" back to back. Offset 0 is a uint32
        LE frame count; frame count+1 more uint32 LE byte-offsets follow (each
        offset is where a frame starts, the last one is the end of the file/blob).
        (Multi-group CEL files, used for directional animation sets, wrap several
        of these blobs behind an outer offset table - this decoder only handles
        the plain single-group case, which covers every UI/panel asset; it detects
        and warns rather than silently mis-decoding if it hits a multi-group file.)
      - A frame's own bytes may start with an optional 10-byte sub-header, detected
        by checking whether the frame's first little-endian uint16 equals exactly
        10; if so, those 10 bytes are skipped.
      - Pixel data is control-byte-prefixed, scanline by scanline, left to right:
        a control byte 0x00-0x7F is an "opaque run" - that many raw palette-index
        bytes follow. A control byte 0x80-0xFF is a "transparent run" of length
        256-control (i.e. -control as a signed byte) - no data bytes, just skip
        that many pixels. Height isn't stored - it's simply how many full rows of
        exactly frameWidth pixels got decoded before the frame's bytes run out.
      - Frame width is NEVER stored in the file - DevilutionX's own code supplies
        it externally from a per-asset (sometimes per-frame) constant at the
        LoadCel() call site. You must pass -Width or -Widths yourself; see the
        LoadCel(...) call in Source/*.cpp for the asset you're decoding.

    Palette (.pal) is a flat, headerless 768-byte file: 256 sequential {R,G,B}
    triples, index i's color at bytes [3i, 3i+3). No transparency/alpha info -
    that comes entirely from the CEL control-byte scheme above.

.PARAMETER InputPath
    Path to the source .cel file.

.PARAMETER PalettePath
    Path to a raw 768-byte .pal file.

.PARAMETER OutputDir
    Directory to write one <basename>_frameNN.png per frame into (created if needed).

.PARAMETER Width
    Frame width in pixels, applied to every frame. Use this OR -Widths, not both.

.PARAMETER Widths
    Comma-separated per-frame widths (e.g. "95,41,41,41,41,41,41,41,41" for
    data\charbut.cel) when frames aren't all the same width. Use this OR -Width.

.EXAMPLE
    .\oracool_cel_to_png.ps1 -InputPath ctrlpan\panel8.cel -PalettePath town.pal -OutputDir out\panel8 -Width 640

.EXAMPLE
    .\oracool_cel_to_png.ps1 -InputPath data\charbut.cel -PalettePath town.pal -OutputDir out\charbut -Widths "95,41,41,41,41,41,41,41,41"
#>
param(
    [Parameter(Mandatory=$true)][string]$InputPath,
    [Parameter(Mandatory=$true)][string]$PalettePath,
    [Parameter(Mandatory=$true)][string]$OutputDir,
    [int]$Width = 0,
    [string]$Widths = ""
)

Add-Type -AssemblyName System.Drawing

function ReadUInt32LE($b, $offset) {
    return [uint32]$b[$offset] + ([uint32]$b[$offset+1] * 256) + ([uint32]$b[$offset+2] * 65536) + ([uint32]$b[$offset+3] * 16777216)
}
function ReadUInt16LE($b, $offset) {
    return [int]$b[$offset] + ([int]$b[$offset+1] * 256)
}

if ($Width -le 0 -and [string]::IsNullOrWhiteSpace($Widths)) {
    Write-Error "Must supply -Width or -Widths - CEL files never store frame width themselves. See the LoadCel(...) call site for this asset in Source/*.cpp."
    exit 1
}

$bytes = [System.IO.File]::ReadAllBytes($InputPath)
$fileSize = $bytes.Length

$maybeNumFrames = ReadUInt32LE $bytes 0
$isSingleGroup = (ReadUInt32LE $bytes (($maybeNumFrames * 4) + 4)) -eq $fileSize
if (-not $isSingleGroup) {
    Write-Error "This looks like a multi-group CEL (directional/animation set) - this decoder only handles single-group CEL (panel/button/background assets). Aborting rather than mis-decode."
    exit 1
}

$numFrames = $maybeNumFrames
Write-Output "CEL: $numFrames frame(s), file size $fileSize bytes"

$frameWidths = @()
if ($Widths -ne "") {
    $frameWidths = $Widths.Split(",") | ForEach-Object { [int]$_.Trim() }
    if ($frameWidths.Count -ne $numFrames) {
        Write-Error "-Widths gave $($frameWidths.Count) value(s) but the file has $numFrames frame(s)."
        exit 1
    }
} else {
    for ($i = 0; $i -lt $numFrames; $i++) { $frameWidths += $Width }
}

# 256-entry palette: flat 768-byte file, sequential {R,G,B} per index, no header.
$palBytes = [System.IO.File]::ReadAllBytes($PalettePath)
if ($palBytes.Length -lt 768) {
    Write-Error "Palette file is $($palBytes.Length) bytes, expected 768 (256 x RGB)."
    exit 1
}
$palette = New-Object 'System.Drawing.Color[]' 256
for ($i = 0; $i -lt 256; $i++) {
    $palette[$i] = [System.Drawing.Color]::FromArgb($palBytes[$i*3], $palBytes[$i*3+1], $palBytes[$i*3+2])
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$baseName = [System.IO.Path]::GetFileNameWithoutExtension($InputPath)

for ($frame = 0; $frame -lt $numFrames; $frame++) {
    $frameStart = ReadUInt32LE $bytes (4 + 4*$frame)
    $frameEnd = ReadUInt32LE $bytes (4 + 4*($frame+1))
    $frameWidth = $frameWidths[$frame]

    $pos = $frameStart
    # Optional 10-byte per-frame sub-header, detected by its own size field.
    if (($frameEnd - $pos) -ge 2 -and (ReadUInt16LE $bytes $pos) -eq 10) {
        $pos += 10
    }

    # Decode into a growable list of full rows (each row = frameWidth pixel indices,
    # -1 meaning transparent), since height isn't known up front.
    $rows = New-Object System.Collections.Generic.List[object]
    $row = New-Object 'int[]' $frameWidth
    $col = 0

    while ($pos -lt $frameEnd) {
        $control = $bytes[$pos]
        $pos++
        if ($control -ge 0x80) {
            $runLength = 256 - $control
            for ($i = 0; $i -lt $runLength; $i++) {
                if ($col -ge $frameWidth) {
                    $rows.Add($row)
                    $row = New-Object 'int[]' $frameWidth
                    $col = 0
                }
                $row[$col] = -1
                $col++
            }
        } else {
            $runLength = $control
            for ($i = 0; $i -lt $runLength; $i++) {
                if ($col -ge $frameWidth) {
                    $rows.Add($row)
                    $row = New-Object 'int[]' $frameWidth
                    $col = 0
                }
                $row[$col] = $bytes[$pos]
                $pos++
                $col++
            }
        }
    }
    if ($col -eq $frameWidth) {
        $rows.Add($row)
    } elseif ($col -ne 0) {
        Write-Warning "Frame $frame ended mid-row ($col of $frameWidth pixels) - width is probably wrong for this asset."
    }

    $frameHeight = $rows.Count
    if ($frameHeight -eq 0) {
        Write-Warning "Frame $frame decoded to 0 rows - skipping."
        continue
    }

    $bmp = New-Object System.Drawing.Bitmap($frameWidth, $frameHeight, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($y = 0; $y -lt $frameHeight; $y++) {
        $r = $rows[$y]
        for ($x = 0; $x -lt $frameWidth; $x++) {
            $idx = $r[$x]
            if ($idx -lt 0) {
                $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
            } else {
                $c = $palette[$idx]
                $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $c.R, $c.G, $c.B))
            }
        }
    }

    $outPath = Join-Path $OutputDir ("{0}_frame{1:D2}.png" -f $baseName, $frame)
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "  Frame ${frame}: ${frameWidth}x${frameHeight} -> $outPath"
}

Write-Output "Done: $numFrames frame(s) from $InputPath"
