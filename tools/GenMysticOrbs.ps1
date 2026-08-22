# Generates the Mystic Orbs - Phase 1 of the D2MXL-to-ORCL plan.
#
# WHAT AN ORB IS
#
# A consumable that adds ONE fixed small stat to an item, permanently, with a hard cap on how many
# any single item can take. Three properties make the mechanism work and all three are decisions:
#
#   - the cap is per ITEM rather than per orb type, so six into one weapon finishes it and the
#     seventh has to go somewhere else. That is what makes an orb a decision rather than an
#     accumulator;
#   - the value is FIXED, never rolled, so the player can plan. A rolled orb is just another affix;
#   - it is permanent. Levski's Roar already sells gambling - the reroll ladder - and orbs are
#     deliberately the opposite of it.
#
# WHY THE VALUES ARE SMALL
#
# An orb is worth roughly a third of an affix, and that is the point. Six of them should be a real
# upgrade to a good base and never a substitute for finding a better item, because the moment
# orbing beats finding, the drop tables stop mattering.
#
# ONE WALK, ONE ORDER - the discipline every generator here follows. The ids, the AllItemsList rows,
# the ICURS ids, the CEL frame sizes, the icon cut specs and the power rows all come out of the
# single loop below. A CEL stores no names and no sizes, so a frame's POSITION in the file is the
# only thing tying it to an id: any of those six drifting means every icon after the first mismatch
# is silently wrong.
#
#     pwsh -File tools\GenMysticOrbs.ps1
param(
    [string]$OutDir = "Source\oracool",
    [string]$ArtDir = (Join-Path ([System.IO.Path]::GetTempPath()) "oracool-orbs"),
    # One past the last jewel. cursor.cpp static_asserts the adjacency.
    [int]$FirstCursorId = 477
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$repo = Split-Path -Parent $PSScriptRoot
$out = if ([System.IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $repo $OutDir }
$artDir = $ArtDir
if (Test-Path $artDir) { Remove-Item -Recurse -Force $artDir }
New-Item -ItemType Directory -Force $artDir | Out-Null

$cell = 28

# --- the eight orbs ---------------------------------------------------------------------------
#
# One per stat channel that already exists AND reads clearly on an item description. Power is an
# ItemPower row - the same shape the affix tables use - so ApplyItemPower drops it into exactly the
# fields every other item's stats land in, with the same signs and the same flag semantics.
#
# param1 and param2 are equal on every row, deliberately: SaveItemPower rolls a range between them,
# and an orb that rolled would be an affix wearing a different name.
#
# Qlvl is the depth an orb starts dropping at, and it is NOT flat. The four attribute orbs are the
# ones a new character can use immediately; find and resistance are mid-game concerns.
$orbs = @(
    @{ Id = 'MIGHT';    Name = 'Orb of Might';    Power = 'IPL_STR';      Value =  3; Qlvl =  4; Colour = @(190,  72,  56); Value2 =  900
       Line = '+3 to Strength' }
    @{ Id = 'GRACE';    Name = 'Orb of Grace';    Power = 'IPL_DEX';      Value =  3; Qlvl =  4; Colour = @( 84, 168,  92); Value2 =  900
       Line = '+3 to Dexterity' }
    @{ Id = 'INSIGHT';  Name = 'Orb of Insight';  Power = 'IPL_MAG';      Value =  3; Qlvl =  4; Colour = @( 70, 116, 206); Value2 =  900
       Line = '+3 to Magic' }
    @{ Id = 'VIGOUR';   Name = 'Orb of Vigour';   Power = 'IPL_VIT';      Value =  3; Qlvl =  4; Colour = @(206, 120,  48); Value2 =  900
       Line = '+3 to Vitality' }
    @{ Id = 'WARDING';  Name = 'Orb of Warding';  Power = 'IPL_ALLRES';   Value =  5; Qlvl = 12; Colour = @(146, 100, 190); Value2 = 1600
       Line = '+5 to all resistances' }
    @{ Id = 'FURY';     Name = 'Orb of Fury';     Power = 'IPL_DAMMOD';   Value =  2; Qlvl = 10; Colour = @(176,  46,  46); Value2 = 1600
       Line = '+2 to damage' }
    @{ Id = 'FORTUNE';  Name = 'Orb of Fortune';  Power = 'IPL_MAGICFIND'; Value = 5; Qlvl = 16; Colour = @(206, 190,  92); Value2 = 2200
       Line = '+5% magic find' }
    @{ Id = 'AVARICE';  Name = 'Orb of Avarice';  Power = 'IPL_GOLDFIND'; Value =  8; Qlvl = 16; Colour = @(214, 174,  54); Value2 = 2200
       Line = '+8% gold find' }
)

<#
.SYNOPSIS
Draws an orb: a sphere with a highlight, ringed.

Deliberately NOT the jewel's lozenge and NOT the salvage material's shape. Three socketable-ish
families now share a 28x28 cell, and a player scanning the ground tells them apart by SILHOUETTE
before colour - the salvage set already proved how close two colours look at this size.
#>
function New-Orb([int[]]$rgb) {
    $bmp = New-Object System.Drawing.Bitmap $cell, $cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $clamp = { param($v) [Math]::Max(0, [Math]::Min(255, [int]$v)) }
    $mid  = [System.Drawing.Color]::FromArgb(255, $rgb[0], $rgb[1], $rgb[2])
    $lit  = [System.Drawing.Color]::FromArgb(255, (& $clamp ($rgb[0] + 78)), (& $clamp ($rgb[1] + 78)), (& $clamp ($rgb[2] + 78)))
    $dark = [System.Drawing.Color]::FromArgb(255, (& $clamp ($rgb[0] - 62)), (& $clamp ($rgb[1] - 62)), (& $clamp ($rgb[2] - 62)))

    $inset = 3.0
    $d = $cell - $inset * 2
    $sphere = New-Object System.Drawing.Drawing2D.GraphicsPath
    $sphere.AddEllipse($inset, $inset, $d, $d)

    # A radial gradient centred up and left - one light source, the same one the jewels and the
    # salvage tablets use, so the whole icon sheet reads as lit from one place.
    $brush = New-Object System.Drawing.Drawing2D.PathGradientBrush $sphere
    $brush.CenterPoint = New-Object System.Drawing.PointF (($inset + $d * 0.35), ($inset + $d * 0.32))
    $brush.CenterColor = $lit
    $brush.SurroundColors = @($dark)
    $g.FillPath($brush, $sphere)

    # A ring, so the orb keeps an edge against a dark floor.
    $ring = New-Object System.Drawing.Pen $mid, 1.6
    $g.DrawEllipse($ring, $inset, $inset, $d, $d)
    $edge = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 10, 9, 12)), 1
    $g.DrawEllipse($edge, ($inset - 0.5), ($inset - 0.5), ($d + 1), ($d + 1))

    # The specular dot. Small and off-centre; it is what makes a flat disc read as a sphere.
    $spec = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(210, 255, 255, 255))
    $g.FillEllipse($spec, ($inset + $d * 0.26), ($inset + $d * 0.20), ($d * 0.20), ($d * 0.16))

    $spec.Dispose(); $edge.Dispose(); $ring.Dispose(); $brush.Dispose(); $sphere.Dispose(); $g.Dispose()
    return $bmp
}

