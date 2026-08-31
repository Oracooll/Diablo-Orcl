---
date: 2026-08-13
version: 1.1.76
area: UI / Item hover panel
---

# Item Specs Get a Colour Per Line

## What was asked

> Dial down the brightness of the outline of the tool tip pop-up window. To be as dim as the outline
> of the silhouette.
>
> We need to work on Item Specs design:
> 1. Name to be colored in item tier color
> 2. Basic stats to be colored WHITE
> 3. Prefixes to be colored Blue
> 4. Affixes to be colored Blue
> 5. Requirements - Basic stat, so WHITE.

## The tooltip border, and one constant instead of two

The tooltip's border was `PanelBorderColor = 194` - which is `PAL16_YELLOW + 2`, the exact value the
silhouette's outline started at before being walked down to +9. Being asked to match them is a
standing requirement, not a one-off edit, so they became one named constant rather than two literals
to keep in step:

```cpp
// oracool/ornate_border.h
constexpr uint8_t ThemeEdgeColor = PAL16_YELLOW + 9;
```

Two thin gold edges on screen at once get compared by eye, and a promise to hand-edit two numbers
together is broken the first time only one of them is tuned.

## Why the item panel needed new plumbing, not a new colour

`DrawString` takes **one** colour for a whole string, and the hover panel drew all of `InfoString`
in `InfoColor`. That is exactly right for every other hover in the game - a monster name, an NPC, a
shrine - which is one line. An item is not one line: it is a name, then what the item *is*, then
what was *rolled onto* it, then what it *demands of you*. Those are four kinds of information
sharing one string.

So the text now carries a colour alongside it:

```cpp
void AddPanelString(string_view str, UiFlags color);   // records the colour
void SetPanelString(StringOrView str, UiFlags color);  // replaces, seeding line 0
extern std::vector<UiFlags> InfoStringLineColors;
```

### Per line, not per call

`PushLineColors` counts newlines and pushes one entry per **line**, not one per call. Several
callers pass strings with an embedded newline ("Right-click to read, then\nleft-click to target"),
and counting calls would slide every colour after the first such string one row up - a bug that
would look like a random mis-colouring rather than an off-by-one.

### The fallback is load-bearing

`DrawCursorTooltip` uses the per-line colours only when
`InfoStringLineColors.size() == lineCount`, and otherwise draws the whole block in `InfoColor`
exactly as before. This is not defensive padding: every non-item hover populates `InfoString`
directly without going through `AddPanelString`, so an empty or mismatched list is the *normal*
case for most of the game, not an error. `ClearPanelStrings()` replaced the six bare
`InfoString = {}` sites so the two can never be cleared apart.

## The scheme

| Line | Colour | Source |
|---|---|---|
| Name | the item's tier colour | `Item::getTextColor()`, already tier-aware |
| Tier label ("unique item", Oracool tier name) | the same tier colour | see below |
| damage / armor / durability / charges | white | `ItemBaseStatColor` |
| Prefix and suffix powers | blue | `ItemAffixColor` |
| Oracool tier affixes (up to 3+3) | blue | `ItemAffixColor` |
| Unique item powers | blue | `ItemAffixColor` |
| `Required: N Str ...` | white | `ItemBaseStatColor` |

Two lines were not in the specification and needed a decision:

- **The tier label** is not a base stat and not a roll - it names the tier, so it takes the tier's
  colour along with the name it belongs to.
- **"Not Identified"** (the unidentified view) takes the affix colour, because it stands in for the
  affix lines being withheld. It is a statement about the rolls, not about the base item.

The two colours are named constants with the reasoning attached, so the scheme is stated once
rather than inferred from thirty call sites: base stats are what the item **is**, affixes are what
was **added**, and a glance separates them without reading a word.

## Files

