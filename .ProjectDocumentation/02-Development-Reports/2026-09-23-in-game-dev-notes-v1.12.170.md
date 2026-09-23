# /dev in the game — v1.12.170

2026-09-23

> i need you to create a command /dev which will keep a development.md file in the folder with
> every message i input after the command. timestamped. datestamped. every once in a while i will
> feed you this file and you will process the notes in it.

Built first as a Claude Code command (`.claude/commands/dev.md`). Asked which `/dev` was meant, the
user answered **"Type /dev in the game"** — so the game's own chat line has it now.

## How it works

Press Enter, type `/dev <note>`, press Enter. The note is appended to `development.md`:

    ## 2026-09-23 14:05:12 | v1.12.170 | town (54,72)

    the belt icons still look off on the fifth HUD

and the event log confirms it: *Noted in development.md: ...*

**The stamp carries the build and where the hero stands** — town, a dungeon level, a set level or
a rift, and the tile. That is the one thing a note typed in the game can carry that a note typed
anywhere else cannot, and it is most of what processing a bug note needs.

## Where the file goes

**The repository root, in a Debug build** — the same `development.md` the notes are processed from.
The build writes the root into the generated `config.h` as `ORACOOL_SOURCE_DIR`, and
`oracool/dev_notes.cpp` reads it **only under `_DEBUG`**: an unused macro emits nothing, so a
release binary never carries the developer's local path. A release build files its notes in the
preferences folder, beside the saves.

## Where it lives in the code

`TextCmdList` in `control.cpp` — the chat line's always-available commands (`/help`, `/arena`,
`/seedinfo`), **not** `debug.cpp`'s list, which speaks through the red cheat-console channel. A note
is not a cheat.

## Limits worth knowing

- **About 66 characters a note.** The chat box is the user's own "black rectangle which fits 66
  zeroes", and `DrawTalkPan` truncates input to what fits in it; the buffer behind it is 80 bytes,
  less the five of `/dev `. Longer thoughts go in as several `/dev` lines, each its own entry.
- **Debug builds only, in single player.** `IsChatAvailable()` opens the chat line in single player
  only under `_DEBUG`. That is the build in use, and a note inbox is a development tool.

## The build that failed

The first compile failed on `return _("Usage: /dev <note>");` — `_()` returns a `string_view` in this
codebase, not a `std::string`. Wrapped; rebuilt clean.

## Files

- `Source/oracool/dev_notes.h` / `.cpp` (new)
- `Source/control.cpp` — `TextCmdDev` and its row in `TextCmdList`
- `Source/CMakeLists.txt` — `ORACOOL_SOURCE_DIR` in `config.h`, the new source

Built clean, 832/832. No asset changes.
