#!/usr/bin/env node
/**
 * tools/GenVanillaSkillSounds.js - every skill cue played from the player's own diabdat.mpq / hellfire.mpq.
 *
 * User, 2026-09-27: "remove all chatgpt sounds from the game. they are no good. replace with vanilla sounds per your
 * decision." The delivered WAVs (the class-skill-sounds package, RfA-02/03/04/19/20/27) are gone; each cue SLOT they
 * filled - tools/skill_sound_slots.csv, the (skill, event, class, page) of all 529 - now names a vanilla file, which the
 * engine loads from the player's archives at runtime exactly as it loads its own sounds. Nothing of Blizzard's is
 * packed.
 *
 * The choice is a rule per class page and event (fire spells cast with Firebolt and land with its impact...), refined by
 * keywords in the skill's name (Nova, Teleport, bone, poison, holy...), and the Barbarian's war cries each take a
 * monster's roar. Aura LOOPS are dropped: vanilla has no seamless two-second loops, and a one-shot repeating under the
 * hero would be worse than silence; the aura keeps its start and stop cues.
 *
 * The user's own picks come first (tools/skill_sound_picks.json, from the review page's choices, 2026-09-28): a slot
 * named there plays that file, or nothing for null, whatever the rules below would pick.
 *
 * Outputs:
 *   Source/oracool/skill_sounds_data.inc      the table the game reads (SkillSound rows, vanilla paths)
 *   <review.json>                             every placement, for the review page (optional third argument)
 *
 * Usage: node tools/GenVanillaSkillSounds.js tools/skill_sound_slots.csv Source/oracool/skill_sounds_data.inc [review.json]
 */
'use strict';
const fs = require('fs');
const path = require('path');

const [slotsPath, outPath, reviewPath] = process.argv.slice(2);
if (!outPath) {
	console.error('usage: node GenVanillaSkillSounds.js <slots.csv> <out.inc> [review.json]');
	process.exit(1);
}

