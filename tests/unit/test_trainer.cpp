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

#include <QtTest/QtTest>
#include <QRandomGenerator>
#include <QTemporaryDir>

#include "traineranalyzer.h"
#include "trainerrecording.h"
#include "trainerhistory.h"
#include "trainerspeed.h"
#include <QElapsedTimer>

class TestTrainer : public QObject
{
    Q_OBJECT

private:
    static void act(TrainerRecording& r, TrainerAction::Type t, int x, int y, qint64 ms)
    {
        TrainerAction a;
        a.type = t;
        a.x = x;
        a.y = y;
        a.tMs = ms;
        r.actions.append(a);
    }

    static QVector<TrainerFinding> findingsOf(const TrainerReport& rep, TrainerFinding::Type t)
    {
        QVector<TrainerFinding> out;
        for(const TrainerFinding& f : rep.findings)
        {
            if(f.type == t)
                out.append(f);
        }
        return out;
    }

private slots:
    void testBoardValuesAndCascade();
    void testThreeBV();
    void testDeduce121Pattern();
    void testDeduce11Pattern();
    void testFilledNumber();
    void testEnumerationFindsHarderCells();
    void testDeductionsAreSoundOnRandomBoards();
    void testDeductionDetailsAndExplanation();
    void testFindingCarriesDeduction();
    void testDepthIsLayered();
    void testFlagsCountAsKnownOnlyWhenProvable();
    void testLocalWindowIgnoresFarInformation();
    void testHistorySummaryAndPlaylist();
    void testSpeedPuzzlesAreValid();
    void testCursorJumpPointsAtMissedCell();
    void testShortMoveIsNotAJump();
    void testClicksCouldBeChord();
    void testRejectedAndDominatedChord();
    void testUnnecessaryGuessAndWrongFlag();
    void testJsonRoundTrip();
};

void TestTrainer::testBoardValuesAndCascade()
{
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        "*...\n"
        "....\n"
        "....\n");
    TrainerBoard b(r);
    QCOMPARE(b.value[b.index(1, 0)], qint8(1));
    QCOMPARE(b.value[b.index(1, 1)], qint8(1));
    QCOMPARE(b.value[b.index(3, 2)], qint8(0));

    const TrainerBoard::Result res = b.reveal(b.index(3, 2));
    QCOMPARE(res.opened.size(), 11); // everything but the mine
    QVERIFY(b.isWon());
}

void TestTrainer::testThreeBV()
{
    // One opening plus nothing else: 3BV = 1
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        "*...\n"
        "....\n"
        "....\n");
    QCOMPARE(TrainerBoard(r).threeBV(), 1);

    // No zero cells: every safe cell is one click
    TrainerRecording r2 = TrainerRecording::fromMinefieldString(
        "*.*\n"
        ".*.\n");
    QCOMPARE(TrainerBoard(r2).threeBV(), 3);
}

void TestTrainer::testDeduce121Pattern()
{
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        ".*.*.\n"
        ".....\n"
        ".....\n");
    TrainerBoard b(r);
    b.reveal(b.index(2, 2)); // opens rows 1 and 2: 1 1 2 1 1 over zeros

    const QHash<int, TrainerDeduction> d = TrainerSolver::deduce(b, false);
    QVERIFY(d.contains(b.index(3, 0)));
    QVERIFY(d.value(b.index(3, 0)).mine);
    QCOMPARE(d.value(b.index(3, 0)).kind, TrainerDeduction::Kind::Pattern);
    QCOMPARE(d.value(b.index(3, 0)).pattern, QString("1-2-1"));
    QVERIFY(d.contains(b.index(1, 0)));
    QVERIFY(d.value(b.index(1, 0)).mine);
    QVERIFY(d.contains(b.index(2, 0)));
    QVERIFY(!d.value(b.index(2, 0)).mine);
    QVERIFY(d.contains(b.index(0, 0)));
    QVERIFY(!d.value(b.index(0, 0)).mine);
    QVERIFY(d.contains(b.index(4, 0)));
    QVERIFY(!d.value(b.index(4, 0)).mine);
}

void TestTrainer::testDeduce11Pattern()
{
    // Wall on the left: 1 1 at the start of the row proves the third cell safe
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        ".*..*\n"
        ".....\n"
        ".....\n");
    TrainerBoard b(r);
    b.reveal(b.index(2, 2));

    const QHash<int, TrainerDeduction> d = TrainerSolver::deduce(b, false);
    const int third = b.index(2, 0);
    QVERIFY(d.contains(third));
    QVERIFY(!d.value(third).mine);
    QCOMPARE(d.value(third).pattern, QString("1-1"));
}

