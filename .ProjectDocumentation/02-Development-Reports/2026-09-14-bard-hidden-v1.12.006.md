# The Bard is hidden

2026-09-14 — v1.12.006

## Why

> "Bard causes too much headache, so remove this class from our mod. I want to introduce Necromancer in his place
> later. Remove from game all Bard items. Hide them, dont remove them. We might resurrect this class later."
>
> "there are items who's assets are bard music instruments. we need to hide these assets from the game."
>
> "search previous RfAs for items that were requested to serv Bard item availability in the game. Hide such assets
> from the game."

## The switch

`oracool/hidden_classes.h` is new. Every piece of this change asks it, so bringing the Bard back is a one-line
edit.

## What is hidden

- **Hero creation.** The Bard row is gone, whether it was offered by `hfbard.mpq` or by the Test Bard option.
- **Hero list.** `SelHeroGetHeroInfo` skips heroes of a hidden class. Their save files are not touched.
- **Settings.** Test Bard is now `Invisible` and defaults off.
- **Items.** `IsHiddenItemBase` covers two bases:

| Base | Uniques | Evidence it was made for the Bard |
|---|---|---|
| War Lute (mace family) | Sunless Oath, Oath of the Unmoved, Worldroot Severance | RfA-04 batch 12 icon "a battle-scarred lute"; RfA-08 `luteflip` tumble; the unique package's CATALOG: every one "recommended: Bard"; Worldroot Severance carries +18% Song Duration |
| Canticle (shield hand) | The Ivory Refusal, Twelve-Nail Ward, Last Hearth's Guard | RfA-04 "a small chained hymn book"; CATALOG: every one "recommended: Bard" |

These are excluded in two places:

- **Drops:** `GetItemIndexForDroppableItem`, the chokepoint for loot, all four vendors and Smart Loot candidates.
- **Vendor uniques:** `CreateUniqueVendorItem`.

This is a generation filter, like the Town Portal scroll's. It is not `IsItemAvailable`, so an item already
carried still loads. The Bard's two starting weapons were already `IDROP_NEVER`. They stay available so a returning
Bard keeps them.

## What is not hidden

- **The other 32 uniques the package marks "recommended: Bard".** They sit on shared bases (daggers, long swords,
  helms, boots, rings, amulets, relics) that every hero wears. Hiding them would shrink the unique pool by a
  seventh for no class reason.
- **Art.** `luteflip.png`, the war lute and canticle icons and the six unique icons are all kept. The Canticle's
  tumble sheet is shared with the Arcane Focus, which stays live.
- **Class data.** Tree rows, stats, sprites, sounds, the song auras and their rings are all kept.

## Census

The Orcl Skill Census now shows the Bard's 72 rows as "Hidden class". After the hide it counts 319 built, 26
reworded, 15 needing engine work, 2 retired and 72 hidden.

## Tests

`OracoolHiddenClass.TheBardAndHisInstrumentsAreHiddenNotDeleted` checks three things:

- which class and bases are hidden;
- that the Arcane Focus is not;
- that the rows still exist.

Debug: 777/777.
