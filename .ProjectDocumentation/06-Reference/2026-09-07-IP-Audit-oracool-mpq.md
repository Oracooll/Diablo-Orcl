# IP audit of oracool.mpq and the repository (2026-09-07)

**Prompt:** the loading-screen paintings had been packed into oracool.mpq as PNGs; the user asked whether Blizzard's art may be distributed at all ("they are intelectual property of blizzard"), then: "can we audit oracool.mpq for blizzard IP? we might have missed something. Some of the art i have done - canvases, etc... use blizzard assets in them, but rearranged and processed in a certain way."

## The rule

Nothing of Blizzard's ships: not verbatim, not cropped, tiled, recoloured, traced or rearranged. DevilutionX's own rule, kept by this fork. Whatever the game needs of the original art it reads from the player's own diabdat.mpq / hellfire.mpq at runtime (the loading screens are now decoded from the CEL at load; the belt's spell icons are cut from the player's archive at draw time).

## The archive: clean

All 430 files under Packaging/resources/oracool_assets traced through the tool that made them to their inputs: delivered packs (with their own no-original-art statements), the user's paintings from the Resources root, commissioned green-screen sheets, the skill-sound delivery, and 21 generated colour tables. No file traces to 00-original-game-art or to an archive extraction, and none bears a vanilla name. The four spellbook bezels once cut from a vanilla screenshot were removed in the dead-asset audit (v1.9.302).

## The repository: one file, one hazard - fixed

- `tools/town.pal`: a byte copy of Blizzard's town palette, staged by build_asset_studio.cmd and committed on 2026-08-16 (v1.7.8). Untracked and ignored; the script stages it locally at build time. It remains in the pushed history from that commit onward; purging needs a history rewrite and a force push, which is the user's call.
- `tools/CutSpellbookBezel.ps1`: read a vanilla screenshot and would have put Blizzard pixels back into ui/. Removed.

## What the tools cannot see: the user's own source paintings

The audit proves the tool chain added no Blizzard pixels. It cannot see inside the paintings the user supplied, and the user says some were built from rearranged, processed Blizzard textures. The shipped files that come from user-supplied paintings, for the user to classify:

- ui/panel_bg.png (the 340x720 canvas, from "340x720 Canvas.png")
- ui/inventory_panel.png (the panel background and the grid stone texture kit)
- ui/levski_bg.png and the Levski button states (from "levskis roar.png")
- ui/book_frame_wide.png, book_frame_tall.png (the two book templates)
- ui/main_menu_bg.png, choose_hero_bg.png, hero_select_bg.png, hero_settings_bg.png, difficulty_bg.png (the front-end paintings)
- ui/middle_hud.png and the orbs (hud-v6), ui/menu_icons.png (the first HUD art), ui/burger_menu_button.png
- ui/waypoint_panel.png (the stone texture and border kit)
- objects/orclroar.cel (the monument painting), objects/orclwayp.cel (the waypoint painting)

Anything on this list that started from Blizzard pixels is a derivative and has to be replaced or repainted before a public release; the game can fall back to the vanilla art it reads from the player's archive meanwhile. The wiki bundle inlines 96 sprites copied from ui/, so the same answer applies to the published wiki.

## Outcome (same day)

The user classified the list: everything except the front-end paintings and the HUD contains reworked Blizzard textures. Those files (panel_bg, inventory_panel, the Levski skin and buttons, both book frames, the waypoint panel and icons, the monument and waypoint sprites, plus the nine 16:9 cutscene paintings he redid) left the repository for `Resources\03-private-assets\oracool_private_assets` and are packed into `oracool_private.mpq`, which the engine mounts ahead of `oracool.mpq` (v1.11.004). A build without that folder runs on vanilla fallbacks.

Distribution, decided as the "community norm" ("i am not profiting out of this product on behalf of blizzard ip"): the private archive ships inside release zips as one extra file, described in the README as non-commercial fan work for owners of Diablo, and never enters the source repository or GitHub. Verbatim Blizzard files (the extracted PNGs, `tools/town.pal`) stay out of both. The remaining exposure is pushed history (town.pal from v1.7.8, the derivative sprites until v1.11.003), which only a history rewrite and force push remove; that is deferred to the user's next push request.