void TestTrainer::testFilledNumber()
{
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        "*.\n"
        "..\n");
    TrainerBoard b(r);
    QVERIFY(TrainerSolver::deduce(b).isEmpty());

    // A lone 1 with three hidden neighbours proves nothing
    b.reveal(b.index(1, 1));
    QVERIFY(TrainerSolver::deduce(b).isEmpty());

    // Once only one hidden neighbour is left it must be the mine
    b.reveal(b.index(1, 0));
    b.reveal(b.index(0, 1));
    const QHash<int, TrainerDeduction> d = TrainerSolver::deduce(b);
    QVERIFY(d.contains(0));
    QVERIFY(d.value(0).mine);
    QCOMPARE(d.value(0).kind, TrainerDeduction::Kind::FilledNumber);
}

void TestTrainer::testEnumerationFindsHarderCells()
{
    // Pair rules alone cannot finish this frontier; enumeration must not
    // contradict them and must only add proven cells
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        "..*..*\n"
        "......\n"
        "......\n"
        "*....*\n");
    TrainerBoard b(r);
    b.reveal(b.index(2, 2));
    const QHash<int, TrainerDeduction> basic = TrainerSolver::deduce(b, false);
    const QHash<int, TrainerDeduction> full = TrainerSolver::deduce(b, true);
    QVERIFY(full.size() >= basic.size());
    for(auto it = full.constBegin(); it != full.constEnd(); ++it)
        QCOMPARE(it.value().mine, b.isMine(it.key()));
}

void TestTrainer::testDeductionsAreSoundOnRandomBoards()
{
    QRandomGenerator rng(12345);
    int enumerationOnly = 0;
    int patterns = 0;
    for(int round=0; round<300; ++round)
    {
        const int w = 16, h = 16, mines = 40;
        TrainerRecording r;
        r.width = w;
        r.height = h;
        r.mines = mines;
        r.mineLayout = QVector<bool>(w*h, false);
        for(int placed=0; placed<mines;)
        {
            const int c = rng.bounded(w*h);
            if(!r.mineLayout[c])
            {
                r.mineLayout[c] = true;
                ++placed;
            }
        }

        TrainerBoard b(r);
        // Reveal a handful of random safe cells to get varied frontiers
        for(int k=0; k<6; ++k)
        {
            const int c = rng.bounded(w*h);
            if(!b.isMine(c))
                b.reveal(c);
        }

        const QHash<int, TrainerDeduction> d = TrainerSolver::deduce(b, true);
        for(auto it = d.constBegin(); it != d.constEnd(); ++it)
        {
            QVERIFY2(it.value().mine == b.isMine(it.key()),
                     qPrintable(QString("round %1 cell %2 kind %3")
                                .arg(round).arg(it.key()).arg(int(it.value().kind))));
            QVERIFY(!b.revealed[it.key()]);
            if(it.value().kind == TrainerDeduction::Kind::Enumeration)
                ++enumerationOnly;
            if(it.value().kind == TrainerDeduction::Kind::Pattern)
                ++patterns;
        }
    }
    QVERIFY(patterns > 0);
    QVERIFY(enumerationOnly > 0);
}

