# Generates the Imbuement Shards - the Mystic Orbs' replacement (plan: 01-Project-Overview/Plan - Imbuement
# Shards.md; decisions on the "Imbuement Shards" artifact, answered 2026-09-19).
#
# WHAT A SHARD IS
#
# A consumable that is RECORDED on the item it is dropped onto - a ledger of kinds, up to twenty per item -
# and whose effect is computed from that ledger every time the character sheet is totalled (the
# Imbuements bonus provider in oracool/stat_sheet.cpp). Nothing is written into the item's stat fields,
# which is the one structural difference from the orbs: a rebuilt or rerolled item keeps its shards, a
# tooltip can list them, and the crafting refusal the orbs needed is gone. Values are fixed, never rolled.
#
# TWO ISLANDS, ONE TABLE
#
# The eight orb item indices are positional save format (an item index is what a save stores), so they
# are RE-LABELLED as the first eight kinds and stay exactly where the orbs were - in the item table, the
# cursor enum and the icon sheet. The sixteen new kinds are appended at the END of all three (after the
# Necromancer's bases in the table, after the unique-base icons on the sheet). Both islands come out of
# the one $shards list below, in ShardKind order, which is what keeps the ids, the rows, the cursor ids,
# the frame sizes, the cut specs and the kind table from drifting apart - a CEL stores no names and no
# sizes, so a frame's POSITION is the only thing tying it to an id.
#
# Emits into Source/oracool:
#   shards_kinds.inc              the ShardKind table (kind, item index, name, line, limit, band) - all 24
#   shards_enum.inc               the 8 re-labelled ids (at the orbs' place in itemdat.h)
#   shards_enum_late.inc          the 16 new ids (after the Necromancer bases)
#   shards_data.inc               AllItemsList rows for the 8 (at the orbs' place in itemdat.cpp)
#   shards_data_late.inc          rows for the 16 (after the Necromancer rows)
#   shards_curs.inc / _late.inc   ICURS ids, same two places
#   shards_curs_widths.inc, shards_curs_heights.inc, and their _late twins   frame sizes (cursor.cpp)
#   shards_icon_specs.txt / shards_icon_specs_late.txt   cut specs (build_item_icons.cmd, same two places)
#
#     powershell -ExecutionPolicy Bypass -File tools\GenImbuementShards.ps1
param(
    [string]$OutDir = "Source\oracool",
    # Where the PROCEDURAL placeholders are drawn. Stays in %TEMP%: this folder is WIPED recursively
    # below, so pointing it at Resources would delete delivered art.
    [string]$ArtDir = (Join-Path ([System.IO.Path]::GetTempPath()) "oracool-shards"),
    # Where the SPECS point once RfA-18 (batch 41) lands: real art per file, with a fallback to the
    # placeholder, so a partial delivery still builds a whole sheet. Relative to the repository root,
    # which is where build_item_icons.cmd runs.
    [string]$SpecArtDir = "..\Resources\01-in-use-assets\items\shards"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$repo = Split-Path -Parent $PSScriptRoot
$out = if ([System.IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $repo $OutDir }
$artDir = $ArtDir
if (Test-Path $artDir) { Remove-Item -Recurse -Force $artDir }
New-Item -ItemType Directory -Force $artDir | Out-Null

$cell = 28
$realCount = 0
function Resolve-SpecPath([string]$slug, [string]$placeholder) {
    $rel = Join-Path $SpecArtDir ("$slug.png")
    if (Test-Path (Join-Path $repo $rel)) {
        $script:realCount++
        return $rel
    }
    return $placeholder
}

# --- the twenty-four kinds, in ShardKind order --------------------------------------------------------
#
# Late = $false: one of the eight re-labelled orb indices (Might, Grace, Insight, Vigour, Warding, Fury,
# Fortune, Avarice - in THAT order, which is the orbs' enum order and therefore save format).
# Limit 0 means "the cap"; Ease's limit is "until every requirement is zero" and is enforced in code.
# Band is the qlvl column of the item row, which is what the drop walk gates on (BandedQlvl): shallow 1,
# middle 9 (the Caves' rung), deep 17 (the second difficulty). Line is the shard's own description.
$shards = @(
    @{ Id = 'STRENGTH';   Name = 'Shard of Strength';   Line = '+1 to Strength';                              Limit = 0;  Band = 9;  Value = 1200; Late = $false; Colour = @(190,  72,  56) }
    @{ Id = 'DEXTERITY';  Name = 'Shard of Dexterity';  Line = '+1 to Dexterity';                             Limit = 0;  Band = 9;  Value = 1200; Late = $false; Colour = @( 84, 168,  92) }
    @{ Id = 'MAGIC';      Name = 'Shard of Magic';      Line = '+1 to Magic';                                 Limit = 0;  Band = 9;  Value = 1200; Late = $false; Colour = @( 70, 116, 206) }
    @{ Id = 'VITALITY';   Name = 'Shard of Vitality';   Line = '+1 to Vitality';                              Limit = 0;  Band = 9;  Value = 1200; Late = $false; Colour = @(206, 120,  48) }
    @{ Id = 'WARDING';    Name = 'Shard of Warding';    Line = '+2 to all resistances';                       Limit = 0;  Band = 9;  Value = 1200; Late = $false; Colour = @(146, 100, 190) }
    @{ Id = 'FURY';       Name = 'Shard of Fury';       Line = '+1 to damage';                                Limit = 0;  Band = 9;  Value = 1200; Late = $false; Colour = @(176,  46,  46) }
    @{ Id = 'FORTUNE';    Name = 'Shard of Fortune';    Line = '+2% magic find';                              Limit = 0;  Band = 9;  Value = 1200; Late = $false; Colour = @(206, 190,  92) }
    @{ Id = 'AVARICE';    Name = 'Shard of Avarice';    Line = '+3% gold find';                               Limit = 0;  Band = 9;  Value = 1200; Late = $false; Colour = @(214, 174,  54) }
    @{ Id = 'BLOOD';      Name = 'Shard of Blood';      Line = '+5 to life';                                  Limit = 0;  Band = 9;  Value = 1200; Late = $true;  Colour = @(160,  30,  40) }
    @{ Id = 'SPIRIT';     Name = 'Shard of Spirit';     Line = '+5 to mana';                                  Limit = 0;  Band = 9;  Value = 1200; Late = $true;  Colour = @( 60, 140, 200) }
    @{ Id = 'KEENNESS';   Name = 'Shard of Keenness';   Line = '+2% damage';                                  Limit = 0;  Band = 9;  Value = 1200; Late = $true;  Colour = @(200,  90,  60) }
    @{ Id = 'PRECISION';  Name = 'Shard of Precision';  Line = '+2% to hit';                                  Limit = 0;  Band = 9;  Value = 1200; Late = $true;  Colour = @(210, 150,  90) }
    @{ Id = 'FLAME';      Name = 'Shard of Flame';      Line = '+1-2 fire damage';                            Limit = 0;  Band = 9;  Value = 1200; Late = $true;  Colour = @(230, 110,  30) }
    @{ Id = 'SPARK';      Name = 'Shard of Spark';      Line = '+1-3 lightning damage';                       Limit = 0;  Band = 9;  Value = 1200; Late = $true;  Colour = @(120, 170, 240) }
    @{ Id = 'BULWARK';    Name = 'Shard of Bulwark';    Line = '+2 to armour';                                Limit = 0;  Band = 1;  Value =  600; Late = $true;  Colour = @(150, 150, 160) }
    @{ Id = 'STONE';      Name = 'Shard of Stone';      Line = '-1 damage taken';                             Limit = 10; Band = 17; Value = 2400; Late = $true;  Colour = @(110, 105, 100) }
    @{ Id = 'EMBER';      Name = 'Shard of Ember';      Line = '+3% fire resistance';                         Limit = 0;  Band = 1;  Value =  600; Late = $true;  Colour = @(220,  70,  40) }
    @{ Id = 'STORM';      Name = 'Shard of Storm';      Line = '+3% lightning resistance';                    Limit = 0;  Band = 1;  Value =  600; Late = $true;  Colour = @( 90, 120, 230) }
    @{ Id = 'VEIL';       Name = 'Shard of Veil';       Line = '+3% magic resistance';                        Limit = 0;  Band = 1;  Value =  600; Late = $true;  Colour = @(160, 110, 200) }
    @{ Id = 'RADIANCE';   Name = 'Shard of Radiance';   Line = '+1 to light radius';                          Limit = 5;  Band = 1;  Value =  600; Late = $true;  Colour = @(240, 220, 140) }
    @{ Id = 'ARCANA';     Name = 'Shard of Arcana';     Line = '+1 to spell levels';                          Limit = 3;  Band = 17; Value = 2400; Late = $true;  Colour = @(120,  70, 190) }
    @{ Id = 'REFINEMENT'; Name = 'Shard of Refinement'; Line = 'every affix +3% of its value';                Limit = 10; Band = 17; Value = 2400; Late = $true;  Colour = @(230, 230, 240) }
    @{ Id = 'TEMPERING';  Name = 'Shard of Tempering';  Line = '+10 maximum durability';                      Limit = 10; Band = 1;  Value =  600; Late = $true;  Colour = @( 90, 100, 120) }
    @{ Id = 'EASE';       Name = 'Shard of Ease';       Line = '-3 to each requirement';                      Limit = 0;  Band = 1;  Value =  600; Late = $true;  Colour = @(130, 190, 170) }
)

<#
.SYNOPSIS
Draws a shard: an elongated crystal, faceted, lit from up-left like every other stand-in on the sheet.

Deliberately NOT the orb's sphere, the jewel's lozenge or the salvage tablet: a player scanning the ground
tells the families apart by SILHOUETTE before colour.
#>
function New-Shard([int[]]$rgb) {
    $bmp = New-Object System.Drawing.Bitmap $cell, $cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $clamp = { param($v) [Math]::Max(0, [Math]::Min(255, [int]$v)) }
    $mid  = [System.Drawing.Color]::FromArgb(255, $rgb[0], $rgb[1], $rgb[2])
    $lit  = [System.Drawing.Color]::FromArgb(255, (& $clamp ($rgb[0] + 70)), (& $clamp ($rgb[1] + 70)), (& $clamp ($rgb[2] + 70)))
    $dark = [System.Drawing.Color]::FromArgb(255, (& $clamp ($rgb[0] - 70)), (& $clamp ($rgb[1] - 70)), (& $clamp ($rgb[2] - 70)))

    # A tall crystal: top point, two shoulders, two lower flanks, bottom point - tilted a little.
    $pts = @(
        (New-Object System.Drawing.PointF 15.0, 2.0),
        (New-Object System.Drawing.PointF 22.0, 9.0),
        (New-Object System.Drawing.PointF 19.0, 25.0),
        (New-Object System.Drawing.PointF 12.0, 26.0),
        (New-Object System.Drawing.PointF 6.0, 12.0)
    )
    $body = New-Object System.Drawing.Drawing2D.GraphicsPath
    $body.AddPolygon($pts)
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush ((New-Object System.Drawing.PointF 6.0, 4.0), (New-Object System.Drawing.PointF 22.0, 26.0), $lit, $dark)
    $g.FillPath($brush, $body)
    # The lit facet: the left face, a lighter wedge.
    $facet = New-Object System.Drawing.Drawing2D.GraphicsPath
    $facet.AddPolygon(@((New-Object System.Drawing.PointF 15.0, 2.0), (New-Object System.Drawing.PointF 6.0, 12.0), (New-Object System.Drawing.PointF 12.0, 26.0), (New-Object System.Drawing.PointF 14.0, 10.0)))
    $fb = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(110, 255, 255, 255))
    $g.FillPath($fb, $facet)
    $edge = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 10, 9, 12)), 1
    $g.DrawPath($edge, $body)
    $ridge = New-Object System.Drawing.Pen $mid, 1
    $g.DrawLine($ridge, 15.0, 2.0, 14.0, 10.0)
    $g.DrawLine($ridge, 14.0, 10.0, 12.0, 26.0)
    $spec = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(200, 255, 255, 255))
    $g.FillEllipse($spec, 12.5, 5.0, 3.0, 2.2)

    $spec.Dispose(); $ridge.Dispose(); $edge.Dispose(); $fb.Dispose(); $facet.Dispose(); $brush.Dispose(); $body.Dispose(); $g.Dispose()
    return $bmp
}

