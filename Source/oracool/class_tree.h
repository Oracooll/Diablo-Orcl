/**
 * @file oracool/class_tree.h
 *
 * Oracool: Diablo II's class skill trees - three pages of ten (nine for the Paladin), for four of
 * this game's classes.
 *
 * Grew out of oracool/paladin_tree, which was written for one class when only the Paladin's sheet
 * had arrived. Three more followed the same afternoon (Barbarian, Sorceress, Rogue), all in the
 * same format, so the page machinery generalized rather than being copied three times. The
 * Paladin's behaviour is unchanged and its tests still pin it.
 *
 * ## One enum, four classes
 *
 * Every tree skill in the game is one value of PaladinTreeSkill's successor, ClassTreeSkill, and
 * each row carries the class it belongs to. That keeps investment, persistence and the UI working
 * on a single index type instead of four parallel ones. A class's own skills are contiguous, and
 * ClassTreeIconIndex is a skill's position WITHIN its class - which is also its position in that
 * class's icon strip, so the art and the table cannot drift.
 *
 * ## A fourth page, from Diablo III
 *
 * Added 2026-08-25 at the user's request: a Passive Skills sheet per class, in Diablo III's shape -
 * always on, one rank, bought rather than levelled (maxRank 1, Kind::Passive). Five classes take
 * their D3 counterpart's list verbatim: Paladin from the Crusader, Barbarian from the Barbarian,
 * Sorceress from the Wizard, Rogue from the Demon Hunter, Monk from the Monk. Diablo III has no
 * Bard, so hers are authored in the same idiom - as her three song pages already are.
 *
 * Every one of them shipped INERT (`implemented` false), which was the whole point: named
 * placeholders whose effects would arrive later. 41 of the 110 have since been built; the rest are
 * still inert. The UI draws an inert row greyed with a red X and refuses to invest in it, so no
 * point can be sunk into one by mistake. The art has all arrived: every row, inert or not, has its
 * frame in its class's icon strip, so the empty plate for a missing frame is only a fallback now.
 *
 * D3's own unlock levels (10 to 70) are NOT reproduced; they are laid out three to a tier down this
 * game's existing ladder, in D3's order. Same reasoning as the prerequisite graph below: borrowing
 * the shape without inventing the numbers.
 *
 * ## Gating: level tiers, not a prerequisite graph
 *
 * Every skill sits in one of Diablo II's six tiers - character level 1, 6, 12, 18, 24, 30 - and
 * the tier is the whole gate. D2 also has a per-skill prerequisite graph including cross-tree
 * links; it is deliberately NOT reproduced, because rebuilding it from memory would mean inventing
 * edges and presenting them as D2's. See the same note in the Paladin's original module.
 *
 * ## Investment: one accessor over two stores
 *
 * Points come from Phase 2.1's pool (oracool/skill_points.h). Where a tree skill has a SpellID its
 * investment lives in Player::_pSkillInvestment keyed by that SpellID, which means it flows into
 * Player::GetSpellLevel and therefore into every ladder that already scales with spell level - for
 * free. That is what makes the Sorceress page mostly a wiring job: this engine already HAS Fire
 * Bolt, Fireball, Fire Wall, Inferno, Lightning, Chain Lightning, Nova, Charged Bolt, Teleport,
 * Telekinesis, Mana Shield and Guardian, and investing in the tree raises them.
 *
 * Skills without a SpellID (every aura, every passive, and the actives whose mechanics do not
 * exist yet) use Player::_pClassTreeInvestment, indexed by position-within-class and persisted by
 * the HeroChunkClassTree chunk. ClassTreeInvestment() hides which store a skill uses; nothing
 * outside this module should reach for either array.
 *
 * ## Honesty about what the engine cannot do
 *
 * Each row's description states D2's effect. Where this engine has no channel for it - there is no
 * cold damage, no chill, no poison duration, no stamina, no monster-facing aura pass - the skill is
 * listed, described, and INERT rather than approximated, `implemented` says so, and the UI draws it
 * greyed. A test asserts the inert ones contribute nothing.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "player.h"
#include "spelldat.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution {

struct Player;

namespace oracool {

struct ItemBonusTotals;

/**
 * @brief Every tree skill in the game, grouped by class and, within a class, in icon-strip order.
 *
 * A class's block is contiguous and starts at its FIRST_* marker; ClassTreeIconIndex is the offset
 * from that marker, which is the index into that class's strip.
 */
