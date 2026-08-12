---
title: 2026-08-12 - Pulling the Disliked Armor Icons Pending a Redo
date: 2026-08-12
tags: [dev-report]
summary: User feedback on the six new equipment icons (shoulders, bracers, gloves, belt, legs, boots) was blunt - "I don't like the new armor assets, remove them from the game." Blanked the shipped icon art to a flat neutral placeholder without touching any of the underlying equipment-slot engineering, since that infrastructure is independent of what the art looks like. A ChatGPT prompt for a replacement nine-piece set went out alongside this.
---

# Pulling the Disliked Armor Icons Pending a Redo

Direct request, no ambiguity about the verdict: the six item icons from the earlier quality pass ([[2026-08-12 - Item Icon Quality Pass]], [[2026-08-12 - Fixing Torn Edges and Floating Debris in Item Icons]]) aren't good enough, pull them.

## What "remove them from the game" actually touches

Worth being precise here because it would have been easy to overreach. The six new equipment slots - Shoulders, Bracers, Gloves, Belt, Legs, Boots - are real engineering: new `ItemType`/`ILOC_*` entries, `NUM_INVLOC` grown from 7 to 13, save-format remap fixes, right-click equip, multiplayer pack validation, panel geometry. None of that has anything to do with what the icons look like, and the user's complaint was specifically about the art ("they don't look very good... bad background removal"), not the feature. So this pass touches exactly one thing: `oracool_items.cel`, the single CEL file holding all six icons (it doubles as the ground/cursor sprite too - Diablo 1 doesn't re-skin the player model for armor, so this one file is the entire visual surface of these items).

One constraint shaped the approach: `InitCursor` loads this file unconditionally at startup and treats a missing file as fatal (per the comment in `build_item_icons.cmd`). Deleting it outright would crash the game on boot. So "remove" means replace its content, not remove the file.

## The fix

Reused `tools/ItemIconCel.cs` completely unmodified - no changes to the cutting/background-removal/quantisation pipeline that file represents. Instead, six small solid-fill PNGs (one flat neutral tone, RGB 80/72/64) were generated at each icon's exact output size (56×56 for five of them, 56×28 for the belt) and run through the existing tool as substitute "sheets" with an identity crop. A uniform fill sits comfortably above both of the tool's luma cutoffs, so the backdrop flood-fill never seeds and the whole frame survives as one solid, hole-free square - the tool's own logic does the right thing here with zero new code paths to get wrong.

The six real source sheets in `Oracool.MPQ/02-source-art/items/` (outside the repo, the user's own art library) were left untouched. `tools/build_item_icons.cmd` still points at them unchanged - the next time it's run against a replacement set, it will overwrite this placeholder exactly the way it's meant to.

The debug spawn commands (`givebset`/`givemset`/`giverset`/`giveuset`/`givepset`) were left in place. They're test tooling, not shipped assets, and they're what the next art pass will need to check the replacements against.

## Verification

Same closed-loop check as the grid-grain fix: rebuilt `oracool_items.cel`, mirrored it into all three asset channels (hash-matched across all three), repacked `oracool.mpq`, then extracted `data\inv\oracool_items.cel` back out of the freshly-built archive and hash-matched it against the source - confirms the blank actually reached the shipped archive rather than repeating the earlier pipeline gap.

Debug build required re-discovering how to invoke it correctly this session - `cmake --build` run from a bare shell fails with `cstdint`/`limits.h` not found, because MSVC's `cl.exe` depends on `INCLUDE`/`LIB` environment variables that only exist inside a Developer Command Prompt. Fixed by importing `vcvars64.bat`'s environment into the shell before invoking `cmake`/`ctest` directly (rather than shelling out to a `.bat` wrapper per command, which doesn't persist environment changes back to the caller). Worth remembering for next time rather than re-discovering it again.

Clean build at `ORACOOL_VERSION` **1.1.22**, confirmed embedded in the built exe. Tests **349/351**, the same two pre-existing failures as every prior build this session.

## Also delivered: a prompt for a replacement set

Alongside the removal, the user asked for a ChatGPT prompt to generate a full nine-piece replacement set - the original six plus Helmet, Armor and Shield, so the whole equipment panel reads as one consistent set rather than six new pieces bolted onto three vanilla ones. The prompt (delivered directly in chat, not filed here) folds in what this session's icon-quality debugging actually learned the hard way:

- Flat single-color background, not white or black - dark items against black repeats the exact backdrop-removal failure mode this pipeline was built to work around, and white caused its own edge-fringing problems in the earlier white-background review.
- Generously thick connecting details - the single biggest source of the floating-debris defects fixed earlier was fine straps/wisps thinning below the surviving-alpha threshold on a ~4x downscale into a 56px cell.
- Crisp edges, no feathering - soft alpha edges are exactly what the `PostProcess` compositing step exists to clean up after the fact; starting closer to clean costs less downstream.
- Large source resolution and consistent lighting/framing across all nine, generated as separate images rather than one grid sheet, since multi-subject grids drift in scale and lighting between cells.

## Related

- [[2026-08-12 - Item Icon Quality Pass]]
- [[2026-08-12 - Fixing Torn Edges and Floating Debris in Item Icons]]
- [[2026-08-12 - Six New Equipment Slots]]
- [[2026-08-12 - Grid Grain, and a Silent Asset Pipeline Gap]]
