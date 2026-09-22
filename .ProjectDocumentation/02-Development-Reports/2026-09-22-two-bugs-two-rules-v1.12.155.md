# Two bugs, two rules — v1.12.155

2026-09-22

Four things, two of them reported as bugs and both real.

## Bug: a rerolled weapon lost its affix lines

> when i rerolled a weapon i stopped seeing its affixes on its pop-up display.

A magic item carries its affixes **twice**: in `_iPrePower` / `_iSufPower`, the vanilla pair
that `PrintItemDetails` prints directly, and in the `_iOracoolAffixes` record, which until now
only a **tiered** item printed — rare, buffed unique, primal, set.

`RebuildOracoolItemWithAffixes` — the Mystic's reroll — rebuilds the item from its base with
`GetItemAttrs`, which clears both vanilla slots, and replays the affixes through
`SaveItemPower`, which writes the stats in and sets neither slot again. So a rerolled **magic**
weapon kept its stats, its name and its value, and lost every affix *line* on the pop-up. A
rare or a primal was fine, because tiers print the record. That is why it read as random.

Fixed in the printer rather than in the item: the record is the truth either way, and the new
branch runs only when neither vanilla slot is set and the record is not empty, so it cannot
double-print. It also gives the crafted items that write straight into the record — see
`oracool/crafting.cpp` — the affix lines they have never had.

## Bug: a 2x3 item shrank to one cell on the craft bench

> i placed a 2x3 item in the smaller frame in Craft tab and it shrunk down to 1 slot only. Fix
> it. Size remains 2x3 when placed there. items keep original size when placed in that frame.
> they still leave 6 unoccupied slots for ingredients.

The craft grid was **one item per cell whatever its size** — the bench's own rule applied
twelve times. That was defensible while the grid was invisible and each cell held one reagent;
it stopped being defensible the moment the canvas painted twelve cells, because an item drawn
to fit a 28px cell is an item shrunk to a twelfth of itself.

It has footprints now, modelled on Levski's Cube, which has had them all along:

- `CraftCells[c]` holds the anchor covering cell `c`, plus one — the Cube's `GridCells` by
  another name and for the same reason: a 2x3 occupies six cells and every one of them has to
  name the same item, or the outlined item and the described item are two different items;
- `CraftFitsAt` / `CraftMarkCells` / `PlaceInCraftGrid` / `TakeFromCraftGrid` do the placing;
- a click anywhere on a 2x3 picks up the whole item, and a drop prefers the cell under the
  cursor and falls back to the first that fits.

**The recipes did not change.** `CraftGrid` is still an `Item` array indexed by anchor and is
still handed to `CanCraftFromLevskiGrid` unchanged; the covered cells live in the occupancy
map beside it, not in the item array. A 2x3 takes six of twelve and leaves six, which is
exactly the arithmetic the user did.

### The undo that came with it

A recipe rewrites the grid in place with no idea of footprints — four powders and a sword
become a sword — and the result's size is not the inputs'. So the map is re-derived after every
transmute, largest footprint first, and **re-deriving it can fail**: twelve array slots are not
twelve free cells once items take more than one each.

The Cube's own comment records what happens when that failure is discarded — "no room" becomes
an item that quietly stops existing. So the transmute snapshots the whole bench first and puts
it back if the relayout fails, on both hosts. This was not asked for; it is the failure mode
the feature brings with it.

### The bench had the opposite bug in the same frame

On Reroll and Imbue the item was *fitted to the frame*, which blew a ring up to 87x115. It
draws at its own size, centred, now. Nothing needs fitting there: the frame is 87 by 115 and
the largest item in the game is a 2x3 at 56 by 84.

## Rule: the settled item's affixes go green and red

> when an affix is rerolled, use red font for other affixes from now on as they are now
> untouchable. use green text for the rerolled one.

The first reroll locks an item to one affix for good, and until now the others were merely
dimmed to whitegold — a shade, not an answer. Red says they cannot be worked; green says which
one still can.

Asked of `counters.lockedAffix` rather than of the reroll count, because that field **is** the
rule: it is what `RunControl` refuses on, so the colours and the refusal cannot drift apart.

## Rule: the lone plate moves to the frame

> bring transmute icon 6px away from smaller frame of gillian in craft tab. i dont want it so
> far away.

The row was anchored left and centred a row of one, which put Craft's Transmute and Reroll's
plate seventy pixels from the frame they act on. It is anchored at the **six-pixel clearance**
now and distributes leftward, so a lone plate simply *is* that anchor — and it lands exactly
where Imbue's third plate stands, which is the one place on the page the eye is already used to
finding a button.

## And her Imbue got its own drawing

> use this icon as Imbue button icon - "Imbue - rune shield glyph 56x56.png"

It held Griswold's old star for one build, after his Recharge became a fuel pump. Cut by
`tools/CutShopGlyphs.ps1` with the rest; its ink is 39x42, so it fits to 20x21 and survives the
trip to 28px cleanly.

## Files

- `Source/items.cpp` — the pop-up branch
- `Source/oracool/workshop.cpp` — footprints, colours, the row anchor, the bench draw
- `tools/CutShopGlyphs.ps1` — the rune shield added
- `Packaging/resources/oracool_assets/ui/shop_glyph_imbue.png` (replaced)

Built clean, 832/832. `oracool.mpq` repacked.

## What to look at in play

1. Reroll a **magic** weapon — not a rare — and read its pop-up. The affix lines stay.
2. Reroll once more and look at the list: one green, the rest red.
3. Drop a 2x3 weapon on her craft bench. It covers six cells at its own size, six stay free,
   and clicking any part of it picks it up.
4. Fill the bench with a 2x3 plus reagents and transmute. If the result cannot be laid out the
   bench should come back exactly as it was, with a message — nothing lost.
5. Her Imbue button: a rune shield.
