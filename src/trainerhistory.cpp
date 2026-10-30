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

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cmath>

#include "trainerhistory.h"

namespace
{
// Bounding box of the cells, grown by `pad` and to at least `minSize` cells
QRect areaAround(const QVector<int>& cells, int width, int height, int pad, int minSize)
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
    x0 -= pad; y0 -= pad; x1 += pad; y1 += pad;
    while(x1 - x0 + 1 < minSize) { --x0; ++x1; }
    while(y1 - y0 + 1 < minSize) { --y0; ++y1; }
    x0 = qMax(0, x0);
    y0 = qMax(0, y0);
    x1 = qMin(width - 1, x1);
    y1 = qMin(height - 1, y1);
    return QRect(QPoint(x0, y0), QPoint(x1, y1));
}

QVector<QPair<QString, int>> sortedCounts(const QMap<QString, int>& counts)
{
    QVector<QPair<QString, int>> v;
    for(auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        v.append(qMakePair(it.key(), it.value()));
    std::stable_sort(v.begin(), v.end(), [](const QPair<QString, int>& a, const QPair<QString, int>& b)
                     { return a.second > b.second; });
    return v;
}
}

QStringList TrainerHistory::recentRecordings(const QString& dir, int count)
{
    QDir d(dir);
    const QFileInfoList files = d.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Time);
    QStringList paths;
    for(const QFileInfo& f : files)
    {
        if(count > 0 && paths.size() >= count)
            break;
        paths.append(f.absoluteFilePath());
    }
    return paths;
}

void TrainerHistory::addGame(const QString& path, const TrainerRecording& recording, const TrainerReport& report)
{
    games.append({path, recording, report});
}

bool TrainerHistory::addFile(const QString& path, QString* error)
{
    TrainerRecording r;
    if(!TrainerRecording::load(path, r, error))
        return false;
    addGame(path, r, TrainerAnalyzer::analyze(r));
    return true;
}

int TrainerHistory::gameCount() const
{
    return games.size();
}

const TrainerRecording& TrainerHistory::recording(int i) const
{
    return games[i].recording;
}

QString TrainerHistory::path(int i) const
{
    return games.value(i).path;
}

QString TrainerHistory::categoryOf(const TrainerDeduction& d)
{
    switch(d.kind)
    {
        case TrainerDeduction::Kind::SatisfiedNumber: return QStringLiteral("satisfied number");
        case TrainerDeduction::Kind::FilledNumber:    return QStringLiteral("filled number");
        case TrainerDeduction::Kind::Pattern:
        {
            static const QStringList named = {QStringLiteral("1-1"), QStringLiteral("1-2"),
                                              QStringLiteral("1-2-1"), QStringLiteral("1-2-2-1")};
            if(named.contains(d.pattern))
                return d.pattern + QStringLiteral(" pattern");
            return QStringLiteral("other number pairs");
        }
        case TrainerDeduction::Kind::Enumeration:     return QStringLiteral("advanced deduction");
    }
    return QStringLiteral("other");
}

QString TrainerHistory::tipFor(const QString& category)
{
    if(category == QLatin1String("satisfied number"))
        return QStringLiteral("Before moving on, check whether the number you just finished has all its mines: "
                              "its other cells are then safe, and with flags down one chord opens them.");
    if(category == QLatin1String("1-1 pattern"))
        return QStringLiteral("When the hidden cells of one 1 are all next to another 1, the second 1's extra cells are safe.");
    if(category == QLatin1String("1-2 pattern"))
        return QStringLiteral("A 2 next to a 1 that shares all but one cell: the shared cells give at most one mine, so the 2's extra cell is a mine and the 1's extra cells are safe.");
    if(category == QLatin1String("1-2-1 pattern"))
        return QStringLiteral("1-2-1 along a wall: mines beside the 1s, safe beside the 2.");
    if(category == QLatin1String("1-2-2-1 pattern"))
        return QStringLiteral("1-2-2-1 along a wall: mines beside the two 2s, safe beside the 1s.");
    if(category == QLatin1String("other number pairs"))
        return QStringLiteral("Compare two neighbouring numbers: subtract what they share and see what is left for each.");
    if(category == QLatin1String("advanced deduction"))
        return QStringLiteral("Look at the whole group of numbers together and try the possible mine placements.");
    if(category == QLatin1String("best chord square"))
        return QStringLiteral("When several numbers are satisfied, chord the one with the most hidden neighbours.");
    if(category == QLatin1String("Cursor jump"))
        return QStringLiteral("Finish the area you are in before moving the cursor across the board.");
    if(category == QLatin1String("Unnecessary guess"))
        return QStringLiteral("Before guessing, look for a safe move that is just as close.");
    if(category == QLatin1String("Rejected chord"))
        return QStringLiteral("Only chord when the number of flags matches the number.");
    if(category == QLatin1String("Wasted chord"))
        return QStringLiteral("Do not chord numbers that have nothing left to open.");
    if(category == QLatin1String("Low value chord"))
        return QStringLiteral("If only one cell would open, click it directly instead of flagging and chording.");
    if(category == QLatin1String("Clicks -> chord"))
        return QStringLiteral("Several clicks around one number? Flag its mines and chord once.");
    if(category == QLatin1String("Better chord"))
        return QStringLiteral("Pick the satisfied number that opens the most cells.");
    if(category == QLatin1String("Wrong flag"))
        return QStringLiteral("Only flag cells the numbers prove to be mines.");
    return QString();
}

