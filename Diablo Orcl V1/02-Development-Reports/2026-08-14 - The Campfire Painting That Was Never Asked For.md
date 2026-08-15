---
date: 2026-08-14
version: 1.5.20
area: Front end / character creation art
status: reverted
---

> **Reverted at 1.5.20** - the painting is out again on the user's call. See the closing section; the
> rest of this report is kept for what the investigation turned up, which outlived the change.

# The Campfire Painting That Was Never Asked For

> yes, wire up the choose hero background

## Found while counting screens

Auditing the twenty front-end screens turned up an asset that ships and is never requested.
`ui\choose_hero_bg.png` - 1.74 MB, the six classes around a campfire - sits in `oracool_assets`, has
a slot in `UiBackground` and a comment naming the screen it belongs to, and **no call site**.
`git log -S "UiBackground::ChooseHero"` returns nothing, so it was never wired and later removed: it
was declared and then forgotten.

`AddSelHeroBackground` handed **all three** hero screens `UiBackground::HeroSelect`, so the class
list has been showing the neutral hero/settings painting since the day the campfire was cut.

## Why one line would not do it

The obvious fix - change the slot in `AddSelHeroBackground` - fails, because that function is not
"set the background at the top of the dialog". It is also called to put the background **back** after
a modal takes it away:

- `SelheroClassSelectorSelect` - the shareware refusal, raised **from the class list**.
- `SelheroNameSelect` - the invalid-name message, raised **from the name box**.

Those two want different paintings restored. So the background became state rather than a constant:

```cpp
oracool::UiBackground selhero_background = oracool::UiBackground::HeroSelect;

void SetSelHeroBackground(oracool::UiBackground slot)
{
	if (slot == selhero_background) return;
	selhero_background = slot;
	RemoveSelHeroBackground();
	AddSelHeroBackground();
}
```

`AddSelHeroBackground` reads the variable, so both restore paths give back whatever the interrupted
screen was showing, without either of them having to know which screen that was.

## Where it is set, and why there

Three call sites, chosen so that every path is covered by one of them rather than by enumerating
paths:

| Set to | Where | Covers |
|---|---|---|
| `ChooseHero` | the class-list branch of `SelheroListSelect` | the New Hero button, Esc from the name box, the shareware refusal, **and** the fresh-install path that opens straight onto the class list |
| `ChooseHero` | `SelheroClassSelectorSelect` | forward into the name box - see below |
| `HeroSelect` | `selhero_List_Init` | Esc from the class list, Esc from the multiplayer Continue prompt |

`selhero_Init` resets the variable **before** `AddSelHeroBackground` reads it. The dialog is
opened and closed repeatedly in a session, and walking forward from the class list into the
difficulty screen leaves the variable on `ChooseHero` - without the reset, the next visit would open
the character list on the campfire.

## Cost

Effectively none. `ui_backgrounds` caches each slot's quantized sprite against the screen size and
the palette, per slot - so the swap builds each painting once and every later crossing is an insert.
The two screens run on the same pinned `ui_art\diablo.pal` (`AddUiBackground` pins it before `Build`
quantizes), so the cache key does not flap between them either.

## Scope: the name box too

> carry the campfire through the name box too

Wired to the class list first, which is what the asset's own comment says it is for - and that made
the sequence alternate: characters, campfire, **characters**, difficulty. A painting that flicks away
and back in the middle of a job. Picking a class and naming what you picked are one act, so the
second table row above now sets `ChooseHero` as well, and the background changes when you START
making a character and not again until the character exists.

That row is a no-op at runtime today - both ways into the name box (the class list, and the retry
after a rejected name) already have the campfire up. It is stated anyway so the name box declares its
own background rather than inheriting one by luck, and so there is a line to change on the day it
wants art of its own.

It also retires a trap the first pass had to work around. While the two screens wanted different
paintings, the `SetSelHeroBackground` call in `SelheroClassSelectorSelect` had to sit **below** its
early return, or the shareware refusal - which returns to the class list - would have lost the
campfire on its way out. With both on `ChooseHero` the position no longer matters.

## Files

- `Source/DiabloUI/hero/selhero.cpp` - `selhero_background`, `SetSelHeroBackground`, three call
  sites, the reset in `selhero_Init`.

## Verification

Debug config builds clean at `1.5.16`; full suite **352/354**, the two known pre-existing failures.

The asset was checked as reachable rather than assumed: `build\x64-Debug\assets\ui\choose_hero_bg.png`
is byte-identical to the source (1,778,017 bytes) and sits in the same directory
`hero_settings_bg.png` and `main_menu_bg.png` load from, so the load path is the proven one. Worth
knowing what a miss would have looked like: `AddUiBackground` returning false does not fall back to
the other painting, it falls back to the stock 640x480 `ui_art\selhero` plate - which would have been
a visible step backwards, not a silent no-op.

Not seen in game. Worth confirming: the campfire comes up when you start making a character and stays
up through naming it, Esc from the name box keeps it, and Esc from the class list restores the
characters' own painting.

## Reverted, 1.5.20

> replace the new hero background with the background from the other front end screens. this one
> doesnt fit the d2r style.

Seen in game, and the answer was about the art rather than the wiring: the campfire does not sit with
the rest of the front end. The class list and the name box are back on `hero_settings_bg.png` with
the character list, which is where they were before 1.5.15.

Everything the wiring added came out with it - `selhero_background`, `SetSelHeroBackground`, the
three call sites, the reset in `selhero_Init`. With one painting behind all three screens there is
nothing to choose between, and the two places that put the background back after a modal have no
question to answer. `AddSelHeroBackground` is a plain function again.

`UiBackground::ChooseHero` came out of the enum too, **rather than being left unwired**. That is the
exact state this whole report began by finding: a slot with an asset and no caller, which took an
audit to notice. Recreating it deliberately would be worse than never having removed it.

`ui\choose_hero_bg.png` (1.74 MB) is still in `Packaging/resources/oracool_assets/ui` and is now
referenced by nothing. Left in place rather than deleted - it is the user's art and deleting shipped
work is their call, not mine - but it is dead weight in the packed MPQ and should either come out or
find a screen.

**What outlived the change.** The reason a one-line fix was not enough - that `AddSelHeroBackground`
is also the "put it back after a modal" path - is a property of this screen, not of the campfire, and
the next person to give one of these screens its own art will meet it again. That is the part of this
report worth keeping.

Debug config builds clean at `1.5.20`; full suite **352/354**, the two known pre-existing failures.
