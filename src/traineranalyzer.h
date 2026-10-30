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
 * @file traineranalyzer.h
 * @brief Replay, deduction and efficiency analysis for recorded games
 * @author LibreMines Trainer contributors
 * @date 2026-10-30
 *
 * This file contains the pure (widget free) part of the trainer:
 * - TrainerBoard replays a TrainerRecording with the same rules as
 *   LibreMinesGameEngine (reveal, zero cascade, flag cycle, chording).
 * - TrainerSolver finds every cell that can be proven safe or mined from the
 *   numbers visible on a board and labels the reasoning that proves it
 *   (satisfied number, 1-1 / 1-2 / 1-2-1 / 1-2-2-1 patterns, or exhaustive
 *   enumeration).
 * - TrainerAnalyzer walks through the recording and reports inefficient
 *   play: cursor jumps that leave a provable cell next to the cursor,
 *   rejected / wasted / dominated chords, click sequences a single chord
 *   could replace, unnecessary guesses and wrong flags.
 */

#ifndef TRAINERANALYZER_H
#define TRAINERANALYZER_H

#include <QByteArray>
#include <QHash>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

#include "trainerrecording.h"

/**
 * @brief Minimal minesweeper board able to replay recorded actions
 *
 * Mirrors the rules of LibreMinesGameEngine so that replaying a recording
 * reproduces exactly what the player saw.
 *
 * @since trainer-v1
 */
class TrainerBoard
{
public:
    /**
     * @brief Flag state of a hidden cell
     */
    enum class Mark : quint8
    {
        None = 0,     ///< No mark
        Flag = 1,     ///< Real flag (counts for chording)
        Question = 2  ///< Question mark (ignored by chording)
    };

    /**
     * @brief Outcome of applying one action
     */
    struct Result
    {
        QVector<int> opened;    ///< Cells revealed by the action (including cascade)
        bool exploded = false;  ///< A mine was revealed
        bool rejected = false;  ///< Chord refused because flags != number
        bool noEffect = false;  ///< The action changed nothing
    };

    TrainerBoard() = default;

    /**
     * @brief Create an untouched board for the recording's mine layout
     */
    explicit TrainerBoard(const TrainerRecording& recording);

    int width = 0;             ///< Columns
    int height = 0;            ///< Rows
    bool useQuestionMark = false; ///< Flag cycle includes a question mark
    QVector<qint8> value;      ///< -1 for mines, otherwise 0..8
    QVector<bool> revealed;    ///< Revealed cells
    QVector<Mark> marks;       ///< Player marks on hidden cells
    bool exploded = false;     ///< A mine was revealed

    int index(int x, int y) const { return y*width + x; }
    int xOf(int i) const { return i % width; }
    int yOf(int i) const { return i / width; }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
    bool isMine(int i) const { return value[i] < 0; }

    /**
     * @brief Indices of the up to 8 neighbours of cell i
     */
    QVector<int> neighbours(int i) const;

    /**
     * @brief Apply a recorded action and report what changed
     */
    Result apply(const TrainerAction& action);

    /**
     * @brief Reveal one cell with the engine's semantics
     */
    Result reveal(int i);

    /**
     * @brief Chord on cell i with the engine's semantics
     */
    Result chord(int i);

    /**
     * @brief Advance the flag cycle of cell i
     */
    Result toggleMark(int i);

    /**
     * @brief Cells a chord on i would open, without changing the board
     *
     * Returns an empty list if the chord would be rejected.
     */
    QVector<int> chordPreview(int i, bool* wouldExplode = nullptr) const;

    int flagsAround(int i) const;            ///< Real flags around i
    int wrongFlagsAround(int i) const;       ///< Flags around i that are not on mines
    int unflaggedMinesAround(int i) const;   ///< Mines around i not flagged yet
    int revealedCount() const;               ///< Number of revealed cells
    bool isWon() const;                      ///< All safe cells revealed

    /**
     * @brief Compact copy of the visible state (one char per cell)
     *
     * 'h' hidden, 'f' flag, 'q' question mark, 'r' revealed.
     */
    QByteArray snapshot() const;

    /**
     * @brief Restore the visible state from snapshot()
     */
    void restore(const QByteArray& snap);

    /**
     * @brief The 3BV (minimum left clicks) of the board
     */
    int threeBV() const;

    /**
     * @brief 3BV already achieved on the current state
     */
    int threeBVSolved() const;
};

/**
 * @brief A cell that can be proven safe or mined from the visible numbers
 *
 * @since trainer-v1
 */
