/*****************************************************************************
 * LibreMines                                                                *
 * Copyright (C) 2020-2026  Bruno Bollos Correa                              *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation, either version 3 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

/**
 * @file trainerhistory.h
 * @brief Analysis of many recorded games and practice puzzle generation
 * @author MineCoach contributors
 * @date 2026-10-30
 *
 * TrainerHistory analyses a set of recordings (usually the most recent ones),
 * counts the mistakes and missed patterns across all of them and turns the
 * exact positions where a pattern was missed into practice puzzles. The
 * playlist favours the patterns that are missed most often.
 */

#ifndef TRAINERHISTORY_H
#define TRAINERHISTORY_H

#include <QPair>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

#include "traineranalyzer.h"
#include "trainerrecording.h"

/**
 * @brief One practice puzzle cut from a recorded game
 *
 * @since trainer-v1
 */
struct TrainerPuzzle
{
    /**
     * @brief What the player has to do
     */
    enum class Type : quint8
    {
        SafeCell = 0, ///< Click any cell that is 100% safe
        BestChord = 1 ///< Click the number whose chord opens the most cells
    };

    Type type = Type::SafeCell;  ///< Kind of puzzle
    int game = -1;               ///< Index of the game in TrainerHistory
    int actionIndex = 0;         ///< Move of the game the position is taken from
    QByteArray snapshot;         ///< Board state shown in the puzzle
    QRect area;                  ///< The only numbers that may be used to solve the puzzle
    QRect view;                  ///< Cells shown: area plus one ring, so every used number's
                                 ///< neighbours are visible
    QString category;            ///< Pattern / mistake the puzzle trains
    TrainerDeduction deduction;  ///< SafeCell: the missed deduction
    QVector<TrainerDeduction> prerequisites; ///< SafeCell: earlier steps of the deduction
    int chordCell = -1;          ///< BestChord: the best number to chord on
    int chordOpens = 0;          ///< BestChord: cells that chord opens
};

/**
 * @brief Totals across the analysed games
 *
 * @since trainer-v1
 */
struct TrainerHistorySummary
{
    int games = 0;              ///< Games analysed
    int wins = 0;               ///< Games won
    double avgThreeBVPerSecond = 0; ///< Average 3BV/s
    double avgEfficiency = 0;   ///< Average click efficiency (percent)
    int jumps = 0;              ///< Cursor jumps
    int missedPatterns = 0;     ///< Jumps that left a provable safe cell behind
    int guesses = 0;            ///< Guesses while an equally fast safe move existed
    int wastedClicks = 0;       ///< Clicks with no effect
    int savableClicks = 0;      ///< Clicks better chording would have saved
    QVector<QPair<QString, int>> mistakes;       ///< Finding type -> count, most frequent first
    QVector<QPair<QString, int>> missedByPattern;///< Pattern -> times missed, most frequent first
    int puzzleCandidates = 0;   ///< Positions usable as practice puzzles

    /**
     * @brief Plain text summary (used by the command line and tests)
     */
    QString toText() const;
};

/**
 * @brief Analyses several recordings and builds practice playlists
 *
 * @since trainer-v1
 */
class TrainerHistory
{
public:
    /**
     * @brief Paths of the most recent recordings in a directory, newest first
     *
     * @param dir Directory with *.json recordings
     * @param count Maximum number of files (0 = all)
     */
    static QStringList recentRecordings(const QString& dir, int count);

    /**
     * @brief Add one analysed game
     */
    void addGame(const QString& path, const TrainerRecording& recording, const TrainerReport& report);

    /**
     * @brief Load and analyse one file; returns false if it cannot be read
     */
    bool addFile(const QString& path, QString* error = nullptr);

    /**
     * @brief Number of games added
     */
    int gameCount() const;

    /**
     * @brief Recording of game i
     */
    const TrainerRecording& recording(int i) const;

    /**
     * @brief File game i was loaded from
     */
    QString path(int i) const;

    /**
     * @brief Totals across all games
     */
    TrainerHistorySummary summary() const;

    /**
     * @brief Every usable puzzle position (newest games first)
     */
    QVector<TrainerPuzzle> candidates() const;

    /**
     * @brief A practice playlist weighted towards the most missed patterns
     *
     * @param maxPuzzles Maximum length of the playlist
     */
    QVector<TrainerPuzzle> playlist(int maxPuzzles) const;

    /**
     * @brief Check an answer
     *
     * @param recording Game the puzzle comes from
     * @param puzzle The puzzle
     * @param cell Cell the player clicked
     * @param feedback Explanation of the verdict
     * @return true if the answer is right
     */
    static bool checkAnswer(const TrainerRecording& recording, const TrainerPuzzle& puzzle,
                            int cell, QString* feedback = nullptr);

    /**
     * @brief Training category of a deduction ("1-1", "satisfied number", ...)
     */
    static QString categoryOf(const TrainerDeduction& d);

    /**
     * @brief Short advice for a pattern or mistake category
     */
    static QString tipFor(const QString& category);

private:
    struct Game
    {
        QString path;
        TrainerRecording recording;
        TrainerReport report;
    };
    QVector<Game> games;
};

#endif // TRAINERHISTORY_H
