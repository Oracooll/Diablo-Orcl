---
date: 2026-08-16
area: Strategic roadmap
status: draft for user review
---

# Megaplan: Beyond D2, in D1's Clothes

The user's brief, verbatim in spirit: *an improved D2 with D1 graphics - which I like better than
D2. One town, Tristram, forever. New dungeon ZONES rather than new acts. AI-generated 2D assets,
arranged by the engine. Sprite scaling so existing art works harder.*

This document is the whole map. Each phase is sized, ordered by dependency, and honest about risk.
Companion reading: [[D1 vs D2 Engine Comparison and Import Feasibility]] (06-Reference).

---

## Part I - The three questions answered

### "Why can't we introduce new Acts?"

We can - and the proof is already running inside this executable. **Hellfire's Hive and Crypt ARE
new zones bolted onto D1 without a new town**: new tilesets, new entrances from the same overworld,
new monster rosters, new music, reusing existing dungeon generators underneath. This fork already
absorbed both (25 waypoints, Hellfire integration audit, 2026-08-14). Adding OUR own zones is the
same recipe with our own ingredients.

What a new zone actually consists of, in this engine:

| Ingredient | What it is | Source |
|---|---|---|
| Tileset | .CEL tile art + .MIN/.TIL assembly + .SOL walkability + .AMP automap shapes | AI art + my cutting/metadata tools |
| Palette | 256-colour .PAL + lighting falloff tables | Derived from the art at build time |
| Generator | Room/corridor layout algorithm | REUSE - the four DRLG generators are tileset-agnostic at heart (Crypt reuses Cathedral's, Nest reuses Caves') |
| Entrance | A trigger tile + transition | Precedent: Hive/Crypt openings in town, stairs on deep levels |
| Monsters | Roster per level | Recolors + scaling first (Part II), AI sprites later |
| Waypoint | List entry + unlock bit | Machinery freshly widened to 24 bits; widen again as needed |
| Save format | Level-state slot + waypoint bits | Known quantity, we have done it |

No second town needed, ever: zones hang off Tristram (a cave mouth, a shrine, a torn-open grave)
or off existing dungeon depths, exactly as Hive and Crypt do. Tristram stays the single warm hub -
which is also cheaper: towns are the most art- and script-dense levels in the game.

### "All assets are 2D, right - can I generate infinite amounts with AI?"

Yes, with a pipeline, and with honesty about which assets are AI-friendly:

- **Friendliest: dungeon tilesets.** Floors and walls are static, need no animation, and forgive
  style drift far more than characters do. The workflow: the user generates large mood sheets
  (stone, roots, ice, flesh, bone - whatever the zone is); I quantize to the 256-colour palette
  (pipeline exists - it built the HUD), cut into the 2:1 isometric tile geometry, author the
  .MIN/.TIL/.SOL/.AMP metadata with a new tool, and wire a generator to it. First zone is the
  expensive one (the tool gets built); every zone after is mostly art time.
- **Friendly: items, UI, portraits, static props.** Already proven - the entire item-icon catalogue
  (140+ icons), HUD, inventory panel, waypoint pads all came through this pipeline from user art.
- **Hardest: monsters.** A monster needs 8 directions x several animations (stand, walk, attack,
  hit, death) x consistent identity across every frame. AI models fight consistency across
  rotations. Mitigations, in order of cheapness: (1) recolors of existing monsters via TRN -
  machinery exists (divine TRN, lesser-unique tints); (2) SCALING existing monsters (below);
  (3) AI-generated monsters reserved for slow, bulky, symmetric designs (blobs, golems, worms,
  totems) where direction consistency is forgiving; (4) the PNG->CLX importer built for player
  sprites already handles the import half.

So: infinite is optimistic, but *"a genuinely new-looking zone every few sessions"* is realistic.

### "We must introduce sprite scaling"

Right call, and the right way to do it in an 8-bit palette engine is **at load time, not per
frame**. Palette indices cannot be interpolated (index 143 is not "between" 142 and 144), so
scaling is nearest-neighbour resampling of CLX pixel runs - cheap to do once when the sprite sheet
loads, wasteful to redo every frame on the CPU renderer.

Plan: a CLX->CLX resampler (integer-friendly factors: 0.5x, 0.75x, 1.25x, 1.5x, 2x) producing real
scaled sprite sheets, so the renderer, hit-testing, and clipping see honest width/height values and
NOTHING downstream changes. Wire a per-monster scale factor into the monster data, and scale the
collision/selection metadata alongside.

What it buys immediately, with zero new art:
- **Giant and runt monster variants** - a "Colossal" lesser-unique affix (1.4x, more HP), swarm
  runts (0.6x, fast, fragile).
- **Boss silhouettes** - a 1.75x Butcher-class version of any monster reads as a boss from across
  the screen.
- **Zone rosters from one bestiary** - the same fallen at three scales, three tints, is nine
  visually distinct enemies.
