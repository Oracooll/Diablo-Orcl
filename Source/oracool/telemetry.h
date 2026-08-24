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
 *
 * ## Cleaning rules for anyone reading this file back
 *
 * Both were derived the hard way, and both change the answer rather than tidying it.
 *
 *  1. **Drop any session containing a `debug` row.** What a console command contaminates is the
 *     economy, not one item, so the session is the honest granularity. This was rule one from the
 *     2026-08-21 read-back.
 *  2. **Drop `pickup` rows with `level == 0` when measuring DROP rates.** Nothing drops in town;
 *     anything picked up there was put there. Added 2026-08-25, when 415 of 552 pickups turned out
 *     to be town rows - 344 of them gems and runes in even counts of four, bench-spawned to test
 *     the socket system, and untagged because they were dropped on the floor and collected rather
 *     than conjured by a console command. Rule 1 does not catch them. Without this rule the drop
 *     sample is three quarters noise and reads as an inverted rarity ladder, which is exactly the
 *     wrong conclusion.
 *
 * Kills need no equivalent rule, but time-to-kill has two caveats of its own:
 *
 *  - Rows written before **2026-08-25** are unusable. Before 2026-08-21 the clock started in the
 *    stagger reaction, which a one-shot kill never reaches, so the missing times were missing in a
 *    biased direction. Between then and 2026-08-25 the "clock is running" sentinel was set by
 *    OR-ing 1 into the timestamp, which corrupted it: a kill in the same millisecond as the first
 *    hit recorded zero either way. Read `value2 > 0` only from sessions after that.
 *  - The clock starts at ANY damage, not at the player's. `ApplyMonsterDamage` does not know who
 *    dealt the blow, so a trap or another monster's damage starts it too, and the row then measures
 *    time-since-something-hurt-it rather than time-to-kill. Attributing damage would mean threading
 *    the attacker through that function; until then, treat outliers on levels with traps as
 *    suspect rather than as slow fights.
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

/**
 * @brief Marks that the debug console ran @p command in this session.
 *
 * A session that used the console is not a session about balance, and the analysis has to be able to
 * tell. The first read-back of this file showed 39 Primal pickups against 16 Rare - an inverted
 * rarity ladder, and alarming until you remember the session had run givepset and giveitemset. Debug
 * spawns and real drops were indistinguishable rows.
 *
 * Emitted as a `debug` event so a reader can drop every session that contains one. Cheaper and more
 * honest than trying to tag individual items: what is contaminated is the SESSION, not one pickup.
 */
void TelemetryRecordDebugCommand(const std::string &command);

/** @brief CSV field escaping, exposed for tests: quotes fields containing comma/quote/newline. */
std::string TelemetryEscapeCsvField(const std::string &field);

} // namespace devilution::oracool
