# Vanilla formulas meeting raised caps (v1.9.131)

**Date:** 2026-08-31
**Version:** 1.9.131
**Trigger:** user, away: "do a few audits."
**Tests:** 589/590. Two new tests; one red, the level-3 dungeon golden.

Eight audits. Two findings, one of them undefined behaviour, and one balance observation that is the
most consequential thing in this report even though it is not a defect.

---

## Fixed

### 1. Spell damage wrapped NEGATIVE at high spell levels

`ScaleSpellEffect` is **exponential**: it multiplies by 9/8 per spell level, doubling roughly every
six. That was safe in vanilla, where a spell level could not exceed **15**.

This fork raised `MaxSpellLevel` to **98**, and `Player::GetSpellLevel` sums three stores — books,
invested points and item `+spell levels` — while clamping **none** of them, so the effective level
can pass even that.

| Spell level | `ScaleSpellEffect(99, L)` | Flash's max (× 3) |
|---|---|---|
| 15 (vanilla's cap) | 558 | 1,674 |
| 30 | 3,250 | 9,750 |
| 50 | 34,200 | 102,600 |
| 98 (this fork's cap) | 9,770,000 | 29,300,000 |
| 135 | 763,000,000 | **2,290,000,000 — past INT_MAX** |

At 135 the result wraps negative. Signed overflow is **undefined behaviour**, not merely a silly
number, and it surfaced as Flash reporting negative maximum damage.

It now saturates at `INT_MAX/8`, chosen to leave headroom for the largest multiplier any caller
applies afterwards (×5, in the Elemental case). That ceiling is 2.7e8; the largest base in use
reaches 1.4e8 at spell level 98, so **nothing reachable today changes** — this only stops the wrap.

Found by a test that walks **every** spell across levels 1–250 and asserts no formula reports
negative or inverted damage. The sweep is the durable part: it will catch the next formula added
with a multiply in it, and it was cheap to write because the contract is already total ("-1 means no
damage" holds for every SpellID since the 2026-08-15 fix).

### 2. The new name row lost every translation

From yesterday's character-sheet work, so mine. Spell names are marked `P_("spell", "Zeal")` in
spelldat, which puts the context into the catalogue key. The new row translated them with plain
`_()`, so the lookup missed and **every name fell back to English** in a translated build — silently,
which is how it would have survived a play-test.

It uses `pgettext("spell", ...)` now, like `items.cpp` and `objects.cpp` already did. The button
prefix also became a whole format string per side ("Left button: {:s}") rather than a translated
"Left" plus a colon: a bare direction word is ambiguous to translate and several languages need the
parts in the other order.

---

## The balance question, which is not a defect

**At spell level 98, a spell does roughly ten million damage** — before Flash's own ×3. Monster hit
points are in the thousands.

That is not a bug in any line of code. It is vanilla's exponential curve, designed against a cap of
15, meeting this fork's cap of 98. Every spell that goes through `ScaleSpellEffect` inherits it, and
book spells reach 98 legitimately, one book at a time.

The overflow fix does **not** touch this — it only stops the wrap far above it. Reshaping the curve
is a balance decision, and there is more than one reasonable answer:

- clamp the effective spell level used by damage formulas (leave the cap, bound the curve);
- make the growth linear or root-shaped past some level;
- lower `MaxSpellLevel` toward something the curve was built for;
- leave it, on the grounds that reaching spell level 98 should trivialise the game.

That is the user's call, and worth taking deliberately: it is the difference between spells being
strong at the cap and spells being the only thing that matters.

---

## Clean audits

- **Monster name translation.** `Monster::name()` does use `pgettext("monster", ...)`. An earlier
  grep of this suggested otherwise; it only covered `.cpp` files and the accessor is in a header.
  Recorded because the false alarm is worth not repeating.
- **Oracool's own `_(x.name)` sites.** Class tree, item sets and runewords mark their names with
  plain `N_()`, so `_()` is the right lookup there. Only spelldat uses `P_()`.
- **Enum-indexed tables.** Every one checked is declared with an explicit `[SomeCount]` size, so it
  cannot be indexed past its end by a grown enum.
- **The new full-width row.** `PlaceWidgets` derives the + buttons and RESET from the *measured*
  row list, so adding four rows shifted them correctly rather than leaving them behind. The switch
  on `CharRowExtra` needed no new case: those rows carry no widget.
- **Zeal's to-hit across its own chain.** Narrowing the bonus to the armed-skill latch could have
  paid it only on the first strike. It does not: the two sites that clear the latch are the plain
  attack paths, and the comment there already states the rule — the repeat paths "deliberately leave
  the latch alone, since you are still holding the same button".
- **Torment scaling.** `4 × HP + 12800, × up to 5.0` stays inside `int`, and damage and armour class
  are explicitly clamped to 255 with a comment saying why. Someone already thought about this one.
- **Shift-by-level UB.** `CalculateArmorPierce` does `tmac >>= (_pIEnAc - 1)`, which would be
  undefined for a shift of 32 or more. `IPL_TARGAC` only ever contributes 1–3 and there are five
  sources game-wide, so the value tops out around 9. **Latent, not reachable** — left alone rather
  than changing upstream code for a case nothing can produce, but worth remembering if more
  armour-piercing items are ever added.

---

## Verification

589/590 after the change. The formula sweep was checked against the defect it found: with the
saturation removed it fails, naming Flash at level 135.

Nothing here is visual, so nothing needs a screenshot — but the character sheet is worth one glance
for the translation fix, since the name row now reads "Left button: Firebolt" rather than
"Left: Firebolt".
