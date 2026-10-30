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

#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <climits>
#include <cmath>
#include <functional>

#include "traineranalyzer.h"

// ===========================================================================
// TrainerBoard
// ===========================================================================

TrainerBoard::TrainerBoard(const TrainerRecording& recording) :
    width(recording.width),
    height(recording.height),
    useQuestionMark(recording.useQuestionMark),
    value(recording.width*recording.height, 0),
    revealed(recording.width*recording.height, false),
    marks(recording.width*recording.height, Mark::None)
{
    for(int i=0; i<value.size(); ++i)
    {
        if(recording.mineLayout.value(i, false))
        {
            value[i] = -1;
            continue;
        }
        qint8 n = 0;
        for(int nb : neighbours(i))
        {
            if(recording.mineLayout.value(nb, false))
                ++n;
        }
        value[i] = n;
    }
}

QVector<int> TrainerBoard::neighbours(int i) const
{
    QVector<int> result;
    result.reserve(8);
    const int x = xOf(i);
    const int y = yOf(i);
    // Same visiting order as LibreMinesGameEngine (x outer, y inner)
    for(int nx = x-1; nx <= x+1; ++nx)
    {
        for(int ny = y-1; ny <= y+1; ++ny)
        {
            if((nx == x && ny == y) || !inside(nx, ny))
                continue;
            result.append(index(nx, ny));
        }
    }
    return result;
}

TrainerBoard::Result TrainerBoard::apply(const TrainerAction& action)
{
    if(!inside(action.x, action.y))
    {
        Result r;
        r.noEffect = true;
        return r;
    }

    const int i = index(action.x, action.y);
    switch(action.type)
    {
        case TrainerAction::Type::Reveal: return reveal(i);
        case TrainerAction::Type::Flag:   return toggleMark(i);
        case TrainerAction::Type::Chord:  return chord(i);
    }
    return Result();
}

TrainerBoard::Result TrainerBoard::reveal(int i)
{
    Result r;
    if(exploded || revealed[i] || marks[i] != Mark::None)
    {
        r.noEffect = true;
        return r;
    }

    if(isMine(i))
    {
        exploded = true;
        r.exploded = true;
        return r;
    }

    QVector<int> stack{i};
    revealed[i] = true;
    r.opened.append(i);
    while(!stack.isEmpty())
    {
        const int c = stack.takeLast();
        if(value[c] != 0)
            continue;
        for(int nb : neighbours(c))
        {
            if(!revealed[nb] && marks[nb] == Mark::None && !isMine(nb))
            {
                revealed[nb] = true;
                r.opened.append(nb);
                stack.append(nb);
            }
        }
    }
    return r;
}

TrainerBoard::Result TrainerBoard::chord(int i)
{
    Result r;
    if(exploded || !revealed[i] || value[i] <= 0)
    {
        r.noEffect = true;
        return r;
    }

    if(flagsAround(i) != value[i])
    {
        r.rejected = true;
        r.noEffect = true;
        return r;
    }

    for(int nb : neighbours(i))
    {
        if(revealed[nb] || marks[nb] != Mark::None)
            continue;

        Result sub = reveal(nb);
        r.opened += sub.opened;
        if(sub.exploded)
        {
            r.exploded = true;
            break;
        }
    }
    r.noEffect = r.opened.isEmpty() && !r.exploded;
    return r;
}

TrainerBoard::Result TrainerBoard::toggleMark(int i)
{
    Result r;
    if(exploded || revealed[i])
    {
        r.noEffect = true;
        return r;
    }

    switch(marks[i])
    {
        case Mark::None:
            marks[i] = Mark::Flag;
            break;
        case Mark::Flag:
            marks[i] = useQuestionMark ? Mark::Question : Mark::None;
            break;
        case Mark::Question:
            marks[i] = Mark::None;
            break;
    }
    return r;
}

QVector<int> TrainerBoard::chordPreview(int i, bool* wouldExplode) const
{
    TrainerBoard copy = *this;
    const Result r = copy.chord(i);
    if(wouldExplode)
        *wouldExplode = r.exploded;
    return r.opened;
}

int TrainerBoard::flagsAround(int i) const
{
    int n = 0;
    for(int nb : neighbours(i))
    {
        if(marks[nb] == Mark::Flag)
            ++n;
    }
    return n;
}

int TrainerBoard::wrongFlagsAround(int i) const
{
    int n = 0;
    for(int nb : neighbours(i))
    {
        if(marks[nb] == Mark::Flag && !isMine(nb))
            ++n;
    }
    return n;
}

int TrainerBoard::unflaggedMinesAround(int i) const
{
    int n = 0;
    for(int nb : neighbours(i))
    {
        if(isMine(nb) && marks[nb] != Mark::Flag)
            ++n;
    }
    return n;
}

int TrainerBoard::revealedCount() const
{
    return static_cast<int>(std::count(revealed.begin(), revealed.end(), true));
}

bool TrainerBoard::isWon() const
{
    for(int i=0; i<value.size(); ++i)
    {
        if(!isMine(i) && !revealed[i])
            return false;
    }
    return true;
}

QByteArray TrainerBoard::snapshot() const
{
    QByteArray s(value.size(), 'h');
    for(int i=0; i<value.size(); ++i)
    {
        if(revealed[i])
            s[i] = 'r';
        else if(marks[i] == Mark::Flag)
            s[i] = 'f';
        else if(marks[i] == Mark::Question)
            s[i] = 'q';
    }
    return s;
}

void TrainerBoard::restore(const QByteArray& snap)
{
    exploded = false;
    for(int i=0; i<value.size() && i<snap.size(); ++i)
    {
        revealed[i] = snap[i] == 'r';
        marks[i] = snap[i] == 'f' ? Mark::Flag :
                   snap[i] == 'q' ? Mark::Question : Mark::None;
    }
}

int TrainerBoard::threeBV() const
{
    QVector<bool> marked(value.size(), false);
    int bv = 0;
    for(int i=0; i<value.size(); ++i)
    {
        if(value[i] != 0 || marked[i])
            continue;
        ++bv;
        QVector<int> stack{i};
        marked[i] = true;
        while(!stack.isEmpty())
        {
            const int c = stack.takeLast();
            for(int nb : neighbours(c))
            {
                if(marked[nb] || isMine(nb))
                    continue;
                marked[nb] = true;
                if(value[nb] == 0)
                    stack.append(nb);
            }
        }
    }
    for(int i=0; i<value.size(); ++i)
    {
        if(!marked[i] && !isMine(i))
            ++bv;
    }
    return bv;
}

int TrainerBoard::threeBVSolved() const
{
    QVector<bool> marked(value.size(), false);
    int bv = 0;
    for(int i=0; i<value.size(); ++i)
    {
        if(value[i] != 0 || marked[i])
            continue;
        // Walk the whole opening; it counts if any of its zeros was revealed
        bool opened = false;
        QVector<int> stack{i};
        marked[i] = true;
        while(!stack.isEmpty())
        {
            const int c = stack.takeLast();
            if(value[c] == 0 && revealed[c])
                opened = true;
            for(int nb : neighbours(c))
            {
                if(marked[nb] || isMine(nb))
                    continue;
                marked[nb] = true;
                if(value[nb] == 0)
                    stack.append(nb);
            }
        }
        if(opened)
            ++bv;
    }
    for(int i=0; i<value.size(); ++i)
    {
        if(!marked[i] && !isMine(i) && revealed[i])
            ++bv;
    }
    return bv;
}

// ===========================================================================
// TrainerSolver
// ===========================================================================

