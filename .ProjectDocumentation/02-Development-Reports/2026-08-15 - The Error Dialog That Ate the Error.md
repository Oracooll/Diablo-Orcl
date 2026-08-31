---
date: 2026-08-15
version: 1.5.44
area: Asset deployment, audio robustness, front-end list layout, object hit-testing
---

# The Error Dialog That Ate the Error

Three reports in one pass: hero names colliding with the pentagrams, a waypoint you have to click in
the corner, and a crash on the Wandering Trader quest. The third turned out not to be about the
Wandering Trader at all.

## 1. The crash that was not the crash

> Clicking on the Wandering Trader quests crashes the game and pops the attached error.

The dialog read `Failed to open file: fonts\gamedialogwhite.trn`. That file exists in
`Packaging/resources/assets/fonts/` - and a diff of the source asset tree against the deployed one
showed thirteen files present in the first and absent from the second. `CMake/Assets.cmake` lists
`devilutionx_assets` **explicitly**, and the three `gamedialog*.trn` files were simply never in it.

The interesting part is why that message appeared instead of the real one.

`ColorTranslations` (text_render.cpp) names those three files, and `LoadFileInMem` is **fatal** on a
missing file, not optional. So a missing `.trn` does not degrade, it kills the process. And
`GetColorFromFlags` has this:

```cpp
if (HasAnyOf(flags, UiFlags::ColorDialogWhite))
    return gbRunGame ? ColorInGameDialogWhite : ColorDialogWhite;
```

`ColorDialogWhite` maps to `nullptr` - no translation, the base font is already that colour - so the
front end never touches the file. In game, the same flag becomes `ColorInGameDialogWhite`, which maps
to `gamedialogwhite.trn`. Grepping for the flag found its only in-game consumer:

| Consumer | Where |
|---|---|
| `UiText`'s default flags | `ui_item.h:287` |
| ...which is what `UiOkDialog` builds its message from | `dialogs.cpp:81,93` |

**The only thing in the running game that needs `gamedialogwhite.trn` is the error dialog itself.**

So the sequence was: something failed on the Wandering Trader → `app_fatal` → `UiOkDialog` sets
`inDialog = true` → its render loop draws the message → missing `.trn` → a *second* `app_fatal` →
`UiOkDialog` again → and now:

```cpp
if (!gbActive || inDialog) {
    ... SDL_ShowSimpleMessageBox(...);   // native box, second message
    return;
}
```

That `inDialog` branch is why the screenshot shows a bare Windows message box rather than the game's
own styled dialog - and why it carries the *font* error rather than the original. Since `gbActive`
must have been true (the user was playing, and a false `gbActive` freezes rendering entirely), the
`inDialog` arm is the only way in. Every in-game fatal in this build has been reporting itself as a
missing font, whatever actually broke.

Ten files added to `devilutionx_assets`: the three `gamedialog*.trn`, plus seven stock font rows
(`12-1f1/1f3/1f5`, `24-1f1/1f3/1f5`, `30-e0`) that were missing from the same list. The font rows are
loaded through `LoadOptionalClx` and only degraded to `?`, so they were cosmetic; the three `.trn`
files were the fatal ones.

One reproduction then put the real message on screen:

> `Audio file not found` — `sfx\hellfire\trader1.wav`

### What was actually broken

The Wandering Trader's log entry is a voice line and nothing else - `Speeches[TEXT_TRADER]` is
`{ "", true, TSFX_TRADER1 }`, an empty text string paired with a streamed sound - and
`sfx\hellfire\trader1.wav` is not in this install's Hellfire archives. Reading the entry therefore
took the game down.

The fatal was in `LoadAudioFile` (sound.cpp), and it was **the odd one out in its own function**:

```cpp
bool LoadAudioFile(const char *path, bool stream, bool errorDialog, SoundSample &result)
```

Every other failure path there honours `errorDialog` and returns false. The mildest of the three - a
file that is merely absent - was the only one that called `ErrDlg` unconditionally and killed the
process.

And the engine was already built to cope. `sound_file_load` discards this function's return and hands
back a `TSnd` whose buffer never loaded, and `StreamPlay` guards on `DSB.IsLoaded()` before playing
one. The graceful path existed; it was simply unreachable past the dialog.

Three changes:

1. The not-found case honours `errorDialog`, logs a warning and returns false, like its neighbours.
2. `sound_file_load` passes `errorDialog=false` - one missing effect goes quiet, the game continues. A
   wholly absent archive is still caught, and caught better, by `CheckArchivesUpToDate` at startup.
3. `snd_play_snd` gained the `IsLoaded()` check `StreamPlay` already had. Without it the fix would
   have traded a crash for a null dereference: `PlayWithVolumeAndPan` walks `stream_` unconditionally,
   and `stream_` is null exactly when the load failed.

### And the blank parchment behind it

With the crash gone the entry would have opened an empty scrolling text box, because `scrlltxt` is
true while the text is `""`. `InitQTextMsg` now opens the overlay only when there is something to
read. Nineteen `Speeches` entries have no text and four of those still ask to scroll; this is visible
in this build rather than in vanilla because the quest log lists every quest
(`Oracool.questLogRevealAll`), so normally unreachable entries can be opened.

It also steps around a gettext trap: `_("")` does not return an empty string, it returns the
catalogue's metadata header - so on any translated build an empty entry would have scrolled the `.po`
header at the player.

