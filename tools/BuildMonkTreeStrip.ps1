# Oracool asset pipeline: builds ui\monk_tree_icons.png from the Monk package's 21 individual
# icon files.
#
# Unlike the other five class trees this one needed no measuring: the package ships one 256x256
# PNG per skill rather than a composite sheet, so the only work is keying the chroma green and
# packing them into a strip in enum order (Staff 1-7, Body 1-7, Spirit 1-7 - the order the design
# doc lists the branches in).
#
# Green is unmixed rather than cut, exactly as in CutClassTree.ps1: these emblems are white with a
# cast shadow, and a binary key leaves a one-pixel green halo that reads as a glow on the window's
# dark fill. Partially-green pixels get proportional alpha AND have their green channel pulled back
# to max(R,B).
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\BuildMonkTreeStrip.ps1 -Source <extracted package dir>
# Run from the repository root.
param(
  [Parameter(Mandatory = $true)][string]$Source,
  [int]$Icon = 56
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$order = @(
  "staff\01-sweeping-reed.png", "staff\02-breaking-current.png", "staff\03-reed-in-the-wind.png",
  "staff\04-vaulting-strike.png", "staff\05-wheel-of-heaven.png", "staff\06-seven-reeds.png",
  "staff\07-master-of-the-long-staff.png",
  "body\01-open-palm.png", "body\02-flowing-step.png", "body\03-iron-robe.png",
  "body\04-counterstroke.png", "body\05-purifying-breath.png", "body\06-hundred-fists.png",
  "body\07-perfect-vessel.png",
  "spirit\01-inner-sight.png", "spirit\02-healing-mantra.png", "spirit\03-temple-bell.png",
  "spirit\04-spirit-ward.png", "spirit\05-radiant-palm.png", "spirit\06-tranquility.png",
  "spirit\07-enlightenment.png"
)

$KEY_FULL = 60

$strip = New-Object System.Drawing.Bitmap -ArgumentList ($Icon * $order.Count), $Icon
$g = [System.Drawing.Graphics]::FromImage($strip)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

for ($i = 0; $i -lt $order.Count; $i++) {
  $path = Join-Path (Join-Path $Source "icons") $order[$i]
  if (-not (Test-Path $path)) { throw "missing icon: $path" }
  $src = New-Object System.Drawing.Bitmap($path)
  $w = $src.Width; $h = $src.Height

  $cell = New-Object System.Drawing.Bitmap -ArgumentList $w, $h
  $minX = $w; $maxX = -1; $minY = $h; $maxY = -1
  for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
      $c = $src.GetPixel($x, $y)
      $r = [int]$c.R; $gg = [int]$c.G; $b = [int]$c.B
      $mx = $r; if ($b -gt $mx) { $mx = $b }
      $ex = $gg - $mx
      if ($ex -ge $KEY_FULL) { $cell.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0)); continue }
      $a = 255; $outG = $gg
      if ($ex -gt 0) {
        $a = [int](255 * (1.0 - ($ex / $KEY_FULL)))
        $outG = $mx
      }
      if ($a -le 8) { $cell.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0)); continue }
      $cell.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($a, $r, $outG, $b))
      if ($x -lt $minX) { $minX = $x }
      if ($x -gt $maxX) { $maxX = $x }
      if ($y -lt $minY) { $minY = $y }
      if ($y -gt $maxY) { $maxY = $y }
    }
  }
  if ($maxX -lt 0) { throw "icon $($order[$i]) is entirely background" }

  $cw = $maxX - $minX + 1; $ch = $maxY - $minY + 1
  $scale = [Math]::Min(($Icon / $cw), ($Icon / $ch))
  $dw = [int][Math]::Round($cw * $scale); $dh = [int][Math]::Round($ch * $scale)
  $dx = ($i * $Icon) + [int](($Icon - $dw) / 2)
  $dy = [int](($Icon - $dh) / 2)
  $rect = New-Object System.Drawing.Rectangle $dx, $dy, $dw, $dh
  $g.DrawImage($cell, $rect, $minX, $minY, $cw, $ch, [System.Drawing.GraphicsUnit]::Pixel)
  $cell.Dispose(); $src.Dispose()
  Write-Host ("  {0,2}: {1}" -f $i, $order[$i])
}

$g.Dispose()
$outPath = "Packaging\resources\oracool_assets\ui\monk_tree_icons.png"
New-Item -ItemType Directory -Force -Path (Split-Path $outPath) | Out-Null
$strip.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
$strip.Dispose()
Write-Host ("wrote {0} ({1} icons at {2}px)" -f $outPath, $order.Count, $Icon)
