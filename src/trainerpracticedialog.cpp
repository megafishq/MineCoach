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

#include <QApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "trainerpracticedialog.h"
#include "traineranalysisdialog.h"

// ===========================================================================
// TrainerHistoryDialog
// ===========================================================================

TrainerHistoryDialog::TrainerHistoryDialog(const QString& recordingsDir, QWidget* parent) :
    QDialog(parent),
    dir(recordingsDir)
{
    setWindowTitle(tr("Recent Games - MineCoach"));
    resize(760, 680);

    const int available = TrainerHistory::recentRecordings(dir, 0).size();

    sbGames = new QSpinBox(this);
    sbGames->setRange(1, qMax(1, available));
    sbGames->setValue(qMin(50, qMax(1, available)));
    QPushButton* buttonAnalyze = new QPushButton(tr("Analyze"), this);
    labelAvailable = new QLabel(tr("(%1 recorded games)").arg(available), this);

    QHBoxLayout* top = new QHBoxLayout();
    top->addWidget(new QLabel(tr("Analyze the last"), this));
    top->addWidget(sbGames);
    top->addWidget(new QLabel(tr("games"), this));
    top->addWidget(buttonAnalyze);
    top->addWidget(labelAvailable);
    top->addStretch();

    text = new QTextBrowser(this);

    sbPuzzles = new QSpinBox(this);
    sbPuzzles->setRange(1, 100);
    sbPuzzles->setValue(20);
    buttonPractice = new QPushButton(tr("Start practice"), this);
    buttonPractice->setEnabled(false);
    QPushButton* buttonClose = new QPushButton(tr("Close"), this);

    QHBoxLayout* bottom = new QHBoxLayout();
    bottom->addWidget(new QLabel(tr("Practice playlist of"), this));
    bottom->addWidget(sbPuzzles);
    bottom->addWidget(new QLabel(tr("puzzles from your own missed moments"), this));
    bottom->addWidget(buttonPractice);
    bottom->addStretch();
    bottom->addWidget(buttonClose);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(text, 1);
    layout->addLayout(bottom);

    connect(buttonAnalyze, &QPushButton::clicked, this, &TrainerHistoryDialog::SLOT_analyze);
    connect(buttonPractice, &QPushButton::clicked, this, &TrainerHistoryDialog::SLOT_practice);
    connect(buttonClose, &QPushButton::clicked, this, &QDialog::accept);

    if(available > 0)
        QTimer::singleShot(0, this, &TrainerHistoryDialog::SLOT_analyze);
    else
        text->setHtml(tr("<p>No recorded games yet. Play a few games and come back.</p>"));
}

void TrainerHistoryDialog::SLOT_analyze()
{
    const QStringList files = TrainerHistory::recentRecordings(dir, sbGames->value());
    history = TrainerHistory();

    QProgressDialog progress(tr("Analyzing games..."), tr("Stop"), 0, files.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);
    for(int i = 0; i < files.size(); ++i)
    {
        progress.setValue(i);
        if(progress.wasCanceled())
            break;
        history.addFile(files[i]);
        qApp->processEvents();
    }
    progress.setValue(files.size());

    const TrainerHistorySummary s = history.summary();

    auto row = [](const QString& name, int count, const QString& tip)
    {
        return QStringLiteral("<tr><td><b>%1</b></td><td align='right'>&nbsp;%2&nbsp;</td><td>%3</td></tr>")
               .arg(name.toHtmlEscaped()).arg(count).arg(tip.toHtmlEscaped());
    };

    QString html;
    html += tr("<h2>Your last %1 games</h2>").arg(s.games);
    html += tr("<p>Won %1 of %2 &nbsp;|&nbsp; average 3BV/s <b>%3</b> &nbsp;|&nbsp; average efficiency <b>%4%</b><br>"
               "Cursor jumps %5, of which <b>%6 left a safe cell behind</b> &nbsp;|&nbsp; unnecessary guesses %7"
               " &nbsp;|&nbsp; wasted clicks %8 &nbsp;|&nbsp; clicks better chording would save %9</p>")
            .arg(s.wins).arg(s.games)
            .arg(QString::number(s.avgThreeBVPerSecond, 'f', 2))
            .arg(QString::number(s.avgEfficiency, 'f', 0))
            .arg(s.jumps).arg(s.missedPatterns).arg(s.guesses).arg(s.wastedClicks).arg(s.savableClicks);

    html += tr("<h3>Patterns you miss most</h3>");
    if(s.missedByPattern.isEmpty())
        html += tr("<p>No missed patterns. Nice.</p>");
    else
    {
        html += QStringLiteral("<table cellspacing='4'>");
        for(const auto& m : s.missedByPattern)
            html += row(m.first, m.second, TrainerHistory::tipFor(m.first));
        html += QStringLiteral("</table>");
    }

    html += tr("<h3>Most common mistakes</h3>");
    if(s.mistakes.isEmpty())
        html += tr("<p>None found.</p>");
    else
    {
        html += QStringLiteral("<table cellspacing='4'>");
        for(const auto& m : s.mistakes)
            html += row(m.first, m.second, TrainerHistory::tipFor(m.first));
        html += QStringLiteral("</table>");
    }

    html += tr("<h3>Practice</h3><p>%1 positions from these games can be practiced. "
               "The playlist favours the patterns you miss most.</p>").arg(s.puzzleCandidates);
    text->setHtml(html);

    buttonPractice->setEnabled(s.puzzleCandidates > 0);
    sbPuzzles->setMaximum(qMax(1, s.puzzleCandidates));
    sbPuzzles->setValue(qMin(20, qMax(1, s.puzzleCandidates)));
}

