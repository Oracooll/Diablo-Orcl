# Six Keys, a Bell, and the Plate That Finally Fit

**Versions:** 1.7.57 - 1.7.61
**Date:** 2026-08-17
**Files:** `Source/panels/spell_book.{cpp,hpp}`, `spell_icons.{cpp,hpp}`, `Source/oracool/hud_art.{h,cpp}`, `skill_sounds.{h,cpp}`, `hero_chunks.{h,cpp}`, `Source/control.{h,cpp}`, `Source/diablo.cpp`, `Source/engine/render/scrollrt.cpp`, `test/writehero_test.cpp`, the stinger WAV

The Abilities-window batch (ten user requests plus three follow-up reports), shipped across five versions. What each one turned out to be:

---

## The plate that never fit (1.7.58)

> Fix the damn background of the skills. it has been like this forever. Dont you see it.

`DrawSkillIconPlate` sized itself from `GetSmallSpellIconSize()` — the vanilla **37×38** spell icon — while a tree cell is **56×56** and the class strips draw at their own natural size on top. The plate has been ~19px narrow and ~18px short under every tree icon since plates existed. It never read as a backing because it never was one; it was a smaller square behind a larger picture.

Fixed by rescaling the plate to cover the cell (Phase 0.6's scaler, cached per percentage, rounded **up** so it can never be narrower than the art). Two follow-ups from the first build:

- *"i see other icons on top"* — the scaled list was indexed with the raw `SpellID` instead of through `SpellITbl`. A SpellID is not a frame number; the blank plate is frame 26 *through the table*. The backing had been whatever real spell icon sat at that index.
- The scaling lives in `spell_icons.cpp`, because the sprite list and the translation table are file-local there — exporting two internals to fix one draw call would have been the wrong trade.

## Red means empty (1.7.58)

> Unlocked skills with 0 points in them are unavailable and inactive, ergo need to have red background, not green.

A third plate state, distinct from the two that existed: **Grey** = not earned, **Pink** = earned but blocked right now (mana, shield), **Red** = earned and empty — the only one of the three the player clears by spending a point. `PAL16_RED` is one of the game's own ramps, so unlike the green this needed no palette injection.

## Spending on the icon (1.7.58)

The gold `+ 0` bar under every cell is gone. A green **+** sits bottom-right when a point can go in; a red **−** sits bottom-left when a rank can come back; both are 13×13 invisible hit boxes with 4px bars, outlined in 1px black on hover (2px first, thinned on request). The rank number sits between them on the icon's bottom edge.

Refunds are free and unlimited — *"redistribute skill points at will"* — with one consequence handled: an aura refunded to zero is put out, because `ToggleClassAura` already refuses to light an empty aura, and leaving one burning would be the only way to hold a state the rules forbid.

## The bell (1.7.59)

> the first metalic jing i dont like. The second "HELL BELL" part i love.

The envelope showed exactly what the user heard: three metallic clicks at 0.30/0.43/0.55s, the bell swelling from 0.60s. The WAV is cut at **0.597s** — the trough between the last click and the bell's attack — keeping 1.85s, with a 4ms fade-in so the cut itself cannot click. Header lengths rewritten in place; the MPQ carries the trimmed file.

## The chime that would not die (1.7.59, then properly 1.7.60)

Two attempts, and the second is the honest design:

1. First fix: the completion check *self-armed* on its first call — record, don't ring. **Still rang.** The load pipeline calls `CalcPlrInv` more than once while equipment lands piece by piece, so the first call recorded a half-dressed character and a later one watched the set "become" complete.
2. Real fix: a completion check is a **no-op** until the arm at the *end* of `LoadGameLevel`. Only the settled character is a baseline. A load can no longer ring by construction, not by racing.

## Arrows up, nav row gone, points on the HUD (1.7.60)

The prev/next arrows moved into the title band's extreme ends — inside the ornate border, outside the centred title's reach. That left the 26px nav row holding nothing, so it was deleted and **every sheet's content starts 26px higher**, which is most of what "fit the contents in the blue box" asked for (the box the user drew matches the declared content rect almost exactly; the code now actually honours it).

The `Points: N` readout — visible only while the window was open — became a HUD element: a `LevelUpIconSize` placeholder frame above the RMB well, themed fill, gold border, count centred, hidden at zero. Sized exactly as the promised art so the swap is one draw call.

## F1–F6 (1.7.61)

The reserved span: intercepted in `PressKey` **before** the keymapper, so no ini row — the old `Help=F1` and `QuickSpell=F5..F8` defaults every settled install still carries — can double-book them. Help and the quick-spell actions stay keymappable elsewhere, default-unbound.

- **Window open** → an F-key edits bindings: binds the hovered ability (known spells; implemented, known tree actives — the same gate click-to-ready uses). SHIFT unbinds; so does pressing a key on the ability that already holds it.
- **In play** → the vanilla quick-spell path readies the bound ability. Slots 0-5 of `_pSplHotKey` *are* the six keys — one array, one meaning, and the speedbook's own labels stay correct for free.
- **The badge**: "F1".."F6", FontSize12, a 13×13 invisible frame at the icon's top-right, list rows and tree cells both.
- **Persistence**: hero chunk tag 7 — u8 count + one `PackReadiedSpell` byte per key, the type re-derived on load from the spell masks exactly as the readied slots do. V1 loads through the hero pack, which never carried the vanilla hotkey array; without the chunk every binding died with the session. `Writehero` re-baselined for the intended tail change.

---

## Verification

Each version built Debug-clean and ran the full suite at **441/443** (the two standing baseline failures) before its commit. The stinger edit is verified by envelope, not by ear — the ear is the user's part.

## Still open from this batch

- The tooltip fix, plates, spend boxes and hotkeys all await the user's in-game pass.
- "Fit the contents" is delivered as the nav-row reclamation plus the pre-existing hard clip at the content rect; if any sheet still visually escapes the box in play, that is a new report against a specific sheet.
- The unspent-points frame and the F-key badge both await their real art.
