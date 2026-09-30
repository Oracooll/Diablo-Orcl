/**
 * @file oracool_skill_assets_test.cpp
 *
 * The skill asset sweep (asset audit, 2026-09-26): every skill a player can see names art and sound
 * files, and a missing one is SILENT at runtime. A strip one frame short draws placeholder letters, a
 * cue that will not load plays nothing, a ring that is not in the archive leaves only a verbose log
 * line, and a PngOnly missile with no sheet keeps its borrowed sprite. None of it crashes and none of
 * it fails a test, so this asks the mounted archives directly.
 *
 * For every row of every VISIBLE class (hidden classes - oracool/hidden_classes.h - are skipped, as
 * are rows on no page and inert rows):
 *
 *   - ICON: the class's tree strip (hud_art) has a frame at the row's ClassTreeIconIndex. The
 *     Necromancer's strip (ui\necro_tree_icons.png) does not exist yet - his 72 glyphs are batch 39,
 *     held in RfA-17 and re-requested in RfA-27 - so an ABSENT Necromancer strip is recorded as
 *     "awaiting art" rather than failed. Once any strip for him loads, it is held to the same rule.
 *   - SOUNDS: every cue the generated skill-sound table lists for the row (SkillSoundPath, all seven
 *     events) is a file in the archives. A row with no cue is normal and not a failure.
 *   - RING: a Kind::Aura row has a ground ring in aura_ground's table and ui\aura_<id>.png exists.
 *   - MISSILES: the row's spell (ClassTreeSpellId) -> GetSpellData(spell).sMissiles -> each missile's
 *     MissileGraphicID -> MissileSpriteData's file name, resolved exactly as MissileFileData::LoadGFX
 *     resolves it: missiles\<name>.png wins when present; a PngOnly row needs it; any other row falls
 *     back to missiles\<name>.cl2 (one direction) or missiles\<name>1.cl2 .. <name><N>.cl2. Vanilla
 *     files come from the player's own archives, which this test mounts. Hellfire-range sheets are
 *     reported "not checked" when hellfire.mpq is not mounted. Missiles with no sprite of their own
 *     (the Warcry dispatcher that carries ~200 fork actives, invisible control missiles) have nothing
 *     to check here - their custom effects are drawn by code this sweep cannot see.
 *
 * OUTPUT: skill_assets_report.md in the working directory - a summary, every missing asset, then one
 * table per class with icon / sounds / ring / missiles for each row. Anything missing fails the test,
 * except the known Necromancer icon gap.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "engine/assets.hpp"
#include "init.h"
#include "misdat.h"
#include "oracool/aura_ground.h"
#include "oracool/class_tree.h"
#include "oracool/hidden_classes.h"
#include "oracool/hud_art.h"
#include "levels/gendung.h"
#include "objects.h"
#include "oracool/levski_roar.h"
#include "oracool/oracool.h"
#include "oracool/skill_sounds.h"
#include "oracool/stonegate.h"
#include "oracool/sprite_import.h"
#include "player.h"
#include "spelldat.h"

using namespace devilution;

namespace {

using oracool::ClassTreeKind;
using oracool::ClassTreeSkill;
using oracool::SkillSoundEvent;

/**
 * @brief The archives, once per PROCESS. The shuffled lane runs this binary whole three times in one
 * process, and a second LoadCoreArchives crashes it (the audit tests' MountTestArchives note, QA-01).
 */
void MountArchivesOnce()
{
	static const bool mounted = [] {
		LoadCoreArchives();
		LoadGameArchives();
		return true;
	}();
	(void)mounted;
}

const char *ClassLabel(HeroClass heroClass)
{
	switch (heroClass) {
	case HeroClass::Warrior:
		return "Paladin";
	case HeroClass::Rogue:
		return "Rogue";
	case HeroClass::Sorcerer:
		return "Sorceress";
	case HeroClass::Monk:
		return "Monk";
	case HeroClass::Bard:
		return "Bard";
	case HeroClass::Barbarian:
		return "Barbarian";
	case HeroClass::Necromancer:
		return "Necromancer";
	default:
		return "?";
	}
}

const char *KindLabel(ClassTreeKind kind)
{
	switch (kind) {
	case ClassTreeKind::Active:
		return "Active";
	case ClassTreeKind::Aura:
		return "Aura";
	case ClassTreeKind::Passive:
		return "Passive";
	default:
		return "?";
	}
}

