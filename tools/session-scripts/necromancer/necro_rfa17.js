// Writes RfA-17 from the row list. Re-run after the ledger is settled (pass the ledger's rows as JSON if they changed).
const fs = require('fs');
const rows = require('./necro_rows.js');
const subjects = {
	RaiseSkeleton: 'a skeleton rising waist-deep out of the ground, sword arm up',
	SkeletonMastery: 'a skull wearing a small crown',
	CommandTheDead: 'a pointing skeletal hand with three small skulls following its line',
	ClayGolem: 'a squat golem of cracked clay, arms hanging',
	GolemMastery: 'a golem head inside a gear-like ring',
	GatherTheDead: 'four small skulls drawn inward by arrows to a centre point',
	RaiseSkeletalMage: 'a robed skeleton holding up a small orb',
	SummonResist: 'a ribcage behind a small round ward',
	DarkMending: 'a broken bone joined by stitches',
	BloodGolem: 'a golem with a heart visible in its chest, a drop falling from it',
	BonePlating: 'a ribcage as a breastplate, rivets on the ribs',
	FrenzyOfTheDead: 'two crossed skeletal arms with speed marks',
	IronGolem: 'a golem of riveted plates, square-shouldered',
	LastingBond: 'an hourglass whose upper bulb is a skull',
	UnholyOffering: 'a skull cracking in two over an open hand',
	FireGolem: 'a golem wreathed in flame tongues',
	NecroRevive: 'a horned monster silhouette rising with a halo of three motes',
	ArmyOfTheDead: 'a row of five skulls over a broken ground line',
	Teeth: 'a fan of five fanged teeth flying outward',
	BoneArmor: 'three bones orbiting a small figure',
	PoisonDagger: 'a dagger with drops falling from its point',
	CorpseExplosion: 'a ribcage bursting outward in shards',
	BoneSplinters: 'three thin bone shards in a narrow spread',
	Blight: 'a bolt ending in a bubbling pool',
	BoneWall: 'a palisade of upright bones',
	BoneSpikes: 'three spikes of bone erupting from the ground',
	PoisonExplosion: 'a ribcage inside a billowing cloud',
	BoneSpear: 'one long barbed spear of bone, diagonal, motion lines',
	Decompose: 'a hand dissolving into drips',
	Marrow: 'a long bone split lengthwise, its core marked',
	BonePrison: 'a ring of bones curving inward over a small figure',
	BoneStorm: 'a spiral of bone shards around a centre',
	Virulence: 'a drop with a skull inside it',
	NecroBoneSpirit: 'a skull with a long ghostly tail, flying',
	PoisonNova: 'a ring of drops radiating from a centre',
	DeathNova: 'a ring of bone shards radiating, a skull at the centre',
	AmplifyDamage: 'a skull under a downward-pointing broken sword',
	CurseMastery: 'an open eye inside a pentagon',
	EssenceTap: 'a wisp flowing from a skull into an orb',
	DimVision: 'an eye half closed with a bar through it',
	NecroWeaken: 'a bent sword drooping',
	Frailty: 'a cracked bone about to snap',
	NecroIronMaiden: 'a spiked coffin, door ajar',
	Terror: 'a screaming face with hands on its cheeks',
	Bane: 'a heart pierced by a thorn, drops falling',
	Confuse: 'a head with a spiral over it',
	LifeTap: 'a heart with a spigot, one drop',
	WideMalice: 'two concentric rings with outward arrows',
	Attract: 'a figure with four arrows pointing at it',
	Decrepify: 'a bent old figure leaning on a stick',
	DeathMark: 'a skull inside crosshairs',
	LowerResist: 'a round ward cracked through the middle',
	SoulHarvest: 'a scythe blade gathering three wisps',
	Doom: 'a skull under a falling hourglass',
	LifeFromDeath: 'a small orb rising from a fallen skull',
	FueledByDeath: 'a winged boot over a bone',
	StandAlone: 'a lone robed figure inside a ring',
	SwiftHarvesting: 'a scythe with speed marks',
	CommanderOfTheRisenDead: 'a banner topped with a skull',
	ExtendedServitude: 'a chain with a link shaped like a skull',
	RigorMortis: 'a stiff outstretched skeletal hand, frost marks',
	OverwhelmingEssence: 'an orb brimming over its rim',
	DarkReaping: 'a scythe blade with a drop on its tip',
	SpreadingMalediction: 'three pentagons linked by lines',
	EternalTorment: 'an infinity sign made of thorned chain',
	FinalService: 'a skeleton kneeling before a robed figure',
	GrislyTribute: 'a cup held up under dripping drops',
	DrawLife: 'three wisps drawn into a heart',
	Serration: 'a bone blade with a saw edge',
	AberrantAnimator: 'a skeleton bristling with spikes',
	BloodIsPower: 'a drop inside a clock face',
	RathmasShield: 'a serpent coiled into a round shield',
};
const pages = ['summoning', 'poison-and-bone', 'curses', 'passive-skills'];
const pageNames = ['Summoning', 'Poison & Bone', 'Curses', 'Passive Skills'];
const levels = [1, 6, 12, 18, 24, 30];
const slug = n => n.toLowerCase().replace(/'/g, '').replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
let glyphs = '';
pages.forEach((pg, pi) => {
	glyphs += `\n### ${pageNames[pi]} (strip frames ${pi * 18}-${pi * 18 + 17})\n\n| # | Path | Skill | Kind | ${pi === 3 ? 'Row' : 'Level'} | Glyph subject |\n|---|---|---|---|---|---|\n`;
	rows.forEach((r, i) => {
		if (r[3] !== pi) return;
		if (!subjects[r[0]]) throw new Error('no subject for ' + r[0]);
		glyphs += `| ${i} | \`glyphs/necromancer/${pg}/${slug(r[1])}.png\` | ${r[1]} | ${r[6]} | ${pi === 3 ? r[4] + 1 : levels[r[4]]} | ${subjects[r[0]]} |\n`;
	});
});

const families = {
	wands: ['Bone Wand', 'Grave Wand', 'Yew Wand', 'Tomb Wand', 'Petrified Wand', 'Grim Wand', 'Lich Wand', 'Unholy Wand'],
	scythes: ['Reaping Scythe', 'Bone Scythe', 'Grave Scythe', 'War Scythe', 'Dread Scythe', 'Harvest Scythe', 'Doom Scythe', 'Deathbringer'],
	heads: ['Preserved Head', 'Zombie Head', 'Fetish Head', 'Gargoyle Head', 'Demon Head', 'Unraveller Head', 'Overseer Head', 'Bloodlord Skull'],
};
const familyLevels = [1, 5, 10, 15, 20, 26, 33, 40];
let bases = '';
for (const [fam, names] of Object.entries(families)) {
	bases += `\n### ${fam[0].toUpperCase() + fam.slice(1)}\n\n| # | Base | Level | Icon cells | Notes |\n|---|---|---|---|---|\n`;
	names.forEach((n, i) => { bases += `| ${i + 1} | ${n} | ${familyLevels[i]} | ${fam === 'scythes' ? '2 x 3' : (fam === 'wands' ? '1 x 3' : '2 x 2')} | ${i < 3 ? 'plain, worn' : i < 6 ? 'finer, marked' : 'ornate, the family at its height'} |\n`; });
}
const doc = `# Diablo Orcl — Request for Assets 17

> **STATUS: DRAFT, 2026-09-17. Batches 37 and 38 can go out now. HOLD batch 39 (the glyphs) until the user has
> settled the skill ledger in *The Road to Necromancy* - then regenerate this file with
> \`necro_rfa17.js\` so the table matches the final rows.**

**Art for the seventh hero, the Necromancer** (plan: \`.ProjectDocumentation/01-Project-Overview/Plan - The Necromancer.md\`).
He is in the game since v1.12.030: the Sorcerer's body dyed dark green, ash-skinned, with a bone staff and
blood-red boots. Everything below has a placeholder today; this art replaces the placeholders and nothing else changes.

- **Batch 37** - his face: a hero-select portrait and an inventory silhouette
- **Batch 38** - missiles, effects and curse markers
- **Batch 40** - the 24 item bases of the three families (icons and ground tumbles)
- **Batch 39** - 72 skill glyphs *(on hold, see above)*

Everything referenced here is this project's own art. See the delivered packs in \`Resources/02. Oracooll Assets/delivered-packs/\`.

---

## BATCH 37 — Portrait and silhouette (folder \`batch-37-necromancer-face\`)

| # | Path | Canvas | What it shows |
|---|---|---|---|
| 1 | \`ui_art/hero6.png\` | **180 × 76**, opaque | The hero-select portrait, same framing, lighting and painted style as \`ui_art/hero5.png\` (the Barbarian): head and shoulders, three-quarter view. A gaunt, ash-pale man, white hair, sunken eyes; a high dark-green collar with bone clasps; a cold green rim light. No text, no frame. |
| 2 | \`ui/silhouette_necromancer.png\` | **about 248 × 356**, real alpha | The figure behind the inventory's equipment slots, in the exact manner of \`ui/silhouette_sorcerer.png\` and \`ui/silhouette_monk.png\` (same height, same flat tone, same soft edge): a robed man standing, arms slightly out, a staff-less pose so the hand slots read. Hooded or bare-headed with long hair - it must not be mistaken for the Sorcerer's turbaned outline. |

Deliver a \`-preview.png\` of each beside the file it imitates.

---

## BATCH 38 — Missiles, effects and curse markers (folder \`batch-38-necromancer-missiles\`)

**Format: identical to RfA-16 batch 35.** Animated sprite sheets; frames in one horizontal strip; directional sheets
are 16 strips stacked from **South, clockwise**; **binary alpha** with a darkest-shade outline; palette ramps
128-255 only and **no green ramp (152-159)**; no text, drop shadows or glow outside the cell. Deliver a preview
beside each sheet and one \`notes.txt\` with cell size, frame count and direction count.

Bone is ivory to grey-brown (ramps 200-207 and 240-255). **Poison is NOT green** - the palette has none. Poison
here is a sickly yellow-brown, as the delivered \`missiles/acid_cloud.png\` and \`missiles/acid_javelin.png\` are; match them.

| # | Path | Skill | Directions × frames | Cell | What it shows |
|---|---|---|---|---|---|
| 1 | \`missiles/bone_tooth.png\` | Teeth, Bone Splinters | 16 × 1 | 32 × 32 | one barbed tooth of bone in flight, point first |
| 2 | \`missiles/bone_spear.png\` | Bone Spear | 16 × 1 | 96 × 96 | a long spear of fused bone, barbed head, a faint pale trail |
| 3 | \`missiles/bone_spirit.png\` | Bone Spirit | 16 × 6 | 64 × 64 | a skull with a ragged ghostly tail, the tail rippling over 6 frames, loopable |
| 4 | \`missiles/bone_hit.png\` | every bone skill's impact | 1 × 8 | 64 × 64 | a burst of bone chips and dust, gone by the last frame |
| 5 | \`missiles/bone_wall.png\` | Bone Wall, Bone Prison | 1 × 10 | 64 × 96 | one tile-wide segment of upright bones erupting from the floor (frames 1-6), then standing (7-10 loop). Drawn at the game's angle so segments tile side by side |
| 6 | \`missiles/bone_spikes.png\` | Bone Spikes | 1 × 10 | 96 × 96 | three spikes of bone stabbing up out of the floor and sinking back |
| 7 | \`missiles/bone_armor_shell.png\` | Bone Armor | 1 × 12 | 96 × 128 | three long bones orbiting an empty centre at hero height - the hero is drawn inside it, as with \`missiles/ice_armor_shell.png\`; loopable |
| 8 | \`missiles/bone_storm.png\` | Bone Storm | 1 × 12 | 160 × 128 | a wide flat whirl of bone shards around an empty centre, loopable |
| 9 | \`missiles/poison_bolt.png\` | Poison Nova, Blight | 16 × 1 | 32 × 32 | a small teardrop of yellow-brown venom with a short trail |
| 10 | \`missiles/corpse_explosion.png\` | Corpse Explosion, Death Mark | 1 × 12 | 160 × 128 | a body bursting from the floor centre: a dark red burst with bone fragments thrown outward, then a settling mist |
| 11 | \`missiles/raise_dead.png\` | Raise Skeleton, Raise Skeletal Mage, Revive | 1 × 12 | 96 × 128 | a column of pale wisps rising from a point on the floor and thinning out - played over the corpse as the minion stands up |
| 12 | \`missiles/curse_cast.png\` | every curse | 1 × 12 | 192 × 128 | a flat ring of dark violet script opening outward on the floor from the centre and fading - the area a curse lands on |

### Curse markers - small signs drawn over a cursed monster's head

One sheet: \`ui/curse_markers.png\`, **a strip of 14 cells, each 24 × 24**, binary alpha, dark outline, each a
different simple sigil in a different hue so the curse can be told at a glance in a crowd. In this order:

| Cell | Curse | Sigil | Hue |
|---|---|---|---|
| 0 | Amplify Damage | broken sword | red |
| 1 | Dim Vision | closed eye | grey-blue |
| 2 | Weaken | drooping blade | pale yellow |
| 3 | Frailty | cracked bone | ivory |
| 4 | Iron Maiden | spiked ring | steel blue |
| 5 | Terror | screaming mouth | orange |
| 6 | Bane | thorned drop | yellow-brown |
| 7 | Confuse | spiral | violet |
| 8 | Life Tap | heart with a drop | pink-red |
| 9 | Attract | four inward arrows | gold |
| 10 | Decrepify | bent figure | brown |
| 11 | Death Mark | skull in crosshairs | white |
| 12 | Lower Resist | cracked ward | cyan-blue |
| 13 | Doom | hourglass | dark violet |

### Not requested

Skeletons, mages, golems and the Revived are the game's own monster sprites (recoloured where needed). No minion
bodies are asked for.

---

## BATCH 40 — The 24 item bases (folder \`batch-40-necromancer-bases\`)

**Art request B of the plan.** Each base needs an inventory ICON and a ground TUMBLE, in the format of RfA-14 (\`RfA-14-reference\`)
and the delivered \`unqbase\` icons: icon cells of 28 x 28 px, PNG, RGBA, on the item's own transparent ground; the tumble a
13-frame drop strip like \`RfA-14 - Amulet Ground Tumble.md\` describes. Wands are one-handed and short (a forearm), scythes
two-handed and tall, heads fist-sized with the jaw and hair showing. Bone is ivory to grey-brown; nothing green.

Until these arrive the game shows a mace for a wand, an axe for a scythe and a small shield for a head.
${bases}
---

## BATCH 39 — 72 skill glyphs (folder \`batch-39-necromancer-glyphs\`) — ON HOLD

**Format: identical to RfA-13 (batch 31). It is strict and machine-checked** by \`tools/BuildGlyphStrips.ps1\`:

| Rule | Requirement |
|---|---|
| Canvas | **56 × 56** PNG, RGBA |
| Colours | exactly two: white \`RGB(243,243,243)\` and shadow \`RGB(12,7,7)\` |
| Alpha | binary |
| Shadow | offset **(−2, −1)** |
| Never | greys, colour, anti-aliasing, plate or frame |

One bold pictogram per skill, readable at native size, not to be mistaken for any other glyph on its page.
**#** is the skill's frame in his strip. Deliver a contact sheet per page.
${glyphs}`;
const out = 'C:/Users/hroga/OneDrive/2. Personal Files/Software/Diablo/Resources/02. Oracooll Assets/ChatGPT RfA/RfA-17 - The Necromancer (draft).md';
fs.writeFileSync(out, doc);
console.log('written', doc.split('\n').length, 'lines');
