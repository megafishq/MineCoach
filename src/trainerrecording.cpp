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

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include "trainerrecording.h"

namespace
{
const int RecordingFormatVersion = 1;

QString actionTypeToString(TrainerAction::Type t)
{
    switch(t)
    {
        case TrainerAction::Type::Reveal: return QStringLiteral("reveal");
        case TrainerAction::Type::Flag:   return QStringLiteral("flag");
        case TrainerAction::Type::Chord:  return QStringLiteral("chord");
    }
    return QStringLiteral("reveal");
}

bool actionTypeFromString(const QString& s, TrainerAction::Type& t)
{
    if(s == QLatin1String("reveal")) { t = TrainerAction::Type::Reveal; return true; }
    if(s == QLatin1String("flag"))   { t = TrainerAction::Type::Flag;   return true; }
    if(s == QLatin1String("chord"))  { t = TrainerAction::Type::Chord;  return true; }
    return false;
}

void setError(QString* error, const QString& msg)
{
    if(error)
        *error = msg;
}
}

bool TrainerRecording::isMine(int x, int y) const
{
    if(x < 0 || y < 0 || x >= width || y >= height)
        return false;
    return mineLayout.value(y*width + x, false);
}

QJsonObject TrainerRecording::toJson() const
{
    QJsonObject json;
    json["format"] = QStringLiteral("libremines-trainer");
    json["version"] = RecordingFormatVersion;
    json["width"] = width;
    json["height"] = height;
    json["mines"] = mines;
    json["useQuestionMark"] = useQuestionMark;
    json["won"] = won;
    json["durationMs"] = static_cast<double>(durationMs);
    json["startedAt"] = startedAt.toString(Qt::ISODateWithMs);

    QJsonArray rows;
    for(int y=0; y<height; ++y)
    {
        QString row;
        row.reserve(width);
        for(int x=0; x<width; ++x)
            row.append(isMine(x, y) ? QLatin1Char('*') : QLatin1Char('.'));
        rows.append(row);
    }
    json["minefield"] = rows;

    QJsonArray jsonActions;
    for(const TrainerAction& a : actions)
    {
        QJsonObject o;
        o["t"] = static_cast<double>(a.tMs);
        o["type"] = actionTypeToString(a.type);
        o["x"] = a.x;
        o["y"] = a.y;
        jsonActions.append(o);
    }
    json["actions"] = jsonActions;

    // Cursor samples are stored compactly as [t, x, y] triples
    QJsonArray jsonCursor;
    for(const TrainerCursorSample& s : cursor)
    {
        jsonCursor.append(QJsonArray{static_cast<double>(s.tMs),
                                     qRound(s.x*1000)/1000.0,
                                     qRound(s.y*1000)/1000.0});
    }
    json["cursor"] = jsonCursor;

    return json;
}

bool TrainerRecording::fromJson(const QJsonObject& json, TrainerRecording& out, QString* error)
{
    if(json.value("format").toString() != QLatin1String("libremines-trainer"))
    {
        setError(error, QStringLiteral("Not a LibreMines Trainer recording"));
        return false;
    }

    TrainerRecording r;
    r.width = json.value("width").toInt();
    r.height = json.value("height").toInt();
    r.mines = json.value("mines").toInt();
    r.useQuestionMark = json.value("useQuestionMark").toBool();
    r.won = json.value("won").toBool();
    r.durationMs = static_cast<qint64>(json.value("durationMs").toDouble());
    r.startedAt = QDateTime::fromString(json.value("startedAt").toString(), Qt::ISODateWithMs);

    if(r.width <= 0 || r.height <= 0 || r.width > 255 || r.height > 255)
    {
        setError(error, QStringLiteral("Invalid board dimensions"));
        return false;
    }

    const QJsonArray rows = json.value("minefield").toArray();
    if(rows.size() != r.height)
    {
        setError(error, QStringLiteral("Minefield row count does not match height"));
        return false;
    }
    r.mineLayout = QVector<bool>(r.width*r.height, false);
    for(int y=0; y<r.height; ++y)
    {
        const QString row = rows.at(y).toString();
        if(row.size() != r.width)
        {
            setError(error, QStringLiteral("Minefield row %1 has the wrong length").arg(y));
            return false;
        }
        for(int x=0; x<r.width; ++x)
            r.mineLayout[y*r.width + x] = row.at(x) == QLatin1Char('*');
    }

    for(const QJsonValue& v : json.value("actions").toArray())
    {
        const QJsonObject o = v.toObject();
        TrainerAction a;
        if(!actionTypeFromString(o.value("type").toString(), a.type))
        {
            setError(error, QStringLiteral("Unknown action type"));
            return false;
        }
        a.tMs = static_cast<qint64>(o.value("t").toDouble());
        a.x = o.value("x").toInt();
        a.y = o.value("y").toInt();
        if(a.x < 0 || a.y < 0 || a.x >= r.width || a.y >= r.height)
        {
            setError(error, QStringLiteral("Action outside of the board"));
            return false;
        }
        r.actions.append(a);
    }

    for(const QJsonValue& v : json.value("cursor").toArray())
    {
        const QJsonArray triple = v.toArray();
        if(triple.size() != 3)
            continue;
        TrainerCursorSample s;
        s.tMs = static_cast<qint64>(triple.at(0).toDouble());
        s.x = static_cast<float>(triple.at(1).toDouble());
        s.y = static_cast<float>(triple.at(2).toDouble());
        r.cursor.append(s);
    }

    out = r;
    return true;
}

bool TrainerRecording::save(const QString& path, QString* error) const
{
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        setError(error, file.errorString());
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Compact));
    return true;
}

bool TrainerRecording::load(const QString& path, TrainerRecording& out, QString* error)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
    {
        setError(error, file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if(doc.isNull() || !doc.isObject())
    {
        setError(error, parseError.errorString());
        return false;
    }
    return fromJson(doc.object(), out, error);
}

TrainerRecording TrainerRecording::fromMinefieldString(const QString& minefield)
{
    TrainerRecording r;
    QStringList rows;
    for(const QString& line : minefield.split(QLatin1Char('\n')))
    {
        const QString trimmed = line.trimmed();
        if(!trimmed.isEmpty())
            rows.append(trimmed);
    }

    r.height = rows.size();
    r.width = rows.isEmpty() ? 0 : rows.first().size();
    r.mineLayout = QVector<bool>(r.width*r.height, false);
    for(int y=0; y<r.height; ++y)
    {
        for(int x=0; x<r.width && x<rows[y].size(); ++x)
        {
            if(rows[y].at(x) == QLatin1Char('*'))
            {
                r.mineLayout[y*r.width + x] = true;
                r.mines++;
            }
        }
    }
    r.startedAt = QDateTime::currentDateTime();
    return r;
}
