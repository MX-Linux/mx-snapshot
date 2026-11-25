/**********************************************************************
 *  settings.cpp
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

#include "settings.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>

#include <exception>
#include <stdexcept>

#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include "messagehandler.h"
#include "squashfsutils.h"

namespace
{
// Qt cannot enumerate system processes (QProcess only manages its own
// children), so read /proc/<pid>/comm directly — the same source pgrep uses.
// comm holds the process name truncated to 15 characters, so the name passed
// in must fit within that limit.
bool isProcessRunning(const QByteArray &name)
{
    QDirIterator it(QStringLiteral("/proc"), QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        it.next();
        if (!it.fileName().at(0).isDigit()) {
            continue; // not a PID entry
        }
        QFile comm(it.filePath() + "/comm");
        if (comm.open(QIODevice::ReadOnly) && comm.readLine().trimmed() == name) {
            return true;
        }
    }
    return false;
}

QString trimQuotesValue(QString value)
{
    value = value.trimmed();
    if (value.startsWith('\'') && value.endsWith('\'') && value.length() > 1) {
        value = value.mid(1, value.length() - 2);
    }
    if (value.startsWith('"') && value.endsWith('"') && value.length() > 1) {
        value = value.mid(1, value.length() - 2);
    }
    return value;
}

QString readOsReleaseValue(const QString &key)
{
    QFile file("/etc/os-release");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream in(&file);
    const QString keyPrefix = key + "=";
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#') || !line.startsWith(keyPrefix)) {
            continue;
        }
        return trimQuotesValue(line.section('=', 1));
    }
    return {};
}

QString ensureCowSpacesize(QString options)
{
    static const QRegularExpression pattern(R"((^|\s)cow_spacesize=\S+)");
    if (options.contains(pattern)) {
        return options;
    }
    if (options.trimmed().isEmpty()) {
        return "cow_spacesize=80%";
    }
    return options.trimmed() + " cow_spacesize=80%";
}

QString loggedInUserName()
{
    return Cmd::loggedInUserName();
}

QString userConfigBaseDir()
{
    const QString username = loggedInUserName();

    if (!username.isEmpty()) {
        const QString candidateHome = QDir::cleanPath("/home/" + username);
        if (QDir(candidateHome).exists()) {
            return QDir::cleanPath(candidateHome + "/.config");
        }
    }

    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
}

// Redirects QSettings' user-scope path to the invoking user's ~/.config before
// any member initializer below reads a QSettings value. QSettings::setPath()
// is a process-wide static, so this only needs to run once; it must happen
// before Settings::editBootMenu's initializer (getEditBootMenuSetting()) runs,
// which is why this is itself a member initializer ordered ahead of it in
// settings.h rather than a call inside the constructor body (loadConfig(),
// called from the body, already runs far too late for the ones in the
// member-initializer list).
bool redirectUserConfigPath()
{
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, userConfigBaseDir());
    return true;
}

void chownUserConfigPath(const QString &path, const QString &userConfigDir)
{
    const QString username = loggedInUserName();
    if (username.isEmpty() || path.isEmpty()) {
        return;
    }
    const QString cleanPath = QDir::cleanPath(path);
    const QString cleanConfigDir = QDir::cleanPath(userConfigDir);
    if (cleanPath != cleanConfigDir && !cleanPath.startsWith(cleanConfigDir + "/")) {
        return;
    }
    // Never chown through a symlink: run as root the chown would follow it and
    // could hand ownership of an arbitrary target file to the user.
    struct stat st;
    const QByteArray pathBytes = path.toLocal8Bit();
    if (lstat(pathBytes.constData(), &st) != 0 || (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))) {
        return;
    }
    // Only chown when the file is not already owned by the target user. The
    // previous isWritable() guard was wrong: a root process can write almost any
    // file, so the chown was always skipped and root-created config/exclude files
    // stayed root-owned, leaving the user unable to edit them.
    const struct passwd *pw = getpwnam(username.toLocal8Bit().constData());
    if (pw == nullptr || st.st_uid == pw->pw_uid) {
        return;
    }
    // Confine resolved paths too: an intermediate symlink must not turn a
    // user-config path into a system file or a directory outside the user's home.
    const QString home = QFileInfo(QString::fromLocal8Bit(pw->pw_dir)).canonicalFilePath();
    const QString config = QFileInfo(cleanConfigDir).canonicalFilePath();
    const QString resolved = QFileInfo(path).canonicalFilePath();
    if (home.isEmpty() || config.isEmpty() || !config.startsWith(home + "/")
        || (resolved != config && !resolved.startsWith(config + "/"))) {
        return;
    }
    // -h: operate on the path itself (defends against a TOCTOU symlink swap).
    Cmd().procAsRoot("chown", {"-h", username + ":", path}, nullptr, nullptr, Cmd::QuietMode::Yes);
}

} // namespace

Settings::Settings(const QCommandLineParser &argParser, bool isGuiApp)
    : x86(SystemInfo::is386()),
      maxCores(Cmd().getOut("nproc", Cmd::QuietMode::Yes).trimmed().toUInt()),
      monthly(argParser.isSet("month")),
      overrideSize(argParser.isSet("override-size")),
      allowOmittedMounts(argParser.isSet("allow-omitted-mounts")),
      userConfigPathReady(redirectUserConfigPath()),
      editBootMenu(getEditBootMenuSetting()),
      isGuiApp(isGuiApp),
      forceInstaller(getInitialSettings().forceInstaller),
      grubmbr(argParser.isSet("grub-mbr")),
      live(getInitialSettings().live),
      makeIsohybrid(getInitialSettings().makeIsohybrid),
      configFile("/etc/" + qApp->applicationName() + ".conf"),
      guiEditor(getInitialSettings().guiEditor),
      snapshotBasename(getInitialSettings().snapshotBasename),
      stamp(getInitialSettings().stamp),
      users(getInitialSettings().users)
{
    try {
        if (!initializeConfiguration()) {
            throw std::runtime_error("Failed to initialize configuration");
        }

        // Clean up leftovers of a previous interrupted run. Only invoke the root
        // helper when something is actually left behind: a normal startup then
        // performs no elevated call at all (previously cleanup_overlay ran, and
        // could prompt, on every launch even with nothing to clean).
        const QString appName = QCoreApplication::applicationName();
        const QString overlayBase = "/run/" + appName + "/bind-root-overlay";
        // Two possible bind-root setups left behind from a previous run:
        // installed-to-live (Debian, marker at /tmp/installed-to-live/cleanup.conf)
        // and installed-to-live-arch (Arch, state at /run/<app>/cleanup-arch.state
        // only). Run whichever cleanup matches.
        const bool archStatePresent = QFileInfo::exists("/run/" + appName + "/cleanup-arch.state");
        bool archCleanupOk = true;
        if (archStatePresent) {
            const QString archScript
                = "/usr/share/" + appName + "/scripts/installed-to-live-arch";
            if (QFileInfo::exists(archScript)) {
                archCleanupOk = Cmd().procAsRoot("installed-to-live-arch", {"cleanup"});
            } else {
                qCritical().noquote() << QObject::tr(
                    "Pending Arch bind-root cleanup state but installed-to-live-arch is missing: %1")
                                             .arg(archScript);
                archCleanupOk = false;
            }
        }
        const bool hasLiveCleanup = QFileInfo::exists("/tmp/installed-to-live/cleanup.conf");
        const bool hasOverlayBase = QFileInfo::exists(overlayBase);
        const bool bindRootMounted = Cmd().run("mountpoint -q /.bind-root", Cmd::QuietMode::Yes)
            || (hasOverlayBase
                && Cmd().run("mountpoint -q \"" + overlayBase + "/root\"", Cmd::QuietMode::Yes));
        if (hasLiveCleanup || hasOverlayBase || bindRootMounted) {
            const bool cleanupOk = Cmd().procAsRoot("snapshot-lib", {"cleanup"}) && archCleanupOk;
            // Remove the overlay directories only when it is safe: either the
            // cleanup above succeeded or nothing is mounted under them anymore.
            if (hasOverlayBase && (cleanupOk || !bindRootMounted)) {
                Cmd().procAsRoot("snapshot-lib", {"cleanup_overlay", appName});
            }
        }

        loadConfig(argParser.value("compression-level"));
        setVariables();
        kernel = getInitialKernel(argParser); // Initialize kernel after config is loaded
        preempt = argParser.isSet("preempt"); // Initialize preempt from command line
        processArgs(argParser);

        if (monthly) {
            setMonthlySnapshot(argParser);
        }

        // Append the NVIDIA boot option after bootOptions is finalized (set in
        // setVariables(), or reset in setMonthlySnapshot() for monthly builds).
        // This is the non-interactive CLI equivalent of the GUI's NVIDIA prompt.
        if (argParser.isSet("nvidia") && !bootOptions.contains("xorg=nvidia")) {
            bootOptions = bootOptions.isEmpty() ? QStringLiteral("xorg=nvidia") : bootOptions + " xorg=nvidia";
        }

        processExclArgs(argParser);

        // Validate final configuration
        if (!checkConfiguration()) {
            throw std::runtime_error("Configuration validation failed");
        }

    } catch (const std::exception &e) {
        handleInitializationError(QObject::tr("Exception during initialization: %1").arg(e.what()));
        throw;
    } catch (...) {
        handleInitializationError(QObject::tr("Unknown exception during initialization"));
        throw std::runtime_error("Unknown exception during initialization");
    }
}

// Check if compression is available in the kernel (lz4, lzo, xz)
bool Settings::checkCompression() const
{
    // Skip the check for gzip (always supported) or if the kernel config file
    // is unavailable. On Arch, /boot/config-* is typically absent so the check
    // passes by default; if the user has a kernel package that does ship the
    // config (e.g. linux-lts), we'll actually validate it.
    if (compression == "gzip" || !QFileInfo::exists("/boot/config-" + kernel)) {
        return true;
    }
    // Argument array: compression/kernel are user-supplied (config file or CLI)
    // and must never be parsed by a shell.
    return Cmd().proc("grep", {"-q", "^CONFIG_SQUASHFS_" + compression.toUpper() + "=y", "/boot/config-" + kernel});
}

// Adds or removes exclusion to the exclusion string
void Settings::addRemoveExclusion(bool add, QString exclusion)
{
    exclusion.remove(QRegularExpression("^/"));
    if (add) {
        sessionExcludes.append('"' + exclusion + "\" ");
    } else {
        sessionExcludes.remove('"' + exclusion + "\" ");
    }
}

bool Settings::checkSnapshotDir() const
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (!Cmd().procAsRoot("mkdir", {"-p", snapshotDir}, nullptr, nullptr, Cmd::QuietMode::No)) {
        qDebug() << QObject::tr("Could not create work directory. ") + snapshotDir;
        return false;
    }
    const QString username = loggedInUserName();
    if (!username.isEmpty()) {
        Cmd().procAsRoot("chown", {username + ":", snapshotDir}, nullptr, nullptr, Cmd::QuietMode::Yes);
    }
    return true;
}

QString Settings::resolveWorkDirParent(const QString &dir)
{
    if (dir != "/home") {
        return dir;
    }
    QString userName = QString::fromUtf8(qgetenv("SUDO_USER")).trimmed();
    if (userName.isEmpty()) {
        userName = QString::fromUtf8(qgetenv("LOGNAME")).trimmed();
    }
    return "/home/" + userName;
}

bool Settings::supportsWorkDirectory(const QString &candidate) const
{
    if (Cmd::elevationDenied()) {
        return false;
    }

    const QString resolvedCandidate = resolveWorkDirParent(candidate);
    if (!QFile::exists(resolvedCandidate) || !FileSystemUtils::isOnSupportedPartition(resolvedCandidate)) {
        return false;
    }

    // The work directory is created by the user, but mksquashfs and other
    // snapshot steps write into it through the elevated helper. A FUSE mount
    // without allow_other/allow_root or an NFS root_squash export can pass the
    // user-side filesystem probe yet reject those writes. Keep the probe
    // directory user-private (like the real QTemporaryDir) and make one tiny
    // root write inside it to verify the access that matters.
    QTemporaryDir rootProbe(QDir(resolvedCandidate).filePath(".mx-snapshot-root-check-XXXXXX"));
    if (!rootProbe.isValid()) {
        qDebug() << "Rejecting" << resolvedCandidate << ": could not create elevated probe directory";
        return false;
    }
    const QString rootProbeFile = rootProbe.filePath("root-write");
    if (!Cmd().procAsRoot("touch", {rootProbeFile}, nullptr, nullptr, Cmd::QuietMode::Yes)) {
        qDebug() << "Rejecting" << resolvedCandidate << ": elevated probe write failed";
        return false;
    }
    return true;
}

bool Settings::checkTempDir()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // Resolve /home to the invoking user's home BEFORE any validation or
    // free-space comparison: the home can be a separate mount (NFS automount,
    // FUSE/ecryptfs, ...) even when /home itself sits on a supported local
    // filesystem, so checks against /home would not cover the directory
    // actually used for the work dir.
    tempDirParent = resolveWorkDirParent(tempDirParent);
    // Set workdir location if not defined in .conf file, doesn't exist, or cannot support
    // the filesystem operations used by the intermediate artifacts as either
    // the user or the elevated snapshot helper.
    if (tempDirParent.isEmpty() || !supportsWorkDirectory(tempDirParent)) {
        QStringList candidates;
        // Resolve every candidate: snapshotDir can be configured as literally
        // "/home" too, and must get the same per-user expansion and
        // validation as the built-in fallback.
        for (const QString &rawCandidate : {QStringLiteral("/tmp"), QStringLiteral("/home"), snapshotDir}) {
            const QString candidate = resolveWorkDirParent(rawCandidate);
            if (candidate.isEmpty() || candidates.contains(candidate)) {
                continue;
            }
            if (!supportsWorkDirectory(candidate)) {
                continue;
            }
            candidates << candidate;
        }
        if (Cmd::elevationDenied()) {
            return false;
        }
        if (candidates.isEmpty()) {
            qCritical() << QObject::tr(
                "No suitable filesystem found for the temp directory. Tried /tmp, /home, and the snapshot directory.");
            return false;
        }
        tempDirParent = candidates.takeFirst();
        for (const QString &candidate : std::as_const(candidates)) {
            tempDirParent = FileSystemUtils::largerFreeSpace(tempDirParent, candidate);
        }
    }
    tmpdir.reset(new QTemporaryDir(tempDirParent + "/mx-snapshot-XXXXXXXX"));
    if (!tmpdir->isValid()) {
        qCritical() << QObject::tr("Could not create temp directory:") << tmpdir->path();
        qCritical() << QObject::tr("Please check that the parent directory exists and is writable:");
        qCritical() << tempDirParent;
        return false;
    }
    workDir = tmpdir->path();
    freeSpaceWork = FileSystemUtils::getFreeSpace(workDir);

    QDir().mkpath(workDir + "/iso-template/antiX");
    qDebug() << "Work directory is placed in" << tempDirParent;
    return true;
}

bool Settings::checkConfiguration() const
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    // Check compression format
    if (!checkCompression()) {
        qCritical() << QObject::tr("Compression format '%1' is not supported by the current kernel").arg(compression);
        return false;
    }

    // Check cores setting
    if (cores == 0 || cores > maxCores) {
        qCritical() << QObject::tr("Invalid cores setting: %1. Must be between 1 and %2").arg(cores).arg(maxCores);
        return false;
    }

    // Check throttle setting. 99 matches the range processArgs() already
    // validates the --throttle CLI value against (and the GUI's spinThrottle,
    // which relies on QSpinBox's default 0-99 range) -- mksquashfs's -throttle
    // takes an I/O percentage with no lower cap of its own.
    if (throttle > 99) {
        qCritical() << QObject::tr("Invalid throttle setting: %1. Must be between 0 and 99").arg(throttle);
        return false;
    }

    // Check snapshot directory
    if (snapshotDir.isEmpty()) {
        qCritical() << QObject::tr("Snapshot directory cannot be empty");
        return false;
    }

    // Note: Directory creation is handled later with elevated permissions in checkSnapshotDir()

    if (const QString nameError = snapshotNameError(snapshotName); !nameError.isEmpty()) {
        qCritical().noquote() << nameError;
        return false;
    }

    // Check kernel
    if (kernel.isEmpty()) {
        qCritical() << QObject::tr("Kernel version cannot be empty");
        return false;
    }

    if (!QFileInfo::exists("/boot/vmlinuz-" + kernel)) {
        qCritical() << QObject::tr("Kernel file not found: /boot/vmlinuz-%1").arg(kernel);
        return false;
    }

    // Check if SQUASHFS is available in kernel (skip on Arch where /boot/config-* is absent)
    if (!isArch
        && QProcess::execute("grep", {"-q", "^CONFIG_SQUASHFS=[ym]", "/boot/config-" + kernel}) != 0) {
        qCritical() << QObject::tr("Kernel %1 doesn't support Squashfs").arg(kernel);
        return false;
    }

    // Validate exclusions
    if (!validateExclusions()) {
        return false;
    }

#ifdef CLI_BUILD
    // For CLI-only builds, always validate space requirements at startup
    if (!validateSpaceRequirements()) {
        return false;
    }
#else
    // Validate space requirements only when running in CLI
    if (!isGuiApp && !validateSpaceRequirements()) {
        return false;
    }
#endif

    qDebug() << "Configuration validation passed";
    return true;
}

QString Settings::snapshotNameError(const QString &name)
{
    QString baseName = name;
    if (baseName.endsWith(".iso")) {
        baseName.chop(4);
    }
    if (baseName.trimmed().isEmpty()) {
        return QObject::tr("Snapshot name cannot be empty");
    }
    static const QRegularExpression invalidCharacters("[<>:\"/\\|?*]");
    if (name.contains(invalidCharacters)) {
        return QObject::tr("Snapshot name contains invalid characters: %1").arg(name);
    }
    return {};
}

bool Settings::validateExclusions() const
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    // Check if exclusion file exists and is readable
    if (!snapshotExcludes.fileName().isEmpty() && !snapshotExcludes.exists()) {
        qCritical() << QObject::tr("Exclusion file does not exist: %1").arg(snapshotExcludes.fileName());
        return false;
    }

    // Validate session exclusions format
    if (!sessionExcludes.isEmpty()) {
        // Check for balanced quotes
        const int quoteCount = sessionExcludes.count('"');
        if (quoteCount % 2 != 0) {
            qCritical() << QObject::tr("Unbalanced quotes in exclusion list");
            return false;
        }
    }

    qDebug() << "Exclusion validation passed";
    return true;
}

bool Settings::validateSpaceRequirements() const
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    // Check if we have minimum free space (at least 1GB)
    constexpr quint64 MIN_FREE_SPACE = 1024 * 1024; // 1GB in KiB

    // Get free space for the snapshot directory, or for its nearest existing
    // ancestor: checkSnapshotDir() creates the missing levels later (mkdir -p).
    QString pathToCheck = QDir::cleanPath(QFileInfo(snapshotDir).absoluteFilePath());
    while (!QFileInfo::exists(pathToCheck)) {
        const QString parent = QFileInfo(pathToCheck).absolutePath();
        if (parent == pathToCheck) {
            break;
        }
        pathToCheck = parent;
    }

    const quint64 availableSpace = FileSystemUtils::getFreeSpace(pathToCheck);
    if (availableSpace < MIN_FREE_SPACE) {
        qCritical() << QObject::tr("Insufficient free space: %1 KiB available, minimum %2 KiB required")
                           .arg(availableSpace).arg(MIN_FREE_SPACE);
        return false;
    }

    // Only check work directory space if it has been initialized (checkTempDir() called)
    if (freeSpaceWork > 0 && freeSpaceWork < MIN_FREE_SPACE) {
        qCritical() << QObject::tr("Insufficient free space in work directory: %1 KiB available, minimum %2 KiB required")
                           .arg(freeSpaceWork).arg(MIN_FREE_SPACE);
        return false;
    }

    qDebug() << "Space requirements validation passed";
    return true;
}

bool Settings::initializeConfiguration()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    // Validate maxCores
    if (maxCores == 0) {
        qCritical() << QObject::tr("Failed to determine number of CPU cores");
        return false;
    }

    // Check if config file exists and is readable
    if (!configFile.exists()) {
        qWarning() << QObject::tr("Configuration file does not exist: %1").arg(configFile.fileName());
        qWarning() << QObject::tr("Using default settings");
    } else if (!configFile.open(QIODevice::ReadOnly)) {
        qCritical() << QObject::tr("Cannot read configuration file: %1").arg(configFile.fileName());
        qCritical() << QObject::tr("Error: %1").arg(configFile.errorString());
        return false;
    } else {
        configFile.close();
    }

    // Check for required system tools
    QStringList requiredTools = {"mksquashfs", "xorriso", "lslogins"};
    for (const QString &tool : requiredTools) {
        if (QProcess::execute("which", {tool}) != 0) {
            qCritical() << QObject::tr("Required tool not found: %1").arg(tool);
            return false;
        }
    }

    // Check for required directories
    QStringList requiredDirs = {"/boot", "/etc", "/usr/lib"};
    for (const QString &dir : requiredDirs) {
        if (!QDir(dir).exists()) {
            qCritical() << QObject::tr("Required directory not found: %1").arg(dir);
            return false;
        }
    }

    qDebug() << "Configuration initialization passed";
    return true;
}

void Settings::handleInitializationError(const QString &error) const
{
    qCritical() << "Settings initialization error:" << error;

    // Log to system log if available
    if (QFile::exists("/usr/bin/logger")) {
        QProcess::execute("logger", {"-t", qApp->applicationName(), "Settings initialization error: " + error});
    }

    // Show error message appropriate for current mode (GUI or CLI)
    MessageHandler::showMessage(MessageHandler::Critical, QObject::tr("Initialization Error"),
                               QObject::tr("Failed to initialize application settings:\n\n%1").arg(error));
}

QStringList Settings::getEditorCommand() const
{
    // Parse a configured or .desktop-provided editor command into an argument
    // list. QProcess::splitCommand honours quotes but performs no shell
    // evaluation, and the executable must resolve on PATH — so a crafted
    // gui_editor value or Exec= line yields arguments, never shell syntax.
    // Callers execute the returned list directly via QProcess (no bash -c),
    // which is what makes running it from a root process safe against
    // injection through the configuration.
    const auto resolveCommand = [this](const QString &commandLine) -> QStringList {
        QStringList args = QProcess::splitCommand(commandLine);
        // Drop .desktop Exec field codes (%f, %U, ...) and the historical -b flag.
        static const QRegularExpression fieldCode("^%[a-zA-Z]$");
        args.removeIf([](const QString &arg) { return arg == "-b" || fieldCode.match(arg).hasMatch(); });
        if (args.isEmpty()) {
            return {};
        }
        const QString executable = QStandardPaths::findExecutable(args.constFirst(), path);
        if (executable.isEmpty()) {
            return {};
        }
        args[0] = executable;
        return args;
    };

    QStringList editor = resolveCommand(guiEditor);
    if (editor.isEmpty()) {
        const QString defaultEditor = Cmd().getOut("xdg-mime query default text/plain");
        const QString desktopFile
            = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, defaultEditor, QStandardPaths::LocateFile);
        QFile file(desktopFile);
        if (file.open(QIODevice::ReadOnly)) {
            while (!file.atEnd()) {
                const QString line = file.readLine();
                if (line.startsWith("Exec=")) {
                    editor = resolveCommand(line.mid(QStringLiteral("Exec=").size()).trimmed());
                    break;
                }
            }
            file.close();
        }
    }
    if (editor.isEmpty()) { // Use nano as backup editor
        editor = resolveCommand(QStringLiteral("nano"));
        if (editor.isEmpty()) {
            editor = QStringList {QStringLiteral("nano")};
        }
    }

    const QString editorName = QFileInfo(editor.constFirst()).fileName();
    const bool isRoot = getuid() == 0;
    const bool isEditorThatElevates
        = QRegularExpression(R"((kate|kwrite|featherpad|code|codium)$)").match(editorName).hasMatch();
    const bool isCliEditor = QRegularExpression(R"(nano|vi|vim|nvim|micro|emacs)").match(editorName).hasMatch();

    if (isEditorThatElevates && !isRoot) {
        return editor;
    }

    // Only elevate when the process itself is already root (CLI run via sudo),
    // where the target files were created root-owned. In a normal unprivileged
    // GUI session the files are already owned by the invoking user, so
    // elevating here would just force an unnecessary pkexec/sudo prompt.
    QStringList elevate;
    if (isRoot) {
        const QString elevationTool = Cmd::elevationTool();
        if (!elevationTool.isEmpty()) {
            elevate << elevationTool;
            if (isEditorThatElevates) {
                // Editors that elevate internally are dropped back to the
                // logged-in user; adjust the user switch flag per tool.
                const QString username = Cmd::loggedInUserName();
                if (!username.isEmpty()) {
                    elevate << (elevationTool.contains("sudo") ? QStringLiteral("-u") : QStringLiteral("--user"))
                            << username;
                }
            }
        }
    }

    if (isCliEditor) {
        return QStringList {QStringLiteral("x-terminal-emulator"), QStringLiteral("-e")} + elevate + editor;
    }
    // GUI editors need the invoking session's display across the elevation
    // boundary; forward the current values as literal env arguments.
    return elevate
           + QStringList {QStringLiteral("env"), "DISPLAY=" + qEnvironmentVariable("DISPLAY"),
                          "XAUTHORITY=" + qEnvironmentVariable("XAUTHORITY")}
           + editor;
}

// Return the size of the snapshot folder in MiB
QString Settings::getSnapshotSize() const
{
    qint64 totalSize = 0;
    if (QFileInfo::exists(snapshotDir)) {
        QDir directory(snapshotDir);
        QStringList isoFiles = directory.entryList(QStringList() << "*.iso", QDir::Files);
        for (const QString &file : isoFiles) {
            totalSize += QFileInfo(directory.absoluteFilePath(file)).size();
        }
    }
    return QString::number(totalSize / (1024 * 1024)) + "MiB";
}

// Number of snapshots in snapshot_dir
int Settings::getSnapshotCount() const
{
    if (QFileInfo::exists(snapshotDir)) {
        QFileInfoList list = QDir(snapshotDir).entryInfoList(QStringList("*.iso"), QDir::Files);
        return list.size();
    }
    return 0;
}



// Include localized and relocated XDG directories alongside the default exclusions.
QString Settings::getXdgUserDirs(const QString &folder)
{
    QStringList resultParts;
    resultParts.reserve(18); // For 3 users x 6 folders, not worth getting the number of users on the system

    const static QHash<QString, QString> englishDirs {
        {"DOCUMENTS", "Documents"}, {"DOWNLOAD", "Downloads"}, {"DESKTOP", "Desktop"},
        {"MUSIC", "Music"},         {"PICTURES", "Pictures"},  {"VIDEOS", "Videos"},
    };
    for (const QString &user : std::as_const(users)) {
        QString dir
            = Cmd().getOutAsRoot("runuser", {"-u", user, "--", "/usr/bin/xdg-user-dir", folder}, Cmd::QuietMode::Yes);
        const QStringList lines = dir.split('\n', Qt::SkipEmptyParts);
        dir.clear();
        for (const QString &line : lines) {
            const QString candidate = line.trimmed();
            if (candidate.startsWith('/')) {
                dir = QDir::cleanPath(candidate);
                break;
            }
        }
        const QString home = "/home/" + user;
        if (!dir.isEmpty() && dir != home && dir != home + '/' + englishDirs.value(folder)) {
            dir.remove(QRegularExpression("^/"));
            QString exclusion = folder == "DESKTOP" ? "/!(minstall.desktop)" : "/*\" \"" + dir + "/.*";
            dir.append(exclusion);
            resultParts << dir;
        }
    }
    QString result = resultParts.join("\" \"");
    return result.isEmpty() ? QString() : "\" \"" + result;
}

void Settings::selectKernel()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    kernel.remove(QRegularExpression("^/boot/vmlinuz-"));
    if (kernel.isEmpty() || !QFileInfo::exists("/boot/vmlinuz-" + kernel)) {
        kernel = currentKernel;
        if (!QFileInfo::exists("/boot/vmlinuz-" + kernel)) {
            QDir directory("/boot");
            QStringList vmlinuzFiles = directory.entryList(QStringList() << "vmlinuz-*", QDir::Files, QDir::Name);
            if (!vmlinuzFiles.isEmpty()) {
                kernel = vmlinuzFiles.last().remove(QRegularExpression("^vmlinuz-"));
            }
            if (!QFileInfo::exists("/boot/vmlinuz-" + kernel)) {
                QString message = QObject::tr("Could not find a usable kernel");
                QString details = QObject::tr("Searched for kernel files in /boot/ but none were found or accessible.");
                MessageHandler::showMessage(MessageHandler::Critical, QObject::tr("Error"), message + "\n\n" + details);
                throw std::runtime_error("Could not find a usable kernel");
            }
        }
    }
    // Check if SQUASHFS is available (skip on Arch — kernel config not shipped)
    if (!isArch
        && QProcess::execute("grep", {"-q", "^CONFIG_SQUASHFS=[ym]", "/boot/config-" + kernel}) != 0) {
        QString message = QObject::tr("Current kernel doesn't support Squashfs, cannot continue.");
        MessageHandler::showMessage(MessageHandler::Critical, QObject::tr("Error"), message);
        throw std::runtime_error("Current kernel doesn't support Squashfs");
    }
}

void Settings::setVariables()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    try {
        // live and users are now const members initialized in constructor
        if (users.isEmpty()) {
            qWarning() << QObject::tr("No users found in the system");
        }
    } catch (...) {
        qCritical() << QObject::tr("Failed to determine system information");
        throw;
    }

    QString distroVersionFile;
    if (QFileInfo::exists("/etc/mx-version")) {
        distroVersionFile = "/etc/mx-version";
    } else if (QFileInfo::exists("/etc/antix-version")) {
        distroVersionFile = "/etc/antix-version";
    }

    QHash<QString, QString> lsbRelease;
    QFile lsbFile(QStringLiteral("/etc/lsb-release"));
    if (lsbFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!lsbFile.atEnd()) {
            const QString line = QString::fromUtf8(lsbFile.readLine()).trimmed();
            const int eq = line.indexOf('=');
            if (eq > 0) {
                QString value = line.mid(eq + 1);
                if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"')) {
                    value = value.mid(1, value.size() - 2);
                }
                lsbRelease.insert(line.left(eq), value);
            }
        }
        lsbFile.close();
    }

    const QString osName = readOsReleaseValue("NAME");
    const QString osVersionId = readOsReleaseValue("VERSION_ID");
    const QString osVersionCodename = readOsReleaseValue("VERSION_CODENAME");
#ifdef ARCH_BUILD
    isArch = true;
#else
    isArch = osName.contains("Arch", Qt::CaseInsensitive);
#endif

    if (lsbRelease.contains("DISTRIB_ID")) {
        projectName = lsbRelease.value("DISTRIB_ID");
    } else if (!osName.isEmpty()) {
        projectName = osName;
    } else {
        projectName = Cmd().getOut("lsb_release -is");
    }
    projectName.remove('"');

    if (!distroVersionFile.isEmpty()) {
        QFile versionFile(distroVersionFile);
        if (versionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            distroVersion = QString::fromUtf8(versionFile.readLine()).section('_', 0, 0).trimmed();
            versionFile.close();
        }
        distroVersion.remove(QRegularExpression("^" + projectName + "[_-]"));
    } else if (!osVersionId.isEmpty()) {
        distroVersion = osVersionId;
    } else {
        distroVersion = Cmd().getOut("lsb_release -rs");
    }

    fullDistroName = projectName + "-" + distroVersion + "_" + QString(x86 ? "386" : "x64");
    releaseDate = QDate::currentDate().toString("MMMM dd, yyyy");

    if (codename.isEmpty()) {
        if (lsbRelease.contains("DISTRIB_CODENAME")) {
            codename = lsbRelease.value("DISTRIB_CODENAME");
        } else if (!osVersionCodename.isEmpty()) {
            codename = osVersionCodename;
        } else if (isArch) {
            codename = "rolling";
        } else {
            codename = Cmd().getOut("lsb_release -cs");
        }
        codename.remove('"');
    }

    bootOptions = monthly ? "quiet splasht nosplash" : SystemInfo::readKernelOpts();
    if (isArch) {
        bootOptions = ensureCowSpacesize(bootOptions);
    }
}

QString Settings::getFilename() const
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (stamp == "datetime") {
        return snapshotBasename + "-" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmm") + ".iso";
    } else {
        QString name;
        QDir dir;
        int n = 1;
        do {
            name = snapshotBasename + QString::number(n) + ".iso";
            dir.setPath(snapshotDir + '/' + name);
            n++;
        } while (QFileInfo::exists(dir.absolutePath()));
        return name;
    }
}

quint64 Settings::getLiveRootSpace()
{
    // rootspaceneeded is the size of the linuxfs file * a compression factor + contents of the rootfs, conservative
    // but fast factors are same as used in live-remaster

    // The MX live layout (/live/config/initrd.out, /live/boot-dev/antiX/linuxfs,
    // /live/linux) doesn't exist on Arch live media, where SystemInfo::isLive()
    // also returns true. Return 0 so the caller can fall back to a generic
    // estimate instead of reading defaults that point at non-existent files.
    if (!QFileInfo::exists("/live/config/initrd.out") && !QFileInfo::exists("/live/linux")) {
        return 0;
    }

    // Load some live variables
    QSettings livesettings("/live/config/initrd.out", QSettings::NativeFormat);
    QString sqfile_full = livesettings.value("SQFILE_FULL", "/live/boot-dev/antiX/linuxfs").toString();
    QString toram_mp = livesettings.value("TORAM_MP", "/live/to-ram").toString();
    QString sqfile_path = livesettings.value("SQFILE_PATH", "antiX").toString().remove(QRegularExpression("^/+"));
    QString sqfile_name = livesettings.value("SQFILE_NAME", "linuxfs").toString();
    if (!toram_mp.isEmpty() && QFileInfo::exists(toram_mp + "/" + sqfile_path + "/" + sqfile_name)) {
        sqfile_full = toram_mp + "/" + sqfile_path + "/" + sqfile_name;
    }

    // Get compression factor by reading the linuxfs squashfs file, if available:
    // the superblock stores the compression id as a little-endian u16 at offset
    // 20. Read it directly — sqfile_full comes from live-media config and must
    // not be interpolated into a shell pipeline (the old dd|od approach).
    QString linuxfs_compression_type;
    QFile sqfile(sqfile_full);
    if (sqfile.open(QIODevice::ReadOnly) && sqfile.seek(20)) {
        const QByteArray bytes = sqfile.read(2);
        if (bytes.size() == 2) {
            const quint16 id = static_cast<quint8>(bytes.at(0))
                               | (static_cast<quint16>(static_cast<quint8>(bytes.at(1))) << 8);
            linuxfs_compression_type = QString::number(id);
        }
    }
    constexpr quint8 default_factor = 30;
    quint8 c_factor = default_factor;
    // gzip, xz, or lz4
    QMap<QString, QString> compression_types
        = {{"1", "gzip"}, {"2", "lzo"}, {"3", "lzma"}, {"4", "xz"}, {"5", "lz4"}, {"6", "zstd"}};
    if (compression_types.contains(linuxfs_compression_type)) {
        c_factor = compressionFactor.value(compression_types.value(linuxfs_compression_type));
    } else {
        qWarning() << "Unknown compression type:" << linuxfs_compression_type;
    }
    quint64 rootfs_file_size = 0;
    quint64 linuxfs_file_size = QStorageInfo("/live/linux/").bytesTotal() * 100 / c_factor;
    if (QFileInfo::exists("/live/persist-root")) {
        rootfs_file_size = QStorageInfo("/live/persist-root/").bytesTotal();
    }

    // Add rootfs file size to the calculated linuxfs file size.  probaby conservative, as rootfs will likely have some
    // overlap with linuxfs
    return linuxfs_file_size + rootfs_file_size;
}

QString Settings::getUsedSpace()
{
    constexpr double factor = 1024 * 1024 * 1024;
    QString out = "\n- " + QObject::tr("Used space on / (root): ");
    bool estimated = false;
    if (live) {
        rootSize = getLiveRootSpace();
        estimated = true;
    }
    // Recalculate every time for installed systems (rootSize is a member that
    // would otherwise hold stale data across calls); also fall back here when
    // getLiveRootSpace returned 0 on non-MX live media (e.g. Arch live), still
    // labeled "estimated" because live root sizing is fundamentally an estimate.
    if (!live || rootSize == 0) {
        QStorageInfo rootInfo("/");
        rootSize = rootInfo.bytesTotal() - rootInfo.bytesFree();
    }
    out += QString::number(static_cast<double>(rootSize) / factor, 'f', 2) + "GiB";
    if (estimated) {
        out += " -- " + QObject::tr("estimated");
    }
    // isRoot() is true exactly when /home shares the root filesystem — the
    // separate-partition case (worth reporting) is !isRoot(), not isRoot().
    QStorageInfo homeInfo("/home/");
    if (homeInfo.isValid() && !homeInfo.isRoot()) {
        homeSize = homeInfo.bytesTotal() - homeInfo.bytesFree();
        out.append("\n- " + QObject::tr("Used space on /home: ")
                   + QString::number(static_cast<double>(homeSize) / factor, 'f', 2) + "GiB");
    } else {
        homeSize = 0; // /home on root
    }
    return out;
}



int Settings::getDebianVerNum()
{
    QFile file("/etc/debian_version");
    if (!file.exists()) {
        // Non-Debian system (Arch, etc.); return a recent version so callers
        // gating mksquashfs feature checks behave sensibly.
        return Release::Bookworm;
    }
    QStringList list;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString line = in.readLine();
        list = line.split('.');
        file.close();
    } else {
        qCritical() << "Could not open /etc/debian_version:" << file.errorString() << "Assumes Bookworm";
        return Release::Bookworm;
    }
    bool ok = false;
    int ver = list.at(0).toInt(&ok);
    if (ok) {
        return ver;
    } else {
        QString verName = list.at(0).split('/').at(0);
        if (verName == "bullseye") {
            return Release::Bullseye;
        } else if (verName == "bookworm") {
            return Release::Bookworm;
        } else if (verName == "trixie") {
            return Release::Trixie;
        } else if (verName == "forky") {
            return Release::Forky;
        } else if (verName == "duke") {
            return Release::Duke;
        } else {
            qCritical() << "Unknown Debian version:" << ver << "Assumes Bullseye";
            return Release::Bullseye;
        }
    }
}





QString Settings::getFreeSpaceStrings(const QString &path)
{
    constexpr float factor = 1024 * 1024;
    freeSpace = FileSystemUtils::getFreeSpace(path);
    QString out = QString::number(static_cast<double>(freeSpace) / factor, 'f', 2) + "GiB";

    qDebug().noquote() << QString("- " + QObject::tr("Free space on %1, where snapshot folder is placed: ").arg(path)
                                  + out)
                       << '\n';

    qDebug().noquote() << QObject::tr(
                              "The free space should be sufficient to hold the compressed data from / and /home\n\n"
                              "      If necessary, you can create more available space\n"
                              "      by removing previous snapshots and saved copies:\n"
                              "      %1 snapshots are taking up %2 of disk space.\n")
                              .arg(QString::number(getSnapshotCount()), getSnapshotSize());
    return out;
}



void Settings::excludeItem(const QString &item)
{
    static const QMap<QString, void (Settings::*)(bool)> itemExclusions {
        {QStringLiteral("Desktop"), &Settings::excludeDesktop},
        {QStringLiteral("Documents"), &Settings::excludeDocuments},
        {QStringLiteral("Downloads"), &Settings::excludeDownloads},
        {QStringLiteral("Flatpaks"), &Settings::excludeFlatpaks},
        {QStringLiteral("Music"), &Settings::excludeMusic},
        {QStringLiteral("Networks"), &Settings::excludeNetworks},
        {QStringLiteral("Pictures"), &Settings::excludePictures},
        {QStringLiteral("Steam"), &Settings::excludeSteam},
        {QStringLiteral("Videos"), &Settings::excludeVideos},
        {QStringLiteral("VirtualBox"), &Settings::excludeVirtualBox}};

    auto it = itemExclusions.find(item);
    if (it != itemExclusions.end()) {
        (this->*(it.value()))(true);
    }
}

void Settings::excludeUserFolder(bool exclude, Exclude flag, const QString &folderName, const QString &xdgKey)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(flag);
    }
    const QString folder = "home/*/" + folderName + "/";
    const QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdgKey);
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeDesktop(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Desktop);
    }
    QString exclusion = "/home/*/Desktop/!(minstall.desktop)" + getXdgUserDirs("DESKTOP");
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeDocuments(bool exclude)
{
    excludeUserFolder(exclude, Exclude::Documents, "Documents", "DOCUMENTS");
}

