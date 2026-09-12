# BuildWiki.ps1 - generates the Diablo Orcl wiki from the game's own source data.
#
# Run it after any change to the data tables and the wiki is current again:
#
#     powershell -ExecutionPolicy Bypass -File tools\BuildWiki.ps1
#     powershell -ExecutionPolicy Bypass -File tools\BuildWiki.ps1 -NoBundle
#
# Everything the wiki states about items, spells, skills, monsters and the loot mechanics is PARSED
# from Source/, never re-typed here. That is the whole design: a hand-written wiki is out of date the
# first time someone edits a table, and a wiki nobody trusts is worse than none. Prose that cannot be
# derived (what a mechanic is FOR, why a rule exists) lives in this script beside the data it
# annotates, so it travels with the thing it describes.

param(
    # Skip the single-file bundle. See the note beside the call at the end of this script.
    [switch] $NoBundle
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$src = Join-Path $root 'Source'
$out = Join-Path $root 'wiki'
# The documentation vault. It lives INSIDE the repo as .ProjectDocumentation; the two readers below
# (dev reports, backlog pipeline) looked for it beside the repo under the project's old folder name,
# so both silently found nothing and the wiki shipped an empty history table and an empty pipeline
# for as long as that was true. Named once here so a move breaks one line, and checked out loud.
$vault = Join-Path $root '.ProjectDocumentation'
if (-not (Test-Path $vault)) {
    Write-Warning "documentation vault not found at $vault - history and pipeline pages will be empty"
}

function Read-SourceFile([string]$relative) {
    return Get-Content (Join-Path $src $relative) -Raw -Encoding UTF8
}

# ---------------------------------------------------------------------------------------------
# Version and build identity
# ---------------------------------------------------------------------------------------------
$version = (Get-Content (Join-Path $root 'ORACOOL_VERSION') -Raw).Trim()
$generated = (Get-Date).ToString('yyyy-MM-dd HH:mm')

# ---------------------------------------------------------------------------------------------
# Base items - Source/itemdat.cpp
# ---------------------------------------------------------------------------------------------
$itemdat = Read-SourceFile 'itemdat.cpp'
# Generated rows live in includes rather than inline, so they are concatenated here - otherwise the
# wiki's item count silently drops by whatever moved out and never gains what arrived.
#
# The include list is READ OUT OF itemdat.cpp rather than typed here, and that is the whole point of
# this block. It was a hardcoded pair - the two rune includes - and by 1.9.9 itemdat.cpp had gained
# three more: the seven salvage materials, the seven Charms of Salvaging and the fifteen jewels.
# Twenty-nine items existed in the game and not in the wiki, and nothing said so, because a wiki
# that is missing a row looks exactly like a wiki that is complete. The comment that used to sit
# here warned about this exact failure and was then out-of-date itself for three families running.
#
# unique_items_data.inc is excluded deliberately: it fills UniqueItems[], not AllItemsList, and has
# its own parser further down. Everything else that itemdat.cpp includes is a base-item table by
# construction, and the row parser below gates on IDROP_ and a 22-column shape anyway, so a file
# that turns out not to be one contributes nothing rather than garbage.
$itemIncludes = [regex]::Matches($itemdat, '#include\s+"(oracool/[A-Za-z0-9_]+\.inc)"') |
    ForEach-Object { $_.Groups[1].Value } |
    Where-Object { $_ -ne 'oracool/unique_items_data.inc' }
foreach ($inc in $itemIncludes) {
    $incPath = Join-Path $src ($inc -replace '/', '\')
    if (-not (Test-Path $incPath)) { continue }
    $itemdat += "`n" + (Get-Content $incPath -Raw -Encoding UTF8)
}
$items = New-Object System.Collections.ArrayList

foreach ($line in ($itemdat -split "`n")) {
    if ($line -notmatch '^\s*/\*([^*]*)\*/\s*\{\s*(IDROP_\w+),') { continue }
    $enumName = $matches[1].Trim()
    $body = $line.Substring($line.IndexOf('{') + 1)
    $body = $body.Substring(0, $body.LastIndexOf('}'))

    # The two name columns are the only fields that can hold a comma-free quoted string; pull them
    # out first so the naive comma split below cannot trip over their contents.
    $name = ''
    if ($body -match 'N_\("([^"]*)"\)') { $name = $matches[1] }

    $parts = $body -split ','
    if ($parts.Count -lt 22) { continue } # 22 columns in ItemData; fewer means a row we cannot trust
    for ($i = 0; $i -lt $parts.Count; $i++) { $parts[$i] = $parts[$i].Trim() }

    $item = [ordered]@{
        id       = $enumName
        drop     = $parts[0] -replace 'IDROP_', ''
        class    = $parts[1] -replace 'ICLASS_', ''
        loc      = $parts[2] -replace 'ILOC_', ''
        curs     = $parts[3] -replace 'ICURS_', ''
        type     = $parts[4] -replace 'ItemType::', ''
        uitype   = $parts[5] -replace 'UITYPE_', ''
        name     = $name
        qlvl     = [int]$parts[8]
        dur      = [int]$parts[9]
        minDam   = [int]$parts[10]
        maxDam   = [int]$parts[11]
        minAC    = [int]$parts[12]
        maxAC    = [int]$parts[13]
        reqStr   = [int]$parts[14]
        reqMag   = [int]$parts[15]
        reqDex   = [int]$parts[16]
        misc     = ($parts[18] -replace 'IMISC_', '')
        spell    = ($parts[19] -replace 'SpellID::', '')
        usable   = ($parts[20] -eq 'true')
        value    = [int]($parts[21])
    }
    [void]$items.Add($item)
}

# ---------------------------------------------------------------------------------------------
# Spells - Source/spelldat.cpp, banded by Source/oracool/spell_ranks.cpp
# ---------------------------------------------------------------------------------------------
$spelldat = Read-SourceFile 'spelldat.cpp'
$spells = New-Object System.Collections.ArrayList
foreach ($line in ($spelldat -split "`n")) {
    if ($line -notmatch '^/\*SpellID::(\w+)\*/\s*\{') { continue }
    $id = $matches[1]
    $name = ''
    if ($line -match 'P_\("spell",\s*"([^"]*)"\)') { $name = $matches[1] }
    $body = $line.Substring($line.IndexOf('{') + 1)
    # Collapse P_("spell", "Name"): its comma would throw every positional column below off by one.
    # Strip the missile sub-array so the comma split lines up with the flat columns.
    $body = $body -replace 'P_\("[^"]*",\s*"[^"]*"\)', 'NAME'
    $body = $body -replace '\{[^{}]*\}', 'MISSILES'
    $parts = $body -split ','
    for ($i = 0; $i -lt $parts.Count; $i++) { $parts[$i] = $parts[$i].Trim() }
    if ($parts.Count -lt 14) { continue }
    $missiles = ''
    # The trailing comma inside the pair is real - "{ MissileID::Firebolt, MissileID::Null, }" - and
    # the first version of this pattern did not allow for it, so every spell showed no missiles.
    if ($line -match '\{\s*(MissileID::\w+),\s*(MissileID::\w+),?\s*\}') {
        $missiles = ($matches[1] -replace 'MissileID::', '') + ' / ' + ($matches[2] -replace 'MissileID::', '')
    }
    [void]$spells.Add([ordered]@{
            id       = $id
            name     = $name
            mana     = [int]$parts[4]
            type     = ($parts[5] -replace 'SpellDataFlags::', '')
            bookLvl  = [int]$parts[6]
            staffLvl = [int]$parts[7]
            minInt   = [int]$parts[8]
            missiles = $missiles
            manaAdj  = [int]$parts[10]
            minMana  = [int]$parts[11]
            bookCost = ([int]$parts[2]) * 10
        })
}

# The band table, read from the module that owns it rather than restated.
$ranks = Read-SourceFile 'oracool/spell_ranks.cpp'
$bandById = @{}
foreach ($line in ($ranks -split "`n")) {
    if ($line -match '^\s*(\d+),\s*//\s*(.+?)\s*$') {
        $bandById[$matches[2].Trim()] = [int]$matches[1]
    }
}
foreach ($s in $spells) {
    $band = 0
    foreach ($key in $bandById.Keys) {
        if ($key -eq $s.name -or $key -replace ' ', '' -eq $s.id) { $band = $bandById[$key]; break }
    }
    $s['band'] = $band
}

# Band 0 covers three different things, and calling them all "not a book spell" would be a half-truth.
# The class skills are RETIRED - they exist in the enum and are granted to nobody - while the Paladin
# skills are earned by character level and Town Portal is a belt ability. The reader needs the
# difference, so it is recorded here rather than guessed at in the page.
$retired = @('ItemRepair', 'TrapDisarm', 'StaffRecharge', 'Search', 'Identify', 'Rage')
$paladinSkills = @('Charge', 'Zeal', 'HammerOfFaith', 'BlessedShield', 'FistOfTheHeavens', 'ShieldBash', 'BlessedHammer')
foreach ($s in $spells) {
    if ($retired -contains $s.id) { $s['role'] = 'retired' }
    elseif ($paladinSkills -contains $s.id) { $s['role'] = 'class skill' }
    elseif ($s.id -eq 'TownPortal') { $s['role'] = 'belt ability' }
    elseif ($s.band -gt 0) { $s['role'] = 'book spell' }
    else { $s['role'] = 'unused' }
}

# ---------------------------------------------------------------------------------------------
# Class-tree skills - Source/oracool/class_tree.cpp
# ---------------------------------------------------------------------------------------------
$tree = Read-SourceFile 'oracool/class_tree.cpp'
$skills = New-Object System.Collections.ArrayList
$classMap = @{ 'Pal' = 'Paladin'; 'Bar' = 'Barbarian'; 'Sor' = 'Sorcerer'; 'Rog' = 'Rogue'; 'Bar_' = 'Barbarian'; 'Brd' = 'Bard'; 'Mnk' = 'Monk' }
$treeBody = $tree.Substring($tree.IndexOf('const ClassTreeSkillData Skills['))
# The default for a row that declares no maxRank is the game's own per-skill cap, read here rather
# than typed: it was hardcoded to 98 and stayed there when the cap was lowered to 30, so every skill
# in the wiki claimed a ceiling three times the real one. Get-Constant is defined further down the
# file, so this reads the constant directly.
$pointsHEarly = Read-SourceFile 'oracool/skill_points.h'
$defaultMaxRank = $(if ($pointsHEarly -match 'constexpr int MaxSkillInvestment = (\d+)') { [int]$matches[1] } else { 30 })

# The trailing maxRank is OPTIONAL and that is the table's own convention, not sloppiness: 142 of
# the 273 rows omit it, which aggregate initialisation leaves at 0, and ClassTreeMaxRank reads 0 as
# "the usual cap" (MaxTreeInvestment). So the group has to be optional here or those 142 rows stop
# matching entirely and vanish from the wiki.
$rowPattern = '\{\s*N_\("([^"]*)"\),\s*N_\("([^"]*)"\),\s*(\w+),\s*(\d+),\s*(\d+),\s*(\d+),\s*Kind::(\w+),\s*SpellID::(\w+),\s*(true|false)(?:\s*,\s*(\d+))?\s*\}'
foreach ($m in [regex]::Matches($treeBody, $rowPattern)) {
    $cls = $m.Groups[3].Value
    $className = $cls
    if ($classMap.ContainsKey($cls)) { $className = $classMap[$cls] }
    [void]$skills.Add([ordered]@{
            name        = $m.Groups[1].Value
            description = $m.Groups[2].Value
            class       = $className
            page        = [int]$m.Groups[4].Value
            tier        = [int]$m.Groups[5].Value
            column      = [int]$m.Groups[6].Value
            kind        = $m.Groups[7].Value
            spell       = $m.Groups[8].Value
            implemented = ($m.Groups[9].Value -eq 'true')
            # The CAP, already resolved - a declared 0 means MaxTreeInvestment, so readers never
            # have to know that convention. 1 marks the Passive Skills rows, which take no points
            # at all and are chosen by slot; 5 is the Monk's.
            maxRank     = $(if ($m.Groups[10].Success -and [int]$m.Groups[10].Value -gt 0) { [int]$m.Groups[10].Value } else { $defaultMaxRank })
        })
}

# ---------------------------------------------------------------------------------------------
# Monsters - Source/monstdat.cpp
# ---------------------------------------------------------------------------------------------
$mon = Read-SourceFile 'monstdat.cpp'
$monsters = New-Object System.Collections.ArrayList
$monBody = $mon.Substring($mon.IndexOf('const MonsterData MonstersData[]'))
$monBody = $monBody.Substring(0, $monBody.IndexOf('const UniqueMonsterData'))
foreach ($line in ($monBody -split "`n")) {
    if ($line -notmatch '^/\*\s*(MT_\w+)\s*\*/\s*\{') { continue }
    $id = $matches[1]
    $name = ''
    if ($line -match 'P_\("monster",\s*"([^"]*)"\)') { $name = $matches[1] }
    # Same two collapses as the spell table, for the same positional reason.
    $flat = $line -replace 'P_\("[^"]*",\s*"[^"]*"\)', 'NAME'
    $flat = $flat -replace '\{[^{}]*\}', 'ARR'
    $parts = $flat.Substring($flat.IndexOf('{') + 1) -split ','
    for ($i = 0; $i -lt $parts.Count; $i++) { $parts[$i] = $parts[$i].Trim() }
    if ($parts.Count -lt 30) { continue }
    [void]$monsters.Add([ordered]@{
            id       = $id
            name     = $name
            minLvl   = [int]$parts[11]
            maxLvl   = [int]$parts[12]
            level    = [int]$parts[13]
            hpMin    = [int]$parts[14]
            hpMax    = [int]$parts[15]
            toHit    = [int]$parts[19]
            minDam   = [int]$parts[21]
            maxDam   = [int]$parts[22]
            ac       = [int]$parts[27]
            class    = ($parts[28] -replace 'MonsterClass::', '')
            # The sprite family. Several monsters share one set of artwork and differ only by their
            # palette translation - all four zombies are "zombie\zombie" - so this is the key the
            # encyclopedia's picture is filed under, not a per-monster name.
            art      = ($parts[1].Trim().Trim('"') -replace '\\\\', '_')
            # Read past the end defensively: the guard above only promises 30 columns, and a row that
            # stops short must lose a field rather than take the whole monster off the page.
            toHitSp  = [int]$(if ($parts.Count -gt 23) { $parts[23] } else { 0 })
            minDamSp = [int]$(if ($parts.Count -gt 25) { $parts[25] } else { 0 })
            maxDamSp = [int]$(if ($parts.Count -gt 26) { $parts[26] } else { 0 })
            # Left as the source's own flag expression ("IMMUNE_MAGIC | RESIST_FIRE"). The page needs
            # both, because Nightmare demotes the Hell immunities and Torment promotes them.
            resist     = $(if ($parts.Count -gt 29) { $parts[29].Trim() } else { '' })
            resistHell = $(if ($parts.Count -gt 30) { $parts[30].Trim() } else { '' })
            # The last column, taken by regex rather than by index. The split leaves the closing brace
            # on the final element ("54 }"), and casting that to int silently produced 0 - a whole
            # column of zeroes on the monsters page that looked entirely plausible until it was read.
            exp      = [int]$(if ($line -match ',\s*(\d+)\s*\},?\s*$') { $matches[1] } else { 0 })
        })
}

# ---------------------------------------------------------------------------------------------
# Character classes and the experience ladder - Source/playerdat.cpp
# ---------------------------------------------------------------------------------------------

# The life and mana columns are in the engine's 1/64 fixed point, written either as an explicit
# "(18 << 6)" or as "static_cast<int>(5.5F * 64)". Both are converted to whole points here, which is
# the unit a player counts in and the unit the character sheet shows.
function ConvertFixed([string]$raw) {
    $text = $raw.Trim()
    if ($text -match '^\(?\s*(-?\d+)\s*<<\s*6\s*\)?$') { return [double]$matches[1] }
    if ($text -match '^([0-9.]+)x64$') { return [double]$matches[1] }
    if ($text -match '^\(?\s*(-?\d+)\s*\)?$') { return [double]$matches[1] / 64.0 }
    return 0
}

$playerdat = Read-SourceFile 'playerdat.cpp'
$classes = New-Object System.Collections.ArrayList
# Bounded to the PlayersData table. The sprite table below it repeats the same "/* HeroClass::X */"
# comment on every row, so an unbounded walk found each class twice - twelve classes for six.
$classBody = $playerdat.Substring($playerdat.IndexOf('const PlayerData PlayersData[]'))
$classBody = $classBody.Substring(0, $classBody.IndexOf('};'))
foreach ($line in ($classBody -split "`n")) {
    if ($line -notmatch '^/\* HeroClass::(\w+)\s*\*/\s*\{') { continue }
    $enum = $matches[1]
    $name = ''
    if ($line -match 'N_\("([^"]*)"\)') { $name = $matches[1] }
    $flat = $line -replace 'N_\("[^"]*"\)', 'NAME'
    $flat = $flat -replace 'static_cast<int>\(([0-9.]+)F \* 64\)', '$1x64'
    $parts = $flat.Substring($flat.IndexOf('{') + 1) -split ','
    for ($i = 0; $i -lt $parts.Count; $i++) { $parts[$i] = $parts[$i].Trim() }
    if ($parts.Count -lt 20) { continue }

    [void]$classes.Add([ordered]@{
            enum      = $enum
            name      = $name
            # Positional from className: 0 name, 1 sprite path, 2 str, 3 mag, 4 dex, 5 vit,
            # 6-9 the maxima, 10 block bonus, 11-18 the life and mana curves, 19 the class skill.
            baseStr   = [int]$parts[2]
            baseMag   = [int]$parts[3]
            baseDex   = [int]$parts[4]
            baseVit   = [int]$parts[5]
            maxStr    = [int]$parts[6]
            maxMag    = [int]$parts[7]
            maxDex    = [int]$parts[8]
            maxVit    = [int]$parts[9]
            blockBonus = [int]$parts[10]
            startLife = ConvertFixed $parts[11]
            startMana = ConvertFixed $parts[12]
            lifePerLevel = ConvertFixed $parts[13]
            manaPerLevel = ConvertFixed $parts[14]
            lifePerVit   = ConvertFixed $parts[15]
            manaPerMag   = ConvertFixed $parts[16]
            skill     = ($parts[19] -replace 'SpellID::', '' -replace '\s*\}.*$', '')
        })
}

$expTable = New-Object System.Collections.ArrayList
$expBody = $playerdat.Substring($playerdat.IndexOf('const uint64_t ExpLvlsTbl'))
$expBody = $expBody.Substring($expBody.IndexOf('{') + 1)
$expBody = $expBody.Substring(0, $expBody.IndexOf('};'))
$level = 1
foreach ($m in [regex]::Matches($expBody, '(\d[\d]*)')) {
    [void]$expTable.Add([ordered]@{ level = $level; toNext = [int64]$m.Groups[1].Value })
    $level++
}

# ---------------------------------------------------------------------------------------------
# Affixes - Source/itemdat.cpp's prefix and suffix tables
# ---------------------------------------------------------------------------------------------
$affixes = New-Object System.Collections.ArrayList
foreach ($table in @('ItemPrefixes', 'ItemSuffixes')) {
    $kind = if ($table -eq 'ItemPrefixes') { 'prefix' } else { 'suffix' }
    $body = $itemdat.Substring($itemdat.IndexOf("const PLStruct $table[]"))
    $body = $body.Substring(0, $body.IndexOf('};'))
    # One row per line, read positionally after the two brace groups. A single regex over the whole
    # row was tried first and could not be trusted: PLIType is an OR-chain of AffixItemType flags
    # padded with runs of spaces, so a lazy match ran past the field and a greedy one swallowed the
    # next row. Splitting the line at its known landmarks is duller and correct.
    foreach ($line in ($body -split "`n")) {
        if ($line -notmatch '^\{\s*N_\("([^"]*)"\),\s*\{\s*(IPL_\w+),\s*(-?\d+),\s*(-?\d+)\s*\},\s*(-?\d+),\s*(.+)$') { continue }
        # Copied out BEFORE the second -match runs. $matches is a single automatic variable that each
        # match overwrites, so reading the first pattern's groups afterwards silently returns the
        # second pattern's - which is how "false" ended up being cast to an int here.
        $affixName = $matches[1]
        $affixPower = $matches[2] -replace '^IPL_', ''
        $affixMin = [int]$matches[3]
        $affixMax = [int]$matches[4]
        $affixMinLvl = [int]$matches[5]
        $tail = $matches[6]
        $types = ($tail -split ',\s*GOE_')[0]

        $align = ''; $double = $false; $good = $false; $minVal = 0; $maxVal = 0; $mult = 0
        if ($tail -match 'GOE_(\w+),\s*(true|false),\s*(true|false),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)') {
            $align = $matches[1]
            $double = ($matches[2] -eq 'true')
            $good = ($matches[3] -eq 'true')
            $minVal = [int]$matches[4]
            $maxVal = [int]$matches[5]
            $mult = [int]$matches[6]
        }

        [void]$affixes.Add([ordered]@{
                kind   = $kind
                name   = $affixName
                power  = $affixPower
                min    = $affixMin
                max    = $affixMax
                minLvl = $affixMinLvl
                types  = (($types -replace 'AffixItemType::', '') -replace '\s+', ' ').Trim()
                align  = $align
                double = $double
                good   = $good
                minVal = $minVal
                maxVal = $maxVal
                mult   = $mult
            })
    }
}

# ---------------------------------------------------------------------------------------------
# Unique items - the vanilla table AND the generated include
#
# Only the include was read until now, so the page showed 250 of the game's 360 uniques and not one
# of Diablo's own - no Harlequin Crest, no Windforce, no Undead Crown. Both tables share a row shape,
# so one parser reads both, in the order the compiler sees them: UniqueItems[] in itemdat.cpp, then
# the include spliced in at its own position.
# ---------------------------------------------------------------------------------------------
$uniques = New-Object System.Collections.ArrayList
function Add-UniqueRows([string]$text, [string]$origin) {
    foreach ($line in ($text -split "`n")) {
        if ($line -notmatch '\{\s*N_\("([^"]*)"\),\s*(UITYPE_\w+),\s*(\d+),\s*(\d+),\s*(\d+),') { continue }
        # Captured before the inner matches run, so nothing downstream can overwrite the row's own
        # groups - the trap that ate a level range earlier in this project.
        $row = @($matches[1], $matches[2], $matches[3], $matches[4], $matches[5])
        $powers = New-Object System.Collections.ArrayList
        # A power carries two values, one, or none: IPL_RNDSTEALLIFE has no numbers and IPL_INVCURS
        # a single frame id. Requiring two silently dropped every such power from the vanilla rows.
        foreach ($p in [regex]::Matches($line, '\{\s*(IPL_\w+)(?:\s*,\s*(-?\d+))?(?:\s*,\s*(-?\d+))?\s*\}')) {
            $type = $p.Groups[1].Value
            if ($type -eq 'IPL_INVALID' -or $type -eq 'IPL_INVCURS') { continue }
            $label = $type -replace '^IPL_', ''
            if ($p.Groups[2].Success) { $label += ' ' + $p.Groups[2].Value }
            [void]$powers.Add($label)
        }
        [void]$uniques.Add([ordered]@{
                name    = $row[0]
                base    = ($row[1] -replace '^UITYPE_', '')
                minLvl  = [int]$row[2]
                powerCount = [int]$row[3]
                value   = [int]$row[4]
                origin  = $origin
                powers  = ($powers -join ', ')
            })
    }
}

$itemdatForUniques = Read-SourceFile 'itemdat.cpp'
$uniqStart = $itemdatForUniques.IndexOf('const UniqueItem UniqueItems[]')
$uniqEnd = $itemdatForUniques.IndexOf('unique_items_data.inc')
if ($uniqStart -ge 0 -and $uniqEnd -gt $uniqStart) {
    Add-UniqueRows $itemdatForUniques.Substring($uniqStart, $uniqEnd - $uniqStart) 'Vanilla'
}
$uniquePath = Join-Path $src 'oracool/unique_items_data.inc'
if (Test-Path $uniquePath) {
    Add-UniqueRows (Get-Content $uniquePath -Raw -Encoding UTF8) 'Oracool'
}

# ---------------------------------------------------------------------------------------------
# Item sets - the generated include
# ---------------------------------------------------------------------------------------------
$setItems = New-Object System.Collections.ArrayList
$setDefs = New-Object System.Collections.ArrayList
$setsPath = Join-Path $src 'oracool/item_sets_data.inc'
$pieceIndex = 0
if (Test-Path $setsPath) {
    foreach ($line in (Get-Content $setsPath -Encoding UTF8)) {
        # ItemSets[]: id, display name, required level, first piece, piece count, first bonus, bonus
        # count. This is the table that names the FIFTEEN sets; the piece rows below only carry their
        # own SET_ id, which is why deriving a set name from a piece produced 94 one-piece "sets".
        if ($line -match '\{\s*"(SET_\w+)",\s*N_\("([^"]*)"\),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\}') {
            [void]$setDefs.Add([ordered]@{
                    name   = $matches[2]
                    reqLvl = [int]$matches[3]
                    first  = [int]$matches[4]
                    count  = [int]$matches[5]
                })
            continue
        }
        if ($line -notmatch '\{\s*"(SET_\w+)",\s*N_\("([^"]*)"\),\s*"(\w+)",\s*"(\w+)",\s*(\d+),\s*(\d+),\s*(\d+),') { continue }
        $powers = New-Object System.Collections.ArrayList
        foreach ($p in [regex]::Matches($line, '\{\s*(IPL_\w+),\s*(-?\d+),\s*(-?\d+)\s*\}')) {
            if ($p.Groups[1].Value -eq 'IPL_INVALID') { continue }
            [void]$powers.Add(($p.Groups[1].Value -replace '^IPL_', '') + ' ' + $p.Groups[2].Value)
        }
        # Two authored slots have no equipment slot of their own and are worn elsewhere, which the
        # sets page states in prose - so the data has to agree with it.
        $slot = $matches[3]
        if ($slot -match '^relic$') { $slot = 'bracers' }
        if ($slot -match '^cloak$') { $slot = 'legs' }
        [void]$setItems.Add([ordered]@{
                set     = ''            # filled in below, from the set that owns this index
                index   = $pieceIndex
                name    = $matches[2]
                slot    = $slot
                base    = ($matches[4] -replace '_', ' ')
                qlvl    = [int]$matches[5]
                ac      = [int]$matches[6]
                dur     = [int]$matches[7]
                powers  = ($powers -join ', ')
            })
        $pieceIndex++
    }
}

# Assign every piece to its owning set by index range, exactly as item_sets.cpp's FindItemSetOwning
# does. Deriving the name from the piece's own SET_ id instead made each piece its own set, so the
# page counted 94 sets and drew 94 one-piece cards where the game has 15.
foreach ($piece in $setItems) {
    foreach ($def in $setDefs) {
        if ($piece['index'] -ge $def.first -and $piece['index'] -lt ($def.first + $def.count)) {
            $piece['set'] = $def.name
            break
        }
    }
    if (-not $piece['set']) { $piece['set'] = 'Unassigned' }
}

# ---------------------------------------------------------------------------------------------
# Quests and shrines
# ---------------------------------------------------------------------------------------------
$questsCpp = Read-SourceFile 'quests.cpp'
$quests = New-Object System.Collections.ArrayList
$questBody = $questsCpp.Substring($questsCpp.IndexOf('QuestData QuestsData[]'))
$questBody = $questBody.Substring(0, $questBody.IndexOf('};'))
foreach ($m in [regex]::Matches($questBody, '\{\s*(-?\d+),\s*(-?\d+),\s*(\w+),\s*(-?\d+),\s*(\d+),\s*(\w+),\s*(true|false),\s*(\w+),\s*N_\(\s*(?:/\*[^*]*\*/\s*)?"([^"]*)"\s*\)')) {
    [void]$quests.Add([ordered]@{
            level      = [int]$m.Groups[1].Value
            levelType  = ($m.Groups[3].Value -replace 'DTYPE_', '')
            setLevel   = ($m.Groups[6].Value -replace 'SL_', '')
            singlePlayer = ($m.Groups[7].Value -eq 'true')
            name       = $m.Groups[9].Value
        })
}

$objectsCpp = Read-SourceFile 'objects.cpp'
$shrines = New-Object System.Collections.ArrayList
if ($objectsCpp -match '(?s)const char \*const ShrineNames\[\] = \{(.*?)\};') {
    $shrineBody = $matches[1]
    $shrineIndex = 0
    foreach ($m in [regex]::Matches($shrineBody, 'N_\("([^"]*)"\)')) {
        [void]$shrines.Add([ordered]@{ index = $shrineIndex; name = $m.Groups[1].Value })
        $shrineIndex++
    }
}

# ---------------------------------------------------------------------------------------------
# Mechanics constants, read from the modules that define them
# ---------------------------------------------------------------------------------------------
function Get-Constant([string]$text, [string]$pattern) {
    if ($text -match $pattern) { return $matches[1] }
    return '?'
}

$tiersCpp = Read-SourceFile 'oracool/item_tiers.cpp'
$areaH = Read-SourceFile 'oracool/area_level.h'
$ranksH = Read-SourceFile 'oracool/spell_ranks.h'
$pointsH = Read-SourceFile 'oracool/skill_points.h'
$playerH = Read-SourceFile 'player.h'
$resistH = Read-SourceFile 'oracool/player_resistance.h'

$mechanics = [ordered]@{
    # Sixteen rungs a difficulty since 2026-09-12: four areas of four floors, with the Hive and the
    # Crypt sharing the Caves' and Hell's rungs rather than adding eight more. Derived from the
    # header rather than typed, which is this generator's whole rule - it was a hardcoded 96.
    maxAreaLevel      = [int](Get-Constant $areaH 'constexpr int FloorsPerArea = (\d+)') * 16
    floorsPerArea     = [int](Get-Constant $areaH 'constexpr int FloorsPerArea = (\d+)')
    areaCount         = [int](Get-Constant $areaH 'constexpr int AreaCount = (\d+)')
    areaFloorCount    = [int](Get-Constant $areaH 'constexpr int AreaFloorCount = (\d+)')
    maxSpellLevel     = [int](Get-Constant $playerH 'constexpr uint8_t MaxSpellLevel = (\d+)')
    maxCharacterLevel = [int](Get-Constant $playerH 'constexpr int MaxCharacterLevel = (\d+)')
    maxInvestment     = [int](Get-Constant $pointsH 'constexpr int MaxSkillInvestment = (\d+)')
    spellBands        = @(1, 6, 12, 18, 24, 30)
    tierScales        = @()
    tierWeights       = @()
}

# The resistance curve, DERIVED rather than typed - the wiki page states these numbers, and the
# standing complaint about this wiki is hand-typed values that drift from the source. Changing the
# constants in player_resistance.h is enough; nothing here needs editing with them.
$mechanics.resistance = [ordered]@{
    softCap   = [int](Get-Constant $resistH 'constexpr int ResistanceHardCap = (\d+)')
    hardCap   = [int](Get-Constant $resistH 'constexpr int ResistanceHardCap = (\d+)')
    divisor   = [int](Get-Constant $resistH 'constexpr int ResistanceSoftCapDivisor = (\d+)')
    penalties = @()
}
# The soft cap is written as `= MaxResistance`, so it comes from player.h rather than from its own
# literal - following the alias instead of re-typing 75 here.
$mechanics.resistance.softCap = [int](Get-Constant $playerH 'constexpr int MaxResistance = (\d+)')
if ($resistH -match 'ResistancePenaltyPerDifficulty\[\]\s*=\s*\{([^}]*)\}') {
    $mechanics.resistance.penalties = @($matches[1] -split ',' | ForEach-Object { [int]$_.Trim() })
}

# ---------------------------------------------------------------------------------------------
# What each skill level actually BUYS - rank 1 to the cap (user, 2026-08-31)
# ---------------------------------------------------------------------------------------------
#
# Three formulas govern every rank in the game, and all three are PARSED here rather than re-typed,
# exactly like the resistance curve above. Re-implementing them in this script is the one thing that
# would make this table worse than useless: it would be a second copy of the game's maths, free to
# drift, in the document people consult precisely because they do not want to read the code.
#
#   power   Source/missiles.cpp   ScaleSpellEffect - `value += value / N` once per level. This is
#                                 the exponential the 2026-08-31 cap change was about.
#   aura    Source/oracool/aura_field.cpp  AuraRadiusForPoints - a base, a step every N points, a cap.
#   rank    Source/oracool/spell_ranks.h   RankRequiredLevel - the Rule of Rangs, +1 character level
#                                 per rank past the first.
$missilesCpp = Read-SourceFile 'missiles.cpp'
$auraCpp = Read-SourceFile 'oracool/aura_field.cpp'

$powerDivisor = 0
if ($missilesCpp -match 'int ScaleSpellEffect\([^)]*\)\s*\{(?:[^}]*?)value \+= value / (\d+);') {
    $powerDivisor = [int]$matches[1]
}
if ($powerDivisor -le 0) { throw "BuildWiki: could not read ScaleSpellEffect's divisor from missiles.cpp - the wiki will not state a growth curve it cannot derive" }

$auraBase = 0; $auraStep = 0; $auraCap = 0
if ($auraCpp -match 'return std::min\((\d+) \+ \(points - 1\) / (\d+), (\d+)\);') {
    $auraBase = [int]$matches[1]; $auraStep = [int]$matches[2]; $auraCap = [int]$matches[3]
}
if ($auraBase -le 0) { throw "BuildWiki: could not read AuraRadiusForPoints from aura_field.cpp" }

$rankStep = 0
if ($ranksH -match 'return baseLevel \+ \(rank > 1 \? rank - 1 : 0\);') { $rankStep = 1 }
if ($rankStep -le 0) { throw "BuildWiki: the Rule of Rangs in spell_ranks.h is not the shape this table assumes" }

# A worked example rather than a bare multiplier: the growth is INTEGER - `value += value / 8`
# truncates every step - so a float power would be subtly wrong at every rank. Running the real loop
# on a round base of 100 is both exact and easier to read than 1.125^n.
$powerSample = 100
$progressionRows = @()
$powerValue = $powerSample
for ($rank = 1; $rank -le $mechanics.maxInvestment; $rank++) {
    $powerValue = $powerValue + [math]::Floor($powerValue / $powerDivisor)
    $radius = [math]::Min($auraBase + [math]::Floor(($rank - 1) / $auraStep), $auraCap)
    $progressionRows += , [ordered]@{
        rank        = $rank
        power       = [int]$powerValue
        powerTimes  = [math]::Round($powerValue / $powerSample, 2)
        auraRadius  = [int]$radius
        levelsAbove = ($rank - 1) * $rankStep
    }
}

$mechanics.skillProgression = [ordered]@{
    cap           = $mechanics.maxInvestment
    powerDivisor  = $powerDivisor
    powerSample   = $powerSample
    auraBase      = $auraBase
    auraStep      = $auraStep
    auraCap       = $auraCap
    lifetimePoints = ($mechanics.maxCharacterLevel - 1)
    rows          = $progressionRows
}

foreach ($m in [regex]::Matches($tiersCpp, '\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\},\s*//\s*(\w+)')) {
    $mechanics.tierScales += , [ordered]@{
        tier       = $m.Groups[5].Value
        power      = [int]$m.Groups[1].Value
        require    = [int]$m.Groups[2].Value
        durability = [int]$m.Groups[3].Value
        value      = [int]$m.Groups[4].Value
    }
}


# The band -> book-ilvl map. Fourth hand-typed table found inside the generated wiki (after the
# quality curves, the crafting recipes and the runeword grid size), so it is parsed now: the values
# happened to be right, but "happened to be right" is not the standard this generator exists for.
$ranksCpp = Read-SourceFile 'oracool/spell_ranks.cpp'
$mechanics.bookItemLevel = [ordered]@{}
if ($ranksCpp -match '(?s)int SpellBookItemLevel\(SpellID spell\)(.*?)\n\}') {
    foreach ($m in [regex]::Matches($matches[1], 'case (\d+):\s*\r?\n\s*return (\d+);')) {
        $mechanics.bookItemLevel[$m.Groups[1].Value] = [int]$m.Groups[2].Value
    }
}

# The per-band quality scales. These were hand-copied into affixes.html's JS, which is exactly the
# drift the reaudit is meant to catch - a retune in item_tiers.cpp would have left the wiki quoting
# the old curve with no way to notice.
$mechanics.qualityBands = [ordered]@{}
foreach ($name in @('Rare', 'BuffedUnique', 'Primal')) {
    if ($tiersCpp -match ("{0}ByBand\[BaseItemTierCount\] = \{{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)" -f $name)) {
        $mechanics.qualityBands[$name] = @([int]$matches[1], [int]$matches[2], [int]$matches[3], [int]$matches[4])
    }
}
if ($tiersCpp -match 'TierWeights\[BaseItemTierCount\] = \{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)') {
    $mechanics.tierWeights = @([int]$matches[1], [int]$matches[2], [int]$matches[3], [int]$matches[4])
}

# ---------------------------------------------------------------------------------------------
# INI options - Source/options.cpp's Oracool block
# ---------------------------------------------------------------------------------------------
$optionsCpp = Read-SourceFile 'options.cpp'
$options = New-Object System.Collections.ArrayList
foreach ($m in [regex]::Matches($optionsCpp, '\n\s*,\s*(\w+)\("([^"]+)",\s*OptionEntryFlags::\w+,\s*N_\("([^"]+)"\),\s*N_\("([^"]+)"\),\s*([^,\)]+)')) {
    [void]$options.Add([ordered]@{
            field       = $m.Groups[1].Value
            key         = $m.Groups[2].Value
            label       = $m.Groups[3].Value
            description = $m.Groups[4].Value
            default     = $m.Groups[5].Value.Trim()
        })
}

# ---------------------------------------------------------------------------------------------
# Art assets - what actually ships, measured rather than assumed
# ---------------------------------------------------------------------------------------------
Add-Type -AssemblyName System.Drawing
$assets = New-Object System.Collections.ArrayList
# oracool_assets, NOT assets. `assets` is DevilutionX's stock resource folder and holds five PNGs
# (ui_art buttons); this fork's own art - the 100-odd files this page exists to show - lives in
# `oracool_assets`, which is what CMake packs into oracool.mpq (see the pack rule in CMakeLists).
# Pointed at the stock folder, the gallery listed five vanilla buttons and called them the art
# assets, while wiki/sprites/ui kept a frozen copy of the real ones from before the path changed -
# stale images nothing on any page referenced, still being inlined into the published bundle.
$assetRoot = Join-Path $root 'Packaging\resources\oracool_assets'
# Cleared first, so an asset deleted from the game also leaves the wiki. Copying over a directory
# that is never emptied is how the stale set above survived every rebuild.
$spriteRootOut = Join-Path $out 'sprites'
if (Test-Path $spriteRootOut) { Remove-Item $spriteRootOut -Recurse -Force }
if (Test-Path $assetRoot) {
    foreach ($file in (Get-ChildItem $assetRoot -Recurse -Include *.png -ErrorAction SilentlyContinue)) {
        $w = 0; $h = 0
        try {
            $img = [System.Drawing.Image]::FromFile($file.FullName)
            $w = $img.Width; $h = $img.Height
            $img.Dispose()
        } catch { }
        $relative = $file.FullName.Substring($assetRoot.Length + 1).Replace('\', '/')
        [void]$assets.Add([ordered]@{
                path   = $relative
                width  = $w
                height = $h
                kb     = [math]::Round($file.Length / 1024, 1)
            })

        # COPIED into the wiki rather than linked across the repo. The wiki is served from its own
        # folder (and is meant to survive being zipped and sent somewhere), so a relative path out to
        # Packaging/ would resolve for exactly one way of opening it and break for the rest.
        $spriteDir = Join-Path $out 'sprites'
        $target = Join-Path $spriteDir $relative.Replace('/', '\')
        $targetDir = Split-Path -Parent $target
        if (-not (Test-Path $targetDir)) { New-Item -ItemType Directory -Path $targetDir -Force | Out-Null }
        Copy-Item $file.FullName $target -Force
    }
}

# ---------------------------------------------------------------------------------------------
# Dev reports - the project's own history
# ---------------------------------------------------------------------------------------------
$reports = New-Object System.Collections.ArrayList
$reportDir = Join-Path $vault '02-Development-Reports'
if (Test-Path $reportDir) {
    foreach ($file in (Get-ChildItem $reportDir -Filter *.md | Sort-Object Name -Descending)) {
        $text = Get-Content $file.FullName -Raw -Encoding UTF8
        $ver = ''
        if ($text -match '\*\*Version:\*\*\s*([0-9.]+)') { $ver = $matches[1] }
        $title = $file.BaseName
        # An explicit order index. The files are named by date, and several land on the same day, so
        # sorting a table by the date column alone shuffles same-day reports into an arbitrary order -
        # which put 1.8.5 above 1.8.7 on the history page. This preserves the filename sort.
        [void]$reports.Add([ordered]@{ order = $reports.Count; title = $title; version = $ver; file = $file.Name })
    }
}

# ---------------------------------------------------------------------------------------------
# The socket economy - Source/oracool/gems.cpp, charms.cpp, runewords.cpp, and the drop hook
# ---------------------------------------------------------------------------------------------
# Gems and runes share one effect table in gems.cpp, told apart by the index constant's own name.
# The rows use designated initialisers, which is exactly why they can be parsed at all: every
# number arrives labelled, so nothing here depends on column order.
$gemsCpp = Read-SourceFile 'oracool/gems.cpp'
$charmsCpp = Read-SourceFile 'oracool/charms.cpp'
$wordsCpp = Read-SourceFile 'oracool/runewords.cpp'
$itemsCpp2 = Read-SourceFile 'items.cpp'

function Get-PrettyName([string]$constant) {
    $bare = $constant -replace '^IDI_ORACOOL_(GEM|RUNE|CHARM)_', ''
    $bare = $bare -replace '_(CHIPPED|FLAWED|NORMAL|FLAWLESS|PERFECT)$', ''
    return ($bare.Substring(0, 1) + $bare.Substring(1).ToLower())
}

$gems = New-Object System.Collections.ArrayList
$runes = New-Object System.Collections.ArrayList
if ($gemsCpp -match '(?s)constexpr GemData Gems\[\] = \{(.*?)\n\};') {
    $tableBody = $matches[1]
    foreach ($row in [regex]::Matches($tableBody, '(?s)\{\s*\.idx = (IDI_ORACOOL_\w+)(.*?)\}')) {
        $idx = $row.Groups[1].Value
        $fieldText = $row.Groups[2].Value
        $fields = [ordered]@{}
        foreach ($f in [regex]::Matches($fieldText, '\.(\w+) = (-?\d+|true|false)')) {
            $raw = $f.Groups[2].Value
            if ($raw -eq 'true') { $fields[$f.Groups[1].Value] = $true }
            elseif ($raw -eq 'false') { $fields[$f.Groups[1].Value] = $false }
            else { $fields[$f.Groups[1].Value] = [int]$raw }
        }
        $entry = [ordered]@{ name = (Get-PrettyName $idx); constant = $idx; fields = $fields }
        if ($idx -like '*_RUNE_*') { [void]$runes.Add($entry) } else { [void]$gems.Add($entry) }
    }
}


# The other 28 runes live in their own generated include rather than inline in gems.cpp, so the
# table walk above cannot see them - it stops at the file's own rows. Same row shape, same parse.
$runesEffectsInc = Get-Content (Join-Path $src 'oracool\runes_effects.inc') -Raw -Encoding UTF8
foreach ($row in [regex]::Matches($runesEffectsInc, '(?s)\{\s*\.idx = (IDI_ORACOOL_\w+)(.*?)\}')) {
    $idx = $row.Groups[1].Value
    $fields = [ordered]@{}
    foreach ($f in [regex]::Matches($row.Groups[2].Value, '\.(\w+) = (-?\d+|true|false|ItemSpecialEffect::\w+)')) {
        $raw = $f.Groups[2].Value
        if ($raw -eq 'true') { $fields[$f.Groups[1].Value] = $true }
        elseif ($raw -eq 'false') { $fields[$f.Groups[1].Value] = $false }
        elseif ($raw -like 'ItemSpecialEffect::*') { $fields[$f.Groups[1].Value] = $raw.Substring('ItemSpecialEffect::'.Length) }
        else { $fields[$f.Groups[1].Value] = [int]$raw }
    }
    [void]$runes.Add([ordered]@{ name = (Get-PrettyName $idx); constant = $idx; fields = $fields })
}

# The jewels (v1.9.9) - same story as the 28 runes above and for the same reason. Their rows are
# `#include`d into Gems[] rather than written in gems.cpp, so the table walk cannot see them: it
# reads the FILE's text, and the file holds an include directive, not the fifteen rows.
#
# Note what that near-miss would have looked like. The walk sorts a row into $runes or $gems on
# `*_RUNE_*` in the constant, with gems as the else - so if the jewel rows HAD been inline, all
# fifteen would have been filed as gems and the wiki would have reported a 50-gem ladder in a
# perfectly clean build. They get a collection of their own here for that reason as much as any.
# D2MXL-to-ORCL Phases 2-4 (v1.9.20-1.9.23): signets, milestones and the named encounters. All
# parsed, because every number in them is a tuning value the telemetry is expected to correct - a
# typed copy would be wrong the first time any of them moves.
$signetsCpp = Read-SourceFile 'oracool/signets.cpp'
$signetsHdr = Read-SourceFile 'oracool/signets.h'
$milestones = @()
if ($signetsCpp -match '(?s)const char \*MilestoneName\(Milestone milestone\)(.*?)\n\}') {
    foreach ($m in [regex]::Matches($matches[1], 'case Milestone::(\w+):\s*\r?\n\s*return N_\("([^"]+)"\)')) {
        $milestones += $m.Groups[2].Value
    }
}
$progression = [ordered]@{
    signetCap  = [int](Get-Constant $signetsHdr 'constexpr int SignetLifetimeCap = (\d+)')
    dropPercent = [int](Get-Constant $itemsCpp2 'constexpr int SignetDropPercent = (\d+)')
    milestones = $milestones
}

# The named encounters - place, tileset and reward, read out of the Places[] table and the generated
# item table so the wiki cannot claim an encounter pays something it does not.
$encCpp = Read-SourceFile 'oracool/named_encounters.cpp'
$encTable = Get-Content (Join-Path $src 'oracool\encounter_items_table.inc') -Raw -Encoding UTF8
$encPlaces = @()
if ($encCpp -match '(?s)constexpr EncounterPlace Places\[\] = \{(.*?)\n\};') {
    # The `floor` between MT_ and the name arrived at v1.9.49, and this pattern did not follow it:
    # it required the name straight after the monster, matched nothing from that day on, and the
    # loop below breaks on an empty $encPlaces - so the encounters table published blank for a week
    # without a word. Caught by the empty-collection guard at the end of this script on its first
    # run. The floor is captured rather than skipped, because it is the answer to "how deep does
    # this arena count as", which is exactly what a reader of that table wants to know.
    foreach ($m in [regex]::Matches($matches[1], '\{\s*SL_(\w+),\s*DTYPE_(\w+),\s*MT_(\w+),\s*(\d+),\s*"([^"]+)"\s*\}')) {
        $encPlaces += [ordered]@{
            arena   = (Get-Culture).TextInfo.ToTitleCase(($m.Groups[1].Value -replace '_', ' ').ToLower())
            dungeon = (Get-Culture).TextInfo.ToTitleCase($m.Groups[2].Value.ToLower())
            floor   = [int]$m.Groups[4].Value
            name    = $m.Groups[5].Value
        }
    }
}
# The map and reward names come from AllItemsList, so a renamed item renames itself here too.
$encRows = New-Object System.Collections.ArrayList
$encIdx = 0
foreach ($m in [regex]::Matches($encTable, 'NamedEncounter::(\w+),\s*(IDI_\w+),\s*(IDI_\w+)')) {
    if ($encIdx -ge $encPlaces.Count) { break }
    $mapItem = $items | Where-Object { $_.id -eq $m.Groups[2].Value } | Select-Object -First 1
    $rewardItem = $items | Where-Object { $_.id -eq $m.Groups[3].Value } | Select-Object -First 1
    $row = [ordered]@{
        name    = $encPlaces[$encIdx].name
        arena   = $encPlaces[$encIdx].arena
        dungeon = $encPlaces[$encIdx].dungeon
        floor   = $encPlaces[$encIdx].floor
        map     = if ($null -ne $mapItem) { $mapItem.name } else { $m.Groups[2].Value }
        reward  = if ($null -ne $rewardItem) { $rewardItem.name } else { $m.Groups[3].Value }
    }
    [void]$encRows.Add($row)
    $encIdx++
}

# Treasure classes (v1.9.13) - the per-zone drop tables. Parsed straight out of the Classes[] array
# and its dungeon switch, so the wiki's table IS the game's table. The row order in the array is the
# order the switch maps onto, and the switch is read too rather than assumed, because "row 4 is
# Hell" is exactly the kind of correspondence that survives a reorder in the source and not on the
# page.
$treasureCpp = Read-SourceFile 'oracool/treasure_class.cpp'
$tcRows = @()
if ($treasureCpp -match '(?s)constexpr TreasureClass Classes\[\] = \{(.*?)\n\};') {
    foreach ($m in [regex]::Matches($matches[1], '\{\s*"([^"]+)",\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\}')) {
        $tcRows += [ordered]@{
            name        = $m.Groups[1].Value
            socketable  = [int]$m.Groups[2].Value
            gem         = [int]$m.Groups[3].Value
            rune        = [int]$m.Groups[4].Value
            jewel       = [int]$m.Groups[5].Value
            charm       = [int]$m.Groups[6].Value
            orb         = [int]$m.Groups[7].Value
            set         = [int]$m.Groups[8].Value
        }
    }
}
$treasureClasses = New-Object System.Collections.ArrayList
if ($treasureCpp -match '(?s)size_t ClassIndexFor\(dungeon_type dungeon\)(.*?)\n\}') {
    $switchBody = $matches[1]
    foreach ($m in [regex]::Matches($switchBody, 'case DTYPE_(\w+):\s*\r?\n\s*return (\d+);')) {
        $index = [int]$m.Groups[2].Value
        if ($index -ge $tcRows.Count) { continue }
        $row = [ordered]@{ dungeon = (Get-Culture).TextInfo.ToTitleCase($m.Groups[1].Value.ToLower()) }
        foreach ($k in $tcRows[$index].Keys) { $row[$k] = $tcRows[$index][$k] }
        [void]$treasureClasses.Add($row)
    }
}
$treasureBonuses = [ordered]@{
    champion = [int](Get-Constant $treasureCpp '(?s)lesserAffix != LesserUniqueAffix::None\)\s*\r?\n\s*return (\d+);')
    unique   = [int](Get-Constant $treasureCpp '(?s)monster\.isUnique\(\)\)\s*\r?\n\s*return (\d+);')
    boss     = [int](Get-Constant $treasureCpp '(?s)IsEndgameBoss\(monster\)\)\s*\r?\n\s*return (\d+);')
}

# Endgame bosses (v1.9.14). The profile and the traits both come out of endgame_boss.cpp; the
# champion numbers they are compared against come out of monster.cpp, so the page can state the
# relation rather than two lists of numbers a reader has to diff themselves.
$bossCpp = Read-SourceFile 'oracool/endgame_boss.cpp'
$monsterCpp = Read-SourceFile 'monster.cpp'
$endgameBoss = [ordered]@{
    healthPercent   = [int](Get-Constant $bossCpp 'constexpr int HealthPercent = (\d+)')
    damagePercent   = [int](Get-Constant $bossCpp 'constexpr int DamagePercent = (\d+)')
    armorBonus      = [int](Get-Constant $bossCpp 'constexpr int ArmorBonus = (\d+)')
    packSize        = [int](Get-Constant $bossCpp 'constexpr int PackSize = (\d+)')
    introPercent    = [int](Get-Constant $bossCpp 'constexpr int BossIntroPercent = (\d+)')
    champHealth     = [int](Get-Constant $monsterCpp 'constexpr int LesserUniqueHealthPercent = (\d+)')
    champDamage     = [int](Get-Constant $monsterCpp 'constexpr int LesserUniqueDamagePercent = (\d+)')
    champArmor      = [int](Get-Constant $monsterCpp 'constexpr int LesserUniqueArmorBonus = (\d+)')
    champPack       = [int](Get-Constant $monsterCpp 'constexpr int LesserUniquePackSize = (\d+)')
    traits          = @()
}
foreach ($m in [regex]::Matches($bossCpp, 'case BossTrait::(\w+):\s*\r?\n\s*return N_\("([^"]+)"\)')) {
    $endgameBoss.traits += $m.Groups[2].Value
}

# Monster variants (v1.9.7) and their per-dungeon rosters (v1.9.11). Parsed rather than typed for
# the usual reason, and one specific one: the rosters are a design statement - the Cathedral gets no
# elemental variant, town gets none at all - and a typed copy of a design statement is the kind of
# claim that stays on the page for months after the table under it changes.
$variantsCpp = Read-SourceFile 'oracool/monster_variants.cpp'
$variantRosters = New-Object System.Collections.ArrayList
foreach ($m in [regex]::Matches($variantsCpp, '(?s)constexpr MonsterVariant (\w+)Roster\[\] = \{(.*?)\};')) {
    $names = [regex]::Matches($m.Groups[2].Value, 'MonsterVariant::(\w+)') | ForEach-Object { $_.Groups[1].Value }
    [void]$variantRosters.Add([ordered]@{ dungeon = $m.Groups[1].Value; variants = @($names) })
}
# The per-difficulty ladders (v1.9.16) - the variant rate, the treasure scale and the champion
# affix pool. All three parsed, because all three are the answer to "does a re-run mean anything",
# and a typed answer to that question is one that stops being true the next time it is tuned.
$difficultyLadder = New-Object System.Collections.ArrayList
$variantRates = @{}
if ($variantsCpp -match '(?s)int VariantPercentFor\(_difficulty difficulty\)(.*?)\n\}') {
    foreach ($m in [regex]::Matches($matches[1], 'case DIFF_(\w+):\s*\r?\n\s*return (\d+);')) {
        $variantRates[$m.Groups[1].Value] = [int]$m.Groups[2].Value
    }
}
$treasureScales = @{}
if ($treasureCpp -match '(?s)int DifficultyTreasureScale\(_difficulty difficulty\)(.*?)\n\}') {
    foreach ($m in [regex]::Matches($matches[1], 'case DIFF_(\w+):\s*\r?\n\s*return (\d+);')) {
        $treasureScales[$m.Groups[1].Value] = [int]$m.Groups[2].Value
    }
}
# The affix pool is a "from here on" test rather than a list, so the arrival difficulty of each
# modifier is read out of ChampionAffixAllowedOn's own switch and inverted into per-rung lists.
$affixArrival = @{}
$diffCpp = Read-SourceFile 'oracool/monster_difficulty.cpp'
if ($diffCpp -match '(?s)bool ChampionAffixAllowedOn\(LesserUniqueAffix affix, _difficulty difficulty\)(.*?)\n\}') {
    $body = $matches[1]
    # The gap between a case group and its return holds the comment explaining WHY that rung, so
    # the middle is matched with a negative lookahead on the next case label rather than by
    # excluding characters - a "[^c]*?" here silently matched nothing, because every one of those
    # comments contains the letter c.
    foreach ($m in [regex]::Matches($body, '(?s)((?:\s*case LesserUniqueAffix::\w+:)+)((?:(?!case LesserUniqueAffix::).)*?)return ([^;]+);')) {
        $verdict = $m.Groups[3].Value.Trim()
        foreach ($c in [regex]::Matches($m.Groups[1].Value, 'LesserUniqueAffix::(\w+)')) {
            $affix = $c.Groups[1].Value
            if ($affix -eq 'Dread' -or $affix -eq 'None') { continue }
            if ($verdict -eq 'true') { $affixArrival[$affix] = 'NORMAL' }
            elseif ($verdict -match 'DIFF_(\w+)') { $affixArrival[$affix] = $matches[1] }
        }
    }
}
foreach ($rung in @('NORMAL', 'NIGHTMARE', 'HELL', 'TORMENT')) {
    $arriving = @($affixArrival.Keys | Where-Object { $affixArrival[$_] -eq $rung } | Sort-Object)
    $scale = 100
    if ($treasureScales.ContainsKey($rung)) { $scale = $treasureScales[$rung] }
    $rate = 15
    if ($variantRates.ContainsKey($rung)) { $rate = $variantRates[$rung] }
    [void]$difficultyLadder.Add([ordered]@{
        name           = (Get-Culture).TextInfo.ToTitleCase($rung.ToLower())
        variantPercent = $rate
        treasureScale  = $scale
        newAffixes     = $arriving
    })
}

$monsterVariants = [ordered]@{
    percent       = [int](Get-Constant $variantsCpp 'constexpr int VariantPercent = (\d+)')
    hollowLife    = [int](Get-Constant $variantsCpp 'constexpr int HollowLifePercent = (\d+)')
    hollowDamage  = [int](Get-Constant $variantsCpp 'constexpr int HollowDamagePercent = (\d+)')
    feralLife     = [int](Get-Constant $variantsCpp 'constexpr int FeralLifePercent = (\d+)')
    feralDamage   = [int](Get-Constant $variantsCpp 'constexpr int FeralDamagePercent = (\d+)')
    rosters       = $variantRosters
}

$jewels = New-Object System.Collections.ArrayList
$jewelsEffectsInc = Get-Content (Join-Path $src 'oracool\jewels_effects.inc') -Raw -Encoding UTF8
foreach ($row in [regex]::Matches($jewelsEffectsInc, '(?s)\{\s*\.idx = (IDI_ORACOOL_JEWEL_\w+)(.*?)\}')) {
    $idx = $row.Groups[1].Value
    $fields = [ordered]@{}
    foreach ($f in [regex]::Matches($row.Groups[2].Value, '\.(\w+) = (-?\d+|true|false)')) {
        $raw = $f.Groups[2].Value
        if ($raw -eq 'true') { $fields[$f.Groups[1].Value] = $true }
        elseif ($raw -eq 'false') { $fields[$f.Groups[1].Value] = $false }
        else { $fields[$f.Groups[1].Value] = [int]$raw }
    }
    [void]$jewels.Add([ordered]@{ name = (Get-PrettyName $idx); constant = $idx; fields = $fields })
}

# Depth and price, off the generated data rows - both files, so the five shipped runes and the 28
# appended ones read from the same place the game does.
$runeQlvl = @{}
foreach ($file in @('runes_shipped_data.inc', 'runes_data.inc')) {
    $text = Get-Content (Join-Path $src "oracool\$file") -Raw -Encoding UTF8
    foreach ($m in [regex]::Matches($text, 'IDI_ORACOOL_RUNE_(\w+)\*/.*?N_\("Rune"\),\s*(\d+),.*?false,\s*(\d+)')) {
        $key = $m.Groups[1].Value
        $runeQlvl[$key] = @{ qlvl = [int]$m.Groups[2].Value; value = [int]$m.Groups[3].Value }
    }
}
foreach ($rune in $runes) {
    $key = $rune.constant -replace '^IDI_ORACOOL_RUNE_', ''
    if ($runeQlvl.ContainsKey($key)) {
        $rune.qlvl = $runeQlvl[$key].qlvl
        $rune.value = $runeQlvl[$key].value
    }
}

# The ladder order - the enum interleaves runes with charms and the gem ladder, so this is the only
# correct sequence, and it is what the wiki must present them in.
$orderInc = Get-Content (Join-Path $src 'oracool\runes_order.inc') -Raw -Encoding UTF8
$runeOrder = @([regex]::Matches($orderInc, 'IDI_ORACOOL_RUNE_(\w+),') | ForEach-Object { $_.Groups[1].Value })
$ordered = New-Object System.Collections.ArrayList
foreach ($key in $runeOrder) {
    $match = $runes | Where-Object { ($_.constant -replace '^IDI_ORACOOL_RUNE_', '') -eq $key }
    if ($match) { [void]$ordered.Add($match) }
}
$runes = $ordered

$gemQualities = New-Object System.Collections.ArrayList
foreach ($q in [regex]::Matches($gemsCpp, 'case GemQuality::(\w+):\s*\r?\n\s*return (\d+);')) {
    [void]$gemQualities.Add([ordered]@{ name = $q.Groups[1].Value; percent = [int]$q.Groups[2].Value })
}

$charms = New-Object System.Collections.ArrayList
if ($charmsCpp -match '(?s)constexpr CharmData Charms\[\] = \{(.*?)\n\};') {
    foreach ($row in [regex]::Matches($matches[1], '\{\s*(IDI_ORACOOL_CHARM_\w+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\}')) {
        [void]$charms.Add([ordered]@{
                name          = (Get-PrettyName $row.Groups[1].Value)
                life          = [int]$row.Groups[2].Value
                fireResist    = [int]$row.Groups[3].Value
                lightResist   = [int]$row.Groups[4].Value
                toHit         = [int]$row.Groups[5].Value
                magicFind     = [int]$row.Groups[6].Value
                goldFind      = [int]$row.Groups[7].Value
            })
    }
}
$charmCap = [int](Get-Constant (Read-SourceFile 'oracool/charms.h') 'CharmActiveCap = (\d+)')

$runewords = New-Object System.Collections.ArrayList
if ($wordsCpp -match '(?s)constexpr RunewordDefinition Runewords\[\] = \{(.*?)\n\};') {
    $pattern = '(?s)N_\("([^"]+)"\),\s*static_cast<uint8_t>\(SocketHost::(\w+)\),\s*(\d+),\s*\{([^}]*)\},\s*([-\d,\s]*?)\}'
    foreach ($row in [regex]::Matches($matches[1], $pattern)) {
        $runeList = @()
        foreach ($r in [regex]::Matches($row.Groups[4].Value, 'IDI_ORACOOL_RUNE_(\w+)')) {
            $runeList += ($r.Groups[1].Value.Substring(0, 1) + $r.Groups[1].Value.Substring(1).ToLower())
        }
        $nums = @($row.Groups[5].Value -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '^-?\d+$' } | ForEach-Object { [int]$_ })
        while ($nums.Count -lt 8) { $nums += 0 }
        [void]$runewords.Add([ordered]@{
                name        = $row.Groups[1].Value
                host        = $row.Groups[2].Value
                sockets     = [int]$row.Groups[3].Value
                runes       = ($runeList -join ' ')
                damagePct   = $nums[0]
                damageMod   = $nums[1]
                toHit       = $nums[2]
                allResist   = $nums[3]
                armor       = $nums[4]
                spellLevels = $nums[5]
                mana        = $nums[6]
                life        = $nums[7]
            })
    }
}

# The drop hook's own numbers, read out of items.cpp so tuning them updates the wiki with them.
$socketRules = [ordered]@{
    # The per-family drop percentages used to be read here, as four constants in items.cpp. They
    # stopped existing at v1.9.13: the rate and the family split both come from the floor's treasure
    # class now, so they live in $treasureClasses and are rendered per zone. Nothing is read here in
    # their place deliberately - a single number would have to be an average, and an average across
    # six zones that were deliberately made to differ is a worse answer than no number.
    socketedPercent   = [int](Get-Constant $itemsCpp2 '(?s)void TryAddSocketsToDroppedItem.*?GenerateRnd\(100\) >= (\d+)')
    socketWeights     = @()
    qualityWeights    = @(40, 30, 18, 9, 3)
    etherealPercent   = [int](Get-Constant $itemsCpp2 '(?s)void TryMakeDroppedItemEthereal.*?GenerateRnd\(100\) >= (\d+)')
    etherealBonusPct  = 135
    charmActiveCap    = $charmCap
    # The grid's size moved into the skin header (levski_roar.h now forwards to levski_skin::GridColumns
    # and GridRows), so it is read from there - found 2026-09-07 when the build stopped on a '?'.
    levskiGridColumns = [int](Get-Constant (Read-SourceFile 'oracool/levski_roar_skin.h') 'constexpr int GridColumns = (\d+)')
    levskiGridRows    = [int](Get-Constant (Read-SourceFile 'oracool/levski_roar_skin.h') 'constexpr int GridRows = (\d+)')
    recipes           = @()
}


# The recipes, parsed from crafting.cpp's own name and input tables rather than retyped - the
# hand-written list said three recipes and "Sol is the top of the ladder" long after neither was
# true, which is precisely the drift this generator exists to prevent.
$craftingCpp = Read-SourceFile 'oracool/crafting.cpp'
$recipeNames = @{}
$recipeInputs = @{}
if ($craftingCpp -match '(?s)const char \*CraftingRecipeName\(int index\)(.*?)\n\}') {
    foreach ($m in [regex]::Matches($matches[1], 'case (\d+):\s*\r?\n\s*return N_\("([^"]+)"\)')) {
        $recipeNames[[int]$m.Groups[1].Value] = $m.Groups[2].Value
    }
}
if ($craftingCpp -match '(?s)const char \*CraftingRecipeInputs\(int index\)(.*?)\n\}') {
    foreach ($m in [regex]::Matches($matches[1], 'case (\d+):\s*\r?\n\s*return N_\("([^"]+)"\)')) {
        $recipeInputs[[int]$m.Groups[1].Value] = $m.Groups[2].Value
    }
}
# No venue column: the monument is the only place a recipe runs (v1.9.142), so a "where" column
# would be seventeen repetitions of one fact. The page says it once, in prose.
$recipeRows = New-Object System.Collections.ArrayList
foreach ($key in ($recipeNames.Keys | Sort-Object)) {
    [void]$recipeRows.Add([ordered]@{ name = $recipeNames[$key]; formula = $recipeInputs[$key] })
}
$socketRules.recipes = $recipeRows

# The socket-count weights, read out of the drop hook so retuning them retunes the wiki.
if ($itemsCpp2 -match 'constexpr int SocketWeights\[Item::MaxItemSockets\] = \{([^}]*)\}') {
    $socketRules.socketWeights = @($matches[1] -split ',' | ForEach-Object { $_.Trim() } |
        Where-Object { $_ -match '^\d+$' } | ForEach-Object { [int]$_ })
}


# ---------------------------------------------------------------------------------------------
# Runewords - Source/oracool/runewords_data.inc (GENERATED; see tools/GenRunewords.ps1)
# ---------------------------------------------------------------------------------------------
$runewordsInc = Get-Content (Join-Path $src 'oracool\runewords_data.inc') -Raw -Encoding UTF8
$runewords = New-Object System.Collections.ArrayList
$runeNameByConst = @{}
foreach ($rune in $runes) { $runeNameByConst[$rune.constant] = $rune.name }

# The tail was '([-\d,\s]*?)\}' - digits, commas and space only - which could never reach the row's
# closing brace, because every row carries an ItemSpecialEffect::X token after its first eight
# numbers. So NO row matched and the wiki published an empty Runewords page while the source held
# 370 of them. [^{}] accepts the token; the eight stats are still the first eight numbers.
$pattern = '(?s)N_\("([^"]+)"\),\s*static_cast<uint8_t>\(RunewordHost::(\w+)\),\s*(\d+),\s*\{([^}]*)\},\s*([^{}]*)\}'
foreach ($m in [regex]::Matches($runewordsInc, $pattern)) {
    $sequence = @()
    foreach ($r in [regex]::Matches($m.Groups[4].Value, 'IDI_ORACOOL_RUNE_\w+')) {
        $name = $runeNameByConst[$r.Value]
        if ($name) { $sequence += $name }
    }
    $nums = @($m.Groups[5].Value -split ',' | ForEach-Object { $_.Trim() } |
        Where-Object { $_ -match '^-?\d+$' } | ForEach-Object { [int]$_ } | Select-Object -First 8)
    while ($nums.Count -lt 8) { $nums += 0 }

    $grants = @()
    if ($nums[0]) { $grants += "+$($nums[0])% damage" }
    if ($nums[1]) { $grants += "+$($nums[1]) damage" }
    if ($nums[2]) { $grants += "+$($nums[2])% to hit" }
    if ($nums[3]) { $grants += "+$($nums[3])% all resists" }
    if ($nums[4]) { $grants += "+$($nums[4]) armour" }
    if ($nums[5]) { $grants += "+$($nums[5]) to spell levels" }
    if ($nums[6]) { $grants += "+$($nums[6]) mana" }
    if ($nums[7]) { $grants += "+$($nums[7]) life" }

    # The word's tier is its deepest rune - that is what gates when it can be built at all.
    $deepest = ''
    $deepestRung = -1
    foreach ($runeName in $sequence) {
        $rung = [array]::IndexOf(@($runes | ForEach-Object { $_.name }), $runeName)
        if ($rung -gt $deepestRung) { $deepestRung = $rung; $deepest = $runeName }
    }

    [void]$runewords.Add([ordered]@{
            name    = $m.Groups[1].Value
            host    = $m.Groups[2].Value
            sockets = [int]$m.Groups[3].Value
            runes   = ($sequence -join ' ')
            deepest = $deepest
            grants  = ($grants -join ', ')
        })
}

# ---------------------------------------------------------------------------------------------
# Pipeline - the vault's own backlog table (Diablo Orcl/07-Backlog/Pipeline.md)
# ---------------------------------------------------------------------------------------------
# Parsed rather than retyped, and the vault file is the source: the user edits it in Obsidian and
# the page follows. The old Idea-Backlog.md is NOT read - a dozen of its "not started" entries had
# shipped without being moved, which is exactly the staleness this page exists to avoid.
$pipeline = New-Object System.Collections.ArrayList
$pipelinePath = Join-Path $vault '07-Backlog\Pipeline.md'
if (Test-Path $pipelinePath) {
    foreach ($line in (Get-Content $pipelinePath -Encoding UTF8)) {
        if ($line -notmatch '^\|') { continue }
        if ($line -match '^\|\s*-{2,}' -or $line -match '^\|\s*Name\s*\|') { continue }
        $cells = @($line.Trim('|') -split '\|' | ForEach-Object { $_.Trim() })
        if ($cells.Count -lt 6) { continue }
        [void]$pipeline.Add([ordered]@{
                name    = $cells[0]
                group   = $cells[1]
                size    = $cells[2]
                save    = $cells[3]
                blocked = $cells[4]
                summary = $cells[5]
            })
    }
}
# ---------------------------------------------------------------------------------------------
# Debug console commands - Source/debug.cpp
# ---------------------------------------------------------------------------------------------
$debugCpp = Read-SourceFile 'debug.cpp'
$debugCmds = New-Object System.Collections.ArrayList
if ($debugCpp -match '(?s)std::vector<DebugCmdItem> DebugCmdList = \{(.*?)\n\};') {
    foreach ($row in [regex]::Matches($matches[1], '\{\s*"(\w+)",\s*"([^"]*)",\s*"([^"]*)",\s*&\w+\s*\}')) {
        [void]$debugCmds.Add([ordered]@{
                command = $row.Groups[1].Value
                args    = $row.Groups[3].Value
                summary = $row.Groups[2].Value
            })
    }
}

# ---------------------------------------------------------------------------------------------
# What each debug command will ACCEPT (user, 2026-08-31)
# ---------------------------------------------------------------------------------------------
#
# The wiki listed every command and every command's summary, and that was not enough to use them:
# `tiledata` says "leave name empty to see a list", and that list existed only inside the running
# game. The user went looking for the tile-coordinate overlay, tried four spellings, and got the
# argument list printed at them instead - which is the wiki failing at the one thing this page is
# for.
#
# Three sources, all PARSED. Nothing below is typed out here, so an option added to any of them
# reaches the wiki without anyone remembering to come back.
$tileDataOptions = @()
if ($debugCpp -match '(?s)std::string DebugCmdShowTileData\(const string_view parameter\)\s*\{\s*std::string paramList\[\] = \{(.*?)\};') {
    $tileDataOptions = @([regex]::Matches($matches[1], '"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
}
if ($tileDataOptions.Count -eq 0) { throw "BuildWiki: could not read DebugCmdShowTileData's paramList - the wiki will not list options it cannot derive" }

# The towner short names `visit` answers to - the map's keys ARE the vocabulary.
$townerNames = @()
if ($debugCpp -match '(?s)TownerShortNameToTownerId = \{(.*?)\n\};') {
    $townerNames = @([regex]::Matches($matches[1], '\{\s*"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
}

# `arrow`'s four effects, from the handler's own comparisons rather than from its summary.
$arrowEffects = @()
if ($debugCpp -match '(?s)std::string DebugCmdArrow\(const string_view parameter\)(.*?)\n\}') {
    $arrowEffects = @([regex]::Matches($matches[1], '"([a-z]+)"') | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
}

# And the lists that some summaries spell out in parentheses while their SIBLINGS do not. givebset
# names all nine materials; givemset, giverset, giveuset, givepset and giveeset take the same
# {tier} and say only "optionally of material {tier}". Rather than copy the list five times here,
# the placeholder is the key: a list found on any command that documents it is offered to every
# command that takes the same placeholder. That is why this is a map keyed by "{tier}" and "{q}".
$optionsByPlaceholder = @{}
foreach ($c in $debugCmds) {
    if ($c.summary -match '\{(\w+)\}[^(]*\(([a-z]+(?:/[a-z]+)+)\)') {
        $optionsByPlaceholder['{' + $matches[1] + '}'] = @($matches[2] -split '/')
    }
}

foreach ($c in $debugCmds) {
    $opts = @()
    $source = ''
    if ($c.command -eq 'tiledata') { $opts = $tileDataOptions; $source = 'DebugCmdShowTileData' }
    elseif ($c.command -eq 'visit') { $opts = $townerNames; $source = 'TownerShortNameToTownerId' }
    elseif ($c.command -eq 'arrow') { $opts = $arrowEffects; $source = 'DebugCmdArrow' }
    else {
        foreach ($ph in $optionsByPlaceholder.Keys) {
            if ($c.args -like "*$ph*") { $opts = $optionsByPlaceholder[$ph]; $source = 'the command table'; break }
        }
    }
    $c.options = @($opts)
    $c.optionSource = $source
}

# ---------------------------------------------------------------------------------------------
# Autosave triggers - the declarations in Source/oracool/auto_save.h are the list
# ---------------------------------------------------------------------------------------------
$autoSaveH = Read-SourceFile 'oracool/auto_save.h'
$autoSaveTriggers = New-Object System.Collections.ArrayList
foreach ($t in [regex]::Matches($autoSaveH, 'void ScheduleAutoSaveFor(\w+)\(\);')) {
    # "ForStorePurchase" -> "Store purchase": the declaration names ARE the trigger list, so this
    # cannot drift from the code the way a hand-typed list would.
    $words = [regex]::Replace($t.Groups[1].Value, '(?<!^)([A-Z])', ' $1')
    [void]$autoSaveTriggers.Add(($words.Substring(0, 1) + $words.Substring(1).ToLower()))
}

# ---------------------------------------------------------------------------------------------
# Emit
# ---------------------------------------------------------------------------------------------
if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out | Out-Null }

# ---------------------------------------------------------------------------------------------
# Spell descriptions - Source/oracool/spell_descriptions.cpp
#
# One authored sentence per spell, and until now the one table the wiki never read: the spells page
# listed mana and missiles but could not say what any spell DOES. Matched onto the spell rows by the
# enum name in each line's comment, so a spell that moves in the enum keeps its sentence.
# ---------------------------------------------------------------------------------------------
$descCpp = Read-SourceFile 'oracool/spell_descriptions.cpp'
$spellDesc = @{}
foreach ($m in [regex]::Matches($descCpp, '/\*\s*(\w+)\s*\*/\s*(?:N_\()?"([^"]*)"')) {
    $spellDesc[$m.Groups[1].Value] = $m.Groups[2].Value
}
foreach ($s in $spells) {
    $s['desc'] = $(if ($spellDesc.ContainsKey($s['id'])) { $spellDesc[$s['id']] } else { '' })
}

# ---------------------------------------------------------------------------------------------
# Monster difficulty scaling - Source/monster.cpp and Source/oracool/monster_difficulty.cpp
#
# The four bonus constants are parsed rather than transcribed, so a retune in the game cannot leave
# the encyclopedia quoting yesterday's numbers. The HP and damage shapes are recorded as structured
# coefficients (InitMonster), which is what lets the page compute a real per-difficulty stat block
# client-side instead of printing a prose rule and making the reader do the arithmetic.
# ---------------------------------------------------------------------------------------------
$monsterCpp = Read-SourceFile 'monster.cpp'
# The default sits between the description and the allowed-value list: N_("..."), 20, { 11, 12, ... }.
# Anchoring on the closing paren of the description is what keeps this off the list's first entry -
# a looser pattern reads 11 and quietly reports Torment as x1.1.
$tormentDefault = [int](Get-Constant $optionsCpp 'tormentDifficultyMultiplier[^\n]*?\),\s*(\d+),\s*\{')
$monsterScaling = [ordered]@{
    toHitBonus = [ordered]@{
        normal    = 0
        nightmare = [int](Get-Constant $monsterCpp 'constexpr int NightmareToHitBonus = (\d+)')
        hell      = [int](Get-Constant $monsterCpp 'constexpr int HellToHitBonus = (\d+)')
    }
    acBonus = [ordered]@{
        normal    = 0
        nightmare = [int](Get-Constant $monsterCpp 'constexpr int NightmareAcBonus = (\d+)')
        hell      = [int](Get-Constant $monsterCpp 'constexpr int HellAcBonus = (\d+)')
    }
    # maxHitPoints = hpMultiple * base + flatAdded (single-player values; HP is stored in 1/64ths,
    # but both the base and the addition are quoted here in whole hit points).
    hitPoints = @(
        [ordered]@{ difficulty = 'Normal';    multiple = 1; added = 0 }
        [ordered]@{ difficulty = 'Nightmare'; multiple = 3; added = 50 }
        [ordered]@{ difficulty = 'Hell';      multiple = 4; added = 100 }
        [ordered]@{ difficulty = 'Torment';   multiple = 4; added = 100; timesMultiplier = $true }
    )
    # damage = multiple * base + added, clamped to 255 on Torment.
    damage = @(
        [ordered]@{ difficulty = 'Normal';    multiple = 1; added = 0 }
        [ordered]@{ difficulty = 'Nightmare'; multiple = 2; added = 4 }
        [ordered]@{ difficulty = 'Hell';      multiple = 4; added = 6 }
        [ordered]@{ difficulty = 'Torment';   multiple = 4; added = 6; timesMultiplier = $true }
    )
    # Monster::level - the combat level, which is NOT the area ladder.
    levelBonus = [ordered]@{ normal = 0; nightmare = 15; hell = 30; tormentBase = 30 }
    # Monster::exp - the +1000 happens BEFORE the multiply, so Nightmare is not simply twice Normal.
    experience = [ordered]@{ addBefore = 1000; normal = 1; nightmare = 2; hell = 4; tormentBase = 4 }
    torment = [ordered]@{
        default = $(if ($tormentDefault -gt 0) { $tormentDefault / 10.0 } else { 2.0 })
        min     = 1.1
        max     = 5.0
        ini     = 'Torment Difficulty Multiplier'
    }
    resistanceRule = 'Nightmare demotes the Hell immunities to resistances; Hell uses them as authored; Torment promotes resistances to immunities but always leaves one school un-immune.'
}

# ---------------------------------------------------------------------------------------------
# Per-level skill gains - Source/oracool/class_tree.cpp
#
# Every aura, mastery and passive that grows with investment does it through one helper:
#   Scaled(points, base, perPoint) = base + perPoint * (points - 1)
# so the ladder for a skill is fully described by the channel it feeds and that pair of numbers.
# They are PARSED here, per `case Skill::X:`, rather than transcribed - tools/BuildSkillsWorkbook.ps1
# transcribed the same table by hand and has already drifted (it still says the cap is 98, where the
# source now says 30).
#
# Skills whose ladder is not a Scaled() call - the Holy pulses, Thorns, Cleansing, Conviction and the
# Paladin's *PercentAt tables - deliberately emit nothing rather than a guess. Their own description
# text carries the per-level figure, and a wrong number would be worse than no number.
# ---------------------------------------------------------------------------------------------
$treeCpp = Read-SourceFile 'oracool/class_tree.cpp'
$skillGains = [ordered]@{}
$caseHits = [regex]::Matches($treeCpp, 'case Skill::(\w+):')
for ($i = 0; $i -lt $caseHits.Count; $i++) {
    $skillName = $caseHits[$i].Groups[1].Value
    $from = $caseHits[$i].Index
    $to = $(if ($i + 1 -lt $caseHits.Count) { $caseHits[$i + 1].Index } else { $treeCpp.Length })
    $body = $treeCpp.Substring($from, $to - $from)
    $gains = New-Object System.Collections.ArrayList
    foreach ($g in [regex]::Matches($body, '(\w+)\s*\+=\s*Scaled\(\w+,\s*(\d+),\s*(\d+)\)')) {
        [void]$gains.Add([ordered]@{
                channel = $g.Groups[1].Value
                base    = [int]$g.Groups[2].Value
                per     = [int]$g.Groups[3].Value
            })
    }
    # A fallthrough group shares one body; the first label in the group is the one that carries it.
    if ($gains.Count -gt 0 -and -not $skillGains.Contains($skillName)) { $skillGains[$skillName] = $gains }
}

# ---------------------------------------------------------------------------------------------
# Inventory icon index - Source/itemdat.h's item_cursor_graphic
#
# Items carry their icon as an enum NAME, but the exported icons are filed by NUMBER (the exporter
# cuts frame i from the cursor sheet, and frame i is exactly _iCurs == i). The enum's values are
# explicit and SPARSE - 0, 1, 4, 5, 6, 10, 12 - so the number cannot be inferred from position and
# has to be read. An unmatched name gets -1, and the page omits the picture rather than linking a
# file that is not there.
# ---------------------------------------------------------------------------------------------
$itemdatH = Read-SourceFile 'itemdat.h'
$cursBody = $itemdatH.Substring($itemdatH.IndexOf('enum item_cursor_graphic'))
$cursBody = $cursBody.Substring(0, $cursBody.IndexOf('};'))
# The enum #includes ten .inc files, and EVERY Oracool icon - gems, runes, jewels, orbs, charms,
# signets - is declared in one of them. Reading itemdat.h alone left 98 items with no icon index at
# all. The includes carry no explicit values, so they must be spliced in at their own position for
# the running count to stay right.
$cursBody = [regex]::Replace($cursBody, '#include\s+"(oracool/[A-Za-z0-9_]+\.inc)"', {
        param($m) Read-SourceFile $m.Groups[1].Value
    })
$cursIndex = @{}
$nextCurs = 0
foreach ($m in [regex]::Matches($cursBody, 'ICURS_(\w+)\s*(?:=\s*(\d+))?\s*,')) {
    if ($m.Groups[2].Success) { $nextCurs = [int]$m.Groups[2].Value }
    $cursIndex[$m.Groups[1].Value] = $nextCurs
    $nextCurs++
}
foreach ($it in $items) {
    $it['cursIndex'] = $(if ($cursIndex.ContainsKey($it['curs'])) { $cursIndex[$it['curs']] } else { -1 })
}

$data = [ordered]@{
    version   = $version
    generated = $generated
    classes   = $classes
    expTable  = $expTable
    affixes   = $affixes
    uniques   = $uniques
    setItems  = $setItems
    quests    = $quests
    shrines   = $shrines
    items     = $items
    spells    = $spells
    skills    = $skills
    monsters  = $monsters
    monsterScaling = $monsterScaling
    skillGains = $skillGains
    mechanics = $mechanics
    options   = $options
    gems      = $gems
    runes     = $runes
    jewels    = $jewels
    monsterVariants = $monsterVariants
    treasureClasses = $treasureClasses
    progression = $progression
    encounters = $encRows
    treasureBonuses = $treasureBonuses
    endgameBoss = $endgameBoss
    difficultyLadder = $difficultyLadder
    gemQualities = $gemQualities
    charms    = $charms

    runewords = $runewords
    pipeline  = $pipeline
    socketRules = $socketRules
    debugCmds = $debugCmds
    autoSaveTriggers = $autoSaveTriggers
    assets    = $assets
    reports   = $reports
}

# -------------------------------------------------------------------------------------------------
# The empty-collection guard.
#
# Every reader in this file is a regex or a directory walk over something that lives somewhere else,
# and the failure mode they share is that finding NOTHING looks exactly like finding nothing to find:
# the loop runs zero times, the table renders empty, the page publishes, and no line of output says
# so. Three of them had been doing that for a long time before the 2026-08-31 audit - dev reports and
# the backlog pipeline were looking for the vault beside the repo under the project's old folder
# name, and the art gallery was measuring DevilutionX's stock resources instead of this fork's.
#
# So: anything that comes back empty is named, loudly, at the point where it can still be noticed.
# Not fatal - a genuinely empty collection is legal (there may one day be no open pipeline entries) -
# but never silent again. A key listed here that SHOULD sometimes be empty belongs in $mayBeEmpty
# with a reason, which is a smaller and more honest thing to write than a comment explaining why the
# warning is ignored.
$mayBeEmpty = @('pipeline') # the backlog can legitimately be cleared
$emptyCollections = @()
foreach ($key in $data.Keys) {
    $value = $data[$key]
    if ($null -eq $value) { $emptyCollections += $key; continue }
    # Hashtables carry sub-tables (socketRules) rather than rows; counting their keys is the wrong
    # question, so only real collections are checked.
    if ($value -is [System.Collections.IDictionary]) { continue }
    if ($value -is [string]) { continue }
    if ($value -is [System.Collections.ICollection] -and $value.Count -eq 0) { $emptyCollections += $key }
}
$emptyCollections = @($emptyCollections | Where-Object { $mayBeEmpty -notcontains $_ })
if ($emptyCollections.Count -gt 0) {
    Write-Warning ("PARSED NOTHING: {0} - a reader found no rows. Its page will publish blank." -f ($emptyCollections -join ', '))
}

$json = $data | ConvertTo-Json -Depth 8 -Compress
Set-Content -Path (Join-Path $out 'data.js') -Value ("const WIKI = " + $json + ";") -Encoding UTF8

Write-Host ("           {0} runewords, {1} pipeline entries" -f $runewords.Count, $pipeline.Count)
Write-Host ("wiki data: {0} items, {1} spells, {2} skills, {3} monsters, {4} options, {5} assets, {6} reports" -f `
        $items.Count, $spells.Count, $skills.Count, $monsters.Count, $options.Count, $assets.Count, $reports.Count)
Write-Host ("           {0} classes, {1} exp rows, {2} affixes, {3} uniques, {4} set items, {5} quests, {6} shrines" -f `
        $classes.Count, $expTable.Count, $affixes.Count, $uniques.Count, $setItems.Count, $quests.Count, $shrines.Count)

# The bundle is part of the wiki, not a separate deliverable, so building one without the other is
# never what anyone wanted. On 2026-08-19 the multi-page wiki was rebuilt alone and the single-file
# bundle kept serving the previous Pipeline text - no error, no warning, nothing to notice.
#
# -NoBundle exists for the case that actually needs it: BundleWiki.ps1 inlines megabytes of sprites,
# so a run that only wants data.js refreshed can skip it. Anyone who does is choosing the drift, and
# tools\BundleWiki.ps1 -Verify will say so.
if ($NoBundle) {
    Write-Host "wiki bundle: SKIPPED (-NoBundle). tools\BundleWiki.ps1 -Verify will report it stale."
} else {
    & (Join-Path $PSScriptRoot 'BundleWiki.ps1')
}

# ---------------------------------------------------------------------------------------------
# The fourth channel
# ---------------------------------------------------------------------------------------------
#
# The wiki exists in four places: these pages, data.js, the bundle - and the published Artifact,
# which is the only one the user actually reads. The first three have checks. The fourth had none,
# and nothing here can give it one: publishing is not something a PowerShell script can do or
# inspect.
#
# So this is a reminder rather than a check, and it is deliberately the LAST thing printed.
#
# Found 2026-08-20, and it is worth stating the shape because it has now happened at four different
# layers of this project. The user reported the wiki listing 56 debug commands where the source had
# 62. The pages were right, data.js was right, the bundle was right, and BundleWiki.ps1 -Verify said
# "current" - all correct, all irrelevant, because the artifact was a day old. Same failure as the
# MPQ that was never repacked and the bundle that was never rebuilt: the thing produced was current
# and the thing consumed was not.
$artifactUrl = 'https://claude.ai/code/artifact/79abf513-fd1b-4f64-89aa-7c0696606337'
Write-Host ''
if ($NoBundle) {
    Write-Host 'REPUBLISH: nothing to publish - no bundle was written this run.' -ForegroundColor Yellow
} else {
    Write-Host 'REPUBLISH THE ARTIFACT - the wiki is not updated until you do.' -ForegroundColor Yellow
    Write-Host ('  file: {0}' -f (Join-Path $out 'oracool-wiki-bundle.html'))
    Write-Host ('  url:  {0}' -f $artifactUrl)
    Write-Host '  Pass that url when publishing. Without it a SECOND artifact is created and the'
    Write-Host '  one the user has bookmarked stays stale - which is the failure this line exists for.'
}
