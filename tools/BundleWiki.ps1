# BundleWiki.ps1 - folds the whole wiki into one self-contained HTML file.
#
#     powershell -ExecutionPolicy Bypass -File tools\BundleWiki.ps1
#     powershell -ExecutionPolicy Bypass -File tools\BundleWiki.ps1 -Verify
#
# The multi-page wiki under wiki/ is the working copy: fourteen HTML files plus data.js, wiki.css,
# wiki.js and a sprites folder. That shape cannot be published as a hosted Artifact, which must be a
# single file with no external requests at all. So this walks the pages, inlines every stylesheet,
# script and sprite, and rewrites the navigation to switch sections in place rather than load a URL.
#
# Run BuildWiki.ps1 first - this bundles whatever is on disk. It now runs this script itself, so the
# ordinary path cannot leave the two out of step.
#
# -Verify builds nothing. It recomputes the fingerprint of every file under wiki/ and compares it to
# the one stamped into the bundle, exiting non-zero when they differ. That is the check for the
# failure found on 2026-08-19: the bundle had been published from pages sixteen minutes out of date,
# and neither script said a word, because neither knew the other existed.

param(
    [switch] $Verify
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'WikiFingerprint.ps1')
$root = Split-Path -Parent $PSScriptRoot
$wiki = Join-Path $root 'wiki'
$outFile = Join-Path $wiki 'oracool-wiki-bundle.html'

if ($Verify) {
    # BOTH invariants are evaluated and reported before either decides the exit code (external audit
    # of v1.9.97, finding 7). The bundle check used to `exit 1` where it stood, so a stale bundle
    # hid the pages-versus-code answer entirely - one failed invariant masking the second, and the
    # second is the one that says whether what the player reads describes the game they have.
    $fp = Get-WikiSourceFingerprint -WikiRoot $wiki
    $stamped = Get-StampedWikiFingerprint -BundlePath $outFile
    $bundleOk = $true
    if ($null -eq $stamped) {
        Write-Host "wiki bundle: NO FINGERPRINT - built before this check existed, or by hand." -ForegroundColor Yellow
        Write-Host "             run tools\BundleWiki.ps1 to stamp it."
        $bundleOk = $false
    } elseif ($stamped -ne $fp.Hash) {
        Write-Host "wiki bundle: STALE - it does not match the pages under wiki/." -ForegroundColor Red
        Write-Host ("             pages now: {0}" -f $fp.Hash)
        Write-Host ("             bundle has: {0}" -f $stamped)
        Write-Host ("             {0} source files hashed. Run tools\BundleWiki.ps1." -f $fp.Count)
        $bundleOk = $false
    }

    # Second, separate question - and the one the 2026-08-20 audit found nobody was asking. The
    # fingerprint proves the bundle matches the PAGES. Nothing proved the pages matched the CODE,
    # and they had drifted 24 versions behind without a single check complaining: data.js said
    # 1.8.40 while the game was at 1.8.64.
    #
    # A full pages-versus-source check is not possible here (most of a page is prose). But data.js
    # carries the ORACOOL_VERSION it was generated from, so a mismatch is exact proof that
    # BuildWiki.ps1 has not been run since the last version bump - which is the drift that actually
    # happens.
    #
    # A WARNING rather than a failure: the wiki legitimately lags a bump for as long as it takes to
    # write the pages for it, and a check that fails during normal work is a check that gets
    # ignored. It exits 0 so this stays usable in a build gate.
    $version = (Get-Content (Join-Path $root 'ORACOOL_VERSION') -Raw).Trim()
    $dataJs = Get-Content (Join-Path $wiki 'data.js') -TotalCount 1 -Encoding UTF8
    $built = if ($dataJs -match '"version"\s*:\s*"([^"]+)"') { $Matches[1] } else { $null }
    if ($built -ne $version) {
        Write-Host ("wiki pages:  BEHIND THE CODE - data.js was generated at {0}, ORACOOL_VERSION is {1}." -f $built, $version) -ForegroundColor Yellow
        Write-Host  "             Run tools\BuildWiki.ps1. Anything shipped since then is undocumented."
    } else {
        Write-Host ("wiki pages:  generated at {0}, matching ORACOOL_VERSION" -f $built)
    }

    if ($bundleOk) {
        Write-Host ("wiki bundle: current ({0} source files, {1})" -f $fp.Count, $fp.Hash.Substring(0, 12))
        exit 0
    }
    exit 1
}

