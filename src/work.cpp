/**********************************************************************
 *  work.cpp
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

#include "work.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTextStream>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <stdexcept>

#include <unistd.h>

#include "checksumutils.h"
#include "elevationbroker.h"
#include "filesystemutils.h"
#include "log.h"
#include "snapdstate.h"
#include "squashfsutils.h"

namespace
{
// cleanUp() is no longer [[noreturn]] — it walks the tear-down sequence and
// then asks the event loop to exit. requestExit() centralizes the choice
// between Qt's event-loop exit (preferred) and a hard std::exit fallback for
// the rare case where no QCoreApplication exists.
void requestExit(int code)
{
    if (QCoreApplication::instance()) {
        QCoreApplication::exit(code);
        return;
    }
    std::exit(code);
}

QString loggedInUserName()
{
    return Cmd::loggedInUserName();
}

quint64 parseDuKilobytes(const QString &output, bool *ok)
{
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        if (ok) {
            *ok = false;
        }
        return 0;
    }
    const QString firstField = lines.constLast().section('\t', 0, 0).trimmed();
    return firstField.toULongLong(ok);
}

QString mksquashfsHelp()
{
    QProcess process;
    process.start(QStringLiteral("mksquashfs"), {QStringLiteral("-help")});
    if (!process.waitForStarted() || !process.waitForFinished(5000)) {
        return {};
    }
    return QString::fromLocal8Bit(process.readAllStandardOutput())
           + QString::fromLocal8Bit(process.readAllStandardError());
}

// Goes through the elevated helper (root), and uses the plain `poweroff`
// command rather than a D-Bus call to org.freedesktop.login1: logind only
// exists under systemd, but MX Linux/antiX also ship a SysVinit option with
// no logind at all, where the login1 D-Bus name can't be activated (this is
// what "Launch helper exited with unknown return code 1" from dbus-send
// means: the bus tried to activate org.freedesktop.login1 and the spawned
// process exited immediately). `poweroff` itself works under either init
// system and needs no D-Bus.
void requestPowerOff(Cmd &shell)
{
    if (!shell.runAsRoot("poweroff")) {
        qWarning() << "Failed to request poweroff.";
    }
}

// Run one command as the session user. Only meaningful when we are root: the
// installer link lives at a path whose every component the user controls, so
// touching it with root's privileges is the thing to avoid, not a convenience.
// Writing as the user makes a redirected path fail harmlessly instead of
// becoming a root-privileged write somewhere else on the filesystem.
bool runAsSessionUser(Cmd &shell, const QString &user, const QString &cmd, const QStringList &args)
{
    return shell.proc("runuser", QStringList {"-u", user, "--", cmd} + args, nullptr, nullptr, Cmd::QuietMode::Yes);
}

// Place the installer Desktop symlink at linkPath (a path inside the user's own
// Desktop, either on the real filesystem or the same path seen through the
// bind-root overlay).
//
// The link must end up owned by the session user: XFCE and Thunar refuse to
// launch a .desktop file the user does not own, which is what left the live
// installer icon startable only from a terminal.
//
// So it is always created with the session user's credentials, never root's.
// Unprivileged that is just us; under sudo (the CLI's normal mode) we drop to
// the user rather than creating a root-owned link -- and rather than pointing a
// root-privileged write at a path the user controls. `ln -n` only guards the
// final component, so a root `ln` here would still follow a symlink planted at
// Desktop/ itself and land the link in whatever directory that names; dropping
// privileges is what actually closes that, since the user cannot write there.
bool placeInstallerLink(Cmd &shell, const QString &source, const QString &linkPath, const QString &user)
{
    // Preserve existing files and symlinks, including dangling symlinks.
    const QFileInfo linkInfo(linkPath);
    if (linkInfo.exists() || linkInfo.isSymLink()) {
        return false; // Nothing was created, so cleanup must not remove it.
    }
    if (geteuid() != 0) {
        if (QFile::link(source, linkPath)) {
            return true;
        }
        qDebug() << "Could not create installer link:" << linkPath;
        return false;
    }
    // No -f: do not replace a file created after the existence check.
    // -T: never create a link inside an existing directory.
    if (!runAsSessionUser(shell, user, "ln", {"-sT", "--", source, linkPath})) {
        qDebug() << "Could not create installer link as" << user << "at" << linkPath;
        return false;
    }
    return true;
}

// Counterpart to placeInstallerLink: unlink it with the same credentials that
// created it. As root this would otherwise be an unlink through user-controlled
// path components, i.e. an arbitrary root-privileged delete.
void removeInstallerLink(Cmd &shell, const QString &linkPath, const QString &user)
{
    if (geteuid() != 0) {
        QFile::remove(linkPath);
        return;
    }
    if (user.isEmpty()) {
        // Removing it as root would be an unlink through path components the user
        // controls, so leave it rather than risk an arbitrary privileged delete.
        qWarning() << "No session user to remove the installer link as; leaving" << linkPath;
        return;
    }
    runAsSessionUser(shell, user, "rm", {"-f", "--", linkPath});
}
} // namespace

Work::Work(Settings *settings, QObject *parent)
    : QObject(parent),
      settings(settings)
{
    if (!settings) {
        qCritical() << "Work constructor: Settings pointer cannot be null";
        throw std::invalid_argument("Settings pointer cannot be null");
    }

    qDebug() << "Work object initialized for settings:" << settings->snapshotName;
}

bool Work::isEnvironmentReady() const
{
    // Check if work directory exists and is accessible
    if (settings->workDir.isEmpty() || !QDir(settings->workDir).exists()) {
        return false;
    }

    // Check if snapshot directory is accessible
    if (settings->snapshotDir.isEmpty() || !QDir(settings->snapshotDir).exists()) {
        return false;
    }

    // Check if required tools are available
    const QStringList requiredTools {"mksquashfs", "xorriso"};
    for (const QString &tool : requiredTools) {
        if (QStandardPaths::findExecutable(tool).isEmpty()) {
            return false;
        }
    }

    return true;
}

// Checks if there's enough space on partitions, if not post error, cleanup and exit
// For installed systems we account for /home when on a separate partition.
void Work::checkEnoughSpace()
{
    quint64 required_space = getRequiredSpace();
    // Check foremost if enough space for ISO on snapshot_dir; checkNoSpaceAndExit
    // kicks off cleanUp() on its own when there's not enough room, so we just
    // bail out of this pipeline stage in that case.
    if (!checkNoSpaceAndExit(required_space, settings->freeSpace, settings->snapshotDir)) {
        return;
    }

    /* If snapshot and workdir are on the same partition we need about double the size of required_space.
     * If both TMP work_dir and ISO don't fit on snapshot_dir, see if work_dir can be put on /home or /tmp
     * we already checked that ISO can fit on snapshot_dir so if TMP work fits on /home or /tmp move
     * the work_dir to the appropriate place and return */
    if (QStorageInfo(settings->workDir + "/").device() == QStorageInfo(settings->snapshotDir + "/").device()) {
        if (settings->freeSpace < required_space * 2) {
            if (checkAndMoveWorkDir("/tmp", required_space)) {
                return;
            }
            if (checkAndMoveWorkDir("/home", required_space)) {
                return;
            }
            (void) checkNoSpaceAndExit(required_space * 2, settings->freeSpace,
                                       settings->snapshotDir); // surfaces the error and triggers cleanUp
            return;
        }
    } else { // If not on the same partitions, check if work_dir has enough free space for temp files for required_space
        (void) checkNoSpaceAndExit(required_space, settings->freeSpaceWork, settings->workDir);
        return;
    }
}

bool Work::checkInstalled(const QString &package) const
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    // Validate package name contains only safe characters to prevent command injection.
    // Debian allows lowercase letters, digits, plus, minus, dot (+ optional ":amd64");
    // Arch also allows underscore. Accept a superset since both target distros tolerate it.
    static const QRegularExpression validPackageName("^[a-z0-9+_.:-]+$");
    if (!validPackageName.match(package).hasMatch()) {
        qWarning() << "Invalid package name:" << package;
        return false;
    }

    if (settings->isArch) {
        QProcess pacmanQuery;
        pacmanQuery.start("pacman", {"-Q", package});
        if (!pacmanQuery.waitForFinished(5000)) {
            pacmanQuery.kill();
            pacmanQuery.waitForFinished(1000);
            return false;
        }
        return pacmanQuery.exitStatus() == QProcess::NormalExit && pacmanQuery.exitCode() == 0;
    }

    QProcess dpkgQuery;
    dpkgQuery.start("dpkg-query", {"-W", "-f=${Status}", "--", package});
    if (!dpkgQuery.waitForFinished(5000)) {
        dpkgQuery.kill();
        dpkgQuery.waitForFinished(1000);
        return false;
    }
    if (dpkgQuery.exitStatus() != QProcess::NormalExit || dpkgQuery.exitCode() != 0) {
        return false;
    }
    const QByteArray status = dpkgQuery.readAllStandardOutput();
    return status.contains("install ok installed");
}

void Work::cleanUp()
{
    // Idempotency guard: cleanUp can be called from multiple paths (signal,
    // close event, error handler) and reentrancy here corrupts the state.
    if (cleanupStarted) {
        return;
    }
    cleanupStarted = true;
    // Cleared on every return below, after requestExit() has asked the event
    // loop to stop, so a window close during teardown can be refused without
    // ever trapping the user once cleanup is over.
    cleanupRunning = true;
    const auto cleanupFinished = qScopeGuard([this] { cleanupRunning = false; });

    // Stop a running mksquashfs before queueing any other privileged request:
    // the broker executes FIFO, so anything queued below would otherwise wait
    // for the squash to finish on its own. The out-of-band KILL terminates the
    // currently running child; snapshot-lib's pkill remains the catch-all (and
    // covers root/CLI runs, which bypass the broker).
    if (started) {
        ElevationBroker::instance().killActiveChild();
        Cmd().procAsRoot("snapshot-lib", {"kill_mksquashfs"}, nullptr, nullptr, Cmd::QuietMode::Yes);
    }
    Cmd().procAsRoot("snapshot-lib", {"chown_conf"}, nullptr, nullptr, Cmd::QuietMode::Yes);
    if (!started) {
        shell.close();
        initrd_dir.remove();
        cleanupBindRootOverlay();
        requestExit(EXIT_SUCCESS);
        return;
    }
    emit message(tr("Cleaning..."));
    QTextStream out(stdout);
    out << "\033[?25h";
    out.flush();
    shell.close();
    QProcess::execute("sync", {});
    QDir::setCurrent("/");
    // Remove the installer Desktop link from the real (bind-mounted) /home; the
    // overlay copy is discarded with the rest of the bind-root state.
    if (!installerLinkToRemove.isEmpty()) {
        removeInstallerLink(shell, installerLinkToRemove, loggedInUserName());
        installerLinkToRemove.clear();
    }
    // Two possible bind-root setups, two cleanup paths:
    //   - mx-remaster's installed-to-live (Debian) drops a marker at
    //     /tmp/installed-to-live/cleanup.conf and is undone via snapshot-lib;
    //   - installed-to-live-arch persists state at /run/<app>/cleanup-arch.state
    //     and undoes itself via its own `cleanup` subcmd.
    const QString appName = QCoreApplication::applicationName();
    const bool archStatePresent = QFileInfo::exists("/run/" + appName + "/cleanup-arch.state");
    if (archStatePresent) {
        shell.procAsRoot("installed-to-live-arch", {"cleanup"}, nullptr, nullptr, Cmd::QuietMode::Yes);
    }
    if (QFileInfo::exists("/tmp/installed-to-live/cleanup.conf")) {
        Cmd().procAsRoot("snapshot-lib", {"cleanup"});
    }
    cleanupBindRootOverlay();
    initrd_dir.remove();
    settings->tmpdir.reset();
    if (done) {
        emit message(tr("Done"));
        Cmd().procAsRoot("snapshot-lib",
                         {"copy_log", QCoreApplication::applicationName(), QDir::cleanPath(Log::getLog())}, nullptr,
                         nullptr, Cmd::QuietMode::Yes);
        if (settings->shutdown) {
            const QString logDestination = settings->snapshotDir + "/" + settings->snapshotName + ".log";
            if (!QFile::copy(Log::getLog(), logDestination)) {
                qWarning() << "Failed to copy log to" << logDestination;
            }
            QProcess::execute("sync", {});
            requestPowerOff(shell);
        }
        requestExit(EXIT_SUCCESS);
        return;
    }
    emit message(tr("Interrupted or failed to complete"));
    Cmd().procAsRoot("snapshot-lib",
                     {"copy_log", QCoreApplication::applicationName(), QDir::cleanPath(Log::getLog())}, nullptr,
                     nullptr, Cmd::QuietMode::Yes);
    requestExit(EXIT_FAILURE);
}

