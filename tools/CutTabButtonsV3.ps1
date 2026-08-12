# Oracool asset pipeline: cuts ui\inventory_tabs_v3.png out of the user's green-screen
# "TAB BUTTONS v3 (1-9, X) - SQUARE - NO BORDERS" sheet.
#
# Output format is dictated by oracool::DrawInventoryTab (Source/oracool/hud_art.cpp), which
# blits a uniform grid where COLUMN = tab index 0..9 and ROW = state, with TabCellSize (28x28,
# from inventory_layout.h). So the sheet is 280x84 - the same shape as the existing
# inventory_tabs.png, deliberately, so this is a drop-in candidate rather than a new format.
#
# Row order is NOT the source's column order. inv.cpp calls DrawInventoryTab(..., selected ? 1 : 0),
# so row 0 must be INACTIVE and row 1 ACTIVE, while the source sheet is laid out
# ACTIVE | INACTIVE | CLICKED left to right. Copying the columns straight across would invert
# every tab highlight in the game - hence the explicit mapping below.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutTabButtonsV3.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$src = "..\Oracool.MPQ\02-source-art\inventory\tab-buttons-v3-arabic-1-9-x-square-noborder-narrow.png"
$outName = "inventory_tabs_v3.png"
$cell = 28          # TabCellSize in inventory_layout.h
$tabs = 10
$states = 3
$GreenCut = 25      # matches ItemIconCel's green test

if (-not (Test-Path $src)) { throw "source sheet not found: $src" }
$bmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path $src))
$w = $bmp.Width; $h = $bmp.Height

# --- read pixels once ---
$data = $bmp.LockBits((New-Object System.Drawing.Rectangle(0,0,$w,$h)), [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $data.Stride
$buf = New-Object byte[] ($stride * $h)
[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $buf, 0, $buf.Length)
$bmp.UnlockBits($data)

function IsContent([int]$x, [int]$y) {
    $i = $y * $stride + $x * 4
    return (($buf[$i+1] - [Math]::Max($buf[$i+2], $buf[$i])) -lt $GreenCut)
}

# --- locate the 30 buttons by connected components, rather than hardcoding a grid ---
$seen = New-Object 'bool[,]' $w, $h
$comps = @()
for ($sy=0; $sy -lt $h; $sy++) {
  for ($sx=0; $sx -lt $w; $sx++) {
    if ($seen[$sx,$sy]) { continue }
    if (-not (IsContent $sx $sy)) { continue }
    $q = New-Object System.Collections.Generic.Queue[int]
    $q.Enqueue($sy*$w+$sx); $seen[$sx,$sy]=$true
    $n=0; $mnX=$sx;$mxX=$sx;$mnY=$sy;$mxY=$sy
    while ($q.Count -gt 0) {
      $cur=$q.Dequeue(); $x=$cur % $w; $y=[int][Math]::Floor($cur / $w); $n++
      if($x -lt $mnX){$mnX=$x}; if($x -gt $mxX){$mxX=$x}
      if($y -lt $mnY){$mnY=$y}; if($y -gt $mxY){$mxY=$y}
      foreach($d in @(@(1,0),@(-1,0),@(0,1),@(0,-1))){
        $nx=$x+$d[0]; $ny=$y+$d[1]
        if($nx -lt 0 -or $nx -ge $w -or $ny -lt 0 -or $ny -ge $h){continue}
        if($seen[$nx,$ny]){continue}
        if(-not (IsContent $nx $ny)){continue}
        $seen[$nx,$ny]=$true; $q.Enqueue($ny*$w+$nx)
      }
    }
    # The sheet's title, column headers and row labels are text and fall far below this.
    if ($n -ge 2000) { $comps += [PSCustomObject]@{ X=$mnX; Y=$mnY; W=($mxX-$mnX+1); H=($mxY-$mnY+1) } }
  }
}
if ($comps.Count -ne ($tabs * $states)) { throw "expected $($tabs*$states) buttons, found $($comps.Count)" }

# Three columns; group by x, then order each column top-to-bottom = tab 1..9,X.
$colXs = ($comps | Select-Object -ExpandProperty X | Sort-Object -Unique)
if ($colXs.Count -ne 3) { throw "expected 3 button columns, found $($colXs.Count): $($colXs -join ',')" }
# Source column order is ACTIVE, INACTIVE, CLICKED (left to right, per the sheet's own headers).
$srcActive, $srcInactive, $srcClicked = $colXs
# Destination row order is what DrawInventoryTab expects: 0 inactive, 1 active, 2 clicked.
$rowForColumn = @{ $srcInactive = 0; $srcActive = 1; $srcClicked = 2 }
Write-Host "source columns: active=$srcActive inactive=$srcInactive clicked=$srcClicked"
Write-Host "-> sheet rows:  0=inactive 1=active 2=clicked"

$out = New-Object System.Drawing.Bitmap ($tabs*$cell), ($states*$cell), ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($out)
$g.Clear([System.Drawing.Color]::FromArgb(0,0,0,0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

foreach ($colX in $colXs) {
    $row = $rowForColumn[$colX]
    $col = @($comps | Where-Object { $_.X -eq $colX } | Sort-Object Y)
    if ($col.Count -ne $tabs) { throw "column $colX has $($col.Count) buttons, expected $tabs" }
    for ($t = 0; $t -lt $tabs; $t++) {
        $b = $col[$t]
        # Green -> transparent on a per-button crop, at source resolution.
        $crop = New-Object System.Drawing.Bitmap $b.W, $b.H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        for ($yy=0; $yy -lt $b.H; $yy++) {
            for ($xx=0; $xx -lt $b.W; $xx++) {
                $i = ($b.Y+$yy)*$stride + ($b.X+$xx)*4
                $bl=$buf[$i]; $gr=$buf[$i+1]; $rd=$buf[$i+2]
                if (($gr - [Math]::Max($rd,$bl)) -ge $GreenCut) {
                    $crop.SetPixel($xx,$yy,[System.Drawing.Color]::FromArgb(0,0,0,0))
                } else {
                    $crop.SetPixel($xx,$yy,[System.Drawing.Color]::FromArgb(255,$rd,$gr,$bl))
                }
            }
        }
        # Contain-fit into the 28x28 cell: no distortion, no cropping of the glyph. The leftover
        # is transparent padding, which is exactly what DrawInventoryTab's comment expects.
        $scale = [Math]::Min($cell / [double]$b.W, $cell / [double]$b.H)
        $dw = [int][Math]::Round($b.W * $scale); $dh = [int][Math]::Round($b.H * $scale)
        $dx = $t*$cell + [int][Math]::Floor(($cell-$dw)/2)
        $dy = $row*$cell + [int][Math]::Floor(($cell-$dh)/2)
        $g.DrawImage($crop, $dx, $dy, $dw, $dh)
        $crop.Dispose()
    }
}
$g.Dispose(); $bmp.Dispose()

foreach ($dir in @("Packaging\resources\oracool_assets\ui", "Packaging\resources\assets\ui", "build\x64-Debug\assets\ui")) {
    if (Test-Path (Split-Path $dir -Parent)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        $out.Save((Join-Path (Resolve-Path $dir) $outName), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "wrote $dir\$outName"
    }
}
$out.Dispose()
Write-Host "done - $($tabs*$cell)x$($states*$cell), ${cell}px cells"
