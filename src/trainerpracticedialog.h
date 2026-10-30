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
 * @file trainerpracticedialog.h
 * @brief Recent games overview and practice playlist windows
 * @author MineCoach contributors
 * @date 2026-10-30
 *
 * TrainerHistoryDialog analyses the most recent recordings, shows the most
 * common mistakes and most missed patterns, and starts a practice playlist.
 * TrainerPracticeDialog plays that playlist: each puzzle is a small area cut
 * from one of the player's own games at the moment a pattern was missed.
 */

#ifndef TRAINERPRACTICEDIALOG_H
#define TRAINERPRACTICEDIALOG_H

#include <QDialog>
#include <QMap>
#include <QPair>

#include "trainerhistory.h"

class QLabel;
class QPushButton;
class QSpinBox;
class QTextBrowser;
class QVBoxLayout;
class TrainerBoardView;

/**
 * @brief Overview of the most recent games and entry point to practice
 *
 * @since trainer-v1
 */
class TrainerHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Build the window and analyse the default number of games
     *
     * @param recordingsDir Directory with the trainer recordings
     * @param parent Parent widget
     */
    explicit TrainerHistoryDialog(const QString& recordingsDir, QWidget* parent = nullptr);

private Q_SLOTS:
    void SLOT_analyze();
    void SLOT_practice();

private:
    QString dir;
    TrainerHistory history;
    QSpinBox* sbGames;
    QSpinBox* sbPuzzles;
    QLabel* labelAvailable;
    QTextBrowser* text;
    QPushButton* buttonPractice;
};

/**
 * @brief Plays a practice playlist of mini puzzles
 *
 * @since trainer-v1
 */
class TrainerPracticeDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Build the window
     *
     * @param history Games the puzzles come from (must outlive the dialog)
     * @param playlist Puzzles to play, in order
     * @param parent Parent widget
     */
    TrainerPracticeDialog(const TrainerHistory& history, const QVector<TrainerPuzzle>& playlist,
                          QWidget* parent = nullptr);

private Q_SLOTS:
    void SLOT_cellClicked(int cell);
    void SLOT_next();
    void SLOT_showSolution();

private:
    void vShowPuzzle();
    void vAnswer(int cell, bool correct, const QString& feedback);
    void vShowSummary();

    const TrainerHistory& history;
    QVector<TrainerPuzzle> puzzles;
    int current = 0;
    bool bAnswered = false;
    QMap<QString, QPair<int, int>> scores; ///< category -> (correct, played)

    QLabel* labelTitle;
    QLabel* labelSource;
    QLabel* labelPrompt;
    QLabel* labelResult;
    QWidget* boardHolder;
    QVBoxLayout* boardLayout;
    TrainerBoardView* view = nullptr;
    TrainerBoardView* simple = nullptr;
    QTextBrowser* explanation;
    QPushButton* buttonSolution;
    QPushButton* buttonNext;
};

#endif // TRAINERPRACTICEDIALOG_H
