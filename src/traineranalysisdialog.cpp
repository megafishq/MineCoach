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

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <cmath>

#include "traineranalysisdialog.h"

namespace
{
const QColor ColorFrom(30, 110, 230);
const QColor ColorAction(220, 40, 40);
const QColor ColorSuggested(30, 170, 70);
const QColor ColorClue(245, 190, 20);
const QColor ColorExtra(150, 90, 210);
const QColor ColorCursor(255, 120, 0);

QColor numberColor(int n)
{
    static const QColor colors[] = {
        QColor(0, 0, 0),
        QColor(25, 118, 210), QColor(56, 142, 60), QColor(211, 47, 47), QColor(40, 53, 147),
        QColor(141, 42, 42), QColor(0, 131, 143), QColor(20, 20, 20), QColor(110, 110, 110)
    };
    return colors[qBound(0, n, 8)];
}

QString swatch(const QColor& c, const QString& text)
{
    return QStringLiteral("<span style='background:%1'>&nbsp;&nbsp;&nbsp;</span>&nbsp;%2&nbsp;&nbsp;")
           .arg(c.name(), text);
}
}

// ===========================================================================
// TrainerBoardView
// ===========================================================================

TrainerBoardView::TrainerBoardView(const TrainerRecording& recording, QWidget* parent) :
    QWidget(parent),
    rec(recording),
    board(recording)
{
    state = board.snapshot();
    setMinimumSize(200, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void TrainerBoardView::setState(const QByteArray& snapshot, const Overlay& o)
{
    state = snapshot;
    overlay = o;
    update();
}

void TrainerBoardView::setShowMines(bool show)
{
    bShowMines = show;
    update();
}

QSize TrainerBoardView::sizeHint() const
{
    return QSize(qMax(1, rec.width)*30, qMax(1, rec.height)*30);
}

void TrainerBoardView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    if(rec.width <= 0 || rec.height <= 0)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double s = qMax(6.0, qMin(double(width())/rec.width, double(height())/rec.height));
    const double ox = (width() - s*rec.width)/2;
    const double oy = (height() - s*rec.height)/2;
    auto cellRect = [&](int i)
    {
        return QRectF(ox + (i % rec.width)*s, oy + (i / rec.width)*s, s, s);
    };

    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(qMax(6, int(s*0.6)));
    p.setFont(font);

    for(int i=0; i<rec.width*rec.height; ++i)
    {
        const QRectF r = cellRect(i);
        const char c = i < state.size() ? state[i] : 'h';
        const bool mine = board.isMine(i);

        if(c == 'r')
        {
            p.fillRect(r, QColor(232, 232, 232));
            p.setPen(QColor(200, 200, 200));
            p.drawRect(r);
            const int v = board.value[i];
            if(v > 0)
            {
                p.setPen(numberColor(v));
                p.drawText(r, Qt::AlignCenter, QString::number(v));
            }
            continue;
        }

        // Hidden cell with a small bevel
        p.fillRect(r, QColor(160, 170, 182));
        p.setPen(QColor(205, 212, 220));
        p.drawLine(r.topLeft(), r.topRight());
        p.drawLine(r.topLeft(), r.bottomLeft());
        p.setPen(QColor(110, 118, 128));
        p.drawLine(r.bottomLeft(), r.bottomRight());
        p.drawLine(r.topRight(), r.bottomRight());

        if(bShowMines && mine && c != 'f')
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(30, 30, 30, 110));
            p.drawEllipse(r.center(), s*0.18, s*0.18);
            p.setBrush(Qt::NoBrush);
        }

        if(c == 'f')
        {
            const QPointF base(r.left() + s*0.38, r.top() + s*0.78);
            p.setPen(QPen(QColor(40, 40, 40), qMax(1.0, s*0.06)));
            p.drawLine(base, QPointF(base.x(), r.top() + s*0.2));
            QPainterPath flag;
            flag.moveTo(base.x(), r.top() + s*0.2);
            flag.lineTo(r.left() + s*0.75, r.top() + s*0.34);
            flag.lineTo(base.x(), r.top() + s*0.48);
            flag.closeSubpath();
            p.fillPath(flag, QColor(220, 30, 30));
            if(bShowMines && !mine)
            {
                p.setPen(QPen(QColor(0, 0, 0), qMax(1.0, s*0.08)));
                p.drawLine(r.topLeft() + QPointF(s*0.2, s*0.2), r.bottomRight() - QPointF(s*0.2, s*0.2));
                p.drawLine(r.topRight() + QPointF(-s*0.2, s*0.2), r.bottomLeft() + QPointF(s*0.2, -s*0.2));
            }
        }
        else if(c == 'q')
        {
            p.setPen(QColor(30, 30, 30));
            p.drawText(r, Qt::AlignCenter, QStringLiteral("?"));
        }
    }

    // ---- Overlays ---------------------------------------------------------
    const double border = qMax(2.0, s*0.11);
    auto outline = [&](int cell, const QColor& color, double inset)
    {
        if(cell < 0)
            return;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(color, border));
        p.drawRect(cellRect(cell).adjusted(inset, inset, -inset, -inset));
    };

    for(int c : overlay.extraCells)
    {
        QColor fill = ColorExtra;
        fill.setAlpha(80);
        p.fillRect(cellRect(c), fill);
    }
    if(overlay.suggestedCell >= 0)
    {
        QColor fill = ColorSuggested;
        fill.setAlpha(90);
        p.fillRect(cellRect(overlay.suggestedCell), fill);
    }
    for(int c : overlay.clueCells)
        outline(c, ColorClue, border/2);
    outline(overlay.suggestedCell, ColorSuggested, border/2);
    outline(overlay.fromCell, ColorFrom, border*1.5);
    outline(overlay.actionCell, ColorAction, border*1.5);

    // Cursor path between the two clicks
    if(overlay.cursorFromMs >= 0 && overlay.cursorToMs >= overlay.cursorFromMs)
    {
        QPolygonF path;
        for(const TrainerCursorSample& smp : rec.cursor)
        {
            if(smp.tMs < overlay.cursorFromMs || smp.tMs > overlay.cursorToMs)
                continue;
            path.append(QPointF(ox + smp.x*s, oy + smp.y*s));
        }
        if(path.size() >= 2)
        {
            p.setPen(QPen(ColorCursor, qMax(1.5, s*0.08), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(path);
            p.setBrush(ColorCursor);
            p.setPen(Qt::NoPen);
            p.drawEllipse(path.first(), s*0.12, s*0.12);

            // Arrow head at the end of the path
            const QPointF end = path.last();
            QPointF dir = end - path[path.size() - 2];
            for(int k=path.size() - 3; k >= 0 && std::hypot(dir.x(), dir.y()) < s*0.3; --k)
                dir = end - path[k];
            const double len = std::hypot(dir.x(), dir.y());
            if(len > 0.01)
            {
                dir /= len;
                const QPointF normal(-dir.y(), dir.x());
                const double a = s*0.35;
                QPolygonF head;
                head << end << end - dir*a + normal*a*0.5 << end - dir*a - normal*a*0.5;
                p.drawPolygon(head);
            }
        }
    }
}

// ===========================================================================
// TrainerAnalysisDialog
// ===========================================================================

TrainerAnalysisDialog::TrainerAnalysisDialog(const TrainerRecording& recording, QWidget* parent) :
    QDialog(parent),
    rec(recording),
    report(TrainerAnalyzer::analyze(recording))
{
    setWindowTitle(tr("Game Analysis - MineCoach"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    resize(1180, 720);

    const TrainerStats& st = report.stats;
    labelSummary = new QLabel(this);
    labelSummary->setTextFormat(Qt::RichText);
    labelSummary->setWordWrap(true);
    labelSummary->setText(
        tr("<b>%1</b> &nbsp; %2x%3, %4 mines &nbsp;|&nbsp; <b>%5 s</b> &nbsp;|&nbsp; "
           "3BV %6/%7 &nbsp;|&nbsp; 3BV/s <b>%8</b> &nbsp;|&nbsp; efficiency <b>%9%</b><br>"
           "Clicks: %10 left, %11 right, %12 chords &nbsp;|&nbsp; wasted %13 &nbsp;|&nbsp; "
           "savable with better chording %14<br>"
           "Cursor travelled %15 cells &nbsp;|&nbsp; %16 jumps, <b>%17 left a provable cell behind</b>"
           " &nbsp;|&nbsp; %18 unnecessary guesses")
        .arg(rec.won ? tr("Won") : tr("Lost"))
        .arg(rec.width).arg(rec.height).arg(rec.mines)
        .arg(QString::number(st.durationSec, 'f', 2))
        .arg(st.threeBVSolved).arg(st.threeBV)
        .arg(QString::number(st.threeBVPerSecond, 'f', 2))
        .arg(QString::number(st.efficiency, 'f', 0))
        .arg(st.leftClicks).arg(st.rightClicks).arg(st.chords)
        .arg(st.wastedClicks).arg(st.savableClicks)
        .arg(QString::number(st.cursorTravelCells, 'f', 0))
        .arg(st.jumps).arg(st.jumpsWithMissedPattern).arg(st.guesses));

    // ---- Left side: board, legend and replay controls ----------------------
    boardView = new TrainerBoardView(rec, this);

    QLabel* legend = new QLabel(this);
    legend->setTextFormat(Qt::RichText);
    legend->setText(swatch(ColorFrom, tr("previous click")) +
                    swatch(ColorAction, tr("this click")) +
                    swatch(ColorCursor, tr("cursor path")) +
                    swatch(ColorClue, tr("pattern numbers")) +
                    swatch(ColorSuggested, tr("better cell / chord")) +
                    swatch(ColorExtra, tr("affected cells")));

    sliderStep = new QSlider(Qt::Horizontal, this);
    sliderStep->setRange(0, rec.actions.size());
    QPushButton* buttonPrev = new QPushButton(QStringLiteral("<"), this);
    QPushButton* buttonNext = new QPushButton(QStringLiteral(">"), this);
    buttonPrev->setFixedWidth(32);
    buttonNext->setFixedWidth(32);
    labelStep = new QLabel(this);
    cbShowMines = new QCheckBox(tr("Show mines"), this);
    cbShowMines->setChecked(true);

    QHBoxLayout* stepRow = new QHBoxLayout();
    stepRow->addWidget(buttonPrev);
    stepRow->addWidget(sliderStep, 1);
    stepRow->addWidget(buttonNext);
    stepRow->addWidget(cbShowMines);

    QWidget* left = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(boardView, 1);
    leftLayout->addWidget(legend);
    leftLayout->addLayout(stepRow);
    leftLayout->addWidget(labelStep);

    // ---- Right side: findings -------------------------------------------
    comboFilter = new QComboBox(this);
    comboFilter->addItems({tr("All findings"), tr("Cursor jumps / missed patterns"),
                           tr("Chording"), tr("Guesses and flags"), tr("Major issues only")});
    listFindings = new QListWidget(this);
    listFindings->setWordWrap(true);
    listFindings->setAlternatingRowColors(true);
    textDetail = new QTextBrowser(this);

    QWidget* right = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(comboFilter);
    rightLayout->addWidget(listFindings, 3);
    rightLayout->addWidget(textDetail, 2);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    QPushButton* buttonClose = new QPushButton(tr("Close"), this);
    QHBoxLayout* bottom = new QHBoxLayout();
    bottom->addStretch();
    bottom->addWidget(buttonClose);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(labelSummary);
    mainLayout->addWidget(splitter, 1);
    mainLayout->addLayout(bottom);

    connect(buttonClose, &QPushButton::clicked, this, &QDialog::accept);
    connect(listFindings, &QListWidget::currentRowChanged, this, &TrainerAnalysisDialog::SLOT_findingSelected);
    connect(sliderStep, &QSlider::valueChanged, this, &TrainerAnalysisDialog::SLOT_stepChanged);
    connect(comboFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrainerAnalysisDialog::SLOT_filterChanged);
    connect(buttonPrev, &QPushButton::clicked, this, [this]{ sliderStep->setValue(sliderStep->value() - 1); });
    connect(buttonNext, &QPushButton::clicked, this, [this]{ sliderStep->setValue(sliderStep->value() + 1); });
    connect(cbShowMines, &QCheckBox::toggled, boardView, &TrainerBoardView::setShowMines);

    vFillFindings();
    if(!visibleFindings.isEmpty())
        listFindings->setCurrentRow(0);
    else
    {
        sliderStep->setValue(rec.actions.size());
        SLOT_stepChanged(rec.actions.size());
    }
}

bool TrainerAnalysisDialog::bFilterAccepts(const TrainerFinding& f) const
{
    using T = TrainerFinding::Type;
    switch(comboFilter->currentIndex())
    {
        case 1: return f.type == T::CursorJump;
        case 2: return f.type == T::RejectedChord || f.type == T::WastedChord ||
                       f.type == T::DominatedChord || f.type == T::ClicksCouldBeChord ||
                       f.type == T::LowValueChord;
        case 3: return f.type == T::UnnecessaryGuess || f.type == T::WrongFlag ||
                       f.type == T::WastedClick;
        case 4: return f.severity >= 2;
        default: return true;
    }
}

void TrainerAnalysisDialog::vFillFindings()
{
    listFindings->blockSignals(true);
    listFindings->clear();
    visibleFindings.clear();
    for(int i=0; i<report.findings.size(); ++i)
    {
        const TrainerFinding& f = report.findings[i];
        if(!bFilterAccepts(f))
            continue;
        QListWidgetItem* item = new QListWidgetItem(
            QStringLiteral("Move %1  [%2]  %3")
            .arg(f.actionIndex + 1).arg(TrainerFinding::typeName(f.type), f.title));
        item->setForeground(f.severity >= 3 ? QColor(200, 30, 30) :
                            f.severity == 2 ? QColor(200, 110, 0) : palette().color(QPalette::Text));
        listFindings->addItem(item);
        visibleFindings.append(i);
    }
    if(visibleFindings.isEmpty())
    {
        QListWidgetItem* item = new QListWidgetItem(report.findings.isEmpty() ?
            tr("No inefficiencies found - clean game!") : tr("Nothing in this category."));
        item->setFlags(Qt::NoItemFlags);
        listFindings->addItem(item);
    }
    listFindings->blockSignals(false);
}

QString TrainerAnalysisDialog::actionText(int index) const
{
    if(index >= rec.actions.size())
        return tr("End of game - %1").arg(rec.won ? tr("won") : tr("lost"));

    const TrainerAction& a = rec.actions[index];
    QString what;
    switch(a.type)
    {
        case TrainerAction::Type::Reveal: what = tr("reveal"); break;
        case TrainerAction::Type::Flag:   what = tr("flag");   break;
        case TrainerAction::Type::Chord:  what = tr("chord");  break;
    }
    const qint64 t0 = rec.actions.isEmpty() ? 0 : rec.actions.first().tMs;
    QString text = tr("Move %1/%2 at %3 s: %4 %5")
                   .arg(index + 1).arg(rec.actions.size())
                   .arg(QString::number((a.tMs - t0)/1000.0, 'f', 2))
                   .arg(what, TrainerAnalyzer::cellName(a.y*rec.width + a.x, rec.width));
    const int opened = report.openedPerAction.value(index);
    if(opened > 0)
        text += tr(" - opened %1 cell(s)").arg(opened);
    if(index > 0)
        text += tr(" - %1 ms since the previous move")
                .arg(a.tMs - rec.actions[index - 1].tMs);
    return text;
}

void TrainerAnalysisDialog::SLOT_findingSelected(int row)
{
    if(row < 0 || row >= visibleFindings.size())
        return;

    const TrainerFinding& f = report.findings[visibleFindings[row]];

    TrainerBoardView::Overlay o;
    o.fromCell = f.fromCell;
    o.actionCell = f.actionCell;
    o.suggestedCell = f.suggestedCell;
    o.clueCells = f.clueCells;
    o.extraCells = f.extraCells;
    o.cursorFromMs = f.cursorFromMs;
    o.cursorToMs = f.cursorToMs;
    if(o.cursorFromMs < 0 && f.actionIndex > 0)
    {
        o.cursorFromMs = rec.actions[f.actionIndex - 1].tMs;
        o.cursorToMs = rec.actions[f.actionIndex].tMs;
    }

    bSyncing = true;
    sliderStep->setValue(f.actionIndex);
    bSyncing = false;
    labelStep->setText(actionText(f.actionIndex));
    boardView->setState(report.snapshots.value(f.actionIndex), o);

    QString html = QStringLiteral("<h3>%1</h3><p>%2</p>")
                   .arg(f.title.toHtmlEscaped(), f.detail.toHtmlEscaped());
    if(f.wastedClicks > 0)
        html += tr("<p><b>Clicks lost:</b> %1</p>").arg(f.wastedClicks);
    if(f.type == TrainerFinding::Type::CursorJump)
    {
        double travel = 0;
        const TrainerCursorSample* prev = nullptr;
        for(const TrainerCursorSample& s : rec.cursor)
        {
            if(s.tMs < o.cursorFromMs || s.tMs > o.cursorToMs)
                continue;
            if(prev)
                travel += std::hypot(s.x - prev->x, s.y - prev->y);
            prev = &s;
        }
        html += tr("<p>The cursor travelled %1 cells in %2 ms between the two clicks. "
                   "Finishing the green cell first would have kept the cursor local.</p>")
                .arg(QString::number(travel, 'f', 1)).arg(o.cursorToMs - o.cursorFromMs);
    }
    html += tr("<p style='color:gray'>Coordinates are (column,row), counted from the top-left cell (1,1). "
               "The board shows the position just before this move.</p>");
    textDetail->setHtml(html);
}

void TrainerAnalysisDialog::SLOT_stepChanged(int step)
{
    if(bSyncing)
        return;

    TrainerBoardView::Overlay o;
    if(step < rec.actions.size())
    {
        const TrainerAction& a = rec.actions[step];
        o.actionCell = a.y*rec.width + a.x;
        if(step > 0)
        {
            const TrainerAction& prev = rec.actions[step - 1];
            o.fromCell = prev.y*rec.width + prev.x;
            o.cursorFromMs = prev.tMs;
            o.cursorToMs = a.tMs;
        }
    }
    labelStep->setText(actionText(step));
    boardView->setState(report.snapshots.value(step), o);

    // List the findings attached to this move
    QStringList notes;
    for(const TrainerFinding& f : report.findings)
    {
        if(f.actionIndex == step)
            notes.append(QStringLiteral("<li><b>%1</b>: %2</li>")
                         .arg(TrainerFinding::typeName(f.type).toHtmlEscaped(), f.title.toHtmlEscaped()));
    }
    textDetail->setHtml(notes.isEmpty() ?
                        tr("<p>No issues on this move.</p>") :
                        QStringLiteral("<ul>%1</ul>").arg(notes.join(QString())));
}

void TrainerAnalysisDialog::SLOT_filterChanged(int index)
{
    Q_UNUSED(index)
    vFillFindings();
    if(!visibleFindings.isEmpty())
        listFindings->setCurrentRow(0);
}
