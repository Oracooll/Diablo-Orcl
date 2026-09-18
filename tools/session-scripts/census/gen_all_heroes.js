// Builds the all-heroes skill ledger from rows.json (extracted from class_tree.cpp by rows.js).
const fs = require('fs');
const [rowsPath, outPath, version] = process.argv.slice(2);
const rows = JSON.parse(fs.readFileSync(rowsPath, 'utf8'));

const classes = {
	Pal: { name: 'Paladin', pages: ['Combat Skills', 'Offensive Auras', 'Defensive Auras'] },
	Bar: { name: 'Barbarian', pages: ['Combat Skills', 'Combat Masteries', 'Warcries'] },
	Sor: { name: 'Sorcerer', pages: ['Cold Spells', 'Lightning Spells', 'Fire Spells'] },
	Rog: { name: 'Rogue', pages: ['Bow & Crossbow', 'Passive & Magic', 'Javelin & Spear'] },
	Bard: { name: 'Bard', pages: ['Melody', 'Harmony', 'Poetry'] },
	Monk: { name: 'Monk', pages: ['Way of the Staff', 'Way of the Body', 'Way of the Spirit'] },
	Nec: { name: 'Necromancer', pages: ['Summoning', 'Poison & Bone', 'Curses'] },
};

// Built, but the row's meaning was changed so the engine could carry it.
const reworded = {
	'No Escape': 'Was "what you throw" - there are no thrown weapons; now works by distance.',
	'Spear Mastery': 'Was marked "no spear type"; reads the Spear and Pike bases instead.',
	'Blunt': 'Justice does not exist here; Blessed Hammer only.',
	'Towering Shield': 'Skills have no cooldowns, so "readies sooner" was dropped.',
	'Hold Your Ground': 'There is no dodge to give up for a Paladin; block chance only.',
	'Righteousness': 'Wrath is mana here.',
	'Insurmountable': 'Wrath is mana here.',
	'Wrathful': 'Wrath is mana here.',
	'Illusionist': 'No escape skills with cooldowns to reset; speed burst only.',
	'Galvanizing Ward': 'No shield object; the next blow is halved instead.',
	'Dominance': 'No shield object; damage reduction that stacks instead.',
	'Unstable Anomaly': 'Built as a once-a-minute death save that throws enemies back.',
	'Prodigy': '"Simple" spells defined as 6 mana or less.',
	'Arcane Dynamo': '"Simple" spells defined as 6 mana or less.',
	'Numbing Traps': 'There are no traps; anything chilled or frozen strikes weaker.',
	'Night Stalker': 'Hatred is mana here.',
	'Blood Vengeance': 'Hatred and discipline are both mana here.',
	'Sharpshooter': 'The Rogue had no critical blows; this adds one.',
	'Chorus': 'Allies are your summons (single player).',
	'Unity': 'Allies are your summons (single player).',
	'Rhythm': 'Songs have no beat to strike in time with; faster attacks while one plays.',
	'Stagecraft': 'Songs never broke on a hit; now blows do not stagger you while one plays.',
	'Reed in the Wind': 'Staff blocks cannot move you; block chance with a staff instead.',
	'Seize the Initiative': 'Attack speed cannot change per blow; damage against the unwounded instead.',
	'Alacrity': 'No separate spirit-builder speed; faster attacks always.',
	'Momentum': '"Ground covered" measured as 2 seconds on the move.',
	'Mythic Rhythm': 'Spirit builders and spenders are not split here; melee skill blows charge spell damage.',
	'The Guardian\'s Path': 'Spirit is mana here.',
	'Weapons Master': 'No spears or polearms as a type; staff for speed, mace for Rage.',
	// Built from the user's census notes (2026-09-14).
	'Weapon Throw': 'Replaces Double Throw, as the note asked. Uses the normal attack; the sword or axe spins through the air (RfA-16 art, v1.12.010).',
	'Throwing Mastery': 'Masters Weapon Throw, the single throw the note kept.',
	'Static Field': 'An aura pulsing like Holy Fire, as the note asked; strips a share of remaining life. Its own floor ring since v1.12.010.',
	'Thunder Storm': 'An aura like Holy Fire, as the note asked; a forked bolt falls on the struck enemy (RfA-16 art, v1.12.010).',
	'Meteor': 'A burning rock falls for a second, bursts, and the ground burns for three (RfA-16 art, v1.12.010).',
	'Decoy': 'The Golem\'s slot and brain, as the note suggested, wearing the Rogue\'s own sprites for her current gear in a blue ghost tint (v1.12.009).',
	'Poison Javelin': 'Acid instead of poison, as the note asked. An acid javelin flies and leaves a vapour pool (RfA-16 art, v1.12.010).',
	'Plague Javelin': 'An acid cloud instead of poison, as the note asked. Javelin and cloud are RfA-16 art (v1.12.010).',
	'Valkyrie': 'A Companion (v1.12.018): dark-gold Rogue archer, 30 s +5/lvl, 150 life to 1000 at lvl 20, resistances 90% at lvl 11, 50% of your damage +5%/lvl, Volley. Formation, focus fire, stances (J), walks aside, returns after level changes.',
	'Ancestral Call': 'A Companion (v1.12.018): the three Ancients together - Korlic (Leap), Talic (Whirlwind), Madawc (Hammer Toss) - 20 s +2/lvl, each 35% of your damage +3%/lvl. Was a 30-second Golem spirit.',
	'Spirit Guardian': 'A Companion (v1.12.018): a Monk guard that holds nearby enemies and taunts every 8 s, 30 s +5/lvl, 30% of your damage +3%/lvl. Was a 30-second Golem spirit.',
	'Decoy': 'A Companion (v1.12.018): a blue ghost of you that draws every enemy within 6 tiles and strikes no one, 15 s +1/lvl. Life and resistances grow with level.',
	'Custom Engineering': 'Built on Hellfire\'s trap runes, as the note asked: stronger runes that are sometimes not used up.',
	'Grenadier': 'No Magic Star missile exists in the engine; the grenade is a Fireball wearing a tumbling clay bomb (RfA-16 art, v1.12.010), bursting in the fire explosion.',
	// The Necromancer (2026-09-18, phases N5-N9): two Diablo III passives rewritten for this engine.
	'Life from Death': 'Was "health globes", which do not exist here: a monster that dies within six tiles heals a twenty-fifth of your life instead.',
	'Blood is Power': 'Was a cooldown reset; skills here have no cooldowns. Losing life feeds Essence instead, one point for every twenty-fifth of your life lost.',
};

