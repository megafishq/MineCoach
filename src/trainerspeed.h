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
 * @file trainerspeed.h
 * @brief Position generator and scoring for the speed trainer drills
 * @author MineCoach contributors
 * @date 2026-10-30
 *
 * TrainerSpeed generates random mid-game Expert positions and cuts small
 * puzzles out of them for the timed drills: find the safe cell, pick the best
 * chord, and flag-then-chord. Every puzzle is checked to be solvable from the
 * numbers it shows. Scores and personal bests are computed here so they can
 * be tested without the user interface.
 */

#ifndef TRAINERSPEED_H
#define TRAINERSPEED_H

#include <QRandomGenerator>

#include "trainerhistory.h"

/**
 * @brief A generated board position a puzzle is cut from
 *
 * @since trainer-v1
 */
struct TrainerSpeedPosition
{
    TrainerRecording recording; ///< Mine layout (no actions)
    QByteArray snapshot;        ///< Visible state (TrainerBoard::snapshot format)
};

/**
 * @brief Result of a flick targets round
 *
 * @since trainer-v1
 */
struct TrainerFlickResult
{
    int targets = 0;          ///< Targets shown
    int hits = 0;             ///< Targets clicked
    int misses = 0;           ///< Clicks on the wrong cell
    double avgMs = 0;         ///< Average time from target shown to hit
    double avgReactionMs = 0; ///< Average time until the cursor started moving
    double efficiency = 0;    ///< Average straight distance / travelled distance (0..1)
    int score = 0;            ///< See TrainerSpeed::flickScore()
};

/**
 * @brief Generates speed drill puzzles
 *
 * @since trainer-v1
 */
class TrainerSpeed
{
public:
    /**
     * @brief Create a generator (seed 0 = random)
     */
    explicit TrainerSpeed(quint32 seed = 0);

    /**
     * @brief A "click the safe cell" puzzle (patterns preferred)
     */
    bool nextSafePuzzle(TrainerSpeedPosition& pos, TrainerPuzzle& puzzle);

    /**
     * @brief A "chord the best number" puzzle; all flags shown are correct
     */
    bool nextChordPuzzle(TrainerSpeedPosition& pos, TrainerPuzzle& puzzle);

    /**
     * @brief A "flag the mines, then chord" puzzle around puzzle.chordCell
     */
    bool nextFlagChordPuzzle(TrainerSpeedPosition& pos, TrainerPuzzle& puzzle);

    /**
     * @brief Score of a flick round: 1000 x accuracy / average seconds per target
     */
    static int flickScore(int hits, int misses, double avgMs);

    /**
     * @brief Score of a sprint round: 100 per correct answer, -50 per mistake
     */
    static int sprintScore(int correct, int mistakes);

private:
    bool randomPosition(TrainerSpeedPosition& pos, int maxProgress, bool flagProvable);
    static QRect windowAround(int cell, int width, int height, int radius);

    QRandomGenerator rng;
};

#endif // TRAINERSPEED_H
