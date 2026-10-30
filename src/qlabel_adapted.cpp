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
 *****************************************************************************
 */


#include "qlabel_adapted.h"
#include <QMouseEvent>
#include <QHash>

QLabel_adapted::QLabel_adapted(QWidget *parent, const uchar x, const uchar y):
    QLabel(parent),
    xCell(x),
    yCell(y)
{
    setFocusPolicy(Qt::ClickFocus);
}

uchar QLabel_adapted::getXCell()
{
    return xCell;
}

uchar QLabel_adapted::getYCell()
{
    return yCell;
}

void QLabel_adapted::setPixmapCached(const QPixmap& pxmap)
{
    // Store the normal pixmap
    normalPixmap = pxmap;

    // Inverting an image is expensive and every cell shares the same few theme
    // pixmaps, so keep one inverted copy per source pixmap
    static QHash<qint64, QPixmap> invertedCache;
    auto it = invertedCache.constFind(pxmap.cacheKey());
    if(it == invertedCache.constEnd())
    {
        if(invertedCache.size() > 256)
            invertedCache.clear();
        QImage img = pxmap.toImage();
        img.invertPixels();
        it = invertedCache.insert(pxmap.cacheKey(), QPixmap::fromImage(img));
    }
    invertedPixmap = *it;

    // Set the normal pixmap initially
    setPixmap(normalPixmap);
    isInverted = false;
}

void QLabel_adapted::setPixmapInverted()
{
    // Cached pixmap swap
    if (!isInverted) {
        setPixmap(invertedPixmap);
        isInverted = true;
    }
}

void QLabel_adapted::setPixmapNormal()
{
    // Cached pixmap swap
    if (isInverted) {
        setPixmap(normalPixmap);
        isInverted = false;
    }
}

void QLabel_adapted::mouseReleaseEvent(QMouseEvent *e)
{
    setPixmapNormal();

    Q_EMIT SIGNAL_released(e);
}

void QLabel_adapted::mousePressEvent(QMouseEvent *e)
{
    setPixmapInverted();

    Q_EMIT SIGNAL_clicked(e);
}

