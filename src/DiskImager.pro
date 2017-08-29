###################################################################
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, see http://gnu.org/licenses/
#
#
#  Copyright (C) 2009, Justin Davis <tuxdavis@gmail.com>
#  Copyright (C) 2009-2017 ImageWriter developers
#                 https://sourceforge.net/projects/win32diskimager/
###################################################################
TEMPLATE = app
TARGET = ../../Win32DiskImager
DEPENDPATH += .
INCLUDEPATH += .
#CONFIG += release
DEFINES -= UNICODE
QT += widgets
VERSION = 1.0
VERSTR = '\\"$${VERSION}\\"'
DEFINES += VER=\"$${VERSTR}\"
DEFINES += WINVER=0x0601
DEFINES += _WIN32_WINNT=0x0601
QMAKE_TARGET_PRODUCT = "Win32 Image Writer"
QMAKE_TARGET_DESCRIPTION = "Image Writer for Windows to write USB and SD images"
QMAKE_TARGET_COPYRIGHT = "Copyright (C) 2009-2017 Windows ImageWriter Team"

# Input
HEADERS += disk.h\
           mainwindow.h\
           droppablelineedit.h \
           elapsedtimer.h

FORMS += mainwindow.ui

SOURCES += disk.cpp\
           main.cpp\
           mainwindow.cpp\
           droppablelineedit.cpp \
           elapsedtimer.cpp

RESOURCES += gui_icons.qrc translations.qrc

RC_FILE = DiskImager.rc

LANGUAGES  +=    lang/diskimager_de.ts
LANGUAGES  +=    lang/diskimager_en.ts
LANGUAGES  +=    lang/diskimager_es.ts
LANGUAGES  +=    lang/diskimager_fr.ts
LANGUAGES  +=    lang/diskimager_it.ts
LANGUAGES  +=    lang/diskimager_nl.ts
LANGUAGES  +=    lang/diskimager_pl.ts
LANGUAGES  +=    lang/diskimager_ta_IN.ts
LANGUAGES  +=    lang/diskimager_zh_CN.ts
LANGUAGES  +=    lang/diskimager_zh_TW.ts