# The page order is the sidebar's order, and it is stated here rather than parsed out of wiki.js so
# that a page which exists but is not linked cannot silently vanish from the bundle.
$pages = @(
    @{ file = 'index.html'; id = 'overview'; label = 'Overview'; group = 'Start here' },
    @{ file = 'start.html'; id = 'start'; label = 'Getting started'; group = 'Start here' },
    @{ file = 'mechanics.html'; id = 'mechanics'; label = 'Core mechanics'; group = 'Start here' },
    @{ file = 'saving.html'; id = 'saving'; label = 'Saving and progress'; group = 'Start here' },
    @{ file = 'classes.html'; id = 'classes'; label = 'Classes'; group = 'Characters' },
    @{ file = 'skills.html'; id = 'skills'; label = 'Class trees'; group = 'Characters' },
    @{ file = 'spells.html'; id = 'spells'; label = 'Spells'; group = 'Characters' },
    @{ file = 'items.html'; id = 'items'; label = 'Base items'; group = 'Items' },
    @{ file = 'tiers.html'; id = 'tiers'; label = 'Base tiers'; group = 'Items' },
    @{ file = 'affixes.html'; id = 'affixes'; label = 'Quality and affixes'; group = 'Items' },
    @{ file = 'prefixes.html'; id = 'prefixes'; label = 'Prefixes and suffixes'; group = 'Items' },
    @{ file = 'uniques.html'; id = 'uniques'; label = 'Unique items'; group = 'Items' },
    @{ file = 'sets.html'; id = 'sets'; label = 'Item sets'; group = 'Items' },
    @{ file = 'sockets.html'; id = 'sockets'; label = 'Sockets and gems'; group = 'Items' },
    # NOTE: this list is a SECOND copy of wiki/wiki.js's NAV. A new page must be added to both, or
    # it renders in the served wiki and is silently missing from the published bundle - which is the
    # copy the user actually reads.
    @{ file = 'salvage.html'; id = 'salvage'; label = 'Salvaging'; group = 'Items' },
    @{ file = 'areas.html'; id = 'areas'; label = 'Areas and levels'; group = 'World' },
    @{ file = 'monsters.html'; id = 'monsters'; label = 'Monsters'; group = 'World' },
    @{ file = 'world.html'; id = 'world'; label = 'Quests, shrines, town'; group = 'World' },
    @{ file = 'ui.html'; id = 'ui'; label = 'HUD and windows'; group = 'Interface' },
    @{ file = 'controls.html'; id = 'controls'; label = 'Controls'; group = 'Interface' },
    @{ file = 'options.html'; id = 'options'; label = 'INI options'; group = 'Interface' },
    @{ file = 'colours.html'; id = 'colours'; label = 'Text colours'; group = 'Interface' },
    @{ file = 'palettes.html'; id = 'palettes'; label = 'Palettes'; group = 'Interface' },
    @{ file = 'engine.html'; id = 'engine'; label = 'Engine improvements'; group = 'Project' },
    @{ file = 'assets.html'; id = 'assets'; label = 'Art assets'; group = 'Project' },
    @{ file = 'debug.html'; id = 'debug'; label = 'Debug console'; group = 'Project' },
    @{ file = 'history.html'; id = 'history'; label = 'Version history'; group = 'Project' },
    @{ file = 'pipeline.html'; id = 'pipeline'; label = 'Pipeline'; group = 'Project' }
)

$css = Get-Content (Join-Path $wiki 'wiki.css') -Raw -Encoding UTF8
$dataJs = Get-Content (Join-Path $wiki 'data.js') -Raw -Encoding UTF8
$wikiJs = Get-Content (Join-Path $wiki 'wiki.js') -Raw -Encoding UTF8

# wiki.js builds the sidebar from URLs and binds on DOMContentLoaded; in the bundle the sidebar is
# written once below and sections are switched in place, so both are stripped out and the table
# renderer - the part worth keeping - is left untouched.
$wikiJs = $wikiJs -replace '(?s)/\* ---------- navigation ---------- \*/.*?/\* ---------- tables ----------', '/* ---------- tables ----------'
$wikiJs = $wikiJs -replace "document\.addEventListener\('DOMContentLoaded', buildNav\);", ''

