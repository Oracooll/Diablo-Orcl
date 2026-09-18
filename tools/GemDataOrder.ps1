# GemDataOrder.ps1 - sorts a GemData row's designated initialisers into the struct's declaration order.
#
# Dot-sourced by GenRunes.ps1 and GenJewels.ps1, which write `{ .idx = ..., .field = n, ... }` rows
# for the Gems[] table in Source/oracool/gems.cpp. C++20 requires designators in member declaration
# order. MSVC 14.52 let the generators' effect-by-effect order through; MSVC 14.51 does not (error
# C7560, "designators must appear in member declaration order"), which is how the rule was first met
# when the tree was rebuilt on the second machine on 2026-09-18. The order is READ from struct
# GemData in gems.cpp on every run, so there is no second list to keep in step: the generators emit
# whatever order the struct declares, and a designator naming a member the struct does not have
# stops the run instead of reaching the compiler.

$script:GemDataFieldOrder = $null

function Get-GemDataFieldOrder {
    if ($script:GemDataFieldOrder) { return $script:GemDataFieldOrder }
    $cpp = Join-Path (Split-Path -Parent $PSScriptRoot) 'Source\oracool\gems.cpp'
    $fields = New-Object System.Collections.ArrayList
    $inside = $false
    foreach ($line in (Get-Content $cpp)) {
        if (-not $inside) { if ($line -match '^\s*struct GemData\s*\{') { $inside = $true }; continue }
        if ($line -match '^\s*\};') { break }
        $code = ($line -replace '//.*$', '') -replace '/\*.*?\*/', ''
        if ($code -match '^\s*[\w:]+\s+(.+);\s*$') {
            foreach ($decl in ($Matches[1] -split ',')) {
                if ($decl -match '^\s*(\w+)') { [void]$fields.Add($Matches[1]) }
            }
        }
    }
    if ($fields.Count -eq 0) { throw "struct GemData not found in $cpp" }
    $script:GemDataFieldOrder = $fields
    return $fields
}

function Sort-GemDesignators([string]$effects) {
    $order = Get-GemDataFieldOrder
    $parts = [regex]::Split($effects.Trim(), ',\s*(?=\.\w+\s*=)')
    $keyed = foreach ($part in $parts) {
        $text = $part.Trim()
        if ($text -notmatch '^\.(\w+)\s*=') { throw "not a designated initialiser: '$text'" }
        $name = $Matches[1]
        $index = $order.IndexOf($name)
        if ($index -lt 0) { throw "struct GemData in gems.cpp has no member '$name' (in '$effects')" }
        [pscustomobject]@{ Index = $index; Text = $text }
    }
    return (($keyed | Sort-Object Index | ForEach-Object { $_.Text }) -join ', ')
}
