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

#include <QComboBox>
#include <QCursor>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <cmath>

#include "trainerspeeddialog.h"
#include "traineranalysisdialog.h"

namespace
{
const QColor TargetColor(255, 140, 20);
const int FlickTargets = 30;
}

// ===========================================================================
// TrainerFlickArena
// ===========================================================================

TrainerFlickArena::TrainerFlickArena(QWidget* parent) :
    QWidget(parent),
    rng(QRandomGenerator::global()->generate())
{
    setMouseTracking(true);
    setMinimumSize(450, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize TrainerFlickArena::sizeHint() const
{
    return QSize(Cols*28, Rows*28);
}

double TrainerFlickArena::cellSize() const
{
    return qMax(6.0, qMin(double(width())/Cols, double(height())/Rows));
}

QPointF TrainerFlickArena::origin() const
{
    const double s = cellSize();
    return QPointF((width() - s*Cols)/2, (height() - s*Rows)/2);
}

int TrainerFlickArena::cellAt(const QPointF& pos) const
{
    const double s = cellSize();
    const QPointF o = origin();
    const int x = int(std::floor((pos.x() - o.x())/s));
    const int y = int(std::floor((pos.y() - o.y())/s));
    if(x < 0 || y < 0 || x >= Cols || y >= Rows)
        return -1;
    return y*Cols + x;
}

void TrainerFlickArena::vStart(int targets)
{
    bRunning = true;
    total = targets;
    remaining = targets;
    hits = misses = 0;
    sumMs = sumReactionMs = sumEfficiency = 0;
    target = -1;
    clock.start();
    vNextTarget();
}

void TrainerFlickArena::vStop()
{
    bRunning = false;
    target = -1;
    update();
}

void TrainerFlickArena::vNextTarget()
{
    // Keep targets at least 4 cells apart so every target needs a real flick
    const int previous = target;
    for(int tries = 0; tries < 100; ++tries)
    {
        target = rng.bounded(Cols*Rows);
        if(previous < 0)
            break;
        const int dx = target % Cols - previous % Cols;
        const int dy = target / Cols - previous / Cols;
        if(std::hypot(dx, dy) >= 4)
            break;
    }
    shownAt = clock.elapsed();
    reactionAt = -1;
    startPos = mapFromGlobal(QCursor::pos());
    lastPos = startPos;
    travelled = 0;
    update();
}

void TrainerFlickArena::mouseMoveEvent(QMouseEvent* event)
{
    if(!bRunning)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF p = event->position();
#else
    const QPointF p = event->localPos();
#endif
    travelled += std::hypot(p.x() - lastPos.x(), p.y() - lastPos.y());
    lastPos = p;
    if(reactionAt < 0 && std::hypot(p.x() - startPos.x(), p.y() - startPos.y()) > cellSize()*0.3)
        reactionAt = clock.elapsed();
}

void TrainerFlickArena::mousePressEvent(QMouseEvent* event)
{
    if(!bRunning || event->button() != Qt::LeftButton)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF p = event->position();
#else
    const QPointF p = event->localPos();
#endif
    if(cellAt(p) != target)
    {
        ++misses;
        Q_EMIT SIGNAL_progress(hits, misses, remaining, -1);
        return;
    }

    const double ms = clock.elapsed() - shownAt;
    const double s = cellSize();
    const QPointF centre = origin() + QPointF((target % Cols + 0.5)*s, (target / Cols + 0.5)*s);
    travelled += std::hypot(p.x() - lastPos.x(), p.y() - lastPos.y());
    const double straight = std::hypot(centre.x() - startPos.x(), centre.y() - startPos.y());
    const double efficiency = travelled > 1 ? qMin(1.0, straight / travelled) : 1.0;

    ++hits;
    sumMs += ms;
    sumReactionMs += (reactionAt >= 0 ? reactionAt - shownAt : ms);
    sumEfficiency += efficiency;
    --remaining;
    Q_EMIT SIGNAL_progress(hits, misses, remaining, ms);

    if(remaining <= 0)
    {
        bRunning = false;
        target = -1;
        update();
        TrainerFlickResult r;
        r.targets = total;
        r.hits = hits;
        r.misses = misses;
        r.avgMs = sumMs / hits;
        r.avgReactionMs = sumReactionMs / hits;
        r.efficiency = sumEfficiency / hits;
        r.score = TrainerSpeed::flickScore(hits, misses, r.avgMs);
        Q_EMIT SIGNAL_finished(r);
        return;
    }
    vNextTarget();
}

void TrainerFlickArena::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double s = cellSize();
    const QPointF o = origin();

    for(int y = 0; y < Rows; ++y)
    {
        for(int x = 0; x < Cols; ++x)
        {
            const QRectF r(o.x() + x*s, o.y() + y*s, s, s);
            const bool isTarget = (y*Cols + x) == target;
            p.fillRect(r, isTarget ? TargetColor : QColor(160, 170, 182));
            p.setPen(QColor(205, 212, 220));
            p.drawLine(r.topLeft(), r.topRight());
            p.drawLine(r.topLeft(), r.bottomLeft());
            p.setPen(QColor(110, 118, 128));
            p.drawLine(r.bottomLeft(), r.bottomRight());
            p.drawLine(r.topRight(), r.bottomRight());
            if(isTarget)
            {
                p.setPen(QPen(Qt::white, qMax(2.0, s*0.1)));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(r.center(), s*0.28, s*0.28);
            }
        }
    }

    if(!bRunning)
    {
        p.fillRect(rect(), QColor(0, 0, 0, 90));
        QFont f = p.font();
        f.setPixelSize(qMax(14, int(s*0.9)));
        f.setBold(true);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, tr("Press Start"));
    }
}