struct TrainerDeduction
{
    /**
     * @brief The reasoning that proves the cell
     *
     * Ordered from simplest to hardest.
     */
    enum class Kind : quint8
    {
        SatisfiedNumber = 0, ///< A number already has all its mines: other neighbours are safe
        FilledNumber = 1,    ///< A number has exactly as many hidden neighbours as mines left
        Pattern = 2,         ///< Two numbers compared (1-1, 1-2, 1-2-1, 1-2-2-1, ...)
        Enumeration = 3      ///< Only provable by checking every arrangement locally
    };

    int cell = -1;           ///< The proven cell
    bool mine = false;       ///< true if proven mine, false if proven safe
    Kind kind = Kind::SatisfiedNumber; ///< Reasoning used
    QString pattern;         ///< Pattern name such as "1-2-1" (Pattern kind only)
    QVector<int> clues;      ///< Revealed number cells used by the reasoning
    QVector<int> clueRemaining; ///< Mines still unaccounted for around each clue
    QVector<int> group;      ///< Every cell proven by the same step (includes cell)
    QVector<int> region;     ///< Unknown hidden cells the clues talk about
    QVector<int> shared;     ///< Pattern: unknown cells next to both numbers
    QVector<int> knownMines; ///< Hidden cells already known to be mines (subtracted from the numbers)
    int solutions = 0;       ///< Enumeration: number of arrangements that fit
    QVector<int> support;    ///< Previously proven cells this step relied on
    int depth = 1;           ///< Steps of reasoning needed: 0 = your own (provable) flag,
                             ///< 1 = follows directly from the open numbers and your flags
};

/**
 * @brief Finds provable cells and labels the pattern behind each one
 *
 * @since trainer-v1
 */
class TrainerSolver
{
public:
    /**
     * @brief All hidden cells provable from the revealed numbers
     *
     * Flags are ignored: only information visible on revealed numbers is used,
     * so the result reflects what the player could have known.
     *
     * When a window is given, only the numbers within `radius` cells
     * (Chebyshev distance) of `center` are used. That models what a player can
     * work out by looking at one area: any depth of reasoning inside the area,
     * nothing from further away.
     *
     * @param board Board state to analyse
     * @param useEnumeration Also try exhaustive local enumeration
     * @param center Centre cell of the window, or -1 for the whole board
     * @param radius Window radius in cells (ignored without a centre)
     * @return Map cell index -> deduction
     */
    static QHash<int, TrainerDeduction> deduce(const TrainerBoard& board, bool useEnumeration = true,
                                               int center = -1, int radius = 0);

    /**
     * @brief Like deduce(), but only numbers inside `area` (in cells) are used
     *
     * Used by practice puzzles: only what the puzzle shows may be used.
     */
    static QHash<int, TrainerDeduction> deduceInArea(const TrainerBoard& board, const QRect& area,
                                                     bool useEnumeration = true);

    /**
     * @brief Human readable name of a deduction kind
     */
    static QString kindName(TrainerDeduction::Kind kind);

    /**
     * @brief Step by step explanation of a deduction
     *
     * Plain text sentences that walk from the numbers to the conclusion,
     * using "(column,row)" cell names.
     *
     * @param board Board state the deduction was made on
     * @param d The deduction to explain
     * @return One sentence per step
     */
    static QStringList explainSteps(const TrainerBoard& board, const TrainerDeduction& d,
                                    const QVector<TrainerDeduction>& prerequisites = QVector<TrainerDeduction>());

    /**
     * @brief The earlier deductions a deduction depends on, shallowest first
     *
     * Follows TrainerDeduction::support recursively. Flags placed by the
     * player (depth 0) are included so explanations can mention them.
     */
    static QHash<int, TrainerDeduction> deduceImpl(const TrainerBoard& board, bool useEnumeration,
                                                   int center, int radius, const QRect& area);

    /**
     * @brief Is one specific cell provably safe? (targeted, stronger check)
     *
     * deduce() skips very large groups of numbers to stay fast. This check
     * looks only at the numbers around `cell`, grows the area as far as an
     * exhaustive search stays affordable, and in the endgame also uses the
     * number of mines left. Leaving numbers out only makes it more cautious,
     * so a "safe" answer is always correct.
     *
     * @param board Board state
     * @param cell Cell to check
     * @param area Only numbers inside this area may be used (null = whole board)
     * @param proof Filled with the reasoning when the cell is safe
     * @return true if the cell is certainly safe
     */
    static bool proveSafe(const TrainerBoard& board, int cell, const QRect& area = QRect(),
                          TrainerDeduction* proof = nullptr);

    static QVector<TrainerDeduction> prerequisitesOf(const QHash<int, TrainerDeduction>& all,
                                                     const TrainerDeduction& d);
};