// ---- the vanilla sounds used, by a short name ------------------------------------------------------------------------
const V = {
	cast2: 'sfx\\misc\\cast2.wav', cast4: 'sfx\\misc\\cast4.wav', cast6: 'sfx\\misc\\cast6.wav',
	cast7: 'sfx\\misc\\cast7.wav', cast8: 'sfx\\misc\\cast8.wav',
	swing: 'sfx\\misc\\swing.wav', swing2: 'sfx\\misc\\swing2.wav', bowShot: 'sfx\\misc\\bfire.wav',
	fireArrow: 'sfx\\misc\\fballbow.wav', arrowHit: 'sfx\\misc\\sting1.wav',
	firebolt: 'sfx\\misc\\fbolt1.wav', fireHit: 'sfx\\misc\\firimp2.wav', flameWave: 'sfx\\misc\\flamwave.wav',
	lightning: 'sfx\\misc\\lning1.wav', chargedBolt: 'sfx\\misc\\cbolt.wav', lightningHit: 'sfx\\misc\\elecimp1.wav',
	nova: 'sfx\\misc\\nova.wav', elemental: 'sfx\\misc\\elementl.wav', apocalypse: 'sfx\\misc\\apoc.wav',
	holyBolt: 'sfx\\misc\\holybolt.wav', manaShield: 'sfx\\misc\\mshield.wav', teleport: 'sfx\\misc\\teleport.wav',
	ethereal: 'sfx\\misc\\ethereal.wav', infravision: 'sfx\\misc\\infravis.wav', invisible: 'sfx\\misc\\invisibl.wav',
	resurrect: 'sfx\\misc\\resur.wav', golem: 'sfx\\misc\\golum.wav', guardian: 'sfx\\misc\\guard.wav',
	boneSpirit: 'sfx\\misc\\bonesp.wav', boneHit: 'sfx\\misc\\bsimpct.wav', acid: 'sfx\\misc\\acids1.wav',
	acidSplash: 'sfx\\misc\\acids2.wav', bloodStarHit: 'sfx\\misc\\blsimpt.wav', doomSerpents: 'sfx\\misc\\dserp.wav',
	stoneCurse: 'sfx\\misc\\scurimp.wav', shatter: 'sfx\\misc\\shatter.wav', explosion: 'sfx\\misc\\nestxpld.wav',
	townPortal: 'sfx\\misc\\sentinel.wav', shrine: 'sfx\\misc\\gshrine.wav', fountain: 'sfx\\misc\\fountain.wav',
	questDone: 'sfx\\misc\\questdon.wav', cauldron: 'sfx\\misc\\caldron.wav',
	readBook: 'sfx\\items\\readbook.wav', grab: 'sfx\\items\\invgrab.wav', staffClack: 'sfx\\items\\invstaf.wav',
	shieldClank: 'sfx\\items\\invshiel.wav', heavyArmour: 'sfx\\items\\invharm.wav', anvil: 'sfx\\items\\invanvl.wav',
	magic: 'sfx\\items\\magic.wav', magic1: 'sfx\\items\\magic1.wav', gem: 'sfx\\items\\invrock.wav',
	scroll: 'sfx\\items\\invscrol.wav', lidOpen: 'sfx\\items\\cropen.wav',
	// Monster voices, for the Barbarian's war cries: monsters\<folder>\<name><a|h|d|s><1|2>.wav.
	hornedDemon: 'monsters\\rhino\\rhinoa1.wav', hornedDemon2: 'monsters\\rhino\\rhinoa2.wav',
	overlord: 'monsters\\fat\\fata1.wav', overlord2: 'monsters\\fat\\fata2.wav',
	blackKnight: 'monsters\\black\\blacka1.wav', blackKnight2: 'monsters\\black\\blacka2.wav',
	skeletonKing: 'monsters\\sking\\skinga1.wav', skeletonKing2: 'monsters\\sking\\skinga2.wav',
	slayer: 'monsters\\mega\\megaa1.wav', diablo: 'monsters\\diablo\\diabloa1.wav', diablo2: 'monsters\\diablo\\diabloa2.wav',
	gargoyle: 'monsters\\gargoyle\\gargoa1.wav', butcher: 'monsters\\fatc\\fatca1.wav', fallen: 'monsters\\falspear\\phalla1.wav',
};

