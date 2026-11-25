/**********************************************************************
 *  batchprocessing.cpp
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

#include "batchprocessing.h"

#include <QDebug>
#include <QTextStream>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "excludesutils.h"
#include "log.h"
#include "squashfsutils.h"
#include "terminalcursor.h"
#include "unitsecrets.h"
#include "work.h"

using namespace std::chrono_literals;

namespace
{
// The excludes prompt blocks in read(2) with no event loop running, so the
// self-pipe signal handler in main.cpp could never act on a signal there.
// Nothing has been set up yet, so let SIGINT/SIGTERM/SIGHUP end the process
// while waiting for an answer, then restore the previous handlers.
class DefaultSignalsWhilePrompting
{
public:
    DefaultSignalsWhilePrompting()
    {
        for (std::size_t i = 0; i < handledSignals.size(); ++i) {
            previous[i] = std::signal(handledSignals[i], SIG_DFL);
        }
    }
    ~DefaultSignalsWhilePrompting()
    {
        for (std::size_t i = 0; i < handledSignals.size(); ++i) {
            std::signal(handledSignals[i], previous[i]);
        }
    }
    DefaultSignalsWhilePrompting(const DefaultSignalsWhilePrompting &) = delete;
    DefaultSignalsWhilePrompting &operator=(const DefaultSignalsWhilePrompting &) = delete;

private:
    static constexpr std::array<int, 3> handledSignals {SIGINT, SIGTERM, SIGHUP};
    std::array<void (*)(int), 3> previous {};
};
} // namespace

Batchprocessing::Batchprocessing(Settings *settings, QObject *parent)
    : QObject(parent),
      settings(settings),
      work(settings, this)
{
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { work.cleanUp(); });
    setConnections();
}

// cancel() can arrive from a signal during any step: Cmd waits in nested event
// loops that still deliver it. Each step is therefore followed by an
// isCleaningUp() check, so nothing runs against a torn-down environment.
void Batchprocessing::run()
{
    const QString mountWarning = SystemInfo::systemMountWarning();
    if (!mountWarning.isEmpty()) {
        if (!settings->allowOmittedMounts) {
            qCritical().noquote() << mountWarning << tr("Use --allow-omitted-mounts to continue.");
            return;
        }
        qWarning().noquote() << mountWarning;
    }

    // Check updated excludes before any work
    checkUpdatedDefaultExcludesCli();
    if (work.isCleaningUp()) {
        return;
    }

    if (!settings->checkCompression()) {
        qCritical().noquote() << tr("Error")
                              << tr("Current kernel doesn't support selected compression algorithm, "
                                    "please edit the configuration file and select a different algorithm.");
        return;
    }

    const QString path = FileSystemUtils::nearestExistingPath(settings->snapshotDir);
    qDebug() << "Free space:" << settings->getFreeSpaceStrings(path);
    if (!settings->monthly && !settings->overrideSize) {
        qDebug() << "Unused space:" << settings->getUsedSpace();
    }
    if (work.isCleaningUp()) {
        return;
    }

    // Non-interactive: name services whose credentials go into the respin, but
    // don't stop (see MainWindow::confirmUnitCredentials()).
    if (settings->resetAccounts) {
        const UnitSecrets::ScanResult scan = UnitSecrets::findCredentials(QStringLiteral("/etc/systemd/system"));
        if (!scan.findings.isEmpty()) {
            qWarning().noquote()
                << tr("Warning: these services in /etc/systemd/system contain what look like passwords, tokens or "
                      "keys, and will be included in the snapshot as they are:")
                << scan.findings.join(", ");
        }
        if (!scan.unreadable.isEmpty()) {
            qWarning().noquote() << tr("Warning: these service files or folders could not be checked for passwords, tokens "
                                       "or keys, and will be included in the snapshot as they are:")
                                 << scan.unreadable.join(", ");
        }
    }

    TerminalCursor cursor(STDERR_FILENO);
    work.startTimer();
    Cmd::clearElevationDenied();
    if (!settings->checkSnapshotDir() || !settings->checkTempDir()) {
        work.cleanUp();
        return;
    }
    if (work.isCleaningUp()) {
        return;
    }
    settings->otherExclusions();
    work.setupEnv();
    abortIfElevationDenied();
    if (work.isCleaningUp()) {
        return;
    }
    if (!settings->monthly && !settings->overrideSize) {
        work.checkEnoughSpace();
        if (work.isCleaningUp()) {
            return;
        }
    }
    // checkEnoughSpace() can relocate the work dir onto another partition, which
    // re-runs setupEnv() (and its own privileged steps) internally — check again
    // here in case that nested run was the one that got denied.
    abortIfElevationDenied();
    if (work.isCleaningUp()) {
        return;
    }
    work.copyNewIso();
    abortIfElevationDenied();
    if (work.isCleaningUp()) {
        return;
    }
    work.savePackageList(settings->snapshotName);
    if (work.isCleaningUp()) {
        return;
    }

    if (settings->editBootMenu) {
        cursor.show();
        qDebug() << tr("The program will pause the build and open the boot menu in your text editor.");
        // getEditorCommand() is a resolved argument list executed directly (no
        // shell), so neither the configured editor nor the workDir-derived file
        // paths can inject commands.
        const QString bootDir = settings->workDir + "/iso-template/boot";
        QStringList editorCmd = settings->getEditorCommand();
        const QString editorProgram = editorCmd.takeFirst();
        const bool editorSucceeded
            = Cmd().proc(editorProgram, editorCmd << bootDir + "/grub/grub.cfg"
                                                  << bootDir + "/syslinux/syslinux.cfg"
                                                  << bootDir + "/isolinux/isolinux.cfg");
        if (!editorSucceeded) {
            qCritical().noquote()
                << tr("The boot-menu editor failed; the snapshot cannot continue with potentially unedited files.");
            work.cleanUp();
            return;
        }
        if (work.isCleaningUp()) {
            return;
        }
        cursor.hide();
    }
    disconnect(&timer, &QTimer::timeout, nullptr, nullptr);
    work.createIso(settings->snapshotName);
}

// SIGINT/SIGTERM/SIGHUP: cleanUp() kills mksquashfs, tears the bind-root down
// and requests exit; it is a no-op once a teardown has started. run() sees
// isCleaningUp() when the interrupted step returns.
void Batchprocessing::cancel()
{
    work.cleanUp();
}

// A failed or refused root operation (the CLI normally runs as root, so this
// means the privileged helper rejected a command) must abort the run through
// the regular cleanup path rather than continue with skipped steps.
void Batchprocessing::abortIfElevationDenied()
{
    if (!Cmd::elevationDenied()) {
        return;
    }
    qCritical().noquote() << tr("Administrator access was not granted; the snapshot cannot continue.");
    work.cleanUp();
}

void Batchprocessing::setConnections()
{
    connect(&timer, &QTimer::timeout, this, &Batchprocessing::progress);
    connect(&work.shell, &Cmd::started, this, [this] { timer.start(500ms); });
    connect(&work.shell, &Cmd::done, this, [this] { timer.stop(); });
    connect(&work.shell, &Cmd::outputAvailable, this,
            [this](const QString &out) { handleStreamChunk(stdoutBuffer, out, true); });
    connect(&work.shell, &Cmd::errorAvailable, this,
            [this](const QString &out) { handleStreamChunk(stderrBuffer, out, false); });
    connect(&work, &Work::message, [](const QString &out) { qDebug().noquote() << out; });
    connect(&work, &Work::messageBox,
            [](BoxType /*unused*/, const QString &title, const QString &msg) { qDebug().noquote() << title << msg; });
}

