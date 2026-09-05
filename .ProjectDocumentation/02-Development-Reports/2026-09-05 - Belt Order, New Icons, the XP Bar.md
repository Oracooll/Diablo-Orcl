# Belt Order, New Icons, the XP Bar (v1.9.219)

**Date:** 2026-09-05 · **Request** (with two 1280x720 screenshots): the orb cropping only at 4:3; burger to slot 6, portal to slot 5, potions 1-4; sweep the drop zone for new burger/portal icons and use them; XP bar closer to the belt; XP counter hidden, shown on hover, monster pool on click.

## The clip only at 4:3

`scrollrt.cpp`: the orb clip against `SidePanelContentBottom` runs only when `gnScreenWidth * 3 <= gnScreenHeight * 4 + 8`. At 960x720 the 340px panels reach the orbs; at 16:9 they do not, and the clip was costing the sphere its crown for nothing.

## Belt order

`Player::SpdList` numbering (0 menu, 1-4 items, 5 portal) is saved data and the BeltItem keys, so it stays. What moved is the CELL each slot is drawn in: `BeltCellOfSlot` in `hud_layout.h` (1-4 → 0-3, 5 → 4, 0 → 5), applied inside `GetBeltSlotRect`, which every draw call and hit-test already goes through. `GetBeltCellRect` is the raw row geometry for things that span it (the XP bar). Test pins the order.

## The icons

The sweep found `diablo-bottom-hud-2400x384-v1/icons/` back in the drop-zone root - GPT's burger and portal sets (masters, 112px sources, 28px runtime states, 84x28 strips) plus a minimal burger variant. Filed under the pack as `icons/`; the two runtime strips ship unchanged as `ui\burger_menu_button.png` and `ui\town_portal_icon.png`. Both icons are 28x28 with three states now; the burger's blink alternates hover and click instead of unlit and lit. An 88x88 charcoal tile that also arrived is filed under `textures/`, unused.

## The XP bar and counter

The cutter emits `BeltBarTop` (the painted belt bar's top edge, plate-local 67) and `GetBeltRunTop()` returns it on screen (the cells' top on the plateless row). `GetXPBarRect` hangs 1px above that line; it no longer derives from the counter. The counter is drawn only while the cursor is over the bar (padded 2px) or the bar is held, directly above the bar; holding shows the remaining-monster pool as the counter's own click used to. `IsPointOverXpCounter` and the click test both use the bar now.

## Addendum, v1.9.220 - the bar halved, the hover text gone, a shadow pilot

"make xp bar half as thick. remove the bottom half to increase gap between belt. remove the 10% indents. Remove the pop-up message when hovering." And: "can you cast shadows beneath UI assets ... cast same angle shadow as texts in hero stats window to all skills/spells/auras slots in abilities windows as a pilot test."

