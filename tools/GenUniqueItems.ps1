# Generates Source/oracool/unique_items_data.inc from the 250-unique expansion package.
#
# The package (Resources/01-in-use-assets/unique-items/unique-item-expansion-250.zip) is data only -
# names, bases, requirements, drop bands, lore, visual briefs, exact affix tokens. Sprites are
# deliberately outside it and are still to come, which is why nothing here allocates an icon: a
# unique keeps its BASE item's sprite until the art arrives. That also means no CEL frame order to
# get wrong, unlike the item sets.
#
# Identity rides the vanilla UniqueItem machinery. Appending rows to UniqueItems[] gets naming,
# drop-rolling, description and _iUid persistence for free - _iUid is already an int and already
# saved - so 250 new uniques cost no save-format change and no new item field.
#
# Affix tokens are resolved through Source/oracool/unique_affixes.cpp, NOT through the package's own
# `enginePower` column. The package is mostly right and wrong in one place; see that file's header.
# This script reports every disagreement rather than silently preferring one side.
#
#     pwsh -File tools\GenUniqueItems.ps1

param(
    [string]$Package    = "..\Resources\01-in-use-assets\unique-items\unique-item-expansion-250.zip",
    # The art, delivered separately ("i am waiting for the sprites", 2026-08-17, and then they came).
    # 250 native 28px-per-cell PNGs plus sprite-manifest.json, joined to the design by id.
    [string]$SpriteZip  = "..\Resources\01-in-use-assets\unique-items\unique-item-sprites-250.zip",
    [string]$AffixTable = "Source\oracool\unique_affixes.cpp",
    [string]$BaseEnum   = "Source\itemdat.h",
    [string]$OutFile    = "Source\oracool\unique_items_data.inc",
    # The four icon artefacts, same one-walk-one-order discipline as the item sets: a CEL ties a
    # frame to an id by POSITION alone, so the spec list, the enum, and the width/height rows must
    # come out of a single pass or drift silently.
    [string]$IconSpecFile = "Source\oracool\unique_items_icon_specs.txt",
    [string]$CursEnumFile = "Source\oracool\unique_items_curs.inc",
    [string]$CursWidthFile = "Source\oracool\unique_items_curs_widths.inc",
    [string]$CursHeightFile = "Source\oracool\unique_items_curs_heights.inc",
    # One past the set items' last frame: 412 + 94. cursor.cpp static_asserts the adjacency.
    [int]$FirstCursorId = 506
)

$ErrorActionPreference = "Stop"

# --- the package -----------------------------------------------------------------------------------
$work = Join-Path ([System.IO.Path]::GetTempPath()) "oracool-uniques"
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
Expand-Archive -Path $Package -DestinationPath $work -Force
$jsonPath = Get-ChildItem $work -Recurse -Filter "unique-items.json" | Select-Object -First 1
if (-not $jsonPath) { throw "no unique-items.json inside $Package" }
$pkg = Get-Content $jsonPath.FullName -Raw | ConvertFrom-Json
Write-Host "package: $($pkg.items.Count) items"

# --- the sprites -----------------------------------------------------------------------------------
$spriteWork = Join-Path ([System.IO.Path]::GetTempPath()) "oracool-unique-sprites"
if (Test-Path $spriteWork) { Remove-Item -Recurse -Force $spriteWork }
Expand-Archive -Path $SpriteZip -DestinationPath $spriteWork -Force
$manifestPath = Get-ChildItem $spriteWork -Recurse -Filter "sprite-manifest.json" | Select-Object -First 1
if (-not $manifestPath) { throw "no sprite-manifest.json inside $SpriteZip" }
$manifest = Get-Content $manifestPath.FullName -Raw | ConvertFrom-Json
$cellsDir = Get-ChildItem $spriteWork -Recurse -Directory -Filter "native-28px-cells" | Select-Object -First 1
if (-not $cellsDir) { throw "no native-28px-cells directory inside $SpriteZip" }
# id -> { slug, grid }. Joined by id, never by display name - the handoff's own rule 2.
$spriteById = @{}
foreach ($s in $manifest.items) { $spriteById[$s.id] = @{ Slug = $s.spriteSlug; Grid = $s.grid } }
Write-Host "sprites: $($spriteById.Count) manifest records, cells at $($cellsDir.FullName)"

