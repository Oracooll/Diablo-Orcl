# Diablo Orcl wiki

A generated reference for the whole game: items, spells, skills, monsters, the loot mechanics, the
interface, every INI option, and the shipped sprites.

## Reading it

Open `wiki/index.html` in a browser. Everything works from the file system except the sprite gallery
and the shared `data.js`, which some browsers block on `file://` - if pages look empty, serve the
folder instead:

    powershell -ExecutionPolicy Bypass -File tools\ServeWiki.ps1 -Port 8777

then open <http://localhost:8777/>.

## Keeping it true

    powershell -ExecutionPolicy Bypass -File tools\BuildWiki.ps1

Run that after any change to the data tables. It re-parses `Source/` and rewrites `wiki/data.js` and
`wiki/sprites/`. The version and timestamp in the sidebar say how current the page you are reading
is, so a stale wiki announces itself.

**Nothing in the tables is typed by hand.** Item stats come from `itemdat.cpp`, spells from
`spelldat.cpp`, skills from `oracool/class_tree.cpp`, monsters from `monstdat.cpp`, options from
`options.cpp`, and the loot constants from the modules that define them. Prose that cannot be derived
- what a mechanic is for, why a rule exists - lives in the page or in the generator beside the data
it annotates.

## Files

| File | What it is |
|---|---|
| `index.html` and the other pages | The site. Plain HTML; each page owns its own table setup. |
| `wiki.css` | The theme. Edit here. |
| `wiki.js` | Navigation and the shared sortable/filterable table. |
| `data.js` | Generated. Do not edit - `BuildWiki.ps1` overwrites it. |
| `sprites/` | Generated copy of `Packaging/resources/oracool_assets`. Wiped and refilled on every build. |
| `encyclopedia/` | The encyclopedia's own art - monster portraits, item icons, skill glyphs, spell icons. NOT wiped by the build; cut by `tools/BuildEncyclopediaArt.ps1`. |

## Published site — PARKED BY DECISION, not merely pending

The address, if it ever goes up, is <https://www.oracooll.com> via Cloudflare Pages connected to the
GitHub repository: no build step, output directory `wiki`.

**It has never been live**, and on 2026-09-12 the user decided to keep it that way for now: the
Claude Artifact below stays the only hosted copy. Read this as a decision, not a to-do.

Two things were settled at the same time, and they are the reason this section is worth reading
before anyone revives the idea:

- **The production branch would be `renderer-32bit`, not `oracool-v1-main`.** The original plan named
  the frozen V1 branch, whose last wiki commit is `cd0521a` - before the 64-rung ladder, before the
  encyclopedia, before the 2026-09-12 audits. Pointing Pages at it would serve a months-old wiki.
- **Going live means pushing, and that is the expensive part.** As of 2026-09-12 the branch is 45
  commits ahead and `wiki/` alone is 49 MB, against a standing rule not to push (free-account quota).
  Publishing also puts the extracted Blizzard art - the item icons, monster portraits, skill glyphs
  and spell icons under `encyclopedia/` - into a public repository. That follows from the wiki
  carve-out in the IP note, but it is a change of exposure worth stating out loud.

The folder is ready for it: every `href` and `src` is relative and nothing fetches, so it works at a
domain root unchanged — verified by serving it over HTTP rather than assumed.

`_headers` is already here for that day; Pages reads it from the deployed directory and it is inert
until then. It exists because none of the generated filenames carry a content hash — `wiki.css`,
`wiki.js` and `data.js` keep their names across every rebuild, so without a short browser cache the
site would serve yesterday's tables against today's pages.

## Bundled single-file copy

The bundled build is also published as a Claude Artifact:

  https://claude.ai/code/artifact/79abf513-fd1b-4f64-89aa-7c0696606337

It is private until shared from the page's share menu. To refresh it after a data change:

    powershell -ExecutionPolicy Bypass -File tools\BuildWiki.ps1
    powershell -ExecutionPolicy Bypass -File tools\BundleWiki.ps1

then republish `wiki/oracool-wiki-bundle.html` to the same URL.
