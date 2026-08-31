# Plan: what comes next

**Written 2026-08-21, against v1.9.3.** Supersedes the ad-hoc "what's next" answers; the Pipeline
table stays the master list, this is the reading of it.

## Where the project actually is

The item layer is finished and audited. 143 uniques, 94 set pieces, 35 gems, 33 runes, 15 charms,
370 runewords, 134 tier items - every one has a drop path, every gate is under the level ceiling,
852 affix values match their source packages, and no added item leaks into the seeded pool.

What is NOT finished is everything that gives those items somewhere to be found and someone to take
them from. That is the shape of the next phase.

## Three things blocked on you, not on code

| | What is needed |
|---|---|
| Levski's Roar window art | The window still wears the generic ornate border. The monument itself is done. |
| The stash chest | Two packs rejected. The next one needs its three states drawn at **one scale** - a shared cut box pins the frame, not what is inside it. |
| Retire the Crafting window | Needs `menu_icons.png` recut, because removing a burger entry shifts every icon after it. |

Also blocked on art, and newly so: **the Sorcerer's tree**. Four of seventeen rows pay out and the
other thirteen are honestly inert. A fuller tree means new rows, and a row needs an icon in her
strip.

## Recommended order

### 1. Telemetry-driven balance pass - Medium, unblocked

**Do this first.** The CSV has been recording every kill and every drop since Phase 0.9 and **has
never been read back**. Everything below this line is tuning guesswork until it has been.

It also answers questions this week raised and could not settle: is 3% the right rate for a named
set piece, is 8% right for a tier item, do the deepest runes ever actually appear in a real session.
Those are currently my estimates, not measurements.

Cheap, self-contained, and it makes every later balance decision evidence-based.

### 2. Named set drops, the rest of the row - Medium, unblocked

Sets now drop piece by piece. Two gaps remain from the original row:

- **No gold mechanic tied to them.** The Rat King's Tithe is *about* gold and cannot express it:
  `_pGoldFind` exists and charms feed it, but no `IPL_` writes it from an item. Wiring one is small
  and would light up that set plus the gold_from_monsters affix token currently marked Inert.
- **Sets do not drop as sets.** Worth deciding rather than building: "drops as a set" collapses the
  chase the 3% rate exists to create. My view is it should stay piece-by-piece and the *hint* should
  improve instead - a dropped piece naming its set and how many you have.

### 3. Aura-carrying champion packs - Medium, unblocked

The cheapest large increase in how the game *feels*, because both halves already exist: the aura
system runs the Paladin's tree, and lesser-unique affixes already roll on monsters. A Fanaticism
pack is those two systems meeting.

### 4. Jewels - Medium, save-breaking

The third socket family, and the only one with rolled rather than fixed effects. Gems and runes have
proved the machinery; jewels reuse it wholesale. Save-breaking, which is free under the always-new-
game rule but should still be batched with anything else that breaks the format.

### 5. TRN recolour monster variants - Medium, unblocked

The cheapest bestiary multiplier there is, and a prerequisite in spirit for Zone 1: it proves the
per-zone roster idea before any new tileset exists.

## Deliberately not next

- **The 107 absent uniques.** They need ten base-item families that do not exist - shoulder mantle,
  reliquary, cloak, battle cloak, spear, pike, war lute, arcane focus, war quiver, canticle. That is
  a content project of its own, and the set-piece argument for it evaporated when relic and cloak
  were re-slotted.
- **Zones 1 and 2.** Large, and better attempted once the telemetry pass has said what the existing
  content is worth.
- **Legendary powers, hirelings, transmog, paragon.** All Phase 6, all fine, none of them urgent
  while the game has no zone-specific reason to farm.

## One standing risk

The consumables crash is fixed by reasoning across v1.8.94-98 and **has never been reproduced**. If
garbage rows ever reappear in Griswold's list, the remaining suspect is the row-to-index mapping in
`WitchBuyEnter` rather than the draw - and the evidence would be worth capturing before anything
else is changed.