void TrainerHistoryDialog::SLOT_practice()
{
    const QVector<TrainerPuzzle> list = history.playlist(sbPuzzles->value());
    if(list.isEmpty())
        return;
    TrainerPracticeDialog dialog(history, list, this);
    dialog.exec();
}

// ===========================================================================
// TrainerPracticeDialog
// ===========================================================================

TrainerPracticeDialog::TrainerPracticeDialog(const TrainerHistory& h, const QVector<TrainerPuzzle>& playlist,
                                             QWidget* parent) :
    QDialog(parent),
    history(h),
    puzzles(playlist)
{
    setWindowTitle(tr("Practice - MineCoach"));
    resize(900, 720);

    labelTitle = new QLabel(this);
    labelTitle->setTextFormat(Qt::RichText);
    labelSource = new QLabel(this);
    labelSource->setStyleSheet("color: gray;");
    labelPrompt = new QLabel(this);
    labelPrompt->setWordWrap(true);
    labelResult = new QLabel(this);
    labelResult->setTextFormat(Qt::RichText);
    labelResult->setWordWrap(true);

    boardHolder = new QWidget(this);
    boardLayout = new QVBoxLayout(boardHolder);
    boardLayout->setContentsMargins(0, 0, 0, 0);

    explanation = new QTextBrowser(this);
    explanation->setOpenLinks(false);

    buttonSolution = new QPushButton(tr("Show solution"), this);
    buttonNext = new QPushButton(tr("Next"), this);
    QPushButton* buttonClose = new QPushButton(tr("Close"), this);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(buttonSolution);
    buttons->addStretch();
    buttons->addWidget(buttonNext);
    buttons->addWidget(buttonClose);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(labelTitle);
    layout->addWidget(labelSource);
    layout->addWidget(labelPrompt);
    layout->addWidget(boardHolder, 3);
    layout->addWidget(labelResult);
    layout->addWidget(explanation, 2);
    layout->addLayout(buttons);

    connect(buttonSolution, &QPushButton::clicked, this, &TrainerPracticeDialog::SLOT_showSolution);
    connect(buttonNext, &QPushButton::clicked, this, &TrainerPracticeDialog::SLOT_next);
    connect(buttonClose, &QPushButton::clicked, this, &QDialog::accept);

    auto focus = [this](const QUrl& url)
    {
        if(current >= puzzles.size())
            return;
        const int cell = TrainerAnalysisDialog::cellFromLink(url, history.recording(puzzles[current].game).width);
        if(view)
            view->setFocusCell(cell);
        if(simple)
            simple->setFocusCell(cell);
    };
    connect(explanation, &QTextBrowser::highlighted, this, focus);
    connect(explanation, &QTextBrowser::anchorClicked, this, focus);

    vShowPuzzle();
}

void TrainerPracticeDialog::vShowPuzzle()
{
    // Fresh views: a board view is bound to one recording
    delete view;
    delete simple;
    view = nullptr;
    simple = nullptr;
    bAnswered = false;

    const TrainerPuzzle& p = puzzles[current];
    const TrainerRecording& rec = history.recording(p.game);

    labelTitle->setText(tr("<h2 style='margin:0'>Puzzle %1 of %2 &nbsp;<span style='color:gray'>%3</span></h2>")
                        .arg(current + 1).arg(puzzles.size()).arg(p.category.toHtmlEscaped()));
    labelSource->setText(tr("From your game of %1, move %2")
                         .arg(rec.startedAt.toString("yyyy-MM-dd HH:mm"))
                         .arg(p.actionIndex + 1));
    if(p.type == TrainerPuzzle::Type::SafeCell)
        labelPrompt->setText(tr("Click a cell that is 100% safe. Only the numbers shown count; "
                                "the flags are the ones you had placed."));
    else
        labelPrompt->setText(tr("Several numbers can be chorded. Click the one whose chord opens the most cells."));
    labelResult->clear();
    explanation->clear();

    view = new TrainerBoardView(rec, boardHolder);
    view->setState(p.snapshot, TrainerBoardView::Overlay());
    view->setShowMines(false);
    view->setCrop(p.view.isNull() ? p.area : p.view);
    connect(view, &TrainerBoardView::SIGNAL_cellClicked, this, &TrainerPracticeDialog::SLOT_cellClicked);

    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(view, 1);
    if(p.type == TrainerPuzzle::Type::SafeCell)
    {
        simple = new TrainerBoardView(rec, boardHolder);
        simple->setState(p.snapshot, TrainerBoardView::Overlay());
        simple->setCrop(p.view.isNull() ? p.area : p.view);
        simple->setSimplified(p.deduction);
        simple->hide();
        row->addWidget(simple, 1);
    }
    // Replace the previous row layout
    while(QLayoutItem* item = boardLayout->takeAt(0))
        delete item;
    boardLayout->addLayout(row);

    buttonSolution->setEnabled(true);
    buttonNext->setEnabled(false);
    buttonNext->setText(current + 1 < puzzles.size() ? tr("Next") : tr("Finish"));
}