constexpr SkillSoundEvent AllEvents[] = {
	SkillSoundEvent::Cast,
	SkillSoundEvent::Impact,
	SkillSoundEvent::Arrive,
	SkillSoundEvent::Start,
	SkillSoundEvent::Loop,
	SkillSoundEvent::Stop,
	SkillSoundEvent::Learn,
};

const char *EventLabel(SkillSoundEvent event)
{
	switch (event) {
	case SkillSoundEvent::Cast:
		return "Cast";
	case SkillSoundEvent::Impact:
		return "Impact";
	case SkillSoundEvent::Arrive:
		return "Arrive";
	case SkillSoundEvent::Start:
		return "Start";
	case SkillSoundEvent::Loop:
		return "Loop";
	case SkillSoundEvent::Stop:
		return "Stop";
	case SkillSoundEvent::Learn:
		return "Learn";
	default:
		return "?";
	}
}

bool AssetExists(const std::string &path)
{
	return FindAsset(path.c_str()).ok();
}

std::string Join(const std::vector<std::string> &parts, const char *empty)
{
	if (parts.empty())
		return empty;
	std::string out;
	for (const std::string &part : parts) {
		if (!out.empty())
			out += ", ";
		out += part;
	}
	return out;
}

struct Verdict {
	std::string text;
	bool missing = false;
	bool unchecked = false;
};

/** @brief Whether @p graphic's sheet is in the archives, resolved the way MissileFileData::LoadGFX resolves it. */
Verdict CheckMissileGraphic(MissileGraphicID graphic, bool haveHellfire)
{
	Verdict out;
	const MissileFileData &data = GetMissileSpriteData(graphic);
	const std::string name = data.name;
	if (name.empty()) {
		out.text = "(unnamed sprite row)";
		return out;
	}

	// A PNG beside the CL2 wins for every row, so a present PNG is the answer whatever the flags say.
	const std::string png = "missiles\\" + name + ".png";
	if (AssetExists(png)) {
		out.text = name + ".png";
		return out;
	}
	const auto flags = static_cast<uint8_t>(data.flags);
	if ((flags & static_cast<uint8_t>(MissileGraphicsFlags::PngOnly)) != 0) {
		out.text = "MISSING " + png;
		out.missing = true;
		return out;
	}

	std::vector<std::string> files;
#ifdef UNPACKED_MPQS
	files.push_back("missiles\\" + name + ".clx");
#else
	if (data.animFAmt <= 1) {
		files.push_back("missiles\\" + name + ".cl2");
	} else {
		for (unsigned i = 1; i <= data.animFAmt; i++)
			files.push_back("missiles\\" + name + std::to_string(i) + ".cl2");
	}
#endif
	std::vector<std::string> absent;
	for (const std::string &file : files) {
		if (!AssetExists(file))
			absent.push_back(file);
	}
	if (absent.empty()) {
		out.text = files.size() == 1 ? name + " (cl2)" : name + " (" + std::to_string(files.size()) + " cl2)";
		return out;
	}
	// InitMissileGFX loads the rows past BloodStarRedExplosion only for Hellfire; without hellfire.mpq
	// their CL2s cannot be asked about, which is not the same thing as missing.
	const bool hellfireRange = static_cast<int>(graphic) > static_cast<int>(MissileGraphicID::BloodStarRedExplosion);
	if (hellfireRange && !haveHellfire) {
		out.text = name + " (Hellfire sheet, hellfire.mpq not mounted - not checked)";
		out.unchecked = true;
		return out;
	}
	out.text = "MISSING " + absent.front();
	if (absent.size() > 1)
		out.text += " (+" + std::to_string(absent.size() - 1) + " more)";
	out.missing = true;
	return out;
}

struct RowResult {
	int index;
	std::string name;
	ClassTreeKind kind;
	std::string icon;
	std::string sounds;
	std::string ring;
	std::string missiles;
};

struct ClassResult {
	HeroClass heroClass;
	int rows = 0;
	int frames = 0;
	std::vector<RowResult> checked;
};

std::string Cell(const std::string &text)
{
	// Markdown tables split on '|'; nothing this writes should contain one, but a skill name might.
	std::string out;
	for (const char c : text)
		out += c == '|' ? '/' : c;
	return out;
}

} // namespace

