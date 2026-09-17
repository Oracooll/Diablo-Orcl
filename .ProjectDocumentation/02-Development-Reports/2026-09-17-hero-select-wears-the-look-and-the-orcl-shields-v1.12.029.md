# Hero select wears the look, and the sixteen Orcl shields

2026-09-17 — v1.12.029

## Why

> "Hero-select screen - add the swaps here as well."
> "Option A: by what the name suggests (recommended) - Go with this option."

Two of the gaps listed when asked whether the gear looks could be called Built.

## The hero-select screen

`oracool/hero_preview` loaded the plain town-stand CL2 from the class and `_pgfxnum` alone, so it showed
neither a swapped shield or sword nor the Barbarian's dye.

- `_uiheroinfo` gained `gearLook` - `oracool::GearLookCode`, filled in `pfile.cpp` beside `gfxnum` on
  the same free terms (the save is already unpacked there). `selhero.cpp` passes it on.
- `SetHeroPreview(class, gfxnum, gearLook)` builds the SAME request the game builds
  (`MakePlayerSheetRequest` has an overload that needs no Player; a test asserts the two produce one
  key), so the menu and the game share one cache: a look worn in play is there in the menu at once.
- A look never built before is not built on the spot - arrowing through a list of heroes would stall on
  each. It is requested, the plain sheet is shown, and `DrawHeroPreview` feeds the mixer itself (this
  screen has no game tick) until `IsPlayerSheetSettled`, then reloads. About half a second, once ever.
- The screen draws palette INDICES through a level-to-UI translation, so a sheet with colours of its own
  is shown through each colour's fallback index first. The Barbarian's shirt is one of eight blues
  there instead of sixteen; at menu scale, behind a nearest match into another palette, that does not
  show. `HeroColoursFor(class, gfxnum)` gives the dye without a Player, and the plain sheet wears it too.

The preview fits the figure to its box by whole-number scale, so the Barbarian's 120% does not make
him look bigger here - a taller sheet just fills the same box.

## The sixteen Orcl shields (Option A)

| look | shields |
|---|---|
| light tier's | Leather, Bone, Ruby, Fallen, Spectral |
| medium tier's | Iron, Steel, Crusader, Diamond, Glacial, Seraphic |
| heavy tier's | Royal, Obsidian, Infernal, Onyx, Cyborg |

By name rather than by position on their level 2-49 ladder: by position everyone past level 34 carries
the heavy look, usually over heavy armour that shows it anyway, and the swap goes quiet where most of
the game is played. A test walks all sixteen, so none can fall back to "Own" unnoticed.

## Verified how

796 tests pass (1 new). The preview was NOT looked at - it needs the front end running, and there is
no tool that renders it. What to look at: a hero holding a big shield over light armour in the list
shows that shield (after a half-second the first time); a light-armour Barbarian shows blue and grey;
arrowing quickly through the list never stalls.

## Closed

User, 2026-09-17, after an in-game pass over v1.12.029: "all works ok. dont apply on companions. mark as
built". The gear looks (v1.12.021-029) are BUILT. Companions are excluded by decision, not by omission.
