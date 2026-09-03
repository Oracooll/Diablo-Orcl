# Staff charge badges, and a wrapped enum (v1.9.197-198)

## The badges (v1.9.198)

User request: "we need to put badges with remaining charges of staff spells. lets put it on bottom
left corner. 0-9 charges to use red font. 10 and over - regular white font. badges travel with
skill/spell icon everywhere they appear." And, mid-implementation: "when charges reach 0, put the
red X over the icon, like we do with broken items."

**Bottom-left was the only free corner.** The two top corners are the left- and right-button hotkey
badges, and bottom-right is the rank. Nothing had claimed bottom-left since the rank moved out of
bottom-centre on 2026-09-02.

**One function, four call sites**, because "travel with the icon everywhere" is a rule about every
draw site at once and a rule like that survives only if there is one thing to call:

| Site | Where |
|---|---|
| the LMB/RMB wells | a readied charge cast |
| the skill picker | every cell in the new Staff spells section |
| the speedbook | every `SpellType::Charges` entry |
| the Abilities window | the spell row for a staff-held spell |

`StaffChargesFor(player, spell)` is the single source for the number: the staff is always the
left-hand slot, which is where `_pISpells` is built from. It answers -1 when the equipped staff
does not cast that spell, and the badge then draws nothing.

**Red below ten, white at ten and above**, and drawn at zero too — a spent staff still says so,
which is when the number matters most. `badge.h` said "THE COLOUR IS ALWAYS WHITE"; that comment
now records the one exception and why it is a warning rather than a category.

**Zero charges also gets the red X.** The stroke was `DrawUnbuiltCross`, private to the Abilities
window since 2026-08-18. Two callers in two files is when a private helper becomes shared, so it
moved to `oracool::DrawRedCross`; the Abilities window keeps its own name as a one-line wrapper.
The cross draws before the badge so the number stays legible on top of it.

## The wrapped enum (v1.9.197)

Found by reading the build's **warnings** while doing the above:

```
spelldat.h(511): warning C4340: 'Warcry': value wrapped from positive to negative value
spelldat.h(511): warning C4369: enumerator value '128' cannot be represented as 'signed char'
```

`MissileID` was `int8_t`. Round 6's `Warcry` was the 129th entry, so 128 wrapped to -128, and
`GetMissileData` indexes `MissilesData` positionally by that value. Every cry in the game — 21
skills across Rounds 6, 8 and 9 — read the missile table 128 rows before its first row. Undefined
behaviour on every cast.

The audit three commits earlier missed it because it grepped for "error" and never read a warning.

`MissileID` is `int16_t` now; the save already used int32_t for the field. Two pins were added:
`MissilesData` gets the size assert `SpellsData` got in the audit, and `SpellID` gains a second
assert at 127 — its existing one guards the 128-bit spell mask, which 128 passes, while the enum's
own `int8_t` storage would wrap at exactly that value.

624/625, the standing baseline.

## To look at in play

Equip a staff and check the number in the bottom-left of its icon in all four places. Spend it
below ten and the number turns red. Spend it to zero and the icon is struck out. And cast any
Barbarian cry — that path was reading garbage until v1.9.197.
