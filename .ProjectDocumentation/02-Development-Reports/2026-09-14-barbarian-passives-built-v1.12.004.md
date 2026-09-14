# The Barbarian's passives, built

2026-09-14 — v1.12.004

## Why

> "Now try to develop all passives os Barbarian you can. And make an artifact with whats not developed and why."

## Built

### Passive Skills page (work while slotted)

| Passive | Cell / level | Rule | Hook |
|---|---|---|---|
| Pound of Flesh | 0,0 / 2 | a kill heals 3% of max life | `OnPassiveMonsterKilled` |
| Weapons Master | 1,0 / 8 | sword +15% damage; axe +15% to hit; staff FastAttack; mace +1 Rage per landed melee blow | `ApplyPassive` (sheet), `OnPassiveHit` (mace) |
| Berserker Rage | 1,2 / 12 | +25% damage while Rage ≥ half the pool | `PassiveDamageDealtPercent` |
| No Escape | 3,1 / 22 | +25% damage to enemies 5+ tiles away | `PassiveDamageDealtPercent` |
| Juggernaut | 4,1 / 28 | slows 50% shorter; half of staggers ignored; a stagger that lands has a 30% chance to heal 20%, 10 s cooldown | `SlowPlayer`, `StartPlrHit` → `PassiveShrugsOffStagger` |
| Inspiring Presence | 4,2 / 30 | warcry blessings last 200%; 1% life a second while any is on | warcries `StartBuff` → `PassiveWarcryDurationPercent`; `ProcessPassivesTick` + `AnyWarcryBuffActive` |
| Earthen Might | 5,0 / 32 | Ground Stomp, Seismic Slam and each Earthquake pulse give 3 Rage per enemy struck | rfa12 `EarthenMightRage` |

No Escape was reworded: its "what you throw" had nothing to act on, so it now works by distance.

### Masteries

- **Spear Mastery**: +10% to hit (+5% per level) and +10% damage (+6% per level) while holding a spear or pike.
  - It was marked inert because the engine has no spear item type.
  - The Spear and Pike bases have been identified since RfA-12 (`WieldingSpearOrPike`), so that reason no longer held.

## Not developed

Published as the Barbarian Passives Ledger artifact.

- **Double Throw, Throwing Mastery:** the engine has no thrown weapons.
- **Increased Stamina:** the engine tracks no stamina.
- **Boon of Bul-Kathos:** retired from the page by the user on 2026-09-12. It was the 19th passive, and the page holds 18.

## Tests

- **`oracool_rage_test`**:
  - every page passive is built and none says "Not yet built";
  - Juggernaut's slow shortening and Inspiring Presence's duration answer only while slotted.
- **`oracool_audit_test`**: the passive-page census goes from 43 to 50.

Debug: 771/771. Release built and copied to RTM.
