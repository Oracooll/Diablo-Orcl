# Oracool asset pipeline: splits the delivered chest sheet into the three RGBA state masters
# ReliquaryCel.cs expects.
#
#     powershell -ExecutionPolicy Bypass -File tools\CutChestStates.ps1
#
# Source: the green-keyed sheet the user dropped in Oracool.MPQ's root on 2026-08-20, three chests
# in one row. It replaces the grand-reliquary pack as the live chest; that pack stays in the drop
# zone untouched (user: "the old one is there just in case. keep it").
#
# ## The sheet reads RIGHT to left
#
# The art is drawn open, ajar, closed from left to right. ReliquaryCel's states are closed, opening,
# open - so the panels are emitted in reverse. Getting this backwards would give a chest that starts
# full of loot and closes when you open it, which is the kind of thing that looks like an animation
# bug rather than a swapped index.
#
# ## ONE shared content box, not three
#
# The three panels measure 517, 375 and 338 wide, and the difference is almost entirely the baked
# drop shadow: the open lid throws a longer one. Boxing each state to its own bounds would scale
# them differently and shift their centres, so the chest would jump as it opened.
#
# ReliquaryCel's own header calls this out as the pack's geometry contract - "one crop, one scale,
# one bottom-centred placement" - and it applies here for the same reason. So every state is cut
# with the SAME box: the union of all three, positioned by each panel's own left edge and the shared
# bottom, which keeps the chests sitting on one ground line.
#
# ## Green key, graded
#
# Same treatment as the town portal cut: alpha by green DOMINANCE rather than a binary test, with
# the green spill pulled down to the other channels. A hard key leaves a green rim on every
# anti-aliased edge, and this art has a lot of them - rivets, hasps, and the shadow's soft border.
#
# The BAKED SHADOW is deliberately kept. ReliquaryCel's own shadow painter is switched off (user,
# 2026-08-18: "use the non shadow asset"), so nothing doubles it, and an isometric object with no
# shadow floats.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
# Found by GLOB, not by name. The drop-zone files arrive with generated names containing a Cyrillic
# abbreviation, and a literal path here did not survive this script's own encoding - it failed
# looking for a file whose name it had already corrupted. The timestamp is the stable part.
$dropZone = Join-Path (Split-Path -Parent $root) 'Oracool.MPQ'
$sheet = (Get-ChildItem $dropZone -Filter '*00_54_51*.png' | Select-Object -First 1)
if ($null -eq $sheet) { throw "no chest sheet matching *00_54_51*.png in $dropZone" }
$sheet = $sheet.FullName


$outDir = Join-Path $env:TEMP 'oracool-chest-states'
if (Test-Path $outDir) { Remove-Item $outDir -Recurse -Force }
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

function Test-Green([System.Drawing.Color]$c) {
    return ($c.G -gt 140 -and $c.R -lt 130 -and $c.B -lt 130)
}

$img = [System.Drawing.Bitmap]::FromFile($sheet)
try {
    # 1. Column runs = the panels. Detected rather than hard-coded, so a re-export that shifts them
    #    still cuts - and a re-export that changes their NUMBER fails loudly instead of silently
    #    producing two states and a sliver.
    $lit = New-Object 'bool[]' $img.Width
    for ($x = 0; $x -lt $img.Width; $x++) {
        for ($y = 0; $y -lt $img.Height; $y += 2) {
            if (-not (Test-Green $img.GetPixel($x, $y))) { $lit[$x] = $true; break }
        }
    }
    $panels = New-Object System.Collections.ArrayList
    $start = -1
    for ($x = 0; $x -lt $img.Width; $x++) {
        if ($lit[$x]) {
            if ($start -lt 0) { $start = $x }
        } elseif ($start -ge 0) {
            if (($x - $start) -gt 40) { [void]$panels.Add([pscustomobject]@{ Left = $start; Right = $x - 1 }) }
            $start = -1
        }
    }
    if ($start -ge 0 -and ($img.Width - $start) -gt 40) {
        [void]$panels.Add([pscustomobject]@{ Left = $start; Right = $img.Width - 1 })
    }
    if ($panels.Count -ne 3) { throw "expected 3 chest panels, found $($panels.Count) - re-measure" }

    # 2. Each panel's vertical extent, and the shared box from all three.
    $boxes = foreach ($p in $panels) {
        $top = $img.Height; $bottom = -1
        for ($y = 0; $y -lt $img.Height; $y++) {
            for ($x = $p.Left; $x -le $p.Right; $x += 2) {
                if (-not (Test-Green $img.GetPixel($x, $y))) {
                    if ($y -lt $top) { $top = $y }
                    if ($y -gt $bottom) { $bottom = $y }
                    break
                }
            }
        }
        [pscustomobject]@{ Left = $p.Left; Right = $p.Right; Top = $top; Bottom = $bottom }
    }
    $boxW = ($boxes | ForEach-Object { $_.Right - $_.Left + 1 } | Measure-Object -Maximum).Maximum
    $boxH = ($boxes | ForEach-Object { $_.Bottom - $_.Top + 1 } | Measure-Object -Maximum).Maximum
    $sharedBottom = ($boxes | ForEach-Object { $_.Bottom } | Measure-Object -Maximum).Maximum

    # 3. Emit closed, opening, open - the sheet's order reversed. See the header.
    $stateNames = @('closed', 'opening', 'open')
    for ($i = 0; $i -lt 3; $i++) {
        $src = $boxes[2 - $i]
        $out = New-Object System.Drawing.Bitmap -ArgumentList $boxW, $boxH, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        # Bottom-aligned on the shared ground line, left-aligned on the panel's own left edge: the
        # shadow grows to the LEFT as the lid opens, so a centred placement would walk the chest.
        $offsetY = $boxH - ($sharedBottom - $src.Top + 1)
        for ($y = $src.Top; $y -le $sharedBottom; $y++) {
            for ($x = $src.Left; $x -le $src.Right; $x++) {
                $c = $img.GetPixel($x, $y)
                $lead = $c.G - [Math]::Max($c.R, $c.B)
                $dstX = $x - $src.Left
                $dstY = $y - $src.Top + $offsetY
                if ($dstX -ge $boxW -or $dstY -ge $boxH -or $dstY -lt 0) { continue }
                if ($lead -le 0) {
                    $out.SetPixel($dstX, $dstY, [System.Drawing.Color]::FromArgb(255, $c.R, $c.G, $c.B))
                } elseif ($lead -lt 60) {
                    $a = [int](255 * (60 - $lead) / 60)
                    $g2 = [Math]::Max($c.R, $c.B)
                    $out.SetPixel($dstX, $dstY, [System.Drawing.Color]::FromArgb($a, $c.R, $g2, $c.B))
                }
            }
        }
        $path = Join-Path $outDir ("chest_" + $stateNames[$i] + ".png")
        $out.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        $out.Dispose()
        Write-Host ("{0,-8} <- sheet panel {1}  {2}x{3}" -f $stateNames[$i], (3 - $i), $boxW, $boxH)
    }
    Write-Host "states written to $outDir"
} finally {
    $img.Dispose()
}