TrainerHistorySummary TrainerHistory::summary() const
{
    TrainerHistorySummary s;
    QMap<QString, int> mistakes, patterns;
    double sumBV = 0, sumEff = 0;
    for(const Game& g : games)
    {
        const TrainerStats& st = g.report.stats;
        s.games++;
        if(g.recording.won)
            s.wins++;
        sumBV += st.threeBVPerSecond;
        sumEff += st.efficiency;
        s.jumps += st.jumps;
        s.missedPatterns += st.jumpsWithMissedPattern;
        s.guesses += st.guesses;
        s.wastedClicks += st.wastedClicks;
        s.savableClicks += st.savableClicks;
        for(const TrainerFinding& f : g.report.findings)
        {
            mistakes[TrainerFinding::typeName(f.type)]++;
            if(f.hasDeduction && !f.deduction.mine &&
               (f.type == TrainerFinding::Type::CursorJump || f.type == TrainerFinding::Type::UnnecessaryGuess))
                patterns[categoryOf(f.deduction)]++;
        }
    }
    if(s.games > 0)
    {
        s.avgThreeBVPerSecond = sumBV / s.games;
        s.avgEfficiency = sumEff / s.games;
    }
    s.mistakes = sortedCounts(mistakes);
    s.missedByPattern = sortedCounts(patterns);
    s.puzzleCandidates = candidates().size();
    return s;
}

QVector<TrainerPuzzle> TrainerHistory::candidates() const
{
    QVector<TrainerPuzzle> result;
    QSet<QString> seen;
    for(int gi = 0; gi < games.size(); ++gi)
    {
        const Game& g = games[gi];
        TrainerBoard board(g.recording);
        for(const TrainerFinding& f : g.report.findings)
        {
            const QString key = QStringLiteral("%1/%2/%3").arg(gi).arg(f.actionIndex).arg(int(f.type));
            if(seen.contains(key) || f.actionIndex >= g.report.snapshots.size())
                continue;

            TrainerPuzzle p;
            p.game = gi;
            p.actionIndex = f.actionIndex;
            p.snapshot = g.report.snapshots[f.actionIndex];
            board.restore(p.snapshot);

            if(f.hasDeduction && !f.deduction.mine &&
               (f.type == TrainerFinding::Type::CursorJump || f.type == TrainerFinding::Type::UnnecessaryGuess))
            {
                const TrainerDeduction& d = f.deduction;
                QVector<int> involved = d.clues + d.region + d.group + d.knownMines;
                for(const TrainerDeduction& pre : f.prerequisites)
                    involved += pre.clues + pre.group;
                p.type = TrainerPuzzle::Type::SafeCell;
                p.area = areaAround(involved, g.recording.width, g.recording.height, 1, 5);
                p.category = categoryOf(d);
                p.deduction = d;
                p.prerequisites = f.prerequisites;

                // The puzzle must be solvable from what it shows
                const QHash<int, TrainerDeduction> inArea = TrainerSolver::deduceInArea(board, p.area);
                if(!inArea.contains(d.cell) || inArea.value(d.cell).mine)
                    continue;
            }
            else if(f.type == TrainerFinding::Type::DominatedChord && f.suggestedCell >= 0)
            {
                // The same best square is often missed several moves in a row
                const QString chordKey = QStringLiteral("chord/%1/%2").arg(gi).arg(f.suggestedCell);
                if(seen.contains(chordKey))
                    continue;
                seen.insert(chordKey);
                QVector<int> involved = f.extraCells;
                involved << f.actionCell << f.suggestedCell;
                p.type = TrainerPuzzle::Type::BestChord;
                p.area = areaAround(involved, g.recording.width, g.recording.height, 1, 5);
                p.category = QStringLiteral("best chord square");
                p.chordCell = f.suggestedCell;
                p.chordOpens = board.chordPreview(f.suggestedCell).size();
                if(p.chordOpens <= 0)
                    continue;
            }
            else
            {
                continue;
            }
            p.view = p.area.adjusted(-1, -1, 1, 1)
                           .intersected(QRect(0, 0, g.recording.width, g.recording.height));
            seen.insert(key);
            result.append(p);
        }
    }
    return result;
}

