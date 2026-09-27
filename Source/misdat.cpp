/**
 * @file misdat.cpp
 *
 * Implementation of data related to missiles.
 */
#include "misdat.h"

#include <cstdint>

#include "engine/load_cl2.hpp"
#include "engine/load_clx.hpp"
#include "missiles.h"
#include "oracool/sprite_import.h"
#include "oracool/warcries.h"
#include "mpq/mpq_common.hpp"
#include "utils/file_name_generator.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {
constexpr auto Physical = MissileDataFlags::Physical;
constexpr auto Fire = MissileDataFlags::Fire;
constexpr auto Lightning = MissileDataFlags::Lightning;
constexpr auto Magic = MissileDataFlags::Magic;
constexpr auto Acid = MissileDataFlags::Acid;
constexpr auto Cold = MissileDataFlags::Cold;
constexpr auto Arrow = MissileDataFlags::Arrow;
constexpr auto Invisible = MissileDataFlags::Invisible;
} // namespace

/** Data related to each missile ID. */
const MissileData MissilesData[] = {
	// clang-format off
// id                      mAddProc,                mProc,                        mlSFX,       miSFX,       mFileNum,                               flags,                 MovementDistribution;
/*Arrow*/                { &AddArrow,               &ProcessArrow,                SFX_NONE,    SFX_NONE,    MissileGraphicID::Arrow,                Physical | Arrow,      MissileMovementDistribution::Blockable   },
/*Firebolt*/             { &AddFirebolt,            &ProcessGenericProjectile,    LS_FBOLT1,   LS_FIRIMP2,  MissileGraphicID::Fireball,             Fire,                  MissileMovementDistribution::Blockable   },
/*Guardian*/             { &AddGuardian,            &ProcessGuardian,             LS_GUARD,    LS_GUARDLAN, MissileGraphicID::Guardian,             Physical,              MissileMovementDistribution::Disabled    },
/*Phasing*/              { &AddPhasing,             &ProcessTeleport,             LS_TELEPORT, SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*NovaBall*/             { &AddNovaBall,            &ProcessNovaBall,             SFX_NONE,    SFX_NONE,    MissileGraphicID::Lightning,            Lightning,             MissileMovementDistribution::Unblockable },
/*FireWall*/             { &AddFireWall,            &ProcessFireWall,             LS_WALLLOOP, LS_FIRIMP2,  MissileGraphicID::FireWall,             Fire,                  MissileMovementDistribution::Disabled    },
/*Fireball*/             { &AddFireball,            &ProcessFireball,             LS_FBOLT1,   LS_FIRIMP2,  MissileGraphicID::Fireball,             Fire,                  MissileMovementDistribution::Blockable   },
/*LightningControl*/     { &AddLightningControl,    &ProcessLightningControl,     SFX_NONE,    SFX_NONE,    MissileGraphicID::Lightning,            Lightning | Invisible, MissileMovementDistribution::Disabled    },
/*Lightning*/            { &AddLightning,           &ProcessLightning,            LS_LNING1,   LS_ELECIMP1, MissileGraphicID::Lightning,            Lightning,             MissileMovementDistribution::Disabled    },
/*MagmaBallExplosion*/   { &AddMissileExplosion,    &ProcessMissileExplosion,     SFX_NONE,    SFX_NONE,    MissileGraphicID::MagmaBallExplosion,   Physical,              MissileMovementDistribution::Disabled    },
/*TownPortal*/           { &AddTownPortal,          &ProcessTownPortal,           LS_SENTINEL, LS_ELEMENTL, MissileGraphicID::TownPortal,           Magic,                 MissileMovementDistribution::Disabled    },
/*FlashBottom*/          { &AddFlashBottom,         &ProcessFlashBottom,          LS_NOVA,     LS_ELECIMP1, MissileGraphicID::FlashBottom,          Magic,                 MissileMovementDistribution::Disabled    },
/*FlashTop*/             { &AddFlashTop,            &ProcessFlashTop,             SFX_NONE,    SFX_NONE,    MissileGraphicID::FlashTop,             Magic,                 MissileMovementDistribution::Disabled    },
/*ManaShield*/           { &AddManaShield,          nullptr,                      LS_MSHIELD,  SFX_NONE,    MissileGraphicID::ManaShield,           Magic | Invisible,     MissileMovementDistribution::Disabled    },
/*FlameWave*/            { &AddFlameWave,           &ProcessFlameWave,            SFX_NONE,    SFX_NONE,    MissileGraphicID::FireWall,             Fire,                  MissileMovementDistribution::Unblockable },
/*ChainLightning*/       { &AddChainLightning,      &ProcessChainLightning,       LS_LNING1,   LS_ELECIMP1, MissileGraphicID::Lightning,            Lightning,             MissileMovementDistribution::Disabled    },
/*ChainBall*/            { nullptr,                 nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::Lightning,            Lightning,             MissileMovementDistribution::Disabled    },
/*BloodHit*/             { nullptr,                 nullptr,                      LS_BLODSTAR, LS_BLSIMPT,  MissileGraphicID::BloodHit,             Physical,              MissileMovementDistribution::Disabled    },
/*BoneHit*/              { nullptr,                 nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::BoneHit,              Physical,              MissileMovementDistribution::Disabled    },
/*MetalHit*/             { nullptr,                 nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::MetalHit,             Physical,              MissileMovementDistribution::Disabled    },
/*Rhino*/                { &AddRhino,               &ProcessRhino,                SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical,              MissileMovementDistribution::Blockable   },
/*MagmaBall*/            { &AddMagmaBall,           &ProcessGenericProjectile,    SFX_NONE,    SFX_NONE,    MissileGraphicID::MagmaBall,            Fire,                  MissileMovementDistribution::Blockable   },
/*ThinLightningControl*/ { &AddLightningControl,    &ProcessLightningControl,     SFX_NONE,    SFX_NONE,    MissileGraphicID::ThinLightning,        Lightning | Invisible, MissileMovementDistribution::Disabled    },
/*ThinLightning*/        { &AddLightning,           &ProcessLightning,            SFX_NONE,    SFX_NONE,    MissileGraphicID::ThinLightning,        Lightning,             MissileMovementDistribution::Disabled    },
/*BloodStar*/            { &AddGenericMagicMissile, &ProcessGenericProjectile,    SFX_NONE,    SFX_NONE,    MissileGraphicID::BloodStar,            Magic,                 MissileMovementDistribution::Blockable   },
/*BloodStarExplosion*/   { &AddMissileExplosion,    &ProcessMissileExplosion,     SFX_NONE,    SFX_NONE,    MissileGraphicID::BloodStarExplosion,   Magic,                 MissileMovementDistribution::Disabled    },
/*Teleport*/             { &AddTeleport,            &ProcessTeleport,             LS_ELEMENTL, SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*FireArrow*/            { &AddElementalArrow,      &ProcessElementalArrow,       SFX_NONE,    SFX_NONE,    MissileGraphicID::FireArrow,            Fire | Arrow,          MissileMovementDistribution::Blockable   },
/*DoomSerpents*/         { nullptr,                 nullptr,                      LS_DSERP,    SFX_NONE,    MissileGraphicID::DoomSerpents,         Magic | Invisible,     MissileMovementDistribution::Disabled    },
/*FireOnly*/             { nullptr,                 nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::FireWall,             Fire,                  MissileMovementDistribution::Disabled    },
/*StoneCurse*/           { &AddStoneCurse,          &ProcessStoneCurse,           LS_SCURIMP,  SFX_NONE,    MissileGraphicID::None,                 Magic | Invisible,     MissileMovementDistribution::Disabled    },
/*BloodRitual*/          { nullptr,                 nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical,              MissileMovementDistribution::Disabled    },
/*Invisibility*/         { nullptr,                 nullptr,                      LS_INVISIBL, SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Golem*/                { &AddGolem,               nullptr,                      LS_GOLUM,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Etherealize*/          { &AddEtherealize,         &ProcessEtherealize,          LS_ETHEREAL, SFX_NONE,    MissileGraphicID::Etherealize,          Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Spurt*/                { nullptr,                 nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::Spurt,                Physical,              MissileMovementDistribution::Disabled    },
/*ApocalypseBoom*/       { &AddApocalypseBoom,      &ProcessApocalypseBoom,       SFX_NONE,    SFX_NONE,    MissileGraphicID::ApocalypseBoom,       Physical,              MissileMovementDistribution::Disabled    },
/*Healing*/              { &AddHealing,             nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*FireWallControl*/      { &AddFireWallControl,     &ProcessFireWallControl,      SFX_NONE,    SFX_NONE,    MissileGraphicID::FireWall,             Fire | Invisible,      MissileMovementDistribution::Disabled    },
/*Infravision*/          { &AddInfravision,         &ProcessInfravision,          LS_INFRAVIS, SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Identify*/             { &AddIdentify,            nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*FlameWaveControl*/     { &AddFlameWaveControl,    &ProcessFlameWaveControl,     LS_FLAMWAVE, SFX_NONE,    MissileGraphicID::FireWall,             Fire,                  MissileMovementDistribution::Disabled    },
/*Nova*/                 { &AddNova,                &ProcessNova,                 LS_NOVA,     SFX_NONE,    MissileGraphicID::Lightning,            Lightning,             MissileMovementDistribution::Disabled    },
/*Rage*/                 { &AddRage,                &ProcessRage,                 SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Apocalypse*/           { &AddApocalypse,          &ProcessApocalypse,           LS_APOC,     SFX_NONE,    MissileGraphicID::ApocalypseBoom,       Magic,                 MissileMovementDistribution::Disabled    },
/*ItemRepair*/           { &AddItemRepair,          nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*StaffRecharge*/        { &AddStaffRecharge,       nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*TrapDisarm*/           { &AddTrapDisarm,          nullptr,                      LS_TRAPDIS,  SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Inferno*/              { &AddInferno,             &ProcessInferno,              LS_SPOUTSTR, SFX_NONE,    MissileGraphicID::Inferno,              Fire,                  MissileMovementDistribution::Disabled    },
/*InfernoControl*/       { &AddInfernoControl,      &ProcessInfernoControl,       SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Fire | Invisible,      MissileMovementDistribution::Disabled    },
/*FireMan*/              { nullptr,                 nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical,              MissileMovementDistribution::Blockable   },
/*Krull*/                { nullptr,                 nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::Krull,                Fire | Arrow,          MissileMovementDistribution::Blockable   },
/*ChargedBolt*/          { &AddChargedBolt,         &ProcessChargedBolt,          LS_CBOLT,    SFX_NONE,    MissileGraphicID::ChargedBolt,          Lightning,             MissileMovementDistribution::Blockable   },
/*HolyBolt*/             { &AddHolyBolt,            &ProcessHolyBolt,             LS_HOLYBOLT, LS_ELECIMP1, MissileGraphicID::HolyBolt,             Physical,              MissileMovementDistribution::Blockable   },
/*Resurrect*/            { &AddResurrect,           nullptr,                      SFX_NONE,    LS_RESUR,    MissileGraphicID::None,                 Magic | Invisible,     MissileMovementDistribution::Disabled    },
/*Telekinesis*/          { &AddTelekinesis,         nullptr,                      LS_ETHEREAL, SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*LightningArrow*/       { &AddElementalArrow,      &ProcessElementalArrow,       SFX_NONE,    SFX_NONE,    MissileGraphicID::LightningArrow,       Lightning | Arrow,     MissileMovementDistribution::Blockable   },
/*Acid*/                 { &AddAcid,                &ProcessGenericProjectile,    LS_ACID,     SFX_NONE,    MissileGraphicID::Acid,                 Acid,                  MissileMovementDistribution::Blockable   },
/*AcidSplat*/            { &AddMissileExplosion,    &ProcessAcidSplate,           SFX_NONE,    SFX_NONE,    MissileGraphicID::AcidSplat,            Acid,                  MissileMovementDistribution::Disabled    },
/*AcidPuddle*/           { &AddAcidPuddle,          &ProcessAcidPuddle,           LS_PUDDLE,   SFX_NONE,    MissileGraphicID::AcidPuddle,           Acid,                  MissileMovementDistribution::Disabled    },
/*HealOther*/            { &AddHealOther,           nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Elemental*/            { &AddElemental,           &ProcessElemental,            LS_ELEMENTL, SFX_NONE,    MissileGraphicID::Elemental,            Fire,                  MissileMovementDistribution::Unblockable },
/*ResurrectBeam*/        { &AddResurrectBeam,       &ProcessResurrectBeam,        SFX_NONE,    SFX_NONE,    MissileGraphicID::Resurrect,            Physical,              MissileMovementDistribution::Disabled    },
/*BoneSpirit*/           { &AddBoneSpirit,          &ProcessBoneSpirit,           LS_BONESP,   LS_BSIMPCT,  MissileGraphicID::BoneSpirit,           Magic,                 MissileMovementDistribution::Blockable   },
/*WeaponExplosion*/      { &AddWeaponExplosion,     &ProcessWeaponExplosion,      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical,              MissileMovementDistribution::Disabled    },
/*RedPortal*/            { &AddRedPortal,           &ProcessRedPortal,            LS_SENTINEL, LS_ELEMENTL, MissileGraphicID::RedPortal,            Physical,              MissileMovementDistribution::Disabled    },
/*DiabloApocalypseBoom*/ { &AddApocalypseBoom,      &ProcessApocalypseBoom,       SFX_NONE,    SFX_NONE,    MissileGraphicID::DiabloApocalypseBoom, Physical,              MissileMovementDistribution::Disabled    },
/*DiabloApocalypse*/     { &AddDiabloApocalypse,    nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Mana*/                 { &AddMana,                nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Magi*/                 { &AddMagi,                nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*LightningWall*/        { &AddLightningWall,       &ProcessLightningWall,        LS_LMAG,     LS_ELECIMP1, MissileGraphicID::Lightning,            Lightning,             MissileMovementDistribution::Disabled    },
/*LightningWallControl*/ { &AddFireWallControl,     &ProcessLightningWallControl, SFX_NONE,    SFX_NONE,    MissileGraphicID::Lightning,            Lightning | Invisible, MissileMovementDistribution::Disabled    },
/*Immolation*/           { &AddNova,                &ProcessImmolation,           LS_FBOLT1,   LS_FIRIMP2,  MissileGraphicID::Fireball,             Fire,                  MissileMovementDistribution::Disabled    },
/*SpectralArrow*/        { &AddSpectralArrow,       &ProcessSpectralArrow,        SFX_NONE,    SFX_NONE,    MissileGraphicID::Arrow,                Physical | Arrow,      MissileMovementDistribution::Disabled    },
/*FireballBow*/          { &AddImmolation,          &ProcessFireball,             IS_FBALLBOW, LS_FIRIMP2,  MissileGraphicID::Fireball,             Fire,                  MissileMovementDistribution::Blockable   },
/*LightningBow*/         { &AddLightningBow,        &ProcessLightningBow,         IS_FBALLBOW, SFX_NONE,    MissileGraphicID::Lightning,            Lightning | Invisible, MissileMovementDistribution::Disabled    },
/*ChargedBoltBow*/       { &AddChargedBoltBow,      &ProcessChargedBolt,          LS_CBOLT,    SFX_NONE,    MissileGraphicID::ChargedBolt,          Lightning,             MissileMovementDistribution::Blockable   },
/*HolyBoltBow*/          { &AddHolyBolt,            &ProcessHolyBolt,             LS_HOLYBOLT, LS_ELECIMP1, MissileGraphicID::HolyBolt,             Physical,              MissileMovementDistribution::Blockable   },
/*Warp*/                 { &AddWarp,                &ProcessTeleport,             LS_ETHEREAL, SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Reflect*/              { &AddReflect,             nullptr,                      LS_MSHIELD,  SFX_NONE,    MissileGraphicID::Reflect,              Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Berserk*/              { &AddBerserk,             nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*RingOfFire*/           { &AddRingOfFire,          &ProcessRingOfFire,           SFX_NONE,    SFX_NONE,    MissileGraphicID::FireWall,             Fire | Invisible,      MissileMovementDistribution::Disabled    },
/*StealPotions*/         { &AddStealPotions,        nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*StealMana*/            { &AddStealMana,           nullptr,                      IS_CAST7,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*RingOfLightning*/      { nullptr,                 nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::Lightning,            Lightning | Invisible, MissileMovementDistribution::Disabled    },
/*Search*/               { &AddSearch,              &ProcessSearch,               SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Aura*/                 { nullptr,                 nullptr,                      SFX_NONE,    LS_ELECIMP1, MissileGraphicID::FlashBottom,          Magic | Invisible,     MissileMovementDistribution::Disabled    },
/*Aura2*/                { nullptr,                 nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::FlashTop,             Magic | Invisible,     MissileMovementDistribution::Disabled    },
/*SpiralFireball*/       { nullptr,                 nullptr,                      LS_FBOLT1,   LS_FIRIMP2,  MissileGraphicID::Fireball,             Fire,                  MissileMovementDistribution::Disabled    },
/*RuneOfFire*/           { &AddRuneOfFire,          &ProcessRune,                 SFX_NONE,    SFX_NONE,    MissileGraphicID::Rune,                 Physical,              MissileMovementDistribution::Disabled    },
/*RuneOfLight*/          { &AddRuneOfLight,         &ProcessRune,                 SFX_NONE,    SFX_NONE,    MissileGraphicID::Rune,                 Physical,              MissileMovementDistribution::Disabled    },
/*RuneOfNova*/           { &AddRuneOfNova,          &ProcessRune,                 SFX_NONE,    SFX_NONE,    MissileGraphicID::Rune,                 Physical,              MissileMovementDistribution::Disabled    },
/*RuneOfImmolation*/     { &AddRuneOfImmolation,    &ProcessRune,                 SFX_NONE,    SFX_NONE,    MissileGraphicID::Rune,                 Physical,              MissileMovementDistribution::Disabled    },
/*RuneOfStone*/          { &AddRuneOfStone,         &ProcessRune,                 SFX_NONE,    SFX_NONE,    MissileGraphicID::Rune,                 Physical,              MissileMovementDistribution::Disabled    },
/*BigExplosion*/         { &AddBigExplosion,        &ProcessBigExplosion,         LS_NESTXPLD, LS_NESTXPLD, MissileGraphicID::BigExplosion,         Fire,                  MissileMovementDistribution::Disabled    },
/*HorkSpawn*/            { &AddHorkSpawn,           &ProcessHorkSpawn,            SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*Jester*/               { &AddJester,              nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*OpenNest*/             { &AddOpenNest,            nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*OrangeFlare*/          { &AddGenericMagicMissile, &ProcessGenericProjectile,    SFX_NONE,    SFX_NONE,    MissileGraphicID::OrangeFlare,          Magic,                 MissileMovementDistribution::Blockable   },
/*BlueFlare*/            { &AddGenericMagicMissile, &ProcessGenericProjectile,    SFX_NONE,    SFX_NONE,    MissileGraphicID::BlueFlare2,           Magic,                 MissileMovementDistribution::Blockable   },
/*RedFlare*/             { &AddGenericMagicMissile, &ProcessGenericProjectile,    SFX_NONE,    SFX_NONE,    MissileGraphicID::RedFlare,             Magic,                 MissileMovementDistribution::Blockable   },
/*YellowFlare*/          { &AddGenericMagicMissile, &ProcessGenericProjectile,    SFX_NONE,    SFX_NONE,    MissileGraphicID::YellowFlare,          Magic,                 MissileMovementDistribution::Blockable   },
/*BlueFlare2*/           { &AddGenericMagicMissile, &ProcessGenericProjectile,    SFX_NONE,    SFX_NONE,    MissileGraphicID::BlueFlare2,           Magic,                 MissileMovementDistribution::Blockable   },
/*YellowExplosion*/      { &AddMissileExplosion,    &ProcessMissileExplosion,     LS_FIRIMP2,  SFX_NONE,    MissileGraphicID::YellowFlareExplosion, Physical,              MissileMovementDistribution::Disabled    },
/*RedExplosion*/         { &AddMissileExplosion,    &ProcessMissileExplosion,     LS_FIRIMP2,  SFX_NONE,    MissileGraphicID::RedFlareExplosion,    Physical,              MissileMovementDistribution::Disabled    },
/*BlueExplosion*/        { &AddMissileExplosion,    &ProcessMissileExplosion,     LS_FIRIMP2,  SFX_NONE,    MissileGraphicID::BlueFlareExplosion,   Physical,              MissileMovementDistribution::Disabled    },
/*BlueExplosion2*/       { &AddMissileExplosion,    &ProcessMissileExplosion,     LS_FIRIMP2,  SFX_NONE,    MissileGraphicID::BlueFlareExplosion2,  Physical,              MissileMovementDistribution::Disabled    },
/*OrangeExplosion*/      { &AddMissileExplosion,    &ProcessMissileExplosion,     LS_FIRIMP2,  SFX_NONE,    MissileGraphicID::OrangeFlareExplosion, Physical,              MissileMovementDistribution::Disabled    },
// Oracool: the Paladin's Blessed Hammer. Movement Disabled because it does NOT travel on a velocity
// vector - ProcessBlessedHammer writes position.traveled itself each tick from an angle and a radius,
// which is the one thing no other missile in this table does. It draws its own spin sheet since
// 2026-09-11 (it wore items\mace.cel painted gold before). MAGIC damage since the same day (user: "in D2 it does magic dmg. Let's
// make it Magic DMG here as well") - it was Physical, on the reasoning that it is a hammer.
/*BlessedHammer*/        { &AddBlessedHammer,       &ProcessBlessedHammer,        IS_CAST2,    SFX_NONE,    MissileGraphicID::BlessedHammerSpin,    Magic,                 MissileMovementDistribution::Disabled    },
// Oracool: Blessed Shield's throw. The sprite named here is only the last fallback - AddBlessedShieldThrow
// swaps in its own spin sheet (blessed_shield_spin, delivered 2026-09-11) and, without that,
// items\shield.cel, the tumble a dropped shield plays. HolyBolt was chosen when the base game had no
// shield missile art at all. Blockable so a wall stops it, like every other thrown thing. MAGIC damage
// since 2026-09-11 (user: "make blessed shield Magic dmg type as well"), as the hammer; it was Physical.
// It bounces on to two more monsters since the same day (ProcessBlessedShieldThrow), with no splash.
/*BlessedShieldThrow*/   { &AddBlessedShieldThrow,  &ProcessBlessedShieldThrow,   IS_CAST2,    SFX_NONE,    MissileGraphicID::HolyBolt,             Magic,                 MissileMovementDistribution::Blockable   },
// Oracool: Fist of the Heavens' descent. Same story - AddFallingMace swaps in its own bolt sheet
// (fist_of_heavens_bolt, 2026-09-11) and, without that, items\mace.cel's tumble. Invisible would be
// wrong; this one is the whole point of the effect.
/*FallingMace*/          { &AddFallingMace,         &ProcessFallingMace,          SFX_NONE,    SFX_NONE,    MissileGraphicID::ApocalypseBoom,       Physical,              MissileMovementDistribution::Disabled    },
// Oracool: Fist of the Heavens' mini-Nova bolt - NovaBall's own add and process functions with
// ChargedBolt's smaller sprite. See the note at MissileID::MiniNovaBall.
/*MiniNovaBall*/         { &AddNovaBall,            &ProcessNovaBall,             SFX_NONE,    SFX_NONE,    MissileGraphicID::ChargedBolt,          Lightning,             MissileMovementDistribution::Unblockable },
// Oracool, Round 1: the first cold missile, and the first user of DamageType::Cold. Firebolt's own
// add and process functions - a bolt that flies at a target and bursts is a bolt whatever it is made
// of, and the difference between the two is entirely in the flags, the art and what the hit does.
//
// LS_FBOLT1 / LS_FIRIMP2 are Firebolt's sounds, and now only the FALLBACK: since 2026-09-11 a
// player's cold missile plays its own class-tree cue (sfx\skills\sorcerer\cold-spells) in their
// place - oracool::ColdMissileCueSkill says which. They still sound for a cold missile with no cue.
/*IceBolt*/              { &AddFirebolt,            &ProcessGenericProjectile,    LS_FBOLT1,   LS_FIRIMP2,  MissileGraphicID::IceBolt,              Cold,                  MissileMovementDistribution::Blockable   },
// Oracool, Round 2. Every one of these is described at its Add function.
/*IceImpact*/            { &AddMissileExplosion,    &ProcessMissileExplosion,     SFX_NONE,    SFX_NONE,    MissileGraphicID::IceImpact,            Cold,                  MissileMovementDistribution::Disabled    },
/*IceBlast*/             { &AddIceBlast,            &ProcessGenericProjectile,    LS_FBOLT1,   LS_FIRIMP2,  MissileGraphicID::IceBlast,             Cold,                  MissileMovementDistribution::Blockable   },
/*GlacialSpike*/         { &AddGlacialSpike,        &ProcessGenericProjectile,    LS_FBOLT1,   LS_FIRIMP2,  MissileGraphicID::GlacialSpike,         Cold,                  MissileMovementDistribution::Blockable   },
/*GlacialShatter*/       { &AddGlacialShatter,      &ProcessMissileExplosion,     SFX_NONE,    SFX_NONE,    MissileGraphicID::GlacialShatter,       Cold,                  MissileMovementDistribution::Disabled    },
/*FrostNova*/            { &AddFrostNova,           &ProcessFrostNova,            LS_NOVA,     SFX_NONE,    MissileGraphicID::FrostNova,            Cold,                  MissileMovementDistribution::Disabled    },
/*Blizzard*/             { &AddBlizzard,            &ProcessBlizzard,             LS_NOVA,     SFX_NONE,    MissileGraphicID::BlizzardShard,        Cold | Invisible,      MissileMovementDistribution::Disabled    },
/*BlizzardShard*/        { &AddBlizzardShard,       &ProcessBlizzardShard,        SFX_NONE,    LS_FIRIMP2,  MissileGraphicID::BlizzardShard,        Cold,                  MissileMovementDistribution::Disabled    },
/*FrozenOrb*/            { &AddFrozenOrb,           &ProcessFrozenOrb,            LS_FBOLT1,   SFX_NONE,    MissileGraphicID::FrozenOrb,            Cold,                  MissileMovementDistribution::Unblockable },
/*ColdArmor*/            { &AddColdArmor,           &ProcessColdArmor,            LS_MSHIELD,  SFX_NONE,    MissileGraphicID::IceArmorShell,        Cold | Invisible,      MissileMovementDistribution::Disabled    },
// Oracool, Round 3: the bow skills' arrows. One Add and one Process for the family; see them. The
// Arrow flag is what makes these ARROWS to the hit roll - ranged to-hit, armour pierce, knockback.
/*SkillArrow*/           { &AddRogueArrow,          &ProcessRogueArrow,           SFX_NONE,    SFX_NONE,    MissileGraphicID::Arrow,                Physical | Arrow,      MissileMovementDistribution::Blockable   },
/*MagicArrow*/           { &AddRogueArrow,          &ProcessRogueArrow,           SFX_NONE,    SFX_NONE,    MissileGraphicID::Arrow,                Magic | Arrow,         MissileMovementDistribution::Blockable   },
/*FlameArrow*/           { &AddRogueArrow,          &ProcessRogueArrow,           SFX_NONE,    SFX_NONE,    MissileGraphicID::FireArrow,            Fire | Arrow,          MissileMovementDistribution::Blockable   },
/*FrostArrow*/           { &AddRogueArrow,          &ProcessRogueArrow,           SFX_NONE,    SFX_NONE,    MissileGraphicID::FrostArrow,           Cold | Arrow,          MissileMovementDistribution::Blockable   },
/*GuidedArrow*/          { &AddRogueArrow,          &ProcessRogueArrow,           SFX_NONE,    SFX_NONE,    MissileGraphicID::Arrow,                Physical | Arrow,      MissileMovementDistribution::Blockable   },
/*FreezingBurst*/        { &AddMissileExplosion,    &ProcessMissileExplosion,     SFX_NONE,    SFX_NONE,    MissileGraphicID::FreezingBurst,        Cold,                  MissileMovementDistribution::Disabled    },
/*Warcry*/               { &oracool::AddWarcry,     nullptr,                      SFX_NONE,    SFX_NONE,    MissileGraphicID::None,                 Physical | Invisible,  MissileMovementDistribution::Disabled    },
/*WarcryRing*/           { &AddWarcryRing,          &ProcessWarcryRing,           SFX_NONE,    SFX_NONE,    MissileGraphicID::WarcryRing,           Physical,              MissileMovementDistribution::Disabled    },
// Oracool (2026-09-11): Blessed Shield's hit flash - holyexpl, scaled by AddBlessedShieldImpact, lit and
// timed by the generic explosion. It deals nothing; the shield dealt the damage.
/*BlessedShieldImpact*/  { &AddBlessedShieldImpact, &ProcessMissileExplosion,     SFX_NONE,    SFX_NONE,    MissileGraphicID::HolyBoltExplosion,    Magic,                 MissileMovementDistribution::Disabled    },
// Oracool (2026-09-14, RfA-16): the census skills' art. Drawn only - they strike nothing.
/*AcidJavelin*/          { &AddAcidJavelin,         &ProcessAcidJavelin,          SFX_NONE,    SFX_NONE,    MissileGraphicID::AcidJavelin,          Physical,              MissileMovementDistribution::Disabled    },
/*AcidCloud*/            { &AddCensusEffect,        &ProcessCensusEffect,         SFX_NONE,    SFX_NONE,    MissileGraphicID::AcidCloud,            Physical,              MissileMovementDistribution::Disabled    },
/*MeteorFall*/           { &AddCensusEffect,        &ProcessCensusEffect,         SFX_NONE,    SFX_NONE,    MissileGraphicID::Meteor,               Physical,              MissileMovementDistribution::Disabled    },
/*MeteorImpact*/         { &AddCensusEffect,        &ProcessCensusEffect,         SFX_NONE,    SFX_NONE,    MissileGraphicID::MeteorImpact,         Physical,              MissileMovementDistribution::Disabled    },
/*ThunderBolt*/          { &AddCensusEffect,        &ProcessCensusEffect,         SFX_NONE,    SFX_NONE,    MissileGraphicID::ThunderBolt,          Physical,              MissileMovementDistribution::Disabled    },
// The Necromancer's (2026-09-18, RfA-17 batch 38): drawn only, like the census effects. The three bolts fly the javelin's way.
/*BoneToothBolt*/        { &AddAcidJavelin,         &ProcessAcidJavelin,          SFX_NONE,    SFX_NONE,    MissileGraphicID::BoneTooth,            Magic,                 MissileMovementDistribution::Disabled    },
/*BoneSpearBolt*/        { &AddAcidJavelin,         &ProcessAcidJavelin,          SFX_NONE,    SFX_NONE,    MissileGraphicID::BoneSpear,            Magic,                 MissileMovementDistribution::Disabled    },
/*PoisonBoltFlight*/     { &AddAcidJavelin,         &ProcessAcidJavelin,          SFX_NONE,    SFX_NONE,    MissileGraphicID::PoisonBolt,           Acid,                  MissileMovementDistribution::Disabled    },
/*BoneHitBurst*/         { &AddCensusEffect,        &ProcessCensusEffect,         SFX_NONE,    SFX_NONE,    MissileGraphicID::BoneHitNecro,              Magic,                 MissileMovementDistribution::Disabled    },
/*BoneWallEffect*/       { &AddCensusEffect,        &ProcessCensusEffect,         SFX_NONE,    SFX_NONE,    MissileGraphicID::BoneWall,             Magic,                 MissileMovementDistribution::Disabled    },
/*BoneSpikesEffect*/     { &AddCensusEffect,        &ProcessCensusEffect,         SFX_NONE,    SFX_NONE,    MissileGraphicID::BoneSpikes,           Magic,                 MissileMovementDistribution::Disabled    },
/*BoneStormEffect*/      { &AddCensusEffect,        &ProcessCensusEffect,         SFX_NONE,    SFX_NONE,    MissileGraphicID::BoneStorm,            Magic,                 MissileMovementDistribution::Disabled    },
/*CorpseBurst*/          { &AddCensusEffect,        &ProcessCensusEffect,         SFX_NONE,    SFX_NONE,    MissileGraphicID::CorpseExplosion,      Physical,              MissileMovementDistribution::Disabled    },
/*RaiseDeadEffect*/      { &AddCensusEffect,        &ProcessCensusEffect,         SFX_NONE,    SFX_NONE,    MissileGraphicID::RaiseDead,            Magic,                 MissileMovementDistribution::Disabled    },
/*CurseCastEffect*/      { &AddCensusEffect,        &ProcessCensusEffect,         SFX_NONE,    SFX_NONE,    MissileGraphicID::CurseCast,            Magic,                 MissileMovementDistribution::Disabled    },
/*RiftPortalGold*/       { &AddRiftPortal,          &ProcessRiftPortal,           SFX_NONE,    SFX_NONE,    MissileGraphicID::RiftPortalGold,       Magic,                 MissileMovementDistribution::Disabled    },
/*RiftPortalPurple*/     { &AddRiftPortal,          &ProcessRiftPortal,           SFX_NONE,    SFX_NONE,    MissileGraphicID::RiftPortalPurple,     Magic,                 MissileMovementDistribution::Disabled    },
	// clang-format on
};

// Oracool: MissilesData is indexed by MissileID positionally, so a row missing or a row too many
// silently shifts every missile past it onto another's behaviour. Pinned after Round 6 appended
// MissileID::Warcry - which, at the enum's old int8_t, wrapped to -128 and read this table from
// before its first row. See MissileID in spelldat.h for that story.
static_assert(sizeof(MissilesData) / sizeof(MissilesData[0]) == static_cast<size_t>(MissileID::RiftPortalPurple) + 1,
    "MissilesData needs a row for every MissileID, in the enum's order");

namespace {

constexpr std::array<uint8_t, 16> Repeat(uint8_t v) // NOLINT(readability-identifier-length)
{
	return { v, v, v, v, v, v, v, v, v, v, v, v, v, v, v, v };
}

const std::array<uint8_t, 16> MissileAnimDelays[] {
	{},
	Repeat(1),
	Repeat(2),
	{ 0, 1 },
	{ 1 },
};

const std::array<uint8_t, 16> MissileAnimLengths[] {
	{},
	Repeat(1),
	Repeat(4),
	Repeat(5),
	Repeat(6),
	Repeat(7),
	Repeat(8),
	Repeat(9),
	Repeat(10),
	Repeat(12),
	Repeat(13),
	Repeat(14),
	Repeat(15),
	Repeat(16),
	Repeat(17),
	Repeat(19),
	Repeat(20),
	{ 9, 4 },
	{ 15, 14, 3 },
	{ 13, 11 },
	{ 16, 16, 16, 16, 16, 16, 16, 16, 8 },
	Repeat(2), // Oracool 2026-09-26: ice_ground's two variants
};

constexpr uint8_t AnimLen_0 = 0;        // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_1 = 1;        // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_4 = 2;        // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_5 = 3;        // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_6 = 4;        // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_7 = 5;        // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_8 = 6;        // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_9 = 7;        // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_10 = 8;       // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_12 = 9;       // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_13 = 10;      // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_14 = 11;      // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_15 = 12;      // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_16 = 13;      // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_17 = 14;      // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_19 = 15;      // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_20 = 16;      // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_9_4 = 17;     // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_15_14_3 = 18; // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_13_11 = 19;   // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_16x8_8 = 20;  // NOLINT(readability-identifier-naming)
constexpr uint8_t AnimLen_2 = 21;       // NOLINT(readability-identifier-naming)

} // namespace

/** Data related to each missile graphic ID. */
MissileFileData MissileSpriteData[] = {
	// clang-format off
// id                          sprites,   animWidth,  animWidth2, name,        animFAmt, flags,                               animDelayIdx, animLenIdx
/*Arrow*/                    { {},               96,          16, "arrows",           1, MissileGraphicsFlags::NotAnimated,              0, AnimLen_16      },
/*Fireball*/                 { {},               96,          16, "fireba",          16, MissileGraphicsFlags::None,                     0, AnimLen_14      },
/*Guardian*/                 { {},               96,          16, "guard",            3, MissileGraphicsFlags::None,                     1, AnimLen_15_14_3 },
/*Lightning*/                { {},               96,          16, "lghning",          1, MissileGraphicsFlags::None,                     0, AnimLen_8       },
/*FireWall*/                 { {},              128,          32, "firewal",          2, MissileGraphicsFlags::None,                     0, AnimLen_13_11   },
/*MagmaBallExplosion*/       { {},              128,          32, "magblos",          1, MissileGraphicsFlags::None,                     1, AnimLen_10      },
/*TownPortal*/               { {},               96,          16, "portal",           2, MissileGraphicsFlags::None,                     3, AnimLen_16      },
/*FlashBottom*/              { {},              160,          48, "bluexfr",          1, MissileGraphicsFlags::None,                     0, AnimLen_19      },
/*FlashTop*/                 { {},              160,          48, "bluexbk",          1, MissileGraphicsFlags::None,                     0, AnimLen_19      },
/*ManaShield*/               { {},               96,          16, "manashld",         1, MissileGraphicsFlags::NotAnimated,              0, AnimLen_1       },
/*BloodHit*/                 { {},               96,          16, {},                 4, MissileGraphicsFlags::None,                     0, AnimLen_15      },
/*BoneHit*/                  { {},              128,          32, {},                 3, MissileGraphicsFlags::None,                     2, AnimLen_8       },
/*MetalHit*/                 { {},               96,          16, {},                 3, MissileGraphicsFlags::None,                     2, AnimLen_10      },
/*FireArrow*/                { {},               96,          16, "farrow",          16, MissileGraphicsFlags::None,                     0, AnimLen_4       },
/*DoomSerpents*/             { {},               96,          16, "doom",             9, MissileGraphicsFlags::MonsterOwned,             1, AnimLen_15      },
/*Golem*/                    { {},                0,           0, {},                 1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_0       },
/*Spurt*/                    { {},              128,          32, {},                 2, MissileGraphicsFlags::None,                     2, AnimLen_8       },
/*ApocalypseBoom*/           { {},               96,          16, "newexp",           1, MissileGraphicsFlags::None,                     1, AnimLen_15      },
/*StoneCurseShatter*/        { {},              128,          32, "shatter1",         1, MissileGraphicsFlags::None,                     1, AnimLen_12      },
/*BigExplosion*/             { {},              160,          48, "bigexp",           1, MissileGraphicsFlags::None,                     0, AnimLen_15      },
/*Inferno*/                  { {},               96,          16, "inferno",          1, MissileGraphicsFlags::None,                     0, AnimLen_20      },
/*ThinLightning*/            { {},               96,          16, "thinlght",         1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_8       },
/*BloodStar*/                { {},              128,          32, "flare",            1, MissileGraphicsFlags::None,                     0, AnimLen_16      },
/*BloodStarExplosion*/       { {},              128,          32, "flareexp",         1, MissileGraphicsFlags::None,                     0, AnimLen_7       },
/*MagmaBall*/                { {},              128,          32, "magball",          8, MissileGraphicsFlags::MonsterOwned,             1, AnimLen_16      },
/*Krull*/                    { {},               96,          16, "krull",            1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_14      },
/*ChargedBolt*/              { {},               64,           0, "miniltng",         1, MissileGraphicsFlags::None,                     1, AnimLen_8       },
/*HolyBolt*/                 { {},               96,          16, "holy",            16, MissileGraphicsFlags::None,                     4, AnimLen_14      },
/*HolyBoltExplosion*/        { {},              160,          48, "holyexpl",         1, MissileGraphicsFlags::None,                     0, AnimLen_8       },
/*LightningArrow*/           { {},               96,          16, "larrow",          16, MissileGraphicsFlags::None,                     0, AnimLen_4       },
/*FireArrowExplosion*/       { {},               64,           0, {},                 1, MissileGraphicsFlags::None,                     0, AnimLen_6       },
/*Acid*/                     { {},               96,          16, "acidbf",          16, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_8       },
/*AcidSplat*/                { {},               96,          16, "acidspla",         1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_8       },
/*AcidPuddle*/               { {},               96,          16, "acidpud",          2, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_9_4     },
/*Etherealize*/              { {},               96,          16, {},                 1, MissileGraphicsFlags::None,                     0, AnimLen_1       },
/*Elemental*/                { {},               96,          16, "firerun",          8, MissileGraphicsFlags::None,                     1, AnimLen_12      },
/*Resurrect*/                { {},               96,          16, "ressur1",          1, MissileGraphicsFlags::None,                     0, AnimLen_16      },
/*BoneSpirit*/               { {},               96,          16, "sklball",          9, MissileGraphicsFlags::None,                     1, AnimLen_16x8_8  },
/*RedPortal*/                { {},               96,          16, "rportal",          2, MissileGraphicsFlags::None,                     0, AnimLen_16      },
/*DiabloApocalypseBoom*/     { {},              160,          48, "fireplar",         1, MissileGraphicsFlags::MonsterOwned,             1, AnimLen_17      },
/*BloodStarBlue*/            { {},               96,          16, "scubmisb",         1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_16      },
/*BloodStarBlueExplosion*/   { {},              128,          32, "scbsexpb",         1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_6       },
/*BloodStarYellow*/          { {},               96,          16, "scubmisc",         1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_16      },
/*BloodStarYellowExplosion*/ { {},              128,          32, "scbsexpc",         1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_6       },
/*BloodStarRed*/             { {},               96,          16, "scubmisd",         1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_16      },
/*BloodStarRedExplosion*/    { {},              128,          32, "scbsexpd",         1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_6       },
/*HorkSpawn*/                { {},               96,          16, "spawns",           8, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_9       },
/*Reflect*/                  { {},              160,          64, "reflect",          1, MissileGraphicsFlags::NotAnimated,              0, AnimLen_1       },
/*OrangeFlare*/              { {},               96,           8, "ms_ora",          16, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_15      },
/*BlueFlare*/                { {},               96,           8, "ms_bla",          16, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_15      },
/*RedFlare*/                 { {},               96,           8, "ms_reb",          16, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_15      },
/*YellowFlare*/              { {},               96,           8, "ms_yeb",          16, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_15      },
/*Rune*/                     { {},               96,           8, "rglows1",          1, MissileGraphicsFlags::None,                     0, AnimLen_10      },
/*YellowFlareExplosion*/     { {},              220,          78, "ex_yel2",          1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_10      },
/*BlueFlareExplosion*/       { {},              212,          86, "ex_blu2",          1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_10      },
/*RedFlareExplosion*/        { {},              292,         114, "ex_red3",          1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_7       },
/*BlueFlare2*/               { {},               96,           8, "ms_blb",          16, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_15      },
/*OrangeFlareExplosion*/     { {},               96,         -12, "ex_ora1",          1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_13      },
/*BlueFlareExplosion2*/      { {},              292,         114, "ex_blu3",          1, MissileGraphicsFlags::MonsterOwned,             0, AnimLen_7       },
// Oracool, the Cold pack. The names are the PNG sheets' own (missiles\ice_bolt.png), so LoadGFX's
// import finds them by the same name it would have used for a .cl2 - see oracool/sprite_import.h.
//
// animFAmt is the DIRECTION count, not the frame count: 16 for the bolt because it is drawn once per
// facing, 1 for the impact because a burst of frost looks the same from every side. animWidth2 is
// the horizontal draw offset, half the frame width less the 32px tile half-width, exactly as
// Fireball's 96/16 pair is.
/*IceBolt*/                  { {},               96,          16, "ice_bolt",        16, MissileGraphicsFlags::None,                     0, AnimLen_16      },
/*IceImpact*/                { {},               96,          16, "ice_impact",       1, MissileGraphicsFlags::None,                     1, AnimLen_10      },
// Round 2. Widths are the brief's; animWidth2 is (frame - 64) / 2, the same rule as Fireball's
// 96/16 and Fire Wall's 128/32, so a sprite sits on its tile whatever its frame size.
/*IceBlast*/                 { {},               96,          16, "ice_blast",       16, MissileGraphicsFlags::None,                     0, AnimLen_16      },
/*GlacialSpike*/             { {},              128,          32, "glacial_spike",   16, MissileGraphicsFlags::None,                     0, AnimLen_16      },
/*GlacialShatter*/           { {},              128,          32, "glacial_shatter",  1, MissileGraphicsFlags::None,                     1, AnimLen_12      },
/*FrostNova*/                { {},              160,          48, "frost_nova",       1, MissileGraphicsFlags::None,                     0, AnimLen_19      },
/*BlizzardShard*/            { {},              128,          32, "blizzard_shard",   1, MissileGraphicsFlags::None,                     1, AnimLen_13      },
/*FrozenOrb*/                { {},              128,          32, "frozen_orb",      16, MissileGraphicsFlags::None,                     1, AnimLen_16      },
/*IceArmorShell*/            { {},               96,          16, "ice_armor_shell",  1, MissileGraphicsFlags::None,                     1, AnimLen_8       },
/*IceArmorBreak*/            { {},               96,          16, "ice_armor_break",  1, MissileGraphicsFlags::None,                     1, AnimLen_10      },
// Round 3. The frost arrow is cut like Fire Arrow's "farrow" - sixteen facings, four frames each.
/*FrostArrow*/               { {},               96,          16, "frost_arrow",     16, MissileGraphicsFlags::None,                     0, AnimLen_4       },
/*FreezingBurst*/            { {},              128,          32, "freezing_burst",   1, MissileGraphicsFlags::None,                     1, AnimLen_12      },
/*BlessedHammerSpin*/        { {},               48,          -8, "blessed_hammer_spin", 1, MissileGraphicsFlags::None,                 1, AnimLen_16      },
// The briefs' sheets (2026-09-11), all nine delivered. They stay PngOnly - no .cl2 stands behind
// them - so a build without one gets no sprite and its caller falls back. Sizes are the briefs'; animWidth2 is
// (frame - 64) / 2 as above. The bolt's cells are 64 wide and 128 tall - the height is the sheet's.
/*FistOfHeavensBolt*/        { {},               64,           0, "fist_of_heavens_bolt", 1, MissileGraphicsFlags::PngOnly,             1, AnimLen_10      },
/*BlessedShieldSpin*/        { {},               48,          -8, "blessed_shield_spin", 1, MissileGraphicsFlags::PngOnly,              1, AnimLen_16      },
/*HolySpark*/                { {},               64,           0, "holy_spark",        1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*MagicArrowLight*/          { {},               96,          16, "magic_arrow",      16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_4       },
/*GuidedArrowGold*/          { {},               96,          16, "guided_arrow",     16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_4       },
/*WarcryRing*/               { {},              160,          48, "warcry_ring",       1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*HitFire*/                  { {},               64,           0, "hit_fire",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_6       },
/*HitLightning*/             { {},               64,           0, "hit_lightning",     1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_6       },
/*HitCold*/                  { {},               64,           0, "hit_cold",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_6       },
// RfA-16 (2026-09-14), batch 35. Cells and frame counts are the delivery's notes; animWidth2 is (frame - 64) / 2.
// The javelin is sixteen facings of one frame. Cloud, meteor and bolt step every second tick (delay row 2).
/*ThrownSword*/              { {},               48,          -8, "thrown_sword",      1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*ThrownAxe*/                { {},               48,          -8, "thrown_axe",        1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*AcidJavelin*/              { {},               64,           0, "acid_javelin",     16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_1       },
/*AcidCloud*/                { {},              128,          32, "acid_cloud",        1, MissileGraphicsFlags::PngOnly,                 2, AnimLen_12      },
/*Meteor*/                   { {},               96,          16, "meteor",            1, MissileGraphicsFlags::PngOnly,                 2, AnimLen_10      },
/*MeteorImpact*/             { {},              160,          48, "meteor_impact",     1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_14      },
/*ThunderBolt*/              { {},               64,           0, "thunder_bolt",      1, MissileGraphicsFlags::PngOnly,                 2, AnimLen_8       },
/*Grenade*/                  { {},               32,         -16, "grenade",           1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
// RfA-17 (2026-09-18), batch 38 - the Necromancer. Cells and frame counts are the delivery's notes; animWidth2 is (frame - 64) / 2.
// The tooth, the spear and the poison bolt are sixteen facings of one frame; the wall, the raise step every second tick.
/*BoneTooth*/                { {},               32,         -16, "bone_tooth",       16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_1       },
/*BoneSpear*/                { {},               96,          16, "bone_spear",       16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_1       },
/*BoneSpiritNecro*/          { {},               64,           0, "bone_spirit",      16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_6       },
/*BoneHitNecro*/             { {},               64,           0, "bone_hit",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*BoneWall*/                 { {},               64,           0, "bone_wall",         1, MissileGraphicsFlags::PngOnly,                 2, AnimLen_10      },
/*BoneSpikes*/               { {},               96,          16, "bone_spikes",       1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_10      },
/*BoneArmorShell*/           { {},               96,          16, "bone_armor_shell",  1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*BoneStorm*/                { {},              160,          48, "bone_storm",        1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*PoisonBolt*/               { {},               32,         -16, "poison_bolt",      16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_1       },
/*CorpseExplosion*/          { {},              160,          48, "corpse_explosion",  1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*RaiseDead*/                { {},               96,          16, "raise_dead",        1, MissileGraphicsFlags::PngOnly,                 2, AnimLen_12      },
/*CurseCast*/                { {},              192,          64, "curse_cast",        1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
// Oracool 2026-09-20: the Rift Monument's portals - the town portal's own animation (16 frames, delay 3), one strip each,
// resampled to 90% by tools/BuildRiftPortals.ps1 (user, 2026-09-20: "scale down to 90%"): 86x115 frames. animWidth2 is
// the draw's left shift, so the frame's centre sits at (width / 2 - animWidth2) right of the tile: 96 / 2 - 16 = 32 before,
// and 86 / 2 - 11 = 32 now - the oval stays where it was.
/*RiftPortalGold*/           { {},               86,          11, "portal_gold",       2, MissileGraphicsFlags::PngOnly,                 3, AnimLen_16      }, // vanilla's portal1/portal2 recoloured: row 0 opens, row 1 stands
/*RiftPortalPurple*/         { {},               86,          11, "portal_purple",     2, MissileGraphicsFlags::PngOnly,                 3, AnimLen_16      },
/*TownPortalInTown*/         { {},               86,          11, "portal_town",       2, MissileGraphicsFlags::PngOnly,                 3, AnimLen_16      }, // the town's own portal at 90%, shadow stripped (2026-09-20)
// Oracool 2026-09-26: the frozen floor, two 128x128 frames that are two VARIANTS of one patch (the Cold brief), not an
// animation - Brittle Ground picks one a tile and holds it (rfa12_actives). The ellipse's centre is y 80 of 128.
/*IceGround*/                { {},              128,          32, "ice_ground",        1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_2       },
// Oracool 2026-09-26: RfA-27 batches 52-57. Cells, frames, directions and anchors are the delivery notes; animWidth2 is (frame - 64) / 2,
// animFAmt the direction count. Every sheet is one of the lengths the table already had. The draw anchors live in
// missiles.cpp (ArtEffectAnchor), beside AddArtEffect, which applies them.
// RfA-27 batch 52, strike flashes
/*VotiveStrike*/             { {},              96,          16, "votive_strike",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*JudgmentStrike*/           { {},              96,          16, "judgment_strike",        1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*OathbrandStrike*/          { {},              96,          16, "oathbrand_strike",       1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*RendStrike*/               { {},              96,          16, "rend_strike",            1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*ClaspOfRuin*/              { {},              96,          16, "clasp_of_ruin",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*HammerOfTheAncients*/      { {},             160,          48, "hammer_of_the_ancients",  1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_10      },
/*CinderTouch*/              { {},              96,          16, "cinder_touch",           1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*StaffFlurry*/              { {},              96,          16, "staff_flurry",           1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_6       },
/*StaffEcho*/                { {},              96,          16, "staff_echo",             1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*TigerClaw*/                { {},              96,          16, "tiger_claw",             1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*PressurePoint*/            { {},              96,          16, "pressure_point",         1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*SevenSidedStrike*/         { {},              96,          16, "seven_sided_strike",     1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*ExplodingPalm*/            { {},              96,          16, "exploding_palm",         1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
// RfA-27 batch 53, arcs, sweeps and thrusts
/*CleaveArc*/                { {},             128,          32, "cleave_arc",            16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*BackhandArc*/              { {},             128,          32, "backhand_arc",          16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*AegisSlam*/                { {},             128,          32, "aegis_slam",            16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*SweepArc*/                 { {},             128,          32, "sweep_arc",             16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*LowBranch*/                { {},             128,          32, "low_branch",            16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*RearwardReach*/            { {},             128,          32, "rearward_reach",        16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*TurningPike*/              { {},             128,          32, "turning_pike",          16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*CrusadeSweep*/             { {},             160,          48, "crusade_sweep",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*DragonTailSweep*/          { {},             160,          48, "dragon_tail_sweep",      1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*WhirlingKick*/             { {},             160,          48, "whirling_kick",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*HolyLance*/                { {},             192,          64, "holy_lance",            16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*LongThrust*/               { {},             192,          64, "long_thrust",           16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_6       },
/*ReapingPoint*/             { {},             192,          64, "reaping_point",         16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_6       },
/*ChillTouch*/               { {},             192,          64, "chill_touch",           16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*FurnaceMouth*/             { {},             192,          64, "furnace_mouth",         16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
// RfA-27 batch 54, projectiles and travelling waves
/*BarbedArrow*/              { {},              96,          16, "barbed_arrow",          16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_4       },
/*ShockArrow*/               { {},              96,          16, "shock_arrow",           16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_4       },
/*PiercingArrow*/            { {},              96,          16, "piercing_arrow",        16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_4       },
/*CripplingArrow*/           { {},              96,          16, "crippling_arrow",       16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_4       },
/*BarrageArrow*/             { {},              96,          16, "barrage_arrow",         16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_4       },
/*PhantomArrow*/             { {},              96,          16, "phantom_arrow",         16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_4       },
/*Harpoon*/                  { {},              96,          16, "harpoon",               16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_1       },
/*AnchorJavelin*/            { {},              96,          16, "anchor_javelin",        16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_1       },
/*ValkyrieSpear*/            { {},             128,          32, "valkyrie_spear",        16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_1       },
/*IceNeedle*/                { {},              32,         -16, "ice_needle",            16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_1       },
/*IceLance*/                 { {},              96,          16, "ice_lance",             16, MissileGraphicsFlags::PngOnly,                 0, AnimLen_1       },
/*ArcSpark*/                 { {},              64,           0, "arc_spark",              1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_6       },
/*ChiWave*/                  { {},              64,           0, "chi_wave",               1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_6       },
/*SoulWisp*/                 { {},              32,         -16, "soul_wisp",              1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_6       },
/*SeismicWave*/              { {},             128,          32, "seismic_wave",          16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_4       },
/*HeavenSplitterWave*/       { {},             128,          32, "heaven_splitter_wave",  16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_4       },
/*DragonsWrathWave*/         { {},             128,          32, "dragons_wrath_wave",    16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_4       },
/*WhiteoutWall*/             { {},             192,          64, "whiteout_wall",         16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_4       },
// RfA-27 batch 55, ground bursts, pillars and fields
/*GroundStomp*/              { {},             160,          48, "ground_stomp",           1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*MountainPole*/             { {},             160,          48, "mountain_pole",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*FlameRing*/                { {},             160,          48, "flame_ring",             1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*AbsoluteZero*/             { {},             192,          64, "absolute_zero",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*BlindingFlash*/            { {},             160,          48, "blinding_flash",         1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*DeathNova*/                { {},             160,          48, "death_nova",             1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*EmberBurst*/               { {},             128,          32, "ember_burst",            1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*AshenBurst*/               { {},             128,          32, "ashen_burst",            1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*ExplodingPalmBurst*/       { {},             128,          32, "exploding_palm_burst",   1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*ValkyrieBurst*/            { {},             160,          48, "valkyrie_burst",         1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*RainOfArrows*/             { {},             128,          32, "rain_of_arrows",         1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*ArmyOfTheDead*/            { {},             160,          48, "army_of_the_dead",       1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*HeavensDescent*/           { {},             160,          48, "heavens_descent",        1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*BonePrison*/               { {},              96,          16, "bone_prison",            1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*LightningRodBurst*/        { {},              96,          16, "lightning_rod_burst",    1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*FuneralStarBurst*/         { {},             160,          48, "funeral_star_burst",     1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*WrathPillar*/              { {},              64,           0, "wrath_pillar",           1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*WaveOfLight*/              { {},              96,          16, "wave_of_light",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*AncestralCourt*/           { {},             192,          64, "ancestral_court",        1, MissileGraphicsFlags::PngOnly,                 2, AnimLen_12      },
/*Earthquake*/               { {},             192,          64, "earthquake",             1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*FaradayRing*/              { {},             128,          32, "faraday_ring",           1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*LightningRod*/             { {},              48,          -8, "lightning_rod",          1, MissileGraphicsFlags::PngOnly,                 2, AnimLen_8       },
/*StormConductor*/           { {},              48,          -8, "storm_conductor",        1, MissileGraphicsFlags::PngOnly,                 2, AnimLen_8       },
/*StormArc*/                 { {},              64,           0, "storm_arc",              1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_4       },
/*EmberMine*/                { {},              32,         -16, "ember_mine",             1, MissileGraphicsFlags::PngOnly,                 2, AnimLen_8       },
/*FuneralStarCharge*/        { {},              64,           0, "funeral_star_charge",    1, MissileGraphicsFlags::PngOnly,                 2, AnimLen_8       },
// RfA-27 batch 56, body overlays
/*StaticCharge*/             { {},              96,          16, "static_charge",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*Conduit*/                  { {},              96,          16, "conduit",                1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*Immolate*/                 { {},              96,          16, "immolate",               1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*MantraOfClarity*/          { {},              96,          16, "mantra_of_clarity",      1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*MantraOfEvasion*/          { {},              96,          16, "mantra_of_evasion",      1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*MantraOfRetribution*/      { {},              96,          16, "mantra_of_retribution",  1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*AstralProjection*/         { {},              96,          16, "astral_projection",      1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*PoisonDagger*/             { {},              96,          16, "poison_dagger",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*FrenzyOfTheDead*/          { {},              64,           0, "frenzy_of_the_dead",     1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*Serenity*/                 { {},              96,          16, "serenity",               1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
/*DarkMending*/              { {},              64,           0, "dark_mending",           1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*UnholyOffering*/           { {},              96,          16, "unholy_offering",        1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_12      },
// RfA-27 batch 57, movement puffs
/*ShadowStep*/               { {},              96,          16, "shadow_step",            1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*VaultDust*/                { {},              96,          16, "vault_dust",             1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*LeapingCrane*/             { {},              96,          16, "leaping_crane",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*ShoulderGate*/             { {},              96,          16, "shoulder_gate",          1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*GatherTheDead*/            { {},              64,           0, "gather_the_dead",        1, MissileGraphicsFlags::PngOnly,                 1, AnimLen_8       },
/*RideTheLightning*/         { {},             128,          32, "ride_the_lightning",    16, MissileGraphicsFlags::PngOnly,                 1, AnimLen_4       },
// 2026-09-27: vanilla's Resurrect beam (ressur1) at 60%, re-stippled and twisted red and blue by
// tools/BuildRedemptionRise.ps1 - Redemption plays it over each corpse it consumes. 58x96 frames; animWidth2 -3 keeps the
// column on its tile's centre (58 / 2 + 3 = 32); vanilla's own timing, one tick a frame.
/*RedemptionRise*/           { {},              58,          -3, "redemption_rise",   1, MissileGraphicsFlags::PngOnly,                 0, AnimLen_16      },
/*None*/                     { {},                0,           0, {},                 0, MissileGraphicsFlags::None,                     0, 0               },
	// clang-format on
};

// Oracool 2026-09-26 (RfA-27): indexed by MissileGraphicID positionally, like MissilesData - ninety rows went in at once.
static_assert(sizeof(MissileSpriteData) / sizeof(MissileSpriteData[0]) == static_cast<size_t>(MissileGraphicID::None) + 1,
    "MissileSpriteData needs a row for every MissileGraphicID, in the enum's order");

uint8_t MissileFileData::animDelay(uint8_t dir) const
{
	return MissileAnimDelays[animDelayIdx][dir];
}

uint8_t MissileFileData::animLen(uint8_t dir) const
{
	return MissileAnimLengths[animLenIdx][dir];
}

void MissileFileData::LoadGFX()
{
	if (sprites)
		return;

	if (name[0] == '\0')
		return;

	// Oracool: a PNG sheet supplied for this missile wins over the CL2 (2026-09-03, the Cold pack).
	// Same rule and same reasoning as the player bodies' import - drop the art beside the file it
	// replaces, and anything not supplied loads exactly as before, so the set can grow one missile at
	// a time. animFAmt IS the direction count, so it decides the sheet's shape here as well as below;
	// asking it once for both is what keeps a sixteen-facing sheet from being read as one row.
	if (std::optional<OwnedClxSpriteListOrSheet> png
	    = oracool::LoadPngMissileSheet(name, animWidth, animFAmt); // one row per file/direction, as the exporter writes them (2 for the portals since 2026-09-20)
	    png.has_value()) {
		sprites.emplace(std::move(*png));
		return;
	}

	// A PngOnly slot waiting on its art: stay empty. MissileArtLoaded answers no, and the skill
	// draws what it borrowed.
	if ((static_cast<uint8_t>(flags) & static_cast<uint8_t>(MissileGraphicsFlags::PngOnly)) != 0)
		return;

#ifdef UNPACKED_MPQS
	char path[MaxMpqPathSize];
	*BufCopy(path, "missiles\\", name, ".clx") = '\0';
	sprites.emplace(LoadClxListOrSheet(path));
#else
	if (animFAmt == 1) {
		char path[MaxMpqPathSize];
		*BufCopy(path, "missiles\\", name) = '\0';
		sprites.emplace(OwnedClxSpriteListOrSheet { LoadCl2(path, animWidth) });
	} else {
		FileNameGenerator pathGenerator({ "missiles\\", name }, DEVILUTIONX_CL2_EXT);
		sprites.emplace(OwnedClxSpriteListOrSheet { LoadMultipleCl2Sheet<16>(pathGenerator, animFAmt, animWidth) });
	}
#endif
}

void InitMissileGFX(bool loadHellfireGraphics)
{
	if (HeadlessMode)
		return;

	for (size_t mi = 0; MissileSpriteData[mi].animFAmt != 0; mi++) {
		if (!loadHellfireGraphics && mi > static_cast<uint8_t>(MissileGraphicID::BloodStarRedExplosion))
			break;
		if (MissileSpriteData[mi].flags == MissileGraphicsFlags::MonsterOwned)
			continue;
		MissileSpriteData[mi].LoadGFX();
	}
}

void FreeMissileGFX()
{
	for (auto &missileData : MissileSpriteData) {
		missileData.FreeGFX();
	}
}

} // namespace devilution