// ===========================================================================
// TrainerSpeedDialog
// ===========================================================================

TrainerSpeedDialog::TrainerSpeedDialog(const QString& settingsFile, const TrainerHistory& h, QWidget* parent) :
    QDialog(parent),
    settingsPath(settingsFile),
    history(h)
{
    setWindowTitle(tr("Speed Trainer - MineCoach"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    resize(1100, 760);

    // Missed patterns from the player's own games are mixed into the sprint
    for(const TrainerPuzzle& p : history.candidates())
    {
        if(p.type == TrainerPuzzle::Type::SafeCell)
            ownPuzzles.append(p);
    }

    comboDrill = new QComboBox(this);
    comboDrill->addItems({tr("Flick targets"), tr("Pattern sprint"), tr("Chord sprint"),
                          tr("Flag + chord rhythm")});
    buttonStart = new QPushButton(tr("Start"), this);
    buttonStart->setDefault(true);
    labelDescription = new QLabel(this);
    labelDescription->setWordWrap(true);

    QHBoxLayout* top = new QHBoxLayout();
    top->addWidget(new QLabel(tr("Drill:"), this));
    top->addWidget(comboDrill);
    top->addWidget(buttonStart);
    top->addWidget(labelDescription, 1);

    labelHud = new QLabel(this);
    labelHud->setTextFormat(Qt::RichText);
    QFont hudFont = labelHud->font();
    hudFont.setPointSize(hudFont.pointSize() + 4);
    labelHud->setFont(hudFont);
    labelFeedback = new QLabel(this);
    labelFeedback->setTextFormat(Qt::RichText);

    arenaHolder = new QWidget(this);
    arenaLayout = new QVBoxLayout(arenaHolder);
    arenaLayout->setContentsMargins(0, 0, 0, 0);
    flick = new TrainerFlickArena(arenaHolder);
    arenaLayout->addWidget(flick);

    record = new QTextBrowser(this);
    record->setMaximumHeight(170);

    QPushButton* buttonClose = new QPushButton(tr("Close"), this);
    QHBoxLayout* bottom = new QHBoxLayout();
    bottom->addStretch();
    bottom->addWidget(buttonClose);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(labelHud);
    layout->addWidget(arenaHolder, 1);
    layout->addWidget(labelFeedback);
    layout->addWidget(record);
    layout->addLayout(bottom);

    ticker.setInterval(100);
    connect(&ticker, &QTimer::timeout, this, &TrainerSpeedDialog::SLOT_tick);
    connect(buttonStart, &QPushButton::clicked, this, &TrainerSpeedDialog::SLOT_start);
    connect(buttonClose, &QPushButton::clicked, this, &QDialog::accept);
    connect(comboDrill, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrainerSpeedDialog::SLOT_drillChanged);
    connect(flick, &TrainerFlickArena::SIGNAL_progress, this, &TrainerSpeedDialog::SLOT_flickProgress);
    connect(flick, &TrainerFlickArena::SIGNAL_finished, this, &TrainerSpeedDialog::SLOT_flickFinished);

    SLOT_drillChanged(0);
}

QString TrainerSpeedDialog::drillKey() const
{
    static const QStringList keys = {"Flick", "PatternSprint", "ChordSprint", "FlagChord"};
    return keys.value(comboDrill->currentIndex());
}

void TrainerSpeedDialog::SLOT_drillChanged(int index)
{
    // Stop whatever was running
    bRunning = false;
    ticker.stop();
    flick->vStop();
    delete board;
    board = nullptr;

    switch(index)
    {
        case Flick:
            labelDescription->setText(tr("%1 cells light up one at a time on an Expert sized grid. "
                                         "Click each as fast as you can.").arg(FlickTargets));
            break;
        case PatternSprint:
            labelDescription->setText(tr("%1 seconds: click a 100% safe cell in as many positions as you can. "
                                         "+100 per correct cell, -50 per mistake.").arg(SprintSeconds));
            break;
        case ChordSprint:
            labelDescription->setText(tr("%1 seconds: click the number whose chord opens the most cells "
                                         "(all flags are correct). +100 / -50.").arg(SprintSeconds));
            break;
        case FlagChord:
            labelDescription->setText(tr("%1 seconds: right-click to flag every mine around the highlighted "
                                         "number, then chord it (left or middle click). +100 / -50.")
                                      .arg(SprintSeconds));
            break;
    }
    flick->setVisible(index == Flick);
    labelHud->clear();
    labelFeedback->clear();
    buttonStart->setText(tr("Start"));
    vShowRecord();
}

void TrainerSpeedDialog::SLOT_start()
{
    labelFeedback->clear();
    if(comboDrill->currentIndex() == Flick)
    {
        labelHud->setText(tr("<b>%1</b> targets left").arg(FlickTargets));
        flick->setFocus();
        flick->vStart(FlickTargets);
        return;
    }

    correct = mistakes = streak = bestStreak = 0;
    sumCorrectMs = 0;
    bRunning = true;
    roundClock.start();
    ticker.start();
    buttonStart->setText(tr("Restart"));
    vNextPuzzle();
    vUpdateHud();
}

void TrainerSpeedDialog::SLOT_tick()
{
    if(!bRunning)
        return;
    if(roundClock.elapsed() >= SprintSeconds*1000)
    {
        bRunning = false;
        ticker.stop();
        delete board;
        board = nullptr;
        const QString details = tr("%1 correct, %2 mistakes, best streak %3, %4 s per correct answer")
                                .arg(correct).arg(mistakes).arg(bestStreak)
                                .arg(correct > 0 ? QString::number(sumCorrectMs/correct/1000.0, 'f', 2)
                                                 : QStringLiteral("-"));
        vFinishRound(TrainerSpeed::sprintScore(correct, mistakes), details);
        return;
    }
    vUpdateHud();
}

void TrainerSpeedDialog::vUpdateHud()
{
    const double left = qMax(0.0, SprintSeconds - roundClock.elapsed()/1000.0);
    labelHud->setText(tr("<b>%1 s</b> &nbsp;&nbsp; score <b>%2</b> &nbsp;&nbsp; correct %3 &nbsp; mistakes %4"
                         " &nbsp;&nbsp; streak %5")
                      .arg(QString::number(left, 'f', 1))
                      .arg(TrainerSpeed::sprintScore(correct, mistakes))
                      .arg(correct).arg(mistakes).arg(streak));
}

void TrainerSpeedDialog::vNextPuzzle()
{
    delete board;
    board = nullptr;

    bool ok = false;
    const int drill = comboDrill->currentIndex();
    if(drill == PatternSprint)
    {
        // Every third position comes from your own missed patterns, if any
        if(!ownPuzzles.isEmpty() && (correct + mistakes) % 3 == 2)
        {
            puzzle = ownPuzzles[ownPuzzleIndex++ % ownPuzzles.size()];
            position.recording = history.recording(puzzle.game);
            position.snapshot = puzzle.snapshot;
            ok = true;
        }
        else
            ok = speed.nextSafePuzzle(position, puzzle);
    }
    else if(drill == ChordSprint)
        ok = speed.nextChordPuzzle(position, puzzle);
    else if(drill == FlagChord)
        ok = speed.nextFlagChordPuzzle(position, puzzle);

    if(!ok)
    {
        labelFeedback->setText(tr("Could not generate a position, try again."));
        bRunning = false;
        ticker.stop();
        return;
    }

    liveState = puzzle.snapshot;
    board = new TrainerBoardView(position.recording, arenaHolder);
    board->setShowMines(false);
    board->setCrop(puzzle.view.isNull() ? puzzle.area : puzzle.view);
    TrainerBoardView::Overlay o;
    if(puzzle.type == TrainerPuzzle::Type::FlagAndChord)
        o.clueCells = {puzzle.chordCell};
    board->setState(liveState, o);
    arenaLayout->addWidget(board);
    connect(board, &TrainerBoardView::SIGNAL_cellPressed, this, &TrainerSpeedDialog::SLOT_cellPressed);
    board->show();
    puzzleClock.start();
}

void TrainerSpeedDialog::SLOT_cellPressed(int cell, int button)
{
    if(!bRunning || !board)
        return;
    const TrainerRecording& rec = position.recording;

    if(puzzle.type == TrainerPuzzle::Type::SafeCell || puzzle.type == TrainerPuzzle::Type::BestChord)
    {
        if(button == Qt::RightButton)
            return;
        if(puzzle.type == TrainerPuzzle::Type::SafeCell && button != Qt::LeftButton)
            return;
        QString feedback;
        const bool ok = TrainerHistory::checkAnswer(rec, puzzle, cell, &feedback);
        if(!ok && (feedback.startsWith("Click") || feedback.contains("already open") ||
                   feedback.contains("is flagged")))
            return; // not an answer
        vAnswered(ok, feedback);
        return;
    }

    // Flag + chord
    TrainerBoard b(rec);
    b.restore(liveState);
    if(button == Qt::RightButton)
    {
        if(b.revealed[cell] || b.marks[cell] == TrainerBoard::Mark::Flag)
            return;
        if(!puzzle.mustFlag.contains(cell))
        {
            vAnswered(false, tr("%1 is not one of the mines around the highlighted number.")
                             .arg(TrainerAnalyzer::cellName(cell, rec.width)));
            return;
        }
        b.toggleMark(cell);
        liveState = b.snapshot();
        TrainerBoardView::Overlay o;
        o.clueCells = {puzzle.chordCell};
        board->setState(liveState, o);
        return;
    }
    if(cell != puzzle.chordCell)
        return;
    for(int m : puzzle.mustFlag)
    {
        if(b.marks[m] != TrainerBoard::Mark::Flag)
        {
            vAnswered(false, tr("Chorded before all its mines were flagged."));
            return;
        }
    }
    vAnswered(true, tr("Chord opened %1 cell(s).").arg(puzzle.chordOpens));
}

void TrainerSpeedDialog::vAnswered(bool ok, const QString& feedback)
{
    const double ms = puzzleClock.elapsed();
    if(ok)
    {
        ++correct;
        ++streak;
        bestStreak = qMax(bestStreak, streak);
        sumCorrectMs += ms;
        labelFeedback->setText(QStringLiteral("<span style='color:#1e9a46'><b>%1</b> &nbsp;%2 s</span>")
                               .arg(feedback.toHtmlEscaped(), QString::number(ms/1000.0, 'f', 2)));
        vUpdateHud();
        vNextPuzzle();
        return;
    }

    ++mistakes;
    streak = 0;
    labelFeedback->setText(QStringLiteral("<span style='color:#d23030'><b>%1</b></span>")
                           .arg(feedback.toHtmlEscaped()));
    vUpdateHud();
    // Show the answer briefly, then move on
    TrainerBoardView::Overlay o;
    if(puzzle.type == TrainerPuzzle::Type::SafeCell)
    {
        o.groupCells = puzzle.deduction.group;
        o.clueCells = puzzle.deduction.clues;
    }
    else if(puzzle.type == TrainerPuzzle::Type::BestChord)
        o.suggestedCell = puzzle.chordCell;
    else
    {
        o.clueCells = {puzzle.chordCell};
        o.groupCells = puzzle.mustFlag;
        o.groupIsMine = true;
    }
    if(board)
        board->setState(liveState, o);
    bRunning = false;
    QTimer::singleShot(700, this, [this]()
    {
        if(!ticker.isActive())
            return; // round stopped or drill changed meanwhile
        if(roundClock.elapsed() >= SprintSeconds*1000)
        {
            bRunning = true; // let the ticker finish the round
            SLOT_tick();
            return;
        }
        bRunning = true;
        vNextPuzzle();
    });
}

void TrainerSpeedDialog::SLOT_flickProgress(int hits, int misses, int remaining, double lastMs)
{
    labelHud->setText(tr("<b>%1</b> targets left &nbsp;&nbsp; hits %2 &nbsp; misses %3")
                      .arg(remaining).arg(hits).arg(misses));
    if(lastMs >= 0)
        labelFeedback->setText(tr("%1 ms").arg(qRound(lastMs)));
    else
        labelFeedback->setText(tr("<span style='color:#d23030'>miss</span>"));
}

void TrainerSpeedDialog::SLOT_flickFinished(const TrainerFlickResult& r)
{
    vFinishRound(r.score, tr("%1 ms per target, reaction %2 ms, accuracy %3%, path efficiency %4%")
                 .arg(qRound(r.avgMs)).arg(qRound(r.avgReactionMs))
                 .arg(qRound(100.0*r.hits/(r.hits + r.misses)))
                 .arg(qRound(100.0*r.efficiency)));
}

void TrainerSpeedDialog::vFinishRound(int score, const QString& details)
{
    QSettings settings(settingsPath, QSettings::IniFormat);
    const QString key = QStringLiteral("Speed/") + drillKey();
    const int best = settings.value(key + "/Best", 0).toInt();
    const bool newBest = score > best;
    if(newBest)
        settings.setValue(key + "/Best", score);
    QStringList recent = settings.value(key + "/Recent").toStringList();
    recent.prepend(QStringLiteral("%1|%2|%3").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"))
                   .arg(score).arg(details));
    while(recent.size() > 20)
        recent.removeLast();
    settings.setValue(key + "/Recent", recent);

    labelHud->setText(tr("<b>Score %1</b>%2").arg(score)
                      .arg(newBest ? tr(" &nbsp;<span style='color:#1e9a46'>New personal best!</span>") : QString()));
    labelFeedback->setText(details.toHtmlEscaped());
    buttonStart->setText(tr("Again"));
    vShowRecord();
}

void TrainerSpeedDialog::vShowRecord()
{
    QSettings settings(settingsPath, QSettings::IniFormat);
    const QString key = QStringLiteral("Speed/") + drillKey();
    const int best = settings.value(key + "/Best", 0).toInt();
    const QStringList recent = settings.value(key + "/Recent").toStringList();

    QString html = tr("<p><b>Personal best: %1</b></p>").arg(best > 0 ? QString::number(best) : tr("none yet"));
    if(recent.isEmpty())
        html += tr("<p style='color:gray'>No rounds played yet.</p>");
    else
    {
        html += QStringLiteral("<table cellspacing='3'>");
        for(const QString& line : recent.mid(0, 10))
        {
            const QStringList parts = line.split('|');
            html += QStringLiteral("<tr><td style='color:gray'>%1</td><td align='right'>&nbsp;<b>%2</b>&nbsp;</td>"
                                   "<td>%3</td></tr>")
                    .arg(parts.value(0).toHtmlEscaped(), parts.value(1).toHtmlEscaped(),
                         parts.value(2).toHtmlEscaped());
        }
        html += QStringLiteral("</table>");
    }
    record->setHtml(html);
}