void Settings::excludeDownloads(bool exclude)
{
    excludeUserFolder(exclude, Exclude::Downloads, "Downloads", "DOWNLOAD");
}

void Settings::excludeFlatpaks(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Flatpaks);
    }
    QString exclusion = "home/*/.local/share/flatpak/*\" \"home/*/.local/share/flatpak/.*\" "
                        "\"var/lib/flatpak/*\" \"var/lib/flatpak/.*\" "
                        "\"home/*/.var/app/*\" \"home/*/.var/app/.*";
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeMusic(bool exclude)
{
    excludeUserFolder(exclude, Exclude::Music, "Music", "MUSIC");
}

void Settings::excludeNetworks(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Networks);
    }
    addRemoveExclusion(exclude, QStringLiteral("/etc/NetworkManager/system-connections/*"));
    addRemoveExclusion(exclude, QStringLiteral("/etc/wicd/*"));
    addRemoveExclusion(exclude, QStringLiteral("/var/lib/connman/*"));
    addRemoveExclusion(exclude, QStringLiteral("/var/lib/iwd/*"));
    addRemoveExclusion(exclude, QStringLiteral("/etc/wpa_supplicant/wpa_supplicant*.conf"));
}

void Settings::excludePictures(bool exclude)
{
    excludeUserFolder(exclude, Exclude::Pictures, "Pictures", "PICTURES");
}

