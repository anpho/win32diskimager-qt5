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

#ifndef WINVER
#define WINVER 0x0601
#endif

#include <QtWidgets>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDirIterator>
#include <QClipboard>
#include <cstdio>
#include <cstdlib>
#include <windows.h>
#include <winioctl.h>
#include <dbt.h>
#include <shlobj.h>
#include <iostream>
#include <sstream>

#include "disk.h"
#include "mainwindow.h"
#include "elapsedtimer.h"

MainWindow* MainWindow::instance = NULL;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUi(this);
    elapsed_timer = new ElapsedTimer();
    statusbar->addPermanentWidget(elapsed_timer);   // "addpermanent" puts it on the RHS of the statusbar
    getLogicalDrives();
    status = STATUS_IDLE;
    progressbar->reset();
    clipboard = QApplication::clipboard();
    statusbar->showMessage(tr("Waiting for a task."));
    hVolume = INVALID_HANDLE_VALUE;
    hFile = INVALID_HANDLE_VALUE;
    hRawDisk = INVALID_HANDLE_VALUE;
    if (QCoreApplication::arguments().count() > 1)
    {
        QString fileLocation = QApplication::arguments().at(1);
        QFileInfo fileInfo(fileLocation);
        leFile->setText(fileInfo.absoluteFilePath());
    }
    // Add supported hash types.
    cboxHashType->addItem("MD5",QVariant(QCryptographicHash::Md5));
    cboxHashType->addItem("SHA1",QVariant(QCryptographicHash::Sha1));
    cboxHashType->addItem("SHA256",QVariant(QCryptographicHash::Sha256));
    connect(this->cboxHashType, SIGNAL(currentIndexChanged(int)), SLOT(on_cboxHashType_IdxChg()));
    updateHashControls();
    setReadWriteButtonState();
    sectorData = NULL;
    sectorsize = 0ul;

    loadSettings();
    if (myHomeDir.isEmpty()){
        initializeHomeDir();
    }
}

MainWindow::~MainWindow()
{
    saveSettings();
    if (hRawDisk != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hRawDisk);
        hRawDisk = INVALID_HANDLE_VALUE;
    }
    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }
    if (hVolume != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hVolume);
        hVolume = INVALID_HANDLE_VALUE;
    }
    if (sectorData != NULL)
    {
        delete[] sectorData;
        sectorData = NULL;
    }
    if (sectorData2 != NULL)
    {
        delete[] sectorData2;
        sectorData2 = NULL;
    }
    if (elapsed_timer != NULL)
    {
        delete elapsed_timer;
        elapsed_timer = NULL;
    }
    if (cboxHashType != NULL)
    {
       cboxHashType->clear();
    }
}


void MainWindow::saveSettings()
{
    QSettings userSettings("HKEY_CURRENT_USER\\Software\\Win32DiskImager", QSettings::NativeFormat);
    userSettings.beginGroup("Settings");
    userSettings.setValue("ImageDir", myHomeDir);
    userSettings.endGroup();
}

void MainWindow::loadSettings()
{
    QSettings userSettings("HKEY_CURRENT_USER\\Software\\Win32DiskImager", QSettings::NativeFormat);
    userSettings.beginGroup("Settings");
    myHomeDir = userSettings.value("ImageDir").toString();
}

void MainWindow::initializeHomeDir()
{
    myHomeDir = QDir::homePath();
    if (myHomeDir == NULL){
        myHomeDir = qgetenv("USERPROFILE");
    }
    /* Get Downloads the Windows way */
    QString downloadPath = qgetenv("DiskImagesDir");
    if (downloadPath.isEmpty()) {
        PWSTR pPath = NULL;
        static GUID downloads = {0x374de290, 0x123f, 0x4565, 0x91, 0x64, 0x39,
                                 0xc4, 0x92, 0x5e, 0x46, 0x7b};
        if (SHGetKnownFolderPath(downloads, 0, 0, &pPath) == S_OK) {
            downloadPath = QDir::fromNativeSeparators(QString::fromWCharArray(pPath));
            LocalFree(pPath);
            if (downloadPath.isEmpty() || !QDir(downloadPath).exists()) {
                downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
            }
        }
    }
    if (downloadPath.isEmpty())
        downloadPath = QDir::currentPath();
    myHomeDir = downloadPath;
}