namespace
{
struct Constraint
{
    int origin = -1;       // revealed number cell
    QVector<int> cells;    // all hidden neighbours
    int mines = 0;         // number on the cell
};

int chebyshev(const TrainerBoard& b, int p, int q)
{
    return qMax(qAbs(b.xOf(p) - b.xOf(q)), qAbs(b.yOf(p) - b.yOf(q)));
}

double euclid(const TrainerBoard& b, int p, int q)
{
    const double dx = b.xOf(p) - b.xOf(q);
    const double dy = b.yOf(p) - b.yOf(q);
    return std::sqrt(dx*dx + dy*dy);
}

class SolverState
{
public:
    explicit SolverState(const TrainerBoard& b, int center = -1, int radius = 0) : board(b)
    {
        for(int i=0; i<b.value.size(); ++i)
        {
            if(!b.revealed[i] || b.value[i] <= 0)
                continue;
            // Only numbers inside the window take part
            if(center >= 0 && chebyshev(b, i, center) > radius)
                continue;
            Constraint c;
            c.origin = i;
            c.mines = b.value[i];
            for(int nb : b.neighbours(i))
            {
                if(!b.revealed[nb])
                    c.cells.append(nb);
            }
            if(!c.cells.isEmpty())
            {
                byOrigin.insert(i, constraints.size());
                constraints.append(c);
            }
        }
    }

    // Unknown cells of constraint k and the mines still missing among them
    void reduce(int k, QVector<int>& unknown, int& remaining) const
    {
        const Constraint& c = constraints[k];
        unknown.clear();
        remaining = c.mines;
        for(int cell : c.cells)
        {
            auto it = known.constFind(cell);
            if(it == known.constEnd())
                unknown.append(cell);
            else if(it->mine)
                --remaining;
        }
    }

    int remainingAt(int origin) const
    {
        auto it = byOrigin.constFind(origin);
        if(it == byOrigin.constEnd())
            return -1;
        QVector<int> unknown;
        int remaining = 0;
        reduce(*it, unknown, remaining);
        return remaining;
    }

    // Hidden cells of constraint k that are already proven (mine or safe)
    QVector<int> knownIn(int k) const
    {
        QVector<int> cells;
        for(int cell : constraints[k].cells)
        {
            if(known.contains(cell))
                cells.append(cell);
        }
        return cells;
    }

    // Move this round's deductions into the known set
    void commit(int depth)
    {
        for(auto it = pending.begin(); it != pending.end(); ++it)
        {
            it->depth = depth;
            known.insert(it.key(), it.value());
        }
        pending.clear();
    }

    // Hidden cells of constraint k that are already known to be mines
    QVector<int> knownMinesOf(int k) const
    {
        QVector<int> mines;
        for(int cell : constraints[k].cells)
        {
            auto it = known.constFind(cell);
            if(it != known.constEnd() && it->mine)
                mines.append(cell);
        }
        return mines;
    }

    // `details` carries the explanation data (group, region, shared, ...)
    bool add(int cell, bool mine, TrainerDeduction::Kind kind, const QVector<int>& clues,
             const QVector<int>& clueRemaining, const QString& pattern = QString(),
             const TrainerDeduction& details = TrainerDeduction())
    {
        if(known.contains(cell) || pending.contains(cell))
            return false;
        TrainerDeduction d = details;
        d.cell = cell;
        d.mine = mine;
        d.kind = kind;
        d.clues = clues;
        d.clueRemaining = clueRemaining;
        d.pattern = pattern;
        if(d.group.isEmpty())
            d.group = {cell};
        pending.insert(cell, d);
        return true;
    }

    // Name 1-2-1 / 1-2-2-1 lines that contain the two compared numbers
    QString lineName(int a, int b, const QString& fallback) const
    {
        const int dx = board.xOf(b) - board.xOf(a);
        const int dy = board.yOf(b) - board.yOf(a);
        if(qAbs(dx) + qAbs(dy) != 1)
            return fallback;

        // Remaining values along the line, k = 0 is a, k = 1 is b
        QVector<int> ks;
        QVector<int> vals;
        for(int k=-3; k<=4; ++k)
        {
            const int x = board.xOf(a) + k*dx;
            const int y = board.yOf(a) + k*dy;
            int v = -1;
            if(board.inside(x, y))
                v = remainingAt(board.index(x, y));
            ks.append(k);
            vals.append(v);
        }

        const QVector<QVector<int>> shapes = {{1, 2, 2, 1}, {1, 2, 1}};
        const QStringList names = {QStringLiteral("1-2-2-1"), QStringLiteral("1-2-1")};
        for(int s=0; s<shapes.size(); ++s)
        {
            const QVector<int>& shape = shapes[s];
            for(int start=0; start + shape.size() <= vals.size(); ++start)
            {
                const int kStart = ks[start];
                const int kEnd = ks[start + shape.size() - 1];
                if(kStart > 0 || kEnd < 1)
                    continue; // window must contain both a and b
                bool match = true;
                for(int j=0; j<shape.size() && match; ++j)
                    match = vals[start + j] == shape[j];
                if(match)
                    return names[s];
            }
        }
        return fallback;
    }