TEST(OracoolSkillAssets, EveryVisibleSkillHasItsIconSoundsRingAndMissileArt)
{
	MountArchivesOnce();
	if (!HaveDiabdat())
		GTEST_SKIP() << "needs diabdat.mpq - the vanilla missile sheets are read from it";

	// hellfire.mpq's presence, asked of a file only it carries (the Crypt's tiles). HaveHellfire reads a
	// global the test DLL does not export.
	const bool haveHellfire = AssetExists("nlevels\\l5data\\l5.cel");

	std::vector<ClassResult> classes;
	std::vector<std::string> hiddenClasses;
	std::vector<std::string> missing;
	int offPage = 0;
	int inert = 0;
	int soundsChecked = 0;
	int ringsChecked = 0;
	int sheetsChecked = 0;
	int sheetsUnchecked = 0;
	int necroAwaiting = 0;

	for (int c = 0; c <= static_cast<int>(HeroClass::LAST); c++) {
		const auto heroClass = static_cast<HeroClass>(c);
		if (!oracool::ClassHasTree(heroClass))
			continue;
		if (oracool::IsClassHidden(heroClass)) {
			hiddenClasses.emplace_back(ClassLabel(heroClass));
			continue;
		}

		// The rows a player can see: every page's cells, as the Abilities window builds them.
		std::set<ClassTreeSkill> onPage;
		ClassTreeSkill cells[oracool::MaxSkillsPerClass];
		for (int page = 0; page < static_cast<int>(oracool::ClassTreePageCount); page++) {
			const size_t count = oracool::BuildClassTreePage(heroClass, page, cells);
			for (size_t i = 0; i < count; i++)
				onPage.insert(cells[i]);
		}

		ClassResult result;
		result.heroClass = heroClass;
		result.frames = oracool::ClassTreeStripFrameCount(heroClass);
		const std::string label = ClassLabel(heroClass);

		for (int index = 0;; index++) {
			const std::optional<ClassTreeSkill> found = oracool::ClassTreeSkillAtIndex(heroClass, index);
			if (!found.has_value())
				break;
			result.rows++;
			const ClassTreeSkill skill = *found;
			const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skill);
			if (onPage.count(skill) == 0) {
				offPage++;
				continue;
			}
			if (!data.implemented) {
				inert++;
				continue;
			}

			RowResult row;
			row.index = index;
			row.name = data.name != nullptr ? data.name : "?";
			row.kind = data.kind;
			const std::string who = label + " #" + std::to_string(index) + " " + row.name;

			// ---- icon ----
			const int iconIndex = oracool::ClassTreeIconIndex(skill);
			if (result.frames == 0 && heroClass == HeroClass::Necromancer) {
				row.icon = "awaiting art";
				necroAwaiting++;
			} else if (result.frames == 0) {
				row.icon = "MISSING (no strip)";
				missing.push_back(who + ": icon - the class strip did not load");
			} else if (iconIndex < 0 || iconIndex >= result.frames) {
				row.icon = "MISSING (frame " + std::to_string(iconIndex) + " of " + std::to_string(result.frames) + ")";
				missing.push_back(who + ": icon - frame " + std::to_string(iconIndex) + " is past the strip's "
				    + std::to_string(result.frames) + " frames");
			} else {
				row.icon = "ok (" + std::to_string(iconIndex) + ")";
			}

			// ---- sounds ----
			std::vector<std::string> cues;
			for (const SkillSoundEvent event : AllEvents) {
				const char *path = oracool::SkillSoundPath(skill, event);
				if (path == nullptr)
					continue;
				soundsChecked++;
				if (AssetExists(path)) {
					cues.emplace_back(EventLabel(event));
				} else {
					cues.push_back(std::string(EventLabel(event)) + " MISSING");
					missing.push_back(who + ": sound " + EventLabel(event) + " - " + path);
				}
			}
			row.sounds = Join(cues, "-");

			// ---- aura ring ----
			if (data.kind == ClassTreeKind::Aura) {
				ringsChecked++;
				const char *id = oracool::AuraRingFileId(skill);
				if (id == nullptr) {
					row.ring = "MISSING (no ring in the table)";
					missing.push_back(who + ": ring - aura_ground's table has no entry");
				} else {
					const std::string path = std::string("ui\\aura_") + id + ".png";
					if (AssetExists(path)) {
						row.ring = std::string("aura_") + id;
					} else {
						row.ring = "MISSING " + path;
						missing.push_back(who + ": ring - " + path);
					}
				}
			} else {
				row.ring = "n/a";
			}

			// ---- missiles ----
			std::vector<std::string> sheets;
			const SpellID spell = oracool::ClassTreeSpellId(skill);
			if (spell != SpellID::Invalid && spell != SpellID::Null && spell <= SpellID::LAST) {
				std::set<int> seen;
				bool noSprite = false;
				for (const MissileID missile : GetSpellData(spell).sMissiles) {
					if (missile == MissileID::Null)
						continue;
					const MissileGraphicID graphic = GetMissileData(missile).mFileNum;
					if (graphic >= MissileGraphicID::None) {
						noSprite = true;
						continue;
					}
					if (!seen.insert(static_cast<int>(graphic)).second)
						continue;
					const Verdict verdict = CheckMissileGraphic(graphic, haveHellfire);
					sheets.push_back(verdict.text);
					if (verdict.unchecked) {
						sheetsUnchecked++;
					} else {
						sheetsChecked++;
					}
					if (verdict.missing)
						missing.push_back(who + ": missile sheet - " + verdict.text);
				}
				if (noSprite)
					sheets.emplace_back("(no sprite of its own)");
			}
			row.missiles = Join(sheets, "-");

			result.checked.push_back(std::move(row));
		}
		EXPECT_GT(result.rows, 0) << label << " has a tree but no rows";
		classes.push_back(std::move(result));
	}

	int rowsChecked = 0;
	for (const ClassResult &cls : classes)
		rowsChecked += static_cast<int>(cls.checked.size());

	// ---- the report ----
	{
		std::ofstream report("skill_assets_report.md");
		report << "# Skill asset report\n\n";
		report << "Written by `OracoolSkillAssets.EveryVisibleSkillHasItsIconSoundsRingAndMissileArt` "
		          "(test/oracool_skill_assets_test.cpp). Every file below was asked of the mounted archives "
		          "(oracool.mpq first, then the player's own).\n\n";
		report << "- Classes checked: ";
		for (size_t i = 0; i < classes.size(); i++)
			report << (i == 0 ? "" : ", ") << ClassLabel(classes[i].heroClass);
		report << "\n";
		report << "- Hidden, skipped: " << Join(hiddenClasses, "none") << "\n";
		report << "- Rows checked: " << rowsChecked << " (skipped: " << offPage << " on no page, " << inert << " inert)\n";
		report << "- Sound cues checked: " << soundsChecked << "; aura rings checked: " << ringsChecked
		       << "; missile sheets checked: " << sheetsChecked << "\n";
		report << "- Hellfire archive: " << (haveHellfire ? "mounted" : "NOT mounted") << " (" << sheetsUnchecked
		       << " Hellfire-range sheet(s) not checked)\n";
		report << "- Necromancer icons: "
		       << (necroAwaiting > 0 ? "awaiting art - ui\\necro_tree_icons.png is not in the archives (batch 39, RfA-17 hold, re-requested in RfA-27); "
		                                   + std::to_string(necroAwaiting) + " rows draw placeholder letters"
		                             : std::string("strip present, held to the same rule as every class"))
		       << "\n";
		report << "- **Missing: " << missing.size() << "**\n\n";

		report << "## Missing\n\n";
		if (missing.empty()) {
			report << "Nothing (the Necromancer icon gap above is known and not counted).\n\n";
		} else {
			for (const std::string &line : missing)
				report << "- " << Cell(line) << "\n";
			report << "\n";
		}

		for (const ClassResult &cls : classes) {
			report << "## " << ClassLabel(cls.heroClass) << " (" << cls.rows << " rows, " << cls.checked.size()
			       << " checked, strip " << cls.frames << " frames)\n\n";
			report << "| # | skill | kind | icon | sounds | ring | missiles |\n";
			report << "|---|---|---|---|---|---|---|\n";
			for (const RowResult &row : cls.checked) {
				report << "| " << row.index << " | " << Cell(row.name) << " | " << KindLabel(row.kind) << " | "
				       << Cell(row.icon) << " | " << Cell(row.sounds) << " | " << Cell(row.ring) << " | "
				       << Cell(row.missiles) << " |\n";
			}
			report << "\n";
		}
	}

	if (necroAwaiting > 0)
		RecordProperty("necromancer_icons", "awaiting art");

	EXPECT_GT(rowsChecked, 0) << "no visible class had a row to check";
	std::string list;
	for (const std::string &line : missing)
		list += "\n  " + line;
	EXPECT_TRUE(missing.empty()) << missing.size() << " skill asset(s) missing (see skill_assets_report.md):" << list;
}