// uint16_t since the Passive Skills page (2026-08-25) took the tree past 255 rows. It was uint8_t
// with None at 0xFF, and at 273 skills 0xFF became a real Rogue row - so the sentinel and the
// underlying type had to grow together. Player::_pOracoolActiveAura, which stores one of these,
// grew with it; see the note there and in hero_chunks.cpp's HeroChunkActiveAura.
enum class ClassTreeSkill : uint16_t {
	// ---------------- Paladin: Combat Skills ----------------
	Sacrifice,
	FIRST = Sacrifice,
	PALADIN_FIRST = Sacrifice,
	Smite,
	Zeal,
	Charge,
	Vengeance,
	BlessedHammer,
	Conversion,
	FistOfTheHeavens,
	// ---------------- Paladin: Offensive Auras ----------------
	Might,
	HolyFire,
	Thorns,
	BlessedAim,
	Concentration,
	HolyFreeze,
	HolyShock,
	Sanctuary,
	Fanaticism,
	Conviction,
	// ---------------- Paladin: Defensive Auras ----------------
	Prayer,
	ResistFire,
	Defiance,
	ResistCold,
	Cleansing,
	ResistLightning,
	Vigor,
	Meditation,
	Redemption,
	Salvation,
	// ---------------- Paladin: Combat Skills, appended ----------------
	// Two Combat Skills rows living at the END of the Paladin block rather than beside their
	// page-mates. Their `page` field is what puts them on the Combat Skills sheet - the table's
	// grouping is for reading, and BuildClassTreePage sorts by tier and column regardless.
	//
	// Appended because a skill's POSITION in its class block is its icon-strip frame AND its slot in
	// Player::_pClassTreeInvestment. Inserting them beside Vengeance would have shifted every
	// Paladin aura down two frames and two save slots, silently reassigning the points a live
	// character had already paid.
	HammerOfFaith,
	BlessedShield,
	// ---------------- Paladin: Passive Skills ----------------
	// Diablo III's Crusader passives, placeholders until the effects are built.
	//
	// Appended at the END of the class block for exactly the reason Hammer of Faith and Blessed
	// Shield were: a skill's position within its class IS its icon-strip frame AND its
	// _pClassTreeInvestment slot, so inserting them anywhere else would silently reassign the
	// points a live character has already paid.
	HeavenlyStrength,
	Fervor,
	Vigilant,
	Righteousness,
	Insurmountable,
	CrusaderFanaticism,
	Indestructible,
	HolyCause,
	Wrathful,
	DivineFortress,
	CrusadersStride, // was Lord Commander (2026-09-14)
	HoldYourGround,
	Sanctified, // was Long Arm of the Law (2026-09-14)
	IronMaiden,
	Renewal,
	Finery,
	Blunt,
	ToweringShield,
	// ---------------- Paladin: the RfA-12 skills (2026-09-13) ----------------
	// The 162 empty ability-page cells filled from RfA-12's final list. Appended at the END of the class
	// block like every addition before them: position within the class is the icon frame and the
	// investment slot, so nothing already saved moves.
	VotiveStrike,
	Judgment,
	Oathbrand,
	HolyLance,
	Crusade,
	AegisSlam,
	HeavensDescent,
	WrathOfTheHeavens,
	Valor,
	Radiance,
	BaneOfEvil,
	Condemnation,
	TitheOfAsh,
	Retaliation,
	DoomProcession,
	Dominion,
	Steadfast,
	ResistMagic,
	Immovable,
	WardingLight,
	Mercy,
	AuraOfProtection,
	Endurance,
	Sanctity,
	PALADIN_LAST = Sanctity,

	// ---------------- Barbarian: Combat Skills ----------------
	Bash,
	BARBARIAN_FIRST = Bash,
	Leap,
	DoubleSwing,
	Stun,
	DoubleThrow,
	LeapAttack,
	Concentrate,
	Frenzy,
	Whirlwind,
	Berserk,
	// ---------------- Barbarian: Combat Masteries ----------------
	SwordMastery,
	AxeMastery,
	MaceMastery,
	PoleArmMastery,
	ThrowingMastery,
	SpearMastery,
	Toughness, // was Increased Stamina (2026-09-14)
	IronSkin,
	IncreasedSpeed,
	NaturalResistance,
	// ---------------- Barbarian: Warcries ----------------
	Howl,
	FindPotion,
	Taunt,
	Shout,
	FindItem,
	BattleCry,
	BattleOrders,
	GrimWard,
	WarCry,
	BattleCommand,
	// ---------------- Barbarian: Passive Skills ----------------
	// Diablo III's Barbarian passives, placeholders until the effects are built.
	//
	// Appended at the END of the class block for exactly the reason Hammer of Faith and Blessed
	// Shield were: a skill's position within its class IS its icon-strip frame AND its
	// _pClassTreeInvestment slot, so inserting them anywhere else would silently reassign the
	// points a live character has already paid.
	PoundOfFlesh,
	Ruthless,
	NervesOfSteel,
	WeaponsMaster,
	InspiringPresence,
	BerserkerRage,
	Bloodthirst,
	Animosity,
	Superstition,
	ToughAsNails,
	NoEscape,
	Relentless,
	Brawler,
	Juggernaut,
	Unforgiving,
	BoonOfBulKathos,
	EarthenMight,
	SwordAndBoard,
	Rampage,
	// ---------------- Barbarian: the RfA-12 skills (2026-09-13) ----------------
	// The 162 empty ability-page cells filled from RfA-12's final list. Appended at the END of the class
	// block like every addition before them: position within the class is the icon frame and the
	// investment slot, so nothing already saved moves.
	Cleave,
	Backhand,
	GroundStomp,
	Rend,
	HammerOfTheAncients,
	SeismicSlam,
	ClaspOfRuin,
	Earthquake,
	GripOfIron,
	DeepWounds,
	HeavyFoot,
	BattleHardened,
	Bloodlust,
	LongReach,
	UnfinishedBusiness,
	LastingWounds,
	ThreateningShout,
	RallyingCry,
	Intimidate,
	SplitRanks,
	IronWill,
	Bloodcall,
	AncestralCall,
	EarthshakerCry,
	BARBARIAN_LAST = EarthshakerCry,