# Sprites, inlined as data URIs. The gallery is the one thing that cannot survive as a file
# reference, and it is also the heaviest: a budget keeps the published file inside the size limit,
# and anything skipped still appears in the file table with its measurements.
$spriteRoot = Join-Path $wiki 'sprites'
$spriteMap = @{}
$budgetBytes = 7MB
$used = 0
$skipped = 0

# The encyclopedia's own pictures - one idle frame per monster family and the inventory icons - are
# inlined FIRST, ahead of the asset gallery. They are small (under half a megabyte all told) but they
# are also load-bearing: the monster and item tables are built around them, whereas a gallery sprite
# that misses the budget only costs the reader one picture on a page about pictures. Taking them
# first means a growing gallery can never silently empty the encyclopedia.
#
# They live outside wiki/sprites deliberately: BuildWiki wipes that folder on every run and refills
# it from oracool_assets, so anything parked there would not survive a rebuild.
$encRoot = Join-Path $wiki 'encyclopedia'
if (Test-Path $encRoot) {
    foreach ($file in (Get-ChildItem $encRoot -Recurse -Include *.png | Sort-Object Length)) {
        $relative = 'encyclopedia/' + $file.FullName.Substring($encRoot.Length + 1).Replace('\', '/')
        $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
        $spriteMap[$relative] = 'data:image/png;base64,' + [Convert]::ToBase64String($bytes)
        $used += $file.Length
    }
}
if (Test-Path $spriteRoot) {
    foreach ($file in (Get-ChildItem $spriteRoot -Recurse -Include *.png | Sort-Object Length)) {
        if ($used + $file.Length -gt $budgetBytes) { $skipped++; continue }
        $relative = $file.FullName.Substring($spriteRoot.Length + 1).Replace('\', '/')
        $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
        $spriteMap[$relative] = 'data:image/png;base64,' + [Convert]::ToBase64String($bytes)
        $used += $file.Length
    }
}
$spriteJson = ($spriteMap | ConvertTo-Json -Depth 3 -Compress)
if ($spriteMap.Count -eq 0) { $spriteJson = '{}' }

# Each page contributes its <main> and its own inline script. The scripts declare consts at top level
# and several share names (rows, classes), so each is wrapped in an IIFE - without that the second
# page to declare `rows` would throw and every page after it would stay blank.
$sections = New-Object System.Text.StringBuilder
$scripts = New-Object System.Text.StringBuilder
foreach ($page in $pages) {
    $path = Join-Path $wiki $page.file
    if (-not (Test-Path $path)) { continue }
    $html = Get-Content $path -Raw -Encoding UTF8

    $main = ''
    if ($html -match '(?s)<main>(.*?)</main>') { $main = $matches[1] }
    $main = $main -replace '(?s)<footer></footer>', ''

    # In-page links become section switches.
    foreach ($other in $pages) {
        $main = $main -replace ('href="' + [regex]::Escape($other.file) + '"'), ('href="#' + $other.id + '"')
    }

    [void]$sections.AppendLine('<section class="page" id="page-' + $page.id + '">' + $main + '</section>')

    $script = ''
    if ($html -match '(?s)<script src="wiki\.js"></script>\s*<script>(.*?)</script>') { $script = $matches[1] }
    if ($script.Trim()) {
        [void]$scripts.AppendLine('PAGE_SCRIPTS["' + $page.id + '"] = function () {')
        [void]$scripts.AppendLine($script)
        [void]$scripts.AppendLine('};')
    }
}

$navHtml = New-Object System.Text.StringBuilder
$lastGroup = ''
foreach ($page in $pages) {
    if ($page.group -ne $lastGroup) {
        [void]$navHtml.Append('<h4>' + $page.group + '</h4>')
        $lastGroup = $page.group
    }
    [void]$navHtml.Append('<a href="#' + $page.id + '" data-page="' + $page.id + '">' + $page.label + '</a>')
}

# The fingerprint of everything this bundle was built from, for -Verify. Computed here, at the end
# of the reading and before the writing, so it describes the inputs actually used. The stamp sits on
# line two rather than line one because the Artifact publisher takes the page's name from the first
# <title> it finds, and a comment above it would be the first thing in the file.
$fingerprint = (Get-WikiSourceFingerprint -WikiRoot $wiki).Hash

$shell = @"
<title>Oracool Codex</title>
<!-- wiki-source-fingerprint: $fingerprint -->
<style>
$css
.page { display: none; }
.page.active { display: block; }
</style>
<div class="shell">
<nav class="side">
<div class="brand"><b>Diablo Orcl</b><span id="brandver"></span></div>
$($navHtml.ToString())
</nav>
<main>
$($sections.ToString())
<footer id="foot"></footer>
</main>
</div>
<script>
$dataJs
$wikiJs

const PAGE_SCRIPTS = {};
$($scripts.ToString())

const SPRITES = $spriteJson;
const RAN = {};

document.getElementById('brandver').textContent = 'v' + WIKI.version + ' · ' + WIKI.generated;
document.getElementById('foot').innerHTML =
    'Generated from Source/ by <code>tools/BuildWiki.ps1</code> · v' + WIKI.version +
    ' · ' + WIKI.generated + ' · every table on this page is parsed from the game\'s own data.';

function showPage(id) {
    document.querySelectorAll('.page').forEach(function (p) { p.classList.remove('active'); });
    const section = document.getElementById('page-' + id);
    if (!section) { showPage('overview'); return; }
    section.classList.add('active');

    document.querySelectorAll('nav.side a').forEach(function (a) {
        a.classList.toggle('active', a.dataset.page === id);
    });

    // Page scripts run once, the first time their section is shown. Running them at load would mean
    // twenty tables built for one the reader is looking at; running them every time would stack
    // duplicate tables under the same mount.
    if (!RAN[id] && PAGE_SCRIPTS[id]) {
        RAN[id] = true;

        // Every page in the multi-page wiki owns ids like "table" and "stats" - unique per document
        // there, duplicated twenty times over here. Left alone, document.getElementById returns the
        // FIRST match in the bundle, so several pages built their tables inside the class-tree page
        // and rendered blank themselves. Both lookups are scoped to the section for the duration of
        // its own script, which keeps the page sources identical to the multi-page originals.
        const realGetById = document.getElementById.bind(document);
        const realQuery = document.querySelector.bind(document);
        document.getElementById = function (elementId) {
            return section.querySelector('[id="' + elementId + '"]') || realGetById(elementId);
        };
        document.querySelector = function (selector) {
            return section.querySelector(selector) || realQuery(selector);
        };
        try { PAGE_SCRIPTS[id](); } catch (err) { console.error('page ' + id, err); }
        document.getElementById = realGetById;
        document.querySelector = realQuery;
        // Sprite sources are data URIs in this build; the gallery writes plain paths. Two roots:
        // the asset gallery under sprites/, and the encyclopedia's monster and item art, which keeps
        // its own prefix in the map so the two can never collide on a shared file name.
        section.querySelectorAll('img[src^="sprites/"], img[src^="encyclopedia/"]').forEach(function (img) {
            const src = img.getAttribute('src');
            const key = src.startsWith('sprites/') ? src.substring('sprites/'.length) : src;
            if (SPRITES[key]) img.src = SPRITES[key];
            else img.replaceWith(Object.assign(document.createElement('div'), {
                className: 'tag', textContent: 'sprite not bundled'
            }));
        });
    }
    window.scrollTo(0, 0);
}

window.addEventListener('hashchange', function () {
    showPage((location.hash || '#overview').substring(1));
});
showPage((location.hash || '#overview').substring(1));
</script>
"@

Set-Content -Path $outFile -Value $shell -Encoding UTF8
$size = [math]::Round((Get-Item $outFile).Length / 1MB, 2)
Write-Host ("bundled {0} pages, {1} sprites inlined ({2} skipped for size) -> {3} MB" -f `
        $pages.Count, $spriteMap.Count, $skipped, $size)
Write-Host $outFile

# Read the stamp back out of the file just written. Belt and braces: it proves the stamp survived
# encoding and is where Get-StampedWikiFingerprint looks for it, so -Verify can never fail for the
# one reason that would make it worthless - a check that has never once passed.
$readBack = Get-StampedWikiFingerprint -BundlePath $outFile
if ($readBack -ne $fingerprint) {
    throw "the fingerprint did not survive the write - stamped $fingerprint, read back $readBack"
}
