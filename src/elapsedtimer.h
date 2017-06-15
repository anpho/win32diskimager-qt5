/**********************************************************************
 *  This program is free software; you can redistribute it and/or     *
 *  modify it under the terms of the GNU General Public License       *
 *  as published by the Free Software Foundation; either version 2    *
 *  of the License, or (at your option) any later version.            *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the     *
 *  GNU General Public License for more details.                      *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, see http://gnu.org/licenses/     *
 *  ---                                                               *
 *  Copyright (C) 2009, Justin Davis <tuxdavis@gmail.com>             *
 *  Copyright (C) 2009-2014 ImageWriter developers                    *
 *                 https://sourceforge.net/projects/win32diskimager/  *
 **********************************************************************/

#ifndef ELAPSEDTIMER_H
#define ELAPSEDTIMER_H

#ifndef WINVER
#define WINVER 0x0601
#endif

#include <QtWidgets>
#include <QLabel>
#include <QTime>
#include <QString>

class ElapsedTimer : public QLabel
{
    Q_OBJECT

public:
    ElapsedTimer(QWidget *parent = 0);
    ~ElapsedTimer();
    int ms();
    void update(unsigned long long progress, unsigned long long total);
    void start();
    void stop();

private:
    struct timeStruct_t
    {
        unsigned short hour = 0;
        unsigned short min  = 0;
        unsigned short sec  = 0;
    };

    QTime *timer;
    void secsToHMS(unsigned int secs, timeStruct_t *ts);
    static const unsigned short MS_PER_SEC = 1000;
    static const unsigned short SECS_PER_MIN = 60;
    static const unsigned short MINS_PER_HOUR = 60;
    static const unsigned short SECS_PER_HOUR = (SECS_PER_MIN * MINS_PER_HOUR);
};

#endif // ELAPSEDTIMER_H