$enumLines = @(); $dataLines = @(); $cursLines = @()
$widthLines = @(); $heightLines = @(); $specLines = @(); $powerLines = @()
$cursor = $FirstCursorId

foreach ($orb in $orbs) {
    $idi = "IDI_ORACOOL_ORB_$($orb.Id)"
    $icurs = "ICURS_ORACOOL_ORB_$($orb.Id)"
    $slug = "orb_" + $orb.Id.ToLowerInvariant()
    $png = Join-Path $artDir ("$slug.png")

    $bmp = New-Orb $orb.Colour
    try { $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $bmp.Dispose() }

    $enumLines += "`t$idi,"
    # ICLASS_MISC / ILOC_UNEQUIPABLE / IMISC_NONE - a rune's row exactly, which is what makes orbs
    # stack, sort and drop through the code that already exists.
    $dataLines += "/*$idi*/ { IDROP_REGULAR, ICLASS_MISC, ILOC_UNEQUIPABLE, $icurs, ItemType::Misc, UITYPE_NONE, N_(`"$($orb.Name)`"), N_(`"Mystic Orb`"), $($orb.Qlvl), 0, 0, 0, 0, 0, 0, 0, 0, ItemSpecialEffect::None, IMISC_NONE, SpellID::Null, false, $($orb.Value2) },"
    $cursLines += "`t$icurs,"
    $widthLines += "`t$cell, // $($orb.Name)"
    $heightLines += "`t$cell, // $($orb.Name)"
    $specLines += "$png,0,0,$cell,$cell,$cell,$cell,$slug,30,false,asis"

    # param1 == param2: SaveItemPower rolls a range between them, and an orb that rolled would be an
    # affix wearing a different name.
    $powerLines += "`t{ $idi, ItemPower { $($orb.Power), $($orb.Value), $($orb.Value) }, N_(`"$($orb.Line)`") }, // $($orb.Name)"

    $cursor++
}

function Write-Inc([string]$file, [string]$what, [string[]]$lines) {
    $header = @(
        "// GENERATED by tools/GenMysticOrbs.ps1 - do not edit by hand.",
        "// $what",
        ""
    )
    [System.IO.File]::WriteAllLines((Join-Path $out $file), ($header + $lines), (New-Object System.Text.UTF8Encoding $false))
    Write-Host ("  $file  ($($lines.Count) lines)")
}

Write-Inc "mystic_orbs_enum.inc"   "The eight Mystic Orb ids."                              $enumLines
Write-Inc "mystic_orbs_data.inc"   "Their AllItemsList rows, in enum order."                $dataLines
Write-Inc "mystic_orbs_curs.inc"   "Their ICURS ids - appended after the jewels."           $cursLines
Write-Inc "mystic_orbs_curs_widths.inc"  "Frame widths, in the same order."                 $widthLines
Write-Inc "mystic_orbs_curs_heights.inc" "Frame heights, in the same order."                $heightLines
Write-Inc "mystic_orbs_powers.inc" "Their powers and description lines, in enum order."     $powerLines

[System.IO.File]::WriteAllLines((Join-Path $out "mystic_orbs_icon_specs.txt"), $specLines, (New-Object System.Text.UTF8Encoding $false))
Write-Host ("  mystic_orbs_icon_specs.txt  ($($specLines.Count) specs)")
Write-Host ""
Write-Host ("$($enumLines.Count) orbs, cursor ids $FirstCursorId..$($cursor - 1), art in $artDir")
Write-Host "Now: tools\build_item_icons.cmd, then tools\build_oracool_mpq.cmd."
