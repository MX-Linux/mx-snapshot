/**********************************************************************
 *  work.h
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

#ifndef WORK_H
#define WORK_H

#include <QElapsedTimer>

#include "cmd.h"
#include "settings.h"

enum BoxType {question, information, warning, critical};

class Work : public QObject
{
    Q_OBJECT
public:
    enum HashType {md5, sha512};
    Q_ENUM(HashType)

    Work(Settings *settings);
    virtual ~Work();

    QElapsedTimer e_timer;
    bool done = false;
    bool started = false;

    QTemporaryDir initrd_dir;
    [[ noreturn ]] void cleanUp();
    bool checkInstalled(const QString &package);
    bool createIso(const QString &filename);
    bool installPackage(const QString &package);
    bool mkDir(const QString &file_name);
    bool replaceStringInFile(const QString &old_text, const QString &new_text, const QString &file_path);
    void checkEnoughSpace();
    void closeInitrd(const QString &initrd_dir, const QString &file);
    void copyModules(const QString &to, const QString &kernel);
    void copyNewIso();
    void makeChecksum(const Work::HashType &hash_type, const QString &folder, const QString &file_name);
    void openInitrd(const QString &file, const QString &initrd_dir);
    void replaceMenuStrings();
    void savePackageList(const QString &file_name);
    void setupEnv();
    void writeSnapshotInfo();

signals:
    void message(const QString &msg);
    void messageBox(BoxType box_type, const QString &title, const QString &msg);

private:
    Settings *settings;

};

#endif // WORK_H
