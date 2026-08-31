# A refunded skill stops existing (v1.9.68)

**Date:** 2026-08-27
**Version:** 1.9.67 → 1.9.68
**Tests:** 563, of which 561 pass — the two standing baseline failures, unchanged.

---

## Two symptoms, one cause

Reported after refunding the single point in Smite:

> "the smite icons remains on rmb slot. it need to disappear and be replaced with something else like
> regular/fist attack."
>
> "there is a gold background next to TP spell icon? it should not be there at all. It reads Shield
> Bash! Why? Makes no sense. There is something very wrong with the code."

The instinct was right. One cause behind both.

`InnateSpellsBitmask` granted a Paladin skill on **level and shield alone**. It had never known about
the class tree, so a row with no points in it was still in `_pAblSpells` — and that mask is what the
quick list, the speedbook and the wells all read as "you have this".

So after a refund:

- the quick list skipped the zero-point row in its **SKILLS** section, then found the same skill in
  the mask and listed it again under **SPELLS** — where it drew with **no icon**, because a tree
  skill has no vanilla spell art to fall back on. That is the gold plate reading "Shield Bash".
- the readied slot kept its `SpellID` regardless, so the well went on drawing it and a click went on
  casting it.

**Invisible until yesterday.** Smite moved to level 1 in v1.9.65, which put it in the mask from
character creation instead of from level 8. The defect was years-shaped and one day old in practice.

## The rule now

A tree row must have a point in it to be in the mask. That is not a new idea — it is what the Red
plate has said all along: *"earned and spendable, but nothing invested yet — so the skill exists and
does nothing"*. Being in the mask is the difference between existing and doing something.

`RefreshInnateSpells` recomputes the mask **and** releases anything holding a lost skill: both mouse
buttons and all sixteen F-key bindings. It runs on refund and on invest — the first point is what
puts a skill in the mask, so without it a freshly bought skill would not be selectable until the next
load.

Cleared to `Invalid` rather than to a named attack, because `Invalid` **is** the basic attack on both
buttons. The well falls back to the fist or the sword by itself, and this does not have to know which
of the two the player last chose — which is exactly what the request asked for.

## The stat pool wears the skill pool's frame

> "i like the icon that pops up when skill points are available. use it also for stat points instead
> of the + icon. Now it will show how many stat points are available for distribution."

The vanilla level-up plus said only *that* something was waiting. The frame says *how much*.

`GetLevelUpIconRect` moved to `PointsIconSize` with the same 6 px gap the skill frame uses, so the
two indicators flanking the belt are now the same size at the same height. The rect had to move, not
just the drawing: it is the hit target as well as the frame. The old art is demoted to the fallback
for a build whose points frame is missing.

## Testing

`InnateMaskFollowsTheShield` needed updating rather than fixing. It builds a character with no tree
investment, so under the new rule every assertion would have passed **for the wrong reason** — the
skills absent because unbought, and the shield gate it exists to test never exercised. It now invests
a point in each row it examines.

`RefundingTheLastPointTakesTheSkillOffTheButtons` is new and covers the reported path end to end:
unbought is absent from the mask, the first point grants it, and a refund takes it off both buttons
and the hotkey. Verified by reintroduction — removing the investment check fails it on the mask and
on the buttons.

One thing that test taught me: it needed `gbIsHellfire = true`. `IsValidSpell` refuses every SpellID
past the Diablo range without it, and the Paladin skills all sit past it — so the clearing was being
skipped for a reason with nothing to do with the subject. Oracool is built on Hellfire and always has
it set, so this never affected the game; it would have made the test quietly meaningless.
