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
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QSplitter>
#include <QTextBrowser>
#include <QToolTip>
#include <QUrl>
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
const QColor ColorFocus(0, 190, 230);
const QColor ColorMine(215, 50, 50);
const QColor ColorKnownMine(70, 70, 78);
const QColor ColorShared(250, 220, 120);
const QColor ColorProvableMine(235, 110, 20);      // unflagged mine the numbers already prove
const QColor ColorUnprovableMine(40, 40, 40, 110); // unflagged mine nothing proves yet

const double AxisMargin = 22;

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

void drawHiddenCell(QPainter& p, const QRectF& r)
{
    p.fillRect(r, QColor(160, 170, 182));
    p.setPen(QColor(205, 212, 220));
    p.drawLine(r.topLeft(), r.topRight());
    p.drawLine(r.topLeft(), r.bottomLeft());
    p.setPen(QColor(110, 118, 128));
    p.drawLine(r.bottomLeft(), r.bottomRight());
    p.drawLine(r.topRight(), r.bottomRight());
}

void drawMineDot(QPainter& p, const QRectF& r, const QColor& color, double scale)
{
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(r.center(), r.width()*scale, r.width()*scale);
    p.setBrush(Qt::NoBrush);
}

void drawCheck(QPainter& p, const QRectF& r, const QColor& color)
{
    QPainterPath check;
    check.moveTo(r.left() + r.width()*0.25, r.top() + r.height()*0.52);
    check.lineTo(r.left() + r.width()*0.43, r.top() + r.height()*0.70);
    check.lineTo(r.left() + r.width()*0.76, r.top() + r.height()*0.32);
    p.setPen(QPen(color, qMax(1.5, r.width()*0.11), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(check);
}

QRect boundingCells(const QVector<int>& cells, int width, int height, int pad)
{
    if(cells.isEmpty() || width <= 0)
        return QRect();
    int x0 = width, y0 = height, x1 = -1, y1 = -1;
    for(int c : cells)
    {
        x0 = qMin(x0, c % width);
        x1 = qMax(x1, c % width);
        y0 = qMin(y0, c / width);
        y1 = qMax(y1, c / width);
    }
    x0 = qMax(0, x0 - pad);
    y0 = qMax(0, y0 - pad);
    x1 = qMin(width - 1, x1 + pad);
    y1 = qMin(height - 1, y1 + pad);
    return QRect(QPoint(x0, y0), QPoint(x1, y1));
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
    setMouseTracking(true);
}

void TrainerBoardView::setState(const QByteArray& snapshot, const Overlay& o)
{
    state = snapshot;
    overlay = o;
    board.restore(snapshot);
    // Which hidden mines could the player already prove at this moment?
    provable = TrainerSolver::deduce(board);
    update();
}

void TrainerBoardView::setShowMines(bool show)
{
    bShowMines = show;
    update();
}

void TrainerBoardView::setCrop(const QRect& cells)
{
    crop = cells;
    update();
}

void TrainerBoardView::setSimplified(const TrainerDeduction& deduction)
{
    simplified = deduction;
    bSimplified = true;
    update();
}

void TrainerBoardView::setFocusCell(int cell)
{
    if(cell == focusCell)
        return;
    focusCell = cell;
    update();
}

QSize TrainerBoardView::sizeHint() const
{
    const QRect d = drawnCells();
    return QSize(int(AxisMargin) + qMax(1, d.width())*30, int(AxisMargin) + qMax(1, d.height())*30);
}

QRect TrainerBoardView::drawnCells() const
{
    const QRect full(0, 0, rec.width, rec.height);
    return crop.isNull() ? full : crop.intersected(full);
}

TrainerBoardView::Geometry TrainerBoardView::geometry() const
{
    const QRect d = drawnCells();
    Geometry g;
    if(d.width() <= 0 || d.height() <= 0)
        return g;
    g.s = qMax(4.0, qMin((width() - AxisMargin - 2)/d.width(), (height() - AxisMargin - 2)/d.height()));
    g.ox = AxisMargin + (width() - AxisMargin - g.s*d.width())/2;
    g.oy = AxisMargin + (height() - AxisMargin - g.s*d.height())/2;
    return g;
}

int TrainerBoardView::cellAt(const QPointF& pos) const
{
    const Geometry g = geometry();
    const QRect d = drawnCells();
    if(g.s <= 0)
        return -1;
    const int cx = int(std::floor((pos.x() - g.ox)/g.s)) + d.x();
    const int cy = int(std::floor((pos.y() - g.oy)/g.s)) + d.y();
    if(!d.contains(cx, cy))
        return -1;
    return cy*rec.width + cx;
}

void TrainerBoardView::mouseMoveEvent(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF pos = event->position();
    const QPoint global = event->globalPosition().toPoint();
#else
    const QPointF pos = event->localPos();
    const QPoint global = event->globalPos();
#endif
    const int cell = cellAt(pos);
    if(cell != hoverCell)
    {
        hoverCell = cell;
        Q_EMIT SIGNAL_cellHovered(cell);
        update();
    }
    if(cell < 0)
    {
        QToolTip::hideText();
        return;
    }

    const int x = cell % rec.width, y = cell / rec.width;
    QString what;
    const char c = cell < state.size() ? state[cell] : 'h';
    if(c == 'r')
        what = board.value[cell] > 0 ? tr("revealed %1").arg(board.value[cell]) : tr("revealed empty cell");
    else if(c == 'f')
        what = tr("flagged");
    else if(c == 'q')
        what = tr("question mark");
    else
        what = tr("hidden");
    QToolTip::showText(global, tr("(%1,%2)  column %1, row %2\n%3").arg(x + 1).arg(y + 1).arg(what), this);
}

void TrainerBoardView::mousePressEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const int cell = cellAt(event->position());
#else
    const int cell = cellAt(event->localPos());
#endif
    if(cell >= 0)
        Q_EMIT SIGNAL_cellClicked(cell);
}

void TrainerBoardView::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    if(hoverCell != -1)
    {
        hoverCell = -1;
        Q_EMIT SIGNAL_cellHovered(-1);
        update();
    }
}

