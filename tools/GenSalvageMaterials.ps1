# GenSalvageMaterials.ps1 - the seven salvage materials, in one walk.
#
#     powershell -ExecutionPolicy Bypass -File tools\GenSalvageMaterials.ps1
#
# Emits, in the SAME order every time, so the ids, the CEL frames and the table rows cannot drift
# apart - the one-walk-one-order discipline GenRunes.ps1 and GenUniqueItems.ps1 already follow:
#
#   Source/oracool/salvage_enum.inc          the IDI_ORACOOL_SALVAGE_* ids
#   Source/oracool/salvage_data.inc          their AllItemsList rows
#   Source/oracool/salvage_curs.inc          their ICURS_ORACOOL_SALVAGE_* ids
#   Source/oracool/salvage_curs_widths.inc   CEL frame widths, in frame order
#   Source/oracool/salvage_curs_heights.inc  the heights
#   Source/oracool/salvage_icon_specs.txt    the ItemIconCel specs, appended after the runes
#   %TEMP%/oracool-salvage-orbs/*.png        the placeholder art itself
#
# ## The art is PLACEHOLDER, and drawn here rather than sourced
#
# User request, 2026-08-20: "add placeholder item sprites for the materials as well - some orb
# looking sprites with white, blue, yellow, gold, orange, green, dark gray colors."
#
# Drawing them in the generator keeps the placeholder honest: there is no hand-made file to mistake
# for finished art, and when the real sprites arrive this script's PNG half is deleted and the spec
# lines repoint at them. Nothing else about the pipeline changes.
#
# They go through ItemIconCel's "asis" mode - the PNG already IS the 28x28 frame, with real alpha -
# so nothing crops, rescales or re-centres art that is already exactly cell-sized.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'Source\oracool'
$artDir = Join-Path $env:TEMP 'oracool-salvage-orbs'
if (-not (Test-Path $artDir)) { New-Item -ItemType Directory -Path $artDir -Force | Out-Null }

# The seven, in the order the user listed both the salvage buttons and the colours - "whites, magic,
# rare, uniques, primal, set, ethereal" against "white, blue, yellow, gold, orange, green, dark
# gray". They line up one to one, which is why this table carries both and nothing else has to.
#
# qval is the sell value. It climbs with the tier the material comes from, so a bag of Primal Vines
# is worth carrying out and a bag of White Scales is not.
$materials = @(
    @{ Id = 'WHITE_SCALES';        Name = 'White Scales';        Colour = @(232, 232, 224); Value = 40 }
    @{ Id = 'MAGIC_POWDER';        Name = 'Magic Powder';        Colour = @( 72, 118, 214); Value = 120 }
    @{ Id = 'RARE_FIBRES';         Name = 'Rare Fibres';         Colour = @(214, 200,  60); Value = 320 }
    @{ Id = 'UNIQUE_ENCRUSTMENTS'; Name = 'Unique Encrustments'; Colour = @(196, 152,  54); Value = 700 }
    @{ Id = 'PRIMAL_VINES';        Name = 'Primal Vines';        Colour = @(206, 112,  40); Value = 1500 }
    @{ Id = 'SET_ENGRAVINGS';      Name = 'Set Engravings';      Colour = @( 96, 150,  72); Value = 900 }
    @{ Id = 'ETHEREAL_IMBUEITIES'; Name = 'Ethereal Imbueities'; Colour = @( 74,  74,  78); Value = 1100 }
)

$cell = 28

<#
.SYNOPSIS
Draws one 28x28 orb: a lit sphere with a soft rim, on real transparency.