	// ---------------- Sorceress: Cold Spells ----------------
	IceBolt,
	SORCERER_FIRST = IceBolt,
	FrozenArmor,
	FrostNova,
	IceBlast,
	ShiverArmor,
	GlacialSpike,
	Blizzard,
	ChillingArmor,
	FrozenOrb,
	ColdMastery,
	// ---------------- Sorceress: Lightning Spells ----------------
	ChargedBoltSkill,
	StaticField,
	TelekinesisSkill,
	NovaSkill,
	LightningSkill,
	ChainLightningSkill,
	TeleportSkill,
	ThunderStorm,
	EnergyShield,
	LightningMastery,
	// ---------------- Sorceress: Fire Spells ----------------
	FireBoltSkill,
	Warmth,
	InfernoSkill,
	Blaze,
	FireBallSkill,
	FireWallSkill,
	Enchant,
	Meteor,
	FireMastery,
	Hydra,
	// ---------------- Sorceress: Passive Skills ----------------
	// Diablo III's Wizard passives, placeholders until the effects are built.
	//
	// Appended at the END of the class block for exactly the reason Hammer of Faith and Blessed
	// Shield were: a skill's position within its class IS its icon-strip frame AND its
	// _pClassTreeInvestment slot, so inserting them anywhere else would silently reassign the
	// points a live character has already paid.
	PowerHungry,
	Blur,
	ManaAttunement, // was Evocation (2026-09-14)
	GlassCannon,
	Prodigy,
	AstralPresence,
	Illusionist,
	ColdBlooded,
	Conflagration,
	Paralysis,
	GalvanizingWard,
	TemporalFlux,
	Dominance,
	ArcaneDynamo,
	UnstableAnomaly,
	UnwaveringWill,
	Audacity,
	ElementalExposure,
	// ---------------- Sorcerer: the RfA-12 skills (2026-09-13) ----------------
	// The 162 empty ability-page cells filled from RfA-12's final list. Appended at the END of the class
	// block like every addition before them: position within the class is the icon frame and the
	// investment slot, so nothing already saved moves.
	ChillTouch,
	IceNeedle,
	Frostbite,
	IceLance,
	BrittleGround,
	FrozenSentinel,
	Whiteout,
	AbsoluteZero,
	Arc,
	StaticCharge,
	BallLightning,
	Conduit,
	LightningRod,
	FaradayRing,
	StormCrucible,
	RideTheLightning,
	CinderTouch,
	EmberMine,
	FlameRing,
	AshenBrand,
	FurnaceMouth,
	Firestorm,
	Immolate,
	FuneralStar,
	SORCERER_LAST = FuneralStar,

	// ---------------- Rogue: Bow & Crossbow ----------------
	MagicArrow,
	ROGUE_FIRST = MagicArrow,
	FireArrow,
	ColdArrow,
	MultipleShot,
	ExplodingArrow,
	IceArrow,
	GuidedArrow,
	Strafe,
	ImmolationArrow,
	FreezingArrow,
	// ---------------- Rogue: Passive & Magic ----------------
	InnerSight,
	CriticalStrike,
	Dodge,
	SlowMissiles,
	Avoid,
	Penetrate,
	Decoy,
	Evade,
	Valkyrie,
	Pierce,
	// ---------------- Rogue: Javelin & Spear ----------------
	Jab,
	PowerStrike,
	PoisonJavelin,
	Impale,
	// Named for their rows (2026-09-11): these two were swapped - the value at Charged Strike
	// row was called LightningBoltSkill. Values unchanged, so nothing saved moves.
	ChargedStrike,
	LightningBoltSkill,
	PlagueJavelin,
	Fend,
	LightningStrike,
	LightningFury,
	// ---------------- Rogue: Passive Skills ----------------
	// Diablo III's Demon Hunter passives, placeholders until the effects are built.
	//
	// Appended at the END of the class block for exactly the reason Hammer of Faith and Blessed
	// Shield were: a skill's position within its class IS its icon-strip frame AND its
	// _pClassTreeInvestment slot, so inserting them anywhere else would silently reassign the
	// points a live character has already paid.
	ThrillOfTheHunt,
	TacticalAdvantage,
	BloodVengeance,
	SteadyAim,
	CullTheWeak,
	NightStalker,
	Brooding,
	HotPursuit,
	Archery,
	NumbingTraps,
	Perfectionist,
	CustomEngineering,
	Grenadier,
	Sharpshooter,
	Ballistics,
	Leech,
	Ambush,
	Awareness,
	SingleOut,
	// ---------------- Rogue: the RfA-12 skills (2026-09-13) ----------------
	// The 162 empty ability-page cells filled from RfA-12's final list. Appended at the END of the class
	// block like every addition before them: position within the class is the icon frame and the
	// investment slot, so nothing already saved moves.
	BarbedShaft,
	ShockArrow,
	PiercingShot,
	RainOfArrows,
	CripplingShot,
	HuntersMark,
	Barrage,
	PhantomVolley,
	SoftTread,
	Swiftness,
	ScentOfBlood,
	Sharpen,
	DeadGround,
	Deadeye,
	ShadowStep,
	HuntersClaim,
	Sweep,
	Brace,
	Harpoon,
	Vault,
	ReapingPoint,
	AnchorJavelin,
	TurningPike,
	ValkyriesSpear,
	ROGUE_LAST = ValkyriesSpear,

