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

#include <QDateTime>

#include <cmath>

#include "trainerspeed.h"

namespace
{
const int BoardWidth = 30;   // Expert
const int BoardHeight = 16;
const int BoardMines = 99;
const int Attempts = 40;
}

TrainerSpeed::TrainerSpeed(quint32 seed) :
    rng(seed == 0 ? QRandomGenerator::global()->generate() : seed)
{
}

QRect TrainerSpeed::windowAround(int cell, int width, int height, int radius)
{
    const int x = cell % width, y = cell / width;
    return QRect(QPoint(qMax(0, x - radius), qMax(0, y - radius)),
                 QPoint(qMin(width - 1, x + radius), qMin(height - 1, y + radius)));
}

bool TrainerSpeed::randomPosition(TrainerSpeedPosition& pos, int maxProgress, bool flagProvable)
{
    TrainerRecording r;
    r.width = BoardWidth;
    r.height = BoardHeight;
    r.mines = BoardMines;
    r.startedAt = QDateTime::currentDateTime();
    r.mineLayout = QVector<bool>(BoardWidth*BoardHeight, false);

    // Safe opening around a random start cell, like the first click of a game
    const int start = rng.bounded(BoardWidth*BoardHeight);
    const int sx = start % BoardWidth, sy = start / BoardWidth;
    for(int placed = 0; placed < BoardMines;)
    {
        const int c = rng.bounded(BoardWidth*BoardHeight);
        if(r.mineLayout[c] || (qAbs(c % BoardWidth - sx) <= 1 && qAbs(c / BoardWidth - sy) <= 1))
            continue;
        r.mineLayout[c] = true;
        ++placed;
    }

    TrainerBoard board(r);
    board.reveal(start);

    // Play a random number of safe moves to reach varied mid-game frontiers
    const int steps = rng.bounded(maxProgress + 1);
    for(int s = 0; s < steps; ++s)
    {
        const QHash<int, TrainerDeduction> ded = TrainerSolver::deduce(board, false);
        QVector<int> safe;
        for(auto it = ded.constBegin(); it != ded.constEnd(); ++it)
        {
            if(!it->mine && !board.revealed[it.key()])
                safe.append(it.key());
        }
        if(safe.isEmpty())
            break;
        // Reveal about half of what is provable: progress fast, keep variety
        for(int c : safe)
        {
            if(rng.bounded(2) == 0)
                board.reveal(c);
        }
    }

    // Flags as a player would have them: provable mines, all or about half
    const QHash<int, TrainerDeduction> ded = TrainerSolver::deduce(board, false);
    for(auto it = ded.constBegin(); it != ded.constEnd(); ++it)
    {
        if(it->mine && (flagProvable || rng.bounded(2) == 0))
            board.toggleMark(it.key());
    }

    if(board.revealedCount() < 10)
        return false;
    pos.recording = r;
    pos.snapshot = board.snapshot();
    return true;
}

bool TrainerSpeed::nextSafePuzzle(TrainerSpeedPosition& pos, TrainerPuzzle& puzzle)
{
    for(int attempt = 0; attempt < Attempts; ++attempt)
    {
        if(!randomPosition(pos, 6, false))
            continue;
        TrainerBoard board(pos.recording);
        board.restore(pos.snapshot);

        const QHash<int, TrainerDeduction> ded = TrainerSolver::deduce(board, false);
        QVector<int> patterns, others;
        for(auto it = ded.constBegin(); it != ded.constEnd(); ++it)
        {
            if(it->mine || board.revealed[it.key()] || board.marks[it.key()] != TrainerBoard::Mark::None)
                continue;
            (it->kind == TrainerDeduction::Kind::Pattern ? patterns : others).append(it.key());
        }
        // Prefer real patterns most of the time, they are what needs training
        const QVector<int>& pool = (!patterns.isEmpty() && (others.isEmpty() || rng.bounded(10) < 7))
                                   ? patterns : others;
        if(pool.isEmpty())
            continue;
        const int cell = pool[rng.bounded(int(pool.size()))];

        const QRect area = windowAround(cell, BoardWidth, BoardHeight, 2);
        const QHash<int, TrainerDeduction> inArea = TrainerSolver::deduceInArea(board, area);
        if(!inArea.contains(cell) || inArea.value(cell).mine)
            continue;

        puzzle = TrainerPuzzle();
        puzzle.type = TrainerPuzzle::Type::SafeCell;
        puzzle.snapshot = pos.snapshot;
        puzzle.area = area;
        puzzle.view = area.adjusted(-1, -1, 1, 1).intersected(QRect(0, 0, BoardWidth, BoardHeight));
        puzzle.deduction = inArea.value(cell);
        puzzle.prerequisites = TrainerSolver::prerequisitesOf(inArea, puzzle.deduction);
        puzzle.category = TrainerHistory::categoryOf(puzzle.deduction);
        return true;
    }
    return false;
}

