# RfA-03 applied: aura rings, event sounds, drop tumbles, UI plates (v1.11.032)

**Date:** 2026-09-11
**Branch:** renderer-32bit, local commits only
**Request:** `Resources\ChatGPT RfA\RfA-03 - Aura Rings, Event Sounds, Drop Tumbles, UI Plates.md`, batches 8-11, under the Gold asset loop

All four batches arrived together. Each file passed its checks:
- the rings at 512x256 with real soft alpha peaking at 166 (65%);
- the WAVs mono 22050 Hz 16-bit and inside their duration ranges;
- the tumbles and plates at exact size, with alpha only 0 or 255;
- every preview viewed.

The work was split three ways across files that did not overlap. Two agents took the rings with the plates, and the tumbles. I took the event sounds and reviewed both diffs.

## Batch 8: ten aura rings

Nine for the Bard's songs (Melody of Life, Battle Hymn, Song of Swiftness, Song of Fortitude, Dirge of Dread, Discord, Inspiration, Tale of Heroes, Weaken) and one for the Monk's Healing Mantra. `AuraFiles` in `oracool/aura_ground.cpp` grew from 20 to 30. **Nothing had restricted rings to the Paladin**: `GetActiveClassAura` requires only `Kind::Aura` and the player's own class, so the missing table rows were the whole gap.

## Batch 9: five event sounds

`oracool::PlayUiEventSound` (skill_sounds) plays `sfx\ui\{salvage,transmute,orb-absorb,socket,runeword-complete}.wav`. It returns false when there is no audio or the file is missing, so each caller keeps its old vanilla sound as the fallback. The replacements:
- Salvage-all's `IS_ISHIEL`
- A successful transmute's `IS_ISHIEL`
- Orb absorbed: `IS_CAST7`
- Gem or rune socketed: `IS_IGRAB`

**A socket that completes a runeword plays the runeword sound INSTEAD of the socket click**, never both, which keeps the 2026-09-03 one-sound rule.

## Batch 10: five drop tumbles

- **Tables:** `gemflip`, `runeflip`, `charmflip`, `orbflip` and `signetflip` are appended to ItemDropNames, ItemAnimLs, ItemDropSnds and ItemInvSnds as indices 43-47. `ITEMTYPES` goes from 43 to 48, with static_asserts holding every table to that count.
- **Loading:** `LoadPngItemDropSheet` in sprite_import loads them through the missile route's quantisation. `InitItemGFX` tries the PNG first and falls back to the CEL; without its PNG a new entry uses `larmor`, which also has 13 frames.
- **Mapping:** `GetItemDropAnimIndex` maps icon families by range: gems, runes, charms (Phase 1, Salvaging, growing, and the encounter rewards but not their maps), mystic orbs, and the signet. Contiguity asserts guard the ranges, and everything else stays on larmor.
- **Sounds:** gems and runes use the rock sounds, charms and the signet the ring sounds, the orb the bloodstone sounds.
- **Save safety:** the drop animation index only reaches visuals, sounds and label centring, never item generation. The one saved related value, the frame count, is 13 before and after.

## Batch 11: two UI plates

- **Runeword Book slot keys** (84x20): idle, hover and selected states. A static_assert ties the plate to `SlotKeyRect`'s computed width.
- **Shop toast** (312x34): asserts hold it to `ToastHeight` and the toast's width.
- Both fall back to the old border when their PNG is missing.

## Also

My edit helper had again turned inv.cpp's two bare-LF include lines into CRLF. They were restored byte for byte, and the other touched files were checked against HEAD.

## Verification

Debug and Release built. ctest **699/699 with no expected value changed**, so no saved item rebuilds differently. Both trees repacked, RTM refreshed with exe 1.11.032, packages filed under `02-source-art\delivered-packs`.

Not seen or heard in play. Things to check:
1. A Bard song or Healing Mantra showing its ring.
2. The five event sounds.
3. A gem, rune, charm, orb or signet dropped on the floor.
4. The Runeword Book's slot keys and the shop's refusal banner.