QVector<TrainerPuzzle> TrainerHistory::playlist(int maxPuzzles) const
{
    const QVector<TrainerPuzzle> all = candidates();
    if(all.isEmpty() || maxPuzzles <= 0)
        return QVector<TrainerPuzzle>();

    // Group by category; inside a category take one puzzle per game in turn
    // (newest games first) so a single game does not fill the playlist
    QMap<QString, QVector<TrainerPuzzle>> byCategory;
    {
        QMap<QString, QMap<int, QVector<TrainerPuzzle>>> byGame;
        for(const TrainerPuzzle& p : all)
            byGame[p.category][p.game].append(p);
        for(auto c = byGame.constBegin(); c != byGame.constEnd(); ++c)
        {
            QVector<TrainerPuzzle>& out = byCategory[c.key()];
            for(int round = 0; ; ++round)
            {
                bool any = false;
                for(auto g = c.value().constBegin(); g != c.value().constEnd(); ++g)
                {
                    if(round < g.value().size())
                    {
                        out.append(g.value()[round]);
                        any = true;
                    }
                }
                if(!any)
                    break;
            }
        }
    }

    // Patterns are the point; chord puzzles get at most a quarter of the list
    const QString chordCategory = QStringLiteral("best chord square");

    // Slots proportional to how often each category was missed, at least one each
    QVector<QPair<QString, int>> order;
    for(auto it = byCategory.constBegin(); it != byCategory.constEnd(); ++it)
    {
        int available = it.value().size();
        if(it.key() == chordCategory)
            available = qMin(available, qMax(1, maxPuzzles / 4));
        order.append(qMakePair(it.key(), available));
    }
    std::stable_sort(order.begin(), order.end(), [](const QPair<QString, int>& a, const QPair<QString, int>& b)
                     { return a.second > b.second; });

    int availableTotal = 0;
    for(const auto& c : order)
        availableTotal += c.second;
    const int total = qMin(maxPuzzles, availableTotal);
    QMap<QString, int> quota;
    int used = 0;
    for(const auto& c : order)
    {
        if(used >= total)
            break;
        quota[c.first] = 1;
        ++used;
    }
    const int weightSum = qMax(1, availableTotal);
    for(const auto& c : order)
    {
        if(used >= total)
            break;
        if(!quota.contains(c.first))
            continue;
        const int wanted = qMax(1, int(std::round(double(total) * c.second / weightSum)));
        const int extra = qMin(wanted - quota[c.first], qMin(c.second - quota[c.first], total - used));
        if(extra > 0)
        {
            quota[c.first] += extra;
            used += extra;
        }
    }
    // Fill what is left with the most missed categories
    for(int pass = 0; used < total && pass < total; ++pass)
    {
        for(const auto& c : order)
        {
            if(used >= total)
                break;
            if(quota.value(c.first) < c.second)
            {
                quota[c.first]++;
                used++;
            }
        }
    }

    // Interleave categories so the same pattern does not repeat back to back
    QVector<TrainerPuzzle> result;
    QMap<QString, int> taken;
    while(result.size() < total)
    {
        bool added = false;
        for(const auto& c : order)
        {
            const int k = taken.value(c.first);
            if(k < quota.value(c.first))
            {
                result.append(byCategory[c.first][k]);
                taken[c.first] = k + 1;
                added = true;
                if(result.size() >= total)
                    break;
            }
        }
        if(!added)
            break;
    }
    return result;
}