void Work::abortIfElevationDenied()
{
    if (!Cmd::elevationDenied()) {
        return;
    }
    emit messageBox(BoxType::critical, tr("Error"),
                    tr("Administrator access was not granted; the snapshot cannot continue."));
    cleanUp();
}

// Check if we can put work_dir on another partition with enough space, move work_dir there and setupEnv again
bool Work::checkAndMoveWorkDir(const QString &dir, quint64 req_size)
{
    // Reject candidates that cannot be used by both the user and elevated
    // helper up front — checkTempDir() would later silently relocate to a
    // different parent if we accepted one, breaking this move-to-dir contract.
    const QString candidate = Settings::resolveWorkDirParent(dir);
    if (!settings->supportsWorkDirectory(candidate)) {
        return false;
    }
    // See first if the dir is on different partition otherwise it's irrelevant
    if (QStorageInfo(candidate + "/").device() != QStorageInfo(settings->snapshotDir + "/").device()
        && FileSystemUtils::getFreeSpace(candidate) > req_size) {
        // See Work::cleanUp for the rationale on the dual cleanup paths
        // (installed-to-live for Debian, installed-to-live-arch for Arch).
        const QString appName = QCoreApplication::applicationName();
        if (QFileInfo::exists("/run/" + appName + "/cleanup-arch.state")) {
            shell.procAsRoot("installed-to-live-arch", {"cleanup"}, nullptr, nullptr, Cmd::QuietMode::Yes);
        }
        if (QFileInfo::exists("/tmp/installed-to-live/cleanup.conf")) {
            Cmd().procAsRoot("snapshot-lib", {"cleanup"});
        }
        // checkTempDir() below replaces settings->workDir with a new path; the old
        // one is still sitting in sessionExcludes from otherExclusions() earlier in
        // the pipeline, and mksquashfs never learns about the replacement. Without
        // re-pointing the exclusion, a relocation onto the very partition being
        // squashed (e.g. /tmp -> /home) makes mksquashfs squash its own output
        // directory while writing to it.
        const QString previousWorkDir = settings->workDir;
        settings->tempDirParent = candidate;
        if (!settings->checkTempDir()) {
            cleanUp();
            return false;
        }
        settings->addRemoveExclusion(false, previousWorkDir);
        settings->addRemoveExclusion(true, settings->workDir);
        setupEnv();
        return true;
    }
    return false;
}

bool Work::checkNoSpaceAndExit(quint64 needed_space, quint64 free_space, const QString &dir)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    constexpr float factor = 1024 * 1024;
    constexpr double kibToMib = 1024.0;
    qDebug().noquote() << "Needed space:" << QString::number(needed_space / kibToMib, 'f', 2) << "MiB";
    qDebug().noquote() << "Free space  :" << QString::number(free_space / kibToMib, 'f', 2) << "MiB on" << dir;
    if (needed_space > free_space) {
        emit messageBox(
            BoxType::critical, tr("Error"),
            tr("There's not enough free space on your target disk, you need at least %1")
                    .arg(QString::number(static_cast<double>(needed_space) / factor, 'f', 2) + "GiB")
                + "\n"
                + tr("You have %1 free space on %2")
                      .arg(QString::number(static_cast<double>(free_space) / factor, 'f', 2) + "GiB", dir)
                + "\n"
                + tr("If you are sure you have enough free space rerun the program with -o/--override-size option"));
        cleanUp();
        return false;
    }
    return true;
}

bool Work::setupBindRootOverlay()
{
    const QString rootFsType = Cmd().getOut("findmnt -n -o FSTYPE /", Cmd::QuietMode::Yes).trimmed();
    if (rootFsType == "overlay") {
        qDebug() << "Root filesystem is overlay; skipping bind-root overlay setup.";
        bindRootPath = "/.bind-root";
        bindRootOverlayActive = false;
        bindRootOverlayBase.clear();
        return true;
    }

    if (!ensureOverlayModuleLoaded()) {
        qWarning() << "Kernel overlay filesystem support is unavailable; "
                      "will attempt overlay mount and fall back to plain bind-mount on failure.";
    }

    const QString appName = QCoreApplication::applicationName();
    const QString overlayBase = "/run/" + appName + "/bind-root-overlay";
    const QString lowerDir = overlayBase + "/lower";
    const QString upperDir = overlayBase + "/upper";
    const QString workDir = overlayBase + "/work";
    const QString bindRoot = overlayBase + "/root";

    bindRootOverlayActive = false;
    bindRootOverlayBase.clear();
    bindRootPath = "/.bind-root";

    shell.procAsRoot("mkdir", {"-p", overlayBase, lowerDir, upperDir, workDir, bindRoot}, nullptr, nullptr,
                     Cmd::QuietMode::Yes);

    if (shell.procAsRoot("mountpoint", {"-q", bindRoot}, nullptr, nullptr, Cmd::QuietMode::Yes)) {
        shell.procAsRoot("umount", {"--recursive", bindRoot}, nullptr, nullptr, Cmd::QuietMode::Yes);
    }
    if (shell.procAsRoot("mountpoint", {"-q", lowerDir}, nullptr, nullptr, Cmd::QuietMode::Yes)) {
        shell.procAsRoot("umount", {"--recursive", lowerDir}, nullptr, nullptr, Cmd::QuietMode::Yes);
    }

    if (!shell.procAsRoot("mount", {"--bind", "/", lowerDir}, nullptr, nullptr, Cmd::QuietMode::Yes)) {
        qWarning() << "Failed to bind mount / to" << lowerDir;
        return false;
    }
    // Sever mount propagation: / is normally a shared mount, so without this
    // any mount made while the snapshot runs (ISOs, USB sticks, ...) would
    // propagate into the bind, keep cleanup's umount from succeeding, and a
    // recursive umount of the bind would propagate back and rip the user's
    // mounts out from under them.
    if (!shell.procAsRoot("mount", {"--make-private", lowerDir}, nullptr, nullptr, Cmd::QuietMode::Yes)) {
        qWarning() << "Failed to make bind mount private at" << lowerDir;
        teardownOverlayBase(overlayBase);
        return false;
    }

    const QString overlayOptions = "lowerdir=" + lowerDir + ",upperdir=" + upperDir + ",workdir=" + workDir;
    if (!shell.procAsRoot("mount", {"-t", "overlay", "overlay", "-o", overlayOptions, bindRoot}, nullptr, nullptr,
                          Cmd::QuietMode::Yes)) {
        // Overlay genuinely isn't available (kernel without CONFIG_OVERLAY_FS,
        // or a hardened/locked-down kernel that refuses overlay).
        teardownOverlayBase(overlayBase);
        if (!settings->isArch) {
            // Debian/MX kernels universally ship overlay; a failure here means
            // something is genuinely wrong. Match historical behavior and abort
            // rather than producing a non-isolated snapshot silently.
            qWarning() << "overlay mount failed at" << bindRoot << "— aborting (Debian/MX path requires overlay)";
            return false;
        }
        // Arch: fall back to a plain bind-mount at /.bind-root and let
        // installed-to-live-arch run the rest of the setup against the live
        // tree directly. This loses the "isolated from runtime changes"
        // property — the snapshot will see whatever the system writes during
        // squashing — so warn loudly.
        qWarning() << "overlay mount failed at" << bindRoot
                   << "— falling back to plain bind-mount (snapshot is no longer isolated from live writes)";
        bindRootPath = "/.bind-root";
        bindRootOverlayBase.clear();
        bindRootOverlayActive = false;
        // installed-to-live-arch `start` will create + bind-mount /.bind-root
        // itself, so we don't pre-mount anything here.
        return true;
    }

    bindRootPath = bindRoot;
    bindRootOverlayBase = overlayBase;
    bindRootOverlayActive = true;
    return true;
}

// Undo a partially built overlay base. The lower dir is a bind mount of the
// real /, so the directory must NEVER be rm -rf'd while anything under it is
// still mounted — that would delete the running system through the bind.
// Delegate to snapshot-lib cleanup_overlay, which derives the same
// /run/<app>/bind-root-overlay path and holds the hardened teardown logic
// (unmount retries, lazy-detach fallback, refusal to delete while any mount
// remains anywhere below the base).
void Work::teardownOverlayBase(const QString &overlayBase)
{
    if (!shell.procAsRoot("snapshot-lib", {"cleanup_overlay", QCoreApplication::applicationName()}, nullptr, nullptr,
                          Cmd::QuietMode::Yes)) {
        qWarning() << "cleanup_overlay could not fully tear down" << overlayBase
                   << "— overlay state left under /run for inspection";
    }
}

// A respin must not carry this computer's snap store registration (see
// SnapdState). The device key and serial assertion are excluded in
// Settings::otherExclusions(); the state that refers to them is replaced here
// by a copy without them, bind-mounted over the bind-root's state.json so the
// host's file is never touched. The cleanup of installed-to-live(-arch)
// unmounts everything under the bind-root, this mount included.
bool Work::resetSnapdIdentity()
{
    const QString statePath = bindRootPath + "/var/lib/snapd/state.json";
    if (!shell.procAsRoot("stat", {"-c", "%n", statePath}, nullptr, nullptr, Cmd::QuietMode::Yes)) {
        return true; // snapd isn't installed or never ran
    }
    // The state holds the store session: read it with a Cmd whose output
    // doesn't reach the log or the window.
    Cmd reader;
    reader.setOutputSuppressed(true);
    QString state;
    if (!reader.procAsRoot("cat", {statePath}, &state, nullptr, Cmd::QuietMode::Yes)) {
        qWarning() << "Cannot read" << statePath;
        return false;
    }
    const std::optional<QByteArray> cleaned = SnapdState::withoutDeviceIdentity(state.toUtf8());
    if (!cleaned) {
        qWarning() << "Not a snapd state file:" << statePath;
        return false;
    }
    // Written by root with a private umask: the bind source's owner and mode
    // are what end up in the snapshot.
    const QString cleanedPath = settings->workDir + "/snapd-state.json";
    return shell.procAsRoot("sh", {"-c", "umask 077 && cat > \"$1\"", "sh", cleanedPath}, nullptr, &*cleaned,
                            Cmd::QuietMode::Yes)
        && shell.procAsRoot("mount", {"--bind", cleanedPath, statePath}, nullptr, nullptr, Cmd::QuietMode::Yes);
}