void Settings::excludeSteam(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Steam);
    }
    addRemoveExclusion(exclude, QStringLiteral("home/*/.steam"));
    addRemoveExclusion(exclude, QStringLiteral("home/*/.local/share/Steam"));
}

void Settings::excludeSwapFile()
{
    QFile file("/etc/fstab");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open /etc/fstab";
        return;
    }
    QByteArray content = file.readAll();
    QStringList lines = QString::fromUtf8(content).split('\n');

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.startsWith('/') && !trimmedLine.startsWith("/dev/")) {
            QStringList parts = trimmedLine.split(QRegularExpression("\\s+"));
            if (parts.size() > 3 && parts.at(2) == "swap") {
                addRemoveExclusion(true, parts[0].remove(0, 1));
            }
        }
    }
}

void Settings::excludeVideos(bool exclude)
{
    excludeUserFolder(exclude, Exclude::Videos, "Videos", "VIDEOS");
}

void Settings::excludeVirtualBox(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::VirtualBox);
    }
    addRemoveExclusion(exclude, QStringLiteral("home/*/VirtualBox VMs"));
}

// Load settings from config file
void Settings::loadConfig(const QString &compressionOptions)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    QSettings settingsSystem(configFile.fileName(), QSettings::IniFormat);
    if (settingsSystem.status() != QSettings::NoError) {
        qWarning() << QObject::tr("Error reading system configuration file: %1").arg(configFile.fileName());
    }

    // QSettings' user-scope path was already redirected to the logged-in user's
    // ~/.config by userConfigPathReady's initializer, before this constructor
    // body even started running.
    const QString configDir = userConfigBaseDir();
    QSettings settingsUser;
    if (settingsUser.status() != QSettings::NoError) {
        qWarning() << QObject::tr("Error accessing user configuration");
    }

    const QString systemSnapshotExcludes = trimQuotes(settingsSystem.value("snapshot_excludes").toString());
    const bool userConfiguredSnapshotExcludes = settingsUser.contains("snapshot_excludes");

    // Read all keys from system settings
    settingsSystem.beginGroup("");
    QStringList systemKeys = settingsSystem.allKeys();
    settingsSystem.endGroup();

    // Merge system settings into user settings
    for (const QString &key : std::as_const(systemKeys)) {
        if (key != "mksq_opt" && !settingsUser.contains(key)) {
            QVariant value = settingsSystem.value(key);
            settingsUser.setValue(key, value);
        }
    }

    sessionExcludes.clear();
    snapshotDir = trimQuotes(settingsUser.value("snapshot_dir", "/home/snapshot").toString());
    if (!snapshotDir.endsWith("/snapshot")) {
        snapshotDir = QDir::cleanPath(snapshotDir + "/snapshot");
    }
    const QString userConfigDir = QDir::cleanPath(configDir + "/" + QCoreApplication::organizationName());
    const QString userExcludesPath =
        QDir::cleanPath(userConfigDir + "/" + qApp->applicationName() + "-exclude.list");
    const QString userConfigPath = settingsUser.fileName();
    // Create and repair the directory even when the exclude list already exists
    // or a custom list is selected. QSettings may have created it as root while
    // reading the initial settings earlier in the constructor.
    if (!QDir().mkpath(userConfigDir)) {
        qWarning() << QObject::tr("Could not create user configuration directory: %1").arg(userConfigDir);
    }
    chownUserConfigPath(userConfigDir, userConfigDir);
    const QString systemExcludesPath = QDir::cleanPath("/etc/" + qApp->applicationName() + "-exclude.list");
    QString localPath = QDir::cleanPath("/usr/local/share/excludes/" + qApp->applicationName() + "-exclude.list");
    QString usrPath = QDir::cleanPath("/usr/share/excludes/" + qApp->applicationName() + "-exclude.list");
    const QString fallbackExcludesPath = QFileInfo::exists(localPath) ? localPath : usrPath;
    excludesSourcePath = QFileInfo::exists(systemExcludesPath) ? systemExcludesPath : fallbackExcludesPath;
    QString configuredExcludesPath = trimQuotes(settingsUser.value("snapshot_excludes", userExcludesPath).toString());

    if (!userConfiguredSnapshotExcludes || configuredExcludesPath == systemSnapshotExcludes) {
        configuredExcludesPath = userExcludesPath;
        settingsUser.setValue("snapshot_excludes", configuredExcludesPath);
    }

    const bool usingDefaultUserPath = configuredExcludesPath == userExcludesPath;
    if (usingDefaultUserPath && !QFileInfo::exists(userExcludesPath)) {
        if (!excludesSourcePath.isEmpty() && QFileInfo::exists(excludesSourcePath)) {
            if (QFile::copy(excludesSourcePath, userExcludesPath)) {
                qDebug() << "Copied exclusion file from" << excludesSourcePath << "to" << userExcludesPath;
            } else {
                qWarning() << QObject::tr("Could not copy exclusion file from %1 to %2")
                                  .arg(excludesSourcePath, userExcludesPath);
                configuredExcludesPath = excludesSourcePath;
            }
        }
    }
    if (!QFileInfo::exists(configuredExcludesPath)) {
        qDebug() << "Configured snapshot_excludes file not found (" << configuredExcludesPath
                 << "), using fallback path:" << excludesSourcePath;
        configuredExcludesPath = excludesSourcePath;
    }
    snapshotExcludes.setFileName(configuredExcludesPath);
    // snapshotBasename, makeIsohybrid, guiEditor, stamp, forceInstaller are now const members
    makeMd5sum = settingsUser.value("make_md5sum", "no").toString() != "no";
    // The ISO checksum switched from SHA-512 to SHA-256; honour the old key
    // until the user saves the new one.
    makeSha256sum = settingsUser.value("make_sha256sum", settingsUser.value("make_sha512sum", "no")).toString() != "no";
    compression = trimQuotes(settingsUser.value("compression", "zstd").toString());
    // The CLI path validates --compression in main.cpp; the config file value
    // was taken on trust. Reject anything outside the supported set so a
    // malformed value never reaches mksquashfs or the kernel-config probe.
    static const QStringList allowedCompression {"lz4", "lzo", "gzip", "xz", "zstd"};
    if (!allowedCompression.contains(compression)) {
        qWarning().noquote() << QObject::tr("Unsupported compression '%1' in configuration, using zstd.").arg(compression);
        compression = "zstd";
    }
    const QString systemOptions = trimQuotes(settingsSystem.value("mksq_opt").toString());
    const QString preference = trimQuotes(settingsUser.value("mksq_opt").toString());
    struct stat st {};
    const QString path = configFile.fileName();
    const bool trusted = (path == "/etc/mx-snapshot.conf" || path == "/etc/iso-snapshot-cli.conf")
                         && lstat(QFile::encodeName(path).constData(), &st) == 0 && S_ISREG(st.st_mode)
                         && st.st_uid == 0 && (st.st_mode & (S_IWGRP | S_IWOTH)) == 0;
    const QString trustedOptions = trusted ? systemOptions : QString();
    const bool useUntrustedConfig = !trusted && preference.isEmpty();
    const QString userOptions = useUntrustedConfig ? systemOptions : preference;
    auto options = SquashfsUtils::selectExtraOptions(trustedOptions, userOptions, compressionOptions);
    if (!options) {
        if (!compressionOptions.isEmpty()) {
            throw std::runtime_error(QObject::tr("Unsupported --compression-level value: %1")
                                         .arg(compressionOptions).toStdString());
        }
        qWarning() << QObject::tr("Ignoring unsupported mksq_opt in %1: %2")
                          .arg(useUntrustedConfig ? path : userConfigPath, userOptions);
        options = SquashfsUtils::selectExtraOptions(trustedOptions, {}, {});
    }
    mksqOpt = *options;
    // edit_boot_menu is now const, initialized in constructor
    tempDirParent = trimQuotes(settingsUser.value("workdir").toString());

    const QVariant coresValue = settingsUser.value("cores", maxCores);
    uint storedCores = coresValue.toUInt();
    if (storedCores == 0 || storedCores > maxCores) {
        qWarning() << QObject::tr("Invalid stored cores setting (%1). Using detected CPU count: %2")
                          .arg(coresValue.toString()).arg(maxCores);
        storedCores = maxCores;
        settingsUser.setValue("cores", storedCores);
    }
    cores = storedCores;

    const QVariant throttleValue = settingsUser.value("throttle", 0);
    uint storedThrottle = throttleValue.toUInt();
    if (storedThrottle > 99) {
        qWarning() << QObject::tr("Invalid stored throttle setting (%1). Using 0.").arg(throttleValue.toString());
        storedThrottle = 0;
        settingsUser.setValue("throttle", storedThrottle);
    }
    throttle = storedThrottle;
    resetAccounts = false;

    // Sync before repairing file ownership: QSettings uses atomic replacement,
    // so its final write can create a new root-owned inode.
    settingsUser.sync();
    chownUserConfigPath(userConfigPath, userConfigDir);
    chownUserConfigPath(configuredExcludesPath, userConfigDir);
}