bool TrainerSpeed::nextChordPuzzle(TrainerSpeedPosition& pos, TrainerPuzzle& puzzle)
{
    for(int attempt = 0; attempt < Attempts; ++attempt)
    {
        if(!randomPosition(pos, 6, true))
            continue;
        TrainerBoard board(pos.recording);
        board.restore(pos.snapshot);

        auto chordable = [&board](int c)
        {
            if(!board.revealed[c] || board.value[c] <= 0 || board.flagsAround(c) != board.value[c] ||
               board.wrongFlagsAround(c) > 0)
                return 0;
            return static_cast<int>(board.chordPreview(c).size());
        };

        QVector<int> centres;
        for(int c = 0; c < board.value.size(); ++c)
        {
            if(chordable(c) > 0)
                centres.append(c);
        }
        if(centres.isEmpty())
            continue;
        const int centre = centres[rng.bounded(int(centres.size()))];
        const QRect area = windowAround(centre, BoardWidth, BoardHeight, 2);

        // The answer is the best chord inside the area; insist on a real choice
        int best = -1, bestOpens = 0, options = 0;
        for(int y = area.top(); y <= area.bottom(); ++y)
        {
            for(int x = area.left(); x <= area.right(); ++x)
            {
                const int c = y*BoardWidth + x;
                const int opens = chordable(c);
                if(opens <= 0)
                    continue;
                ++options;
                if(opens > bestOpens)
                {
                    best = c;
                    bestOpens = opens;
                }
            }
        }
        if(best < 0 || (options < 2 && attempt < Attempts - 5))
            continue;

        puzzle = TrainerPuzzle();
        puzzle.type = TrainerPuzzle::Type::BestChord;
        puzzle.snapshot = pos.snapshot;
        puzzle.area = area;
        puzzle.view = area.adjusted(-1, -1, 1, 1).intersected(QRect(0, 0, BoardWidth, BoardHeight));
        puzzle.chordCell = best;
        puzzle.chordOpens = bestOpens;
        puzzle.category = QStringLiteral("best chord square");
        return true;
    }
    return false;
}

bool TrainerSpeed::nextFlagChordPuzzle(TrainerSpeedPosition& pos, TrainerPuzzle& puzzle)
{
    for(int attempt = 0; attempt < Attempts; ++attempt)
    {
        // No flags yet: the drill is placing them
        if(!randomPosition(pos, 6, false))
            continue;
        TrainerBoard board(pos.recording);
        board.restore(pos.snapshot);
        for(int c = 0; c < board.value.size(); ++c)
        {
            if(board.marks[c] != TrainerBoard::Mark::None)
                board.toggleMark(c);
        }
        pos.snapshot = board.snapshot();

        // A number whose mines are all provable and that still opens something
        QVector<int> candidates;
        for(int c = 0; c < board.value.size(); ++c)
        {
            if(!board.revealed[c] || board.value[c] <= 0)
                continue;
            const QRect area = windowAround(c, BoardWidth, BoardHeight, 2);
            const QHash<int, TrainerDeduction> inArea = TrainerSolver::deduceInArea(board, area, false);
            int mines = 0, safe = 0;
            bool allKnown = true;
            for(int nb : board.neighbours(c))
            {
                if(board.revealed[nb])
                    continue;
                if(!inArea.contains(nb))
                {
                    allKnown = false;
                    break;
                }
                if(inArea.value(nb).mine)
                    ++mines;
                else
                    ++safe;
            }
            if(allKnown && mines > 0 && safe > 0)
                candidates.append(c);
            if(candidates.size() >= 12)
                break;
        }
        if(candidates.isEmpty())
            continue;

        const int number = candidates[rng.bounded(int(candidates.size()))];
        puzzle = TrainerPuzzle();
        puzzle.type = TrainerPuzzle::Type::FlagAndChord;
        puzzle.snapshot = pos.snapshot;
        puzzle.area = windowAround(number, BoardWidth, BoardHeight, 2);
        puzzle.view = puzzle.area.adjusted(-1, -1, 1, 1).intersected(QRect(0, 0, BoardWidth, BoardHeight));
        puzzle.chordCell = number;
        for(int nb : board.neighbours(number))
        {
            if(!board.revealed[nb] && board.isMine(nb))
                puzzle.mustFlag.append(nb);
        }
        puzzle.chordOpens = 0;
        for(int nb : board.neighbours(number))
        {
            if(!board.revealed[nb] && !board.isMine(nb))
                ++puzzle.chordOpens;
        }
        puzzle.category = QStringLiteral("flag and chord");
        return true;
    }
    return false;
}

int TrainerSpeed::flickScore(int hits, int misses, double avgMs)
{
    if(hits <= 0 || avgMs <= 0)
        return 0;
    const double accuracy = double(hits) / (hits + misses);
    return int(std::lround(1000.0 * accuracy / (avgMs / 1000.0)));
}

int TrainerSpeed::sprintScore(int correct, int mistakes)
{
    return correct*100 - mistakes*50;
}
