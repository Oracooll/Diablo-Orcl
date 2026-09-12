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

## Published site — LIVE at <https://orclwiki.oracooll.com>

Cloudflare Pages project **`orclwiki`** (account `h.rogachev@gmail.com`). Live since 2026-09-12.
`orclwiki.pages.dev` serves the same thing.

**It is a DIRECT UPLOAD, not a Git connection.** That is the point: the repository is never pushed,
so going live costs nothing against the GitHub quota. Redeploy with

    npx wrangler pages deploy wiki --project-name=orclwiki --branch=main --commit-dirty=true

`--branch=main` is not a git branch - it is the label Pages treats as **Production**, and it is what
the custom domain serves. Deploying under any other name (the first attempt used `renderer-32bit`)
creates a *preview* at `<hash>.orclwiki.pages.dev` and leaves the live site untouched. The project
had an older production deployment from the v1.9.180 era, which is exactly what a preview deploy
leaves in place.

Two things that will otherwise waste somebody's afternoon:

- **`_headers` caches for five minutes**, and none of the generated filenames carry a content hash.
  Straight after a deploy the browser will keep serving the old `data.js`, so the sidebar shows the
  previous version and the site looks like it did not deploy. Check the server instead:
  `orclwiki.oracooll.com/data.js?cachebust=1` and read the version at the front.
- **The wrangler token has `pages (write)` but only `zone (read)`.** Deploys work; anything touching
  DNS or adding a new custom domain needs the dashboard.

The site carries the extracted Blizzard art under `encyclopedia/` - item icons, monster portraits,
skill glyphs, spell icons - per the wiki carve-out in the IP note. It is public.

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
