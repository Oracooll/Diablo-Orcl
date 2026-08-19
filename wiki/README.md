# Diablo Orcl V1 wiki

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
| `sprites/` | Generated copy of `Packaging/resources/assets`. |