// The spell table names only the missile a cast launches; everything spawned from code - Blessed Hammer's spin, the
// hit flashes, the census effects, the necro bolts, ice_ground - is reached here instead: EVERY sheet the missile
// table registers must be in the archives, whoever spawns it (asset audit, 2026-09-26).
TEST(OracoolSkillAssets, EveryRegisteredMissileSheetIsInTheArchives)
{
	MountArchivesOnce();
	if (!HaveDiabdat())
		GTEST_SKIP() << "needs the player's own archives for the vanilla sheets";
	const bool haveHellfire = AssetExists("nlevels\\l5data\\l5.cel");
	std::vector<std::string> missing;
	int checked = 0;
	int awaiting = 0;
	for (int g = 0; g < static_cast<int>(MissileGraphicID::None); g++) {
		const Verdict verdict = CheckMissileGraphic(static_cast<MissileGraphicID>(g), haveHellfire);
		checked++;
		// RfA-27's 90 effect sheets were rejected and are awaiting redelivery (2026-09-26): registered and wired, drawn
		// only once their art arrives. Not missing - awaited. Once delivered they are checked like every other row.
		if (verdict.missing && IsAwaitingRedeliveryArt(static_cast<MissileGraphicID>(g))) {
			awaiting++;
			continue;
		}
		if (verdict.missing)
			missing.push_back(std::to_string(g) + ": " + verdict.text);
	}
	EXPECT_GT(checked, 100) << "the missile table looks empty";
	RecordProperty("awaiting_redelivery", awaiting);
	std::string list;
	for (const std::string &m : missing)
		list += "\n  " + m;
	EXPECT_TRUE(missing.empty()) << missing.size() << " missile sheets are not in the archives:" << list;
}

