# A Fingerprint So the Bundle Cannot Lie

**Date:** 2026-08-19
**Scope:** `tools/` only. No engine change, no version bump.

## The failure being closed

The wiki exists twice: as pages under `wiki/`, and as `oracool-wiki-bundle.html`, one self-contained
file for publishing. `BuildWiki.ps1` wrote the first. `BundleWiki.ps1` wrote the second. Neither knew
the other existed.

So when the Pipeline repair went in earlier today, running `BuildWiki.ps1` updated the pages and left
the bundle serving the previous text. It printed a success line while doing so. The only reason it
was caught is that the bundle's mtime happened to be sixteen minutes behind `data.js` and something
made me look.

Two things were wrong, and they need different answers:

1. **The ordinary path could drift.** Fixed by removing the choice.
2. **Drift was undetectable.** Fixed by making the bundle carry proof of what it was made from.

## One: the build bundles

`BuildWiki.ps1` now calls `BundleWiki.ps1` at the end. Building one without the other was never what
anyone wanted, and the two-command dance existed only because the scripts grew separately.

`-NoBundle` opts out, for the case that genuinely wants it: the bundler inlines ~10 MB of sprites, so
a run that only needs `data.js` refreshed can skip it. It says out loud that it skipped, and names
the check that will now object.

## Two: the bundle carries its own fingerprint

`tools/WikiFingerprint.ps1` is the one definition of "what the bundle was built from": a SHA-256 over
every file under `wiki/` except the bundle itself, path and content both, sorted and case-folded so a
checkout on a case-sensitive filesystem produces the same answer. 99 files today.

`BundleWiki.ps1` computes it after reading its inputs and stamps it into line two of the file:

```html
<title>Oracool Codex</title>
<!-- wiki-source-fingerprint: d7caebf11a4a... -->
```

Line two, not line one, because the Artifact publisher takes the page's name from the first `<title>`
it finds.

`BundleWiki.ps1 -Verify` builds nothing. It recomputes the fingerprint and compares:

```
wiki bundle: current (99 source files, d7caebf11a4a)
```

## Why a hash and not a timestamp

The mtime comparison is what caught this by hand, and it is the wrong check to automate. Modification
times move when nothing changed - a checkout, a copy, OneDrive touching a file - and a check that
cries wolf gets ignored, which leaves you worse off than no check at all. A content hash is silent
until the content actually differs.

There is one honest consequence: `data.js` carries a `generated` timestamp to the minute, so any
`BuildWiki.ps1` run that lands in a new minute changes the fingerprint. That is not a false positive.
The bundle really does not contain the `data.js` that is on disk. It only ever surfaces after
`-NoBundle`, because every other path rebuilds the bundle in the same breath.

## Proven, not assumed

All three states were exercised against the real files rather than reasoned about:

| State | Result |
|---|---|
| Bundle predating the check | `NO FINGERPRINT`, exit 1 |
| A page edited after bundling | `STALE`, both hashes printed, exit 1 |
| Freshly bundled | `current`, exit 0 |
| Same page restored | `current`, exit 0 |

The stale case was produced by appending one HTML comment to `pipeline.html`, verifying, then
restoring it - the smallest edit that should trip it.

`BundleWiki.ps1` also reads its own stamp back out of the file it just wrote and throws if it does not
match. That guards the failure that would make the whole thing worthless: a check that has never once
passed, because the stamp never survived encoding or landed where the reader looks.

## Files

- `tools/WikiFingerprint.ps1` - new. `Get-WikiSourceFingerprint`, `Get-StampedWikiFingerprint`.
- `tools/BundleWiki.ps1` - stamps the fingerprint, adds `-Verify`, reads the stamp back.
- `tools/BuildWiki.ps1` - calls the bundler; `-NoBundle` opts out.

## The check to run

```
powershell -ExecutionPolicy Bypass -File tools\BundleWiki.ps1 -Verify
```

Non-zero means the published bundle is not the wiki.
