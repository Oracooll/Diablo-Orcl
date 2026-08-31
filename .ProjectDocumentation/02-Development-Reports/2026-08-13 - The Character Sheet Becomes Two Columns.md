---
date: 2026-08-13
version: 1.1.62
area: UI / Character sheet
---

# The Character Sheet Becomes Two Columns

## What was asked

> Let's arrange all data in two columns: Column 1 — type of data, i.e. Name, Class, Level, Experience, Next Level, Gold, Str, Mag, Dex, Vit, AC, To Hit, DMG, PtD, Res Mag, Res Fire, Res Lightning, Life, Mana. Column 2 — the data, current value of these. Column 1 text aligned to right of an imaginary text box, long enough to fit the longest text string, attached flush to the left edge of the Character screen. Column 2 — imaginary text box, attached a couple of pixels away from column 1, text inside aligned to the left.

Follow-up, on whether Str/Mag/Dex/Vit should show one value or two: *"I think we have enough width to fit both for now."*

## What the sheet was

Vanilla's layout, inherited unchanged through every previous pass: a scatter of hand-placed
`{ x, y }` literals in one `panelEntries[]` table. Two label columns at x 88 and x 253, a separate
top block at x 9/161/211, a "Gold" header whose y was patched at load time depending on whether the
small font is tall, and values drawn centred inside 45- and 57-pixel field slots. Row spacing came
out of the literals, so nothing was derivable from anything else — moving one row meant editing
coordinates and hoping.

The previous pass (v1.1.60) had already removed the field bezels and their backgrounds. What was
left was the same scatter with nothing behind it.

## What it is now

One table, `CharRows[]`, in the order the request lists. A row declares its label, one or two value
functions, and how much blank space to leave above it. Its only positional input is its index.

```cpp
struct CharRow {
    const char *label;              // "" for the Base/Now header row
    ValueFunc value;
    ValueFunc secondValue = nullptr;
    int gapAbove = 0;
    CharRowExtra extra = CharRowExtra::None;
};
```

Twenty rows — nineteen from the request plus a `Base` / `Now` header over the two value columns —
at 28px each, with a 7px gap opening five groups (identity, progression, attributes, combat,
resists, vitals). That lands at exactly 595px, which is exactly the content area between the title
band's separator and the bottom margin. A runtime assert holds it there.

### The columns

- **Label column** starts at x 6 — flush to the panel's left edge, clear of the 3px ornate bevel —
  and is **measured, not hardcoded**: its width is the widest translated label in the table, taken
  from the font at load time. Labels are right-aligned inside it, so their right edges line up.
