/**********************************************************************
 *  main.cpp
 **********************************************************************
 * Copyright (C) 2015-2026 MX Authors
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
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include <QScopeGuard>
#include <QSocketNotifier>

#include <cstdio>
#include <csignal>
#include <cstring>
#include <exception>
#include <functional>
#include <sys/socket.h>
#include <unistd.h>

#ifndef CLI_BUILD
#include "mainwindow.h"
#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#endif

#include "batchprocessing.h"
#include "common.h"
#include "log.h"
#include "instancelock.h"
#include "messagehandler.h"
#include "cmd.h"

#ifndef VERSION
    #define VERSION "?.?.?.?"
#endif

static QTranslator qtTran, qtBaseTran, appTran;
// Self-pipe pair for POSIX signal delivery: the raw handler writes the signal
// number to signalFd[1]; a QSocketNotifier on signalFd[0] does the Qt work on
// the event loop (qDebug/quit are not async-signal-safe).
static int signalFd[2] = {-1, -1};
// Cancel action of the running front end (CLI pipeline or main window) for
// SIGINT/SIGTERM/SIGHUP. QCoreApplication::quit() cannot do this: it does
// nothing outside exec(), where the CLI pipeline runs, and exiting the event
// loops mid-step would make every later Cmd wait return without waiting.
static std::function<void()> cancelRun;
// Set by a signal that arrives before a front end can cancel, e.g. while
// Settings waits for authentication: QCoreApplication::exit() ended those
// nested waits, and no run may start afterwards.
static bool quitRequested = false;
QString currentKernel {};

void checkSquashfs();
void setTranslation();
bool takeInstanceLock();
void signalHandler(int signal);

int main(int argc, char *argv[])
{
    if (getuid() == 0) {
        qputenv("XDG_RUNTIME_DIR", "/run/user/0");
        qunsetenv("SESSION_MANAGER");
        qputenv("HOME", "/root");
    }

    // Parse arguments early so --help/--version can exit without creating a GUI application.
    // Note: QCommandLineParser help text is translated when options/descriptions are created,
    // so for localized help/version we construct a temporary QCoreApplication and build the parser after installing translators.
    QStringList arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        arguments << QString::fromLocal8Bit(argv[i]);
    }
    const bool wantsHelp = arguments.contains(QLatin1String("--help")) || arguments.contains(QLatin1String("-h"));
    const bool wantsVersion = arguments.contains(QLatin1String("--version"));

    // Non-blocking on both ends: the handler must never block, and the reader
    // just drains whatever is queued.
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, signalFd) != 0) {
        perror("socketpair");
    }
    const std::array<int, 3> signalList {SIGINT, SIGTERM, SIGHUP}; // allow SIGQUIT CTRL-\?
    for (auto signalName : signalList) {
        signal(signalName, signalHandler);
    }

    QProcess proc;
    proc.start("logname", {}, QIODevice::ReadOnly);
    proc.waitForFinished();
    const QString logname = QString::fromLatin1(proc.readAllStandardOutput().trimmed());

    QCoreApplication::setApplicationVersion(VERSION);
    QCoreApplication::setApplicationName(QFileInfo(QString::fromLocal8Bit(argv[0])).baseName());
    QCoreApplication::setOrganizationName("MX-Linux");

    const auto setupParser = [](QCommandLineParser &parser) {
        parser.setApplicationDescription(QObject::tr("Tool used for creating a live-CD from the running system"));
        parser.addHelpOption();
        parser.addVersionOption();
#ifndef CLI_BUILD
        parser.addOption({{"c", "cli"}, QObject::tr("Use CLI only")});
#endif

        const QVector<QCommandLineOption> options {
            {"cores", QObject::tr("Number of CPU cores to be used."), "number"},
            {{"d", "directory"}, QObject::tr("Output directory"), "path"},
            {{"f", "file"}, QObject::tr("Output filename"), "name"},
            {{"k", "kernel"},
             QObject::tr("Name a different kernel to use other than the default running kernel, use format returned by "
                         "'uname -r'")
                 + " " + QObject::tr("Or the full path: %1").arg("/boot/vmlinuz-x.xx.x..."),
             "version, or path"},
            {{"l", "compression-level"},
             QObject::tr("Compression level options.") + " "
                 + QObject::tr("Use quotes: \"-Xcompression-level <level>\", or \"-Xalgorithm <algorithm>\", or \"-Xhc\", "
                               "see mksquashfs man page"),
             "\"option\""},
            {{"m", "month"},
             QObject::tr("Create a monthly snapshot, add 'Month' name in the ISO name, skip used space calculation") + " "
                 + QObject::tr("This option sets reset-accounts and compression to defaults, arguments changing those "
                               "items will be ignored") + " "
                 + QObject::tr("Optionally specify a suffix to add to the month name (e.g., '1' for 'July.1')"), ""},
            {{"n", "no-checksums"}, QObject::tr("Don't calculate checksums for resulting ISO file"), ""},
            {{"o", "override-size"}, QObject::tr("Skip calculating free space to see if the resulting ISO will fit"), ""},
            {{"p", "preempt"}, QObject::tr("Option to fix issue with calculating checksums on preempt_rt kernels"), ""},
            {{"r", "reset"}, QObject::tr("Resetting accounts (for distribution to others)"), ""},
            {{"s", "checksums"}, QObject::tr("Calculate checksums for resulting ISO file"), ""},
            {{"t", "throttle"},
             QObject::tr("Throttle the I/O input rate by the given percentage. This can be used to reduce the I/O and CPU "
                         "consumption of Mksquashfs."),
             "number"},
            {{"w", "workdir"}, QObject::tr("Work directory"), "path"},
            {{"x", "exclude"},
             QObject::tr("Exclude main folders, valid choices: ")
                 + "Desktop, Documents, Downloads, Flatpaks, Music, Networks, Pictures, Steam, Videos, VirtualBox. "
                 + QObject::tr("Use the option one time for each item you want to exclude"),
             "one item"},
            {{"z", "compression"},
             QObject::tr("Compression format, valid choices: ") + "lz4, lzo, gzip, xz, zstd",
             "format"},
            {"shutdown", QObject::tr("Shutdown computer when done.")},
            {"grub-mbr", QObject::tr("Use grub for legacy mbr iso boot instead of isolinux/syslinux.")},
            {"nvidia", QObject::tr("Add the 'xorg=nvidia' boot option to the ISO (for booting on a system with an "
                                   "NVIDIA card).")}};

        for (const auto &option : options) {
            parser.addOption(option);
        }
        parser.addPositionalArgument(
            QStringLiteral("suffix"),
            QObject::tr("Optional suffix for a monthly snapshot; only valid together with --month."),
            QStringLiteral("[suffix]"));
    };

    if (wantsHelp || wantsVersion) {
        QCoreApplication tempApp(argc, argv);
        setTranslation();

        QCommandLineParser parser;
        setupParser(parser);
        parser.process(tempApp);

        if (wantsHelp) {
            fputs(qPrintable(parser.helpText()), stdout);
            return EXIT_SUCCESS;
        }
        if (wantsVersion) {
            printf("%s %s\n", qPrintable(QCoreApplication::applicationName()),
                   qPrintable(QCoreApplication::applicationVersion()));
            return EXIT_SUCCESS;
        }
    }

    QCommandLineParser parser;
    setupParser(parser);

    if (!parser.parse(arguments)) {
        fprintf(stderr, "%s\n", qPrintable(parser.errorText()));
        return EXIT_FAILURE;
    }
    const QStringList positionalArgs = parser.positionalArguments();
    if (!positionalArgs.isEmpty() && (!parser.isSet("month") || positionalArgs.size() != 1)) {
        fprintf(stderr, "%s\n",
                qPrintable(QObject::tr("A single suffix positional argument is only valid together with --month.")));
        return EXIT_FAILURE;
    }
    const QString compressionValue = parser.value("compression");
    const QStringList allowedComp {"lz4", "lzo", "gzip", "xz", "zstd"};
    if (!compressionValue.isEmpty() && !allowedComp.contains(compressionValue)) {
        qDebug() << "Error: Unsupported compression format:" << compressionValue;
        qDebug() << "Supported formats:" << allowedComp.join(", ");
        qDebug() << "Please use one of the supported formats or omit the option to use default (zstd).";
        return EXIT_FAILURE;
    }

    QCoreApplication *app;
#ifdef CLI_BUILD
    app = new QCoreApplication(argc, argv);
#else
    // Determine if we should run in CLI mode based on multiple factors
    const bool forceCliMode = parser.isSet("cli") ||
                              QFileInfo(QString::fromLocal8Bit(argv[0])).baseName().endsWith("cli") ||
                              !qEnvironmentVariableIsEmpty("MX_SNAPSHOT_CLI");
    const bool noWindowSystem = qEnvironmentVariableIsEmpty("DISPLAY") &&
                                qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
    const QString qpa = QString::fromLocal8Bit(qgetenv("QT_QPA_PLATFORM"));
    const bool headlessQpa = (qpa == QLatin1String("offscreen") ||
                              qpa == QLatin1String("minimal") ||
                              qpa == QLatin1String("linuxfb"));
    const bool useCliMode = forceCliMode || noWindowSystem || headlessQpa;

    if (useCliMode) {
        app = new QCoreApplication(argc, argv);
    } else {
        // Set Qt platform to XCB (X11) if not already set and we're in X11 environment
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
            if (!qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
                qputenv("QT_QPA_PLATFORM", "xcb");
            }
        }
        app = new QApplication(argc, argv);
        QApplication::setWindowIcon(QIcon::fromTheme("mx-snapshot"));
        QApplication::setApplicationDisplayName(QObject::tr("MX Snapshot"));
    }
#endif

    // Deliver POSIX signals to the event loop: the raw handler only writes to
    // the socketpair; all Qt calls happen here, where they are safe.
    auto *sigNotifier = new QSocketNotifier(signalFd[0], QSocketNotifier::Read, app);
    QObject::connect(sigNotifier, &QSocketNotifier::activated, app, [] {
        char sig = 0;
        if (::read(signalFd[0], &sig, 1) != 1) {
            return;
        }
        const auto signame = strsignal(sig);
        qDebug() << "\nReceived signal:" << (signame != nullptr ? signame : "Unknown signal");
        if (cancelRun) {
            cancelRun();
        } else {
            quitRequested = true;
            QCoreApplication::exit(EXIT_FAILURE);
        }
    });

    int exitCode = EXIT_FAILURE;

    if (logname == "root") {
        const QString message = QObject::tr(
            "You seem to be logged in as root, please log out and log in as normal user to use this program.");
        MessageHandler::showMessage(MessageHandler::Critical, QObject::tr("Error"), message);
    } else {
        setTranslation();
        try {
            checkSquashfs();

            const bool isGuiApp = QCoreApplication::instance()->inherits("QApplication");
            const bool hasAuthTools = !Cmd::elevationTool().isEmpty();
            if (getuid() != 0 && (!isGuiApp || !hasAuthTools)) {
                qDebug().noquote() << QObject::tr("You must run this program with sudo or pkexec.");
            } else if (!takeInstanceLock()) {
                exitCode = EXIT_FAILURE;
            } else {
                const Log setLog(Log::defaultLogPath(app->applicationName()));
                qInstallMessageHandler(Log::messageHandler);
                qDebug().noquote() << app->applicationName() << QObject::tr("version:") << app->applicationVersion();
                if (argc > 1) {
                    qDebug().noquote() << "Args:" << app->arguments();
                }

                // Create settings instance for dependency injection
                Settings settings(parser, isGuiApp);

                if (quitRequested) {
                    qDebug().noquote() << QObject::tr("Cancelled before the snapshot started.");
                } else if (!isGuiApp) {
                    Batchprocessing batch(&settings);
                    cancelRun = [&batch] { batch.cancel(); };
                    const auto clearCancel = qScopeGuard([] { cancelRun = nullptr; });
                    batch.run();
                    // QCoreApplication::exit() requested by Work::cleanUp() inside
                    // run() has no effect before exec() starts, and the queued
                    // quit() below always makes exec() return 0. Read the real
                    // outcome from Batchprocessing instead. exec() also emits
                    // aboutToQuit, which tears down a successful run; signals stay
                    // routed to cancel() until then.
                    QTimer::singleShot(0, app, &QCoreApplication::quit);
                    app->exec();
                    exitCode = batch.succeeded() ? EXIT_SUCCESS : EXIT_FAILURE;
                }
#ifndef CLI_BUILD
                else {
                    MainWindow w(&settings);
                    if (!quitRequested) {
                        cancelRun = [&w] { w.cancelFromSignal(); };
                        const auto clearCancel = qScopeGuard([] { cancelRun = nullptr; });
                        w.show();
                        exitCode = app->exec();
                    }
                }
#endif
            }
        } catch (const std::exception &e) {
            qCritical().noquote() << QObject::tr("Fatal error:") << e.what();
        } catch (...) {
            qCritical().noquote() << QObject::tr("Fatal error: unknown exception");
        }
    }

    qInstallMessageHandler(nullptr);
    delete app;
    app = nullptr;
    return exitCode;
}

// Taken before the log is opened (a second instance would truncate the first
// one's log) and before Settings, whose constructor tears down whatever a
// previous run left behind and must never mistake a live run for leftovers.
// Fails closed: running unlocked would let this instance tear down another
// one's live snapshot, which is exactly what the lock prevents. Any local user
// can hold the lock anyway, so failing open would not protect availability.
bool takeInstanceLock()
{
    QString error;
    switch (InstanceLock::acquire(InstanceLock::defaultPath, &error)) {
    case InstanceLock::Result::Acquired:
        return true;
    case InstanceLock::Result::Busy:
        MessageHandler::showMessage(
            MessageHandler::Critical, QObject::tr("Error"),
            QObject::tr("Another snapshot is already running (MX Snapshot or iso-snapshot-cli). "
                        "Wait for it to finish, then try again."));
        return false;
    case InstanceLock::Result::Unavailable:
        MessageHandler::showMessage(
            MessageHandler::Critical, QObject::tr("Error"),
            QObject::tr("Cannot use the lock file %1 (%2). It keeps two snapshots from running at once. "
                        "Restarting the computer clears it, or an administrator can delete the file.")
                .arg(QString::fromLatin1(InstanceLock::defaultPath), error));
        return false;
    }
    return false;
}

void setTranslation()
{
    const QString localeName = QLocale().name();
    const QString translationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    const QString appName = QCoreApplication::applicationName();

    if (qtTran.load("qt_" + localeName, translationsPath)) {
        QCoreApplication::installTranslator(&qtTran);
    }

    if (qtBaseTran.load("qtbase_" + localeName, translationsPath)) {
        QCoreApplication::installTranslator(&qtBaseTran);
    }

    if (appTran.load("mx-snapshot_" + localeName, "/usr/share/" + appName + "/locale")) {
        QCoreApplication::installTranslator(&appTran);
    }
}

void checkSquashfs()
{
    QProcess proc;
    proc.start("uname", {"-r"});
    proc.waitForFinished();
    currentKernel = proc.readAllStandardOutput().trimmed();

    // Arch ships kernels without /boot/config-* by default and mksquashfs is userspace
    // anyway, so the QFile::exists guard naturally skips the check on Arch.
    const QString configPath = "/boot/config-" + currentKernel;
    if (QFile::exists(configPath) && QProcess::execute("grep", {"-q", "^CONFIG_SQUASHFS=[ym]", configPath}) != 0) {
        const QString message = QObject::tr("Current kernel doesn't support Squashfs, cannot continue.");
        MessageHandler::showMessage(MessageHandler::Critical, QObject::tr("Error"), message);
        exit(EXIT_FAILURE);
    }
}

void signalHandler(int signal)
{
    // Only async-signal-safe calls are allowed here; the QSocketNotifier hooked
    // to the other end of the pair does the actual logging and quit.
    const char byte = static_cast<char>(signal);
    [[maybe_unused]] const ssize_t written = ::write(signalFd[1], &byte, 1);
}
