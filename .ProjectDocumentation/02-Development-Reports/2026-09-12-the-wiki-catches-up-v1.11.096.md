# The wiki catches up with fifteen patches

**v1.11.096** — 2026-09-12

The site was built at v1.11.081. Fifteen commits had landed since. This is the audit of what
those patches actually changed for a reader, and the answer is narrower than the commit count
suggests: **one player-visible rule, and a lot of art**.

## What the audit found

Read end to end, v1.11.082–095 is an art-and-correctness sprint. Nothing new is *encounterable* —
no new monster, item, skill, affix or area — so every generated table in the wiki was already
right. What moved was in two places only.

| Patch | Reaches the wiki? | Why |
|---|---|---|
| v1.11.082 damage palette | **yes** | white physical, blue cold, green acid — a rule a player reads off the screen |
| v1.11.084 palette re-cut | **yes** | re-cut all of `oracool_items.cel` against a corrected `town.pal` |
| v1.11.085/086/089/090 | **yes** | salvage materials, Mystic Orbs, encounter items, Knellbranch, the signet |
| v1.11.092 Sorceress glyphs | **yes** | 13 rows stopped wearing vanilla `spelicon` art |
| v1.11.083 Nightmare drops | no | a fixed bug, not a documented rule |
| v1.11.087 silent UI sounds | no | the wiki documents no sounds |
| v1.11.093 ground tumbles | no | floor art; the wiki shows inventory icons |
| v1.11.088/091/094/095 | no | comments, tests and tooling |
| 5b1ce9d7 inventory hover | no | a fixed bug |

## The damage palette

`ColorMagicDamage` was **absent from the colours page entirely**, and the cause was a generator bug
worth recording. `BuildFontColourLegend.pl` reads `RgbDefinedColors` with a regex anchored on each
entry's `// was fonts\<name>.trn` tail, then keys the result **by that filename**. Every colour had
one, because the table began life as the record of which `.trn` each colour used to be. A colour
born after stage 4 was never a file — and `ColorMagicDamage` (2026-09-11) is the first. It matched
nothing, and dropped out silently. The tail is now optional and the table is keyed by enum as well,
so the next fileless colour appears on its own.

Five "Used for" strings had gone stale the moment the palette was settled, all in the same
direction — they described the *old* assignment:

- `ColorUiSilver` — "in play floating fire damage" → front-end only (fire is red)
- `ColorBlue` — "magic damage" → **cold** damage
- `ColorOrange` — "floating magic damage" → the runeword book only
- `ColorWhite` — gained physical damage
- `ColorOracoolGreen` — gained acid

The caller counts corroborate it independently, since they are counted from the tree rather than
written down: Orange 2→1 (lost the floating magic line), Green 7→8 (gained acid), White 166→165,
Yellow 3→2 — exactly the shape of `DamageTextColor` collapsing into a one-line deferral.

Then the thing the page was missing. A player learns the element colours from the floating numbers,
and the wiki only said it *sideways*, in a "Used for" column — which is precisely how it went stale
unnoticed. There is now an explicit six-row **damage palette** table on `colours.html`, and it is
**derived**: the mapping is parsed out of `charpanel.cpp`'s `DamageTypeColor` and the hex comes from
the legend's own rows, so neither half can drift from the game without this page changing with it.
The generator asserts six elements and asserts them distinct, mirroring the engine's own
static assertion — a shared ink would stop the build rather than print quietly.

Physical is the one arm that `break`s instead of returning, taking the fallback after the switch,
because white is the colour that wants the default. The parser handles that explicitly rather than
by accident.

## The art

`oracool_items.cel` changed in five of the fifteen commits and the skill strips in a sixth, so every
item icon and thirteen glyphs on the site were stale. The refresh is a chain, and the trap in it is
that **the exporter skips any directory that already exists** — a re-run without clearing is a no-op
that looks like success. Cleared `objcurs`, `objcurs2`, `objcurs3`, re-exported, re-cut.

The archive needed no repacking: it was written at 21:33 against source art last touched at 21:29.
Checked rather than assumed.

**866 files exported — 179 + 61 + 626.** Identical to every previous run, which is the check worth
having: it says no index shifted, so none of the other icons went wrong.

What changed, and why each number is the right one:

- **311 item icons**, every single one at index ≥ 231 — i.e. entirely inside the fork's own sheet.
  Zero vanilla icons moved. That is exactly v1.11.084's footprint: the palette re-cut touched only
  `oracool_items.cel`, and the other three CELs use indices 152–159 zero times.
- **13 skill glyphs, all Sorcerer.** v1.11.092 stamped thirteen Sorceress rows. Not twelve, not
  fourteen, and no other class — the tightest confirmation in the run.

Structural checks are what kept passing while derived values were wrong, so I looked at the
artefacts. **Knellbranch** (curs 541) was the one blank frame of 626, fixed in v1.11.089; it now
draws a sabre. The smallest icon on disk is 886 bytes, so no blank survives anywhere. A Sorcerer
glyph renders as a white two-colour burst, not coloured vanilla art.

Its cursor index came from `data.js` rather than from arithmetic — my own frame-to-curs calculation
said 542, which is unchanged and wrong. The generated data knew; I did not.

## Deployed

370 files uploaded to `orclwiki`, production branch label `main`. Verified on
**orclwiki.oracooll.com**, not the preview URL: stamp `?v=1.11.096`, the damage palette section
present, the `ColorMagicDamage` row at field 29, "24 names live code draws with" (was 23), and
`curs_541.png` byte-identical on the wire at 998 bytes.

Note for the next deploy: `/colours.html` answers **308**, not 200. Pages serves clean URLs, so a
verification curl needs `-L` or it reports an empty body and looks like a failed deploy.

Bundle intact at 18 `makeTable` calls — the regression that silently emptied every table once.