/**
 * @brief Levski's Cube's animated sheet (2026-10-01) loads from the archive the way the game asks for it, whole. A sheet
 * that fails to load leaves the old painting behind without a word.
 */
TEST(OracoolSkillAssets, LevskiCubeSheetLoadsWithEveryFrame)
{
	MountArchivesOnce();
	const std::optional<oracool::ColouredSpriteList> sheet = oracool::LoadPngObjectSheetColoured("levski_cube", 128);
	ASSERT_TRUE(sheet.has_value()) << "objects\levski_cube.png did not load";
	EXPECT_EQ(sheet->list.numSprites(), 26u);
	EXPECT_NE(sheet->colours, nullptr);
}

/**
 * @brief The Cube in a town built as the game builds it, with graphics on: it wears the sheet, draws in its own colours, and
 * its closed loop moves. The user saw the old painting standing still in play (v1.12.273).
 */
TEST(OracoolSkillAssets, LevskiCubeWearsItsSheetInTown)
{
	MountArchivesOnce();
	const bool savedHeadless = HeadlessMode;
	HeadlessMode = false;
	leveltype = DTYPE_TOWN;
	currlevel = 0;
	setlevel = false;
	memset(dObject, 0, sizeof(dObject));
	oracool::InitTownObjectPool();
	oracool::AddStashChestObject();
	oracool::AddLevskiRoarObject();
	oracool::AddStonegateObject();
	oracool::AddWaypointSigilObject();
	Object *cube = nullptr;
	for (int i = 0; i < ActiveObjectCount; i++) {
		if (oracool::IsLevskiRoarObject(Objects[ActiveObjects[i]]))
			cube = &Objects[ActiveObjects[i]];
	}
	ASSERT_NE(cube, nullptr);
	EXPECT_EQ(cube->_oAnimLen, 26u);
	EXPECT_NE(oracool::LevskiCubeColoursFor(*cube), nullptr);
	const uint32_t before = cube->_oAnimFrame;
	for (int tick = 0; tick < 8; tick++)
		oracool::ProcessLevskiCubeAnimation();
	EXPECT_NE(cube->_oAnimFrame, before);
	HeadlessMode = savedHeadless;
	oracool::InitTownObjectPool();
	FreeObjectGFX();
}
