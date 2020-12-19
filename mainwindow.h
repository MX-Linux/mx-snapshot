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
#include <QDir>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>

#include "cmd.h"

namespace Ui {
class MainWindow;
}


class MainWindow : public QDialog
{
    Q_OBJECT

public:
    enum HashType {md5, sha512};
    Q_ENUM(HashType)

    explicit MainWindow(QWidget *parent = nullptr, const QCommandLineParser &arg_parser = QCommandLineParser());
    ~MainWindow();

    void addRemoveExclusion(bool add, QString exclusion);
    QStringList args;

    bool checkCompression();
    bool i686;
    bool live;
    bool force_installer;
    bool reset_accounts;

    int debian_version;

    QDir lib_mod_dir;
    QDir snapshot_dir;
    QFile config_file;
    QFile gui_editor;
    QFile snapshot_excludes;
    QString edit_boot_menu;
    QString kernel_used;
    QString make_isohybrid;
    QString make_chksum;
    QString compression;
    QString mksq_opt;
    QString save_message;
    QString session_excludes;
    QString snapshot_basename;
    QString stamp;
    QString version;
    QString work_dir;
    // command line options
    bool preempt;

    int getDebianVersion();
    int getSnapshotCount();

    bool checkInstalled(QString package);
    bool createIso(QString package);
    bool installPackage(QString package);
    bool isLive();
    bool isOnSupportedPart(QDir dir);
    bool isi686();
    bool replaceStringInFile(QString old_text, QString new_text, QString file_path);

    void checkDirectories();
    void checkSaveWork();
    void cleanUp();
    void closeApp();
    void closeInitrd(QString initrd_dir, QString file);
    void copyModules(QString to, QString kernel);
    void copyNewIso();
    void fixPermissions();
    void listFreeSpace();
    void listUsedSpace();
    void loadSettings();
    void makeChecksum(HashType hash_type, QString folder, QString file_name);
    void mkDir(QString file_name);
    void openInitrd(QString file, QString initrd_dir);
    void replaceMenuStrings();
    void savePackageList(QString file_name);
    void setup();
    void setupEnv();
    void writeSnapshotInfo();

    QString getEditor();
    QString getFilename();
    QString getLiveRootSpace();
    QString getSnapshotSize();
    QString getXdgUserDirs(const QString &folder);
    QString largerFreeSpace(QString dir1, QString dir2);
    QString largerFreeSpace(QString dir1, QString dir2, QString dir3);
    QStringList listUsers();

protected:
    void keyPressEvent(QKeyEvent* event);

public slots:
    void outputAvailable(const QString &output);
    void procStart();
    void procDone();
    void progress();
    void displayOutput();
    void disableOutput();

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
    void on_radioRespin_clicked(bool checked);

private:
    Ui::MainWindow *ui;
    Cmd *shell;
    QHash<QString, QString> englishDirs; // English names of /home directories
    QElapsedTimer e_timer;
    QStringList users; // list of users with /home folders
    QTimer timer;


};

#endif // MainWindow_H