	// ---------------- Bard: Melody ----------------
	// Seven per page rather than ten: the Bard's sheet is the user's own design, not Diablo II's,
	// and it names seven songs per discipline. Everything else about the tree is unchanged.
	MelodyOfLife,
	BARD_FIRST = MelodyOfLife,
	BattleHymn,
	SongOfSwiftness,
	SongOfFortitude,
	DirgeOfDread,
	Lullaby,
	EpicSolo,
	// ---------------- Bard: Harmony ----------------
	SoundShock,
	BardShout,
	SonicBarrier,
	Discord,
	Resonance,
	EchoingSong,
	PerfectHarmony,
	// ---------------- Bard: Poetry ----------------
	Daze,
	Charm,
	Inspiration,
	TaleOfHeroes,
	Weaken,
	OdeToGlory,
	LegendaryBallad,
	// ---------------- Bard: Passive Skills ----------------
	// No Diablo III class matches the Bard, so these are authored in D3's idiom (user, 2026-08-25) - as her three song pages already are.
	//
	// Appended at the END of the class block for exactly the reason Hammer of Faith and Blessed
	// Shield were: a skill's position within its class IS its icon-strip frame AND its
	// _pClassTreeInvestment slot, so inserting them anywhere else would silently reassign the
	// points a live character has already paid.
	PerfectPitch,
	Crescendo,
	Sustain,
	Countermelody,
	Rhythm,
	Refrain,
	Encore,
	Cadence,
	Timbre,
	Virtuoso,
	Dissonance,
	Improvisation,
	Chorus,
	Overture,
	Reverberation,
	Stagecraft,
	Requiem,
	MagnumOpus,
	// ---------------- Bard: the RfA-12 skills (2026-09-13) ----------------
	// The 162 empty ability-page cells filled from RfA-12's final list. Appended at the END of the class
	// block like every addition before them: position within the class is the icon frame and the
	// investment slot, so nothing already saved moves.
	MinstrelsTune,
	BalladOfResilience,
	HuntersChant,
	SerenadeOfSteel,
	SongOfPlenty,
	Nocturne,
	AnthemOfValor,
	SirensCall,
	HymnOfRenewal,
	SymphonyOfWar,
	SovereignMeasure,
	PluckedNeedle,
	ShatterNote,
	TuningFork,
	Thunderclap,
	DissonantThread,
	SoundWave,
	DeafeningRoar,
	ChordOfWarding,
	Feedback,
	GrandFinale,
	MusicOfTheSpheres,
	BitterCouplet,
	MockingRhyme,
	Epitaph,
	Elegy,
	SonnetOfSight,
	Satire,
	VerseOfBinding,
	HeroicCouplet,
	Tragedy,
	Saga,
	LastWord,
	BARD_LAST = LastWord,

	// ---------------- Monk: Way of the Staff ----------------
	// Also the user's own design, and the most fully specified of the six: six sequential skills per
	// branch, one per tier, each requiring the one below it, and the branch's mastery beside the
	// sixth (it was a seventh rung at level 36 until 2026-09-07; the user capped the ability pages at
	// level 30). The page is a ladder, not a grid;
	// it sat in the MIDDLE column until 2026-09-07, when the user set one rule for every tree
	// ("fill column 1 first, then column two, then column 3"), so the ladder stands in column 0 now.
	SweepingReed,
	MONK_FIRST = SweepingReed,
	BreakingCurrent,
	ReedInTheWind,
	VaultingStrike,
	WheelOfHeaven,
	SevenReeds,
	MasterOfTheLongStaff,
	// ---------------- Monk: Way of the Body ----------------
	OpenPalm,
	FlowingStep,
	IronRobe,
	Counterstroke,
	PurifyingBreath,
	HundredFists,
	PerfectVessel,
	// ---------------- Monk: Way of the Spirit ----------------
	MonkInnerSight,
	HealingMantra,
	TempleBell,
	SpiritWard,
	RadiantPalm,
	Tranquility,
	Enlightenment,
	// ---------------- Monk: Passive Skills ----------------
	// Diablo III's Monk passives, placeholders until the effects are built.
	//
	// Appended at the END of the class block for exactly the reason Hammer of Faith and Blessed
	// Shield were: a skill's position within its class IS its icon-strip frame AND its
	// _pClassTreeInvestment slot, so inserting them anywhere else would silently reassign the
	// points a live character has already paid.
	Resolve,
	FleetFooted,
	ExaltedSoul,
	Transcendence,
	ChantOfResonance,
	SeizeTheInitiative,
	TheGuardiansPath,
	SixthSense,
	Determination,
	RelentlessAssault,
	SereneMind, // was Beacon of Ytar (2026-09-14)
	Alacrity,
	MonkHarmony,
	CombinationStrike,
	NearDeathExperience,
	Unity,
	Momentum,
	MythicRhythm,
	// ---------------- Monk: the RfA-12 skills (2026-09-13) ----------------
	// The 162 empty ability-page cells filled from RfA-12's final list. Appended at the END of the class
	// block like every addition before them: position within the class is the icon frame and the
	// investment slot, so nothing already saved moves.
	StaffParry,
	LongThrust,
	LowBranch,
	RearwardReach,
	MountainPole,
	BambooRain,
	DragonTailSweep,
	StaffOfEchoes,
	RiverStance,
	HeavenSplitter,
	ThousandReeds,
	TigerClaw,
	DeepBreath,
	LeapingCrane,
	PressurePoint,
	IronFist,
	WhirlingKick,
	ShoulderGate,
	SevenSidedStrike,
	MountainStance,
	ExplodingPalm,
	DragonsWrath,
	MantraOfClarity,
	MantraOfEvasion,
	ChiWave,
	BlindingFlash,
	MantraOfRetribution,
	Serenity,
	SpiritGuardian,
	WaveOfLight,
	InnerFire,
	AstralProjection,
	AncestralCourt,
	MONK_LAST = AncestralCourt,
	// ======================= NECROMANCER (2026-09-17) =======================
	// A seventh block, appended after the Monk's so no existing index moves. Diablo II's three pages at eighteen
	// rows each, and a Passive Skills page of eighteen from Diablo III's Necromancer. All 72 are inert at N1.
	// ---------------- Necromancer: Summoning ----------------
	RaiseSkeleton,
	NECROMANCER_FIRST = RaiseSkeleton,
	SkeletonMastery,
	CommandTheDead,
	ClayGolem,
	GolemMastery,
	GatherTheDead,
	RaiseSkeletalMage,
	SummonResist,
	DarkMending,
	BloodGolem,
	BonePlating,
	FrenzyOfTheDead,
	IronGolem,
	LastingBond,
	UnholyOffering,
	FireGolem,
	NecroRevive,
	ArmyOfTheDead,
	// ---------------- Necromancer: Poison & Bone ----------------
	Teeth,
	BoneArmor,
	PoisonDagger,
	CorpseExplosion,
	BoneSplinters,
	Blight,
	BoneWall,
	BoneSpikes,
	PoisonExplosion,
	BoneSpear,
	Decompose,
	Marrow,
	BonePrison,
	BoneStorm,
	Virulence,
	NecroBoneSpirit,
	PoisonNova,
	DeathNova,
	// ---------------- Necromancer: Curses ----------------
	AmplifyDamage,
	CurseMastery,
	EssenceTap,
	DimVision,
	NecroWeaken,
	Frailty,
	NecroIronMaiden,
	Terror,
	Bane,
	Confuse,
	LifeTap,
	WideMalice,
	Attract,
	Decrepify,
	DeathMark,
	LowerResist,
	SoulHarvest,
	Doom,
	// ---------------- Necromancer: Passive Skills ----------------
	LifeFromDeath,
	FueledByDeath,
	StandAlone,
	SwiftHarvesting,
	CommanderOfTheRisenDead,
	ExtendedServitude,
	RigorMortis,
	OverwhelmingEssence,
	DarkReaping,
	SpreadingMalediction,
	EternalTorment,
	FinalService,
	GrislyTribute,
	DrawLife,
	Serration,
	AberrantAnimator,
	BloodIsPower,
	RathmasShield,
	NECROMANCER_LAST = RathmasShield,
	LAST = RathmasShield,