void TrainerPracticeDialog::SLOT_cellClicked(int cell)
{
    if(bAnswered || current >= puzzles.size())
        return;
    QString feedback;
    const bool ok = TrainerHistory::checkAnswer(history.recording(puzzles[current].game), puzzles[current],
                                                cell, &feedback);
    // Clicks outside the puzzle or on open cells are not answers yet
    if(!ok && (feedback.startsWith("Click") || feedback.contains("already open") || feedback.contains("is flagged")))
    {
        labelResult->setText(QStringLiteral("<span style='color:gray'>%1</span>").arg(feedback.toHtmlEscaped()));
        return;
    }
    vAnswer(cell, ok, feedback);
}

void TrainerPracticeDialog::SLOT_showSolution()
{
    if(bAnswered || current >= puzzles.size())
        return;
    vAnswer(-1, false, tr("Here is the solution."));
}

void TrainerPracticeDialog::vAnswer(int cell, bool correct, const QString& feedback)
{
    bAnswered = true;
    const TrainerPuzzle& p = puzzles[current];
    const TrainerRecording& rec = history.recording(p.game);

    QPair<int, int>& score = scores[p.category];
    score.second++;
    if(correct)
        score.first++;

    labelResult->setText(QStringLiteral("<b style='color:%1'>%2</b>")
                         .arg(correct ? "#1e9a46" : "#d23030", feedback.toHtmlEscaped()));

    TrainerBoard board(rec);
    board.restore(p.snapshot);

    TrainerBoardView::Overlay o;
    QString html;
    if(p.type == TrainerPuzzle::Type::SafeCell)
    {
        o.clueCells = p.deduction.clues;
        o.groupCells = p.deduction.group;
        o.groupIsMine = false;
        o.extraCells = p.deduction.shared;
        if(simple)
            simple->show();
        html += QStringLiteral("<p><b>%1</b></p><ol>").arg(p.category.toHtmlEscaped());
        for(const QString& step : TrainerSolver::explainSteps(board, p.deduction, p.prerequisites))
            html += QStringLiteral("<li>%1</li>").arg(TrainerAnalysisDialog::linkifyCells(step.toHtmlEscaped()));
        html += QStringLiteral("</ol>");
    }
    else
    {
        o.suggestedCell = p.chordCell;
        o.extraCells = board.chordPreview(p.chordCell);
        html += tr("<p>Chording the %1 at %2 opens %3 cell(s), the most of any satisfied number here.</p>")
                .arg(board.value[p.chordCell])
                .arg(TrainerAnalysisDialog::linkifyCells(TrainerAnalyzer::cellName(p.chordCell, rec.width)))
                .arg(p.chordOpens);
    }
    if(cell >= 0 && !correct)
        o.actionCell = cell;
    view->setState(p.snapshot, o);

    const QString tip = TrainerHistory::tipFor(p.category);
    if(!tip.isEmpty())
        html += tr("<p style='color:gray'>Tip: %1</p>").arg(tip.toHtmlEscaped());
    explanation->setHtml(html);

    buttonSolution->setEnabled(false);
    buttonNext->setEnabled(true);
    buttonNext->setFocus();
}

void TrainerPracticeDialog::SLOT_next()
{
    if(current + 1 < puzzles.size())
    {
        ++current;
        vShowPuzzle();
        return;
    }
    current = puzzles.size();
    vShowSummary();
}

void TrainerPracticeDialog::vShowSummary()
{
    delete view;
    delete simple;
    view = nullptr;
    simple = nullptr;

    int correct = 0, played = 0;
    QString rows;
    for(auto it = scores.constBegin(); it != scores.constEnd(); ++it)
    {
        correct += it.value().first;
        played += it.value().second;
        rows += QStringLiteral("<tr><td>%1</td><td align='right'>&nbsp;%2 / %3&nbsp;</td></tr>")
                .arg(it.key().toHtmlEscaped()).arg(it.value().first).arg(it.value().second);
    }
    labelTitle->setText(tr("<h2 style='margin:0'>Practice finished: %1 / %2 correct</h2>").arg(correct).arg(played));
    labelSource->clear();
    labelPrompt->setText(tr("Patterns you got wrong are worth another round; play a few games and analyze again "
                            "to get fresh puzzles."));
    labelResult->clear();
    explanation->setHtml(QStringLiteral("<table cellspacing='4'>%1</table>").arg(rows));
    buttonSolution->setEnabled(false);
    buttonNext->setEnabled(false);
}