- **Value column** starts 6px past the label column's right edge, left-aligned.
- **Second value column** follows 6px later, for the four attributes (Base / Now) and for life and
  mana (maximum / current, keeping vanilla's reading order).
- Rows with a single value get the whole remaining span rather than a 44px slot, because experience
  can run to thirteen characters.

Labels are drawn gold, values keep the colour coding they already had (blue for buffed, red for
drained or damaged, whitegold for capped). That colour coding is why showing both Base and Now is
not redundant — Now is the one that turns blue.

### Why the buttons are pinned right, not placed after the values

The four +stat buttons and the RESET button are right-aligned against the panel's edge, at a fixed
x, instead of being laid out after the second value column. That inverts the failure mode: a
translation with a long "Points to distribute" now widens the label column into the slack between
the values and the buttons, rather than pushing the buttons off the panel where they would be
undrawable and unclickable.

## Two invariants that had to move with it

**The +stat buttons.** `ChrBtnsRect[4]` (control.cpp) is read by four separate places — the mouse
press handler, the release handler, gamepad navigation, and the touch renderer — and written by
nobody. It is now written once, by the character panel's own layout pass, from the same row tops
the values are drawn at. Draw and hit-test cannot disagree because there is only one source.

The mapping from row to button index is a subtraction (`extra - StatStrength`), so three
`static_assert`s pin `CharRowExtra`'s stat order to `CharacterAttribute`'s. Getting that wrong
would compile, draw, and click — just on the wrong stat.

**The RESET button.** `ResetStatsButtonPosition` was a `constexpr Point` in control.h, shared with
the hit-test precisely so the two could not drift (they had, once, in OE-022). Its position now
depends on measured text, so it cannot be constexpr any more. Rather than let the constant rot, it
became `GetResetStatsButtonPosition()` in charpanel.hpp and both hit-test sites call it. The size
stays a constant in control.h.

## What was deleted

`LoadCharPanel()` no longer loads or composes anything. It used to render every label into an
`OwnedSurface` and encode that to a CLX held for the session — a baked image. Labels now have to be
drawn per frame, because their x depends on a measurement, so the CLX, the surface, `charbg.clx`
(already only being read for its dimensions), `PanelEntry`, `DrawShadowString`, the field-height
padding constants and the small-font-tall y patching all went with it. What used to be an image
composition is now a layout measurement.

`FreeCharPanel()` invalidates the measurement instead of releasing a sprite, so a language change
between sessions re-measures the label column.

## Files

- `Source/panels/charpanel.cpp` — rewritten around `CharRows[]`, `EnsureLayout()`, `DrawRow()`.
- `Source/panels/charpanel.hpp` — adds `GetResetStatsButtonPosition()`; content origin is now the
  panel's left edge rather than a centred 320px block.
- `Source/control.h` — `ResetStatsButtonPosition` removed, `ResetStatsButtonSize` kept.
- `Source/control.cpp` — both reset hit-tests call the new accessor.

## Verification

Debug config builds clean at `1.1.62`. No test references any of the changed symbols.

Confirmed in game (screenshot `2026-08-13 09-45-12`), and approved: *"From user point of view - I
like it."*

### The measurement was checked, not assumed

The first look at the screenshot suggested "Points to distribute" was starting 2-3px left of its
column — i.e. that the measured width and the rendered width disagreed, which would have been a
hole in the whole approach. Scanning the screenshot's pixels row by row for the first lit column
says otherwise: that row's leftmost lit pixel is at x=7 against a column starting at x=6, which is
the glyph's own left bearing.

The code agrees. `DrawString` computes its right-align offset as
`rect.x + rect.width - GetLineWidth(text, size, opts.spacing)` (text_render.cpp:433, :747) — the
same call, with the same spacing, that measures the column here. The longest label therefore starts
at exactly the column's left edge, by construction rather than by luck.

Measured result: label column 139px, values at x 151, second value column at x 201, buttons at 291.

### One real risk found and closed

139px of label against a ceiling of 177px is only 38px of headroom. A translation a third longer
than English — plausible for "Points to distribute" in German or Russian — would have pushed the
"Now" column under the +stat buttons: unreadable, and it would have happened silently, in a
language nobody here tests in.

`LabelColumnWidth` is now clamped to that ceiling, with a runtime assert behind it. The failure
mode becomes bounded and obvious instead: `DrawString` clips to its rect (text_render.cpp:767), so
an over-long label loses its leading characters at the column's left edge and can reach neither the
values nor the bevel.

`fmt/format.h`, `qol/stash.h` and `utils/display.h` were kept through the rewrite "to be safe" and
turned out to be dead once the composition path went; removed, and the build proves it.

## Tuning pass (v1.1.63)

Four adjustments after seeing it in game:

1. **Points to distribute moved under Vitality**, with no gap — it belongs to the attribute block
   it is spent on, not to the combat group it was sitting in. Row count and total height are
   unchanged, so the 595px fit still holds.
2. **Now and Base columns swapped.** Now is the left column: what the character actually has reads
   first, and Base sits on the right.
3. **The + buttons follow Base** instead of being pinned to the panel's right edge — Base is the
   number they increase, so they now sit 8px past it. That makes the whole run a single chain
   hanging off the measured label width, and the clamp was rewritten against the new ceiling (179px,
   against English's 139).
4. **RESET moved under Base**, on the points row, directly below the four + buttons that spend
   those points.

Life and mana were swapped to match: current on the left with Now, maximum on the right with Base.
Leaving them in vanilla's max-first order would have meant "what you have" changing sides depending
on which half of the sheet you were reading.

The points row is now the one single-value row that cannot take the full width out to the margin,
since RESET occupies its Base column — it takes a column-width slot instead.

The RESET and + buttons were also **not clickable** at their previous positions, for a reason that
had nothing to do with this layout. See
[[2026-08-13 - Three Windows Routing Clicks Through a Rect They Outgrew]].

## Open

- The spell book is still on the old layout — 340x720, title, separator, and as many spells as the
  new height allows. It needs `SpellPages[6][7]` repartitioned and the tab row moved off its
  hardcoded `y = 348`.