bool Work::ensureOverlayModuleLoaded()
{
    auto overlayRegistered = []() {
        QFile filesystems("/proc/filesystems");
        if (!filesystems.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }
        while (!filesystems.atEnd()) {
            const QByteArray line = filesystems.readLine().trimmed();
            const auto parts = line.split('\t');
            if (!parts.isEmpty() && parts.last() == "overlay") {
                return true;
            }
        }
        return false;
    };

    if (overlayRegistered()) {
        return true;
    }
    shell.procAsRoot("modprobe", {"overlay"}, nullptr, nullptr, Cmd::QuietMode::Yes);
    return overlayRegistered();
}

void Work::cleanupBindRootOverlay()
{
    if (bindRootOverlayBase.isEmpty()) {
        bindRootOverlayActive = false;
        bindRootPath = "/.bind-root";
        return;
    }
    teardownOverlayBase(bindRootOverlayBase);
    bindRootOverlayActive = false;
    bindRootOverlayBase.clear();
    bindRootPath = "/.bind-root";
}

// Repack the edited initrd. False when the archive could not be fully written
// (e.g. the work partition is full), so a truncated initrd is never shipped.
bool Work::closeInitrd(const QString &initrd_dir, const QString &file)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (!QDir::setCurrent(initrd_dir)) {
        qWarning() << "Cannot enter the initrd directory" << initrd_dir;
        return false;
    }
    // pipefail: a failing find or cpio must fail the repack too, not only gzip.
    // The output path is passed as $1 so the shell never parses it.
    return shell.proc("/bin/bash",
                      {"-c", R"(set -o pipefail; (find . |cpio -o -H newc --owner root:root |gzip -9) >"$1")", "_",
                       file});
}

// copyModules(mod_dir/kernel kernel)
bool Work::copyModules(const QString &to, const QString &kernel)
{
    if (!shell.proc("/usr/share/" + qApp->applicationName() + "/scripts/copy-initrd-modules",
                    {"-e", "--to=" + to, "--kernel=" + kernel})) {
        return false;
    }
    // -F/--force: missing optional initrd programs (ntfs-3g, eject, cryptsetup, ...)
    // become a warning instead of aborting the whole snapshot.
    if (!shell.procAsRoot("copy-initrd-programs", {"-e", "-F", "--to=" + to}, nullptr, nullptr,
                          Cmd::QuietMode::No)) {
        return false;
    }
    // Ownership fixup is cosmetic (the initrd is repacked root:root anyway),
    // so a chown failure doesn't fail the copy.
    const QString username = loggedInUserName();
    if (!username.isEmpty()) {
        shell.procAsRoot("chown", {"-R", username + ":", to}, nullptr, nullptr, Cmd::QuietMode::Yes);
    }
    return true;
}

// Copying the iso-template filesystem
void Work::copyNewIso()
{
    emit message(tr("Copying the new-iso filesystem..."));
    QDir::setCurrent(settings->workDir);

    QString templateTar = "/usr/lib/iso-template/iso-template.tar.gz";
    if (settings->isArch) {
        templateTar = "/usr/lib/iso-template/arch/iso-template.tar.gz";
        if (!QFileInfo::exists(templateTar)) {
            const QString fallback = QDir::homePath() + "/mx-iso-template/arch/iso-template.tar.gz";
            if (QFileInfo::exists(fallback)) {
                templateTar = fallback;
            }
        }
    } else if (QFile::exists("/usr/lib/iso-template/iso-template-multi.tar.gz")
               && QFile::exists("/usr/lib/sysvinit/init") && QFile::exists("/usr/lib/systemd/systemd")) {
        // Use multi-init template if available and both init systems are installed
        templateTar = "/usr/lib/iso-template/iso-template-multi.tar.gz";
    }

    if (!QFileInfo::exists(templateTar)) {
        emit messageBox(BoxType::critical, tr("Error"), tr("ISO template not found: ") + templateTar);
        cleanUp();
        return;
    }

    if (settings->isArch) {
        QDir().mkpath("iso-template");
        if (!shell.proc("tar", {"xf", templateTar, "-C", "iso-template"})) {
            emit messageBox(BoxType::critical, tr("Error"), tr("Could not extract the ISO template: ") + templateTar);
            cleanUp();
            return;
        }

        const QString diskDirPath = settings->workDir + "/iso-template/.disk";
        QDir().mkpath(diskDirPath);
        QDir diskDir(diskDirPath);
        const QStringList oldUuids = diskDir.entryList(QStringList() << "*.uuid", QDir::Files);
        for (const QString &oldUuid : oldUuids) {
            diskDir.remove(oldUuid);
        }
        const QString uuidStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd-HH-mm-ss-00");
        QFile uuidFile(diskDir.filePath(uuidStamp + ".uuid"));
        if (!uuidFile.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to create .disk UUID file:" << uuidFile.fileName();
        }

        const QString antiXPath = settings->workDir + "/iso-template/antiX";
        if (QFileInfo::exists(antiXPath)) {
            QDir(antiXPath).removeRecursively();
        }

        const QString bootDir = settings->workDir + "/iso-template/boot";
        const QString efiDir = settings->workDir + "/iso-template/efi";
        if (!QFileInfo::exists(bootDir) || !QFileInfo::exists(efiDir)) {
            const QString wrongBoot = settings->workDir + "/boot";
            const QString wrongEfi = settings->workDir + "/efi";
            QString details = tr("Arch ISO template is missing boot/ or efi/ directories.");
            if (QFileInfo::exists(wrongBoot) || QFileInfo::exists(wrongEfi)) {
                details += "\n"
                           + tr("Detected boot/ or efi/ under the work directory root; "
                                "the template may have been extracted to the wrong location.");
            }
            details += "\n" + tr("Template: %1").arg(templateTar);
            emit messageBox(BoxType::critical, tr("Error"), details);
            cleanUp();
            return;
        }

        const QString archCpuDir = settings->x86 ? "i686" : "x86_64";
        const QString archBootDir = "iso-template/arch/boot/" + archCpuDir;
        const QString archBootPath = settings->workDir + "/" + archBootDir;
        const QString kernelPath = "/boot/vmlinuz-" + settings->kernel;
        QDir().mkpath(archBootDir);

        shell.procAsRoot("cp", {kernelPath, archBootPath + "/vmlinuz-linux"}, nullptr, nullptr, Cmd::QuietMode::No);
        const QString cpUser = loggedInUserName();
        if (!cpUser.isEmpty()) {
            shell.procAsRoot("chown", {cpUser + ":", archBootPath + "/vmlinuz-linux"}, nullptr, nullptr,
                             Cmd::QuietMode::Yes);
        }
        shell.proc("chmod", {"a+r", archBootPath + "/vmlinuz-linux"});

        QString initramfsSource;
        const QString archisoPath = "/boot/archiso.img";
        if (QFileInfo::exists(archisoPath)) {
            const QString archisoKernel = initramfsKernelVersion(archisoPath);
            const QString expectedKernel = kernelImageVersion(kernelPath);
            if (!expectedKernel.isEmpty()) {
                qDebug() << "Expected kernel from image:" << expectedKernel;
            } else {
                qWarning() << "Could not determine kernel version from" << kernelPath;
            }
            if (!archisoKernel.isEmpty()) {
                qDebug() << "archiso initramfs kernel:" << archisoKernel;
            }
            const bool needsRebuild
                = archisoKernel.isEmpty() || (!expectedKernel.isEmpty() && archisoKernel != expectedKernel);
            if (needsRebuild) {
                emit message(tr("Stale archiso initramfs detected, rebuilding..."));
                if (!rebuildArchisoInitramfs(archisoPath, kernelPath)) {
                    const QString selectedKernel = expectedKernel.isEmpty() ? settings->kernel : expectedKernel;
                    QString details = archisoKernel.isEmpty()
                                          ? tr("Could not determine the kernel version of /boot/archiso.img "
                                               "(selected kernel is %1).")
                                                .arg(selectedKernel)
                                          : tr("Found /boot/archiso.img built for kernel %1, but the selected "
                                               "kernel is %2.")
                                                .arg(archisoKernel, selectedKernel);
                    details += "\n"
                               + tr("Rebuilding /boot/archiso.img failed; check the mkinitcpio output in the log "
                                    "for errors (e.g. a missing hook). Please fix them and rebuild it manually, "
                                    "or remove the stale file.");
                    emit messageBox(BoxType::critical, tr("Error"), details);
                    cleanUp();
                    return;
                }
                // rebuildArchisoInitramfs() only succeeds for the selected kernel.
            }
            initramfsSource = archisoPath;
        } else {
            // /boot/archiso.img doesn't exist — try to create one so the ISO can boot as live media.
            // A regular initramfs lacks the archiso hook and will fail at switch_root.
            emit message(tr("No /boot/archiso.img found, attempting to create one..."));
            if (rebuildArchisoInitramfs(archisoPath, kernelPath)) {
                initramfsSource = archisoPath;
            } else {
                emit messageBox(
                    BoxType::warning, tr("Warning"),
                    tr("Could not create /boot/archiso.img (is the 'archiso' package installed?). "
                       "Falling back to the regular initramfs — the resulting ISO will likely fail to "
                       "boot (\"Failed to start Switch Root\")."));
                if (QFileInfo::exists("/boot/initramfs-" + settings->kernel + ".img")) {
                    initramfsSource = "/boot/initramfs-" + settings->kernel + ".img";
                } else if (QFileInfo::exists("/boot/initramfs-linux.img")) {
                    initramfsSource = "/boot/initramfs-linux.img";
                }
            }
        }

        if (initramfsSource.isEmpty()) {
            emit messageBox(BoxType::critical, tr("Error"), tr("Could not find an initramfs image to use."));
            cleanUp();
            return;
        }

        shell.procAsRoot("cp", {initramfsSource, archBootPath + "/archiso.img"}, nullptr, nullptr,
                         Cmd::QuietMode::No);
        if (!cpUser.isEmpty()) {
            shell.procAsRoot("chown", {cpUser + ":", archBootPath + "/archiso.img"}, nullptr, nullptr,
                             Cmd::QuietMode::Yes);
        }
        shell.proc("chmod", {"a+r", archBootPath + "/archiso.img"});
    } else {
        if (!shell.proc("tar", {"xf", templateTar})) {
            emit messageBox(BoxType::critical, tr("Error"), tr("Could not extract the ISO template: ") + templateTar);
            cleanUp();
            return;
        }

        // check to make sure grub mbr is possible
        if (settings->grubmbr) {
            QString file = settings->workDir + "/iso-template/boot/grub/i386-pc/eltorito.img";
            qDebug() << "file is " << file;
            if (QFile::exists(file)) {
                // comment out switch_to_syslinux menus as they don't work when grub is main boot
                shell.proc("sed", {"-i", "/^[^#]*switch_to_syslinux/s/^/#/",
                                   settings->workDir + "/iso-template/boot/grub/config/bootmenu.cfg"});
            } else {
                emit messageBox(BoxType::critical, tr("Error"),
                                tr("--grub-mbr option specified but boot/grub/i386-pc/eltorito.img is missing from "
                                   "iso-template"));
                cleanUp();
                return;
            }
        }
        if (!shell.run("cp /usr/lib/iso-template/template-initrd.gz iso-template/antiX/initrd.gz")) {
            emit messageBox(BoxType::critical, tr("Error"),
                            tr("Could not copy the template initrd: ") + "/usr/lib/iso-template/template-initrd.gz");
            cleanUp();
            return;
        }
        if (!shell.proc("cp", {"/boot/vmlinuz-" + settings->kernel, "iso-template/antiX/vmlinuz"})) {
            emit messageBox(BoxType::critical, tr("Error"),
                            tr("Could not copy the kernel: ") + "/boot/vmlinuz-" + settings->kernel);
            cleanUp();
            return;
        }

        if (!makeChecksum(HashType::md5, settings->workDir + "/iso-template/antiX", "vmlinuz")) {
            if (!cleanupStarted) {
                emit messageBox(BoxType::critical, tr("Error"),
                                tr("Could not create the checksum for %1.").arg("vmlinuz"));
                cleanUp();
            }
            return;
        }

        QString path = initrd_dir.path();
        if (!initrd_dir.isValid()) {
            qDebug() << tr("Could not create temp directory. ") + path;
            cleanUp();
            return;
        }

        if (!openInitrd(settings->workDir + "/iso-template/antiX/initrd.gz", path)) {
            if (!cleanupStarted) {
                emit messageBox(BoxType::critical, tr("Error"), tr("Could not extract the template initrd."));
                cleanUp();
            }
            return;
        }

        // Strip modules, but only inside our own temp dir so a wrong path can never
        // remove a real /lib/modules. Key the guard on the actual temp base
        // (QTemporaryDir honours $TMPDIR) rather than a literal "/tmp/", which
        // silently skipped the strip whenever TMPDIR pointed elsewhere (e.g. /var/tmp),
        // leaving the template's stale modules to pile up with the copied ones.
        if (initrd_dir.isValid() && path.startsWith(QDir::tempPath() + '/')) {
            QDir modulesDir(path + "/lib/modules");
            if (modulesDir.exists()) {
                modulesDir.removeRecursively();
            }
        }

        // For old versions we copy initrd-release for newer ones we copy initrd_release
        QString sourcePath = "/etc/initrd-release";
        QString destinationPath = path + "/etc/initrd-release";
        QFileInfo sourceFileInfo(sourcePath);
        if (sourceFileInfo.exists() && sourceFileInfo.isFile()) {
            if (!QFile::exists(destinationPath) || QFile::remove(destinationPath)) {
                QFile::copy(sourcePath, destinationPath);
            }
        }
        sourcePath = "/etc/initrd_release";
        destinationPath = path + "/etc/initrd_release";
        sourceFileInfo.setFile(sourcePath);
        if (sourceFileInfo.exists() && sourceFileInfo.isFile()) {
            if (!QFile::exists(destinationPath) || QFile::remove(destinationPath)) {
                QFile::copy(sourcePath, destinationPath);
            }
        }

        if (initrd_dir.isValid()) {
            if (!copyModules(path, settings->kernel)) {
                if (!cleanupStarted) {
                    emit messageBox(BoxType::critical, tr("Error"),
                                    tr("Could not copy the kernel modules or programs into the initrd."));
                    cleanUp();
                }
                return;
            }
            const QString initrdFile = settings->workDir + "/iso-template/antiX/initrd.gz";
            if (!closeInitrd(path, initrdFile)) {
                if (!cleanupStarted) {
                    emit messageBox(BoxType::critical, tr("Error"),
                                    tr("Could not create the new initrd: %1").arg(initrdFile));
                    cleanUp();
                }
                return;
            }
            const QStringList ucToolDirs = {"/usr/bin", "/usr/local/bin"};
            if (!QStandardPaths::findExecutable("uc-tool", ucToolDirs).isEmpty()) {
                qDebug() << "uc-tool found, injecting ucode into initrd";
                shell.proc("uc-tool", {"-q", "-i", settings->workDir + "/iso-template/antiX/initrd.gz"});
            } else {
                qDebug() << "uc-tool not found, skipping ucode injection";
            }
            if (!makeChecksum(HashType::md5, settings->workDir + "/iso-template/antiX", "initrd.gz")) {
                if (!cleanupStarted) {
                    emit messageBox(BoxType::critical, tr("Error"),
                                    tr("Could not create the checksum for %1.").arg("initrd.gz"));
                    cleanUp();
                }
                return;
            }
            initrd_dir.remove();
        }
    }

    replaceMenuStrings();
}