void TestTrainer::testDeductionDetailsAndExplanation()
{
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        ".*.*.\n"
        ".....\n"
        ".....\n");
    TrainerBoard b(r);
    b.reveal(b.index(2, 2));

    const QHash<int, TrainerDeduction> d = TrainerSolver::deduce(b, false);
    const TrainerDeduction mine = d.value(b.index(3, 0));
    QCOMPARE(mine.kind, TrainerDeduction::Kind::Pattern);
    QVERIFY(mine.group.contains(b.index(3, 0)));
    QVERIFY(!mine.shared.isEmpty());
    for(int c : mine.shared)
        QVERIFY(mine.region.contains(c));
    for(int c : mine.group)
        QVERIFY(!mine.shared.contains(c));

    const QStringList steps = TrainerSolver::explainSteps(b, mine);
    QVERIFY(steps.size() >= 5);
    QVERIFY(steps.join(' ').contains("(4,1)"));      // the proven cell is named
    QVERIFY(steps.join(' ').contains("a mine"));

    // Every kind produces an explanation
    for(auto it = d.constBegin(); it != d.constEnd(); ++it)
        QVERIFY(!TrainerSolver::explainSteps(b, it.value()).isEmpty());

    // Satisfied number: the known mines are recorded
    // (0,1) has only (0,0) hidden -> mine; then (1,0) is satisfied -> (2,0) safe
    TrainerRecording r2 = TrainerRecording::fromMinefieldString(
        "*..\n"
        "...\n");
    TrainerBoard b2(r2);
    b2.reveal(b2.index(1, 0));
    b2.reveal(b2.index(0, 1));
    b2.reveal(b2.index(1, 1));
    const QHash<int, TrainerDeduction> d1 = TrainerSolver::deduce(b2, false);
    QVERIFY(d1.value(0).mine);
    QCOMPARE(d1.value(0).kind, TrainerDeduction::Kind::FilledNumber);
    // Without a flag the shortest route to (2,0) is the 1-1 pattern
    QCOMPARE(d1.value(b2.index(2, 0)).kind, TrainerDeduction::Kind::Pattern);
    // With the mine flagged, the 1 at (1,0) is simply satisfied
    b2.toggleMark(0);
    const QHash<int, TrainerDeduction> d2 = TrainerSolver::deduce(b2, false);
    const TrainerDeduction safe = d2.value(b2.index(2, 0));
    QVERIFY(!safe.mine);
    QCOMPARE(safe.kind, TrainerDeduction::Kind::SatisfiedNumber);
    QVERIFY(safe.knownMines.contains(0));
}

void TestTrainer::testFindingCarriesDeduction()
{
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        ".*.*.....*.*\n"
        "............\n"
        "............\n");
    act(r, TrainerAction::Type::Reveal, 2, 2, 0);
    act(r, TrainerAction::Type::Reveal, 10, 0, 1500);

    const TrainerReport rep = TrainerAnalyzer::analyze(r);
    const QVector<TrainerFinding> jumps = findingsOf(rep, TrainerFinding::Type::CursorJump);
    QCOMPARE(jumps.size(), 1);
    QVERIFY(jumps.first().hasDeduction);
    QVERIFY(!jumps.first().deduction.mine); // jumps only ever point at safe cells
    QCOMPARE(jumps.first().deduction.cell, jumps.first().suggestedCell);
    QVERIFY(!jumps.first().deduction.group.isEmpty());
}

void TestTrainer::testDepthIsLayered()
{
    QRandomGenerator rng(777);
    int deep = 0;
    for(int round=0; round<200; ++round)
    {
        const int w = 16, h = 16, mines = 40;
        TrainerRecording r;
        r.width = w;
        r.height = h;
        r.mines = mines;
        r.mineLayout = QVector<bool>(w*h, false);
        for(int placed=0; placed<mines;)
        {
            const int c = rng.bounded(w*h);
            if(!r.mineLayout[c]) { r.mineLayout[c] = true; ++placed; }
        }
        TrainerBoard b(r);
        for(int k=0; k<6; ++k)
        {
            const int c = rng.bounded(w*h);
            if(!b.isMine(c))
                b.reveal(c);
        }

        const QHash<int, TrainerDeduction> d = TrainerSolver::deduce(b);
        for(auto it = d.constBegin(); it != d.constEnd(); ++it)
        {
            const TrainerDeduction& x = it.value();
            QVERIFY(x.depth >= 1); // no flags on these boards
            for(int s : x.support)
            {
                QVERIFY(d.contains(s));
                QVERIFY2(d.value(s).depth < x.depth, "a step may only use earlier rounds");
            }
            const QVector<TrainerDeduction> pre = TrainerSolver::prerequisitesOf(d, x);
            for(int i=1; i<pre.size(); ++i)
                QVERIFY(pre[i-1].depth <= pre[i].depth);
            if(x.depth >= 2)
            {
                ++deep;
                QVERIFY(!pre.isEmpty());
            }
        }
    }
    QVERIFY(deep > 0);
}