	None = 0xFFFF,
};

constexpr size_t ClassTreeSkillCount = 506; // 434 + the Necromancer's 72 (2026-09-17); // 272 since 2026-09-06: the Paladin's Holy Bolt row removed (user: "There is a spell like this already in the game"); was 273
/**
 * @brief The most skills any one class has - the size of the per-character investment array.
 *
 * 64 since the Passive Skills page (2026-08-25) took the Paladin, Barbarian and Rogue to 49 apiece.
 * Was 32, which had been enough since the Paladin reached 31 (Hammer of Faith and Blessed Shield,
 * 2026-08-16). It bounds BuildClassTreePage's search, so a class with more skills than this simply
 * loses the overflow, and it must not exceed the size of Player::_pClassTreeInvestment, which it
 * indexes. The two are grown together - see the note beside that array.
 */
constexpr size_t MaxSkillsPerClass = 96;

/**
 * @brief Every class's row count is inside the array that stores its investments.
 *
 * Audit, 2026-08-31. The READS of Player::_pClassTreeInvestment are bounds-checked
 * (GetClassTreeInvestment, ClassTreeSkillAtIndex); the two WRITES were not - InvestClassTreePoint
 * and its refund index straight off ClassTreeIconIndex. The array is a member of Player, in the
 * middle of the struct the save format is built from, so an index past the end would not crash: it
 * would quietly write into whatever field follows and then persist it.
 *
 * Nothing can reach that today - the largest class is 49 of 64 - but the pipeline has two rows that
 * add tree skills per class (the 110 passives, and the Sorcerer's thin tree), so the headroom is
 * fifteen rows in front of work that is planned. This turns "someone will remember to grow the
 * array" into a build failure that names the class that outgrew it.
 *
 * Asserted on the ENUM SPANS rather than by counting Skills[], because that table is `const` and not
 * `constexpr` - and the spans are what ClassTreeIconIndex actually subtracts, so this is the same
 * arithmetic the write performs.
 */
static_assert(static_cast<size_t>(ClassTreeSkill::BARBARIAN_FIRST) - static_cast<size_t>(ClassTreeSkill::PALADIN_FIRST) <= MaxSkillsPerClass,
    "the Paladin has more tree rows than _pClassTreeInvestment can hold - grow MaxSkillsPerClass AND that array together");
static_assert(static_cast<size_t>(ClassTreeSkill::SORCERER_FIRST) - static_cast<size_t>(ClassTreeSkill::BARBARIAN_FIRST) <= MaxSkillsPerClass,
    "the Barbarian has more tree rows than _pClassTreeInvestment can hold - grow MaxSkillsPerClass AND that array together");
static_assert(static_cast<size_t>(ClassTreeSkill::ROGUE_FIRST) - static_cast<size_t>(ClassTreeSkill::SORCERER_FIRST) <= MaxSkillsPerClass,
    "the Sorcerer has more tree rows than _pClassTreeInvestment can hold - grow MaxSkillsPerClass AND that array together");
static_assert(static_cast<size_t>(ClassTreeSkill::BARD_FIRST) - static_cast<size_t>(ClassTreeSkill::ROGUE_FIRST) <= MaxSkillsPerClass,
    "the Rogue has more tree rows than _pClassTreeInvestment can hold - grow MaxSkillsPerClass AND that array together");
static_assert(static_cast<size_t>(ClassTreeSkill::MONK_FIRST) - static_cast<size_t>(ClassTreeSkill::BARD_FIRST) <= MaxSkillsPerClass,
    "the Bard has more tree rows than _pClassTreeInvestment can hold - grow MaxSkillsPerClass AND that array together");
static_assert(static_cast<size_t>(ClassTreeSkill::NECROMANCER_FIRST) - static_cast<size_t>(ClassTreeSkill::MONK_FIRST) <= MaxSkillsPerClass,
    "the Monk has more tree rows than _pClassTreeInvestment can hold - grow MaxSkillsPerClass AND that array together");
static_assert(static_cast<size_t>(ClassTreeSkill::NECROMANCER_LAST) - static_cast<size_t>(ClassTreeSkill::NECROMANCER_FIRST) < MaxSkillsPerClass,
    "the Necromancer has more tree rows than _pClassTreeInvestment can hold - grow MaxSkillsPerClass AND that array together");
