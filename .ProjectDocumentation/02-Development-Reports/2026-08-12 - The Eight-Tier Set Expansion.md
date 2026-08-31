---
title: 2026-08-12 - The Eight-Tier Set Expansion
date: 2026-08-12
tags: [dev-report]
summary: "Ship every item you harvest... name them, categorize them, give them adequate stats and make them part of the game." Eight green-screen composite sheets, one per tier (Iron through Diamond), each holding all nine worn/armor/shield pieces in a 3x3 grid, turned into 73 new items - the largest single content addition this project has shipped. Two real bugs in the green-key pipeline surfaced and got fixed along the way, plus a cmd.exe argument-list limit neither of the previous, smaller batches had ever reached.
---

# The Eight-Tier Set Expansion

The instruction was unambiguous: "ship every item you harvest from these files... name them, categorize them, whatever you decide. give them adequate stats and make them part of the game." Also folded in: the leather-tier Armor and Shield held back from the previous batch specifically because no item existed yet to attach them to.

## What arrived

Eight composite sheets, one per tier - "Iron Set v1", "Steel Set v1", "Steel Set v2", "Gold Set v1", "Obsidian Set v1", "Obsidian Set v2", "Bone Set v1", "Diamond Set v1" - each a single 1402x1122 green-screen image holding all nine pieces (gloves, shoulders, bracers, belt, legs, boots, armor, shield, helm) in a 3x3 grid. Sorted and renamed to `item-set-<tier>-vN.png` per the vault's own versioning convention and filed alongside the existing single-item sources.

## Cutting a grid instead of a single render

Every source so far this session had been one item per file. This batch needed per-cell crop rectangles instead. Measured one sheet's actual layout directly rather than assuming a clean three-way split: column bands separated cleanly by wide gutters, but row bands measured across the *full width* merged into one blob, because different columns' content happened to reach different heights within their own cells. Isolating just column 0 (gloves) resolved it - a 42px-tall title-text band, a clean ~30px gutter, then the glove art starting at y=92. The other eight cells use plain uniform thirds of the canvas (1402/3, 1122/3); `ContentBoxByGreenKey` tightens the rest, and the gutters measured wide enough (100px+) that a few pixels of slop in the cell boundaries risks nothing.

## Two real bugs in the green-key pipeline

The first test cut (Iron tier, all nine cells) came back clean. Cutting all eight tiers did not.

**Bug one:** Obsidian and Infernal's content boxes matched their *full* search rectangles on every single cell - the tightening had stopped working entirely, uniformly, for exactly two of eight tiers. Traced to a design inconsistency introduced when this pipeline was built the previous session: `ContentBoxByGreenKey` decided "is this background" using a hard cutoff at `GreenKeyFullThreshold` (80), but `ExtractWithGreenKey`'s actual alpha ramp already treats anything below that as mostly-to-fully transparent. The two composites' background happened to sit at excess ~78-85 (Obsidian) and ~60-69 (Infernal) - not the ~190-245 the ramp was originally tuned against - which put their real background pixels *just* under the box's cutoff on every sample, so the box never tightened at all. Fixed by making the content-box test use the ramp's actual midpoint instead of a second, independently-set number.

**Bug two, found on the next full re-cut:** `infernal_belt` and several other Infernal cells rendered as a visible dark checkerboard instead of clean transparency. The midpoint fix corrected *bounding-box* detection but left the alpha ramp itself still built for the original renders' 190-245 background. A histogram of Infernal's actual background (sampled broadly across the belt cell, avoiding edges) showed two dense clusters - content around -20..10, background around 50-70 - with almost nothing between. Background pixels in that gap were getting substantial partial alpha, which `PostProcess`'s `KeepAlpha` cutoff then forced fully opaque, dithering a color blended from green-tinted background into visible noise. Recalibrated the ramp to 40/10 - inside the measured valley for every distribution seen so far, with the original individual-render batch (190-245) left untouched with enormous margin to spare. Re-cut and re-inspected: checkerboard gone, `infernal_belt` reads as the intended skull-and-chain design.

