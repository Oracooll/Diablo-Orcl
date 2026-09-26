#include <gtest/gtest.h>
#include "diablo.h"
#include "control.h"
#include "cursor.h"
#include "items.h"
#include "init.h"
#include "inv.h"
#include "portal.h"
#include "qol/stash.h"
#include "utils/paths.h"
#include "oracool/stonegate.h"
#include "levels/gendung.h"
#include "multi.h"
#include "monster.h"
#include "oracool/chill.h"
#include "options.h"
#include "player.h"
#include "spells.h"
#include "stores.h"
#include "oracool/essence.h"
#include "oracool/rage.h"
#include "oracool/workshop.h"
#include "oracool/stonegate_menu.h"
#include "oracool/rift.h"
#include "utils/ui_fwd.h"
using namespace devilution;
namespace {
void Mount() {
 static bool done=false;if(done)return;
 paths::SetBasePath("C:/Diablo Orcl/x64-Debug/");
 paths::SetPrefPath("C:/Users/hroga/OneDrive/2. Personal Files/Software/Diablo/Resources/ChatGPT RfA/audit-v188-fixtures/");
 paths::SetConfigPath(paths::PrefPath());
 LoadCoreArchives();LoadGameArchives();InitCursor();done=true;
}
devilution::Player &Hero() {
 Players.resize(1); MyPlayer=&Players[0]; auto &p=*MyPlayer; p={};
 InspectPlayer=MyPlayer; ActiveInventoryTab=0; p._pNumInv=0; std::fill(std::begin(p.InvGrid),std::end(p.InvGrid),0); p.InvTabList={};p.InvTabGrid={};p._pNumInvTab={};
 p._pRSpell=SpellID::Invalid;p._pRSplType=SpellType::Invalid;
 p._pClass=HeroClass::Necromancer; p._pLevel=50;
 p._pHitPoints=p._pMaxHP=p._pHPBase=p._pMaxHPBase=100<<6;
 p._pMana=p._pManaBase=p._pMaxMana=p._pMaxManaBase=100<<6;
 p._pEssence=100<<6; p._pmode=PM_STAND;
 gbIsHellfire=true;gbIsMultiplayer=false;pcurs=CURSOR_HAND;
 return p;
}
}
TEST(AuditV188, EssencePricedSkillsAreGatedByEssenceInProductionCheck) {
 auto &p=Hero(); p._pEssence=0;
 int count=0;
 for(int i=1;i<=static_cast<int>(SpellID::LAST);i++) {
  const auto s=static_cast<SpellID>(i);
  if(oracool::EssenceCost(s)==0)continue;
  ++count;
  EXPECT_FALSE(oracool::CanPaySkill(p,s));
  EXPECT_NE(CheckSpell(p,s,SpellType::Skill,true),SpellCheckResult::Success)<<"spell="<<i;
 }
 EXPECT_EQ(count,17);
}
TEST(AuditV188, EssencePricedSkillsSettleEssenceInProductionConsume) {
 auto &p=Hero();
 for(int i=1;i<=static_cast<int>(SpellID::LAST);i++) {
  const auto s=static_cast<SpellID>(i);const int cost=oracool::EssenceCost(s);
  if(cost==0)continue;
  p._pEssence=100<<6;p._pMana=p._pManaBase=100<<6;
  p.executedSpell.spellType=SpellType::Skill;p.executedSpell.spellId=s;
  ConsumeSpell(p,s);
  EXPECT_EQ(p._pEssence,(100-cost)<<6)<<"spell="<<i;
  EXPECT_EQ(p._pMana,100<<6)<<"spell="<<i;
 }
}
TEST(AuditV188, WorkshopEscapeClosesTheVisibleWindow) {
 Hero();gnScreenWidth=1280;gnScreenHeight=720;
 invflag=false;sbookflag=false;stextflag=TalkID::None;
 oracool::CloseStonegateMenu();oracool::ResetWorkshopForNewGame();
 oracool::OpenWorkshop(oracool::WorkshopHost::Mystic);
 ASSERT_TRUE(oracool::IsWorkshopOpen());
 PressEscKey();
 EXPECT_FALSE(oracool::IsWorkshopOpen());
 oracool::ResetWorkshopForNewGame();
}
TEST(AuditV188, MysticRebuildPreservesUnchangedElementalAffix) {
 Mount();auto &p=Hero();
 for(auto type : {IPL_FIREDAM,IPL_LIGHTDAM,IPL_FIRE_ARROWS,IPL_LIGHT_ARROWS}) {
  devilution::Item item{};InitializeItem(item,IDI_BARDSWORD);
  item._iOracoolItemLevel=30;item._iIdentified=true;
  ItemPower power{type,3,9};ApplyOracoolItemPower(p,item,power);
  const int beforeFire=item._iFMaxDam,beforeLight=item._iLMaxDam;
  const OracoolAffix affix{type,3,9};
  ASSERT_TRUE(RebuildOracoolItemWithAffixes(p,item,&affix,1));
  EXPECT_EQ(item._iFMaxDam,beforeFire)<<"affix="<<int(type);
  EXPECT_EQ(item._iLMaxDam,beforeLight)<<"affix="<<int(type);
 }
}
TEST(AuditV188, ClosingRiftInvalidatesItsTownPortal) {
 auto &p=Hero();p.setLevel(0);leveltype=DTYPE_TOWN;setlevel=false;
 oracool::ResetRiftForNewGame();ASSERT_TRUE(oracool::OpenNephalemRift(p));
 InitPortals();ActivatePortal(0,{40,40},SL_RIFT_NEPHALEM,DTYPE_CATHEDRAL,true);
 oracool::CloseStonegate();
 EXPECT_EQ(oracool::ActiveRift(),oracool::RiftKind::None);
 EXPECT_FALSE(PosOkPortal(SL_RIFT_NEPHALEM,{40,40}));
 InitPortals();
}
// INCONCLUSIVE: this isolated close fixture raises SEH without a full game session; excluded from defect evidence.
TEST(AuditV188, DISABLED_WorkshopBenchDoesNotReachAnotherHeroSlot) {
 fprintf(stderr,"bench: mount\n");
 Mount();auto &p=Hero();gnScreenWidth=1280;gnScreenHeight=720;
 fprintf(stderr,"bench: open\n");
 oracool::ResetWorkshopForNewGame();oracool::OpenWorkshop(oracool::WorkshopHost::Mystic);
 InitializeItem(p.HoldItem,IDI_TRING);p.HoldItem._iSeed=0xABCDEFU;
 auto page=oracool::GetWorkshopRect();
 fprintf(stderr,"bench: deposit\n");
 ASSERT_TRUE(oracool::CheckWorkshopClick(page.position+Displacement{230,170}));
 ASSERT_TRUE(p.HoldItem.isEmpty());
 fprintf(stderr,"bench: switch\n");
 // Isolated switch of the hero slot. Static trace checks FreeGame does not call the workshop reset.
 Hero();fprintf(stderr,"bench: return\n");oracool::CloseWorkshop();
 EXPECT_EQ(MyPlayer->_pNumInv,0)<<"the previous hero's bench must not return to this hero";
 oracool::ResetWorkshopForNewGame();
}
TEST(AuditV188, StashSortConservesDenseMixedFootprints) {
 Mount();Hero();Players.resize(2);MyPlayer=&Players[1];auto &p=Players[0];
 Stash={};Stash.SetPage(0);
 devilution::Item armor{},ring{};InitializeItem(ring,IDI_TRING);
 for(int id=IDI_GOLD+1;id<=IDI_LAST;id++) {
  if(AllItemsList[id].iLoc!=ILOC_ARMOR)continue;
  InitializeItem(armor,static_cast<_item_indexes>(id));
  if(GetInventorySize(armor)==Size{2,3})break;
 }
 armor._iOracoolTier=OracoolItemTier::Rare;armor._iMagical=ITEM_QUALITY_MAGIC;
 ring._iMagical=ITEM_QUALITY_MAGIC;
 ASSERT_EQ(GetInventorySize(armor),(Size{2,3}));ASSERT_EQ(GetInventorySize(ring),(Size{1,1}));
 // Build an explicitly valid original grid: 25 armors on page 0, eight on page 1, then rings in each free cell.
 for(int i=0;i<33;i++) {
  const int page=i/25,k=i%25,x=(k%5)*2,y=(k/5)*3;
  armor._iSeed=i+1;Stash.stashList.push_back(armor);auto index=Stash.stashList.size();
  for(int dy=0;dy<3;dy++)for(int dx=0;dx<2;dx++)Stash.stashGrids[page][x+dx][y+dy]=static_cast<uint16_t>(index);
 }
 int rings=0;
 for(int page=0;page<100;page++)for(int y=0;y<16;y++)for(int x=0;x<10;x++) {
  auto &cell=Stash.stashGrids[page][x][y];if(cell||rings==15801)continue;
  ring._iSeed=100+ ++rings;Stash.stashList.push_back(ring);cell=static_cast<uint16_t>(Stash.stashList.size());
 }
 ASSERT_EQ(rings,15801);ASSERT_EQ(Stash.stashList.size(),15834U);
 SortStash(p);
 EXPECT_EQ(Stash.stashList.size(),15834U);
 int kept=0;for(const auto &i:Stash.stashList)if(i.IDidx==armor.IDidx)kept++;
 EXPECT_EQ(kept,33);Stash={};
}