// Create squashfs and then the iso
bool Work::createIso(const QString &filename)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    // Squash the filesystem copy
    const bool useUnbuffer = !QStandardPaths::findExecutable("unbuffer").isEmpty();
    using Release::Version;

    const QString archCpuDir = settings->x86 ? "i686" : "x86_64";
    const QString squashfsPath = settings->isArch
                                     ? settings->workDir + "/iso-template/arch/" + archCpuDir + "/airootfs.sfs"
                                     : settings->workDir + "/iso-template/antiX/linuxfs";
    if (settings->isArch) {
        QDir().mkpath(settings->workDir + "/iso-template/arch/" + archCpuDir);
    }

    // -percentage only exists in squashfs-tools 4.6+ (and MX builds that
    // backport it). Read the local tool's help directly instead of using a
    // shell pipeline, then include only the options this build advertises.
    const QString squashfsHelp = mksquashfsHelp();
    const bool throttleSupported = SquashfsUtils::helpListsOption(squashfsHelp, QStringLiteral("-throttle"));
    const bool percentageSupported = SquashfsUtils::helpListsOption(squashfsHelp, QStringLiteral("-percentage"));

    const SquashfsUtils::Command squashfsCommand = SquashfsUtils::buildCommand({
        .bindRootPath = bindRootPath,
        .outputPath = squashfsPath,
        .compression = settings->compression,
        .cores = settings->cores,
        .throttle = settings->throttle,
        .throttleSupported = throttleSupported,
        .percentageSupported = percentageSupported,
        .mksqOpt = settings->mksqOpt,
        .excludesFileName = settings->snapshotExcludes.fileName(),
        .sessionExcludes = settings->sessionExcludes,
        .useUnbuffer = useUnbuffer,
    });

    emit message(tr("Squashing filesystem..."));
    if (!shell.procAsRoot(squashfsCommand.program, squashfsCommand.args, nullptr, nullptr, Cmd::QuietMode::No)) {
        // Failure here can be a real mksquashfs error OR the user hitting
        // Cancel mid-squash, in which case cleanUp() has already run and
        // is asking the event loop to exit. Don't pile a misleading
        // "could not create linuxfs" error box on top of that.
        if (cleanupStarted) {
            return false;
        }
        emit messageBox(
            BoxType::critical, tr("Error"),
            tr("Could not create linuxfs file, please check /var/log/%1.log").arg(QCoreApplication::applicationName()));
        cleanUp();
        return false;
    }
    if (cleanupStarted) {
        return false;
    }
    writeUnsquashfsSize(shell.readAllOutput());

    if (settings->isArch) {
        const QString archIsoDir = "iso-2/arch/" + archCpuDir;
        QDir().mkpath(archIsoDir);
        if (!shell.run("mv iso-template/arch/" + archCpuDir + "/airootfs.sfs* " + archIsoDir) || cleanupStarted) {
            if (!cleanupStarted) {
                emit messageBox(BoxType::critical, tr("Error"),
                                tr("Could not move %1 to the ISO directory.").arg("airootfs.sfs"));
                cleanUp();
            }
            return false;
        }
        if (!makeChecksum(HashType::sha512, settings->workDir + "/" + archIsoDir, "airootfs.sfs")) {
            if (!cleanupStarted) {
                emit messageBox(BoxType::critical, tr("Error"),
                                tr("Could not create the checksum for %1.").arg("airootfs.sfs"));
                cleanUp();
            }
            return false;
        }
    } else {
        // Move linuxfs files to iso-2/antiX folder
        QDir().mkpath("iso-2/antiX");
        if (!shell.run("mv iso-template/antiX/linuxfs* iso-2/antiX") || cleanupStarted) {
            if (!cleanupStarted) {
                emit messageBox(BoxType::critical, tr("Error"),
                                tr("Could not move %1 to the ISO directory.").arg("linuxfs"));
                cleanUp();
            }
            return false;
        }
        if (!makeChecksum(HashType::md5, settings->workDir + "/iso-2/antiX", "linuxfs")) {
            if (!cleanupStarted) {
                emit messageBox(BoxType::critical, tr("Error"),
                                tr("Could not create the checksum for %1.").arg("linuxfs"));
                cleanUp();
            }
            return false;
        }
    }
    if (cleanupStarted) {
        return false;
    }

    const QString appName = QCoreApplication::applicationName();
    const bool archStatePresent = QFileInfo::exists("/run/" + appName + "/cleanup-arch.state");
    if (archStatePresent) {
        shell.procAsRoot("installed-to-live-arch", {"cleanup"}, nullptr, nullptr, Cmd::QuietMode::Yes);
    }
    if (QFileInfo::exists("/tmp/installed-to-live/cleanup.conf")) {
        Cmd().procAsRoot("snapshot-lib", {"cleanup"});
    }

    if (cleanupStarted) {
        return false;
    }

    // Create the iso file. The static xorriso flags stay inline; the two variable
    // paths (output ISO and iso-2 dir) are passed as bash positional parameters
    // ($1, $2) so the shell never parses them — this prevents command injection
    // through snapshotDir/filename/workDir.
    QDir::setCurrent(settings->workDir + "/iso-template");
    // On Arch the label derives from fullDistroName, which can contain spaces on
    // a pure Arch host (projectName comes from os-release NAME="Arch Linux").
    // Boot configs may consume the label (archisolabel=, search --label), where a
    // space breaks parsing, and ISO 9660 caps volume IDs at 32 characters — so
    // sanitize rather than passing the display name through verbatim.
    QString volumeLabel = "MXLIVE";
    if (settings->isArch) {
        volumeLabel = settings->fullDistroName;
        volumeLabel.replace(' ', '-');
        volumeLabel.truncate(32);
    }
    QString cmd;
    if (settings->isArch) {
        // Arch ISO uses the GRUB eltorito image for BIOS boot and a flat efi.img at the
        // template root. isohybrid is folded into xorriso here (no separate post-step).
        const QString archAntiXPath = settings->workDir + "/iso-2/antiX";
        if (QFileInfo::exists(archAntiXPath)) {
            QDir(archAntiXPath).removeRecursively();
        }

        QString bootArgs;
        const QString biosBootRel = "boot/grub/i386-pc/eltorito.img";
        const QString biosBootPath = settings->workDir + "/iso-template/" + biosBootRel;
        if (QFileInfo::exists(biosBootPath)) {
            bootArgs += " -b " + biosBootRel
                        + " -no-emul-boot -boot-load-size 4 -boot-info-table -c boot.catalog";
        } else {
            qWarning() << "Missing BIOS boot image:" << biosBootPath;
        }

        const QString efiBootRel = "efi.img";
        const QString efiBootPath = settings->workDir + "/iso-template/" + efiBootRel;
        if (QFileInfo::exists(efiBootPath)) {
            bootArgs += " -eltorito-alt-boot -e " + efiBootRel + " -no-emul-boot";
        } else {
            qWarning() << "Missing EFI boot image:" << efiBootPath;
        }

        QString isohybridArgs;
        if (settings->makeIsohybrid) {
            static const QStringList mbrCandidates = {
                "/usr/lib/syslinux/bios/isohdpfx.bin",
                "/usr/lib/syslinux/isohdpfx.bin",
                "/usr/share/syslinux/isohdpfx.bin",
                "/usr/lib/ISOLINUX/isohdpfx.bin",
            };
            QString foundMbrPath;
            for (const QString &candidate : mbrCandidates) {
                if (QFileInfo::exists(candidate)) {
                    foundMbrPath = candidate;
                    break;
                }
            }
            if (!foundMbrPath.isEmpty()) {
                // foundMbrPath comes from the trusted candidate list above, never user input.
                isohybridArgs = " -isohybrid-mbr \"" + foundMbrPath + "\" -isohybrid-gpt-basdat";
                qDebug() << "Using isohybrid MBR:" << foundMbrPath;
            } else {
                qWarning() << "isohdpfx.bin not found; skipping xorriso hybrid flags.";
            }
        }

        // The volume label is user-influenced (fullDistroName), so it rides in $3.
        cmd = R"(xorriso -as mkisofs -l -V "$3" -R -J -pad -iso-level 3)" + isohybridArgs + bootArgs
              + R"( -o "$1" . "$2")";
    } else if (settings->grubmbr) {
        // grub for mbr boot instead of isolinux
        cmd = R"(xorriso -as mkisofs -l -V MXLIVE -R -J -pad -iso-level 3 -no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info )"
              R"(-b boot/grub/i386-pc/eltorito.img -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot -isohybrid-apm-hfsplus -isohybrid-gpt-basdat -o "$1" . "$2")";
    } else {
        cmd = R"(xorriso -as mkisofs -l -V MXLIVE -R -J -pad -iso-level 3 -no-emul-boot -boot-load-size 4 -boot-info-table )"
              R"(-b boot/isolinux/isolinux.bin -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot -c )"
              R"(boot/isolinux/isolinux.cat -o "$1" . "$2")";
    }

    if (Cmd().getOut("umask", Cmd::QuietMode::Yes) != "0022") {
        cmd.prepend("umask 022; ");
    }
    emit message(tr("Creating CD/DVD image file..."));
    if (!shell.proc("/bin/bash",
                    {"-c", cmd, "_", settings->snapshotDir + "/" + filename, settings->workDir + "/iso-2",
                     volumeLabel},
                    nullptr, nullptr, Cmd::QuietMode::No)) {
        if (cleanupStarted) {
            return false;
        }
        emit messageBox(
            BoxType::critical, tr("Error"),
            tr("Could not create ISO file, please check whether you have enough space on the destination partition."));
        cleanUp();
        return false;
    }
    if (cleanupStarted) {
        return false;
    }

    // Make it isohybrid (Arch already folded the hybrid MBR into xorriso above)
    if (settings->makeIsohybrid && !settings->isArch) {
        emit message(tr("Making hybrid iso"));
        if (!shell.proc("isohybrid", {"--uefi", settings->snapshotDir + "/" + filename}, nullptr, nullptr,
                        Cmd::QuietMode::No)) {
            if (cleanupStarted) {
                return false;
            }
            emit messageBox(BoxType::critical, tr("Error"),
                            tr("Could not make the ISO hybrid; it would not boot correctly from USB."));
            cleanUp();
            return false;
        }
    }
    if (cleanupStarted) {
        return false;
    }

    // Make ISO checksums
    if (settings->makeMd5sum && !makeChecksum(HashType::md5, settings->snapshotDir, filename)) {
        if (!cleanupStarted) {
            emit messageBox(BoxType::critical, tr("Error"),
                            tr("Could not create the %1 checksum for the ISO.").arg("MD5"));
            cleanUp();
        }
        return false;
    }
    if (settings->makeSha256sum && !makeChecksum(HashType::sha256, settings->snapshotDir, filename)) {
        if (!cleanupStarted) {
            emit messageBox(BoxType::critical, tr("Error"),
                            tr("Could not create the %1 checksum for the ISO.").arg("SHA256"));
            cleanUp();
        }
        return false;
    }

    auto elapsedTime = QTime(0, 0).addMSecs(e_timer.elapsed());
    done = true;
    emit message(tr("Done"));
    if (settings->shutdown) {
        cleanUp();
        return true;
    }
    emit messageBox(BoxType::information, tr("Success"),
                    tr("MX Snapshot completed successfully!") + '\n'
                        + tr("Snapshot took %1 to finish.").arg(elapsedTime.toString("hh:mm:ss")) + "\n\n"
                        + tr("Thanks for using MX Snapshot, run MX Live USB Maker next!"));
    return true;
}

