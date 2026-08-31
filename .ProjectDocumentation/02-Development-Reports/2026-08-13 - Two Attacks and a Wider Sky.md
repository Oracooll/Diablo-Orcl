---
date: 2026-08-13
version: 1.4.9
area: UI / Abilities window, HUD, front-end screens, art vault, class roster
---

# Two Attacks and a Wider Sky

Three files turned up in the MPQ folder's root and became two unrelated jobs.

> sweep the mpq root folder. you'll find two icons i want introduced into the game - Regular Attack
> and Fist Attack. Also there is a 21:9 image which i want you to place on the main menu screen and
> crop into that image according to the resolution of the monitor of the player.

## Part one: the two attacks

> Regular Attack - to be in the Skill List, to be selectable from LMB and RMB. It is a regular attack
> with a weapon. Appears automatically if player has not selected other skill and is wearing a weapon.
> Fist Attack - unselectable skill. Appears automatically on the LMB slot if character has not
> selected other skill and is wearing no weapon.

### They are not new spells

The obvious implementation is two new `SpellID` entries. It is the wrong one.

The engine already has this state. `SpellID::Invalid` in `_pRSpell` means "nothing readied", and
nothing readied is **exactly** the condition under which a click swings the weapon instead of casting.
Regular Attack is not a thing the player could acquire; it is what is left when nothing else is
chosen. Adding enum entries would mean new rows in `SpellITbl`, `SpellsData`, the save format's spell
bitmasks and every table sized by `MAX_SPELLS` - all to describe something the character already has.

So `oracool/attack_skills.h` is a **presentation** of that one state, and it makes exactly one
decision:

```
armed   -> Regular Attack
unarmed -> Fist Attack
```

That framing also answers the question the brief leaves open - why one is selectable and the other is
not, which would otherwise look arbitrary. Selecting Regular Attack means "ready the basic attack",
which is a real action: it clears the readied spell. There is nothing separate to select for Fist
Attack, because it is the same underlying state and which icon you get is decided by what is in your
hand rather than by a click. Its row exists to say so.

"Armed" is read from `_pgfxnum & 0xF` - the weapon class the engine itself already resolved from the
equipped items, and the one that decides which attack animation actually plays. Reading `InvBody`'s
two hand slots here would have been a second, parallel answer to the same question, free to disagree
with the animation on screen. Note that bits 4-5 of that field are the armour graphic
(`PlayerArmorGraphic::Medium = 1 << 4`), so the mask matters; `player.h`'s comment saying the weapon
is "the 3 lower bits" is stale, and `UsesRangedWeapon` right below it already uses `0xF`.

### Where they appear

| Place | Behaviour |
|---|---|
| Skills sheet, row 0 | Regular Attack. Clickable - clears the readied spell. |
| Skills sheet, row 1 | Fist Attack. Inert, like the auras. |
| LMB well | Always the basic attack's icon. |
| RMB well | The basic attack's icon whenever no spell is readied. |

The LMB well had been drawn empty since the HUD overhaul, waiting on an "assign a skill to LMB"
mechanic that was deferred. It turns out it never needed one: **left click already always attacks**,
and it has never been assignable. The well was the plate's one permanently-empty socket for want of an
icon to put in it, not for want of a feature.

Exactly one of the two rows is drawn lit and the other blended - the same treatment a locked aura
gets, used here as an *indicator* rather than as an availability state. That is what makes the pair
read as one status line instead of two independent abilities, and it is what tells you at a glance
which one your hands are currently doing.

