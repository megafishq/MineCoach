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

#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

#include <cmath>

#include "trainerrecorder.h"

namespace
{
// ~120 Hz is enough to reconstruct the path between two clicks
const int CursorSampleIntervalMs = 8;
// Ignore jitter smaller than this (in cells)
const float CursorMinimumMove = 0.02f;
}

TrainerRecorder::TrainerRecorder(QObject* parent) :
    QObject(parent)
{
    sampler.setTimerType(Qt::PreciseTimer);
    sampler.setInterval(CursorSampleIntervalMs);
    connect(&sampler, &QTimer::timeout, this, &TrainerRecorder::SLOT_sampleCursor);
}

void TrainerRecorder::vStart(int width, int height, int mines, bool useQuestionMark,
                             QWidget* board, std::function<int()> cellLength)
{
    current = TrainerRecording();
    current.width = width;
    current.height = height;
    current.mines = mines;
    current.useQuestionMark = useQuestionMark;
    current.startedAt = QDateTime::currentDateTime();

    boardWidget = board;
    cellLengthFn = cellLength;
    bRecording = true;
    clock.start();
    sampler.start();
}

void TrainerRecorder::vStop()
{
    bRecording = false;
    sampler.stop();
}

bool TrainerRecorder::bFinish(bool won, const QVector<bool>& mineLayout)
{
    if(!bRecording)
        return false;

    SLOT_sampleCursor();
    vStop();

    if(current.actions.isEmpty())
        return false;

    current.won = won;
    current.mineLayout = mineLayout;
    // The engine's clock starts on the first reveal and stops on the last action
    current.durationMs = current.actions.last().tMs - current.actions.first().tMs;

    last = current;
    bHasLast = true;
    lastPath.clear();

    QDir dir(recordingsDirectory());
    if(!dir.exists())
        QDir().mkpath(dir.path());

    const QString name = QStringLiteral("game-%1-%2x%3-%4.json")
                         .arg(current.startedAt.toString("yyyyMMdd-HHmmss"))
                         .arg(current.width).arg(current.height)
                         .arg(won ? QStringLiteral("won") : QStringLiteral("lost"));
    QString error;
    if(last.save(dir.absoluteFilePath(name), &error))
        lastPath = dir.absoluteFilePath(name);
    else
        qWarning() << "Unable to save trainer recording:" << error;

    return true;
}

bool TrainerRecorder::isRecording() const
{
    return bRecording;
}

bool TrainerRecorder::hasLastRecording() const
{
    return bHasLast;
}

const TrainerRecording& TrainerRecorder::lastRecording() const
{
    return last;
}

QString TrainerRecorder::lastRecordingPath() const
{
    return lastPath;
}

QString TrainerRecorder::recordingsDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
           .absoluteFilePath(QStringLiteral("trainer_recordings"));
}

void TrainerRecorder::SLOT_reveal(const uchar x, const uchar y)
{
    vRecordAction(TrainerAction::Type::Reveal, x, y);
}

void TrainerRecorder::SLOT_flag(const uchar x, const uchar y)
{
    vRecordAction(TrainerAction::Type::Flag, x, y);
}

void TrainerRecorder::SLOT_chord(const uchar x, const uchar y)
{
    vRecordAction(TrainerAction::Type::Chord, x, y);
}

void TrainerRecorder::vRecordAction(TrainerAction::Type type, int x, int y)
{
    if(!bRecording)
        return;

    // Make sure the path ends exactly where the click happened
    SLOT_sampleCursor();

    TrainerAction a;
    a.type = type;
    a.tMs = clock.elapsed();
    a.x = x;
    a.y = y;
    current.actions.append(a);
}

void TrainerRecorder::SLOT_sampleCursor()
{
    if(!bRecording || !boardWidget || !cellLengthFn)
        return;

    const int cell = cellLengthFn();
    if(cell <= 0)
        return;

    const QPoint p = boardWidget->mapFromGlobal(QCursor::pos());
    TrainerCursorSample s;
    s.tMs = clock.elapsed();
    s.x = static_cast<float>(p.x()) / cell;
    s.y = static_cast<float>(p.y()) / cell;

    if(!current.cursor.isEmpty())
    {
        const TrainerCursorSample& prev = current.cursor.last();
        if(std::fabs(prev.x - s.x) < CursorMinimumMove && std::fabs(prev.y - s.y) < CursorMinimumMove)
            return;
    }
    current.cursor.append(s);
}