bool Work::installPackage(const QString &package)
{
    if (settings->isArch) {
        // Substitute Arch's installer (gazelle-installer) for the MX equivalent.
        QString archPackage = package;
        if (package == "mx-installer") {
            if (checkInstalled("gazelle-installer")) {
                return true;
            }
            archPackage = "gazelle-installer";
        }
        emit message(tr("Installing ") + archPackage);
        // Try an enabled repo first (e.g. archPackage ships on arch.mxrepo.com) -
        // it needs no extra tooling and doesn't touch the AUR. Only fall back to
        // an AUR helper if pacman can't find it in any configured repo. paru
        // runs as the regular user (AUR helpers refuse root) and escalates
        // internally for the install step. If both are unavailable or fail,
        // warn and carry on — the snapshot is still valid, just without the
        // installer on the ISO.
        bool installed = shell.procAsRoot("pacman", {"-S", "--noconfirm", "--needed", archPackage}, nullptr,
                                          nullptr, Cmd::QuietMode::No);
        if (!installed) {
            if (!QStandardPaths::findExecutable("paru").isEmpty()) {
                installed = shell.proc("paru", {"-S", "--noconfirm", "--needed", archPackage}, nullptr, nullptr,
                                       Cmd::QuietMode::No);
            } else {
                emit message(tr("paru not found; cannot install %1 from the AUR.").arg(archPackage));
            }
        }
        if (!installed) {
            emit messageBox(BoxType::warning, tr("Warning"),
                            tr("Could not install %1; continuing without the installer.").arg(archPackage));
            return false;
        }
        return true;
    }

    emit message(tr("Installing ") + package);
    shell.procAsRoot("apt-get", {"update"}, nullptr, nullptr, Cmd::QuietMode::No);
    if (!shell.procAsRoot("apt-get", {"install", "-y", package}, nullptr, nullptr, Cmd::QuietMode::No)) {
        emit messageBox(BoxType::critical, tr("Error"), tr("Could not install ") + package);
        return false;
    }
    return true;
}

// Create checksums for different files
bool Work::makeChecksum(Work::HashType hash_type, const QString &folder, const QString &file_name)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    emit message(tr("Calculating checksum..."));
    shell.run("sync");
    QDir::setCurrent(folder);
    const QString ce = QVariant::fromValue(hash_type).toString();
    QString cmd;

    if (settings->preempt) {
        // Check free space available on /tmp
        if (!shell.proc("/bin/bash",
                        {"-c",
                         R"(DUF=$(du -BM -- "$1" | grep -oE '^[[:digit:]]+'); )"
                         R"(TDA=$(df -BM --output=avail /tmp | grep -oE '^[[:digit:]]+'); ((TDA/10*8 >= DUF)))",
                         "_", file_name})) {
            settings->preempt = false;
        }
    }
    // Per-run private temp dir for the preempt copy (mkdtemp: 0700, owned by
    // this process, unpredictable name) instead of a fixed world-guessable
    // /tmp path a concurrent run or another local user could pre-create or
    // tamper with. Removed automatically when this instance goes out of scope
    // — never by the shell script, and never a directory this run didn't create.
    std::optional<QTemporaryDir> checksumTempDir;
    if (settings->preempt) {
        checksumTempDir.emplace(QStringLiteral("/tmp/snapshot-checksum-XXXXXX"));
        if (!checksumTempDir->isValid()) {
            qWarning() << "Could not create private checksum temp dir, checksumming in place:"
                       << checksumTempDir->errorString();
            settings->preempt = false;
        }
    }
    if (!settings->preempt) {
        cmd = ChecksumUtils::directCommand(ce);
    } else {
        // Free pagecache
        shell.run("sync; sleep 1");
        Cmd().procAsRoot("snapshot-lib", {"drop_caches"});
        shell.run("sleep 1");
        cmd = ChecksumUtils::preemptCommand(ce, checksumTempDir->path());
    }
    // file_name -> $1 and folder -> $2 are passed to bash as positional parameters
    // so the shell never parses them (no injection via a crafted file name or
    // output folder); ce and the temp dir path are trusted values concatenated
    // in by ChecksumUtils.
    const bool commandOk = shell.proc("/bin/bash", {"-c", cmd, "_", file_name, folder});
    QDir::setCurrent(settings->workDir);
    return commandOk && ChecksumUtils::verifyChecksumFile(folder, file_name, ce);
}

// Unpack the template initrd into initrd_dir. False when it could not be
// fully extracted, so a broken template never gets repacked into the ISO.
bool Work::openInitrd(const QString &file, const QString &initrd_dir)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    emit message(tr("Building new initrd..."));
    shell.proc("chmod", {"a+rx", initrd_dir});
    // Without this check a failed chdir would unpack into the work directory.
    if (!QDir::setCurrent(initrd_dir)) {
        qWarning() << "Cannot enter the initrd directory" << initrd_dir;
        return false;
    }
    // pipefail: a corrupt archive must fail even if cpio exits cleanly.
    // The archive path is passed as $1 so the shell never parses it.
    if (!shell.proc("/bin/bash", {"-c", R"(set -o pipefail; gunzip -c -- "$1" |cpio -idum)", "_", file})) {
        qWarning() << "Could not extract the initrd" << file;
        return false;
    }
    // A truncated archive can still extract "successfully"; the boot script is
    // the part the initrd cannot do without.
    if (!QFileInfo(initrd_dir + "/init").isFile()) {
        qWarning() << "Extracted initrd has no init script:" << file;
        return false;
    }
    return true;
}

