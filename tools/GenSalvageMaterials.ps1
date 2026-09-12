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

# Where the SPECS point: the real art since batches 19 and 20 (RfA-07, 2026-09-12). Per-file, with
# a fallback to the procedural placeholder above, so a partial delivery still builds a whole sheet.
#
# Relative to the repository root, because build_item_icons.cmd runs from there and a
# machine-specific absolute path in a tracked file breaks at the next folder move.
#
# NOTE the difference from GenJewels.ps1 and GenGrowingCharms.ps1: those open by wiping $artDir
# (`Remove-Item -Recurse -Force`), so pointing THEIR art dir at Resources would delete the delivery.
# This script only creates the folder if absent (line above) and never wipes it - checked before
# writing this, rather than assumed from the other two.
$specArtDir = '..\Resources\01-in-use-assets\items\salvage'
$realCount = 0

# Real art if it is there, else that file's placeholder. Existence is checked against the resolved
# path; the SPEC keeps the relative one.
function Resolve-SpecPath([string]$slug, [string]$placeholder) {
    $rel = Join-Path $specArtDir ("$slug.png")
    if (Test-Path (Join-Path $root $rel)) {
        $script:realCount++
        return $rel
    }
    return $placeholder
}

# The seven, in the order the user listed both the salvage buttons and the colours - "whites, magic,
# rare, uniques, primal, set, ethereal" against "white, blue, yellow, gold, orange, green, dark
# gray". They line up one to one, which is why this table carries both and nothing else has to.
#
# qval is the sell value. It climbs with the tier the material comes from, so a bag of Primal Vines
# is worth carrying out and a bag of White Scales is not.
# Button is the salvage tier's own word, reused as the charm's suffix so "Charm of Salvaging:
# Uniques" and the Uniques button cannot come to disagree. CharmValue climbs steeply: a charm that
# removes a whole tier of inventory management is worth more than the gear it eats.
$materials = @(
    @{ Id = 'WHITE_SCALES';        Name = 'White Scales';        Button = 'Whites';   Colour = @(232, 232, 224); Value = 40;   CharmValue = 4000;  CharmMinLvl = 1 }
    @{ Id = 'MAGIC_POWDER';        Name = 'Magic Powder';        Button = 'Magic';    Colour = @( 72, 118, 214); Value = 120;  CharmValue = 8000;  CharmMinLvl = 4 }
    @{ Id = 'RARE_FIBRES';         Name = 'Rare Fibres';         Button = 'Rare';     Colour = @(214, 200,  60); Value = 320;  CharmValue = 16000; CharmMinLvl = 8 }
    @{ Id = 'UNIQUE_ENCRUSTMENTS'; Name = 'Unique Encrustments'; Button = 'Uniques';  Colour = @(196, 152,  54); Value = 700;  CharmValue = 26000; CharmMinLvl = 13 }
    @{ Id = 'PRIMAL_VINES';        Name = 'Primal Vines';        Button = 'Primal';   Colour = @(206, 112,  40); Value = 1500; CharmValue = 40000; CharmMinLvl = 20 }
    @{ Id = 'SET_ENGRAVINGS';      Name = 'Set Engravings';      Button = 'Set';      Colour = @( 96, 150,  72); Value = 900;  CharmValue = 30000; CharmMinLvl = 15 }
    @{ Id = 'ETHEREAL_IMBUEITIES'; Name = 'Ethereal Imbueities'; Button = 'Ethereal'; Colour = @( 74,  74,  78); Value = 1100; CharmValue = 34000; CharmMinLvl = 17 }
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

<#
.SYNOPSIS
Draws one 28x28 Charm of Salvaging: a bevelled tablet in its tier's colour.

.DESCRIPTION
Deliberately NOT an orb. A charm and the material it produces share a colour, and if they shared a
silhouette too the backpack would show fourteen circles in seven colours and every one would need
its tooltip read. A tablet against a sphere separates them at a glance, which is the whole job of an
inventory icon.
#>
function New-Charm([int[]]$rgb) {
    $bmp = New-Object System.Drawing.Bitmap -ArgumentList $cell, $cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $lo = 5
    $hi = $cell - 6
    for ($y = $lo; $y -le $hi; $y++) {
        for ($x = $lo; $x -le $hi; $x++) {
            # Clipped corners, so the tablet reads as cut stone rather than a plain box.
            if (($x - $lo) + ($y - $lo) -lt 3) { continue }
            if (($hi - $x) + ($y - $lo) -lt 3) { continue }
            if (($x - $lo) + ($hi - $y) -lt 3) { continue }
            if (($hi - $x) + ($hi - $y) -lt 3) { continue }

            # Lit top-left bevel, shadowed bottom-right - the same light direction the orbs use, so
            # the two families read as belonging together.
            $shade = 0.80
            if ($x -le $lo + 1 -or $y -le $lo + 1) { $shade = 1.15 }
            if ($x -ge $hi - 1 -or $y -ge $hi - 1) { $shade = 0.50 }

            $r = [Math]::Min(255, [int]($rgb[0] * $shade))
            $g = [Math]::Min(255, [int]($rgb[1] * $shade))
            $b = [Math]::Min(255, [int]($rgb[2] * $shade))
            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $r, $g, $b))
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

# The charms' rows are collected SEPARATELY and appended after every material's, so the CEL frame
# order is all-materials-then-all-charms - which is the order the two ICURS blocks are included in
# itemdat.h.
#
# Interleaving them was written and thrown away first: the specs would have run material, charm,
# material, charm while the enums stayed contiguous by family, so frame N would have belonged to a
# different id than the size tables claimed and every material would have drawn as a charm. Nothing
# would have failed to build. One walk still, but two baskets.
$charmEnumLines = @()
$charmDataLines = @()
$charmCursLines = @()
$charmWidthLines = @()
$charmHeightLines = @()
$charmSpecLines = @()

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
    $specLines += "$(Resolve-SpecPath ("salvage_" + $m.Id.ToLowerInvariant()) $png),0,0,$cell,$cell,$cell,$cell,salvage_$($m.Id.ToLowerInvariant()),30,false,asis"

    # The matching Charm of Salvaging. IDROP_REGULAR, unlike the materials: these are meant to drop
    # as well as be sold by Adria and Griswold.
    $cidi = "IDI_ORACOOL_CHARM_SALVAGE_$($m.Id)"
    $cicurs = "ICURS_ORACOOL_CHARM_SALVAGE_$($m.Id)"
    $cpng = Join-Path $artDir ("charm_salvage_" + $m.Id.ToLowerInvariant() + ".png")
    $charm = New-Charm $m.Colour
    try { $charm.Save($cpng, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $charm.Dispose() }

    $charmEnumLines += "`t$cidi,"
    $charmDataLines += "/*$cidi*/ { IDROP_REGULAR, ICLASS_MISC, ILOC_UNEQUIPABLE, $cicurs, ItemType::Misc, UITYPE_NONE, N_(`"Charm of Salvaging: $($m.Button)`"), N_(`"Charm`"), $($m.CharmMinLvl), 0, 0, 0, 0, 0, 0, 0, 0, ItemSpecialEffect::None, IMISC_NONE, SpellID::Null, false, $($m.CharmValue) },"
    $charmCursLines += "`t$cicurs,"
    $charmWidthLines += "`t$cell, // Charm of Salvaging: $($m.Button)"
    $charmHeightLines += "`t$cell, // Charm of Salvaging: $($m.Button)"
    $charmSpecLines += "$(Resolve-SpecPath ("charm_salvage_" + $m.Id.ToLowerInvariant()) $cpng),0,0,$cell,$cell,$cell,$cell,charm_salvage_$($m.Id.ToLowerInvariant()),30,false,asis"
}

