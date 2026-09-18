# GenRunes.ps1 - one walk, one order: the 33 Diablo II runes.
#
#     powershell -ExecutionPolicy Bypass -File tools\GenRunes.ps1
#
# Emits, from the single table below:
#   Source/oracool/runes_enum.inc          the 28 NEW IDI_ORACOOL_RUNE_* ids
#   Source/oracool/runes_data.inc          their AllItemsList rows
#   Source/oracool/runes_curs.inc          their ICURS_ORACOOL_RUNE_* ids
#   Source/oracool/runes_curs_widths.inc   the CEL frame widths, in frame order
#   Source/oracool/runes_curs_heights.inc  the heights
#   Source/oracool/runes_icon_specs.txt    the cut specs, in the same frame order
#   Source/oracool/runes_effects.inc       the GemData rows carrying each rune's effects
#
# The same discipline as GenItemSets.ps1 and GenUniqueItems.ps1, and for the same reason: a CEL
# stores no names and no sizes, so a frame's POSITION in the file is the only thing tying it to an
# id. One generator walking one list is what keeps the ids, the sizes, the cut order and the data
# rows from drifting apart.
#
# Five runes (El, Tir, Ral, Ort, Sol) shipped in v1.7.8 and are NOT regenerated here: item indices
# are positional save format, so those five keep their ids and their icons where they are. This
# emits the other 28, appended after the gem ladder - which is why IsOracoolRuneIdx becomes two
# ranges rather than one.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'GemDataOrder.ps1')
$out = Join-Path $root 'Source\oracool'
# item-sets\, not items\ - the sheet has always lived there and this path had the wrong folder
# (found 2026-09-12 while rebuilding the Resources folders; it predates that move).
#
# RELATIVE to the repository root, like every sibling generator and like build_item_icons.cmd's own
# ART variable. It was an absolute C:\Users\hroga\... literal until 2026-09-12, which it then baked
# into all 28 lines of the committed runes_icon_specs.txt - so the icon sheet could only be rebuilt
# on this one machine, in this one user profile. The spec files are tracked; a machine-specific path
# in a tracked file is a path that is wrong for everybody else.
$artRel = '..\Resources\01-in-use-assets\item-sets\item-runes-v1.png'
$art = Join-Path $root $artRel

# The sheet's grid, MEASURED (non-green runs) rather than guessed - 11 columns x 3 rows, D2's own
# rune order reading left to right, top to bottom.
$cols = @(
    @{ x = 39; w = 101 }, @{ x = 169; w = 99 }, @{ x = 299; w = 98 }, @{ x = 422; w = 101 },
    @{ x = 551; w = 100 }, @{ x = 676; w = 99 }, @{ x = 798; w = 99 }, @{ x = 926; w = 100 },
    @{ x = 1053; w = 98 }, @{ x = 1180; w = 98 }, @{ x = 1306; w = 99 }
)
$rows = @(@{ y = 239; h = 125 }, @{ y = 452; h = 124 }, @{ y = 670; h = 124 })

