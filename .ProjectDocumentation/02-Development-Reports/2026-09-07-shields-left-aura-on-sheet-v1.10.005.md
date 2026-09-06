# Monster shields left of the frame; a burning aura names the sheet's right button (v1.10.005)

**Date:** 2026-09-07
**Requests:** "move the shields left of frame" / "when assigning vigor to rmb, hero stats right button says ATTACK and shows DMG. this is incorrect. fix and audit all non-attack skills if they have the same mistake."

## The shields

A render of the monster health bar (built from monhealthbar.cpp's geometry with the real CLX frames) showed the resistance shields anchored at frame height minus 6, hanging from y=15 to y=37 across the Class and Hit Points readout lines that arrived on 2026-09-05. They now run leftward from a 5px gap at the frame's left edge, vertically centred on the frame, still magic / fire / lightning left to right (laid from the right, lightning first).

## The right button

An aura and the readied right-button skill are one slot: lighting Vigor empties `_pRSpell` (ClearClassAuraForRightButton), so the sheet's four readers of that field - name, colour, amount, label - all saw the basic attack: "Right button: Attack" over the weapon's damage, while the well showed Vigor. The HUD's hover had already been fixed for this on 2026-08-19 (control.cpp asks GetActiveClassAura first). charpanel.cpp now does the same through one `AuraOnButton(leftButton)` helper: "Right button: Vigor" in blue, label "Aura", value "On". The label passed the column-width assert.

## The audit

Which non-attack skills could show the same mistake? The sheet's name reads a SpellID; every tree skill that is Active carries one and is named correctly, and a spell without a damage formula shows a dash. The weapon-damage branch fires only for a Paladin skill with rangeTiles 1, and all seven Paladin skills are attacks. The only state in the game that empties a button without a SpellID is the aura (the one `_pRSpell = Invalid` in class_tree.cpp), so the Paladin's twelve auras were the whole fault and are all covered by the one helper. No other class has a toggle that lives on a button.

## Tests

New: a burning Vigor names the right button, is blue, quotes no weapon damage, leaves the left button alone, and the button is the attack again once the aura is out. Suite 687/687.