    const TrainerBoard& board;
    QVector<Constraint> constraints;
    QHash<int, int> byOrigin;
    QHash<int, TrainerDeduction> known;    ///< Proven in earlier rounds
    QHash<int, TrainerDeduction> pending;  ///< Proven in the current round
};

QVector<int> subtract(const QVector<int>& a, const QVector<int>& b)
{
    QVector<int> r;
    for(int x : a)
    {
        if(!b.contains(x))
            r.append(x);
    }
    return r;
}

QVector<int> intersect(const QVector<int>& a, const QVector<int>& b)
{
    QVector<int> r;
    for(int x : a)
    {
        if(b.contains(x))
            r.append(x);
    }
    return r;
}

bool singlesPass(SolverState& s)
{
    bool changed = false;
    QVector<int> unknown;
    int remaining = 0;
    for(int k=0; k<s.constraints.size(); ++k)
    {
        s.reduce(k, unknown, remaining);
        if(unknown.isEmpty())
            continue;
        const int origin = s.constraints[k].origin;
        TrainerDeduction details;
        details.group = unknown;
        details.region = unknown;
        details.knownMines = s.knownMinesOf(k);
        if(remaining == 0)
            details.support = details.knownMines;
        else
            details.support = s.knownIn(k);
        if(remaining == 0)
        {
            for(int cell : unknown)
                changed |= s.add(cell, false, TrainerDeduction::Kind::SatisfiedNumber,
                                 {origin}, {remaining}, QString(), details);
        }
        else if(remaining == unknown.size())
        {
            for(int cell : unknown)
                changed |= s.add(cell, true, TrainerDeduction::Kind::FilledNumber,
                                 {origin}, {remaining}, QString(), details);
        }
    }
    return changed;
}

bool pairsPass(SolverState& s)
{
    struct Pending { int cell; bool mine; int helper; int target; int rh; int rt;
                     TrainerDeduction details; };
    QVector<Pending> pending;

    QVector<int> ua, ub;
    int ra = 0, rb = 0;
    for(int a=0; a<s.constraints.size(); ++a)
    {
        s.reduce(a, ua, ra);
        if(ua.isEmpty())
            continue;
        for(int b=a+1; b<s.constraints.size(); ++b)
        {
            if(chebyshev(s.board, s.constraints[a].origin, s.constraints[b].origin) > 2)
                continue;
            s.reduce(b, ub, rb);
            if(ub.isEmpty())
                continue;

            const QVector<int> shared = intersect(ua, ub);
            if(shared.isEmpty())
                continue;
            const QVector<int> aOnly = subtract(ua, shared);
            const QVector<int> bOnly = subtract(ub, shared);

            const int maxShared = qMin(qMin(ra, rb), static_cast<int>(shared.size()));
            const int minShared = qMax(0, qMax(ra - static_cast<int>(aOnly.size()),
                                               rb - static_cast<int>(bOnly.size())));

            QVector<int> support = s.knownIn(a);
            for(int c : s.knownIn(b))
            {
                if(!support.contains(c))
                    support.append(c);
            }

            QVector<int> knownMines = s.knownMinesOf(a);
            for(int m : s.knownMinesOf(b))
            {
                if(!knownMines.contains(m))
                    knownMines.append(m);
            }

            // side: cells only next to "target"; "helper" is the other number
            auto conclude = [&](const QVector<int>& side, int rTarget, int rHelper,
                                int target, int helper)
            {
                if(side.isEmpty())
                    return;
                TrainerDeduction details;
                details.group = side;
                details.region = ua + bOnly;
                details.shared = shared;
                details.knownMines = knownMines;
                details.support = support;
                const int minSide = rTarget - maxShared;
                const int maxSide = rTarget - minShared;
                if(minSide == side.size())
                {
                    for(int cell : side)
                        pending.append({cell, true, helper, target, rHelper, rTarget, details});
                }
                else if(maxSide == 0)
                {
                    for(int cell : side)
                        pending.append({cell, false, helper, target, rHelper, rTarget, details});
                }
            };

            const int oa = s.constraints[a].origin;
            const int ob = s.constraints[b].origin;
            conclude(bOnly, rb, ra, ob, oa);
            conclude(aOnly, ra, rb, oa, ob);
        }
    }

    bool changed = false;
    for(const Pending& p : pending)
    {
        const QString base = QStringLiteral("%1-%2").arg(p.rh).arg(p.rt);
        const QString name = s.lineName(p.helper, p.target, base);
        changed |= s.add(p.cell, p.mine, TrainerDeduction::Kind::Pattern,
                         {p.helper, p.target}, {p.rh, p.rt}, name, p.details);
    }
    return changed;
}

void enumerationPass(SolverState& s)
{
    const int MaxComponentCells = 24;
    const quint64 NodeBudget = 300000;

    struct Reduced { int origin; QVector<int> cells; int remaining; };
    QVector<Reduced> reduced;
    QHash<int, QVector<int>> cellToReduced;
    QVector<int> unknown;
    int remaining = 0;
    for(int k=0; k<s.constraints.size(); ++k)
    {
        s.reduce(k, unknown, remaining);
        if(unknown.isEmpty())
            continue;
        for(int cell : unknown)
            cellToReduced[cell].append(reduced.size());
        reduced.append({s.constraints[k].origin, unknown, remaining});
    }

    // Connected components of frontier cells linked through constraints
    QSet<int> visited;
    for(auto it = cellToReduced.constBegin(); it != cellToReduced.constEnd(); ++it)
    {
        if(visited.contains(it.key()))
            continue;

        QVector<int> cells;
        QSet<int> compConstraints;
        QVector<int> stack{it.key()};
        visited.insert(it.key());
        while(!stack.isEmpty())
        {
            const int c = stack.takeLast();
            cells.append(c);
            for(int r : cellToReduced.value(c))
            {
                compConstraints.insert(r);
                for(int other : reduced[r].cells)
                {
                    if(!visited.contains(other))
                    {
                        visited.insert(other);
                        stack.append(other);
                    }
                }
            }
        }

        if(cells.size() > MaxComponentCells)
            continue;

        const int n = cells.size();
        QHash<int, int> local;
        for(int j=0; j<n; ++j)
            local.insert(cells[j], j);

        const QVector<int> cons = QVector<int>(compConstraints.begin(), compConstraints.end());
        QVector<QVector<int>> cellCons(n);
        QVector<int> consMines(cons.size(), 0);
        QVector<int> consOpen(cons.size(), 0);
        QVector<int> consNeed(cons.size(), 0);
        for(int j=0; j<cons.size(); ++j)
        {
            consOpen[j] = reduced[cons[j]].cells.size();
            consNeed[j] = reduced[cons[j]].remaining;
            for(int cell : reduced[cons[j]].cells)
                cellCons[local.value(cell)].append(j);
        }

        QVector<int> assign(n, -1);
        QVector<bool> canMine(n, false), canSafe(n, false);
        quint64 nodes = 0;
        bool aborted = false;
        bool anySolution = false;
        int solutionCount = 0;

        std::function<void(int)> dfs = [&](int k)
        {
            if(aborted)
                return;
            if(++nodes > NodeBudget)
            {
                aborted = true;
                return;
            }
            if(k == n)
            {
                anySolution = true;
                ++solutionCount;
                for(int j=0; j<n; ++j)
                {
                    if(assign[j] == 1) canMine[j] = true;
                    else canSafe[j] = true;
                }
                return;
            }
            for(int v=0; v<=1; ++v)
            {
                assign[k] = v;
                bool ok = true;
                for(int c : cellCons[k])
                {
                    consMines[c] += v;
                    consOpen[c] -= 1;
                    if(consMines[c] > consNeed[c] || consMines[c] + consOpen[c] < consNeed[c])
                        ok = false;
                }
                if(ok)
                    dfs(k + 1);
                for(int c : cellCons[k])
                {
                    consMines[c] -= v;
                    consOpen[c] += 1;
                }
                assign[k] = -1;
            }
        };
        dfs(0);

        if(aborted || !anySolution)
            continue;

        for(int j=0; j<n; ++j)
        {
            if(canMine[j] && canSafe[j])
                continue;
            // All numbers of the component take part in the reasoning
            QVector<int> clues, clueRemaining;
            for(int c=0; c<cons.size(); ++c)
            {
                clues.append(reduced[cons[c]].origin);
                clueRemaining.append(reduced[cons[c]].remaining);
            }
            TrainerDeduction details;
            details.region = cells;
            details.solutions = solutionCount;
            for(int c : cons)
            {
                const int k = s.byOrigin.value(reduced[c].origin);
                for(int m : s.knownMinesOf(k))
                {
                    if(!details.knownMines.contains(m))
                        details.knownMines.append(m);
                }
                for(int c2 : s.knownIn(k))
                {
                    if(!details.support.contains(c2))
                        details.support.append(c2);
                }
            }
            s.add(cells[j], !canSafe[j], TrainerDeduction::Kind::Enumeration, clues, clueRemaining,
                  QString(), details);
        }
    }
}
}

namespace
{
// Solve in rounds: round N only uses what was proven before it, so every
// deduction's depth is the shortest chain of steps that reaches it
void solveInRounds(SolverState& s, bool useEnumeration)
{
    for(int round = 1; ; ++round)
    {
        singlesPass(s);
        pairsPass(s);
        if(s.pending.isEmpty() && useEnumeration)
            enumerationPass(s);
        if(s.pending.isEmpty())
            break;
        s.commit(round);
    }
}
}

QHash<int, TrainerDeduction> TrainerSolver::deduce(const TrainerBoard& board, bool useEnumeration,
                                                   int center, int radius)
{
    SolverState s(board, center, radius);
    solveInRounds(s, useEnumeration);

    // The player's flags are their own knowledge: a flag that is provable
    // from the numbers counts as already known (depth 0). Unprovable flags are
    // ignored, so the hidden mines never leak into the analysis.
    bool anyFlag = false;
    for(auto it = s.known.constBegin(); it != s.known.constEnd(); ++it)
    {
        if(it->mine && board.marks[it.key()] == TrainerBoard::Mark::Flag)
        {
            anyFlag = true;
            break;
        }
    }
    if(!anyFlag)
        return s.known;

    SolverState seeded(board, center, radius);
    for(auto it = s.known.constBegin(); it != s.known.constEnd(); ++it)
    {
        if(it->mine && board.marks[it.key()] == TrainerBoard::Mark::Flag)
        {
            TrainerDeduction flag = it.value();
            flag.depth = 0;
            flag.support.clear();
            seeded.known.insert(it.key(), flag);
        }
    }
    solveInRounds(seeded, useEnumeration);
    return seeded.known;
}

