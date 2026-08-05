# Diablo Oracool Edition — Changelog

This file tracks every feature Oracool Edition adds on top of DevilutionX 1.5.5, organized by the Oracool release version each one first shipped in. Oracool's own version number (shown on the main menu, e.g. `Oracool Edition v0.1.3`) is independent of the DevilutionX engine version and the multiplayer compatibility version — it only tracks this mod's own feature history.

All features are single-player-only unless stated otherwise, and every feature that can reasonably be made optional is controlled by a setting in `diablo.ini` under `[Oracool Edition]`.

---

## v0.1.0 — Initial Release

The first Oracool Edition build. Ports and re-verifies the full 1.5.4-era Oracool feature set onto the DevilutionX 1.5.5 engine base, plus several new quality-of-life additions.

### Local, Portable Save Files

The game now keeps its configuration file (`diablo.ini`) and its save games (`Saved_Games`) in the same folder as the game's executable, instead of scattering them into Windows' per-user Documents/AppData locations the way vanilla DevilutionX does. This makes the whole install self-contained and portable — you can copy the game folder anywhere (a USB drive, a different PC, a backup folder) and your settings and characters travel with it. This groundwork also introduced the entire `[Oracool Edition]` settings category in `diablo.ini` and the single-player guard every subsequent Oracool feature is built on, so nothing here ever touches a multiplayer game.

### Runtime Branding

The game window and process identify themselves as `Diablo Oracool Edition` rather than plain DevilutionX. Purely cosmetic — no gameplay, save, or multiplayer behavior changes.

### World & Item Quality-of-Life Batch

A bundle of five independent, individually toggleable conveniences:
- **Unique Item Drop Multiplier** — scales how often eligible drops become vanilla Unique items.
- **Unlock All Town Entrances** — every dungeon branch's town entrance is open from the start, no level requirements.
- **Permanent Infravision** — see monsters through walls at all times, as if the Infravision spell were always active.
- **Auto Identify Drops** — picked-up items are automatically identified, skipping the scroll/Cain step. (This setting is the single shared switch every later Oracool item tier — Rare, Buffed Unique, and eventually Primal — also respects; none of them force their own identification state.)
- **Auto Pickup Range** — a configurable radius around your character within which enabled pickup categories are grabbed automatically.

### Griswold Unique Items Shop

Adds a `Buy unique items` option to Griswold's store: an independent stock of identified, non-duplicate Unique items, generated once per game and excluding anything above your character's level. Price is the item's normal value times a configurable `Griswold Unique Item Price Multiplier`. Purchased items are gone for good — they don't restock — and Griswold's Premium Refresh/Refresh Until features never touch this stock. Purchased uniques can also be resold back to Griswold normally, even ones based on quest-range items.

### Automatic Saving

The game saves itself periodically, and immediately after a level change, a non-gold item pickup, or a store purchase (with rapid-fire pickups/purchases debounced into a single save rather than spamming saves). Saves are only ever taken while it's actually safe to do so — gameplay active, character alive, no menus/stores open, nothing held on the cursor, no demo recording in progress. A brief "Game Saved" message can optionally confirm each one.

### Gold Stacks Buff

