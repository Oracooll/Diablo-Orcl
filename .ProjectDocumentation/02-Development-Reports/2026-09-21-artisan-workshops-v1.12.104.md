# The Mystic Workshop, and the Jeweller's page (v1.12.104)

**Date:** 2026-09-21 · **Version:** v1.12.104 · **Tests:** 832/832

User: "Build Mystic Workshop with my answers from the artefact. Use placeholder canvas and ui buttons ... Also Build
Jeweller workshop"; "Both their dialog windows to have the same options as Griswold: Talk to XXXXX / Enter Shop /
Leave XXXXX"; and then their painted canvases.

## The dialogs

Ogden and Gillian read like Griswold now: Talk to Ogden / Enter Shop / Leave Ogden, and Talk to Gillian / Enter Shop
/ Leave Gillian. Ogden's Enter Shop opens his workshop page; Gillian's opens the Mystic Workshop.

## The Jeweller's workshop

`ArtisanWorkshopGeometry` in levski_roar.cpp: the transmute window at 340x720, docked where the shop panel sits, worn
by both artisan hosts. Its canvas is the user's painting (`ui\artisan_workshop.png`); the grid, the recipe bezel, the
track and the TRANSMUTE plate are drawn in code over it as placeholders, and the shared side panel stands in when the
painting is missing. `ListSkinGeometry` gained `docked`, which is what moves the window to the panel's slot.

## The Mystic Workshop (`oracool/workshop`)

A new window at the same size and dock, wearing Gillian's painting (`ui\mystic_workshop.png`), with its two tabs in
the column beside it and every control drawn in code. Built to the user's verdicts on the Artisans page:

- **The bench** is one 2x3 slot that holds exactly one item whatever its size ("Make it 2x3, capable of holding only
  1 item"). Click to put an item down or take it back; the window returns it when it closes.
- **Reroll** lists only the item's own rolled affixes. Pick one, press REROLL, pay, and the board shows a MENU: the
  affix as it stands beside three the pool offers at this item's level ("Go with Menu"). The pool is not narrowed to
  the affix's family, so a better roll of the same affix can be among them, which is what the user asked for.
  Choosing an alternative rebuilds the item with it; choosing the first keeps what was there.
- **The lock**: the first reroll settles which affix that item may ever reroll, and the list greys the rest.
- **Imbue** lists the shards already on the item. IMBUE works in the first shard from the pack the item can take,
  REMOVE takes the selected one off, CLEANSE takes them all, each priced and each destroying what it removes.
- **The rising cost** is per item and doubles per attempt: 500 gold a level for a reroll, 250 for a removal, and a
  cleanse is a removal per shard. The counters live for the game, keyed by the item's seed, deliberately NOT an item
  field yet - that is an item-format bump, and V1 always starts a new game.
- **Refusals**: gold, quest items, unidentified items and anything held together by a runeword.
- Every control presses and RUNS ON THE RELEASE, inside the button that was pressed - the rule of v1.12.102.

## The affix engine

Two new doors in items.cpp, because an affix's stats are written into the item's own fields as it is rolled and there
is no way to take one back out:

- `RollOracoolAffixFor` rolls one affix for an item on a SCRATCH copy and returns only the record, so a menu can be
  shown before anything is applied.
- `RebuildOracoolItemWithAffixes` rebuilds the item from its base and replays exactly the affixes given, carrying
  across the seed, item level, tier, sockets and their stones, the shard ledger, the name, the ethereal bargain and
  Kanai's unbound level.

## Left for the next pass

Gillian's four grid recipes (Rework Charms, Recast Set Pieces, Enrich Magic, Cleanse Shards) have no door now that
her menu line opens the workshop: they want a third tab on it. The workshop's own CLEANSE covers the fourth.