void TestTrainer::testFlagsCountAsKnownOnlyWhenProvable()
{
    // (0,1) only touches (0,0): the mine is provable
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        "*..\n"
        "...\n");
    TrainerBoard b(r);
    b.reveal(b.index(1, 0));
    b.reveal(b.index(0, 1));
    b.reveal(b.index(1, 1));

    TrainerBoard flagged = b;
    flagged.toggleMark(0);
    const QHash<int, TrainerDeduction> d = TrainerSolver::deduce(flagged);
    QCOMPARE(d.value(0).depth, 0);                 // your flag is known
    QCOMPARE(d.value(b.index(2, 0)).depth, 1);     // so this is one step away

    // A flag on a safe cell is not knowledge: same result as without it
    TrainerBoard wrong = b;
    wrong.toggleMark(b.index(2, 1));
    const QHash<int, TrainerDeduction> dw = TrainerSolver::deduce(wrong);
    const QHash<int, TrainerDeduction> dn = TrainerSolver::deduce(b);
    QCOMPARE(dw.size(), dn.size());
    for(auto it = dn.constBegin(); it != dn.constEnd(); ++it)
    {
        QVERIFY(dw.contains(it.key()));
        QCOMPARE(dw.value(it.key()).mine, it.value().mine);
        QCOMPARE(dw.value(it.key()).depth, it.value().depth);
    }
}

void TestTrainer::testLocalWindowIgnoresFarInformation()
{
    // A row of 1s along a wall: only the two ends pin the mines down, the
    // middle is solved by walking inwards from far away
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        ".*..*..*.\n"
        ".........\n");
    TrainerBoard b(r);
    for(int x=0; x<9; ++x)
        b.reveal(b.index(x, 1));
    const int middleMine = b.index(4, 0);

    const QHash<int, TrainerDeduction> global = TrainerSolver::deduce(b);
    QVERIFY(global.contains(middleMine));
    QVERIFY(global.value(middleMine).mine);

    // Looking only at the numbers right around it is not enough
    const QHash<int, TrainerDeduction> local = TrainerSolver::deduce(b, true, b.index(4, 1), 1);
    QVERIFY(!local.contains(middleMine));

    // Any depth is fine as long as the information is close: the edge cell
    // is solvable from the numbers next to it
    const QHash<int, TrainerDeduction> edge = TrainerSolver::deduce(b, true, b.index(1, 1), 1);
    QVERIFY(edge.contains(b.index(2, 0)));
    QVERIFY(!edge.value(b.index(2, 0)).mine);

    // Every local deduction agrees with the truth
    for(auto it = local.constBegin(); it != local.constEnd(); ++it)
        QCOMPARE(it.value().mine, b.isMine(it.key()));
}

void TestTrainer::testHistorySummaryAndPlaylist()
{
    // Two games with the same missed pattern (a jump away from (2,0))
    TrainerHistory history;
    for(int g=0; g<2; ++g)
    {
        TrainerRecording r = TrainerRecording::fromMinefieldString(
            ".*.*.....*.*\n"
            "............\n"
            "............\n");
        act(r, TrainerAction::Type::Reveal, 2, 2, 0);
        act(r, TrainerAction::Type::Reveal, 10, 0, 1500);
        history.addGame(QString("game%1.json").arg(g), r, TrainerAnalyzer::analyze(r));
    }
    QCOMPARE(history.gameCount(), 2);

    const TrainerHistorySummary s = history.summary();
    QCOMPARE(s.games, 2);
    QCOMPARE(s.missedPatterns, 2);
    QVERIFY(!s.missedByPattern.isEmpty());
    QCOMPARE(s.missedByPattern.first().second, 2);
    QVERIFY(s.toText().contains("Missed patterns"));

    const QVector<TrainerPuzzle> list = history.playlist(10);
    QCOMPARE(list.size(), 2);
    const TrainerPuzzle& p = list.first();
    QCOMPARE(p.type, TrainerPuzzle::Type::SafeCell);
    QVERIFY(p.area.contains(2, 0));

    const TrainerRecording& rec = history.recording(p.game);
    TrainerBoard b(rec);
    QString why;
    QVERIFY2(TrainerHistory::checkAnswer(rec, p, b.index(2, 0), &why), qPrintable(why));
    QVERIFY(!TrainerHistory::checkAnswer(rec, p, b.index(1, 0), &why)); // a mine
    QVERIFY(why.contains("mine"));
    QVERIFY(!TrainerHistory::checkAnswer(rec, p, b.index(2, 2), &why)); // already open
    QVERIFY(why.contains("already open"));

    // The playlist never exceeds what was asked for
    QCOMPARE(history.playlist(1).size(), 1);
}