QVector<TrainerDeduction> TrainerSolver::prerequisitesOf(const QHash<int, TrainerDeduction>& all,
                                                         const TrainerDeduction& d)
{
    QVector<TrainerDeduction> result;
    QSet<int> seen;
    QVector<int> stack = d.support;
    while(!stack.isEmpty())
    {
        const int c = stack.takeLast();
        if(seen.contains(c) || !all.contains(c))
            continue;
        seen.insert(c);
        const TrainerDeduction& p = all[c];
        result.append(p);
        stack += p.support;
    }
    std::sort(result.begin(), result.end(), [](const TrainerDeduction& a, const TrainerDeduction& b)
    {
        return a.depth != b.depth ? a.depth < b.depth : a.cell < b.cell;
    });
    return result;
}

namespace
{
QString cellList(const QVector<int>& cells, int width)
{
    QStringList names;
    for(int c : cells)
        names.append(TrainerAnalyzer::cellName(c, width));
    return names.join(", ");
}

QString mineCount(int n)
{
    return n == 1 ? QStringLiteral("1 mine") : QStringLiteral("%1 mines").arg(n);
}

QString cellCount(int n)
{
    return n == 1 ? QStringLiteral("1 cell") : QStringLiteral("%1 cells").arg(n);
}

// Cells of `cells` that touch `clue`
QVector<int> touching(const TrainerBoard& b, int clue, const QVector<int>& cells)
{
    QVector<int> r;
    const QVector<int> around = b.neighbours(clue);
    for(int c : cells)
    {
        if(around.contains(c))
            r.append(c);
    }
    return r;
}

QString knownPart(const TrainerBoard& b, int clue, const QVector<int>& knownMines)
{
    const QVector<int> mine = touching(b, clue, knownMines);
    if(mine.isEmpty())
        return QString();
    return QStringLiteral(" (%1 already known at %2)").arg(mineCount(mine.size()), cellList(mine, b.width));
}
}

// One line reason for an earlier step of a chain
QString shortReason(const TrainerBoard& b, const TrainerDeduction& p)
{
    const int w = b.width;
    const QString what = QStringLiteral("%1 is %2").arg(TrainerAnalyzer::cellName(p.cell, w),
                                                        p.mine ? QStringLiteral("a mine") : QStringLiteral("safe"));
    const int clue = p.clues.value(0, -1);
    const QString clueName = clue >= 0 ? QStringLiteral("the %1 at %2").arg(b.value[clue])
                                          .arg(TrainerAnalyzer::cellName(clue, w)) : QString();
    switch(p.kind)
    {
        case TrainerDeduction::Kind::SatisfiedNumber:
            return QStringLiteral("%1, because %2 already has all its mines.").arg(what, clueName);
        case TrainerDeduction::Kind::FilledNumber:
            return QStringLiteral("%1, because %2 has exactly that many unknown cells left.").arg(what, clueName);
        case TrainerDeduction::Kind::Pattern:
            return QStringLiteral("%1, from the %2 pattern of %3 and the %4 at %5.")
                   .arg(what, p.pattern, clueName).arg(b.value[p.clues.value(1)])
                   .arg(TrainerAnalyzer::cellName(p.clues.value(1), w));
        case TrainerDeduction::Kind::Enumeration:
            return QStringLiteral("%1, by checking every arrangement around it.").arg(what);
    }
    return what + QLatin1Char('.');
}

QStringList TrainerSolver::explainSteps(const TrainerBoard& b, const TrainerDeduction& d,
                                        const QVector<TrainerDeduction>& prerequisites)
{
    QStringList steps;
    const int w = b.width;

    // Earlier steps of the chain (flags you placed count as known)
    QVector<int> flags;
    bool anyProven = false;
    for(const TrainerDeduction& p : prerequisites)
    {
        if(p.depth == 0)
            flags.append(p.cell);
        else
            anyProven = true;
    }
    if(!flags.isEmpty())
        steps << QStringLiteral("You had already flagged %1.").arg(cellList(flags, w));
    if(anyProven)
    {
        steps << QStringLiteral("This needs %1 steps of reasoning. Before the last step:").arg(d.depth);
        for(const TrainerDeduction& p : prerequisites)
        {
            if(p.depth > 0)
                steps << QStringLiteral("Step %1: %2").arg(p.depth).arg(shortReason(b, p));
        }
        steps << QStringLiteral("Last step:");
    }

    switch(d.kind)
    {
        case TrainerDeduction::Kind::SatisfiedNumber:
        {
            const int clue = d.clues.value(0);
            const int n = b.value[clue];
            QVector<int> hidden;
            for(int nb : b.neighbours(clue))
            {
                if(!b.revealed[nb])
                    hidden.append(nb);
            }
            const QVector<int> mines = touching(b, clue, d.knownMines);
            steps << QStringLiteral("The %1 at %2 needs %3 around it.")
                     .arg(n).arg(TrainerAnalyzer::cellName(clue, w), mineCount(n));
            steps << QStringLiteral("Its hidden neighbours are %1.").arg(cellList(hidden, w));
            steps << QStringLiteral("%1 of them %2 already known to be %3: %4.")
                     .arg(mines.size()).arg(mines.size() == 1 ? "is" : "are")
                     .arg(mines.size() == 1 ? "a mine" : "mines", cellList(mines, w));
            steps << QStringLiteral("That is all %1, so the remaining %2 (%3) %4 safe.")
                     .arg(mineCount(n), cellCount(d.group.size()), cellList(d.group, w),
                          d.group.size() == 1 ? "is" : "are");
            break;
        }
        case TrainerDeduction::Kind::FilledNumber:
        {
            const int clue = d.clues.value(0);
            const int r = d.clueRemaining.value(0);
            steps << QStringLiteral("The %1 at %2 still needs %3%4.")
                     .arg(b.value[clue]).arg(TrainerAnalyzer::cellName(clue, w), mineCount(r),
                          knownPart(b, clue, d.knownMines));
            steps << QStringLiteral("Only %1 around it %2 still unknown: %3.")
                     .arg(cellCount(d.group.size()), d.group.size() == 1 ? "is" : "are",
                          cellList(d.group, w));
            steps << QStringLiteral("Every remaining mine has to go there, so %1 %2 %3.")
                     .arg(cellList(d.group, w), d.group.size() == 1 ? "is" : "are",
                          d.group.size() == 1 ? "a mine" : "mines");
            break;
        }
        case TrainerDeduction::Kind::Pattern:
        {
            const int h = d.clues.value(0), t = d.clues.value(1);
            const int rh = d.clueRemaining.value(0), rt = d.clueRemaining.value(1);
            const QVector<int> hCells = touching(b, h, d.region);
            const QVector<int> tCells = touching(b, t, d.region);
            QVector<int> hOnly, tOnly;
            for(int c : hCells) { if(!d.shared.contains(c)) hOnly.append(c); }
            for(int c : tCells) { if(!d.shared.contains(c)) tOnly.append(c); }
            const QString hName = QStringLiteral("the %1 at %2").arg(b.value[h]).arg(TrainerAnalyzer::cellName(h, w));
            const QString tName = QStringLiteral("the %1 at %2").arg(b.value[t]).arg(TrainerAnalyzer::cellName(t, w));

            steps << QStringLiteral("Compare two numbers: %1 and %2.").arg(hName, tName);
            steps << QStringLiteral("%1 still needs %2%3 among %4.")
                     .arg(hName.left(1).toUpper() + hName.mid(1), mineCount(rh),
                          knownPart(b, h, d.knownMines), cellList(hCells, w));
            steps << QStringLiteral("%1 still needs %2%3 among %4.")
                     .arg(tName.left(1).toUpper() + tName.mid(1), mineCount(rt),
                          knownPart(b, t, d.knownMines), cellList(tCells, w));
            steps << QStringLiteral("They share %1: %2.").arg(cellCount(d.shared.size()), cellList(d.shared, w));
            if(d.mine)
            {
                const int maxShared = qMin(qMin(rh, rt), static_cast<int>(d.shared.size()));
                steps << QStringLiteral("Because of %1, the shared cells can hold at most %2.")
                         .arg(hName, mineCount(maxShared));
                steps << QStringLiteral("%1 needs %2, so at least %3 must be in its other %4 (%5).")
                         .arg(tName.left(1).toUpper() + tName.mid(1), mineCount(rt),
                              mineCount(rt - maxShared), cellCount(tOnly.size()), cellList(tOnly, w));
                steps << QStringLiteral("There are exactly that many, so %1 %2 %3.")
                         .arg(cellList(d.group, w), d.group.size() == 1 ? "is" : "are",
                              d.group.size() == 1 ? "a mine" : "all mines");
            }
            else
            {
                const int minShared = qMax(0, rh - static_cast<int>(hOnly.size()));
                steps << QStringLiteral("%1 can put at most %2 outside the shared cells, so the shared cells hold at least %3.")
                         .arg(hName.left(1).toUpper() + hName.mid(1), mineCount(hOnly.size()),
                              mineCount(minShared));
                steps << QStringLiteral("That already gives %1 all %2 it needs, so its other %3 (%4) %5 safe.")
                         .arg(tName, mineCount(rt), cellCount(tOnly.size()), cellList(tOnly, w),
                              tOnly.size() == 1 ? "is" : "are");
            }
            if(d.pattern == QLatin1String("1-2-1"))
                steps << QStringLiteral("Shortcut: in a 1-2-1 along a wall, the cells beside the 1s are mines and the cell beside the 2 is safe.");
            else if(d.pattern == QLatin1String("1-2-2-1"))
                steps << QStringLiteral("Shortcut: in a 1-2-2-1 along a wall, the cells beside the two 2s are mines and the cells beside the 1s are safe.");
            break;
        }
        case TrainerDeduction::Kind::Enumeration:
            steps << QStringLiteral("No single number or pair of numbers settles this, so look at all of them together: %1.")
                     .arg(cellList(d.clues, w));
            steps << QStringLiteral("Try every way of placing mines in the %1 unknown cells they touch.")
                     .arg(d.region.size());
            steps << QStringLiteral("%1 arrangement%2 fit every number, and in all of them %3 is %4.")
                     .arg(d.solutions).arg(d.solutions == 1 ? "" : "s")
                     .arg(TrainerAnalyzer::cellName(d.cell, w),
                          d.mine ? QStringLiteral("a mine") : QStringLiteral("safe"));
            break;
    }
    return steps;
}

