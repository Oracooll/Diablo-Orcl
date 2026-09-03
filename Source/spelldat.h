/**
 * @file spelldat.h
 *
 * Interface of all spell data.
 */
#pragma once

#include <cstdint>
#include <type_traits>

#include "effects.h"
#include "utils/enum_traits.h"

namespace devilution {

// Oracool: 52 -> 53 with SpellID::Charge, then -> 59 with the Paladin's other six skills. NOTE for
// anything bounded by this: items.cpp's GetItemSpell and CreateSpellBook walk
// `gbIsHellfire ? MAX_SPELLS : 37` looking for droppable spells, so a new id is a candidate for books
// and staves unless its sBookLvl and sStaffLvl are both -1. All seven skills' are, deliberately -
// they are earned by level, not found. MAX_ITEM_SPELLS below is the belt-and-braces on that.
#define MAX_SPELLS 120

/**
 * @brief Upper bound for the spell ids ITEM GENERATION may roll - books, staves, scrolls.
 *
 * Oracool: pinned at 52 when SpellID::Charge became id 52. Item generation does not merely SKIP
 * ineligible spells, it draws `GenerateRnd(maxSpells)` and walks from there, so widening the bound
 * changes which spell a given seed lands on even for a spell that can never be chosen. Charge is
 * earned, never found, so letting it move the loot tables would be a bug with no upside - and it was
 * caught as one: PackTest's fixtures encode a seed's expected book, and they broke the moment
 * MAX_SPELLS went up.
 *
 * A new id that IS meant to drop belongs below this line, with this raised to match.
 */
#define MAX_ITEM_SPELLS 52

enum class SpellType : uint8_t {
	Skill,
	FIRST = Skill,
	Spell,
	Scroll,
	Charges,
	LAST = Charges,
	Invalid,
};

enum class SpellID : int8_t {
	Null,
	FIRST = Null,
	Firebolt,
	Healing,
	Lightning,
	Flash,
	Identify,
	FireWall,
	TownPortal,
	StoneCurse,
	Infravision,
	Phasing,
	ManaShield,
	Fireball,
	Guardian,
	ChainLightning,
	FlameWave,
	DoomSerpents,
	BloodRitual,
	Nova,
	Invisibility,
	Inferno,
	Golem,
	Rage,
	Teleport,
	Apocalypse,
	Etherealize,
	ItemRepair,
	StaffRecharge,
	TrapDisarm,
	Elemental,
	ChargedBolt,
	HolyBolt,
	Resurrect,
	Telekinesis,
	HealOther,
	BloodStar,
	BoneSpirit,
	LastDiablo = BoneSpirit,
	Mana,
	Magi,
	Jester,
	LightningWall,
	Immolation,
	Warp,
	Reflect,
	Berserk,
	RingOfFire,
	Search,
	RuneOfFire,
	RuneOfLight,
	RuneOfNova,
	RuneOfImmolation,
	RuneOfStone,

	/**
	 * Oracool: the Paladin's Charge (user request 2026-08-15 - "New paladin skills we introduce or
	 * have already introduced will be independent, not replacing Repair Skill").
	 *
	 * It used to be a behaviour substitution ON SpellID::ItemRepair, which is exactly why it
	 * displaced Repair and why it could not be assigned freely. Appended LAST so no existing value
	 * moves - every save field and table below is indexed positionally.
	 *
	 * Costs no save-format change, which is worth recording because it looks like it should: the
	 * hero file (PlayerPack) persists spell LEVELS only for ids 0..46, so the five runes at 47..51
	 * already do not survive a reload and Charge simply joins them. What does persist is
	 * _pMemSpells/_pAblSpells, both uint64, where bit 52 fits with room to spare.
	 */
	Charge,

	/**
	 * Oracool: the Paladin's other six skills (user request 2026-08-15 - "Make sure all skills are
	 * selectible and their icons appear as they should on LMB/RMB").
	 *
	 * Every one of them needs an id for the same reason Charge did: a row on the Skills sheet is
	 * assignable to a mouse button exactly when it carries a SpellID, and the readied pair the HUD's
	 * wells and the save format both speak is a SpellID. Without one they were listed and inert.
	 *
	 * Zeal is here despite applying itself to every melee swing rather than being cast - it is a
	 * skill the player picks, so it belongs on a button like the rest, and casting it simply does
	 * nothing extra. The other five have no mechanics yet at all (oracool::IsPaladinSkillImplemented);
	 * all six carry MissileID::Null in both slots, exactly as Charge does, so a cast is a no-op rather
	 * than an error.
	 *
	 * Appended LAST so no existing value moves - every save field and table below is indexed
	 * positionally. Same save arithmetic as Charge: PlayerPack persists spell LEVELS only for ids
	 * 0..46, so these join the runes in not persisting a level, while _pMemSpells/_pAblSpells are
	 * uint64 and bit 57 is still comfortably inside them.
	 */
	Zeal,
	HammerOfFaith,
	BlessedShield,
	FistOfTheHeavens,
	ShieldBash,
	BlessedHammer,
	/**
	 * Oracool, Round 1 of the inert-skill plan (2026-09-03): the Sorceress's Ice Bolt, and the first
	 * spell in this game that deals cold.
	 *
	 * ID 59, and worth knowing what that number is close to. GetSpellBitmask is `1ULL << (id - 1)`
	 * over a uint64, so id 64 is the last one that can exist at all - five seats left after this. The
	 * cold line alone wants eleven more, so Round 2 opens by widening those masks rather than by
	 * writing spells; see the plan. The static_assert below is what makes running out a compile
	 * error rather than a spell that silently shares another's bit.
	 */
	IceBolt,