A bow or a staff counts as armed, so it draws the sword icon. Correct by the brief ("a regular attack
with a weapon") if slightly literal in the art; a per-weapon icon set would be a different request.

### Two things that fell out

`ClearReadiedSpell` came out of `spells.cpp`'s anonymous namespace and into `spells.h`. Un-readying is
no longer internal housekeeping - it is the Regular Attack row's whole job - and `control.cpp`'s
shift-click on the RMB well was already writing the same two fields by hand. One named function is
what stops a third caller doing it slightly differently.

The Skills sheet now mixes two icon sources - the engine's 37px small spell icon and the 38px attack
strip - so the text column moved out of `DrawSpellRow` into a shared `RowTextX()` derived from the
wider of the two. Without it, the attack rows' text would start one pixel right of the class skill's,
in the same list.

## Part two: the main menu

The stock background is `ui_art\mainmenu.pcx`: 640x480, drawn centred, with black bars around it at
every resolution this edition offers (the curated list starts at 960x720). The new art is 1680x720 -
which is exactly the 21:9 entry in that same list.

### Cover, then centre-crop

Scaled to **cover** the screen and centre-cropped, not fitted:

```
scale = max(screenW / 1680, screenH / 720)
crop  = centred (screenW / scale) x (screenH / scale)
```

| Resolution | scale | crop from the master |
|---|---|---|
| 960x720 (4:3) | 1.000 | 960x720 - pure crop, 1:1 pixels |
| 1280x720 (16:9) | 1.000 | 1280x720 - pure crop, 1:1 pixels |
| 1680x720 (21:9) | 1.000 | the whole image |
| 1920x1080 | 1.500 | 1280x720, upscaled |
| 3440x1440 | 2.048 | 1680x703, upscaled |

Sampling is bilinear at pixel centres, which at every 720-tall mode lands on exact integers and
degenerates to a straight copy - so the resolutions the art was drawn for get it pixel for pixel
rather than a resampled version of it.

The painting has a gold frame drawn into it, centred, 881x515. The narrowest crop any listed
resolution asks for is 960 wide by 703 tall, so the frame survives intact everywhere. That is worth
recording: it is the constraint that decides whether "cover" is safe here at all.

### The palette is not touched, and that is the whole trick

`LoadBackgroundArt` loads the palette **out of the .pcx it reads** and applies it globally. The logo,
the focus arrows, the cursor and every font colour on this screen index into that same palette, so
shipping our own would recolour all of them. The UI text ramps alone occupy palette entries 176-191
(gold) and 224-239 (grey), via `fonts\goldui.trn` and friends, which map the fonts' 192-207 source
ramp onto them.

So the original palette still loads, and the new art quantizes into it. Measured before committing to
it rather than assumed: `mainmenu.pcx` is a near-black plate using **five** entries out of a full
256-colour palette, and the other 251 are a general-purpose spread. Mean per-pixel error came out at
**6.5/255**. The loss is confined to some warmth in the torch glow; the night scene is exactly the
kind of image that palette happens to cover well.

The stock background is **replaced**, not layered under: it is a 640x480 plate drawn centred, so
leaving it in the item list would paint a near-black rectangle over the middle of the new art. If the
asset is missing, `AddMainMenuBackground` returns false and the caller falls back to the original -
a menu with no background at all is a black screen.

The prepared sprite is cached against resolution and palette, so the one-off cost of scaling and
quantizing 1.2 megapixels is paid once per session rather than on every return to the menu.

## Part three: the wells, measured (1.1.90-91)

> can you enlarge the new icons to fit more snug in the LMB and RMB slots? And also measure precisely
> pixel by pixel their alignment in the slot. Make them dead center.

Measuring turned up a real bug underneath the cosmetic ask.

### The wells are not where their rects say they are

`ScalePlateRect` truncates each scaled edge to a whole pixel. For both wells all four edges truncate
**downward**, so each rect's centre sits up to a pixel left of and above the opening it stands for:

| | true opening | true centre | truncated rect | rect centre |
|---|---|---|---|---|
| LMB | x 5.914..55.115, y 6.150..57.480 | (30.514, 31.815) | (5,6) 50x51 | (30.0, 31.5) |
| RMB | x 300.649..349.613, same y | (325.131, 31.815) | (300,6) 49x51 | (324.5, 31.5) |

Centring inside the rect therefore put the icon **one pixel left and one pixel high in both wells**.
At 38px inside a ~49x51 opening that was lost in the margin. It would not have been at 46.

This is also, in hindsight, what `DrawSpell`'s hand-tuned `RmbIconNudge { 1, 3 }` had been half
discovering since the art pass: the `+1` on x is exactly this correction, arrived at by eye. That
nudge is left alone - the engine's spell icon art genuinely is off-centre inside its own 37x38
sprite, so its right position is not a pure geometric centre and re-deriving it is a separate job.

`CentreInWell` works from the unrounded source geometry and rounds once, at the end. Three
`static_assert`s hold it: the icon must fit the narrower well, it must **not** be more than 4px
narrower than it (the "has it gone slack" half - a later plate change that widened the wells would
otherwise silently restore the rattle this replaced), and the two computed origins are pinned to
their hand-derived values so a change to the plate scale cannot quietly shift both icons.

### 38 -> 46 -> 38

The enlargement itself did not survive, and the reason is worth recording because it is the more
important constraint.

46 was chosen off a rendered comparison at 38/44/46/47/48: at 47 the RMB's right-hand gap falls to
0.61px and at 48 to 0.35px, under a pixel, so any later change to the art or the scale factor would
put the icon on the bezel; 46 kept every gap above 1.1px while visibly filling the recess. It could
not simply replace the 38px strip - the Abilities window's row is 44px tall and `hud_art` blits
unscaled - so the cutter briefly emitted two strips from one source in one pass.

Then, on an explicit call:

> shrink the new icons to the size of Repair Skill icon when you are done. We need to have
> consistency.

Which is right, and identifies something the "fit the well" framing missed entirely. **The RMB well
is not a fixed slot for these icons.** It alternates between the attack icon and whatever spell or
skill is readied, and that one comes from the game's own `spelli2` CEL at **37x38** and cannot be
resized. A 46px attack icon therefore made the slot's contents change size depending on its state -
a bigger, better-fitting icon at the cost of the well visibly resizing every time you readied or
cleared a spell.

So the target was never the well. It is the icon the same well draws the rest of the time. Back to
one 38px strip (38 not 37 because `DrawStripIcon` derives its cell size from the strip's height and
so needs square cells; a pixel of extra width is invisible next to a state change), which also keeps
the attack icons on the same rhythm as the aura and Barbarian strips throughout the Abilities window.

`SkillWellIconSize` and its `static_assert` survived the round trip, but the pair is now asymmetric
on purpose: the "must fit the well" bound stays, and the "must be snug against the well" bound is
gone, with a comment saying why an obvious-looking invariant is deliberately absent.

The **exact centring is kept** - it is orthogonal to size, and at 38px it still moves both icons a
pixel right and a pixel down onto the openings' true centres.

The cutter also kept a check that matters more than it looks: each cell's opaque bounding box must
fill the whole cell. The wells centre the **cell**, so art that did not fill its own cell would sit
off-centre no matter how exact the arithmetic. Both cells pass.

## Part four: the art vault's root (1.1.91)

> rename and sort the files in root mpq.

Seven loose files in `Oracool.MPQ/`'s root - two source sheets, their two description documents, two
raw `ChatGPT Image 13.08.2026 г., 14_16_36.png`-style exports, and the menu background master - filed
into `02-source-art/` under the vault's own kebab-case convention, which the README has documented
since the folder was reorganised. The root is now the seven numbered folders and `README.md`.

Sorting turned up a live breakage. `CutBarbSkills.ps1` and `CutPaladinAuras.ps1` both read their
source sheets from **`C:\Users\hroga\Downloads\`**, and those files are no longer there - the loose
copies in the vault root were the only ones left. Both scripts were unrunnable and nothing said so.
They now read from the vault, beside the icons they cut, and use `Resolve-Path` like the other
cutters (`Bitmap::FromFile` resolves a relative path against the *process* working directory, not
PowerShell's, and the two are not always the same).

All three cutters were re-run end to end afterwards - 2, 24 and 18 icons - as proof rather than
inspection.

The generated strips in those folders keep their shipped filenames (`aura_icons.png`,
`barb_skill_icons.png`, `attack_icons.png`) rather than being renamed to the convention. They are
archive copies of what ships under exactly that name in `ui\`, and the correspondence is worth more
than the consistency.

## Part five: Spells for everyone again (1.1.92)

> enable again spells ability sheet for all characters

Reversing [[2026-08-13 - Two More Sheets, and the Speedbook Retires]]'s "reserve spells ability sheet
for sorcerer only". `IsSheetAvailable(Spells)` is unconditional again.

Kept as a comment rather than deleted, because the engine agrees with the reversal: book spells are
not class-gated at all. A spell is memorised by reading its book, and any class that finds one can -
so reserving the sheet was hiding a list of things a Paladin can genuinely learn.

Two comments elsewhere had gone stale with it and were rewritten rather than left lying: the arrow
guard (`AvailableSheetCount() > 1`, which was justified by "a Rogue is down to Skills alone" and no
longer is) and `FirstAvailableSheet`'s, which now says the quiet part - it always returns Spells
today and stays a search precisely *because* availability has already changed twice.

## Part six: a background for the settings screen (1.1.93)

> Apply "Chracter Select and Settings Screen Background.png" from root mpq folder as background for
> Settings page

Same treatment as the main menu, and the module grew a second user, so `oracool/main_menu_bg` became
**`oracool/ui_backgrounds`**: a `UiBackground` enum, one cache slot per screen, and a `static_assert`
tying the enum to the asset-path table so an entry cannot be added without its file.

Per-screen slots matter here rather than being tidiness. Each slot caches against the palette it was
built under, and the two screens load *different* palettes - the main menu takes `mainmenu.pcx`'s,
the settings screen takes `ui_art\diablo.pal` via `UiLoadBlackBackground`. A single shared slot would
have rebuilt 1.2 megapixels every time the player moved between the two. (The palettes differ in 18
bytes out of 768, so the quantization quality carries over unchanged - it was worth measuring rather
than assuming, since the whole approach rests on the target palette being a general-purpose spread.)

`UiLoadBlackBackground()` still runs, and that is deliberate: it is what loads the palette the art
has to quantize against. It leaves `ArtBackground` empty, which is *why* this screen was a black
plate - so there was nothing to replace, only something to add, and `UiAddBackground` stays as the
fallback where it goes on doing exactly what it did before: nothing.

Checked offline the same way as the menu, with the list, title and logo boxed over the crop: at
1280x720 the seven-item list sits over the dark ruins and the gold reads cleanly. One watch-for that
the render cannot settle - the per-option **description** text below the list is `ColorUiSilverDark`,
and at some list lengths it lands on the wet cobbles, which are the brightest thing in the lower
half of the painting.

## Part seven: the settings screen's layout (1.1.94)

> this is the Settings screen. Arrange items alphabetically. Increase the font of the menu items and
> bring them and the Diablo animated logo a bit lower to align the whole package vertically.

**Alphabetical categories.** Only the display order moves: each row's value stays the category's index
into `GetCategories()`, which is what `ItemSelected` looks it up by (`selectedCategory =
sgOptions.GetCategories()[vecItemValue]`). Sorting the rows and leaving the values alone is the whole
change. `itemToSelect` is resolved after the sort, so coming back out of a category still lands on it
rather than on whatever now occupies its old position. Names are compared as displayed, because
"alphabetical" means alphabetical in the language on screen.

**Bigger font, on the categories list only.** `FontSize24` -> `FontSize30`, with the row pitch
following the font's own line height from `text_render.cpp`'s `LineHeights` (24 -> 26, 30 -> 38)
rather than being eyeballed - that table is why the old pitch of 26 sat flush.

Deliberately not on the option lists. Their rows are "Name: Value" and already need two-line handling
to fit at 24, and `IsOptionTooLong` measures against `GameFontTables::GameFont24` **by name** - so
raising those without changing that measurement in step would leave two-line detection quietly lying
about what fits.

**Vertical centring.** The block is logo, title at +161, list at +204, and 80px reserved for the
option description. The list is the only variable-height part, so the block's extent falls out of its
height, and the whole thing is offset by `(screenHeight - blockHeight) / 2`.

The clamp at zero matters: a list long enough to hit the existing `screenHeight - 272` cap fills the
screen, `blockTop` comes out negative, clamps to 0, and the layout is byte-for-byte the old one. So
Keymapping and Padmapping - the long lists that were already scrolling - do not move at all. Only
screens with slack to spare do.

The logo and title had to move down the function to get this: they were pushed from the screen's top
edge before the list items even existed, and the block's height cannot be known until they do. They
are still added after the background and before the list, so the draw order is unchanged.

Note the two requests pull against each other. At `FontSize30` and the font's natural pitch of 38 the
nine-category list grows by about 130px, which is most of the slack there was to centre with - the
block came out 702 of 720 and the centring could only add 9px. On the user's call the pitch went to
**34**, which buys 44px back and puts `blockTop` at 31.

That tightening exposed something worth keeping. A row **taller** than the font's line height is
fine - it is how the main menu gets its blank space (86px rows at `FontSize30`), the text simply sits
at the top. A row **shorter** than it silently clips: `DrawString` anchors each glyph's bottom at
`rect.y + lineHeight` and clips at `rect.y + rect.height`, so 34px rows of 38px-line-height text lose
the bottom four pixels of every glyph - and this list has *Gameplay*, *Keymapping* and *Padmapping*
in it, descenders first.

`UiList` therefore gained an optional explicit line height, defaulting to -1 (the font's own, so every
existing caller is untouched), and the settings list passes its row height. The row pitch and the text
line height were always two different numbers; the widget had just never been asked to tell them
apart.

## Part eight: the hero-select screen (1.1.95)

> apply character-select-and-settings-background-master-21x9.png file as background for Select Hero
> game screen

The same painting the settings screen uses, so the shipped asset was renamed `settings_bg.png` ->
**`hero_settings_bg.png`**: the enum entries are named for the screens because that is what a caller
knows, but a file serving two screens should not be named after one of them.

Two enum entries, one file. They need **separate cache slots** even though they share the art,
because a slot caches its quantized result against the palette it was built under, and these two
screens load different palettes (`ui_art\diablo.pal` versus `ui_art\selhero.pcx`'s). One shared slot
would re-quantize 1.2 megapixels every time the player moved between them.

`AddUiBackground` gained an `atFront` flag for this. The hero-select screen adds and removes its
background around modal dialogs - `RemoveSelHeroBackground` erases `vecSelHeroDialog.begin()` - so its
background has to be the first item, not merely an early one.

## Part nine: rearranging the character-select screen (1.1.96-97)

Six changes, all to `selhero_List_Init` and `selhero_Init`. At 960x720 (`uiPosition` = 160,120):

| # | Asked | Landed |
|---|---|---|
| 1 | logo up ~120 | `UiAddLogo` default y is the UI rect's own top (120 here), not 0 - so -120 puts it flush at **0** |
| 2 | title bigger, up ~100 | FontSize30 -> **42**, y 281 -> **181** |
| 3 | list right until the pentagram is nearly flush | x 425 -> **632**, right edge 952, **8px** off the screen edge |
| 4 | drop the "Select Hero" sign | removed |
| 5 | double the gap | item height 26 -> **52** |
| 6 | four buttons, 800px line, 50px up | row at y **632..670**, cells of 200 from x 80 to 880 |

Four of those pulled something else with them.

**"New Hero" was never a button.** It was the last row of the character list, dispatched by its value.
It is a real `UiArtTextButton` now, calling the same branch - which empties the list on a fresh
install, and *that* turned up two latent bugs in `UiInitList` that no screen had ever been able to
reach:

- `SelectedItemMax = std::max(size() - 1, 0)` wraps `size_t` to `SIZE_MAX` on an empty list, after
  which the guarded `GetItem(selectedItem)` indexes an empty vector.
- An empty list has a viewport of 0 against a max of 0, which reads as "0 >= 1, there is more to
  scroll to" and drew a scrollbar beside nothing.

Both fixed at the source rather than worked around in the caller. (With no characters, OK now does
what New Hero does, because `SelheroListSelect(0)` with a save count of zero *is* the new-hero
branch - which is the right behaviour, if by luck rather than design.)

**The scrollbar moved to the list's left.** With the list against the screen edge there is no room on
its right, and a scrollbar drawn off the edge is a control the player cannot reach.

**The viewport is derived, not fixed at 6.** At double the pitch, six rows would run straight through
the new action row. It is now `(buttonRowTop - gap - listTop) / itemHeight`, so the list cannot
overlap the buttons whatever the pitch - at the cost of four visible characters at 720 tall instead of
six.

**Two rects grew to match their fonts**, the same trap as the settings list: `DrawString` anchors a
glyph's bottom at `rect.y + lineHeight` and clips at `rect.y + rect.height`. The title needed 42
rather than 35 at FontSize42, and the buttons 38 rather than 35 at FontSize30 - the latter was already
shaving three pixels off every label, invisibly, because none of *OK*, *Delete*, *Cancel* has a
descender.

Two consequences worth an eye in game, both falling out of what was asked rather than chosen:

- **The focus pentagram grew.** `DrawSelector` picks its sprite from the row height: under 30 it uses
  the 20px one, 42 and over the 42px one. Doubling the pitch to 52 crosses that line, so the pentagram
  goes from 20px to 42px beside 24px text.
- **The other hero screens did not move.** Class selection, name entry and "Character Exists" still
  put OK/Cancel at the old `uiPosition.y + 429`, so stepping into New Hero moves the buttons.

## Part ten: more hero-screen work, and where it stopped (1.1.98)

Four of the six things asked for are in; the fifth is a new screen and is not.

**The character list is vertically centred.** Between the title's bottom and the action row, with both
the viewport and the top derived from that band. It grew from four visible rows to **seven**, simply
by no longer starting at the offset the old 640x480 layout put it at.

**The other hero screens' buttons line up.** Class selection, name entry and "Character Exists" now
put OK/Cancel at `HeroButtonRowTop()` - the same height as the character list's action row - so
stepping into New Hero no longer jumps them up the screen. Their x positions are untouched; only the
height was asked for.

**The delete confirmation shares the hero/settings painting.** It reuses the Settings cache slot
rather than getting its own, because it loads the same palette (`ui_art\diablo.pal` via
`UiLoadBlackBackground`) and a slot caches against nothing else.

**`ui\choose_hero_bg.png` ships**, packed and wired as `UiBackground::ChooseHero`.

### What is NOT done, and why

The Diablo 2 style class-selection screen - figures as clickable hotspots, a pentagram jumping between
them, the class name above each head, "coming soon..." under the Necromancer, a name box on the same
screen with OK gated on it, the logo moved to the top, and the new background actually switched on.

That is not a variant of the existing class screen, it is a different screen. Switching it on means:

- **Suppressing the portrait and stats panel.** They live in `vecSelHeroDialog`, which is built once
  in `selhero_Init` and shared by every sub-screen; only `vecSelDlgItems` is rebuilt per screen. The
  campfire painting cannot carry a hero portrait and a stat block on top of it.
- **Replacing `UiList` navigation with hotspot navigation.** Six figures across the width of the
  screen is not a list, and `DrawSelector` only knows how to flank a list row.
- **Merging name entry into the class screen.** Today it is a separate step (`SelheroNameSelect`);
  "OK inactive until the name box is filled" only means anything once both are on screen together.

It is also the one piece here that cannot be verified without playing it. Stopping at the boundary
rather than landing half a screen is deliberate.

One measurement worth having before that work starts: the master is 1680x720 like the others, so at
**960x720** the cover-crop takes x 360..1319 - and the outermost two figures sit within about 70px and
45px of the screen edges. Every listed resolution keeps all six, but 4:3 is the tight one, and any
"class name above the head" label on the outer two will want clamping.

## Part eleven: the character-select screen shows the character (1.1.99)

> Lets keep it as it is but we will remove the current image and the stats and will substitute them
> with full size animated character.

The class portrait (a 180x76 painting) and the five stat rows are gone. In their place the screen
draws the character's **own in-game sprite**, animated, in the gear it actually wears.

### Three things had to be true, and all three were

**The sprites load outside a game.** `plrgfx\<class>\<caw>\<caw>st.cl2` - the TOWN stand, which is the
idle breathing loop rather than the dungeon's shorter, dungeon-lit one. `LoadCl2Sheet(path, width)`
needs no `Player`: three prefix letters and a frame width from `PlayersSpriteData` are the whole
input.

**The gear came free.** This looked like it would need save-format work. It did not:
`pfile_ui_set_hero_infos` already does a full `UnPackPlayer` + `LoadHeroItems` + `CalcPlrInv` into
`Players[0]` before calling `Game2UiPlayer`, so `_pgfxnum` - armour in the high nibble, weapon in the
low - is sitting there fully computed. It was simply never copied into `_uiheroinfo`. One field, one
assignment, and a level 30 character shows up in the plate and axe it is carrying rather than in a
generic class pose.

**The palette was the only real obstacle.** CL2 pixels are palette INDICES, and a front-end screen has
a UI palette loaded, not a level one - drawn raw the character comes out in entirely the wrong
colours. So `oracool/hero_preview` builds a 256-entry translation from the level palette to whatever
palette the screen currently has and draws through `ClxDrawTRN`, which is the same mechanism the
engine already uses for class tints. Rebuilt only when the screen's palette changes.

Two details in that translation worth keeping:

- **Any level palette will do.** Player sprites live almost entirely in the palette's global half
  (128-255), which is identical across town and every dungeon type - that is why a character looks
  the same in the catacombs as in town. Town's is the one that always exists.
- **Index 0 is excluded as a destination.** The scaling pass treats 0 as "no pixel here", so a
  translation that could emit it would punch holes through the character wherever the level palette's
  colour happened to be nearest the UI palette's entry 0.

### Drawn, not widgeted

The figure is rendered directly in the dialog loop rather than as a `UiItemBase`. It animates off the
shared frame clock and has nothing to click, so a widget would have meant a new `UiType`, a new
`Render` overload and a case in the dispatch - all ceremony, no benefit. It renders at 1:1 into a
scratch surface (`ClxDrawTRN` cannot scale), is blown up **2x** with nearest-neighbour so the pixel art
stays pixel art, and lands centred in `HeroPreviewRect()` standing on its bottom edge.

`SelheroSetStats` kept its name. Five call sites mean it, and what it does has not changed - it is
still "make the left-hand side describe this character". It just describes them by showing them.

One inherited wrinkle now visible that was not before: **Bard and Barbarian have no sprites of their
own.** `GetPlayerSpriteClass` maps them onto Rogue and Warrior when the Hellfire artwork is absent, so
a Barbarian shows the Warrior's body. That is existing engine behaviour, not something the preview
introduces - but a stat block never showed it and a picture of the character does.

## Part twelve: screenshots in the menus (1.2.0)

> you need to make possible screenshot taking while in menus. i cant take ss now. maybe there is
> limitation.

There was, and it was two limitations stacked.

**The key never arrived.** "Screenshot" is a Keymapper action, and the Keymapper is only consulted
while a game is running. On the title screen, the main menu, settings, character select or any dialog,
the key reached nothing at all - there is no binding table in that event loop to reach.

**And the function could not have run there anyway.** `CaptureScreen` opens with `DrawAndBlit()`,
which renders the dungeon view and the HUD. Neither exists on a front-end screen.

So `CaptureUiScreen()` captures the UI surface as it already stands - the menu loop has drawn that
frame this iteration, so the frame to save is simply the one on screen. The fiddly half (grab the real
palette BEFORE the red flash tints it, write, delay, restore) is shared with the in-game path through
a common `CaptureTo`, so the two cannot drift.

Bound in `UiHandleEvents` to **Print Screen and F12**. F12 is not decoration: Windows 11 binds PrtScn
to the Snipping Tool by default and swallows it before the game sees it, which is the likeliest reason
the key appeared dead even where it was bound. F12 is unclaimed by both the OS and these screens.

### The frame was captured too early (1.2.2)

The first menu screenshots came out with **no list, no buttons and no scrollbar** - just the
background, the logo, the title and the character. Not a render bug: the player was navigating those
controls at the time.

Events are polled at the TOP of `UiPollAndRender`, before `UiRenderListItems` draws anything in
`gUiItems`, and the previous frame's copy of those controls has already been wiped by this frame's
background blit. So a capture taken on the keypress saves a screen mid-build.

The keypress now only sets a flag; the capture happens after `UiFadeIn()`, where the frame is complete
and presented. Worth recording because the failure is invisible in code review - the capture call was
in a perfectly reasonable place, and only a screenshot of a screenshot showed it was the wrong one.

### The folder (1.2.1)

> why that folder? why not the default ss folder?

Because it was never chosen - it was `paths::PrefPath()`, wherever the game keeps its saves and .ini.
For this build that resolves to `build\x64-Debug\Saved_Games\`, so screenshots were landing inside the
build output: a place you go to run a game, not a place you go to look at pictures.

They now go to the folder Windows registers for exactly this (`FOLDERID_Pictures` + `Screenshots/`,
the same one Win+PrtScn uses), falling back to `PrefPath` if the shell cannot resolve it or the
platform has no such notion.

`<shlobj.h>` is included **last and with `NOMINMAX`**, which is not fussiness: `windows.h` defines
`min`/`max` as macros, and putting it at the top of the file turned every `std::min` in the headers
below into a syntax error - twenty of them, all in files that had not been touched.

Version rolled 1.1.99 -> **1.2.0** rather than 1.1.100, which is only a nicer-looking number, not a
statement about the release.

## Part thirteen: the list moves over the button, and everything grows (1.2.3-1.2.4)

> i want the list with heroes to be over the New Hero button, not flush to the right edge.

A reversal of part nine's instruction, and the better arrangement: the list and the button it sits
above now share a centre line, so the two read as one column instead of two things that happen to be
on the same screen. `HeroListX()` derives that x from `HeroButtonRect(NewHeroButtonIndex, ...)` rather
than from the screen edge, which means the 800px button row remains the single thing positioning both.

> 1. increase font of buttons and hero names. 2. increase size of hero three times.

Buttons went to **FontSize42** and character names to **FontSize30**, with the row pitch out to 52px to
carry the taller line. The figure went from 2x to **3x** - not to 3x on the way to 4x: 4x is not
available. The preview area is 373px tall at 720 and the stand frame is 96px, so 384px would overflow
the screen it stands on. `PreviewScale` carries that as a comment because the next person to raise it
will find out the hard way otherwise.

> 3. replace the pentagrams cursors with golden border sitting below the selected item.

Which shipped as a 3px rule at gold index 178, inset 10px from each end, drawn under the row. One
detail in it survived into what replaced it: a row whose height is padded for spacing - the 52px
character rows, the trimmed main menu - would otherwise put the rule at the bottom of the *padding*
rather than under the *words*, so it was clamped to the tallest single-line row height the game already
used. The rule itself lasted one version. See part fifteen.

`ArtFocus` - the three spinning pentagram sprite lists - is still loaded and freed and is now drawn
nowhere. Left in place deliberately: putting the pentagram back is a one-function change for as long as
that stays true.

## Part fourteen: focus leaves the list (1.2.5)

> build the button navigation

> This selector should be able to be navigated to the buttons at the bottom of the screen.

The engine has no concept of this. `SelectedItem` is an index into `gUiList` and nothing else; a
`UiArtTextButton` has an `Activate()` and has never had keyboard focus in its life. So the ring is
implemented **in `selhero.cpp` only**, not bolted onto the shared focus model where it would have to be
correct on every screen in the game.

`UiPollAndRender` grew an optional event handler, called before `UiFocusNavigation` and
`UiHandleEvents`. Returning **false** hands the event on untouched, and that is the whole safety
argument: Escape, the mouse, the controller and every key the row does not claim reach exactly what
they reached before.

- **Down** only leaves the list once the list has nowhere further to go, so it still walks the
  characters first.
- **Up** returns to the list from anywhere in the row.
- **Left/Right** walk the buttons, skipping disabled ones - Delete greys out whenever the highlighted
  row is not a real character, and stopping the ring on a greyed-out word looks like the screen has
  hung.
- **Return** activates. Nothing may touch `HeroActionButtons` after that call: activating New Hero or
  Cancel rebuilds `vecSelDlgItems`, and every pointer in that array points into what was just freed.

While focus is on a button the list must stop marking a row, or the screen shows two selections at
once - hence `UiListSelectorHidden`, which is the one piece of this that had to be shared.

Worth recording for the next person who reaches for it: `GetMenuHeldUpDownAction()` reads the
controller stick and dpad only. It is not the keyboard path, and wiring the row into it would have
produced something that worked on a gamepad and did nothing on a keyboard.

## Part fifteen: the border becomes a glow (1.2.6)

> the border bellow the items in not nice. remove it. try finding a way to make the selectem item
> glow!

The rule is gone. The selected item now **glows**: its own text is redrawn several times around itself
in the dark gold, and then the real text is laid back on top.

Core last is the whole trick. The glyphs keep their exact edges, so the row does not simply look
fatter - it looks lit.

### Why not a translucent halo

The obvious implementation is a soft halo blended into the background, and it is not available in a
menu. Blending goes through `paletteTransparencyLookup`, and only `LoadPalette(..., blend = true)`
rebuilds it. The front end never asks: `UiLoadDefaultPalette` passes `false` and `LoadPalInMem` just
copies. So on any of these screens that table still holds whatever the last **dungeon** palette
generated, and a blended halo would come out in colours from a palette that is not on screen.

Drawing text on text needs no table.

### Why it fades instead of blobbing

Three rings, two colours, measured out of the shipped files rather than assumed:

- The **innermost** ring is `ColorUiGold` - `fonts\goldui.trn`, the font's 192-207 ramp onto palette
  indices **176-191**. In `ui_art\diablo.pal` index 176 is (255,227,164) and 191 is (20,11,0); the ramp
  runs light to dark. This is the same gold the text itself uses, so the letters sit in a band of
  full-brightness gold.
- The **outer two** are `ColorUiGoldDark` - `fonts\golduis.trn`, the same ramp compressed into
  **178-190**. They top out dimmer and bottom out at the background.

Rings are drawn **outermost first** so each overdraws the last, which is what puts the brightest part
of the aura against the letters. The softness inside each ring is free: the glyphs are already
antialiased across those ramps, so every offset copy feathers at its own edges.

### The dead end, recorded because it was cheap to check and expensive to ship

A white-hot core with a gold aura would be the textbook version, and `ColorWhite` exists. But
`white.trn` maps onto indices 242-254, and in the **UI** palette those are not a white ramp at all -
they are the VGA/system block: 249 is pure red, 250 pure green, 252 pure blue. `ColorWhite` is an
in-game-palette colour. Used in the front end it would have printed the selected row in primary
colours. `ColorWhitegold` (193-207) is likewise a burnt-orange ramp here, not a white one.

Which leaves 176 as the brightest thing in the gold family, and the text already uses it. The glow had
to be built by adding an aura *below* the text's brightness, not a core above it.

### The colour has to replace, not join

`GetColorFromFlags` returns on the **first** colour bit it finds, in its own fixed order. ORing
`ColorUiGoldDark` onto a row that already says `ColorUiGold` therefore changes nothing at all - it
would silently draw a second copy of the row in the same gold, i.e. bold text and no glow. Hence
`AllTextColorFlags` and `WithHaloColor`, which mask every colour bit out before setting the halo's.

### The pulse, and its removal (1.2.8)

> can you make glow brighter and stop it from blinking?

It shipped at 1.2.6 breathing between a one- and two-pixel radius every 700ms, on the reasoning that it
replaced something that moved (the spinning pentagram). Seen in game that reads as blinking, not as
breathing, so it is gone - `GlowRadius` is a constant 2 and the aura is steady.

Brighter came from the ring colours. Both rings were `ColorUiGoldDark`, which by construction tops out
below the text's own brightness; the inner ring is now `ColorUiGold`, the full 176-191 gold, so the
letters are wrapped in the brightest thing this palette has rather than in a dimmer version of
themselves. There is nothing above 176 to reach for - see the dead end above.

### And wider (1.2.9)

> increase radius then.

Which is the only lever left once the inner ring is already at the palette's ceiling: `GlowRadius` 2 ->
**3**, so the aura spreads a pixel further. The colour rule is unchanged - innermost bright, the rest
dark - so the extra ring lengthens the falloff rather than thickening the hot band.

It fits, and the tight case is worth writing down. The settings menu's 34px rows are the smallest pitch
any of these lists uses; `DrawString` anchors a glyph's bottom at `rect.y + lineHeight`, so at
FontSize30 the glyph band runs roughly y+12 to y+34 within the row, leaving **12 clear pixels** between
one row's glyph bottom and the next row's glyph top. A 3px aura sits inside that with room to spare.
Past it the aura would reach the neighbours, and the direction that matters is *upwards*: rows are
drawn in ascending order, so a halo spreading up would tint the descenders of the row above, which has
already been drawn and will not be redrawn over it.

One note kept from the animated version, because it will catch the next person who reaches for that
clock: `GetAnimationFrame`'s second parameter is named `fps` and is a **millisecond divisor**. The
period is `frames * fps`. Reading it as frames-per-second gives an answer 3600x wrong in the wrong
direction.

### Three widgets, one effect

- **List rows** - every list in the front end: the main menu, settings, the character list, the
  dialogs. One selection indicator across the whole thing was the point; a UI with two would read as a
  bug.
- **The settings rows specifically** carry per-argument colours (the option name gold, its value
  silver) and `DrawStringWithColors` reads each argument's own flags. So the halo pass gets a rebuilt
  argument vector - rebuilt rather than copied because an argument's flags are private to it, and
  rebuilt per frame because that function takes the vector **by value** and so never writes an int
  argument's formatted form back to the item.
- **The name box.** The pentagrams that used to flank it are gone with everything else, so what is
  typed into it glows the same way a focused row does. The caret and the selection highlight are left
  out of the halo passes: they are solid rectangles, and smearing them across the ring offsets would
  blur the box rather than light up the name in it.
- **The character screen's action buttons**, through an exported `DrawFocusGlow(const
  UiArtTextButton &)`. It redraws the label itself, because it is called after the button has already
  been rendered and the halo has just been laid over the label's own outer pixels.

`DrawSelector` is deleted rather than left as a no-op stub; the only `DrawSelector` remaining in the
tree is `stores.cpp`'s, which is unrelated and in-game.

## Part sixteen: loading your only character asserted (1.2.7)

> i get this error when trying to load a character.
> *(Debug Assertion Failed - vector subscript out of range, `<vector>` line 1939)*

Reported against 1.2.6, caused in **part nine**, and it had been sitting there through every build
since.

`SelheroLoadSelect(int value)` opened with `vecSelHeroDlgItems[value]->m_value == 0` - read the choice
back out of the list. That works for exactly one of its three callers: the multiplayer Continue/New
Game list, whose callback really is handed a row in that list. The other two call it directly with a
hardcoded `1` while `vecSelHeroDlgItems` holds **a completely different list** - the character list on
single-player's fall-through, the class list after creating a hero.

Vanilla never noticed because the character list always ended in a "New Hero" row, so index 1 existed,
and character *i* carries `m_value == i`, so index 1 answered "not Continue" by arithmetic accident.
Moving New Hero out to a button emptied that seat. On an account with **exactly one character** the
list is one item long and the index is 1.

Which is why it took until now to see it: two or more characters and it still works, zero characters
and you cannot load one. The save folder had a single `single_0.sv`.

Fixed by giving the function a *choice* instead of a row - `SelheroChoiceContinue` /
`SelheroChoiceNewGame` - and putting the one translation from row to choice in the one callback that
owns a list where that translation is meaningful. The numbers are unchanged; what changed is that the
function no longer reads a global it does not own.

Worth keeping in mind for the rest of this screen: `vecSelHeroDlgItems` is reused for four different
lists (characters, classes, Continue/New Game, difficulty) and every function that subscripts it is
trusting a caller to have loaded the right one. The other three subscripts were checked and are all
framework callbacks against the list they belong to.

## Part seventeen: the figure doubles, and slows down (1.3.0)

> make the character preview in the selection screen twice biger. there is a lot of room as far as i
> see. make the preview animation 3x slower. they spin too fast.

The room was real, and part eleven had been unable to reach it. What was being scaled was the whole
**96x96 CL2 frame**, which is mostly transparent padding - sized for the animations that swing a weapon,
not for standing still. So the area was mostly being spent on nothing, and 4x was recorded as
impossible when what was impossible was 4x *of the padding*.

`PreviewInk` fixes that: the union of every frame's opaque pixels, measured once when the sprite loads
by drawing each frame through a translation whose every entry is opaque and looking at which pixels
were touched. Asking *where* rather than *what colour* means it does not depend on the palette, so it
is a load-time cost rather than a per-frame one.

The **union**, not the current frame's own box, is the load-bearing word. A per-frame box would shift
as the idle loop breathed, and since the figure is positioned from this box the character would drift
around its area instead of standing in it.

Scale is then `min(6, areaWidth / inkWidth, areaHeight / inkHeight)` - the target the user asked for,
clamped so no resolution or later layout change can push the figure off its area. Whole numbers only:
a fractional scale through a nearest-neighbour resample gives uneven pixel runs, which at this
magnification is the difference between pixel art and a mess.

The preview also got 16px of the title gap back. `HeroContentTop`'s 24px is the **list's** breathing
room - rows of text right under a heading in the same style need the separation - and the figure does
not, because it reads as a picture rather than as another line. `HeroPreviewTop` is that gap at 8, and
every pixel of it is a pixel of scale, since the figure is height-bound: at 720 the area is 430x385, so
6x is available to an ink box up to 71 wide and **64 tall**, and anything taller quietly takes 5x
instead.

**And 3x slower.** `GetAnimationFrame`'s second parameter is a millisecond divisor, so the default 60
means a frame every 60ms; `PreviewFrameMs` is 180. At full speed the town stand read as a fidget rather
than as a character breathing.

## Part eighteen: the glow goes silver (1.3.1)

> silver is the most bright? use silver at its brightest and lets see what hapens.

Yes, and it is worth writing down because it reads backwards. Every `.trn` in the font set was measured
against `ui_art\diablo.pal` to answer "what other colours are there", and the answer was that the
brightest thing in the menu palette is not the gold:

| ramp | .trn | indices | top of ramp | luminance |
|---|---|---|---|---|
| silver | `grayui` | 224-239 | (243,243,243) | **243** |
| gold | `goldui` | 176-191 | (255,227,164) | 228 |
| silver dark | `grayuis` | 226-238 | (204,204,204) | 204 |
| gold dark | `golduis` | 178-190 | (221,196,126) | 195 |
| amber | `whitegold` | 193-207 | (244,201,150) | 208 |

Gold's top has *more red* than white does - 255 against 243 - but much less green and blue, and
luminance is mostly green. So the warm colour that looks like the brightest thing on the screen is the
dimmer of the two. The silver ramp is also clean and neutral all the way down to black at 239, which is
exactly what a falloff wants.

So `GlowInnerColor` / `GlowOuterColor` are silver and silver-dark. A white halo around gold text also
*separates* rather than thickening: the aura is a different hue from the letters instead of more of
them, which the gold-on-gold version could never be.

The two constants exist so this is one line to change again.

### What is not available, measured rather than assumed

- **red** maps to 227-238 here, which is the grey ramp - (184,184,184).
- **blue** maps to 176-190: byte for byte the gold.
- **white** maps to 242-254, the VGA/system block - 249 pure red, 250 pure green, 252 pure blue.
- **orange** (152-159) and **yellow** (144-151) are eight-shade ramps in maroon and rose. Too short to
  fade.
- **buttonface** and **buttonpushed** run from the 200s into that same VGA block.
- **amber** (`whitegold`, peach to burnt orange) is usable and genuinely different, but is the only one
  that needs code: `GetColorFromFlags` never tests `UiFlags::ColorWhitegold`. That bit is dead - the
  ramp is what the function returns when it recognises no colour at all.

A new hue - green, purple, cyan - would need a palette edit. There is no such block in the UI palette,
which is the same conclusion the note in `ui_flags.hpp` reached when the flags enum was widened.

## Part nineteen: amber, and 150 (1.3.2)

> replace silver with brightest amber. the other two worked great! Maybe speed up the animation just a
> bit. maybe make it 150.

Silver was bright and cold; amber is `fonts\whitegold.trn`, indices **193-207** - fifteen shades from
(244,201,150) peach through (199,75,31) burnt orange to black. Firelight, which is the one thing in
this palette that looks like something is actually burning.

Two things fell out of it worth recording.

**`ColorWhitegold` already worked, by accident.** `GetColorFromFlags` has never tested that bit - it is
what the function returns when it recognises *no* colour at all, and roughly sixty call sites across
the stores, the character sheet, the inventory and the waypoint menu have been getting the ramp that
way for years. So the glow needed no engine change. An explicit check was added anyway, placed **last**
so every existing caller stays on exactly the path it was already taking, and the fallback stays where
it is.

**There is no dark amber.** Every other bright ramp here ships a compressed companion - goldui/golduis,
grayui/grayuis - and this one does not. The outer rings use `golduis` instead: warm, and topping out at
195 against amber's 208, so the ordering a falloff needs still holds, with less headroom between rings
than the silver pair had. If the aura wants more depth, the honest fix is to cut a `whitegolds.trn` the
way golduis relates to goldui - a new UiFlags bit, a `text_color` entry, and an MPQ repack.

`PreviewFrameMs` 180 -> **150**. The default 60 was a fidget, 3x was a touch slow, and this is where it
settled.

## Part twenty: no Bard, and three screens become one layout (1.3.3)

> I dont want the bard in my build. Hide it from everywhere including the Devilution settings.

The row is **gone** from the class list rather than switched off - `gbBard` and the Test Bard option
are both ignored there now, so the class is not offered however the game was installed or whatever the
.ini says. The option itself gains `OptionEntryFlags::Invisible`, which is what the settings menu's
`IsValidEntry` already checks, so it stops appearing there while an existing .ini still parses.

`gbBard` is deliberately left alone: it means "Hellfire's bard artwork is present", not "offer the
Bard", and two sprite-fallback paths read it. Any Bard character that somehow already exists still
loads - hiding a save is data loss, and that was not what was asked.

> The class-select and name-entry screens still use the older button font and their own x positions -
> apply whatever improvements you deem fit.

They were hanging off the 640x480 dialog art: `uiPosition.x + 264`, `uiPosition.y + 246`, a 176px box
to squeeze the class rows into, and buttons at FontSize30 in hand-placed 140 and 144px rects. Those
numbers composed against a painting that is no longer there. The captions were not even centred - the
old `x + 242`, 365-wide rect sits **104px right** of the screen's middle at 960.

Three changes, and all three are the same change: stop laying these screens out separately.

- **The buttons keep their cells.** OK is `HeroButtonRect(OkButtonIndex, 4)` and Cancel is
  `HeroButtonRect(CancelButtonIndex, 4)` on *every* screen that has them - the character list, the
  class list, the name box, the multiplayer Continue prompt. So neither moves as you step between
  them. That is worth more than packing a two-button row closer together: a control that stays put is
  one you stop having to look for. Same font as the rest of the row, from a shared `HeroButtonFlags`.
- **One centred column.** `HeroFormX`/`HeroFormCaptionRect`/`HeroFormBodyTopFor` put the caption under
  the title and centre the body in the same band the character list uses. Three screens that were
  three layouts are one layout with different contents in it.
- **The character list's own font and row height** for the class list and the Continue prompt. The
  "shrink the rows if there are more than four classes" rule went with the 176px box it was squeezing
  them into; the band is the whole screen now. The name box went up a size with them, in a rect tall
  enough for it - the old 33px one was cut for FontSize24.

## Part twenty-one: Cancel takes the last cell (1.3.4)

> switch places of Cancel and New Hero buttons and also move Cancel in NEW HERO Screen at its new
> location and move the list ofchoices of new heros also above the newly positioned Cancel button.

The row is now **OK, Delete, New Hero, Cancel**, and the last cell is the one the list column stands
over. So the swap is not really about the two buttons - it is about what stands under the list, and
the answer is now Cancel on **both** list screens.

The class list moved into that column with it. The character list and the class list are the same
control doing the same job one step apart, and they now stand in the same place with the same button
underneath. "Choose Class" went with the list, because a heading belongs over what it heads.

Almost none of this needed moving. `HeroListX` was written in part thirteen as a *relationship* -
centred over a named button's own rect rather than a second calculation that lands in the same place -
so repointing it at `CancelButtonIndex` was the whole change, and the character list, its scrollbar
and the preview area's right edge all followed without being touched. `AddHeroFormButtons` had already
collapsed the three screens' OK/Cancel pairs into one place in part twenty, so Cancel moved on all of
them at once.

The name box stayed in the centred column: it is not a list, and nothing was asked about it.

## Part twenty-two: the name box joins the column, and the scrollbar becomes the theme's (1.3.5)

> When creating new char - The Enter Name column (along with the text input field) to mo to the right
> "Cancel Button" column.

Done, and it completes what part twenty-one started: every step of making a character now happens in
the same place on screen. Pick a class there, name it there, with the same Cancel underneath
throughout.

One thing had to give to make it fit. The name box went from a 400px centred column to the 320px list
one, and `UiEdit` insets its text by **43px a side** - which is not padding, it is clearance for the
two pentagram cursors that used to flank the box and have been gone since part thirteen. At 43 a
fifteen-character name had 234px to fit in; at **12** it has 296. Every edit box in the game gains the
same 62px, and none of them wanted the gap.

> Heroes list is 7 visible position. From 8 onwards a scrol bar apperas. It is ugly and out of plase.
> I want you to put the same elegant scrol bar which you use for Char Stats ui window in game and
> relocate it to right edge of Cancel Button column.

The vanilla bar - a 25px tiled channel between two arrow buttons - was the last piece of old front-end
furniture on these screens, and against a painted background it read as a control bolted on rather
than part of the window.

**The colours are not an approximation of the in-game bar. They are the same colours.** The character
sheet draws its groove and thumb in indices sampled from `textbox_frame00`, and that warm ramp exists
in the front-end palette too - just at different indices. Matching by RGB rather than by eye:

| | in-game | front end | RGB |
|---|---|---|---|
| bevel shadow (left edge) | 204 | **188** | (57,49,29) |
| bevel body | 202 | **186** | (91,81,52) |
| bevel highlight (right edge) | 198 | **182** | (152,139,93) |

So the two bars are the same bar rather than a pair that happen to look alike.

The groove is the one thing that had to change rather than move. In game it is `DrawThemedFill` - a
half-transparent darkening of the panel behind it - and blending is unavailable in a menu for the same
reason the glow could not use it: `paletteTransparencyLookup` still holds whatever the last dungeon
palette generated. It is drawn solid in the ornate frame's own near-black instead, which is the colour
that theme already uses to separate the frame from whatever it surrounds.

**The widget stayed.** Only `Render(const UiScrollbar &)` changed, so every hit test in
`HandleMouseEventScrollBar` still works and the bar can still be dragged - the rect is sized for the
hand, the drawn bar for the eye, and the thumb is drawn at `ThumbRect`'s own position so the two can
never disagree about where it is. The arrows are no longer drawn but their zones still scroll when
clicked, which is what a player would expect of the top and bottom of a bar anyway.

That also means **every** menu scrollbar changed at once - the settings list and the multiplayer game
list included. One scrollbar across the front end was the point; a UI with two would read as a bug,
the same argument the focus glow was built on.

And it could finally move to the list's **right**. It was on the left because the vanilla art is 25px
wide against the 20px between the list and the screen edge at 960. Three pixels of bar in a 12px grab
column fits the space that was there all along.

## Part twenty-three: the delete prompt, and a shared header for the chrome (1.3.6)

> bring the Delete Character screen up to date with the other screens - bigger fonts, location of the
> Diablo logo the yes no buttons to be on the second and third button columns

> Exactly - shared header and footers as much as possible across the different screens!

Which is the interesting half. Three screens could live with the geometry sitting in the file that
owned most of them; this was the **fourth**, and it is in a different file. Copying the numbers across
would have made "the buttons are in the same place" a thing that is true today rather than a thing
that stays true - and this screen is the proof of what that costs, since it is the one that drifted
while the other three were being brought into line.

So `DiabloUI/hero/hero_layout.h` now holds the chrome: the logo's height, the title's band and font,
the action row and its four cells, the band of screen between them, and the centred form column.
Header-only - a dozen constants and six one-line functions, where a .cpp would be more build wiring
than code. `selhero.cpp` lost about 90 lines to it and reads better for it.

The delete prompt then needed almost nothing of its own:

- **Logo** at `HeroLogoTop()`, **title** in `HeroTitleRect()` at FontSize42 (was 30 in a 35px rect).
- **Message** at FontSize30 in the centred form column, and wrapped at `GameFont30` - the wrap must
  match the font it is drawn in, or a FontSize30 message wrapped for GameFont24 overruns its column.
- **Yes and No** in the second and third button cells (the user's numbering), at the shared button
  font and flags.

Yes/No stopped being a two-row `UiList`, because a list is vertical and the action row is not. Two
`UiArtTextButton`s take its place with the same focus treatment the character screen's row already
uses: left/right between them, the exported `DrawFocusGlow` marking the focused one, and
`UiFocusNavigationSelect` routed to activate it rather than to index a list that no longer exists.

That last part also retires the third instance of the pattern that caused part sixteen's crash -
`selyesno_value = vecSelYesNoDialogItems[value]->m_value == 0`, a function reading its answer back out
of a global list. It is now two functions that each simply say which answer they are.

Default focus stays on **Yes**, where the list's first row was. Defaulting a destructive prompt to No
is defensible, but it is a behaviour change and this pass is about how the screen looks.

## Part twenty-four: the OK dialog, and the last of the 640x480 numbers (1.3.7)

> update selok screen to the shared layout too

The fifth and last single-player screen still composing against the old dialog art. It carried the
clearest evidence of what that cost: the body text was **wrapped at 400px and drawn into a 560px
rect**, so every line broke 160 pixels short of its own box. Nothing looked broken enough to notice,
which is how it survived.

Everything now comes from `hero_layout.h` - logo height, title band and font, the centred column, the
action row - and the wrap width is the column's own, at the font it is actually drawn in.

The lone OK is the one thing that needed a decision rather than a lookup. A single answer belongs in
the middle of the row, the way the delete prompt's two sit either side of the middle - so it uses
`HeroButtonRect(1, 3)`. Three cells rather than the row's usual four is what puts one dead centre, and
it keeps the click target 266px wide. A one-cell row would also have centred it, and would have made
the entire 800px strip along the bottom of the screen quietly clickable.

There is no focus ring to navigate here, so the glow is simply drawn on OK every frame: it is the only
control on the screen, so it is always the focused one.

Its `UiList` went the same way as the delete prompt's, and for the same reason - a one-row list to hold
one button was ceremony. That is the last of the three "read the answer back out of a global list"
sites this session found.

**Multiplayer is deliberately not part of this.** Per the user, V1 is single-player and multiplayer
goes to the community after public release, so `selgame.cpp` and `selconn.cpp` keep the old layout.
They still get whatever arrives through shared code - the themed scrollbar, the focus glow, the edit
box's reclaimed padding - and stay visually adrift where it does not. Worth saying in the handover
notes rather than leaving as a surprise.

## Part twenty-five: the delete prompt shows who it is asking about (1.3.8)

> during deletion process - put a sprite of the sold hero here. *(with the spot marked on a screenshot)*

> give it the same background as the delete screen *(the OK dialog)*

Two finishes to the pair of dialogs.

**The OK dialog's background.** Part twenty-four gave it the shared layout but left it dropping to a
black plate, because every one of its eight callers passes `background = false` and that branch meant
"black". That branch now loads the same painting the delete prompt uses. `UiLoadBlackBackground()`
still runs first - it is what loads the palette the art quantizes against, and the fallback if the
asset is missing. The `background = true` branch (the main-menu art) is untouched and still unused.

**The figure on the delete prompt.** Loaded at the **call site**, not in the dialog, and that is the
load-bearing detail: `SelheroFree()` runs immediately before `UiSelHeroYesNoDialog` and calls
`FreeHeroPreview()`, so by the time the dialog exists there is nothing loaded. `selhero.cpp` sets it
again from `selhero_heroInfo` just before invoking the dialog - which also keeps the character data in
the file that owns it. The dialog draws whatever is loaded, the same handshake the character screen
already uses between `SelheroSetStats` and its render loop, and `DrawHeroPreview` is a no-op when
nothing is, so the dialog cannot break if some future caller forgets.

The message block is **measured, not fixed**: the newlines `WordWrapString` inserted are counted and
the text sized to its real height, so the figure sits under the message whether a name wraps it to two
lines or four. A fixed height would leave a gap under a short name and overlap a long one.

The figure fits itself to the band it is given (see `PreviewScaleFor`), so it lands around 4x here
against the character screen's 6x - the area is shorter, and nothing had to be told that.

### It did not appear, and the reason was drawn over it (1.3.9)

> i dont see the sprite during deletion. check your code.

Nothing wrong with the sprite. `UiInitList` copies **every item it is given** into `gUiItems`, and
`UiPollAndRender` re-renders that whole list at the END of the frame - after anything the screen drew
itself. Both dialogs were handing it the vector that also holds the **background**, so each frame went:
draw the backdrop, draw the character, draw the glow, then repaint the backdrop over both.

The character screen never hit this because it has always passed a *different* vector to `UiInitList`
(`vecSelDlgItems`) than the one it renders by hand (`vecSelHeroDialog`). The dialogs had one vector for
everything, which was fine for as long as they drew nothing of their own.

So both are split now: a backdrop the file renders itself, and an items vector holding only what the
shared code must own - the buttons, which have to be in `gUiItems` for the mouse to find them.

Worth recording that **the focus glow was invisible on both dialogs for the same reason**, and had been
since it was added. Nobody reported it; it only surfaced because the missing sprite forced a look at
the draw order. A screenshot of the delete prompt taken before this shows Yes and No with no halo at
all - the evidence was sitting there in plain view.

The general shape of it: in this UI, *anything a screen draws between `UiRenderItems` and
`UiPollAndRender` survives only if the background is not in the list `UiInitList` was given.*

## Part twenty-six: the class order (1.4.0)

> Change order of classes in Select New Hero screen: 1. Barb 2. Paladin 3. Sorcerer 4. Rogue

Rows only. Each item carries its own `HeroClass` as `m_value`, and both `SelheroClassSelectorFocus` and
`SelheroClassSelectorSelect` read that rather than the position, so nothing else on the screen depends
on the sequence - which is the one thing worth checking before reordering a list anywhere in this
codebase, given what indexing a list by position cost in parts sixteen and twenty-three.

Two placements the request did not cover:

- **Barbarian keeps its switch.** It goes first, but still behind `gbBarbarian || testBarbarian`, so
  turning it off leaves the other three in the user's order rather than reshuffling them.
- **Monk goes last.** It is not in the requested ordering and only exists on a Hellfire install, so it
  follows the four rather than being interleaved with them.

## Part twenty-seven: tab 10, and SORT leaves the row (1.4.1)

> Remove the S button and place tab 10 there and make the button work as the other 9 tab buttons and
> put a gold SORT INVENTORY text button where i marked on the picture

This turned out to **recover a page of storage that was already being saved**.

The inventory has always been ten pages deep: the vanilla backpack plus
`Player::NumExtraInventoryTabs` (9). The tab row also has ten positions - but the last one was SORT, so
only nine pages could ever be opened. The tenth was written to the save file, drained by SORT, and
otherwise invisible. `SortInventoryBySellValue` even had a constant apologising for it:

> `constexpr int PlaceableExtraTabs = Player::NumExtraInventoryTabs - 1;`
> *"the final extra tab has no way to be viewed... the placement loops stop one short, so it is never
> refilled"*

Moving SORT out of the row fixes all of it at once. The last position becomes tab 10, the hit-test
loop runs to `TabCount` instead of stopping short, and `PlaceableExtraTabs` is simply
`NumExtraInventoryTabs` again - a sort now uses the page it used to quarantine. **No save change**: the
storage, the save sub-file and the accessors were all already ten deep.

**SORT** is a text button in the footer under the grid, gold and white-for-a-moment on click - the same
word and the same treatment the stash's own Sort button uses, so the two read as one control in two
windows.

It shares the footer with the gold readout, and both are drawn from scrollrt **after the orbs**. The
band under the grid starts exactly at `OrbClearanceBottom`, so all of it is vertically behind the mana
orb at 960x720; the rows are centred at screen x 790 and the orb spans 658..755, so they clear it
horizontally, and drawing late is what keeps the overlap a non-issue. SORT sits above gold because a
button behind an orb is worse than a number behind one. `DrawInventoryGoldRow` is now
`DrawInventoryFooter`, since it draws both.

### The tests had pinned the old contract

`TabClickDoesNotClaimTheSortButtonPosition` asserted that the last tab position must **not** be
claimed as a tab - the fix for a real bug ("SORT doesn't sort... clicks on it land on ghost tab 10").
That contract is now inverted, so the test was rewritten rather than deleted, and split in two:

- `EveryTabPositionOpensAStoragePage` walks all ten positions, checks each opens its own page, and
  asserts `TabCount == NumExtraInventoryTabs + 1` - so a tab position without storage behind it fails
  loudly rather than running off the array.
- `TabClickDoesNotClaimTheSortButton` keeps what the old test was really guarding: whatever the tab
  hit-test does, it must not eat a click meant for SORT.

## Part twenty-eight: the difficulty screen's painting (1.4.2)

> take file Difficulty Selection Background.png and put it as a background on the Difficulty
> Selection screen.

`ui\difficulty_bg.png`, a fifth `UiBackground` slot, and one line at the screen's `UiAddBackground`.
The master is 1915x821 - 21:9, the same shape as the others - so the existing cover-crop fills every
listed resolution from the one file.

Two things about *where* this screen lives, both worth knowing before it is redesigned:

- **It is drawn by `selgame.cpp`**, the multiplayer file, but it is single-player's screen too. That is
  the "dangerous hack" `SelheroLoadSelect` documents: single-player tears down the character screen and
  runs selgame's items through selhero's own render loop. So this is in scope despite the file it lives
  in - see the note on V1 being single-player only.
- **Only values 0 and 1** get the painting. `selgame_GameSelection_Select` serves three screens from
  one function, and value 2 is the multiplayer game list, which keeps the stock plate.

The palette is already loaded by the `LoadBackgroundArt("ui_art\\selgame")` that brought us here, which
is what `AddUiBackground` quantizes against - the ordering its header insists on.

Layout is untouched: the screen is still on the 640x480 numbers, with FontSize30 buttons at their own
positions. The user is redesigning it, and a background was what was asked for.

## Part twenty-nine: the difficulty screen becomes its painting (1.4.3)

The user's mockup: the art's four horizontal bands ARE the four rows, each with its blurb on the left
and its name on the right, a title across the top, and OK/CANCEL at the bottom. *"this is rought. you
place the assets on proper spots"* - so the placement is mine, and four questions settled the rest:

| | chosen |
|---|---|
| click target | the name, not the whole band |
| descriptions | all four on screen at once |
| locked tiers | greyed out, with the required level shown |
| logo | none on this screen |

**The bands are measured, not assumed.** They compress toward the bottom - 204, 196, 181 and 164 rows
of 821 - so the row positions come from per-mille centres taken off the artwork. A `UiList` can only
space its rows evenly, so the pitch runs between the first and last measured centres and the two middle
rows land within ~7px of theirs. Against a 180px band that is invisible, and it buys the keyboard, the
mouse and the amber focus glow for nothing, which is what the "click the name" answer was for.

**The blurbs lost their first line.** Every translated description starts `"<Name> Difficulty\n"`,
which would print the name twice now that each row has its own label. `DifficultyDescription` returns
the pointer just past that newline - still a valid C string, since the body runs to the same
terminator - so the existing translations are reused rather than duplicated.

**One place for the thresholds.** The screen needs to know what is locked *without* the popup that
`IsDifficultyAllowed` and `IsSinglePlayerDifficultyAllowed` raise as a side effect, so
`DifficultyLevelRequirement` was extracted and both of those now read it. A locked row gets
`ElementDisabled`, which dims the name and makes `UiFocus` step over it, plus a "requires level N" line
under it. The popup still fires if you click one - you can just see it coming.

**Two ordering details that would have been bugs:**

- The **buttons are pushed before the list.** A row is a whole band tall, so the last one's rect reaches
  down past the button line, and `UiItemMouseEvents` gives the click to the first item whose rect
  contains it. Buttons first means CANCEL stays clickable.
- The **button row sits 16px above the bottom** rather than the shared 50. With no logo the bottom is
  free, and Torment's band is both the lowest and the shortest, so its name needs the room.

`selgame_Diff_Focus` is gone with the single description panel it used to rewrite.

**Untouched on purpose:** the multiplayer create-game path shares this screen and gets the redesign for
free, minus Torment, which stays single-player only.

### The blurbs were unreadable (1.4.4)

> description of difficulties is very dark and unreadable. Make it brighter - same as the title of
> this screen.

They inherited `ColorUiSilverDark` from the panel they used to live in, where they sat on a plate
rather than on a painting. That ramp tops out at 204 against `ColorUiSilver`'s 243, and over art this
dark the difference is the whole of legibility. Now the title's own colour, as asked.

The locked rows' names and their "requires level N" lines stay on the dark ramp: there, being dim is
the point - it is what says the row cannot be picked.

### And smaller (1.4.5)

> can you reduce the font of the description?

FontSize24 -> **FontSize12**, which is the size the old screen drew this same text at. Three things had
to move together: the size flag, the explicit line height (26 -> 16) and **the font the wrap is
measured at** (`GameFont24` -> `GameFont12`). Leaving that last one behind is what made the OK dialog
break its lines 160px short of its own box for as long as it did - the same mistake has now been
available to make twice and caught both times by looking for it.

## Part thirty: a way back in - PNG sprites for a class (1.4.6)

The user is generating a full Barbarian sprite set and asked for the import path to exist before the
art lands. It did not: the engine loads UI art from PNG but characters only as CL2 out of the archive,
which is the gap between "we have art for a new class" and "the class looks like itself".

That gap is why `PlayersData` lists the Barbarian's class path as **"warrior"** and the Bard's as
**"rogue"** - not a runtime fallback, an alias in the data. Of six classes this install has art for
three: warrior, rogue and sorceror. There is no Hellfire archive here, so the Monk exports zero sheets.

`oracool/sprite_import.h/.cpp` closes it. A sheet is laid out exactly as the exporter writes one - **8
rows, one per facing in Direction order, by N frame columns** - so a set can be exported, edited or
regenerated and dropped straight back.

Three decisions worth keeping:

- **Quantized into the palette's shared half (128-255) only.** That range is identical in town and all
  four dungeon tilesets, which is what lets one import look right everywhere rather than only under
  whichever palette happened to be loaded when it was read. It also frees index 0 to mean transparent
  with no ambiguity - a real sprite pixel can never land on it.
- **Looked up under the class's OWN name**, not the class it borrows CL2s from. `plrgfx\barbarian\...`
  rather than `plrgfx\warrior\...`, because supplying art is precisely how a class stops borrowing.
  `ClassSpriteFolder` exists for that one distinction.
- **Per animation, not per class.** A missing or malformed PNG falls through to the CL2 exactly as
  before, so a class can be converted one animation at a time and a half-finished set still runs.

### Verified, not merely compiled

Twice this session a renderer built cleanly and did nothing on screen. So the exporter grew a
`--verify` pass: it writes each PNG, reads it back off disk, puts it through the **game's own
importer**, and checks the frame counts per facing survive the round trip. `SpriteSheetFromSurface` was
split out of `LoadPngSpriteSheet` for exactly this - the file path is the only part the two do not
share.

    warrior: 255 sheets, 28264 frames.
    round-trip through the importer: 255 sheets verified, 0 failed.

That is the whole Warrior - every gear combination, every animation - out through PNG and back into
sprites the engine will draw.

## Part thirty-one: the roster of six (1.4.7-1.4.8)

> so we can add the monk to our V1 as long as the user has monk.mpq file to put in our game folder?
> write it now so it's ready

### Only one of Hellfire's three classes was actually drawn

Hellfire ships three classes and one new sprite set. The `classPath` column of `PlayersData` says so
outright: Bard is `"rogue"` and Barbarian is `"warrior"` - **aliases in the data**, not runtime
fallbacks. Only the Monk has `"monk"`, and its frame widths match no other class in the game: 112 for
idle and walk, 130 for attack, 98 for hit and block, 114 for casting, 160 for death, against everyone
else's 96/128/96/96/128.

So `hfmonk.mpq` is the one archive that carries a class rather than a stat block, and the Monk is the
one added class worth gating on its own file:

```cpp
inline bool HaveMonk()
{
	return bool(hfmonk_mpq);
}
```

Deliberately **not** `HaveHellfire()`. `gbIsHellfire` is set by `hellfire.mpq` and changes the whole
game - quests, levels, monsters, item tables. This asks only "is there a Monk to draw". The class list
went from `if (gbIsHellfire)` to `if (HaveMonk())`, and that is the entire change, because
`hfmonk.mpq` is already loaded at startup outside any Hellfire check - the sprites are reachable the
moment the file is in the folder.

What was checked before writing it, since the offer came with a warning that its starting kit might
reach into Hellfire's item data:

- **The kit is clean.** `IDI_SHORTSTAFF` plus two heal potions, with no `gbIsHellfire` branch. The
  Sorcerer's kit next to it has three.
- **Its combat rules are class logic.** The staff damage bonus, the halved bonus for holding something
  else, the level-scaled fist damage. No content lookups.
- **Its voice cannot fail at init.** Every monk sound is flagged `sfx_STREAM | sfx_MONK`, and
  `effects.cpp` skips streamed entries *before* reaching `sound_file_load`, so they are never
  preloaded. Whether `hfvoice.mpq` is present decides only whether the Monk is heard, not whether the
  game starts.

Under `UNPACKED_MPQS` there is no separate monk path at all, so there `HaveMonk()` necessarily means
the same thing as `HaveHellfire()`.

### The half that did not work, and could not have (1.4.9)

`hfmonk.mpq` arrived, and the change was wrong. Not the gate - `HaveMonk()` reads the archive
correctly - but everything downstream of it. `FindMpqFile` in `engine/assets.cpp` searched every
`hf*.mpq` **only inside `gbIsHellfire`**:

```cpp
|| (gbIsHellfire && (at(hfvoice_mpq) || ... || at(hfmonk_mpq) || at(hellfire_mpq)))
```

And `gbIsHellfire` is set by `hellfire.mpq`, which is exactly the file the whole point was to do
without. So the class list would have offered a Monk whose every sprite lookup missed - the
missing-sprite abort at character creation that part thirty-one had listed as the thing to watch for.
Loading an archive and searching it are two different questions, and 1.4.7 only answered the first.

The fix is to search `hfmonk.mpq` whether or not this is a Hellfire game, and what makes that safe was
measured rather than assumed. Probed by path with `tools/oracool_mpq_extract.exe`, the archive answers
for `plrgfx\monk\` and `sfx\monk\` and refuses everything else offered to it - `ui_art\title.pcx`,
`data\char.cel`, `levels\towndata\town.cel`, `music\dtowne.wav`, `sfx\misc\walk1.wav`,
`monsters\zombie\zombiea.cl2`, `items\armor2.cel`, and the warrior and rogue sprite sheets the
Barbarian and Bard borrow. It carries a class, not an expansion, so there is nothing in it that could
shadow `diabdat.mpq`. It is appended **after** the Hellfire group rather than lifted out of it, so a
real Hellfire game keeps the archive precedence it had.

That probe also closed the voice question 1.4.7 left open: `sfx\monk\monk01.wav` is in `hfmonk.mpq`
itself, so the Monk speaks without `hfvoice.mpq`.

### Verified by enumeration

    monk: 291 sheets (.cl2 files), 30744 frames total. 6 combinations had no file.
    round-trip through the importer: 291 sheets verified, 0 failed.

Every armour x weapon x animation combination the naming scheme allows, loaded through the game's own
`LoadCl2Sheet` and pushed back through the game's own PNG importer. It also settles how much of a
class Hellfire actually drew: **291 sheets against the Warrior's 255**, 30,744 frames against 28,264,
and only 6 gaps where the Warrior has 42. The Monk is not a reskin with a stat block. It is the most
completely drawn class in either game.

### The Bard needs less than the Monk, not more (1.4.8)

> Ok, I will and in that case bring back the Bard and it's option in Options list. Let's have the whole
> roster of 6 available.

This reverses part twenty, and it is cheap to reverse because part twenty only ever hid the class -
`gbBard` was left alone there precisely because it means "the artwork is present", not "offer the
Bard".

Where the Monk needs an archive, the Bard needs nothing:

- `classPath` is `"rogue"`, so it wears the Rogue's sprites, out of `diabdat.mpq`.
- `sound_init()` folds `HeroClass::Bard` into `sfx_ROGUE`, so it speaks with the Rogue's voice, also
  out of `diabdat.mpq`. `hfbard.mpq` only ever replaced that voice.
- Its stats, its Identify class skill (a plain vanilla spell), the dual-wield rule in `inv.cpp` and its
  two starting weapons are all compiled in.

So the row comes back on `gbBard || testBard`, and the option goes back to visible with its default
flipped to true - the same shape as the Barbarian's switch two lines below it in `options.cpp`.

### The part that was more than three edits

`IsItemAvailable()` had the Bard's Sword and Dagger behind the Test Bard switch. That function is not
only a generation filter: `UnPackItem` (`pack.cpp:330`) **clears any item it rejects**. With the switch
in the condition, turning the Bard off would silently delete an existing Bard's starting weapons the
next time that character loaded - and the switch is now one click away in the settings menu, which is
what makes the trap worth removing rather than documenting.

Both items are therefore available unconditionally now, and nothing is given up for it. They are
`IDROP_NEVER`, and every generation path skips `IDROP_NEVER`: `GetItemIndexForDroppableItem`, which
loot, all four vendors and the uniques funnel through, and `FirstBaseItemForEquipLocation`'s plain
path. They still cannot spawn anywhere. The only way to hold one is to be a Bard.

Their save indices were checked rather than assumed. `IDI_BARDSWORD` is 37 and `IDI_BARDDAGGER` 38,
below every boundary in `RemapItemIdxToDiablo` (83, 92, 161) and `RemapItemIdxFromDiablo` (83, 88,
156), so they already round-tripped through a non-Hellfire save unchanged. `IsItemAvailable` was the
only thing standing between them and their owner.

## Files

- `tools/CutAttackIcons.ps1` - green-keys the two cards to a 38px strip.
- `tools/CutBarbSkills.ps1`, `tools/CutPaladinAuras.ps1` - repointed from Downloads to the vault.
- `Packaging/resources/{assets,oracool_assets}/ui/attack_icons.png`, `main_menu_bg.png`,
  `hero_settings_bg.png`.
- `Oracool.MPQ/02-source-art/{attack-skills,auras,barb-skills,main-menu}/` - the sorted sources.
- `Source/oracool/attack_skills.h/.cpp` - the armed/unarmed rule, names, details, and both wells.
- `Source/oracool/ui_backgrounds.h/.cpp` - crop, scale, quantize, one cache slot per screen
  (replaces `main_menu_bg.h/.cpp`).
- `Source/DiabloUI/settingsmenu.cpp` - background, alphabetical categories, font and centring.
- `Source/DiabloUI/hero/selhero.cpp` - background, the character-list rearrangement, the preview.
- `Source/oracool/hero_preview.h/.cpp` - the sprite, the palette translation, the scaled draw, the ink
  box and the fitted scale.
- `Source/oracool/sprite_import.h/.cpp`, `Source/player.cpp` - PNG sheets as player animations.
- `tools/oracool_sprite_export.cpp` - the class sprite exporter and its round-trip verification.
- `Source/DiabloUI/diabloui.h`, `Source/pfile.cpp` - `_uiheroinfo::gfxnum`.
- `Source/DiabloUI/ui_item.h`, `diabloui.cpp` - `UiList`'s optional explicit line height, and the
  two empty-list bugs in `UiInitList`.
- `Source/panels/spell_book.cpp` - Spells available to every class again.
- `Source/options.cpp` - Test Bard hidden from the settings menu at 1.3.3, then back in it and on by
  default at 1.4.8.
- `Source/init.h` - `HaveMonk()`, and why it is not `HaveHellfire()`.
- `Source/engine/assets.cpp` - `hfmonk.mpq` searched outside `gbIsHellfire`, which is what makes
  `HaveMonk()` mean anything.
- `Source/DiabloUI/hero/selhero.cpp` - the Monk row on `HaveMonk()`, the Bard row restored.
- `Source/items.cpp` - the Bard's two starting weapons off the Test Bard switch, so turning the class
  off cannot strip them off a character.
- `Source/oracool/hud_art.h/.cpp` - third and fourth users of `DrawStripIcon`.
- `Source/oracool/hud_layout.h/.cpp` - `SkillWellIconSize`, `CentreInWell` and its three asserts.
- `Source/panels/spell_book.cpp` - the two rows, the shared text column, the click.
- `Source/panels/spell_list.cpp` - the RMB well's fallback.
- `Source/engine/render/scrollrt.cpp` - the LMB well's draw call.
- `Source/control.cpp` - tooltips for both wells; shift-clear now calls `ClearReadiedSpell`.
- `Source/spells.h/.cpp` - `ClearReadiedSpell` exported.
- `Source/engine/render/text_render.cpp` - `ColorWhitegold` selectable by flag rather than only as the
  fallback.
- `Source/DiabloUI/mainmenu.cpp` - the background swap.
- `Source/DiabloUI/diabloui.cpp` - `DrawFocusGlow` and both its callers' halo passes; `DrawSelector`
  removed.
- `Source/DiabloUI/hero/selhero.cpp` - the action row's focus ring, its navigation handler, the
  focused button's glow, and `SelheroLoadSelect` taking a choice rather than a row.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.89** through **1.4.9**. Tests **351/353** at each, and
**352/354** from 1.4.1 where one inventory test became two -
`Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, the same two
pre-existing failures as every build this session.

`oracool.mpq` repacked (67 files, 11,102,632 bytes at 1.1.93) and
the new assets extracted back out of the packed archive and SHA-256 matched against source - the
closed-loop check every asset change in this project gets, after the time a pack step reported
success and shipped a stale file.

The well centring is checked three ways: by the `static_assert`s (which is why the build compiling at
all is evidence), by the cutter's own bounding-box check on each cell, and by rendering the plate
with the shipped art composited at the exact origins the code computes - including the RMB well in
all three of its states, which is what makes the size match against the readied-spell icon visible.

The three duplicate files deleted from the vault root were SHA-256 matched against the copies kept
under `02-source-art/` before being removed.

The menu composition was checked offline by rendering the crop at 960x720, 1280x720 and 1920x1080
with the logo band and the three-item list boxed on top: the list sits over the dark ruined street,
so the gold text has the contrast it needs.

The glow's colour arithmetic was checked against the shipped files before it was written, not after it
looked wrong: `goldui.trn`, `golduis.trn`, `white.trn` and `whitegold.trn` were read byte by byte for
their 192-207 mappings, and each target index looked up in `ui_art\diablo.pal` for its RGB and
luminance. That is where the white-core idea died - see part fifteen - and it died before it cost a
build.

**Not seen in game.** To confirm:
- the two attack rows on the Skills sheet, and that removing your weapon swaps which one is lit;
- clicking Regular Attack while a spell is readied - the RMB well should switch to the attack icon;
- clicking Fist Attack should do nothing at all;
- both wells' tooltips;
- the menu background at more than one resolution, especially a 1080p or 1440p one, where the crop
  is genuinely upscaled rather than 1:1;
- **the glow itself** - whether the aura is strong enough to find at a glance on the main menu, the
  settings list and the character list, and whether the 700ms breath reads as alive or as a twitch;
- the action row: Down out of the list, Left/Right along the buttons, Delete skipped when the
  highlighted row is not a character, Up back into the list, Return on each of the four;
- the name box on the New Hero screen, which is where the glow has the least background behind it;
- **loading a character with only one on the account**, which is the assert from part sixteen, and
  again with two, since the two-character case is the one that always worked and must keep working;
- **the Bard**: that the row is back in the class list, and that a new Bard starts holding both the
  Sword and the Dagger rather than an empty hand - that pair is what the `IsItemAvailable` change is
  for;
- **the Monk**, whose sprites are now verified by enumeration but whose *play* is not: the row
  appearing as a fifth class, the figure drawn at character select, and the voice on the first grunt.

### Running as Hellfire, on purpose

`hellfire.mpq`, `hfmonk.mpq`, `hfmusic.mpq` and `hfvoice.mpq` all went into the build folder, and the
first of those is not a Monk switch - it sets `gbIsHellfire`, which is 25 dungeon levels instead of
17, Hellfire's quests, monsters and item tables, and save files named `.hsv` rather than `.sv`, so
characters made in Diablo mode stop being looked for until it is removed. Offered the choice, the user
kept it. So V1 runs as Hellfire, and `HaveMonk()` plus the `assets.cpp` widening remain what make the
`hfmonk.mpq`-only configuration work if that is ever wanted back.

What that costs the front-end work was measured rather than guessed, because `UiLoadDefaultPalette`
swaps `ui_art\diablo.pal` for `ui_art\hellfire.pal` and every colour this session chose is a palette
**index**. Compared byte for byte, the two palettes differ at indices **10-31 and 240-245 only**:

| ramp | indices | differs? |
|---|---|---|
| gold (`goldui.trn`) | 176-191 | no |
| amber (`whitegold.trn`) - the focus glow | 193-207 | no |
| silver (`grayui.trn`) | 224-239 | no |
| scrollbar bevel | 182, 186, 188, 191 | no |

Every ramp the redesigned screens draw with is byte-identical between the two. The focus glow, the
themed scrollbar and the gold and silver text are the same colours in Hellfire mode as in Diablo mode.

Two things do change and were **not** designed for:

- **The logo.** `diabloui.cpp:627` loads `ui_art\hf_logo2` at 16 frames instead of `ui_art\smlogo` at
  15. The shared hero chrome places it from `HeroLogoTop()`, so different art at a different size may
  shift the header on every screen built on `hero_layout.h`.
- **The waypoint list stops at 17.** `waypoint_menu.cpp` holds a `std::array<const char *, 17>` with a
  `static_assert` sizing the panel to exactly seventeen rows. Hellfire's Nest and Crypt waypoints have
  nowhere to appear. Nothing indexes out of range - the list is simply short.
  *Fixed at 1.5.0 - see [[2026-08-14 - Twenty-Five Waypoints, and the Bytes We Could Not Borrow]],
  which also found that the sigils for those levels were never placed in the first place.*