void TestTrainer::testSpeedPuzzlesAreValid()
{
    TrainerSpeed speed(4242);
    QElapsedTimer clock;
    clock.start();
    for(int i=0; i<10; ++i)
    {
        TrainerSpeedPosition pos;
        TrainerPuzzle p;

        QVERIFY(speed.nextSafePuzzle(pos, p));
        QCOMPARE(p.type, TrainerPuzzle::Type::SafeCell);
        QVERIFY(!pos.recording.isMine(p.deduction.cell % pos.recording.width,
                                      p.deduction.cell / pos.recording.width));
        QString why;
        QVERIFY2(TrainerHistory::checkAnswer(pos.recording, p, p.deduction.cell, &why), qPrintable(why));

        QVERIFY(speed.nextChordPuzzle(pos, p));
        QCOMPARE(p.type, TrainerPuzzle::Type::BestChord);
        QVERIFY2(TrainerHistory::checkAnswer(pos.recording, p, p.chordCell, &why), qPrintable(why));

        QVERIFY(speed.nextFlagChordPuzzle(pos, p));
        QCOMPARE(p.type, TrainerPuzzle::Type::FlagAndChord);
        TrainerBoard b(pos.recording);
        b.restore(p.snapshot);
        QVERIFY(!p.mustFlag.isEmpty());
        for(int m : p.mustFlag)
        {
            QVERIFY(b.isMine(m));
            QVERIFY(b.neighbours(p.chordCell).contains(m));
            b.toggleMark(m);
        }
        QCOMPARE(b.flagsAround(p.chordCell), int(b.value[p.chordCell]));
        bool boom = true;
        const QVector<int> opens = b.chordPreview(p.chordCell, &boom);
        QVERIFY(!boom);
        QCOMPARE(int(opens.size()) > 0, true);
    }
    // 30 puzzles must be quick enough for a live sprint
    qDebug() << "speed puzzle generation:" << clock.elapsed() << "ms for 30 puzzles";
    QVERIFY(clock.elapsed() < 30000);

    QCOMPARE(TrainerSpeed::sprintScore(10, 2), 900);
    QCOMPARE(TrainerSpeed::flickScore(30, 0, 500), 2000);
    QCOMPARE(TrainerSpeed::flickScore(0, 5, 500), 0);
}

void TestTrainer::testCursorJumpPointsAtMissedCell()
{
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        ".*.*.....*.*\n"
        "............\n"
        "............\n");
    act(r, TrainerAction::Type::Reveal, 2, 2, 0);
    act(r, TrainerAction::Type::Reveal, 10, 0, 1500);

    const TrainerReport rep = TrainerAnalyzer::analyze(r);
    const QVector<TrainerFinding> jumps = findingsOf(rep, TrainerFinding::Type::CursorJump);
    QCOMPARE(jumps.size(), 1);
    QCOMPARE(jumps.first().actionIndex, 1);
    QCOMPARE(jumps.first().fromCell, 2*12 + 2);
    QCOMPARE(jumps.first().suggestedCell, 2); // (2,0) is safe and right next to the old spot
    QVERIFY(!jumps.first().clueCells.isEmpty());
    QCOMPARE(rep.stats.jumps, 1);
    QCOMPARE(rep.stats.jumpsWithMissedPattern, 1);
}

void TestTrainer::testShortMoveIsNotAJump()
{
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        ".*.*.\n"
        ".....\n"
        ".....\n");
    act(r, TrainerAction::Type::Reveal, 2, 2, 0);
    act(r, TrainerAction::Type::Reveal, 2, 0, 800);
    const TrainerReport rep = TrainerAnalyzer::analyze(r);
    QCOMPARE(rep.stats.jumps, 0);
    QVERIFY(findingsOf(rep, TrainerFinding::Type::CursorJump).isEmpty());
    QVERIFY(findingsOf(rep, TrainerFinding::Type::UnnecessaryGuess).isEmpty());
}

