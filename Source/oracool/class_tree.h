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
enum class ClassTreeSkill : uint8_t {
	// ---------------- Paladin: Combat Skills ----------------
	Sacrifice,
	FIRST = Sacrifice,
	PALADIN_FIRST = Sacrifice,
	Smite,
	HolyBolt,
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
	PALADIN_LAST = BlessedShield,

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
	IncreasedStamina,
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
	BARBARIAN_LAST = BattleCommand,

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
	SORCERER_LAST = Hydra,

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
	LightningBoltSkill,
	ChargedStrike,
	PlagueJavelin,
	Fend,
	LightningStrike,
	LightningFury,
	ROGUE_LAST = LightningFury,

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
	BARD_LAST = LegendaryBallad,

	// ---------------- Monk: Way of the Staff ----------------
	// Also the user's own design, and the most fully specified of the six: seven sequential skills
	// per branch, one per tier, each requiring the one below it. That linear shape is why every
	// Monk skill sits in the middle column - the page is a ladder, not a grid.
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
	MONK_LAST = Enlightenment,
	LAST = Enlightenment,

	None = 0xFF,
};

constexpr size_t ClassTreeSkillCount = 163;
/**
 * @brief The most skills any one class has - the size of the per-character investment array.
 *
 * 32 since the Paladin reached 31 (Hammer of Faith and Blessed Shield, 2026-08-16). It bounds
 * BuildClassTreePage's search, so a class with more skills than this simply loses the overflow, and
 * it must not exceed the size of Player::_pClassTreeInvestment, which it indexes. The two are grown
 * together - see the static_assert beside that array.
 */
constexpr size_t MaxSkillsPerClass = 32;
/** @brief Tiers a page can have. Seven since the Monk; Diablo II's five classes use the first six. */
constexpr int ClassTreeTierCount = 7;
/** @brief Points a single tree skill accepts, matching the spell-investment cap. */
// 98, up from D2s 20 (user, 2026-08-19): the cap is the whole pool a character can earn, and the
// Rule of Rangs - one character level per rank past the first - is what paces depth instead.
constexpr int MaxTreeInvestment = 98;
constexpr size_t ClassTreePageCount = 3;

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
	/** 0-2, the page within the class's tree. */
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
 * Costs the Sorceress thirteen rows (her whole castable set) and one row each from the Rogue
 * (Golem), Bard (Berserk) and Monk (Search). Costs the Paladin nothing: his seven actives carry
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
 * @brief Per-tick work: the Paladin's Prayer and Meditation auras, and the Sorceress's Warmth.
 * Called once per game logic tick for the local player.
 */
void ProcessClassTreeTick(Player &player);

/**
 * @brief Fills @p out with the skills on @p page of @p heroClass's tree, in grid order. Returns
 * how many; @p out must hold at least MaxSkillsPerClass.
 */
size_t BuildClassTreePage(HeroClass heroClass, int page, ClassTreeSkill *out);

/** @brief The line the hover panel puts under the description: what the points bought. */
std::string ClassTreeEffectLine(const Player &player, ClassTreeSkill skill);

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