$kindLines = @()
$enumEarly = @(); $enumLate = @()
$dataEarly = @(); $dataLate = @()
$cursEarly = @(); $cursLate = @()
$widthEarly = @(); $widthLate = @(); $heightEarly = @(); $heightLate = @()
$specEarly = @(); $specLate = @()

$kindIndex = 0
foreach ($s in $shards) {
    $idi = "IDI_ORACOOL_SHARD_$($s.Id)"
    $icurs = "ICURS_ORACOOL_SHARD_$($s.Id)"
    $slug = "shard_" + $s.Id.ToLowerInvariant()
    $png = Join-Path $artDir ("$slug.png")
    $kind = (Get-Culture).TextInfo.ToTitleCase($s.Id.ToLowerInvariant())

    $bmp = New-Shard $s.Colour
    try { $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $bmp.Dispose() }

    # ICLASS_MISC / ILOC_UNEQUIPABLE / IMISC_NONE - a rune's row exactly, which is what makes shards
    # stack, sort and drop through the code that already exists. The qlvl column is the drop band.
    $data = "/*$idi*/ { IDROP_REGULAR, ICLASS_MISC, ILOC_UNEQUIPABLE, $icurs, ItemType::Misc, UITYPE_NONE, N_(`"$($s.Name)`"), N_(`"Imbuement Shard`"), $($s.Band), 0, 0, 0, 0, 0, 0, 0, 0, ItemSpecialEffect::None, IMISC_NONE, SpellID::Null, false, $($s.Value) },"
    $spec = "$(Resolve-SpecPath $slug $png),0,0,$cell,$cell,$cell,$cell,$slug,30,false,asis"
    $kindLines += "`t{ ShardKind::$kind, $idi, N_(`"$($s.Name)`"), N_(`"$($s.Line)`"), $($s.Limit), $($s.Band) },"

    if ($s.Late) {
        $enumLate += "`t$idi,"; $dataLate += $data; $cursLate += "`t$icurs,"
        $widthLate += "`t$cell, // $($s.Name)"; $heightLate += "`t$cell, // $($s.Name)"; $specLate += $spec
    } else {
        $enumEarly += "`t$idi,"; $dataEarly += $data; $cursEarly += "`t$icurs,"
        $widthEarly += "`t$cell, // $($s.Name)"; $heightEarly += "`t$cell, // $($s.Name)"; $specEarly += $spec
    }
    $kindIndex++
}

