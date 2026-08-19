# BundleWiki.ps1 - folds the whole wiki into one self-contained HTML file.
#
#     powershell -ExecutionPolicy Bypass -File tools\BundleWiki.ps1
#
# The multi-page wiki under wiki/ is the working copy: fourteen HTML files plus data.js, wiki.css,
# wiki.js and a sprites folder. That shape cannot be published as a hosted Artifact, which must be a
# single file with no external requests at all. So this walks the pages, inlines every stylesheet,
# script and sprite, and rewrites the navigation to switch sections in place rather than load a URL.
#
# Run BuildWiki.ps1 first - this bundles whatever is on disk.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$wiki = Join-Path $root 'wiki'
$outFile = Join-Path $wiki 'oracool-wiki-bundle.html'

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
    @{ file = 'areas.html'; id = 'areas'; label = 'Areas and levels'; group = 'World' },
    @{ file = 'monsters.html'; id = 'monsters'; label = 'Monsters'; group = 'World' },
    @{ file = 'world.html'; id = 'world'; label = 'Quests, shrines, town'; group = 'World' },
    @{ file = 'ui.html'; id = 'ui'; label = 'HUD and windows'; group = 'Interface' },
    @{ file = 'controls.html'; id = 'controls'; label = 'Controls'; group = 'Interface' },
    @{ file = 'options.html'; id = 'options'; label = 'INI options'; group = 'Interface' },
    @{ file = 'assets.html'; id = 'assets'; label = 'Art assets'; group = 'Project' },
    @{ file = 'debug.html'; id = 'debug'; label = 'Debug console'; group = 'Project' },
    @{ file = 'history.html'; id = 'history'; label = 'Version history'; group = 'Project' }
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

$shell = @"
<title>Oracool Codex</title>
<style>
$css
.page { display: none; }
.page.active { display: block; }
</style>
<div class="shell">
<nav class="side">
<div class="brand"><b>Diablo Orcl V1</b><span id="brandver"></span></div>
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
        // Sprite sources are data URIs in this build; the gallery writes plain paths.
        section.querySelectorAll('img[src^="sprites/"]').forEach(function (img) {
            const key = img.getAttribute('src').substring('sprites/'.length);
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