# The 33 runes in D2's order. `shipped` marks the five that already exist - they are skipped by the
# id, data, icon and size emitters, and included only in the effects table, which is keyed by item
# index and covers all 33 in one place.
#
# `effects` is a fragment of designated initialisers for the GemData row. Where D2 leans on a
# channel this engine does not have - cold, poison, crushing blow, open wounds, deadly strike,
# freeze, "monster flees" - the effect moves to the nearest channel that exists and the move is
# named in `note`, never fudged silently. Hel and Zod act on the HOST ITEM (its requirements, its
# durability) rather than on the totals, so they carry their own fields.
$runes = @(
    @{ n = 'El'; shipped = $true; note = "D2's +50 Attack Rating at its own ~10 AR : 1% convention" },
    @{ n = 'Eld'; effects = '.weaponDamagePercent = 7, .armorHitPoints = 10, .shieldBonusAc = 7, .shieldFlags = ItemSpecialEffect::FastBlock';
        note = 'damage vs undead has no channel - it becomes flat % damage; the stamina half becomes life' },
    @{ n = 'Tir'; shipped = $true; note = '' },
    @{ n = 'Nef'; effects = '.weaponFlags = ItemSpecialEffect::Knockback, .damageReduction = 3';
        note = "D2's -30 missile damage becomes flat damage reduction" },
    @{ n = 'Eth'; effects = '.weaponToHit = 10, .armorMana = 10, .shieldMana = 10';
        note = '"-25% target defence" is to-hit in this engine' },
    @{ n = 'Ith'; effects = '.weaponDamageMod = 9, .armorMana = 8, .shieldMana = 8'; note = '' },
    @{ n = 'Tal'; effects = '.weaponDamageMod = 5, .armorMagicRes = 30, .shieldMagicRes = 35';
        note = 'poison has no channel; poison resist becomes magic resist' },
    @{ n = 'Ral'; shipped = $true; note = '' },
    @{ n = 'Ort'; shipped = $true; note = '' },
    @{ n = 'Thul'; effects = '.weaponDamageMod = 8, .armorMagicRes = 30, .shieldMagicRes = 35';
        note = 'this engine has no cold at all; cold resist becomes magic resist' },
    @{ n = 'Amn'; effects = '.weaponFlags = ItemSpecialEffect::StealLife5, .armorFlags = ItemSpecialEffect::Thorns, .shieldFlags = ItemSpecialEffect::Thorns';
        note = "D2's 7% life steal rides the engine's own steal flag" },
    @{ n = 'Sol'; shipped = $true; note = 'min-only damage cannot cross max in this engine' },
    @{ n = 'Shael'; effects = '.weaponFlags = ItemSpecialEffect::FasterAttack, .armorFlags = ItemSpecialEffect::FasterHitRecovery, .shieldFlags = ItemSpecialEffect::FastBlock';
        note = "straight onto the engine's own speed ladders" },
    @{ n = 'Dol'; effects = '.lifePerKill = 4, .armorHitPoints = 10, .shieldMana = 10';
        note = '"monster flees" has no channel; replenish life becomes life per kill' },
    @{ n = 'Hel'; effects = '.requirementPercentReduction = 20';
        note = "acts on the host's own strength/magic/dexterity requirements" },
    @{ n = 'Io'; effects = '.allVitality = 10'; note = '' },
    @{ n = 'Lum'; effects = '.allMagic = 10'; note = "D2's energy is magic here" },
    @{ n = 'Ko'; effects = '.allDexterity = 10'; note = '' },
    @{ n = 'Fal'; effects = '.allStrength = 10'; note = '' },
    @{ n = 'Lem'; effects = '.allGoldFind = 75'; note = '' },
    @{ n = 'Pul'; effects = '.weaponFlags = ItemSpecialEffect::TripleDemonDamage, .armorBonusAc = 25, .shieldBonusAc = 25';
        note = "the engine's own demon flag carries D2's intent" },
    @{ n = 'Um'; effects = '.weaponDamageMod = 10, .armorFireRes = 15, .armorLightRes = 15, .armorMagicRes = 15, .shieldFireRes = 22, .shieldLightRes = 22, .shieldMagicRes = 22';
        note = 'open wounds has no channel - it becomes flat damage' },
    @{ n = 'Mal'; effects = '.weaponDamageMod = 12, .damageReduction = 7';
        note = '"prevent monster heal" has no channel; magic damage reduction becomes flat' },
    @{ n = 'Ist'; effects = '.allMagicFind = 30'; note = '' },
    @{ n = 'Gul'; effects = '.weaponToHit = 20, .armorMagicRes = 5, .shieldMagicRes = 5'; note = '' },
    @{ n = 'Vex'; effects = '.weaponFlags = ItemSpecialEffect::StealMana5, .armorFireRes = 5, .shieldFireRes = 5';
        note = "D2's 7% mana steal rides the engine's own steal flag" },
    @{ n = 'Ohm'; effects = '.weaponDamagePercent = 50, .armorMagicRes = 5, .shieldMagicRes = 5'; note = '' },
    @{ n = 'Lo'; effects = '.weaponDamagePercent = 20, .armorLightRes = 5, .shieldLightRes = 5';
        note = 'deadly strike has no channel - it becomes % damage' },
    @{ n = 'Sur'; effects = '.weaponMana = 20, .armorMana = 50, .shieldMana = 50';
        note = 'blind has no channel' },
    @{ n = 'Ber'; effects = '.weaponDamagePercent = 25, .damageReduction = 8';
        note = 'crushing blow has no channel - it becomes % damage' },
    @{ n = 'Jah'; effects = '.weaponToHit = 25, .armorHitPoints = 50, .shieldBonusAc = 20';
        note = '"ignore target defence" is to-hit here' },
    @{ n = 'Cham'; effects = '.weaponDamageMod = 15, .armorMagicRes = 10, .shieldMagicRes = 10';
        note = 'freeze has no channel' },
    @{ n = 'Zod'; effects = '.indestructible = true'; note = "sets the host's durability" }
)