function Write-Inc([string]$file, [string]$what, [string[]]$lines) {
    $header = @(
        "// GENERATED by tools/GenImbuementShards.ps1 - do not edit by hand.",
        "// $what",
        ""
    )
    [System.IO.File]::WriteAllLines((Join-Path $out $file), ($header + $lines), (New-Object System.Text.UTF8Encoding $false))
    Write-Host ("  $file  ($($lines.Count) lines)")
}

Write-Inc "shards_kinds.inc"             "The 24 kinds, in ShardKind order: kind, item index, name, line, limit (0 = the cap), drop band (qlvl)." $kindLines
Write-Inc "shards_enum.inc"              "The 8 re-labelled orb indices - the orbs' place in the enum, which is save format."               $enumEarly
Write-Inc "shards_enum_late.inc"         "The 16 new indices, appended after the Necromancer bases."                                         $enumLate
Write-Inc "shards_data.inc"              "AllItemsList rows for the 8, at the orbs' place."                                                   $dataEarly
Write-Inc "shards_data_late.inc"         "AllItemsList rows for the 16, after the Necromancer rows."                                          $dataLate
Write-Inc "shards_curs.inc"              "ICURS ids for the 8 - the orbs' frames on the sheet."                                               $cursEarly
Write-Inc "shards_curs_late.inc"         "ICURS ids for the 16 - appended after the unique-base icons."                                       $cursLate
Write-Inc "shards_curs_widths.inc"       "Frame widths for the 8, in the same order."                                                         $widthEarly
Write-Inc "shards_curs_heights.inc"      "Frame heights for the 8, in the same order."                                                        $heightEarly
Write-Inc "shards_curs_widths_late.inc"  "Frame widths for the 16, in the same order."                                                        $widthLate
Write-Inc "shards_curs_heights_late.inc" "Frame heights for the 16, in the same order."                                                       $heightLate
# The spec files go out WITHOUT a BOM: build_item_icons.cmd concatenates them with `type`, and a BOM
# landing mid-file makes the first line after it unparseable.
[System.IO.File]::WriteAllLines((Join-Path $out "shards_icon_specs.txt"), $specEarly, (New-Object System.Text.UTF8Encoding $false))
[System.IO.File]::WriteAllLines((Join-Path $out "shards_icon_specs_late.txt"), $specLate, (New-Object System.Text.UTF8Encoding $false))

Write-Host ("generated {0} shards ({1} re-labelled, {2} new); real art: {3} of 24 specs, from {4}" -f `
        $shards.Count, $enumEarly.Count, $enumLate.Count, $realCount, $SpecArtDir)
Write-Host "  placeholders drawn into $artDir (fallback only)"
Write-Host "Now: tools\build_item_icons.cmd, then tools\build_oracool_mpq.cmd."
