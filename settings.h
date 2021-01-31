/**********************************************************************
 *  settings.h
 **********************************************************************
 * Copyright (C) 2020 MX Authors
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QCommandLineParser>
#include <QFile>
#include <QTemporaryDir>

#include <cmd.h>

static QHash<QString, uint> compression_factor {
    {"gzip", 37},
    {"lzo", 52},
    { "lzma", 52 },
    { "xz", 31 },
    { "lz4", 52 }};

class Settings
{
public:
    enum Exclude {
        Desktop = 0x1,
        Documents = 0x2,
        Downloads = 0x4,
        Music = 0x8,
        Networks = 0x16,
        Pictures = 0x32,
        Videos = 0x64
    };
    Q_DECLARE_FLAGS(Exclusions, Exclude)

    Settings(const QCommandLineParser &arg_parser);
    ~Settings();

    Cmd *shell;
    Exclusions exclusions;
    QFile config_file;
    QFile gui_editor;
    QFile snapshot_excludes;
    QHash<QString, QString> englishDirs; // English names of /home directories
    QScopedPointer<QTemporaryDir> tmpdir;
    QString compression;
    QString kernel;
    QString mksq_opt;
    QString save_message;
    QString session_excludes;
    QString snapshot_basename;
    QString snapshot_dir;
    QString snapshot_name;
    QString stamp;
    QString tempdir_parent;
    QString version;
    QString work_dir;
    QStringList users; // list of users with /home folders
    bool edit_boot_menu;
    bool force_installer;
    bool i686;
    bool live;
    bool make_chksum;
    bool make_isohybrid;
    bool preempt; // command line option
    bool reset_accounts;
    short debian_version;
    quint64 free_space = 0;
    quint64 free_space_work = 0;
    quint64 home_size = 0;
    quint64 root_size = 0;


    //QString getUsedSpace();
    QString getEditor();
    QString getFilename();
    QString getFreeSpaceStrings(const QString &path);
    QString getSnapshotSize();
    QString getUsedSpace();
    QString getXdgUserDirs(const QString& folder);
    QString largerFreeSpace(const QString &dir1, const QString &dir2);
    QString largerFreeSpace(const QString &dir1, const QString &dir2, const QString &dir3);
    QStringList listUsers();
    bool checkCompression();
    bool checkSnapshotDir();
    bool checkTempDir();
    bool isLive();
    bool isOnSupportedPart(const QString &dir);
    bool isi686();
    short getDebianVersion();
    int getSnapshotCount();
    quint64 getFreeSpace(const QString &path);
    quint64 getLiveRootSpace();
    void addRemoveExclusion(bool add, QString exclusion);
    void excludeAll();
    void excludeDesktop(bool exclude);
    void excludeDocuments(bool exclude);
    void excludeDownloads(bool exclude);
    void excludeItem(const QString &item);
    void excludeMusic(bool exclude);
    void excludeNetworks(bool exclude);
    void excludePictures(bool exclude);
    void excludeVideos(bool exclude);
    void loadConfig();
    void otherExclusions();
    void processArgs(const QCommandLineParser &arg_parser);
    void processExclArgs(const QCommandLineParser &arg_parser);
    void selectKernel();
    void setMonthlySnapshot(const QCommandLineParser &arg_parser);
    void setVariables();
};

#endif // SETTINGS_H