QString Work::initramfsKernelVersion(const QString &initramfsPath) const
{
    if (!QFileInfo::exists(initramfsPath) || QStandardPaths::findExecutable("lsinitcpio").isEmpty()) {
        return {};
    }
    // Images in /boot are root-only (0600), so the probe must run elevated.
    // The analysis header reports the kernel as "==> Kernel: <version>".
    const QString output
        = Cmd().getOutAsRoot("lsinitcpio", {"-a", "-n", initramfsPath}, Cmd::QuietMode::Yes);
    static const QRegularExpression kernelRegex(R"(^==> Kernel: (\S+))", QRegularExpression::MultilineOption);
    const auto match = kernelRegex.match(output);
    if (!match.hasMatch() || match.captured(1) == "unknown") {
        return {};
    }
    return match.captured(1);
}

QString Work::kernelImageVersion(const QString &kernelPath) const
{
    if (!QFileInfo::exists(kernelPath)) {
        return {};
    }
    if (QStandardPaths::findExecutable("file").isEmpty()) {
        return {};
    }
    QString output;
    Cmd().proc("file", {"-b", kernelPath}, &output, nullptr, Cmd::QuietMode::Yes);
    const QRegularExpression versionRegex(R"(version\s+([^\s,]+))");
    const auto match = versionRegex.match(output);
    return match.hasMatch() ? match.captured(1) : QString();
}

// Rebuild /boot/archiso.img for the selected kernel. Succeeds only when the new
// image is (as far as lsinitcpio can tell) built for that kernel: a preset builds
// for its own ALL_kver, which need not be the kernel chosen for the snapshot.
bool Work::rebuildArchisoInitramfs(const QString &archisoPath, const QString &kernelPath)
{
    if (QStandardPaths::findExecutable("mkinitcpio").isEmpty()) {
        qWarning() << "mkinitcpio not found; cannot rebuild archiso initramfs.";
        return false;
    }
    if (!QFileInfo::exists(kernelPath)) {
        qWarning() << "Kernel image not found:" << kernelPath;
        return false;
    }

    const QString expectedKernel = kernelImageVersion(kernelPath);
    const auto buildMatchesKernel = [&](const QStringList &mkinitcpioArgs) {
        emit message(tr("Rebuilding initramfs with: mkinitcpio %1").arg(mkinitcpioArgs.join(' ')));
        if (!shell.procAsRoot("mkinitcpio", mkinitcpioArgs, nullptr, nullptr, Cmd::QuietMode::No)
            || !QFileInfo::exists(archisoPath)) {
            return false;
        }
        // An unreadable version cannot be checked; only a definite mismatch fails.
        const QString builtKernel = initramfsKernelVersion(archisoPath);
        if (!expectedKernel.isEmpty() && !builtKernel.isEmpty() && builtKernel != expectedKernel) {
            qWarning() << "mkinitcpio" << mkinitcpioArgs << "built" << archisoPath << "for kernel" << builtKernel
                       << "but the selected kernel is" << expectedKernel;
            return false;
        }
        return true;
    };

    QString presetName;
    const QDir presetDir("/etc/mkinitcpio.d");
    if (presetDir.exists()) {
        const QStringList presets = presetDir.entryList({"*.preset"}, QDir::Files, QDir::Name);
        for (const QString &presetFile : presets) {
            if (presetFile == "archiso.preset") {
                presetName = "archiso";
                break;
            }
            QFile file(presetDir.filePath(presetFile));
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }
            QTextStream stream(&file);
            while (!stream.atEnd()) {
                const QString line = stream.readLine();
                if (line.contains("archiso.img")) {
                    presetName = presetFile.left(presetFile.size() - QString(".preset").size());
                    break;
                }
            }
            if (!presetName.isEmpty()) {
                break;
            }
        }
    }
    if (!presetName.isEmpty()) {
        if (buildMatchesKernel({"-p", presetName})) {
            return true;
        }
        qWarning() << "The archiso preset" << presetName
                   << "did not produce an initramfs for the selected kernel; building it directly.";
    }

    const QStringList configCandidates {
        "/usr/lib/archiso/mkinitcpio.conf",
        "/etc/mkinitcpio-archiso.conf",
        "/usr/share/archiso/configs/releng/airootfs/etc/mkinitcpio.conf.d/archiso.conf",
        "/usr/share/archiso/configs/baseline/airootfs/etc/mkinitcpio.conf.d/archiso.conf",
    };
    QString configPath;
    for (const QString &candidate : configCandidates) {
        if (QFileInfo::exists(candidate)) {
            configPath = candidate;
            break;
        }
    }
    if (configPath.isEmpty()) {
        qWarning() << "No archiso config found for rebuilding archiso initramfs.";
        return false;
    }
    return buildMatchesKernel({"-c", configPath, "-k", kernelPath, "-g", archisoPath});
}

// Replace text in menu items in grub.cfg, syslinux.cfg, isolinux.cfg
void Work::replaceMenuStrings()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QString fullDistroNameSpace = settings->fullDistroName;
    fullDistroNameSpace.replace("_", " ");

    // Guard each grub file with an existence check: templates don't all ship
    // every file, and the grubenv append below would otherwise create a stray
    // grubenv.cfg in the ISO via the >> redirection.
    const QString grub_cfg {"/iso-template/boot/grub/grub.cfg"};
    const bool haveGrubCfg = QFileInfo::exists(settings->workDir + grub_cfg);
    if (haveGrubCfg) {
        replaceStringInFile("%DISTRO%", settings->projectName + "-" + settings->distroVersion,
                            settings->workDir + grub_cfg);
        replaceStringInFile("%DISTRO_NAME%", settings->projectName, settings->workDir + grub_cfg);
        replaceStringInFile("%FULL_DISTRO_NAME%", settings->fullDistroName, settings->workDir + grub_cfg);
        replaceStringInFile("%FULL_DISTRO_NAME_SPACE%", fullDistroNameSpace, settings->workDir + grub_cfg);
        replaceStringInFile("%RELEASE_DATE%", settings->releaseDate, settings->workDir + grub_cfg);
    }

    const QString grubenv_cfg {"/iso-template/boot/grub/grubenv.cfg"};
    const QString boot_pararameter_regexp {"(lang|kbd|kbvar|kbopt|tz)=[^[:space:]]*"};
    // bootOptions ($1) and the target file ($2) are passed as bash positional
    // parameters so the shell never parses them as code (no injection through a
    // crafted work dir or kernel option). $1 is intentionally left unquoted in
    // printf: word splitting emits one option per line, while positional expansion
    // still prevents command-substitution/quote injection in the value.
    if (QFileInfo::exists(settings->workDir + grubenv_cfg)) {
        const QString grubenv_script = "printf '%s\\n' $1 | grep -E '^" + boot_pararameter_regexp + "' >> \"$2\"";
        shell.proc("/bin/bash", {"-c", grubenv_script, "_", settings->bootOptions, settings->workDir + grubenv_cfg});
    }
    if (haveGrubCfg) {
        const QString options_script = "sed -i \"s|%OPTIONS%|$(sed -r 's/[[:space:]]" + boot_pararameter_regexp
            + "/ /g; s/^[[:space:]]+//; s/[[:space:]]+/ /g' <<<\" $1\")|\" \"$2\"";
        shell.proc("/bin/bash", {"-c", options_script, "_", settings->bootOptions, settings->workDir + grub_cfg});
    }
    // The Arch ISO template is GRUB-only — no syslinux / isolinux. Skip
    // files that aren't present so we don't spam "Failed to open file:"
    // warnings during snapshots on Arch.
    const QString syslinux_cfg {"/iso-template/boot/syslinux/syslinux.cfg"};
    const QString isolinux_cfg {"/iso-template/boot/isolinux/isolinux.cfg"};
    const QString checkmediasys_cfg {"/iso-template/boot/syslinux/checkmedia.cfg"};
    const QString checkmediaiso_cfg {"/iso-template/boot/isolinux/checkmedia.cfg"};
    for (const QString &file : {syslinux_cfg, isolinux_cfg, checkmediaiso_cfg, checkmediasys_cfg}) {
        const QString fullPath = settings->workDir + file;
        if (!QFileInfo::exists(fullPath)) {
            continue;
        }
        replaceStringInFile("%OPTIONS%", settings->bootOptions, fullPath);
        replaceStringInFile("%CODE_NAME%", settings->codename, fullPath);
    }

    const QString sys_readme = "/iso-template/boot/syslinux/readme.msg";
    const QString iso_readme = "/iso-template/boot/isolinux/readme.msg";
    const QStringList cfg_files {syslinux_cfg, isolinux_cfg, sys_readme, iso_readme, checkmediaiso_cfg, checkmediasys_cfg};
    for (const QString &file : cfg_files) {
        const QString fullPath = settings->workDir + file;
        if (!QFileInfo::exists(fullPath)) {
            continue;
        }
        replaceStringInFile("%FULL_DISTRO_NAME%", settings->fullDistroName, fullPath);
        replaceStringInFile("%RELEASE_DATE%", settings->releaseDate, fullPath);
    }

    QDir themeDir(settings->workDir + "/iso-template/boot/grub/theme");
    for (const QFileInfo &themeFile : themeDir.entryInfoList({"*.txt"}, QDir::Files)) {
        replaceStringInFile("%ASCII_CODE_NAME%", settings->codename, themeFile.absoluteFilePath());
        replaceStringInFile("%DISTRO%", settings->projectName + "-" + settings->distroVersion,
                            themeFile.absoluteFilePath());
    }
}

// Util function for replacing strings in files
bool Work::replaceStringInFile(const QString &old_text, const QString &new_text, const QString &file_path)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << file_path;
        return false;
    }

    QTextStream stream(&file);
    QString content = stream.readAll();

    if (content.contains(old_text)) {
        content.replace(old_text, new_text);

        file.resize(0);
        stream.seek(0);
        stream << content;
        stream.flush();

        if (stream.status() != QTextStream::Ok) {
            qWarning() << "Failed to write to file:" << file_path;
            file.close();
            return false;
        }
    }

    file.close();
    return true;
}

// Save package list in working directory
void Work::savePackageList(const QString &file_name)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (settings->isArch) {
        const QString archCpuDir = settings->x86 ? "i686" : "x86_64";
        const QString archDir = settings->workDir + "/iso-template/arch";
        QDir().mkpath(archDir);
        const QString fullName = QString("%1/pkglist.%2.txt").arg(archDir, archCpuDir);
        // fullName rides as $1, a positional parameter the shell never parses,
        // so it can't be used to break out of the redirect target. pipefail so a
        // pacman failure isn't masked by awk succeeding on empty input.
        const bool listOk
            = shell.proc("/bin/bash", {"-c", R"(set -o pipefail; pacman -Q | awk '{print $1 " " $2}' > "$1")", "_",
                                       fullName});
        if (!listOk || QFileInfo(fullName).size() <= 0) {
            emit messageBox(BoxType::critical, tr("Error"), tr("Could not create the package list: ") + fullName);
            cleanUp();
        }
        return;
    }
    QFileInfo fi(file_name);
    QDir dir(settings->workDir + "/iso-template/" + fi.completeBaseName());
    if (!dir.mkpath(dir.absolutePath())) {
        emit messageBox(BoxType::critical, tr("Error"),
                        tr("Could not create working directory. ") + dir.absolutePath());
        cleanUp();
        return;
    }
    // file_name is user-controlled (CLI -f/--file or the GUI output filename), so
    // fullName rides as $1, a positional parameter the shell never parses, rather
    // than being interpolated into the command string. pipefail so a dpkg failure
    // isn't masked by awk succeeding on empty input.
    const QString fullName = QString("%1/iso-template/%2/package_list").arg(settings->workDir, fi.completeBaseName());
    const bool listOk
        = shell.proc("/bin/bash", {"-c", R"(set -o pipefail; dpkg -l | awk '/^ii /{printf "%-41s %s\n", $2, $3}' > "$1")",
                                   "_", fullName});
    if (!listOk || QFileInfo(fullName).size() <= 0) {
        emit messageBox(BoxType::critical, tr("Error"), tr("Could not create the package list: ") + fullName);
        cleanUp();
    }
}