# --- the affix mapping, read out of the C++ table so the two cannot drift --------------------------
$affix = @{}
foreach ($line in Get-Content $AffixTable) {
    if ($line -match '^\s*\{\s*"([a-z_]+)",\s*(Power|Approx|Inert),\s*(IPL_[A-Z0-9_]+)') {
        $affix[$Matches[1]] = @{ Fidelity = $Matches[2]; Power = $Matches[3] }
    }
}
Write-Host "affix table: $($affix.Count) tokens"
if ($affix.Count -lt 40) { throw "only $($affix.Count) tokens parsed from $AffixTable - the row format changed?" }

# --- which UITYPE_ values the engine actually has --------------------------------------------------
$engineBases = @{}
$inEnum = $false
foreach ($line in Get-Content $BaseEnum) {
    if ($line -match '^enum unique_base_item') { $inEnum = $true; continue }
    if ($inEnum) {
        if ($line -match '^\};') { break }
        if ($line -match '^\s*(UITYPE_[A-Z0-9_]+)') { $engineBases[$Matches[1]] = $true }
    }
}
Write-Host "engine bases: $($engineBases.Count) UITYPE_ values"

# --- bases the engine carries under another name (2026-09-11) ------------------------------------
# The package names 107 items on BASE_* tokens this engine never had. No new slots (user: "NO NEW
# ITEM SLOTS. convert to one of existing ones"), new bases allowed ("I am ok with new item bases"): each
# token gets its OWN unique_base_item, carried by ONE new base in an existing slot (itemdat.cpp). Never
# an existing type - a saved unique is rebuilt by re-choosing among its type's uniques, so adding to an
# existing type's list would change items that already exist. These LATE items are emitted AFTER every item that was live before, and their icons form
# a second run at the END of the CEL, so no existing _iUid and no existing ICURS id moves.
$aliases = @{
    BASE_GLOVES = "UITYPE_GLOVES"
    BASE_GAUNTLETS = "UITYPE_GAUNTLETS"
    BASE_BOOTS = "UITYPE_BOOTS"
    BASE_GREAVES = "UITYPE_GREAVES"
    BASE_SASH = "UITYPE_SASH"
    BASE_WAR_BELT = "UITYPE_WARBELT"
    BASE_PAULDRONS = "UITYPE_PAULDRONS"
    BASE_SHOULDER_MANTLE = "UITYPE_MANTLE"
    BASE_CLOAK = "UITYPE_ORCLCLOAK"
    BASE_BATTLE_CLOAK = "UITYPE_BATTLECLOAK"
    BASE_RELIC = "UITYPE_RELIC"
    BASE_RELIQUARY = "UITYPE_RELIQUARY"
    BASE_SPEAR = "UITYPE_SPEAR"
    BASE_PIKE = "UITYPE_PIKE"
    BASE_WAR_LUTE = "UITYPE_WARLUTE"
    BASE_WAR_QUIVER = "UITYPE_WARQUIVER"
    BASE_CANTICLE = "UITYPE_CANTICLE"
    BASE_ARCANE_FOCUS = "UITYPE_ARCANEFOCUS"
}
foreach ($target in $aliases.Values) {
    if (-not $engineBases.ContainsKey($target)) { throw "alias target $target is not a unique_base_item value in $BaseEnum" }
}
$lateRows = @()
$lateIconSpecs = @()
$lateCursEnum = @()
$lateCursWidths = @()
$lateCursHeights = @()