/**
 * @brief One piece of feedback about the recorded game
 *
 * @since trainer-v1
 */
struct TrainerFinding
{
    /**
     * @brief Category of the finding
     */
    enum class Type : quint8
    {
        CursorJump = 0,       ///< Cursor jumped away while a provable cell was next to it
        UnnecessaryGuess,     ///< Clicked an unproven cell while proven safe cells existed
        RejectedChord,        ///< Chord with the wrong number of flags
        WastedChord,          ///< Chord that opened nothing
        DominatedChord,       ///< A nearby chord would have opened strictly more
        ClicksCouldBeChord,   ///< Several clicks that one chord would replace
        LowValueChord,        ///< Flagged just to chord a single cell
        WrongFlag,            ///< Flag placed on a safe cell
        WastedClick           ///< Click on a revealed or flagged cell
    };

    Type type = Type::CursorJump; ///< Category
    int actionIndex = 0;   ///< Action the finding refers to (board shown before it)
    int severity = 1;      ///< 1 = minor, 2 = notable, 3 = major
    QString title;         ///< One line summary
    QString detail;        ///< Explanation including the missed pattern or better chord
    int fromCell = -1;     ///< Previous action cell (jumps)
    int actionCell = -1;   ///< Cell the player acted on
    int suggestedCell = -1;///< Cell the player should have used
    QVector<int> clueCells;    ///< Numbers forming the missed pattern
    QVector<int> extraCells;   ///< Other cells to highlight (clicks in a run, chord area)
    int wastedClicks = 0;  ///< Clicks that a better play would have saved
    bool hasDeduction = false;    ///< deduction below is set
    TrainerDeduction deduction;   ///< The logic behind the finding (missed pattern, safe cell)
    QVector<TrainerDeduction> prerequisites; ///< Earlier steps the deduction needs
    qint64 cursorFromMs = -1;  ///< Start of the cursor path to draw
    qint64 cursorToMs = -1;    ///< End of the cursor path to draw

    static QString typeName(Type t); ///< Short category label
};

/**
 * @brief Aggregate statistics of a recorded game
 *
 * @since trainer-v1
 */
struct TrainerStats
{
    int threeBV = 0;           ///< 3BV of the board
    int threeBVSolved = 0;     ///< 3BV achieved by the player
    int leftClicks = 0;        ///< Reveal actions
    int rightClicks = 0;       ///< Flag actions
    int chords = 0;            ///< Chord actions
    int wastedClicks = 0;      ///< Clicks that had no useful effect
    int savableClicks = 0;     ///< Clicks better chording would have saved
    int jumps = 0;             ///< Cursor jumps longer than the threshold
    int jumpsWithMissedPattern = 0; ///< Jumps that left a provable cell behind
    int guesses = 0;           ///< Unforced guesses
    double cursorTravelCells = 0;   ///< Cursor path length in cells
    double durationSec = 0;    ///< Game time
    double efficiency = 0;     ///< 3BV solved / total clicks (percent)
    double threeBVPerSecond = 0; ///< 3BV solved per second
};

/**
 * @brief Full analysis of one recording
 *
 * @since trainer-v1
 */
struct TrainerReport
{
    TrainerStats stats;                ///< Aggregate statistics
    QVector<TrainerFinding> findings;  ///< Findings sorted by action index
    QVector<QByteArray> snapshots;     ///< Board state before each action, plus the final state
    QVector<int> openedPerAction;      ///< Cells opened by each action

    /**
     * @brief Plain text report (used by the command line and tests)
     */
    QString toText(const TrainerRecording& recording) const;
};

/**
 * @brief Replays a recording and produces a TrainerReport
 *
 * @since trainer-v1
 */
class TrainerAnalyzer
{
public:
    /**
     * @brief Tunable thresholds
     */
    struct Options
    {
        double jumpThreshold = 4.0; ///< Distance in cells counted as a jump
        double localRadius = 2.5;   ///< How close a missed cell must be to the old position
        int jumpWindow = 3;         ///< Missed cells must be solvable from numbers this close to the previous click
        int guessWindow = 4;        ///< Safe moves must be solvable from numbers this close to them
        double sameSpeedSlack = 0.5;///< A safe move counts as "just as fast" if at most this much further away
    };

    /**
     * @brief Analyse a recording with default options
     */
    static TrainerReport analyze(const TrainerRecording& recording);

    /**
     * @brief Analyse a recording
     */
    static TrainerReport analyze(const TrainerRecording& recording, const Options& options);

    /**
     * @brief "(col, row)" label with 1-based coordinates
     */
    static QString cellName(int cell, int width);
};

#endif // TRAINERANALYZER_H
