/**
 * @file oracool/combat_odds.h
 *
 * The hero sheet's two odds bars (user, 2026-09-27): under Armor class, the chance the last monster that hit the hero
 * has of hitting them now; under To hit, the chance the hero has now of hitting the last monster they attacked.
 *
 * Each is the game's own hit formula (monster.cpp's MonsterAttackPlayer, player.cpp's PlrHitMonst, missiles.cpp's
 * MonsterMHit for arrows), re-run every time the sheet asks, with the MONSTER's side frozen as it was at the blow and
 * the HERO's side read live - so changing armour, to hit or level moves the bar straight away, which is what makes it
 * worth looking at while choosing gear. Only the local hero is tracked; a new game starts with neither known.
 */
#pragma once

#include <string>

namespace devilution {

struct Monster;
struct Player;

namespace oracool {

/**
 * @brief A monster's melee blow has landed on @p player (the local hero only; others are ignored). @p monsterToHit is
 * the blow's to-hit after any curse on the monster, before the level and armour terms; @p minimumHit the dungeon
 * level's floor (monster.cpp's GetMinHit).
 */
void NoteMonsterHitPlayer(const Player &player, const Monster &monster, int monsterToHit, int minimumHit);

/**
 * @brief @p player swung at or shot at @p monster (the local hero only). @p arrow: a bow shot, whose chance also
 * loses @p distancePenalty (missiles.cpp's dist * dist / 2); otherwise a melee swing.
 */
void NotePlayerAttackedMonster(const Player &player, const Monster &monster, bool arrow, int distancePenalty);

/** @brief The same two notes from plain values - for the hooks above, and for tests and previews without a Monster. */
void NoteAttacker(std::string name, int monsterToHit, int monsterLevel, bool demon, bool undead, int minimumHit);
void NoteTarget(std::string name, int monsterArmor, bool arrow, int distancePenalty);

/**
 * @brief The chance, 0-100, the last monster to hit @p player hits them now - landing, then not slipped by Dodge or
 * Mantra of Evasion and not blocked by a shield (since 2026-09-27), standing - and its name. False when none has yet.
 */
bool ChanceToBeHit(const Player &player, int &chance, std::string &name);

/** @brief The chance, 5-95, @p player hits the last monster they attacked now, and its name. False when none yet. */
bool ChanceToHit(const Player &player, int &chance, std::string &name);

/** @brief Forgets both - a new game, or another character. */
void ClearCombatOdds();

} // namespace oracool
} // namespace devilution