if ($runes.Count -ne 33) { throw "expected 33 runes, have $($runes.Count)" }

# Depth and price ladders. D2 runs its runes from level 11 (El) to 69 (Zod); this maps that span
# onto our own area ladder, so El is a first-floor find and Zod lands on the last rung anything is
# allowed to need. The five shipped runes' qlvls move with everything else - qlvl is drop gating,
# not save format.
#
# The span ends at 48, Hell/Hell (user, 2026-09-12: "hell/hell should be the threshhold for reaching
# god tier items. everything should be droppable by then"). It ran to 94 against the old 96-rung
# ladder, which had a second cost: BandedQlvl passes anything authored past 51 through its fallback,
# so the whole top half of the ladder - Fal upwards - shared one depth instead of climbing.
function Get-Qlvl([int]$index) { return [Math]::Max(1, 3 + [Math]::Round(($index) * 45.0 / 32.0)) }
function Get-Value([int]$index) {
    return [Math]::Min(30000, [int][Math]::Round(1200 * [Math]::Pow(1.13, $index)))
}

$enumLines = New-Object System.Collections.ArrayList
$dataLines = New-Object System.Collections.ArrayList
$cursLines = New-Object System.Collections.ArrayList
$widthLines = New-Object System.Collections.ArrayList
$heightLines = New-Object System.Collections.ArrayList
$specLines = New-Object System.Collections.ArrayList
$effectLines = New-Object System.Collections.ArrayList
$shippedLines = New-Object System.Collections.ArrayList