void Batchprocessing::progress()
{
    static bool toggle = false;
    QTextStream stream(stderr);
    stream << "\033[2KProcessing command" << (toggle ? "...\r" : "\r");
    stream.flush();
    toggle = !toggle;
}

void Batchprocessing::handleStreamChunk(QString &buffer, const QString &chunk, bool toStdout)
{
    for (const QChar ch : chunk) {
        if (ch == QLatin1Char('\n')) {
            flushStreamLine(buffer, toStdout, false);
            buffer.clear();
            continue;
        }
        if (ch == QLatin1Char('\r')) {
            flushStreamLine(buffer, toStdout, true);
            buffer.clear();
            continue;
        }
        buffer += ch;
    }
}

void Batchprocessing::flushStreamLine(const QString &line, bool toStdout, bool isTransient)
{
    bool ok = false;
    const int percentage = SquashfsUtils::parsePercentageLine(line, &ok);
    QTextStream stderrStream(stderr);
    if (ok) {
        stderrStream << QStringLiteral("\r\033[K %1%").arg(percentage);
        stderrStream.flush();
        progressLineActive = true;
        return;
    }
    if (line.isEmpty() && isTransient) {
        return;
    }
    QTextStream stream(toStdout ? stdout : stderr);
    if (progressLineActive) {
        stream << QLatin1Char('\n');
        progressLineActive = false;
    }
    if (isTransient) {
        stream << QStringLiteral("\r\033[K") << line;
    } else {
        stream << line << QLatin1Char('\n');
        // Mirror committed lines into the log file so /tmp/iso-snapshot-cli.log
        // captures mksquashfs/xorriso output (transient \r progress lines and
        // the percentage indicator are intentionally skipped to avoid spam).
        Log::appendToFile(toStdout ? QtDebugMsg : QtWarningMsg, line);
    }
    stream.flush();
}

bool Batchprocessing::isSourceExcludesNewer(QString &diffOutput) const
{
    const QString configuredPath = settings->snapshotExcludes.fileName();
    const QString sourcePath = settings->getExcludesSourcePath();

    qDebug().noquote() << "CLI excludes check:"
                       << "configured=" << configuredPath
                       << "source=" << sourcePath;

    return ExcludesUtils::isSourceExcludesNewer(configuredPath, sourcePath, diffOutput);
}