	/**
	 * Oracool, Round 2 (2026-09-03): the rest of the Sorceress's cold page. Ids 60-67 - the first
	 * of them was the reason the spell masks widened, and the assert below is what would have
	 * stopped it otherwise. Cold Mastery is a passive and has no id; the three arrows are Round 3's.
	 */
	IceBlast,
	GlacialSpike,
	FrostNova,
	Blizzard,
	FrozenOrb,
	FrozenArmor,
	ShiverArmor,
	ChillingArmor,

	/**
	 * Oracool, Round 3 (2026-09-03): the Rogue's bow page. Ids 68-77. None is cast - each is SHOT,
	 * through the latch in oracool/rogue_arrows.h - so every one carries MissileID::Null, like Zeal.
	 */
	MagicArrow,
	FireArrow,
	ColdArrow,
	MultipleShot,
	ExplodingArrow,
	IceArrow,
	GuidedArrow,
	Strafe,
	ImmolationArrow,
	FreezingArrow,

	/**
	 * Oracool, Round 4 (2026-09-03): the melee skills - the Barbarian's nine and the Monk's eight.
	 * Ids 78-94. Like the bow page, none is cast: each is SWUNG, through the latch in
	 * oracool/melee_skills.h, so every one carries MissileID::Null. Double Throw stays inert (no
	 * thrown weapons in this engine) and has no id.
	 */
	Bash,
	Leap,
	DoubleSwing,
	Stun,
	LeapAttack,
	Concentrate,
	Frenzy,
	Whirlwind,
	BerserkBlow,
	SweepingReed,
	BreakingCurrent,
	VaultingStrike,
	WheelOfHeaven,
	SevenReeds,
	OpenPalm,
	HundredFists,
	RadiantPalm,
	// Oracool, Round 6 (2026-09-03): the timed shouts and songs - oracool/warcries.h. All cast as
	// MissileID::Warcry, untargeted; the Barbarian's seven, the Bard's four, the Monk's three, the
	// Rogue's two, the Paladin's Vengeance.
	Howl,
	Taunt,
	Shout,
	BattleCry,
	BattleOrders,
	WarCry,
	BattleCommand,
	Lullaby,
	SoundShock,
	BardShout,
	Daze,
	TempleBell,
	PurifyingBreath,
	Tranquility,
	InnerSight,
	SlowMissiles,
	Vengeance,
	// Oracool, Round 7 (2026-09-03): the Rogue's Javelin & Spear page. This engine has no javelin
	// and no spear, so the thrusts are melee skills on Round 4's latch (oracool/melee_skills.h) and
	// the two thrown bolts ride the engine's own Lightning and Nova at the rank.
	Jab,
	PowerStrike,
	Impale,
	ChargedStrike,
	Fend,
	LightningStrike,
	LightningBoltSkill,
	LightningFury,

	LAST = LightningFury,
	Invalid = -1,
};

/**
 * @brief A set of spells: 128 bits, one per SpellID.
 *
 * Oracool, Round 2 of the inert-skill plan (2026-09-03). The four sets a character carries - known,
 * innate, item and scroll spells - were each a bare uint64, and GetSpellBitmask was
 * `1ULL << (id - 1)`, so id 64 was the last spell that could exist at all. Ice Bolt took id 59 and
 * the rest of the cold line wanted eleven more; the first of them would have shared a bit with
 * Firebolt and the two would have been learned, forgotten and readied together.
 *
 * Two words rather than a std::bitset, and the reason is the save. The fixed hero struct and the
 * level save both store the LOW word as the uint64 they always have (see low()), so every existing
 * file reads unchanged; the high word is never persisted, and does not need to be: the only set that
 * is saved is _pMemSpells - book spells, all of which sit below 64 - while innate, item and scroll
 * spells are rebuilt from investment and inventory on every load. A tree skill above 64 therefore
 * comes back through the investment chunk, which is count-prefixed for exactly this growth.
 *
 * The operators are the ones the 130-odd existing sites already use, so `(mask & bit) != 0` and
 * `mask |= bit` read exactly as before - the 0 in that idiom converts through the uint64 constructor
 * into an empty mask. Mind that the same constructor makes `mask == 5` compile and mean "is this
 * mask exactly spells 1 and 3"; nothing writes that, and nothing should.
 */
struct SpellMask {
	uint64_t low = 0;
	uint64_t high = 0;

