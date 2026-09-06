# Generates Source/oracool/skill_sounds_data.inc from the delivered class-skill-sounds package.
#
# 304 WAVs covering all six class trees, delivered as data with an authoritative manifest
# (Resources/02-source-art/skill-sounds/class-skill-sounds.zip). The package's own integration
# contract says to resolve through the manifest's stable id and never to build paths from display
# text, which is exactly what this does: the manifest is read once here, at build time, and the
# engine only ever sees a generated table.
#
# The join is on (class, skill name). That is the ONLY key the two sides share - the package knows
# nothing about ClassTreeSkill enum values, and the tree knows nothing about sound ids. A skill name
# that matches no tree row, or a tree row whose class disagrees, is a hard error here rather than a
# sound that silently never plays.
#
#     pwsh -File tools\GenSkillSounds.ps1

param(
    [string]$Package  = "..\Resources\02-source-art\skill-sounds\class-skill-sounds.zip",
    [string]$TreeFile = "Source\oracool\class_tree.cpp",
    [string]$TreeEnum = "Source\oracool\class_tree.h",
    [string]$OutFile  = "Source\oracool\skill_sounds_data.inc"
)

$ErrorActionPreference = "Stop"

# --- the manifest ---------------------------------------------------------------------------------
$work = Join-Path ([System.IO.Path]::GetTempPath()) "oracool-skill-sounds"
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
Expand-Archive -Path $Package -DestinationPath $work -Force
$manifestPath = Get-ChildItem $work -Recurse -Filter "audio-manifest.csv" | Select-Object -First 1
if (-not $manifestPath) { throw "no audio-manifest.csv inside $Package" }
$rows = Import-Csv $manifestPath.FullName
Write-Host "manifest: $($rows.Count) sounds"

# --- the tree's enum names, in declaration order --------------------------------------------------
# The .inc rows have to name ClassTreeSkill values, and the enum is the only place those live. Taken
# in order and paired positionally with the table's rows below - the same one-walk-two-artefacts
# discipline the item-set generator uses, and for the same reason: a positional pairing that drifts
# is silent, so the two lists are read in one pass and their lengths checked against each other.
$enumNames = @()
$inEnum = $false
foreach ($line in Get-Content $TreeEnum) {
    if ($line -match '^\s*enum class ClassTreeSkill') { $inEnum = $true; continue }
    if ($inEnum) {
        if ($line -match '^\s*\};') { break }
        # Skip the FIRST/LAST/_FIRST/_LAST aliases: they are the same value under another name and
        # would double-count every class boundary.
        if ($line -match '^\s*([A-Za-z][A-Za-z0-9_]*)\s*,\s*(//.*)?$') {
            $n = $Matches[1]
            if ($n -notmatch '^(FIRST|LAST|None)$' -and $n -notmatch '_(FIRST|LAST)$') { $enumNames += $n }
        }
    }
}
Write-Host "enum: $($enumNames.Count) skills"

# --- the tree's rows: name + class, in the same declaration order ---------------------------------
$treeRows = @()
$inTable = $false
foreach ($line in Get-Content $TreeFile) {
    if ($line -match '^const ClassTreeSkillData Skills\[') { $inTable = $true; continue }
    if ($inTable) {
        if ($line -match '^\};') { break }
        if ($line -match '^\s*\{\s*N_\("([^"]+)"\)') { $treeRows += @{ Name = $Matches[1]; Class = $null } }
        # The class token can be on the row's first or second physical line (long descriptions wrap).
        if ($treeRows.Count -gt 0 -and $null -eq $treeRows[-1].Class) {
            if ($line -match '(?<![A-Za-z])(Pal|Bar|Sor|Rog|Bard|Monk)\s*,\s*\d') { $treeRows[-1].Class = $Matches[1] }
        }
    }
}
Write-Host "tree: $($treeRows.Count) rows"
if ($treeRows.Count -ne $enumNames.Count) {
    throw "the enum has $($enumNames.Count) skills but the table has $($treeRows.Count) rows - they are paired positionally"
}

# The package's class words, mapped onto the table's aliases. "paladin" is the Warrior slot in this
# fork (see the note at the top of class_tree.cpp).
$classAlias = @{ paladin = "Pal"; barbarian = "Bar"; sorcerer = "Sor"; rogue = "Rog"; bard = "Bard"; monk = "Monk" }

# (class alias, skill name) -> enum name. The pair is the key because two classes can carry the same
# skill name, and a name-only lookup would resolve one of them to the other's sounds.
$byKey = @{}
for ($i = 0; $i -lt $treeRows.Count; $i++) {
    $key = "$($treeRows[$i].Class)|$($treeRows[$i].Name)"
    if ($byKey.ContainsKey($key)) { throw "two tree rows are both '$key'" }
    $byKey[$key] = $enumNames[$i]
}

# --- emit -----------------------------------------------------------------------------------------
$eventEnum = @{ cast = "Cast"; impact = "Impact"; arrive = "Arrive"; start = "Start"; loop = "Loop"; stop = "Stop"; learn = "Learn" }

$out = @()
$unmatched = @()
foreach ($r in $rows | Sort-Object class, skill, event) {
    $alias = $classAlias[$r.class]
    if (-not $alias) { throw "manifest class '$($r.class)' is not one of the six" }
    $key = "$alias|$($r.skill)"
    if (-not $byKey.ContainsKey($key)) { $unmatched += $key; continue }
    $ev = $eventEnum[$r.event]
    if (-not $ev) { throw "manifest event '$($r.event)' has no SkillSoundEvent" }
    # The staged path under Packaging/resources/oracool_assets, which becomes the MPQ-relative path.
    $path = "sfx\\skills\\" + ($r.path -replace '^sounds/','' -replace '/','\\')
    $out += "`t{ Skill::$($byKey[$key]), SkillSoundEvent::$ev, `"$path`" },"
}
if ($unmatched.Count -gt 0) {
    throw "these manifest sounds match no tree row: $(($unmatched | Sort-Object -Unique) -join ', ')"
}

$header = @()
$header += "// GENERATED by tools/GenSkillSounds.ps1 - do not edit."
$header += "//"
$header += "// Source: Resources/02-source-art/skill-sounds/class-skill-sounds.zip (audio-manifest.csv)."
$header += "// Joined to the class tree on (class, skill name) - the only key the two sides share."
$header += "//"
$header += "// $($out.Count) sounds across $($rows.class | Sort-Object -Unique | Measure-Object | Select-Object -ExpandProperty Count) classes."
$header += ""
$header += "// clang-format off"
$header += "const SkillSound SkillSounds[] = {"
$body = $out
$footer = @("};", "// clang-format on")

Set-Content -Path $OutFile -Value ($header + $body + $footer) -Encoding utf8
Write-Host "wrote $OutFile : $($out.Count) rows"
