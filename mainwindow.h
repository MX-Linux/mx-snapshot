/**********************************************************************
 *  mainwindow.h
 **********************************************************************
 * Copyright (C) 2015 MX Authors
 *
 * Authors: Adrian
 *          MX Linux <http://mxlinux.org>
 *
 * This file is part of MX Snapshot.
 *
 * MX Snapshot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MX Snapshot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MX Snapshot.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCommandLineParser>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>

#include "cmd.h"
#include "settings.h"
#include "work.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QDialog, public Settings
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr, const QCommandLineParser &arg_parser = QCommandLineParser());
    ~MainWindow();

    [[ noreturn ]] void cleanUp();
    bool installPackage(const QString &package);
    void checkSaveWork();
    void closeApp();
    void listFreeSpace();
    void listUsedSpace();
    void loadSettings();
    void setConnections();
    void setExclusions();
    void setOtherOptions();
    void setup();

protected:
    void keyPressEvent(QKeyEvent* event);

public slots:
    void disableOutput();
    void displayOutput();
    void outputAvailable(const QString &output);
    void procDone();
    void procStart();
    void processMsg(const QString &msg);
    void processMsgBox(BoxType box_type, const QString &title, const QString &msg);
    void progress();

private slots:

    void on_buttonAbout_clicked();
    void on_buttonBack_clicked();
    void on_buttonCancel_clicked();
    void on_buttonEditExclude_clicked();
    void on_buttonHelp_clicked();
    void on_buttonNext_clicked();
    void on_buttonSelectSnapshot_clicked();
    void on_cbCompression_currentIndexChanged(const QString &arg1);
    void on_excludeDesktop_toggled(bool checked);
    void on_excludeDocuments_toggled(bool checked);
    void on_excludeDownloads_toggled(bool checked);
    void on_excludeMusic_toggled(bool checked);
    void on_excludeNetworks_toggled(bool checked);
    void on_excludePictures_toggled(bool checked);
    void on_excludeVideos_toggled(bool checked);
    void on_radioPersonal_clicked(bool checked);
    void on_radioRespin_toggled(bool checked);
    void on_checksums_toggled(bool checked);

private:
    Ui::MainWindow *ui;
    QTimer timer;
    Work work;
    bool monthly;


};

#endif // MainWindow_H