void Settings::excludeAll()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    excludeDesktop(true);
    excludeDocuments(true);
    excludeDownloads(true);
    excludeFlatpaks(true);
    excludeMusic(true);
    excludeNetworks(true);
    excludePictures(true);
    excludeSteam(true);
    excludeVideos(true);
    excludeVirtualBox(true);
}

void Settings::otherExclusions()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // Add exclusions snapshot and work dirs
    addRemoveExclusion(true, snapshotDir);
    addRemoveExclusion(true, workDir);

    if (resetAccounts) {
        addRemoveExclusion(true, QStringLiteral("/etc/minstall.conf"));
        // Host identity and root's private data must not be shared by every
        // system made from a respin. mksquashfs wildcards don't match dotfiles
        // with "*", so directories get a second pattern for those.
        //
        // root's home is the admin's (keys, tokens, shell history, tool
        // logins), so keep only the shell startup files, which hold the
        // distro's defaults.
        const QStringList resetExcludes {
            QStringLiteral("var/lib/systemd/random-seed"),
            QStringLiteral("var/lib/tailscale/*"),
            QStringLiteral("var/lib/tailscale/.*"),
            QStringLiteral("root/*"),
            QStringLiteral("root/.!(bashrc|profile|bash_profile|bash_logout)"),
            // snapd's store registration; Work::resetSnapdIdentity() drops the
            // matching entries from its state, so snapd registers anew.
            QStringLiteral("var/lib/snapd/device/private-keys-v1/*"),
            QStringLiteral("var/lib/snapd/assertions/asserts-v0/serial/*"),
        };
        for (const QString &exclusion : resetExcludes) {
            addRemoveExclusion(true, exclusion);
        }
        if (isArch) {
            // Arch's sshdgenkeys.service recreates missing host keys before
            // sshd starts; Debian's ssh.service does not, so keep them there.
            addRemoveExclusion(true, QStringLiteral("etc/ssh/ssh_host_*"));
        }
        if (!isArch) {
            // Exclude /etc/localtime if link and timezone not America/New_York.
            // Skipped on Arch — /etc/timezone is absent and the heuristic doesn't apply.
            QFileInfo localtimeInfo("/etc/localtime");
            QFile timezoneFile("/etc/timezone");
            if (localtimeInfo.isSymLink()) {
                if (timezoneFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&timezoneFile);
                    QString timezone = in.readLine();
                    if (timezone != "America/New_York") {
                        addRemoveExclusion(true, "/etc/localtime");
                    }
                    timezoneFile.close();
                }
            }
        }
    }
    excludeSwapFile();
}