- Later: prop/tile scale variants for zone dressing.

---

## Part II - The Megaplan

Ordering rule: systems before content, tools before art batches, and ONE batched save-format
break early so features can land without breaking saves twice.

### Phase 0 - Foundations (expanded 2026-08-16: "what more will help us later?")

Sized by the question each later phase will ask of it. Nothing here is visible to the player;
everything here is visible to every phase.

**0.1 The save break that ends save breaks.** The queued Charge-SpellID break, upgraded from
"batch the known fields" to **self-describing chunked extensions** (tag + length + payload) on
both PlayerPack and the item extension records. A reader skips unknown chunks; a writer appends
new ones freely. Sockets, skill points, charms, hireling state, zone progress - none of them
needs a break ever again, because the format stops being a fixed layout. The known fields
(sockets, skill points, charm flags, monster scale) become the first chunks. This is the single
highest-leverage item in the whole megaplan.

**0.2 Zone-ready world layout, in the same break.** The things Phase 4 cannot retrofit cheaply:
- Level-state slots sized by a COUNT FIELD rather than the hardcoded 25, so a new zone is a data
  change, not a format change.
- Waypoint mask widened once more, generously (64 bits - it was 16, then 24; stop re-widening).
- Entity caps audited and raised in the same pass (MaxMonsters/items/missiles per level are
  serialized arrays - swarm-runt packs and dense zones in Phases 3-5 will hit today's caps, and
  raising caps IS a save change).
- **A reserved companion entity slot** (Phase 6). The golem already proves the pattern: a
  player-owned unit living in a reserved monster slot. Reserving a second such slot now - plus a
  PlayerPack chunk for "companion identity + its packed equipment items" - means hirelings arrive
  later without touching the format. The chunked format (0.1) makes the payload free to define
  later; the SLOT in the entity arrays is the part to claim while the arrays are already open.

**0.3 Named RNG streams.** Loot rolls and cosmetic rolls drawn from separate named streams. Two
of our worst historical bugs (Thunderous shifting SpawnLoot's item stream; the drop-pool
recreation break) are this exact class. Sockets, gems, gambling and crafting each add new rolls
in Phase 1 - separate the streams BEFORE four features start interleaving them.

**0.4 The bonus-source aggregation refactor.** CalcPlrItemVals is a monolith that today sums
equipment. Phase 1 wants sockets, gems, runewords and charms feeding it; Phase 2 adds auras and
skill effects; Phase 6 adds a hireling. Refactor once now into "a list of bonus providers folded
into a stat sheet", with the current behaviour pinned by tests, and every later feature becomes a
provider that registers itself instead of another surgery on the monolith.

Two Phase 6 requirements shape this refactor NOW, at zero extra cost, or reshape it later at
full cost (user prompt, 2026-08-16: "maybe the phase 6 items?"):
- **The stat sheet must be per-ENTITY, not per-player.** A hireling is a second sheet fed by its
  own providers (its equipment). If the refactor bakes in "the player" as the only owner, Phase 6
  re-does it; if it takes an owner parameter from day one, a hireling is just a second caller.
- **Providers must support CONDITIONAL bonuses** - "IF three pieces of this set are worn, THEN
  +X". That is the entire set-bonus system (the one ColorOracoolGreen has been waiting for) as a
  provider flavour rather than a new mechanism. Designing the provider interface with a condition
  hook costs a function pointer today and a rewrite later.

**0.5 Zone registry - content as data.** One structure defines a zone: tileset, palette,
generator, roster (with per-entry TRN/scale/affix-pool), music, entrance, waypoint, quest hooks.
Existing levels migrate into the registry first (proving it changes nothing), and Phase 4's
"a zone a week" cadence becomes filling in a row instead of touching fifteen files. Phase 3's
variants (tint + scale + immunities per difficulty) live in the roster entries, so the bestiary
multiplier is data entry too.

**0.6 Sprite scaler** (unchanged): CLX->CLX nearest-neighbour resampler + per-monster scale
factor + scaled collision metadata, so everything downstream sees honest sprites.

**0.7 Tileset pipeline v1** (unchanged): quantize -> cut -> .MIN/.TIL/.SOL/.AMP authoring tool,
proven against a recolor zone before any AI art is commissioned.

**0.8 The iteration loop, attacked directly.** Our slowest cycle is "build, user launches, user
reports". Three tools shrink it:
- **Headless level render**: a debug command that generates a dungeon from a seed and writes the
  composed level to a PNG - so I can SEE a new zone's tiles, seams and lighting without anyone
  launching the game. (HeadlessMode and the timedemo harness already prove the engine runs
  windowless; this adds a renderer dump.)
- **Asset hot-reload**: a debug key that re-quantizes and reloads PNG-sourced assets in a running
  game, turning art iteration from restart-per-tweak into seconds.
