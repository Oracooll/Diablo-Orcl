# ScalePainting.ps1 - a user painting with real alpha (Resources\<name>.png) scaled to a town object's frame
# width, content box cropped first, into Resources\01-in-use-assets\objects\<folder>\<out>.png for a
# FramesCel.cs build. The general form of ScaleLevskiCube.ps1 (2026-09-20); first used for the Rift
# Monument (Resources\Rift Monument.png, 1161x1355 -> 128 wide, tools\build_stonegate_cel.cmd).
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [Parameter(Mandatory = $true)][string]$OutName,
    [int]$Width = 128,
    # Toning (user, 2026-09-20: "make the monument asset less bright and more worn-down stone grey-ish"):
    # saturation kept (1 = as painted), brightness kept, and a cool cast on the greys. Applied AFTER the
    # resample, to the frame alone.
    [double]$Saturation = 1.0,
    [double]$Brightness = 1.0,
    [double]$CoolCast = 0.0,
    # A colour cast toward $TintRgb ("R,G,B") by $TintStrength (0..1), luminance kept - the Rift Monument's
    # blue-grey to match Tristram's rocks (user, 2026-09-20: "recolour the rift monument to match the rocks
    # scattered all over Tristram. they are very blue-ish").
    [string]$TintRgb = "",
    [double]$TintStrength = 0.0
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
$tint = $null
if ($TintRgb -ne "" -and $TintStrength -gt 0) { $p = $TintRgb.Split(","); $tint = @([double]$p[0], [double]$p[1], [double]$p[2]); $tl = 0.299 * $tint[0] + 0.587 * $tint[1] + 0.114 * $tint[2]; if ($tl -le 0) { $tl = 1 } }
if ($Saturation -ne 1.0 -or $Brightness -ne 1.0 -or $CoolCast -ne 0.0 -or $tint -ne $null) {
    for ($y = 0; $y -lt $h; $y++) { for ($x = 0; $x -lt $Width; $x++) {
        $c = $out.GetPixel($x, $y); if ($c.A -eq 0) { continue }
        $lum = 0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B
        $r = ($lum + ($c.R - $lum) * $Saturation) * $Brightness * (1.0 - $CoolCast)
        $g = ($lum + ($c.G - $lum) * $Saturation) * $Brightness
        $b = ($lum + ($c.B - $lum) * $Saturation) * $Brightness * (1.0 + $CoolCast)
        if ($tint -ne $null) { $l2 = 0.299 * $r + 0.587 * $g + 0.114 * $b; $r = $r * (1 - $TintStrength) + $l2 * $tint[0] / $tl * $TintStrength; $g = $g * (1 - $TintStrength) + $l2 * $tint[1] / $tl * $TintStrength; $b = $b * (1 - $TintStrength) + $l2 * $tint[2] / $tl * $TintStrength }
        $out.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($c.A, [int][Math]::Max(0, [Math]::Min(255, $r)), [int][Math]::Max(0, [Math]::Min(255, $g)), [int][Math]::Max(0, [Math]::Min(255, $b))))
    } }
    Write-Host ("toned: saturation {0}, brightness {1}, cool cast {2}" -f $Saturation, $Brightness, $CoolCast)
}
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
