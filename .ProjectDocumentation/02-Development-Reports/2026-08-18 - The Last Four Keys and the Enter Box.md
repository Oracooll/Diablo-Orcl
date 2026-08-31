# The Last Four Keys and the Enter Box

**Version:** 1.7.84 → 1.7.85
**Date:** 2026-08-18
**Request:** items 10–13 and 16 of the 2026-08-18 review list.

This closes the list. Items 1–9 shipped across 1.7.78–1.7.83.

## F9–F12 (1.7.84)

Reserved on the same terms item 9 established: intercepted in `PressKey` **before**
`sgOptions.Keymapper.KeyPressed(vkey)`, which is what makes them unremappable — the keymapper never
sees the press, so no ini row can double-book them.

Deliberately **not** behind `CanPlayerTakeAction()`, unlike F1–F8. Changing the speed, opening the
log and taking a screenshot are observation or presentation rather than acts in the world; the
loading-screen handler already made exactly that argument for the screenshot key in its own comment.

**Item 13 was real work, not a no-op.** F12 was only caught during *loading screens*
(`DisableInputEventHandler`); the in-game binding was `SDLK_PRINTSCREEN` alone. The one key most
people reach for did nothing for the whole game.

`QuickMessage`'s F9..F12 defaults are now unbound, as `QuickSpell`'s F5..F8 were before them — they
would have been rows that looked bound and were not.

### Game speed

New `oracool/game_speed`. Its own module because two callers need it: the key handler that moves the
speed and the clock corner that reports it. It mirrors demo mode's three lines, and all three are
load-bearing — `nTickRate` is what the engine reads, the option is what survives the session, and
`gnTickDelay` is what the main loop actually waits on. Setting only the first changes nothing until
the next level load.

Clamped **20..60, step 2** (user's call), single-player only: `nTickRate` is agreed at game creation
and shared by every peer, so moving it mid-session in multiplayer would desync.

### The readout

Under the clock, with an ini option **Off / On / Blink**, defaulting to Blink. Blink means what the
user specified: hidden, then shown with the new value blinking for one second when a change is
initiated, then hidden again — so it reports the change rather than nagging.

Its band is **reserved whether or not it draws**. A line that appears and disappears would otherwise
shove anything below it around twice a second.

## The Enter box (1.7.85)

`DrawTalkPan` was six `DrawPanelBox` blits out of the vanilla `ctrlpan\talkpanl` composite: a top
cap, two tapered bevel loops, the black inset, and a 310×55 lower plate with three **VOICE** buttons
baked into it by `LoadMainPanel`. Those are Diablo's multiplayer voice-chat controls, and the panel
edging around them belonged to a HUD this fork no longer has. All of it is gone. What remains is a
black rectangle we draw, framed with the same `DrawOrnateBorder` the minimap and log wear.

**The text rect did not move, and could not.** `DrawString` returns how much fitted and
`ChatInputState->truncate(len)` cuts the input to it — so that 250×39 rect at `lineHeight 13` *is*
the length limit. The 66 zeroes across three rows are that rect. Changing it would have silently
changed how much can be typed, which is why the frame is drawn outside a margin around it rather
than inset into it.

### The history window

Moved into the minimap's column: same left edge and same width, so it is flush right by
construction; it rises from the main panel and **stops at the minimap's bottom border** instead of
sliding underneath it. Each message block wears the ornate frame.

Derived from `GetMiniMapScreenRect()`, the way `event_log.cpp` already does — the minimap's 306×175
is computed per zoom level and is not a constant anyone should restate. It no longer dodges the side
panels or caps at 540px; it has a column of its own now.

### The log stands aside

The log and the history would otherwise fight for that column, so opening chat closes it and closing
chat brings it back. Remembered rather than simply closed, so "temporary" means temporary.

### Retired with the buttons

The `ctrlpan\talkbutt` CEL load, `talkButtons`, `TalkButtonsDown`, and the VOICE/MUTE bake in
`mainpanel.cpp` that was painting into a `pBtmBuff` region nothing blits any more.
`ctrlpan\talkpanl` itself still loads — other panel drawing reads that buffer.

`control_check_talk_btn` / `control_release_talk_btn` survive as **no-ops** rather than being
deleted. Both are called from `diablo.cpp`'s mouse handlers ahead of every other click test, and
that ordering is the part that is easy to get subtly wrong.

## Verification

Debug clean at both versions; suite **445 of 447**, the two standing baseline failures. `Writehero`
did not move — nothing here touches the save format.

**To see it in game:** F9/F10 change speed and clamp at 20 and 60, with the readout blinking for a
second per press; F11 toggles the log; F12 takes a screenshot in normal play, not just on loading
screens. Press Enter for a plain bordered black box with no vanilla edging and no VOICE buttons,
still taking 66 zeroes across three rows, with the history filling the minimap column beneath it and
the log stepping aside until you press Escape.
