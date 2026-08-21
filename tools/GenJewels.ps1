# Generates the jewels - the third socket family, after gems and runes.
#
# WHY THEY ARE NOT "ROLLED"
#
# The Pipeline row asked for "a socketable with rolled affixes rather than a fixed effect". That
# cannot be built the way it is worded, and the reason is structural rather than a matter of effort:
# a socket stores ONE uint16_t, the socketed item's base index (Item::_iSocketed). Gems and runes
# work because their effect is a pure function of that index. A per-instance roll has nowhere to
# live - it would need a parallel seed array on every item, a save format bump, and a rewrite of
# every socket walk.
#
# The engine's own answer to "varied socketable" is already on the shelf: the GEM ladder is 35
# indices - seven types by five qualities - and the variety a player experiences is WHICH gem drops.
# Jewels do the same thing with a different axis: five affix families by three grades. From the
# player's side that is a jewel that might be any of fifteen things; from the engine's side it is
# fifteen ordinary rows in the table that already exists, and no save change at all.
#
# ONE WALK, ONE ORDER - the discipline every generator here follows. The ids, the AllItemsList rows,
# the ICURS ids, the CEL frame sizes, the icon cut specs and the effect rows all come out of the
# single loop below. A CEL stores no names and no sizes, so a frame's POSITION in the file is the
# only thing tying it to an id: any of those six drifting means every icon after the first mismatch
# is silently wrong.
#
#     pwsh -File tools\GenJewels.ps1
param(
    [string]$OutDir = "Source\oracool",
    [string]$ArtDir = (Join-Path ([System.IO.Path]::GetTempPath()) "oracool-jewels"),
    # One past the last Charm of Salvaging. cursor.cpp static_asserts the adjacency.
    [int]$FirstCursorId = 462
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$repo = Split-Path -Parent $PSScriptRoot
$out = if ([System.IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $repo $OutDir }
$artDir = $ArtDir
if (Test-Path $artDir) { Remove-Item -Recurse -Force $artDir }
New-Item -ItemType Directory -Force $artDir | Out-Null

$cell = 28

# --- the five families -----------------------------------------------------------------------------
# Each is one idea expressed in whichever field its host supports. That per-host difference is not a
# compromise, it is how the gems already read: a Sapphire is mana everywhere, but a Ruby is fire
# damage in a weapon and fire RESISTANCE in armour, because those are the same idea from the two
# ends. Every field below exists in GemData and is already applied by the socket walk.
$families = @(
    @{ Id = 'FERVOR';  Name = 'Fervor';  Colour = @(198,  54,  54)
       Fields = @{ weaponToHit = 15; armorToHit = 15; shieldBonusAc = 10 } }
    @{ Id = 'FOCUS';   Name = 'Focus';   Colour = @( 62, 104, 200)
       Fields = @{ weaponMana = 12; armorMana = 12; shieldMana = 12 } }
    @{ Id = 'AEGIS';   Name = 'Aegis';   Colour = @(150, 158, 170)
       Fields = @{ armorBonusAc = 12; shieldBonusAc = 12; weaponDamageMod = 3 } }
    @{ Id = 'RUIN';    Name = 'Ruin';    Colour = @(206, 112,  40)
       Fields = @{ weaponDamageMod = 4; armorStrength = 6; shieldBonusAc = 8 } }
    @{ Id = 'WARDING'; Name = 'Warding'; Colour = @(120,  84, 176)
       Fields = @{ armorMagicRes = 12; shieldMagicRes = 12; weaponToHit = 8 } }
)

# --- the three grades ------------------------------------------------------------------------------
# Percentages rather than three hand-written value sets, so a family is tuned in ONE place and its
# ladder follows. qlvl climbs steeply: a Radiant jewel should be a late find, not a common one.
$grades = @(
    @{ Id = 'FLAWED';  Prefix = 'Flawed ';  Percent =  60; Qlvl =  6; Value =  800; Bright = -28 }
    @{ Id = 'PLAIN';   Prefix = '';         Percent = 100; Qlvl = 14; Value = 2000; Bright =   0 }
    @{ Id = 'RADIANT'; Prefix = 'Radiant '; Percent = 160; Qlvl = 24; Value = 4800; Bright =  34 }
)

<#
.SYNOPSIS
Draws a faceted jewel: a lozenge with a bright top-left facet and a dark bottom-right one.

Deliberately NOT the salvage orb's shape. A player scanning the ground has to tell a jewel from a
material at a glance, and shape reads faster than colour - the four salvage materials already prove
how close two colours can look at 28 pixels.
#>
function New-Jewel([int[]]$rgb, [int]$bright) {
    $bmp = New-Object System.Drawing.Bitmap $cell, $cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $clamp = { param($v) [Math]::Max(0, [Math]::Min(255, [int]$v)) }
    $base = @(
        (& $clamp ($rgb[0] + $bright)),
        (& $clamp ($rgb[1] + $bright)),
        (& $clamp ($rgb[2] + $bright))
    )
    $lit  = [System.Drawing.Color]::FromArgb(255, (& $clamp ($base[0] + 60)), (& $clamp ($base[1] + 60)), (& $clamp ($base[2] + 60)))
    $mid  = [System.Drawing.Color]::FromArgb(255, $base[0], $base[1], $base[2])
    $dark = [System.Drawing.Color]::FromArgb(255, (& $clamp ($base[0] - 55)), (& $clamp ($base[1] - 55)), (& $clamp ($base[2] - 55)))

    $cx = $cell / 2.0
    $top = 3.0; $bot = $cell - 3.0; $left = 6.0; $right = $cell - 6.0
    $whole = New-Object System.Drawing.Drawing2D.GraphicsPath
    $whole.AddPolygon(@(
        (New-Object System.Drawing.PointF($cx, $top)),
        (New-Object System.Drawing.PointF($right, $cx)),
        (New-Object System.Drawing.PointF($cx, $bot)),
        (New-Object System.Drawing.PointF($left, $cx))))
    $brushMid = New-Object System.Drawing.SolidBrush $mid
    $g.FillPath($brushMid, $whole)

    # Top-left facet lit, bottom-right shadowed - one light source, same as the salvage tablets.
    $upper = New-Object System.Drawing.Drawing2D.GraphicsPath
    $upper.AddPolygon(@(
        (New-Object System.Drawing.PointF($cx, $top)),
        (New-Object System.Drawing.PointF($cx, $cx)),
        (New-Object System.Drawing.PointF($left, $cx))))
    $brushLit = New-Object System.Drawing.SolidBrush $lit
    $g.FillPath($brushLit, $upper)

    $lower = New-Object System.Drawing.Drawing2D.GraphicsPath
    $lower.AddPolygon(@(
        (New-Object System.Drawing.PointF($cx, $bot)),
        (New-Object System.Drawing.PointF($cx, $cx)),
        (New-Object System.Drawing.PointF($right, $cx))))
    $brushDark = New-Object System.Drawing.SolidBrush $dark
    $g.FillPath($brushDark, $lower)

    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 12, 10, 14)), 1
    $g.DrawPath($pen, $whole)

    $pen.Dispose(); $brushMid.Dispose(); $brushLit.Dispose(); $brushDark.Dispose()
    $upper.Dispose(); $lower.Dispose(); $whole.Dispose(); $g.Dispose()
    return $bmp
}