// Setup the environment before taking the snapshot
void Work::setupEnv()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // Checks if work_dir looks OK
    if (!settings->workDir.contains("/mx-snapshot")) {
        cleanUp();
        return;
    }

    const bool bootIsMount = shell.run("mountpoint /boot");

    // Install installer if absent (mx-installer on Debian, gazelle-installer on Arch)
    const QString installerPkg = settings->isArch ? QStringLiteral("gazelle-installer")
                                                  : QStringLiteral("mx-installer");
    if (settings->forceInstaller && !checkInstalled(installerPkg)) {
        installPackage(installerPkg);
    }

    writeSnapshotInfo();
    // abortIfElevationDenied() routes through cleanUp(), which is not
    // [[noreturn]] here (see Step 4d) — bail out after each checkpoint.
    writeVersionFile();
    abortIfElevationDenied();
    if (cleanupStarted) {
        return;
    }
    writeLsbRelease();
    abortIfElevationDenied();
    if (cleanupStarted) {
        return;
    }

    if (!setupBindRootOverlay()) {
        emit messageBox(BoxType::critical, tr("Error"),
                        tr("Could not prepare a safe bind-root overlay. Snapshot cannot continue."));
        cleanUp();
        return;
    }
    abortIfElevationDenied();
    if (cleanupStarted) {
        return;
    }

    // Arch-only: place an installer Desktop shortcut. On MX, mx-installer's own
    // postinst + installed-to-live flow handles this, so we skip it there.
    QString installerSource;
    if (settings->isArch) {
        if (QFileInfo::exists("/usr/share/applications/gazelle-installer.desktop")) {
            installerSource = "/usr/share/applications/gazelle-installer.desktop";
        } else if (QFileInfo::exists("/usr/share/applications/minstall.desktop")) {
            installerSource = "/usr/share/applications/minstall.desktop";
        }
        if (installerSource.isEmpty()) {
            qDebug() << "No installer desktop file found";
        }
    }

    // On Debian we shell out to mx-remaster's /usr/sbin/installed-to-live;
    // on Arch we use installed-to-live-arch (scripts-arch/), which mirrors
    // the same CLI shape but is implemented as a self-contained shell port
    // of the legacy BindRootManager Qt class.
    const QString installedToLive = settings->isArch ? QStringLiteral("installed-to-live-arch")
                                                     : QStringLiteral("installed-to-live");

    // Setup environment if creating a respin (reset root/demo, remove personal accounts)
    if (settings->resetAccounts) {
        QStringList args {"-F", "-b", bindRootPath, "start"};
        if (bootIsMount) {
            args << "bind=/boot";
        }
        args << "empty=/home" << "general" << "version-file";
        if (settings->isArch) {
            // Only a reset snapshot has the demo account to autologin on tty1.
            args << "live-setup=autologin";
        }
        if (!shell.procAsRoot(installedToLive, args, nullptr, nullptr, Cmd::QuietMode::No)) {
            emit messageBox(BoxType::critical, tr("Error"),
                            tr("Could not prepare the snapshot bind-root environment."));
            cleanUp();
            return;
        }
        // Belt-and-braces: ensure demo's Desktop has the link even if the skel copy
        // happened before our skel symlink was placed.
        if (!installerSource.isEmpty()) {
            const QString demoDesktop = bindRootPath + "/home/demo/Desktop";
            shell.procAsRoot("mkdir", {"-p", demoDesktop}, nullptr, nullptr, Cmd::QuietMode::Yes);
            shell.procAsRoot("ln", {"-sf", installerSource, demoDesktop + "/minstall.desktop"}, nullptr, nullptr,
                             Cmd::QuietMode::Yes);
            shell.procAsRoot("chown", {"-R", "1000:1000", demoDesktop}, nullptr, nullptr, Cmd::QuietMode::Yes);
        }
        if (!resetSnapdIdentity()) {
            emit messageBox(BoxType::critical, tr("Error"),
                            tr("Could not remove this computer's snap store registration from the snapshot."));
            cleanUp();
            return;
        }
    } else {
        QStringList args {"-F", "-b", bindRootPath, "start", "bind=/home"};
        if (bootIsMount) {
            args << "bind=/boot";
        }
        args << "live-files" << "version-file" << "adjtime";
        if (settings->isArch) {
            args << "live-setup";
        }
        if (!shell.procAsRoot(installedToLive, args, nullptr, nullptr, Cmd::QuietMode::No)) {
            emit messageBox(BoxType::critical, tr("Error"),
                            tr("Could not prepare the snapshot bind-root environment."));
            cleanUp();
            return;
        }
        // Drop a link in the current user's Desktop too, so a personal snapshot
        // boots live with a working installer icon.
        if (!installerSource.isEmpty()) {
            const QString currentUser = loggedInUserName();
            if (!currentUser.isEmpty()) {
                const QString userDesktop = "/home/" + currentUser + "/Desktop";
                if (QFileInfo::exists(userDesktop)) {
                    const bool homeIsMountpoint = shell.run("mountpoint -q /home", Cmd::QuietMode::Yes);
                    const QString installerLinkPath = userDesktop + "/minstall.desktop";
                    // Only a live overlay keeps this out of the running system. A
                    // separate /home is bind-mounted into the snapshot, and the
                    // overlay fallback binds / itself, so in both of those cases the
                    // write reaches the real filesystem and has to be taken back out
                    // on cleanup -- by its real path, which stays valid after the
                    // bind is gone.
                    const bool writesToRealFs = homeIsMountpoint || !bindRootOverlayActive;
                    const QString linkPath
                        = homeIsMountpoint ? installerLinkPath : bindRootPath + installerLinkPath;
                    if (placeInstallerLink(shell, installerSource, linkPath, currentUser) && writesToRealFs) {
                        installerLinkToRemove = installerLinkPath;
                    }
                }
            }
        }
    }
    // Setup has mounted the snapshot root. Only change skel in an isolated
    // overlay; plain binds share the host skel. Reset snapshots already have
    // the installer link in the staged demo Desktop above.
    if (!installerSource.isEmpty() && bindRootOverlayActive) {
        const QString skelDesktopDir = bindRootPath + "/etc/skel/Desktop";
        shell.procAsRoot("mkdir", {"-p", skelDesktopDir}, nullptr, nullptr, Cmd::QuietMode::Yes);
        shell.procAsRoot("ln", {"-sf", installerSource, skelDesktopDir + "/minstall.desktop"}, nullptr, nullptr,
                         Cmd::QuietMode::Yes);
    }
    if (!bindRootOverlayActive) {
        shell.procAsRoot(installedToLive, {"-b", bindRootPath, "read-only"}, nullptr, nullptr,
                         Cmd::QuietMode::Yes);
    }
}

void Work::writeLsbRelease()
{
    // The lsb-release template lives under root-owned /usr(/local)/share, so the
    // GUI (unprivileged) cannot write it directly — a plain QFile write silently
    // failed there, shipping stale distro metadata in the ISO. Written through the
    // authenticated helper (not the passwordless snapshot-lib): unlike the other
    // snapshot-lib verbs, this writes arbitrary caller-supplied content to a
    // template file, so it requires admin auth. Values are passed as argv (no shell).
    shell.runHelperAction("write_lsb_release", {settings->projectName, settings->distroVersion, settings->codename},
                          nullptr, Cmd::QuietMode::Yes);
}

// Write date of the snapshot in a "snapshot_created" file
void Work::writeSnapshotInfo()
{
    Cmd().procAsRoot("snapshot-lib", {"datetime_log"}, nullptr, nullptr, Cmd::QuietMode::Yes);
}

void Work::writeVersionFile()
{
    // Same rationale as writeLsbRelease(): the mx-version template is root-owned,
    // so write it through the authenticated helper rather than an unprivileged
    // QFile that silently fails in the GUI. Values are passed as argv and only
    // used as file content.
    shell.runHelperAction("write_version_file", {settings->fullDistroName, settings->codename, settings->releaseDate},
                          nullptr, Cmd::QuietMode::Yes);
}

void Work::writeUnsquashfsSize(const QString &text)
{
    const QString archCpuDir = settings->x86 ? "i686" : "x86_64";
    const QString infoPath = settings->isArch
                                 ? settings->workDir + "/iso-template/arch/" + archCpuDir + "/airootfs.info"
                                 : settings->workDir + "/iso-template/antiX/linuxfs.info";
    QSettings file(infoPath, QSettings::NativeFormat);
    file.setValue("UncompressedSizeKB",
                  text.section(QRegularExpression(" uncompressed filesystem size \\("), 1, 1).section(" ", 0, 0));
}