# --- Faster Cast Rate, authored (2026-09-11) --------------------------------------------------------
# User: "add FCR to uniques, sets and runewords too". The package has no cast-rate token at all, so this
# table is where it comes from: caster pieces only - staves, circlets, amulets, rings, arcane foci and
# relics - each with a free power slot. FIXED values, never ranges: Item::_iPLFastCast is not a stored
# field, and the loader re-derives a unique's share from its row (RederiveFastCast in items.cpp), which
# is only exact when the row cannot roll.
$authoredFastCast = @{
    UNIQUE_RAIN_OVER_BLACKSTONE = 30
    UNIQUE_CROWNLESS_VERDICT = 20
    UNIQUE_THE_CROOKED_MERIDIAN = 20
    UNIQUE_THE_PALE_SURVEYOR = 20
    UNIQUE_MOONWAKE_FOCUS = 20
    UNIQUE_THE_UNWRITTEN_GOSPEL = 20
    UNIQUE_SAINT_ORRA_S_LAST_THOUGHT = 15
    UNIQUE_HALO_OF_BROKEN_HOURS = 15
    UNIQUE_THE_FERRYMAN_S_MEMORY = 15
    UNIQUE_RELIQUARY_OF_ONE_BREATH = 15
    UNIQUE_THE_UNLIT_LANTERN = 15
    UNIQUE_STONEWAKE = 10
    UNIQUE_DIADEM_OF_THE_DROWNED_STAR = 10
    UNIQUE_ROOKHEART_PENDANT = 10
    UNIQUE_THE_WIDOW_S_STAR = 10
    UNIQUE_BAND_OF_RED_WINTER = 10
    UNIQUE_VOTIVE_RING = 10
    UNIQUE_THE_HOLLOW_WEDDING = 10
    UNIQUE_LEDGER_OF_LAST_WORDS = 10
    UNIQUE_STONE_OF_QUIET_THUNDER = 10
}
$fastCastUsed = New-Object System.Collections.Generic.HashSet[string]

# --- walk the items --------------------------------------------------------------------------------
$rows = @()
$skippedNoBase = @{}
$iconSpecs = @()
$cursEnum = @()
$cursWidths = @()
$cursHeights = @()
$disagreements = @()
$liveAffixes = 0
$inertAffixes = 0
$mergedAffixes = 0
$emptyItems = @()

