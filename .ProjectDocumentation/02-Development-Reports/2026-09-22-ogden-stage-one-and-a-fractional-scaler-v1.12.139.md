# Ogden's nine, part one — and a sprite scaler that takes a box — v1.12.137–139

**Date:** 2026-09-22
**Version:** v1.12.139 (v1.12.137 failed to compile; see below)
**Branch:** renderer-32bit

Seven of the user's nine Ogden items, plus the fractional scaler they asked for once the eighth
turned out to need one. Items 6 and 7 - the in-window Recipes page and the new Craft tab - follow.

## 1. Titles up forty

Only the titles. The two arrow plates keep their four-pixel clearance over the painted band, so the
name now stands clear of them rather than sharing their line.

## 2. Griswold's gold, on every tab

His pile and his bare number, his own file, in place of the centred "Gold: 12,345" at y 306.

Not at *his* y: his grid ends at 618 and this window's frame at 600, so "under the grid" is a
different number in each. It sits at the window's foot, and the area under the frame was restacked
around it - message at 602, gold at 626, and the rune confirmation moved to a stacked pair at x 110.

**Everything down there stays left of x 175**, where `GetHealthOrbRect` begins. Griswold's
Refresh-until ends at 154 and the stash's gold at 165 for the same reason. That is why the two
answers stack instead of sitting side by side: 110..172 is one button wide. A `static_assert` holds
the gold clear of them, and it earned its keep immediately - the first draft had the number 140 wide
and the assert refused to compile.

## 3, 4. The stash counts too

`CountInPack` walked the backpack and its tabs and stopped. A player whose gems were all in the
stash saw a board of red zeroes and greyed arrows while standing on a hoard of them.

```cpp
int CountOwned(player, idx)  { return CountInPack(player, idx) + CountInStash(idx); }
void TakeOwned(player, idx, n)  // pack first, then the stash
Landing GiveOwned(player, idx)  // backpack, else stash, else the floor
```

Pack first is deliberate: it is what the player is carrying and can see without opening another
window, and spending it keeps the stash as the deeper store. `TakeFromStash` walks the list
**backwards**, because `RemoveStashItem` erases and every index after it shifts down.

The landing is reported under the grid, with the item's name - a board of thirty-five cells makes
"Sent to Stash" alone ambiguous. The floor is a last resort rather than a refusal: by then the
materials are spent, and refusing would mean either swallowing them or unwinding the whole step.

## 5. Griswold's tabs

`TabRect` asks `GetSideTabRect`; the column draws with `DrawSideTab`. The local 27x80 plates and the
stack-of-capitals label are gone, along with `TabTop`/`TabSize`/`TabGap`.

Worth stating: this window docks in *exactly* the shop panel's rect, so its tabs had to be the shop's
tabs. A column of its own at a 27px width and a 96px top was the same furniture in the wrong place -
invisible until the two windows sat in the same slot, then obvious.

## 8. Ogden's welcome, and why it went

> Ogden is silent when clocked on - bring his welcoming audio file back.

The first branch of `TalkToBarOwner` is `!player._pLvlVisited[0]`, **true for any character who has
not yet been down to level 1**. Since the quest speech was deferred on 2026-09-20, that branch queues
the intro, opens the menu and plays nothing - so an early click was silent, and his welcome only ever
reached the ear after the first dungeon visit with no quest pending.

He now sounds it on every click, as **audio only**: `PlaySFX(Speeches[TEXT_OGDEN1].sfxnr)` rather
than `TownerTalk`, because `InitQTextMsg` opens the quest-text panel over the menu and keeping that
panel off his click is the whole point of the September change. Both requests, at once. The trailing
`TownerTalk` went with it, or the non-quest path would have played him twice.

## 9. The jewels' own board, and the scaler

Their board is **5x3 of 42x50 cells** - the opening divided by five and by three, filling it. The
gems and runes keep 7x5 of 30px.

The icons were the interesting half. `DrawSpriteScaled` takes an **integer** factor, so a 28px sprite
can be 28 or 56 and nothing between: asked for 80% of a 42px cell (~33) it could only undershoot by a
third or overshoot by a third. So:

```cpp
void DrawSpriteToFit(const Surface &out, Rectangle target, ClxSprite sprite, const uint8_t *trn = nullptr);
```

**It takes a box, not a factor.** That is the actual fix - "80% of this cell" is a question about the
destination, and a scale factor can only answer it when the ratio is a whole number.

**It is destination-driven.** Each destination pixel asks which source pixel it came from. The
reverse - walking the source and writing a block per pixel - is the obvious way to write it and is
wrong at fractional ratios: wherever two destination pixels map to one source pixel, a source-driven
loop leaves one unwritten and the icon comes out with holes punched through it.

It lives in `ornate_border.h` with the theme's other primitives, so the Cube's grid and any future
one can use it.

Gems and runes keep their natural 28px on purpose: their cell is 30, so fitting *them* to 80% would
have made those icons smaller.

## The failed build

**v1.12.137**: `const std::string name = _(...)`. `_()` returns a `string_view` here, which fmt takes
and `std::string` will not implicitly adopt.

The ordering check run just before that build is the counter-example worth keeping: `ShapeFor(Tab)`
had landed 130 lines above the `Tab` enum, and grepping for the declaration order caught it for
nothing. Looking costs one command; assuming costs a build.

## Files

- `Source/oracool/workshop.cpp` — items 1, 2, 3, 4, 5, 9.
- `Source/oracool/ornate_border.h` / `.cpp` — `DrawSpriteToFit`.
- `Source/towners.cpp` — item 8. **CRLF**, so edited with the Edit tool.

No asset changes; the canvases were packed at v1.12.133.

## Outstanding

- **6.** The Recipes tab still opens the Levski window instead of listing his recipes on his own
  recipe canvas.
- **7.** No Craft tab yet - his cube canvas with a transmute grid and the Transmute plate.