void MainWindow::setReadWriteButtonState()
{
    bool fileSelected = !(leFile->text().isEmpty());
    bool deviceSelected = (cboxDevice->count() > 0);
    QFileInfo fi(leFile->text());

    // set read and write buttons according to status of file/device
    bRead->setEnabled(deviceSelected && fileSelected && (fi.exists() ? fi.isWritable() : true));
    bWrite->setEnabled(deviceSelected && fileSelected && fi.isReadable());
    bVerify->setEnabled(deviceSelected && fileSelected && fi.isReadable());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    if (status == STATUS_READING)
    {
        if (QMessageBox::warning(this, tr("Exit?"), tr("Exiting now will result in a corrupt image file.\n"
                                                       "Are you sure you want to exit?"),
                                 QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        {
            status = STATUS_EXIT;
        }
        event->ignore();
    }
    else if (status == STATUS_WRITING)
    {
        if (QMessageBox::warning(this, tr("Exit?"), tr("Exiting now will result in a corrupt disk.\n"
                                                       "Are you sure you want to exit?"),
                                 QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        {
            status = STATUS_EXIT;
        }
        event->ignore();
    }
    else if (status == STATUS_VERIFYING)
    {
        if (QMessageBox::warning(this, tr("Exit?"), tr("Exiting now will cancel verifying image.\n"
                                                       "Are you sure you want to exit?"),
                                 QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        {
            status = STATUS_EXIT;
        }
        event->ignore();
    }
}

void MainWindow::on_tbBrowse_clicked()
{
    // Use the location of already entered file
    QString fileLocation = leFile->text();
    QFileInfo fileinfo(fileLocation);

    // See if there is a user-defined file extension.
    QString fileType = qgetenv("DiskImagerFiles");
    if (fileType.length() && !fileType.endsWith(";;"))
    {
        fileType.append(";;");
    }
    fileType.append(tr("Disk Images (*.img *.IMG);;*.*"));
    // create a generic FileDialog
    QFileDialog dialog(this, tr("Select a disk image"));
    dialog.setNameFilter(fileType);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setConfirmOverwrite(false);
    if (fileinfo.exists())
    {
        dialog.selectFile(fileLocation);
    }
    else
    {
        dialog.setDirectory(myHomeDir);
    }

    if (dialog.exec())
    {
        // selectedFiles returns a QStringList - we just want 1 filename,
        //	so use the zero'th element from that list as the filename
        fileLocation = (dialog.selectedFiles())[0];

        if (!fileLocation.isNull())
        {
            leFile->setText(fileLocation);
            QFileInfo newFileInfo(fileLocation);
            myHomeDir = newFileInfo.absolutePath();
        }
        setReadWriteButtonState();
        updateHashControls();
    }
}

void MainWindow::on_bHashCopy_clicked()
{
    QString hashSum(hashLabel->text());
    if ( !(hashSum.isEmpty()) )
    {
        clipboard->setText(hashSum);
    }
}

// generates the hash
void MainWindow::generateHash(char *filename, int hashish)
{
    hashLabel->setText(tr("Generating..."));
    QApplication::processEvents();

    QCryptographicHash filehash((QCryptographicHash::Algorithm)hashish);

    // may take a few secs - display a wait cursor
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QFile file(filename);
    file.open(QFile::ReadOnly);
    filehash.addData(&file);

    QByteArray hash = filehash.result();

    // display it in the textbox
    hashLabel->setText(hash.toHex());
    bHashCopy->setEnabled(true);
    // redisplay the normal cursor
    QApplication::restoreOverrideCursor();
}


// on an "editingFinished" signal (IE: return press), if the lineedit
// contains a valid file, update the controls
void MainWindow::on_leFile_editingFinished()
{
    setReadWriteButtonState();
    updateHashControls();
}

void MainWindow::on_bCancel_clicked()
{
    if ( (status == STATUS_READING) || (status == STATUS_WRITING) )
    {
        if (QMessageBox::warning(this, tr("Cancel?"), tr("Canceling now will result in a corrupt destination.\n"
                                                         "Are you sure you want to cancel?"),
                                 QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        {
            status = STATUS_CANCELED;
        }
    }
    else if (status == STATUS_VERIFYING)
    {
        if (QMessageBox::warning(this, tr("Cancel?"), tr("Cancel Verify.\n"
                                                         "Are you sure you want to cancel?"),
                                 QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        {
            status = STATUS_CANCELED;
        }

    }
}

void MainWindow::on_bWrite_clicked()
{
    bool passfail = true;
    if (!leFile->text().isEmpty())
    {
        QFileInfo fileinfo(leFile->text());
        if (fileinfo.exists() && fileinfo.isFile() &&
                fileinfo.isReadable() && (fileinfo.size() > 0) )
        {
            if (leFile->text().at(0) == cboxDevice->currentText().at(1))
            {
                QMessageBox::critical(this, tr("Write Error"), tr("Image file cannot be located on the target device."));
                return;
            }

            // build the drive letter as a const char *
            //   (without the surrounding brackets)
            QString qs = cboxDevice->currentText();
            qs.replace(QRegExp("[\\[\\]]"), "");
            QByteArray qba = qs.toLocal8Bit();
            const char *ltr = qba.data();
            if (QMessageBox::warning(this, tr("Confirm overwrite"), tr("Writing to a physical device can corrupt the device.\n"
                                                                       "(Target Device: %1 \"%2\")\n"
                                                                       "Are you sure you want to continue?").arg(cboxDevice->currentText()).arg(getDriveLabel(ltr)),
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
            {
                return;
            }
            status = STATUS_WRITING;
            bCancel->setEnabled(true);
            bWrite->setEnabled(false);
            bRead->setEnabled(false);
            bVerify->setEnabled(false);
            double mbpersec;
            unsigned long long i, lasti, availablesectors, numsectors;
            int volumeID = cboxDevice->currentText().at(1).toLatin1() - 'A';
            // int deviceID = cboxDevice->itemData(cboxDevice->currentIndex()).toInt();
            hVolume = getHandleOnVolume(volumeID, GENERIC_WRITE);
            if (hVolume == INVALID_HANDLE_VALUE)
            {
                status = STATUS_IDLE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            DWORD deviceID = getDeviceID(hVolume);
            if (!getLockOnVolume(hVolume))
            {
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            if (!unmountVolume(hVolume))
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            hFile = getHandleOnFile(LPCWSTR(leFile->text().data()), GENERIC_READ);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            hRawDisk = getHandleOnDevice(deviceID, GENERIC_WRITE);
            if (hRawDisk == INVALID_HANDLE_VALUE)
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            availablesectors = getNumberOfSectors(hRawDisk, &sectorsize);
            if (!availablesectors)
            {
                //For external card readers you may not get device change notification when you remove the card/flash.
                //(So no WM_DEVICECHANGE signal). Device stays but size goes to 0. [Is there special event for this on Windows??]
                removeLockOnVolume(hVolume);
                CloseHandle(hRawDisk);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                hRawDisk = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                hVolume = INVALID_HANDLE_VALUE;
                passfail = false;
                status = STATUS_IDLE;
                return;

            }
            numsectors = getFileSizeInSectors(hFile, sectorsize);
            if (!numsectors)
            {
                //For external card readers you may not get device change notification when you remove the card/flash.
                //(So no WM_DEVICECHANGE signal). Device stays but size goes to 0. [Is there special event for this on Windows??]
                removeLockOnVolume(hVolume);
                CloseHandle(hRawDisk);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                hRawDisk = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                hVolume = INVALID_HANDLE_VALUE;
                status = STATUS_IDLE;
                return;

            }
            if (numsectors > availablesectors)
            {
                bool datafound = false;
                i = availablesectors;
                unsigned long nextchunksize = 0;
                while ( (i < numsectors) && (datafound == false) )
                {
                    nextchunksize = ((numsectors - i) >= 1024ul) ? 1024ul : (numsectors - i);
                    sectorData = readSectorDataFromHandle(hFile, i, nextchunksize, sectorsize);
                    if(sectorData == NULL)
                    {
                        // if there's an error verifying the truncated data, just move on to the
                        //  write, as we don't care about an error in a section that we're not writing...
                        i = numsectors + 1;
                    } else {
                        unsigned int j = 0;
                        unsigned limit = nextchunksize * sectorsize;
                        while ( (datafound == false) && ( j < limit ) )
                        {
                            if(sectorData[j++] != 0)
                            {
                                datafound = true;
                            }
                        }
                        i += nextchunksize;
                    }
                }
                // delete the allocated sectorData
                delete[] sectorData;
                sectorData = NULL;
                // build the string for the warning dialog
                std::ostringstream msg;
                msg << "More space required than is available:"
                    << "\n  Required: " << numsectors << " sectors"
                    << "\n  Available: " << availablesectors << " sectors"
                    << "\n  Sector Size: " << sectorsize
                    << "\n\nThe extra space " << ((datafound) ? "DOES" : "does not") << " appear to contain data"
                    << "\n\nContinue Anyway?";
                if(QMessageBox::warning(this, tr("Not enough available space!"),
                                        tr(msg.str().c_str()), QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok)
                {
                    // truncate the image at the device size...
                    numsectors = availablesectors;
                }
                else    // Cancel
                {
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    hVolume = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hRawDisk = INVALID_HANDLE_VALUE;
                    bCancel->setEnabled(false);
                    setReadWriteButtonState();
                    return;
                }
            }

            progressbar->setRange(0, (numsectors == 0ul) ? 100 : (int)numsectors);
            lasti = 0ul;
            update_timer.start();
            elapsed_timer->start();
            for (i = 0ul; i < numsectors && status == STATUS_WRITING; i += 1024ul)
            {
                sectorData = readSectorDataFromHandle(hFile, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize);
                if (sectorData == NULL)
                {
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    hRawDisk = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hVolume = INVALID_HANDLE_VALUE;
                    bCancel->setEnabled(false);
                    setReadWriteButtonState();
                    return;
                }
                if (!writeSectorDataToHandle(hRawDisk, sectorData, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize))
                {
                    delete[] sectorData;
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    sectorData = NULL;
                    hRawDisk = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hVolume = INVALID_HANDLE_VALUE;
                    bCancel->setEnabled(false);
                    setReadWriteButtonState();
                    return;
                }
                delete[] sectorData;
                sectorData = NULL;
                QCoreApplication::processEvents();
                if (update_timer.elapsed() >= ONE_SEC_IN_MS)
                {
                    mbpersec = (((double)sectorsize * (i - lasti)) * ((float)ONE_SEC_IN_MS / update_timer.elapsed())) / 1024.0 / 1024.0;
                    statusbar->showMessage(QString("%1 MB/s").arg(mbpersec));
                    elapsed_timer->update(i, numsectors);
                    update_timer.start();
                    lasti = i;
                }
                progressbar->setValue(i);
                QCoreApplication::processEvents();
            }
            removeLockOnVolume(hVolume);
            CloseHandle(hRawDisk);
            CloseHandle(hFile);
            CloseHandle(hVolume);
            hRawDisk = INVALID_HANDLE_VALUE;
            hFile = INVALID_HANDLE_VALUE;
            hVolume = INVALID_HANDLE_VALUE;
            if (status == STATUS_CANCELED){
                passfail = false;
            }
        }
        else if (!fileinfo.exists() || !fileinfo.isFile())
        {
            QMessageBox::critical(this, tr("File Error"), tr("The selected file does not exist."));
            passfail = false;
        }
        else if (!fileinfo.isReadable())
        {
            QMessageBox::critical(this, tr("File Error"), tr("You do not have permision to read the selected file."));
            passfail = false;
        }
        else if (fileinfo.size() == 0)
        {
            QMessageBox::critical(this, tr("File Error"), tr("The specified file contains no data."));
            passfail = false;
        }
        progressbar->reset();
        statusbar->showMessage(tr("Done."));
        bCancel->setEnabled(false);
        setReadWriteButtonState();
        if (passfail){
            QMessageBox::information(this, tr("Complete"), tr("Write Successful."));
        }

    }
    else
    {
        QMessageBox::critical(this, tr("File Error"), tr("Please specify an image file to use."));
    }
    if (status == STATUS_EXIT)
    {
        close();
    }
    status = STATUS_IDLE;
    elapsed_timer->stop();
}

void MainWindow::on_bRead_clicked()
{
    QString myFile;
    if (!leFile->text().isEmpty())
    {
        myFile = leFile->text();
        QFileInfo fileinfo(myFile);
        if (fileinfo.path()=="."){
            myFile=(myHomeDir + "/" + leFile->text());
        }
        // check whether source and target device is the same...
        if (myFile.at(0) == cboxDevice->currentText().at(1))
        {
            QMessageBox::critical(this, tr("Write Error"), tr("Image file cannot be located on the target device."));
            return;
        }
        // confirm overwrite if the dest. file already exists
        if (fileinfo.exists())
        {
            if (QMessageBox::warning(this, tr("Confirm Overwrite"), tr("Are you sure you want to overwrite the specified file?"),
                                     QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::No)
            {
                return;
            }
        }
        bCancel->setEnabled(true);
        bWrite->setEnabled(false);
        bRead->setEnabled(false);
        bVerify->setEnabled(false);
        status = STATUS_READING;
        double mbpersec;
        unsigned long long i, lasti, numsectors, filesize, spaceneeded = 0ull;
        int volumeID = cboxDevice->currentText().at(1).toLatin1() - 'A';
        hVolume = getHandleOnVolume(volumeID, GENERIC_READ);
        if (hVolume == INVALID_HANDLE_VALUE)
        {
            status = STATUS_IDLE;
            bCancel->setEnabled(false);
            setReadWriteButtonState();
            return;
        }
        DWORD deviceID = getDeviceID(hVolume);
        if (!getLockOnVolume(hVolume))
        {
            CloseHandle(hVolume);
            status = STATUS_IDLE;
            hVolume = INVALID_HANDLE_VALUE;
            bCancel->setEnabled(false);
            setReadWriteButtonState();
            return;
        }
        if (!unmountVolume(hVolume))
        {
            removeLockOnVolume(hVolume);
            CloseHandle(hVolume);
            status = STATUS_IDLE;
            hVolume = INVALID_HANDLE_VALUE;
            bCancel->setEnabled(false);
            setReadWriteButtonState();
            return;
        }
        hFile = getHandleOnFile(LPCWSTR(myFile.data()), GENERIC_WRITE);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            removeLockOnVolume(hVolume);
            CloseHandle(hVolume);
            status = STATUS_IDLE;
            hVolume = INVALID_HANDLE_VALUE;
            bCancel->setEnabled(false);
            setReadWriteButtonState();
            return;
        }
        hRawDisk = getHandleOnDevice(deviceID, GENERIC_READ);
        if (hRawDisk == INVALID_HANDLE_VALUE)
        {
            removeLockOnVolume(hVolume);
            CloseHandle(hFile);
            CloseHandle(hVolume);
            status = STATUS_IDLE;
            hVolume = INVALID_HANDLE_VALUE;
            hFile = INVALID_HANDLE_VALUE;
            bCancel->setEnabled(false);
            setReadWriteButtonState();
            return;
        }
        numsectors = getNumberOfSectors(hRawDisk, &sectorsize);
        if(partitionCheckBox->isChecked())
        {
            // Read MBR partition table
            sectorData = readSectorDataFromHandle(hRawDisk, 0, 1ul, 512ul);
            numsectors = 1ul;
            // Read partition information
            for (i=0ul; i<4ul; i++)
            {
                uint32_t partitionStartSector = *((uint32_t*) (sectorData + 0x1BE + 8 + 16*i));
                uint32_t partitionNumSectors = *((uint32_t*) (sectorData + 0x1BE + 12 + 16*i));
                // Set numsectors to end of last partition
                if (partitionStartSector + partitionNumSectors > numsectors)
                {
                    numsectors = partitionStartSector + partitionNumSectors;
                }
            }
        }
        filesize = getFileSizeInSectors(hFile, sectorsize);
        if (filesize >= numsectors)
        {
            spaceneeded = 0ull;
        }
        else
        {
            spaceneeded = (unsigned long long)(numsectors - filesize) * (unsigned long long)(sectorsize);
        }
        if (!spaceAvailable(myFile.left(3).replace(QChar('/'), QChar('\\')).toLatin1().data(), spaceneeded))
        {
            QMessageBox::critical(this, tr("Write Error"), tr("Disk is not large enough for the specified image."));
            removeLockOnVolume(hVolume);
            CloseHandle(hRawDisk);
            CloseHandle(hFile);
            CloseHandle(hVolume);
            status = STATUS_IDLE;
            sectorData = NULL;
            hRawDisk = INVALID_HANDLE_VALUE;
            hFile = INVALID_HANDLE_VALUE;
            hVolume = INVALID_HANDLE_VALUE;
            bCancel->setEnabled(false);
            setReadWriteButtonState();
            return;
        }
        if (numsectors == 0ul)
        {
            progressbar->setRange(0, 100);
        }
        else
        {
            progressbar->setRange(0, (int)numsectors);
        }
        lasti = 0ul;
        update_timer.start();
        elapsed_timer->start();
        for (i = 0ul; i < numsectors && status == STATUS_READING; i += 1024ul)
        {
            sectorData = readSectorDataFromHandle(hRawDisk, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize);
            if (sectorData == NULL)
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hRawDisk);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hRawDisk = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                hVolume = INVALID_HANDLE_VALUE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            if (!writeSectorDataToHandle(hFile, sectorData, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize))
            {
                delete[] sectorData;
                removeLockOnVolume(hVolume);
                CloseHandle(hRawDisk);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                sectorData = NULL;
                hRawDisk = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                hVolume = INVALID_HANDLE_VALUE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            delete[] sectorData;
            sectorData = NULL;
            if (update_timer.elapsed() >= ONE_SEC_IN_MS)
            {
                mbpersec = (((double)sectorsize * (i - lasti)) * ((float)ONE_SEC_IN_MS / update_timer.elapsed())) / 1024.0 / 1024.0;
                statusbar->showMessage(QString("%1MB/s").arg(mbpersec));
                update_timer.start();
                elapsed_timer->update(i, numsectors);
                lasti = i;
            }
            progressbar->setValue(i);
            QCoreApplication::processEvents();
        }
        removeLockOnVolume(hVolume);
        CloseHandle(hRawDisk);
        CloseHandle(hFile);
        CloseHandle(hVolume);
        hRawDisk = INVALID_HANDLE_VALUE;
        hFile = INVALID_HANDLE_VALUE;
        hVolume = INVALID_HANDLE_VALUE;
        progressbar->reset();
        statusbar->showMessage(tr("Done."));
        bCancel->setEnabled(false);
        setReadWriteButtonState();
        if (status == STATUS_CANCELED){
            QMessageBox::information(this, tr("Complete"), tr("Read Canceled."));
        } else {
            QMessageBox::information(this, tr("Complete"), tr("Read Successful."));

        }
        updateHashControls();
    }
    else
    {
        QMessageBox::critical(this, tr("File Info"), tr("Please specify a file to save data to."));
    }
    if (status == STATUS_EXIT)
    {
        close();
    }
    status = STATUS_IDLE;
    elapsed_timer->stop();
}

// Verify image with device
void MainWindow::on_bVerify_clicked()
{
    bool passfail = true;
    if (!leFile->text().isEmpty())
    {
        QFileInfo fileinfo(leFile->text());
        if (fileinfo.exists() && fileinfo.isFile() &&
                fileinfo.isReadable() && (fileinfo.size() > 0) )
        {
            if (leFile->text().at(0) == cboxDevice->currentText().at(1))
            {
                QMessageBox::critical(this, tr("Verify Error"), tr("Image file cannot be located on the target device."));
                return;
            }
            status = STATUS_VERIFYING;
            bCancel->setEnabled(true);
            bWrite->setEnabled(false);
            bRead->setEnabled(false);
            bVerify->setEnabled(false);
            double mbpersec;
            unsigned long long i, lasti, availablesectors, numsectors, result;
            int volumeID = cboxDevice->currentText().at(1).toLatin1() - 'A';
            hVolume = getHandleOnVolume(volumeID, GENERIC_READ);
            if (hVolume == INVALID_HANDLE_VALUE)
            {
                status = STATUS_IDLE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            DWORD deviceID = getDeviceID(hVolume);
            if (!getLockOnVolume(hVolume))
            {
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            if (!unmountVolume(hVolume))
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            hFile = getHandleOnFile(LPCWSTR(leFile->text().data()), GENERIC_READ);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            hRawDisk = getHandleOnDevice(deviceID, GENERIC_READ);
            if (hRawDisk == INVALID_HANDLE_VALUE)
            {
                removeLockOnVolume(hVolume);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                status = STATUS_IDLE;
                hVolume = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                bCancel->setEnabled(false);
                setReadWriteButtonState();
                return;
            }
            availablesectors = getNumberOfSectors(hRawDisk, &sectorsize);
            if (!availablesectors)
            {
                //For external card readers you may not get device change notification when you remove the card/flash.
                //(So no WM_DEVICECHANGE signal). Device stays but size goes to 0. [Is there special event for this on Windows??]
                removeLockOnVolume(hVolume);
                CloseHandle(hRawDisk);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                hRawDisk = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                hVolume = INVALID_HANDLE_VALUE;
                passfail = false;
                status = STATUS_IDLE;
                return;

            }
            numsectors = getFileSizeInSectors(hFile, sectorsize);
            if (!numsectors)
            {
                //For external card readers you may not get device change notification when you remove the card/flash.
                //(So no WM_DEVICECHANGE signal). Device stays but size goes to 0. [Is there special event for this on Windows??]
                removeLockOnVolume(hVolume);
                CloseHandle(hRawDisk);
                CloseHandle(hFile);
                CloseHandle(hVolume);
                hRawDisk = INVALID_HANDLE_VALUE;
                hFile = INVALID_HANDLE_VALUE;
                hVolume = INVALID_HANDLE_VALUE;
                status = STATUS_IDLE;
                return;

            }
            if (numsectors > availablesectors)
            {
                bool datafound = false;
                i = availablesectors;
                unsigned long nextchunksize = 0;
                while ( (i < numsectors) && (datafound == false) )
                {
                    nextchunksize = ((numsectors - i) >= 1024ul) ? 1024ul : (numsectors - i);
                    sectorData = readSectorDataFromHandle(hFile, i, nextchunksize, sectorsize);
                    if(sectorData == NULL)
                    {
                        // if there's an error verifying the truncated data, just move on to the
                        //  write, as we don't care about an error in a section that we're not writing...
                        i = numsectors + 1;
                    } else {
                        unsigned int j = 0;
                        unsigned limit = nextchunksize * sectorsize;
                        while ( (datafound == false) && ( j < limit ) )
                        {
                            if(sectorData[j++] != 0)
                            {
                                datafound = true;
                            }
                        }
                        i += nextchunksize;
                    }
                }
                // delete the allocated sectorData
                delete[] sectorData;
                sectorData = NULL;
                // build the string for the warning dialog
                std::ostringstream msg;
                msg << "Size of image larger than device:"
                    << "\n  Image: " << numsectors << " sectors"
                    << "\n  Device: " << availablesectors << " sectors"
                    << "\n  Sector Size: " << sectorsize
                    << "\n\nThe extra space " << ((datafound) ? "DOES" : "does not") << " appear to contain data"
                    << "\n\nContinue Anyway?";
                if(QMessageBox::warning(this, tr("Size Mismatch!"),
                                        tr(msg.str().c_str()), QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok)
                {
                    // truncate the image at the device size...
                    numsectors = availablesectors;
                }
                else    // Cancel
                {
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    hVolume = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hRawDisk = INVALID_HANDLE_VALUE;
                    bCancel->setEnabled(false);
                    setReadWriteButtonState();
                    return;
                }
            }
            progressbar->setRange(0, (numsectors == 0ul) ? 100 : (int)numsectors);
            update_timer.start();
            elapsed_timer->start();
            lasti = 0ul;
            for (i = 0ul; i < numsectors && status == STATUS_VERIFYING; i += 1024ul)
            {
                sectorData = readSectorDataFromHandle(hFile, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize);
                if (sectorData == NULL)
                {
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    hRawDisk = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hVolume = INVALID_HANDLE_VALUE;
                    bCancel->setEnabled(false);
                    setReadWriteButtonState();
                    return;
                }
                sectorData2 = readSectorDataFromHandle(hRawDisk, i, (numsectors - i >= 1024ul) ? 1024ul:(numsectors - i), sectorsize);
                if (sectorData2 == NULL)
                {
                    QMessageBox::critical(this, tr("Verify Failure"), tr("Verification failed at sector: %1").arg(i));
                    removeLockOnVolume(hVolume);
                    CloseHandle(hRawDisk);
                    CloseHandle(hFile);
                    CloseHandle(hVolume);
                    status = STATUS_IDLE;
                    hRawDisk = INVALID_HANDLE_VALUE;
                    hFile = INVALID_HANDLE_VALUE;
                    hVolume = INVALID_HANDLE_VALUE;
                    bCancel->setEnabled(false);
                    setReadWriteButtonState();
                    return;
                }
                result = memcmp(sectorData, sectorData2, ((numsectors - i >= 1024ul) ? 1024ul:(numsectors - i)) * sectorsize);
                if (result)
                {
                    QMessageBox::critical(this, tr("Verify Failure"), tr("Verification failed at sector: %1").arg(i));
                    passfail = false;
                    break;

                }
                if (update_timer.elapsed() >= ONE_SEC_IN_MS)
                {
                    mbpersec = (((double)sectorsize * (i - lasti)) * ((float)ONE_SEC_IN_MS / update_timer.elapsed())) / 1024.0 / 1024.0;
                    statusbar->showMessage(QString("%1MB/s").arg(mbpersec));
                    update_timer.start();
                    elapsed_timer->update(i, numsectors);
                    lasti = i;
                }
                delete[] sectorData;
                delete[] sectorData2;
                sectorData = NULL;
                sectorData2 = NULL;
                progressbar->setValue(i);
                QCoreApplication::processEvents();
            }
            removeLockOnVolume(hVolume);
            CloseHandle(hRawDisk);
            CloseHandle(hFile);
            CloseHandle(hVolume);
            delete[] sectorData;
            delete[] sectorData2;
            sectorData = NULL;
            sectorData2 = NULL;
            hRawDisk = INVALID_HANDLE_VALUE;
            hFile = INVALID_HANDLE_VALUE;
            hVolume = INVALID_HANDLE_VALUE;
            if (status == STATUS_CANCELED){
                passfail = false;
            }

        }
        else if (!fileinfo.exists() || !fileinfo.isFile())
        {
            QMessageBox::critical(this, tr("File Error"), tr("The selected file does not exist."));
            passfail = false;
        }
        else if (!fileinfo.isReadable())
        {
            QMessageBox::critical(this, tr("File Error"), tr("You do not have permision to read the selected file."));
            passfail = false;
        }
        else if (fileinfo.size() == 0)
        {
            QMessageBox::critical(this, tr("File Error"), tr("The specified file contains no data."));
            passfail = false;
        }
        progressbar->reset();
        statusbar->showMessage(tr("Done."));
        bCancel->setEnabled(false);
        setReadWriteButtonState();
        if (passfail){
            QMessageBox::information(this, tr("Complete"), tr("Verify Successful."));
        }
    }
    else
    {
        QMessageBox::critical(this, tr("File Error"), tr("Please specify an image file to use."));
    }
    if (status == STATUS_EXIT)
    {
        close();
    }
    status = STATUS_IDLE;
    elapsed_timer->stop();
}

// getLogicalDrives sets cBoxDevice with any logical drives found, as long
// as they indicate that they're either removable, or fixed and on USB bus
void MainWindow::getLogicalDrives()
{
    // GetLogicalDrives returns 0 on failure, or a bitmask representing
    // the drives available on the system (bit 0 = A:, bit 1 = B:, etc)
    unsigned long driveMask = GetLogicalDrives();
    int i = 0;
    ULONG pID;

    cboxDevice->clear();

    while (driveMask != 0)
    {
        if (driveMask & 1)
        {
            // the "A" in drivename will get incremented by the # of bits
            // we've shifted
            char drivename[] = "\\\\.\\A:\\";
            drivename[4] += i;
            if (checkDriveType(drivename, &pID))
            {
                cboxDevice->addItem(QString("[%1:\\]").arg(drivename[4]), (qulonglong)pID);
            }
        }
        driveMask >>= 1;
        cboxDevice->setCurrentIndex(0);
        ++i;
    }
}

// support routine for winEvent - returns the drive letter for a given mask
//   taken from http://support.microsoft.com/kb/163503
char FirstDriveFromMask (ULONG unitmask)
{
    char i;

    for (i = 0; i < 26; ++i)
    {
        if (unitmask & 0x1)
        {
            break;
        }
        unitmask = unitmask >> 1;
    }

    return (i + 'A');
}

// register to receive notifications when USB devices are inserted or removed
// adapted from http://www.known-issues.net/qt/qt-detect-event-windows.html
bool MainWindow::nativeEvent(const QByteArray &type, void *vMsg, long *result)
{
    Q_UNUSED(type);
    MSG *msg = (MSG*)vMsg;
    if(msg->message == WM_DEVICECHANGE)
    {
        PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)msg->lParam;
        switch(msg->wParam)
        {
        case DBT_DEVICEARRIVAL:
            if (lpdb -> dbch_devicetype == DBT_DEVTYP_VOLUME)
            {
                PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME)lpdb;
                if(DBTF_NET)
                {
                    char ALET = FirstDriveFromMask(lpdbv->dbcv_unitmask);
                    // add device to combo box (after sanity check that
                    // it's not already there, which it shouldn't be)
                    QString qs = QString("[%1:\\]").arg(ALET);
                    if (cboxDevice->findText(qs) == -1)
                    {
                        ULONG pID;
                        char longname[] = "\\\\.\\A:\\";
                        longname[4] = ALET;
                        // checkDriveType gets the physicalID
                        if (checkDriveType(longname, &pID))
                        {
                            cboxDevice->addItem(qs, (qulonglong)pID);
                            setReadWriteButtonState();
                        }
                    }
                }
            }
            break;
        case DBT_DEVICEREMOVECOMPLETE:
            if (lpdb -> dbch_devicetype == DBT_DEVTYP_VOLUME)
            {
                PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME)lpdb;
                if(DBTF_NET)
                {
                    char ALET = FirstDriveFromMask(lpdbv->dbcv_unitmask);
                    //  find the device that was removed in the combo box,
                    //  and remove it from there....
                    //  "removeItem" ignores the request if the index is
                    //  out of range, and findText returns -1 if the item isn't found.
                    cboxDevice->removeItem(cboxDevice->findText(QString("[%1:\\]").arg(ALET)));
                    setReadWriteButtonState();
                }
            }
            break;
        } // skip the rest
    } // end of if msg->message
    *result = 0; //get rid of obnoxious compiler warning
    return false; // let qt handle the rest
}

void MainWindow::updateHashControls()
{
    QFileInfo fileinfo(leFile->text());
    bool validFile = (fileinfo.exists() && fileinfo.isFile() &&
                      fileinfo.isReadable() && (fileinfo.size() >0));

    bHashCopy->setEnabled(false);
    hashLabel->clear();

    if (cboxHashType->currentIndex() != 0 && !leFile->text().isEmpty() && validFile)
    {
            bHashGen->setEnabled(true);
    }
    else
    {
        bHashGen->setEnabled(false);
    }

    // if there's a value in the md5 label make the copy button visible
    bool haveHash = !(hashLabel->text().isEmpty());
    bHashCopy->setEnabled(haveHash );
}

void MainWindow::on_cboxHashType_IdxChg()
{
    updateHashControls();
}

void MainWindow::on_bHashGen_clicked()
{
    generateHash(leFile->text().toLatin1().data(),cboxHashType->currentData().toInt());

}