foreach ($it in $pkg.items) {
    # A base this engine has no UITYPE_ for cannot be rolled onto anything, so the item is SKIPPED
    # rather than emitted pointing at UITYPE_NONE - which would make it undroppable in a way that
    # looked like a live row. Counted and named at the end instead.
    $late = $aliases.ContainsKey($it.baseToken)
    $baseToken = if ($late) { $aliases[$it.baseToken] } else { $it.baseToken }
    if (-not $engineBases.ContainsKey($baseToken)) {
        if (-not $skippedNoBase.ContainsKey($it.baseToken)) { $skippedNoBase[$it.baseToken] = 0 }
        $skippedNoBase[$it.baseToken]++
        continue
    }

    $powers = @()
    foreach ($a in $it.affixes) {
        if (-not $affix.ContainsKey($a.token)) { throw "item $($it.id): unknown affix token '$($a.token)'" }
        $m = $affix[$a.token]

        # The package's own opinion, kept only as a cross-check. Recorded, never obeyed.
        if ($a.enginePower -and $m.Power -ne $a.enginePower -and $m.Fidelity -ne "Inert") {
            $disagreements += "$($a.token): package says $($a.enginePower), table says $($m.Power)"
        }

        if ($m.Fidelity -eq "Inert") { $inertAffixes++; continue }

        # The elemental powers declare a RANGE ([1,6]); everything else a scalar. ItemPower's two
        # parameters are exactly min and max, so a range needs no reshaping - but a scalar cast over
        # an array is the silent kind of wrong, so the two are separated here rather than coerced.
        $isRange = $a.value -is [System.Array]
        $v = if ($isRange) { [int]$a.value[0] } else { [int]$a.value }
        # items.cpp only raises a steal flag for exactly 3 and 5; every other value falls through
        # both ifs and does nothing at all.
        if ($m.Power -in @("IPL_STEALLIFE", "IPL_STEALMANA") -and $v -notin @(3, 5)) {
            throw "item $($it.id): '$($a.token)' is $v, but only 3 and 5 exist"
        }
        # The two speed powers carry a discrete TIER in param1, not a magnitude. The package already
        # declares 1..3, which is a tier - but clamp anyway, because a future batch declaring "+20%"
        # would otherwise become tier 20 and read as garbage out of SaveItemPower.
        if ($m.Power -eq "IPL_FASTATTACK")  { $v = [Math]::Max(1, [Math]::Min(4, $v)) }
        if ($m.Power -eq "IPL_FASTRECOVER") { $v = [Math]::Max(1, [Math]::Min(3, $v)) }
        # IPL_GETHIT SUBTRACTS its parameter, so a reduction has to arrive positive. No current token
        # maps to it, but the rule belongs with the other packing rules rather than in a future
        # reader's memory.
        if ($m.Power -eq "IPL_GETHIT") { $v = [Math]::Abs($v) }

        # param2 is the range's upper bound, or param1 again for a scalar.
        $p2 = if ($isRange) { [int]$a.value[1] } else { $v }
        $liveAffixes++
        $powers += [pscustomobject]@{ P = $m.Power; V1 = $v; V2 = $p2 }
    }

    # MERGE duplicate additive powers into one slot.
    #
    # 42 items carried IPL_ACP twice (audit, 2026-08-20): the documented flat_armor -> IPL_ACP
    # approximation landing on an item that also has a real enhanced_armor_percent. The two stacked
    # correctly, so nothing was broken - but the popup showed two identical-looking "+N% armor"
    # lines, which reads as a rendering fault rather than as two affixes, and it burned a power slot
    # out of seven for no information.
    #
    # STRICTLY the powers SaveItemPower applies with `+=` on a scalar. Merging anything else would be
    # wrong in a way that is hard to see later: the elemental pair carry a min/max RANGE and
    # OVERWRITE each other, the speed powers carry a discrete tier where 1+1 is not 2, and the steal
    # flags only fire on exactly 3 or 5 - summing two 3s would produce a 6 that silently does
    # nothing. Anything not on this list keeps every one of its slots.
    $additive = @(
        "IPL_ACP", "IPL_DAMP", "IPL_TOHIT", "IPL_DAMMOD", "IPL_LIFE", "IPL_MANA",
        "IPL_STR", "IPL_MAG", "IPL_DEX", "IPL_VIT", "IPL_ATTRIBS",
        "IPL_FIRERES", "IPL_LIGHTRES", "IPL_MAGICRES", "IPL_ALLRES",
        "IPL_LIGHT", "IPL_DUR", "IPL_SPLLVLADD", "IPL_FASTCAST"
    )
    $merged = @()
    foreach ($p in $powers) {
        $prior = if ($additive -contains $p.P) { $merged | Where-Object { $_.P -eq $p.P } | Select-Object -First 1 } else { $null }
        if ($null -ne $prior) {
            $prior.V1 = $prior.V1 + $p.V1
            $prior.V2 = $prior.V1   # scalar: param2 mirrors param1, as the emit path above does
            $mergedAffixes++
        } else {
            $merged += $p
        }
    }
    # The authored cast rate, after the package's own affixes and before the icon.
    if ($authoredFastCast.ContainsKey($it.id)) {
        $fcr = [int]$authoredFastCast[$it.id]
        $merged += [pscustomobject]@{ P = "IPL_FASTCAST"; V1 = $fcr; V2 = $fcr }
        [void]$fastCastUsed.Add($it.id)
        $liveAffixes++
    }
    $powers = @($merged | ForEach-Object { "{ $($_.P), $($_.V1), $($_.V2) }" })

    # A unique that grants nothing is the failure this project already shipped once, in the item-set
    # bonus ladders. It is a build error here, not something to find in play.
    if ($powers.Count -eq 0) { $emptyItems += $it.id; continue }

    # --- the icon: one CEL frame, one ICURS_ id, one width/height row, all in emitted order -------
    # The handoff's own gate: "Missing art is a hard build error, not a silent generic-icon
    # fallback." An emitted unique with no sprite stops the generator.
    if (-not $spriteById.ContainsKey($it.id)) { throw "item $($it.id): no sprite-manifest record" }
    $sprite = $spriteById[$it.id]
    $png = Join-Path $cellsDir.FullName "$($sprite.Slug).png"
    if (-not (Test-Path $png)) { throw "item $($it.id): sprite '$($sprite.Slug).png' missing from native-28px-cells" }
    $gw = [int]$sprite.Grid[0]; $gh = [int]$sprite.Grid[1]
    $cursName = "ICURS_ORACOOL_UNQ_" + ($it.id -replace '^UNIQUE_', '')
    $spec = "$png,0,0,$($gw*28),$($gh*28),$($gw*28),$($gh*28),$($sprite.Slug),30,false,asis"
    if ($late) {
        # The second run: the id is a NAME, numbered by the enum where that run sits (after the charm
        # icons, see itemdat.h), because its frames are appended at the end of the CEL.
        $cursId = $cursName
        $lateIconSpecs += $spec
        $lateCursEnum += "`t$cursName,"
        $lateCursWidths += "`t$gw * 28, // $($sprite.Slug)"
        $lateCursHeights += "`t$gh * 28, // $($sprite.Slug)"
    } else {
        $cursId = $FirstCursorId + $rows.Count
        $iconSpecs += $spec
        $cursEnum += "`t$cursName = $cursId,"
        $cursWidths += "`t$gw * 28, // $($sprite.Slug)"
        $cursHeights += "`t$gh * 28, // $($sprite.Slug)"
    }

    # The icon rides IPL_INVCURS, vanilla's own channel - SaveItemPower does `item._iCurs = param1`,
    # and the inventory footprint follows _iCurs through InvItemWidth3/Height3, so the package's
    # declared grid becomes the item's real footprint with no new field anywhere. Appended LAST so
    # the description loop prints the real affixes first (PrintItemPower renders INVCURS as a lone
    # space, vanilla's own way of making it description-safe).
    $powers += "{ IPL_INVCURS, $cursId, 0 }"

    # SEVEN slots, not six. 26 of the emitted uniques carry six live affixes, and the icon needs a
    # slot of its own - which is why UniqueItem::powers grew to 7 (see itemdat.h).
    if ($powers.Count -gt 7) { throw "item $($it.id) has $($powers.Count) powers, more than the seven slots" }
    $numPl = $powers.Count
    while ($powers.Count -lt 7) { $powers += "{ IPL_INVALID, 0, 0 }" }

    # UIValue is the gold value the vanilla roller assigns. The package has no such field, so it is
    # derived from the drop band rather than invented per item: a late-band unique should not be
    # worth the same as an early one, and deriving it means retuning the bands retunes every price.
    $value = switch ($it.powerBand) { "early" { 6000 } "mid" { 20000 } "late" { 60000 } default { 20000 } }

    $name = $it.name -replace '\\', '\\\\' -replace '"', '\"'
    $row = "`t{ N_(`"$name`"), $baseToken, $([int]$it.dropLevel), $numPl, $value, { $($powers -join ', ') } },"
    if ($late) { $lateRows += $row } else { $rows += $row }
}

# --- report ----------------------------------------------------------------------------------------
$missingFastCast = @($authoredFastCast.Keys | Where-Object { -not $fastCastUsed.Contains($_) })
if ($missingFastCast.Count -gt 0) { throw "authored Faster Cast Rate names items that were not emitted: $($missingFastCast -join ', ')" }
Write-Host "faster cast rate: authored on $($fastCastUsed.Count) uniques"
if ($emptyItems.Count -gt 0) {
    throw "these items compile to NO working affix at all: $($emptyItems -join ', ')"
}
Write-Host "affixes: $liveAffixes live, $inertAffixes inert, $mergedAffixes merged into an existing slot"
if ($disagreements.Count -gt 0) {
    Write-Host "package/table disagreements (table wins, by design):"
    $disagreements | Sort-Object -Unique | ForEach-Object { Write-Host "  $_" }
}
if ($skippedNoBase.Count -gt 0) {
    $total = ($skippedNoBase.Values | Measure-Object -Sum).Sum
    Write-Host "skipped $total items on $($skippedNoBase.Count) bases this engine has no UITYPE_ for:"
    $skippedNoBase.GetEnumerator() | Sort-Object Value -Descending | ForEach-Object { Write-Host "  $($_.Key) x$($_.Value)" }
}

$out = @()
$out += "// GENERATED by tools/GenUniqueItems.ps1 - do not edit."
$out += "//"
$out += "// Source: Resources/01-in-use-assets/unique-items/unique-item-expansion-250.zip."
$out += "// Affix meanings come from oracool/unique_affixes.cpp, NOT the package's enginePower column."
$out += "//"
$out += "// $($rows.Count + $lateRows.Count) of the package's $($pkg.items.Count) items: $($rows.Count) on bases the engine always had, then"
$out += "// $($lateRows.Count) LATE ones on bases mapped through the generator's alias table - appended after the"
$out += "// others so no existing _iUid moves. Each carries its own icon through IPL_INVCURS."
$out += ""
$out += "// clang-format off"
$out += $rows
if ($lateRows.Count -gt 0) {
    $out += "`t// The late uniques (2026-09-11), on bases mapped through the alias table. APPEND-ONLY from here."
    $out += $lateRows
}
$out += "// clang-format on"

