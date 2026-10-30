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
 * @file trainerrecorder.h
 * @brief Records player input and cursor movement during a game
 * @author LibreMines Trainer contributors
 * @date 2026-10-30
 *
 * TrainerRecorder listens to the GUI's cell signals (reveal, flag, chord)
 * and samples the mouse position over the board at a high rate. When the
 * game ends it stores the mine layout and writes the recording to the
 * application data directory so it can be analysed immediately or later.
 */

#ifndef TRAINERRECORDER_H
#define TRAINERRECORDER_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <functional>

#include "trainerrecording.h"

/**
 * @brief Captures one game at a time into a TrainerRecording
 *
 * @since trainer-v1
 */
class TrainerRecorder : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Create an idle recorder
     */
    explicit TrainerRecorder(QObject* parent = nullptr);

    /**
     * @brief Start recording a new game
     *
     * @param width Board columns
     * @param height Board rows
     * @param mines Number of mines
     * @param useQuestionMark Whether the flag cycle includes question marks
     * @param board Widget holding the cells (cursor is mapped into it)
     * @param cellLength Callback returning the current cell size in pixels
     */
    void vStart(int width, int height, int mines, bool useQuestionMark,
                QWidget* board, std::function<int()> cellLength);

    /**
     * @brief Discard the game in progress (restart / quit)
     */
    void vStop();

    /**
     * @brief Finish the game, store the layout and save it to disk
     *
     * @param won Whether the game was won
     * @param mineLayout Final mine layout, index = y*width + x
     * @return true if a recording with at least one action was produced
     */
    bool bFinish(bool won, const QVector<bool>& mineLayout);

    /**
     * @brief Whether a game is being recorded
     */
    bool isRecording() const;

    /**
     * @brief Whether a finished recording is available
     */
    bool hasLastRecording() const;

    /**
     * @brief The most recently finished recording
     */
    const TrainerRecording& lastRecording() const;

    /**
     * @brief File the last recording was saved to (empty if saving failed)
     */
    QString lastRecordingPath() const;

    /**
     * @brief Directory where recordings are written
     */
    static QString recordingsDirectory();

public Q_SLOTS:
    /**
     * @brief Record a left click on a hidden cell
     */
    void SLOT_reveal(const uchar x, const uchar y);

    /**
     * @brief Record a flag toggle
     */
    void SLOT_flag(const uchar x, const uchar y);

    /**
     * @brief Record a chord on a revealed number
     */
    void SLOT_chord(const uchar x, const uchar y);

private Q_SLOTS:
    /**
     * @brief Sample the cursor position (called by the internal timer)
     */
    void SLOT_sampleCursor();

private:
    void vRecordAction(TrainerAction::Type type, int x, int y);

    TrainerRecording current;          ///< Game being recorded
    TrainerRecording last;             ///< Last finished game
    QString lastPath;                  ///< Where the last game was saved
    bool bRecording = false;           ///< A game is in progress
    bool bHasLast = false;             ///< last holds a finished game
    QElapsedTimer clock;               ///< Time base of the current game
    QTimer sampler;                    ///< Cursor sampling timer
    QPointer<QWidget> boardWidget;     ///< Widget holding the cells
    std::function<int()> cellLengthFn; ///< Current cell size in pixels
};

#endif // TRAINERRECORDER_H
