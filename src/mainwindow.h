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
 *  along with this program; if not, see http://gnu.org/licenses/
 *  ---                                                               *
 *  Copyright (C) 2009, Justin Davis <tuxdavis@gmail.com>             *
 *  Copyright (C) 2009-2014 ImageWriter developers                    *
 *                          http://launchpad.net/~image-writer-devs   *
 **********************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#ifndef WINVER
#define WINVER 0x0601
#endif

#include <QtWidgets>
#include <windows.h>
#include <memory>
#include "ui_mainwindow.h"

class QClipboard;
class ElapsedTimer;

class MainWindow : public QMainWindow, public Ui::MainWindow
{
    Q_OBJECT
    public:
        static MainWindow& get() {
            // !NOT thread safe  - first call from main only
            if (!instance_p.get())
                instance_p.reset( new MainWindow());
            return *instance_p.get();
        }

        ~MainWindow();
        void closeEvent(QCloseEvent *event);
        enum Status {STATUS_IDLE=0, STATUS_READING, STATUS_WRITING, STATUS_EXIT, STATUS_CANCELED};
        bool nativeEvent(const QByteArray &type, void *vMsg, long *result);
    protected slots:
        void on_tbBrowse_clicked();
        void on_bCancel_clicked();
        void on_bWrite_clicked();
        void on_bRead_clicked();
        void on_leFile_editingFinished();
        void on_bHashCopy_clicked();
private slots:
        void on_cboxHash_currentIndexChanged();
        void on_bHashGen_clicked();
        void on_HashType_stateChanged();

private:
        static std::shared_ptr<MainWindow> instance_p;
        MainWindow(QWidget *parent=0);
        // find attached devices
        void getLogicalDrives();
        void setReadWriteButtonState();
        void saveSettings();
        void loadSettings();
        void initializeHomeDir();
        void updateMd5Controls();

        HANDLE hVolume;
        HANDLE hFile;
        HANDLE hRawDisk;
        static const unsigned short ONE_SEC_IN_MS = 1000;
        unsigned long long sectorsize;
        int status;
        char *sectorData;
        QTime update_timer;
        ElapsedTimer *elapsed_timer = NULL;
        QClipboard *clipboard;
        void generateMd5(char *filename, int hashish);
        QString myHomeDir;
};

#endif // MAINWINDOW_H