// Classes hidden from the mod (oracool/hidden_classes.h) - their rows stay listed, marked as such.
const hiddenClasses = new Set(['Bard']);

function status(r) {
	if (hiddenClasses.has(r.cls)) return 'hidden';
	if (r.page === 'RetiredFromTreePage') return 'retired';
	if (r.impl) return reworded[r.name] ? 'reworded' : 'built';
	return 'engine';
}

function reason(r) {
	const d = r.desc.replace(/\\"/g, '"').replace(/\\'/g, "'");
	if (hiddenClasses.has(r.cls)) return 'The Bard is hidden from the mod (2026-09-14); the row is kept for when the class returns.';
	if (r.page === 'RetiredFromTreePage') return 'Taken off the page by you (a full page holds 18 passives). Kept in the table only because a row\'s position is its identity.';
	if (r.impl) return reworded[r.name] || '';
	const m = d.match(/(?:Inert|Not yet built):\s*(.*)$/);
	if (m) return m[1].charAt(0).toUpperCase() + m[1].slice(1);
	return 'Not built.';
}

function effect(r) {
	let d = r.desc.replace(/\\"/g, '"').replace(/\\'/g, "'");
	d = d.replace(/\s*(?:Inert|Not yet built)[:.].*$/, '');
	return d;
}

// The character level a row first opens at - class_tree.cpp's IsClassTreeSkillUnlocked:
// Passive Skills rows by their place on the page (PassiveSkillRequiredLevel, 2 per cell),
// borrowed Paladin rows by paladin_skills.cpp's minLevel, every other row by its tier (TierLevels).
// Each further rank of an active or aura needs one level more (RankRequiredLevel).
const TierLevels = [1, 6, 12, 18, 24, 30, 36];
const PaladinBorrowed = {
	'Smite': { level: 1, shield: true },
	'Zeal': { level: 6 },
	'Charge': { level: 6 },
	'Hammer of Faith': { level: 12 },
	'Blessed Hammer': { level: 18 },
	'Blessed Shield': { level: 18, shield: true },
	'Fist of the Heavens': { level: 30 },
};

function level(r) {
	if (r.page === 'RetiredFromTreePage') return { lv: '—', lvn: 'Off the page' };
	if (+r.page === 3) return { lv: String(2 * (r.tier * 3 + r.col + 1)), lvn: 'Passive: learned at this level, then slotted' };
	const borrowed = r.cls === 'Pal' ? PaladinBorrowed[r.name] : undefined;
	if (borrowed) return { lv: borrowed.level + (borrowed.shield ? ' + shield' : ''), lvn: 'Paladin skill: its own level gate' + (borrowed.shield ? ', and a shield in hand' : '') };
	return { lv: String(TierLevels[r.tier]), lvn: 'Tier ' + (r.tier + 1) + '; each further rank needs one level more' };
}

function pageName(r) {
	if (r.page === 'RetiredFromTreePage') return 'Off the page';
	const p = +r.page;
	if (p === 3) return 'Passive Skills';
	return classes[r.cls].pages[p] || ('Page ' + (p + 1));
}

const data = rows.map(r => ({
	c: r.cls,
	n: r.name.replace(/\\'/g, "'"),
	p: pageName(r),
	pi: r.page === 'RetiredFromTreePage' ? 9 : +r.page,
	t: r.tier,
	col: r.col,
	...level(r),
	k: r.kind,
	s: status(r),
	e: effect(r),
	w: reason(r),
}));

const counts = { built: 0, reworded: 0, engine: 0, retired: 0, hidden: 0 };
for (const d of data) counts[d.s]++;

const html = fs.readFileSync(__dirname + '/all_heroes.template.html', 'utf8')
	.replace('/*DATA*/', JSON.stringify(data))
	.replace('/*CLASSES*/', JSON.stringify(Object.fromEntries(Object.entries(classes).map(([k, v]) => [k, v.name]))))
	.replace(/__VERSION__/g, version)
	.replace('__TOTAL__', String(data.length))
	.replace('__BUILT__', String(counts.built))
	.replace('__REWORDED__', String(counts.reworded))
	.replace('__ENGINE__', String(counts.engine))
	.replace('__RETIRED__', String(counts.retired))
	.replace('__HIDDEN__', String(counts.hidden));
fs.writeFileSync(outPath, html);
console.log('rows', data.length, JSON.stringify(counts));
