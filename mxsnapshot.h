/**********************************************************************
 *  mxsnapshot.h
 **********************************************************************
 * Copyright (C) 2015 MX Authors
 *
 * Authors: Adrian
 *          MX & MEPIS Community <http://forum.mepiscommunity.org>
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


#ifndef MXSNAPSHOT_H
#define MXSNAPSHOT_H

#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#include <QSettings>

#include <QDir>

namespace Ui {
class mxsnapshot;
}

class mxsnapshot : public QDialog
{
    Q_OBJECT

protected:
    QProcess *proc;
    QTimer *timer;

public:
    explicit mxsnapshot(QWidget *parent = 0);
    ~mxsnapshot();

    QString getCmdOut(QString cmd);
    int runCmd(QString cmd);
    QString getVersion(QString name);
    void addRemoveExclusion(bool add, QString exclusion);
    QSettings settings;

    bool live;
    bool reset_accounts;
    bool i686;
    QDir snapshot_dir;
    QDir lib_mod_dir;
    QFile config_file;
    QFile snapshot_excludes;
    QFile gui_editor;
    QString version;
    QString work_dir;
    QString stamp;
    QString snapshot_basename;
    QString make_md5sum;
    QString make_isohybrid;
    QString edit_boot_menu;
    QString kernel_used;
    QString save_message;
    QString mksq_opt;
    QString session_excludes;

    int getSnapshotCount();
    bool installPackage(QString package);
    bool checkInstalled(QString package);
    bool replaceStringInFile(QString oldtext, QString newtext, QString filepath);
    bool createIso(QString filename);
    bool isLive();
    bool isi686();
    void loadSettings();
    void setup();
    void listUsedSpace();
    void listFreeSpace();    
    void checkEditor();
    void checkDirectories();
    void checkSaveWork();
    void openInitrd(QString file, QString initrd_dir);
    void closeInitrd(QString initrd_dir, QString file);
    void copyModules(QString to, QString kernel);
    void copyNewIso();
    void mkDir(QString filename);
    void savePackageList(QString filename);
    void setupEnv();
    void cleanUp();
    void makeMd5sum(QString folder, QString filename);
    void fixPermissions();
    void replaceMenuStrings();
    QString getSnapshotSize();
    QString getFilename();
    QStringList listUsers();

public slots:
    void procStart();
    void procTime();
    void procDone(int);
    void setConnections();
    void disconnectAll();
    void onStdoutAvailable();

private slots:
    void on_buttonNext_clicked();
    void on_buttonAbout_clicked();
    void on_buttonHelp_clicked();
    void on_buttonBack_clicked();
    void on_buttonEditConfig_clicked();
    void on_buttonEditExclude_clicked();
    void on_excludeDocuments_toggled(bool checked);
    void on_excludeDownloads_toggled(bool checked);
    void on_excludePictures_toggled(bool checked);
    void on_excludeMusic_toggled(bool checked);
    void on_excludeVideos_toggled(bool checked);
    void on_excludeDesktop_toggled(bool checked);
    void on_buttonSelectSnapshot_clicked();
    void on_radioRespin_clicked(bool checked);
    void on_radioPersonal_clicked(bool checked);

private:
    Ui::mxsnapshot *ui;

};

#endif // MXSNAPSHOT_H

