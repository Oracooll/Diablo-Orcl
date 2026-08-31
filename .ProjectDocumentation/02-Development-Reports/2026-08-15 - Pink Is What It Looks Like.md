---
date: 2026-08-15
version: 1.5.62
area: Abilities window colour coding
---

# Pink Is What It Looks Like

> make auras and barb sheets brown too, but's call it pink, because that's what it looks to me, not
> beige or brown. pink.

Two changes, and the second is the one worth a report.

## The plate colour reaches every sheet that has plates

Auras and Barbarian now draw on the same plate the Skills sheet got at 1.5.61, so all three read as
one family. `DrawAuraIcon` and `DrawBarbSkillIcon` gained the tint parameter their Paladin and attack
siblings already had; nothing else moved.

What this settles is what the plate colour actually *means*. At 1.5.61 it looked like it separated
one sheet from the other three. It doesn't: it separates **ability kinds** — the things drawn on
plates — from **spells and class skills**, which are drawn as the engine's own single-ramp icons and
keep blue and yellow. Class Skills stays yellow for exactly that reason, and so do the HUD's two
wells, which nobody asked to change.

## The name follows the eye

The ramp is `PAL16_BEIGE`. The engine's own header calls it beige. I called it brown. The user looks
at the screen and sees pink, so `SkillPlateTint::Brown` is now `SkillPlateTint::Pink`.

This is not a cosmetic rename. Every future reader of this code will be looking at the running game
while they read it, and a symbol that disagrees with the screen costs them the same five seconds
every time. The palette table's name is an implementation detail one call deep — `SetSpellTrans`
takes a `SpellType`, and the beige ramp arrives via `SpellType::Scroll`, which is *already* a name
that means something other than what it says here. Adding a second layer of "well, actually it's
called beige" would have been the third name for one colour.

So the enum names what you see, and the comment at `SkillPlateTint` records that the engine files it
under beige — one sentence, at the one place a reader needs it, instead of a name that quietly
argues with the screen forever.

## State

352/354, the standing baseline (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and
`Timedemo.WarriorLevel1to2`, both failing before this change).

Not tested in-game — the user runs the game.