QString TrainerSolver::kindName(TrainerDeduction::Kind kind)
{
    switch(kind)
    {
        case TrainerDeduction::Kind::SatisfiedNumber: return QStringLiteral("satisfied number");
        case TrainerDeduction::Kind::FilledNumber:    return QStringLiteral("all hidden cells are mines");
        case TrainerDeduction::Kind::Pattern:         return QStringLiteral("pattern");
        case TrainerDeduction::Kind::Enumeration:     return QStringLiteral("advanced logic");
    }
    return QString();
}

// ===========================================================================
// TrainerAnalyzer
// ===========================================================================

QString TrainerFinding::typeName(Type t)
{
    switch(t)
    {
        case Type::CursorJump:         return QStringLiteral("Cursor jump");
        case Type::UnnecessaryGuess:   return QStringLiteral("Unnecessary guess");
        case Type::RejectedChord:      return QStringLiteral("Rejected chord");
        case Type::WastedChord:        return QStringLiteral("Wasted chord");
        case Type::DominatedChord:     return QStringLiteral("Better chord");
        case Type::ClicksCouldBeChord: return QStringLiteral("Clicks -> chord");
        case Type::LowValueChord:      return QStringLiteral("Low value chord");
        case Type::WrongFlag:          return QStringLiteral("Wrong flag");
        case Type::WastedClick:        return QStringLiteral("Wasted click");
    }
    return QString();
}

QString TrainerAnalyzer::cellName(int cell, int width)
{
    return QStringLiteral("(%1,%2)").arg(cell % width + 1).arg(cell / width + 1);
}

namespace
{
QString clueText(const TrainerBoard& b, int clue, int remaining)
{
    const QString base = QStringLiteral("the %1 at %2")
                         .arg(b.value[clue]).arg(TrainerAnalyzer::cellName(clue, b.width));
    if(remaining >= 0 && remaining != b.value[clue])
        return base + QStringLiteral(" (%1 mine%2 still unaccounted for)")
                      .arg(remaining).arg(remaining == 1 ? QString() : QStringLiteral("s"));
    return base;
}

QString patternTip(const QString& pattern)
{
    if(pattern == QLatin1String("1-2-1"))
        return QStringLiteral("Tip: in a 1-2-1 along a wall, the cells beside the 1s are mines and the cell beside the 2 is safe.");
    if(pattern == QLatin1String("1-2-2-1"))
        return QStringLiteral("Tip: in a 1-2-2-1 along a wall, the cells beside the two 2s are mines and the cells beside the 1s are safe.");
    if(pattern == QLatin1String("1-1"))
        return QStringLiteral("Tip: when one 1's hidden cells all touch another 1, the second 1's extra cells are safe.");
    if(pattern == QLatin1String("1-2"))
        return QStringLiteral("Tip: the shared cells can hold at most 1 mine for the 2, so the 2's extra cell must be a mine.");
    return QStringLiteral("Tip: compare the two numbers - the shared cells limit how many mines each can still place elsewhere.");
}

QString describeDeduction(const TrainerBoard& b, const TrainerDeduction& d)
{
    const QString cell = TrainerAnalyzer::cellName(d.cell, b.width);
    const QString verdict = d.mine ? QStringLiteral("is a mine") : QStringLiteral("is safe");
    switch(d.kind)
    {
        case TrainerDeduction::Kind::SatisfiedNumber:
        {
            const int clue = d.clues.value(0, -1);
            QString s = QStringLiteral("Satisfied number: %1 already has all its mines, so %2 %3.")
                        .arg(clueText(b, clue, -1), cell, verdict);
            if(clue >= 0 && b.flagsAround(clue) == b.value[clue] && b.wrongFlagsAround(clue) == 0)
                s += QStringLiteral(" Its flags were already placed - one chord on %1 would open it.")
                     .arg(TrainerAnalyzer::cellName(clue, b.width));
            return s;
        }
        case TrainerDeduction::Kind::FilledNumber:
            return QStringLiteral("Filled number: %1 has exactly as many hidden cells as mines left, so %2 %3.")
                   .arg(clueText(b, d.clues.value(0, -1), d.clueRemaining.value(0, -1)), cell, verdict);
        case TrainerDeduction::Kind::Pattern:
            return QStringLiteral("%1 pattern between %2 and %3: %4 %5. %6")
                   .arg(d.pattern,
                        clueText(b, d.clues.value(0, -1), d.clueRemaining.value(0, -1)),
                        clueText(b, d.clues.value(1, -1), d.clueRemaining.value(1, -1)),
                        cell, verdict, patternTip(d.pattern));
        case TrainerDeduction::Kind::Enumeration:
        {
            QStringList clues;
            for(int c : d.clues)
                clues.append(TrainerAnalyzer::cellName(c, b.width));
            return QStringLiteral("Advanced logic: every mine arrangement that fits the numbers at %1 leaves %2 %3.")
                   .arg(clues.join(", "), cell, d.mine ? QStringLiteral("as a mine") : QStringLiteral("safe"));
        }
    }
    return QString();
}

// Best provable cell close to `from` that the player left behind
bool pickMissedNear(const TrainerBoard& b, const QHash<int, TrainerDeduction>& ded,
                    int from, int target, double radius, TrainerDeduction& best)
{
    const double targetDist = euclid(b, from, target);
    bool found = false;
    double bestDist = 0;
    for(auto it = ded.constBegin(); it != ded.constEnd(); ++it)
    {
        const TrainerDeduction& d = it.value();
        // Only safe cells are unfinished work: a mine never has to be clicked
        if(d.mine || b.revealed[d.cell] || d.cell == target ||
           b.marks[d.cell] != TrainerBoard::Mark::None)
            continue;
        const double dist = euclid(b, from, d.cell);
        if(dist > radius || dist >= targetDist)
            continue;

        bool better = !found;
        if(!better && d.kind != best.kind)
            better = d.kind < best.kind;          // simplest reasoning first
        else if(!better)
            better = dist < bestDist;
        if(better)
        {
            best = d;
            bestDist = dist;
            found = true;
        }
    }
    return found;
}

QString cellsText(const QVector<int>& cells, int width)
{
    QStringList names;
    for(int c : cells)
        names.append(TrainerAnalyzer::cellName(c, width));
    return names.join(", ");
}
}