Both fixes are general (`ItemIconCel.cs`'s shared constants), not per-image special cases, so any future single-subject or grid-composite green-screen source benefits without rediscovering either bug.

## A third bug, in the build script itself

`build_item_icons.cmd` had grown to 80 `^`-continued command-line arguments. It failed partway through with `Bad spec: ^` - not a parser bug, a cmd.exe limit on how long a single caret-continued logical line can reliably get, that the previous 7-9-item version of this script had never come close to. Added `@specfile` support to `ItemIconCel.exe` (one spec per line, read from a file) and rebuilt the script around `echo`-ing each spec into a temp file rather than chaining them as arguments - sidesteps the limit entirely rather than working around a ceiling that would only need rediscovering at the next batch.

## Naming, tiering, and stats

Explicitly delegated ("whatever you decide"), so stated plainly rather than hidden in the numbers: tier order (Iron < Steel < Crusader < Bone < Royal < Obsidian < Infernal < Diamond) is a visual-escalation judgment call made while looking at the eight sheets, not a documented lore ranking - Bone in particular could reasonably sit elsewhere. Iron's own helm cell was re-sourced into the *existing* Iron Helm item rather than creating a duplicate - that item already occupies this tier's helm slot, so only its icon changed, for visual consistency with the rest of the Iron set.

Stats are formulaic, not hand-tuned per item: each of the nine slots has an Iron-tier (multiplier 1.0) base anchored to the existing leather items (six worn slots) or to vanilla's own comparable items (Helm, and the two new Armor/Shield items), then durability/AC/value scale by a shared per-tier multiplier (1.00/1.30/1.65/2.10/2.70/3.50/4.60/6.00 across the eight tiers), value additionally by `multiplier^1.4` to mirror vanilla's own steeper value curve, and minMLvl/minStr scale additively per tier step. `IDROP_NEVER` throughout, same reason as every other Oracool item this session: joining the loot tables perturbs `pack_test`'s RNG-seeded golden items, a deliberate separate follow-up nobody has asked for yet.

A caught transcription error, worth recording: the first pass at writing 71 rows into `itemdat.cpp` was hand-typed from partially-remembered generator output rather than the file itself, and several value fields were wrong (Crusader Gloves: 182 typed vs. 192 actually computed) - exactly the class of error a generator script exists to prevent, reintroduced by not reading its output back before using it. Caught by re-reading the actual generated file and diffing against what had been typed, before it reached a build; the whole 71-row block was then spliced in verbatim from the generator's output instead.

## Engineering scope

Seven items already existed at their slots (the six worn types, Iron Helm) and needed only their `oracool_items.cel` source swapped. Two (Leather Armor, Leather Shield) were cut in the previous session's batch but held back with no item to attach to. The remaining 71 are genuinely new: `IDI_ORACOOL_*`/`ICURS_ORACOOL_*` enum ranges extended by 73 entries each, 73 new `AllItemsList` rows, `InvItemWidth3`/`InvItemHeight3` grown to 80 frames, `IsOracoolItemIdx`'s range (shared by the save-remap guard and the `drop`-command fallback) extended to cover all of it. `PackTest`'s Diablo round-trip test was changed to iterate `IsOracoolItemIdx`'s own range directly instead of a hand-maintained array of names - the same reasoning as the generator-output lesson above: a name list only 80 items long is already too long to safely hand-maintain.

Two new `ItemType`s needed no new engine support - `ItemType::MediumArmor` and `ItemType::Shield` already exist and are already used by vanilla rows at `ILOC_ARMOR` and `ILOC_ONEHAND` respectively, so Armor and Shield needed no new equip-location plumbing at all, matching the exact reasoning that made Iron Helm lightweight two units of work ago.

## Verification

Every one of the 80 shipped icons checked with the same border-flood-fill puncture detector built earlier this session: zero missing, one flagged (`infernal_boots`, 102px - two orders of magnitude below the belt's confirmed-real 212px gap, left alone). Closed-loop MPQ check repeated: extracted the packed CEL back out of the rebuilt `oracool.mpq` and hash-matched it against source; all three asset channels agree. Debug build clean at `ORACOOL_VERSION` **1.1.27**, confirmed embedded in the exe. Full suite: **349/351**, the same two pre-existing failures as every build this session - including, explicitly, `PackTest.PackItem_diablo_roundtrip_preserves_oracool_worn_items` run in isolation against all 80 items.

## Follow-up (v1.1.28): the give*set commands couldn't actually reach any of this

Caught by the user in the first live session, from a single screenshot: "all assets seem to be of the same type. are you sure the give*set debug command can spawn other items?" Correct on both counts. `FirstBaseItemForEquipLocation` returns the *first* item in `AllItemsList` matching each slot - which is always the leather-tier item for the worn slots and vanilla Cap for the helm - and all 71 tiered items sit later in the table, `IDROP_NEVER`, structurally unreachable from those commands. The only path to a Steel/Bone/Diamond piece was `drop <name>`, one item at a time, 73 times. The expansion shipped with no usable way to spawn itself for testing.

Fixed by giving all five `give*set` commands an optional material-tier argument (`givebset diamond`, `givemset steel`): a non-empty prefix switches `FirstBaseItemForEquipLocation` to case-insensitive item-name-prefix matching (ignoring the `IDROP_NEVER` skip entirely, since the selector is now the prefix itself), empty keeps the exact previous behavior. Slots with no item at that tier - amulet and rings for every material prefix, helm for "leather" - fall into the existing "N slot(s) have no base item yet" report rather than failing silently. Same 349/351 baseline.

## What this pass could not verify

Asked to spawn every item in a live session, look at them, and confirm before standing down. That is not something this tooling can do: there is no desktop screenshot or keyboard/mouse-input capability available here, only a browser pane, which does not apply to a native game window. Everything gets verified up to "the asset pipeline and the engine agree this is correct data" - palette-accurate icon previews, the automated save/pack round trip, a clean build - and stops there. The actual paperdoll/inventory rendering, the `drop <name>` debug-console spawn path, and how 73 new icons actually read at real screen scale all still need a live session with human eyes, which this pass could not provide.

## Related

- [[2026-08-12 - Green-Screen Chroma Key Replaces the Blanked Six]]
- [[2026-08-12 - The Iron Helm, a New Item at an Old Slot]]
- [[2026-08-12 - Enclosed Punctures, the Mirror Image of Floating Debris]]
