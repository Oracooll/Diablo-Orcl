# v1.12.417 - the Open Decisions batch (2026-10-06)

The user answered all 70 rows on the Orcl Open Decisions page (https://claude.ai/artifact/Th7FaTCFneUBhLLvHUdKH5): 69 "do as recommended", one own answer (blessedwall: Blessed Hammer keeps passing through walls, as in D3). 56 needed code and went in as one build; 14 needed none.

Built Debug, ctest 957/957 (one fix before green: the new sheet label "Attack frames (first swing)" was wider than the label column; it is now "First swing frames"). diablo.ini md5 unchanged around ctest.

## Changed (56)

### Items, services, arenas, quests
- Auric Amulet's +25% gold find works in single player (AuricAmuletGoldFind).
- White items print their details; life/mana values print whole (ScaledWhole).
- The Mystic's price reads "?" while the item is unidentified (Reroll too).
- Mana-to-life / life-to-mana powers do nothing for Rage users and are hidden from them.
- Tooltip card bullets count only printed stones.
- Sealed maps are spent when the arena boss dies, never on reading; re-entry is free until then.
- An arena boss that cannot be placed falls back to the farthest free tile (PlaceLesserUniqueMonst fallbackFrom); the arrival exit searches rings 2-3.
- Hellfire quests complete on pickup with their entrances unlocked (guards in OpenHive, TalkToBarmaid, quest marks).

### Monsters and allies
- Champions and Dread bosses use the floor's ordinary monster level (to-hit, block, XP brake).
- Dread bosses give 4x XP (other uniques 2x).
- Thunderous's death burst is the champion's own spell: names it on a death, spares its escort, keeps its ring colours; AddNovaBall no longer indexes Players[] with a monster source.
- Monster Charged Bolt (Counselors) rolls the caster's damage instead of a flat 15.
- Stone Curse lasts half as long on anything that FightsAsUnique.
- Companion blows no longer retarget the monster onto the hero; taunts skip bosses.
- Kills by converted monsters (melee and missiles) pay XP.
- Revived archers and casters shoot their own missiles (RevivedShot); the bone-tooth sprite swap is for mage minions only (IsMageMinion).
- Army of the Dead is refused in town, unpaid.

### Paladin, Barbarian, Monk, Sorcerer
- Hammer of Faith's splash is a real side weapon blow (StrikeSideWeaponBlow) per neighbour.
- War cry strikes take the damage passives and on-hit passives; War Cry and Earthshaker count only monsters affected.
- Self buffs (Iron Will, Bloodcall, Rallying Cry, mantras, Astral Projection) are refused when already full (StartBuffUnlessFull).
- Earthquake is refused with nothing in reach; Serenity is refused with nothing to clear.
- Text fixes: Towering Shield, Wrath, Leap Attack, Dynamo, Guardian's Path hover order.
- Bleeds merge (MergeBleed); Iron Robe's own armour removed; Iron Fist's level-up damage gated.
- Ball Lightning +10% damage per rank.
- Seven Reeds and Hundred Fists swing profiles retuned.
- Doppelganger rolls only on a primary blow (not side blows or field pulses).

### Rogue, Necromancer
- Javelin and spear throws are refused with a bow (click + red skill-bar plate).
- Poison Explosion's cloud lasts 3 s and poisons again every second.
- Vault: 3 vaults within 15 s, then 15 s cooldown (sliding window; can no longer be dodged); the hover says so.
- Strafe under Hunter's Claim aims only at monsters its arrows can hit.
- Eternal Torment's text: curses last until the monster dies or you leave the floor.

### Hero sheet
- Block chance is read against the last monster that hit the hero (LastAttackerLevel), equal level before that.
- Warrior/Barbarian double-damage chance in Advanced Stats (melee only).
- Barbarian and Bard bow stat share halved once, not twice (divisors 150/125 + the blow's 50%).
- "First swing frames" label, with StartAttack's MeleeHitFrame-2 clamp.

### Combat
- +Armour vs demons/undead counts against monster missiles too.
- Hellfire Rage rechecks item requirements at start and end (CalcPlrInv / CalcPlrInvKeepingLife), also on leaving the floor.
- The melee hit frame is snapshotted at StartAttack (SwingHitFrame), so a mid-swing click can't add or lose a blow.
- A melee skill from a quick-cast key is one-shot; the held latch returns after its swing.
- A block clears the melee latches like a hit (ClearSwingLatchesOnInterrupt).
- Teleport never lands on a closed door; Apocalypse is refused in town, unpaid.
- A staff charge or scroll moved during the cast animation cancels the cast (no free cast).

### Controls and windows
- Gamepad Cancel acts as Esc (top window first).
- Esc closes the Mystic's reroll offer and the Salvage question before their windows.
- Companion and minion panel headers act on release.
- A Telekinesis cursor clicked over a window is not spent.
- The mouse wheel scrolls a docked shop only over the shop.
- Exiting with Cube/workshop items that cannot go back asks twice (red message, then leaves).
- Telemetry: new session id per game, plus a hero_class column (an old 8-column file is rotated to balance_telemetry.old.csv).

## Left as is (14)
reqlvl, sellsock, riftobj, riftclock, freezecharge, blessedwall (user: passing through walls stays), chargerefund, respecbuff, momentum, areapaid, keyboard, stashremote, beltany, dblthrow.

## Known limits to watch in play
- Revived magma/storm/acid beasts shoot on their plain attack animation; revived Counselors don't fade or teleport.
- The Thunderous burst now also hits the hero's minions and companions.
- Closing the window (Alt+F4) with staged Cube items still goes straight to SaveOnExit.
- The Monk's bow stat share is still halved twice (not on the list).