// ---- the rules -------------------------------------------------------------------------------------------------------
// Each page: a default per event, then keyword overrides - [regex on the skill name, { event: sound }]. First match wins.
const Pages = {
	'paladin/combat-skills': {
		Cast: 'swing2', Impact: 'bloodStarHit',
		by: [
			[/Blessed Hammer/, { Cast: 'cast6', Impact: 'holyBolt' }],
			[/Blessed Shield|Aegis/, { Cast: 'swing', Impact: 'shieldClank' }],
			[/Fist of the Heavens|Wrath of the Heavens/, { Cast: 'lightning', Impact: 'lightningHit' }],
			[/Heaven's Descent/, { Cast: 'holyBolt', Impact: 'nova' }],
			[/Holy Lance|Judgment|Oathbrand|Votive/, { Impact: 'holyBolt' }],
			[/Conversion/, { Cast: 'cast8', Impact: 'infravision' }],
			[/Sacrifice/, { Impact: 'acidSplash' }],
			[/Vengeance/, { Cast: 'cast8', Impact: 'fireHit' }],
		],
	},
	'paladin/offensive-auras': {
		Start: 'cast8', Stop: 'invisible',
		by: [
			[/Holy Fire|Tithe of Ash|Doom Procession/, { Start: 'flameWave' }],
			[/Holy Freeze/, { Start: 'shatter' }],
			[/Holy Shock/, { Start: 'lightningHit' }],
			[/Thorns|Retaliation/, { Start: 'arrowHit' }],
			[/Sanctuary|Radiance|Bane of Evil/, { Start: 'holyBolt' }],
			[/Conviction|Condemnation|Dominion/, { Start: 'doomSerpents' }],
		],
	},
	'paladin/defensive-auras': {
		Start: 'manaShield', Stop: 'invisible',
		by: [
			[/Prayer|Meditation|Mercy|Redemption|Cleansing/, { Start: 'fountain' }],
			[/Salvation|Sanctity|Warding Light/, { Start: 'holyBolt' }],
		],
	},
	'sorcerer/fire-spells': {
		Cast: 'firebolt', Impact: 'fireHit', Start: 'firebolt', Stop: 'invisible',
		by: [
			[/Fire Wall|Blaze|Flame Ring|Inferno/, { Cast: 'flameWave' }],
			[/Meteor|Funeral Star/, { Cast: 'cast4', Impact: 'apocalypse' }],
			[/Hydra/, { Cast: 'elemental' }],
			[/Ember Mine/, { Cast: 'cast4', Impact: 'explosion' }],
		],
	},
	'sorcerer/cold-spells': {
		Cast: 'stoneCurse', Impact: 'shatter', Start: 'manaShield', Stop: 'shatter',
		by: [
			[/Frost Nova|Absolute Zero|Whiteout/, { Cast: 'nova' }],
			[/Blizzard/, { Cast: 'cast4' }],
			[/Frozen Orb/, { Cast: 'elemental' }],
			[/Frozen Sentinel/, { Cast: 'townPortal' }],
		],
	},
	'sorcerer/lightning-spells': {
		Cast: 'lightning', Impact: 'lightningHit', Start: 'manaShield', Stop: 'invisible',
		by: [
			[/Charged Bolt|Static Charge/, { Cast: 'chargedBolt' }],
			[/Nova|Faraday Ring|Storm Crucible/, { Cast: 'nova' }],
			[/Teleport|Ride the Lightning/, { Cast: 'teleport', Impact: 'teleport' }],
			[/Telekinesis/, { Cast: 'cast2' }],
			[/Static Field/, { Cast: 'elemental' }],
			[/Conduit/, { Cast: 'cast4' }],
			[/Thunder Storm/, { Start: 'lightning' }],
		],
	},
	'rogue/bow-and-crossbow': {
		Cast: 'bowShot', Impact: 'arrowHit',
		by: [
			[/Fire Arrow|Exploding Arrow|Immolation Arrow/, { Cast: 'fireArrow', Impact: 'fireHit' }],
			[/Cold Arrow|Ice Arrow|Freezing Arrow/, { Impact: 'shatter' }],
			[/Shock Arrow/, { Impact: 'lightningHit' }],
			[/Magic Arrow|Guided Arrow|Phantom Volley/, { Cast: 'cast2', Impact: 'bloodStarHit' }],
			[/Hunter's Mark/, { Cast: 'infravision' }],
		],
	},
	'rogue/javelin-and-spear': {
		Cast: 'swing2', Impact: 'arrowHit',
		by: [
			[/Lightning|Charged Strike/, { Cast: 'lightning', Impact: 'lightningHit' }],
			[/Plague|Poison/, { Impact: 'acid' }],
			[/Valkyrie's Spear/, { Cast: 'holyBolt' }],
		],
	},
	'rogue/passive-and-magic': {
		Cast: 'cast8', Arrive: 'guardian',
		by: [
			[/Decoy/, { Arrive: 'invisible' }],
			[/Inner Sight|Hunter's Claim/, { Cast: 'infravision' }],
			[/Slow Missiles/, { Cast: 'ethereal' }],
			[/Shadow Step/, { Cast: 'teleport' }],
		],
	},
	'monk/way-of-the-body': {
		Cast: 'swing', Impact: 'bloodStarHit',
		by: [
			[/Dragon's Wrath/, { Cast: 'flameWave' }],
			[/Purifying Breath/, { Cast: 'cast6' }],
			[/Seven-Sided Strike/, { Cast: 'teleport' }],
			[/Exploding Palm/, { Impact: 'explosion' }],
			[/Leaping Crane|Shoulder Gate|Whirling Kick/, { Cast: 'swing2' }],
		],
	},
	'monk/way-of-the-spirit': {
		Cast: 'cast6', Impact: 'holyBolt', Start: 'manaShield', Stop: 'invisible', Arrive: 'guardian',
		by: [
			[/Wave of Light/, { Cast: 'holyBolt', Impact: 'nova' }],
			[/Chi Wave/, { Cast: 'elemental' }],
			[/Inner Sight/, { Cast: 'infravision' }],
			[/Temple Bell/, { Cast: 'shrine' }],
			[/Serenity|Tranquility|Healing Mantra/, { Cast: 'fountain', Start: 'fountain' }],
			[/Mantra/, { Cast: 'cast8' }],
			[/Astral Projection/, { Cast: 'ethereal' }],
			[/Blinding Flash|Radiant Palm/, { Cast: 'holyBolt' }],
		],
	},
	'monk/way-of-the-staff': {
		Cast: 'swing2', Impact: 'staffClack',
		by: [
			[/Bamboo Rain|Thousand Reeds|Seven Reeds/, { Cast: 'swing' }],
			[/Heaven Splitter|Wheel of Heaven/, { Impact: 'lightningHit' }],
		],
	},
	'bard/harmony': {
		Cast: 'cast7', Start: 'shrine', Stop: 'invisible',
		by: [
			[/Sound Shock/, { Cast: 'nova' }],
			[/Shout/, { Cast: 'overlord' }],
			[/Sonic Barrier/, { Start: 'manaShield' }],
		],
	},
	'bard/melody': { Cast: 'cast7', Start: 'shrine', Stop: 'invisible', by: [[/Melody of Life/, { Start: 'fountain' }]] },
	'bard/poetry': { Cast: 'cast7', Start: 'shrine', Stop: 'invisible', Arrive: 'guardian', by: [] },
	'necromancer/curses': {
		Cast: 'doomSerpents', Impact: 'resurrect',
		by: [[/Life Tap/, { Cast: 'acidSplash' }], [/Terror/, { Cast: 'diablo2' }]],
	},
	'necromancer/poison-and-bone': {
		Cast: 'boneSpirit', Impact: 'boneHit',
		by: [
			[/Poison|Blight|Decompose/, { Cast: 'acid', Impact: 'acidSplash' }],
			[/Corpse Explosion/, { Cast: 'cast4', Impact: 'explosion' }],
			[/Death Nova|Poison Nova/, { Cast: 'nova' }],
			[/Bone Armor/, { Cast: 'manaShield' }],
		],
	},
	'necromancer/summoning': {
		Cast: 'resurrect', Impact: 'resurrect',
		by: [
			[/Golem/, { Cast: 'golem' }],
			[/Dark Mending|Unholy Offering/, { Cast: 'cast8' }],
			[/Frenzy of the Dead/, { Cast: 'cast4' }],
		],
	},
	'barbarian/combat-skills': {
		Cast: 'swing', Impact: 'bloodStarHit',
		by: [
			[/Double Swing|Frenzy|Whirlwind|Leap/, { Cast: 'swing2' }],
			[/Earthquake|Ground Stomp|Seismic Slam|Hammer of the Ancients/, { Cast: 'swing2', Impact: 'explosion' }],
			[/Weapon Throw/, { Impact: 'arrowHit' }],
			[/Bash|Stun|Backhand/, { Impact: 'heavyArmour' }],
		],
	},
	'barbarian/warcries': {
		Cast: 'overlord', Arrive: 'guardian',
		by: [
			[/^Howl$/, { Cast: 'hornedDemon' }],
			[/^Taunt$/, { Cast: 'fallen' }],
			[/^Shout$/, { Cast: 'blackKnight' }],
			[/^Battle Cry$/, { Cast: 'overlord' }],
			[/^Battle Orders$/, { Cast: 'skeletonKing' }],
			[/^Battle Command$/, { Cast: 'slayer' }],
			[/^War Cry$/, { Cast: 'diablo' }],
			[/^Earthshaker Cry$/, { Cast: 'diablo2' }],
			[/^Threatening Shout$/, { Cast: 'gargoyle' }],
			[/^Rallying Cry$/, { Cast: 'overlord2' }],
			[/^Intimidate$/, { Cast: 'hornedDemon2' }],
			[/^Split Ranks$/, { Cast: 'blackKnight2' }],
			[/^Iron Will$/, { Cast: 'skeletonKing2' }],
			[/^Bloodcall$/, { Cast: 'butcher' }],
			[/^Grim Ward$/, { Cast: 'golem' }],
			[/^Find (Item|Potion)$/, { Cast: 'grab' }],
			[/^Ancestral Call$/, { Cast: 'golem', Arrive: 'guardian' }],
		],
	},
	'barbarian/combat-masteries': { by: [] },
};

// ---- the user's picks: "<class>.<Skill>.<Event>" -> vanilla path, or null for silence ------------------------------------
const Picks = JSON.parse(fs.readFileSync(path.join(__dirname, 'skill_sound_picks.json'), 'utf8')).picks;
const pickKey = slot => `${slot.class}.${slot.skill}.${slot.event}`;

/** @brief The vanilla sound for one slot, or null for none (an aura's loop). */
function pick(slot) {
	if (slot.event === 'Learn')
		return 'readBook'; // a point spent is a page read: every class, every passive
	if (slot.event === 'Loop')
		return null;
	const page = Pages[`${slot.class}/${slot.page}`];
	if (!page)
		throw new Error(`no rule for ${slot.class}/${slot.page}`);
	for (const [re, events] of page.by) {
		if (re.test(slot.name) && events[slot.event])
			return events[slot.event];
	}
	if (page[slot.event])
		return page[slot.event];
	throw new Error(`no ${slot.event} rule for ${slot.name} (${slot.class}/${slot.page})`);
}

// ---- run -------------------------------------------------------------------------------------------------------------
const lines = fs.readFileSync(slotsPath, 'utf8').split(/\r?\n/).filter(Boolean);
const slots = lines.slice(1).map(l => {
	const [skill, event, cls, page, name] = l.split(',');
	return { skill, event, class: cls, page, name };
});
const rows = [];
const review = [];
const pathOf = new Map(); // slot -> the file it plays
for (const slot of slots) {
	const chosen = Picks[pickKey(slot)];
	const key = chosen ? null : pick(slot);
	if (key !== null && !V[key])
		throw new Error(`unknown sound ${key}`);
	const file = chosen ? chosen.path : key === null ? null : V[key];
	review.push({ ...slot, sound: chosen ? 'picked' : key, path: file });
	if (file !== null) {
		rows.push(slot);
		pathOf.set(slot, file);
	}
}
const unused = Object.keys(Picks).filter(k => !slots.some(s => pickKey(s) === k));
if (unused.length)
	throw new Error('picks naming no slot: ' + unused.join(', '));
const esc = p => p.replace(/\\/g, '\\\\');
let out = '// GENERATED by tools/GenVanillaSkillSounds.js - do not edit. Every cue is a VANILLA sound, loaded from the player\'s\n'
	+ '// diabdat.mpq / hellfire.mpq (user, 2026-09-27: "remove all chatgpt sounds from the game ... replace with vanilla\n'
	+ '// sounds per your decision"). The slots are tools/skill_sound_slots.csv; aura loops are left out on purpose.\n'
	+ '// The user\'s picks on the review page (tools/skill_sound_picks.json) come before the rules.\n'
	+ `//\n// ${rows.length} cues across 7 classes, from ${new Set(rows.map(r => pathOf.get(r))).size} vanilla sounds.\n\n`
	+ '// clang-format off\nconst SkillSound SkillSounds[] = {\n';
for (const r of rows)
	out += `\t{ Skill::${r.skill}, SkillSoundEvent::${r.event}, "${esc(pathOf.get(r))}" },\n`;
out += '};\n// clang-format on\n';
fs.writeFileSync(outPath, String.fromCharCode(0xFEFF) + out.split(String.fromCharCode(10)).join(String.fromCharCode(13, 10))); // CRLF, as the file has always been
console.log(`${rows.length} cues written, ${slots.length - rows.length} left silent, ${new Set(rows.map(r => pathOf.get(r))).size} vanilla sounds, ${Object.keys(Picks).length} user picks`);
if (reviewPath)
	fs.writeFileSync(reviewPath, JSON.stringify({ sounds: V, placements: review }, null, 1));