TrainerReport TrainerAnalyzer::analyze(const TrainerRecording& recording)
{
    return analyze(recording, Options());
}

TrainerReport TrainerAnalyzer::analyze(const TrainerRecording& recording, const Options& options)
{
    TrainerReport report;
    TrainerStats& stats = report.stats;

    if(recording.width <= 0 || recording.height <= 0)
        return report;

    TrainerBoard board(recording);
    stats.threeBV = board.threeBV();

    QHash<int, TrainerDeduction> ded;
    bool dedValid = false;
    auto ensureDeductions = [&]()
    {
        if(!dedValid)
        {
            ded = TrainerSolver::deduce(board);
            dedValid = true;
        }
    };

    const QVector<TrainerAction>& actions = recording.actions;
    int prevCell = -1;
    qint64 prevT = 0;

    for(int i=0; i<actions.size(); ++i)
    {
        const TrainerAction& a = actions[i];
        report.snapshots.append(board.snapshot());
        if(!board.inside(a.x, a.y))
        {
            report.openedPerAction.append(0);
            continue;
        }
        const int cell = board.index(a.x, a.y);

        // ---- Cursor jumps ------------------------------------------------
        if(prevCell >= 0 && !board.exploded)
        {
            const double dist = euclid(board, prevCell, cell);
            if(dist >= options.jumpThreshold)
            {
                stats.jumps++;
                // Only what the player could see around the previous click
                const QHash<int, TrainerDeduction> local =
                    TrainerSolver::deduce(board, true, prevCell, options.jumpWindow);
                TrainerDeduction missed;
                if(pickMissedNear(board, local, prevCell, cell, options.localRadius, missed))
                {
                    stats.jumpsWithMissedPattern++;
                    TrainerFinding f;
                    f.type = TrainerFinding::Type::CursorJump;
                    f.actionIndex = i;
                    f.severity = 2;
                    f.fromCell = prevCell;
                    f.actionCell = cell;
                    f.suggestedCell = missed.cell;
                    f.clueCells = missed.clues;
                    f.hasDeduction = true;
                    f.deduction = missed;
                    f.prerequisites = TrainerSolver::prerequisitesOf(local, missed);
                    f.cursorFromMs = prevT;
                    f.cursorToMs = a.tMs;
                    QString missedName;
                    switch(missed.kind)
                    {
                        case TrainerDeduction::Kind::SatisfiedNumber: missedName = QStringLiteral("satisfied number"); break;
                        case TrainerDeduction::Kind::FilledNumber:    missedName = QStringLiteral("filled number"); break;
                        case TrainerDeduction::Kind::Pattern:         missedName = missed.pattern + QStringLiteral(" pattern"); break;
                        case TrainerDeduction::Kind::Enumeration:     missedName = QStringLiteral("advanced deduction"); break;
                    }
                    f.title = QStringLiteral("Jumped %1 cells from %2 to %3 - missed %4: %5 %6")
                              .arg(QString::number(dist, 'f', 1),
                                   cellName(prevCell, board.width), cellName(cell, board.width),
                                   missedName, cellName(missed.cell, board.width),
                                   missed.mine ? QStringLiteral("is a mine") : QStringLiteral("is safe"));
                    f.detail = QStringLiteral("%1 It was %2 cells from your previous click.")
                               .arg(describeDeduction(board, missed),
                                    QString::number(euclid(board, prevCell, missed.cell), 'f', 1));
                    report.findings.append(f);
                }
            }
        }

        // ---- Per action checks -------------------------------------------
        switch(a.type)
        {
            case TrainerAction::Type::Reveal:
            {
                stats.leftClicks++;
                if(board.revealed[cell] || board.marks[cell] != TrainerBoard::Mark::None)
                {
                    stats.wastedClicks++;
                    break;
                }
                if(board.revealedCount() == 0)
                    break; // first click is always a guess

                ensureDeductions();
                const bool provenSafe = ded.contains(cell) && !ded.value(cell).mine;
                if(provenSafe)
                    break;

                // Was a 100% safe move available that was just as fast? A
                // reveal or a chord is one click each, so "as fast" means no
                // further from where the cursor was than the guessed cell.
                const int origin = prevCell >= 0 ? prevCell : cell;
                const double guessDist = euclid(board, origin, cell);
                const double reach = guessDist + options.sameSpeedSlack;
                const int window = qMax(options.guessWindow, int(std::ceil(reach)) + options.guessWindow);
                const QHash<int, TrainerDeduction> area = TrainerSolver::deduce(board, true, origin, window);

                struct SafeMove { int cell; bool chord; double dist; };
                QVector<SafeMove> moves;
                for(auto it = area.constBegin(); it != area.constEnd(); ++it)
                {
                    const TrainerDeduction& d = it.value();
                    if(d.mine || board.revealed[d.cell] || board.marks[d.cell] != TrainerBoard::Mark::None)
                        continue;
                    const double dd = euclid(board, origin, d.cell);
                    if(dd <= reach)
                        moves.append({d.cell, false, dd});
                }
                // Chords: every flag around the number is a proven mine and it
                // still has something to open
                for(int c=0; c<board.value.size(); ++c)
                {
                    if(!board.revealed[c] || board.value[c] <= 0 ||
                       board.flagsAround(c) != board.value[c])
                        continue;
                    const double dd = euclid(board, origin, c);
                    if(dd > reach)
                        continue;
                    bool flagsProven = true, opensSomething = false;
                    for(int nb : board.neighbours(c))
                    {
                        if(board.marks[nb] == TrainerBoard::Mark::Flag)
                            flagsProven = flagsProven && area.contains(nb) && area.value(nb).mine;
                        else if(!board.revealed[nb])
                            opensSomething = true;
                    }
                    if(flagsProven && opensSomething)
                        moves.append({c, true, dd});
                }
                std::sort(moves.begin(), moves.end(), [](const SafeMove& a, const SafeMove& b)
                          { return a.dist < b.dist; });

                // The safe move must also be visible from the numbers around it
                int best = -1;
                bool bestChord = false;
                QHash<int, TrainerDeduction> bestLocal;
                for(const SafeMove& m : moves)
                {
                    const QHash<int, TrainerDeduction> local =
                        TrainerSolver::deduce(board, true, m.cell, options.guessWindow);
                    bool ok = true;
                    if(m.chord)
                    {
                        for(int nb : board.neighbours(m.cell))
                        {
                            if(board.marks[nb] == TrainerBoard::Mark::Flag)
                                ok = ok && local.contains(nb) && local.value(nb).mine;
                        }
                    }
                    else
                    {
                        ok = local.contains(m.cell) && !local.value(m.cell).mine;
                    }
                    if(ok)
                    {
                        best = m.cell;
                        bestChord = m.chord;
                        bestLocal = local;
                        break;
                    }
                }
                if(best < 0)
                    break; // no equally fast safe move: the guess may have been worth it

                stats.guesses++;
                TrainerFinding f;
                f.type = TrainerFinding::Type::UnnecessaryGuess;
                f.actionIndex = i;
                f.severity = board.isMine(cell) ? 3 : 2;
                f.fromCell = prevCell;
                f.actionCell = cell;
                f.suggestedCell = best;
                const QString distances = QStringLiteral(" It was %1 cells from your cursor; the guess was %2.")
                                          .arg(QString::number(euclid(board, origin, best), 'f', 1),
                                               QString::number(guessDist, 'f', 1));
                if(bestChord)
                {
                    const QVector<int> opens = board.chordPreview(best);
                    QVector<int> flags;
                    for(int nb : board.neighbours(best))
                    {
                        if(board.marks[nb] == TrainerBoard::Mark::Flag)
                            flags.append(nb);
                    }
                    f.clueCells = {best};
                    f.extraCells = opens;
                    f.title = QStringLiteral("Guessed %1%2 while a chord on %3 was 100% safe and just as close")
                              .arg(cellName(cell, board.width),
                                   board.isMine(cell) ? QStringLiteral(" (a mine!)") : QString(),
                                   cellName(best, board.width));
                    f.detail = QStringLiteral("The %1 at %2 already had all its mines flagged, and those flags "
                                              "(%3) follow from the numbers, so a chord there would have opened "
                                              "%4 cell(s) with no risk.%5")
                               .arg(board.value[best]).arg(cellName(best, board.width))
                               .arg(cellsText(flags, board.width)).arg(opens.size()).arg(distances);
                }
                else
                {
                    f.clueCells = bestLocal.value(best).clues;
                    f.hasDeduction = true;
                    f.deduction = bestLocal.value(best);
                    f.prerequisites = TrainerSolver::prerequisitesOf(bestLocal, f.deduction);
                    f.title = QStringLiteral("Guessed %1%2 while %3 was 100% safe and just as close")
                              .arg(cellName(cell, board.width),
                                   board.isMine(cell) ? QStringLiteral(" (a mine!)") : QString(),
                                   cellName(best, board.width));
                    f.detail = describeDeduction(board, f.deduction) + distances;
                }
                if(prevCell >= 0)
                {
                    f.cursorFromMs = prevT;
                    f.cursorToMs = a.tMs;
                }
                report.findings.append(f);
                break;
            }
            case TrainerAction::Type::Flag:
            {
                stats.rightClicks++;
                if(board.revealed[cell])
                {
                    stats.wastedClicks++;
                    break;
                }
                if(board.marks[cell] == TrainerBoard::Mark::None && !board.isMine(cell))
                {
                    TrainerFinding f;
                    f.type = TrainerFinding::Type::WrongFlag;
                    f.actionIndex = i;
                    f.severity = 2;
                    f.actionCell = cell;
                    f.title = QStringLiteral("Flagged %1, which is not a mine")
                              .arg(cellName(cell, board.width));
                    ensureDeductions();
                    if(ded.contains(cell) && !ded.value(cell).mine)
                    {
                        f.detail = describeDeduction(board, ded.value(cell));
                        f.hasDeduction = true;
                        f.deduction = ded.value(cell);
                        f.prerequisites = TrainerSolver::prerequisitesOf(ded, f.deduction);
                    }
                    else
                        f.detail = QStringLiteral("The flag was not justified by the visible numbers.");
                    f.clueCells = ded.value(cell).clues;
                    report.findings.append(f);
                }
                break;
            }
            case TrainerAction::Type::Chord:
            {
                stats.chords++;
                if(!board.revealed[cell] || board.value[cell] <= 0)
                {
                    stats.wastedClicks++;
                    break;
                }

                const int flags = board.flagsAround(cell);
                if(flags != board.value[cell])
                {
                    stats.wastedClicks++;
                    TrainerFinding f;
                    f.type = TrainerFinding::Type::RejectedChord;
                    f.actionIndex = i;
                    f.severity = 2;
                    f.actionCell = cell;
                    f.wastedClicks = 1;
                    f.title = QStringLiteral("Chord on the %1 at %2 was rejected (%3 flag%4 around it)")
                              .arg(board.value[cell]).arg(cellName(cell, board.width))
                              .arg(flags).arg(flags == 1 ? QString() : QStringLiteral("s"));
                    f.detail = flags < board.value[cell] ?
                               QStringLiteral("It still needed %1 more flag(s). Flag first, or click the safe cells directly.")
                               .arg(board.value[cell] - flags) :
                               QStringLiteral("There are more flags than the number - at least one flag is wrong.");
                    report.findings.append(f);
                    break;
                }

                bool boom = false;
                const QVector<int> opened = board.chordPreview(cell, &boom);
                if(boom)
                    break; // the wrong flag is reported when it was placed
                if(opened.isEmpty())
                {
                    stats.wastedClicks++;
                    TrainerFinding f;
                    f.type = TrainerFinding::Type::WastedChord;
                    f.actionIndex = i;
                    f.severity = 1;
                    f.actionCell = cell;
                    f.wastedClicks = 1;
                    f.title = QStringLiteral("Chord on %1 opened nothing").arg(cellName(cell, board.width));
                    f.detail = QStringLiteral("Every neighbour was already open or flagged.");
                    report.findings.append(f);
                    break;
                }

                // Would another chord nearby have opened strictly more?
                int bestCell = -1;
                QVector<int> bestOpened;
                for(int dy=-2; dy<=2; ++dy)
                {
                    for(int dx=-2; dx<=2; ++dx)
                    {
                        const int x = a.x + dx, y = a.y + dy;
                        if((dx == 0 && dy == 0) || !board.inside(x, y))
                            continue;
                        const int c = board.index(x, y);
                        if(!board.revealed[c] || board.value[c] <= 0 ||
                           board.flagsAround(c) != board.value[c] || board.wrongFlagsAround(c) > 0)
                            continue;
                        const QVector<int> alt = board.chordPreview(c);
                        if(alt.size() <= opened.size() || alt.size() <= bestOpened.size())
                            continue;
                        bool superset = true;
                        for(int o : opened)
                            superset = superset && alt.contains(o);
                        if(superset)
                        {
                            bestCell = c;
                            bestOpened = alt;
                        }
                    }
                }
                if(bestCell >= 0)
                {
                    TrainerFinding f;
                    f.type = TrainerFinding::Type::DominatedChord;
                    f.actionIndex = i;
                    f.severity = 2;
                    f.actionCell = cell;
                    f.suggestedCell = bestCell;
                    f.extraCells = bestOpened;
                    f.title = QStringLiteral("Chord on %1 instead of %2 would open %3 cells, not %4")
                              .arg(cellName(bestCell, board.width), cellName(cell, board.width))
                              .arg(bestOpened.size()).arg(opened.size());
                    f.detail = QStringLiteral("The %1 at %2 was already satisfied by your flags and its chord "
                                              "opens everything yours did plus %3 more cell(s). "
                                              "Prefer the satisfied number with the most hidden neighbours.")
                               .arg(board.value[bestCell]).arg(cellName(bestCell, board.width))
                               .arg(bestOpened.size() - opened.size());
                    report.findings.append(f);
                }

                // Flags placed right before only to chord a single cell
                if(opened.size() == 1)
                {
                    const QVector<int> around = board.neighbours(cell);
                    int flagsJustPlaced = 0;
                    for(int j=i-1; j>=0 && actions[j].type == TrainerAction::Type::Flag; --j)
                    {
                        if(around.contains(board.index(actions[j].x, actions[j].y)))
                            flagsJustPlaced++;
                    }
                    if(flagsJustPlaced > 0)
                    {
                        stats.savableClicks += flagsJustPlaced;
                        TrainerFinding f;
                        f.type = TrainerFinding::Type::LowValueChord;
                        f.actionIndex = i;
                        f.severity = 1;
                        f.actionCell = cell;
                        f.suggestedCell = opened.first();
                        f.wastedClicks = flagsJustPlaced;
                        f.title = QStringLiteral("%1 flag(s) + chord on %2 to open a single cell")
                                  .arg(flagsJustPlaced).arg(cellName(cell, board.width));
                        f.detail = QStringLiteral("That cost %1 clicks. Clicking %2 directly costs 1.")
                                   .arg(flagsJustPlaced + 1).arg(cellName(opened.first(), board.width));
                        report.findings.append(f);
                    }
                }
                break;
            }
        }

        const TrainerBoard::Result res = board.apply(a);
        report.openedPerAction.append(res.opened.size());
        if(!res.opened.isEmpty())
            dedValid = false;

        prevCell = cell;
        prevT = a.tMs;
    }
    report.snapshots.append(board.snapshot());

    // ---- Runs of single clicks around one number -------------------------
    TrainerBoard scratch(recording);
    int i = 0;
    while(i < actions.size())
    {
        const TrainerAction& a = actions[i];
        if(a.type != TrainerAction::Type::Reveal || report.openedPerAction.value(i) <= 0)
        {
            ++i;
            continue;
        }
        scratch.restore(report.snapshots[i]);
        const int first = scratch.index(a.x, a.y);

        QVector<int> candidates;
        for(int nb : scratch.neighbours(first))
        {
            if(scratch.revealed[nb] && scratch.value[nb] > 0 && scratch.wrongFlagsAround(nb) == 0)
                candidates.append(nb);
        }

        QVector<int> run{i};
        int k = i + 1;
        while(k < actions.size() && !candidates.isEmpty())
        {
            const TrainerAction& b = actions[k];
            const int t = scratch.index(b.x, b.y);
            if(b.type == TrainerAction::Type::Flag && scratch.isMine(t))
            {
                bool nearCandidate = false;
                for(int c : candidates)
                    nearCandidate = nearCandidate || chebyshev(scratch, c, t) == 1;
                if(!nearCandidate)
                    break;
                ++k;
                continue;
            }
            if(b.type == TrainerAction::Type::Reveal && report.openedPerAction.value(k) > 0)
            {
                QVector<int> narrowed;
                for(int c : candidates)
                {
                    if(chebyshev(scratch, c, t) == 1)
                        narrowed.append(c);
                }
                if(narrowed.isEmpty())
                    break;
                candidates = narrowed;
                run.append(k);
                ++k;
                continue;
            }
            break;
        }

        if(run.size() >= 2)
        {
            int best = -1, bestCost = INT_MAX, bestFlagsUsed = 0;
            for(int c : candidates)
            {
                const int cost = scratch.unflaggedMinesAround(c) + 1;
                int flagsUsed = 0;
                for(int j=i; j<k; ++j)
                {
                    if(actions[j].type == TrainerAction::Type::Flag &&
                       chebyshev(scratch, c, scratch.index(actions[j].x, actions[j].y)) == 1)
                        flagsUsed++;
                }
                if(cost < bestCost)
                {
                    best = c;
                    bestCost = cost;
                    bestFlagsUsed = flagsUsed;
                }
            }
            const int clicksUsed = run.size() + bestFlagsUsed;
            if(best >= 0 && bestCost < clicksUsed)
            {
                QVector<int> targets;
                for(int j : run)
                    targets.append(scratch.index(actions[j].x, actions[j].y));

                stats.savableClicks += clicksUsed - bestCost;
                TrainerFinding f;
                f.type = TrainerFinding::Type::ClicksCouldBeChord;
                f.actionIndex = i;
                f.severity = clicksUsed - bestCost >= 2 ? 2 : 1;
                f.actionCell = targets.first();
                f.suggestedCell = best;
                f.extraCells = targets;
                f.wastedClicks = clicksUsed - bestCost;
                f.cursorFromMs = actions[i].tMs;
                f.cursorToMs = actions[k-1].tMs;
                const int flagsNeeded = bestCost - 1;
                f.title = QStringLiteral("%1 clicks around %2 - chording there costs %3")
                          .arg(clicksUsed).arg(cellName(best, scratch.width)).arg(bestCost);
                f.detail = QStringLiteral("You opened %1 one at a time. %2chording on the %3 at %4 "
                                          "opens all of them in %5 click%6.")
                           .arg(cellsText(targets, scratch.width),
                                flagsNeeded > 0 ?
                                    QStringLiteral("Flagging %1 mine%2 and ").arg(flagsNeeded)
                                        .arg(flagsNeeded == 1 ? QString() : QStringLiteral("s")) :
                                    QStringLiteral("Its mines were already flagged, so "),
                                QString::number(scratch.value[best]),
                                cellName(best, scratch.width),
                                QString::number(bestCost),
                                bestCost == 1 ? QString() : QStringLiteral("s"));
                report.findings.append(f);
            }
            i = k;
        }
        else
        {
            ++i;
        }
    }

    // ---- Aggregate statistics --------------------------------------------
    stats.threeBVSolved = board.threeBVSolved();
    for(int s=1; s<recording.cursor.size(); ++s)
    {
        const double dx = recording.cursor[s].x - recording.cursor[s-1].x;
        const double dy = recording.cursor[s].y - recording.cursor[s-1].y;
        stats.cursorTravelCells += std::sqrt(dx*dx + dy*dy);
    }

    stats.durationSec = recording.durationMs / 1000.0;
    if(stats.durationSec <= 0 && actions.size() > 1)
        stats.durationSec = (actions.last().tMs - actions.first().tMs) / 1000.0;

    const int clicks = stats.leftClicks + stats.rightClicks + stats.chords;
    if(clicks > 0)
        stats.efficiency = 100.0 * stats.threeBVSolved / clicks;
    if(stats.durationSec > 0)
        stats.threeBVPerSecond = stats.threeBVSolved / stats.durationSec;

    std::stable_sort(report.findings.begin(), report.findings.end(),
                     [](const TrainerFinding& x, const TrainerFinding& y)
                     { return x.actionIndex < y.actionIndex; });

    return report;
}

