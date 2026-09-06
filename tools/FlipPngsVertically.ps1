<#
.SYNOPSIS
    Flips every PNG in a folder top-to-bottom, in place, keeping each file's own pixel format.
.DESCRIPTION
    For extracted art that came out upside down (user, 2026-09-07: the durability icons of
    00-original-game-art\duricons). Palettized PNGs stay palettized - GDI+ flips an 8-bit indexed
    bitmap without converting it - so the file stays bit-exact apart from the row order.
.PARAMETER Path
    The folder whose *.png files are flipped. Not recursive unless -Recurse is given.
.EXAMPLE
    .\tools\FlipPngsVertically.ps1 -Path "..\Oracool.MPQ\00-original-game-art\duricons"
#>
param(
    [Parameter(Mandatory = $true)] [string]$Path,
    [switch]$Recurse
)
Add-Type -AssemblyName System.Drawing
$files = Get-ChildItem -Path $Path -Filter *.png -File -Recurse:$Recurse
$n = 0
foreach ($f in $files) {
    $bytes = [System.IO.File]::ReadAllBytes($f.FullName)
    $ms = New-Object System.IO.MemoryStream(, $bytes)
    $bmp = [System.Drawing.Bitmap]::FromStream($ms)
    $bmp.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY)
    $bmp.Save($f.FullName, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $ms.Dispose()
    $n++
}
Write-Host "flipped $n png(s) in $Path"