void TestTrainer::testClicksCouldBeChord()
{
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        "*.*.*\n"
        ".....\n"
        "*.*.*\n");
    act(r, TrainerAction::Type::Reveal, 2, 1, 0);    // the 2 in the middle
    act(r, TrainerAction::Type::Reveal, 1, 0, 300);
    act(r, TrainerAction::Type::Reveal, 3, 0, 600);
    act(r, TrainerAction::Type::Reveal, 1, 1, 900);
    act(r, TrainerAction::Type::Reveal, 3, 1, 1200);

    const TrainerReport rep = TrainerAnalyzer::analyze(r);
    const QVector<TrainerFinding> f = findingsOf(rep, TrainerFinding::Type::ClicksCouldBeChord);
    QCOMPARE(f.size(), 1);
    QCOMPARE(f.first().suggestedCell, 1*5 + 2);
    QCOMPARE(f.first().extraCells.size(), 4);
    QCOMPARE(f.first().wastedClicks, 1); // 4 clicks vs 2 flags + 1 chord
}

void TestTrainer::testRejectedAndDominatedChord()
{
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        ".*.*.\n"
        ".....\n"
        ".....\n"
        ".*.*.\n");
    act(r, TrainerAction::Type::Reveal, 2, 1, 0);
    act(r, TrainerAction::Type::Chord, 2, 1, 200);    // rejected: no flags yet
    act(r, TrainerAction::Type::Reveal, 2, 0, 400);
    act(r, TrainerAction::Type::Flag, 1, 0, 600);
    act(r, TrainerAction::Type::Flag, 3, 0, 800);
    act(r, TrainerAction::Type::Chord, 2, 0, 1000);   // opens 2 cells, the 2 below opens 5

    const TrainerReport rep = TrainerAnalyzer::analyze(r);
    QCOMPARE(findingsOf(rep, TrainerFinding::Type::RejectedChord).size(), 1);

    const QVector<TrainerFinding> dom = findingsOf(rep, TrainerFinding::Type::DominatedChord);
    QCOMPARE(dom.size(), 1);
    QCOMPARE(dom.first().actionCell, 2);
    QCOMPARE(dom.first().suggestedCell, 1*5 + 2);
    QCOMPARE(dom.first().extraCells.size(), 5);
    QCOMPARE(rep.openedPerAction.value(5), 2);
}

void TestTrainer::testUnnecessaryGuessAndWrongFlag()
{
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        ".*.*.\n"
        ".....\n"
        ".....\n"
        "*...*\n"
        "....*\n");
    act(r, TrainerAction::Type::Reveal, 2, 1, 0);   // a 2 with nothing else around: no proof
    act(r, TrainerAction::Type::Flag, 2, 0, 200);   // (2,0) is safe
    act(r, TrainerAction::Type::Reveal, 2, 3, 400); // opens the middle area
    act(r, TrainerAction::Type::Reveal, 4, 3, 900); // guess... onto a mine

    const TrainerReport rep = TrainerAnalyzer::analyze(r);
    QCOMPARE(findingsOf(rep, TrainerFinding::Type::WrongFlag).size(), 1);

    const QVector<TrainerFinding> g = findingsOf(rep, TrainerFinding::Type::UnnecessaryGuess);
    QCOMPARE(g.size(), 1);
    QCOMPARE(g.first().actionIndex, 3);
    QCOMPARE(g.first().severity, 3);
    QVERIFY(g.first().suggestedCell >= 0);
}

void TestTrainer::testJsonRoundTrip()
{
    TrainerRecording r = TrainerRecording::fromMinefieldString(
        ".*.\n"
        "...\n");
    r.won = true;
    r.durationMs = 1234;
    act(r, TrainerAction::Type::Reveal, 0, 1, 10);
    act(r, TrainerAction::Type::Flag, 1, 0, 20);
    act(r, TrainerAction::Type::Chord, 0, 1, 30);
    TrainerCursorSample s;
    s.tMs = 5; s.x = 0.5f; s.y = 1.25f;
    r.cursor.append(s);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("game.json");
    QVERIFY(r.save(path));

    TrainerRecording loaded;
    QString error;
    QVERIFY2(TrainerRecording::load(path, loaded, &error), qPrintable(error));
    QCOMPARE(loaded.width, 3);
    QCOMPARE(loaded.height, 2);
    QCOMPARE(loaded.mines, 1);
    QVERIFY(loaded.won);
    QCOMPARE(loaded.durationMs, qint64(1234));
    QVERIFY(loaded.isMine(1, 0));
    QCOMPARE(loaded.actions.size(), 3);
    QCOMPARE(loaded.actions[2].type, TrainerAction::Type::Chord);
    QCOMPARE(loaded.cursor.size(), 1);
    QCOMPARE(loaded.cursor[0].y, 1.25f);
}

QTEST_MAIN(TestTrainer)
#include "test_trainer.moc"
