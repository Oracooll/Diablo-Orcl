# A worn item shows its sockets (v1.10.006)

**Date:** 2026-09-07
**Request:** "i dont see the gold rings over the asset of a six socket staff. bug?"

Yes. The socket overlay (rings for empty sockets, stones for filled) was drawn on hover in the backpack grid, the stash and Levski's grid, but the body-slot loop in inv.cpp's DrawInv never called it, so a socketed weapon or armour lost its rings the moment it was equipped. A staff is 2x3 cells and takes six sockets, which is why it was the case that showed. The body slots now make the same hover-only call with the sprite's centred bottom-left as the footprint origin.

Also: class_tree.h's note on rows retired as book spells named Golem, Berserk and Search; the two Mana Shield rows (Sonic Barrier, Spirit Ward) are retired by the same rule and are now listed.

Suite 687/687. Needs the user's eye on an equipped socketed item.