- `Source/oracool/ornate_border.h` - `ThemeEdgeColor`.
- `Source/oracool/cursor_tooltip.cpp` - border colour, per-line rendering.
- `Source/oracool/hud_art.cpp` - silhouette outline reads the shared constant.
- `Source/control.h` / `.cpp` - colour-carrying overloads, `SetPanelString`, `ClearPanelStrings`,
  `InfoStringLineColors`.
- `Source/items.cpp` - `ItemBaseStatColor` / `ItemAffixColor` and every item-path call site.

## Two bugs this shipped with, and what they cost

### A blind regex rewrote a function into itself (v1.1.74)

Routing the six `InfoString = {};` clear sites through the new `ClearPanelStrings()` was done with a
`sed`. The pattern also matched the line **inside `ClearPanelStrings()` itself**, turning its body
into a recursive call. Infinite recursion, stack overflow on the first clear - which happens during
load, so the game crashed on the loading screen. It compiled without a warning, because it is
perfectly valid C++.

The specific lesson: never run a pattern-based rewrite across a file where the pattern's *definition*
was just added. Either write the call sites by hand, or define the function afterwards.

The audit that followed found three more clears the regex had missed - `InfoString = StringOrView {}`
in `CheckPanelInfo`, and two differently-indented `InfoString = {}` - which would not have crashed
but would have let one frame's colours survive into the next frame's text. There is now exactly one
`InfoString = {}` in the codebase, inside `ClearPanelStrings()`.

### The colours were right and never ran (v1.1.75)

The first version converted only `GetItemStr` - the **ground item** path. Three others set the item
name with a bare assignment and then called `PrintItemDetails`: `CheckInvHLight` (inventory),
`CheckStashHLight` (stash), and the held-item hover. Each left the colour list exactly one entry
short of the block that followed, the tooltip's size check correctly refused all of it, and every
item rendered in a single colour - which is what the user reported after the crash was fixed.

The fallback being **silent** is what let this ship. So the invariant was made total rather than
partial: the six remaining "assign, then append" blocks (Menu Bar, Town Portal, Experience Meter,
the spell button, and two portal hovers in cursor.cpp) now go through `SetPanelString` as well. That
means *if the colour list is non-empty, it has exactly one entry per line* holds everywhere, which
makes it assertable - and it is now asserted in `DrawCursorTooltip`. The same mistake in future
fails loudly in Debug instead of quietly rendering monochrome.

## Dropping the duplicated "Indestructible" (v1.1.76)

The panel read:

```
Small Axe of the Ages
damage: 2-10  Indestructible
Indestructible
```

The base line and the suffix line both stated it. Per user request the base line now yields:
`AffixStatesIndestructible()` checks whether any line PrintItemDetails is *actually going to print*
says it - the vanilla pre/suf power, an Oracool tier's affixes, or a unique's powers - and if so the
base line drops the word, leaving just `damage: 2-10`.

It mirrors what gets printed rather than asking the item whether it has the property, because a
unique's powers are only listed for uniques and a tier's affixes only for tiered items. An item can
be indestructible with nothing below to say so; those keep the word on the base line, which is then
the only place it appears. `PrintItemDur` (the unidentified view) is untouched for the same reason -
it prints no affix lines at all.

The result also says more than the merged line did: indestructibility now appears in blue, marking
it as rolled rather than inherent.

## Verification

Debug config builds clean at `1.1.76`. `inv_test` - the only suite touching this path - passes 52/52.

Wanted in game:
1. Hover a magic item: blue name, white damage/armour/durability line, blue prefix and suffix,
   white requirements.
2. Hover a Rare / Buffed Unique / Primal item: name and tier label in that tier's colour, its
   affixes blue.
3. Hover an unidentified item: white stats, "Not Identified" blue.
4. Hover a **monster, NPC or shrine** - these must be unchanged, single-coloured. That is the
   fallback path and the thing most likely to have been broken by this change.
5. The tooltip's border and the silhouette's outline should now read as the same gold.
