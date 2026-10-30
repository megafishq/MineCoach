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
 * @file trainerrecording.h
 * @brief Gameplay recording model used by the LibreMines trainer
 * @author LibreMines Trainer contributors
 * @date 2026-10-30
 *
 * A TrainerRecording stores everything needed to replay and analyse a
 * finished game: the board dimensions, the final mine layout, every player
 * action (reveal, flag, chord) with its timestamp and the cursor trajectory
 * over the board. Recordings are serialised as JSON so they can be reopened
 * later or analysed from the command line.
 */

#ifndef TRAINERRECORDING_H
#define TRAINERRECORDING_H

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

/**
 * @brief A single player action on the minefield
 *
 * @since trainer-v1
 */
struct TrainerAction
{
    /**
     * @brief Kind of input the player issued
     */
    enum class Type : quint8
    {
        Reveal = 0, ///< Left click on a hidden cell
        Flag   = 1, ///< Right click on a hidden cell (flag cycle)
        Chord  = 2  ///< Click on a revealed number (clear neighbours)
    };

    Type type = Type::Reveal; ///< What the player did
    qint64 tMs = 0;           ///< Milliseconds since the board became playable
    int x = 0;                ///< Column of the target cell
    int y = 0;                ///< Row of the target cell
};

/**
 * @brief One sample of the mouse position over the board
 *
 * Coordinates are expressed in cell units: (0.5, 0.5) is the centre of the
 * top-left cell. Samples outside the board are kept, so the analysis can see
 * the cursor leaving the minefield.
 *
 * @since trainer-v1
 */
struct TrainerCursorSample
{
    qint64 tMs = 0; ///< Milliseconds since the board became playable
    float x = 0;    ///< Horizontal position in cells
    float y = 0;    ///< Vertical position in cells
};

/**
 * @brief Complete record of one game
 *
 * @since trainer-v1
 */
class TrainerRecording
{
public:
    int width = 0;               ///< Board width (columns)
    int height = 0;              ///< Board height (rows)
    int mines = 0;               ///< Number of mines
    bool useQuestionMark = false;///< Whether flags cycle through a question mark
    bool won = false;            ///< Whether the game was won
    qint64 durationMs = 0;       ///< Game duration as measured by the engine
    QDateTime startedAt;         ///< Wall-clock time the board was created
    QVector<bool> mineLayout;    ///< Final mine layout, index = y*width + x
    QVector<TrainerAction> actions;        ///< Player actions in order
    QVector<TrainerCursorSample> cursor;   ///< Cursor trajectory in order

    /**
     * @brief Whether the cell at (x, y) holds a mine
     */
    bool isMine(int x, int y) const;

    /**
     * @brief Serialise the recording to JSON
     */
    QJsonObject toJson() const;

    /**
     * @brief Parse a recording from JSON
     *
     * @param json Object produced by toJson()
     * @param out Destination recording
     * @param error Optional human readable error message
     * @return true on success
     */
    static bool fromJson(const QJsonObject& json, TrainerRecording& out, QString* error = nullptr);

    /**
     * @brief Write the recording to a JSON file
     */
    bool save(const QString& path, QString* error = nullptr) const;

    /**
     * @brief Read a recording from a JSON file
     */
    static bool load(const QString& path, TrainerRecording& out, QString* error = nullptr);

    /**
     * @brief Build a recording from a textual minefield ('*' = mine)
     *
     * Rows are separated by new lines; any non '*' character is a safe cell.
     * Used by tests and for hand-written training scenarios.
     */
    static TrainerRecording fromMinefieldString(const QString& minefield);
};

#endif // TRAINERRECORDING_H