Set-Content -Path $OutFile -Value $out -Encoding utf8
Write-Host "wrote $OutFile : $($rows.Count) uniques + $($lateRows.Count) late"

# --- the four icon artefacts -----------------------------------------------------------------------
$header = "// GENERATED by tools/GenUniqueItems.ps1 - do not edit. Frame order is the contract."
# NO BOM on the spec list: build_item_icons.cmd appends it with `type`, and a BOM landing mid-file
# becomes the first characters of a path - the System.Drawing NotSupportedException lesson the set
# icons already paid for.
[System.IO.File]::WriteAllLines((Join-Path (Get-Location).Path $IconSpecFile), [string[]]$iconSpecs,
    (New-Object System.Text.UTF8Encoding($false)))
# FIRST/LAST aliases bracket the run so itemdat.h and cursor.cpp can assert adjacency with the set
# icons and size their tables without naming any specific unique.
$cursEnum = @("`tICURS_ORACOOL_UNQ_FIRST = $FirstCursorId,") + $cursEnum +
    @("`tICURS_ORACOOL_UNQ_LAST = $($FirstCursorId + $iconSpecs.Count - 1),")
Set-Content -Path $CursEnumFile   -Value (@($header) + $cursEnum) -Encoding utf8
Set-Content -Path $CursWidthFile  -Value (@($header) + $cursWidths) -Encoding utf8
Set-Content -Path $CursHeightFile -Value (@($header) + $cursHeights) -Encoding utf8
Write-Host "icons: $($iconSpecs.Count) frames, cursor ids $FirstCursorId..$($FirstCursorId + $iconSpecs.Count - 1)"