quint64 Work::getRequiredSpace()
{
    QStringList excludes;
    QFile *file = &settings->snapshotExcludes;

    // Open and read the excludes file — on failure, excludes stays empty
    // so the size estimate is conservative (no exclusions subtracted)
    if (file->open(QIODevice::ReadOnly)) {
        while (!file->atEnd()) {
            QString line = file->readLine().trimmed();
            if (!line.startsWith('#') && !line.isEmpty() && !line.startsWith(".bind-root")) {
                excludes << line;
            }
        }
        file->close();
    } else {
        qWarning() << "Could not open excludes file, space estimate will be conservative:" << file->fileName();
    }

    // Add session excludes
    if (!settings->sessionExcludes.isEmpty()) {
        QString sessionExcludes = settings->sessionExcludes;
        QStringList excludeList = sessionExcludes.split("\" \"");
        excludes.reserve(excludeList.size());
        for (QString exclude : excludeList) {
            exclude = exclude.replace('"', "").trimmed();
            excludes << exclude;
        }
    }
    QString sizeRoot = bindRootPath;
    if (bindRootOverlayActive) {
        const QString overlayLower = bindRootOverlayBase + "/lower";
        if (QFileInfo::exists(overlayLower)) {
            sizeRoot = overlayLower;
        }
    }
    QString sizeRootPrefix = sizeRoot;
    if (!sizeRootPrefix.endsWith('/')) {
        sizeRootPrefix += "/";
    }
    const QStorageInfo sizeRootInfo(sizeRoot);
    const QByteArray sizeRootDevice = sizeRootInfo.device();
    QStorageInfo rootInfo("/");
    QStorageInfo homeInfo("/home");
    QByteArray rootDevice = sizeRootDevice;
    QByteArray homeDevice;
    bool includeHomeDevice = false;
    if (!settings->live) {
        rootDevice = rootInfo.device();
        // isRoot() is true exactly when /home shares the root filesystem, which
        // means device() == rootDevice always holds in that case — the intended
        // check is the opposite: /home is a separate partition (!isRoot()) whose
        // usage must be added to the required-space estimate below. A reset
        // snapshot mounts an empty /home (empty=/home in setupEnv()), so nothing
        // from that partition ends up in it.
        if (!settings->resetAccounts && homeInfo.isValid() && !homeInfo.isRoot()
            && homeInfo.device() != rootDevice) {
            homeDevice = homeInfo.device();
            includeHomeDevice = true;
        }
    }
    // If /home is bind-mounted or reset (empty in overlay), exclude it from the size estimate.
    //
    // A real bind mount always gets its own mount-table entry, so isRoot() is
    // false for it even when its device() matches root's exactly (confirmed
    // empirically: a same-filesystem `mount --bind` reports isRoot=false,
    // device==rootDevice) -- /proc/self/mounts's options field never contains
    // "bind"/"rbind" either (it shows the underlying filesystem's real mount
    // options), so parsing it for that string can never detect one. A
    // genuinely different device is already handled above via
    // includeHomeDevice, so !isRoot() && device()==rootDevice is specifically
    // the signature of a same-device bind mount.
    if (!settings->live && homeInfo.isValid() && homeInfo.device() == rootDevice) {
        bool shouldExclude = false;
        if (!homeInfo.isRoot()) {
            shouldExclude = true;
        } else if (bindRootOverlayActive) {
            const QString overlayHome = bindRootOverlayBase + "/root/home";
            QDir homeDir(overlayHome);
            if (homeDir.exists() && homeDir.isEmpty()) {
                shouldExclude = true;
            }
        }
        if (shouldExclude) {
            excludes << QStringLiteral("home");
        }
    }
    const auto containsWildcard = [](const QString &path) -> bool {
        const QString wildcards = "*?[{";
        for (const QChar &wildcard : wildcards) {
            if (path.contains(wildcard)) {
                return true;
            }
        }
        return false;
    };
    const auto normalizeExclude = [](QString path) -> QString {
        path.replace(QRegularExpression("/+"), "/");
        if (path.size() > 1 && path.endsWith('/')) {
            path.chop(1);
        }
        return path;
    };
    const QString sizeRootBase = sizeRootPrefix.endsWith('/') ? sizeRootPrefix.left(sizeRootPrefix.size() - 1)
                                                              : sizeRootPrefix;
    const auto isAllowedDevice = [&](const QString &path) -> bool {
        QFileInfo info(path);
        QString probePath = path;
        if (info.isSymLink()) {
            probePath = info.dir().absolutePath();
        }
        const QStorageInfo probeInfo(probePath);
        if (!probeInfo.isValid()) {
            return false;
        }
        const QByteArray probeDevice = probeInfo.device();
        return probeDevice == rootDevice || (includeHomeDevice && probeDevice == homeDevice);
    };
    // Expand patterns without shell globbing or symlinked intermediate traversal.
    //
    // The overlay lower directory is a non-recursive bind of /.  Consequently,
    // when /home is a separate filesystem, lower/home is only the underlying
    // (usually empty) mount point and cannot be used to expand home exclusions.
    // Use the real root for those patterns; /home is also bound into the snapshot
    // tree, so it names the same files while preserving the home device identity
    // needed by isAllowedDevice().
    const auto expandExcludePattern = [&](QString rawPattern, const QString &patternRoot) -> QStringList {
        const QString original = rawPattern;
        if (rawPattern.startsWith('/')) {
            rawPattern.remove(0, 1);
        }
        rawPattern.replace(QRegularExpression("/\\*$"), ""); // Remove trailing /*
        QString patternRootPrefix = patternRoot;
        if (!patternRootPrefix.endsWith('/')) {
            patternRootPrefix += '/';
        }
        const QString patternRootBase
            = patternRootPrefix == QStringLiteral("/") ? patternRootPrefix : patternRootPrefix.chopped(1);
        QString fullPattern = QDir(patternRootPrefix).filePath(rawPattern);
        fullPattern = QDir::cleanPath(fullPattern);
        QString relativePattern = fullPattern;
        if (relativePattern.startsWith(patternRootBase)) {
            relativePattern.remove(0, patternRootBase.size());
        }
        if (relativePattern.startsWith('/')) {
            relativePattern.remove(0, 1);
        }
        if (relativePattern.isEmpty()) {
            // A pattern that resolves to the whole root (e.g. "/", ".", "foo/..")
            // is malformed — excluding everything is meaningless. Returning the
            // root here made the nested-path filter collapse every exclude into
            // the root, so excl_size == root_size and the space check was bypassed
            // (near-zero estimate). Ignore it instead, which keeps the estimate
            // conservative (nothing subtracted).
            qWarning() << "Ignoring exclude pattern that resolves to the whole root:" << original;
            return {};
        }
        QStringList components = relativePattern.split('/', Qt::SkipEmptyParts);
        QStringList current {patternRootBase};
        for (int i = 0; i < components.size(); ++i) {
            const QString &component = components.at(i);
            const bool isLast = (i == components.size() - 1);
            QStringList next;
            if (containsWildcard(component)) {
                const QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(component));
                if (!regex.isValid()) {
                    continue;
                }
                for (const QString &base : current) {
                    QFileInfo baseInfo(base);
                    if (!baseInfo.exists() || !baseInfo.isDir() || baseInfo.isSymLink()) {
                        continue;
                    }
                    QDir dir(base);
                    QDir::Filters filters = QDir::NoDotAndDotDot;
                    if (isLast) {
                        filters |= QDir::AllEntries;
                    } else {
                        filters |= QDir::Dirs | QDir::NoSymLinks;
                    }
                    const QFileInfoList entries = dir.entryInfoList(filters);
                    for (const QFileInfo &entry : entries) {
                        if (!regex.match(entry.fileName()).hasMatch()) {
                            continue;
                        }
                        if (!isLast && entry.isSymLink()) {
                            continue;
                        }
                        next.append(entry.filePath());
                    }
                }
            } else {
                for (const QString &base : current) {
                    const QString candidate = QDir(base).filePath(component);
                    QFileInfo candidateInfo(candidate);
                    if (!candidateInfo.exists() && !candidateInfo.isSymLink()) {
                        continue;
                    }
                    if (!isLast) {
                        if (!candidateInfo.isDir() || candidateInfo.isSymLink()) {
                            continue;
                        }
                    }
                    next.append(candidate);
                }
            }
            current = next;
            if (current.isEmpty()) {
                break;
            }
        }
        return current;
    };
    QStringList expandedExcludes;
    expandedExcludes.reserve(excludes.size());
    for (const QString &rawValue : excludes) {
        QString cleaned = rawValue;
        const int bangIndex = cleaned.indexOf('!');
        if (bangIndex != -1) { // Truncate things like "!(minstall.desktop)"
            cleaned.truncate(bangIndex);
        }
        if (cleaned.isEmpty()) {
            continue;
        }
        QString relativeExclude = cleaned;
        if (relativeExclude.startsWith('/')) {
            relativeExclude.remove(0, 1);
        }
        const bool isHomeExclude = relativeExclude == QStringLiteral("home")
                                   || relativeExclude.startsWith(QStringLiteral("home/"));
        const QString patternRoot = includeHomeDevice && isHomeExclude ? QStringLiteral("/") : sizeRootBase;
        const QStringList matches = expandExcludePattern(cleaned, patternRoot);
        for (const QString &match : matches) {
            if (!isAllowedDevice(match)) {
                continue;
            }
            const QString normalized = normalizeExclude(match);
            if (!normalized.isEmpty()) {
                expandedExcludes.append(normalized);
            }
        }
    }
    excludes = expandedExcludes;

    // Filter out nested paths to avoid double-counting in size calculation
    std::sort(excludes.begin(), excludes.end(), [](const QString &a, const QString &b) {
        return a.length() < b.length();
    });

    QStringList filteredExcludes;
    for (const QString &path : excludes) {
        bool isNested = false;
        for (const QString &accepted : filteredExcludes) {
            if (accepted == "/") {
                isNested = path != "/";
                break;
            }
            if (path == accepted || path.startsWith(accepted + '/')) {
                isNested = true;
                break;
            }
        }
        if (!isNested) {
            filteredExcludes.append(path);
        }
    }
    excludes = filteredExcludes;

    emit message(tr("Calculating total size of excluded files..."));
    bool ok = true;
    quint64 excl_size = 0;
    if (!excludes.isEmpty()) {
        QTemporaryFile excludeList;
        excludeList.setAutoRemove(true);
        if (!excludeList.open()) {
            ok = false;
        } else {
            for (const QString &path : excludes) {
                excludeList.write(path.toLocal8Bit());
                excludeList.write("\0", 1);
            }
            excludeList.flush();
            QStringList duArgs = settings->live ? QStringList {"-sc", "-P", "--apparent-size"}
                                                : QStringList {"-sxc", "-P"};
            duArgs << "--files0-from=" + excludeList.fileName();
            excl_size = parseDuKilobytes(shell.getOutAsRoot("du", duArgs, Cmd::QuietMode::Yes), &ok);
            excludeList.close();
        }
    }
    if (!ok) {
        qDebug() << "Error: calculating size of excluded files\n"
                    "If you are sure you have enough free space rerun the program with -o/--override-size option";
        cleanUp();
        return 0;
    }
    emit message(tr("Calculating size of root..."));
    quint64 root_size = 0;
    if (settings->live) {
        root_size = parseDuKilobytes(shell.getOutAsRoot("du", {"-s", "-P", "--apparent-size", sizeRoot},
                                                        Cmd::QuietMode::Yes),
                                     &ok);
    } else {
        ok = rootInfo.isValid() && rootInfo.isReady();
        if (ok) {
            root_size = (rootInfo.bytesTotal() - rootInfo.bytesFree()) / 1024;
            if (includeHomeDevice) {
                ok = homeInfo.isValid() && homeInfo.isReady();
                if (ok) {
                    root_size += (homeInfo.bytesTotal() - homeInfo.bytesFree()) / 1024;
                }
            }
        }
    }
    constexpr double kibToMib = 1024.0;
    if (!ok) {
        qDebug() << "Error: calculating root size.\n"
                    "If you are sure you have enough free space rerun the program with -o/--override-size option";
        cleanUp();
        return 0;
    }
    if (excl_size > root_size) {
        qWarning() << "Excluded size exceeds root size; clamping excluded size for estimate."
                   << "Root:" << root_size << "Excluded:" << excl_size;
        excl_size = root_size;
    }
    qDebug().noquote() << "SIZE         " << QString::number(root_size / kibToMib, 'f', 2) << "MiB";
    qDebug().noquote() << "SIZE EXCLUDES" << QString::number(excl_size / kibToMib, 'f', 2) << "MiB";
    const uint c_factor = settings->compressionFactor.value(settings->compression);
    qDebug() << "COMPRESSION  " << c_factor;
    qDebug().noquote() << "SIZE NEEDED  "
                       << QString::number(((root_size - excl_size) * c_factor / 100.0) / kibToMib, 'f', 2) << "MiB";
    qDebug().noquote() << "SIZE FREE    " << QString::number(settings->freeSpace / kibToMib, 'f', 2) << "MiB" << '\n';

    return (root_size - excl_size) * c_factor / 100;
}
