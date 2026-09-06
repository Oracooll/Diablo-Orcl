# BrightenPanelBg.ps1 - writes ui\panel_bg.png (the 340x720 canvas every side panel shares) from the
# dark-stone master with a gamma lift.
#
#     powershell -ExecutionPolicy Bypass -File tools\BrightenPanelBg.ps1 [-Gamma 0.85]
#
# User, 2026-09-04: "the common 340x720 canvas seem a bit dark. can you brighten it up a bit?"
#
# The master is the canvas exactly as it shipped from v1.8.x to v1.9.209 (filed that day as
# Resources\02-source-art\inventory-panel\panel-bg-340x720-dark-stone-master.png). This script never
# reads the shipped file, so it can be re-run with a different gamma without compounding lifts.
#
# A GAMMA lift rather than a multiply: out = 255 * (in/255)^gamma. Gamma below 1 raises the midtones
# - the stone's body - while the blacks in the cracks stay black and nothing clips at the top, so
# the texture keeps its contrast where a multiply would flatten it and blow the highlights. 0.85
# lifts a mid-grey of 60 to about 74, a quarter brighter; 1.0 is the master unchanged.

param([double] $Gamma = 1.0) # SINCE v1.9.212 THE LIFT IS THE "Panel Gamma" INI SETTING, applied in hud_art.cpp at load; the shipped file is the master. 0.85 shipped in v1.9.210, 0.75 in v1.9.211.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$master = Join-Path (Split-Path -Parent $root) 'Resources\02-source-art\inventory-panel\panel-bg-340x720-dark-stone-master.png'
$out = Join-Path $root 'Packaging\resources\oracool_assets\ui\panel_bg.png'
if (-not (Test-Path $master)) { throw "missing master: $master" }

$lut = New-Object byte[] 256
for ($i = 0; $i -lt 256; $i++) { $lut[$i] = [byte][math]::Round(255.0 * [math]::Pow($i / 255.0, $Gamma)) }

$src = New-Object System.Drawing.Bitmap $master
if ($src.Width -ne 340 -or $src.Height -ne 720) { throw "master is $($src.Width)x$($src.Height), expected 340x720" }
$dst = New-Object System.Drawing.Bitmap $src.Width, $src.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$sumBefore = 0.0; $sumAfter = 0.0
for ($y = 0; $y -lt $src.Height; $y++) {
  for ($x = 0; $x -lt $src.Width; $x++) {
    $c = $src.GetPixel($x, $y)
    $n = [System.Drawing.Color]::FromArgb($c.A, $lut[$c.R], $lut[$c.G], $lut[$c.B])
    $dst.SetPixel($x, $y, $n)
    $sumBefore += 0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B
    $sumAfter += 0.299 * $n.R + 0.587 * $n.G + 0.114 * $n.B
  }
}
$dst.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$px = $src.Width * $src.Height
Write-Host ("panel_bg.png  gamma {0}  mean luma {1:N1} -> {2:N1}" -f $Gamma, ($sumBefore / $px), ($sumAfter / $px))
$dst.Dispose(); $src.Dispose()