for ($i = 0; $i -lt $runes.Count; $i++) {
    $rune = $runes[$i]
    $name = $rune.n
    $upper = $name.ToUpper()
    $lower = $name.ToLower()
    $qlvl = Get-Qlvl $i
    $value = Get-Value $i

    # The effects table covers all 33 - it is keyed by item index, and the five shipped runes keep
    # their existing rows in gems.cpp, so only the new 28 are emitted here.
        if ($rune.shipped) {
            # The five that shipped in v1.7.8 keep their item indices - those are positional save
            # format - but their ROWS are regenerated here so all 33 sit on one ladder. They used to
            # be spaced as though five were the whole set (El 3, Tir 7 ... Sol 19), which left the
            # ladder non-monotonic once the other 28 arrived: Ral would have gated shallower than
            # Tal, three places behind it. A test asserts the ladder climbs.
            $shippedLines.Add(("/*IDI_ORACOOL_RUNE_{0}*/ {{ IDROP_REGULAR, ICLASS_MISC, ILOC_UNEQUIPABLE, ICURS_ORACOOL_RUNE_{0}, ItemType::Misc, UITYPE_NONE, N_(`"{1} Rune`"), N_(`"Rune`"), {2}, 0, 0, 0, 0, 0, 0, 0, 0, ItemSpecialEffect::None, IMISC_NONE, SpellID::Null, false, {3} }}," -f `
                    $upper, $name, $qlvl, $value)) | Out-Null
        }

    if (-not $rune.shipped) {
        [void]$enumLines.Add("`tIDI_ORACOOL_RUNE_$upper,")

        $curs = "ICURS_ORACOOL_RUNE_$upper"
        [void]$cursLines.Add("`t$curs,")
        [void]$widthLines.Add("`t1 * 28, // rune_$lower")
        [void]$heightLines.Add("`t1 * 28, // rune_$lower")

        $col = $cols[$i % 11]
        $row = $rows[[Math]::Floor($i / 11)]
        [void]$specLines.Add("$artRel,$($col.x),$($row.y),$($col.w),$($row.h),28,28,rune_$lower,30,false,green")

        $displayName = "$name Rune"
        $dataLines.Add(("/*IDI_ORACOOL_RUNE_{0}*/ {{ IDROP_REGULAR, ICLASS_MISC, ILOC_UNEQUIPABLE, {1}, ItemType::Misc, UITYPE_NONE, N_(`"{2}`"), N_(`"Rune`"), {3}, 0, 0, 0, 0, 0, 0, 0, 0, ItemSpecialEffect::None, IMISC_NONE, SpellID::Null, false, {4} }}," -f `
                $upper, $curs, $displayName, $qlvl, $value)) | Out-Null

        if ($rune.effects) {
            $comment = if ($rune.note) { " // $($rune.note)" } else { '' }
            [void]$effectLines.Add("`t{ .idx = IDI_ORACOOL_RUNE_$upper, $(Sort-GemDesignators $rune.effects) },$comment")
        }
    }
}

function Write-Inc([string]$file, [string]$what, $lines) {
    $header = @(
        "// GENERATED by tools/GenRunes.ps1 - do not edit by hand.",
        "// $what",
        ""
    )
    Set-Content -Path (Join-Path $out $file) -Value ($header + $lines) -Encoding UTF8
}

Write-Inc 'runes_enum.inc' 'The 28 runes added to the five that shipped in v1.7.8.' $enumLines
Write-Inc 'runes_data.inc' 'Their AllItemsList rows, in enum order.' $dataLines
Write-Inc 'runes_shipped_data.inc' 'The five v1.7.8 runes'' rows - same ladder, original indices.' $shippedLines
Write-Inc 'runes_curs.inc' 'Their ICURS ids - appended after the uniques, so the CEL frames follow.' $cursLines
Write-Inc 'runes_curs_widths.inc' 'Frame widths, in CEL frame order.' $widthLines
Write-Inc 'runes_curs_heights.inc' 'Frame heights, in CEL frame order.' $heightLines
Write-Inc 'runes_effects.inc' 'The GemData rows for the 28 new runes.' $effectLines

$orderLines = New-Object System.Collections.ArrayList
foreach ($rune in $runes) {
    [void]$orderLines.Add("`tIDI_ORACOOL_RUNE_$($rune.n.ToUpper()),")
}
Write-Inc 'runes_order.inc' 'All 33 in Diablo II order - the ONLY correct successor list, since the enum interleaves the five shipped runes with charms and the whole gem ladder.' $orderLines
# The spec file goes out WITHOUT a BOM: build_item_icons.cmd concatenates it into one spec list with
# `type`, and a BOM landing mid-file makes the first line after it unparseable.
[System.IO.File]::WriteAllLines((Join-Path $out 'runes_icon_specs.txt'), $specLines,
    (New-Object System.Text.UTF8Encoding($false)))

Write-Host ("generated {0} new runes: {1} enum, {2} data, {3} curs, {4} specs, {5} effect rows" -f `
        $enumLines.Count, $enumLines.Count, $dataLines.Count, $cursLines.Count, $specLines.Count, $effectLines.Count)
