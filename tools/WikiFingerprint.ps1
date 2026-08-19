# WikiFingerprint.ps1 - the one definition of "what the bundle was built from".
#
# Dot-sourced by BundleWiki.ps1, which STAMPS the fingerprint into the bundle it writes, and by
# BundleWiki.ps1 -Verify, which recomputes it and compares. Both sides must agree on exactly which
# files count, which is the only reason this is a separate file rather than a function in one of
# them.
#
# Why a content hash rather than timestamps: the drift found on 2026-08-19 was a bundle sixteen
# minutes older than data.js, which a mtime check would have caught - but mtimes also move when
# nothing changed (a checkout, a copy, OneDrive touching a file), and a check that cries wolf is a
# check that gets ignored. A content hash is silent until the content actually differs.

$ErrorActionPreference = 'Stop'

<#
.SYNOPSIS
Hashes every wiki source file the bundle is built from.

.DESCRIPTION
Everything under wiki/ EXCEPT the bundle itself - the pages, data.js, wiki.css, wiki.js, the sprite
tree and README.md. Deliberately a whole-directory sweep rather than the $pages list in
BundleWiki.ps1: a page that exists but is not yet listed is precisely the kind of thing that should
make the check fire, and a new asset added under wiki/ should not need this file edited too.

Paths are lower-cased and forward-slashed before hashing so the fingerprint does not change when the
repository is checked out on a case-sensitive filesystem.
#>
function Get-WikiSourceFingerprint {
    param(
        [Parameter(Mandatory = $true)][string] $WikiRoot,
        [string] $BundleName = 'oracool-wiki-bundle.html'
    )

    $files = Get-ChildItem -Path $WikiRoot -Recurse -File |
        Where-Object { $_.Name -ne $BundleName } |
        ForEach-Object {
            $rel = $_.FullName.Substring($WikiRoot.Length).TrimStart('\', '/').Replace('\', '/').ToLowerInvariant()
            [pscustomobject]@{ Rel = $rel; Path = $_.FullName }
        } | Sort-Object Rel

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $acc = [System.IO.MemoryStream]::new()
        foreach ($f in $files) {
            # Path AND content: renaming a page without touching a byte of it still changes the wiki.
            $nameBytes = [System.Text.Encoding]::UTF8.GetBytes($f.Rel + "`n")
            $acc.Write($nameBytes, 0, $nameBytes.Length)
            $hash = $sha.ComputeHash([System.IO.File]::ReadAllBytes($f.Path))
            $acc.Write($hash, 0, $hash.Length)
        }
        $final = $sha.ComputeHash($acc.ToArray())
        $acc.Dispose()
    } finally {
        $sha.Dispose()
    }

    return [pscustomobject]@{
        Hash  = ([System.BitConverter]::ToString($final) -replace '-', '').ToLowerInvariant()
        Count = $files.Count
    }
}

<# .SYNOPSIS Reads the fingerprint BundleWiki.ps1 stamped into a bundle, or $null if there is none. #>
function Get-StampedWikiFingerprint {
    param([Parameter(Mandatory = $true)][string] $BundlePath)

    if (-not (Test-Path $BundlePath)) { return $null }
    # The stamp is on the second line, but read a generous head rather than assuming that: a bundle
    # written before the stamp existed has no such line at all, and must report "absent" rather than
    # matching whatever happens to sit there.
    $head = Get-Content -Path $BundlePath -TotalCount 20 -Encoding UTF8
    foreach ($line in $head) {
        if ($line -match '<!--\s*wiki-source-fingerprint:\s*([0-9a-f]{64})\s*-->') { return $Matches[1] }
    }
    return $null
}