/** @brief Tiers a page can have. Seven since the Monk; Diablo II's five classes use the first six. */
constexpr int ClassTreeTierCount = 7;
/** @brief Points a single tree skill accepts, matching the spell-investment cap. */
// THIRTY (user, 2026-08-31), down from 98. 98 was the whole pool a character can earn, so it never
// bound anything - and against an exponential damage curve that made one-skill builds dominant by
// six orders of magnitude. See MaxSkillInvestment in oracool/skill_points.h for the full reasoning;
// these two must stay equal, which the static_assert there enforces.
constexpr int MaxTreeInvestment = 30;
/**
 * @brief Pages in a class's tree. Four since Passive Skills (user, 2026-08-25).
 *
 * The first three are Diablo II's own sheets (the Bard's and Monk's are the user's own design in
 * that shape). The fourth is Diablo III's: a page of always-on passives, one rank each, bought
 * rather than levelled. It is DERIVED nowhere else - the Abilities window has one sheet per page
 * and must be grown with it.
 */
constexpr size_t ClassTreePageCount = 4;

/**
 * @brief The Passive Skills page. Everything about it differs from the three D2 pages.
 *
 * Scoped by PAGE and not by ClassTreeKind::Passive, which matters: the Barbarian's ten Combat
 * Masteries, the Rogue's Passive & Magic page and the Monk's Perfect Vessel are all Kind::Passive
 * and all still cost points and still hold ranks, because in Diablo II investing deeper IS the
 * mechanic. Only this page is free, automatic and slotted (user, 2026-08-25).
 */
constexpr int PassiveSkillsPage = 3;

/**
 * @brief The page of a row kept in the table but shown on no page (user, 2026-09-12: "i dont want 19th
 * (lvl 36) skill"). A row's POSITION is its identity - the icon strip, saved points and passive slots all
 * index by it - so a skill leaves its page by this value rather than by deletion. BuildClassTreePage
 * never asks for it, IsPassiveSkillRow is false for it, and the passives after it arrive a cell sooner.
 */
constexpr int RetiredFromTreePage = -1;

/** @brief Slots a character can fill with passives. Unslotted passives do nothing. */
constexpr size_t PassiveSlotCount = 4;

/** @brief Whether @p skill is a Passive Skills page row - free, auto-unlocking and slot-gated. */
bool IsPassiveSkillRow(ClassTreeSkill skill);

/**
 * @brief The character level at which @p skill unlocks: one passive every even level.
 *
 * The nth passive on the page (counting from zero, in grid reading order) arrives at level
 * 2n+2 - so the first at 2 and the eighteenth, the last a 3x6 page holds, at 36. Returns 0 for a row
 * that is not on the Passive Skills page; those are gated by tier.
 */
int PassiveSkillRequiredLevel(ClassTreeSkill skill);

/** @brief The character level that opens slot @p slot: 1, 10, 20, 30. */
int PassiveSlotRequiredLevel(int slot);

/** @brief How many of the four slots @p player has opened. */
int UnlockedPassiveSlotCount(const Player &player);

/** @brief The passive in @p slot, or None - empty, locked, or holding something invalid. */
ClassTreeSkill PassiveInSlot(const Player &player, int slot);

/** @brief The slot @p skill occupies, or -1. This is the whole of "is this passive active". */
int PassiveSlotOf(const Player &player, ClassTreeSkill skill);

/**
 * @brief Puts @p skill into @p slot. Refuses a locked slot, a locked skill, another class's skill,
 * a row that is not a passive, or a skill already sitting in a different slot.
 */
bool SetPassiveSlot(Player &player, int slot, ClassTreeSkill skill);

/** @brief Empties @p slot. False if it was already empty or the slot does not exist. */
bool ClearPassiveSlot(Player &player, int slot);

/** @brief What kind of thing a row is, which decides what a click does. */
enum class ClassTreeKind : uint8_t {
	/** Cast or swung - clicking readies it on the mouse button that clicked. */
	Active,
	/** Burns until switched off. Exactly one aura at a time, Paladin only. */
	Aura,
	/** Always on once paid for - masteries and the like. Clicking does nothing. */
	Passive,
};

struct ClassTreeSkillData {
	/** Untranslated; run through _() at the point of display. */
	const char *name;
	/** What the skill does in Diablo II, plus this engine's adaptation where they differ. */
	const char *description;
	HeroClass heroClass;
	/** 0-3, the page within the class's tree - or RetiredFromTreePage, for a row shown on none. */
	int page;
	/** 0 to ClassTreeTierCount-1. The character level required is ClassTreeTierMinLevel(tier). */
	int tier;
	/** 0-2, the grid column on its page. */
	int column;
	ClassTreeKind kind;
	/**
	 * @brief The spell slot, or SpellID::Invalid. Read it through ClassTreeSpellId(), which also
	 * resolves the handful of Paladin skills whose slot is owned by oracool/paladin_skills.h.
	 */
	SpellID spellId;
	/** Whether this row does anything yet - false means listed, described, and inert. */
	bool implemented;
	/**
	 * @brief Points this one skill accepts, or 0 to mean MaxTreeInvestment.
	 *
	 * Zero rather than a real default because every row is positional aggregate initialisation:
	 * appending a field leaves the existing rows' value at 0, and reading 0 as "the usual cap"
	 * is what let the Monk introduce per-skill caps without touching 140 other rows. Read it
	 * through ClassTreeMaxRank(), never directly.
	 */
	int maxRank;
};

/**
 * @brief Whether @p skill has been retired from the tree because its slot is a BOOK spell.
 *
 * User rule, 2026-08-20: "Spells cant be affected by skill points, only by books. Vanila D1." A row
 * whose SpellID a Book could teach is a spell, not a skill, so it leaves the tree - it is still
 * castable, still readiable, still raised by books, just no longer a place to spend points.
 *
 * Costs the Sorceress thirteen rows (her whole castable set), one row each from the Rogue
 * (Golem), Bard (Berserk) and Monk (Search), and the two Mana Shield rows (the Bard's Sonic
 * Barrier, the Monk's Spirit Ward - audit, 2026-09-07). Costs the Paladin nothing: his seven actives carry
 * sBookLvl -1 deliberately, so this predicate never sees them.
 *
 * The row stays in the table. ClassTreeIconIndex is both the icon-strip position and the
 * _pClassTreeInvestment index, so deleting rows would drift the art and misalign existing saves;
 * BuildClassTreePage skips them instead, leaving the grid cell empty.
 */