bool TrainerHistory::checkAnswer(const TrainerRecording& recording, const TrainerPuzzle& puzzle,
                                 int cell, QString* feedback)
{
    auto say = [feedback](const QString& text) { if(feedback) *feedback = text; };

    TrainerBoard board(recording);
    board.restore(puzzle.snapshot);
    const QRect shown = puzzle.view.isNull() ? puzzle.area : puzzle.view;
    if(cell < 0 || cell >= board.value.size() ||
       !shown.contains(board.xOf(cell), board.yOf(cell)))
    {
        say(QStringLiteral("Click a cell inside the puzzle."));
        return false;
    }
    const QString name = TrainerAnalyzer::cellName(cell, board.width);

    if(puzzle.type == TrainerPuzzle::Type::SafeCell)
    {
        if(board.revealed[cell])
        {
            say(QStringLiteral("%1 is already open. Click a hidden cell.").arg(name));
            return false;
        }
        if(board.marks[cell] != TrainerBoard::Mark::None)
        {
            say(QStringLiteral("%1 is flagged. Click an unflagged hidden cell.").arg(name));
            return false;
        }
        const QHash<int, TrainerDeduction> inArea = TrainerSolver::deduceInArea(board, puzzle.area);
        if(inArea.contains(cell) && !inArea.value(cell).mine)
        {
            if(puzzle.deduction.group.contains(cell))
                say(QStringLiteral("Correct, %1 is safe.").arg(name));
            else
                say(QStringLiteral("Correct, %1 is safe too (%2). The pattern this puzzle trains is shown below.")
                    .arg(name, TrainerSolver::kindName(inArea.value(cell).kind)));
            return true;
        }
        if(board.isMine(cell))
            say(QStringLiteral("Boom: %1 is a mine.").arg(name));
        else
            say(QStringLiteral("%1 happens to be safe, but the numbers do not prove it: that was a guess.").arg(name));
        return false;
    }

    // Best chord
    if(!board.revealed[cell] || board.value[cell] <= 0)
    {
        say(QStringLiteral("Click a number to chord on."));
        return false;
    }
    if(board.flagsAround(cell) != board.value[cell])
    {
        say(QStringLiteral("The %1 at %2 does not have all its flags, so that chord would be rejected.")
            .arg(board.value[cell]).arg(name));
        return false;
    }
    bool boom = false;
    const int opens = board.chordPreview(cell, &boom).size();
    if(boom)
    {
        say(QStringLiteral("A flag around %1 is wrong, so that chord would hit a mine.").arg(name));
        return false;
    }
    if(opens >= puzzle.chordOpens)
    {
        say(QStringLiteral("Correct: chording %1 opens %2 cell(s).").arg(name).arg(opens));
        return true;
    }
    say(QStringLiteral("Chording %1 opens %2 cell(s); %3 opens %4.")
        .arg(name).arg(opens).arg(TrainerAnalyzer::cellName(puzzle.chordCell, board.width)).arg(puzzle.chordOpens));
    return false;
}

QString TrainerHistorySummary::toText() const
{
    QString out;
    QTextStream ts(&out);
    ts << QString("Games analysed: %1 (%2 won)\n").arg(games).arg(wins);
    ts << QString("Average 3BV/s %1 | efficiency %2%\n")
          .arg(avgThreeBVPerSecond, 0, 'f', 2).arg(avgEfficiency, 0, 'f', 0);
    ts << QString("Cursor jumps %1 (%2 left a safe cell behind) | guesses %3 | wasted clicks %4 | savable %5\n")
          .arg(jumps).arg(missedPatterns).arg(guesses).arg(wastedClicks).arg(savableClicks);
    ts << "\nMost common mistakes:\n";
    for(const auto& m : mistakes)
        ts << QString("  %1: %2\n").arg(m.first).arg(m.second);
    ts << "\nMissed patterns:\n";
    for(const auto& m : missedByPattern)
        ts << QString("  %1: %2\n").arg(m.first).arg(m.second);
    ts << QString("\nPractice puzzles available: %1\n").arg(puzzleCandidates);
    return out;
}
