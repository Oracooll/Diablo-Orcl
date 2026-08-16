/**
 * @file oracool/telemetry.h
 *
 * Oracool: Megaplan Phase 0.9 - balance telemetry.
 *
 * The project's slowest loop is tuning-by-anecdote: the user plays, remembers an impression,
 * reports it, and the numbers get adjusted on feel. This module turns every play session into
 * DATA instead: an append-only CSV beside the saves, one row per noteworthy combat event, flushed
 * per row so even a crash loses nothing. The assistant reads the file after a session and tunes
 * Phases 3-5 (drop rates, monster stats, zone difficulty) against what actually happened.
 *
 * Strictly local (a file in the save directory, nothing networked), option-gated
 * (sgOptions.Oracool.balanceTelemetry), and deliberately narrow: kills with time-to-kill, player
 * deaths, and item pickups by tier. Events that would fire every tick (damage dealt, mana spent)
 * stay out - a firehose CSV is as useless as no CSV.
 *
 * Columns: time,session,event,level,player_level,subject,value1,value2
 *   kill:   subject = monster name, value1 = monster level, value2 = time-to-kill in ms (0 if the
 *           first hit was never seen, e.g. a golem kill or trap)
 *   death:  subject = what killed the player
 *   pickup: subject = item name, value1 = tier (0 basic/magic-less .. 3 primal, matching
 *           OracoolItemTier), value2 = sell value
 */
#pragma once

#include <string>

namespace devilution {
struct Monster;
struct Item;
} // namespace devilution

namespace devilution::oracool {

/** @brief Notes the first time @p monster takes player damage, starting its time-to-kill clock. */
void TelemetryRecordFirstHit(const Monster &monster);

/** @brief Appends a kill row (and closes the monster's time-to-kill clock). */
void TelemetryRecordKill(const Monster &monster);

/** @brief Appends a death row. */
void TelemetryRecordPlayerDeath(const std::string &source);

/** @brief Appends a pickup row for an item entering the player's possession from the world. */
void TelemetryRecordPickup(const Item &item);

/** @brief CSV field escaping, exposed for tests: quotes fields containing comma/quote/newline. */
std::string TelemetryEscapeCsvField(const std::string &field);

} // namespace devilution::oracool