bool IsClassTreeRowRetiredAsSpell(ClassTreeSkill skill);

/** @brief The cap on @p skill, resolving the table's 0 to MaxTreeInvestment. */
int ClassTreeMaxRank(ClassTreeSkill skill);

const ClassTreeSkillData &GetClassTreeSkillData(ClassTreeSkill skill);

/** @brief The spell slot @p skill readies, or SpellID::Invalid. The one authority. */
SpellID ClassTreeSpellId(ClassTreeSkill skill);

/**
 * @brief The tree row a readied @p spell came from, for @p heroClass, or None.
 *
 * The reverse of ClassTreeSpellId, and the reason it exists: the HUD's LMB and RMB wells are handed
 * a SpellID and have to draw a picture for it. They used to ask the retired Skills sheet's own
 * seven-icon Paladin strip, which meant a skill readied from a tree page wore a DIFFERENT picture in
 * the well than the one you clicked (user, 2026-08-18 - "icons in Abilities Sheets to match the
 * icons in LMB and RMB").
 *
 * Class-scoped on purpose. Spell ids are global while tree rows are not, so asking "which row is
 * this" without saying whose tree would match another class's row for any id two classes share.
 */
ClassTreeSkill ClassTreeSkillForSpell(HeroClass heroClass, SpellID spell);

/** @brief Character level required by @p tier: Diablo II's 1, 6, 12, 18, 24, 30. */
int ClassTreeTierMinLevel(int tier);

/** @brief Display name of @p page for @p heroClass, for the window's title band. */
string_view GetClassTreePageName(HeroClass heroClass, int page);

/** @brief Whether @p heroClass has a tree at all. All four playable-in-V1 classes do. */
bool ClassHasTree(HeroClass heroClass);

/** @brief @p skill's position within its own class - its index into that class's icon strip. */
int ClassTreeIconIndex(ClassTreeSkill skill);

/**
 * @brief The inverse of ClassTreeIconIndex: @p heroClass's skill at @p index, or nullopt.
 *
 * Exists so the burning aura can be persisted as (class, index-within-class) instead of an absolute
 * enum ordinal that shifts whenever an earlier class gains rows.
 */
std::optional<ClassTreeSkill> ClassTreeSkillAtIndex(HeroClass heroClass, int index);

/** @brief Whether @p player's level meets @p skill's tier, and it is their class's skill. */
bool IsClassTreeSkillUnlocked(const Player &player, ClassTreeSkill skill);

/** @brief Points sunk into @p skill, from whichever store it uses. See the file comment. */
int ClassTreeInvestment(const Player &player, ClassTreeSkill skill);

/** @brief Whether an invest click would take: unlocked, a point unspent, cap not reached. */
bool CanInvestClassTreePoint(const Player &player, ClassTreeSkill skill);

/** @brief Whether @p skill has a rank that can be taken back - simply "is anything invested". */
bool CanRefundClassTreePoint(const Player &player, ClassTreeSkill skill);

/**
 * @brief Takes one point back out of @p skill and returns it to the unspent pool.
 *
 * Free and unlimited, by design (user, 2026-08-17: "We want players to be able to redistribute skill
 * points at will"). There is no respec cost and no confirmation, which is what makes the trees a
 * place to experiment rather than a set of decisions to regret.
 *
 * Puts out an aura that this drops to zero: ToggleClassAura already refuses to LIGHT an aura with
 * nothing invested, so leaving one burning would be the only way to hold a state the rules forbid.
 */
bool RefundClassTreePoint(Player &player, ClassTreeSkill skill);

/** @brief Spends one of Phase 2.1's unspent points on @p skill. False changes nothing. */
bool InvestClassTreePoint(Player &player, ClassTreeSkill skill);

/** @brief The burning aura, or None. Paladin only; decoded from Player::_pOracoolActiveAura. */
ClassTreeSkill GetActiveClassAura(const Player &player);

/**
 * @brief Click rule for an aura row: activates @p skill, or switches it off if already burning.
 * Refuses a non-aura, a locked tier, the wrong class, or an aura with nothing invested in it.
 */
bool ToggleClassAura(Player &player, ClassTreeSkill skill);

/**
 * @brief Puts out whatever aura is burning, because the right button has just been given a skill.
 *
 * The aura and the readied right-button skill are ONE slot (user, 2026-08-19: "if i put a combat
 * skill on RMB i cant seem to put an aura there. if i put reg attack then i AM able"). D2 works the
 * same way - an aura is what the right button is set to, not a badge riding along beside it - and the
 * alternative was the state that broke v1.7.91, where an aura sat on the well and hid every skill
 * readied afterwards.
 *
 * So: lighting an aura clears the readied right-button skill (ToggleClassAura does that end), and
 * readying a right-button skill calls this. No third state, and nothing invisible.
 */
void ClearClassAuraForRightButton(Player &player);

/**
 * @brief Releases the aura's loop for a level change or a game teardown, tracker included.
 *
 * Use these two rather than SilenceClassAuraLoop/ResumeClassAuraLoop from skill_sounds.h. Those
 * talk to the sound layer alone, and this file keeps a record of what it believes is playing so
 * that the per-tick re-assertion can be a no-op. A lifecycle call that moves one without the other
 * leaves the two disagreeing, and the tick then "corrects" the wrong one (audit, 2026-08-26).
 */
void SilenceAuraLoopForTransition();

/** @brief Re-attaches the loop for @p skill after a transition, tracker included. */
void ResumeAuraLoopAfterTransition(ClassTreeSkill skill);