.DESCRIPTION
Per-pixel rather than a GDI+ gradient brush, because the frame is 28 pixels across - at that size a
brush's dithering reads as noise, and the edge has to be controlled exactly or quantisation turns it
into a ragged halo. The alpha ramps over the outermost pixel only, which is what ItemIconCel's
PostProcess expects to find.
#>
function New-Orb([int[]]$rgb) {
    $bmp = New-Object System.Drawing.Bitmap -ArgumentList $cell, $cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $c = ($cell - 1) / 2.0
    $radius = 12.0
    # The highlight sits up and left of centre, the convention every round item icon in this game
    # already uses - so these read as lit from the same place as the potions beside them.
    $lightX = $c - 3.5
    $lightY = $c - 4.0

    for ($y = 0; $y -lt $cell; $y++) {
        for ($x = 0; $x -lt $cell; $x++) {
            $dx = $x - $c
            $dy = $y - $c
            $d = [Math]::Sqrt($dx * $dx + $dy * $dy)
            if ($d -gt $radius) { continue }

            # Lambert-ish shading from the highlight, floored so the dark side still shows its hue
            # rather than going to black - which matters most for Ethereal Imbueities, whose base
            # colour is already nearly black.
            $lx = $x - $lightX
            $ly = $y - $lightY
            $ld = [Math]::Sqrt($lx * $lx + $ly * $ly)
            $lit = 1.0 - [Math]::Min(1.0, $ld / ($radius * 1.6))
            $shade = 0.42 + 0.78 * $lit

            $r = [Math]::Min(255, [int]($rgb[0] * $shade))
            $g = [Math]::Min(255, [int]($rgb[1] * $shade))
            $b = [Math]::Min(255, [int]($rgb[2] * $shade))

            # A darker rim, so the orb has an edge against a light inventory backing as well as a
            # dark one.
            if ($d -gt $radius - 1.6) {
                $r = [int]($r * 0.55); $g = [int]($g * 0.55); $b = [int]($b * 0.55)
            }

            # Alpha only on the last pixel of the radius. Anything wider and quantisation smears it.
            $a = 255
            if ($d -gt $radius - 1.0) { $a = [int](255 * ($radius - $d)) }
            if ($a -lt 0) { $a = 0 }
            if ($a -gt 255) { $a = 255 }

            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($a, $r, $g, $b))
        }
    }
    return $bmp
}

$enumLines = @()
$dataLines = @()
$cursLines = @()
$widthLines = @()
$heightLines = @()
$specLines = @()

foreach ($m in $materials) {
    $idi = "IDI_ORACOOL_SALVAGE_$($m.Id)"
    $icurs = "ICURS_ORACOOL_SALVAGE_$($m.Id)"
    $png = Join-Path $artDir ("salvage_" + $m.Id.ToLowerInvariant() + ".png")

    $orb = New-Orb $m.Colour
    try { $orb.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $orb.Dispose() }

    $enumLines += "`t$idi,"
    # ICLASS_MISC / ILOC_UNEQUIPABLE / IMISC_NONE - identical to a rune's row, which is what makes
    # them stack and sort through the same code rather than needing a family of their own.
    $dataLines += "/*$idi*/ { IDROP_NEVER, ICLASS_MISC, ILOC_UNEQUIPABLE, $icurs, ItemType::Misc, UITYPE_NONE, N_(`"$($m.Name)`"), N_(`"Salvage`"), 1, 0, 0, 0, 0, 0, 0, 0, 0, ItemSpecialEffect::None, IMISC_NONE, SpellID::Null, false, $($m.Value) },"
    $cursLines += "`t$icurs,"
    $widthLines += "`t$cell, // $($m.Name)"
    $heightLines += "`t$cell, // $($m.Name)"
    $specLines += "$png,0,0,$cell,$cell,$cell,$cell,salvage_$($m.Id.ToLowerInvariant()),30,false,asis"
}

function Write-Inc([string]$file, [string]$what, [string[]]$lines) {
    $header = @(
        "// GENERATED by tools/GenSalvageMaterials.ps1 - do not edit by hand.",
        "// $what",
        ""
    )
    Set-Content -Path (Join-Path $out $file) -Value ($header + $lines) -Encoding UTF8
    Write-Host ("  {0}  ({1} lines)" -f $file, $lines.Count)
}

Write-Inc 'salvage_enum.inc' 'The seven salvage materials, appended after the runes.' $enumLines
Write-Inc 'salvage_data.inc' 'Their AllItemsList rows, in enum order.' $dataLines
Write-Inc 'salvage_curs.inc' 'Their ICURS ids - appended after the runes, so the CEL frames follow.' $cursLines
Write-Inc 'salvage_curs_widths.inc' 'Frame widths, in CEL frame order.' $widthLines
Write-Inc 'salvage_curs_heights.inc' 'Frame heights, in CEL frame order.' $heightLines

# NO BOM. Set-Content -Encoding UTF8 writes one in Windows PowerShell, and build_item_icons.cmd
# concatenates these files with `type` - so the BOM lands in the middle of the spec list, glued to
# the first path, and ItemIconCel throws out of Path.GetFullPath on a filename it cannot see is
# wrong. runes_icon_specs.txt has no BOM, which is why the runes worked and these did not.
[System.IO.File]::WriteAllLines((Join-Path $out 'salvage_icon_specs.txt'), $specLines, (New-Object System.Text.UTF8Encoding $false))
Write-Host ("  salvage_icon_specs.txt  ({0} specs)" -f $specLines.Count)

Write-Host ""
Write-Host ("{0} materials, art in {1}" -f $materials.Count, $artDir)
Write-Host "Now: tools\build_item_icons.cmd, then tools\build_oracool_mpq.cmd."