void Settings::processArgs(const QCommandLineParser &argParser)
{
    shutdown = argParser.isSet("shutdown");
    const QString kernelArg = argParser.value("kernel");
    if (!kernelArg.isEmpty()) {
        kernel = kernelArg;
    }
    // preempt is now const, initialized in constructor
    QDir dir;
    if (!argParser.value("directory").isEmpty()) {
        const QString directory = argParser.value("directory");
        if (QFileInfo::exists(directory)) {
            dir.setPath(directory);
            snapshotDir = dir.absolutePath() + "/snapshot";
        } else {
            qWarning() << "Warning: Specified directory does not exist:" << directory;
            qWarning() << "Using default snapshot directory:" << snapshotDir;
        }
    }

    if (!argParser.value("workdir").isEmpty()) {
        const QString workdir = argParser.value("workdir");
        if (QFileInfo::exists(workdir)) {
            dir.setPath(workdir);
            tempDirParent = dir.absolutePath();
        } else {
            qWarning() << "Warning: Specified work directory does not exist:" << workdir;
            qWarning() << "Using default work directory:" << tempDirParent;
        }
    }

    if (!argParser.value(QStringLiteral("file")).isEmpty()) {
        const auto fileArg = argParser.value("file");
        snapshotName = fileArg + (fileArg.endsWith(".iso") ? QString() : ".iso");
    } else {
        snapshotName = getFilename();
    }
    if (QFile::exists(snapshotDir + '/' + snapshotName)) {
        QString message
            = QObject::tr("Output file %1 already exists. Please use another file name, or delete the existent file.")
                  .arg(snapshotDir + '/' + snapshotName);
        MessageHandler::showMessage(MessageHandler::Critical, QObject::tr("Error"), message);
        throw std::runtime_error("Output file already exists");
    }
    resetAccounts = argParser.isSet("reset");
    if (resetAccounts) {
        excludeAll();
    }
    if (argParser.isSet("month")) {
        resetAccounts = true;
    }
    if (argParser.isSet("checksums")) {
        makeSha256sum = makeMd5sum = true;
    }
    if (argParser.isSet("month")) {
        makeSha256sum = true;
        makeMd5sum = false;
    }
    if (argParser.isSet("no-checksums")) {
        makeSha256sum = makeMd5sum = false;
    }
    if (!argParser.value("compression").isEmpty()) {
        compression = argParser.value("compression");
    }
    if (!argParser.value("cores").isEmpty()) {
        bool ok {false};
        const uint val = argParser.value("cores").toUInt(&ok);
        if (!ok || val == 0 || val > maxCores) {
            qWarning() << "Invalid cores value:" << argParser.value("cores")
                       << "- must be between 1 and" << maxCores;
            qWarning() << "Using default:" << cores;
        } else {
            cores = val;
        }
    }
    if (!argParser.value("throttle").isEmpty()) {
        bool ok {false};
        const uint val = argParser.value("throttle").toUInt(&ok);
        if (!ok || val > 99) {
            qWarning() << "Invalid throttle value:" << argParser.value("throttle")
                       << "- must be between 0 and 99";
            qWarning() << "Using default:" << throttle;
        } else {
            throttle = val;
        }
    }
    selectKernel();
}