/**
 * @brief Contributes the burning aura AND every paid-for passive onto @p totals. Auras scale with
 * the points in them; passives are always on once bought. Effects this engine has no channel for
 * contribute nothing.
 */
void ApplyClassTreeToTotals(const Player &player, ItemBonusTotals &totals);

/**
 * @brief Whether the character is running for free right now - the Paladin's Vigor aura or the
 * Barbarian's Increased Speed mastery. Both reach an engine with no walk-speed modifier the same
 * way: by holding on the double-speed frame skip the run toggle uses.
 */
bool IsClassTreeRunActive(const Player &player);

/**
 * @brief Movement Speed +X% per level of the Paladin's Vigor. 5 since 2026-09-12 (user: "make vigor +5%
 * faster walk per level for every level"), every level counted - see StrideTicksFor. It was 15, and
 * five ranks reached the run, past which nothing showed.
 */
constexpr int VigorMoveSpeedPerRank = 5;

/** @brief The character's Movement Speed bonus in percent: worn affixes (a curse counts against) plus the burning Vigor. */
int MovementSpeedBonusPercent(const Player &player);

/** @brief The slow on @p player right now, in percent (0 when none). Cold and curses land here. */
int PlayerSlowPercent(const Player &player);

/**
 * @brief Slows @p player by @p percent for @p ticks. Overlapping slows keep the deeper and the longer,
 * never add. The channel every cold or cursing effect that targets a PLAYER uses (2026-09-07: "curses
 * and cold spells decrease it"); nothing in the engine slows a player yet, so it waits for its first caller.
 */
void SlowPlayer(const Player &player, int ticks, int percent);

/** @brief One tick of the slow's clock; called from the class tree's per-player tick. */
void TickMovementSlow(const Player &player);

/** @brief Forgets every slow - the new-game reset. */
void ClearMovementSlows();

/** @brief Ends @p player's slow at once. Serenity. */
void ClearPlayerSlow(const Player &player);

/**
 * @brief Movement Speed as the sheet shows it: 100 is a plain walk, abilities and items above, slows
 * below. What the feet do with it is WalkFrameSkipFor's business.
 */
int MovementSpeedPercent(const Player &player);

/** @brief The fastest and slowest strides, in ticks: 250% and about 83% of a walk. */
constexpr int MinStrideTicks = 4;
constexpr int MaxStrideTicks = 12;

/**
 * @brief Ticks for one stride at @p percent Movement Speed, carrying the fraction a whole tick cannot
 * show in @p carryMilliTicks (thousandths) so that every percent counts on average. Pure - the test's.
 */
int StrideTicksFor(int percent, int &carryMilliTicks);

/**
 * @brief The walk-animation frame skip for @p player's next stride: 8 less its ticks, so -4 (12 ticks)
 * through -2 (a plain walk) and 2 (the run) up to 4 (250%). Carries each player's fraction, so call it
 * once per stride. StartWalkAnimation takes the larger of this and the binary run sources.
 */
int8_t WalkFrameSkipFor(const Player &player);

/** @brief Thorns at @p points: the share of each melee blow taken that goes back, in percent (25, +10 a level). */
int ThornsReturnPercentAt(int points);
/** @brief The share @p player's lit Thorns returns right now, or 0. Read where a monster's blow lands. */
int ThornsReturnPercent(const Player &player);
/** @brief Cleansing at @p points: how much sooner a slow or a chill wears off, in percent (20, +5 a level, to 90). */
int CleansingShortenPercentAt(int points);
/** @brief The shortening @p player's lit Cleansing gives right now, or 0. Applied in SlowPlayer. */
int CleansingShortenPercent(const Player &player);

/**
 * @brief How many frames of the cast animation Faster Cast Rate skips (2026-09-11).
 *
 * @p castFrame is the class's frame the spell leaves on (_pSFNum). Every skipped frame is one tick off
 * the wait, so +X% casts in castFrame * 100 / (100 + X) ticks, rounded. Never the whole wait - at most
 * castFrame - 1 - because DoSpell fires ON that frame and an animation that starts past it would never
 * fire. Nothing is skipped for 0 or less.
 */
int CastFrameSkip(int castFrame, int fasterCastPercent);

/**
 * @brief Per-tick work: the Paladin's Prayer and Meditation auras, and the Sorceress's Warmth.
 * Called once per game logic tick for the local player.
 */
void ProcessClassTreeTick(Player &player);

/**
 * @brief Fills @p out with the skills on @p page of @p heroClass's tree, in grid order. Returns
 * how many; @p out must hold at least MaxSkillsPerClass.
 */
size_t BuildClassTreePage(HeroClass heroClass, int page, ClassTreeSkill *out);

/**
 * @brief The block the hover panel puts under the description, in Diablo II's shape: "Current Skill
 * Level: N" over this rank's numbers, then (when @p withNext) "Next Level" over the next rank's.
 * The picker passes false - it shows the name and the current rank only.
 */
std::string ClassTreeEffectLine(const Player &player, ClassTreeSkill skill, bool withNext = true);

/**
 * @brief Why @p skill cannot be readied yet, or an empty string if it can.
 *
 * Exists because a locked row used to refuse SILENTLY (user, 2026-08-18: "left/right clicks seem to
 * do nothing in the three new abilities windows"). For the seven Paladin rows that borrow a real
 * skill the gate is not the tier at all - IsPaladinSkillUnlocked carries its own minimum level AND a
 * shield requirement, so Smite is locked at level 1 and stays locked bare-handed at level 20. The
 * cell looked clickable and said nothing when clicked.
 *
 * Phrased as a whole sentence ready for the message line, so callers do not assemble it.
 */
std::string ClassTreeLockReason(const Player &player, ClassTreeSkill skill);

} // namespace oracool
} // namespace devilution
