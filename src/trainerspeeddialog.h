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
 * @file trainerspeeddialog.h
 * @brief Aim Lab style speed trainer window
 * @author MineCoach contributors
 * @date 2026-10-30
 *
 * Four timed drills with scores and personal bests:
 * - Flick targets: click lit cells on an Expert sized grid as fast as possible.
 * - Pattern sprint: click the safe cell in as many positions as possible.
 * - Chord sprint: chord the number that opens the most.
 * - Flag + chord rhythm: flag the mines around a number, then chord it.
 */

#ifndef TRAINERSPEEDDIALOG_H
#define TRAINERSPEEDDIALOG_H

#include <QDialog>
#include <QElapsedTimer>
#include <QPointF>
#include <QTimer>
#include <QWidget>

#include "trainerspeed.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTextBrowser;
class QVBoxLayout;
class TrainerBoardView;

/**
 * @brief Grid where single target cells light up to be clicked
 *
 * @since trainer-v1
 */
class TrainerFlickArena : public QWidget
{
    Q_OBJECT

public:
    explicit TrainerFlickArena(QWidget* parent = nullptr);

    /**
     * @brief Start a round of `targets` targets
     */
    void vStart(int targets);

    /**
     * @brief Stop the round without a result
     */
    void vStop();

    QSize sizeHint() const override;

Q_SIGNALS:
    /**
     * @brief A target was hit or missed (for the live display)
     */
    void SIGNAL_progress(int hits, int misses, int remaining, double lastMs);

    /**
     * @brief The round is over
     */
    void SIGNAL_finished(const TrainerFlickResult& result);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void vNextTarget();
    double cellSize() const;
    QPointF origin() const;
    int cellAt(const QPointF& pos) const;

    static const int Cols = 30;
    static const int Rows = 16;

    bool bRunning = false;
    int target = -1;
    int remaining = 0;
    int total = 0;
    int hits = 0;
    int misses = 0;
    double sumMs = 0;
    double sumReactionMs = 0;
    double sumEfficiency = 0;
    QElapsedTimer clock;
    qint64 shownAt = 0;
    qint64 reactionAt = -1;
    QPointF startPos;
    QPointF lastPos;
    double travelled = 0;
    QRandomGenerator rng;
};

/**
 * @brief The speed trainer window
 *
 * @since trainer-v1
 */
class TrainerSpeedDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Build the window
     *
     * @param settingsFile INI file personal bests and recent scores are kept in
     * @param history Optional analysed games whose missed patterns are mixed
     *                into the pattern sprint (may be empty)
     * @param parent Parent widget
     */
    TrainerSpeedDialog(const QString& settingsFile, const TrainerHistory& history, QWidget* parent = nullptr);

private Q_SLOTS:
    void SLOT_start();
    void SLOT_tick();
    void SLOT_drillChanged(int index);
    void SLOT_flickProgress(int hits, int misses, int remaining, double lastMs);
    void SLOT_flickFinished(const TrainerFlickResult& result);
    void SLOT_cellPressed(int cell, int button);

private:
    enum Drill { Flick = 0, PatternSprint = 1, ChordSprint = 2, FlagChord = 3 };

    void vNextPuzzle();
    void vAnswered(bool correct, const QString& feedback);
    void vFinishRound(int score, const QString& details);
    void vShowRecord();
    void vUpdateHud();
    QString drillKey() const;

    QString settingsPath;
    const TrainerHistory& history;
    QVector<TrainerPuzzle> ownPuzzles;
    TrainerSpeed speed;

    QComboBox* comboDrill;
    QLabel* labelDescription;
    QLabel* labelHud;
    QLabel* labelFeedback;
    QPushButton* buttonStart;
    QWidget* arenaHolder;
    QVBoxLayout* arenaLayout;
    TrainerFlickArena* flick;
    TrainerBoardView* board = nullptr;
    QTextBrowser* record;

    // Sprint state
    bool bRunning = false;
    QTimer ticker;
    QElapsedTimer roundClock;
    QElapsedTimer puzzleClock;
    int correct = 0;
    int mistakes = 0;
    int streak = 0;
    int bestStreak = 0;
    double sumCorrectMs = 0;
    TrainerSpeedPosition position;
    TrainerPuzzle puzzle;
    QByteArray liveState;      ///< Board state including flags placed in this puzzle
    int ownPuzzleIndex = 0;
    static const int SprintSeconds = 60;
};

#endif // TRAINERSPEEDDIALOG_H