- **Debug commands for the new systems** as they land: zone-jump, givesockets, giverune, setscale -
  the give*set commands proved how much these pay off.

**0.9 Balance telemetry.** The Event Log grows a structured session log (CSV): kills per zone,
time-to-kill per monster type, damage taken, deaths, drops by tier, mana starvation moments.
I cannot play the game - but I can read the data a play session leaves behind. Every playtest the
user runs becomes tuning input for Phases 3-5 instead of an anecdote. (Local file, no network,
off by default outside our own builds.)

**0.10 The regression floor.** Golden-save tests for the new format (serialize a maxed character,
pin the bytes; run every migration against archived old saves), a formalized migration-step
framework (MigrateHiddenBeltSlots proved the ad-hoc pattern; make it a numbered list the loader
walks), and statistical drop-table tests so Phase 5's tuning cannot silently break Phase 1's
drop rates.

*Suggested order inside the phase: 0.3 and 0.4 first (pure code, testable), then 0.1/0.2 (the
break, with 0.10's tests around it), then 0.5-0.7 (tools), 0.8-0.9 riding along wherever
convenient.*

### Phase 1 - The item endgame (D2's crown, improved)
1. Sockets on weapons/armor/helms/shields; gems with per-slot effects; runes.
2. **Runewords** - and improve on D2: the recipe book is discoverable in-game (drops as readable
   pages) instead of wiki-mandatory.
3. Charms (with D2's lesson learned: cap charm inventory space so the backpack stays a backpack -
   a dedicated charm pouch, not inventory pollution).
4. Ethereal items, Magic/Gold Find affixes, gambling at Wirt.
5. Crafting window (cube-style recipes, no cube item needed - it is a UI, not an object).

### Phase 2 - Character depth
1. Skill points on level-up; per-skill invested levels feeding the existing ladders (Zeal's frame
   ladder becomes point-driven rather than purely character-level-driven).
2. Skill tree UI per class (the Abilities window already has the sheet structure).
3. Respec (gold cost, at Adria).
4. **Paladin auras implemented** (design doc already exists: [[Paladin Auras - Gameplay Implementation Plan]]).
5. Run toggle (walk-speed scaling, Charge's own proven mechanism; optional stamina).

### Phase 3 - The bestiary multiplier (content from what we own)
1. TRN recolor variants wired into zone rosters.
2. Scale variants (giant/runt) + the Colossal lesser-unique affix.
3. Per-difficulty immunities/resistances (data exists, make it difficulty-aware).
4. Champion PACKS polish: aura-carrying lesser uniques (Fanaticism pack, Might pack - D2's scariest
   idea, cheap here because the affix system exists).

### Phase 4 - New dungeon zones (the "acts" answer)
1. **Zone 1 as pipeline proof: a recolor zone.** Exactly Hellfire's own trick (Crypt = recolored
   Cathedral): new palette + retinted tileset + new roster (Phase 3 variants) + new waypoints +
   entrance. Validates every tool with zero AI-art risk.
2. **Zone 2: first AI-generated tileset** (user art through pipeline v1), reusing an existing
   generator with new tile mappings.
3. **Zone quests**: each zone gets a quest chain in the D1 style (a voice, a horror, a reward) -
   the quest machinery is vanilla and well-understood.
4. Repeat as appetite dictates. Each zone after the second is: art batch + roster + quest + week.

### Phase 5 - The endgame loop (why players stay)
1. Treasure-class-style drop tables (zone- and boss-specific drops worth farming).
2. Endgame bosses: scaled + tinted + affix-loaded versions of existing monsters guarding the best
   tables (Uber-style, built entirely from Phase 0-3 machinery).
3. Difficulty re-runs that mean something: new immunities, new lesser-affix pools, drop tiers.
4. Stretch: seasonal/challenge characters (SP-friendly: a checkbox at creation, a ladder file).

### Phase 6 - Optional depth (only if the appetite holds)
1. Hirelings (golem framework -> persistent companion; equipment is the expensive half).
2. A second charm/jewel tier, set-bonus system (ColorOracoolGreen is already reserved for it).

---

## Part III - Risks and rules

- **The palette is the aesthetic.** Every AI asset gets quantized into it; that constraint is what
  will make generated art look like Diablo instead of like an AI pasted onto Diablo. The pipeline
  enforces it mechanically - no exceptions, ever.
- **One save break, not five.** Phase 0.1 is the only planned break. Anything that misses it waits
  for the next planned one.
- **Recolor before generate.** Every new-art system proves itself on recolored/scaled existing
  assets first. Failing cheap beats failing expensive.
- **Tristram is sacred.** No feature may require a second town. Zone entrances integrate into the
  existing world.
- **The regression suite grows with every phase** - Phase 0's save break gets golden-file tests
  before any Phase 1 feature lands on top of it.
- **Feel is decided in play, not in code review.** Every phase ends with a playable build and the
  user's verdict; Zeal took three iterations and that was the system working, not failing.