void Settings::processExclArgs(const QCommandLineParser &argParser)
{
    static const QSet<QString> valid_options {"Desktop",  "Documents", "Downloads", "Flatpaks", "Music",
                                              "Networks", "Pictures",  "Steam",     "Videos",   "VirtualBox"};
    if (argParser.isSet("exclude")) {
        const QStringList options = argParser.values("exclude");
        for (const QString &option : options) {
            if (valid_options.contains(option)) {
                excludeItem(option);
            } else {
                qDebug() << "Unknown option:" << option << '\n'
                         << "Please use one of these options" << valid_options.values();
            }
        }
    }
}



void Settings::setMonthlySnapshot(const QCommandLineParser &argParser)
{
    QString name;
    QFile versionFile(QStringLiteral("/etc/mx-version"));
    if (versionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        name = QString::fromUtf8(versionFile.readLine()).section(' ', 0, 0).trimmed();
        versionFile.close();
    } else {
        qDebug() << "/etc/mx-version not found or unreadable. Not MX Linux?";
        name = "MX_" + QString(x86 ? "386" : "x64");
    }
    if (argParser.value("file").isEmpty()) {
        auto month = QDate::currentDate().toString("MMMM");
        const QString monthSuffix = argParser.positionalArguments().value(0);
        if (!monthSuffix.isEmpty()) {
            month += "." + monthSuffix;
        }
        auto suffix = name.section('_', 1, 1);
        // XDG_CURRENT_DESKTOP is a colon-separated list (e.g. "KDE" or "ubuntu:GNOME");
        // it is set to "KDE" on both X11 and Wayland Plasma sessions, while
        // DESKTOP_SESSION varies by session file ("plasma", "plasmax11", ...).
        const QStringList currentDesktop
            = QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP")).split(':', Qt::SkipEmptyParts);
        bool isPlasma = currentDesktop.contains(QLatin1String("KDE"), Qt::CaseInsensitive)
                        || qgetenv("DESKTOP_SESSION") == "plasma";
        // Plain sudo strips both variables; only when neither is set fall back to
        // detecting a running Plasma shell (named plasmashell on Plasma 5 and 6).
        // A present non-KDE value is authoritative, so don't second-guess it.
        if (!isPlasma && currentDesktop.isEmpty() && qgetenv("DESKTOP_SESSION").isEmpty()) {
            isPlasma = isProcessRunning("plasmashell");
        }
        if (isPlasma) {
            suffix = "KDE";
        }
        snapshotName = name.section('_', 0, 0) + '_' + month + '_' + suffix + ".iso";
    }
    if (QFile::exists(snapshotDir + '/' + snapshotName)) {
        QString message
            = QObject::tr("Output file %1 already exists. Please use another file name, or delete the existent file.")
                  .arg(snapshotDir + '/' + snapshotName);
        MessageHandler::showMessage(MessageHandler::Critical, QObject::tr("Error"), message);
        throw std::runtime_error("Output file already exists");
    }
    if (argParser.value("compression").isEmpty()) {
        compression = "zstd";
    }
    resetAccounts = true;
    bootOptions = isArch ? ensureCowSpacesize("quiet splasht nosplash") : "quiet splasht nosplash";
    excludeAll();
}

