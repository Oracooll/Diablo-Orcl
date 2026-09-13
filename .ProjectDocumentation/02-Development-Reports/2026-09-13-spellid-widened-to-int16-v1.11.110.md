# SpellID widened to int16_t and SpellMask to four words, with no save byte moved

2026-09-13 — v1.11.110

## Why

114 of the 162 RfA-12 skills are actives or warcries, and every active in this engine is cast, readied,
priced and levelled through a `SpellID`. The enum was `int8_t` with ids 0-125 used, so it had two ids
left and needed 114. The spell-mask sets held 128 bits. This unit makes room and adds nothing: no spell,
no row, no behaviour.

## What changed

- **`enum class SpellID : int16_t`**, and the two forward declarations that must match it (`diablo.h`,
  `inv.h`) - the compiler refuses a mismatch outright (C3433), which is how the second and third were
  found.
- **`SpellMask` has four words.** `third` and `fourth` join `low` and `high`, with a four-word constructor
  and every operator widened. Nothing above `low` is persisted - the save still writes the low word
  alone, which is all a book spell needs, and everything higher is rebuilt from investment on load - so
  the save format is untouched.
- **`GetSpellBitmask`** reads the id as `int`, covers 256 ids, and stays total: anything outside 1-256 is
  an empty mask.
- **43 byte casts of a spell id** (`static_cast<int8_t>(spellID)` and friends) became `int16_t`, across
  `diablo.cpp`, `items.cpp`, `loadsave.cpp`, `objects.cpp`, `player.cpp`, `stores.cpp`, `hud_menu.cpp`
  and `spell_icons.cpp`. They were harmless below 128 and would have wrapped above it: a network
  parameter, a save field and six icon-table lookups among them. Casts of `spellType` and `spellFrom`,
  which are not spell ids, were left alone. A grep afterwards finds no byte cast of a spell id.
- **`PackReadiedSpell` / `UnpackReadiedSpell`** go through `int`. Through `int8_t`, id 128 would have
  packed as 129 and unpacked as -127. A readied spell and the F-key bindings are still one byte, id + 1,
  so every existing save decodes exactly as before.
- **The network range check** in `InitNewSpell` compares as `int16_t`.
- **The asserts** moved with the ceilings: `LAST <= 256` for the mask, and `LAST <= 254` for the byte
  `PackReadiedSpell` writes - the storage ceiling is that byte's now, not the enum's.

## Tests

The spell-mask test that pinned `MAX_SPELLS - 1` as a high-word bit now pins the word boundaries instead:
id 128 is the last bit of the high word, 129 the first of the third, 256 the last the mask holds, and 257
is empty. The full suite passes, and `Writehero.pfile_write_hero` passes UNCHANGED - the hero file's hash
did not move, which is the proof that widening the enum moved no saved byte.