$enumLines = @(); $dataLines = @(); $cursLines = @()
$widthLines = @(); $heightLines = @(); $specLines = @(); $effectLines = @()
$cursor = $FirstCursorId

# Grade-major so the fifteen ids read as three ladders rather than five, and so a whole grade can be
# gated by qlvl as one block.
foreach ($grade in $grades) {
    foreach ($fam in $families) {
        $id = "$($fam.Id)_$($grade.Id)"
        $idi = "IDI_ORACOOL_JEWEL_$id"
        $icurs = "ICURS_ORACOOL_JEWEL_$id"
        $name = "$($grade.Prefix)Jewel of $($fam.Name)"
        $slug = "jewel_" + $id.ToLowerInvariant()
        $png = Join-Path $artDir ("$slug.png")

        $bmp = New-Jewel $fam.Colour $grade.Bright
        try { $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $bmp.Dispose() }

        $enumLines += "`t$idi,"
        # ICLASS_MISC / ILOC_UNEQUIPABLE / IMISC_NONE - a rune's row exactly, which is what makes
        # jewels stack, sort and socket through the code that already exists.
        $dataLines += "/*$idi*/ { IDROP_REGULAR, ICLASS_MISC, ILOC_UNEQUIPABLE, $icurs, ItemType::Misc, UITYPE_NONE, N_(`"$name`"), N_(`"Jewel`"), $($grade.Qlvl), 0, 0, 0, 0, 0, 0, 0, 0, ItemSpecialEffect::None, IMISC_NONE, SpellID::Null, false, $($grade.Value) },"
        $cursLines += "`t$icurs,"
        $widthLines += "`t$cell, // $name"
        $heightLines += "`t$cell, // $name"
        $specLines += "$png,0,0,$cell,$cell,$cell,$cell,$slug,30,false,asis"

        # The effect row, scaled by grade. Designated initialisers, like the gem rows - a positional
        # row here would be one inserted field away from re-reading every trailing number.
        $parts = @()
        foreach ($key in ($fam.Fields.Keys | Sort-Object)) {
            $scaled = [Math]::Max(1, [int][Math]::Round($fam.Fields[$key] * $grade.Percent / 100.0))
            $parts += ".$key = $scaled"
        }
        $effectLines += "`t{ .idx = $idi, $($parts -join ', ') }, // $name"

        $cursor++
    }
}

function Write-Inc([string]$file, [string]$what, [string[]]$lines) {
    $header = @(
        "// GENERATED by tools/GenJewels.ps1 - do not edit by hand.",
        "// $what",
        ""
    )
    [System.IO.File]::WriteAllLines((Join-Path $out $file), ($header + $lines), (New-Object System.Text.UTF8Encoding $false))
    Write-Host ("  $file  ($($lines.Count) lines)")
}

Write-Inc "jewels_enum.inc"    "The fifteen jewel ids, grade-major."                        $enumLines
Write-Inc "jewels_data.inc"    "Their AllItemsList rows, in enum order."                    $dataLines
Write-Inc "jewels_curs.inc"    "Their ICURS ids - appended after the salvage charms."       $cursLines
Write-Inc "jewels_curs_widths.inc"  "Frame widths, in the same order."                      $widthLines
Write-Inc "jewels_curs_heights.inc" "Frame heights, in the same order."                     $heightLines
Write-Inc "jewels_effects.inc" "Their socket effects, for the Gems[] table in gems.cpp."    $effectLines

[System.IO.File]::WriteAllLines((Join-Path $out "jewels_icon_specs.txt"), $specLines, (New-Object System.Text.UTF8Encoding $false))
Write-Host ("  jewels_icon_specs.txt  ($($specLines.Count) specs)")
Write-Host ""
Write-Host ("$($enumLines.Count) jewels, cursor ids $FirstCursorId..$($cursor - 1), art in $artDir")
Write-Host "Now: tools\build_item_icons.cmd, then tools\build_oracool_mpq.cmd."