	constexpr SpellMask() = default;
	/** @brief From a saved or literal uint64: the low word, as every existing site means it. */
	constexpr SpellMask(uint64_t lowWord) // NOLINT(google-explicit-constructor) - the save reads through this
	    : low(lowWord)
	{
	}
	constexpr SpellMask(uint64_t lowWord, uint64_t highWord)
	    : low(lowWord)
	    , high(highWord)
	{
	}

	[[nodiscard]] constexpr bool any() const { return (low | high) != 0; }
	[[nodiscard]] constexpr bool none() const { return !any(); }

	constexpr SpellMask operator&(SpellMask other) const { return { low & other.low, high & other.high }; }
	constexpr SpellMask operator|(SpellMask other) const { return { low | other.low, high | other.high }; }
	constexpr SpellMask operator~() const { return { ~low, ~high }; }
	constexpr SpellMask &operator&=(SpellMask other)
	{
		low &= other.low;
		high &= other.high;
		return *this;
	}
	constexpr SpellMask &operator|=(SpellMask other)
	{
		low |= other.low;
		high |= other.high;
		return *this;
	}
	constexpr bool operator==(SpellMask other) const { return low == other.low && high == other.high; }
	constexpr bool operator!=(SpellMask other) const { return !(*this == other); }
};

// The `(mask & bit) != 0` idiom needs nothing beyond the members above: the 0 converts through the
// uint64 constructor into an empty mask and the member operator!= does the rest. A first draft added
// int and unsigned overloads for it, and they made `EXPECT_EQ(mask, 0ULL)` AMBIGUOUS - two narrowing
// candidates of equal rank - which is a worse outcome than the idiom taking one implicit conversion.

/**
 * @brief The hard ceiling on spell ids - 128 since the masks widened, from the 64 that Ice Bolt
 * came within five of.
 *
 * Asserted rather than trusted, because the failure has no symptom at the point it is caused: a
 * shift past the word is undefined and on this compiler silently wraps, so the new spell would
 * quietly share an old one's bit.
 */
static_assert(static_cast<int>(SpellID::LAST) <= 128,
    "spell ids past 128 do not fit SpellMask - add a third word, and check what saves it");

enum class MagicType : uint8_t {
	Fire,
	Lightning,
	Magic,
	/**
	 * Oracool, Round 1 (2026-09-03). Three, and three is the last one that fits: SpellData::type()
	 * masks the flags with 0b11, so this enum has exactly four seats and cold takes the empty one.
	 */
	Cold,
};

enum class MissileID : int8_t {
	// clang-format off
	Arrow,
	Firebolt,
	Guardian,
	Phasing,
	NovaBall,
	FireWall,
	Fireball,
	LightningControl,
	Lightning,
	MagmaBallExplosion,
	TownPortal,
	FlashBottom,
	FlashTop,
	ManaShield,
	FlameWave,
	ChainLightning,
	ChainBall, // unused
	BloodHit, // unused
	BoneHit, // unused
	MetalHit, // unused
	Rhino,
	MagmaBall,
	ThinLightningControl,
	ThinLightning,
	BloodStar,
	BloodStarExplosion,
	Teleport,
	FireArrow,
	DoomSerpents, // unused
	FireOnly, // unused
	StoneCurse,
	BloodRitual, // unused
	Invisibility, // unused
	Golem,
	Etherealize,
	Spurt, // unused
	ApocalypseBoom,
	Healing,
	FireWallControl,
	Infravision,
	Identify,
	FlameWaveControl,
	Nova,
	Rage, // BloodBoil in Diablo
	Apocalypse,
	ItemRepair,
	StaffRecharge,
	TrapDisarm,
	Inferno,
	InfernoControl,
	FireMan, // unused
	Krull, // unused
	ChargedBolt,
	HolyBolt,
	Resurrect,
	Telekinesis,
	LightningArrow,
	Acid,
	AcidSplat,
	AcidPuddle,
	HealOther,
	Elemental,
	ResurrectBeam,
	BoneSpirit,
	WeaponExplosion,
	RedPortal,
	DiabloApocalypseBoom,
	DiabloApocalypse,
	Mana,
	Magi,
	LightningWall,
	LightningWallControl,
	Immolation,
	SpectralArrow,
	FireballBow,
	LightningBow,
	ChargedBoltBow,
	HolyBoltBow,
	Warp,
	Reflect,
	Berserk,
	RingOfFire,
	StealPotions,
	StealMana,
	RingOfLightning, // unused
	Search,
	Aura, // unused
	Aura2, // unused
	SpiralFireball, // unused
	RuneOfFire,
	RuneOfLight,
	RuneOfNova,
	RuneOfImmolation,
	RuneOfStone,
	BigExplosion,
	HorkSpawn,
	Jester,
	OpenNest,
	OrangeFlare,
	BlueFlare,
	RedFlare,
	YellowFlare,
	BlueFlare2,
	YellowExplosion,
	RedExplosion,
	BlueExplosion,
	BlueExplosion2,
	OrangeExplosion,
	/**
	 * Oracool: the Paladin's Blessed Hammer. Appended last, because misdat.cpp's MissilesData is
	 * indexed by this enum positionally and every existing value has to keep its place.
	 */
	BlessedHammer,
	/** Oracool: Blessed Shield's thrown projectile. Splashes on impact - see ProcessBlessedShieldThrow. */
	BlessedShieldThrow,
	/** Oracool: Fist of the Heavens' descent - plays items\mace.cel, the item drop tumble. */
	FallingMace,
	/**
	 * Oracool: Fist of the Heavens' mini-Nova bolt.
	 *
	 * NovaBall's behaviour with ChargedBolt's sprite, which is the file "miniltng" - literally mini
	 * lightning. That is the whole of "shrunken down animation of Nova": the ring geometry and the
	 * travel distance are already what the user asked for (ProcessNovaCommon's radius is 4 tiles),
	 * so only the bolt needed to get smaller, and the art for that already shipped.
	 */
	MiniNovaBall,
	/** Oracool, Round 1: the Sorceress's Ice Bolt. Firebolt's behaviour, cold damage, its own art. */
	IceBolt,
	/** Oracool, Round 2. Each is described at its Add function in missiles.cpp. */
	IceImpact,
	IceBlast,
	GlacialSpike,
	GlacialShatter,
	FrostNova,
	Blizzard,
	BlizzardShard,
	FrozenOrb,
	ColdArmor,
	/**
	 * Oracool, Round 3: the bow skills' arrows. One family, four elements; which SKILL an arrow
	 * belongs to rides in its var5, and decides what it does when it stops. FreezingBurst is
	 * Freezing Arrow's landing.
	 */
	SkillArrow,
	MagicArrow,
	FlameArrow,
	FrostArrow,
	GuidedArrow,
	FreezingBurst,
	/** Oracool, Round 6: every cry is cast as this. Calls CastWarcry and is gone - oracool/warcries.h. */
	Warcry,
	Null = -1,
	// clang-format on
};

enum class SpellDataFlags : uint8_t {
	// The lower 2 bytes are used to store MagicType.
	Fire = static_cast<uint8_t>(MagicType::Fire),
	Lightning = static_cast<uint8_t>(MagicType::Lightning),
	Magic = static_cast<uint8_t>(MagicType::Magic),
	Cold = static_cast<uint8_t>(MagicType::Cold),
	Targeted = 1U << 2,
	AllowedInTown = 1U << 3,
};
use_enum_as_flags(SpellDataFlags);

struct SpellData {
	const char *sNameText;
	_sfx_id sSFX;
	uint16_t bookCost10;
	uint8_t staffCost10;
	uint8_t sManaCost;
	SpellDataFlags flags;
	int8_t sBookLvl;
	int8_t sStaffLvl;
	uint8_t minInt;
	MissileID sMissiles[2];
	uint8_t sManaAdj;
	uint8_t sMinMana;
	uint8_t sStaffMin;
	uint8_t sStaffMax;

	[[nodiscard]] MagicType type() const
	{
		return static_cast<MagicType>(static_cast<std::underlying_type<SpellDataFlags>::type>(flags) & 0b11U);
	}

	[[nodiscard]] uint32_t bookCost() const
	{
		return bookCost10 * 10;
	}

	[[nodiscard]] uint16_t staffCost() const
	{
		return staffCost10 * 10;
	}

	[[nodiscard]] bool isTargeted() const
	{
		return HasAnyOf(flags, SpellDataFlags::Targeted);
	}

	[[nodiscard]] bool isAllowedInTown() const
	{
		// Oracool: user request - every spell can now be cast in town (missiles.cpp's
		// CheckMissileCol suppresses all monster/player damage while leveltype == DTYPE_TOWN, so
		// this is purely a restriction lift, not a balance change - previously only a handful of
		// utility spells (Town Portal, Identify, Infravision, ...) had the AllowedInTown flag set.
		return true;
	}
};

extern const SpellData SpellsData[];

inline const SpellData &GetSpellData(SpellID spellId)
{
	return SpellsData[static_cast<std::underlying_type<SpellID>::type>(spellId)];
}

} // namespace devilution