- `xpbar.cpp`: `BarHeight` 8 → 4, `GapAboveBelt` 1 → 5 so the top edge stays put and the freed rows become gap. The ten notches and their constants are gone.
- `control.cpp`: the "Experience Meter / Click for more." info text no longer sets on hover; the hover still ends the search there.
- Shadows: `oracool::DrawDropShadow(out, rect)` in ornate_border - the rect's footprint at (-2, +2), the character sheet's own text-shadow offset (vanilla's, in text_render.cpp), darkened by two half-transparent passes, drawn before the slot so only the strip down the left and along the bottom shows. Called at the three slot sites in `spell_book.cpp`: the class sheet rows, the tree cells, the passive slot band. Yes, the same can go under the inventory and stash cells - `DrawGridBezel` sites - once the pilot is judged.

**v1.9.221** - "i dont really see the shadow ... double check if it renders." It rendered and was invisible: `DrawGridBezel` paints its frame 6px OUTSIDE the rect it is given, so a 2px shadow of the rect sat entirely under the bezel. `DrawDropShadow` now takes the bezel width, shadows the slot's full footprint, and the offset is the user's six pixels.

**v1.9.222-223** - the shadow: one half-transparent pass instead of two ("reduce the shadow by half" - read as darkness), then the offset to 3px ("i really meant making it 3px. but keep the current transparent pass"). Now: full bezel footprint, (-3, +3), one pass.

## Addendum, v1.9.224 - shadows on the grids, gaps in the Abilities window

"apply the same shadow to inventory and stash slots. also - introduce 6px gaps between spells in spells windows / between skill slots in passive skills / between all skills in passive skills."

- Shadow under the inventory's equipment slots, the inventory grid and the stash grid, at the bezel sites (`inv.cpp`, `stash.cpp`).
- Spells list: `SpellRowHeight` 64 → 74 (icon + 12 of bezel + 6 of air).
- Tree pages: `TreeRowGap` 6 → 12 - the bezel reaches six below the icon under the counter bar, so six of air between the bar and the next frame is twelve; `TreeBarHeight` 16 → 14 pays for it, so six tiers still fit the 523px list unscrolled (6 + 6 × 86 = 522).
- Passive page: `PassiveRowGap` 4 → 18 (pitch 74). Seven tiers are 606px, so the page scrolls now; the 2026-08-25 "fits unscrolled" assert was dropped. The slot band scrolls with it and is off the top only for the last tier.
- NOT done: 6px gaps between the four passive SLOTS. Four 56px slots with their 6px frames and 6px gaps are 290px; the painted interior between the bezel ornaments is 246 (`AbilitiesInteriorLeft/Right`), which is why the band's pitch is 63 and the frames overlap by five. Either the band overhangs the frame by 22px a side - the thing the user asked to avoid on 2026-08-17 - or the band needs a smaller slot with its own bezel cut. Left for the user to choose.

**v1.9.225** - "make burger default icon and portal default icon brighter... also scale them 10%." `tools/CutBeltButtonIcons.ps1` cuts both strips from GPT's 112px sources at 31px (28 x 1.1), three states each; default and hover take a gamma of 0.7 (lifting default alone put it above hover and inverted the cue), click as delivered. Sizes in hud_art.cpp follow.

## Addendum, v1.9.226 - six pixels of air between the inventory's slots

"introduce a mandatory minimum gap of 6px between items slots in inventory window. that way shadows will be visible. rearrange as necessary."

`inventory_layout.h`: `SlotAir` 6 and `SlotGap` 18 (air plus a frame on each side), and every equipment row is now derived from the one above it by that gap: helm 30, chest 104, belt/rings 206, legs 252, boots 326 in the centre column; shoulders 48, amulet 60, gloves/bracers 122, weapon/shield 262 at the sides. Two static_asserts pin the air for every vertically adjacent pair in each column and for the three column gaps. The centre column uses the whole height above the tab row (boots' frame ends at 388 = TabRowY); the columns already had 9-30px of air. `InvRect` is built from these rects, so hit-testing moved with the art.

## Addendum, v1.9.227 - description shadows, the passive band spread

"introduce text shadows under description of spells in spells abilities window for easier reading. also - spread passive slots 1-4 wider to introduce a mandatory 6px gap between their vertical borders."

- Spells sheet: the detail and damage lines carry `UiFlags::Shadowed` - the character sheet's vanilla two-left-two-down text shadow. The name line is left clean.
- Passive band: pitch 74 (56 + two frames + 6 of air), centred on the panel's 340 rather than the 246 interior, so the outer frames sit 21px onto the bezel ornament either side - the user's choice over the interior rule of 2026-08-17, and the one place in the window that crosses it. The static_assert now checks the panel margin rather than the interior.

## Addendum, v1.9.228 - shadowed hover names, the passive hint above the slots

"apply the same text shadow to abilities tree skill names. also - move Click to select slot text above passive skill slot 1-4. at least 6 px above the slots. apply text shadow." Then: "there is enough room under the title, so dont move down."

- Tree pages draw no names on the cells; the skill's name is the hover panel's title. `DrawHoverPanel` (ornate_border.cpp, one caller - the Abilities window) now shadows its title and text.
- The passive hint is drawn on the PANEL by `DrawPassiveHintAboveList`, in the band between the title and the arch's foot, ending six pixels above the slots' frames, shadowed. The band and grid stayed where they were - the first cut pushed them down to make room and the user said not to.

**v1.9.229** - "add text shadows also under spell names": the Spells sheet's name line takes the same shadow as its description lines.

## Addendum, v1.9.230 - the plate becomes a ring

"remove legacy backing from legacy spell icons and skills in the abilities windows. Replace it with 3px outline of same color."

- Tree cells and the four passive slots: `DrawClassTreeIconOutlined` draws the class strip's icon and a 3px ring just inside the cell (`DrawSkillTintOutline`) in one index off the tint's own ramp - green PAL8_GREEN+2, red PAL16_RED+4, grey PAL16_GRAY+8, pink PAL16_BEIGE+4 - instead of the tinted vanilla plate under it. An empty passive slot is the ring alone.
- NOT done for the legacy spell icons: their "backing" is the vanilla 37x38 icon art's own bevelled square, which the tint recolours through a translation table. There is nothing separate to remove; taking it out means keying the plate ramps out of the icon, and the symbols use the same gold ramps the plate does. Left as is, with the tint on the plate, on both the Spells sheet and the tree pages' legacy rows.
- The HUD's wells keep their plates.

**v1.9.231** - "remove backing and outlines altogether. leave only icons themselves. void of any backing." The ring lasted one build: `DrawClassTreeIconOutlined` now draws the strip icon and nothing else; `DrawSkillTintOutline` stays as a function for the one-line return. The tint is still computed by the callers and ignored. Legacy spell icons: unchanged, for the reason in v1.9.230 - the square is the icon art.

## Addendum, v1.9.232 - the masked cut

"remove backing from legacy spell icons too - do the masked cut."

Not an asset: the vanilla icon sheet is original game art and a re-cut of it cannot ship. `DrawSmallSpellSymbolFittedTo` (spell_icons.cpp) does the cut at draw time instead - the icon and the sheet's own blank plate (SpellID::Null) are drawn through the same translation table into scratch, and only the pixels where the icon differs from the blank are blitted. The plate, identical in both, stays out; the tint still colours the symbol. Cost: a symbol pixel that lands on the plate's own index at that spot (black shading over the black interior) is taken for plate. Used by the Spells sheet and by the tree pages' legacy rows (`DrawLegacySpellIconInCell`). The HUD's wells still draw the full icon.

**v1.9.233** - "apply the masked cut to the lmb/rmb wells too." The five well draws of a legacy icon go through `DrawSmallSpellSymbolFittedTo`: the legacy branch of `TryDrawSkillSpellIcon`, both readied-spell draws in attack_skills.cpp (including the STAFF case - the orange of 2026-09-03 is on the glyph now, not a plate) and both in spell_list.cpp's DrawSpell. The skill picker's cells still draw the full icon with its coloured backing, which the user asked for by name on 2026-09-03.

## Addendum, v1.9.234 - the Abilities window on the 56px sheet

"switch the abilities window to the 56px sheet." The Spells sheet had drawn the 37x38 icon stretched to ~147% since its cells grew to 56 on 2026-08-19 - the growth landed on the draw call that was there, and the sheet was never revisited. `DrawLargeSpellSymbolCentredIn` (spell_icons.cpp) draws the large sheet's frame centred in the cell with no resampling, masked against that sheet's own blank frame 26 exactly as the small cut is. Used by the Spells sheet and the tree pages' legacy rows (`DrawLegacySpellIconInCell`). The wells stay on the small sheet: 46px is nearer 37 than 56. `spelicon.cel` was decoded to `99-original-game-art/png/spelicon/` for reference the same night (51 frames; frame 26 is the blank plate).

**v1.9.235** - "now remove the masking. let the vanilla backing stay." Both masked-cut functions are gone; `DrawLargeSpellIconCentredIn` draws the 56px frame whole in the Abilities window, and the wells are back on `DrawSmallSpellIconFittedTo`. The 56px sheet stays in the Abilities window. The cut is in the history (v1.9.232-234).

**v1.9.236** - "the hover rectangle around a spell in abilities window - make it 3px thick. and let it cast shadow 2px." `DrawHoverOutlineHeavy` (ornate_border): the hover gold at weight 3 over the same ring shifted (-2, +2) in the grey ramp's near-black. Used at the Abilities window's three hover sites - the passive slots, the tree cells and the Spells sheet rows. The 1px `DrawHoverOutline` stays for everything else (and for the "live" rings on a slotted passive and a burning aura).

**v1.9.237** - "the shadow should be black/semi-transparent, like the skill slots, and the rectangle left border to be 3px to the left from the cast shadow from the spell slots." `DrawHoverOutlineHeavy` now sits 12px outside the slot rect (6 bezel + 3 shadow + 3 air, symmetric) and its shadow is four non-overlapping half-transparent bands at (-2, +2). Ring is 80px for a 56px slot, so on the 74px-pitch passive band and Spells rows it reaches 3px onto the neighbours' frames.

**v1.9.238** - "apply same hover rectangle theme in Waypoints list." `DrawHoverOutlineHeavy` took a `clearance` parameter (the slot default is the 12 above); the waypoint list's rows pass 0, since a row there wears no bezel and casts no shadow, and get the 3px gold with the half-transparent shadow on the row's own rect.

**v1.9.239** - "it is too high. overlaps with above and below spells... no overlapping with adjacent spells." Two causes. The Spells sheet passed the whole 74px row as the rect, so the ring stood twelve pixels onto both neighbours; it now passes the row's ICON height with the row's width. And vertically twelve is more than the six of air between frames allows, so the ring clears nine above and below (bezel plus three) - three of ring and two of shadow fit inside the six - while keeping twelve left and right, where the slot's shadow is cast. `DrawHoverOutlineHeavy(out, rect, clearanceX, clearanceY)`; the waypoint list passes 0, 0.