# --- the second icon run: the late uniques, appended at the END of the CEL -------------------------
# Numbered by the enum where unique_items2_curs.inc sits (after the charm icons), never by a number
# here - the run's place in the CEL is decided by build_item_icons.cmd's order, and the ids follow it.
if ($lateIconSpecs.Count -gt 0) {
    [System.IO.File]::WriteAllLines((Join-Path (Get-Location).Path "Source\oracool\unique_items2_icon_specs.txt"),
        [string[]]$lateIconSpecs, (New-Object System.Text.UTF8Encoding($false)))
    $firstLate = ($lateCursEnum[0] -replace '[\t,]', '')
    $lastLate = ($lateCursEnum[-1] -replace '[\t,]', '')
    $lateHeader = "// GENERATED by tools/GenUniqueItems.ps1 - do not edit. Frame order is the contract (the late run)."
    $lateEnum = $lateCursEnum + @("`tICURS_ORACOOL_UNQ2_FIRST = $firstLate,", "`tICURS_ORACOOL_UNQ2_LAST = $lastLate,")
    Set-Content -Path "Source\oracool\unique_items2_curs.inc" -Value (@($lateHeader) + $lateEnum) -Encoding utf8
    Set-Content -Path "Source\oracool\unique_items2_curs_widths.inc" -Value (@($lateHeader) + $lateCursWidths) -Encoding utf8
    Set-Content -Path "Source\oracool\unique_items2_curs_heights.inc" -Value (@($lateHeader) + $lateCursHeights) -Encoding utf8
    Write-Host "late icons: $($lateIconSpecs.Count) frames, $firstLate .. $lastLate"
}