QString Batchprocessing::colorizeDiffAnsi(const QString &diff) const
{
    static const QString green = "\033[32m";
    static const QString red = "\033[31m";
    static const QString blue = "\033[34m";
    static const QString gray = "\033[90m";
    static const QString reset = "\033[0m";

    QStringList lines = diff.split('\n');
    QString colored;
    for (const QString &line : lines) {
        if (line.startsWith("+++ ") || line.startsWith("--- ")) {
            colored += gray + line + reset + '\n';
        } else if (line.startsWith("@@")) {
            colored += blue + line + reset + '\n';
        } else if (line.startsWith('+')) {
            colored += green + line + reset + '\n';
        } else if (line.startsWith('-')) {
            colored += red + line + reset + '\n';
        } else {
            colored += line + '\n';
        }
    }
    return colored;
}

bool Batchprocessing::resetCustomExcludesCli(const QString &configuredPath, const QString &sourcePath) const
{
    return ExcludesUtils::resetCustomExcludes(configuredPath, sourcePath);
}

void Batchprocessing::checkUpdatedDefaultExcludesCli()
{
    QString diffOutput;
    if (!isSourceExcludesNewer(diffOutput)) {
        return;
    }

    const QString configuredPath = settings->snapshotExcludes.fileName();
    const QString sourcePath = settings->getExcludesSourcePath();
    qDebug().noquote() << tr("Detected newer exclusion file at %1 compared to %2. Prompting for action.")
                              .arg(sourcePath, configuredPath);

    QTextStream out(stdout);
    QTextStream in(stdin);
    const DefaultSignalsWhilePrompting defaultSignals;

    const QString showOptionKey = tr("s", "CLI excludes prompt: single-letter shortcut for 'show diff'");
    const QString useOptionKey = tr("u", "CLI excludes prompt: single-letter shortcut for 'use updated default'");
    const QString keepOptionKey = tr("k", "CLI excludes prompt: single-letter shortcut for 'keep custom (update timestamp)'");
    const QString quitOptionKey = tr("q", "CLI excludes prompt: single-letter shortcut for 'quit'");

    const QString showOptionText = tr("show diff", "CLI excludes prompt option label");
    const QString useOptionText = tr("use updated default", "CLI excludes prompt option label");
    const QString keepOptionText = tr("keep custom (update timestamp)", "CLI excludes prompt option label");
    const QString quitOptionText = tr("quit", "CLI excludes prompt option label");

    const QString optionPrompt =
        tr("[%1]%2  [%3]%4  [%5]%6  [%7]%8: ")
            .arg(showOptionKey, showOptionText, useOptionKey, useOptionText, keepOptionKey, keepOptionText, quitOptionKey,
                 quitOptionText);

    while (true) {
        out << tr("The exclusion file at %1 is newer than your configured file at %2.")
                   .arg(sourcePath, configuredPath)
            << '\n';
        out << optionPrompt << Qt::flush;

        const QString response = in.readLine().trimmed();

        if (response.compare(showOptionKey, Qt::CaseInsensitive) == 0
            || response.compare(showOptionText, Qt::CaseInsensitive) == 0) {
            out << colorizeDiffAnsi(diffOutput) << Qt::flush;
            continue;
        }

        if (response.compare(useOptionKey, Qt::CaseInsensitive) == 0
            || response.compare(useOptionText, Qt::CaseInsensitive) == 0) {
            if (resetCustomExcludesCli(configuredPath, sourcePath)) {
                qDebug().noquote() << tr("Reverted to updated default exclusion file.");
                return;
            }
            out << tr("Could not replace the exclusion file with the updated default.") << '\n';
            continue;
        }

        if (response.compare(keepOptionKey, Qt::CaseInsensitive) == 0
            || response.compare(keepOptionText, Qt::CaseInsensitive) == 0) {
            [[maybe_unused]] const bool touched = ExcludesUtils::touchExcludesTimestamp(configuredPath);
            return;
        }

        if (response.compare(quitOptionKey, Qt::CaseInsensitive) == 0
            || response.compare(quitOptionText, Qt::CaseInsensitive) == 0 || response.isEmpty()) {
            // A null (as opposed to merely empty) line means readLine() hit EOF —
            // there was no terminal to prompt (e.g. a cron/piped run) rather than a
            // deliberate choice.
            if (response.isNull()) {
                qCritical().noquote() << tr(
                    "No input available to answer the exclusion file prompt; aborting without creating a snapshot.");
            } else {
                qDebug() << tr("Leaving custom exclusion file unchanged.");
            }
            const bool debugStop = qEnvironmentVariableIsSet("MX_SNAPSHOT_EXCLUDES_DEBUG_STOP");
            if (debugStop) {
                qDebug() << "Debug stop requested; exiting after excludes check.";
                std::exit(0);
            }
            // Either way no snapshot is produced here, so this must not report success.
            std::exit(EXIT_FAILURE);
        }

        out << tr("Invalid choice. Please select again.") << '\n';
    }
}
