/**
 * @file dead.cpp
 *
 * Implementation of functions for placing dead monsters.
 */
#include "dead.h"

#include <array>
#include <cstdint>

#include "diablo.h"
#include "levels/gendung.h"
#include "lighting.h"
#include "misdat.h"
#include "monster.h"
#include "utils/log.hpp"

namespace devilution {

Corpse Corpses[MaxCorpses];
int8_t stonendx;

namespace {
void InitDeadAnimationFromMonster(Corpse &corpse, const CMonster &mon)
{
	const AnimStruct &animData = mon.getAnimData(MonsterGraphic::Death);
	if (animData.sprites) {
		corpse.sprites.emplace(*animData.sprites);
	} else {
		corpse.sprites = std::nullopt;
	}
	corpse.frame = animData.frames - 1;
	corpse.width = animData.width;
}

void MoveLightToCorpse(Monster &monster)
{
	for (int dx = 0; dx < MAXDUNX; dx++) {
		for (int dy = 0; dy < MAXDUNY; dy++) {
			if ((dCorpse[dx][dy] & 0x1F) == monster.corpseId) {
				ChangeLightXY(monster.lightId, { dx, dy });
				return;
			}
		}
	}
	AddUnLight(monster.lightId);
}
} // namespace

void InitCorpses()
{
	int8_t mtypes[MaxMonsters] = {};

	int8_t nd = 0;

	// EVERY entry reset first (user crash, 2026-09-02: "i just entered my town portal to go back to
	// level 9 and my game crashed again" - an access violation in ClxSpriteList::numSprites, reached
	// from DrawDungeon's corpse branch).
	//
	// Corpses is a file-scope array that this function only ever fills from the front. Entries past
	// this level's count kept the PREVIOUS level's `sprites` - views into monster sprite data that
	// FreeMonsters has since released - and dCorpse, which stores a corpse id per tile, is saved with
	// the level and restored on return. A revisit rebuilds this table from what is alive NOW, so a
	// level whose champions have been killed produces FEWER entries than when its corpses were laid
	// down, and a stored id can address one of the stale ones. DrawDungeon's `if (!sprites) return;`
	// cannot catch that: a dangling view is not an empty one. It reads a freed pointer instead.
	//
	// Clearing makes the stale case an EMPTY optional, which that guard already handles - the corpse
	// simply is not drawn. The fork reaches this far more easily than vanilla did: champion packs are
	// uniques, so each takes a slot below, and Lesser Unique Density defaults to 300%.
	for (Corpse &corpse : Corpses)
		corpse = {};

	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		CMonster &monsterType = LevelMonsterTypes[i];
		if (mtypes[monsterType.type] != 0)
			continue;
		// Two slots held back for the blood spatter and the stone-curse shatter, which are written
		// unconditionally below - capping at MaxCorpses here would leave nd at 31 and send those two
		// writes past the end of the array.
		if (static_cast<unsigned>(nd) + 2 >= MaxCorpses)
			break;

		InitDeadAnimationFromMonster(Corpses[nd], monsterType);
		Corpses[nd].translationPaletteIndex = 0;
		nd++;

		monsterType.corpseId = nd;
		mtypes[monsterType.type] = nd;
	}

	nd++; // Unused blood spatter

	if (!HeadlessMode)
		Corpses[nd].sprites.emplace(*GetMissileSpriteData(MissileGraphicID::StoneCurseShatter).sprites);
	Corpses[nd].frame = 11;
	Corpses[nd].width = 128;
	Corpses[nd].translationPaletteIndex = 0;
	nd++;

	stonendx = nd;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		auto &monster = Monsters[ActiveMonsters[i]];
		if (monster.isUnique()) {
			// BOUNDED, and this is the fork's problem rather than vanilla's. Every unique takes a
			// slot here, champion packs ARE uniques (PrepareUniqueMonst sets uniqueType), and Lesser
			// Unique Density defaults to 300% - so a deep floor can carry far more of them than the
			// thirty-one slots this table has. Past the end it was writing into whatever follows
			// Corpses in the data segment, with only an assert after the fact to notice, and asserts
			// are gone in Release.
			//
			// A champion beyond the cap gets corpseId 0, which is the same "no corpse" state a
			// monster starts in - it dies without leaving a body rather than corrupting memory.
			//
			// And SET to 0 (audit, 2026-09-27): nothing did, so the comment above was a hope - the slot kept its last
			// occupant's id and a champion past the cap left another champion's body and colours. It leaves its own type's
			// body now (StartMonsterDeath falls back to it).
			if (static_cast<unsigned>(nd) >= MaxCorpses) {
				if (nd == MaxCorpses) {
					LogWarn("InitCorpses: more uniques on this level than the {} corpse slots - later ones leave their type's body",
					    MaxCorpses);
					nd++; // warned once
				}
				monster.corpseId = 0;
				continue;
			}
			InitDeadAnimationFromMonster(Corpses[nd], monster.type());
			Corpses[nd].translationPaletteIndex = ActiveMonsters[i] + 1;
			nd++;

			monster.corpseId = nd;
		}
	}

	assert(static_cast<unsigned>(nd) <= MaxCorpses);
}

void RestoreUniqueCorpsesAfterLoad()
{
	// The unique half of the table, rebuilt from the monsters that were actually LOADED (user, 2026-09-27: "fix the
	// decisions for me too"). A revisit runs InitCorpses on a monster set it generates and throws away before
	// LoadLevel, so every unique entry described a monster that never came back: a champion killed after the revisit,
	// and every champion body already on the floor, wore another's body and colours. Each loaded unique's saved id gets
	// its own entry back; an entry no loaded unique claims belonged to a champion who died before - which monster that
	// was is not saved, so its body is not drawn rather than drawn wrong.
	std::array<bool, MaxCorpses> claimed {};
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (!monster.isUnique())
			continue;
		const int id = monster.corpseId;
		if (id <= stonendx || id > static_cast<int>(MaxCorpses)) {
			monster.corpseId = 0; // leaves its type's body (StartMonsterDeath)
			continue;
		}
		InitDeadAnimationFromMonster(Corpses[id - 1], monster.type());
		Corpses[id - 1].translationPaletteIndex = static_cast<int>(monster.getId()) + 1;
		Corpses[id - 1].laid = false; // this monster is alive: its look is copied when it dies
		Corpses[id - 1].laidSprites = std::nullopt;
		Corpses[id - 1].hasLaidTrn = false;
		claimed[id - 1] = true;
	}
	for (size_t k = static_cast<size_t>(stonendx); k < MaxCorpses; k++) {
		if (claimed[k])
			continue;
		Corpses[k] = {};
		// Its tiles too (round 47 audit): the body is not drawn, and the corpse table, saved with the level since, would have
		// raised it unseen.
		for (int x = 0; x < MAXDUNX; x++) {
			for (int y = 0; y < MAXDUNY; y++) {
				if ((dCorpse[x][y] & 0x1F) == static_cast<int>(k) + 1)
					dCorpse[x][y] = 0;
			}
		}
	}
}

void AddCorpse(Point tilePosition, int8_t dv, Direction ddir)
{
	dCorpse[tilePosition.x][tilePosition.y] = (dv & 0x1F) + (static_cast<int>(ddir) << 5);
}

void MoveLightsToCorpses()
{
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		auto &monster = Monsters[ActiveMonsters[i]];
		if (!monster.isUnique())
			continue;
		MoveLightToCorpse(monster);
	}
}

} // namespace devilution