// Helper functions for const member initialization
QString Settings::getInitialKernel(const QCommandLineParser &argParser)
{
    QString kernelValue = argParser.value("kernel");

    // Remove path prefix if present
    kernelValue.remove(QRegularExpression("^/boot/vmlinuz-"));

    // If no kernel specified or invalid, use current kernel
    if (kernelValue.isEmpty() || !QFileInfo::exists("/boot/vmlinuz-" + kernelValue)) {
        kernelValue = currentKernel;

        // If current kernel doesn't exist, find the latest one
        if (!QFileInfo::exists("/boot/vmlinuz-" + kernelValue)) {
            QDir directory("/boot");
            QStringList vmlinuzFiles = directory.entryList(QStringList() << "vmlinuz-*", QDir::Files, QDir::Name);
            if (!vmlinuzFiles.isEmpty()) {
                kernelValue = vmlinuzFiles.last().remove(QRegularExpression("^vmlinuz-"));
            }
        }
    }

    return kernelValue;
}

bool Settings::getEditBootMenuSetting()
{
    QSettings settingsUser;
    if (settingsUser.contains("edit_boot_menu")) {
        return settingsUser.value("edit_boot_menu").toString() != "no";
    }

    QSettings settingsSystem("/etc/" + qApp->applicationName() + ".conf", QSettings::IniFormat);
    return settingsSystem.value("edit_boot_menu", "no").toString() != "no";
}