QString TrainerReport::toText(const TrainerRecording& recording) const
{
    QString out;
    QTextStream ts(&out);
    ts << "MineCoach report\n";
    ts << QString("Board %1x%2, %3 mines - %4\n")
          .arg(recording.width).arg(recording.height).arg(recording.mines)
          .arg(recording.won ? QStringLiteral("won") : QStringLiteral("lost"));
    ts << QString("Time %1 s | 3BV %2/%3 | 3BV/s %4 | efficiency %5%\n")
          .arg(stats.durationSec, 0, 'f', 2).arg(stats.threeBVSolved).arg(stats.threeBV)
          .arg(stats.threeBVPerSecond, 0, 'f', 2).arg(stats.efficiency, 0, 'f', 0);
    ts << QString("Clicks: %1 left, %2 right, %3 chords | wasted %4 | savable %5\n")
          .arg(stats.leftClicks).arg(stats.rightClicks).arg(stats.chords)
          .arg(stats.wastedClicks).arg(stats.savableClicks);
    ts << QString("Cursor: %1 cells travelled, %2 jumps (%3 left a provable cell behind), %4 unnecessary guesses\n")
          .arg(stats.cursorTravelCells, 0, 'f', 1).arg(stats.jumps)
          .arg(stats.jumpsWithMissedPattern).arg(stats.guesses);
    ts << "\n";
    for(const TrainerFinding& f : findings)
    {
        ts << QString("#%1 [%2] %3\n").arg(f.actionIndex + 1)
              .arg(TrainerFinding::typeName(f.type), f.title);
        if(!f.detail.isEmpty())
            ts << "    " << f.detail << "\n";
    }
    if(findings.isEmpty())
        ts << "No inefficiencies found. Clean game!\n";
    return out;
}
