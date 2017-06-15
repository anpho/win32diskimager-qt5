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
 *  Copyright (C) 2009-2017 ImageWriter developers                    *
 *                 https://sourceforge.net/projects/win32diskimager/  *
 **********************************************************************/

#include "elapsedtimer.h"

ElapsedTimer::ElapsedTimer(QWidget *parent)
    : QLabel(parent)
{
    timer = new QTime();
    setTextFormat(Qt::PlainText);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setText(QString("0:00/0:00"));
    setVisible(false);
}

ElapsedTimer::~ElapsedTimer()
{
    delete timer;
}

int ElapsedTimer::ms()
{
    return(timer->elapsed());
}

// this uses a parameter to store the results rather than a return value in
//	order to avoid mallocing the struct every time this function is called
//  (which, potentially, is frequently).
void ElapsedTimer::secsToHMS(unsigned int secs, timeStruct_t *hms)
{
	unsigned int mins = 0;

	hms->sec = secs % SECS_PER_MIN;
	if ( secs >= SECS_PER_MIN )
	{
		mins = (secs / SECS_PER_MIN);
		hms->min = mins % MINS_PER_HOUR;
		if ( secs >= SECS_PER_HOUR )
		{
			hms->hour = secs / SECS_PER_HOUR;
		}
	}
}

void ElapsedTimer::update(unsigned long long progress, unsigned long long total)
{
    timeStruct_t tTime, eTime;

    unsigned int baseSecs = timer->elapsed() / MS_PER_SEC;
    unsigned int totalSecs = (unsigned int)((float)baseSecs * ( (float)total/(float)progress ));

    // convert seconds to hours:minues:seconds
    secsToHMS(baseSecs, &eTime);
	secsToHMS(totalSecs, &tTime);

    // build the display string
    const QChar & fillChar = QLatin1Char( '0' );
    QString qs = QString("%1:%2/").arg(eTime.min, 2, 10, fillChar).arg(eTime.sec, 2, 10, fillChar);
    if (eTime.hour > 0)
    {
        qs.prepend(QString("%1:").arg(eTime.hour, 2, 10, fillChar));
    }
    if (tTime.hour > 0)
    {
        qs += (QString("%1:").arg(tTime.hour, 2, 10, fillChar));
    }
    qs += (QString("%1:%2 ").arg(tTime.min, 2, 10, fillChar).arg(tTime.sec, 2, 10, fillChar));
        // added a space following the times to separate the text slightly from the right edge of the status bar...
        // there's probably a more "QT-correct" way to do that (like, margins or something),
        // but this was simple and effective.

    // display
    setText(qs);
}

void ElapsedTimer::start()
{
    setVisible(true);
    timer->start();
}

void ElapsedTimer::stop()
{
    setVisible(false);
    setText(QString("0:00/0:00 "));
}
