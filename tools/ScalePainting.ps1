# ScalePainting.ps1 - a user painting with real alpha (Resources\<name>.png) scaled to a town object's frame
# width, content box cropped first, into Resources\01-in-use-assets\objects\<folder>\<out>.png for a
# FramesCel.cs build. The general form of ScaleLevskiCube.ps1 (2026-09-20); first used for the Rift
# Monument (Resources\Rift Monument.png, 1161x1355 -> 128 wide, tools\build_stonegate_cel.cmd).
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [Parameter(Mandatory = $true)][string]$OutName,
    [int]$Width = 128
)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Source))
$minx = $src.Width; $maxx = -1; $miny = $src.Height; $maxy = -1
for ($y = 0; $y -lt $src.Height; $y++) { for ($x = 0; $x -lt $src.Width; $x++) { if ($src.GetPixel($x, $y).A -gt 0) { if ($x -lt $minx) { $minx = $x }; if ($x -gt $maxx) { $maxx = $x }; if ($y -lt $miny) { $miny = $y }; if ($y -gt $maxy) { $maxy = $y } } } }
$cw = $maxx - $minx + 1; $ch = $maxy - $miny + 1
$h = [int][Math]::Round($ch * $Width / $cw)
$out = New-Object System.Drawing.Bitmap $Width, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($out)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = 'HighQualityBicubic'; $g.SmoothingMode = 'HighQuality'; $g.PixelOffsetMode = 'HighQuality'; $g.CompositingMode = 'SourceCopy'
$g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $Width, $h), (New-Object System.Drawing.Rectangle $minx, $miny, $cw, $ch), [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose()
$out.Save((Join-Path (Resolve-Path $OutDir) $OutName), [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host ("{0} {1}x{2} from a {3}x{4} content box at ({5},{6})" -f $OutName, $Width, $h, $cw, $ch, $minx, $miny)
# The opening: rows where opaque stone stands on both sides of a transparent run.
$omin = $h; $omax = -1; $oxmin = $Width; $oxmax = -1
for ($y = 0; $y -lt $h; $y++) {
    $l = -1; $r = -1
    for ($x = 0; $x -lt $Width; $x++) { if ($out.GetPixel($x, $y).A -gt 128) { if ($l -lt 0) { $l = $x }; $r = $x } }
    if ($l -lt 0) { continue }
    $gapStart = -1; $gapEnd = -1
    for ($x = $l; $x -le $r; $x++) { if ($out.GetPixel($x, $y).A -le 128) { if ($gapStart -lt 0) { $gapStart = $x }; $gapEnd = $x } }
    if ($gapStart -ge 0 -and ($gapEnd - $gapStart) -ge 8) { if ($y -lt $omin) { $omin = $y }; if ($y -gt $omax) { $omax = $y }; if ($gapStart -lt $oxmin) { $oxmin = $gapStart }; if ($gapEnd -gt $oxmax) { $oxmax = $gapEnd } }
}
Write-Host ("opening (transparent run between stone): x {0}..{1} y {2}..{3}" -f $oxmin, $oxmax, $omin, $omax)
$out.Dispose(); $src.Dispose()