QString Settings::trimQuotes(const QString &value) const
{
    QString trimmed = value;

    // Remove leading and trailing whitespace first
    trimmed = trimmed.trimmed();

    // Remove single quotes if they surround the entire value
    if (trimmed.startsWith('\'') && trimmed.endsWith('\'') && trimmed.length() > 1) {
        trimmed = trimmed.mid(1, trimmed.length() - 2);
    }

    // Remove double quotes if they surround the entire value
    if (trimmed.startsWith('"') && trimmed.endsWith('"') && trimmed.length() > 1) {
        trimmed = trimmed.mid(1, trimmed.length() - 2);
    }

    return trimmed;
}

Settings::InitialSettings Settings::getInitialSettings() const
{
    QSettings settingsSystem("/etc/" + qApp->applicationName() + ".conf", QSettings::IniFormat);
    QSettings settingsUser;

    // Merge system settings into user settings
    settingsSystem.beginGroup("");
    QStringList systemKeys = settingsSystem.allKeys();
    settingsSystem.endGroup();

    for (const QString &key : std::as_const(systemKeys)) {
        if (key != "mksq_opt" && !settingsUser.contains(key)) {
            QVariant value = settingsSystem.value(key);
            settingsUser.setValue(key, value);
        }
    }

    return {
        .live = SystemInfo::isLive(),
        .forceInstaller = settingsUser.value("force_installer", true).toBool(),
        .makeIsohybrid = settingsUser.value("make_isohybrid", "yes").toString() == "yes",
        .guiEditor = trimQuotes(settingsUser.value("gui_editor").toString()),
        .snapshotBasename = trimQuotes(settingsUser.value("snapshot_basename", "snapshot").toString()),
        .stamp = trimQuotes(settingsUser.value("stamp").toString()),
        .users = SystemInfo::listUsers()
    };
}