void TrainerBoardView::paintSimplifiedCell(QPainter& p, const QRectF& r, int i) const
{
    const TrainerDeduction& d = simplified;
    const char c = i < state.size() ? state[i] : 'h';
    const double s = r.width();

    const int clueIndex = d.clues.indexOf(i);
    if(clueIndex >= 0)
    {
        // Number reduced to the mines it still needs
        p.fillRect(r, QColor(240, 240, 240));
        p.setPen(QColor(200, 200, 200));
        p.drawRect(r);
        const int remaining = d.clueRemaining.value(clueIndex, board.value[i]);
        const bool reduced = remaining != board.value[i];
        p.setPen(numberColor(remaining));
        // Leave room at the top for the "was N" note when the number was reduced
        p.drawText(reduced ? r.adjusted(0, s*0.25, 0, 0) : r, Qt::AlignCenter, QString::number(remaining));
        if(reduced)
        {
            QFont small = p.font();
            small.setPixelSize(qMax(6, int(s*0.22)));
            small.setBold(false);
            p.save();
            p.setFont(small);
            p.setPen(QColor(120, 120, 120));
            p.drawText(r.adjusted(0, s*0.03, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                       QStringLiteral("was %1").arg(board.value[i]));
            p.restore();
        }
        return;
    }

    if(d.group.contains(i))
    {
        if(d.mine)
        {
            p.fillRect(r, ColorMine.lighter(150));
            drawMineDot(p, r, QColor(40, 20, 20), 0.2);
        }
        else
        {
            p.fillRect(r, ColorSuggested.lighter(160));
            drawCheck(p, r, QColor(20, 110, 40));
        }
        p.setPen(QPen(d.mine ? ColorMine : ColorSuggested, qMax(2.0, s*0.08)));
        p.drawRect(r.adjusted(1, 1, -1, -1));
        return;
    }

    if(d.knownMines.contains(i))
    {
        p.fillRect(r, ColorKnownMine);
        drawMineDot(p, r, QColor(230, 230, 230), 0.16);
        return;
    }

    if(d.region.contains(i))
    {
        drawHiddenCell(p, r);
        if(d.shared.contains(i))
        {
            QColor tint = ColorShared;
            tint.setAlpha(170);
            p.fillRect(r.adjusted(1, 1, -1, -1), tint);
        }
        return;
    }

    // Not part of the reasoning: faded
    p.fillRect(r, c == 'r' ? QColor(248, 248, 248) : QColor(214, 218, 223));
    p.setPen(QColor(232, 232, 232));
    p.drawRect(r);
}

void TrainerBoardView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    if(rec.width <= 0 || rec.height <= 0)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const Geometry g = geometry();
    const QRect d = drawnCells();
    const double s = g.s;
    if(s <= 0)
        return;
    auto cellRect = [&](int i)
    {
        return QRectF(g.ox + (i % rec.width - d.x())*s, g.oy + (i / rec.width - d.y())*s, s, s);
    };
    auto drawn = [&](int i) { return i >= 0 && d.contains(i % rec.width, i / rec.width); };

    // ---- Axis labels: column numbers on top, row numbers on the left -------
    const int focusX = focusCell >= 0 ? focusCell % rec.width : -1;
    const int focusY = focusCell >= 0 ? focusCell / rec.width : -1;
    const int hoverX = hoverCell >= 0 ? hoverCell % rec.width : -1;
    const int hoverY = hoverCell >= 0 ? hoverCell / rec.width : -1;
    QFont axisFont = p.font();
    axisFont.setPixelSize(qBound(8, int(qMin(s*0.45, AxisMargin*0.55)), 12));
    const int every = s >= 16 ? 1 : 5;
    for(int x = d.left(); x <= d.right(); ++x)
    {
        const bool emphasise = x == focusX || x == hoverX;
        if(!emphasise && (x + 1) % every != 0 && x != d.left())
            continue;
        axisFont.setBold(emphasise);
        p.setFont(axisFont);
        p.setPen(x == focusX ? ColorFocus : emphasise ? palette().color(QPalette::Highlight)
                                                      : palette().color(QPalette::WindowText));
        p.drawText(QRectF(g.ox + (x - d.x())*s - 4, g.oy - AxisMargin, s + 8, AxisMargin - 2),
                   Qt::AlignHCenter | Qt::AlignBottom, QString::number(x + 1));
    }
    for(int y = d.top(); y <= d.bottom(); ++y)
    {
        const bool emphasise = y == focusY || y == hoverY;
        if(!emphasise && (y + 1) % every != 0 && y != d.top())
            continue;
        axisFont.setBold(emphasise);
        p.setFont(axisFont);
        p.setPen(y == focusY ? ColorFocus : emphasise ? palette().color(QPalette::Highlight)
                                                      : palette().color(QPalette::WindowText));
        p.drawText(QRectF(g.ox - AxisMargin, g.oy + (y - d.y())*s, AxisMargin - 4, s),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(y + 1));
    }

    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(qMax(6, int(s*0.6)));
    p.setFont(font);

    // ---- Cells --------------------------------------------------------------
    for(int y = d.top(); y <= d.bottom(); ++y)
    {
        for(int x = d.left(); x <= d.right(); ++x)
        {
            const int i = y*rec.width + x;
            const QRectF r = cellRect(i);

            if(bSimplified)
            {
                paintSimplifiedCell(p, r, i);
                continue;
            }

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

            drawHiddenCell(p, r);

            if(bShowMines && mine && c != 'f')
            {
                const bool canProve = provable.contains(i) && provable.value(i).mine;
                drawMineDot(p, r, canProve ? ColorProvableMine : ColorUnprovableMine, canProve ? 0.22 : 0.16);
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
    }

    // ---- Overlays ---------------------------------------------------------
    const double border = qMax(2.0, s*0.11);
    auto outline = [&](int cell, const QColor& color, double inset, Qt::PenStyle style = Qt::SolidLine)
    {
        if(!drawn(cell))
            return;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(color, border, style));
        p.drawRect(cellRect(cell).adjusted(inset, inset, -inset, -inset));
    };

    if(!bSimplified)
    {
        for(int c : overlay.extraCells)
        {
            if(!drawn(c))
                continue;
            QColor fill = ColorExtra;
            fill.setAlpha(80);
            p.fillRect(cellRect(c), fill);
        }
        for(int c : overlay.groupCells)
        {
            if(!drawn(c))
                continue;
            QColor fill = overlay.groupIsMine ? ColorMine : ColorSuggested;
            fill.setAlpha(90);
            p.fillRect(cellRect(c), fill);
            outline(c, overlay.groupIsMine ? ColorMine : ColorSuggested, border/2);
        }
        if(drawn(overlay.suggestedCell))
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
    }

    // Hovered cell and cell focused from a link in the text
    if(drawn(hoverCell))
    {
        p.setPen(QPen(palette().color(QPalette::Highlight), 1.5));
        p.drawRect(cellRect(hoverCell).adjusted(1, 1, -1, -1));
    }
    outline(focusCell, ColorFocus, -border/2, Qt::DashLine);

    // Cursor path between the two clicks
    if(!bSimplified && overlay.cursorFromMs >= 0 && overlay.cursorToMs >= overlay.cursorFromMs)
    {
        QPolygonF path;
        for(const TrainerCursorSample& smp : rec.cursor)
        {
            if(smp.tMs < overlay.cursorFromMs || smp.tMs > overlay.cursorToMs)
                continue;
            path.append(QPointF(g.ox + (smp.x - d.x())*s, g.oy + (smp.y - d.y())*s));
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
// TrainerPatternDialog
// ===========================================================================

TrainerPatternDialog::TrainerPatternDialog(const TrainerRecording& recording, const QByteArray& snapshot,
                                           const TrainerDeduction& d,
                                           const QVector<TrainerDeduction>& prerequisites, QWidget* parent) :
    QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Pattern explained - MineCoach"));
    resize(980, 640);

    TrainerBoard board(recording);
    board.restore(snapshot);

    QString name;
    switch(d.kind)
    {
        case TrainerDeduction::Kind::SatisfiedNumber: name = tr("Satisfied number"); break;
        case TrainerDeduction::Kind::FilledNumber:    name = tr("Filled number"); break;
        case TrainerDeduction::Kind::Pattern:         name = tr("%1 pattern").arg(d.pattern); break;
        case TrainerDeduction::Kind::Enumeration:     name = tr("Advanced deduction"); break;
    }
    QStringList proven;
    for(int c : d.group)
        proven.append(TrainerAnalyzer::cellName(c, recording.width));
    const QString conclusion = tr("%1 %2 %3").arg(proven.join(", "),
                                                  d.group.size() == 1 ? tr("is") : tr("are"),
                                                  d.mine ? (d.group.size() == 1 ? tr("a mine") : tr("mines"))
                                                         : tr("safe"));

    QLabel* title = new QLabel(this);
    title->setTextFormat(Qt::RichText);
    title->setText(QStringLiteral("<h2 style='margin:0'>%1</h2><p style='margin-top:4px'>%2</p>")
                   .arg(name.toHtmlEscaped(), conclusion.toHtmlEscaped()));

    // Area that contains everything the reasoning touches, plus one cell around it
    QVector<int> involved = d.clues + d.region + d.group + d.knownMines;
    involved.append(d.cell);
    const QRect area = boundingCells(involved, recording.width, recording.height, 1);

    TrainerBoardView* exact = new TrainerBoardView(recording, this);
    TrainerBoardView::Overlay o;
    o.clueCells = d.clues;
    o.groupCells = d.group;
    o.groupIsMine = d.mine;
    o.extraCells = d.shared;
    exact->setState(snapshot, o);
    exact->setShowMines(false);
    exact->setCrop(area);

    TrainerBoardView* simple = new TrainerBoardView(recording, this);
    simple->setState(snapshot, TrainerBoardView::Overlay());
    simple->setCrop(area);
    simple->setSimplified(d);

    auto captioned = [this](QWidget* view, const QString& caption, const QString& legend)
    {
        QWidget* box = new QWidget(this);
        QVBoxLayout* l = new QVBoxLayout(box);
        l->setContentsMargins(0, 0, 0, 0);
        QLabel* c = new QLabel(caption, box);
        QFont f = c->font();
        f.setBold(true);
        c->setFont(f);
        l->addWidget(c);
        l->addWidget(view, 1);
        QLabel* lg = new QLabel(box);
        lg->setTextFormat(Qt::RichText);
        lg->setWordWrap(true);
        lg->setText(legend);
        l->addWidget(lg);
        return box;
    };

    const QString exactLegend = swatch(ColorClue, tr("numbers used")) +
                                (d.shared.isEmpty() ? QString() : swatch(ColorExtra, tr("shared cells"))) +
                                swatch(d.mine ? ColorMine : ColorSuggested,
                                       d.mine ? tr("proven mine") : tr("proven safe"));
    const QString simpleLegend = tr("Numbers show the mines they <b>still need</b>. ") +
                                 (d.knownMines.isEmpty() ? QString() : swatch(ColorKnownMine, tr("already a mine"))) +
                                 (d.shared.isEmpty() ? QString() : swatch(ColorShared, tr("shared cells"))) +
                                 swatch(d.mine ? ColorMine : ColorSuggested,
                                        d.mine ? tr("proven mine") : tr("proven safe")) +
                                 swatch(QColor(214, 218, 223), tr("not involved"));

    QHBoxLayout* views = new QHBoxLayout();
    views->addWidget(captioned(exact, tr("On the board (what you saw)"), exactLegend), 1);
    views->addWidget(captioned(simple, tr("Simplified (only what matters)"), simpleLegend), 1);

    QTextBrowser* steps = new QTextBrowser(this);
    steps->setOpenLinks(false);
    QString html = tr("<p><b>Why:</b></p><ol>");
    for(const QString& step : TrainerSolver::explainSteps(board, d, prerequisites))
        html += QStringLiteral("<li style='margin-bottom:4px'>%1</li>")
                .arg(TrainerAnalysisDialog::linkifyCells(step.toHtmlEscaped()));
    html += tr("</ol><p style='color:gray'>Hover a coordinate to highlight that cell. "
               "Coordinates are (column,row); the numbers along the board edges are the columns and rows.</p>");
    steps->setHtml(html);

    const int width = recording.width;
    auto focus = [exact, simple, width](const QUrl& url)
    {
        const int cell = TrainerAnalysisDialog::cellFromLink(url, width);
        exact->setFocusCell(cell);
        simple->setFocusCell(cell);
    };
    connect(steps, &QTextBrowser::highlighted, this, focus);
    connect(steps, &QTextBrowser::anchorClicked, this, focus);
    connect(exact, &TrainerBoardView::SIGNAL_cellHovered, simple, &TrainerBoardView::setFocusCell);
    connect(simple, &TrainerBoardView::SIGNAL_cellHovered, exact, &TrainerBoardView::setFocusCell);

    QPushButton* close = new QPushButton(tr("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    QHBoxLayout* bottom = new QHBoxLayout();
    bottom->addStretch();
    bottom->addWidget(close);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addLayout(views, 3);
    layout->addWidget(steps, 2);
    layout->addLayout(bottom);
}

// ===========================================================================
// TrainerAnalysisDialog
// ===========================================================================

QString TrainerAnalysisDialog::linkifyCells(const QString& html)
{
    static const QRegularExpression cellRef(QStringLiteral("\\((\\d+),(\\d+)\\)"));
    QString out = html;
    out.replace(cellRef, QStringLiteral("<a href=\"cell:\\1,\\2\" style=\"text-decoration:none; color:%1;\">"
                                        "<b>(\\1,\\2)</b></a>").arg(ColorFocus.darker(130).name()));
    return out;
}

int TrainerAnalysisDialog::cellFromLink(const QUrl& url, int width)
{
    if(url.scheme() != QLatin1String("cell"))
        return -1;
    const QStringList parts = url.path().split(QLatin1Char(','));
    if(parts.size() != 2)
        return -1;
    bool okX = false, okY = false;
    const int x = parts[0].toInt(&okX) - 1;
    const int y = parts[1].toInt(&okY) - 1;
    if(!okX || !okY || x < 0 || y < 0 || x >= width)
        return -1;
    return y*width + x;
}

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
    legend->setWordWrap(true);
    legend->setText(swatch(ColorFrom, tr("previous click")) +
                    swatch(ColorAction, tr("this click")) +
                    swatch(ColorCursor, tr("cursor path")) +
                    swatch(ColorClue, tr("pattern numbers")) +
                    swatch(ColorSuggested, tr("better cell / chord")) +
                    swatch(ColorExtra, tr("affected cells")) +
                    swatch(ColorFocus, tr("cell under the mouse in the text")) + QStringLiteral("<br>") +
                    tr("Mines (with Show mines): ") +
                    swatch(ColorProvableMine, tr("provable from the open numbers")) +
                    swatch(QColor(110, 110, 110), tr("not provable yet")));

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
    textDetail->setOpenLinks(false);
    buttonReadMore = new QPushButton(tr("Read more: show the pattern step by step"), this);
    buttonReadMore->setEnabled(false);

    QWidget* right = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(comboFilter);
    rightLayout->addWidget(listFindings, 3);
    rightLayout->addWidget(textDetail, 2);
    rightLayout->addWidget(buttonReadMore);

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
    connect(buttonReadMore, &QPushButton::clicked, this, &TrainerAnalysisDialog::SLOT_readMore);

    // Hovering a coordinate in the text highlights the cell on the board
    auto focus = [this](const QUrl& url) { boardView->setFocusCell(cellFromLink(url, rec.width)); };
    connect(textDetail, &QTextBrowser::highlighted, this, focus);
    connect(textDetail, &QTextBrowser::anchorClicked, this, focus);

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

    currentFinding = visibleFindings[row];
    const TrainerFinding& f = report.findings[currentFinding];

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
    boardView->setFocusCell(-1);
    boardView->setState(report.snapshots.value(f.actionIndex), o);

    QString html = QStringLiteral("<h3>%1</h3><p>%2</p>")
                   .arg(linkifyCells(f.title.toHtmlEscaped()), linkifyCells(f.detail.toHtmlEscaped()));
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
    html += tr("<p style='color:gray'>Hover a coordinate to highlight that cell on the board. "
               "Coordinates are (column,row): the numbers above the board are columns, the ones on the left are rows. "
               "The board shows the position just before this move.</p>");
    textDetail->setHtml(html);

    buttonReadMore->setEnabled(f.hasDeduction);
    buttonReadMore->setToolTip(f.hasDeduction ? QString() :
                               tr("This finding is about clicks, not about a pattern."));
}

void TrainerAnalysisDialog::SLOT_stepChanged(int step)
{
    if(bSyncing)
        return;

    currentFinding = -1;
    buttonReadMore->setEnabled(false);

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
    boardView->setFocusCell(-1);
    boardView->setState(report.snapshots.value(step), o);

    // List the findings attached to this move
    QStringList notes;
    for(const TrainerFinding& f : report.findings)
    {
        if(f.actionIndex == step)
            notes.append(QStringLiteral("<li><b>%1</b>: %2</li>")
                         .arg(TrainerFinding::typeName(f.type).toHtmlEscaped(),
                              linkifyCells(f.title.toHtmlEscaped())));
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

void TrainerAnalysisDialog::SLOT_readMore()
{
    if(currentFinding < 0 || currentFinding >= report.findings.size())
        return;
    const TrainerFinding& f = report.findings[currentFinding];
    if(!f.hasDeduction)
        return;

    TrainerPatternDialog* dialog = new TrainerPatternDialog(rec, report.snapshots.value(f.actionIndex),
                                                            f.deduction, f.prerequisites, this);
    dialog->show();
}