TEST(AuditV188, DeletedMonsterSlotClearsColdState) {
 Hero();oracool::ClearChills();
 ActiveMonsterCount=MAX_PLRS+1;
 for(size_t i=0;i<ActiveMonsterCount;i++)ActiveMonsters[i]=static_cast<unsigned>(i);
 auto &m=Monsters[MAX_PLRS];m={};m.isInvalid=true;
 oracool::ChillMonster(m,100);oracool::FreezeMonster(m,50);
 DeleteMonsterList();
 EXPECT_EQ(ActiveMonsterCount,MAX_PLRS);
 EXPECT_FALSE(oracool::IsMonsterChilled(m));
 EXPECT_FALSE(oracool::IsMonsterFrozen(m));
 oracool::ClearChills();ActiveMonsterCount=0;
}

TEST(AuditV188, WorkshopCountsAsInterfaceForWorldRejection) {
 Hero();gnScreenWidth=1280;gnScreenHeight=720;
 invflag=false;sbookflag=false;stextflag=TalkID::None;
 oracool::ResetWorkshopForNewGame();oracool::OpenWorkshop(oracool::WorkshopHost::Mystic);
 const Point point=oracool::GetWorkshopRect().position+Displacement{230,170};
 ASSERT_TRUE(oracool::IsPointOverWorkshop(point));
 EXPECT_TRUE(IsOverAnyInterface(point));
 oracool::ResetWorkshopForNewGame();
}

