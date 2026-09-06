# External audit (ChatGPT, 2026-09-06), patch A: every tree skill above id 63 had rank 0 (v1.9.305)

**Date:** 2026-09-07. Source: `Oracool.MPQ\ChatGPT Audits` (seven reports against v1.9.304), verified finding by finding against the code before anything was changed.

## SKL-01 (P1, confirmed)

`Player::GetSpellLevel` bounded its lookup by `sizeof(_pSplLvl)` - the 64-wide legacy BOOK-level array - and returned 0 for every spell id from 64 to 125: Frozen Orb, the cold armours, the Rogue's arrows, the class melee skills, the warcries and songs. The tree showed the points spent (it reads `_pSkillInvestment` directly); every mechanic that asked for the rank got 0, or 1 where a caller clamps. Fix: bound by `MAX_SPELLS`, add the book level only where the book store reaches. Test: ids 63, 64 and 125 explicitly, +skill items above the store, and every active class-tree row with a real spell id reaches rank 1 after one point.

## SKL-02 (latent)

`ValidatePlayer`'s per-tick clamp loop indexed `_pSplLvl[b]` up to MAX_SPELLS for any book-enabled spell; none exists above 63 yet. Guarded.

## NET-01 (confirmed, dormant while multiplayer is off)

`PackNetPlayer`/`UnPackNetPlayer` copied MAX_SPELLS bytes through the 64-byte store: a 62-byte over-read on pack and an over-WRITE on unpack into `_pUnspentSkillPoints` and `_pSkillInvestment`. Pack copies 64 and zeroes the packet's tail; unpack copies 64. Packet layout unchanged. Test with canaries on the two following fields.

## SAV-02 (confirmed)

`GetSpellBitmask` shifted by -1 for Null and -2 for Invalid; a malformed readied scroll in a save could reach it through `IsReadiedSpellValid`. Total now: an empty mask outside 1..127. Test.

Suite 638/639, the standing dungeon-generation failure only.