Raises the maximum amount of gold one inventory stack can hold from the vanilla 5,000 up to 65,535 — the highest value the existing save format's gold field can represent without a format change. Every existing gold-handling path (pickup, merging, auto-placement, splitting, store payment, stash withdrawal) already shares one internal limit constant, so they all pick up the new cap automatically. The Auric Amulet's normal 10,000-stack bonus is superseded while this is on, since 65,535 is already the absolute ceiling. Disabled or in multiplayer, the vanilla 5,000 limit (and the Auric Amulet's 10,000) returns exactly as before.

### Fixed Pepin Potions at Griswold

Griswold's `Buy consumables` list always opens with exactly four infinitely-restocking Pepin potions — Healing, Full Healing, Rejuvenation, and Full Rejuvenation — in that order, followed by Adria's normal rotating stock. No other Pepin merchandise appears in Griswold's list, and Adria's own store is untouched.

### Stackable Consumables

Every non-quest consumable — potions, elixirs, scrolls, books, and Hellfire oils — now stacks up to 99 to a single inventory or belt slot instead of eating one slot per item, matching how most modern ARPGs handle consumables. Two consumables merge only if they're the exact same base item and share the same identified state. Picking up, buying, or dragging matching consumables together merges them automatically (topping off an existing stack before falling back to a new slot); using one decrements the stack instead of deleting the slot; a small quantity number is drawn over the icon; shift+right-click opens a split dialog; and selling a stack at a vendor pays out per-unit price times quantity. Monsters that steal a potion (the Succubus) only ever take one unit from a stack, never the whole pile. Old saves with unstacked potions are unaffected until the next matching pickup merges them naturally — no migration pass runs.

### Respawn In Town

Adds a `Respawn In Town` choice to the death menu (between `New Game` and `Load Game`), available only while your character is actually dead. Choosing it revives you in town with every piece of equipped gear, every inventory and belt item, and all your gold fully intact — nothing is scattered on the ground when you die while this is enabled. Every other death-menu option keeps working exactly as before. Multiplayer is untouched; it keeps its own vanilla `Restart In Town` behavior and item-drop rules regardless of this setting.

### Two-Row Main Menu Version Display

The main menu shows two separate lines: the DevilutionX engine version on top (`DevilutionX 1.5.5`) and the Oracool release version underneath (`Oracool Edition v0.1.0`), instead of one conflated line. This keeps Oracool's own version number independent of the engine version and the multiplayer compatibility check, which both continue to track the DevilutionX base exactly as before.

---

## v0.1.1 — Belt Mod

### Belt Mod

Belt slots stop emptying after a single use. Each belt slot now holds its own physical stack of up to 99 units of a consumable (building on Stackable Consumables above); once that stock is fully used up, the slot automatically refills in one batch from a matching stack in your inventory — pulling from more than one inventory stack if needed to reach the cap. If nothing in your inventory matches, the slot simply goes empty and is free for anything to occupy next; belt layout is never "reserved" for a type you've run out of. Manually dragging items onto or off the belt, and reordering belt slots, both keep working exactly as before. While Belt Mod is active, the belt no longer shows its old hotkey-number overlay on occupied slots (the quantity number from Stackable Consumables already conveys what you need to know). Disabling the setting restores vanilla one-shot-then-empty belt behavior, including the hotkey numbers, without touching any stacks you already have.

---

## v0.1.2 — Shared Item Foundation & Rare Items

### Shared Extended Item-Data Foundation

An invisible architecture update with no gameplay effect of its own — it exists purely to give the item tiers below (and future ones) somewhere safe to store more information than a vanilla magic item can hold. Every item can now optionally carry a tier label (Rare / Buffed Unique / Primal), and up to three prefixes and three suffixes with their own rolled values, on top of everything a normal item already stores. This new data lives in its own separately versioned save file that's purely additive — a save that predates this feature, or has no tiered items in it at all, is completely unaffected, and a corrupted or unreadable extension record can never destroy the underlying item, only its extra tier/affix flourish.

### Rare Items

A new item quality tier between Magic and Unique in both power and rarity. Rare items are generated from ordinary white base items using the game's own existing magic-item affix system, but can carry up to two prefixes and two suffixes (four total) instead of vanilla's one-and-one limit — always at least one of each, with a second prefix and a second suffix each independently having a chance to also appear, so smaller Rare items are more common than fully-loaded ones. No affix repeats on the same item, and the game's Good/Evil affix-conflict rule is respected across the whole item, not just one pair. Rare items display in yellow as `Rare {item name}` and are priced by summing the value of every affix they carry. Whether a Rare item drops already identified is governed by the same `Auto Identify Drops` setting as every other item — Rare items never force their own identification state. Hovering a Rare item shows the same detailed floating stat popup vanilla Unique items already use, listing every affix; that popup (for Rare items and vanilla Uniques alike) now correctly renders above every other open panel or dialog, fixing a display-order bug in the underlying engine code. How often Rares appear is controlled by `Rare Item Drop Chance` (a percentage, checked only after an item has already failed to become a real Unique, so Rares never reduce or replace Unique frequency); setting it to zero turns the feature off for new drops while leaving any Rare items you already own untouched.

---

## v0.1.3 — Buffed Uniques

### Buffed Uniques

A second procedurally generated item tier, one step above Rare and reusing almost everything Rare Items already built (the affix engine, the stat popup, the store listing, and the pricing). A Buffed Unique carries at least two prefixes and two suffixes (four total minimum) and up to three of each (six total maximum) — noticeably more powerful than a Rare item, with the same "smaller is more common" weighting. Existing predefined vanilla Unique items (Griswold's, monster drops, everything from the base game) are completely unaffected — Buffed Uniques are an entirely separate, unlimited population, not a change to any specific named Unique. Despite that separation, a Buffed Unique deliberately displays as `Unique {item name}` (not "Buffed Unique") in the same gold color as a real vanilla Unique, so it blends in visually and only reveals its true nature through its (longer) affix list in the hover popup. Like Rare items, identification follows the shared `Auto Identify Drops` setting with no special-casing. How often Buffed Uniques appear is controlled by its own `Buffed Unique Item Drop Chance` percentage, checked immediately after an item fails its real Unique roll and *before* the Rare roll gets a chance — since Buffed Unique is meant to be the rarer of the two new tiers, it gets first opportunity at the item. Setting the chance to zero disables new Buffed Uniques while leaving any you already own untouched.

---

## v0.1.4 — Primal Items

### Primal Items

The highest item tier: the best possible version of a Buffed Unique. A Primal item always carries exactly three prefixes and three suffixes — never fewer, unlike Rare or Buffed Unique's variable counts — and every single one of those six affixes is rolled at the absolute maximum value it's capable of, rather than randomly somewhere in its range. Its durability is also always full the moment it drops. In short: nothing about a Primal item's stats is left to chance, only which six affixes it happens to roll. Primal items display in orange as `Primal {item name}` — distinct from every other item quality. (The roadmap originally called for cyan here, but that color isn't available without a meaningfully riskier engine change and a brand-new, unverifiable art asset; orange was chosen instead, which also happens to match Diablo 3's own convention for Primal Ancient items.) Like every other tier, identification follows the shared `Auto Identify Drops` setting with no special-casing, and everything else — the stat popup, store listing, pricing — is shared with Rare and Buffed Unique. How often Primals appear is controlled by its own `Primal Item Drop Chance` percentage (default 1, the rarest of the three tiers), checked *before* Buffed Unique and Rare get their turn, since Primal being the best tier means it should get first opportunity at any given drop. Setting the chance to zero disables new Primal items while leaving any you already own untouched.

---

## v0.1.5 — Item Tier Corrections

### Description panel wording for Buffed Unique and Primal

Hovering a Buffed Unique or a Primal item previously showed "rare item" in the description panel under the belt row, regardless of which tier the item actually was. Buffed Unique now correctly reads "unique item" (matching real vanilla Uniques, since it's meant to blend in visually), and Primal now correctly reads "primal item". Rare items were already correct and are unaffected.

### Primal items guaranteed to always roll a full 6 affixes

Some Primal items were able to drop with fewer than the guaranteed three prefixes and three suffixes, most likely when the drop's level window was narrow enough to run out of eligible affixes before the full count was reached. The affixes that did roll were still correctly maxed out — only the count guarantee was affected. Primal generation now ignores level-window restrictions when selecting its forced affixes, so every Primal item reliably carries its full six affixes regardless of the level it dropped at.

---

## v0.1.6 — Item Tier Affix-Count Guarantee (all tiers)

### Rare and Buffed Unique now also guarantee their minimum affix count

The previous fix for Primal items' guaranteed affix count didn't go far enough: Rare and Buffed Unique items could still drop under their own guaranteed minimums (1 prefix + 1 suffix for Rare, 2 prefixes + 2 suffixes for Buffed Unique) whenever their level window was narrow. This showed up most clearly on jewelry — rings and amulets draw from a much smaller pool of affixes than weapons or armor, so a Buffed Unique ring could end up with just a single affix instead of its guaranteed four. Every item tier's minimum affix count is now an unconditional guarantee regardless of item type or the level the item dropped at; only the optional "bonus" affixes above the guaranteed minimum still scale with level, exactly as before.

---

## v0.1.7 — Tabbed Inventory

### Tabbed Inventory

Your backpack now has 10 pages instead of one. Ten small numbered tab buttons sit in the gap above the inventory grid — click one to switch which page's contents the grid shows. Tab 1 is your original backpack, completely unchanged; tabs 2 through 10 are nine brand-new storage pages, each the same size as your original backpack. Every item interaction — placing, picking up, stacking potions and scrolls together, equipping straight from a tab with shift-click, multi-cell armor and weapons blocking the cells they cover — works exactly the same in an extra tab as it always has in your main backpack. The selected tab is shown in gold and drawn slightly larger; the other nine are a muted gray. Gold and quest items always stay in your main backpack (tab 1) and can't be moved into an extra tab, so nothing important ever ends up somewhere the game's quest logic can't find it. Everything you store in an extra tab saves and loads with your character exactly like normal — including a Rare, Buffed Unique, or Primal item's full affix list and coloring — and it all stays inside your one existing character save file; no second file is created. `Tabbed Inventory` is on by default and can be turned off at any time without losing anything already stored in a tab.

---

## v0.1.8 — Tabbed Inventory Hover Fix

### Hovering an item in an extra tab now shows its name and stats

Previously, hovering an item you'd already moved into one of the 9 extra backpack tabs showed nothing — no name, no stats, no stat popup for a Rare/Buffed Unique/Primal item's affix list. This made it hard to confirm an item had actually landed after a move, especially for a Primal item where checking the affix list is exactly how you'd want to verify nothing went wrong. Hovering now shows full item info in every tab, the same as your original backpack.

---

## v0.1.9 — Tabbed Inventory Vendor and Panel Fixes

### Vendors now use extra tab space too

Buying an item from any vendor while your original backpack (tab 1) was full used to fail with a "no room" message, even if all 9 extra tabs were completely empty. Vendors now check the extra tabs too, placing your purchase in the first one with room.

### The classic under-belt description panel now works in every tab

The previous hover fix got the floating stat popup working for Rare/Buffed Unique/Primal items in an extra tab, but the classic description panel under your belt still showed nothing for magic items, basic (white) items, and consumables. That panel now shows correctly for every item type in every tab.

---

## v0.1.10 — Tabbed Inventory Storage and Selling Fixes

### Extra tabs no longer lose items across a save

An item stored in an extra tab could reappear there after being dropped, sold, or otherwise removed, if the game was saved once with the item still in the tab and then saved again afterward. The old data was getting left behind in your save file instead of being properly cleared. This is fixed — removing an item from a tab and saving now sticks.

### Ground pickup and the Stash now use extra tab space too

Picking up an item from the ground (by hand or via auto-pickup) or withdrawing something from the shared Stash used to fail with "no room" whenever your original backpack was full, even with empty extra tabs available. Both now use the extra tabs the same way vendor purchases already did.

### Griswold and the Witch can now buy items back from your extra tabs

Items stored in an extra tab weren't showing up in Griswold's or the Witch's Sell Items list at all. They now appear and sell normally, just like anything in your main backpack or belt.

---

## v0.1.11 — Debug Drop Command and Belt Refill Fixes

### Fixed: debug-spawned items vanishing on drop with "sent an invalid packet"

Using the debug console's `drop {name}` command to generate an item could produce one that vanished the moment it was actually thrown onto the ground, along with a `Player '...' sent an invalid packet` message — even though the item worked completely normally in every other respect (viewing its stats, equipping it) right up until that drop. The debug command was picking a random internal "level" purely to decide which item to generate, with no guarantee that level could ever come from a real monster or dungeon depth; roughly half the time it picked one that couldn't, and the moment such an item was dropped, the game's own network-safety check (the same one that runs even in single-player) correctly refused it — silently discarding the item instead of placing it on the ground. The debug command now checks this ahead of time and quietly tries again with a fresh roll whenever it would have produced an unplaceable item, so every item it hands you can always be dropped normally. This only ever affected items generated through the debug console; ordinary drops from monsters, chests, and the dungeon floor were never at risk, since their levels always come from a real source.

### Belt Mod now refills from extra tabs too

When a belt slot ran out and Belt Mod went looking for a matching stack to refill it from, it only ever checked your original backpack (tab 1) — a matching stack sitting in one of the 9 extra tabs was invisible to it, so the slot went empty even though you clearly had more of that potion. The refill now checks the original backpack first, exactly as before, and falls back to the extra tabs (combining stacks across tabs if needed to fill the slot) whenever tab 1 comes up short.

---

## v0.1.12 — Griswold Unique Shop Drop Fix

### Fixed: items bought from Griswold's Unique Items shop vanishing on drop

Uniques purchased from Griswold's `Buy unique items` shop (added back in v0.1.0) disappeared with the same `sent an invalid packet` message the moment they were dropped on the ground — even though they worked completely normally in every other respect. These purchases are internally tagged so the game can later recognize them as legitimately Smith-sourced when reselling them back to Griswold, but that tag combined with the item's Unique flag in a way vanilla's own drop-safety check had never been designed to allow, so it rejected every single one of them, 100% of the time. The check now correctly recognizes this combination as valid. This was specific to items bought from that shop — regular monster-dropped Uniques, and everything else, were never affected.

---

## v0.1.13 — Ground-Item Tier Persistence Fix

### Fixed: Rare/Buffed Unique/Primal items on the ground reverting to plain magic items after save/reload

Leaving one of the new item tiers on the dungeon floor and then saving and reloading the game could turn it back into an ordinary magic item, losing its tier coloring, its extra affixes, and its place in the floating stat popup — even though its name, stats, and durability all came back correctly. The tier and affix data for Rare/Buffed Unique/Primal items lives in its own save file, separate from the game's normal item data, precisely so it doesn't disturb save compatibility — but that file was only ever being written for items in your backpack, belt, equipped slots, extra tabs, and the Stash. Items sitting on the ground were never included, so their tier data was silently dropped the moment the game reloaded. Ground items on the floor you're standing on when you save now keep their tier and affixes exactly like everything else. (Items left on a different floor you're not currently standing on, then revisited later, aren't covered by this fix yet — a rarer case that would need its own separate save mechanism; flagged for a future pass if it turns out to matter in practice.)

---

## v0.2.10 — Mini-Map

### TAB now cycles through no map, mini-map, and the full map

TAB used to be a plain on/off toggle for the full-screen automap. It now cycles through three states: no map, a small always-in-the-corner mini-map, and the full map you're already used to - pressing TAB again from the full map goes back to no map. The mini-map sits in the top-left corner over the live game view, showing a heavily zoomed-out look at nearby rooms and your position, on a dark backing so it stays readable over the dungeon art behind it. Everything else about the full map - zoom, panning, exploration tracking - works exactly as before.

---

## v0.3.21 — Main Menu Spacing

The trimmed main menu (Single Player, Settings, Exit) now has a blank row's worth of breathing room between each entry instead of sitting cramped back-to-back.

---

## v0.3.20 — Level-Up Sound, Smarter Inventory Sort

Leveling up now plays a sound - the same one you hear when the Poisoned Water Supply quest is completed. There was no level-up sound before this.

The Inventory Sort button now puts 1x1 items (potions, scrolls, and the like) on the bottom row(s) instead of scattering them wherever they happened to fit, so your bigger equipment stays grouped together higher up.

---

## v0.3.19 — Trimmed Main Menu

The main menu now only shows Single Player, Settings, and Exit Diablo/Hellfire. Multi Player, Support, and Show Credits have been removed - Oracool Edition is a single-player-focused mod and those entries weren't relevant to it.

---

## v0.3.18 — XP Counter, and a Panel Layering Fix

Fixed: the LOG button, its log window, and the Game Clock were rendering on top of the inventory, character, quest log, spellbook, and Stash screens instead of being covered by them like the mini-map already was. They now behave consistently with the mini-map.

Also new: an XP Counter sits centered below the mini-map, between the Game Clock and the LOG button, showing exactly how much experience you need for your next level in plain gold numbers (e.g. `2000`). Hidden at max level. Has its own on/off setting like the other two.

---

## v0.3.17 — Game Clock

A real-world clock now sits just below the mini-map's left edge, mirroring the LOG button on the opposite side. Shows 24-hour time by default; a new "Game Clock 12 Hour Format" setting switches it to 12-hour time with an AM/PM suffix instead. Like the mini-map and event log, it has its own on/off setting in `diablo.ini`.

---

## v0.3.16 — Event Log Repositioned Under the Mini-Map

The LOG button now sits directly below the mini-map, flush with its right edge, and the log window opens directly below the button and stretches all the way down to just above the bottom UI panel — giving it much more room to show your history than before.

---

## v0.3.15 — Event Log: Matched to the Mini-Map

The event log window's border now matches the mini-map's own dashed gold border, and its left/right edges always line up exactly with the mini-map's, whatever your resolution. Entry text now wraps at the window's edge instead of running past it. The LOG button no longer has a visible box around it — just the text itself, same as before but without the border.

---

## v0.3.14 — Event Log: Sized to the Mini-Map, Scrollable

The event log window now only expands upward as far as 5px below the mini-map's bottom edge, instead of stretching most of the way up the screen. Since that means fewer entries are visible at once, you can now scroll through the log's history with the mouse wheel while it's open.

---

## v0.3.13 — Event Log Window Narrower

The event log window is now no wider than the mini-map, matching what it visually pairs with instead of the arbitrary fixed width it had before.

---

## v0.3.12 — Event Log Crash Fix

Fixed: clicking the new LOG button (added in v0.3.11) could crash or freeze the game at the default screen resolution — the log window was drawn tall enough to run off the bottom of the screen, and the game doesn't check that kind of thing before drawing. The window now opens upward from the button and always sizes itself to fit on screen, however small the window.

---

## v0.3.11 — Event Log

A new "LOG" button sits just above the durability-warning icons in the top-right corner. Click it to open a collapsible, timestamped log of noteworthy things that have happened this session: game saves (automatic and manual), boss/unique monster kills, Rare/Buffed Unique/Primal/Unique/Quest item drops (with the dungeon level they dropped on), and character deaths (with what killed you, where possible). The log has a dark backing and a gold border to match the mini-map's look, shows the most recent events first, and holds up to 200 entries before the oldest ones quietly drop off. It's session-only — nothing here is saved to disk, so a fresh game starts with an empty log. Controlled by a new "Event Log" setting in `diablo.ini` (on by default).

---

## v0.3.10 — Broken Items Get Their Red X on the Ground Too

The red X on broken (0 durability) items now also appears when the item is lying on the dungeon floor, not just in inventory/belt/equipped slots. Ground items turned out to use a completely separate rendering path that the original fix missed.

---

## v0.3.9 — Mini-Map Border: Thinner and Dashed

The mini-map's gold border is now 1px and dashed instead of 2px solid.

---

## v0.3.8 — Mini-Map Is Now Always On, Independent of TAB

The mini-map is now a permanent HUD element instead of one state on a TAB cycle. It's always visible during gameplay whenever the new "Mini-Map" setting is on (the default) - no key press needed to bring it up. TAB now works exactly like it always did in vanilla Diablo: it opens and closes the full-screen map only. The mini-map automatically hides while the full map is open and reappears the moment you close it. The only way to turn the mini-map off is the new "Mini-Map" setting in `diablo.ini`.

---

## v0.3.7 — Mini-Map Cropped and Bordered, Broken Items Get a Red X, Another Potion-Stacking Fix

### Mini-map no longer wastes screen space on empty dark padding

The mini-map's dark backing was a square, but the actual rendered map content is diamond-shaped and roughly twice as wide as it is tall - leaving big, pointless dark bands above and below the real content. The mini-map now crops tightly to the actual rendered area (mostly shorter, not narrower) and has a thin gold border around it.

### Broken items get a red X

An equipped item at 0 durability already turned grayscale (since v0.2.6). It now also gets a red X stamped over its icon, wherever that icon appears - inventory, belt, equipped slots.

### Mana/health potions: another stacking bug fixed

A potion found in the dungeon could refuse to stack with an otherwise-identical one bought from a vendor or carried since character creation. This was a second bug behind the same symptom fixed back in v0.2.3 - potions track an internal "identified" flag that's set inconsistently depending on where they came from, even though it has no actual effect on a potion (only equipment cares about identification). That flag is no longer part of the stacking check.

---

## v0.3.6 — Mini-Map Moved to the Top-Right Corner

The mini-map now sits in the top-right corner instead of the top-left, matching where Diablo 3 and 4 put theirs. Nothing else about it changed.

---

## v0.3.5 — Inventory Tab Numbers: Red to White

Inactive inventory tab numbers were actually rendering as dark red rather than the intended silver/gray - a font-color quirk on our end, not a display issue on yours. They're now white.

---

## v0.3.4 — Marker Right-Sized, Click Feedback on RESET and SRT

Now that the mini-map marker is actually visible, 15x15 turned out to be overkill - it's now 4x4. The RESET and SRT buttons also now show a clear color change while you're clicking them: RESET turns white while held (gold normally), SRT turns gold while held (white normally). Both revert to their normal color on release.

---

## v0.3.3 — Mini-Map Marker Actually Fixed, Sort Button Shortened

### The mini-map marker is now actually visible - it was a positioning bug, not just a size problem

The bigger, bolder marker added in v0.3.2 still wasn't visible in-game. The real cause: it was being drawn using the full screen's center coordinates instead of the mini-map's own small area, so it was rendering completely off-canvas every time, regardless of size. That's now fixed - your position marker actually appears on the mini-map. While fixing it: made it bigger again (15x15, up from 7x7) and enlarged the mini-map itself another 10% (223x223, up from 203x203).

### Sort button shortened

"SORT" is now "SRT" - the button is back to a more compact size.

---

## v0.3.2 — UI Polish: Buttons, Autosave Sign, Bigger Mini-Map and a Visible You

### Reset Stats and Sort buttons now use words, not symbols

The Reset Stats button (character panel) now reads "RESET" in gold, replacing a circular-arrow icon that didn't render well. The inventory Sort button now reads "SORT" in white, replacing the "$" symbol.

### Autosave notification: a static gold sign instead of a quick blink

Automatic saves now show a gold "Game Saved" message that stays on screen for a full second, instead of the brief flashing "Saved" text from v0.2.9. The "Auto Save Notification" setting is now on by default for new installs (existing settings are preserved).

### Mini-map: 56% larger overall, and you can actually see yourself on it now

The mini-map is now 203x203 - 30% larger than the original 130x130 (v0.3.1), then another 20% on top of that. More importantly: your position marker was rendering as a barely-visible 1-2 pixel sliver at the mini-map's zoomed-out scale - it's now a solid, clearly visible block instead. The full-screen map's own marker is unchanged.

### Inventory Sort: pairs up same-size items to use space more efficiently

When sorting, two 2x2 items (like certain helms and shields) now try to stack directly on top of each other in the same column as a second priority after sell value, instead of landing in unrelated scattered gaps.

---

## v0.3.1 — Bigger Mini-Map

The mini-map introduced in v0.2.10 is now 30% larger (169x169 instead of 130x130), showing more of the surrounding area at the same zoom level. Its position and everything else about it is unchanged.

---

## v0.3.0 — Torment Difficulty & Level 99

**This release changes the character save format (the level cap increase needs more room to store experience than the old format had). Existing characters cannot be loaded after updating - start a new character. Existing saves are not deleted, so nothing is lost if you want to keep playing an older build instead.**

### A new difficulty above Hell

Torment sits above Hell as a new, single-player-only difficulty. Its monsters are tougher across the board - more health, harder-hitting, better armored, more accurate - and their loot and gold are worth more too. How much harder is entirely in your hands: a new "Torment Difficulty Multiplier" setting (1.1x to 5.0x, default 2.0x) scales everything Torment does on top of Hell's own numbers, without changing Hell itself at all. Want a gentler step up? Turn it down. Want the hardest thing this game has ever offered? Turn it up to 5x and see what happens.

### Difficulties now require a minimum character level (optional)

Starting a single-player game used to let a level-1 character jump straight into Hell with no prerequisite at all. A new "Difficulty Level Gate" setting (on by default) now requires level 15 for Nightmare, level 30 for Hell, and level 40 for Torment. Turn it off and every difficulty is freely selectable again, exactly as it worked before this update.

### Character level cap raised from 50 to 99

Characters can now keep progressing all the way to level 99, instead of hitting a hard wall at 50. Levels 1-50 need exactly the same experience they always have - nothing about your early or mid-game leveling pace changes. Levels 51-99 are new territory: each level costs more than the last, gently at first and dramatically by the time you're closing in on 99, so the far end is a genuine long-haul goal for a dedicated character rather than something you'll stumble into. Torment's generous experience bonus (on top of Hell's own) is the intended way to make a serious run at it. This is an always-on part of the game now, like Torment itself - there's no setting to turn it off.

### Experience display now shows progress toward your current level

Hovering over the experience bar used to show your total lifetime experience and the total needed for the next level - numbers that get awkwardly large once you're deep into the extended level range. It now shows "gained this level / needed this level" instead (e.g. `0 / 25,000` right after a level-up), which stays readable at any level and matches what the experience bar itself has always shown visually.

### Everything else about Hell, Nightmare, and Normal is completely unchanged

This release only adds a new tier and a longer endgame on top - it doesn't touch how Normal, Nightmare, or Hell already play, and doesn't change how leveling from 1-50 feels.

---

## v0.2.9 — Subtle Autosave Indicator

### Autosaves now use a quieter notification

Automatic saves used to show the same "Game Saved" popup as a manual save from the game menu. That popup now only appears for manual saves. Autosaves instead show a brief "Saved" flash in the top-left corner of the screen that blinks a couple of times and disappears within about a second - easy to miss if you're not looking for it, which is the point. This is controlled by the existing "Auto Save Notification" setting; turning it off silences autosave notifications entirely, same as before.

---

## v0.2.8 — Gold Pickup Goes to the Stash

### Gold no longer takes up inventory space

Picking up gold - on the ground, from Griswold's change on a purchase, or from selling an item - now goes straight into your Stash's shared gold pool instead of filling up inventory slots. The Stash's gold pool is shared across every character on your install, exactly like it already was. Your character panel now shows your combined total (inventory + Stash) as "Gold," matching what the store screens have always shown. Spending at a store still works exactly as before - it draws from your inventory gold first, then the Stash, with no change needed there. If you'd rather carry some gold by hand (to hand-place it, or just because), the Stash's existing "withdraw gold" button still works exactly as it always has.

---

## v0.2.7 — Inventory Sort Button

### A new sort button repacks your backpack and extra tabs by value

A new button next to the left ring slot on the inventory panel repacks your backpack and every one of the 9 extra tabs by sell value, most valuable item first, filling tab 1 before spilling into tab 2 and beyond. It runs immediately on click, no confirmation needed. Gold and quest items are never touched by it - they stay exactly where they are. Equipped items and the belt are also untouched; this only reorganizes what's sitting in your inventory tabs. A new "Inventory Sort Button" setting (on by default) controls whether it appears.

---

## v0.2.6 — Broken Items, Smarter Reset Stats, and Three Bug Fixes

### Items at 0 durability go inactive instead of being destroyed

In single-player, when a weapon, shield, staff, helmet, or armor piece runs out of durability, it now stays in its equipped slot at 0 durability instead of being deleted outright. A broken item is drawn grayed-out, contributes none of its stat bonuses, and can't be used to attack (an empty-handed weapon slot behaves the same way it already does when nothing is equipped there) — but it's still yours, and repairing it at Griswold's fully restores it to normal, exactly as if it had never broken. Multiplayer is unaffected; a shattered item there is still destroyed exactly as before, since there's no network format for a "broken but still equipped" item.

### Reset Stats now only removes points you actually spent

Reset Stats used to reset your Strength/Magic/Dexterity/Vitality all the way back down to your class's starting values, which also wiped out any permanent bonus you'd earned from quests or shrines along the way. It now tracks exactly how many points you've manually put into each attribute via the "+" buttons, and Reset Stats only ever removes that amount — any quest reward or shrine blessing baked into your stats stays untouched. If you already have a character from before this change, Reset Stats won't have anything tracked for points you spent previously; it'll behave correctly for every point you spend going forward.

### Fixed: Ctrl+Click on an item stored in one of the 9 extra inventory tabs didn't send it to the Stash

Ctrl+Click-to-Stash only ever worked for your original backpack page (tab 1) — clicking an item in tabs 2 through 10 while the Stash was open silently did nothing. Fixed; Ctrl+Click now works identically no matter which tab is currently open.

### Fixed: The Butcher's Cleaver couldn't be sold to Griswold

A found Cleaver simply never showed up in Griswold's sell list, even though it's an ordinary lootable Unique like any other. The cause is a quirk baked into Diablo's own item data: the Cleaver happens to share its internal ID with the start of the range used for real quest items (the Rock, the Anvil, and so on), so the sell list's "don't let players sell quest items" check caught it by mistake. Fixed with a specific exception for the Cleaver.

---

## v0.2.5 — Scroll Auto-Pickup and Repair List Sorting

### Scrolls can now be auto-picked-up too

A new "Auto Pickup Scrolls" option (on by default) automatically collects every kind of scroll — Identify, spell scrolls, Town Portal, all of them — when you're near one, the same way potions, elixirs, and oils already do. It's a single toggle for every scroll type, not one per spell.

### Griswold's Repair list now sorts by cost, highest first

The list of items you can repair used to show up in a fixed, arbitrary order. It now sorts by how much each repair actually costs, most expensive first — matching how the Sell list already sorts by price.

---

## v0.2.4 — Small UI Fixes

### Reset Stats button moved next to what it resets

The Reset Stats "R" button used to sit off on its own with no visual connection to the "Points to distribute" number it actually affects. It's now positioned right next to that value instead. Its icon also changed from a plain "R" to a circular-arrow symbol (↺) that reads more clearly as "reset" at a glance — this specific glyph hasn't been visually verified yet, so if it shows up as a "?" instead, let us know and we'll pick a different one.

### Rare items now get their own background tint

Rare items were showing the same blue inventory/belt/Stash background as ordinary Magic items, since a Rare item is still Magic quality underneath its own Oracool tier. Rare items now get a distinct yellow background, matching their yellow name color, so they're visually distinguishable from a plain blue item at a glance instead of only by reading the name.

---

## v0.2.3 — Fixed: Some Potions Silently Refused to Stack

### Vendor-bought and starting potions now stack correctly with ones found on the ground

Diablo's item data has always secretly carried two separate entries for several potions — one used specifically by vendors and starting gear, another used by everything monsters and the dungeon floor actually drop — even though both look completely identical to the player. Because Stackable Consumables checked an internal item index rather than what the potion actually *is*, a Potion of Healing bought from Pepin, a Potion of Mana bought from Adria, or either of the two potions every new character starts with could never stack with an otherwise-identical potion found while exploring. Rejuvenation and Full Rejuvenation potions happened to be unaffected by coincidence; every other potion type could hit this. Fixed — potions (and scrolls, checked separately by which spell they cast) now stack based on what they actually are, not which of the two interchangeable internal entries produced them.

---

## v0.2.2 — Curated Default Settings

### New installs now start with a curated, ready-to-play settings profile

Fresh installs and new characters used to start with a fairly bare-bones vanilla settings profile even though most of Oracool Edition's own features already defaulted on. Based on a full review of what actually makes for a good default experience, a `diablo.ini` created from scratch now starts with: Run in Town on, all the info-display options on (Experience Bar, Enemy Health Bar, Show Monster Type, Show Item Labels, health/mana values on the globes, item graphics in store menus), floating combat numbers on (vertical style), Auto Refill Belt, Disable Crippling Shrines, and every auto-pickup category (Gold, Elixirs, all six potion types, pickup in town) turned on. Randomize Quests now defaults off, and Auto Equip Weapons now defaults off (previously the only auto-equip category that defaulted on). The default window size is now 900x600, and Hardware Cursor For Items now defaults on.

On the Oracool Edition side: Auto Pickup Range now defaults to 5 (was 1), Unique/Rare/Buffed Unique/Primal drop rates all default noticeably higher (Unique Item Drop Multiplier 25x, Rare 20%, Buffed Unique 10%, Primal 5%), and Griswold's Unique Shop now defaults off (with its item count and price multiplier still saved at 8 and 20x for whenever it's turned on) along with Griswold's Refresh Until button and the Auto Save notification.

If you already have a `diablo.ini`, none of this changes anything for you — every one of these is a normal, still-configurable option, and existing settings are always preserved. This only affects what a brand-new install starts with.

### Fixed: reconstructing an item from a save could occasionally flip it from Magic to Unique

A latent bug, unmasked by the higher Unique Item Drop Multiplier default above: reconstructing an item from its stored data (loading the character-select preview list, for example) re-ran the same "is this item Unique" dice roll used for fresh drops, using whatever the Unique Item Drop Multiplier is set to *right now* rather than what it was when the item was originally generated. Raise the multiplier enough after an item was saved, and that reconstruction could occasionally flip a perfectly ordinary Magic item into a Unique one. Reconstruction now always uses the un-buffed roll, matching what the game already does correctly for Oracool's own Rare/Buffed Unique/Primal tiers.

---

## v0.2.1 — Unlimited Potion Auto-Pickup

### Potion auto-pickup no longer stops at 16

Vanilla DevilutionX's Heal/Full Heal/Mana/Full Mana/Rejuvenation/Full Rejuvenation auto-pickup options each had a hard ceiling of 16 potions — auto-pickup would stop grabbing that potion type once you were carrying 16 or more. With Stackable Consumables now a permanent part of Oracool Edition, a single inventory slot can already hold up to 99 of a potion, so that 16-piece ceiling no longer made sense — it would silently stop topping off a stack that still had 83 slots of room left. Each of these six options is now a simple on/off switch, matching how Elixir and Oil auto-pickup already worked: turn it on, and that potion type is always picked up within your pickup range, regardless of how many you're already carrying. Existing `diablo.ini` settings upgrade cleanly — anyone who had a potion type's old numeric setting above 0 will find it now simply on; anyone with it at 0 will find it still off.

---

## v0.2.0 — Foundations Pass

This release is a from-the-ground-up cleanup of everything Oracool Edition has added so far, done specifically to make the mod's own code simpler, more consistent, and easier to build on for whatever comes next (bigger systems in the spirit of Diablo 2/3 — think item sets, crafting, deeper skill trees — are the kind of thing this pass was done in service of, though none of that is here yet). It is **not save-compatible with any previous version** — see below.

### Your existing characters will not carry over to this version

The single biggest change in this release happens where you can't see it: every one of the item-tier bugs fixed in v0.1.11 through v0.1.13 came from the same root cause — Rare/Buffed Unique/Primal tier data was bolted onto the save system as a separate file that had to be manually kept in sync with every place an item could live (your backpack, belt, extra tabs, the Stash, the ground), and it was easy to miss one. That entire separate mechanism has been removed. Tier and affix data now travels with the item itself, in the same place its normal stats already do, for every one of those locations at once — including, as a direct side effect, dropped items on *any* dungeon floor, not just the one you're standing on when you save (closing the last gap noted in v0.1.13).

Making this change meant growing the size of every item record in the save file, which **existing characters cannot be read back into** — the game will refuse to load a save made before this version rather than risk silently losing or corrupting items. If you have characters you want to keep from v0.1.x, please back up your `Saved_Games` folder before updating; you'll need to start fresh with this version.

### Several features are no longer optional — they're just how Oracool Edition plays now

A handful of features never really had a good reason to be turned off — they're not balance choices, they're just what makes this Oracool Edition instead of vanilla Diablo. Their INI toggles have been removed and they are now permanently on: uncapped base attributes (up to 255), Respawn In Town, the Gold Stacks Buff, Stackable Consumables, Belt Mod, Tabbed Inventory, permanent free Town Portal, Griswold buying every ordinary item (with Sell All), Griswold selling consumables (the Pepin potions), Griswold recharging staves, and Griswold's sell list sorting by price. If your `diablo.ini` previously had any of these disabled, that setting is now simply ignored. Everything else in the options menu stays exactly as configurable as before.

### Gold can now stack far higher

The Gold Stacks Buff's cap is no longer 65,535 per stack — tracing through the actual save code showed that number was more conservative than it needed to be. Gold can now stack up to 100,000,000 per pile, comfortably beyond anything a real game would accumulate, with no compatibility cost.

### Griswold's "Buy Basic Items" no longer kicks you out when he's sold out

Buying out Griswold's entire basic stock and then reopening that screen used to bounce you straight back to the store menu instead of just showing you an empty list, which felt like being locked out. It now shows the (empty) list normally, same as every other empty item list in the game.

### The Witch's sell list now sorts by price too

Griswold's sell list has sorted highest-price-first for a while; the Witch's now does the same, unconditionally.

### A couple of small inventory bugs fixed

Picking up a stackable consumable when a matching stack already exists in one of the 9 Tabbed Inventory extra tabs now merges into it, instead of starting a redundant new stack while the one in the extra tab sits there unused. Separately, a rare case involving un-equipping a two-handed weapon while both hands were occupied and your backpack was completely full (falling back to an extra tab) could grab and remove the wrong item from your inventory instead of correctly restoring the item you were trying to unequip — fixed.

### Internal cleanup (no player-visible change)

Griswold's and the Witch's sell-list logic, which had grown into two long, nearly-identical blocks of code across several past updates, is now one shared piece of logic. Several other small pieces of duplicated or unnecessarily fragile code around the shops and inventory were cleaned up along the way. None of this changes how anything behaves — it's purely about making the code easier to keep correct going forward.
