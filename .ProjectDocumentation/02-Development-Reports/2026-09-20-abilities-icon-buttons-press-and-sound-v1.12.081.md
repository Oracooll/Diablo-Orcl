---
date: 2026-09-20
version: 1.12.081
tags: [dev-report, abilities-window, ui, buttons, sound]
---

# Abilities window: the Act buttons' press and sound on the icon buttons (v1.12.081)

## The ask

> "i like these move and sound effects we just applied on the Act buttons. Apply them on all buttons in the abilities windows, regardless if clicking on a button actually produces any meaningful effect in-game." Then narrowed: only the buttons on the spell-plate backing (spelicon frame 26) - "those would be the skills/spells/auras buttons. dont apply on the navigation buttons yet."

## Scope

- **In:** the class tree cells (skills and auras), the four passive slots on the passive page, and the Spells sheet's row icons.
- **Out, for now:** the five tab plates (navigation - painted into the canvas, they keep their own press wash).

## What was built (`Source/panels/spell_book.cpp`)

- `IconButton { kind, id }` names a button: `TreeCell` (the ClassTreeSkill), `PassiveSlot` (the slot), `SpellRow` (the row index). `PressedIcon` is the one held down; `PressIconButton` sets it and plays the UI move sound (`titlemov.wav`).
- **The sink:** each draw site casts its drop shadow from the resting rect, then shifts the face by (-2, +2) through `PressSinkFor` while that button is pressed - the bezel, the icon, the cross, the badges and rings all ride with it; the shadow stays.
- **The hover sound:** `DrawHoverFeedback` records the button under the cursor in `FrameHoverIcon`; `DrawSpellBook` plays the move sound on the frame it changes to a button that was not under the cursor last frame.
- **Every click sounds**, whatever it then does - a refused invest, a locked slot, an unlearned spell row, the counter bar. `CheckSBook` calls `PressIconButton` as soon as it knows what was hit, before any refusal.
- **Duplicates removed:** the click sounds that sat on the individual outcomes in `CheckSBook` (move on an emptying or a refund, select on arming, slotting or readying) are gone, or a click would ring twice. A skill's own learn cue inside `InvestClassTreePoint` is not a click sound and stays. The keyboard F-key path keeps its sounds.
- **Release:** `ReleaseSpellBookButtons` clears the pressed icon; diablo.cpp now calls it from the RIGHT button's release too, since right-clicks press these buttons (refund, empty, ready-to-right).

## Build

Build 61, v1.12.081: clean, ctest 831/831.
