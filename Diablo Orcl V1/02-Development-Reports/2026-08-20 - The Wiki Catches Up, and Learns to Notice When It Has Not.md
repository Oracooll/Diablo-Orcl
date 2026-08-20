---
date: 2026-08-20
version: 1.8.64
tags: [wiki, documentation, tooling]
---

# The Wiki Catches Up, and Learns to Notice When It Has Not

The wiki was 24 versions behind the game. This closes that, and adds the check that would have
said so.

## No version bump

Deliberate. `data.js` embeds the `ORACOOL_VERSION` it was generated from, so bumping without an
engine build would immediately re-open the exact gap this unit closes — the wiki would claim 1.8.65
while documenting 1.8.64. The wiki now says **1.8.64**, and that is the truth.

## The gap the fingerprint check does not cover

The bundle-drift check built on 2026-08-19 works, and reported **in sync** throughout: 99 source
files, one SHA-256 over path and content.

It answers *"does the bundle match the pages?"* Nobody was asking *"do the pages match the code?"*,
and the answer had been no for 24 versions: `data.js` generated at **1.8.40**, `history.html`
ending at **1.8.17**.

A full pages-versus-source check is not possible — most of a page is prose, and prose is the part
`BuildWiki.ps1` deliberately keeps hand-written. But `data.js` carries the version it was generated
from, so a mismatch against `ORACOOL_VERSION` is *exact* proof that `BuildWiki.ps1` has not run
since the last bump. `BundleWiki.ps1 -Verify` now reports it.

A **warning, not a failure**, exiting 0. The wiki legitimately lags a bump for as long as it takes
to write the pages for it, and a check that fails during ordinary work is a check that gets ignored.
That is the same reasoning that made the fingerprint a content hash rather than an mtime comparison.

Both branches were exercised — the failing one by temporarily setting the version to 1.8.99 and
restoring it. A check that has never fired is a check that has not been tested.

## Content

**`controls.html`** — the letter keys were four rows out of fifteen. Now complete, taken from
`diablo.cpp`'s registrations rather than memory: **B C F G I L P Q R S T V W X Z**. Added with the
warning that matters: the keymapper does **not** reject duplicates. Bind two actions to one key and
the later registration silently wins, with nothing said at load. That is exactly how the runeword
book shipped on `B` — already the spellbook — in 1.8.57.

**`monsters.html`** — a Sizes section. Runt 75, Normal 100, Giant 120, Colossal 140, and the part
worth documenting: Runt and Giant are a **two-stage** roll (a quarter of types, then a quarter of
their individuals) and are a **pure hash** of the level seed, type and id. Not saved, not from an
RNG stream — so a monster is the same size after a reload, and a deterministic replay cannot shift.

**`ui.html`** — the runeword book, and Levski's Roar no longer described as placeholder art.

**`world.html`** — what the monument now is, plus the solidity note: solid means *approached from an
adjacent tile*, not merely impassable.

**`assets.html`** — one CEL tool per object family sharing one encoder, and the constraint that
binds them: CEL carries no frame width, so each has a constant in `objdat.h` that moves in the same
commit.

**`debug.html`** — the item-family spawners. The generated command list was already current; the
hand-written "worth knowing" cards were not.

**`history.html`** — 1.8.18 through 1.8.64, from the commit log rather than from memory. Grouped
where a run of versions is one story (the gold-hover hunt, the HUD plate's three attempts, the
runeword book's four), listed individually where it is not.

**`Pipeline.md`** — "Levski's Roar - real art" was half true, so it became two rows: the window,
still on a placeholder border; and the stash chest, back on vanilla with what the next art pack has
to avoid. Verified with `git diff --stat` — 2 insertions, 1 deletion, header intact, 35 rows to 36.
That check exists because this file was corrupted once by an edit that concatenated three rows onto
one line and looked like a clean one-line diff.

## Noted, not fixed

**Two commits both claim v1.8.32** — "Gold reaches the bottom three rows" and "Locate the eleven
typed numbers in the wiki". The history page treats them as one entry, which is honest about what
shipped. Nothing downstream reads the number, so this is a record, not a defect.

## Verification

`BuildWiki.ps1` regenerated everything and rebundled: 386 items, 59 spells, 163 skills, 137
monsters, 24 pages, 68 sprites inlined. Fingerprint `045013808f4d`, current.

Every edited page was checked structurally — `<tr>` counts, no unclosed rows, no over-long line
that would signal concatenation — and `controls.html` and `monsters.html` were **rendered and
looked at**, because on this project a structural check passing has repeatedly meant nothing.