# Materials first, then charms - see the note above the baskets.
$widthLines += $charmWidthLines
$heightLines += $charmHeightLines
$specLines += $charmSpecLines

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
Write-Inc 'salvage_charm_enum.inc' 'The seven Charms of Salvaging, appended after the materials.' $charmEnumLines
Write-Inc 'salvage_charm_data.inc' 'Their AllItemsList rows, in enum order.' $charmDataLines
Write-Inc 'salvage_charm_curs.inc' 'Their ICURS ids - appended after the material frames.' $charmCursLines
Write-Inc 'salvage_curs_widths.inc' 'Frame widths, in CEL frame order: 7 materials, then 7 charms.' $widthLines
Write-Inc 'salvage_curs_heights.inc' 'Frame heights, in CEL frame order: 7 materials, then 7 charms.' $heightLines

# NO BOM. Set-Content -Encoding UTF8 writes one in Windows PowerShell, and build_item_icons.cmd
# concatenates these files with `type` - so the BOM lands in the middle of the spec list, glued to
# the first path, and ItemIconCel throws out of Path.GetFullPath on a filename it cannot see is
# wrong. runes_icon_specs.txt has no BOM, which is why the runes worked and these did not.
[System.IO.File]::WriteAllLines((Join-Path $out 'salvage_icon_specs.txt'), $specLines, (New-Object System.Text.UTF8Encoding $false))
Write-Host ("  salvage_icon_specs.txt  ({0} specs)" -f $specLines.Count)

Write-Host ""
Write-Host ("{0} materials + {0} charms" -f $materials.Count)
Write-Host ("  real art: {0} of {1} specs, from {2}" -f $realCount, $specLines.Count, $specArtDir)
Write-Host ("  placeholders drawn into {0} (fallback only)" -f $artDir)
Write-Host "Now: tools\build_item_icons.cmd, then tools\build_oracool_mpq.cmd."
