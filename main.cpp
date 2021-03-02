/**********************************************************************
 *  main.cpp
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

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QScopedPointer>
#include <QTranslator>

#include "batchprocessing.h"
#include "mainwindow.h"
#include "version.h"
#include <signal.h>
#include <unistd.h>

static QScopedPointer<QFile> logFile;
static QTranslator qtTran, qtBaseTran, appTran;

void checkSquashfs();
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
void runApp(QCommandLineParser parser);
void setLog();
void setTranslation();
void signalHandler(int sig);

int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGHUP, signalHandler);
    //signal(SIGQUIT, signalHandler); // allow SIGQUIT CTRL-\?

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Tool used for creating a live-CD from the running system"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{"c", "cli"}, QObject::tr("Use CLI only")});
    parser.addOption({{"d", "directory"}, QObject::tr("Output directory"), QObject::tr("path")});
    parser.addOption({{"f", "file"}, QObject::tr("Output filename"), QObject::tr("name")});
    parser.addOption({{"k", "kernel"}, QObject::tr("Name a different kernel to use other than the default running kernel, use format returned by 'uname -r'") + " " +
                      QObject::tr("Or the full path: %1").arg("/boot/vmlinuz-x.xx.x..."), QObject::tr("version, or path")});
    parser.addOption({{"m", "month"}, QObject::tr("Create a monthly snapshot, add 'Month' name in the ISO name, skip used space calculation") + " " +
                     QObject::tr("This option sets reset-accounts and compression to defaults, arguments changing those items will be ignored")});
    parser.addOption({{"n", "no-checksums"}, QObject::tr("Don't calculate checksums for resulting ISO file")});
    parser.addOption({{"p", "preempt"}, QObject::tr("Option to fix issue with calculating checksums on preempt_rt kernels")});
    parser.addOption({{"r", "reset"}, QObject::tr("Resetting accounts (for distribution to others)")});
    parser.addOption({{"s", "checksums"}, QObject::tr("Calculate checksums for resulting ISO file")});
    parser.addOption({{"x", "exclude"}, QObject::tr("Exclude main folders, valid choices: ") + QObject::tr("Desktop, Documents, Downloads, Music, Networks, Pictures, Videos.") + " " +
                      QObject::tr("Use the option one time for each item you want to exclude"), QObject::tr("one item")});
    parser.addOption({{"z", "compression"}, QObject::tr("Compression format, valid choices: ") + "lz4, lzo, gzip, xz", QObject::tr("format")});


    QStringList opts;
    for (int i = 0; i < argc; ++i)
        opts << QString(argv[i]);
    parser.parse(opts);

    QStringList allowed_comp {"lz4", "lzo", "gzip", "xz"};
    if (!parser.value("compression").isEmpty())
        if (!allowed_comp.contains(parser.value("compression"))) {
            qDebug() << "Wrong compression format";
            return EXIT_FAILURE;
        }

    if (parser.isSet("cli") or parser.isSet("help")) {
        QCoreApplication app(argc, argv);
        app.setApplicationVersion(VERSION);
        parser.process(app);
        setTranslation();
        checkSquashfs();

        // root guard
        if (system("logname |grep -q ^root$") == 0) {
            qDebug() << QObject::tr("You seem to be logged in as root, please log out and log in as normal user to use this program.");
            exit(EXIT_FAILURE);
        }

        if (getuid() == 0) {
            setLog();
            qDebug().noquote() << qApp->applicationName() << QObject::tr("version:") << qApp->applicationVersion();
            if (argc > 1) qDebug().noquote() << "Args:" << qApp->arguments();
            Batchprocessing  batch(parser);
            QTimer::singleShot(0, &app, &QCoreApplication::quit);
            return app.exec();
        } else {
            qDebug().noquote() << QObject::tr("You must run this program as root.");
            return EXIT_FAILURE;
        }
    } else {
        QApplication app(argc, argv);
        app.setApplicationVersion(VERSION);
        parser.process(app);
        setTranslation();
        checkSquashfs();

        // root guard
        if (system("logname |grep -q ^root$") == 0) {
            QMessageBox::critical(nullptr, QObject::tr("Error"),
                                  QObject::tr("You seem to be logged in as root, please log out and log in as normal user to use this program."));
            exit(EXIT_FAILURE);
        }

        if (getuid() == 0) {
            setLog();
            qDebug().noquote() << qApp->applicationName() << QObject::tr("version:") << qApp->applicationVersion();
            if (argc > 1) qDebug().noquote() << "Args:" << qApp->arguments();
            MainWindow w(nullptr, parser);
            w.show();
            exit(app.exec());
        } else {
            system("su-to-root -X -c " + app.applicationFilePath().toUtf8() + "&");
        }
    }
}

// The implementation of the handler
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QTextStream term_out(stdout);
    msg.contains("\r") ? term_out << msg << flush: term_out << msg << "\n" << flush;

    if (msg.startsWith("\033[2KProcessing")) return;
    QTextStream out(logFile.data());
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ");
    switch (type)
    {
    case QtInfoMsg:     out << QStringLiteral("INF "); break;
    case QtDebugMsg:    out << QStringLiteral("DBG "); break;
    case QtWarningMsg:  out << QStringLiteral("WRN "); break;
    case QtCriticalMsg: out << QStringLiteral("CRT "); break;
    case QtFatalMsg:    out << QStringLiteral("FTL "); break;
    }
    out << context.category << QStringLiteral(": ") << msg << endl;
}

void setTranslation()
{
    if (qtTran.load(QLocale::system(), "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        qApp->installTranslator(&qtTran);


    if (qtBaseTran.load("qtbase_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        qApp->installTranslator(&qtBaseTran);

    if (appTran.load(qApp->applicationName() + "_" + QLocale::system().name(), "/usr/share/" + qApp->applicationName() + "/locale"))
        qApp->installTranslator(&appTran);
}

// Check if SQUASHFS is available
void checkSquashfs()
{
    if (system("[ -f /boot/config-$(uname -r) ]") == 0 && system("grep -q ^CONFIG_SQUASHFS=[ym] /boot/config-$(uname -r)") != 0) {
        QMessageBox::critical(nullptr, QObject::tr("Error"),
                              QObject::tr("Current kernel doesn't support Squashfs, cannot continue."));
        exit(EXIT_FAILURE);
    }
}

void setLog()
{
    QString log_name= "/var/log/" + qApp->applicationName() + ".log";
    system("[ -f " + log_name.toUtf8() + " ] && mv " + log_name.toUtf8() + " " + log_name.toUtf8() + ".old");
    logFile.reset(new QFile(log_name));
    logFile.data()->open(QFile::Append | QFile::Text);
    qInstallMessageHandler(messageHandler);
}

void signalHandler(int sig)
{
    switch (sig)
    {
    case 1: qDebug() << "\nSIGHUP"; break;
    case 2: qDebug() << "\nSIGINT"; break;
    case 3: qDebug() << "\nSIGQUIT"; break;
    case 15: qDebug() << "\nSIGTERM"; break;
    }
    qApp->quit(); // quit app anyway in case a subprocess was killed, but at least this calls aboutToQuit
}
