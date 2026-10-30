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
 * @brief Dialogs presenting the trainer analysis of a recorded game
 * @author MineCoach contributors
 * @date 2026-10-30
 *
 * TrainerAnalysisDialog shows the game statistics, the list of findings
 * produced by TrainerAnalyzer and a board view that replays the game.
 * Selecting a finding rewinds the board to the moment of the mistake and
 * highlights the previous click, the cursor path, the missed pattern and the
 * suggested cell or chord. Cell coordinates in the text are links: hovering
 * one highlights that cell on the board. "Read more" opens a
 * TrainerPatternDialog that shows the pattern exactly as it was on the board
 * next to a simplified version with a step by step explanation.
 */

#ifndef TRAINERANALYSISDIALOG_H
#define TRAINERANALYSISDIALOG_H

#include <QDialog>
#include <QRect>
#include <QWidget>

#include "traineranalyzer.h"
#include "trainerrecording.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QTextBrowser;
class QUrl;

/**
 * @brief Paints a replayed board (or part of it) with trainer overlays
 *
 * Column numbers are drawn above the board and row numbers on the left so
 * every "(column,row)" reference can be found at a glance. Hovering a cell
 * shows its coordinates.
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
        QVector<int> groupCells;  ///< Cells proven by the pattern (green = safe, red = mine)
        bool groupIsMine = false; ///< Colour of groupCells
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

    /**
     * @brief Only draw the given rectangle of cells (null rect = whole board)
     */
    void setCrop(const QRect& cells);

    /**
     * @brief Draw the simplified view of a deduction instead of the real board
     *
     * Numbers show the mines they still need, mines already accounted for are
     * drawn dark, cells that play no part are faded, the shared cells of a
     * pattern are tinted and the proven cells are green (safe) or red (mine).
     */
    void setSimplified(const TrainerDeduction& deduction);

    /**
     * @brief Emphasise one cell and its row/column labels (-1 = none)
     */
    void setFocusCell(int cell);

    QSize sizeHint() const override;

Q_SIGNALS:
    /**
     * @brief The mouse moved onto another cell (-1 when leaving the board)
     */
    void SIGNAL_cellHovered(int cell);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Geometry
    {
        double s = 0;   ///< Cell size in pixels
        double ox = 0;  ///< X of the first drawn column
        double oy = 0;  ///< Y of the first drawn row
    };
    Geometry geometry() const;
    QRect drawnCells() const;
    int cellAt(const QPointF& pos) const;
    void paintSimplifiedCell(QPainter& p, const QRectF& r, int i) const;

    const TrainerRecording& rec;
    TrainerBoard board;
    QByteArray state;
    Overlay overlay;
    bool bShowMines = true;
    QRect crop;
    bool bSimplified = false;
    TrainerDeduction simplified;
    int focusCell = -1;
    int hoverCell = -1;
};

/**
 * @brief "Read more" window explaining one deduction
 *
 * Left: the pattern exactly as it was on the board. Right: the same area
 * reduced to what matters. Below: the reasoning step by step.
 *
 * @since trainer-v1
 */
class TrainerPatternDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Build the window
     *
     * @param recording The recorded game
     * @param snapshot Board state the deduction was made on
     * @param deduction The deduction to explain
     * @param parent Parent widget
     */
    TrainerPatternDialog(const TrainerRecording& recording, const QByteArray& snapshot,
                         const TrainerDeduction& deduction, QWidget* parent = nullptr);
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

    /**
     * @brief Turn "(c,r)" cell names in escaped HTML into hoverable links
     */
    static QString linkifyCells(const QString& html);

    /**
     * @brief Cell index of a link produced by linkifyCells() (-1 if none)
     */
    static int cellFromLink(const QUrl& url, int width);

private Q_SLOTS:
    void SLOT_findingSelected(int row);
    void SLOT_stepChanged(int step);
    void SLOT_filterChanged(int index);
    void SLOT_readMore();

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
    QPushButton* buttonReadMore;
    QSlider* sliderStep;
    QLabel* labelStep;
    QCheckBox* cbShowMines;
    QVector<int> visibleFindings;  ///< list row -> index in report.findings
    int currentFinding = -1;       ///< Index in report.findings shown in the detail panel
    bool bSyncing = false;
};

#endif // TRAINERANALYSISDIALOG_H