**The missing audio itself is not recovered** - `trader1.wav` is absent from this install's `hfvoice.mpq`
(36 MB) and nothing here can conjure it. The trader's line is silent. That is now a silence rather
than a crash.

A false lead worth recording: `oracool_yellow.trn` is also in `ColorTranslations` and also absent from
the deployed tree, which looked like a second landmine. It is not - it ships in `oracool.mpq` instead.
I had "confirmed" it missing by searching the archive for the filename as ASCII, which is meaningless:
MPQ filenames are hashed, and the control test could not find `(listfile)` or any known-present file
either. The archive holds 72 files and `oracool_assets/` holds 72 on disk. Nothing is wrong there.

## 2. Names under the pentagrams

> we need to do something about player names fitting between the pentagrams

`DrawSelector` puts its two sprites at `rect.x` and `rect.x + rect.w - sprite.width()`, while the
label was drawn `AlignCenter` across the **whole** rect. The two ends of every row were being drawn
over twice, so a long enough name ran underneath the art. Not a hero-screen bug - a bug in the shared
list renderer that only the hero screen was long enough to show.

New `LabelRect()` insets the label by one pentagram at each end, and it is applied to every row rather
than only the focused one - inset only when selected and the text would jump sideways as the selection
moved. The padding is read from the sprite at runtime (`ui_art\focus*.pcx` lives in `diabdat.mpq`, so
its width is the archive's fact, not ours).

That leaves the hero list a 168px column, and the question became whether a name fits it. Measuring
the glyph widths straight out of the font CLX files and summing them the way `GetLineWidth` does:

| Name (15 chars) | FontSize30 | FontSize24 | FontSize12 |
|---|---|---|---|
| `Bartholomewwwww` | 282px | 245px | 158px |
| `000000000000000` | 314px | 269px | 164px |
| `MMMMMMMMMMMMMMM` | 329px | 284px | **179px** |

Against 168px, **no available face fits an arbitrary fifteen-character name** - not even FontSize12,
which would be absurd on a 52px row anyway. So the note left in `selhero.cpp` last time ("the fix if
it ever matters is a size down for the list font") was wrong, and measuring is what showed it.

The user's call settled it instead:

> reduce characters to 10 for a name

At ten, FontSize30 gives `Bartholome` 157px and ten digits 139px - both inside 168 with room. Ten
capital Ms are still 219px, so `FitToWidth` was added as a backstop: it shortens with a trailing
ellipsis rather than letting `AlignCenter` clip a string at *both* ends. Multi-line items are skipped
(they arrive pre-wrapped, so `GetLineWidth` would measure only their first line).

The four bottom buttons share the inset, so they were measured too before committing to it - at
FontSize42 in a 240px zone less two 28px pentagrams, `New Hero` is the widest at 182px against 184px
available. It fits by 2px. That margin is recorded at `LabelRect` so a future rewording does not
silently clip.

## 3. Aiming at the waypoint

> we need to move the clickable zone over the WP in the middle of it or covering it's whole size.
> Right now i need to aim at the 6 oclock corner.

`OBJ_WAYPOINT` carried `selFlag = 1`. Per `CheckCursMove` (cursor.cpp) the flag decides which tiles an
object answers from:

- `>= 2` - the tile above it, and the two lower diagonals ("tall objects like doors")
- `1` or `3` - its own tile

At 1 the sigil answered from one tile only. Its art is 144px wide - well over two 64px tiles - and is
drawn centred above its anchor, so that single hittable tile sits at the bottom point of the platform:
the 6 o'clock corner, exactly as reported. Set to `3`, the doors' value, which is own tile *and* above
*and* the lower diagonals - every tile the platform visibly covers.

## Changed

- `CMake/Assets.cmake` - ten stock assets added to `devilutionx_assets`.
- `Source/engine/sound.cpp` - missing audio is non-fatal; `snd_play_snd` guards on `IsLoaded()`.
- `Source/minitext.cpp` - no overlay for a speech with no text.
- `Source/DiabloUI/diabloui.cpp` - `SelectorPadding`, `LabelRect`, `FitToWidth`; both the list and the
  art-text-button renderers now draw into the inset rect.
- `Source/DiabloUI/hero/selhero.cpp` - name cap 15 -> 10; the stale width note corrected.
- `Source/objdat.cpp` - `OBJ_WAYPOINT` selFlag 1 -> 3.

## Verified

- All ten assets now deploy; `gamedialogwhite.trn` confirmed present in the build tree.
- The masking fix worked as intended: it is what turned an unhelpable "missing font" into the real
  `sfx\hellfire\trader1.wav`, which is how the rest of this was found.
- Glyph widths measured from the font CLX files, method validated against a screenshot to 2px.
- Build clean. **352/354** - the standing baseline
  (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`), no regression.
- 1.5.44 confirmed baked into `DiabloOrcl.exe`.
- **Only the crash chain has been seen in play.** The pentagram inset, the 10-character cap and the
  waypoint hit box are all still unconfirmed on screen.

## Carried forward

`LoadFileInMem` being fatal means every entry in `ColorTranslations` is a hard dependency, and
`devilutionx_assets` is a hand-maintained list with no check that the two agree. A missing colour
`.trn` cannot be noticed until something draws with that colour, and because the error dialog is
itself a consumer, the failure disguises itself. Worth a build-time assertion that every non-null
`ColorTranslations` entry is in the deployed asset list.
