<#
.SYNOPSIS
    Converts a DevilutionX/Diablo .pcx file (8-bit indexed, RLE-compressed, with a
    256-color VGA palette) to a viewable .png.

.DESCRIPTION
    Oracool bug-report workflow: the user's in-game screenshot key (F5, via
    Source/capture.cpp) saves screenshots as .pcx into Saved_Games, which isn't
    directly viewable by standard tools in this dev environment. This script decodes
    the format by hand - 128-byte header, RLE-compressed 8bpp scanlines, then a
    256-entry VGA palette in the trailing 769 bytes (marker 0x0C + 768 bytes RGB) -
    and writes a standard .png so the screenshot can actually be looked at.

    Not specific to screenshots - works on any 8bpp RLE PCX with a VGA palette,
    including the project's own asset .pcx files if it ever has any.

.PARAMETER InputPath
    Path to the source .pcx file.

.PARAMETER OutputPath
    Path to write the decoded .png file.

.EXAMPLE
    .\oracool_pcx_to_png.ps1 -InputPath "C:\...\Saved_Games\screenshot000.pcx" -OutputPath "C:\...\shot.png"
#>
param(
    [Parameter(Mandatory=$true)][string]$InputPath,
    [Parameter(Mandatory=$true)][string]$OutputPath
)

Add-Type -AssemblyName System.Drawing

$bytes = [System.IO.File]::ReadAllBytes($InputPath)

function ReadInt16LE($b, $offset) {
    return [int]$b[$offset] + ([int]$b[$offset+1] * 256)
}

$xmin = ReadInt16LE $bytes 4
$ymin = ReadInt16LE $bytes 6
$xmax = ReadInt16LE $bytes 8
$ymax = ReadInt16LE $bytes 10
$bytesPerLine = ReadInt16LE $bytes 66
$nPlanes = $bytes[65]
$bitsPerPixel = $bytes[3]

$width = $xmax - $xmin + 1
$height = $ymax - $ymin + 1

Write-Output "Image: ${width}x${height}, bpp=$bitsPerPixel, planes=$nPlanes, bytesPerLine=$bytesPerLine"

if ($bitsPerPixel -ne 8 -or $nPlanes -ne 1) {
    Write-Warning "This decoder only handles 8bpp, 1-plane PCX (the format DevilutionX writes). Got bpp=$bitsPerPixel planes=$nPlanes - output will likely be wrong."
}

# Decode RLE-compressed scanline data
$rowBytes = $nPlanes * $bytesPerLine
$decoded = New-Object byte[] ($rowBytes * $height)
$srcPos = 128
$dstPos = 0
$totalNeeded = $decoded.Length

while ($dstPos -lt $totalNeeded) {
    $b = $bytes[$srcPos]
    $srcPos++
    if (($b -band 0xC0) -eq 0xC0) {
        $count = $b -band 0x3F
        $val = $bytes[$srcPos]
        $srcPos++
        for ($i = 0; $i -lt $count -and $dstPos -lt $totalNeeded; $i++) {
            $decoded[$dstPos] = $val
            $dstPos++
        }
    } else {
        $decoded[$dstPos] = $b
        $dstPos++
    }
}

# 256-color VGA palette is the last 769 bytes (marker 0x0C + 768 bytes RGB)
$palOffset = $bytes.Length - 769
$palette = New-Object 'System.Drawing.Color[]' 256
if ($bytes[$palOffset] -eq 0x0C) {
    for ($i = 0; $i -lt 256; $i++) {
        $r = $bytes[$palOffset + 1 + $i*3]
        $g = $bytes[$palOffset + 2 + $i*3]
        $bl = $bytes[$palOffset + 3 + $i*3]
        $palette[$i] = [System.Drawing.Color]::FromArgb($r, $g, $bl)
    }
} else {
    Write-Warning "No VGA palette marker (0x0C) found at expected offset - using grayscale fallback. Colors will be wrong."
    for ($i = 0; $i -lt 256; $i++) {
        $palette[$i] = [System.Drawing.Color]::FromArgb($i, $i, $i)
    }
}

$bmp = New-Object System.Drawing.Bitmap($width, $height)
for ($y = 0; $y -lt $height; $y++) {
    $rowStart = $y * $rowBytes
    for ($x = 0; $x -lt $width; $x++) {
        $idx = $decoded[$rowStart + $x]
        $bmp.SetPixel($x, $y, $palette[$idx])
    }
}

$bmp.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "Saved to $OutputPath"
