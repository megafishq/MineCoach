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
 * @file traineranalysisdialog.h
 * @brief Dialog presenting the trainer analysis of a recorded game
 * @author LibreMines Trainer contributors
 * @date 2026-10-30
 *
 * The dialog shows the game statistics, the list of findings produced by
 * TrainerAnalyzer and a board view that replays the game. Selecting a finding
 * rewinds the board to the moment of the mistake and highlights the previous
 * click, the cursor path, the missed pattern and the suggested cell or chord.
 */

#ifndef TRAINERANALYSISDIALOG_H
#define TRAINERANALYSISDIALOG_H

#include <QDialog>
#include <QWidget>

#include "traineranalyzer.h"
#include "trainerrecording.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QSlider;
class QTextBrowser;

/**
 * @brief Paints a replayed board with trainer overlays
 *
 * @since trainer-v1
 */
class TrainerBoardView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Cells and path to highlight on top of the board
     */
    struct Overlay
    {
        int fromCell = -1;        ///< Previous click (blue)
        int actionCell = -1;      ///< Click being examined (red)
        int suggestedCell = -1;   ///< Better cell or chord (green)
        QVector<int> clueCells;   ///< Numbers forming the missed pattern (yellow)
        QVector<int> extraCells;  ///< Additional cells (purple)
        qint64 cursorFromMs = -1; ///< Cursor path start
        qint64 cursorToMs = -1;   ///< Cursor path end
    };

    /**
     * @brief Create a view for the given recording
     */
    TrainerBoardView(const TrainerRecording& recording, QWidget* parent = nullptr);

    /**
     * @brief Show a board state produced by TrainerBoard::snapshot()
     */
    void setState(const QByteArray& snapshot, const Overlay& overlay);

    /**
     * @brief Toggle drawing of mines under hidden cells
     */
    void setShowMines(bool show);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const TrainerRecording& rec;
    TrainerBoard board;
    QByteArray state;
    Overlay overlay;
    bool bShowMines = true;
};

/**
 * @brief Post-game analysis window
 *
 * @since trainer-v1
 */
class TrainerAnalysisDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Analyse the recording and build the window
     */
    TrainerAnalysisDialog(const TrainerRecording& recording, QWidget* parent = nullptr);

private Q_SLOTS:
    void SLOT_findingSelected(int row);
    void SLOT_stepChanged(int step);
    void SLOT_filterChanged(int index);

private:
    void vFillFindings();
    QString actionText(int index) const;
    bool bFilterAccepts(const TrainerFinding& f) const;

    TrainerRecording rec;
    TrainerReport report;

    TrainerBoardView* boardView;
    QLabel* labelSummary;
    QComboBox* comboFilter;
    QListWidget* listFindings;
    QTextBrowser* textDetail;
    QSlider* sliderStep;
    QLabel* labelStep;
    QCheckBox* cbShowMines;
    QVector<int> visibleFindings;  ///< list row -> index in report.findings
    bool bSyncing = false;
};

#endif // TRAINERANALYSISDIALOG_H
