/**********************************************************************
 *  settings.h
 **********************************************************************
 * Copyright (C) 2020-2026 MX Authors
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
#pragma once

#include <QCommandLineParser>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

#include <memory>

#include "cmd.h"
#include "filesystemutils.h"
#include "systeminfo.h"

extern QString currentKernel;

namespace Release
{
enum Version { Jessie = 8, Stretch, Buster, Bullseye, Bookworm, Trixie, Forky, Duke };
}

class Settings
{
public:
    explicit Settings(const QCommandLineParser &argParser, bool isGuiApp);

    // Editor invocation as a resolved argument list (program + args); execute
    // directly with QProcess — never through a shell.
    [[nodiscard]] QStringList getEditorCommand() const;
    [[nodiscard]] QString getFilename() const;
    [[nodiscard]] QString getFreeSpaceStrings(const QString &path);
    [[nodiscard]] QString getSnapshotSize() const;
    [[nodiscard]] QString getUsedSpace();
    [[nodiscard]] QString getXdgUserDirs(const QString &folder);
    [[nodiscard]] static QString resolveWorkDirParent(const QString &dir);
    [[nodiscard]] bool checkCompression() const;
    [[nodiscard]] bool checkConfiguration() const;
    // Empty when name (the ISO file name, ".iso" included) is usable, otherwise
    // the reason it is not. Shared by checkConfiguration() and the GUI.
    [[nodiscard]] static QString snapshotNameError(const QString &name);
    [[nodiscard]] bool checkSnapshotDir() const;
    [[nodiscard]] QString getExcludesSourcePath() const { return excludesSourcePath; }
    [[nodiscard]] bool checkTempDir();
    [[nodiscard]] bool initializeConfiguration();
    // A workflow caller must clear Cmd::elevationDenied() before its first
    // invocation; a prior denial makes this return false without probing.
    [[nodiscard]] bool supportsWorkDirectory(const QString &candidate) const;
    [[nodiscard]] bool validateExclusions() const;
    [[nodiscard]] bool validateSpaceRequirements() const;
    [[nodiscard]] int getSnapshotCount() const;
    [[nodiscard]] quint64 getLiveRootSpace();
    [[nodiscard]] static int getDebianVerNum();
    void addRemoveExclusion(bool add, QString exclusion);
    void excludeAll();
    void excludeDesktop(bool exclude);
    void excludeDocuments(bool exclude);
    void excludeDownloads(bool exclude);
    void excludeFlatpaks(bool exclude);
    void excludeItem(const QString &item);
    void excludeMusic(bool exclude);
    void excludeNetworks(bool exclude);
    void excludePictures(bool exclude);
    void excludeSteam(bool exclude);
    void excludeSwapFile();
    void excludeVideos(bool exclude);
    void excludeVirtualBox(bool exclude);
    void handleInitializationError(const QString &error) const;
    void loadConfig(const QString &compressionOptions);
    void otherExclusions();
    void processArgs(const QCommandLineParser &argParser);
    void processExclArgs(const QCommandLineParser &argParser);
    void selectKernel();
    void setMonthlySnapshot(const QCommandLineParser &argParser);
    void setVariables();

    // Public enums and types
    enum class Exclude {
        Desktop = 1 << 0,
        Documents = 1 << 1,
        Downloads = 1 << 2,
        Flatpaks = 1 << 3,
        Music = 1 << 4,
        Networks = 1 << 5,
        Pictures = 1 << 6,
        Steam = 1 << 7,
        Videos = 1 << 8,
        VirtualBox = 1 << 9
    };
    Q_DECLARE_FLAGS(Exclusions, Exclude)

    // Phase 1: Immutable system configuration (const)
    const bool x86;
    const uint maxCores;
    const bool monthly;
    const bool overrideSize;
    // Must be declared (and therefore initialized) before editBootMenu and every
    // other member below that reads QSettings — it redirects QSettings' user-scope
    // path to the invoking user's ~/.config before any of those reads happen.
    // See redirectUserConfigPath()/userConfigBaseDir() in settings.cpp.
    const bool userConfigPathReady;
    const bool editBootMenu;
    const bool isGuiApp;
    const QHash<QString, quint8> compressionFactor {{"xz", 31},  {"zstd", 35}, {"gzip", 37},
                                                    {"lzo", 52}, {"lzma", 52}, {"lz4", 52}};

    // Phase 2: Mutable UI preferences
    Exclusions exclusions;
    QString bootOptions;
    QString codename;
    QString compression;
    QString distroVersion;
    QString fullDistroName;
    QString kernel;
    QString projectName;
    QString releaseDate;
    bool isArch {};
    bool makeMd5sum {};
    bool makeSha256sum {};
    bool resetAccounts {};
    uint cores {};
    uint throttle {};
    bool grubmbr;

    // Phase 3: Runtime state
    QFile snapshotExcludes;
    std::unique_ptr<QTemporaryDir> tmpdir;
    QStringList mksqOpt;
    QString sessionExcludes;
    QString snapshotDir;
    QString snapshotName;
    QString tempDirParent;
    QString workDir;
    bool preempt {};
    bool shutdown {};
    const bool forceInstaller;
    const bool live;
    const bool makeIsohybrid;
    quint64 freeSpace {};
    quint64 freeSpaceWork {};
    QString excludesSourcePath;

private:
    QFile configFile;
    QString saveMessage;
    QString version;
    const QString guiEditor;
    const QString snapshotBasename;
    const QString stamp;
    const QStringList path {qEnvironmentVariable("PATH").split(":") << "/usr/sbin"};
    const QStringList users; // list of users with /home folders
    quint64 homeSize {};
    quint64 rootSize {};

    // Common helper for folder-based exclusions
    void excludeUserFolder(bool exclude, Exclude flag, const QString &folderName, const QString &xdgKey);

    // Helper functions for const member initialization
    QString getInitialKernel(const QCommandLineParser &argParser);
    bool getEditBootMenuSetting();
    QString trimQuotes(const QString &value) const;

    struct InitialSettings {
        bool live;
        bool forceInstaller;
        bool makeIsohybrid;
        QString guiEditor;
        QString snapshotBasename;
        QString stamp;
        QStringList users;
    };

    InitialSettings getInitialSettings() const;
};
