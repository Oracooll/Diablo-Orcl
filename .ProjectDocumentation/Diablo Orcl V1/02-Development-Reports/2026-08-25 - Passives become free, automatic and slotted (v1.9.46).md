# Passives become free, automatic and slotted (v1.9.46)

The Passive Skills page shipped yesterday as a list. This turns it into a system: nothing on it is
bought, everything on it arrives on its own, and only four of them are live at a time.

## Three rules

**No skill points.** A passive cannot be bought at any level, with any pool. Refused in
`CanInvestClassTreePoint` as well as in the UI, so no path can put a point somewhere it could never
be refunded from.

**One every even level.** The nth passive on the page, in grid reading order, arrives at level
`2n+2` - the first at 2, the eighteenth at 36, the nineteenth at 38 for the two classes that have
one. Nobody outruns their own page.

This breaks the tier's meaning on this page, deliberately. A tier still decides which ROW a cell
sits in, three to a row, but the three cells of a row now open at 2, 4 and 6 rather than together.
`IsClassTreeSkillUnlocked` branches on the page for exactly that reason, and a locked cell prints
its own required level, because the tier can no longer answer that question.

**Four slots, at levels 1, 10, 20, 30.** An unslotted passive is learned and inert. This is where
the page's choice lives - a level-99 character has every passive and runs four.

## The scope line

Asked, because it was the one place the request could have meant two things. "Passives" is scoped to
the **page**, not to `ClassTreeKind::Passive`. The Barbarian's ten Combat Masteries, the Rogue's
Passive & Magic sheet and the Monk's Perfect Vessel are all `Kind::Passive`, and all still cost
points and still hold ranks, because in Diablo II investing deeper *is* the mechanic. Scoping by
kind would have silently deleted that choice and stranded points every existing character has spent.
`TheOlderDiabloTwoPassivesStillCostPoints` is the test that holds the line.

## The gesture

The user picked **slot first, then skill** from four options. Left-click a slot to arm it, then
left-click a passive to fill it. Around that:

- Clicking the armed slot again puts the gesture down. Without it, the only exit from a
  half-finished action would be to finish it.
- Right-click a slot empties it - the same thing the right button means on every other tree page.
- Right-click a **passive** empties whichever slot holds it, so removing one does not require
  finding where it lives.
- Arming is cleared by changing sheet or closing the window. It is a gesture in progress, not
  state, and it must not outlive the page.

Slot-first has one weakness: click a passive with nothing armed and nothing happens. So the band
carries a line that always says which half the window is waiting for - *"Click a slot, then a
passive"* / *"Now click a passive to fill the slot"* - clicking a passive with no slot armed says so
in the message line, and while a slot is armed every eligible passive lights up. The failure mode is
answered three times rather than left to be discovered.

## The layout solved itself

The page needed a slot band it had no room for: seven tiers already overflow the 523px content area
and scroll. But a passive has no rank, so its cells need no counter row - and dropping the counter
shortens every row by exactly the 20px the band costs.

```
82 (band) + 7 x 60 (grid) = 502 <= 523
```

So the page fits **unscrolled**, which is not a nicety: with slot-first, the slot you are aiming at
must never scroll off the top while you reach for the grid. A `static_assert` says so.

## Persistence, and a lesson applied

`HeroChunkPassiveSlots` (tag 12): a count byte and four **class-relative** indices, 0xFF for empty.

Class-relative on purpose. The chunk directly above it stores the burning aura *absolutely*, and
that is precisely why growing the enum at 1.9.45 cost Bard and Monk heroes their lit aura. A
relative index does not move when another class gains rows. The pipeline row asking for the aura to
be converted is still open; this one was simply built right the first time.

Validation happens on the way **out**, not on the way in. A slot holding a passive of the wrong
class, or one the character no longer meets the level for, reads as *empty* rather than as that
skill - so a save can never hand out something unearned.

## Verification

533/535, the two standing baseline failures. Seven tests added, and two of them were verified by
reintroducing the bug they guard:

- Disable the class check in `GetActiveClassAura` → "a Paladin is burning one of the Bard's songs".
- Disable the read-validation in `PassiveInSlot` → "a slot handed out a passive the character has
  not reached".

One existing test had to change rather than be re-baselined:
`EveryInertRowContributesNothing` turned every inert row on by *investing a point*, which passives no
longer accept. It now turns each row on the way that kind is actually turned on - points for D2
rows, a slot for passive rows - so it still proves something. A test that activates nothing would
pass for the wrong reason.

`Writehero.pfile_write_hero` re-baselined for the new chunk, with the reason recorded in its own
history as that test requires. `sizeof(PlayerPack)` is untouched.

## Not verified

How it looks, and how the gesture feels. Both are screenshot questions. The specific things worth
judging: whether the four slots read as slots rather than as four more skills; whether the hint line
is helpful or noise; and whether right-clicking a passive to unslot it is discoverable, since it is
the one part of the gesture the hint does not mention while a slot is armed.
